#include "androidinput.h"

#ifdef OPENGOTHIC_TOUCH_UI

#ifdef __ANDROID__
#include <Tempest/AndroidApi>
#endif
#include <Tempest/Application>
#include <Tempest/Event>
#include <Tempest/Painter>
#include <Tempest/SystemApi>
#include <Tempest/Window>
#ifdef __ANDROID__
#include <android/input.h>
#endif

#include <algorithm>
#include <cmath>

#include "gothic.h"
#include "resources.h"
#include "scripting/modhost.h"

#include "touchassets.h"

using namespace Tempest;
using Mobile::Key;

namespace {

constexpr int      GlyphAlpha = 48; // pixels below this are considered a halo, not a part of the glyph
constexpr char     Section[]  = "MOBILE";
// places of the buttons of the HUD: <id>_x, <id>_y (permille+1, 0: not set), <id>_h (hidden), <id>_s (size step+1)
constexpr char     HudSection[] = "MOBILE_HUD";

// Files of android/assets/touch-icons, in the order of AndroidInput::Icon
const char* const IconFiles[AndroidInput::IconCount] = {
  "pause.png", "documents.png", "journal.png", "map.png", "inventory.png", "sneak.png", "jump.png",
  "attack.png", "attack_left.png", "attack_right.png", "block.png", "weapon.png",
  "target.png", "interact.png", "context.png", "shoot.png", "cast.png", "spells.png",
  "up.png", "down.png", "confirm.png", "back.png", "transfer.png", "use.png",
  "joystick_base.png", "joystick_thumb.png", "button.png", "button_pressed.png", "slot.png", "slot_selected.png",
  "bar_empty.png", "health.png", "mana.png", "oxygen.png", "lungs.png",
  "settings.png", "walk.png", "left.png", "right.png", "keyboard.png",
  "all.png", "weapons.png", "armor.png", "jewelry.png", "magic.png",
  "food.png", "potion.png", "documents.png", "misc.png",
  "use_new.png", "drop.png", "transfer_new.png", "up_new.png", "down_new.png", "inv_slot.png", "inv_slot_selected.png",
  "skip.png", "talk.png", "target_lock.png", "fast_forward.png", "save.png"
  };

const char* const SettingNames[Mobile::SettingCount] = {
  "buttons", "resolution", "shadows", "effects", "fps", "deadzone", "sensitivity", "vibration", "gui",
  "speed", "hudnumbers", "hudxp", "strafe"
  };

// Gives access to the dispatch functions of SystemApi
struct Dispatch : SystemApi {
  using SystemApi::dispatchKeyDown;
  using SystemApi::dispatchKeyUp;
  };

Event::KeyType keyType(Key key) {
  switch(key) {
    case Key::Up:         return Event::K_Up;
    case Key::Down:       return Event::K_Down;
    case Key::Left:       return Event::K_Left;
    case Key::Right:      return Event::K_Right;
    case Key::Action:     return Event::K_LControl;
    case Key::Jump:       return Event::K_LAlt;
    case Key::Weapon:     return Event::K_Space;
    case Key::Inventory:  return Event::K_Tab;
    case Key::Enter:      return Event::K_Return;
    case Key::Escape:     return Event::K_ESCAPE;
    case Key::Log:        return Event::K_L;
    case Key::Map:        return Event::K_M;
    case Key::One:        return Event::K_1;
    case Key::Two:        return Event::K_2;
    case Key::Three:      return Event::K_3;
    case Key::Four:       return Event::K_4;
    case Key::Five:       return Event::K_5;
    case Key::Six:        return Event::K_6;
    case Key::Seven:      return Event::K_7;
    case Key::Eight:      return Event::K_8;
    case Key::Nine:       return Event::K_9;
    case Key::Zero:       return Event::K_0;
    case Key::Walk:       return Event::K_LShift;
    case Key::StrafeLeft: return Event::K_A;
    case Key::StrafeRight:return Event::K_D;
    case Key::Status:     return Event::K_B;
    case Key::Backspace:  return Event::K_Back;
    case Key::Block:      return Event::K_Q;
    case Key::AttackLeft: return Event::K_Z;
    case Key::AttackRight:return Event::K_X;
    case Key::Sneak:      return Event::K_C;
    case Key::Count:      break;
    }
  return Event::K_NoKey;
  }

#ifdef __ANDROID__
Key translate(int32_t keyCode) {
  switch(keyCode) {
    case AKEYCODE_DPAD_UP:
    case AKEYCODE_W:
      return Key::Up;
    case AKEYCODE_DPAD_DOWN:
    case AKEYCODE_S:
      return Key::Down;
    case AKEYCODE_DPAD_LEFT:
      return Key::Left;
    case AKEYCODE_DPAD_RIGHT:
      return Key::Right;
    case AKEYCODE_A:
      return Key::StrafeLeft;
    case AKEYCODE_D:
      return Key::StrafeRight;
    case AKEYCODE_CTRL_LEFT:
    case AKEYCODE_BUTTON_A:
      return Key::Action;
    case AKEYCODE_ALT_LEFT:
    case AKEYCODE_BUTTON_X:
      return Key::Jump;
    case AKEYCODE_SPACE:
    case AKEYCODE_BUTTON_Y:
      return Key::Weapon;
    case AKEYCODE_TAB:
      return Key::Inventory;
    case AKEYCODE_ENTER:
    case AKEYCODE_BUTTON_START:
      return Key::Enter;
    case AKEYCODE_BACK:
    case AKEYCODE_ESCAPE:
    case AKEYCODE_BUTTON_B:
      return Key::Escape;
    case AKEYCODE_L:
      return Key::Log;
    case AKEYCODE_M:
      return Key::Map;
    case AKEYCODE_SHIFT_LEFT:
      return Key::Walk;
    case AKEYCODE_B:
      return Key::Status;
    case AKEYCODE_DEL:
      return Key::Backspace;
    case AKEYCODE_Q:
      return Key::Block;
    case AKEYCODE_C:
      return Key::Sneak;
    case AKEYCODE_0:
      return Key::Zero;
    default:
      break;
    }
  if(AKEYCODE_1<=keyCode && keyCode<=AKEYCODE_9)
    return Key(int(Key::One)+keyCode-AKEYCODE_1);
  return Key::Count;
  }
#endif

int nextValue(int setting, int value) {
  switch(setting) {
    case Mobile::SettingButtons:
      return value<60 ? 60 : value<80 ? 80 : value<100 ? 100 : value<120 ? 120 : value<140 ? 140 : 60;
    case Mobile::SettingResolution:
      return value==25 ? 50 : value==50 ? 75 : value==75 ? 100 : 25;
    case Mobile::SettingShadows:
      return value==0 ? 128 : value==128 ? 256 : value==256 ? 512 :
             value==512 ? 1024 : value==1024 ? 2048 : 0;
    case Mobile::SettingEffects:
      return value ? 0 : 1;
    case Mobile::SettingFps:
      return value==30 ? 45 : value==45 ? 60 : 30;
    case Mobile::SettingDeadzone:
      return value>=40 ? 15 : (value/5+1)*5;
    case Mobile::SettingSensitivity:
      return value>=180 ? 60 : value+20;
    case Mobile::SettingVibration:
      return value ? 0 : 1;
    case Mobile::SettingGui:
      return value>=160 ? 60 : value+10;
    case Mobile::SettingSpeed:
      return value>=300 ? 100 : value+50;
    case Mobile::SettingHudNumbers:
    case Mobile::SettingHudXp:
    case Mobile::SettingStrafe:
      return value ? 0 : 1;
    }
  return value;
  }

std::string settingText(int setting, const std::string& label) {
  const int value = Gothic::settingsGetI(Section,SettingNames[setting]);
  if(setting==Mobile::SettingEffects)
    return value ? "SSAO effects: ON" : "SSAO effects: OFF";
  if(setting==Mobile::SettingVibration)
    return value ? "Vibration: ON" : "Vibration: OFF";
  if(setting==Mobile::SettingHudNumbers)
    return value ? "HP/Mana numbers: ON" : "HP/Mana numbers: OFF";
  if(setting==Mobile::SettingHudXp)
    return value ? "XP counter: ON" : "XP counter: OFF";
  if(setting==Mobile::SettingStrafe)
    return value ? "Side step buttons: ON" : "Side step buttons: OFF";
  if(setting==Mobile::SettingSpeed)
    return label+"   "+std::to_string(value/100)+"."+std::to_string(value%100/10)+"x";

  std::string ret = label+"   "+std::to_string(value);
  const bool percent = setting==Mobile::SettingButtons || setting==Mobile::SettingResolution ||
                       setting==Mobile::SettingDeadzone || setting==Mobile::SettingSensitivity ||
                       setting==Mobile::SettingGui;
  if(percent)
    ret += "%";
  return ret;
  }

void fill(Painter& p, const Mobile::Rect& r, const Color& color) {
  p.setBrush(Brush(color));
  p.drawRect(int(r.x),int(r.y),int(r.w),int(r.h));
  }

void stroke(Painter& p, float x0, float y0, float x1, float y1, float thickness, const Color& color) {
  const float dx  = x1-x0;
  const float dy  = y1-y0;
  const float len = std::max(0.001f,std::sqrt(dx*dx+dy*dy));
  const float nx  = -dy/len*thickness/2;
  const float ny  =  dx/len*thickness/2;
  p.setBrush(Brush(color));
  p.drawTriangle(x0+nx,y0+ny,0.f,0.f, x0-nx,y0-ny,0.f,0.f, x1+nx,y1+ny,0.f,0.f);
  p.drawTriangle(x1+nx,y1+ny,0.f,0.f, x0-nx,y0-ny,0.f,0.f, x1-nx,y1-ny,0.f,0.f);
  }

/// Dark button with a light border
void frame(Painter& p, const Mobile::Rect& r, float border, bool active, bool enabled=true) {
  const float a = enabled ? 1.f : 0.45f;
  fill(p,r,active ? Color(0.80f,0.72f,0.55f,0.95f*a) : Color(0.52f,0.52f,0.52f,0.9f*a));
  fill(p,{r.x+border,r.y+border,r.w-2*border,r.h-2*border},
       active ? Color(0.22f,0.18f,0.10f,0.96f*a) : Color(0.03f,0.04f,0.05f,0.92f*a));
  }

void setFontSize(Painter& p, Font& font, float size) {
  font.setPixelSize(size);
  p.setFont(font);
  }

/// Centers the visible part of the text, not the line box. The text is drawn from its baseline.
void drawCentered(Painter& p, Font& font, const Mobile::Rect& r, const std::string& label,
                  const Color& color = Color(1.f,1.f,1.f)) {
  int pen = 0, left = 0, right = 0, top = 0, bottom = 0;
  bool any = false;
  for(unsigned char ch:label) {
    const auto g = font.letterGeometry(char32_t(ch));
    if(!g.size.isEmpty()) {
      const int l = pen+g.dpos.x;
      const int t = -g.dpos.y;
      if(!any) {
        left = l; right = l+g.size.w; top = t; bottom = t-g.size.h;
        any  = true;
        } else {
        left   = std::min(left,l);
        right  = std::max(right,l+g.size.w);
        top    = std::max(top,t);
        bottom = std::min(bottom,t-g.size.h);
        }
      }
    pen += g.advance.x;
    }
  if(!any)
    return;

  p.setBrush(Brush(color));
  p.drawText(int(r.x+(r.w-float(right-left))/2)-left,int(r.y+(r.h-float(top-bottom))/2)+top,label);
  }

}

