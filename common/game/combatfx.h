#pragma once

#include <Tempest/Matrix4x4>
#include <Tempest/Painter>
#include <Tempest/Vec>

#include <cstdint>
#include <mutex>
#include <string>
#include <vector>

#include "scripting/combatoptions.h"

class Npc;

/// Damage numbers floating in the air. It is switched on by a mod (combat.configure), the code of the game only
/// reports hits. The health of the enemy under its bar is drawn by MainWindow.
class CombatFx final {
  public:
    static CombatFx& inst();

    /// A hit that is going to take `value` of health from `victim`
    void onHit(const Npc& victim, const Npc& attacker, int32_t value);
    void clear();

    /// Floating numbers of the damage
    void paint(Tempest::Painter& p, const Tempest::Matrix4x4& vp, int w, int h, float uiScale);

  private:
    struct Entry {
      Tempest::Vec3 pos;
      std::string   text;
      uint32_t      color  = 0xFFFFFF;
      uint64_t      start  = 0;
      float         drift  = 0;      // sideways, so the numbers of a fast fight do not lie on each other
      bool          taken  = false;
      };

    std::mutex         sync;
    std::vector<Entry> list;
    uint32_t           counter = 0;
  };
