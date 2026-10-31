#include "mobilecontrolsmodel.h"

#include <algorithm>
#include <cmath>
#include <utility>

using namespace Mobile;

namespace {
constexpr size_t   MaxFingers     = 16;
constexpr uint64_t RepeatDelay    = 400;
constexpr uint64_t RepeatInterval = 120;

// Distances are in layout units, multiplied by Controls::unit()
constexpr float    TapSlop        = 22.f;
constexpr uint64_t HoldTime       = 600;   // ms: a button of the HUD held in the editor is hidden or shown
constexpr float    SwipeDistance  = 42.f;

constexpr float    CameraYawSpan   = 180.f;
constexpr float    CameraPitchSpan = 100.f;
constexpr float    StickTurnRate   = 100.f;
constexpr float    StickPressAt    = 0.30f;
constexpr float    StickReleaseAt  = 0.20f;

constexpr float    MenuGestureSplit = 0.42f;

// The buttons of the HUD in the editor are solid: they are round, so a button is free when the distance between the
// centers is not less than the sum of the radii (and a small gap).
constexpr float    HudGap = 3.f;   // layout units, multiplied by Controls::unit()

/// A button of the HUD (not the DONE and RESET, not the hotbar) that blocks the way of the others
bool isHudIcon(const Button& b) {
  return !b.id.empty() && b.visible;
  }

/// true when a round button of the HUD with the center (px,py) and the radius r touches another one
bool hudCollides(const std::vector<Button>& layout, size_t self, float px, float py, float r, float gap) {
  for(size_t i=0; i<layout.size(); ++i) {
    if(i==self || !isHudIcon(layout[i]))
      continue;
    const auto& o    = layout[i].rect;
    const float need = r+o.w/2+gap;
    if(std::hypot(px-(o.x+o.w/2),py-(o.y+o.h/2))<need-0.01f)
      return true;
    }
  return false;
  }

/// Moves the center (px,py) of a dragged button out of the other buttons of the HUD, along the line from their
/// center, so the button slides around an obstacle instead of sticking to it. It stays on the screen.
/// false: there is no free place that is near (a corner between the buttons), the caller keeps the old place.
bool hudResolve(const std::vector<Button>& layout, size_t self, float width, float height, float gap,
                float& px, float& py) {
  const float r = layout[self].rect.w/2;
  auto keep = [&]() {
    px = std::clamp(px,r,width -r);
    py = std::clamp(py,r,height-r);
    };
  keep();
  for(int pass=0; pass<16; ++pass) {
    bool moved = false;
    for(size_t i=0; i<layout.size(); ++i) {
      if(i==self || !isHudIcon(layout[i]))
        continue;
      const auto& o    = layout[i].rect;
      const float ox   = o.x+o.w/2;
      const float oy   = o.y+o.h/2;
      const float need = r+o.w/2+gap;
      float       dx   = px-ox;
      float       dy   = py-oy;
      float       d    = std::hypot(dx,dy);
      if(d>=need-0.01f)
        continue;
      if(d<0.001f) {
        dx = 1; dy = 0; d = 1;
        }
      px = ox+dx/d*need;
      py = oy+dy/d*need;
      keep();
      moved = true;
      }
    if(!moved)
      return !hudCollides(layout,self,px,py,r,gap);
    }
  return !hudCollides(layout,self,px,py,r,gap);
  }
}

void Controls::acquire(Key key) {
  if(refs[size_t(key)]++==0)
    events.push_back({Event::Down,key});
  }

void Controls::release(Key key) {
  auto& n = refs[size_t(key)];
  if(n>0 && --n==0)
    events.push_back({Event::Up,key});
  }

void Controls::pulse(Key key) {
  events.push_back({Event::Down,key});
  events.push_back({Event::Up,key});
  }

void Controls::physical(Key key, bool on) {
  if(physicalHeld[size_t(key)]==on)
    return;
  physicalHeld[size_t(key)] = on;
  if(on)
    acquire(key); else
    release(key);
  }

void Controls::resetPointers() {
  cancel();
  fingers.clear();
  }

void Controls::preferences(float buttons, float zone, float sensitivity) {
  buttons     = std::clamp(buttons,0.6f,1.4f);
  zone        = std::clamp(zone,0.15f,0.4f);
  sensitivity = std::clamp(sensitivity,0.4f,2.f);
  if(buttonScale==buttons && deadzone==zone && lookSensitivity==sensitivity)
    return;

  cancel();
  buttonScale     = buttons;
  deadzone        = zone;
  lookSensitivity = sensitivity;
  rebuild();
  }

void Controls::inventoryLists(bool two) {
  if(twoLists==two)
    return;
  twoLists = two;
  if(current==Mode::Inventory) {
    cancel();
    rebuild();
    }
  }

