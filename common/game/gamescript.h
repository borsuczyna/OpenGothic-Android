#pragma once

#include <zenkit/DaedalusVm.hh>
#include <zenkit/addon/daedalus.hh>
#include <zenkit/CutsceneLibrary.hh>

#include <memory>
#include <vector>
#include <set>
#include <random>

#include <Tempest/Matrix4x4>
#include <Tempest/Painter>

#include "game/definitions/svmdefinitions.h"
#include "game/definitions/spelldefinitions.h"
#include "game/aiouputpipe.h"
#include "game/constants.h"
#include "game/aistate.h"
#include "game/questlog.h"
#include "game/damagecalculator.h"

class GameSession;
class World;
class DirectMemory;
class DialogTest;
class Npc;
class Item;
class Interactive;
class Focus;
class VisualFx;
class ParticleFx;
class Serialize;
class Gothic;

class ScriptFn final {
  public:
    ScriptFn()=default;
    ScriptFn(size_t v):ptr(v){}

    size_t ptr=size_t(-1);

    bool isValid() const { return ptr!=size_t(-1); }

  friend bool operator == (const ScriptFn& a,const ScriptFn& b) {
    return a.ptr==b.ptr;
    }
  };

class GameScript final {
  public:
    GameScript(GameSession &owner);
    ~GameScript();

    struct DlgChoice final {
      std::string      title;
      int32_t          sort     = 0;
      uint32_t         scriptFn = 0;
      bool             isTrade  = false;
      bool             isSubChoice = false;
      bool             isInput  = false;   // the title started with "?": the player types an answer (CoM: the name of the dog)
      zenkit::IInfo*   handle   = nullptr;
      };

    struct PerDist {
      PerDist();
      int32_t range[PERC_Count] = {};
      int     at(PercType perc, int r) const;
      };

    bool         hasSymbolName(std::string_view fn);

    void         initializeInstanceNpc(const std::shared_ptr<zenkit::INpc>& npc, size_t instance);
    void         initializeInstanceItem(const std::shared_ptr<zenkit::IItem>& item, size_t instance);
    void         resetVarPointers();

    void         initDialogs();
    void         saveQuests(Serialize& fout);
    void         loadQuests(Serialize& fin);
    void         saveVar(Serialize& fout);
    void         loadVar(Serialize& fin);
    void         savePerc(Serialize& fout);
    void         loadPerc(Serialize& fin);

    inline auto& getVm() { return vm; }
    /// The report of the layer for Ikarus and LeGo, empty if the mod does not use them
    auto         dmaReport() -> std::string;
    /// Ikarus and LeGo (see game/compatibility): the events of the game that the libraries hook in the original engine.
    /// All of them do nothing when the mod does not use the libraries.
    bool         hasDma() const { return dma!=nullptr; }
    void         dmaChangeLevel(bool begin);
    void         dmaSave(Serialize& fout);
    void         dmaBeforeSave();
    auto         dmaSaveWorldName(std::string_view fallback) -> std::string;
    /// The picture of the next loading screen that the scripts choose (CoM: LOA_01..26), empty if they do not
    auto         dmaLoadingTexture() -> std::string;
    void         dmaLoad(Serialize& fin);
    void         dmaItemTaken(Npc& owner, Item& item);
    int          dmaChangeAttribute(Npc& npc, int attribute, int change);
    void         dmaKey(int key);
    void         dmaKeyState(int key, bool down);
    void         dmaZoneMusic(std::string_view name);
    void         dmaFallDamage(Npc& npc);
    void         dmaFocus(Npc& player, const Focus& focus);
    void         dmaStatusScreen(bool show);
    bool         dmaDropVob(Npc& npc, Item& item);
    bool         dmaSellItem(Npc& npc, Item& item);
    void         dmaEquipItem(Npc& npc, Item& item, bool equip);
    bool         dmaEquipBestWeapons(Npc& npc);
    bool         dmaEnemyCandidate(Npc& npc, Npc& candidate);
    bool         dmaDropUnconscious(Npc& npc);
    bool         dmaDialogCameraOff() const;
    bool         dmaTrialogFinishPending() const;
    bool         dmaUnderwaterPickup() const;
    bool         dmaSaveDisabled() const;
    bool         dmaLoadDisabled() const;
    bool         dmaInCamera() const;
    bool         dmaCutsceneMenuBlocked() const;
    bool         dmaCutsceneStatusHidden() const;
    void         dmaDialogMessage(std::string& name, uint32_t& color, bool& bottom, bool playerLine);
    std::string  dmaLastCallOther() const;
    void         logMoveToTop(std::string_view topic);
    bool         dmaFinishingMove(Npc& npc, Npc& target);
    size_t       dmaPickLockLength(Interactive& mob, size_t length);
    /// The names of the categories of the inventory in the scripts of CoM (TXT_INV_CAT: all, weapons, armor, magic, ...),
    /// empty for the games that do not have the categories
    const std::vector<std::string>& inventoryCategoryNames();
    uint32_t     inventoryCategoryMask(int index) const;
    int          inventoryScriptCategory() const;
    void         setInventoryScriptCategory(int index);
    /// oCInfo.told of the engine (SetDialogueToldProperty of CoM): the dialog has been told, false lets it be told again
    bool         isInfoTold (size_t infoInstance);
    void         setInfoTold(size_t infoInstance, bool told);
    void         dmaSpellBegin(Npc& caster);
    bool         dmaSpellKeepsItem(int spellId);
    /// The mods of Ikarus and LeGo (CoM) keep the party members out of the focus of a player with a weapon
    bool         dmaHidesPartyFocus() const;
    bool         dmaInFinalBoards() const;
    void         dmaInventoryOpen(Npc& npc);
    void         dmaItemInfo(const Item& item);
    void         dmaDialogChoice(const DlgChoice* choice, bool subChoice, int selection);
    bool         dmaTakeInventoryClose();
    void         dmaCsCamera(bool trigger);
    auto         dmaTakeMenuTexts() -> std::vector<std::pair<std::string,std::string>>;
    void         dmaMobCanOpen(Interactive& mob);
    void         dmaSpellFocus(Npc& caster, int spellId);
    bool         dmaSpellTarget(Interactive& mob, TargetType type);
    struct DmaDamage final {
      uint32_t desc     = 0;     // oSDamageDescriptor in the memory of the layer
      bool     dontKill = false;
      DamageCalculator::Damage damage;
      };
    /// A view or a text of LeGo, the positions are in the virtual coordinates of Gothic (0..8192)
    struct DmaOverlayItem final {
      enum Kind : uint8_t { Image, Text, Triangle, Model };
      Kind        kind  = Text;
      float       x = 0, y = 0, w = 0, h = 0;
      std::string name;                 // the texture or the text
      std::string font;
      uint32_t    color = 0xFFFFFFFFu;  // ARGB
      struct Vertex { float x=0, y=0, u=0, v=0; } vertices[3];
      int         blend = 2;           // Gothic: 1 opaque, 2 alpha, 3 additive
      int         priority = 0;
      std::shared_ptr<zenkit::IItem> model;
      };
    void         dmaOverlay(std::vector<DmaOverlayItem>& out, int width, int height);
    auto         dmaDamageBegin(Npc& victim, Npc& attacker, bool isSpell, int spellId, bool ranged, bool dontKill,
                                const DamageCalculator::Damage& damage) -> DmaDamage;
    void         dmaDamageEnd(Npc& victim, DmaDamage& d);
    auto         questLog() const -> const QuestLog&;
    /// The chapter of the story (KAPITEL of the scripts), 0 when the scripts have none
    int          chapter() const;

