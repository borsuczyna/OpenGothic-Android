#pragma once

#include <cstdint>

/// The places of Gothic2.exe that the libraries (LeGo) and the mods (CoM) hook with HookEngineF. OpenGothic does not
/// patch the engine: when something happens in OpenGothic that corresponds to such a place, it calls the functions that
/// are hooked there (DirectMemory::fireHook) with the registers and the stack that the original engine would have.
namespace HookAdr {
  constexpr uint32_t CutsceneMenu          = 0x42ABE7; // eax != 0 blocks the menu
  constexpr uint32_t CutsceneStatus        = 0x6C31D6; // eax != 0 hides player status
  constexpr uint32_t DialogMessage         = 0x7A9240; // ecx = view, [esp+4] = name string, [esp+16] = color*
  constexpr uint32_t SpellTargetValid       = 0x485FC4; // ecx = target type, [esp+4] = target vob, ecx = validity
  constexpr uint32_t EquipBestWeapons        = 0x745FA6; // oCNpc::EquipBestWeapons: esi = oCNpc*
  // LeGo
  constexpr uint32_t ChangeLevelBegin       = 0x6C7290; // oCGame::ChangeLevel
  constexpr uint32_t ChangeLevelEnd         = 0x6C7ACB; // end of oCGame::ChangeLevel
  constexpr uint32_t WriteSavegame          = 0x439105; // oCSavegameManager::SetAndWriteSavegame
  constexpr uint32_t SaveGameWorld          = 0x6C5250; // CoM torch state before world/Daedalus serialization
  constexpr uint32_t LoadingTexture         = 0x42A2D0; // before a loading screen: the hook writes the name of the picture to the text at 0x8B3D84
  constexpr uint32_t SaveWorldName          = 0x437D9F; // the header of a save: the hook writes the name of the area to the zSTRING at [ebp+84]
  constexpr uint32_t ReadSavegame           = 0x42A040; // CGameManager::Read_Savegame, [esp+4] is the slot
  constexpr uint32_t SpriteRender           = 0x64DF20;
  constexpr uint32_t GameRender             = 0x6C8800; // LeGo Render item/view previews
  constexpr uint32_t BarUpdateResolution    = 0x6C30F0;

  // CoM
  constexpr uint32_t InventoryInsert        = 0x70C8A8; // oCNpcInventory::Insert: ecx = oCNpcInventory*, esi = oCItem*
  constexpr uint32_t ChangeAttribute        = 0x72FF60; // oCNpc::ChangeAttribute: ecx = oCNpc*, [esp+4] attribute, [esp+8] change
  constexpr uint32_t OnDamage               = 0x6660E0; // oCNpc::OnDamage: ecx = victim, [esp+4] = oSDamageDescriptor*
  constexpr uint32_t OnDamageScript         = 0x66E220; // after the script of the damage
  constexpr uint32_t CreateFallDamage       = 0x681DA0; // ecx = oCNpc*
  constexpr uint32_t KeyEvent               = 0x6FD434; // oCGame::HandleEvent, the default case: esi = key
  constexpr uint32_t ZoneMusicChanged       = 0x6407C2; // edx = zSTRING* with the name of the zone
  constexpr uint32_t MobCanOpen             = 0x7244F0; // oCMobLockable::CanOpen: ecx = the mob
  constexpr uint32_t CsCameraTrigger        = 0x4C3270; // zCCSCamera::OnTrigger: eax = the camera
  constexpr uint32_t CsCameraUntrigger      = 0x4C3AC0; // zCCSCamera::OnUntrigger
  constexpr uint32_t EvDropVob              = 0x7538C0; // oCNpc::EV_DropVob: ecx = oCNpc*, [esp+4] = the message with the item at +108
  constexpr uint32_t InventoryOpen          = 0x762250; // oCNpc::OpenInventory: the hook of CoM closes it again in a fistfight
  constexpr uint32_t ItemInfoDraw           = 0x706E40; // oCItemContainer::DrawItemInfo: [esp+4] = oCItem*
  constexpr uint32_t SellItem               = 0x68B840; // oCViewDialogTrade::OnTransferLeft: the player sells the selected item
  constexpr uint32_t HideFocusParty         = 0x6C36DB; // CoM: the party members (AIVAR[15]) are not in the focus of the player with a weapon
  constexpr uint32_t NextEnemy              = 0x734F4D; // ebp = searching NPC, edi = candidate, ecx != 0 rejects
  constexpr uint32_t FinishingMove          = 0x751AF0; // oCNpc::EV_FinishingMove: ecx = oCNpc* of the player
  constexpr uint32_t PickLockLength         = 0x723EE2; // oCMobLockable: esi = the mob, the hook leaves the length of the code in ecx
  constexpr uint32_t MovingCutscene         = 0x69B9F1; // oCAIHuman::Moving: CoM replaces the check of the global cutscene (ISINCAMERA, ISINFINALBOARDS)
  constexpr uint32_t SpellSetup             = 0x484930; // oCSpell::Setup: ecx = oCSpell*, [esp+4] = the caster
  constexpr uint32_t EquipItem              = 0x7323C0; // oCNpc::EquipItem: ecx = oCNpc*, [esp+4] = the item (the quivers of CoM)
  constexpr uint32_t UnequipItem            = 0x7326C0; // oCNpc::UnequipItem
  constexpr uint32_t DropUnconscious        = 0x735FAB; // oCNpc::DropUnconscious: esi = oCNpc*, CoM keeps the weapons of the hero in some scenes
  constexpr uint32_t StatusScreenShow       = 0x47ED60;
  constexpr uint32_t StatusScreenClose      = 0x47EB50;
  }
