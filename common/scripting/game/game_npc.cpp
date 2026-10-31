#include "scripting/gameapi.h"
#include "scripting/modutil.h"

#include "game/constants.h"
#include "game/gamescript.h"
#include "game/globaleffects.h"
#include "gothic.h"
#include "world/objects/npc.h"
#include "world/world.h"

using namespace Scripting;

/// A character as the scripts see it: name, health, position, items. The object keeps only the number of the character,
/// so it is safe to keep it for as long as the mod wants.

namespace {

enum Prop : int {
  Valid, Id, Name, Instance, Level, Health, MaxHealth, Mana, MaxMana, Strength, Dexterity, Experience, Dead, GuildOf,
  IsPlayer, X, Y, Z, Position, Rotation, Puppet, InstanceId, WeaponState_, WeaponItem, Overlays, Armor, RangedItem, Unconscious, Talking, RangedShots, CompatibilityState
  };

constexpr const char* HiddenId    = "__id";
constexpr const char* HiddenProto = "__npcProto";

Npc* self(JSContext* ctx, JSValueConst obj) {
  return game::resolveNpc(ctx,obj);
  }

JSValue get(JSContext* ctx, JSValueConst obj, int prop) {
  Npc* npc = self(ctx,obj);
  if(prop==Valid)
    return JS_NewBool(ctx,npc!=nullptr);
  if(prop==Id) {
    JSValue v = JS_GetPropertyStr(ctx,obj,HiddenId);
    return v;
    }
  if(npc==nullptr)
    return JS_UNDEFINED;

  switch(prop) {
    case Name:       return JS_NewString(ctx,gameToUtf8(npc->displayName()).c_str());
    case Instance:   return JS_NewString(ctx,game::instanceName(npc->world(),npc->instanceSymbol()).c_str());
    case Level:      return JS_NewInt32(ctx,npc->level());
    case Health:     return JS_NewInt32(ctx,npc->attribute(ATR_HITPOINTS));
    case MaxHealth:  return JS_NewInt32(ctx,npc->attribute(ATR_HITPOINTSMAX));
    case Mana:       return JS_NewInt32(ctx,npc->attribute(ATR_MANA));
    case MaxMana:    return JS_NewInt32(ctx,npc->attribute(ATR_MANAMAX));
    case Strength:   return JS_NewInt32(ctx,npc->attribute(ATR_STRENGTH));
    case Dexterity:  return JS_NewInt32(ctx,npc->attribute(ATR_DEXTERITY));
    case Experience: return JS_NewInt32(ctx,npc->experience());
    case Dead:       return JS_NewBool(ctx,npc->isDead());
    case GuildOf:      return JS_NewUint32(ctx,npc->guild());
    case IsPlayer:   return JS_NewBool(ctx,npc->isPlayer());
    case Rotation:   return JS_NewFloat64(ctx,double(npc->rotation()));
    case Puppet:     return JS_NewBool(ctx,npc->isPuppet());
    case InstanceId: return JS_NewUint32(ctx,npc->instanceSymbol());
    case Overlays: {
      JSValue arr = JS_NewArray(ctx);
      uint32_t n  = 0;
      for(auto& s:npc->overlayNames())
        JS_SetPropertyUint32(ctx,arr,n++,JS_NewString(ctx,s.c_str()));
      return arr;
      }
    case Armor: {
      const size_t it = npc->armorInstance();
      return JS_NewString(ctx,it==size_t(-1) ? "" : game::instanceName(npc->world(),it).c_str());
      }
    case RangedItem: {
      const size_t it = npc->rangedWeaponInstance();
      return JS_NewString(ctx,it==size_t(-1) ? "" : game::instanceName(npc->world(),it).c_str());
      }
    case Unconscious: return JS_NewBool(ctx,npc->isUnconscious());
    case Talking:     return JS_NewBool(ctx,npc->isTalk());
    case RangedShots:  return JS_NewUint32(ctx,npc->rangedShotCount());
    case CompatibilityState: {
      JSValue state = JS_NewObject(ctx);
      JS_SetPropertyStr(ctx,state,"diving",JS_NewBool(ctx,npc->isDive()));
      JS_SetPropertyStr(ctx,state,"diveElapsedMs",JS_NewInt32(ctx,npc->diveTime()));
      JS_SetPropertyStr(ctx,state,"diveLimitMs",JS_NewInt32(ctx,npc->diveTimeLimit()));
      auto* effects = npc->world().globalFx();
      JS_SetPropertyStr(ctx,state,"requestedModelScale",JS_NewFloat64(ctx,effects==nullptr ? 1.0 : double(effects->modelTimeScale(*npc))));
      return state;
      }
    case WeaponState_: return JS_NewInt32(ctx,int32_t(npc->weaponState()));
    case WeaponItem: {
      const size_t it = npc->meleeWeaponInstance();
      return JS_NewString(ctx,it==size_t(-1) ? "" : game::instanceName(npc->world(),it).c_str());
      }
    case X:          return JS_NewFloat64(ctx,double(npc->position().x));
    case Y:          return JS_NewFloat64(ctx,double(npc->position().y));
    case Z:          return JS_NewFloat64(ctx,double(npc->position().z));
    case Position: {
      JSValue p = JS_NewObject(ctx);
      const auto pos = npc->position();
      JS_SetPropertyStr(ctx,p,"x",JS_NewFloat64(ctx,double(pos.x)));
      JS_SetPropertyStr(ctx,p,"y",JS_NewFloat64(ctx,double(pos.y)));
      JS_SetPropertyStr(ctx,p,"z",JS_NewFloat64(ctx,double(pos.z)));
      return p;
      }
    default: break;
    }
  return JS_UNDEFINED;
  }

JSValue number(JSContext* ctx, JSValueConst v, double& out) {
  if(JS_ToFloat64(ctx,&out,v)<0)
    return JS_EXCEPTION;
  return JS_UNDEFINED;
  }

// --- health, mana
JSValue setHealth(JSContext* ctx, JSValueConst obj, int argc, JSValueConst* argv) {
  Npc* npc = self(ctx,obj);
  double v = 0;
  if(npc==nullptr || argc<1 || JS_IsException(number(ctx,argv[0],v)))
    return JS_UNDEFINED;
  const int32_t max   = npc->attribute(ATR_HITPOINTSMAX);
  const int32_t value = std::max<int32_t>(0,std::min<int32_t>(max,int32_t(v)));
  if(npc->isPuppet()) {
    npc->puppetSetHealth(value);
    return JS_UNDEFINED;
    }
  npc->changeAttribute(ATR_HITPOINTS,value-npc->attribute(ATR_HITPOINTS),true);
  return JS_UNDEFINED;
  }

/// takeDamage(): the damage that a puppet has got since the last call. A puppet does not lose health by itself,
/// the owner of the character is told about it.
JSValue takeDamage(JSContext* ctx, JSValueConst obj, int, JSValueConst*) {
  Npc* npc = self(ctx,obj);
  return JS_NewInt32(ctx,npc==nullptr ? 0 : npc->takePuppetDamage());
  }

/// heal(n) adds, heal(-n) hurts
JSValue heal(JSContext* ctx, JSValueConst obj, int argc, JSValueConst* argv) {
  Npc* npc = self(ctx,obj);
  double v = 0;
  if(npc==nullptr || argc<1 || JS_IsException(number(ctx,argv[0],v)))
    return JS_UNDEFINED;
  npc->changeAttribute(ATR_HITPOINTS,int32_t(v),true);
  return JS_UNDEFINED;
  }

JSValue setMana(JSContext* ctx, JSValueConst obj, int argc, JSValueConst* argv) {
  Npc* npc = self(ctx,obj);
  double v = 0;
  if(npc==nullptr || argc<1 || JS_IsException(number(ctx,argv[0],v)))
    return JS_UNDEFINED;
  const int32_t max   = npc->attribute(ATR_MANAMAX);
  const int32_t value = std::max<int32_t>(0,std::min<int32_t>(max,int32_t(v)));
  npc->changeAttribute(ATR_MANA,value-npc->attribute(ATR_MANA),true);
  return JS_UNDEFINED;
  }

JSValue kill(JSContext* ctx, JSValueConst obj, int, JSValueConst*) {
  Npc* npc = self(ctx,obj);
  if(npc!=nullptr && !npc->isDead()) {
    if(npc->isPuppet()) {
      npc->puppetSetHealth(0);
      return JS_UNDEFINED;
      }
    npc->changeAttribute(ATR_HITPOINTS,-npc->attribute(ATR_HITPOINTS),true);
    }
  return JS_UNDEFINED;
  }

// --- place
JSValue setPosition(JSContext* ctx, JSValueConst obj, int argc, JSValueConst* argv) {
  Npc* npc = self(ctx,obj);
  if(npc==nullptr || argc<3)
    return JS_FALSE;
  double x = 0, y = 0, z = 0;
  if(JS_IsException(number(ctx,argv[0],x)) || JS_IsException(number(ctx,argv[1],y)) || JS_IsException(number(ctx,argv[2],z)))
    return JS_FALSE;
  // Npc::setPosition returns whether it moved, not whether the request succeeded.
  // Being at the requested position already is a successful script operation.
  npc->setPosition(float(x),float(y),float(z));
  return JS_TRUE;
  }

/// animation(): what the character plays, [[sequence, ms since it has started, comb, body state], ...]
JSValue animation(JSContext* ctx, JSValueConst obj, int, JSValueConst*) {
  Npc* npc = self(ctx,obj);
  if(npc==nullptr)
    return JS_NewArray(ctx);
  return game::animationOf(ctx,*npc);
  }

/// setAnimation(layers): a puppet plays what the owner of the character plays, the layers are of animation().
/// An empty array gives the animation back to the walking by the movement. Returns how many of the sequences are known.
JSValue setAnimation(JSContext* ctx, JSValueConst obj, int argc, JSValueConst* argv) {
  Npc* npc = self(ctx,obj);
  if(npc==nullptr || argc<1 || !JS_IsArray(argv[0]))
    return JS_NewInt32(ctx,0);

  JSValue   len = JS_GetPropertyStr(ctx,argv[0],"length");
  uint32_t  n   = 0;
  JS_ToUint32(ctx,&n,len);
  JS_FreeValue(ctx,len);

  std::vector<Npc::AnimLayer> layers;
  for(uint32_t i=0; i<n && i<16; ++i) {
    JSValue e = JS_GetPropertyUint32(ctx,argv[0],i);
    if(JS_IsArray(e)) {
      Npc::AnimLayer l;
      JSValue name = JS_GetPropertyUint32(ctx,e,0);
      l.name = js::str(ctx,name);
      JS_FreeValue(ctx,name);
      uint32_t v[3] = {};
      for(uint32_t k=0; k<3; ++k) {
        JSValue p = JS_GetPropertyUint32(ctx,e,k+1);
        JS_ToUint32(ctx,&v[k],p);
        JS_FreeValue(ctx,p);
        }
      l.elapsed = v[0];
      l.comb    = uint8_t(std::min<uint32_t>(v[1],255));
      l.bs      = v[2];
      layers.push_back(std::move(l));
      }
    JS_FreeValue(ctx,e);
    }
  return JS_NewInt32(ctx,int32_t(npc->setPuppetAnimation(layers)));
  }

/// hurtBy(attacker, damage): a character of this world has been hit by a player of another game: it reacts as to the
/// hit of a player (turns to the attacker, the script decides what to do), and the attacker is its character here
JSValue hurtBy(JSContext* ctx, JSValueConst obj, int argc, JSValueConst* argv) {
  Npc*   npc = self(ctx,obj);
  double d   = 0;
  if(npc==nullptr || argc<2 || JS_ToFloat64(ctx,&d,argv[1])<0)
    return JS_UNDEFINED;
  Npc* attacker = self(ctx,argv[0]);
  if(attacker==nullptr || attacker==npc || d<=0)
    return JS_UNDEFINED;
  npc->hurtBy(*attacker,int32_t(d));
  return JS_UNDEFINED;
  }

/// setEquipment(melee, ranged): a puppet carries the weapons of its owner, also the ones that are not drawn
JSValue setEquipment(JSContext* ctx, JSValueConst obj, int argc, JSValueConst* argv) {
  Npc* npc = self(ctx,obj);
  if(npc==nullptr || argc<1)
    return JS_UNDEFINED;
  size_t item[2] = {size_t(-1),size_t(-1)};
  for(int i=0; i<2 && i<argc; ++i) {
    if(!JS_IsString(argv[i]))
      continue;
    const auto name = js::str(ctx,argv[i]);
    if(!name.empty())
      item[i] = game::findInstance(npc->world(),name);
    }
  npc->setPuppetEquipment(item[0],item[1]);
  return JS_UNDEFINED;
  }

/// takeInteraction(): true if a player has started to talk to the puppet since the last call. The mod gives the
/// character back to the game (setPuppet(false)), the talk is made by its script.
JSValue takeInteraction(JSContext* ctx, JSValueConst obj, int, JSValueConst*) {
  Npc* npc = self(ctx,obj);
  return JS_NewBool(ctx,npc!=nullptr && npc->takePuppetInteraction());
  }

/// takeFinish(): true if a player has finished off the unconscious puppet since the last call
JSValue takeFinish(JSContext* ctx, JSValueConst obj, int, JSValueConst*) {
  Npc* npc = self(ctx,obj);
  return JS_NewBool(ctx,npc!=nullptr && npc->takePuppetFinish());
  }

/// finish(): finishes off an unconscious character
JSValue finish(JSContext* ctx, JSValueConst obj, int, JSValueConst*) {
  Npc* npc = self(ctx,obj);
  if(npc!=nullptr && !npc->isPlayer())
    npc->finishOff();
  return JS_UNDEFINED;
  }

/// setUnconscious(on): a puppet is unconscious when its owner is
JSValue setUnconscious(JSContext* ctx, JSValueConst obj, int argc, JSValueConst* argv) {
  Npc* npc = self(ctx,obj);
  if(npc!=nullptr && argc>0)
    npc->setPuppetUnconscious(JS_ToBool(ctx,argv[0])>0);
  return JS_UNDEFINED;
  }

/// setOverlays(names): a puppet has the overlays (mds) of its owner, the others are taken off
JSValue setOverlays(JSContext* ctx, JSValueConst obj, int argc, JSValueConst* argv) {
  Npc* npc = self(ctx,obj);
  if(npc==nullptr || argc<1 || !JS_IsArray(argv[0]))
    return JS_UNDEFINED;
  JSValue  len = JS_GetPropertyStr(ctx,argv[0],"length");
  uint32_t n   = 0;
  JS_ToUint32(ctx,&n,len);
  JS_FreeValue(ctx,len);

  std::vector<std::string> names;
  for(uint32_t i=0; i<n && i<32; ++i) {
    JSValue e = JS_GetPropertyUint32(ctx,argv[0],i);
    names.push_back(js::str(ctx,e));
    JS_FreeValue(ctx,e);
    }
  npc->setPuppetOverlays(names);
  return JS_UNDEFINED;
  }

/// setArmor(instance): a puppet wears the armor of its owner, "" takes it off
JSValue setArmor(JSContext* ctx, JSValueConst obj, int argc, JSValueConst* argv) {
  Npc* npc = self(ctx,obj);
  if(npc==nullptr || argc<1)
    return JS_UNDEFINED;
  const auto name = js::str(ctx,argv[0]);
  size_t item = size_t(-1);
  if(!name.empty()) {
    item = game::findInstance(npc->world(),name);
    if(item==size_t(-1))
      return JS_UNDEFINED;
    }
  npc->setPuppetArmor(item);
  return JS_UNDEFINED;
  }

/// setWeapon(state, item): a puppet holds the weapon that the owner holds, state is weaponState, item the instance of the melee weapon
JSValue setWeapon(JSContext* ctx, JSValueConst obj, int argc, JSValueConst* argv) {
  Npc* npc = self(ctx,obj);
  int32_t ws = 0;
  if(npc==nullptr || argc<1 || JS_ToInt32(ctx,&ws,argv[0])<0 || ws<0 || ws>int32_t(WeaponState::Mage))
    return JS_UNDEFINED;
  size_t item = size_t(-1);
  if(argc>1 && JS_IsString(argv[1])) {
    const auto name = js::str(ctx,argv[1]);
    if(!name.empty())
      item = game::findInstance(npc->world(),name);
    }
  npc->setPuppetWeapon(WeaponState(ws),item);
  return JS_UNDEFINED;
  }

/// setRotation(degrees): where the character looks
JSValue setRotation(JSContext* ctx, JSValueConst obj, int argc, JSValueConst* argv) {
  Npc*   npc = self(ctx,obj);
  double a   = 0;
  if(npc==nullptr || argc<1 || JS_IsException(number(ctx,argv[0],a)))
    return JS_UNDEFINED;
  npc->setDirection(float(a));
  return JS_UNDEFINED;
  }

/// setPuppet(true): the character is moved by the mod, see Npc::setPuppet. The player cannot be a puppet.
JSValue setPuppet(JSContext* ctx, JSValueConst obj, int argc, JSValueConst* argv) {
  Npc* npc = self(ctx,obj);
  if(npc==nullptr || npc->isPlayer() || argc<1)
    return JS_UNDEFINED;
  npc->setPuppet(JS_ToBool(ctx,argv[0])>0);
  return JS_UNDEFINED;
  }

/// remove(): takes the character out of the world. The player cannot be removed.
JSValue remove(JSContext* ctx, JSValueConst obj, int, JSValueConst*) {
  Npc* npc = self(ctx,obj);
  if(npc==nullptr || npc->isPlayer())
    return JS_FALSE;
  npc->world().removeNpc(*npc);
  return JS_TRUE;
  }

JSValue teleport(JSContext* ctx, JSValueConst obj, int argc, JSValueConst* argv) {
  Npc* npc = self(ctx,obj);
  if(npc==nullptr || argc<1)
    return JS_FALSE;
  const auto name = js::str(ctx,argv[0]);
  auto*      wp   = npc->world().findPoint(name,false);
  if(wp==nullptr)
    return JS_FALSE;
  npc->setPosition(wp->position());
  return JS_TRUE;
  }

// --- items
JSValue addItem(JSContext* ctx, JSValueConst obj, int argc, JSValueConst* argv) {
  Npc* npc = self(ctx,obj);
  if(npc==nullptr || argc<1)
    return JS_FALSE;

  const auto name = js::str(ctx,argv[0]);
  const auto id   = game::findInstance(npc->world(),name);
  double     n    = 1;
  if(argc>1)
    JS_ToFloat64(ctx,&n,argv[1]);
  if(id==size_t(-1) || n<1)
    return JS_FALSE;
  npc->addItem(id,size_t(n));
  return JS_TRUE;
  }

/// npc.aiVar(index) / npc.setAiVar(index, value): the AIVAR of the script (AIV_PARTYMEMBER is 15)
JSValue aiVar(JSContext* ctx, JSValueConst obj, int argc, JSValueConst* argv) {
  Npc* npc = self(ctx,obj);
  int32_t i = -1;
  if(npc==nullptr || argc<1 || JS_ToInt32(ctx,&i,argv[0])!=0 || i<0 || i>=int32_t(zenkit::INpc::aivar_count))
    return JS_UNDEFINED;
  return JS_NewInt32(ctx,npc->handle().aivar[i]);
  }

JSValue setAiVar(JSContext* ctx, JSValueConst obj, int argc, JSValueConst* argv) {
  Npc* npc = self(ctx,obj);
  int32_t i = -1, v = 0;
  if(npc==nullptr || argc<2 || JS_ToInt32(ctx,&i,argv[0])!=0 || JS_ToInt32(ctx,&v,argv[1])!=0 ||
     i<0 || i>=int32_t(zenkit::INpc::aivar_count))
    return JS_FALSE;
  npc->handle().aivar[i] = v;
  return JS_TRUE;
  }

/// npc.flags() / npc.setFlags(value): NPC_FLAG_* of the script (FRIEND 1, IMMORTAL 2, GHOST 4, PROTECTED 8, ...)
JSValue flags(JSContext* ctx, JSValueConst obj, int, JSValueConst*) {
  Npc* npc = self(ctx,obj);
  return npc==nullptr ? JS_UNDEFINED : JS_NewInt32(ctx,int32_t(npc->handle().flags));
  }

JSValue setFlags(JSContext* ctx, JSValueConst obj, int argc, JSValueConst* argv) {
  Npc* npc = self(ctx,obj);
  int32_t v = 0;
  if(npc==nullptr || argc<1 || JS_ToInt32(ctx,&v,argv[0])!=0)
    return JS_FALSE;
  npc->handle().flags = zenkit::NpcFlag(v);
  return JS_TRUE;
  }

/// npc.openTrade(trader): the trade window of the player with a character, no dialog is needed
JSValue openTrade(JSContext* ctx, JSValueConst obj, int argc, JSValueConst* argv) {
  Npc* npc    = self(ctx,obj);
  Npc* trader = argc>0 ? game::resolveNpc(ctx,argv[0]) : nullptr;
  if(npc==nullptr || trader==nullptr || npc==trader || !npc->isPlayer())
    return JS_FALSE;
  Gothic::inst().onOpenTrade(*npc,*trader);
  return JS_TRUE;
  }

JSValue removeItem(JSContext* ctx, JSValueConst obj, int argc, JSValueConst* argv) {
  Npc* npc = self(ctx,obj);
  if(npc==nullptr || argc<1)
    return JS_FALSE;

  const auto id = game::findInstance(npc->world(),js::str(ctx,argv[0]));
  double     n  = 1;
  if(argc>1)
    JS_ToFloat64(ctx,&n,argv[1]);
  if(id==size_t(-1) || n<1)
    return JS_FALSE;
  const size_t have = npc->inventory().itemCount(id);
  npc->delItem(id,uint32_t(std::min(have,size_t(n))));
  return JS_TRUE;
  }

JSValue itemCount(JSContext* ctx, JSValueConst obj, int argc, JSValueConst* argv) {
  Npc* npc = self(ctx,obj);
  if(npc==nullptr || argc<1)
    return JS_NewInt32(ctx,0);
  const auto id = game::findInstance(npc->world(),js::str(ctx,argv[0]));
  if(id==size_t(-1))
    return JS_NewInt32(ctx,0);
  return JS_NewInt64(ctx,int64_t(npc->inventory().itemCount(id)));
  }

JSValue hasItem(JSContext* ctx, JSValueConst obj, int argc, JSValueConst* argv) {
  JSValue n = itemCount(ctx,obj,argc,argv);
  int32_t c = 0;
  JS_ToInt32(ctx,&c,n);
  JS_FreeValue(ctx,n);
  return JS_NewBool(ctx,c>0);
  }

void getter(JSContext* ctx, JSValueConst proto, const char* name, Prop prop) {
  JSAtom a = JS_NewAtom(ctx,name);
  JSValue g = JS_NewCFunction2(ctx,reinterpret_cast<JSCFunction*>(&get),name,0,JS_CFUNC_getter_magic,prop);
  JS_DefinePropertyGetSet(ctx,proto,a,g,JS_UNDEFINED,JS_PROP_C_W_E);
  JS_FreeAtom(ctx,a);
  }

}

