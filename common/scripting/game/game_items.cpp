#include "scripting/gameapi.h"
#include "scripting/modutil.h"

#include <algorithm>
#include <cmath>
#include <vector>

#include "game/constants.h"
#include "game/inventory.h"
#include "physics/dynamicworld.h"
#include "world/objects/item.h"
#include "world/objects/npc.h"
#include "world/world.h"
#include "gothic.h"

using namespace Scripting;

namespace {

/// The farthest an item can be to be taken by a mod: the player takes the item where it is (the animation is only
/// for show), so a mod cannot be allowed to pull the items from the other end of the world.
/// 15 m: enough for a whole room of a tower, the walls are checked by `visible` (see below).
constexpr float MaxPickupDistance = 1500.f;

void put(JSContext* ctx, JSValueConst obj, const char* name, double v) {
  JS_SetPropertyStr(ctx,obj,name,JS_NewFloat64(ctx,v));
  }

void put(JSContext* ctx, JSValueConst obj, const char* name, bool v) {
  JS_SetPropertyStr(ctx,obj,name,JS_NewBool(ctx,v));
  }

void put(JSContext* ctx, JSValueConst obj, const char* name, const std::string& v) {
  JS_SetPropertyStr(ctx,obj,name,JS_NewString(ctx,gameToUtf8(v).c_str()));
  }

/// true if nothing of the level (walls, the floor, closed doors) is between the chest of the player and the point
bool lineOfSight(World& world, Npc& pl, const Tempest::Vec3& to) {
  const auto from = pl.centerPosition();
  const auto r    = world.physic()->ray(from,to);
  return !r.hasCol;
  }

bool lineOfSight(World& world, Npc& pl, Item& it) {
  // the middle of the item can be inside of the shelf or the table that it lies on, so a few points are tried
  const auto mid = it.midPosition();
  if(lineOfSight(world,pl,mid))
    return true;
  const auto p = it.position();
  if(lineOfSight(world,pl,Tempest::Vec3(p.x,p.y+20.f,p.z)))
    return true;
  return lineOfSight(world,pl,Tempest::Vec3(p.x,p.y+45.f,p.z));
  }

/// items.nearby(radius): the items that lie on the ground around the player, the nearest first.
/// [{index, instance, instanceId, name, count, x, y, z, distance, gold, mission, owned, flags, visible}, ...]
/// `visible` is false if a wall or the floor is between the player and the item (no taking through the walls).
/// `index` is only good for items.pickup() in the same call of the script: it changes when an item is taken.
JSValue nearby(JSContext* ctx, JSValueConst, int argc, JSValueConst* argv) {
  JSValue arr   = JS_NewArray(ctx);
  auto*   world = Gothic::inst().world();
  auto*   pl    = Gothic::inst().player();
  if(world==nullptr || pl==nullptr)
    return arr;

  double radius = 300;
  if(argc>0)
    JS_ToFloat64(ctx,&radius,argv[0]);
  radius = std::clamp(radius,0.0,double(MaxPickupDistance));

  struct Found {
    uint32_t index;
    Item*    item;
    float    dist2;
    };

  const auto         pos = pl->position();
  const float        r2  = float(radius*radius);
  std::vector<Found> found;
  for(uint32_t i=0; ; ++i) {
    Item* it = world->itmById(i);
    if(it==nullptr)
      break;
    const float d2 = (it->position()-pos).quadLength();
    if(d2<=r2)
      found.push_back({i,it,d2});
    }
  std::sort(found.begin(),found.end(),[](const Found& a, const Found& b) { return a.dist2<b.dist2; });

  uint32_t n = 0;
  for(auto& f:found) {
    Item& it = *f.item;
    JSValue o = JS_NewObject(ctx);
    const auto p = it.position();
    put(ctx,o,"index",     double(f.index));
    put(ctx,o,"instance",  game::instanceName(*world,it.clsId()));
    put(ctx,o,"instanceId",double(it.clsId()));
    put(ctx,o,"name",      std::string(it.displayName()));
    put(ctx,o,"count",     double(it.count()));
    put(ctx,o,"x",         double(p.x));
    put(ctx,o,"y",         double(p.y));
    put(ctx,o,"z",         double(p.z));
    put(ctx,o,"distance",  double(std::sqrt(f.dist2)));
    put(ctx,o,"gold",      it.isGold());
    put(ctx,o,"mission",   it.isMission());
    // an item of somebody: taking it is a theft, the characters around may see it
    put(ctx,o,"owned",     it.handle().owner!=0 || it.handle().owner_guild!=0);
    put(ctx,o,"flags",     double(uint32_t(it.itemFlag())));
    put(ctx,o,"visible",   lineOfSight(*world,*pl,it));
    JS_SetPropertyUint32(ctx,arr,n++,o);
    }
  return arr;
  }

/// items.pickup(index, animate=true): the player takes the item, the same way as with the action key (the animation,
/// the theft that the characters can see). true if the item is taken. It is not taken in a talk, in the pause, when the
/// player is down or busy, or when the item is farther than 15 m. With `animate` false the player does not stop and
/// does not play the animation: the item just goes to the inventory (the player can walk or run).
JSValue pickup(JSContext* ctx, JSValueConst, int argc, JSValueConst* argv) {
  auto* world = Gothic::inst().world();
  auto* pl    = Gothic::inst().player();
  uint32_t index = 0;
  if(world==nullptr || pl==nullptr || argc<1 || JS_ToUint32(ctx,&index,argv[0])!=0)
    return JS_FALSE;

  Item* it = world->itmById(index);
  if(it==nullptr)
    return JS_FALSE;
  if(Gothic::inst().isPause() || Gothic::inst().isInDialog() || pl->isDown())
    return JS_FALSE;
  if((it->position()-pl->position()).quadLength()>MaxPickupDistance*MaxPickupDistance)
    return JS_FALSE;
  bool animate = true;
  if(argc>1)
    animate = JS_ToBool(ctx,argv[1])>0;
  return JS_NewBool(ctx,pl->takeItem(*it,animate)!=nullptr);
  }

/// items.corpses(radius, unconscious=false): the dead characters around the player (and the unconscious ones, if asked)
/// with the things that the game lets to take from them (the same as searching a body: what the character wears and
/// holds is not in the list). The nearest first.
/// [{id, name, distance, visible, loot: [{instanceId, instance, name, count, flags, gold, mission}, ...]}, ...]
JSValue corpses(JSContext* ctx, JSValueConst, int argc, JSValueConst* argv) {
  JSValue arr   = JS_NewArray(ctx);
  auto*   world = Gothic::inst().world();
  auto*   pl    = Gothic::inst().player();
  if(world==nullptr || pl==nullptr)
    return arr;

  double radius = 300;
  if(argc>0)
    JS_ToFloat64(ctx,&radius,argv[0]);
  radius = std::clamp(radius,0.0,double(MaxPickupDistance));
  bool unconscious = false;
  if(argc>1)
    unconscious = JS_ToBool(ctx,argv[1])>0;

  const auto        pos = pl->position();
  std::vector<Npc*> found;
  world->detectNpc(pos,float(radius),[&](Npc& n) {
    if(&n==pl || n.isPlayer())
      return;
    if(n.isDead() || (unconscious && n.isUnconscious()))
      found.push_back(&n);
    });
  std::sort(found.begin(),found.end(),[&](Npc* a, Npc* b) {
    return (a->position()-pos).quadLength()<(b->position()-pos).quadLength();
    });

  uint32_t n = 0;
  for(auto* npc:found) {
    auto it = npc->inventory().iterator(Inventory::T_Ransack);
    if(!it.isValid())
      continue;

    JSValue loot = JS_NewArray(ctx);
    uint32_t k = 0;
    for(; it.isValid(); ++it) {
      const Item& itm = *it;
      JSValue o = JS_NewObject(ctx);
      put(ctx,o,"instanceId",double(itm.clsId()));
      put(ctx,o,"instance",  game::instanceName(*world,itm.clsId()));
      put(ctx,o,"name",      std::string(itm.displayName()));
      put(ctx,o,"count",     double(it.count()));
      put(ctx,o,"flags",     double(uint32_t(itm.itemFlag())));
      put(ctx,o,"gold",      itm.isGold());
      put(ctx,o,"mission",   itm.isMission());
      JS_SetPropertyUint32(ctx,loot,k++,o);
      }
    if(k==0) {
      JS_FreeValue(ctx,loot);
      continue;
      }

    JSValue o = JS_NewObject(ctx);
    put(ctx,o,"id",       double(npc->uid()));
    put(ctx,o,"name",     std::string(npc->displayName()));
    put(ctx,o,"distance", double((npc->position()-pos).length()));
    put(ctx,o,"x",        double(npc->centerPosition().x));
    put(ctx,o,"y",        double(npc->centerPosition().y));
    put(ctx,o,"z",        double(npc->centerPosition().z));
    put(ctx,o,"visible",  lineOfSight(*world,*pl,npc->centerPosition()));
    JS_SetPropertyStr(ctx,o,"loot",loot);
    JS_SetPropertyUint32(ctx,arr,n++,o);
    }
  return arr;
  }

/// items.lootCorpse(id, instanceId): the player takes all the items of an instance from a dead (or unconscious)
/// character, with no animation. true if something is taken. The same limits as items.pickup.
JSValue lootCorpse(JSContext* ctx, JSValueConst, int argc, JSValueConst* argv) {
  auto* world = Gothic::inst().world();
  auto* pl    = Gothic::inst().player();
  uint32_t id = 0, cls = 0;
  if(world==nullptr || pl==nullptr || argc<2 || JS_ToUint32(ctx,&id,argv[0])!=0 || JS_ToUint32(ctx,&cls,argv[1])!=0)
    return JS_FALSE;

  Npc* npc = world->npcByUid(id);
  if(npc==nullptr || npc==pl || !npc->isDown())
    return JS_FALSE;
  if(Gothic::inst().isPause() || Gothic::inst().isInDialog() || pl->isDown())
    return JS_FALSE;
  if((npc->position()-pl->position()).quadLength()>MaxPickupDistance*MaxPickupDistance)
    return JS_FALSE;

  // only what the game shows when the body is searched: not what the body wears
  size_t count = 0;
  for(auto it=npc->inventory().iterator(Inventory::T_Ransack); it.isValid(); ++it)
    if(it->clsId()==cls)
      count += it.count();
  if(count==0)
    return JS_FALSE;

  pl->addItem(cls,*npc,count);
  return JS_TRUE;
  }

/// items.count(): how many items lie in the world
JSValue count(JSContext* ctx, JSValueConst, int, JSValueConst*) {
  auto* world = Gothic::inst().world();
  uint32_t n = 0;
  if(world!=nullptr)
    while(world->itmById(n)!=nullptr)
      ++n;
  return JS_NewUint32(ctx,n);
  }

}

/// items: the items that lie in the world
void game::installItems(JSContext* ctx) {
  JSValue global = JS_GetGlobalObject(ctx);
  JSValue items  = JS_NewObject(ctx);
  js::fn(ctx,items,"nearby",    nearby,    1);
  js::fn(ctx,items,"pickup",    pickup,    2);
  js::fn(ctx,items,"count",     count,     0);
  js::fn(ctx,items,"corpses",   corpses,   2);
  js::fn(ctx,items,"lootCorpse",lootCorpse,2);
  JS_SetPropertyStr(ctx,global,"items",items);
  JS_FreeValue(ctx,global);
  }
