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
static Tempest::WFile* appLogFile = nullptr; // log tej sesji
#endif

int main(int argc, const char** argv) {
#if defined(__ANDROID__)
    {
        // Pobrany główny katalog danych aplikacji w pamięci wewnętrznej (/data/user/0/...)
        const std::filesystem::path appInternalDir = InstallDetect::androidInternalDataPath();
        
        // 1. Domyślny / Sztywny folder dla wyciągniętego ZIP-a (pamięć root aplikacji)
        const std::filesystem::path zipExtractedDir = appInternalDir; 
        
        // Zmienna na docelowy katalog roboczy gry
        std::filesystem::path finalGameDir;

        // Odczytujemy zapisany wpis ze ścieżką z LauncherActivity.java (plik game.txt)
        std::ifstream in(appInternalDir / "game.txt");
        std::string customPath;
        
        if (in && std::getline(in, customPath) && !customPath.empty()) {
            // Usunięcie ewentualnych białych znaków na końcu
            customPath.erase(customPath.find_last_not_of(" \n\r\t") + 1);
        }

        // --- OBSŁUGA DWÓCH TRYWÓW NA SZTYWNO ---
        if (!customPath.empty() && customPath[0] == '/') {
            // OPTION 2: Wybrany własny folder (np. menedżer plików / karta SD / pamięć zewnętrzna)
            // LauncherActivity zapisał pełną ścieżkę bezwzględną rozpoczynającą się od '/'
            finalGameDir = customPath;
            Tempest::Log::i("OpenGothic: Uruchamianie z wybranego folderu zewnętrznego: ", finalGameDir.c_str());
        } else {
            // OPTION 1: Uruchamianie z rozpakowanego pliku ZIP w pamięci wewnętrznej (root)
            // Jeśli customPath jest pusty lub jest ścieżką względną
            if (!customPath.empty()) {
                finalGameDir = zipExtractedDir / customPath;
            } else {
                finalGameDir = zipExtractedDir; // Główny root pamięci aplikacji po wypakowaniu ZIP
            }

            Tempest::Log::i("OpenGothic: Uruchamianie z pamięci wewnętrznej (ZIP root): ", finalGameDir.c_str());

            // Dla pamięci wewnętrznej upewniamy się, że struktura katalogu istnieje
            if (!std::filesystem::exists(finalGameDir)) {
                std::filesystem::create_directories(finalGameDir);
            }
        }

        // Ustawienie wybranego folderu jako bieżący katalog roboczy dla silnika i zapisów (savegame)
        std::filesystem::current_path(finalGameDir);
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
      if(mode==Tempest::Log::Debug) {
        SafeGpu::onLog(text);
        logFile.flush();
        }
#endif
      });
    }
  catch(...) {
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
        Tempest::Log::d("[zenkit] ", message);
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
  Tempest::Log::i("exit ",ret);
  if(appLogFile!=nullptr)
    appLogFile->flush();
  std::_Exit(ret);
#endif
  return ret;
  }