AndroidInput::Preferences AndroidInput::preferences() {
  Preferences ret;
  if(Gothic::settingsGetI(Section,"version")==0)
    return ret;
  ret.buttons     = Gothic::settingsGetI(Section,SettingNames[Mobile::SettingButtons]);
  ret.resolution  = Gothic::settingsGetI(Section,SettingNames[Mobile::SettingResolution]);
  ret.shadows     = Gothic::settingsGetI(Section,SettingNames[Mobile::SettingShadows]);
  ret.effects     = Gothic::settingsGetI(Section,SettingNames[Mobile::SettingEffects]);
  ret.fps         = Gothic::settingsGetI(Section,SettingNames[Mobile::SettingFps]);
  ret.deadzone    = Gothic::settingsGetI(Section,SettingNames[Mobile::SettingDeadzone]);
  ret.sensitivity = Gothic::settingsGetI(Section,SettingNames[Mobile::SettingSensitivity]);
  if(Gothic::settingsGetI(Section,"version")>=2)
    ret.vibration = Gothic::settingsGetI(Section,SettingNames[Mobile::SettingVibration]);
  if(Gothic::settingsGetI(Section,"version")>=3)
    ret.gui = std::clamp(Gothic::settingsGetI(Section,SettingNames[Mobile::SettingGui]),50,200);
  if(Gothic::settingsGetI(Section,"version")>=4)
    ret.speed = std::clamp(Gothic::settingsGetI(Section,SettingNames[Mobile::SettingSpeed]),100,300);
  if(Gothic::settingsGetI(Section,"version")>=5) {
    ret.hudNumbers = Gothic::settingsGetI(Section,SettingNames[Mobile::SettingHudNumbers])!=0 ? 1 : 0;
    ret.hudXp      = Gothic::settingsGetI(Section,SettingNames[Mobile::SettingHudXp])!=0 ? 1 : 0;
    }
  if(Gothic::settingsGetI(Section,"version")>=6)
    ret.strafe = Gothic::settingsGetI(Section,SettingNames[Mobile::SettingStrafe])!=0 ? 1 : 0;
  return ret;
  }

