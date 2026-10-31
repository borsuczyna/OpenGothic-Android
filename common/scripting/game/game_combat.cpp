#include "scripting/gameapi.h"

#include <algorithm>

#include "game/combatfx.h"
#include "scripting/modjs.h"

using namespace Scripting;

namespace {

/// combat.configure({dealt, taken, enemyHp, size, duration, colorDealt, colorTaken, colorEnemyHp, active}): switches the
/// display of the fight on (the first call) and changes what it shows. The display ends when the mod stops.
JSValue configure(JSContext* ctx, JSValueConst, int argc, JSValueConst* argv) {
  auto& o = js::mod(ctx).combat;
  o.active = true;
  if(argc>=1 && JS_IsObject(argv[0])) {
    js::getBool (ctx,argv[0],"dealt",   o.showDealt);
    js::getBool (ctx,argv[0],"taken",   o.showTaken);
    js::getBool (ctx,argv[0],"enemyHp", o.showEnemyHp);
    if(js::getInt(ctx,argv[0],"size",o.size))
      o.size = std::clamp(o.size,0,3);
    if(js::getInt(ctx,argv[0],"duration",o.duration))
      o.duration = std::clamp(o.duration,300,6000);
    js::getColor(ctx,argv[0],"colorDealt",   o.colorDealt);
    js::getColor(ctx,argv[0],"colorTaken",   o.colorTaken);
    js::getColor(ctx,argv[0],"colorEnemyHp", o.colorEnemyHp);
    js::getBool (ctx,argv[0],"active",       o.active);
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

/// combat: numbers of damage in the air and the health of the enemy. Vanilla until a mod calls combat.configure.
void game::installCombat(JSContext* ctx) {
  JSValue global = JS_GetGlobalObject(ctx);
  JSValue combat = JS_NewObject(ctx);
  js::fn(ctx,combat,"configure",configure,1);
  js::fn(ctx,combat,"disable",  disable,  0);
  js::fn(ctx,combat,"isActive", isActive, 0);
  JS_SetPropertyStr(ctx,global,"combat",combat);
  JS_FreeValue(ctx,global);
  }
