#include "originalsave.h"

#include <Tempest/Log>
#include <Tempest/Dir>

#include <zenkit/SaveGame.hh>
#include <zenkit/World.hh>
#include <zenkit/vobs/Misc.hh>
#include <zenkit/vobs/MovableObject.hh>

#include <algorithm>
#include <cmath>
#include <functional>
#include <map>
#include <mutex>
#include <set>

#include "game/gamesession.h"
#include "game/gamescript.h"
#include "world/objects/npc.h"
#include "world/objects/item.h"
#include "world/objects/interactive.h"
#include "world/triggers/movetrigger.h"
#include "world/world.h"
#include "game/worldstatestorage.h"
#include "camera.h"
#include "resources.h"
#include "gothic.h"

using namespace Tempest;

namespace {

std::string lower(std::string s) {
  for(auto& c:s)
    c = char(std::tolower(uint8_t(c)));
  return s;
  }

std::string worldStem(std::string s) {
  // "NEWWORLD", "NEWWORLD.ZEN" or "NEWWORLD\\NEWWORLD.ZEN"
  auto sl = s.find_last_of("\\/");
  if(sl!=std::string::npos)
    s = s.substr(sl+1);
  auto dot = s.rfind('.');
  if(dot!=std::string::npos)
    s = s.substr(0,dot);
  return lower(s);
  }

using Records = std::vector<std::shared_ptr<zenkit::VNpc>>;

Records collect(const zenkit::World& w) {
  Records                        ret;
  std::set<const zenkit::VNpc*>  seen;
  auto emit = [&](const std::shared_ptr<zenkit::VNpc>& n) {
    if(n!=nullptr && seen.insert(n.get()).second)
      ret.push_back(n);
    };
  std::function<void(const std::shared_ptr<zenkit::VirtualObject>&)> visit = [&](const std::shared_ptr<zenkit::VirtualObject>& v) {
    if(v==nullptr)
      return;
    emit(std::dynamic_pointer_cast<zenkit::VNpc>(v));
    // the hero is saved inside of his arrow, while it is in the air
    if(auto arrow = std::dynamic_pointer_cast<zenkit::AiArrow>(v->ai)) {
      auto owner = std::dynamic_pointer_cast<zenkit::VNpc>(arrow->owner);
      for(int depth=0; owner!=nullptr && depth<16; ++depth) {
        if(owner->player)
          emit(owner);
        owner = std::dynamic_pointer_cast<zenkit::VNpc>(owner->enemy);
        }
      }
    for(auto& c:v->children)
      visit(c);
    };
  for(auto& v:w.world_vobs)
    visit(v);
  for(auto& n:w.npcs)
    emit(n);
  for(auto& sp:w.npc_spawns)
    emit(sp.npc);
  return ret;
  }

void applyNpc(World& world, Npc& npc, const zenkit::VNpc& s) {
  auto& vm = world.script().getVm();
  auto& h  = npc.handle();

  if(!s.player && !s.current_routine.empty()) {
    auto* rtn = vm.find_symbol_by_name(s.current_routine);
    if(rtn!=nullptr && rtn->type()==zenkit::DaedalusDataType::FUNCTION && rtn->count()==0 && !rtn->is_external() &&
       rtn->index()!=uint32_t(h.daily_routine)) {
      npc.clearAiQueue();
      npc.clearGoTo();
      npc.clearState(true);
      npc.excRoutine(rtn->index());
      }
    }

  // inventory
  std::map<size_t,size_t> counts;
  auto count = [&](std::string_view name, size_t n) {
    if(auto* sym = vm.find_symbol_by_name(name))
      counts[sym->index()] += n;
    };
  for(auto& pack:s.packed) {
    size_t at = 0;
    while(at<pack.size()) {
      auto end = pack.find(',',at);
      if(end==std::string::npos)
        end = pack.size();
      auto part = pack.substr(at,end-at);
      auto sep  = part.rfind(':');
      if(sep!=std::string::npos && sep>0)
        count(part.substr(0,sep), size_t(std::max(0,std::atoi(part.c_str()+sep+1))));
      at = end+1;
      }
    }
  std::set<const zenkit::VItem*> seen;
  auto item = [&](const std::shared_ptr<zenkit::VItem>& i) {
    if(i!=nullptr && seen.insert(i.get()).second)
      count(i->instance, size_t(std::max(1,i->s_amount)));
    };
  for(auto& i:s.items)
    item(i);
  for(auto& sl:s.slots)
    if(sl!=nullptr && sl->used)
      item(sl->item);

  npc.clearInventory();
  for(auto& i:counts)
    npc.addItem(i.first,i.second);

  auto equip = [&](std::string_view name) {
    auto* sym = vm.find_symbol_by_name(name);
    if(sym==nullptr)
      return;
    if(npc.itemCount(sym->index())==0)
      npc.addItem(sym->index(),1);
    npc.useItem(sym->index(),Item::NSLOT,true);
    };
  auto slot = [&](std::string_view a, std::string_view b) {
    for(auto name:{a,b})
      for(auto& sl:s.slots)
        if(sl!=nullptr && sl->used && sl->item!=nullptr && sl->name==name) {
          equip(sl->item->instance);
          return;
          }
    };
  slot("ZS_TORSO","");
  slot("ZS_LONGSWORD","ZS_SWORD");
  slot("ZS_CROSSBOW","ZS_BOW");
  for(auto& i:s.items) {
    if(i==nullptr || i->s_flags>=0)
      continue;
    auto name = i->instance;
    for(auto& c:name)
      c = char(std::toupper(uint8_t(c)));
    if(name.rfind("ITAM_",0)==0 || name.rfind("ITRI_",0)==0 || name.rfind("ITBE_",0)==0)
      equip(i->instance);
    }

  // keys 3..0 of the original game are the slots 3..10 here
  for(size_t i=0; i<s.items.size() && i<s.item_short_keys.size(); ++i) {
    const int key = s.item_short_keys[i];
    if(s.items[i]==nullptr || key<0 || key>=8)
      continue;
    if(auto* sym = vm.find_symbol_by_name(s.items[i]->instance))
      npc.useItem(sym->index(),uint8_t(3+key),true);
    }

  // equipment changes the attributes: the saved numbers are written after it
  h.level        = s.level;
  h.exp          = s.xp;
  h.exp_next     = s.xp_next_level;
  h.lp           = s.lp;
  h.guild        = s.guild;
  h.flags        = zenkit::NpcFlag(s.flags);
  h.fight_tactic = s.fight_tactic;
  h.type         = zenkit::NpcType(s.npc_type);
  for(size_t i=0; i<zenkit::VNpc::attribute_count; ++i)
    h.attribute[i] = s.attributes[i];
  for(size_t i=0; i<zenkit::VNpc::hcs_count; ++i)
    h.hitchance[i+1] = s.hit_chance[i];
  for(size_t i=0; i<zenkit::VNpc::protection_count; ++i)
    h.protection[i] = s.protection[i];
  for(size_t i=0; i<zenkit::VNpc::aivar_count; ++i)
    h.aivar[i] = s.aivar[i];
  for(size_t i=0; i<zenkit::VNpc::missions_count; ++i)
    h.mission[i] = s.missions[i];
  npc.setTrueGuild(s.guild_true);
  npc.setAttitude    (Attitude(s.attitude));
  npc.setTempAttitude(Attitude(s.attitude_temp));
  for(auto& t:s.talents) {
    if(t==nullptr)
      continue;
    npc.setTalentValue(Talent(t->talent),t->value);
    npc.setTalentSkill(Talent(t->talent),t->skill);
    }

  npc.setPosition(s.position.x,s.position.y,s.position.z);
  npc.setDirection(float(std::atan2(s.rotation[2][0],s.rotation[2][2])*180.0/3.14159265358979));
  for(auto& o:s.overlays)
    npc.addOverlay(o,0);

  if(s.attributes[ATR_HITPOINTS]<=0 && !npc.isDead()) {
    h.attribute[ATR_HITPOINTS] = 1;
    npc.changeAttribute(ATR_HITPOINTS,-1,false);
    }
  }

}