    const World& world() const;
    World&       world();
    uint64_t     tickCount() const;
    void         tick(uint64_t dt);

    uint32_t     rand(uint32_t max);
    void         removeItem(Item& it);

    void         setInstanceNPC (std::string_view name, Npc& npc);
    void         setInstanceItem(Npc& holder, size_t itemId);

    AiOuputPipe* openAiOuput();
    AiOuputPipe* openDlgOuput(Npc &player, Npc &npc);

    auto*            goldId() const { return itMi_Gold; }
    ScriptFn         playerPercAssessMagic();
    std::string_view currencyName() const { return goldTxt; }
    int              npcDamDiveTime();
    int32_t          criticalDamageMultiplyer() const;
    auto*            lockPickSymbol() const { return ItKE_lockpick; }
    uint32_t         lockPickId() const;

    const zenkit::IFocus&       focusNorm()  const { return cFocusNorm;  }
    const zenkit::IFocus&       focusMelee() const { return cFocusMelee; }
    const zenkit::IFocus&       focusRange() const { return cFocusRange; }
    const zenkit::IFocus&       focusMage()  const;
    const zenkit::IGuildValues& guildVal()   const { return *cGuildVal;  }

    std::string_view            menuMain() const;

    zenkit::DaedalusSymbol*     findSymbol(std::string_view s);
    zenkit::DaedalusSymbol*     findSymbol(const size_t s);
    size_t                      findSymbolIndex(std::string_view s);
    size_t                      symbolsCount() const;

    const AiState&              aiState  (ScriptFn id);
    const zenkit::ISpell&       spellDesc(int32_t splId);
    const VisualFx*             spellVfx (int32_t splId);

    auto dialogChoices(std::shared_ptr<zenkit::INpc> self, std::shared_ptr<zenkit::INpc> npc, const std::vector<uint32_t> &except, bool includeImp) -> std::vector<DlgChoice>;
    auto updateDialog (const GameScript::DlgChoice &dlg, Npc &player, Npc &npc) -> std::vector<GameScript::DlgChoice>;
    void exec(const DlgChoice &dlg, Npc &player, Npc &npc);
    void setDialogAnswer(std::string_view text);
    bool setInfoKnown(Npc& npc, std::string_view infoName, bool known);
    /// -1: no such info, otherwise what the condition of the info says for the npc and the player (the dialog that does not appear)
    int  debugInfoCondition(Npc& npc, Npc& player, std::string_view infoName, bool& known, std::string& detail);

