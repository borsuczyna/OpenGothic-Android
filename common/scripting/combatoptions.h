#pragma once

#include <cstdint>

namespace Scripting {

/// What a mod asks the game to show in a fight (combat.configure in JS). Nothing is shown until a running mod sets
/// `active`, so the game is vanilla without such a mod. Colors are 0xRRGGBB.
struct CombatOptions {
  bool     active        = false;
  bool     showDealt     = true;     // damage that the player deals
  bool     showTaken     = true;     // damage that the player receives
  bool     showEnemyHp   = true;     // health of the enemy in the fight as a number, under its bar at the top
  int      size          = 1;        // 0 small, 1 normal, 2 large, 3 huge
  int      duration      = 1400;     // ms
  uint32_t colorDealt    = 0xFFD83A;
  uint32_t colorTaken    = 0xFF4A3A;
  uint32_t colorEnemyHp  = 0xFFFFFF;
  };

}