bool OriginalSave::pending(OriginalSave& out) {
  auto dir = Gothic::nestedPath({u"Saves",u"import"},Dir::FT_Dir);
  if(dir.empty())
    return false;
  std::filesystem::path p(dir);
  // the path of a folder ends with a separator: the name of it is the last part
  if(!p.has_filename())
    p = p.parent_path();
  std::error_code       ec;
  for(auto name:{"SAVEDAT.SAV","savedat.sav","SaveDat.sav"})
    if(std::filesystem::exists(p/name,ec)) {
      out.dir = p;
      return true;
      }
  return false;
  }

namespace {
std::mutex  importSync;
std::string importSaveName;
}

void OriginalSave::retire(const OriginalSave& src, bool ok, const std::string& title) {
  std::error_code ec;
  auto done = src.dir.parent_path()/(ok ? "import-done" : "import-failed");
  std::filesystem::remove_all(done,ec);
  std::filesystem::rename(src.dir,done,ec);
  if(ec)
    Log::e("Original save: the import folder stays in place: ",ec.message());
  if(ok) {
    std::lock_guard<std::mutex> guard(importSync);
    importSaveName = "Gothic II: "+title;
    }
  }

bool OriginalSave::takeSaveName(std::string& out) {
  std::lock_guard<std::mutex> guard(importSync);
  if(importSaveName.empty())
    return false;
  out = std::move(importSaveName);
  importSaveName.clear();
  return true;
  }

