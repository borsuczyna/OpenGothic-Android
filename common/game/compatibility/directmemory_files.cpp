#include "directmemory.h"

#include <Tempest/Log>

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstring>

#include "game/serialize.h"
#include "hookaddresses.h"
#include "logbudget.h"

using namespace Tempest;
using namespace Compatibility;

// The addresses of the functions of Windows in Gothic2.exe (the small functions that jump to the table of imports), see EngineAdr_G2.d of LeGo
static constexpr uint32_t WINAPI_CLOSEHANDLE = 8079190;
static constexpr uint32_t WINAPI_CREATEFILEA = 8079286;
static constexpr uint32_t WINAPI_WRITEFILE   = 8079292;
static constexpr uint32_t WINAPI_GETFILESIZE = 8079310;
static constexpr uint32_t WINAPI_GETLASTERROR= 8079394;
static constexpr uint32_t WINAPI_READFILE    = 8272388;

static constexpr uint32_t ZOPTIONS_POINTER   = 9230728;  // zoptions_Pointer_Address
static constexpr uint32_t ZOPTIONS_DIR_STRING= 124;      // zCOptions_dir_string_offset
static constexpr uint32_t MENU_SLOT_OFFSET   = 3276;     // menu_savegame_slot_offset
static constexpr int32_t  INVALID_HANDLE     = -1;
static constexpr uint32_t ERROR_FILE_NOT_FOUND = 2;

/// The only slot that the libraries are told about: the game keeps the files of the scripts inside of its own savegame
static constexpr int      SCRIPT_SLOT        = 1;

auto DirectMemory::vfsKey(std::string_view name) -> std::string {
  std::string ret(name);
  for(char& c:ret) {
    if(c=='\\')
      c = '/';
    c = char(std::tolower(static_cast<unsigned char>(c)));
    }
  return ret;
  }

/// Files. LeGo writes the handles of PermMem to SCRPTSAVE.SAV when the game is saved and reads it when the game is loaded,
/// through CreateFile, WriteFile, ReadFile. Here the files are in memory and go into the savegame of OpenGothic.
void DirectMemory::setupFileFunctions() {
  // LeGo calls the small functions of Gothic2.exe that jump into the table of imports (CALL__stdcall with these
  // addresses), so the handlers are on the addresses themselves
  auto winapi = [this](ptr32_t address, const char*, auto fn) {
    cpu.register_stdcall(address,fn);
    };

  winapi(WINAPI_GETLASTERROR,"GetLastError",[this]() -> int32_t {
    return int32_t(lastError);
    });

  winapi(WINAPI_CREATEFILEA,"CreateFileA",[this](ptr32_t pname, uint32_t access, uint32_t share, uint32_t sec,
                                                  uint32_t disposition, uint32_t flags, uint32_t tmpl) -> int32_t {
    (void)share; (void)sec; (void)flags; (void)tmpl;
    // lpFileName is a pointer to a text of C, not a zString
    std::string name;
    {
    auto [ptr, size] = mem32.deref(pname);
    if(ptr!=nullptr)
      name.assign(reinterpret_cast<const char*>(ptr),strnlen(reinterpret_cast<const char*>(ptr),size_t(size)));
    }
    const auto key    = vfsKey(name);
    const bool exists = vfs.find(key)!=vfs.end();
    const bool write  = (access & 0x40000000u)!=0 || (access & 0x10000000u)!=0 || (access & 0x2u)!=0;
    switch(disposition) {
      case 1: // CREATE_NEW
        if(exists) {
          lastError = 80;
          return INVALID_HANDLE;
          }
        vfs[key].clear();
        break;
      case 2: // CREATE_ALWAYS
        vfs[key].clear();
        break;
      case 3: // OPEN_EXISTING
      case 5: // TRUNCATE_EXISTING
        if(!exists) {
          lastError = ERROR_FILE_NOT_FOUND;
          return INVALID_HANDLE;
          }
        if(disposition==5)
          vfs[key].clear();
        break;
      case 4: // OPEN_ALWAYS
        vfs[key];
        break;
      default:
        lastError = 87;
        return INVALID_HANDLE;
      }
    lastError = 0;
    const uint32_t h = nextVHandle;
    nextVHandle += 4;
    VirtualHandle v;
    v.name  = key;
    v.write = write;
    vhandles[h] = v;
    return int32_t(h);
    });

  winapi(WINAPI_CLOSEHANDLE,"CloseHandle",[this](uint32_t h) -> int32_t {
    return vhandles.erase(h)>0 ? 1 : 0;
    });

  winapi(WINAPI_GETFILESIZE,"GetFileSize",[this](uint32_t h, ptr32_t high) -> int32_t {
    auto it = vhandles.find(h);
    if(it==vhandles.end())
      return INVALID_HANDLE;
    if(high!=0)
      mem32.writeInt(high,0);
    return int32_t(vfs[it->second.name].size());
    });

  winapi(WINAPI_WRITEFILE,"WriteFile",[this](uint32_t h, ptr32_t buf, uint32_t n, ptr32_t written, ptr32_t overlapped) -> int32_t {
    (void)overlapped;
    auto it = vhandles.find(h);
    if(it==vhandles.end() || !it->second.write)
      return 0;
    auto& data = vfs[it->second.name];
    if(n>0) {
      auto* src = reinterpret_cast<const uint8_t*>(mem32.deref(buf,n));
      if(src==nullptr)
        return 0;
      if(data.size()<it->second.pos+n)
        data.resize(it->second.pos+n);
      std::memcpy(data.data()+it->second.pos,src,n);
      it->second.pos += n;
      }
    if(written!=0)
      mem32.writeInt(written,int32_t(n));
    return 1;
    });

  winapi(WINAPI_READFILE,"ReadFile",[this](uint32_t h, ptr32_t buf, uint32_t n, ptr32_t read, ptr32_t overlapped) -> int32_t {
    (void)overlapped;
    auto it = vhandles.find(h);
    if(it==vhandles.end())
      return 0;
    auto& data = vfs[it->second.name];
    const uint32_t have = it->second.pos<data.size() ? uint32_t(data.size())-it->second.pos : 0;
    const uint32_t cnt  = std::min(n,have);
    if(cnt>0) {
      auto* dst = reinterpret_cast<uint8_t*>(mem32.derefv(buf,cnt));
      if(dst==nullptr)
        return 0;
      std::memcpy(dst,data.data()+it->second.pos,cnt);
      it->second.pos += cnt;
      }
    if(read!=0)
      mem32.writeInt(read,int32_t(cnt));
    return 1;
    });
  }

