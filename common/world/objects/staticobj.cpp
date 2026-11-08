#include "staticobj.h"

#include <Tempest/Log>

#include "world/visualfilter.h"
#include "world/world.h"
#include "utils/string_frm.h"

using namespace Tempest;

StaticObj::StaticObj(Vob* parent, World& world, const zenkit::VirtualObject& vob, Flags flags)
  : Vob(parent, world, vob, flags) {
  visual.setVisual(vob,world,(flags & Flags::Static));
  visual.setObjMatrix(transform());

  scheme = vob.visual->name;
  shown  = vob.show_visual;

  // the world is made while a mod that hides some visuals is running: they are not drawn from the start
  if(!scheme.empty() && VisualFilter::inst().hides(scheme)) {
    filtered = true;
    visual.setShown(false,-1,world,transform());
    }
  }

void StaticObj::setVobVisual(bool show, bool collideDynamic) {
  shown = show;
  visual.setShown(shown && !filtered,collideDynamic ? 1 : 0,world,transform());
  }

void StaticObj::filterVisual() {
  if(scheme.empty())
    return;
  const bool want = VisualFilter::inst().hides(scheme);
  if(want==filtered)
    return;
  filtered = want;
  visual.setShown(shown && !filtered,-1,world,transform());
  }

void StaticObj::visualInfo(const std::function<void(std::string_view,bool)>& f) const {
  if(!scheme.empty())
    f(scheme,filtered);
  }

void StaticObj::moveEvent() {
  Vob::moveEvent();
  visual.setObjMatrix(transform());
  }

bool StaticObj::setMobState(std::string_view sc, int32_t st) {
  const bool ret = Vob::setMobState(sc,st);

  if(scheme.find(sc)!=0)
    return ret;
  string_frm name("S_S",st);
  if(visual.startAnimAndGet(name,world.tickCount())!=nullptr) {
    // state = st;
    return ret;
    }
  return false;
  }