std::string OriginalSave::worldFile(const OriginalSave& src) {
  zenkit::SaveGame save(zenkit::GameVersion::GOTHIC_2);
  save.load(src.dir);
  return worldStem(save.metadata.world)+".zen";
  }

namespace {

struct ObjCounters {
  uint32_t containers=0, doors=0, unmatched=0, items=0, itemsKept=0, itemsAdded=0, itemsRemoved=0;
  };

/// chests and doors of the save: the contents and the lock; the items that lie in the world
void applyObjects(World& world, const zenkit::World& zw, ObjCounters& cnt) {
  std::vector<const zenkit::VInteractiveObject*> mobs;
  std::vector<const zenkit::VItem*>              items;
  std::function<void(const std::shared_ptr<zenkit::VirtualObject>&)> visit = [&](const std::shared_ptr<zenkit::VirtualObject>& v) {
    if(v==nullptr)
      return;
    if(v->type==zenkit::VirtualObjectType::oCMobContainer || v->type==zenkit::VirtualObjectType::oCMobDoor)
      mobs.push_back(static_cast<const zenkit::VInteractiveObject*>(v.get()));
    else if(v->type==zenkit::VirtualObjectType::oCItem)
      items.push_back(static_cast<const zenkit::VItem*>(v.get()));
    for(auto& c:v->children)
      visit(c);
    };
  for(auto& v:zw.world_vobs)
    visit(v);

  // the objects of the engine, by the same kind
  std::vector<Interactive*> engine;
  for(uint32_t i=0; auto* m = world.mobsiById(i); ++i)
    if(m->isContainer() || m->isDoor())
      engine.push_back(m);

  constexpr float maxDist = 100.f; // one metre: the objects of the level are where the level has put them
  for(auto* src:mobs) {
    const bool chest = src->type==zenkit::VirtualObjectType::oCMobContainer;
    const Tempest::Vec3 at(src->position.x,src->position.y,src->position.z);
    Interactive* best = nullptr;
    float        dist = maxDist*maxDist;
    for(auto* m:engine) {
      if(m==nullptr || m->isContainer()!=chest)
        continue;
      if(!src->vob_name.empty() && !m->tag().empty() && src->vob_name!=m->tag())
        continue;
      const float d = (m->position()-at).quadLength();
      if(d<dist) {
        dist = d;
        best = m;
        }
      }
    if(best==nullptr) {
      ++cnt.unmatched;
      continue;
      }
    std::replace(engine.begin(),engine.end(),best,static_cast<Interactive*>(nullptr));

    if(chest) {
      auto& c = static_cast<const zenkit::VContainer&>(*src);
      best->setAsCracked(!c.locked);
      auto& inv = best->inventory();
      inv.clear(world.script(),*best,true);
      for(auto& it:c.s_items)
        if(it!=nullptr)
          inv.addItem(it->instance,size_t(std::max(1,it->s_amount)),world);
      ++cnt.containers;
      } else {
      auto& d = static_cast<const zenkit::VDoor&>(*src);
      best->setAsCracked(!d.locked);
      ++cnt.doors;
      }
    }

  // the items on the ground: kept where the save has the same item close by, the others are put or taken away
  std::vector<Item*> present;
  for(uint32_t i=0; auto* it = world.itmById(i); ++i)
    present.push_back(it);
  for(auto* src:items) {
    ++cnt.items;
    const size_t cls = world.script().findSymbolIndex(src->instance);
    if(cls==size_t(-1))
      continue;
    const Tempest::Vec3 at(src->position.x,src->position.y,src->position.z);
    Item* best = nullptr;
    float dist = maxDist*maxDist;
    for(auto* it:present) {
      if(it==nullptr || it->clsId()!=cls)
        continue;
      const float d = (it->position()-at).quadLength();
      if(d<dist) {
        dist = d;
        best = it;
        }
      }
    if(best!=nullptr) {
      std::replace(present.begin(),present.end(),best,static_cast<Item*>(nullptr));
      ++cnt.itemsKept;
      } else {
      best = world.addItem(*src);
      if(best==nullptr)
        continue;
      ++cnt.itemsAdded;
      }
    best->setCount(size_t(std::max(1,src->s_amount)));
    }
  for(auto* it:present)
    if(it!=nullptr) {
      world.removeItem(*it);
      ++cnt.itemsRemoved;
      }
  }

struct TrgCounters {
  uint32_t triggers=0, movers=0, moving=0, unmatched=0;
  };

/// triggers and movers of the save: the counters of the triggers, the keyframe of the movers (doors, gates, lifts, levers)
void applyTriggers(World& world, const zenkit::World& zw, TrgCounters& cnt) {
  using zenkit::VirtualObjectType;
  std::vector<const zenkit::VTrigger*> src;
  std::function<void(const std::shared_ptr<zenkit::VirtualObject>&)> visit = [&](const std::shared_ptr<zenkit::VirtualObject>& v) {
    if(v==nullptr)
      return;
    const auto t = v->type;
    if(t==VirtualObjectType::zCTrigger || t==VirtualObjectType::zCTriggerList || t==VirtualObjectType::oCTriggerScript ||
       t==VirtualObjectType::zCMover   || t==VirtualObjectType::oCTriggerChangeLevel || t==VirtualObjectType::oCCSTrigger)
      src.push_back(static_cast<const zenkit::VTrigger*>(v.get()));
    for(auto& c:v->children)
      visit(c);
    };
  for(auto& v:zw.world_vobs)
    visit(v);

  std::vector<AbstractTrigger*> engine;
  world.forEachTrigger([&](AbstractTrigger& t){ engine.push_back(&t); });

  for(auto* s:src) {
    const bool          mover = s->type==VirtualObjectType::zCMover;
    const Tempest::Vec3 at(s->position.x,s->position.y,s->position.z);
    // a named trigger is found by its name, the position tells apart the ones of the same name;
    // a mover is saved where it has moved to, so its name counts for more than its place
    AbstractTrigger* best = nullptr;
    float            dist = 0;
    for(auto* t:engine) {
      if(t==nullptr || (dynamic_cast<MoveTrigger*>(t)!=nullptr)!=mover)
        continue;
      if(s->vob_name!=t->name())
        continue;
      const float d = (t->position()-at).quadLength();
      if(best==nullptr || d<dist) {
        dist = d;
        best = t;
        }
      }
    if(best!=nullptr && s->vob_name.empty() && dist>100.f*100.f)
      best = nullptr;
    if(best==nullptr) {
      ++cnt.unmatched;
      continue;
      }
    std::replace(engine.begin(),engine.end(),best,static_cast<AbstractTrigger*>(nullptr));

    best->importOriginal(s->s_count_can_be_activated,s->s_is_enabled);
    if(auto* m = dynamic_cast<MoveTrigger*>(best)) {
      auto& mv = static_cast<const zenkit::VMover&>(*s);
      m->importMover(int32_t(mv.s_mover_state),mv.s_act_keyframe);
      if(mv.s_mover_state==1 || mv.s_mover_state==3)
        ++cnt.moving;
      ++cnt.movers;
      } else {
      ++cnt.triggers;
      }
    }
  }

void logObjects(std::string_view w, const ObjCounters& c) {
  Log::i("Original save, world ",w,": chests ",c.containers,", doors ",c.doors,", not matched ",c.unmatched,
         "; items on the ground ",c.items,", kept ",c.itemsKept,", added ",c.itemsAdded,", removed ",c.itemsRemoved);
  }

struct Counters {
  uint32_t records=0, matched=0, spawned=0, removed=0, unknown=0;
  };

/// puts the characters of a saved world into the world of a new game; returns the record of the hero, if he is saved here
const zenkit::VNpc* applyWorld(World& world, const Records& records, Counters& cnt) {
  Npc* hero = world.player();
  std::map<size_t,std::vector<Npc*>> present;
  world.detectNpc(hero!=nullptr ? hero->position() : Vec3(),1.0e9f,[&](Npc& n) {
    if(&n!=hero)
      present[n.instanceSymbol()].push_back(&n);
    });

  auto& dvm = world.script().getVm();
  const zenkit::VNpc* heroSrc = nullptr;
  for(auto& r:records) {
    ++cnt.records;
    if(r->player) {
      heroSrc = r.get();
      continue;
      }
    auto* sym = dvm.find_symbol_by_name(r->npc_instance);
    if(sym==nullptr) {
      ++cnt.unknown;
      continue;
      }
    const Vec3 at(r->position.x,r->position.y,r->position.z);
    auto& list = present[sym->index()];
    Npc*  npc  = nullptr;
    if(!list.empty()) {
      auto best = std::min_element(list.begin(),list.end(),[&](Npc* a, Npc* b) {
        return (a->position()-at).quadLength() < (b->position()-at).quadLength();
        });
      npc = *best;
      list.erase(best);
      ++cnt.matched;
      } else {
      npc = world.addNpc(sym->index(),at);
      if(npc==nullptr) {
        ++cnt.unknown;
        continue;
        }
      ++cnt.spawned;
      }
    applyNpc(world,*npc,*r);
    }
  // a character that is not in the save has left the world in the original game
  for(auto& i:present)
    for(auto* npc:i.second) {
      world.removeNpc(*npc);
      ++cnt.removed;
      }
  return heroSrc;
  }

}

