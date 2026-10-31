#include "directmemory.h"

#include <Tempest/Log>

#include <algorithm>
#include <cstdio>
#include <exception>
#include <set>

#include "hookaddresses.h"
#include "logbudget.h"
#include "world/objects/item.h"
#include "world/objects/npc.h"

using namespace Tempest;
using namespace Compatibility;

bool DirectMemory::isHookAnswered(uint32_t address) {
  // the places that OpenGothic calls fireHook for, see directmemory_events.cpp
  static const std::set<uint32_t> answered = {
    HookAdr::ChangeLevelBegin,
    HookAdr::ChangeLevelEnd,
    HookAdr::WriteSavegame,
    HookAdr::SaveGameWorld,
    HookAdr::SaveWorldName,
    HookAdr::LoadingTexture,
    HookAdr::ReadSavegame,
    HookAdr::InventoryInsert,
    HookAdr::ChangeAttribute,
    HookAdr::KeyEvent,
    HookAdr::ZoneMusicChanged,
    HookAdr::CreateFallDamage,
    HookAdr::OnDamage,
    HookAdr::OnDamageScript,
    HookAdr::SpellSetup,
    HookAdr::SpellTargetValid,
    HookAdr::MobCanOpen,
    HookAdr::EvDropVob,
    HookAdr::InventoryOpen,
    HookAdr::ItemInfoDraw,
    HookAdr::SellItem,
    HookAdr::HideFocusParty,
    HookAdr::NextEnemy,
    HookAdr::FinishingMove,
    HookAdr::PickLockLength,
    HookAdr::MovingCutscene,
    HookAdr::CutsceneMenu,
    HookAdr::CutsceneStatus,
    HookAdr::DialogMessage,
    HookAdr::CsCameraTrigger,
    HookAdr::CsCameraUntrigger,
    HookAdr::BarUpdateResolution,
    HookAdr::SpriteRender,
    HookAdr::GameRender,
    HookAdr::StatusScreenShow,
    HookAdr::StatusScreenClose,
    HookAdr::EquipItem,
    HookAdr::EquipBestWeapons,
    HookAdr::UnequipItem,
    HookAdr::DropUnconscious,
    };
  return answered.find(address)!=answered.end();
  }

/// The final boards (the end of the game) of CoM: the player does not move while they run. The scripts replace the check
/// of the global cutscene in oCAIHuman::Moving with ISINCAMERA || ISINFINALBOARDS; the camera cutscenes are locked by
/// OpenGothic itself, only the boards need this.
/// ISINCAMERA of CoM: the hook of zCCSCamera::OnTrigger has set it, the hook of OnUntrigger has cleared it
bool DirectMemory::inCamera() {
  if(!hasHook(HookAdr::CsCameraTrigger))
    return false;
  auto* sym = vm.find_symbol_by_name("ISINCAMERA");
  return sym!=nullptr && sym->type()==zenkit::DaedalusDataType::INT && sym->get_int()!=0;
  }

bool DirectMemory::inFinalBoards() {
  if(!hasHook(HookAdr::MovingCutscene))
    return false;
  auto* sym = vm.find_symbol_by_name("ISINFINALBOARDS");
  return sym!=nullptr && sym->type()==zenkit::DaedalusDataType::INT && sym->get_int()!=0;
  }

bool DirectMemory::hidesPartyFocus() const {
  return hasHook(HookAdr::HideFocusParty);
  }

bool DirectMemory::cutsceneMenuBlocked() {
  HookRegs regs;
  regs.eax = (inCamera() || inFinalBoards()) ? 1 : 0;
  fireHook(HookAdr::CutsceneMenu,regs);
  return regs.eax!=0;
  }

bool DirectMemory::cutsceneStatusHidden() {
  HookRegs regs;
  regs.eax = (inCamera() || inFinalBoards()) ? 1 : 0;
  fireHook(HookAdr::CutsceneStatus,regs);
  return regs.eax!=0;
  }

bool DirectMemory::hasHook(uint32_t address) const {
  auto it = hooks.find(address);
  return it!=hooks.end() && !it->second.empty();
  }