    void  printCannotUseError         (Npc &npc, int32_t atr, int32_t nValue);
    void  printCannotCastError        (Npc &npc, int32_t plM, int32_t itM);
    void  printCannotBuyError         (Npc &npc);
    void  printMobMissingItem         (Npc &npc);
    void  printMobAnotherIsUsing      (Npc &npc);
    void  printMobMissingKey          (Npc &npc);
    void  printMobMissingKeyOrLockpick(Npc &npc);
    void  printMobMissingLockpick     (Npc &npc);
    void  printMobTooFar              (Npc &npc);

    void invokeState(const std::shared_ptr<zenkit::INpc>& hnpc, const std::shared_ptr<zenkit::INpc>& hother, const char* name);
    int  invokeState(Npc* npc, Npc* other, Npc *victim, ScriptFn fn);
    void invokeItem (Npc* npc, ScriptFn fn, Item* itm = nullptr);
    int  invokeMana (Npc& npc, Npc* target, int mana);
    int  invokeManaRelease (Npc& npc, Npc* target, int mana);
    void invokeSpell(Npc& npc, Npc *target, Item&  fn);
    int  invokeCond (Npc& npc, std::string_view func);
    void invokePickLock(Npc& npc, int bSuccess, int bBrokenOpen);
    void invokeRefreshAtInsert(Npc& npc);
    auto canNpcCollideWithSpell(Npc& npc, Npc* shooter, int32_t spellId) -> CollideMask;

    int  playerHotKeyScreenMap_G1(Npc& pl);
    int  playerHotKeyScreenMap(Npc& pl);
    void playerHotLamePotion(Npc& pl);
    void playerHotLameHeal(Npc& pl);

    const PerDist& percRanges() const { return perceptionRanges; }

    bool isDead       (const Npc &pl);
    bool isUnconscious(const Npc &pl);
    bool isTalk       (const Npc &pl);
    bool isAttack     (const Npc &pl) const;

    std::string_view spellCastAnim(Npc& npc, Item&  fn);
    std::string_view messageFromSvm(std::string_view id,int voice) const;
    std::string_view messageByName (std::string_view id) const;
    uint32_t         messageTime   (std::string_view id) const;

    void     printNothingToGet();
    float    tradeValueMultiplier() const { return tradeValMult; }
    void     useInteractive(const std::shared_ptr<zenkit::INpc>& hnpc, std::string_view func);
    Attitude guildAttitude(const Npc& p0,const Npc& p1) const;
    Attitude personAttitude(const Npc& p0,const Npc& p1) const;
    bool     isFriendlyFire(const Npc& src, const Npc& dst) const;

    BodyState schemeToBodystate(std::string_view sc);

    void      onWldItemRemoved(const Item& itm);
    void      fixNpcPosition(Npc& npc, float angle0, float distBias);

    void      eventPlayAni(Npc& npc, std::string_view ani);

    /// The developer tool of the dialogs (DialogTest)
    auto&     infoList() const { return dialogsInfo; }
    bool      npcKnowsInfo(Npc& npc, size_t infoInstance) const;
    DialogTest& dialogTest();

  private:
    template<typename T>
    struct DetermineSignature {
      using signature = void();
      };

    template <typename C, typename R, typename ... P>
    struct DetermineSignature<R(C::*)(P...)> {
      using signature = R(P...);
      };

    struct GlobalOutput : AiOuputPipe {
      explicit GlobalOutput(GameScript& owner):owner(owner){}

      bool output   (Npc &npc, std::string_view text) override;
      bool outputSvm(Npc& npc, std::string_view text) override;
      bool outputOv (Npc& npc, std::string_view text) override;
      bool printScr (Npc& npc, int time, std::string_view msg, int x,int y, std::string_view font) override;
      bool isFinished() override;

      bool close() override { return true; }

      GameScript& owner;
      };

    template <class F>
    void bindExternal(const std::string& name, F function) {
      vm.register_external(name, std::function<typename DetermineSignature<F>::signature> (
                                   [this, function](auto ... v) { return (this->*function)(v...); }));
      }

    void  initCommon();
    void  initSettings();
    void  loadDialogOU();

    Item* findItem(zenkit::IItem* handle);
    Item* findItemById(size_t id);
    Item* findItemById(const std::shared_ptr<zenkit::DaedalusInstance>& handle);
    Npc*  findNpc(zenkit::DaedalusSymbol* s);
    Npc*  findNpc(zenkit::INpc*           handle);
    Npc*  findNpc(const std::shared_ptr<zenkit::INpc>& handle);
    Npc*  findNpcRaw(const std::shared_ptr<zenkit::INpc>& handle);
    Npc*  findNpcById (const std::shared_ptr<zenkit::DaedalusInstance>& handle);
    Npc*  findNpcById (size_t id);
    auto  findInfo    (size_t id) -> zenkit::IInfo*;
    auto  findFocus(std::string_view name) -> zenkit::IFocus;

    void storeItem(Item* it);

    bool aiOutput   (Npc &from, std::string_view name, bool overlay);
    bool aiOutputSvm(Npc &from, std::string_view name, bool overlay);

    bool searchScheme(std::string_view sc, std::string_view listName);