GameSession::GameSession(const OriginalSave& src) {
  zenkit::SaveGame save(zenkit::GameVersion::GOTHIC_2);
  save.load(src.dir);
  const std::string current = worldStem(save.metadata.world);

  // the other worlds of the save, which the engine has the level of
  std::vector<std::string> others;
  for(auto& f:std::filesystem::directory_iterator(src.dir)) {
    if(lower(f.path().extension().string())!=".sav")
      continue;
    const auto stem = lower(f.path().stem().string());
    if(stem==current || stem=="savedat" || stem=="saveinfo" || stem=="savehdr" || stem=="thumb")
      continue;
    if(Resources::hasFile(stem+".zen"))
      others.push_back(stem);
    }
  std::sort(others.begin(),others.end());

  cam.reset(new Camera());
  Gothic::inst().setLoadingProgress(0);
  setupSettings();
  setTime(gtime(8,0));

  vm.reset(new GameScript(*this));
  initPerceptions();

  const std::string heroCls(Gothic::inst().defaultPlayer());
  const int         parts = int(others.size())+1;
  int               part  = 0;
  bool              first = true;
  auto startWorld = [&](const std::string& stem) {
    if(!first) {
      // the world before is kept as a visited one, like at a change of the level
      clearWorld();
      vm->resetVarPointers();
      initPerceptions();
      }
    setWorld(std::unique_ptr<World>(new World(*this,stem+".zen",true,[&](int v){
      Gothic::inst().setLoadingProgress((part*90 + int(v*0.55))/parts);
      })));
    if(first)
      vm->initDialogs();
    first = false;
    wrld->createPlayer(heroCls);
    wrld->postInit();
    if(auto hero = wrld->player())
      vm->setInstanceNPC("HERO",*hero);
    initScripts(true);
    ++part;
    };
  // after the state of the save: the events of the first start are in the save already, the world start triggers fire like at a load
  auto finishWorld = [&](const zenkit::World& zw, std::string_view stem) {
    TrgCounters trg;
    applyTriggers(*wrld,zw,trg);
    wrld->dropTriggerEvents();
    wrld->triggerOnStart(false);
    Log::i("Original save, world ",stem,": triggers ",trg.triggers,", movers ",trg.movers," (moving ",trg.moving,"), not matched ",trg.unmatched);
    };

  for(auto& stem:others) {
    auto zw = save.load_world(stem);
    if(zw==nullptr)
      continue;
    startWorld(stem);
    Counters cnt;
    applyWorld(*wrld,collect(*zw),cnt);
    ObjCounters obj;
    applyObjects(*wrld,*zw,obj);
    logObjects(stem,obj);
    finishWorld(*zw,stem);
    Log::i("Original save, world ",stem,": ",cnt.records," characters, matched ",cnt.matched,", spawned ",cnt.spawned,
           ", removed ",cnt.removed,", unknown ",cnt.unknown);
    }

  auto zw = save.load_world(current);
  if(zw==nullptr)
    throw std::runtime_error("the world of the original save is not readable");
  startWorld(current);
  Npc* hero = wrld->player();
  if(hero==nullptr)
    throw std::runtime_error("no player to import the original save into");

  Counters cnt;
  auto*    heroSrc = applyWorld(*wrld,collect(*zw),cnt);
  {
  ObjCounters obj;
  applyObjects(*wrld,*zw,obj);
  logObjects(current,obj);
  }
  finishWorld(*zw,current);
  if(heroSrc==nullptr)
    throw std::runtime_error("the original save has no player in its current world");
  applyNpc(*wrld,*hero,*heroSrc);

  // the scripts of a death change globals: the saved globals are written last
  vm->importOriginal(save.state,*hero);
  setTime(gtime(int64_t(save.state.day),int32_t(save.state.hour),int32_t(save.state.minute)));
  ticks = 1;
  const uint64_t played = uint64_t(std::max(0,save.metadata.play_time_seconds))*1000u;
  playTimeOffset = played>ticks ? played-ticks : 0;
  cam->reset(hero);
  Gothic::inst().setLoadingProgress(96);
  Log::i("Original save, world ",current,": ",cnt.records," characters, matched ",cnt.matched,", spawned ",cnt.spawned,
         ", removed ",cnt.removed,", unknown ",cnt.unknown,"; other worlds ",others.size());
  // a save that the player has not named is called "---" in the original game
  const bool named = save.metadata.title.find_first_not_of("- ")!=std::string::npos;
  OriginalSave::retire(src,true,named ? save.metadata.title : save.metadata.save_date);
  }

