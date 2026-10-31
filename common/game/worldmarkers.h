#pragma once

#include <Tempest/Matrix4x4>
#include <Tempest/Painter>
#include <Tempest/Vec>

#include <cstdint>
#include <mutex>
#include <vector>

/// Small lights in the world that a mod puts (screen.marker): a diode that shows for a moment where something was
/// taken from. They end by themselves.
class WorldMarkers final {
  public:
    static WorldMarkers& inst();

    void add(const Tempest::Vec3& pos, uint32_t color, int durationMs, int size);
    void clear();
    void paint(Tempest::Painter& p, const Tempest::Matrix4x4& vp, int w, int h, float uiScale);

  private:
    struct Entry {
      Tempest::Vec3 pos;
      uint32_t      color    = 0x00FF00;
      uint64_t      start    = 0;
      int           duration = 1500;
      int           size     = 1;
      };

    std::mutex         sync;
    std::vector<Entry> list;
  };
