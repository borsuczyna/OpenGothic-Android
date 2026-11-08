#include "savestore.h"

#include <Tempest/File>
#include <Tempest/Log>

#include <miniz.h>

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstring>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <map>

#include "game/serialize.h"
#include "gothic.h"

using namespace Tempest;

namespace {

struct Cached {
  std::filesystem::file_time_type time;
  SaveStore::Entry                entry;
  bool                            valid = false;
  };

/// "save_slot_" or "thechroniclesofmyrtana_save_slot_": the name of the file of the quick save without the number
std::string filePrefix() {
  std::string name = Gothic::inst().saveFileName(0);
  const char  tail[] = "0.sav";
  return name.substr(0,name.size()-(sizeof(tail)-1));
  }

/// "quicksave_" or "thechroniclesofmyrtana_quicksave_": the name of the files of the quick save without the number
std::string quickPrefix() {
  std::string name = filePrefix();
  const char  tail[] = "save_slot_";
  name.erase(name.size()-(sizeof(tail)-1));
  return name+"quicksave_";
  }

std::string lower(std::string s) {
  for(auto& c:s)
    c = char(std::tolower(static_cast<unsigned char>(c)));
  return s;
  }

/// number of the slot in the name of the file, false when the file is not a save of this game
bool slotOf(const std::string& name, const std::string& prefix, size_t& slot) {
  const std::string low = lower(name);
  if(low.size()<prefix.size()+5 || low.compare(0,prefix.size(),prefix)!=0)
    return false;
  if(low.compare(low.size()-4,4,".sav")!=0)
    return false;
  size_t id = 0;
  for(size_t i=prefix.size(); i<low.size()-4; ++i) {
    if(low[i]<'0' || low[i]>'9' || i-prefix.size()>8)
      return false;
    id = id*10+size_t(low[i]-'0');
    }
  slot = id;
  return true;
  }

/// number of the quick save in the name of the file (1..QuickSlots), false when the file is not one of them
bool quickOf(const std::string& name, const std::string& prefix, size_t& n) {
  const std::string low = lower(name);
  if(low.size()!=prefix.size()+5 || low.compare(0,prefix.size(),prefix)!=0)
    return false;
  if(low.compare(low.size()-4,4,".sav")!=0)
    return false;
  const char c = low[prefix.size()];
  if(c<'1' || size_t(c-'0')>SaveStore::QuickSlots)
    return false;
  n = size_t(c-'0');
  return true;
  }

bool readEntry(SaveStore::Entry& e) {
  try {
    RFile     fin(e.file);
    Serialize reader(fin);
    reader.setEntry("header");
    reader.read(e.hdr);

    if(reader.setEntry("info")) {
      uint32_t ver = 0;
      reader.read(ver,e.chapter,e.level,e.topic);
      }
    return true;
    }
  catch(...) {
    return false;
    }
  }

}

std::vector<SaveStore::Entry> SaveStore::list() {
  static std::map<std::string,Cached> cache;

  std::vector<Entry> ret;
  const std::string  prefix = filePrefix();
  const std::string  qprefix = quickPrefix();
  std::error_code    ec;
  std::map<std::string,bool> seen;

  for(auto& it:std::filesystem::directory_iterator(std::filesystem::path("."),ec)) {
    if(!it.is_regular_file(ec))
      continue;
    const std::string name = it.path().filename().string();
    size_t            slot  = 0;
    size_t            quick = 0;
    if(quickOf(name,qprefix,quick))
      slot = 0;   // the files of the quick save are all "slot 0", the number is in `quick`
    else if(!slotOf(name,prefix,slot))
      continue;

    const auto time = it.last_write_time(ec);
    auto&      c    = cache[name];
    if(!c.valid || c.time!=time || c.entry.slot!=slot || c.entry.quick!=int(quick)) {
      Entry e;
      e.slot  = slot;
      e.quick = int(quick);
      e.file  = name;
      c.valid = readEntry(e);
      c.time  = time;
      c.entry = std::move(e);
      }
    seen[name] = true;
    if(!c.valid)
      continue;

    Entry e = c.entry;
    e.written = int64_t(time.time_since_epoch().count());
    tm    t = e.hdr.pcTime;
    t.tm_isdst = -1;
    e.stamp = int64_t(std::mktime(&t));
    ret.push_back(std::move(e));
    }

  for(auto i=cache.begin(); i!=cache.end();) {
    if(seen.find(i->first)==seen.end())
      i = cache.erase(i); else
      ++i;
    }

  std::sort(ret.begin(),ret.end(),[](const Entry& a, const Entry& b){
    if(a.stamp!=b.stamp)
      return a.stamp>b.stamp;
    return a.slot>b.slot;
    });
  return ret;
  }