void Controls::gui(float scale) {
  scale = std::clamp(scale,0.4f,2.5f);
  if(guiScale==scale)
    return;
  guiScale = scale;
  if(current==Mode::Inventory) {
    cancel();
    rebuild();
    }
  }

void Controls::features(bool speed, bool strafe) {
  if(featureSpeed==speed && featureStrafe==strafe)
    return;
  cancel();
  featureSpeed  = speed;
  featureStrafe = strafe;
  rebuild();
  }

void Controls::playerDead(bool value) {
  if(dead==value)
    return;
  cancel();
  dead = value;
  rebuild();
  }

void Controls::hudPlaces(const std::map<std::string,HudPlace>& next) {
  places = next;
  cancel();
  rebuild();
  }

bool Controls::takeHudChanged() {
  const bool ret = hudChanged;
  hudChanged = false;
  return ret;
  }

void Controls::dialogPad(bool visible) {
  if(padVisible==visible)
    return;
  padVisible = visible;
  if(current==Mode::Dialog) {
    cancel();
    rebuild();
    }
  }

InventoryLayout Mobile::inventoryLayout(float W, float H, bool twoLists, float gui) {
  InventoryLayout l;
  const float cols = float(InventoryLayout::Columns);
  const float rows = float(InventoryLayout::Rows);

  // q is the size of the interface compared to the largest one that fits the screen
  const float q = std::min(1.f,0.766f*gui);
  l.side = std::min(0.128f*H*q,0.095f*W);
  const float side   = l.side;
  const float top    = 0.125f*H;
  const float tabW   = 0.9f*side;
  const float tabsX  = W-0.03f*W-tabW;
  const float inset  = 0.034f*W;

  const float tabGap = 0.12f*side;
  l.list[1] = {tabsX-tabGap-cols*side,top,cols*side,rows*side};
  l.list[0] = {inset,top,cols*side,rows*side};

  // close button over the tabs, the title of the player's list ends before it
  const float closeW = 0.57f*side;
  l.close = {tabsX+(tabW-closeW)/2,0.046f*H,closeW,closeW};
  l.header[1] = {l.list[1].x,0.053f*H,l.close.x-0.05f*side-l.list[1].x,0.058f*H*q};
  l.header[0] = {l.list[0].x,0.053f*H,cols*side,0.058f*H*q};

  const float tabTop    = top+0.042f*H*q;
  const float tabBottom = top+rows*side-0.042f*H*q;
  const float pitch     = (tabBottom-tabTop)/float(CategoryCount);
  for(int i=0; i<CategoryCount; ++i)
    l.tab[i] = {tabsX,tabTop+float(i)*pitch,tabW,pitch*0.9f};

  const float gap = 0.03f*W;
  const float x0  = twoLists ? l.list[0].x+l.list[0].w+gap : inset;
  const float x1  = twoLists ? l.list[1].x-gap : l.list[1].x-0.054f*W;
  l.info = {x0,top,std::max(0.f,x1-x0),0.5f*H*q};

  const float btn = 0.07f*H*q;
  const float by  = l.info.y+l.info.h-0.02f*H*q-btn;
  const float pad = 0.008f*W;
  l.use   = {l.info.x+pad,by,btn,btn};
  l.drop  = {l.info.x+l.info.w-pad-btn,by,btn,btn};
  l.stack = {l.drop.x-pad-btn,by,btn,btn};
  return l;
  }

void Controls::configure(float w, float h, Mode next) {
  if(w==width && h==height && next==current && !layout.empty())
    return;

  cancel();
  width        = w;
  height       = h;
  current      = next;
  keyboard     = false;
  shift        = false;
  settingsOpen = false;
  editing      = false;
  scale        = std::min(width/1000.f,height/440.f);
  rebuild();
  }

Button& Controls::add(float x, float y, float w, float h, const char* label, Key key) {
  Button b;
  b.rect  = {x,y,w*scale,h*scale};
  b.label = label;
  b.key   = key;
  layout.push_back(std::move(b));
  return layout.back();
  }

Button& Controls::round(float x, float y, float diameter, const char* label, Key key) {
  const float size = diameter*sizeFactor();
  return add(x-size*scale/2,y-size*scale/2,size,size,label,key);
  }

void Controls::rebuild() {
  layout.clear();
  stick = {};
  if(current==Mode::Loading)
    return;

  const float l = 16.f*scale;
  const Anchor a = {l, width-l, 12.f*scale, height-12.f*scale};

  if(editing)
    buildHudEditor(); else
  if(settingsOpen)
    buildSettings(a); else
  if(keyboard)
    buildKeyboard(a); else {
    switch(current) {
      case Mode::Video:
        round(fromRight(70),fromBottom(70),60,"SKIP",Key::Escape);
        break;
      case Mode::Lockpick:
        buildLockpick(a);
        break;
      case Mode::Game:
        buildGame(a);
        break;
      case Mode::Menu:
        buildMenu(a);
        break;
      case Mode::Inventory:
        buildInventory();
        break;
      case Mode::Dialog:
        buildDialog(a);
        break;
      case Mode::Loading:
        break;
      }
    }
  }

