#include "installdetect.h"

#include <Tempest/Platform>

#ifdef __ANDROID__
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

  // the launcher writes the name of the folder of the game that is chosen (Gothic1, Gothic2, Myrtana): there is no command line
  std::u16string folder = u"gothic2";
  {
    std::ifstream in(std::filesystem::path(path)/"game.txt");
    std::string   name;
    if(in && std::getline(in,name)) {
      // a folder of the phone that the player has chosen is a full path; if it is gone, the folders of the launcher are looked into
      while(!name.empty() && (name.back()=='\r' || name.back()==' ' || name.back()=='\t'))
        name.pop_back();
      if(!name.empty() && name[0]=='/') {
        std::error_code ec;
        const std::filesystem::path dir(name);
        if(std::filesystem::is_directory(dir/"Data",ec) || std::filesystem::is_directory(dir/"data",ec))
          return dir.u16string();
        name.clear();
        }
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
  // the ini of a mod from Spine can have spaces and other marks in the name; it only must stay a file name in System
  for(char c:ini) {
    const bool ok = static_cast<unsigned char>(c)>=0x20 && c!='/' && c!='\\' && c!=':' && c!='*' && c!='?' && c!='"' && c!='<' && c!='>' && c!='|';
    if(!ok)
      break;
    ret.push_back(c);
    }
  while(!ret.empty() && ret.back()==' ')
    ret.pop_back();
  if(ret=="." || ret=="..")
    ret.clear();
  return ret;
  }

std::filesystem::path InstallDetect::androidInternalDataPath() {
  auto path = Tempest::AndroidApi::internalDataPath();
  std::filesystem::create_directories(path);
  return path;
  }
#endif

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
