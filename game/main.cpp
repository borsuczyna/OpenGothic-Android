#include <Tempest/Window>
#include <Tempest/Application>
#include <Tempest/Log>
#include <fstream>

#include <zenkit/Logger.hh>

#include <Tempest/VulkanApi>

#if defined(_MSC_VER)
#include <Tempest/DirectX12Api>
#endif

#if defined(__APPLE__)
#include <Tempest/MetalApi>
#endif

#if defined(__IOS__) || defined(__ANDROID__)
#include "utils/installdetect.h"
#include <filesystem>
#include <cstdlib>
#endif

#include "utils/crashlog.h"
#include "utils/gamelog.h"
#include "graphics/safegpu.h"
#include "mainwindow.h"
#include "gothic.h"
#include "build.h"
#include "commandline.h"

#include <dmusic.h>

std::string_view selectDevice(const Tempest::AbstractGraphicsApi& api) {
  auto d = api.devices();

  static Tempest::Device::Props p;
  for(auto& i:d)
    // if(i.type==Tempest::DeviceType::Integrated) {
    if(i.type==Tempest::DeviceType::Discrete) {
      p = i;
      return p.name;
      }
  if(d.size()>0) {
    p = d[0];
    return p.name;
    }
  return "";
  }

std::unique_ptr<Tempest::AbstractGraphicsApi> mkApi(const CommandLine& g) {
  Tempest::ApiFlags flg = g.isValidationMode() ? Tempest::ApiFlags::Validation : Tempest::ApiFlags::NoFlags;
  switch(g.graphicsApi()) {
    case CommandLine::DirectX12:
#if defined(_MSC_VER)
      return std::make_unique<Tempest::DirectX12Api>(flg);
#else
      break;
#endif
    case CommandLine::Vulkan:
#if !defined(__APPLE__)
      return std::make_unique<Tempest::VulkanApi>(flg);
#else
      break;
#endif
    }

#if defined(__APPLE__)
  return std::make_unique<Tempest::MetalApi>(flg);
#else
  return std::make_unique<Tempest::VulkanApi>(flg);
#endif
  }

#if defined(__ANDROID__)
static Tempest::WFile* appLogFile = nullptr; // the log of this run, to be flushed when the process ends
#endif
int main(int argc, const char** argv) {
#if defined(__ANDROID__)
    {
        auto appdir = InstallDetect::androidInternalDataPath();
        std::filesystem::path gameDir = appdir / "gothic2"; // domyślny folder
        
        std::ifstream in(appdir / "game.txt");
        std::string name;
        if(in && std::getline(in, name) && !name.empty()) {
            name.erase(name.find_last_not_of(" \n\r\t") + 1);
            if (!name.empty() && name[0] == '/') {
                // Jeśli to pełna ścieżka bezwzględna z nowego menedżera plików
                gameDir = name;
            } else {
                // Stara ścieżka względna wewnątrz katalogu aplikacji
                gameDir = appdir / name;
            }
        }
        
        // Tworzymy katalog tylko wtedy, gdy to ścieżka wewnętrzna i nie istnieje; 
        // dla zewnętrznych folderów Steama zakładamy, że już istnieją.
        if (name.empty() || name[0] != '/') {
            if(!std::filesystem::exists(gameDir)) {
                std::filesystem::create_directories(gameDir);
            }
        }
        
        std::filesystem::current_path(gameDir);
    }
#elif defined(__IOS__)
    {
        auto appdir = InstallDetect::applicationSupportDirectory();
        std::filesystem::current_path(appdir);
    }
#endif

  try {
    static Tempest::WFile logFile("log.txt");
#if defined(__ANDROID__)
    appLogFile = &logFile;
#endif
    Tempest::Log::setOutputCallback([](Tempest::Log::Mode mode, const char* text) {
      GameLog::push(mode==Tempest::Log::Error ? GameLog::Error : mode==Tempest::Log::Debug ? GameLog::Debug : GameLog::Info, text);
      logFile.write(text,std::strlen(text));
      logFile.write("\n",1);
      if(mode==Tempest::Log::Error)
        logFile.flush();
#if defined(__ANDROID__)
      // the engine reports a pipeline before the driver compiles it: if the driver takes the process down, the line is the last one
      if(mode==Tempest::Log::Debug) {
        // ... and the safe GPU mode keeps its name in the lock for the next start
        SafeGpu::onLog(text);
        logFile.flush();
        }
#endif
      });
    }
  catch(...) {
    // the log is kept in the memory anyway, for the menu of the developer
    Tempest::Log::setOutputCallback([](Tempest::Log::Mode mode, const char* text) {
      GameLog::push(mode==Tempest::Log::Error ? GameLog::Error : mode==Tempest::Log::Debug ? GameLog::Debug : GameLog::Info, text);
      std::fprintf(stderr,"%s\n",text);
      });
    Tempest::Log::e("unable to setup logfile - fallback to console log");
    }
  CrashLog::setup();

  zenkit::Logger::set(zenkit::LogLevel::INFO, [] (zenkit::LogLevel lvl, const char* cat, const char* message) {
    (void)cat;
    switch (lvl) {
      case zenkit::LogLevel::ERROR:
        Tempest::Log::e("[zenkit] ", message);
        break;
      case zenkit::LogLevel::WARNING:
        Tempest::Log::e("[zenkit] ", message);
        break;
      case zenkit::LogLevel::INFO:
        Tempest::Log::i("[zenkit] ", message);
        break;
      case zenkit::LogLevel::DEBUG:
      case zenkit::LogLevel::TRACE:
        Tempest::Log::d("[zenkit] ", message); // unused
        break;
      }
    });
  Dm_setLogger(DmLogLevel_INFO, [](void* ctx, DmLogLevel lvl, char const* msg) {
    switch (lvl) {
      case DmLogLevel_FATAL:
      case DmLogLevel_ERROR:
      case DmLogLevel_WARN:
        Tempest::Log::e("[dmusic] ", msg);
        break;
      case DmLogLevel_INFO:
        Tempest::Log::i("[dmusic] ", msg);
        break;
      case DmLogLevel_DEBUG:
      case DmLogLevel_TRACE:
        Tempest::Log::d("[dmusic] ", msg);
        break;
      }
    }, nullptr);

  Tempest::Log::i(appBuild);
  Workers::setThreadName("Main thread");

  CommandLine          cmd{argc,argv};
  auto                 api     = mkApi(cmd);
  const auto           gpuName = selectDevice(*api);
  CrashLog::setGpu(gpuName);

  Tempest::Device      device{*api,gpuName};
  CrashLog::setGpu(device.properties().name);

  Resources            resources{device};
  Gothic               gothic;
  GameMusic            music;
  gothic.setupGlobalScripts();

  MainWindow           wx(device);
  Tempest::Application app;
  const int ret = app.exec();
#if defined(__ANDROID__)
  // The game runs in a process of its own and the process ends with it: the objects above do not have to be taken apart one by one.
  // Their destruction crashed after the game had ended (SIGSEGV in zenkit::Vfs::~Vfs from ~Resources, Mali-G720, Android 16) and the launcher
  // took it for a crash of the game. Nothing is kept in them that is not written already (the settings are flushed when they change).
  Tempest::Log::i("exit ",ret);
  if(appLogFile!=nullptr)
    appLogFile->flush();
  std::_Exit(ret);
#endif
  return ret;
  }
