# Rzeczywiste blockery Kronik — przegląd statyczny 26.10.2026

Zakres: lista z handoffu z 26.10 (29 adresów bez handlera, 40 hooków bez mostu, 2 atrapy, 39 dynamicznych
callbacków, 11 oczekujących pozycji z sekcji 7B). Metoda: świeży audyt na HEAD **7ba334d2** i DAT
SHA256 `7fba1af7…d44c49` (zgodny z handoffem), odczyt dekompilacji, dezasemblacja DAT i kodu C++.
**Gra, testy, VM-testy ani komendy comcheck nie były uruchamiane.** Żaden wynik poniżej nie jest odbiorem w grze.

## Wynik w skrócie

- Odświeżony audyt jest **identyczny z migawką z 24.10** (te same statusy adresów: 29 / 40 / 2). Surowe wyniki:
  `build-touch/archolos-audit-2026-10-26/` (lokalnie, ignorowane przez Git).
- Przejrzałem pod kątem użycia w DAT **wszystkie 29 adresów bez handlera i wszystkie 40 hooków bez mostu**. Żaden z nich
  nie jest nieobsłużonym skutkiem na drodze fabuły: są martwe w DAT (zero wywołań), mają równoważny kod w silniku,
  albo dotyczą wyłącznie ustawień/logów/telemetrii. Jedyny z widocznym skutkiem dla gracza (nazwa świata w nagłówku zapisu)
  został zaimplementowany w tej turze — patrz niżej.
- Podejrzenie „`MEM_SearchVobByName` zna tylko movery” (582 miejsca w 240 funkcjach DAT) **nie jest blockerem**:
  465 z 476 stałych nazw w skryptach to w ZEN-ach zCMover, które warstwa odzwierciedla. Pozostałe 11 to jedna scena (QA305),
  używana tylko do odczytu `MOVERSTATE`.
- `MEMINT_StackPushVar` (TODO) nie jest już na żadnej ścieżce wykonania: wszystkich 6 jego wywołań ma nadpisania lub nie
  jest hookowanych.
- `WLD_ISRAINING` = false jest bezpieczne: wszystkie 5 miejsc DAT wymaga „nie pada” albo dodaje tylko opcjonalną kwestię.
- Realnie widoczny w logach „not implemented” to tylko `mdl_applyrandomani` / `mdl_applyrandomanifreq` (kosmetyczne).

## Co ustaliłem i jak to sprawdzić

### Adresy bez handlera (29)

| Adresy | Dowód z DAT / kodu | Ocena |
| --- | --- | --- |
| 0x5d3cf0 ZCRENDERER_DRAWTILE, 0x64d8e0 ZRND_D3D_DRAWLINE | `functions.csv`: zero wywołań funkcji opakowujących | martwe |
| 0x7a5e80…0x7abd10 (CATINV_DRAWCATEGORY), 0x709630, 0x70a150, 0x70ade0, 0x70b570, 0x7110a0, 0x711120, 0x762b40, 0x7813f0 | CATINV: wewnętrzne wywołania biblioteki kategorii ekwipunku; `CATINV_DRAWCATEGORY` ma 0 wywołań (jest tylko hookiem) | zastąpione natywnym UI ekwipunku (przegląd z 24.10, tu niepowtarzany) |
| 0x606ad0 ZCVISUAL_LOADVISUAL | jedyne użycie: `MISSINGVISUALFALLBACKHOOK`, a `INIT_MISSINGVISUALFALLBACK` jest **pusta** — hook nigdy nie jest instalowany | martwe |
| 0x6d0000 ZCVOB_SETCOLL | jedyny wywołujący to `_AI_TELEPORTALWAYS`, który ma adapter (`directmemory_events.cpp`) | pokryte |
| 0x6b6490 HideFocus…HOOK2 | równoważny kod: `worldobjects.cpp` (HookAdr::HideFocusParty, `AIVAR[15]` przy wyciągniętej broni) | pokryte |
| 0x7b4870/76, 0x7b4a5c/62/68/6e (GlobalAlloc/Lock, schowek) | wywołuje je wyłącznie `WRITETOCLIPBOARD`, która ma **0 wywołań** w DAT | martwe |

### Hooki bez mostu (40)

