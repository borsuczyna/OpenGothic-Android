#include "worldmarkers.h"

#include <Tempest/Application>

#include <algorithm>
#include <cmath>

using namespace Tempest;

WorldMarkers& WorldMarkers::inst() {
  static WorldMarkers m;
  return m;
  }

void WorldMarkers::add(const Vec3& pos, uint32_t color, int durationMs, int size) {
  Entry e;
  e.pos      = pos;
  e.color    = color&0xFFFFFF;
  e.start    = Application::tickCount();
  e.duration = std::clamp(durationMs,200,5000);
  e.size     = std::clamp(size,0,3);

  std::lock_guard<std::mutex> guard(sync);
  list.push_back(e);
  if(list.size()>40)
    list.erase(list.begin());
  }

void WorldMarkers::clear() {
  std::lock_guard<std::mutex> guard(sync);
  list.clear();
  }

void WorldMarkers::paint(Painter& p, const Matrix4x4& vp, int w, int h, float uiScale) {
  std::lock_guard<std::mutex> guard(sync);
  if(list.empty())
    return;

  const uint64_t now = Application::tickCount();
  list.erase(std::remove_if(list.begin(),list.end(),[&](const Entry& e) { return now-e.start>uint64_t(e.duration); }),list.end());

  for(const auto& e:list) {
    const float t = float(now-e.start)/float(e.duration);
    // lit at once, fades in the last third, pulses a little
    const float fade  = t<0.65f ? 1.f : std::max(0.f,1.f-(t-0.65f)/0.35f);
    const float pulse = 0.85f+0.15f*std::sin(float(now-e.start)*0.02f);

    auto  pos = e.pos;
    float pw  = 1.f;
    vp.project(pos.x,pos.y,pos.z,pw);
    if(pw<=0.f)
      continue;
    pos /= pw;
    if(std::abs(pos.x)>1.1f || std::abs(pos.y)>1.1f)
      continue;

    const float cx = (0.5f*pos.x+0.5f)*float(w);
    const float cy = (0.5f*pos.y+0.5f)*float(h);
    const float r  = (6.f+4.f*float(e.size))*std::max(0.75f,uiScale)*pulse;
    const float cr = float((e.color>>16)&0xFF)/255.f;
    const float cg = float((e.color>>8)&0xFF)/255.f;
    const float cb = float(e.color&0xFF)/255.f;

    auto square = [&](float rad, const Color& c) {
      p.setBrush(c);
      p.drawRect(int(cx-rad),int(cy-rad),int(rad*2),int(rad*2));
      };
    // a glow around a bright core, like a diode
    square(r*2.2f,Color(cr,cg,cb,0.18f*fade));
    square(r*1.5f,Color(cr,cg,cb,0.40f*fade));
    square(r,     Color(cr,cg,cb,0.95f*fade));
    square(r*0.4f,Color(1.f,1.f,1.f,0.9f*fade));
    }
  }