void Controls::buildSettings(const Anchor& a) {
  static const char* labels[SettingCount] = {
    "Button size", "3D resolution", "Shadow quality", "SSAO effects",
    "FPS limit", "Joystick deadzone", "Camera sensitivity", "Vibration", "GUI scale",
    "Fast forward", "HP/Mana numbers", "XP counter", "Side step buttons"
    };
  // The speed is not a part of the vanilla game: its row is there only when a mod has asked for it
  std::vector<int> rows;
  for(int i=0; i<SettingCount; ++i) {
    if(i==SettingSpeed && !featureSpeed)
      continue;
    rows.push_back(i);
    }
  // the last row opens the editor of the HUD
  const int   count = int(rows.size())+1;
  // The rows shrink when there are too many of them for the height of the screen (every new setting adds a row)
  const float pitch = std::min(40.f*scale,(a.b-a.t-36.f*scale)/float(std::max(count-1,1)));
  const float rowH  = std::min(36.f*scale,pitch-2.f*scale)/scale;
  float       y     = a.t;
  for(int i:rows) {
    add(width*0.22f,y,560,rowH,labels[i],Key::Enter).special = Special::FirstSetting+i;
    y += pitch;
    }
  add(width*0.22f,y,560,rowH,"Edit HUD layout",Key::Enter).special = Special::EditHud;

  // The menu of the mods, bottom left: it is never covered by the rows, they start at 22% of the width
  add(a.l,a.b-36.f*scale,150,36,"Mods",Key::Enter).special = Special::Mods;

  // The way out: the same button that has opened the settings, it is drawn as the arrow back
  round(fromRight(62),fromTop(36),50,"SETTINGS",Key::Enter).special = Special::Settings;
  }

namespace {
/// UTF-8 of a code point of the Latin letters, the labels of the keys
std::string utf8(uint32_t c) {
  std::string s;
  if(c<0x80) {
    s.push_back(char(c));
    } else {
    s.push_back(char(0xC0|(c>>6)));
    s.push_back(char(0x80|(c&0x3F)));
    }
  return s;
  }
}

void Controls::buildKeyboard(const Anchor& a) {
  // the last row has the Polish letters, small and capital ones
  static const uint32_t polish[2][9] = {
    {0x105,0x107,0x119,0x142,0x144,0xF3,0x15B,0x17A,0x17C},
    {0x104,0x106,0x118,0x141,0x143,0xD3,0x15A,0x179,0x17B}
    };
  static const char* rows[] = {"1234567890", "qwertyuiop", "asdfghjkl", "zxcvbnm", "._-:/!?,'"};
  const float kw = std::min((width-2*a.l)/11.f,72.f*scale);
  const float kh = 52.f*scale;
  float       y  = a.b-6*kh-10*scale;
  for(const char* row:rows) {
    const size_t n = std::string(row).size();
    float        x = (width-float(n)*kw)/2;
    for(size_t i=0; i<n; ++i) {
      char c = row[i];
      if(shift && 'a'<=c && c<='z')
        c = char(c-'a'+'A');
      const char text[2] = {c,'\0'};
      add(x,y,kw/scale-4,48,text,Key::Enter).character = uint32_t(c);
      x += kw;
      }
    y += kh;
    }
  {
  float x = (width-9*kw)/2;
  for(size_t i=0; i<9; ++i) {
    const uint32_t c = polish[shift ? 1 : 0][i];
    add(x,y,kw/scale-4,48,utf8(c).c_str(),Key::Enter).character = c;
    x += kw;
    }
  }
  add(a.l,a.t,100,54,"HIDE",Key::Enter).special = Special::Keyboard;
  add(a.l+110*scale,a.t,110,54,"SPACE",Key::Enter).character = ' ';
  add(a.l+230*scale,a.t,110,54,shift ? "ABC" : "abc",Key::Enter).special = Special::Shift;
  add(a.r-220*scale,a.t,100,54,"DEL",Key::Backspace).repeat = true;
  add(a.r-110*scale,a.t,110,54,"OK",Key::Enter);
  }

void Controls::buildLockpick(const Anchor&) {
  round(fromLeft(70),  fromBottom(70),66,"LEFT",  Key::Left);
  round(fromLeft(150), fromBottom(70),66,"RIGHT", Key::Right);
  round(fromRight(70), fromBottom(70),66,"CANCEL",Key::Down);
  }