| Hook | Co robi skrypt | Ocena |
| --- | --- | --- |
| CATINV_* (27 adresów) | kategorie/strony/transfery ekwipunku | natywne UI (wymaga odbioru, pozycja 9 listy) |
| 0x699c60 PC_DIVING | zbieranie przedmiotu pod wodą | adapter istnieje wg handoffu (DoTakeVob pod wodą); tu niepowtarzane |
| 0x733837/0x733ea4/0x733eb7/0x69b947 HideFocus | party poza fokusem | pokryte (jw.) |
| 0x76e0f3 OCNPCSTATES_CHANGEROUTINE | tylko `MEM_FATAL` z komunikatem błędu zmiany rutyny | błąd/diagnostyka; adres w dekompilacji zgubiony |
| 0x660bb0 CRAFTINGVIEW_UPDATE / OCINFORMATIONMANAGER__UPDATE_HOOK | UI crafting i pole tekstowe odpowiedzi (imię psa/wina) | natywne UI (odbiór wyglądu craftingu potwierdzony; prompt czeka na próbę) |
| 0x68eba0 ZCVIEWDIALOGCHOICE__HANDLEEVENT | klawisze pola tekstowego (Esc/Enter/shift/capslock) | natywne UI jw. |
| 0x6cfdf8 `_CC_HOOK` | komendy konsoli LeGo | tylko konsola deweloperska |
| 0x4276b0, 0x42926a ApplySomeSettings | opcje ini: tryb G1 dla CATINV, skalowanie widoczności, LOD, ambient vobs | grafika/opcje, bez wpływu na fabułę |
| 0x42a2d0 UPDATELOADINGTEXTURE | losowa tekstura ekranu ładowania (`LOA_01..26`) | **zaimplementowane 26.10** (niżej) |
| 0x7a6060 INVTRANSPARENCYHACK | alfa 64→255 w ekwipunku | kosmetyka |
| 0x4344a3 MARVINMODE, 0x44e056 ZERROR_SETTARGET | konsola/log | diagnostyka |
| 0x68b9d0, 0x68bba6 TELEMETRY_ONTRADE* | wywołują puste `TELEMETRY_PLAYERBUY` | brak skutku w samej modyfikacji |
| 0x437e55 PATCHSAVEGAMEVERSION | wersja zapisu w nagłówku silnika | nie dotyczy formatu OpenGothic |
| **0x437d9f PATCHSAVEGAMEWORLD** | nazwa obszaru (`GETWORLDNAME` wg muzyki strefy) w nagłówku zapisu | **zaimplementowane w tej turze** |

### Dynamiczne callbacki (39)

To uogólnione wrappery bibliotek (`MEM_Call*`, `FF_Apply*`, `LIST_For*`, `_HT_ForEach`, `Buff_*`, `EventPtr_Execute`,
`_CC_Hook`, `_PM_*`) oraz kilka konkretnych miejsc: `ALCHEMYBOOK_TIER1_LEARN_INFO`, `CUTSCENE_START`,
`CALLFUNCTIONCOMMAND`, `STARTFINALBOARDS`, `QUESTSEVENTSMANAGER_REGISTER`, `DIA_MORITZ_DELAYEDRTNCHANGE`,
`SPELL_CAST_SLOWDOWN`, `CALLAFTERWORLDLOADED`. Idą przez `MEM_CallByID`/`FF_ApplyOnceExtGT`, które używają
`invoke_function` (respektuje nadpisania). Nie znalazłem tu nowego braku; celów nie zgaduję (zero bezpośrednich
wywołań nie dowodzi nieużywania).

### MEM_SearchVobByName (582 miejsca)

Narzędzie: `python -I tools/ikarus/vobnames.py --mod <Source> --worlds <katalog z ARCHOLOS_*.ZEN>`
(statyczne; klasę vobu czyta z najbliższego nagłówka klasy w binarnym ZEN).

- stałe nazwy: 476, wszystkie występują w którymś z pięciu światów Kronik;
- z klasą `zCMover`: 465 (odzwierciedlone w `buildMoverPool`/`searchVobsByName`);
- bez movera: 11 — `QA305_OLAF_BARREL_01..10` (zCVob) i `QA305_COPRSE_HOBBO_01` (oCMobContainer), używane w
  `QA305_PREPARENPC_APPLY` / `QA305_HIDEEVENTS_APPLY` wyłącznie jako dodatkowe „MOVERn”; scena i tak steruje
  rzeczywistymi moverami `QA305_MOVER_*` przez `Wld_SendTrigger`, a `CHANGEVOBCOLLISION` pracuje na prawdziwych obiektach.
  Pusty wynik wyszukiwania daje `MOVERSTATE`=0, czyli „nie otwiera się/nie zamyka się” — bez zakleszczenia.
- wywołujący ze zmienną nazwą (`ENABLEMUSICZONE`, `TELEPORTHEROTOVOB`, `PARSERSYMBOLISVOB`): pierwszy ma obsługę
  natywną, pozostałe dwa mają 0 wywołań w DAT.

Ograniczenie: „movery wg ZEN” nie dowodzi, że każdy zostanie utworzony jako `MoveTrigger` w danym świecie ani że
`MOVERSTATE` zmienia się w czasie sceny. To wymaga odbioru (pozycja 14 listy: sceny QA305/KQ402/SQ509 i inne).

