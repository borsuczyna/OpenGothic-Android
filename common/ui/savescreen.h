#pragma once

#include <Tempest/Event>
#include <Tempest/Painter>
#include <Tempest/Rect>
#include <Tempest/Texture2d>

#include <string>
#include <vector>

#include "game/savestore.h"

/// The screens of the saves for a phone: a list of every save there is (no limit of the slots) and the details of the chosen one.
/// The same screen loads a game and saves it. A save can be renamed and deleted here.
class SaveScreen final {
  public:
    enum Mode : uint8_t { Load, Save };

    /// what the menu has to do after a touch
    enum class Result : uint8_t { None, Close, Back };

    SaveScreen(Mode mode, bool polish);
    ~SaveScreen();

    void   paint(Tempest::Painter& p, int w, int h);
    void   touch(float x, float y, int phase, int w, int h);
    void   tap(float x, float y, int w, int h);

    /// the text of a name is typed: the keys have to come to key()
    bool   wantsText() const { return state==State::Rename; }
    void   key(const Tempest::KeyEvent& e);

    /// the keyboard of a computer: up and down choose a save, confirm does what the main button does
    void   step(int dir);
    void   confirm();
    /// closes what is open on the screen (the name, the question), false when there is nothing to close and the menu should go back
    bool   cancel();

    Result takeResult();

  private:
    enum class State : uint8_t { Browse, Confirm, Rename };
    enum class Ask   : uint8_t { Delete, Overwrite };
    enum class Act   : uint8_t { None, Back, Delete, Rename, Primary, Yes, No };

    struct Row {
      bool            fresh = false;   // a new save
      SaveStore::Entry e;
      };

    struct Button {
      Tempest::Rect rect;
      Act           act     = Act::None;
      std::string   label;
      bool          primary = false;
      bool          enabled = true;
      bool          arrow   = false;
      };

    struct Layout;

    Mode                  mode;
    State                 state = State::Browse;
    Ask                   ask   = Ask::Delete;
    Result                result = Result::None;
    bool                  polish = false;
    int                   lastW  = 1920;
    int                   lastH  = 1080;

    std::vector<Row>      rows;
    int                   sel     = 0;
    float                 scroll  = 0;
    float                 scrollMax = 0;
    std::string           freshName;
    std::string           text;          // the name that is typed
    bool                  textFresh = false;

    // touch
    int                   drag        = 0;     // 1 - the list follows the finger
    float                 startY      = 0;
    float                 startScroll = 0;

    // the picture of the chosen save
    Tempest::Texture2d    thumb;
    std::string           thumbFile;
    bool                  hasThumb = false;

    Layout                layout(int w, int h) const;
    std::vector<Button>   buttons(const Layout& l) const;
    const Row*            current() const;
    bool                  canSave() const;
    bool                  canLoad() const;
    SaveGameHeader        liveHeader(int32_t& chapter, int32_t& level) const;

    void                  reload(const std::string& keepFile);
    void                  select(int row);
    void                  ensureVisible(const Layout& l);
    void                  loadThumb();
    void                  doAct(Act a);
    void                  doSave(const std::string& file, const std::string& name);
    void                  startRename();
    void                  acceptRename();

    std::string           tr(const char* en, const char* pl) const { return polish ? pl : en; }
    /// "Quick save 3": the quick save has several files, the number tells which one it is
    std::string           quickTitle(const SaveStore::Entry& e) const {
      std::string t = tr("Quick save","Szybki zapis");
      if(e.quick>0)
        t += " "+std::to_string(e.quick);
      return t;
      }
    std::string           countText(size_t n) const;
  };
