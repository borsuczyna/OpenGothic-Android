#pragma once

#include <zenkit/DaedalusScript.hh>

#include "game/gamescript.h"

#include <map>
#include <functional>
#include <unordered_map>
#include <vector>

#include "cpu32.h"
#include "instancehandles.h"
#include "mem32.h"

class MoveTrigger;
class Serialize;
class Npc;
class Item;
class Interactive;
class Focus;

class DirectMemory {
  public:
    DirectMemory(GameScript& owner, zenkit::DaedalusVm& vm);

    static bool isRequired(zenkit::DaedalusScript& vm);

    auto        demangleAddress(uint32_t addr) -> std::string_view;

    //
    auto        menuMain() const -> std::string_view;

    /// What the libraries have asked for and the layer has not done, for the menu of the developer (mods/devtools)
    auto        report() -> std::string;

    // hooks
    void        tick(uint64_t dt);
    void        eventPlayAni(Npc& npc, std::string_view ani);

    /// The state of the registers and of the stack at a place that the libraries hook. The scripts of the hooks read and
    /// write them (ECX, EAX, ..., MEM_ReadInt(ESP+4)); what they leave there is what the caller gets back.
    struct HookRegs {
      uint32_t             eax = 0, ecx = 0, edx = 0, ebx = 0, ebp = 0, esi = 0, edi = 0;
      std::vector<int32_t> args;    // [esp+4], [esp+8], ...
      };
    bool        hasHook(uint32_t address) const;
    bool        fireHook(uint32_t address, HookRegs& regs);

    /// What the scripts have put on the screen (the views and the texts of LeGo), for the drawing, see directmemory_ui.cpp
    void        collectOverlay(std::vector<GameScript::DmaOverlayItem>& out, int width, int height);

    // events of the game, see directmemory_events.cpp
    void        eventChangeLevel(bool begin);
    void        eventSaveGame(Serialize& fout);
    void        eventBeforeSaveGame();
    auto        saveWorldName(std::string_view fallback) -> std::string;
    auto        loadingTexture() -> std::string;
    void        eventLoadGame(Serialize& fin);
    void        eventItemTaken(Npc& owner, Item& item);
    int         eventChangeAttribute(Npc& npc, int attribute, int change);
    void        eventKey(int key);
    void        eventKeyState(int key, bool down);
    void        eventZoneMusic(std::string_view name);
    void        eventFallDamage(Npc& npc);
    void        eventFocus(Npc& player, const Focus& focus);
    void        eventSpellFocus(Npc& caster, int spellId);
    bool        eventSpellTarget(Interactive& mob, TargetType type);
    void        syncFocusMage(zenkit::IFocus& focus);
    void        clearMobs();
    void        eventMobCanOpen(Interactive& mob);
    bool        eventDropVob(Npc& npc, Item& item);
    void        eventInventoryOpen(Npc& npc);
    void        eventItemInfo(const Item& item);
    void        eventDialogChoice(const GameScript::DlgChoice* choice, bool subChoice, int selection);
    bool        eventSellItem(Npc& npc, Item& item);
    void        eventEquipItem(Npc& npc, Item& item, bool equip);
    bool        eventEquipBestWeapons(Npc& npc);
    bool        eventEnemyCandidate(Npc& npc, Npc& candidate);
    bool        eventDropUnconscious(Npc& npc);
    bool        eventFinishingMove(Npc& npc, Npc& target);
    void        noteAiPlayAni(Npc& npc, std::string_view name);
    size_t      eventPickLockLength(Interactive& mob, size_t length);
    void        eventSpellBegin(Npc& caster);
    bool        spellKeepsItem(int spellId);
    bool        hidesPartyFocus() const;
    bool        dialogCameraDisabled() const { return dialogCameraOff; }
    bool        saveDisabled() const { return saveOff; }
    bool        loadDisabled() const { return loadOff; }
    bool        inCamera();
    bool        cutsceneMenuBlocked();
    bool        cutsceneStatusHidden();
    void        eventDialogMessage(std::string& name, uint32_t& color, bool& bottom, bool playerLine);
    /// who OTHER was in the last command that AI_Function ran (for the developer tests)
    const std::string& lastCallOther() const { return lastCallOtherName; }
    /// Trialoge of LeGo: the commands for the self of the scripts go to the character who speaks now
    auto        triaRedirect(Npc* npc) -> Npc*;
    bool        trialogFinishPending() const;
    bool        underwaterPickup() const { return hasHook(0x699C60); }
    bool        inFinalBoards();
    bool        takeInventoryClose();
    void        eventStatusScreen(bool show);
    void        eventCsCamera(bool trigger);
    auto        takeMenuTexts() -> std::vector<std::pair<std::string,std::string>>;
    void        eventDamageBegin(Npc& victim, Npc& attacker, bool isSpell, int spellId, bool ranged, GameScript::DmaDamage& d);
    void        eventDamageEnd(Npc& victim, GameScript::DmaDamage& d);