### Pozostałe sprawdzone podejrzenia

- **MEMINT_StackPushVar:** 6 wywołujących w DAT (`CALL_RetValAsZStringPtr`, `MEM_ReadInt/WriteInt`, `MEM_ReadString/
  WriteString`, `_AI_Function_Event`) — pierwsze pięć jest nadpisane, a `_AI_FUNCTION_EVENT` jest pomijany w `HookEngineI`.
- **Kod x86 (`ASM_Run`) w DAT:** poza nadpisanymi funkcjami (`MKF/ADDF/TRUNCF/…`) i `MEM_ReadByte_/WriteByte_` (0 wywołań,
  używane są nadpisane `MEM_ReadByte/WriteByte`) zostaje `SQRTF`, którego
  jedyny łańcuch (`DISTANCE2DF` ← `DISTANCE2D`) ma 0 wywołań. `Int64` (`ADD64`…) też nigdzie nie jest wywoływany.
- **WLD_ISRAINING:** `DIA_OLDMAN_GOODWATER_CONDITION` i `EVENTSMANAGER_NS06` wymagają `== FALSE`; `B_SAY_GUILDGREETINGS` dodaje
  `$WEATHER` tylko przy deszczu; silnik nie ma deszczu. `false` jest więc spójne.
- **KmLib / GameServices:** `GetProcAddress` zwraca zastępczy uchwyt 1, a `Cpu32::callFunction(1)` jawnie nic nie wywołuje.
  Osiągnięcia (`GAMESERVICES_UNLOCKACHIEVEMENT`, 52 miejsca) i statystyki są więc bezskutkowe, ale niczego w fabule nie
  blokują (to wywołania bez wartości zwrotnej wpływającej na quest; `PC_SCOPE` to tylko profiler `ChromeTraceEvent`).
  To nadal luka funkcjonalna, nie awaria; nie przedstawiać licznika lokalnego jako Steam.
- **Ostrzeżenie `OCNPC.FOCUS_VOB` w `SPELL_TELEKINESIS_FOCUS_REMOVER`** z `log.txt`: znane i uznane za nieszkodliwe
  (archiwum 20.10); override istnieje w `directmemory_events.cpp`.
- **`crashes/1.log`** (Adreno 619, G1, 0.8.3): `not implemented call [mdl_applyrandomani]` + `[…freq]` to jedyne „not implemented”;
  reszta (`DmLoader`, składnia tekstów G1) nie dotyczy Kronik.

## Zmiana w kodzie: nazwa obszaru w nagłówku zapisu (PATCHSAVEGAMEWORLD)

- `hookaddresses.h`: `HookAdr::SaveWorldName = 0x437D9F` (hook zapisuje zSTRING pod `[ebp+84]`).
- `DirectMemory::saveWorldName(fallback)` (`directmemory_files.cpp`): wołane raz przy zapisie; tymczasowa ramka z zerowym
  zSTRING pod `ebp+84`, `fireHook`, odczyt wyniku; pusty wynik zostawia nazwę pliku świata (skrypt CoM przy pustej nazwie
  i tak zapisuje `""`). Bez hooka nic się nie zmienia. Adres dopisany do `isHookAnswered`.
- `GameScript::dmaSaveWorldName`, użycie: `GameSession::save` (`hdr.world`). **Nie** w `SaveScreen::liveHeader` — jest
  wołane w `paint` co klatkę, a ekran zapisu ma własną nazwę miejsca (`SaveStore::placeName`).
- Zgodność: format nagłówka bez zmian (`world` to już napis), `Serialize::Current` bez zmian, stare zapisy działają;
  `hdr.world` służy tylko do wyświetlania i logu (sprawdzone `grep`em).
- **Do odbioru przez użytkownika:** zapis w Mieście/Silbach/Wilczym Legowisku/Klasztorze/Bagnach i w ENDGAME/SILVERMINE/
  VOLFZACKE/SEWERS — sprawdzić nazwę na liście slotów (stare menu: pole nazwy poziomu; ekran mobilny: „miejsce” zamienia
  nazwę obszaru na zapis z dużą pierwszą literą, więc wielowyrazowa nazwa może mieć małe litery od drugiego słowa).
  Wczytanie starego zapisu nadal pokazuje nazwę pliku świata.

## Co zostaje (kolejność wg handoffu)

