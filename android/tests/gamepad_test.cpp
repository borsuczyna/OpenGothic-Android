// Host-side test of the gamepad model. It has no Android dependencies:
//   c++ -std=c++17 -I common android/tests/gamepad_test.cpp common/ui/gamepadmodel.cpp common/ui/mobilecontrolsmodel.cpp && ./a.out
#include "ui/gamepadmodel.h"

#include <cmath>
#include <cstdio>
#include <cstdlib>

using namespace Mobile;

#define CHECK(cond) \
  do { \
    if(!(cond)) { \
      std::fprintf(stderr,"%s:%d: check failed: %s\n",__FILE__,__LINE__,#cond); \
      std::abort(); \
      } \
    } while(false)

namespace {

struct Rig {
  Controls c;
  Gamepad  pad;
  uint64_t now = 1000;
  std::vector<Event> log;

  explicit Rig(Mode mode=Mode::Game) {
    c.configure(1920,1080,mode);
    c.availability(~uint64_t(0),true,true,true);
    frame(0);
    }

  /// one frame, `ms` after the previous one; the events are kept
  void frame(uint64_t ms=16) {
    frameIn(c.mode(),ms);
    }

  void frameIn(Mode mode, uint64_t ms=16) {
    now += ms;
    pad.update(c,mode,now,1.f);
    for(auto& e:c.drain())
      log.push_back(e);
    }

  size_t count(Event::Kind kind, Key key) const {
    size_t n = 0;
    for(auto& e:log)
      n += (e.kind==kind && e.key==key) ? 1 : 0;
    return n;
    }

  size_t specials(int id) const {
    size_t n = 0;
    for(auto& e:log)
      n += (e.kind==Event::Special && int(e.code)==id) ? 1 : 0;
    return n;
    }

  float yaw() const {
    float v = 0;
    for(auto& e:log)
      v += e.kind==Event::Look ? e.x : 0;
    return v;
    }

  float pitch() const {
    float v = 0;
    for(auto& e:log)
      v += e.kind==Event::Look ? e.y : 0;
    return v;
    }

  void clear() { log.clear(); }
  };

void leftStick() {
  Rig r;
  r.pad.axes(0,-1,0,0,0,0);          // forward
  r.frame();
  CHECK(r.count(Event::Down,Key::Up)==1);
  CHECK(r.count(Event::Down,Key::StrafeLeft)==0 && r.count(Event::Down,Key::StrafeRight)==0);
  r.pad.axes(0.05f,-0.1f,0,0,0,0);   // inside of the dead zone
  r.frame();
  CHECK(r.count(Event::Up,Key::Up)==1);
  }

void strafe() {
  Rig r;
  r.pad.axes(1,0,0,0,0,0);
  r.frame();
  CHECK(r.count(Event::Down,Key::StrafeRight)==1);
  CHECK(r.count(Event::Down,Key::Up)==0);
  CHECK(r.yaw()==0);                 // the left stick never turns the character
  r.pad.axes(-1,0,0,0,0,0);
  r.frame();
  CHECK(r.count(Event::Up,Key::StrafeRight)==1);
  CHECK(r.count(Event::Down,Key::StrafeLeft)==1);

  // diagonal: forward and a side step together
  r.clear();
  r.pad.axes(0.8f,-0.8f,0,0,0,0);
  r.frame();
  CHECK(r.count(Event::Down,Key::Up)==1);
  CHECK(r.count(Event::Down,Key::StrafeRight)==1);
  r.pad.axes(0,0,0,0,0,0);
  r.frame();
  CHECK(r.count(Event::Up,Key::StrafeRight)==1 && r.count(Event::Up,Key::Up)==1);
  }

void camera() {
  Rig r;
  r.pad.axes(0,0,1,0,0,0);
  for(int i=0; i<10; ++i)
    r.frame(100);
  CHECK(std::abs(r.yaw()-150.f)<1.f);   // 1 s of the full right deflection
  CHECK(r.pitch()==0);

  r.clear();
  r.pad.axes(0,0,0,1,0,0);               // down
  for(int i=0; i<10; ++i)
    r.frame(100);
  CHECK(r.pitch()>80.f && r.pitch()<95.f);

  r.clear();
  r.pad.axes(0,0,0.1f,0.1f,0,0);         // inside of the dead zone
  r.frame(100);
  CHECK(r.yaw()==0 && r.pitch()==0);

  // a small push moves the camera slowly
  r.pad.axes(0,0,0.4f,0,0,0);
  r.frame(1000);
  const float slow = r.yaw();
  CHECK(slow>0 && slow<150.f*0.4f);
  }

void buttons() {
  Rig r;
  r.pad.button(Pad::A,true);
  r.frame();
  CHECK(r.count(Event::Down,Key::Action)==1);
  r.pad.axes(0,0,0,0,0,1);               // RT too: still one press of the key
  r.frame();
  CHECK(r.count(Event::Down,Key::Action)==1);
  r.pad.button(Pad::A,false);
  r.frame();
  CHECK(r.count(Event::Up,Key::Action)==0);   // RT holds it
  r.pad.axes(0,0,0,0,0,0);
  r.frame();
  CHECK(r.count(Event::Up,Key::Action)==1);

  r.clear();
  r.pad.button(Pad::X,true);
  r.pad.button(Pad::B,true);
  r.frame();
  CHECK(r.count(Event::Down,Key::Jump)==1 && r.count(Event::Down,Key::Block)==1);

  r.clear();
  r.pad.button(Pad::Y,true);
  r.frame();
  r.frame();
  CHECK(r.count(Event::Down,Key::Weapon)==1 && r.count(Event::Up,Key::Weapon)==1);   // a tap, once for a hold

  r.clear();
  r.pad.button(Pad::Select,true);
  r.pad.button(Pad::DpadUp,true);
  r.pad.button(Pad::DpadLeft,true);
  r.pad.button(Pad::DpadRight,true);
  r.pad.button(Pad::Start,true);
  r.pad.button(Pad::LeftStick,true);
  r.frame();
  CHECK(r.count(Event::Down,Key::Inventory)==1);
  CHECK(r.count(Event::Down,Key::Map)==1);
  CHECK(r.count(Event::Down,Key::Status)==1);
  CHECK(r.count(Event::Down,Key::Log)==1);
  CHECK(r.count(Event::Down,Key::Escape)==1);
  CHECK(r.count(Event::Down,Key::Sneak)==1);

  r.clear();
  r.pad.button(Pad::DpadDown,true);
  r.pad.button(Pad::RightStick,true);
  r.frame();
  CHECK(r.specials(Special::Spell)==1);
  CHECK(r.specials(Special::Target)==1);
  }

void bumpers() {
  Rig r;
  // no weapon: the left bumper switches walking and running, the right does nothing
  r.c.availability(~(keyMask(Key::AttackLeft)|keyMask(Key::AttackRight)|keyMask(Key::Block)),true,true,true);
  r.pad.button(Pad::LeftBumper,true);
  r.pad.button(Pad::RightBumper,true);
  r.frame();
  CHECK(r.count(Event::Down,Key::Walk)==1);
  CHECK(r.count(Event::Down,Key::AttackLeft)==0 && r.count(Event::Down,Key::AttackRight)==0);

  // a melee weapon in the hand: the side hits
  Rig m;
  m.c.availability(~uint64_t(0),true,true,true);
  m.pad.button(Pad::LeftBumper,true);
  m.pad.button(Pad::RightBumper,true);
  m.frame();
  CHECK(m.count(Event::Down,Key::AttackLeft)==1 && m.count(Event::Down,Key::AttackRight)==1);
  CHECK(m.count(Event::Down,Key::Walk)==0);
  m.pad.button(Pad::LeftBumper,false);
  m.frame();
  CHECK(m.count(Event::Up,Key::AttackLeft)==1);
  }

void menu() {
  Rig r(Mode::Menu);
  r.pad.axes(0,0.9f,0,0,0,0);            // down
  r.frame();
  CHECK(r.count(Event::Down,Key::Down)==1 && r.count(Event::Up,Key::Down)==1);
  r.clear();
  r.frame(300);
  CHECK(r.count(Event::Down,Key::Down)==0);        // not yet
  r.frame(100);
  CHECK(r.count(Event::Down,Key::Down)==1);        // after the delay
  r.clear();
  r.frame(110);
  CHECK(r.count(Event::Down,Key::Down)==1);        // and then often
  r.pad.axes(0,0,0,0,0,0);
  r.clear();
  r.frame(500);
  CHECK(r.count(Event::Down,Key::Down)==0);

  // across: the dominant axis only
  r.pad.axes(0.9f,0.6f,0,0,0,0);
  r.frame();
  CHECK(r.count(Event::Down,Key::Right)==1 && r.count(Event::Down,Key::Down)==0);
  r.pad.axes(0,0,0,0,0,0);
  r.frame();

  r.clear();
  r.pad.button(Pad::A,true);
  r.frame();
  CHECK(r.count(Event::Down,Key::Enter)==1);
  r.pad.button(Pad::A,false);
  r.pad.button(Pad::B,true);
  r.frame();
  CHECK(r.count(Event::Up,Key::Enter)==1 && r.count(Event::Down,Key::Escape)==1);
  }

void screenChange() {
  Rig r;
  r.pad.button(Pad::A,true);
  r.frame();
  CHECK(r.count(Event::Down,Key::Action)==1);
  r.clear();
  // the screen changes with A down: the key of the game is let go, the menu does not get a press that was not made for it
  r.frameIn(Mode::Menu);
  CHECK(r.count(Event::Up,Key::Action)==1);
  CHECK(r.count(Event::Down,Key::Enter)==0);
  r.pad.button(Pad::A,false);
  r.frameIn(Mode::Menu);
  r.pad.button(Pad::A,true);
  r.frameIn(Mode::Menu);
  CHECK(r.count(Event::Down,Key::Enter)==1);
  }

void inventory() {
  Rig r(Mode::Inventory);
  r.pad.button(Pad::RightBumper,true);
  r.frame();
  CHECK(r.pad.takeCategoryStep()==1);
  CHECK(r.pad.takeCategoryStep()==0);
  r.pad.button(Pad::RightBumper,false);
  r.pad.button(Pad::LeftBumper,true);
  r.frame();
  CHECK(r.pad.takeCategoryStep()==-1);

  r.clear();
  r.pad.button(Pad::LeftBumper,false);
  r.pad.button(Pad::X,true);
  r.pad.button(Pad::Y,true);
  r.frame();
  CHECK(r.count(Event::Down,Key::Jump)==1 && r.count(Event::Down,Key::Weapon)==1);
  }

void lockpick() {
  Rig r(Mode::Lockpick);
  r.pad.axes(-1,0,0,0,0,0);
  r.frame();
  CHECK(r.count(Event::Down,Key::Left)==1);
  r.pad.axes(1,0,0,0,0,0);
  r.frame();
  CHECK(r.count(Event::Up,Key::Left)==1 && r.count(Event::Down,Key::Right)==1);
  r.clear();
  r.pad.button(Pad::B,true);
  r.frame();
  CHECK(r.count(Event::Down,Key::Down)==1);        // cancel
  }

void transformation() {
  // the game offers the key Enter only while a transformation lasts; A then turns the character back
  Rig r;
  r.c.availability(~keyMask(Key::Enter),true,true,true);
  r.pad.button(Pad::A,true);
  r.frame();
  CHECK(r.count(Event::Down,Key::Enter)==0);
  r.pad.button(Pad::A,false);
  r.frame();

  r.c.availability(~uint64_t(0),true,true,true);
  r.clear();
  r.pad.button(Pad::A,true);
  r.frame();
  CHECK(r.count(Event::Down,Key::Enter)==1 && r.count(Event::Up,Key::Enter)==1);
  CHECK(r.count(Event::Down,Key::Action)==1);
  }

void independentOfTouch() {
  // the same key from a finger/keyboard and the pad is counted: letting go of one does not release the other
  Rig r;
  r.c.physical(Key::Action,true);
  r.pad.button(Pad::A,true);
  r.frame();
  CHECK(r.count(Event::Down,Key::Action)==1);
  r.pad.button(Pad::A,false);
  r.frame();
  CHECK(r.count(Event::Up,Key::Action)==0);
  CHECK(r.c.held(Key::Action));
  }

void blocked() {
  Rig r;
  r.c.showKeyboard(true);                // a text field has the focus
  r.pad.button(Pad::A,true);
  r.pad.axes(0,-1,0,0,0,0);
  r.frame();
  CHECK(r.count(Event::Down,Key::Action)==0 && r.count(Event::Down,Key::Up)==0);
  }

void badInput() {
  Rig r;
  r.pad.axes(NAN,INFINITY,-INFINITY,NAN,NAN,NAN);
  r.frame();
  CHECK(r.log.empty());
  r.pad.axes(9,9,9,9,9,9);               // out of range is clamped
  r.frame(1000);
  CHECK(r.yaw()<=15.01f);                 // a long pause between frames is not a long turn
  r.pad.reset();
  r.frame();
  CHECK(r.c.held(Key::Up)==false);
  }

}

int main() {
  leftStick();
  strafe();
  camera();
  buttons();
  bumpers();
  menu();
  screenChange();
  inventory();
  lockpick();
  transformation();
  independentOfTouch();
  blocked();
  badInput();
  std::puts("gamepad_test: ok");
  return 0;
  }
