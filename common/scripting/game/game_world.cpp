#include "scripting/gameapi.h"

#include <algorithm>
#include <cctype>
#include <map>

#include "game/gametime.h"
#include "gothic.h"
#include "world/objects/item.h"
#include "world/objects/npc.h"
#include "world/waypoint.h"
#include "world/world.h"

using namespace Scripting;

namespace {

JSValue name(JSContext* ctx, JSValueConst, int, JSValueConst*) {
  auto* world = Gothic::inst().world();
  return JS_NewString(ctx,world==nullptr ? "" : std::string(world->name()).c_str());
  }

/// world.time(): {day, hour, minute}
JSValue time(JSContext* ctx, JSValueConst, int, JSValueConst*) {
  auto* world = Gothic::inst().world();
  if(world==nullptr)
    return JS_NULL;
  const auto t = world->time();
  JSValue o = JS_NewObject(ctx);
  JS_SetPropertyStr(ctx,o,"day",   JS_NewInt64(ctx,int64_t(t.day())));
  JS_SetPropertyStr(ctx,o,"hour",  JS_NewInt32(ctx,int32_t(t.hour())));
  JS_SetPropertyStr(ctx,o,"minute",JS_NewInt32(ctx,int32_t(t.minute())));
  return o;
  }

JSValue setTime(JSContext* ctx, JSValueConst, int argc, JSValueConst* argv) {
  auto* world = Gothic::inst().world();
  if(world==nullptr || argc<2)
    return JS_UNDEFINED;
  int32_t h = 0, m = 0;
  JS_ToInt32(ctx,&h,argv[0]);
  JS_ToInt32(ctx,&m,argv[1]);
  world->setDayTime(h,m);
  return JS_UNDEFINED;
  }

/// world.changeLevel("ARCHOLOS_SEWERS.ZEN", "WAYPOINT"): the player goes to another world (as a trigger of a change of the level)
JSValue changeLevel(JSContext* ctx, JSValueConst, int argc, JSValueConst* argv) {
  auto* world = Gothic::inst().world();
  if(world==nullptr || argc<1)
    return JS_FALSE;
  const auto name  = js::str(ctx,argv[0]);
  const auto point = argc>1 ? js::str(ctx,argv[1]) : std::string();
  if(name.empty())
    return JS_FALSE;
  world->triggerChangeWorld(name,point);
  return JS_TRUE;
  }

/// world.waypoint("OC2"): {x, y, z} or null
JSValue waypoint(JSContext* ctx, JSValueConst, int argc, JSValueConst* argv) {
  auto* world = Gothic::inst().world();
  if(world==nullptr || argc<1)
    return JS_NULL;
  auto* wp = world->findPoint(js::str(ctx,argv[0]),false);
  if(wp==nullptr)
    return JS_NULL;

  const auto pos = wp->position();
  JSValue o = JS_NewObject(ctx);
  JS_SetPropertyStr(ctx,o,"x",JS_NewFloat64(ctx,double(pos.x)));
  JS_SetPropertyStr(ctx,o,"y",JS_NewFloat64(ctx,double(pos.y)));
  JS_SetPropertyStr(ctx,o,"z",JS_NewFloat64(ctx,double(pos.z)));
  return o;
  }

/// world.takeShots(): the arrows and bolts that were shot since the last call (the first call starts to watch):
/// [[item, x, y, z, vx, vy, vz, shooter], ...]
JSValue takeShots(JSContext* ctx, JSValueConst, int, JSValueConst*) {
  JSValue arr   = JS_NewArray(ctx);
  auto*   world = Gothic::inst().world();
  if(world==nullptr)
    return arr;
  uint32_t n = 0;
  for(auto& s:world->takeShots()) {
    JSValue e = JS_NewArray(ctx);
    JS_SetPropertyUint32(ctx,e,0,JS_NewString(ctx,game::instanceName(*world,s.item).c_str()));
    const double v[6] = {double(s.pos.x),double(s.pos.y),double(s.pos.z),double(s.vel.x),double(s.vel.y),double(s.vel.z)};
    for(uint32_t i=0; i<6; ++i)
      JS_SetPropertyUint32(ctx,e,i+1,JS_NewFloat64(ctx,v[i]));
    JS_SetPropertyUint32(ctx,e,7,JS_NewUint32(ctx,s.origin));
    JS_SetPropertyUint32(ctx,arr,n++,e);
    }
  return arr;
  }

/// world.shoot(item, x, y, z, vx, vy, vz): an arrow or a bolt that flies, as it was shot in another game. It makes
/// no damage here: the one who has shot it takes care of that. Spells are not shot this way.
JSValue shoot(JSContext* ctx, JSValueConst, int argc, JSValueConst* argv) {
  auto* world = Gothic::inst().world();
  if(world==nullptr || argc<7)
    return JS_FALSE;
  const auto cls = game::findInstance(*world,js::str(ctx,argv[0]));
  double v[6] = {};
  for(int i=0; i<6; ++i)
    if(JS_ToFloat64(ctx,&v[i],argv[i+1])<0)
      return JS_FALSE;
  if(cls==size_t(-1))
    return JS_FALSE;

  Item itm(*world,cls,Item::T_Inventory);
  const auto* b = world->shootRemote(itm,Tempest::Vec3(float(v[0]),float(v[1]),float(v[2])),
                                         Tempest::Vec3(float(v[3]),float(v[4]),float(v[5])));
  return JS_NewBool(ctx,b!=nullptr);
  }

/// world.setHiddenVisuals(["GRASS", "BUSH"]): the static objects (vegetation, stones, ...) whose visual has one of these
/// words in its name are not drawn, in this world and in the next ones. The case does not matter. An empty list shows
/// everything again. It ends when the mod stops: without such a mod the game is vanilla. Returns how many objects of
/// the world are hidden now.
JSValue setHiddenVisuals(JSContext* ctx, JSValueConst, int argc, JSValueConst* argv) {
  auto& mod = js::mod(ctx);
  mod.hideVisuals.clear();
  if(argc>=1 && JS_IsArray(argv[0])) {
    JSValue len = JS_GetPropertyStr(ctx,argv[0],"length");
    uint32_t n = 0;
    JS_ToUint32(ctx,&n,len);
    JS_FreeValue(ctx,len);
    n = std::min<uint32_t>(n,512);
    for(uint32_t i=0; i<n; ++i) {
      JSValue v = JS_GetPropertyUint32(ctx,argv[0],i);
      std::string s = js::str(ctx,v);
      JS_FreeValue(ctx,v);
      if(!s.empty() && s.size()<=64)
        mod.hideVisuals.push_back(std::move(s));
      }
    }
  ModHost::inst().refreshVisualFilter();

  auto* world = Gothic::inst().world();
  if(world==nullptr)
    return JS_NewInt32(ctx,0);
  // the world changes at once, the game does not wait for the next frame
  world->applyVisualFilter();
  game::noteVisualFilter(world);
  int32_t hidden = 0;
  world->visitVisuals([&](std::string_view,bool h){ hidden += h ? 1 : 0; });
  return JS_NewInt32(ctx,hidden);
  }

/// world.visuals(filter, max): the visuals of the static objects of this world, to find the words for setHiddenVisuals:
/// [[name, count, hidden], ...], the most frequent first. `filter` is a part of the name.
JSValue visuals(JSContext* ctx, JSValueConst, int argc, JSValueConst* argv) {
  JSValue arr   = JS_NewArray(ctx);
  auto*   world = Gothic::inst().world();
  if(world==nullptr)
    return arr;
  std::string filter = argc>=1 && !JS_IsUndefined(argv[0]) && !JS_IsNull(argv[0]) ? js::str(ctx,argv[0]) : std::string();
  int32_t     max    = 200;
  if(argc>=2)
    JS_ToInt32(ctx,&max,argv[1]);
  max = std::clamp(max,1,2000);
  for(auto& c:filter)
    c = char(std::toupper(static_cast<unsigned char>(c)));

  std::map<std::string,std::pair<uint32_t,uint32_t>> all;
  world->visitVisuals([&](std::string_view vis, bool hidden){
    std::string up(vis);
    for(auto& c:up)
      c = char(std::toupper(static_cast<unsigned char>(c)));
    if(!filter.empty() && up.find(filter)==std::string::npos)
      return;
    auto& e = all[up];
    e.first++;
    e.second += hidden ? 1 : 0;
    });

  std::vector<std::pair<std::string,std::pair<uint32_t,uint32_t>>> sorted(all.begin(),all.end());
  std::stable_sort(sorted.begin(),sorted.end(),[](const auto& a, const auto& b){ return a.second.first>b.second.first; });
  uint32_t n = 0;
  for(auto& e:sorted) {
    if(int32_t(n)>=max)
      break;
    JSValue row = JS_NewArray(ctx);
    JS_SetPropertyUint32(ctx,row,0,JS_NewString(ctx,e.first.c_str()));
    JS_SetPropertyUint32(ctx,row,1,JS_NewUint32(ctx,e.second.first));
    JS_SetPropertyUint32(ctx,row,2,JS_NewBool(ctx,e.second.second>0));
    JS_SetPropertyUint32(ctx,arr,n++,row);
    }
  return arr;
  }

}

/// world: the name of the world, the time, the way points
void game::installWorld(JSContext* ctx) {
  JSValue global = JS_GetGlobalObject(ctx);
  JSValue world  = JS_NewObject(ctx);
  js::fn(ctx,world,"name",    name,0);
  js::fn(ctx,world,"time",    time,0);
  js::fn(ctx,world,"setTime", setTime,2);
  js::fn(ctx,world,"waypoint",waypoint,1);
  js::fn(ctx,world,"changeLevel",changeLevel,2);
  js::fn(ctx,world,"takeShots",takeShots,0);
  js::fn(ctx,world,"shoot",   shoot,7);
  js::fn(ctx,world,"setHiddenVisuals",setHiddenVisuals,1);
  js::fn(ctx,world,"visuals", visuals,2);
  JS_SetPropertyStr(ctx,global,"world",world);
  JS_FreeValue(ctx,global);
  }