Npc* game::resolveNpc(JSContext* ctx, JSValueConst obj) {
  JSValue v  = JS_GetPropertyStr(ctx,obj,HiddenId);
  uint32_t id = 0;
  const bool ok = JS_IsNumber(v) && JS_ToUint32(ctx,&id,v)==0;
  JS_FreeValue(ctx,v);
  if(!ok)
    return nullptr;

  if(id==PlayerId)
    return Gothic::inst().player();
  auto* world = Gothic::inst().world();
  return world!=nullptr ? world->npcByUid(id) : nullptr;
  }

JSValue game::animationOf(JSContext* ctx, Npc& npc) {
  JSValue arr = JS_NewArray(ctx);
  uint32_t n  = 0;
  for(auto& l:npc.animationLayers()) {
    JSValue e = JS_NewArray(ctx);
    JS_SetPropertyUint32(ctx,e,0,JS_NewString(ctx,l.name.c_str()));
    JS_SetPropertyUint32(ctx,e,1,JS_NewUint32(ctx,l.elapsed));
    JS_SetPropertyUint32(ctx,e,2,JS_NewUint32(ctx,l.comb));
    JS_SetPropertyUint32(ctx,e,3,JS_NewUint32(ctx,l.bs));
    JS_SetPropertyUint32(ctx,arr,n++,e);
    }
  return arr;
  }