void AndroidInput::Controller::bind() {
  if(Gothic::settingsGetI(Section,"version")==0) {
    const Preferences def;
    Gothic::settingsSetI(Section,SettingNames[Mobile::SettingButtons],    def.buttons);
    Gothic::settingsSetI(Section,SettingNames[Mobile::SettingResolution], def.resolution);
    Gothic::settingsSetI(Section,SettingNames[Mobile::SettingShadows],    def.shadows);
    Gothic::settingsSetI(Section,SettingNames[Mobile::SettingEffects],    def.effects);
    Gothic::settingsSetI(Section,SettingNames[Mobile::SettingFps],        def.fps);
    Gothic::settingsSetI(Section,SettingNames[Mobile::SettingDeadzone],   def.deadzone);
    Gothic::settingsSetI(Section,SettingNames[Mobile::SettingSensitivity],def.sensitivity);
    Gothic::settingsSetI(Section,SettingNames[Mobile::SettingVibration],  def.vibration);
    Gothic::settingsSetI(Section,SettingNames[Mobile::SettingGui],        def.gui);
    Gothic::settingsSetI(Section,SettingNames[Mobile::SettingSpeed],      def.speed);
    Gothic::settingsSetI(Section,SettingNames[Mobile::SettingHudNumbers], def.hudNumbers);
    Gothic::settingsSetI(Section,SettingNames[Mobile::SettingHudXp],      def.hudXp);
    Gothic::settingsSetI(Section,SettingNames[Mobile::SettingStrafe],     def.strafe);
    Gothic::settingsSetI(Section,"version",6);
    Gothic::flushSettings();
    } else
  if(Gothic::settingsGetI(Section,"version")<6) {
    // settings saved before the vibration, the scale of the interface, the fast forward and the side steps were added
    const Preferences def;
    const int version = Gothic::settingsGetI(Section,"version");
    if(version<2)
      Gothic::settingsSetI(Section,SettingNames[Mobile::SettingVibration],def.vibration);
    if(version<3)
      Gothic::settingsSetI(Section,SettingNames[Mobile::SettingGui],def.gui);
    if(version<4)
      Gothic::settingsSetI(Section,SettingNames[Mobile::SettingSpeed],def.speed);
    if(version<5) {
      Gothic::settingsSetI(Section,SettingNames[Mobile::SettingHudNumbers],def.hudNumbers);
      Gothic::settingsSetI(Section,SettingNames[Mobile::SettingHudXp],def.hudXp);
      }
    // the side step buttons were always there: they stay until the player switches them off
    Gothic::settingsSetI(Section,SettingNames[Mobile::SettingStrafe],def.strafe);
    Gothic::settingsSetI(Section,"version",6);
    Gothic::flushSettings();
    }
  loadHudPlaces();

#ifdef __ANDROID__
  AndroidApi::setInputHandler([](AInputEvent* event, void* user) {
    return static_cast<Controller*>(user)->onInput(event);
    },this);
#endif
  }

void AndroidInput::Controller::unbind() {
#ifdef __ANDROID__
  AndroidApi::setInputHandler(nullptr,nullptr);
#endif
  if(paused) {
    Gothic::inst().popPause();
    paused = false;
    }
  }

void AndroidInput::Controller::cancel() {
  controls.resetPointers();
  padState = PadState();
  pad.reset();
  }

