#include "installdetect.h"
#if defined __ANDROID__
extern "C" const char* SDL_AndroidGetExternalStoragePath();
#include <Tempest/Platform>

#include <Tempest/AndroidApi>
#endif

#ifdef __WINDOWS__
#include "windows.h"
#include "shlobj.h"
#include "shlwapi.h"
#endif

#include <cstring>
#include <fstream>
#include "utils/fileutil.h"

InstallDetect::InstallDetect() {
#ifdef __WINDOWS__
  pfiles    = programFiles(false);
  pfilesX86 = programFiles(true);
#endif
#if defined(__OSX__) || defined(__IOS__)
  appDir    = applicationSupportDirectory();
#endif
  }

std::u16string InstallDetect::detectG2() {
#if defined(__WINDOWS__)
  auto ret = detectG2(pfiles);
  if(ret.empty())
    ret = detectG2(pfilesX86);
  return ret;
#elif defined(__OSX__) || defined(__IOS__)
  if(FileUtil::exists(appDir))
    return appDir;
  return u"";
#elif defined(__ANDROID__)
  auto path = Tempest::AndroidApi::externalDataPath().u16string();
  if(path.empty())
    return u"";

  std::u16string folder = u"";
  bool isAbsolute = false;
  {
    std::ifstream in(std::filesystem::path(path)/"game.txt");
    std::string   name;
    if(in && std::getline(in,name)) {
      while(!name.empty() && (name.back() == '\r' || name.back() == '\n' || name.back() == ' ' || name.back() == '\t'))
        name.pop_back();
      while(!name.empty() && (name.front() == ' ' || name.front() == '\t'))
        name.erase(name.begin());

      if(!name.empty()) {
        if(name[0] == '/') {
          isAbsolute = true;
          folder = std::filesystem::path(name).u16string();
        } else {
          std::u16string chosen;
          for(char c:name) {
            const bool ok = ('a'<=c && c<='z') || ('A'<=c && c<='Z') || ('0'<=c && c<='9') || c=='_' || c=='-';
            if(!ok)
              break;
            chosen.push_back(char16_t(c));
          }
          if(!chosen.empty())
            folder = chosen;
        }
      }
    }
  }

  // Jeśli to ścieżka bezwzględna, zwracamy ją bezpośrednio wprost ze wskazanego folderu
  if(isAbsolute) {
    if(FileUtil::exists(folder))
      return folder;
    return u"";
  }

  if(folder.empty())
    folder = u"gothic2";

  auto ret = FileUtil::nestedPath(path + u"/", {folder.c_str()}, Tempest::Dir::FT_Dir);
  if(ret.empty() && folder!=u"gothic2")
    ret = FileUtil::nestedPath(path + u"/", {u"gothic2"}, Tempest::Dir::FT_Dir);
  return ret;
#else
  return u"";
#endif
  }

#ifdef __ANDROID__
std::string InstallDetect::androidModIni() {
  const auto base = Tempest::AndroidApi::externalDataPath();
  if(base.empty())
    return "";
  std::ifstream in(std::filesystem::path(base)/"game.txt");
  std::string   line, ini;
  if(!in || !std::getline(in,line) || !std::getline(in,ini))
    return "";
  std::string ret;
  for(char c:ini) {
    const bool ok = ('a'<=c && c<='z') || ('A'<=c && c<='Z') || ('0'<=c && c<='9') || c=='_' || c=='-' || c=='.';
    if(!ok)
      break;
    ret.push_back(c);
    }
  return ret;
  }
#endif

std::filesystem::path InstallDetect::androidInternalDataPath() {
#if defined(__ANDROID__)
  return Tempest::AndroidApi::externalDataPath();
#else
  return Tempest::AndroidApi::internalDataPath();
#endif
}

std::u16string InstallDetect::detectG2(std::u16string pfiles) {
  if(pfiles.empty())
    return u"";
  auto steam = pfiles+u"/Steam/steamapps/common/Gothic II/";
  if(FileUtil::exists(steam))
    return steam;
  auto akela = pfiles+u"/Akella Games/Gothic II/";
  if(FileUtil::exists(akela))
    return akela;
  return u"";
  }

#ifdef __WINDOWS__
std::u16string InstallDetect::programFiles(bool x86) {
  WCHAR path[MAX_PATH]={};
  if(FAILED(SHGetFolderPathW(NULL, (x86 ? CSIDL_PROGRAM_FILESX86 : CSIDL_PROGRAM_FILES), NULL, 0, path)))
    return u"";
  std::u16string ret;
  size_t len=0;
  for(;path[len];++len);

  ret.resize(len);
  std::memcpy(&ret[0],path,len*sizeof(char16_t));
  return ret;
  }
#endif
