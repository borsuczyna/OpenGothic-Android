#pragma once

#include <cstdint>
#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

#include <quickjs.h>

/// Small things the files of the mod system share
namespace Scripting {

std::filesystem::path toPath(std::string_view utf8);
std::string           toUtf8(const std::filesystem::path& path);
std::string           lowerCase(std::string s);
uint64_t              steadyMs();

/// Texts of the game are bytes of the code page of the game (CP1250: Polish, Czech; the same for the letters of
/// German), the font draws a glyph for every byte. Scripts use UTF-8, so the texts are converted on the border.
/// Text of the game -> UTF-8 (text that is valid UTF-8 already is left as it is).
std::string           gameToUtf8(std::string_view text);
/// UTF-8 -> bytes for the font of the game. Letters that the code page does not have become '?', the ones
/// that look the same without an accent become the plain letter (e.g. the Czech ones).
std::string           utf8ToGame(std::string_view text);

/// Path inside of a mod: "a/b/c.js". Paths that leave the mod are refused.
bool        normalizePath(std::string_view path, std::string& out);
std::string dirName(std::string_view path);

bool readFile (const std::filesystem::path& path, std::string& out);
bool writeFile(const std::filesystem::path& path, std::string_view data);

/// Reads JSON with a runtime of its own, so the files do not depend on the contexts of the mods
class JsonReader final {
  public:
    explicit JsonReader(std::string_view json);
    ~JsonReader();
    JsonReader(const JsonReader&) = delete;

    bool ok() const { return JS_IsObject(root); }

    bool string (const char* key, std::string& out) const;
    bool integer(const char* key, int& out) const;
    /// a string or an array of strings
    std::vector<std::string> strings(const char* key) const;

  private:
    JSRuntime* rt   = nullptr;
    JSContext* ctx  = nullptr;
    JSValue    root = JS_UNDEFINED;
  };

}