#ifdef __ANDROID__
namespace {

/// the flags of the source are unsigned in the NDK
bool hasSource(int32_t source, uint32_t mask) {
  return (uint32_t(source) & mask)==mask;
  }

bool isPadSource(int32_t source) {
  return hasSource(source,uint32_t(AINPUT_SOURCE_GAMEPAD)) || hasSource(source,uint32_t(AINPUT_SOURCE_DPAD));
  }

/// The buttons that only a gamepad has; the D-pad is a gamepad's only when the source says so (a keyboard has the arrows too)
bool padButton(int32_t keyCode, int32_t source, Mobile::Pad& out, int& trigger) {
  trigger = 0;
  switch(keyCode) {
    case AKEYCODE_BUTTON_A:      out = Mobile::Pad::A;           return true;
    case AKEYCODE_BUTTON_B:      out = Mobile::Pad::B;           return true;
    case AKEYCODE_BUTTON_X:      out = Mobile::Pad::X;           return true;
    case AKEYCODE_BUTTON_Y:      out = Mobile::Pad::Y;           return true;
    case AKEYCODE_BUTTON_L1:     out = Mobile::Pad::LeftBumper;  return true;
    case AKEYCODE_BUTTON_R1:     out = Mobile::Pad::RightBumper; return true;
    case AKEYCODE_BUTTON_THUMBL: out = Mobile::Pad::LeftStick;   return true;
    case AKEYCODE_BUTTON_THUMBR: out = Mobile::Pad::RightStick;  return true;
    case AKEYCODE_BUTTON_START: out = Mobile::Pad::Start;       return true;
    case AKEYCODE_BUTTON_SELECT:
    case AKEYCODE_BUTTON_MODE:   out = Mobile::Pad::Select;      return true;
    case AKEYCODE_BUTTON_L2:     out = Mobile::Pad::A; trigger = -1; return true;
    case AKEYCODE_BUTTON_R2:     out = Mobile::Pad::A; trigger = +1; return true;
    case AKEYCODE_DPAD_UP:       out = Mobile::Pad::DpadUp;      return isPadSource(source);
    case AKEYCODE_DPAD_DOWN:     out = Mobile::Pad::DpadDown;    return isPadSource(source);
    case AKEYCODE_DPAD_LEFT:     out = Mobile::Pad::DpadLeft;    return isPadSource(source);
    case AKEYCODE_DPAD_RIGHT:    out = Mobile::Pad::DpadRight;   return isPadSource(source);
    default:
      break;
    }
  return false;
  }

}

void AndroidInput::Controller::pushPadAxes() {
  const auto& s = padState;
  pad.axes(s.lx,s.ly,s.rx,s.ry,std::max(s.lt,s.keyLt ? 1.f : 0.f),std::max(s.rt,s.keyRt ? 1.f : 0.f));
  static const Mobile::Pad dpad[4] = {Mobile::Pad::DpadUp,Mobile::Pad::DpadDown,Mobile::Pad::DpadLeft,Mobile::Pad::DpadRight};
  for(size_t i=0; i<4; ++i)
    pad.button(dpad[i],s.dpadKey[i] || s.dpadHat[i]);
  }

int32_t AndroidInput::Controller::onPadKey(AInputEvent* e) {
  Mobile::Pad button = Mobile::Pad::A;
  int         trigger = 0;
  const int32_t code = AKeyEvent_getKeyCode(e);
  if(!padButton(code,AInputEvent_getSource(e),button,trigger))
    return 0;

  const bool down = AKeyEvent_getAction(e)==AKEY_EVENT_ACTION_DOWN;
  if(AKeyEvent_getRepeatCount(e)!=0)
    return 1;
  if(trigger!=0) {
    (trigger<0 ? padState.keyLt : padState.keyRt) = down;
    } else
  if(code==AKEYCODE_DPAD_UP || code==AKEYCODE_DPAD_DOWN || code==AKEYCODE_DPAD_LEFT || code==AKEYCODE_DPAD_RIGHT) {
    const size_t i = code==AKEYCODE_DPAD_UP ? 0 : code==AKEYCODE_DPAD_DOWN ? 1 : code==AKEYCODE_DPAD_LEFT ? 2 : 3;
    padState.dpadKey[i] = down;
    } else {
    pad.button(button,down);
    }
  pushPadAxes();
  return 1;
  }

int32_t AndroidInput::Controller::onPadMotion(AInputEvent* e) {
  if((AMotionEvent_getAction(e) & AMOTION_EVENT_ACTION_MASK)!=AMOTION_EVENT_ACTION_MOVE)
    return 1;
  auto axis = [&](int32_t id) { return AMotionEvent_getAxisValue(e,id,0); };
  auto& s = padState;
  s.lx = axis(AMOTION_EVENT_AXIS_X);
  s.ly = axis(AMOTION_EVENT_AXIS_Y);
  // the right stick of the common pads (Xbox, DualShock/DualSense, Switch) is Z/RZ; RX/RY are the triggers of some of them
  s.rx = axis(AMOTION_EVENT_AXIS_Z);
  s.ry = axis(AMOTION_EVENT_AXIS_RZ);
  s.lt = std::max(axis(AMOTION_EVENT_AXIS_LTRIGGER),axis(AMOTION_EVENT_AXIS_BRAKE));
  s.rt = std::max(axis(AMOTION_EVENT_AXIS_RTRIGGER),axis(AMOTION_EVENT_AXIS_GAS));
  const float hx = axis(AMOTION_EVENT_AXIS_HAT_X);
  const float hy = axis(AMOTION_EVENT_AXIS_HAT_Y);
  s.dpadHat = {hy<-0.5f, hy>0.5f, hx<-0.5f, hx>0.5f};
  pushPadAxes();
  return 1;
  }