    static zenkit::DaedalusVm createVm(Gothic& gothic);

    bool game_initgerman     ();
    bool game_initenglish    ();

    int  hlp_random          (int bound);
    bool hlp_isvalidnpc      (std::shared_ptr<zenkit::INpc> npcRef);
    bool hlp_isitem          (std::shared_ptr<zenkit::IItem> itemRef, int instanceSymbol);
    bool hlp_isvaliditem     (std::shared_ptr<zenkit::IItem> itemRef);
    int  hlp_getinstanceid   (std::shared_ptr<zenkit::DaedalusInstance> instance);
    auto hlp_getnpc          (int instanceSymbol) -> std::shared_ptr<zenkit::INpc>;

    void wld_insertitem      (int itemInstance, std::string_view spawnpoint);
    void wld_insertobject    (std::string_view visual, std::string_view spawnpoint);
    int  wld_removeitem      (std::shared_ptr<zenkit::IItem> itemRef);
    void mob_createitems     (std::string_view tag, int itemInstance, int amount);
    void wld_insertnpc       (int npcInstance, std::string_view spawnpoint);
    void wld_removenpc       (int npcInstance);
    void wld_settime         (int hour, int minute);
    int  wld_getday          ();
    void wld_playeffect      (std::string_view visual, std::shared_ptr<zenkit::DaedalusInstance> sourceId, std::shared_ptr<zenkit::DaedalusInstance> targetId, int effectLevel, int damage, int damageType, int isProjectile);
    void wld_stopeffect      (std::string_view visual);
    int  wld_getplayerportalguild();
    int  wld_getformerplayerportalguild();
    void wld_setguildattitude (int gil1, int att, int gil2);
    int  wld_getguildattitude (int g1, int g0);
    void wld_exchangeguildattitudes (std::string_view name);
    bool wld_istime           (int hour0, int min0, int hour1, int min1);
    bool wld_isfpavailable    (std::shared_ptr<zenkit::INpc> self, std::string_view name);
    bool wld_isnextfpavailable(std::shared_ptr<zenkit::INpc> self, std::string_view name);
    bool wld_ismobavailable   (std::shared_ptr<zenkit::INpc> self, std::string_view name);
    void wld_setmobroutine    (int h, int m, std::string_view name, int st);
    void wld_setobjectroutine (int h, int m, std::string_view name, int st);
    int  wld_getmobstate      (std::shared_ptr<zenkit::INpc> npc, std::string_view scheme);
    void wld_assignroomtoguild(std::string_view name, int g);
    bool wld_detectnpc        (std::shared_ptr<zenkit::INpc> npcRef, int inst, int state, int guild);
    bool wld_detectnpcex      (std::shared_ptr<zenkit::INpc> npcRef, int inst, int state, int guild, int player);
    bool wld_detectitem       (std::shared_ptr<zenkit::INpc> npcRef, int flags);
    void wld_spawnnpcrange    (std::shared_ptr<zenkit::INpc> npcRef, int clsId, int count, float lifeTime);
    void wld_sendtrigger      (std::string_view triggerTarget);
    void wld_senduntrigger    (std::string_view triggerTarget);
    bool wld_israining        ();

    void mdl_setvisual       (std::shared_ptr<zenkit::INpc> npcRef, std::string_view visual);
    void mdl_setvisualbody   (std::shared_ptr<zenkit::INpc> npcRef, std::string_view body, int bodyTexNr, int bodyTexColor, std::string_view head, int headTexNr, int teethTexNr, int armor);
    void mdl_setmodelfatness (std::shared_ptr<zenkit::INpc> npcRef, float fat);
    void mdl_applyoverlaymds (std::shared_ptr<zenkit::INpc> npcRef, std::string_view overlayname);
    void mdl_applyoverlaymdstimed(std::shared_ptr<zenkit::INpc> npcRef, std::string_view overlayname, int ticks);
    void mdl_removeoverlaymds(std::shared_ptr<zenkit::INpc> npcRef, std::string_view overlayname);
    void mdl_setmodelscale   (std::shared_ptr<zenkit::INpc> npcRef, float x, float y, float z);
    void mdl_startfaceani    (std::shared_ptr<zenkit::INpc> npcRef, std::string_view ani, float intensity, float time);
    void mdl_applyrandomani  (std::shared_ptr<zenkit::INpc> npcRef, std::string_view s1, std::string_view s0);
    void mdl_applyrandomanifreq(std::shared_ptr<zenkit::INpc> npcRef, std::string_view s1, float f0);
    void mdl_applyrandomfaceani(std::shared_ptr<zenkit::INpc> npcRef, std::string_view name, float timeMin, float timeMinVar, float timeMax, float timeMaxVar, float probMin);