  private:
    using ptr32_t      = Mem32::ptr32_t;
    using zCParser     = Compatibility::zCParser;
    using zCPar_Symbol = Compatibility::zCPar_Symbol;
    using zCTimer      = Compatibility::zCTimer;
    using oGame        = Compatibility::oGame;
    using zString      = Compatibility::zString;
    using zCMover      = Compatibility::zCMover;

    struct memory_instance : public zenkit::DaedalusTransientInstance {
      explicit memory_instance(DirectMemory &owner, ptr32_t address) : owner(owner), address(address) {}

      void set_int(zenkit::DaedalusSymbol const& sym, uint16_t index, std::int32_t value) override;
      std::int32_t get_int(zenkit::DaedalusSymbol const& sym, uint16_t index) const override;

      void set_float(zenkit::DaedalusSymbol const& sym, uint16_t index, float value) override;
      float get_float(zenkit::DaedalusSymbol const& sym, uint16_t index) const override;

      void set_string(zenkit::DaedalusSymbol const& sym, uint16_t index, std::string_view value) override;
      const std::string& get_string(zenkit::DaedalusSymbol const& sym, uint16_t index) const override;

      DirectMemory& owner;
      ptr32_t       address = 0;
      };

    struct ScriptVar {
      zString data = {};
      };

    GameScript&         gameScript;
    zenkit::DaedalusVm& vm;
    Mem32               mem32;
    Cpu32               cpu;
    InstanceHandles     handles;

    uint32_t    versionHint     = 504628679; // G2
    //int32_t     invMaxItems     = 9;
    zCTimer     zTimer          = {};
    oGame       memGame         = {};
    char        menuName[17]    = {};   // 16 bytes for script + null-charater, invisible to the script
    float       spawnRange      = 1000; // 10 meters, for now

    ptr32_t     oGame_Pointer   = 0;
    ptr32_t     gameman_Ptr     = 0;
    ptr32_t     zFactory_Ptr    = 0;
    ptr32_t     fontMan_Ptr     = 0;
    ptr32_t     focusList[6]    = {};

    ptr32_t     scriptVariables = 0;
    // the variables of the script in the pretended memory: a slot (20 bytes) holds a variable, five elements of an int
    // array or an element of a string array; the arrays of the scripts (PLAYER_TALENT_COOKING[...]) are read by pointer
    std::vector<uint32_t> varSlotFirst;   // the symbol -> its first slot
    std::vector<uint32_t> varSlotSym;     // the slot -> the symbol
    auto        varAddress(uint32_t symbol, uint32_t element = 0) const -> ptr32_t;
    auto        varSymbolAt(ptr32_t address) const -> int64_t;   // the index of the symbol, -1 if it is not a variable
    ptr32_t     scriptSymbols   = 0;

    void        setupFunctionTable();
    void        setupIkarusLoops();
    void        setupEngineMemory();
    void        setupEngineText();

    auto        repeat(zenkit::DaedalusVm& vm) -> zenkit::DaedalusNakedCall;
    auto        while_(zenkit::DaedalusVm& vm) -> zenkit::DaedalusNakedCall;
    auto        mem_goto(zenkit::DaedalusVm& vm) -> zenkit::DaedalusNakedCall;
    void        loop_trap(zenkit::DaedalusSymbol* i);

    uint32_t    traceBackExpression(zenkit::DaedalusVm& vm, uint32_t pc);
    auto        findSymbolByAddress(uint32_t addr) -> const zenkit::DaedalusSymbol*;

