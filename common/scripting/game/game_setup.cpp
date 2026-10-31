#include "scripting/gameapi.h"

#include <filesystem>

#include <Tempest/Log>
#ifdef __ANDROID__
#include <Tempest/AndroidApi>
#endif

#include "gothic.h"
#include "scripting/modutil.h"

using namespace Scripting;

namespace fs = std::filesystem;

/// Where the mods are, what the game gives to them and what starts with the game.
/// Desktop: the folder mods/ in the working directory. Android: mods/ of the files of the application,
/// the launcher puts the zip files there.
void game::setup() {
  auto& host = ModHost::inst();

  host.setGame(Gothic::inst().version().game==1 ? "G1" : "G2");
#if defined(__ANDROID__)
  host.setPlatform("android");
  host.setEnabledByDefault(false);   // the player chooses in the launcher
  fs::path root = Tempest::AndroidApi::externalDataPath();
  root /= "mods";
#else
#if defined(_WIN32)
  host.setPlatform("windows");
#elif defined(__APPLE__)
  host.setPlatform("macos");
#else
  host.setPlatform("linux");
#endif
  std::error_code ec;
  fs::path root = fs::current_path(ec)/"mods";
#endif

  std::error_code ec2;
  fs::create_directories(root,ec2);
  host.addRoot(root);
  host.setDataDir(root/".data");

  host.logSink = [](const ModHost::LogLine& l) {
    const char* tag = l.level==ModHost::LogLine::Error ? "error" : l.level==ModHost::LogLine::Warn ? "warn" : "info";
    Tempest::Log::i("[mods][",l.source,"][",tag,"] ",l.text);
    };

  host.binder = [](JSContext* ctx, Mod&) {
    // the prototype of the characters goes first, the others use it
    installNpc(ctx);
    installPlayer(ctx);
    installNpcs(ctx);
    installItems(ctx);
    installWorld(ctx);
    installQuests(ctx);
    installScreen(ctx);
    installInput(ctx);
    installTouch(ctx);
    installCombat(ctx);
    installUi(ctx);
    installDebug(ctx);
    };

  registerBuiltinMods(host);
  host.scan();
  host.startEnabled();
  }