std::string SaveStore::newFile() {
  size_t next = 1;
  for(auto& e:list())
    next = std::max(next,e.slot+1);
  return Gothic::inst().saveFileName(next);
  }

std::string SaveStore::quickFile(size_t n) {
  return quickPrefix()+std::to_string(n)+".sav";
  }

size_t SaveStore::nextQuick(std::string& file) {
  // the slot that was written last is known after the first save; at the start it is the newest file on the disk
  static size_t last = 0;
  if(last==0) {
    int64_t newest = 0;
    for(auto& e:list()) {
      if(e.quick>0 && (last==0 || e.written>newest)) {
        newest = e.written;
        last   = size_t(e.quick);
        }
      }
    }
  last = last%QuickSlots+1;
  file = quickFile(last);
  return last;
  }

bool SaveStore::newestQuick(Entry& out) {
  bool found = false;
  for(auto& e:list()) {
    if(!e.isQuick())
      continue;
    if(!found || e.written>out.written) {
      out   = e;
      found = true;
      }
    }
  return found;
  }

bool SaveStore::remove(const Entry& e) {
  std::error_code ec;
  return std::filesystem::remove(std::filesystem::path(e.file),ec) && !ec;
  }

bool SaveStore::rename(const Entry& e, std::string_view name) {
  // the save is a zip: every entry is copied as it is, the header gets the new name
  std::vector<char> src;
  {
  std::ifstream fin(e.file,std::ios::binary);
  if(!fin)
    return false;
  src.assign(std::istreambuf_iterator<char>(fin),std::istreambuf_iterator<char>());
  }

  mz_zip_archive rd = {};
  if(!mz_zip_reader_init_mem(&rd,src.data(),src.size(),0))
    return false;

  mz_zip_archive wr = {};
  if(!mz_zip_writer_init_heap(&wr,0,src.size())) {
    mz_zip_reader_end(&rd);
    return false;
    }

  bool ok     = true;
  bool header = false;
  for(mz_uint i=0; i<mz_zip_reader_get_num_files(&rd) && ok; ++i) {
    char fname[512] = {};
    mz_zip_reader_get_filename(&rd,i,fname,sizeof(fname));
    if(std::strcmp(fname,"header")!=0) {
      ok = mz_zip_writer_add_from_zip_reader(&wr,&rd,i)!=MZ_FALSE;
      continue;
      }

    size_t sz  = 0;
    void*  raw = mz_zip_reader_extract_to_heap(&rd,i,&sz,0);
    if(raw==nullptr) {
      ok = false;
      break;
      }
    const auto*  b    = static_cast<const char*>(raw);
    const size_t head = SaveGameHeader::tagSize()+sizeof(uint16_t);
    uint32_t     len  = 0;
    if(sz>=head+sizeof(len)) {
      std::memcpy(&len,b+head,sizeof(len));
      ok = size_t(head)+sizeof(len)+len<=sz;
      } else {
      ok = false;
      }
    if(ok) {
      const uint32_t    n = uint32_t(name.size());
      std::vector<char> out;
      out.reserve(sz+name.size());
      out.insert(out.end(),b,b+head);
      out.insert(out.end(),reinterpret_cast<const char*>(&n),reinterpret_cast<const char*>(&n)+sizeof(n));
      out.insert(out.end(),name.begin(),name.end());
      out.insert(out.end(),b+head+sizeof(len)+len,b+sz);
      ok     = mz_zip_writer_add_mem(&wr,"header",out.data(),out.size(),MZ_DEFAULT_LEVEL)!=MZ_FALSE;
      header = ok;
      }
    mz_free(raw);
    }

  void*  buf = nullptr;
  size_t sz  = 0;
  if(ok && header)
    ok = mz_zip_writer_finalize_heap_archive(&wr,&buf,&sz)!=MZ_FALSE;
  else
    ok = false;
  mz_zip_reader_end(&rd);

  if(ok) {
    const std::string tmp = e.file+".tmp";
    {
    std::ofstream fout(tmp,std::ios::binary|std::ios::trunc);
    fout.write(static_cast<const char*>(buf),std::streamsize(sz));
    ok = bool(fout);
    }
    std::error_code ec;
    if(ok) {
      std::filesystem::rename(std::filesystem::path(tmp),std::filesystem::path(e.file),ec);
      ok = !ec;
      }
    if(!ok)
      std::filesystem::remove(std::filesystem::path(tmp),ec);
    }
  mz_zip_writer_end(&wr);
  return ok;
  }