    struct LPayload {
      zenkit::DaedalusSymbol* i   = nullptr;
      int32_t                 len = 0;
      };
    std::unordered_map<uint32_t,LPayload>      loopPayload;
    std::unordered_map<uint32_t,uint32_t>      loopBacktrack;
    std::vector<const zenkit::DaedalusSymbol*> symbolsByAddress;

    // memory mappings
    void        memoryCallback(zCParser& p, std::memory_order ord);
    void        memoryCallback(ScriptVar& v, uint32_t index, std::memory_order ord);
    void        memoryCallback(zCPar_Symbol& s, uint32_t index, std::memory_order ord);
    auto        instancePointer(zenkit::DaedalusSymbol& sym) -> ptr32_t;
    void        memoryCallback(zCMover& m, uint32_t index, std::memory_order ord);
    void        memoryCallback(Compatibility::MobSlot& m, uint32_t index, std::memory_order ord);
    bool        memAssignString(zString& str, std::string_view cstr);
    void        memFromString(std::string& dst, const zString& str);

    // stacktrace & logs
    void        memPrintstacktraceImplementation();
    void        memSendToSpy(int cat, std::string_view msg);
    void        memReplaceFunc(zenkit::DaedalusFunction dest, zenkit::DaedalusFunction func);
    struct StrVar {
      std::string synced;      // the text that the zString and the symbol had the last time they were the same
      ptr32_t     buf   = 0;   // the buffer of the characters that this layer has made
      bool        known = false;
      ptr32_t     zptr  = 0;   // the buffer that a script has put into the zString, and its length
      int32_t     zlen  = 0;
      bool        owned = false;
      };
    std::unordered_map<uint32_t,StrVar> strVars;   // by the slot of the variable
    std::vector<uint32_t>               ownedStrings;
    // Stable values per native zSTRING address: external string_view arguments
    // must not share one scratch string overwritten by the next member read.
    std::unordered_map<ptr32_t,std::string> transientStrings;
    void        syncOwnedStrings(ptr32_t address, uint32_t size);
    // chests and doors as oCMobLockable, the focus of the player, the focus of the magic
    static constexpr uint32_t MobSlots = 256;
    ptr32_t                          mobBase   = 0;
    uint32_t                         mobCursor = 0;
    std::vector<Interactive*>        mobs;
    std::unordered_map<const Interactive*,uint32_t> mobIndex;
    std::vector<std::pair<std::string,std::string>> menuTexts;   // UPDATE_MENU_ITEM: the item of the menu and its new text
    ptr32_t                          parserBase = 0;        // where zCParser (the parser of the engine) is in the memory
    ptr32_t                          worldTimer = 0;        // oCWorldTimer of the scripts (SCALETIME, STOPTIMEATHOUR)
    int32_t                          worldTimerBits = 0;    // what was written there last, to see the writes of the scripts
    int32_t                          worldTimerDay  = 0;
    bool                             worldTimerInit = false;
    bool                             inventoryClose = false;   // oCNpc::CloseInventory was called by a script
    bool                             lockOpenedByScript = false;   // a script has opened a lock since the cast started
    bool                             finishHook = false;       // the hook of the finishing move is running
    bool                             finishRefused = false;    // and it has queued T_NO for the player
    struct PendingInfo {
      ptr32_t ptr   = 0;
      size_t  id    = 0;
      int32_t told  = 0;
      };
    std::vector<PendingInfo>         pendingInfos;   // oCInfo objects that the scripts got from the manager of the dialogs
    std::unordered_map<ptr32_t,std::string> decoVobs;  // the vobs that scripts insert (MEM_InsertVob): pointer -> visual
    bool                             quiversPending = false;
    Npc*                             triaOriginal = nullptr;   // the character of the dialog (the self of the scripts) while a trialog runs
    Npc*                             triaCurrent  = nullptr;   // who speaks now
    bool                             triaFinishing = false;
    std::unordered_map<ptr32_t,std::weak_ptr<zenkit::INpc>> magicBooks;
    std::weak_ptr<zenkit::IItem>       telekinesisItem;
    std::weak_ptr<zenkit::INpc>        telekinesisCaster;
    void        setupTelekinesisFunctions();
    void        finishTelekinesis();
    void        tickTelekinesis();
    std::string                      lastCallOtherName;
    bool                             saveOff = false;           // ENABLESAVEGAME(FALSE) of CoM: no saving in the scenes
    bool                             loadOff = false;           // ENABLELOADGAME(FALSE) of CoM's scripted cameras
    bool                             dialogCameraOff = false;   // DIACAM_DISABLE of CoM patched oCAICamera::StartDialogCam
    struct VobState {
      bool show = true;
      bool collide = true;
      };
    std::map<std::string,VobState>   vobStates;          // CHANGEVOBCOLLISION of the scripts: "WORLD|VOB" -> the state
    bool                             vobStatesPending = false;
    bool                             inDropUnconscious = false;   // the hook of oCNpc::DropUnconscious is running
    bool                             keepHandWeapons   = false;   // and it has taken the weapons from the hands
    ptr32_t                          dropMessage = 0;       // the message of EV_DropVob that the hook is asked about
    bool                             dropCancelled = false;
    ptr32_t                          focusMagic = 0;
    uint8_t                          keyEvents[1100] = {};   // MEM_KeyPressed reads it (the table of the keys of the engine)
    uint8_t                          keyToggles[1100] = {};
    int                              lastSpellFocus = -1;
    auto        mobPointer(Interactive& mob) -> ptr32_t;
    void        setupFocusMagic();
    void        setupQuiverFunctions();
    void        setupTrialogFunctions();
    void        tickQuivers();
    void        tickVobStates();
    void        setupEngineCalls();
    void        syncHandleObject(uint8_t* raw, zenkit::DaedalusInstance& inst, bool write);