JSValue game::wrapNpc(JSContext* ctx, Npc& npc) {
  JSValue global = JS_GetGlobalObject(ctx);
  JSValue proto  = JS_GetPropertyStr(ctx,global,HiddenProto);
  JS_FreeValue(ctx,global);
  if(!JS_IsObject(proto)) {
    JS_FreeValue(ctx,proto);
    return JS_UNDEFINED;
    }

  const uint32_t id = npc.isPlayer() ? PlayerId : npc.uid();

  JSValue obj = JS_NewObjectProto(ctx,proto);
  JS_FreeValue(ctx,proto);
  JS_DefinePropertyValueStr(ctx,obj,HiddenId,JS_NewUint32(ctx,id),0);
  return obj;
  }

/// The prototype that every character has. Put in the global object, but hidden from the scripts.
void game::installNpc(JSContext* ctx) {
  JSValue proto = JS_NewObject(ctx);

  getter(ctx,proto,"valid",     Valid);
  getter(ctx,proto,"id",        Id);
  getter(ctx,proto,"name",      Name);
  getter(ctx,proto,"instance",  Instance);
  getter(ctx,proto,"level",     Level);
  getter(ctx,proto,"health",    Health);
  getter(ctx,proto,"maxHealth", MaxHealth);
  getter(ctx,proto,"mana",      Mana);
  getter(ctx,proto,"maxMana",   MaxMana);
  getter(ctx,proto,"strength",  Strength);
  getter(ctx,proto,"dexterity", Dexterity);
  getter(ctx,proto,"experience",Experience);
  getter(ctx,proto,"dead",      Dead);
  getter(ctx,proto,"guild",     GuildOf);
  getter(ctx,proto,"isPlayer",  IsPlayer);
  getter(ctx,proto,"x",         X);
  getter(ctx,proto,"y",         Y);
  getter(ctx,proto,"z",         Z);
  getter(ctx,proto,"position",  Position);
  getter(ctx,proto,"rotation",  Rotation);
  getter(ctx,proto,"puppet",    Puppet);
  getter(ctx,proto,"instanceId",InstanceId);
  getter(ctx,proto,"weaponState",WeaponState_);
  getter(ctx,proto,"weaponItem", WeaponItem);
  getter(ctx,proto,"overlays",   Overlays);
  getter(ctx,proto,"armor",      Armor);
  getter(ctx,proto,"rangedItem", RangedItem);
  getter(ctx,proto,"unconscious",Unconscious);
  getter(ctx,proto,"talking",    Talking);
  getter(ctx,proto,"rangedShots",RangedShots);
  getter(ctx,proto,"compatibilityState",CompatibilityState);

  js::fn(ctx,proto,"setHealth", setHealth,1);
  js::fn(ctx,proto,"heal",      heal,1);
  js::fn(ctx,proto,"setMana",   setMana,1);
  js::fn(ctx,proto,"kill",      kill,0);
  js::fn(ctx,proto,"setPosition",setPosition,3);
  js::fn(ctx,proto,"teleport",  teleport,1);
  js::fn(ctx,proto,"setRotation",setRotation,1);
  js::fn(ctx,proto,"setPuppet", setPuppet,1);
  js::fn(ctx,proto,"takeDamage",takeDamage,0);
  js::fn(ctx,proto,"animation", animation,0);
  js::fn(ctx,proto,"setAnimation",setAnimation,1);
  js::fn(ctx,proto,"setWeapon", setWeapon,2);
  js::fn(ctx,proto,"setOverlays",setOverlays,1);
  js::fn(ctx,proto,"setArmor",  setArmor,1);
  js::fn(ctx,proto,"setEquipment",setEquipment,2);
  js::fn(ctx,proto,"hurtBy",    hurtBy,2);
  js::fn(ctx,proto,"takeInteraction",takeInteraction,0);
  js::fn(ctx,proto,"takeFinish",takeFinish,0);
  js::fn(ctx,proto,"finish",    finish,0);
  js::fn(ctx,proto,"setUnconscious",setUnconscious,1);
  js::fn(ctx,proto,"remove",    remove,0);
  js::fn(ctx,proto,"addItem",   addItem,2);
  js::fn(ctx,proto,"removeItem",removeItem,2);
  js::fn(ctx,proto,"openTrade", openTrade,1);
  js::fn(ctx,proto,"flags",     flags,0);
  js::fn(ctx,proto,"setFlags",  setFlags,1);
  js::fn(ctx,proto,"aiVar",     aiVar,1);
  js::fn(ctx,proto,"setAiVar",  setAiVar,2);
  js::fn(ctx,proto,"itemCount", itemCount,1);
  js::fn(ctx,proto,"hasItem",   hasItem,1);

  JSValue global = JS_GetGlobalObject(ctx);
  JS_DefinePropertyValueStr(ctx,global,HiddenProto,proto,0);
  JS_FreeValue(ctx,global);
  }