void Controls::buildGame(const Anchor&) {
  const float s = scale;
  const float k = sizeFactor();
  auto left   = [&](float v) { return fromLeft(v); };
  auto right  = [&](float v) { return fromRight(v); };
  auto top    = [&](float v) { return fromTop(v); };
  auto bottom = [&](float v) { return fromBottom(v); };

  // Diameter d is given for the default button size. A button that the player has moved stands where it was put (the
  // place is a part of the screen, so it is the same in another resolution), a hidden one is not there at all, but
  // in the editor, where it is dimmed. The menu cannot be hidden: it leads to the editor.
  auto icon = [&](const char* id, float x, float y, float d, const char* label, Key key, bool hold=false, int special=0) {
    HudPlace place;
    const auto it = places.find(id);
    if(it!=places.end())
      place = it->second;
    const bool hidden = place.hidden && std::string(id)!="menu";
    if(hidden && !editing)
      return;
    if(place.placed) {
      x = place.x*width;
      y = place.y*height;
      }
    auto& b = round(x,y,d*hudSizeFactor(place.size),label,key);
    b.hold    = hold;
    b.special = special;
    b.id      = id;
    b.dimmed  = hidden;
    };

  icon("menu",      left(61),   top(36), 50, "MENU",      Key::Escape);
  icon("character", left(122),  top(36), 50, "CHARACTER", Key::Status);
  // quick save: next to the speed button; not when the player is dead
  if(!dead)
    icon("quicksave", left(183), top(36), 50, "SAVE",    Key::Enter, false, Special::QuickSave);
  // fast forward is not in the vanilla game, a mod switches it on (touch.fastForward); it is hidden when the player is dead
  if(featureSpeed && !dead)
    icon("speed",   left(244),  top(36), 50, "SPEED",     Key::Enter, false, Special::Speed);
  icon("log",       right(124), top(36), 50, "LOG",       Key::Log);
  icon("map",       right(62),  top(36), 50, "MAP",       Key::Map);

  if(dead && !editing) {
    // the player has died: one button loads the newest save
    auto& b = add(width/2-90*s*k,height*0.62f,180*k,56*k,"LOAD LAST SAVE",Key::Enter);
    b.special = Special::LoadSave;
    }

  const float stickSize = 180*k*s;
  stick = {left(153.5f)-stickSize/2,bottom(166.5f)-stickSize/2,stickSize,stickSize};
  icon("sneak",     left(61),  bottom(36.5f), 50, "SNEAK", Key::Sneak);
  icon("walk",      left(123), bottom(36.5f), 50, "WALK",  Key::Walk);
  // side steps, right next to the floating stick: an option of the settings
  if(featureStrafe) {
    icon("stepleft",  left(284), bottom(166.5f), 56, "< STEP", Key::StrafeLeft,  true);
    icon("stepright", left(350), bottom(166.5f), 56, "STEP >", Key::StrafeRight, true);
    }

  icon("action",    right(198),   bottom(221.5f), 88, "ACTION", Key::Action, true);
  icon("hitleft",   right(285),   bottom(166.5f), 68, "< HIT",  Key::AttackLeft, true);
  icon("hitright",  right(111),   bottom(166.5f), 68, "HIT >",  Key::AttackRight, true);
  icon("block",     right(198),   bottom(111),    88, "BLOCK",  Key::Block, true);
  icon("jump",      right(72),    bottom(266.5f), 66, "JUMP",   Key::Jump, true);
  icon("inventory", right(62),    bottom(36.5f),  50, "INVENTORY", Key::Inventory);
  icon("target",    right(372),   bottom(230),    56, "TARGET", Key::Enter, false, Special::Target);
  icon("spells",    right(372),   bottom(150),    56, "SPELLS", Key::Enter, false, Special::Spell);
  icon("weapon",    right(300),   bottom(276),    52, "WEAPON", Key::Weapon);

  // The hotbar is centered below the status bars. Its slots are of the size of the buttons.
  const float slot = HotbarSlot*k;
  for(int i=0; i<10; ++i) {
    const std::string label = std::to_string((i+1)%10);
    // slots touch each other, the half unit gap only protects from rounding
    add(width/2-slot*5*s+float(i)*slot*s+0.25f*s,height-(HotbarBottom+slot)*s,
        slot-0.5f,slot-0.5f,label.c_str(),Key(int(Key::One)+i));
    }
  applyAvailability();
  }

/// The game screen in which the buttons are dragged. DONE goes back to the settings.
void Controls::buildHudEditor() {
  buildGame({});
  const float s = scale;
  auto add2 = [&](float x, float y, const char* label, int special) {
    auto& b = add(x,y,150,44,label,Key::Enter);
    b.special = special;
    };
  // in the middle of the screen: the corners are taken by the buttons
  add2(width/2-160*s,height*0.5f-22*s,"DONE",Special::HudDone);
  add2(width/2+10*s, height*0.5f-22*s,"RESET",Special::HudReset);
  }