int32_t AndroidInput::Controller::onInput(AInputEvent* e) {
  if(AInputEvent_getType(e)==AINPUT_EVENT_TYPE_KEY) {
    if(onPadKey(e))
      return 1;
    const auto key = translate(AKeyEvent_getKeyCode(e));
    if(key==Key::Count)
      return 0;
    if(AKeyEvent_getRepeatCount(e)==0)
      controls.physical(key,AKeyEvent_getAction(e)==AKEY_EVENT_ACTION_DOWN);
    return 1;
    }
  if(AInputEvent_getType(e)!=AINPUT_EVENT_TYPE_MOTION)
    return 0;

  // The sticks and the triggers of a gamepad are motion with axes, not touches: they must never move a pointer
  const int32_t source = AInputEvent_getSource(e);
  if(hasSource(source,uint32_t(AINPUT_SOURCE_JOYSTICK)) && !hasSource(source,uint32_t(AINPUT_SOURCE_TOUCHSCREEN)))
    return onPadMotion(e);

  const int32_t action = AMotionEvent_getAction(e);
  const int32_t masked = action & AMOTION_EVENT_ACTION_MASK;
  const size_t  index  = size_t((action & AMOTION_EVENT_ACTION_POINTER_INDEX_MASK)
                                >> AMOTION_EVENT_ACTION_POINTER_INDEX_SHIFT);
  const size_t  count  = AMotionEvent_getPointerCount(e);

  if(masked==AMOTION_EVENT_ACTION_CANCEL || masked==AMOTION_EVENT_ACTION_OUTSIDE) {
    controls.resetPointers();
    return 1;
    }

  if(masked==AMOTION_EVENT_ACTION_MOVE) {
    for(size_t i=0; i<count; ++i)
      controls.move(AMotionEvent_getPointerId(e,i),AMotionEvent_getX(e,i),AMotionEvent_getY(e,i));
    } else
  if(index<count) {
    const int32_t id = AMotionEvent_getPointerId(e,index);
    const float   x  = AMotionEvent_getX(e,index);
    const float   y  = AMotionEvent_getY(e,index);
    if(masked==AMOTION_EVENT_ACTION_DOWN || masked==AMOTION_EVENT_ACTION_POINTER_DOWN)
      controls.down(id,x,y,Application::tickCount()); else
    if(masked==AMOTION_EVENT_ACTION_UP || masked==AMOTION_EVENT_ACTION_POINTER_UP)
      controls.up(id,x,y);
    }

  // All motion is owned here. It must never become a mouse click in MenuRoot.
  return 1;
  }
#endif

void AndroidInput::Controller::pointerDown(int id, float x, float y) {
  controls.down(id,x,y,Application::tickCount());
  }

void AndroidInput::Controller::pointerMove(int id, float x, float y) {
  controls.move(id,x,y);
  }

void AndroidInput::Controller::pointerUp(int id, float x, float y) {
  controls.up(id,x,y);
  }

void AndroidInput::Controller::physical(Mobile::Key key, bool down) {
  controls.physical(key,down);
  }

void AndroidInput::Controller::applyPreferences() {
  const auto pref = preferences();
  controls.preferences(float(pref.buttons)/100.f,float(pref.deadzone)/100.f,float(pref.sensitivity)/100.f);
  padSensitivity = float(pref.sensitivity)/100.f;
  // the game is vanilla: the fast forward is on the HUD and in the settings only when a mod has asked for it
  controls.features(Scripting::ModHost::inst().fastForwardWanted(),pref.strafe!=0);
  }

void AndroidInput::Controller::loadHudPlaces() {
  std::map<std::string,Mobile::HudPlace> places;
  for(const char* id:Mobile::HudIds) {
    const std::string key = id;
    const int x = Gothic::settingsGetI(HudSection,(key+"_x").c_str());
    const int y = Gothic::settingsGetI(HudSection,(key+"_y").c_str());
    const int h = Gothic::settingsGetI(HudSection,(key+"_h").c_str());
    const int sz = Gothic::settingsGetI(HudSection,(key+"_s").c_str());   // 0: the normal size, else step+1
    Mobile::HudPlace place;
    if(x>0 && y>0) {
      place.placed = true;
      place.x      = std::clamp(float(x-1)/1000.f,0.f,1.f);
      place.y      = std::clamp(float(y-1)/1000.f,0.f,1.f);
      }
    place.hidden = h!=0;
    if(sz>=1 && sz<=Mobile::HudSizeSteps)
      place.size = sz-1;
    if(place.placed || place.hidden || place.size!=Mobile::HudPlace().size)
      places[key] = place;
    }
  controls.hudPlaces(places);
  }

void AndroidInput::Controller::saveHudPlaces() {
  const auto& places = controls.hudPlaces();
  for(const char* id:Mobile::HudIds) {
    const std::string key = id;
    const auto        it  = places.find(key);
    const bool        has = it!=places.end();
    const auto        p   = has ? it->second : Mobile::HudPlace();
    Gothic::settingsSetI(HudSection,(key+"_x").c_str(),has && p.placed ? int(std::lround(p.x*1000.f))+1 : 0);
    Gothic::settingsSetI(HudSection,(key+"_y").c_str(),has && p.placed ? int(std::lround(p.y*1000.f))+1 : 0);
    Gothic::settingsSetI(HudSection,(key+"_h").c_str(),has && p.hidden ? 1 : 0);
    Gothic::settingsSetI(HudSection,(key+"_s").c_str(),has && p.size!=Mobile::HudPlace().size ? p.size+1 : 0);
    }
  Gothic::flushSettings();
  }

