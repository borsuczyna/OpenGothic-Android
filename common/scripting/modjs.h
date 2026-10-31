#pragma once

#include "modhost.h"

/// Objects that the scripts of mods see. Every object has a file of its own in the api/ folder.
namespace Scripting {

/// Helpers of QuickJS
namespace js {
  Mod&        mod(JSContext* ctx);

  std::string str      (JSContext* ctx, JSValueConst v);
  /// what console.log prints for a value
  std::string text     (JSContext* ctx, JSValueConst v);
  std::string join     (JSContext* ctx, int argc, JSValueConst* argv);
  /// message and stack of the pending exception, the exception is taken
  std::string exception(JSContext* ctx);

  /// Optional properties of an options object (combat.configure, screen.marker): true when the property is there and
  /// has been read, `out` is left as it was otherwise. A color is "#rrggbb" or a number, 0xRRGGBB.
  bool getBool (JSContext* ctx, JSValueConst obj, const char* key, bool& out);
  bool getInt  (JSContext* ctx, JSValueConst obj, const char* key, int& out);
  bool getColor(JSContext* ctx, JSValueConst obj, const char* key, uint32_t& out);

  void fn(JSContext* ctx, JSValueConst obj, const char* name, JSCFunction* f, int argc);

  using Magic = JSValue (*)(JSContext*, JSValueConst, int, JSValueConst*, int);
  void fnMagic(JSContext* ctx, JSValueConst obj, const char* name, Magic f, int argc, int magic);
  }

namespace api {
  void installInfo   (JSContext* ctx, Mod& mod, ModHost& host);  // GothicJS
  void installConsole(JSContext* ctx);                           // console
  void installEvents (JSContext* ctx);                           // events
  void installTimers (JSContext* ctx);                           // setTimeout, setInterval
  void installStorage(JSContext* ctx, Mod& mod);                 // storage
  void installRequire(JSContext* ctx);                           // require
  void installNet    (JSContext* ctx);                           // net
  /// Connections of the mod: events of the network are delivered, called every frame
  void pollNet       (Mod& mod);
  void releaseNet    (Mod& mod);

  /// require() of a file in the folder dir of the mod
  JSValue makeRequire(JSContext* ctx, const std::string& dir);
  }

}