    // views, textures and fonts of LeGo
    struct FontInfo {
      std::string name;
      ptr32_t     object = 0;
      };
    uint32_t    fontHandle(std::string_view font);
    ptr32_t     textureHandle(std::string_view texture);
    void        initItemRenderer();
    void        withRecipeTemporary(uint32_t instance, std::string_view prefix, const std::function<void()>& call);
    std::string craftingChoice;
    ptr32_t     renderList = 0, renderWorld = 0;
    std::unordered_map<uint32_t,std::shared_ptr<Item>> previewItems;
    std::vector<ptr32_t> renderedViews;
    int         uiWidth = 800, uiHeight = 600;
    int         uiBlend = 2;
    bool        collectingSprites = false;
    std::vector<GameScript::DmaOverlayItem> spriteOverlay;
    std::vector<FontInfo>                  fonts;        // the handle is the index+1
    std::unordered_map<ptr32_t,uint32_t>   fontByPtr;
    std::unordered_map<std::string,ptr32_t> texObjects;  // the name of a texture -> its object for the scripts
    std::unordered_map<ptr32_t,std::string> texNames;
    std::vector<ptr32_t>                   openViews;    // the last one is in front
    std::vector<std::pair<uint32_t,uint32_t>> replaced;   // MEM_ReplaceFunc: the function and the one that takes its place

    ptr32_t     ASMINT_InternalStack = 0;
    ptr32_t     ASMINT_CallTarget    = 0;
    void        ASMINT_Init();
    void        ASMINT_CallMyExternal();

    // pointers
    void        setupMemoryFunctions();
    auto        mem_ptrtoinst(ptr32_t address) -> std::shared_ptr<zenkit::DaedalusInstance>;
    int         mem_insttoptr(int index);
    auto        _takeref(zenkit::DaedalusVm& vm) -> zenkit::DaedalusNakedCall;
    int         mem_readint       (int address);
    void        mem_writeint      (int address, int val);
    void        mem_copybytes     (int src, int dst, int size);
    std::string mem_readstring    (int address);
    auto        mem_readstatarr   (zenkit::DaedalusVm& vm) -> zenkit::DaedalusNakedCall;
    auto        mem_writestatarr  (zenkit::DaedalusVm& vm) -> zenkit::DaedalusNakedCall;
    // pointers: MEM_Alloc and MEM_Free ##
    int         mem_alloc  (int amount);
    int         mem_alloc  (int amount, const char* comment);
    void        mem_free   (int address);
    int         mem_realloc(int address, int oldsz, int size);

    // mem-func
    void        setupDirectFunctions();
    int         mem_getfuncidbyoffset(int off);
    void        mem_assigninst       (int index, int ptr);

