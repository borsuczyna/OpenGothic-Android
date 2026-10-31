#include "scripting/gameapi.h"

#include <algorithm>
#include <cstdlib>
#include <string>

#include "game/combatfx.h"
#include "scripting/modjs.h"

using namespace Scripting;

namespace {

bool getBool(JSContext* ctx, JSValueConst o, const char* key, bool& out) {
  JSValue v = JS_GetPropertyStr(ctx,o,key);
  const bool has = !JS_IsUndefined(v);
  if(has)
    out = JS_ToBool(ctx,v)!=0;
  JS_FreeValue(ctx,v);
  return has;
  }

bool getInt(JSContext* ctx, JSValueConst o, const char* key, int& out) {
  JSValue v = JS_GetPropertyStr(ctx,o,key);
  bool has = false;
  if(JS_IsNumber(v)) {
    double d = 0;
    JS_ToFloat64(ctx,&d,v);
    out = int(d);
    has = true;
    }
  JS_FreeValue(ctx,v);
  return has;
  }

/// "#rrggbb" or a number
bool getColor(JSContext* ctx, JSValueConst o, const char* key, uint32_t& out) {
  JSValue v = JS_GetPropertyStr(ctx,o,key);
  bool has = false;
  if(JS_IsString(v)) {
    const std::string s = js::str(ctx,v);
    const char* b = s.c_str();
    if(*b=='#')
      ++b;
    char* end = nullptr;
    const unsigned long c = std::strtoul(b,&end,16);
    if(end!=b) {
      out = uint32_t(c&0xFFFFFF);
      has = true;
      }
    } else
  if(JS_IsNumber(v)) {
    double d = 0;
    JS_ToFloat64(ctx,&d,v);
    out = uint32_t(int64_t(d))&0xFFFFFF;
    has = true;
    }
  JS_FreeValue(ctx,v);
  return has;
  }

/// combat.configure({dealt, taken, enemyHp, size, duration, colors...}): switches the display of the fight on
/// (the first call) and changes what it shows. The display ends when the mod stops.
JSValue configure(JSContext* ctx, JSValueConst, int argc, JSValueConst* argv) {
  auto& o = js::mod(ctx).combat;
  o.active = true;
  if(argc>=1 && JS_IsObject(argv[0])) {
    getBool (ctx,argv[0],"dealt",   o.showDealt);
    getBool (ctx,argv[0],"taken",   o.showTaken);
    getBool (ctx,argv[0],"enemyHp", o.showEnemyHp);
    if(getInt(ctx,argv[0],"size",o.size))
      o.size = std::clamp(o.size,0,3);
    if(getInt(ctx,argv[0],"duration",o.duration))
      o.duration = std::clamp(o.duration,300,6000);
    getColor(ctx,argv[0],"colorDealt",   o.colorDealt);
    getColor(ctx,argv[0],"colorTaken",   o.colorTaken);
    getColor(ctx,argv[0],"colorEnemyHp", o.colorEnemyHp);
    if(argc>=1) {
      bool active = true;
      if(getBool(ctx,argv[0],"active",active))
        o.active = active;
      }
    }
  if(!o.active)
    CombatFx::inst().clear();
  return JS_UNDEFINED;
  }

/// combat.disable(): the display is off
JSValue disable(JSContext* ctx, JSValueConst, int, JSValueConst*) {
  js::mod(ctx).combat.active = false;
  CombatFx::inst().clear();
  return JS_UNDEFINED;
  }

/// combat.isActive()
JSValue isActive(JSContext* ctx, JSValueConst, int, JSValueConst*) {
  return JS_NewBool(ctx,js::mod(ctx).combat.active);
  }

}

/// combat: numbers of damage in the air and the defense of the enemy. Vanilla until a mod calls combat.configure.
void game::installCombat(JSContext* ctx) {
  JSValue global = JS_GetGlobalObject(ctx);
  JSValue combat = JS_NewObject(ctx);
  js::fn(ctx,combat,"configure",configure,1);
  js::fn(ctx,combat,"disable",  disable,  0);
  js::fn(ctx,combat,"isActive", isActive, 0);
  JS_SetPropertyStr(ctx,global,"combat",combat);
  JS_FreeValue(ctx,global);
  }
