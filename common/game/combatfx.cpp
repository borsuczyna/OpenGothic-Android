#include "combatfx.h"

#include <Tempest/Application>

#include <algorithm>
#include <cmath>

#include "scripting/modhost.h"
#include "utils/gthfont.h"
#include "world/objects/npc.h"
#include "resources.h"

using namespace Tempest;

namespace {

float sizeFactor(int size) {
  switch(size) {
    case 0:  return 0.8f;
    case 2:  return 1.35f;
    case 3:  return 1.75f;
    default: return 1.f;
    }
  }

Color rgb(uint32_t c, float a) {
  return Color(float((c>>16)&0xFF)/255.f,float((c>>8)&0xFF)/255.f,float(c&0xFF)/255.f,a);
  }

void drawShadowed(Painter& p, const GthFont& fnt, int x, int baseline, const std::string& text, uint32_t color, float alpha) {
  fnt.tinted(Color(0.f,0.f,0.f,0.85f*alpha)).drawText(p,x+1,baseline+1,text);
  fnt.tinted(Color(0.f,0.f,0.f,0.85f*alpha)).drawText(p,x-1,baseline+1,text);
  fnt.tinted(rgb(color,alpha)).drawText(p,x,baseline,text);
  }

}

CombatFx& CombatFx::inst() {
  static CombatFx fx;
  return fx;
  }

void CombatFx::clear() {
  std::lock_guard<std::mutex> guard(sync);
  list.clear();
  }

void CombatFx::onHit(const Npc& victim, const Npc& attacker, int32_t value) {
  if(value<=0)
    return;
  const auto opt = Scripting::ModHost::inst().combatOptions();
  if(!opt.active)
    return;

  const bool taken = victim.isPlayer();
  const bool dealt = !taken && attacker.isPlayer();
  if((taken && !opt.showTaken) || (dealt && !opt.showDealt) || (!taken && !dealt))
    return;

  Entry e;
  e.pos   = victim.centerPosition();
  e.pos.y += 60.f;
  e.text  = (taken ? "-" : "")+std::to_string(value);
  e.color = taken ? opt.colorTaken : opt.colorDealt;
  e.start = Application::tickCount();
  e.taken = taken;

  std::lock_guard<std::mutex> guard(sync);
  e.drift = float(int(counter%5)-2)*14.f;
  ++counter;
  list.push_back(std::move(e));
  if(list.size()>40)
    list.erase(list.begin());
  }

void CombatFx::paint(Painter& p, const Matrix4x4& vp, int w, int h, float uiScale) {
  const auto opt = Scripting::ModHost::inst().combatOptions();
  if(!opt.active) {
    clear();
    return;
    }

  const uint64_t now  = Application::tickCount();
  const float    life = float(std::max(300,opt.duration));
  auto&          fnt  = Resources::font(uiScale*sizeFactor(opt.size));

  {
  std::lock_guard<std::mutex> guard(sync);
  list.erase(std::remove_if(list.begin(),list.end(),[&](const Entry& e) { return float(now-e.start)>life; }),list.end());

  for(const auto& e:list) {
    const float t = float(now-e.start)/life;
    // rises like a bubble and fades in the last part of its life
    float rise  = 140.f*(1.f-(1.f-t)*(1.f-t));
    float alpha = t<0.6f ? 1.f : std::max(0.f,1.f-(t-0.6f)/0.4f);

    auto  pos = e.pos;
    pos.y += rise;
    pos.x += e.drift*t;
    float pw = 1.f;
    vp.project(pos.x,pos.y,pos.z,pw);
    if(pw<=0.f)
      continue;
    pos /= pw;
    if(std::abs(pos.x)>1.2f || std::abs(pos.y)>1.2f)
      continue;

    const auto ts = fnt.textSize(e.text);
    const int  x  = int((0.5f*pos.x+0.5f)*float(w))-ts.w/2;
    const int  y  = int((0.5f*pos.y+0.5f)*float(h));
    drawShadowed(p,fnt,x,y,e.text,e.color,alpha);
    }
  }
  }
