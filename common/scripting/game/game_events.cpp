#include "scripting/gameapi.h"

#include "game/constants.h"
#include "gothic.h"
#include "scripting/modutil.h"
#include "world/objects/npc.h"
#include "world/visualfilter.h"
#include "world/world.h"

using namespace Scripting;

/// Events that come from what happens in the game: every frame, a new world, damage of the player.
/// They are found by watching the game, so the code of the game does not know about the mods.

namespace {

struct Watch {
  uint64_t    last       = 0;
  std::string world;
  int32_t     health     = -1;
  bool        dead       = false;
  const World* filterWorld = nullptr;   // the world that has the hidden visuals of VisualFilter
  uint32_t    filterRev  = 0;
  };

Watch& watch() {
  static Watch w;
  return w;
  }

JSValue number(JSContext* ctx, JSValue o, const char* key, double v) {
  JS_SetPropertyStr(ctx,o,key,JS_NewFloat64(ctx,v));
  return o;
  }

}

void game::noteVisualFilter(const World* world) {
  watch().filterWorld = world;
  watch().filterRev   = VisualFilter::inst().revision();
  }

void game::tick() {
  const uint64_t nowMs = steadyMs();
  auto& host = ModHost::inst();
  auto& w    = watch();
  const uint64_t dt = w.last==0 ? 0 : nowMs-w.last;
  w.last = nowMs;

  host.tick(nowMs);

  // visuals that the mods hide: a mod has changed the list or stopped, or the world is another one. This is before the
  // check of the running mods, because a mod that has just stopped must show the visuals again.
  if(auto* wl = Gothic::inst().world()) {
    const auto& vf = VisualFilter::inst();
    if(w.filterRev!=vf.revision() || (w.filterWorld!=wl && vf.active()))
      wl->applyVisualFilter();
    w.filterWorld = wl;
    w.filterRev   = vf.revision();
    }

  if(host.runningCount()==0)
    return;

  auto* world = Gothic::inst().world();
  const std::string name = world==nullptr ? std::string() : std::string(world->name());
  if(name!=w.world) {
    const std::string from = w.world;
    w.world  = name;
    w.health = -1;
    w.dead   = false;
    host.emit("world:change",[&](JSContext* ctx) {
      JSValue o = JS_NewObject(ctx);
      JS_SetPropertyStr(ctx,o,"from",from.empty() ? JS_NULL : JS_NewString(ctx,from.c_str()));
      JS_SetPropertyStr(ctx,o,"to",  name.empty() ? JS_NULL : JS_NewString(ctx,name.c_str()));
      return o;
      });
    }

  if(auto* pl = Gothic::inst().player()) {
    const int32_t hp = pl->attribute(ATR_HITPOINTS);
    if(w.health>=0 && hp<w.health && host.hasHandlers("player:damage")) {
      const int32_t lost = w.health-hp;
      host.emit("player:damage",[&](JSContext* ctx) {
        JSValue o = JS_NewObject(ctx);
        number(ctx,o,"damage",lost);
        number(ctx,o,"health",hp);
        number(ctx,o,"maxHealth",pl->attribute(ATR_HITPOINTSMAX));
        return o;
        });
      }
    const bool dead = pl->isDead();
    if(dead && !w.dead)
      host.emit("player:death");
    w.dead   = dead;
    w.health = hp;
    }

  if(host.hasHandlers("tick")) {
    host.emit("tick",[&](JSContext* ctx) {
      JSValue o = JS_NewObject(ctx);
      number(ctx,o,"dt",double(dt));
      number(ctx,o,"time",double(nowMs));
      return o;
      });
    }
  }