bool AndroidInput::Controller::update(Window& window, Mobile::Mode mode) {
  const bool changed = controls.mode()!=mode;
  screenW = float(window.w());
  screenH = float(window.h());
  controls.configure(screenW,screenH,mode);
  applyPreferences();
  if(paused && !controls.settingsVisible()) {
    Gothic::inst().popPause();
    paused = false;
    }

  controls.tick(Application::tickCount());
  pad.update(controls,mode,Application::tickCount(),padSensitivity);
  if(const int step = pad.takeCategoryStep(); step!=0 && action) {
    const int count = Mobile::CategoryCount;
    action(Mobile::Special::FirstCategory+(hud.inventoryCategory+step+count)%count);
    }
  for(const auto& input:controls.drain())
    dispatch(window,mode,input);
  if(controls.takeHudChanged())
    saveHudPlaces();
  return changed;
  }

void AndroidInput::Controller::dispatch(Window& window, Mobile::Mode mode, const Mobile::Event& input) {
  switch(input.kind) {
    case Mobile::Event::MenuTouch:
      if(menuTouch)
        menuTouch(input.x,input.y,int(input.code));
      break;
    case Mobile::Event::Tap:
      if(menuTap)
        menuTap(input.x,input.y);
      break;
    case Mobile::Event::Scroll:
      if(menuScroll)
        menuScroll(input.y);
      break;
    case Mobile::Event::Special:
      onSpecial(int(input.code));
      break;
    case Mobile::Event::Look:
      if(mode==Mobile::Mode::Game && look)
        look(input.x,input.y);
      break;
    case Mobile::Event::Text: {
      // Text input has no key: the character is the key code
      KeyEvent down(input.code,Event::M_NoModifier,Event::KeyDown);
      KeyEvent up  (input.code,Event::M_NoModifier,Event::KeyUp);
      Dispatch::dispatchKeyDown(window,down,256+input.code);
      Dispatch::dispatchKeyUp  (window,up,  256+input.code);
      break;
      }
    case Mobile::Event::Down:
    case Mobile::Event::Up: {
      const auto key  = keyType(input.key);
      const auto code = Mobile::KeyCodeBase+uint32_t(input.key);
      if(input.kind==Mobile::Event::Down) {
        KeyEvent event(key,code,Event::M_NoModifier,Event::KeyDown);
        Dispatch::dispatchKeyDown(window,event,code);
        } else {
        KeyEvent event(key,code,Event::M_NoModifier,Event::KeyUp);
        Dispatch::dispatchKeyUp(window,event,code);
        }
      break;
      }
    }
  }

void AndroidInput::Controller::onSpecial(int id) {
  if(id==Mobile::Special::Settings || id==Mobile::Special::EditHud || id==Mobile::Special::HudDone) {
    // The game stays paused while the settings are open
    if(controls.settingsVisible() && !paused) {
      Gothic::inst().pushPause();
      paused = true;
      } else
    if(paused) {
      Gothic::inst().popPause();
      paused = false;
      }
    return;
    }
  if(id==Mobile::Special::HudReset)
    return;

  const int setting = id-Mobile::Special::FirstSetting;
  if(0<=setting && setting<Mobile::SettingCount) {
    const char* name = SettingNames[setting];
    const int   next = nextValue(setting,Gothic::settingsGetI(Section,name));
    Resources::device().waitIdle();
    Gothic::settingsSetI(Section,name,next);
    Gothic::flushSettings();
    applyPreferences();
    return;
    }

  if(action)
    action(id);
  }

void AndroidInput::Controller::releaseIcons() {
  for(auto& i:icons)
    i = Sprite();
  }

void AndroidInput::Controller::loadIcons(TextureAtlas& atlas) {
  for(size_t i=0; i<icons.size(); ++i) {
    const Pixmap pm = TouchAsset::pixmap(IconFiles[i]);
    if(pm.w()!=pm.h() || pm.format()!=TextureFormat::RGBA8)
      throw std::runtime_error(std::string("unexpected format of the touch icon: ")+IconFiles[i]);
    const int iconSize = int(pm.w());
    iconPx[i] = float(iconSize);
    icons[i] = atlas.load(pm);
    // The last reference to a page of the atlas crashes in Tempest (RectAllocator::Node::decref climbs to the root of the
    // page and calls remove() of its null owner). One reference of every icon is kept until the process ends.
    static auto* keepAlive = new std::vector<Sprite>();
    keepAlive->push_back(icons[i]);

    // Glyphs are centered by their visible pixels, not by the bounds of the cell
    const auto* data = reinterpret_cast<const uint8_t*>(pm.data());
    int x0 = iconSize, y0 = iconSize, x1 = -1, y1 = -1;
    for(int y=0; y<iconSize; ++y) {
      for(int x=0; x<iconSize; ++x) {
        if(data[(size_t(y)*size_t(iconSize)+size_t(x))*4+3]<GlyphAlpha)
          continue;
        x0 = std::min(x0,x);
        x1 = std::max(x1,x);
        y0 = std::min(y0,y);
        y1 = std::max(y1,y);
        }
      }
    if(x1>=x0)
      ink[i] = {float(x0),float(y0),float(x1-x0+1),float(y1-y0+1)}; else
      ink[i] = {0,0,iconPx[i],iconPx[i]};
    }
  }