/// The paths of the savegames of the engine as LeGo builds them (BIN_GetSavefilePath) and the slot that it reads
void DirectMemory::setupSaveSupport() {
  // zCOption: the paths. LeGo cuts the first and the last character, so "\Saves\" is the folder "Saves"
  const ptr32_t options = mem32.alloc(512,"zCOption");
  mem32.alloc(ZOPTIONS_POINTER,4,"zoptions*");
  mem32.writeInt(ZOPTIONS_POINTER,int32_t(options));
  for(uint32_t i=0; i<5; ++i) {
    if(auto* s = mem32.deref<zString>(options+ZOPTIONS_DIR_STRING+i*uint32_t(sizeof(zString))))
      memAssignString(*s,i==2 ? "\\Saves\\" : "\\");
    }

  // CGameManager.menu_load_savegame: the menu that knows the slot
  saveSlotBlock = mem32.alloc(4096,"oCMenuSavegame");
  if(auto* sym = vm.find_symbol_by_name("CGAMEMANAGER.MENU_LOAD_SAVEGAME"))
    mem32.writeInt(gameman_Ptr+sym->offset_as_member(),int32_t(saveSlotBlock));
  setSelectedSlot(SCRIPT_SLOT);
  }

void DirectMemory::setSelectedSlot(int slot) {
  if(saveSlotBlock!=0)
    mem32.writeInt(saveSlotBlock+MENU_SLOT_OFFSET,slot);
  }

/// For the log: the beginning of a file of the scripts as text
void DirectMemory::dumpFile(const char* what, const std::string& name, const std::vector<uint8_t>& data) {
  std::string txt;
  for(size_t i=0; i<data.size() && i<600; ++i) {
    const char c = char(data[i]);
    if(c=='\n')
      txt += "\\n";
    else if(c=='\r')
      txt += "\\r";
    else if(c>=32 && c<127)
      txt += c;
    else {
      char buf[8];
      std::snprintf(buf,sizeof(buf),"<%02x>",unsigned(uint8_t(c)));
      txt += buf;
      }
    }
  Log::i("DMA: file ",what," ",name," (",data.size()," bytes): ",txt);
  }

void DirectMemory::eventBeforeSaveGame() {
  // A levitated item must not be archived as a permanently static object.
  finishTelekinesis();
  if(hasHook(HookAdr::WriteSavegame)) {
    // oCSavegameManager::SetAndWriteSavegame keeps the slot in [ebp+60]
    const ptr32_t frame = mem32.alloc(256,"frame of SetAndWriteSavegame");
    if(frame!=0) {
      mem32.writeInt(frame+60,SCRIPT_SLOT);
      HookRegs r;
      r.ebp = frame;
      fireHook(HookAdr::WriteSavegame,r);
      mem32.free(frame);
      }
    }
  HookRegs regs;
  fireHook(HookAdr::SaveGameWorld,regs);
  }

