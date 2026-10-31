#include "gamepadmodel.h"

#include <algorithm>
#include <cmath>

using namespace Mobile;

namespace {

constexpr float    StickDead     = 0.18f;   // the dead zone of the left stick, the rest of the travel is scaled to 0..1
constexpr float    LookDead      = 0.15f;
constexpr float    LookYawRate   = 150.f;   // degrees per second at the full deflection of the right stick
constexpr float    LookPitchRate = 90.f;
constexpr float    LookCurve     = 1.6f;    // a small deflection moves the camera slower than a linear one: aiming
constexpr float    MoveOn        = 0.30f, MoveOff = 0.20f;
constexpr float    SideOn        = 0.35f, SideOff = 0.25f;
constexpr float    MenuOn        = 0.55f, MenuOff = 0.40f;
constexpr float    TriggerOn     = 0.50f, TriggerOff = 0.30f;
constexpr uint64_t RepeatDelay   = 380;    // ms before a held direction repeats in a menu
constexpr uint64_t RepeatEvery   = 110;

}

bool Gamepad::hysteresis(bool& state, float value, float on, float off) {
  state = state ? value>off : value>on;
  return state;
  }

void Gamepad::radial(float x, float y, float dead, float& ox, float& oy) {
  const float len = std::sqrt(x*x+y*y);
  if(len<=dead) {
    ox = oy = 0;
    return;
    }
  const float k = std::min(1.f,(len-dead)/(1.f-dead))/len;
  ox = x*k;
  oy = y*k;
  }

void Gamepad::button(Pad pad, bool down) {
  held[size_t(pad)] = down;
  anyInput = true;
  }

void Gamepad::axes(float lx_, float ly_, float rx_, float ry_, float lt_, float rt_) {
  auto clamp = [](float v) { return std::isfinite(v) ? std::clamp(v,-1.f,1.f) : 0.f; };
  lx = clamp(lx_);
  ly = clamp(ly_);
  rx = clamp(rx_);
  ry = clamp(ry_);
  lt = std::max(0.f,clamp(lt_));
  rt = std::max(0.f,clamp(rt_));
  anyInput = true;
  }

void Gamepad::reset() {
  held.fill(false);
  lx = ly = rx = ry = lt = rt = 0;
  }

int Gamepad::takeCategoryStep() {
  const int ret = pendingCategory;
  pendingCategory = 0;
  return ret;
  }

void Gamepad::release(Controls& controls) {
  for(size_t i=0; i<size_t(Key::Count); ++i)
    controls.padKey(Key(i),false);
  moveUp = moveDown = moveLeft = moveRight = false;
  triggerL = triggerR = false;
  dir.fill(false);
  repeat.fill(Repeat());
  }

void Gamepad::update(Controls& controls, Mode next, uint64_t now, float look) {
  const float dt = lastTick && now>=lastTick ? float(std::min<uint64_t>(now-lastTick,100))/1000.f : 0.f;
  lastTick = now;

  const bool block = controls.typing() || controls.settingsVisible() || next==Mode::Loading;
  if(next!=mode || block!=blocked) {
    // a button that is down while the screen changes belongs to the screen before: it does nothing until it is let go
    release(controls);
    mode    = next;
    blocked = block;
    latched = held;
    }
  for(size_t i=0; i<held.size(); ++i) {
    if(!held[i])
      latched[i] = false;
    }

  if(!blocked) {
    switch(mode) {
      case Mode::Game:
        game(controls,dt,look);
        break;
      case Mode::Menu:
      case Mode::Dialog:
      case Mode::Inventory:
        navigate(controls,now);
        break;
      case Mode::Lockpick:
        lockpick(controls);
        break;
      case Mode::Video:
        if(edge(Pad::A) || edge(Pad::B) || edge(Pad::Start))
          controls.padTap(Key::Escape);
        break;
      case Mode::Loading:
        break;
      }
    }

  for(size_t i=0; i<before.size(); ++i)
    before[i] = pressed(Pad(i));
  }