void Controls::buildMenu(const Anchor&) {
  if(modsOpen) {
    // the window of the mods has its own close button at the top right: the settings do not cover it
    round(fromRight(70), fromBottom(70),  66,"BACK",    Key::Escape);
    return;
    }
  round(fromRight(62), fromTop(36),     50,"SETTINGS",Key::Enter).special = Special::Settings;
  round(fromRight(70), fromBottom(70),  66,"BACK",    Key::Escape);
  round(fromRight(150),fromBottom(70),  66,"KEYBOARD",Key::Enter).special = Special::Keyboard;
  }

void Controls::buildInventory() {
  const auto l = inventoryLayout(width,height,twoLists,guiScale);
  auto tile = [&](const Rect& r, const char* label, Key key, int special=0) {
    Button b;
    b.rect    = r;
    b.label   = label;
    b.key     = key;
    b.special = special;
    layout.push_back(std::move(b));
    };

  static const char* categories[CategoryCount] = {
    "ALL", "WEAPONS", "ARMOR", "JEWELRY", "MAGIC", "FOOD", "POTIONS", "DOCUMENTS", "OTHER"
    };
  tile(l.close,"CLOSE",Key::Escape);
  for(int i=0; i<CategoryCount; ++i)
    tile(l.tab[i],categories[i],Key::Enter,Special::FirstCategory+i);

  // The lists are scrolled with a finger, so there are no arrows
  tile(l.use,  "USE",  Key::Enter);
  tile(l.drop, "DROP", Key::Weapon);
  tile(l.stack,"STACK",Key::Jump);
  applyAvailability();
  }

void Controls::buildDialog(const Anchor&) {
  round(fromLeft(61),fromTop(36),50,"MENU",Key::Escape);

  // leaves the phrase or the conversation
  round(fromRight(70),fromBottom(70),66,"BACK",Key::Escape);

  // answers are tapped, the cross is a spare way to choose them
  if(padVisible) {
    const float cx  = fromLeft(100);
    const float cy  = fromBottom(100);
    const float gap = 62*sizeFactor()*scale;
    round(cx,    cy-gap,56,"UP",   Key::Up).repeat    = true;
    round(cx,    cy+gap,56,"DOWN", Key::Down).repeat  = true;
    round(cx-gap,cy,    56,"<",    Key::Left).repeat  = true;
    round(cx+gap,cy,    56,">",    Key::Right).repeat = true;
    round(fromRight(170),fromBottom(70),66,"OK",Key::Enter);
    }
  }

bool Controls::hasOwner(bool Finger::* role) const {
  for(const auto& p:fingers) {
    if(p.second.*role && !p.second.blocked)
      return true;
    }
  return false;
  }

void Controls::down(int id, float x, float y, uint64_t now) {
  if(fingers.find(id)!=fingers.end() || fingers.size()>=MaxFingers)
    return;

  if(editing) {
    downEditor(id,x,y,now);
    return;
    }

  Finger f;
  f.startX = f.x = x;
  f.startY = f.y = y;
  for(size_t i=0; i<layout.size(); ++i) {
    if(layout[i].visible && layout[i].enabled && layout[i].rect.contains(x,y)) {
      f.button = int(i);
      break;
      }
    }

  const bool game = current==Mode::Game && !settingsOpen;
  if(f.button>=0) {
    const auto& button = layout[size_t(f.button)];
    if(button.hold) {
      acquire(button.key);
      } else
    if(button.repeat) {
      pulse(button.key);
      f.repeatAt = now+RepeatDelay;
      }
    } else
  if(game && availableMovement && x<width*MenuGestureSplit) {
    // the left part of the screen is the joystick, it appears under the finger
    if(!hasOwner(&Finger::joystick)) {
      f.joystick = true;
      f.originX  = x;
      f.originY  = y;
      joyOx      = x;
      joyOy      = y;
      joyActive  = true;
      joystick(x,y,x,y);
      }
    } else
  if(game && x>width*MenuGestureSplit) {
    f.camera = !hasOwner(&Finger::camera);
    } else
  if(current==Mode::Menu || current==Mode::Inventory || current==Mode::Dialog) {
    f.swipe = true;
    }

  if(f.swipe && (current==Mode::Menu || current==Mode::Inventory || current==Mode::Dialog) && !keyboard) {
    // Menu input has a single gesture owner
    if(hasOwner(&Finger::swipe))
      return;
    events.push_back({Event::MenuTouch,Key::Enter,x,y,0});
    }
  fingers.emplace(id,f);
  }