    void npc_settofightmode  (std::shared_ptr<zenkit::INpc> npcRef, int weaponSymbol);
    void npc_settofistmode   (std::shared_ptr<zenkit::INpc> npcRef);
    bool npc_isinstate       (std::shared_ptr<zenkit::INpc> npcRef, int stateFn);
    bool npc_isinroutine     (std::shared_ptr<zenkit::INpc> npcRef, int stateFn);
    bool npc_wasinstate      (std::shared_ptr<zenkit::INpc> npcRef, int stateFn);
    int  npc_getdisttowp     (std::shared_ptr<zenkit::INpc> npcRef, std::string_view wpname);
    void npc_exchangeroutine (std::shared_ptr<zenkit::INpc> npcRef, std::string_view rname);
    bool npc_isdead          (std::shared_ptr<zenkit::INpc> npcRef);
    bool npc_knowsinfo       (std::shared_ptr<zenkit::INpc> npcRef, int infoinstance);
    void npc_settalentskill  (std::shared_ptr<zenkit::INpc> npcRef, int t, int lvl);
    int  npc_gettalentskill  (std::shared_ptr<zenkit::INpc> npcRef, int skillId);
    void npc_settalentvalue  (std::shared_ptr<zenkit::INpc> npcRef, int t, int lvl);
    int  npc_gettalentvalue  (std::shared_ptr<zenkit::INpc> npcRef, int skillId);
    void npc_setrefusetalk   (std::shared_ptr<zenkit::INpc> npcRef, int timeSec);
    bool npc_refusetalk      (std::shared_ptr<zenkit::INpc> npcRef);
    int  npc_hasitems        (std::shared_ptr<zenkit::INpc> npcRef, int itemId);
    bool npc_hasspell        (std::shared_ptr<zenkit::INpc> npcRef, int splId);
    int  npc_getinvitem      (std::shared_ptr<zenkit::INpc> npcRef, int itemId);
    int  npc_getinvitembyslot(std::shared_ptr<zenkit::INpc> npcRef, int cat, int slotnr);
    int  npc_removeinvitem   (std::shared_ptr<zenkit::INpc> npcRef, int itemId);
    int  npc_removeinvitems  (std::shared_ptr<zenkit::INpc> npcRef, int itemId, int amount);
    int  npc_getbodystate    (std::shared_ptr<zenkit::INpc> npcRef);
    std::shared_ptr<zenkit::INpc> npc_getlookattarget (std::shared_ptr<zenkit::INpc> npcRef);
    int  npc_getdisttonpc    (std::shared_ptr<zenkit::INpc> aRef, std::shared_ptr<zenkit::INpc> bRef);
    bool npc_hasequippedarmor(std::shared_ptr<zenkit::INpc> npcRef);
    void npc_setperctime     (std::shared_ptr<zenkit::INpc> npcRef, float sec);
    void npc_percenable      (std::shared_ptr<zenkit::INpc> npcRef, int pr, int fn);
    void npc_percdisable     (std::shared_ptr<zenkit::INpc> npcRef, int pr);
    std::string npc_getnearestwp    (std::shared_ptr<zenkit::INpc> npcRef);
    std::string npc_getnextwp(std::shared_ptr<zenkit::INpc> npcRef);
    void npc_clearaiqueue    (std::shared_ptr<zenkit::INpc> npcRef);
    bool npc_isplayer        (std::shared_ptr<zenkit::INpc> npcRef);
    int  npc_getstatetime    (std::shared_ptr<zenkit::INpc> npcRef);
    void npc_setstatetime    (std::shared_ptr<zenkit::INpc> npcRef, int val);
    void npc_changeattribute (std::shared_ptr<zenkit::INpc> npcRef, int atr, int val);
    bool npc_isonfp          (std::shared_ptr<zenkit::INpc> npcRef, std::string_view val);
    int  npc_getheighttonpc  (std::shared_ptr<zenkit::INpc> aRef, std::shared_ptr<zenkit::INpc> bRef);
    std::shared_ptr<zenkit::IItem> npc_getequippedmeleeweapon (std::shared_ptr<zenkit::INpc> npcRef);
    std::shared_ptr<zenkit::IItem> npc_getequippedrangedweapon(std::shared_ptr<zenkit::INpc> npcRef);
    std::shared_ptr<zenkit::IItem> npc_getequippedarmor(std::shared_ptr<zenkit::INpc> npcRef);
    bool npc_canseenpc       (std::shared_ptr<zenkit::INpc> npcRef, std::shared_ptr<zenkit::INpc> otherRef);
    bool npc_canseenpcfreelos(std::shared_ptr<zenkit::INpc> npcRef, std::shared_ptr<zenkit::INpc> otherRef);
    bool npc_canseeitem      (std::shared_ptr<zenkit::INpc> npcRef, std::shared_ptr<zenkit::IItem> itemRef);
    bool npc_hasequippedweapon(std::shared_ptr<zenkit::INpc> npcRef);
    bool npc_hasequippedmeleeweapon(std::shared_ptr<zenkit::INpc> npcRef);
    bool npc_hasequippedrangedweapon(std::shared_ptr<zenkit::INpc> npcRef);
    int  npc_getactivespell  (std::shared_ptr<zenkit::INpc> npcRef);
    bool npc_getactivespellisscroll(std::shared_ptr<zenkit::INpc> npcRef);
    bool npc_isinfightmode   (std::shared_ptr<zenkit::INpc> npcRef, int modeI);
    int  npc_getactivespellcat(std::shared_ptr<zenkit::INpc> npcRef);
    int  npc_setactivespellinfo(std::shared_ptr<zenkit::INpc> npcRef, int v);
    int  npc_getactivespelllevel(std::shared_ptr<zenkit::INpc> npcRef);
    void npc_settarget       (std::shared_ptr<zenkit::INpc> npcRef, std::shared_ptr<zenkit::INpc> otherRef);
    bool npc_gettarget       (std::shared_ptr<zenkit::INpc> npcRef);
    bool npc_getnexttarget   (std::shared_ptr<zenkit::INpc> npcRef);
    void npc_sendpassiveperc (std::shared_ptr<zenkit::INpc> npcRef, int id, std::shared_ptr<zenkit::INpc> victimRef, std::shared_ptr<zenkit::INpc> otherRef);
    void npc_sendsingleperc  (std::shared_ptr<zenkit::INpc> npcRef, std::shared_ptr<zenkit::INpc> otherRef, int id);
    bool npc_checkinfo       (std::shared_ptr<zenkit::INpc> npcRef, int imp);
    int  npc_getportalguild  (std::shared_ptr<zenkit::INpc> npcRef);
    bool npc_isinplayersroom (std::shared_ptr<zenkit::INpc> npcRef);
    std::shared_ptr<zenkit::IItem> npc_getreadiedweapon(std::shared_ptr<zenkit::INpc> npcRef);
    bool npc_hasreadiedweapon(std::shared_ptr<zenkit::INpc> npcRef);
    bool npc_hasreadiedmeleeweapon(std::shared_ptr<zenkit::INpc> npcRef);
    bool npc_hasreadiedrangedweapon(std::shared_ptr<zenkit::INpc> npcRef);
    bool npc_hasrangedweaponwithammo(std::shared_ptr<zenkit::INpc> npcRef);
    int  npc_isdrawingspell  (std::shared_ptr<zenkit::INpc> npcRef);
    int  npc_isdrawingweapon (std::shared_ptr<zenkit::INpc> npcRef);
    void npc_perceiveall     (std::shared_ptr<zenkit::INpc> npcRef);
    void npc_stopani         (std::shared_ptr<zenkit::INpc> npcRef, std::string_view name);
    int  npc_settrueguild    (std::shared_ptr<zenkit::INpc> npcRef, int gil);
    int  npc_gettrueguild    (std::shared_ptr<zenkit::INpc> npcRef);
    void npc_clearinventory  (std::shared_ptr<zenkit::INpc> npcRef);
    int  npc_getattitude     (std::shared_ptr<zenkit::INpc> aRef, std::shared_ptr<zenkit::INpc> bRef);
    int  npc_getpermattitude (std::shared_ptr<zenkit::INpc> aRef, std::shared_ptr<zenkit::INpc> bRef);
    void npc_setattitude     (std::shared_ptr<zenkit::INpc> npcRef, int att);
    void npc_settempattitude (std::shared_ptr<zenkit::INpc> npcRef, int att);
    bool npc_hasbodyflag     (std::shared_ptr<zenkit::INpc> npcRef, int bodyflag);
    int  npc_getlasthitspellid(std::shared_ptr<zenkit::INpc> npcRef);
    int  npc_getlasthitspellcat(std::shared_ptr<zenkit::INpc> npcRef);
    void npc_playani         (std::shared_ptr<zenkit::INpc> npcRef, std::string_view name);
    bool npc_isdetectedmobownedbynpc(std::shared_ptr<zenkit::INpc> usrRef, std::shared_ptr<zenkit::INpc> npcRef);
    std::string npc_getdetectedmob  (std::shared_ptr<zenkit::INpc> npcRef);
    bool npc_ownedbynpc      (std::shared_ptr<zenkit::IItem> itmRef, std::shared_ptr<zenkit::INpc> npcRef);
    bool npc_canseesource    (std::shared_ptr<zenkit::INpc> npcRef);
    bool npc_isincutscene    (std::shared_ptr<zenkit::INpc> npcRef);
    int  npc_getdisttoitem   (std::shared_ptr<zenkit::INpc> npcRef, std::shared_ptr<zenkit::IItem> itmRef);
    int  npc_getheighttoitem (std::shared_ptr<zenkit::INpc> npcRef, std::shared_ptr<zenkit::IItem> itmRef);
    int  npc_getdisttoplayer (std::shared_ptr<zenkit::INpc> npcRef);
    bool npc_isdetectedmobownedbyguild(std::shared_ptr<zenkit::INpc> npcRef, int guild);