void Gamepad::game(Controls& c, float dt, float look) {
  // Left stick: forward, back and the side steps. The character turns with the right stick, so a push to the side is a strafe.
  float sx = 0, sy = 0;
  radial(lx,ly,StickDead,sx,sy);
  c.padKey(Key::Up,         hysteresis(moveUp,   -sy,MoveOn,MoveOff));
  c.padKey(Key::Down,       hysteresis(moveDown,  sy,MoveOn,MoveOff));
  c.padKey(Key::StrafeLeft, hysteresis(moveLeft, -sx,SideOn,SideOff));
  c.padKey(Key::StrafeRight,hysteresis(moveRight, sx,SideOn,SideOff));

  // Right stick: the camera
  float cx = 0, cy = 0;
  radial(rx,ry,LookDead,cx,cy);
  if(cx!=0 || cy!=0) {
    auto curve = [](float v) { return std::copysign(std::pow(std::abs(v),LookCurve),v); };
    c.padLook(curve(cx)*LookYawRate*dt*look,curve(cy)*LookPitchRate*dt*look);
    }

  const bool fire  = pressed(Pad::A) || hysteresis(triggerR,rt,TriggerOn,TriggerOff);
  const bool block = pressed(Pad::B) || hysteresis(triggerL,lt,TriggerOn,TriggerOff);
  c.padKey(Key::Action,fire);
  c.padKey(Key::Block, block);
  c.padKey(Key::Jump,  pressed(Pad::X));

  // The bumpers hit to the sides with a weapon in the hand; without it the left one switches walking and running
  const bool melee = c.available(Key::AttackLeft) || c.available(Key::AttackRight);
  c.padKey(Key::AttackLeft, melee && pressed(Pad::LeftBumper));
  c.padKey(Key::AttackRight,melee && pressed(Pad::RightBumper));
  if(!melee && edge(Pad::LeftBumper))
    c.padTap(Key::Walk);

  if(edge(Pad::Y))          c.padTap(Key::Weapon);
  if(edge(Pad::Select))     c.padTap(Key::Inventory);
  if(edge(Pad::Start))      c.padTap(Key::Escape);
  if(edge(Pad::LeftStick))  c.padTap(Key::Sneak);
  if(edge(Pad::DpadUp))     c.padTap(Key::Map);
  if(edge(Pad::DpadLeft))   c.padTap(Key::Status);
  if(edge(Pad::DpadRight))  c.padTap(Key::Log);
  if(edge(Pad::DpadDown))   c.padSpecial(Special::Spell);
  if(edge(Pad::RightStick)) c.padSpecial(Special::Target);
  }

void Gamepad::step(Controls& c, Direction d, Key key, bool on, uint64_t now) {
  auto& r = repeat[size_t(d)];
  if(!on) {
    r.on = false;
    return;
    }
  if(!r.on) {
    r.on   = true;
    r.next = now+RepeatDelay;
    c.padTap(key);
    return;
    }
  for(int n=0; n<3 && now>=r.next; ++n) {
    c.padTap(key);
    r.next += RepeatEvery;
    }
  if(now>=r.next)
    r.next = now+RepeatEvery;
  }

void Gamepad::navigate(Controls& c, uint64_t now) {
  // the stick chooses along the axis it is pushed along
  const bool horizontal = std::abs(lx)>std::abs(ly);
  const bool up    = pressed(Pad::DpadUp)    || hysteresis(moveUp,    horizontal ? 0.f : -ly,MenuOn,MenuOff);
  const bool down  = pressed(Pad::DpadDown)  || hysteresis(moveDown,  horizontal ? 0.f :  ly,MenuOn,MenuOff);
  const bool left  = pressed(Pad::DpadLeft)  || hysteresis(moveLeft,  horizontal ? -lx : 0.f,MenuOn,MenuOff);
  const bool right = pressed(Pad::DpadRight) || hysteresis(moveRight, horizontal ?  lx : 0.f,MenuOn,MenuOff);
  step(c,DirUp,   Key::Up,   up,   now);
  step(c,DirDown, Key::Down, down, now);
  step(c,DirLeft, Key::Left, left, now);
  step(c,DirRight,Key::Right,right,now);

  c.padKey(Key::Enter, pressed(Pad::A));
  c.padKey(Key::Escape,pressed(Pad::B));
  if(edge(Pad::Start))
    c.padTap(Key::Escape);

  if(mode==Mode::Menu && edge(Pad::Y))
    c.padTap(Key::Backspace);
  if(mode==Mode::Inventory) {
    if(edge(Pad::X))           c.padTap(Key::Jump);      // the stack
    if(edge(Pad::Y))           c.padTap(Key::Weapon);    // drop
    if(edge(Pad::Select))      c.padTap(Key::Escape);    // the same button opened it
    if(edge(Pad::LeftBumper))  pendingCategory = -1;
    if(edge(Pad::RightBumper)) pendingCategory = +1;
    }
  }

void Gamepad::lockpick(Controls& c) {
  const bool l =pressed(Pad::DpadLeft)  || hysteresis(moveLeft, -lx,MenuOn,MenuOff);
  const bool r = pressed(Pad::DpadRight) || hysteresis(moveRight, lx,MenuOn,MenuOff);
  c.padKey(Key::Left, l && !r);
  c.padKey(Key::Right,r && !l);
  if(edge(Pad::B) || edge(Pad::DpadDown) || edge(Pad::Select))
    c.padTap(Key::Down);
  }