void Controls::joystick(float x, float y, float ox, float oy) {
  const float radius = stick.w/2;
  joyX = (x-ox)/radius;
  joyY = (y-oy)/radius;
  const float len = std::sqrt(joyX*joyX+joyY*joyY);
  if(len>1) {
    joyX /= len;
    joyY /= len;
    }

  // Forward/back use hysteresis; horizontal displacement steers continuously in tick().
  // A generous central cone prevents accidental turning while walking straight.
  const float values[2] = {-joyY,joyY};
  const Key   keys  [2] = {Key::Up,Key::Down};
  for(size_t i=0; i<2; ++i) {
    const bool on = values[i]>(joyHeld[i] ? StickReleaseAt : StickPressAt);
    if(on==joyHeld[i])
      continue;
    joyHeld[i] = on;
    if(on)
      acquire(keys[i]); else
      release(keys[i]);
    }
  }

void Controls::move(int id, float x, float y) {
  auto it = fingers.find(id);
  if(it==fingers.end() || it->second.blocked)
    return;

  auto& f = it->second;
  if(f.drag) {
    moveEditor(f,x,y);
    return;
    }
  if(f.swipe && (current==Mode::Menu || current==Mode::Inventory || current==Mode::Dialog) && !keyboard)
    events.push_back({Event::MenuTouch,Key::Enter,x,y,1});
  if(f.swipe && std::hypot(x-f.startX,y-f.startY)>TapSlop*scale)
    f.cancelled = true;
  if(f.joystick)
    joystick(x,y,f.originX,f.originY);
  if(f.camera) {
    const float yaw   = (x-f.x)/width *CameraYawSpan  *lookSensitivity;
    const float pitch = (y-f.y)/height*CameraPitchSpan*lookSensitivity;
    events.push_back({Event::Look,Key::Enter,yaw,pitch});
    }
  if(f.button>=0) {
    auto& button = layout[size_t(f.button)];
    if(!button.hold && (!button.rect.contains(x,y) || std::hypot(x-f.startX,y-f.startY)>TapSlop*scale))
      f.cancelled = true;
    }
  f.x = x;
  f.y = y;
  }

void Controls::cancelFinger(Finger& f) {
  if(f.blocked)
    return;
  if(f.button>=0 && layout[size_t(f.button)].hold)
    release(layout[size_t(f.button)].key);
  if(f.joystick) {
    const Key keys[2] = {Key::Up,Key::Down};
    for(size_t i=0; i<2; ++i) {
      if(joyHeld[i])
        release(keys[i]);
      joyHeld[i] = false;
      }
    joyX = joyY = 0;
    joyActive = false;
    }
  f.blocked = true;
  }

void Controls::modMenu(bool on) {
  if(modsOpen==on)
    return;
  cancel();
  modsOpen = on;
  if(on)
    settingsOpen = false;
  rebuild();
  }

void Controls::showKeyboard(bool on) {
  if(keyboard==on)
    return;
  cancel();
  keyboard = on;
  rebuild();
  }

void Controls::activate(Button button) {
  if(button.special==Special::Keyboard) {
    cancel();
    keyboard = !keyboard;
    rebuild();
    return;
    }
  if(button.special==Special::Shift) {
    cancel();
    shift = !shift;
    rebuild();
    return;
    }
  if(button.special==Special::Settings) {
    cancel();
    settingsOpen = !settingsOpen;
    rebuild();
    events.push_back({Event::Special,Key::Enter,0,0,uint32_t(button.special)});
    return;
    }
  if(button.special==Special::EditHud) {
    // the settings give the screen to the HUD: the buttons are dragged where they should be
    cancel();
    settingsOpen = false;
    editing      = true;
    rebuild();
    events.push_back({Event::Special,Key::Enter,0,0,uint32_t(button.special)});
    return;
    }
  if(button.special==Special::HudDone) {
    cancel();
    editing      = false;
    settingsOpen = true;
    rebuild();
    events.push_back({Event::Special,Key::Enter,0,0,uint32_t(button.special)});
    return;
    }
  if(button.special==Special::HudReset) {
    cancel();
    places.clear();
    hudChanged = true;
    rebuild();
    return;
    }
  if(button.special==Special::Mods) {
    // the menu of the mods has the screen: the settings are closed first, they would stay on top of it
    cancel();
    settingsOpen = false;
    rebuild();
    events.push_back({Event::Special,Key::Enter,0,0,uint32_t(button.special)});
    return;
    }
  if(button.special!=Special::None) {
    events.push_back({Event::Special,Key::Enter,0,0,uint32_t(button.special)});
    return;
    }
  if(button.character!=0) {
    events.push_back({Event::Text,Key::Enter,0,0,button.character});
    if(shift) {
      // a capital letter is for one letter only
      shift = false;
      rebuild();
      }
    return;
    }
  if(keyboard && button.key==Key::Enter) {
    cancel();
    keyboard = false;
    rebuild();
    }
  pulse(button.key);
  }