    void ai_output           (std::shared_ptr<zenkit::INpc> selfRef, std::shared_ptr<zenkit::INpc> targetRef, std::string_view outputname);
    void ai_stopprocessinfos (std::shared_ptr<zenkit::INpc> selfRef);
    void ai_processinfos     (std::shared_ptr<zenkit::INpc> npcRef);
    void ai_standup          (std::shared_ptr<zenkit::INpc> selfRef);
    void ai_standupquick     (std::shared_ptr<zenkit::INpc> selfRef);
    void ai_continueroutine  (std::shared_ptr<zenkit::INpc> selfRef);
    void ai_stoplookat       (std::shared_ptr<zenkit::INpc> selfRef);
    void ai_lookat           (std::shared_ptr<zenkit::INpc> selfRef, std::string_view waypoint);
    void ai_lookatnpc        (std::shared_ptr<zenkit::INpc> selfRef, std::shared_ptr<zenkit::INpc> npcRef);
    void ai_removeweapon     (std::shared_ptr<zenkit::INpc> npcRef);
    void ai_unreadyspell     (std::shared_ptr<zenkit::INpc> npcRef);
    void ai_turnaway         (std::shared_ptr<zenkit::INpc> selfRef, std::shared_ptr<zenkit::INpc> npcRef);
    void ai_turntonpc        (std::shared_ptr<zenkit::INpc> selfRef, std::shared_ptr<zenkit::INpc> npcRef);
    void ai_whirlaround      (std::shared_ptr<zenkit::INpc> selfRef, std::shared_ptr<zenkit::INpc> npcRef);
    void ai_outputsvm        (std::shared_ptr<zenkit::INpc> selfRef, std::shared_ptr<zenkit::INpc> targetRef, std::string_view name);
    void ai_outputsvm_overlay(std::shared_ptr<zenkit::INpc> selfRef, std::shared_ptr<zenkit::INpc> targetRef, std::string_view name);
    void ai_startstate       (std::shared_ptr<zenkit::INpc> selfRef, int func, int state, std::string_view wp);
    void ai_playani          (std::shared_ptr<zenkit::INpc> npcRef, std::string_view name);
    void ai_setwalkmode      (std::shared_ptr<zenkit::INpc> npcRef, int modeBits);
    void ai_wait             (std::shared_ptr<zenkit::INpc> npcRef, float ms);
    void ai_waitms           (std::shared_ptr<zenkit::INpc> npcRef, int ms);
    void ai_waittillend      (std::shared_ptr<zenkit::INpc> npcRef, std::shared_ptr<zenkit::INpc> otherRef);
    void ai_aimat            (std::shared_ptr<zenkit::INpc> npcRef, std::shared_ptr<zenkit::INpc> otherRef);
    void ai_shootat          (std::shared_ptr<zenkit::INpc> npcRef, std::shared_ptr<zenkit::INpc> otherRef);
    void ai_stopaim          (std::shared_ptr<zenkit::INpc> npcRef);
    void ai_dropitem         (std::shared_ptr<zenkit::INpc> npcRef, int itemInstance);
    void ai_snd_play         (std::shared_ptr<zenkit::INpc> npcRef, std::string_view sound);
    void ai_stopfx           (std::shared_ptr<zenkit::INpc> npcRef, std::string_view effect);
    void ai_aligntowp        (std::shared_ptr<zenkit::INpc> npcRef);
    void ai_gotowp           (std::shared_ptr<zenkit::INpc> npcRef, std::string_view waypoint);
    void ai_gotofp           (std::shared_ptr<zenkit::INpc> npcRef, std::string_view waypoint);
    void ai_playanibs        (std::shared_ptr<zenkit::INpc> npcRef, std::string_view ani, int bs);
    void ai_equiparmor       (std::shared_ptr<zenkit::INpc> npcRef, int id);
    void ai_equipbestarmor   (std::shared_ptr<zenkit::INpc> npcRef);
    int  ai_equipbestmeleeweapon (std::shared_ptr<zenkit::INpc> npcRef);
    int  ai_equipbestrangedweapon(std::shared_ptr<zenkit::INpc> npcRef);
    bool ai_usemob           (std::shared_ptr<zenkit::INpc> npcRef, std::string_view tg, int state);
    void ai_teleport         (std::shared_ptr<zenkit::INpc> npcRef, std::string_view tg);
    void ai_stoppointat      (std::shared_ptr<zenkit::INpc> npcRef);
    void ai_drawweapon       (std::shared_ptr<zenkit::INpc> npcRef);
    void ai_readymeleeweapon (std::shared_ptr<zenkit::INpc> npcRef);
    void ai_readyrangedweapon(std::shared_ptr<zenkit::INpc> npcRef);
    void ai_readyspell       (std::shared_ptr<zenkit::INpc> npcRef, int spell, int mana);
    void ai_attack            (std::shared_ptr<zenkit::INpc> npcRef);
    void ai_flee             (std::shared_ptr<zenkit::INpc> npcRef);
    void ai_dodge            (std::shared_ptr<zenkit::INpc> npcRef);
    void ai_unequipweapons   (std::shared_ptr<zenkit::INpc> npcRef);
    void ai_unequiparmor     (std::shared_ptr<zenkit::INpc> npcRef);
    void ai_gotonpc          (std::shared_ptr<zenkit::INpc> npcRef, std::shared_ptr<zenkit::INpc> toRef);
    void ai_gotonextfp       (std::shared_ptr<zenkit::INpc> npcRef, std::string_view to);
    void ai_aligntofp        (std::shared_ptr<zenkit::INpc> npcRef);
    void ai_useitem          (std::shared_ptr<zenkit::INpc> npcRef, int item);
    void ai_useitemtostate   (std::shared_ptr<zenkit::INpc> npcRef, int item, int state);
    void ai_setnpcstostate   (std::shared_ptr<zenkit::INpc> npcRef, int state, int radius);
    void ai_finishingmove    (std::shared_ptr<zenkit::INpc> npcRef, std::shared_ptr<zenkit::INpc> othRef);
    void ai_takeitem         (std::shared_ptr<zenkit::INpc> npcRef, std::shared_ptr<zenkit::IItem> itmRef);
    void ai_gotoitem         (std::shared_ptr<zenkit::INpc> npcRef, std::shared_ptr<zenkit::IItem> itmRef);
    void ai_pointat          (std::shared_ptr<zenkit::INpc> npcRef, std::string_view waypoint);
    void ai_pointatnpc       (std::shared_ptr<zenkit::INpc> npcRef, std::shared_ptr<zenkit::INpc> otherRef);
    int  ai_printscreen      (std::string_view msg, int posx, int posy, std::string_view font, int timesec);