    void        directCall(zenkit::DaedalusVm& vm, zenkit::DaedalusSymbol& func);
    auto        mem_callbyid (zenkit::DaedalusVm& vm) -> zenkit::DaedalusNakedCall;
    auto        mem_callbyptr(zenkit::DaedalusVm& vm) -> zenkit::DaedalusNakedCall;
    // ## Direct calls by id/ptr
    auto        memint_stackpushint (zenkit::DaedalusVm& vm) -> zenkit::DaedalusNakedCall;
    //TODO: MEMINT_StackPushString
    auto        memint_stackpushinst(zenkit::DaedalusVm& vm) -> zenkit::DaedalusNakedCall;
    auto        memint_stackpushvar (zenkit::DaedalusVm& vm) -> zenkit::DaedalusNakedCall;
    //
    auto        memint_popstring    (zenkit::DaedalusVm& vm) -> zenkit::DaedalusNakedCall;
    //---
    auto        mem_popintresult   (zenkit::DaedalusVm& vm) -> zenkit::DaedalusNakedCall;
    auto        mem_popstringresult(zenkit::DaedalusVm& vm) -> zenkit::DaedalusNakedCall;
    auto        mem_popinstresult  (zenkit::DaedalusVm& vm) -> zenkit::DaedalusNakedCall;

    // math
    void        setupMathFunctions();
    static int  mkf(int v);
    static int  truncf(int v);
    static int  roundf(int v);
    static int  addf(int a, int b);
    static int  subf(int a, int b);
    static int  mulf(int a, int b);
    static int  divf(int a, int b);

    // strings
    void        setupStringsFunctions();
    // winapi
    void        setupWinapiFunctions();
    void        setupUtilityFunctions();
    void        setupHookEngine();
    void        setupzCParserFunctions();
    void        setupInitFileFunctions();
    // gothic-ui
    void        setupUiFunctions();
    void        setupFontFunctions();
    void        tickUi(uint64_t dt);
    void        tickTimers(uint64_t dt);
    void        syncWorldTimer();
    void        tickInfos();
    //
    void        setupNpcFunctions();
    void        setupWorldFunctions();

    // vobs of the world that scripts find by name: the movers are mirrored as zCMover, see Compatibility::zCMover
    struct MoverPool {
      ptr32_t                  base  = 0;
      std::vector<MoveTrigger*> list;
      const World*             world = nullptr;
      };
    MoverPool   movers;

    // the places of the engine that the libraries have hooked: address -> indices of the functions of the scripts
    std::unordered_map<uint32_t,std::vector<uint32_t>> hooks;
    static bool isHookAnswered(uint32_t address);
    auto        ptrOf(Npc& npc) -> ptr32_t;
    auto        ptrOf(Item& item) -> ptr32_t;

    // files of the scripts (LeGo keeps the handles of PermMem in the savegame through the functions of Windows)
    struct VirtualHandle {
      std::string name;
      uint32_t    pos   = 0;
      bool        write = false;
      };
    std::map<std::string,std::vector<uint8_t>>  vfs;
    std::unordered_map<uint32_t,VirtualHandle>  vhandles;
    uint32_t                                    nextVHandle = 0x1000;
    uint32_t                                    lastError   = 0;
    bool                                        loadingTextureReady = false;
    ptr32_t                                     saveSlotBlock = 0;
    void        setupFileFunctions();
    void        setupSaveSupport();
    void        setSelectedSlot(int slot);
    static auto vfsKey(std::string_view name) -> std::string;
    static void dumpFile(const char* what, const std::string& name, const std::vector<uint8_t>& data);

    // the places of the engine that the libraries want to hook, which are not answered
    std::vector<std::pair<uint32_t,std::string>> hooksUnanswered;
    void        buildMoverPool();

    // the characters of the world as a list for the scripts (MEM_World.voblist_npcs), made again once in a while
    ptr32_t     npcList      = 0;
    uint64_t    npcListTimer = 0;
    bool        hooksReported = false;
    bool        resolutionFired = false;
    ptr32_t     csCamera = 0;
    void        refreshNpcList();
    void        setupInstanceAliases();
    auto        searchVobsByName(std::string_view name) -> std::vector<ptr32_t>;
    auto        vobNamesSearchedByScripts() -> std::vector<std::string>;
  };