void Controls::up(int id, float x, float y) {
  auto it = fingers.find(id);
  if(it==fingers.end())
    return;

  const Finger f = it->second;
  cancelFinger(it->second);
  fingers.erase(it);
  if(f.blocked)
    return;
  if(f.drag) {
    upEditor(f);
    return;
    }

  const float slop = TapSlop*scale;
  if(f.button>=0) {
    // Buttons confirm on release, only when the finger has not wandered off
    const Button button = layout[size_t(f.button)];
    if(button.hold || button.repeat || f.cancelled || !button.rect.contains(x,y) ||
       std::hypot(x-f.startX,y-f.startY)>slop)
      return;
    activate(button);
    return;
    }

  if(!f.swipe || keyboard || settingsOpen)
    return;

  const float dx = x-f.startX;
  const float dy = y-f.startY;
  const bool  vertical = std::abs(dy)>SwipeDistance*scale && std::abs(dy)>std::abs(dx)*1.25f;
  if(current==Mode::Menu || current==Mode::Inventory) {
    events.push_back({Event::MenuTouch,Key::Enter,x,y,2});
    // The inventory list follows the finger, see InventoryMenu::mobileTouch
    if(vertical && current==Mode::Menu)
      events.push_back({Event::Scroll,Key::Enter,0,dy}); else
    if(!f.cancelled && std::hypot(dx,dy)<=slop)
      events.push_back({Event::Tap,Key::Enter,x,y});
    return;
    }
  if(current==Mode::Dialog) {
    events.push_back({Event::MenuTouch,Key::Enter,x,y,2});
    if(!vertical && !f.cancelled && std::hypot(dx,dy)<=slop)
      events.push_back({Event::Tap,Key::Enter,x,y}); else
    if(vertical && padVisible)
      pulse(dy<0 ? Key::Up : Key::Down);
    return;
    }
  if(vertical)
    pulse(dy<0 ? Key::Up : Key::Down);
  }

void Controls::cancel() {
  // A second layout change may happen before drain(). Keep releases queued by
  // the first one: dropping them leaves keys (notably Action/Ctrl) held in the
  // window dispatcher and prevents text fields from accepting characters.
  events.erase(std::remove_if(events.begin(),events.end(),[](const Event& e) {
    return e.kind!=Event::Up;
    }),events.end());
  if((current==Mode::Menu || current==Mode::Inventory || current==Mode::Dialog) && hasOwner(&Finger::swipe))
    events.push_back({Event::MenuTouch,Key::Enter,0,0,3});

  // Release only held controls; cancelling must never commit a tap action.
  for(size_t i=0; i<refs.size(); ++i) {
    if(refs[i])
      events.push_back({Event::Up,Key(i)});
    }
  refs.fill(0);
  physicalHeld.fill(false);
  joyHeld.fill(false);
  joyX = joyY = 0;
  joyActive = false;
  for(auto& p:fingers)
    p.second.blocked = true;
  }

void Controls::tick(uint64_t now) {
  const float dt = lastTick && now>=lastTick ? float(std::min<uint64_t>(now-lastTick,100))/1000.f : 0.f;
  lastTick = now;

  if(current==Mode::Game && std::abs(joyX)>deadzone) {
    const float turn = std::copysign((std::abs(joyX)-deadzone)/(1.f-deadzone),joyX);
    events.push_back({Event::Look,Key::Enter,turn*StickTurnRate*dt,0});
    }

  if(editing) {
    for(auto& p:fingers) {
      auto& f = p.second;
      if(!f.drag || f.moved || f.held || f.holdAt==0 || now<f.holdAt || f.button<0 || size_t(f.button)>=layout.size())
        continue;
      const auto id = layout[size_t(f.button)].id;
      if(id.empty() || id=="menu")
        continue;
      f.held = true;
      auto& place = places[id];
      place.hidden = !place.hidden;
      hudChanged = true;
      rebuild();
      break;
      }
    }

  for(auto& p:fingers) {
    auto& f = p.second;
    if(f.blocked || f.cancelled || f.button<0 || f.repeatAt==0 || now<f.repeatAt)
      continue;
    pulse(layout[size_t(f.button)].key);
    f.repeatAt = now+RepeatInterval;
    }
  }

std::vector<Event> Controls::drain() {
  std::vector<Event> ret;
  ret.swap(events);
  return ret;
  }

bool Controls::pressed(size_t index) const {
  for(const auto& p:fingers) {
    if(!p.second.blocked && !p.second.cancelled && p.second.button==int(index))
      return true;
    }
  return false;
  }

