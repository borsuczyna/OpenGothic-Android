#include "modutil.h"

#include <algorithm>
#include <cctype>
#include <chrono>
#include <fstream>
#include <sstream>

namespace fs = std::filesystem;

namespace Scripting {

namespace {

// CP1250, the bytes 0x80..0xFF as Unicode (0 = not used)
constexpr char16_t Cp1250[128] = {
  0x20AC,0,0x201A,0,0x201E,0x2026,0x2020,0x2021,0,0x2030,0x0160,0x2039,0x015A,0x0164,0x017D,0x0179,
  0,0x2018,0x2019,0x201C,0x201D,0x2022,0x2013,0x2014,0,0x2122,0x0161,0x203A,0x015B,0x0165,0x017E,0x017A,
  0x00A0,0x02C7,0x02D8,0x0141,0x00A4,0x0104,0x00A6,0x00A7,0x00A8,0x00A9,0x015E,0x00AB,0x00AC,0x00AD,0x00AE,0x017B,
  0x00B0,0x00B1,0x02DB,0x0142,0x00B4,0x00B5,0x00B6,0x00B7,0x00B8,0x0105,0x015F,0x00BB,0x013D,0x02DD,0x013E,0x017C,
  0x0154,0x00C1,0x00C2,0x0102,0x00C4,0x0139,0x0106,0x00C7,0x010C,0x00C9,0x0118,0x00CB,0x011A,0x00CD,0x00CE,0x010E,
  0x0110,0x0143,0x0147,0x00D3,0x00D4,0x0150,0x00D6,0x00D7,0x0158,0x016E,0x00DA,0x0170,0x00DC,0x00DD,0x0162,0x00DF,
  0x0155,0x00E1,0x00E2,0x0103,0x00E4,0x013A,0x0107,0x00E7,0x010D,0x00E9,0x0119,0x00EB,0x011B,0x00ED,0x00EE,0x010F,
  0x0111,0x0144,0x0148,0x00F3,0x00F4,0x0151,0x00F6,0x00F7,0x0159,0x016F,0x00FA,0x0171,0x00FC,0x00FD,0x0163,0x02D9
  };

bool validUtf8(std::string_view s) {
  size_t i = 0;
  while(i<s.size()) {
    const unsigned char c = static_cast<unsigned char>(s[i]);
    size_t n = 0;
    if(c>=0xF5 || c==0xC0 || c==0xC1 || (c>=0x80 && c<0xC2))
      return false;
    if(c>=0xF0)      n = 3;
    else if(c>=0xE0) n = 2;
    else if(c>=0xC2) n = 1;
    if(i+n>=s.size())
      return false;
    for(size_t k=1;k<=n;++k)
      if((static_cast<unsigned char>(s[i+k])&0xC0)!=0x80)
        return false;
    i += n+1;
    }
  return true;
  }

void putUtf8(std::string& out, char32_t u) {
  if(u<0x80) {
    out += char(u);
    } else if(u<0x800) {
    out += char(0xC0|(u>>6));
    out += char(0x80|(u&0x3F));
    } else if(u<0x10000) {
    out += char(0xE0|(u>>12));
    out += char(0x80|((u>>6)&0x3F));
    out += char(0x80|(u&0x3F));
    } else {
    out += char(0xF0|(u>>18));
    out += char(0x80|((u>>12)&0x3F));
    out += char(0x80|((u>>6)&0x3F));
    out += char(0x80|(u&0x3F));
    }
  }

/// the letter without the accent for the ones that the code page has not (e.g. Czech letters in a Polish font)
char plainLetter(char32_t u) {
  switch(u) {
    case 0x0160: return 'S'; case 0x0161: return 's';
    case 0x010C: return 'C'; case 0x010D: return 'c';
    case 0x0158: return 'R'; case 0x0159: return 'r';
    case 0x017D: return 'Z'; case 0x017E: return 'z';
    case 0x2018: case 0x2019: return '\'';
    case 0x201C: case 0x201D: return '"';
    case 0x2013: case 0x2014: return '-';
    default:     return '?';
    }
  }

}

std::string gameToUtf8(std::string_view text) {
  const bool ascii = std::all_of(text.begin(),text.end(),[](char c) { return static_cast<unsigned char>(c)<0x80; });
  if(ascii || validUtf8(text))
    return std::string(text);

  std::string out;
  out.reserve(text.size()*2);
  for(char c:text) {
    const unsigned char b = static_cast<unsigned char>(c);
    if(b<0x80) {
      out += c;
      continue;
      }
    const char16_t u = Cp1250[b-0x80];
    putUtf8(out,u==0 ? char32_t('?') : char32_t(u));
    }
  return out;
  }

std::string utf8ToGame(std::string_view text) {
  std::string out;
  out.reserve(text.size());
  for(size_t i=0; i<text.size();) {
    const unsigned char c = static_cast<unsigned char>(text[i]);
    char32_t u  = c;
    size_t   n  = 0;
    if(c>=0xF0) {
      u = c&0x07;
      n = 3;
      } else
    if(c>=0xE0) {
      u = c&0x0F;
      n = 2;
      } else
    if(c>=0xC0) {
      u = c&0x1F;
      n = 1;
      }
    if(c>=0x80 && n==0) {   // a stray byte
      out += '?';
      ++i;
      continue;
      }
    if(i+n>=text.size()) {
      out += '?';
      break;
      }
    for(size_t k=1;k<=n;++k)
      u = (u<<6) | (static_cast<unsigned char>(text[i+k])&0x3F);
    i += n+1;

    if(u<0x80) {
      out += char(u);
      continue;
      }
    char b = 0;
    for(int k=0;k<128;++k)
      if(Cp1250[k]==u) {
        b = char(0x80+k);
        break;
        }
    out += b!=0 ? b : plainLetter(u);
    }
  return out;
  }

fs::path toPath(std::string_view s) {
  const std::u8string u(reinterpret_cast<const char8_t*>(s.data()),s.size());
  return fs::path(u);
  }

std::string toUtf8(const fs::path& p) {
  const auto u = p.u8string();
  return std::string(reinterpret_cast<const char*>(u.data()),u.size());
  }

std::string lowerCase(std::string s) {
  for(auto& c:s)
    c = char(std::tolower(static_cast<unsigned char>(c)));
  return s;
  }

uint64_t steadyMs() {
  using namespace std::chrono;
  return uint64_t(duration_cast<milliseconds>(steady_clock::now().time_since_epoch()).count());
  }

bool normalizePath(std::string_view path, std::string& out) {
  std::vector<std::string> parts;
  std::string cur;
  auto flush = [&]() {
    if(cur.empty() || cur==".") {
      cur.clear();
      return true;
      }
    if(cur=="..") {
      if(parts.empty())
        return false;
      parts.pop_back();
      } else {
      parts.push_back(cur);
      }
    cur.clear();
    return true;
    };

  for(char c:path) {
    if(c=='/' || c=='\\') {
      if(!flush())
        return false;
      } else {
      cur.push_back(c);
      }
    }
  if(!flush())
    return false;

  out.clear();
  for(size_t i=0; i<parts.size(); ++i) {
    if(i>0)
      out += '/';
    out += parts[i];
    }
  return !out.empty();
  }

std::string dirName(std::string_view path) {
  const auto at = path.find_last_of('/');
  return at==std::string_view::npos ? std::string() : std::string(path.substr(0,at));
  }

bool readFile(const fs::path& path, std::string& out) {
  std::ifstream in(path,std::ios::binary);
  if(!in)
    return false;
  std::ostringstream ss;
  ss << in.rdbuf();
  out = ss.str();
  return true;
  }

bool writeFile(const fs::path& path, std::string_view data) {
  std::error_code ec;
  fs::create_directories(path.parent_path(),ec);
  std::ofstream out(path,std::ios::binary|std::ios::trunc);
  if(!out)
    return false;
  out.write(data.data(),std::streamsize(data.size()));
  return bool(out);
  }

JsonReader::JsonReader(std::string_view json) {
  rt = JS_NewRuntime();
  if(rt==nullptr)
    return;
  ctx = JS_NewContext(rt);
  if(ctx==nullptr)
    return;
  root = JS_ParseJSON(ctx,json.data(),json.size(),"json");
  if(JS_IsException(root)) {
    JS_FreeValue(ctx,JS_GetException(ctx));
    root = JS_UNDEFINED;
    }
  }

JsonReader::~JsonReader() {
  if(ctx!=nullptr) {
    JS_FreeValue(ctx,root);
    JS_FreeContext(ctx);
    }
  if(rt!=nullptr)
    JS_FreeRuntime(rt);
  }

bool JsonReader::string(const char* key, std::string& out) const {
  if(!ok())
    return false;
  JSValue v = JS_GetPropertyStr(ctx,root,key);
  bool    r = false;
  if(JS_IsString(v)) {
    if(const char* s = JS_ToCString(ctx,v)) {
      out = s;
      JS_FreeCString(ctx,s);
      r = true;
      }
    }
  JS_FreeValue(ctx,v);
  return r;
  }

bool JsonReader::integer(const char* key, int& out) const {
  if(!ok())
    return false;
  JSValue v = JS_GetPropertyStr(ctx,root,key);
  bool    r = false;
  if(JS_IsNumber(v)) {
    int32_t n = 0;
    r = JS_ToInt32(ctx,&n,v)==0;
    out = n;
    } else
  if(JS_IsString(v)) {
    if(const char* s = JS_ToCString(ctx,v)) {
      out = std::atoi(s);
      JS_FreeCString(ctx,s);
      r = true;
      }
    }
  JS_FreeValue(ctx,v);
  return r;
  }

std::vector<std::string> JsonReader::strings(const char* key) const {
  std::vector<std::string> ret;
  if(!ok())
    return ret;

  auto add = [&](JSValueConst v) {
    if(!JS_IsString(v))
      return;
    if(const char* s = JS_ToCString(ctx,v)) {
      ret.push_back(s);
      JS_FreeCString(ctx,s);
      }
    };

  JSValue v = JS_GetPropertyStr(ctx,root,key);
  if(JS_IsString(v)) {
    add(v);
    } else
  if(JS_IsArray(v)) {
    JSValue len = JS_GetPropertyStr(ctx,v,"length");
    uint32_t n  = 0;
    JS_ToUint32(ctx,&n,len);
    JS_FreeValue(ctx,len);
    for(uint32_t i=0; i<n; ++i) {
      JSValue e = JS_GetPropertyUint32(ctx,v,i);
      add(e);
      JS_FreeValue(ctx,e);
      }
    }
  JS_FreeValue(ctx,v);
  return ret;
  }

}