    int  mob_hasitems        (std::string_view tag, int item);

    void ta_min              (std::shared_ptr<zenkit::INpc> npcRef, int start_h, int start_m, int stop_h, int stop_m, int action, std::string_view waypoint);

    void log_createtopic     (std::string_view topicName, int section);
    void log_settopicstatus  (std::string_view topicName, int status);
    void log_addentry        (std::string_view topicName, std::string_view entry);

    void equipitem           (std::shared_ptr<zenkit::INpc> npcRef, int cls);
    void createinvitem       (std::shared_ptr<zenkit::INpc> npcRef, int itemInstance);
    void createinvitems      (std::shared_ptr<zenkit::INpc> npcRef, int itemInstance, int amount);

    void perc_setrange       (int perc, int dist);

    void info_addchoice      (int infoInstance, std::string_view text, int func);
    void info_clearchoices   (int infoInstance);
    bool infomanager_hasfinished();

    void snd_play            (std::string_view fileS);
    void snd_play3d          (std::shared_ptr<zenkit::INpc> npcRef, std::string_view fileS);

    void exitsession         ();

    void sort(std::vector<DlgChoice>& dlg);
    void setNpcInfoKnown(const zenkit::INpc& npc, const zenkit::IInfo& info);
    bool doesNpcKnowInfo(const zenkit::INpc& npc, size_t infoInstance) const;