auto DirectMemory::ptrOf(Npc& npc) -> ptr32_t {
  return handles.pointerTo(npc.handlePtr());
  }

auto DirectMemory::ptrOf(Item& item) -> ptr32_t {
  return handles.pointerTo(item.handlePtr());
  }

/// Calls the functions that are hooked at the address: the registers are the variables ECX, EAX, ... of LeGo, the stack
/// is a piece of the pretended memory ([esp] is the address of the return, [esp+4] the first argument).
bool DirectMemory::fireHook(uint32_t address, HookRegs& regs) {
  auto it = hooks.find(address);
  if(it==hooks.end() || it->second.empty())
    return false;
  const auto funcs = it->second; // the hook may hook another one

  // once for every place: the log says which of the hooks of the libraries the game has reached
  {
  char key[32];
  std::snprintf(key,sizeof(key),"hook-fired:%x",address);
  if(logBudget(key,1)) {
    std::string names;
    for(auto idx:funcs)
      if(auto* sym = vm.find_symbol_by_index(idx))
        names += (names.empty() ? "" : ", ") + sym->name();
    Log::i("DMA: the first call of the hook ",reinterpret_cast<void*>(uint64_t(address))," -> ",names);
    }
  }

  const uint32_t words = uint32_t(regs.args.size())+4;
  const ptr32_t  stack = mem32.alloc(words*4,"stack of a hook");
  if(stack==0)
    return false;
  for(size_t i=0; i<regs.args.size(); ++i)
    mem32.writeInt(stack+4+ptr32_t(i)*4,regs.args[i]);

  struct Reg {
    const char*     name;
    uint32_t*       value;
    zenkit::DaedalusSymbol* sym = nullptr;
    int32_t         saved = 0;
    };
  uint32_t esp = stack;
  Reg list[] = {
    {"EAX",&regs.eax},{"ECX",&regs.ecx},{"EDX",&regs.edx},{"EBX",&regs.ebx},
    {"ESP",&esp},     {"EBP",&regs.ebp},{"ESI",&regs.esi},{"EDI",&regs.edi},
    };
  // the same registers are used by a hook that is called from a hook
  for(auto& r:list) {
    r.sym = vm.find_symbol_by_name(r.name);
    if(r.sym==nullptr || r.sym->type()!=zenkit::DaedalusDataType::INT)
      r.sym = nullptr;
    if(r.sym!=nullptr) {
      r.saved = r.sym->get_int();
      r.sym->set_int(int32_t(*r.value));
      }
    }

  // the hooks do not change self, other and item of the script that runs now
  auto* gs = vm.global_self();
  auto* go = vm.global_other();
  auto* gi = vm.global_item();
  auto  bakSelf  = gs->get_instance();
  auto  bakOther = go->get_instance();
  auto  bakItem  = gi->get_instance();

  for(auto idx:funcs) {
    auto* sym = vm.find_symbol_by_index(idx);
    if(sym==nullptr || sym->type()!=zenkit::DaedalusDataType::FUNCTION)
      continue;
    if(sym->count()>0) {
      if(logBudget("hook-params"))
        Log::e("DMA: the hook ",sym->name()," has parameters, not supported");
      continue;
      }
    try {
      vm.call_function(sym);
      }
    catch(const std::exception& e) {
      if(logBudget("hook-exception"))
        Log::e("DMA: the hook ",sym->name()," has failed: ",e.what());
      }
    }

  gs->set_instance(bakSelf);
  go->set_instance(bakOther);
  gi->set_instance(bakItem);

  // what the scripts have left in the registers and on the stack
  for(auto& r:list) {
    if(r.sym==nullptr)
      continue;
    if(r.value!=&esp)
      *r.value = uint32_t(r.sym->get_int());
    r.sym->set_int(r.saved);
    }
  for(size_t i=0; i<regs.args.size(); ++i)
    regs.args[i] = mem32.readInt(stack+4+ptr32_t(i)*4);

  mem32.free(stack);
  return true;
  }
