#pragma once

#include "modjs.h"

class Npc;
class World;

namespace Tempest {
  class KeyEvent;
  }

/// The objects of the game that mods see. Every object has a file of its own in the game/ folder.
namespace Scripting::game {

/// Connects the mods with the game: folders of the mods, the objects of the game in the scripts
void setup();

// --- objects of the scripts
void installNpc   (JSContext* ctx);   // the prototype of the characters, see game_npc.cpp
void installPlayer(JSContext* ctx);   // player
void installNpcs  (JSContext* ctx);   // npcs
void installItems (JSContext* ctx);   // items, the things that lie on the ground, see game_items.cpp
void installWorld (JSContext* ctx);   // world
void installQuests(JSContext* ctx);   // quests
void installScreen(JSContext* ctx);   // screen
void installInput (JSContext* ctx);   // input
void installTouch (JSContext* ctx);   // touch, the controls of the touch interface, see game_touch.cpp
void installCombat(JSContext* ctx);   // combat, numbers of damage and health in a fight, see game_combat.cpp
void installUi    (JSContext* ctx);   // ui, the tabs of the menu of the mods, see game_ui.cpp
void installDebug (JSContext* ctx);   // debug, for the developers: the log, the scripts, the triggers, see game_debug.cpp

// --- characters
constexpr uint32_t PlayerId = 0xFFFFFFFEu;

/// A JS object for a character. The player is followed, whichever world it is in.
JSValue wrapNpc(JSContext* ctx, Npc& npc);
Npc*    resolveNpc(JSContext* ctx, JSValueConst obj);

/// Index of the character in the world, -1 if it is not there
int64_t npcIndex(World& world, const Npc& npc);

/// What a character plays: [[sequence, ms since it has started, comb, body state], ...]
JSValue animationOf(JSContext* ctx, Npc& npc);

/// Name of a script symbol of an instance, "" if there is no such
std::string instanceName(World& world, size_t symbol);
/// Finds an item or a character instance by its name
size_t      findInstance(World& world, std::string_view name);

// --- events of the game for the mods, called from the main window
void tick();
void keyEvent(const Tempest::KeyEvent& e, bool down);
bool isKeyDown(std::string_view name);
std::string keyName(int key);

}