void GameScript::importOriginal(const zenkit::SaveState& st, Npc& hero) {
  uint32_t symbols=0, infos=0;
  for(auto& s:st.symbols) {
    auto* sym = vm.find_symbol_by_name(s.name);
    if(sym==nullptr || sym->is_member() || sym->is_const() || sym->type()!=zenkit::DaedalusDataType::INT)
      continue;
    const size_t n = std::min<size_t>(s.values.size(),sym->count());
    for(size_t i=0; i<n; ++i)
      sym->set_int(s.values[i],uint16_t(i));
    ++symbols;
    }

  dlgKnownInfos.clear();
  for(auto& i:st.infos)
    if(i.told && setInfoKnown(hero,i.name,true))
      ++infos;

  quests = QuestLog();
  for(auto& q:st.log) {
    auto& dst = quests.add(q.description, q.section==zenkit::SaveTopicSection::NOTES ? QuestLog::Note : QuestLog::Mission);
    for(auto& e:q.entries)
      dst.entry.push_back(e);
    switch(q.status) {
      case zenkit::SaveTopicStatus::SUCCESS:  dst.status = QuestLog::Status::Success;  break;
      case zenkit::SaveTopicStatus::FAILURE:  dst.status = QuestLog::Status::Failed;   break;
      case zenkit::SaveTopicStatus::OBSOLETE: dst.status = QuestLog::Status::Obsolete; break;
      default:                                dst.status = QuestLog::Status::Running;  break;
      }
    }

  // the table is square: 42 guilds a row in Gothic 1, 66 in Gothic 2
  const size_t stride = size_t(std::lround(std::sqrt(double(st.guild_table_raw.size()))));
  uint32_t     guilds = 0;
  if(stride>0 && stride*stride==st.guild_table_raw.size()) {
    const size_t n = std::min(stride,gilCount);
    for(size_t r=0; r<n; ++r)
      for(size_t c=0; c<n; ++c) {
        auto& dst = gilAttitudes[r*gilCount+c];
        auto  src = int32_t(st.guild_table_raw[r*stride+c]);
        if(dst!=src)
          ++guilds;
        dst = src;
        }
    } else {
    Tempest::Log::e("Original save: guild table of ",st.guild_table_raw.size()," bytes is not square, it is left as it is");
    }
  Tempest::Log::i("Original save: globals ",symbols,", told infos ",infos,", journal topics ",st.log.size(),
                  ", guild attitudes changed ",guilds);
  }