int AndroidInput::Controller::iconFor(const Mobile::Button& b) const {
  const bool game      = controls.gameLayout();
  const bool inventory = controls.mode()==Mobile::Mode::Inventory;

  if(b.character!=0)
    return NoIcon;

  switch(b.special) {
    case Mobile::Special::None:
      break;
    case Mobile::Special::Keyboard:
      return IconKeyboard;
    case Mobile::Special::Settings:
      return controls.settingsVisible() ? IconBack : IconSettings;
    case Mobile::Special::Target:
      // the lock of the aim is on: the icon of the lock
      return hud.targetLocked ? IconTargetLock : IconTarget;
    case Mobile::Special::Spell:
      return IconSpells;
    case Mobile::Special::Speed:
      return IconFastForward;
    case Mobile::Special::QuickSave:
      return IconSave;
    default:
      if(inventory && b.special>=Mobile::Special::FirstCategory)
        return IconCategoryAll+b.special-Mobile::Special::FirstCategory;
      return NoIcon;
    }

  if(game && b.key==Key::Action)
    return hud.actionIcon;

  if(inventory) {
    switch(b.key) {
      case Key::Enter:  return IconInvUse;
      case Key::Weapon: return hud.inventoryTransfer ? IconInvTransfer : IconInvDrop;
      case Key::Jump:   return IconInvTransfer;
      case Key::Up:     return IconInvUp;
      case Key::Down:   return IconInvDown;
      default:          break;
      }
    }

  if(controls.mode()==Mobile::Mode::Video && b.key==Key::Escape)
    return IconSkip;
  if(controls.mode()==Mobile::Mode::Dialog && b.label=="MENU")
    return IconPause;
  if(controls.mode()==Mobile::Mode::Lockpick && b.key==Key::Down)
    return IconBack;

  switch(b.key) {
    case Key::Escape:     return game ? IconPause : IconBack;
    case Key::Status:     return IconStatus;
    case Key::Log:        return IconJournal;
    case Key::Map:        return IconMap;
    case Key::Jump:       return game ? IconJump : IconTransfer;
    case Key::AttackLeft: return IconAttackLeft;
    case Key::AttackRight:return IconAttackRight;
    case Key::Walk:       return IconWalk;
    case Key::Left:       return IconLeft;
    case Key::Right:      return IconRight;
    case Key::StrafeLeft: return IconLeft;
    case Key::StrafeRight:return IconRight;
    case Key::Block:      return IconBlock;
    case Key::Sneak:      return IconSneak;
    case Key::Inventory:  return IconInventory;
    case Key::Weapon:     return game ? IconWeapon : IconTransfer;
    case Key::Up:         return game ? NoIcon : IconUp;
    case Key::Down:       return game ? NoIcon : IconDown;
    case Key::Enter:      return game ? IconBack : IconConfirm;
    default:              break;
    }
  return NoIcon;
  }

void AndroidInput::Controller::paintSprite(Painter& p, Icon id, const Mobile::Rect& r, float alpha) const {
  p.setBrush(Brush(icons[size_t(id)],Color(1.f,1.f,1.f,alpha)));
  p.drawRect(r.x,r.y,r.w,r.h,0.f,0.f,iconPx[size_t(id)],iconPx[size_t(id)]);
  }

void AndroidInput::Controller::paintHotbarSlot(Painter& p, Font&, size_t index) {
  const auto& b = controls.buttons()[index];
  const auto& r = b.rect;

  fill(p,r,Color(0.008f,0.012f,0.018f,0.88f));
  const bool selected = hud.active==int(b.key)-int(Key::One);
  paintSprite(p,controls.pressed(index) || selected ? IconInvSlotSelected : IconInvSlot,r,b.enabled ? 0.95f : 0.65f);
  }

void AndroidInput::Controller::paintButton(Painter& p, Font& font, size_t index) {
  const auto& b      = controls.buttons()[index];
  const bool  active = controls.pressed(index) || (b.special==Mobile::Special::Speed && hud.fastForward) ||
                       (b.special==Mobile::Special::Target && hud.targetLocked);
  const bool  game      = controls.gameLayout();
  const bool  inventory = controls.mode()==Mobile::Mode::Inventory;
  const float s         = controls.unit();
  auto        r         = b.rect;

  if(inventory) {
    paintInventoryButton(p,font,index);
    return;
    }

  const int icon = iconFor(b);
  if(icon==NoIcon) {
    if(game && Key::One<=b.key && b.key<=Key::Zero) {
      paintHotbarSlot(p,font,index);
      return;
      }

    // Plain text button with a border
    fill(p,r,active ? Color(0.85f,0.7f,0.42f,0.95f) : Color(0.5f,0.48f,0.42f,0.7f));
    const float border = 1.5f*s;
    r.x += border;
    r.y += border;
    r.w -= 2*border;
    r.h -= 2*border;
    fill(p,r,active ? Color(0.35f,0.22f,0.07f,0.9f) : Color(0.035f,0.03f,0.025f,0.65f));

    setFontSize(p,font,(controls.settingsVisible() ? 18.f : game ? 11.f : 16.f)*s);
    const int setting = b.special-Mobile::Special::FirstSetting;
    if(0<=setting && setting<Mobile::SettingCount)
      drawCentered(p,font,r,settingText(setting,b.label)); else
      drawCentered(p,font,r,b.label);
    return;
    }

  const float cx  = r.x+r.w/2;
  const float cy  = r.y+r.h/2;
  const float rad = std::min(r.w,r.h)/2;

  // Top bar buttons are drawn as bare glyphs
  const bool utility = game && (b.key==Key::Escape || b.key==Key::Status || b.key==Key::Log ||
                                b.key==Key::Map || b.key==Key::Inventory);
  // a button that is switched off is only a ghost in the editor of the HUD
  const float ghost = b.dimmed ? 0.3f : 1.f;
  paintSprite(p,active ? IconButtonPressed : IconButton,{cx-rad,cy-rad,rad*2,rad*2},ghost);

  const float alpha = (!b.enabled ? 0.28f : active ? 1.f : 0.9f)*ghost;
  p.setBrush(Brush(icons[size_t(icon)],Color(0.88f,0.92f,0.95f,alpha)));
  const auto& uv     = ink[size_t(icon)];
  const float size   = rad*(utility ? 1.35f : 1.32f);
  const float factor = size/std::max(uv.w,uv.h);
  const float iw     = uv.w*factor;
  const float ih     = uv.h*factor;
  p.drawRect(cx-iw/2,cy-ih/2,iw,ih,uv.x,uv.y,uv.x+uv.w,uv.y+uv.h);
  }

