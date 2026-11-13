#pragma once

#include <filesystem>
#include <string>

/// A save folder of the original Gothic II (SAVEDAT.SAV, SAVEINFO.SAV, <WORLD>.SAV).
/// The state of it is put into a session that has been started as a new game.
struct OriginalSave final {
  std::filesystem::path dir;

  /// <game>/Saves/import, when a save of the original game is in it
  static bool        pending(OriginalSave& out);
  /// the level of the save, like "newworld.zen"
  static std::string worldFile(const OriginalSave& src);

  /// the folder is put aside after an attempt, so the next New game is a plain one again
  static void        retire(const OriginalSave& src, bool ok, const std::string& title);
  /// the name for the native save that is made right after a successful import, once
  static bool        takeSaveName(std::string& out);
  };
