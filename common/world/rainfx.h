#pragma once

#include <array>
#include <memory>
#include <cstdint>

#include <Tempest/Vec>

#include "world/objects/pfxemitter.h"
#include "world/objects/sound.h"

class World;
class ParticleFx;

/// The rain of the outdoor worlds: the drops around the camera, the noise of the rain and the weather of the day.
/// The original game has the rain (skyEffects=1 of Gothic.ini); here the player can set its strength in the settings of the touch interface.
class RainFx final {
  public:
    RainFx(World& owner);
    ~RainFx();

    void  tick(uint64_t dt);

    /// The weather that is on now (0..1), the same for the whole world: it darkens the sky, the scripts ask for it (Wld_IsRaining)
    float overcast() const { return weather; }
    bool  isRaining() const { return weather>0.2f; }
    /// 1: the rain is on, 0: the rain is off, -1: the weather of the day
    void  force(int mode) { forced = mode; }

  private:
    static constexpr size_t LayerCount = 3;

    static bool  isOutdoorWorld(std::string_view name);
    struct Plan final {
      bool  rain  = false;
      int   start = 0;      // minutes of the day
      int   len   = 0;      // minutes
      float peak  = 0;
      };
    static Plan  planOf(int64_t day);
    float        scheduled() const;
    void         updateShelter(const Tempest::Vec3& pos);
    void         updateSound(const Tempest::Vec3& pos, float volume);

    World&                              owner;
    std::unique_ptr<ParticleFx>         decl;
    std::array<PfxEmitter,LayerCount>   layer;
    Sound                               noise;

    bool                                outdoor    = false;
    int                                 level      = 0;      // the setting of the player: 0 off, 1 light, 2 normal, 3 heavy
    int                                 forced     = -1;     // Marvin: 1 on, 0 off, -1 the weather of the day
    float                               weather    = 0;      // the weather of the world, smoothed
    float                               shelter    = 0;      // 1: a roof or a room is over the camera
    bool                                covered    = false;
    uint64_t                            settingsTimer = 0;
    uint64_t                            shelterTimer  = 0;
    int64_t                             loggedDay  = -1;
    size_t                              loggedCount = size_t(-1);
    int                                 loggedLevel = -1;
    bool                                loggedOutdoor = false;
    bool                                loggedCovered = false;
  };