/// The name of the world in the header of a save and in the list of the slots: CoM writes the name of the area there
/// (PATCHSAVEGAMEWORLD_HOOK), an empty answer of the scripts leaves the world of the engine.
auto DirectMemory::saveWorldName(std::string_view fallback) -> std::string {
  std::string ret{fallback};
  if(!hasHook(HookAdr::SaveWorldName))
    return ret;

  // oCSavegameManager::SetAndWriteSavegame keeps the zSTRING of the world in [ebp+84]
  const ptr32_t frame = mem32.alloc(256,"frame of the name of the world");
  if(frame==0)
    return ret;
  HookRegs r;
  r.ebp = frame;
  fireHook(HookAdr::SaveWorldName,r);

  if(auto* s = mem32.deref<zString>(frame+84)) {
    std::string name;
    memFromString(name,*s);
    if(!name.empty())
      ret = std::move(name);
    mem32.free(s->ptr);
    }
  mem32.free(frame);
  return ret;
  }

/// The alternative loading screens of CoM (INITALTERNATIVELOADINGSCREENS): the hook writes the name of a picture into the text of
/// the engine where the loading screen reads it. Here the text is a piece of the pretended memory at the same address.
auto DirectMemory::loadingTexture() -> std::string {
  if(!hasHook(HookAdr::LoadingTexture))
    return {};

  const ptr32_t LOADINGTEXNAME = 9118980;
  const uint32_t size = 64;
  if(!loadingTextureReady) {
    try {
      mem32.alloc(LOADINGTEXNAME,size,"name of the loading picture");
      loadingTextureReady = true;
      }
    catch(const std::exception&) {
      if(logBudget("loading-texture-alloc"))
        Log::e("DMA: the text of the loading picture cannot be placed at ",reinterpret_cast<void*>(uint64_t(LOADINGTEXNAME)));
      return {};
      }
    }

  HookRegs r;
  fireHook(HookAdr::LoadingTexture,r);

  auto [ptr, avail] = mem32.deref(LOADINGTEXNAME);
  if(ptr==nullptr)
    return {};
  const char* txt = reinterpret_cast<const char*>(ptr);
  return std::string(txt,strnlen(txt,std::min<size_t>(size_t(avail),size)));
  }

void DirectMemory::eventSaveGame(Serialize& fout) {
  for(auto& f:vfs)
    dumpFile("saved",f.first,f.second);

  fout.setEntry("game/dmavobs");
  fout.write(uint32_t(vobStates.size()));
  for(auto& i:vobStates)
    fout.write(i.first,uint8_t(i.second.show ? 1 : 0),uint8_t(i.second.collide ? 1 : 0));

  fout.setEntry("game/dma");
  fout.write(uint32_t(vfs.size()));
  for(auto& f:vfs) {
    fout.write(f.first);
    fout.write(uint32_t(f.second.size()));
    if(!f.second.empty())
      fout.writeBytes(f.second.data(),f.second.size());
    }
  }

void DirectMemory::eventLoadGame(Serialize& fin) {
  // Raw renderer pointers in Daedalus variables belong to the old process.
  // Rebind the freshly initialized renderer before LeGo's load callbacks.
  initItemRenderer();
  vfs.clear();
  vhandles.clear();
  vobStates.clear();
  if(fin.setEntry("game/dmavobs")) {
    uint32_t count = 0;
    fin.read(count);
    for(uint32_t i=0; i<count; ++i) {
      std::string key;
      uint8_t     show = 1, collide = 1;
      fin.read(key,show,collide);
      vobStates[key] = VobState{show!=0,collide!=0};
      }
    vobStatesPending = true;
    }
  if(fin.setEntry("game/dma")) {
    uint32_t count = 0;
    fin.read(count);
    for(uint32_t i=0; i<count; ++i) {
      std::string name;
      uint32_t    size = 0;
      fin.read(name,size);
      auto& data = vfs[name];
      data.resize(size);
      if(size>0)
        fin.readBytes(data.data(),size);
      }
    }
  for(auto& f:vfs)
    dumpFile("loaded",f.first,f.second);
  setSelectedSlot(SCRIPT_SLOT);
  }

void DirectMemory::eventChangeLevel(bool begin) {
  if(begin)
    finishTelekinesis();
  if(begin)
    previewItems.clear();
  if(!begin)
    vobStatesPending = true;
  HookRegs r;
  fireHook(begin ? HookAdr::ChangeLevelBegin : HookAdr::ChangeLevelEnd,r);
  }