Pixmap SaveStore::preview(const Entry& e) {
  Pixmap pm;
  try {
    RFile     fin(e.file);
    Serialize reader(fin);
    if(reader.setEntry("priview.png"))
      reader.read(pm);   // legacy
    else if(reader.setEntry("preview.png"))
      reader.read(pm);
    else if(reader.setEntry("preview.jpg"))
      reader.read(pm);
    }
  catch(...) {
    }
  return pm;
  }

std::string SaveStore::placeName(std::string_view world, bool polish) {
  std::string name(world);
  const auto  dot = name.find_last_of('.');
  if(dot!=std::string::npos)
    name.erase(dot);
  const auto slash = name.find_last_of("/\\");
  if(slash!=std::string::npos)
    name.erase(0,slash+1);
  std::string low = lower(name);

  // the texts of the game are in the code page of the language, not in UTF-8
  static const struct { const char* zen; const char* en; const char* pl; } known[] = {
    {"newworld","Khorinis","Khorinis"}, {"oldworld","Valley of Mines","Dolina Kopal\xF1"},
    {"addonworld","Jharkendar","Jarkendar"}, {"dragonisland","Dragon Island","Wyspa Smok\xF3w"},
    {"world","Khorinis","Khorinis"},
    };
  for(auto& k:known)
    if(low==k.zen)
      return polish ? k.pl : k.en;

  for(auto& c:name)
    if(c=='_')
      c = ' ';
  if(!name.empty())
    name[0] = char(std::toupper(static_cast<unsigned char>(name[0])));
  for(size_t i=1; i<name.size(); ++i)
    name[i] = char(std::tolower(static_cast<unsigned char>(name[i])));
  return name;
  }

std::string SaveStore::dateText(const SaveGameHeader& hdr) {
  char buf[64] = {};
  std::snprintf(buf,sizeof(buf),"%02d.%02d.%04d\x1f%02d:%02d",
                int(hdr.pcTime.tm_mday),int(hdr.pcTime.tm_mon+1),int(1900+hdr.pcTime.tm_year),
                int(hdr.pcTime.tm_hour),int(hdr.pcTime.tm_min));
  return buf;
  }

std::string SaveStore::playTimeText(const SaveGameHeader& hdr) {
  const auto mins = hdr.playTime/(60*1000);
  char buf[64] = {};
  std::snprintf(buf,sizeof(buf),"%d h %d min",int(mins/60),int(mins%60));
  return buf;
  }
