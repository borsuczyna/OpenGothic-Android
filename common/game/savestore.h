#pragma once

#include <Tempest/Pixmap>

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

#include "savegameheader.h"

/// The saves on the disk. There is no limit of the slots: every file "<game>save_slot_N.sav" is a save,
/// a new one gets the first number that nobody has (0 is the old quick save).
/// The quick save has QuickSlots files "<game>quicksave_N.sav" (N is 1..QuickSlots) that are written in turn: after the last
/// one the first is written again, so a damaged quick save never leaves the player without the previous ones.
class SaveStore final {
  public:
    static constexpr size_t QuickSlots = 5;

    struct Entry {
      size_t         slot    = 0;
      std::string    file;
      SaveGameHeader hdr;
      int32_t        chapter = 0;    // 0 - the save does not tell (made before the list of the saves)
      int32_t        level   = 0;
      std::string    topic;          // the topic of the journal that has changed last
      int64_t        stamp   = 0;    // when it was made, seconds
      int            quick   = 0;    // 1..QuickSlots for a file of the quick save, 0 for the old slot 0 and for the other saves
      int64_t        written = 0;    // when the file was written, to tell which quick save is the newest

      bool           isQuick() const { return slot==0; }
      };

    /// the newest first
    static std::vector<Entry> list();
    /// the file name of the next save
    static std::string        newFile();
    /// the file name of quick save number n (1..QuickSlots)
    static std::string        quickFile(size_t n);
    /// takes the next quick slot in turn (1..QuickSlots), the file name is in `file`
    static size_t             nextQuick(std::string& file);
    /// the newest quick save that can be read (the old slot 0 too), false when there is none
    static bool               newestQuick(Entry& out);

    static bool               remove(const Entry& e);
    /// writes the save again, with the new name in the header
    static bool               rename(const Entry& e, std::string_view name);
    static Tempest::Pixmap    preview(const Entry& e);

    /// "Khorinis" for "NEWWORLD.ZEN"
    static std::string        placeName(std::string_view world, bool polish);
    /// "20.10.2026" and "21:34" with 0x1f between them: the screen draws a dot there, the font of the game may not have it
    static std::string        dateText(const SaveGameHeader& hdr);
    /// "23 h 27 min"
    static std::string        playTimeText(const SaveGameHeader& hdr);
  };