void AndroidInput::Controller::paintInventoryButton(Painter& p, Font& font, size_t index) {
  const auto& b      = controls.buttons()[index];
  const bool  active = controls.pressed(index);
  const float s      = controls.unit();
  const float border = std::max(1.5f,1.5f*s);
  auto        r      = b.rect;
  const float alpha  = b.enabled ? 1.f : 0.4f;

  auto glyph = [&](Icon id, const Mobile::Rect& area, float fitted) {
    const auto& uv   = ink[size_t(id)];
    const float f    = std::min(area.w,area.h)*fitted/std::max(uv.w,uv.h);
    const float iw   = uv.w*f;
    const float ih   = uv.h*f;
    p.setBrush(Brush(icons[size_t(id)],Color(0.88f,0.92f,0.95f,alpha)));
    p.drawRect(area.x+(area.w-iw)/2,area.y+(area.h-ih)/2,iw,ih,uv.x,uv.y,uv.x+uv.w,uv.y+uv.h);
    };

  if(b.special>=Mobile::Special::FirstCategory) {
    // The selected tab is lit
    const bool selected = b.special-Mobile::Special::FirstCategory==hud.inventoryCategory;
    frame(p,r,border,active || selected,true);
    if(selected)
      fill(p,{r.x+border,r.y+border,r.w-2*border,r.h-2*border},Color(0.16f,0.20f,0.25f,0.97f));
    glyph(Icon(IconCategoryAll+b.special-Mobile::Special::FirstCategory),r,0.82f);
    return;
    }

  if(b.key==Key::Escape) {
    frame(p,r,border,active);
    const float m = r.w*0.28f;
    const Color c(0.88f,0.92f,0.95f,0.95f);
    stroke(p,r.x+m,r.y+m,r.x+r.w-m,r.y+r.h-m,std::max(2.f,r.w*0.09f),c);
    stroke(p,r.x+r.w-m,r.y+m,r.x+m,r.y+r.h-m,std::max(2.f,r.w*0.09f),c);
    return;
    }

  frame(p,r,border,active,b.enabled);
  const Icon id = b.key==Key::Enter ? IconInvUse : b.key==Key::Weapon ? (hud.inventoryTransfer ? IconInvTransfer : IconInvDrop) : IconInvTransfer;
  glyph(id,r,0.68f);
  }

void AndroidInput::Controller::paintStick(Painter& p) {
  const auto  r      = controls.stickArea();
  const float cx     = controls.stickCenterX();
  const float cy     = controls.stickCenterY();
  const bool  active = controls.stickActive();
  const float alpha  = active ? 0.9f : 0.12f;   // only a hint of the place until the finger is down

  paintSprite(p,IconJoystickBase,{cx-r.w/2,cy-r.h/2,r.w,r.h},alpha);

  if(active) {
    // Arrows of the directions
    const float reach = r.w*0.40f;
    const float a     = r.w*0.028f;
    p.setBrush(Brush(Color(1.f,1.f,1.f,0.5f)));
    p.drawTriangle(cx,cy-reach-a,0.f,0.f, cx-a*1.3f,cy-reach+a,0.f,0.f, cx+a*1.3f,cy-reach+a,0.f,0.f);
    p.drawTriangle(cx,cy+reach+a,0.f,0.f, cx+a*1.3f,cy+reach-a,0.f,0.f, cx-a*1.3f,cy+reach-a,0.f,0.f);
    p.drawTriangle(cx-reach-a,cy,0.f,0.f, cx-reach+a,cy+a*1.3f,0.f,0.f, cx-reach+a,cy-a*1.3f,0.f,0.f);
    p.drawTriangle(cx+reach+a,cy,0.f,0.f, cx+reach-a,cy-a*1.3f,0.f,0.f, cx+reach-a,cy+a*1.3f,0.f,0.f);
    }

  const float thumb = r.w*0.455f;
  const float tx    = cx+controls.stickX()*r.w*0.3f;
  const float ty    = cy+controls.stickY()*r.h*0.3f;
  paintSprite(p,IconJoystickThumb,{tx-thumb/2,ty-thumb/2,thumb,thumb},alpha);
  }

void AndroidInput::Controller::paint(PaintEvent& event) {
  Painter p(event);
  Font    font = Application::defaultFont();
  font.setBold(true);
  setFontSize(p,font,16*controls.unit());

  const size_t count = controls.buttons().size();
  for(size_t i=0; i<count; ++i) {
    if(controls.buttons()[i].visible)
      paintButton(p,font,i);
    }

  if(controls.editingHud()) {
    setFontSize(p,font,15*controls.unit());
    const float s = controls.unit();
    const Mobile::Rect r = {screenW*0.25f,screenH*0.5f-92*s,screenW*0.5f,60*s};
    drawCentered(p,font,r,"Drag a button to move it, tap it to hide or show it");
    }
  if(controls.settingsVisible())
    return;
  if(controls.mode()==Mobile::Mode::Menu && !controls.typing() && !controls.editingHud())
    return;

  const bool game = controls.gameLayout();
  if(game || controls.mode()==Mobile::Mode::Inventory) {
    if(game && (controls.movementAvailable() || controls.editingHud())) {
      paintStick(p);
      }
    return;
    }
  }

#endif
