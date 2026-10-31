#include "scripting/gameapi.h"

#include <cstdlib>
#include <string>

#include "scripting/modjs.h"
#include "scripting/modutil.h"
#include "game/worldmarkers.h"
#include "gothic.h"

using namespace Scripting;

namespace {

/// screen.print(text): a message on the screen, like the messages of the game
JSValue print(JSContext* ctx, JSValueConst, int argc, JSValueConst* argv) {
  // the font of the game draws a glyph for every byte: UTF-8 of the script is turned into the code page of the game
  Gothic::inst().onPrint(utf8ToGame(js::join(ctx,argc,argv)));
  return JS_UNDEFINED;
  }

/// screen.marker(x, y, z, {color: "#00ff00", duration: 1500, size: 1}): a small light in the world for a moment,
/// it shows where something was taken from (the auto-loot uses it)
JSValue marker(JSContext* ctx, JSValueConst, int argc, JSValueConst* argv) {
  if(argc<3)
    return JS_UNDEFINED;
  double x=0, y=0, z=0;
  JS_ToFloat64(ctx,&x,argv[0]);
  JS_ToFloat64(ctx,&y,argv[1]);
  JS_ToFloat64(ctx,&z,argv[2]);

  uint32_t color = 0x00FF00;
  int      ms    = 1500;
  int      size  = 1;
  if(argc>=4 && JS_IsObject(argv[3])) {
    JSValue c = JS_GetPropertyStr(ctx,argv[3],"color");
    if(JS_IsString(c)) {
      const std::string s = js::str(ctx,c);
      const char* b = s.c_str();
      if(*b=='#')
        ++b;
      char* end = nullptr;
      const unsigned long v = std::strtoul(b,&end,16);
      if(end!=b)
        color = uint32_t(v&0xFFFFFF);
      }
    JS_FreeValue(ctx,c);
    JSValue d = JS_GetPropertyStr(ctx,argv[3],"duration");
    if(JS_IsNumber(d)) { double v=0; JS_ToFloat64(ctx,&v,d); ms = int(v); }
    JS_FreeValue(ctx,d);
    JSValue sz = JS_GetPropertyStr(ctx,argv[3],"size");
    if(JS_IsNumber(sz)) { double v=0; JS_ToFloat64(ctx,&v,sz); size = int(v); }
    JS_FreeValue(ctx,sz);
    }
  WorldMarkers::inst().add(Tempest::Vec3(float(x),float(y),float(z)),color,ms,size);
  return JS_UNDEFINED;
  }

}

/// screen: what the player sees
void game::installScreen(JSContext* ctx) {
  JSValue global = JS_GetGlobalObject(ctx);
  JSValue screen = JS_NewObject(ctx);
  js::fn(ctx,screen,"print",print,1);
  js::fn(ctx,screen,"marker",marker,4);
  JS_SetPropertyStr(ctx,global,"screen",screen);
  JS_FreeValue(ctx,global);
  }