void Controls::applyAvailability() {
  if((current!=Mode::Game && current!=Mode::Inventory) || settingsOpen || keyboard || editing)
    return;

  for(auto& b:layout) {
    bool usable = (availableKeys & keyMask(b.key))!=0;
    if(b.special>=Special::FirstCategory)
      usable = true; else
    if(b.special==Special::Target)
      usable = availableTarget; else
    if(b.special==Special::Spell)
      usable = availableSpells; else
    if(b.special==Special::Speed || b.special==Special::LoadSave || b.special==Special::QuickSave)
      usable = true;

    // The hotbar stays visible while the player can move, other controls hide when unusable
    // Inventory actions disappear when there is no item to act on, the tabs and the close button stay.
    const bool hotbar  = b.key>=Key::One && b.key<=Key::Zero && availableMovement;
    const bool pinned  = current==Mode::Inventory && (b.special>=Special::FirstCategory || b.key==Key::Escape);
    b.visible = (pinned || hotbar) ? true : usable;
    b.enabled = usable;
    }
  }

void Controls::availability(uint64_t keys, bool target, bool spells, bool movement) {
  availableKeys     = keys;
  availableTarget   = target;
  availableSpells   = spells;
  availableMovement = movement;
  applyAvailability();

  // Release only controls that disappeared; never reset a walking finger on focus changes.
  for(auto& pair:fingers) {
    auto& f = pair.second;
    const bool hidden = f.button>=0 && (!layout[size_t(f.button)].visible || !layout[size_t(f.button)].enabled);
    if(hidden || (f.joystick && !movement))
      cancelFinger(f);
    }
  }

/// A finger in the editor: a button of the HUD is taken by its place, the others (DONE, RESET) are tapped as usual
void Controls::downEditor(int id, float x, float y, uint64_t now) {
  Finger f;
  f.startX = f.x = x;
  f.startY = f.y = y;
  for(size_t i=0; i<layout.size(); ++i) {
    if(!layout[i].visible || !layout[i].rect.contains(x,y))
      continue;
    f.button = int(i);
    if(!layout[i].id.empty()) {
      f.drag  = true;
      f.grabX = x-(layout[i].rect.x+layout[i].rect.w/2);
      f.grabY = y-(layout[i].rect.y+layout[i].rect.h/2);
      // a long hold hides the button (or shows it again), the menu cannot be hidden
      if(layout[i].id!="menu")
        f.holdAt = now+HoldTime;
      }
    break;
    }
  fingers.emplace(id,f);
  }

void Controls::moveEditor(Finger& f, float x, float y) {
  f.x = x;
  f.y = y;
  if(f.button<0 || size_t(f.button)>=layout.size())
    return;
  if(!f.moved && std::hypot(x-f.startX,y-f.startY)<=TapSlop*scale)
    return;
  f.moved = true;

  // the button follows the finger and stays on the screen, but it is solid: it slides around the other buttons of
  // the HUD and never lies on one of them (when there is no free place, it stays where it was)
  auto&       b  = layout[size_t(f.button)];
  float       cx = x-f.grabX;
  float       cy = y-f.grabY;
  if(!hudResolve(layout,size_t(f.button),width,height,HudGap*scale,cx,cy))
    return;
  b.rect.x = cx-b.rect.w/2;
  b.rect.y = cy-b.rect.h/2;
  }

void Controls::upEditor(const Finger& f) {
  if(f.button<0 || size_t(f.button)>=layout.size())
    return;
  const auto& b = layout[size_t(f.button)];
  auto&       p = places[b.id];
  if(f.moved) {
    p.placed = true;
    p.x      = (b.rect.x+b.rect.w/2)/width;
    p.y      = (b.rect.y+b.rect.h/2)/height;
    } else
  if(f.held) {
    // it was hidden or shown by the hold
    return;
    } else {
    // a tap: the next size of the button (5 steps, after the biggest the smallest). A size at which the button would
    // lie on another one is skipped; a smaller one always fits.
    const size_t self   = size_t(f.button);
    const float  base   = b.rect.w/hudSizeFactor(p.size);   // the diameter of the normal size
    const float  px     = b.rect.x+b.rect.w/2;
    const float  py     = b.rect.y+b.rect.h/2;
    int          next   = p.size;
    for(int step=1; step<HudSizeSteps; ++step) {
      const int t = (p.size+step)%HudSizeSteps;
      if(t<p.size || !hudCollides(layout,self,px,py,base*hudSizeFactor(t)/2,HudGap*scale)) {
        next = t;
        break;
        }
      }
    if(next==p.size)
      return;
    p.size = next;
    }
  hudChanged = true;
  rebuild();
  }
