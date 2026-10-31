#pragma once

#include <array>
#include <cstdint>

#include "ui/mobilecontrolsmodel.h"

/// A gamepad as a source of the same events as the touch controls. Has no Android/renderer dependencies, so it can be tested on the host.
///
/// The scheme follows zGamePad (the plugin of Union by Gratt), with the camera on the right stick:
///   game      left stick  walk, back and strafe (a stick to the side is a side step, the character does not turn)
///             right stick camera; R3 target; L3 sneak
///             A / RT      action (talk, take, attack, shoot, cast), held
///             B / LT      block, held
///             X jump, Y weapon, LB / RB side hits with a weapon in the hand (without it: LB walk/run, RB nothing)
///             D-pad       up map, left character, right log, down spells; Select inventory, Start menu
///   menus     stick or D-pad choose, A confirm, B back; inventory: X stack, Y drop, LB / RB category
///   lockpick  left / right turn the pick, B or down cancel
namespace Mobile {

enum class Pad : uint8_t {
  A, B, X, Y,
  LeftBumper, RightBumper, LeftStick, RightStick,
  Start, Select,
  DpadUp, DpadDown, DpadLeft, DpadRight,
  Count
  };

class Gamepad {
  public:
    /// the buttons; the triggers are the axes
    void  button(Pad pad, bool down);
    /// -1..1 (y is positive downwards, as on Android); the triggers 0..1
    void  axes(float lx, float ly, float rx, float ry, float lt, float rt);
    /// all buttons and sticks are let go (the pad has gone, the window has lost the focus)
    void  reset();
    /// any input has come from a pad
    bool  seen() const { return anyInput; }

    /// Every frame. `look` is the sensitivity of the camera (1 is the default).
    void  update(Controls& controls, Mode mode, uint64_t now, float look);

    /// -1 / +1 when a bumper has asked for the previous / next category of the inventory since the last call
    int   takeCategoryStep();

  private:
    enum Direction : int { DirUp, DirDown, DirLeft, DirRight, DirCount };

    struct Repeat {
      bool     on   = false;
      uint64_t next = 0;
      };

    bool  pressed(Pad pad) const { return held[size_t(pad)] && !latched[size_t(pad)]; }
    bool  edge   (Pad pad) const { return pressed(pad) && !before[size_t(pad)]; }
    void  release(Controls& controls);
    void  game    (Controls& controls, float dt, float look);
    void  navigate(Controls& controls, uint64_t now);
    void  lockpick(Controls& controls);
    void  step(Controls& controls, Direction d, Key key, bool on, uint64_t now);

    static bool  hysteresis(bool& state, float value, float on, float off);
    static void  radial(float x, float y, float dead, float& ox, float& oy);

    std::array<bool,size_t(Pad::Count)> held{};     // as the pad reports
    std::array<bool,size_t(Pad::Count)> before{};   // pressed() at the previous update
    std::array<bool,size_t(Pad::Count)> latched{};  // down when the screen changed: ignored until it is let go
    float lx = 0, ly = 0, rx = 0, ry = 0, lt = 0, rt = 0;
    bool  anyInput = false;

    Mode     mode      = Mode::Loading;
    bool     blocked   = true;
    uint64_t lastTick  = 0;
    int      pendingCategory = 0;

    // the states of the sticks and the triggers; the dead band between pressing and letting go keeps a stick from flickering
    bool  moveUp = false, moveDown = false, moveLeft = false, moveRight = false;
    bool  triggerL = false, triggerR = false;
    std::array<bool,DirCount>   dir{};
    std::array<Repeat,DirCount> repeat{};
  };

}