1. Odbiór w grze: Q102/Fabio/Jorn i skip, crafting (koszty/ilości/„Wszystko”), dziennik, scenki, PermMem/FF po wielu loadach.
2. PermMem/FF trwałość — wymaga próby (statycznie nie da się wykazać kompletności obiektów biblioteki).
3. `MDL_ApplyRandomAni/Freq` (S_EAT → R_ROAM*) — zaimplementowane 26.10 (patrz niżej), czeka na odbiór.
4. KmLib GameServices/Initialize* — brak usług, bez wpływu na kampanię.
5. Pogoda (`Wld_IsRaining`) — silnik bez deszczu; zmiana wymaga spójnego systemu pogody.

Nie deklaruję zgodności kampanii: ten dokument zawęża listę podejrzeń, nie zastępuje odbioru.

## Zmiana w kodzie: MDL_ApplyRandomAni / MDL_ApplyRandomAniFreq

- Kontrakt: `Mdl_ApplyRandomAni(npc, base, random)` dopisuje animację losową do `base`, `Mdl_ApplyRandomAniFreq(npc, base, sekundy)`
  ustawia częstotliwość (domyślnie 1 s; CoM: `1090519040` = 8.0). Źródło: handoff z 26.10 i API ZenGin.
- Stan: `Npc::randomAni` (nazwy wielkimi literami, lista bez duplikatów, `freq`, aktualnie grana, czas następnej).
  `Pose`/solver bez zmian — `processLayers` nadal wraca szybko przy `hasTransitions==0`, więc losowanie jest w
  `Npc::tickRandomAni` (`npc.cpp`), wołane z `Npc::tick` (nie dla trupów i lalek co-op).
- Zegar liczy się tylko, gdy gra animacja bazowa (`S_EAT`); odstęp to losowo 0.5–1.5 × `freq`, wybór równomierny.
  Po przerwaniu `S_EAT` (np. walką) zegar się zeruje. Czas jest w zegarze świata, nie w `time.slw` modelu — przy
  spowolnieniu losowanie nie zwalnia, ale sama animacja tak (jest odtwarzana przez `Pose`).
- Zabezpieczenie: gra tylko animacje, których `next` to animacja bazowa (inaczej NPC zostałby w pozie, której nikt nie kończy);
  inne są usuwane z listy i raz logowane (`Log::d`). Brak nowych `Log::e`.
- Zmiana wizualu (`Npc::setVisual(Skeleton)`) czyści listę (rejestracje należą do modelu); skrypt rejestruje je ponownie
  przy wejściu w `ZS_MM_RTN_EATGROUND`.
- Zapis: opcjonalny wpis `worlds/<świat>/<katalog><id>/compat-random-ani` (wersja 1); stare zapisy bez niego wczytują się
  z pustą listą, `Serialize::Current` bez zmian. Czas następnej animacji nie jest zapisywany.
- **Do odbioru:** zwierzę z `ZS_MM_RTN_EATGROUND` (np. scavenger/chrząszcz) — co kilka–kilkanaście sekund powinno zagrać
  `R_ROAM1..3` i wrócić do jedzenia; przerwanie walką; zapis/wczytanie podczas jedzenia. Gdy animacje nie grają, w logu
  (poziom debug) powinna być linia „random animation … is not played”, co wskaże, że MDS nie wraca do `S_EAT`.

## Zmiana w kodzie: losowe ekrany ładowania (UPDATELOADINGTEXTURE)

- `HookAdr::LoadingTexture = 0x42A2D0`. Skrypt `UPDATELOADINGTEXTURE` wybiera `LOA_01..26.TGA` i zapisuje nazwę w tekście
  silnika pod 0x8B3D84; `DirectMemory::loadingTexture()` tworzy ten bufor (64 B) pod tym adresem, odpala hook i czyta napis.
- `Gothic::loadingBannerName(script, fallback)` zwraca wybrany obrazek tylko wtedy, gdy jest w paczkach (`LOA_nn-C.TEX` jest w
  `KM_Textures.mod`), inaczej dotychczasowy `LOADING.TGA` / `LOADING_<świat>.TGA`.
- Użycie: zmiana świata (`GameSession::tick`), wczytanie gry i nowa gra, gdy istnieje już sesja ze skryptami. Z menu głównego,
  bez sesji, nie ma jeszcze skryptów — wtedy bez zmiany (jak w oryginale, hook instaluje się dopiero w grze).
- Pierwsza wersja wybierała obrazek po zamknięciu starej gry (`setGameImpl(nullptr)`), więc zawsze wypadał `LOADING.TGA`;
  wybór jest teraz na początku `startGame`/`loadGame`.
- **Potwierdzone przez użytkownika 26.10:** losowe ekrany ładowania działają po wczytaniu zapisu z poziomu trwającej gry.
  Niepotwierdzone: zmiana świata. Nowa gra i wczytanie z menu głównego (bez sesji) nadal pokazują `LOADING.TGA`.