    void saveSym(Serialize& fout, zenkit::DaedalusSymbol& s);

    void onWldInstanceRemoved(const zenkit::DaedalusInstance* obj);
    void makeCurrent(Item* w);

    GameSession&                                                owner;
    zenkit::DaedalusVm                                          vm;
    int32_t                                                     vmLang = -1;
    std::mt19937                                                randGen;
    NpcProcessPolicy                                            aiProcessPolicy = NpcProcessPolicy::AiNormal;

    std::unique_ptr<DirectMemory>                               dma;
    std::unique_ptr<DialogTest>                                 dlgTest;

    std::unique_ptr<SpellDefinitions>                           spells;
    std::unique_ptr<SvmDefinitions>                             svm;
    uint64_t                                                    svmBarrier=0;

    std::set<std::pair<size_t,size_t>>                          dlgKnownInfos;
    std::vector<std::string>                                    invCategoryNames;
    bool                                                        invCategoryNamesInit = false;
    std::vector<std::shared_ptr<zenkit::IInfo>>                 dialogsInfo;
    zenkit::CutsceneLibrary                                     dialogs;
    std::unordered_map<size_t,AiState>                          aiStates;
    std::unique_ptr<AiOuputPipe>                                aiDefaultPipe;

    QuestLog                                                    quests;
    zenkit::DaedalusSymbol*                                     itMi_Gold = nullptr;
    zenkit::DaedalusSymbol*                                     ItKE_lockpick = nullptr;
    zenkit::DaedalusSymbol*                                     B_RefreshAtInsert = nullptr;
    float                                                       tradeValMult = 0.3f;
    zenkit::DaedalusSymbol*                                     spellFxInstanceNames = nullptr;
    zenkit::DaedalusSymbol*                                     spellFxAniLetters = nullptr;
    std::string                                                 goldTxt;
    float                                                       viewTimePerChar = 0.5;
    int32_t                                                     damCriticalMultiplier = 2;
    mutable std::unordered_map<std::string,uint32_t>            msgTimings;
    size_t                                                      gilTblSize=0;
    size_t                                                      gilCount=0;
    std::vector<int32_t>                                        gilAttitudes;
    int                                                         aiOutOrderId=0;

    PerDist                                                     perceptionRanges;

    size_t                                                      ZS_Dead=0;
    size_t                                                      ZS_Unconscious=0;
    size_t                                                      ZS_Talk=0;
    size_t                                                      ZS_Attack=0;
    size_t                                                      ZS_MM_Attack=0;

    zenkit::IFocus                                              cFocusNorm,cFocusMelee,cFocusRange,cFocusMage;
    std::shared_ptr<zenkit::IGuildValues>                       cGuildVal;

  friend struct ScopeCtx;
  };
