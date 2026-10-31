#include "scripting/modjs.h"

#include <cstdlib>

using namespace Scripting;

Mod& js::mod(JSContext* ctx) {
  return *static_cast<Mod*>(JS_GetContextOpaque(ctx));
  }

std::string js::str(JSContext* ctx, JSValueConst v) {
  const char* s = JS_ToCString(ctx,v);
  if(s==nullptr) {
    JS_FreeValue(ctx,JS_GetException(ctx));
    return "<error>";
    }
  std::string ret = s;
  JS_FreeCString(ctx,s);
  return ret;
  }

std::string js::text(JSContext* ctx, JSValueConst v) {
  if(JS_IsString(v) || !JS_IsObject(v))
    return str(ctx,v);

  JSValue j = JS_JSONStringify(ctx,v,JS_UNDEFINED,JS_UNDEFINED);
  if(JS_IsException(j)) {
    JS_FreeValue(ctx,JS_GetException(ctx));
    return str(ctx,v);
    }
  std::string ret = JS_IsUndefined(j) ? str(ctx,v) : str(ctx,j);
  JS_FreeValue(ctx,j);
  return ret;
  }

std::string js::join(JSContext* ctx, int argc, JSValueConst* argv) {
  std::string ret;
  for(int i=0; i<argc; ++i) {
    if(i>0)
      ret += ' ';
    ret += text(ctx,argv[i]);
    }
  return ret;
  }

std::string js::exception(JSContext* ctx) {
  JSValue     ex  = JS_GetException(ctx);
  std::string ret = str(ctx,ex);
  if(JS_IsObject(ex)) {
    JSValue stack = JS_GetPropertyStr(ctx,ex,"stack");
    if(!JS_IsUndefined(stack)) {
      const auto s = str(ctx,stack);
      if(!s.empty())
        ret += "\n"+s;
      }
    JS_FreeValue(ctx,stack);
    }
  JS_FreeValue(ctx,ex);
  return ret;
  }

bool js::getBool(JSContext* ctx, JSValueConst obj, const char* key, bool& out) {
  JSValue v = JS_GetPropertyStr(ctx,obj,key);
  const bool has = !JS_IsUndefined(v);
  if(has)
    out = JS_ToBool(ctx,v)!=0;
  JS_FreeValue(ctx,v);
  return has;
  }

bool js::getInt(JSContext* ctx, JSValueConst obj, const char* key, int& out) {
  JSValue v = JS_GetPropertyStr(ctx,obj,key);
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

bool js::getColor(JSContext* ctx, JSValueConst obj, const char* key, uint32_t& out) {
  JSValue v = JS_GetPropertyStr(ctx,obj,key);
  bool has = false;
  if(JS_IsString(v)) {
    const std::string s = str(ctx,v);
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

void js::fn(JSContext* ctx, JSValueConst obj, const char* name, JSCFunction* f, int argc) {
  JS_SetPropertyStr(ctx,obj,name,JS_NewCFunction(ctx,f,name,argc));
  }

void js::fnMagic(JSContext* ctx, JSValueConst obj, const char* name, Magic f, int argc, int magic) {
  JS_SetPropertyStr(ctx,obj,name,JS_NewCFunction2(ctx,reinterpret_cast<JSCFunction*>(f),name,argc,JS_CFUNC_generic_magic,magic));
  }
