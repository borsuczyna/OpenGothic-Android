#pragma once

#include "graphics/objvisual.h"
#include "vob.h"

class StaticObj : public Vob {
  public:
    StaticObj(Vob* parent, World& world, const zenkit::VirtualObject& vob, Flags flags);

    void  setVobVisual(bool show, bool collideDynamic) override;

  protected:
    void  filterVisual() override;
    void  visualInfo(const std::function<void(std::string_view,bool)>& f) const override;

  private:
    void  moveEvent() override;
    bool  setMobState(std::string_view scheme,int32_t st) override;

    ObjVisual   visual;
    std::string scheme;
    bool        shown    = true;    // what the zen file and the scripts of the mod want
    bool        filtered = false;   // hidden by VisualFilter, it is the only reason to hide it here
  };

