#include "scripting/gameapi.h"

#include "scripting/modjs.h"

using namespace Scripting;

namespace {

/// touch.fastForward(on): the mod asks for the fast forward. Until a mod does it the touch interface is vanilla:
/// there is no button of the speed on the HUD and no setting of it. The request ends when the mod stops.
JSValue fastForward(JSContext* ctx, JSValueConst, int argc, JSValueConst* argv) {
  js::mod(ctx).fastForward = argc<1 || JS_ToBool(ctx,argv[0])!=0;
  return JS_UNDEFINED;
  }

/// touch.isFastForward: the request of this mod
JSValue isFastForward(JSContext* ctx, JSValueConst, int, JSValueConst*) {
  return JS_NewBool(ctx,js::mod(ctx).fastForward);
  }

}

/// touch: the controls of the touch interface (Android). On a computer the calls do nothing.
void game::installTouch(JSContext* ctx) {
  JSValue global = JS_GetGlobalObject(ctx);
  JSValue touch  = JS_NewObject(ctx);
  js::fn(ctx,touch,"fastForward",   fastForward,   1);
  js::fn(ctx,touch,"isFastForward", isFastForward, 0);
  JS_SetPropertyStr(ctx,global,"touch",touch);
  JS_FreeValue(ctx,global);
  }
