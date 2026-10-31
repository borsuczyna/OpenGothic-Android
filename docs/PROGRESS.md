# Postęp: Kroniki Myrtany / Ikarus / LeGo

Przekazanie kontekstu 26.10.2026: [handoff dla Claude](C:/Users/Ernest/Desktop/opengothic/docs/ARCHOLOS_HANDOFF_CLAUDE_2026-10-26.md). Poniższa lista implementacji i odbioru pozostaje snapshotem z 24.10; przygotowanie handoffu nie podnosi postępu.

Aktualizacja 26.10.2026 (przegląd statyczny, bez uruchamiania gry): licznik 24/27 bez zmian — to nadal liczba pozycji
listy, nie procent zgodności. Nowe: [ARCHOLOS_BLOCKERS_2026-10-26.md](ARCHOLOS_BLOCKERS_2026-10-26.md), narzędzie
`tools/ikarus/vobnames.py`, hook nazwy obszaru w nagłówku zapisu (do odbioru: nazwy w slotach zapisu w różnych strefach).

Stan bieżący: 24.10.2026. Poprzedni raport zachowany w [PROGRESS_2026-10-20_ARCHIVE.md](PROGRESS_2026-10-20_ARCHIVE.md).

# Kroniki: bieżąca lista prac

Postęp: **24/27 pozycji**. Licznik dotyczy tej listy, nie procentu zgodności gry.
`[x]` oznacza implementację w kodzie. Użytkownik zezwolił na okresowe kompilacje
przed STOP; nowy pakiet nadal wymaga ręcznych prób. Agent nie uruchamia gry,
testów ani komend comcheck. Użytkownik potwierdził muzykę menu i poprawny wygląd
panelu craftingu po pakiecie naprawczym.

- [x] Muzyka menu Kronik; użytkownik potwierdził wyłącznie właściwy motyw.
- [x] OGG i mapowanie muzyki stref świata.
- [x] Priorytet plików moda w VDF/MOD.
- [x] Kategorie ukończonych i niezaliczonych zadań dziennika.
- [x] Zatrzymanie głosu pominiętej wypowiedzi trialogu.
- [x] STOPALLSOUNDS dla efektów świata, globalnych i głosu DialogMenu.
- [x] WLD_INSERTOBJECT dla obsługiwanych wizualizacji, z zapisem obiektów.
- [x] Equip/Unequip bez zdejmowania przy ponownym wyposażeniu.
- [x] NpcEquipBestWeaponByType i most hooka wyposażania NPC Kronik.
- [x] GetSlotItem rozróżnia dłonie, schowaną broń i przedmioty animacji.
- [x] DropFromSlot dla WEAPON_STATCHECK.
- [x] APPLYHOUSEWALLTEXTURE: rzeczywiste materiały domu i powiązania GPU.
- [x] ENABLELOADGAME: blokada ładowania, w tym quick-load.
- [x] ENABLELOADSAVEGAME: blokada obu operacji.
- [x] MEM_WriteString i pożyczany wskaźnik tekstu.
- [x] Wywołania Ikarusa respektują nadpisania VM.
- [x] Zabezpieczenia pamięci i uchwytów oraz sprzątanie slotów przedmiotów.
- [x] Analizator źródeł/DAT, licznik miejsc wywołania, raport atrap/TODO.
- [x] oCNpc::IsSpellActive, potrzebne dla światła w Q501.
- [x] Ścieżki RemoveFromSlot używane przez pochodnię i chronione ogłuszenie.
- [x] Przegląd CATINV i natywna adaptacja masek/stanu kategorii ekwipunku.
- [x] Sprite'y map LeGo, Render modeli i wybór podglądu receptury w craftingu.
- [x] Synchronizacja TRIA_FINISH przed zmianą rutyn/zamknięciem dialogu Q102.
- [ ] Pozostałe hooki Kronik: kontrakty zdarzeń, rejestrów i stosu. 26.10: wszystkie 40 hooków i 29 adresów przejrzane
  statycznie w [ARCHOLOS_BLOCKERS_2026-10-26.md](ARCHOLOS_BLOCKERS_2026-10-26.md) — brak nieobsłużonego skutku fabularnego;
  zaimplementowano PATCHSAVEGAMEWORLD (nazwa obszaru w nagłówku zapisu). Pozycja zostaje otwarta do odbioru w grze.
- [ ] Pozostałe wywołania natywne/dynamiczne: implementacje i działające zamienniki. 26.10: 39 callbacków dynamicznych to
  wrappery bibliotek; `MEM_SearchVobByName` pokryte (465/476 nazw to movery); RandomAni w kodzie (do odbioru); otwarte zostają KmLib i PermMem.
- [x] Kompilacje pakietów i poprawki kompilatora; oba EXE zlinkowane.
- [ ] Ręczne próby zmian i weryfikacja kampanii.

## Nowe komendy, wyłącznie do uruchomienia przez użytkownika

Na zapisie roboczym. Źródło: `mods/devtools/lib/archoloschecks.js`; komendy są kopiowane
do EXE/kopii moda podczas kompilacji; ostatnio dodano cutscene.

- `comcheck bestweapon` — warunki używania i idempotencja wyposażania.
- `comcheck housewall 0..11|status|restore` — wzory ścian, w tym 11 świąteczny;
  wewnątrz domu sprawdzić też zapis/wczytanie wybranego wzoru.
- `comcheck scenelock load|save|both|off` — po próbie zawsze `off`.
- `comcheck cutscene` — tylko odczyt flag. W naturalnej scence sprawdzić blokadę
  menu również po dwóch minutach, brak HUD/fokusu, dolne pole kwestii NPC oraz
  nazwę/kolor przy planszach końcowych. Po scenie zwykłe zachowanie wraca.
- `comcheck spellactive [SPL_LIGHT]` — przed rzuceniem, w trakcie i po wygaśnięciu;
  sprawdzić naturalny warunek światła w ciemnym pokoju Q501.
- `comcheck torch status|stow` — płonący model, zwrot jednej zgaszonej pochodni,
  normalny klawisz oraz zapis/wczytanie.
- `comcheck category 0..8|status` — maski przedmiotów, zachowanie kategorii własnej,
  reset przy otwarciu handlu/skrzyni/przeszukiwania, przenoszenie ostatniej sztuki.
- `comcheck map archolos|silbach|city|sewers|off` — tekstura, kwadratowe proporcje,
  położenie i obrót kursora; dodatkowo naturalne użycie przedmiotu i mapy questowe.
- `comcheck preview [przedmiot]|off` — 3D przez Render_AddItemPrio; zamknąć przed
  zapisem/wczytaniem lub zmianą świata. Sprawdzić też naturalne okna craftingu.
- `comcheck crafting` — tylko odczyt stanu. Przy naturalnym stanowisku sprawdzić
  kilka receptur, ilości i „Wszystko”, składniki, wymagane/posiadane liczby,
  zakładkę właściwości, wyjście i ponowne wejście. Następnie normalne wytworzenie.
  Nowy pakiet: niezerowa skala panelu/modelu, DLG_CHOICE.TGA, kolory i odstępy
  tekstu, dynamiczny komparator listy LeGo. Skompilowany; użytkownik potwierdził
  wygląd. Pełne wytwarzanie, koszty, zakładki i resize nadal do odbioru.
- Pod wodą: naturalnie podnieść przedmiot. Ma zniknąć ze świata, trafić raz do
  ekwipunku i użyć wodnej animacji; po niej dalsze nurkowanie. Porównać z lądem.
- Spowolnienie: naturalne rzucenie ze zwoju i runy, właściwy koszt many, efekt,
  wygaśnięcie po 60 s i ponowne rzucenie; osobno scena Otisa w Q306. Sprawdzić
  wspólne tempo animacji/ruchu/ciosów oraz powrót do normy. Zapisać w trakcie,
  ponownie uruchomić proces, wczytać i zaczekać na pozostały czas efektu.
- LeGo/zapis: zapisać osobno z zapaloną i schowaną pochodnią, zamknąć proces
  i wczytać oba zapisy; model oraz liczba bez duplikatów. Sprawdzić również
  trwałość obiektów PermMem i FrameFunctions po ponownym uruchomieniu.
- Eliksir nurkowania: naturalnie wypić, nurkować dłużej niż zwykły zapas tlenu,
  sprawdzić pasek, brak obrażeń podczas buffa, wygaśnięcie i ponowne nurkowanie.
  Powtórzyć z zapisem/wczytaniem pod wodą; bez buffa zwykłe duszenie pozostaje.
- `comcheck breath [NPC_INSTANCE]` i `comcheck slowdown [NPC_INSTANCE]` — odczyt
  limitu/pozostałego powietrza, HP i żądanej skali modelu. Domyślnie bohater;
  przy Slowdown wskazać trafionego NPC, a w scenie Otisa obserwować bohatera.
- `comcheck attitude NPC_INSTANCE` — odczyt nastawienia bieżącego i stałego.
  Porównać przed, podczas i po naturalnej walce Q205/Q306/Q602, także po loadzie.
- `comcheck telekinesis` — odczyt fazy i kroków ruchu. Naturalnie przyciągnąć
  luźny przedmiot: koszt many, ruch, ukończenie i opadanie. Przerwać rzucanie,
  zmienić broń, zapisać i wczytać; przedmiot nie może pozostać bez fizyki.
- Zbroje NPC: QA306 Bloodwyn bez zbroi; zmiany munduru w Q102/rozdziałach,
  wielokrotne zastosowanie tego samego modelu i odtworzenie po loadzie.
  Zmiana twarzy NPC nie może zdejmować jego zbroi.
- Dźwięki ZEN: rozmowy tłumu na statku Q101 mają cichnąć po rozmowie z Jornem,
  przestraszony tłum po finale sceny; piła SQ125 po zakończeniu pracy. Zapisać
  po wyłączeniu, wczytać/zmienić świat — wyłączony kanał nie wraca sam.
- Kolejkowane teleporty: naturalne limity odległości Jorna/Ruperta w Q102,
  przygotowanie postaci w Q602 i manekiny domu; brak skoku do środka świata.
- `comcheck musiczone` — tylko odczyt strefy/świata i flag koncertu. Wejście do nakładających się stref, wyjście, dzień/noc, koncert
  In Extremo 18–21, zapis podczas aktywnego koncertu i powrót po zmianie świata.
- Klawiatura Ikarusa: mapę LeGo zamknąć Escape, Enter, PPM i przypisanym
  klawiszem mapy/ekwipunku. Przy zaklęciu zamka sprawdzić puszczenie akcji;
  po Alt+Tab klawisze nie pozostają wciśnięte dla skryptów. Sprawdzić skrót
  pochodni po zmianie przypisania, także drugie przypisanie, bez powtarzania
  przełączenia od trzymania klawisza.
- Obrażenia: ten sam czar z pierścieniem maga i bez, przeciwnicy z ochroną oraz
  odpornością na magię; broń penetrująca nie może trwale odejmować pancerza.
  Oddzielnie strzały i magiczna broń w walkach, które mają kończyć się ogłuszeniem.
- Drużyna: naturalna walka z towarzyszem, zmiana celu po pokonaniu przeciwnika
  i przeciwnik za przeszkodą; towarzysze nie wybierają siebie nawzajem.
- Otwieranie zamków: zwojem wybrać zamkniętą skrzynię lub drzwi; otwarty obiekt,
  ławka i ognisko nie mogą zasłaniać właściwego celu. Kluczowy zamek bez kodu ma
  odmówić otwarcia. Przerwać inwestowanie many, zapisać/wczytać i dokończyć ten
  sam zamek; odgadnięte znaki pozostają, zwój znika dopiero po powodzeniu.
- `comcheck trialog` — tylko odczyt. Q102: naturalnie wybrać „Idę z Jornem” oraz
  w oddzielnym przebiegu „Idę z Fabio”, normalnie i z pomijaniem kwestii. Po końcu
  TRIA_RUNNING=0, TRIA_CPTR=0, bez błędu ZS_TALK_END; kamera, rutyny, dziennik
  i możliwość następnej rozmowy mają wrócić do poprawnego stanu.

## Ograniczenia najnowszego pakietu

- Zmiana ścian jest adaptacją konkretnej funkcji CoM. Nie oznacza pełnej emulacji
  zCResource CacheIn/CacheOut/zmiany nazwy dla dowolnego moda.
- Sprite'y obsługują pozycję, obrót, UV, priorytet i jednolity kolor, jak mapy CoM.
  Gradienty kolorów wierzchołków i pozostałe blend modes są jawnie zgłaszane.
- Render modeli korzysta z InventoryRenderer. Nie emuluje dowolnego świata
  renderowania Gothica, dowolnej rotacji przekazanej do oCItem::Render ani całej
  semantyki nakładania wielu warstw 2D/3D.
- Zapis odtwarza konkretnie SPL_LIGHT i aktywne time.slw; nie jest to ogólna
  serializacja wszystkich efektów, faz cząstek i dźwięków.
- Nie potwierdzono pełnego craftingu, wszystkich questów ani braku critical error.
- Buildy: archolos-compat/time/light/diagnostics/attitude/memory/timers/screenfx/
  telekinesis-build z datą 2026-10-24 zakończyły się kodem 0. Pakiet klawiatury
  i archolos-input-cutscenes-build również skompilowały oba EXE. Wygląd craftingu
  potwierdził użytkownik; brak odbioru kolejnych zmian.
- `CRAFTINGVIEW_UPDATE` korzysta z natywnego wyboru menu. Nie oznacza to pełnej
  emulacji prywatnego InformationManagera ani wszystkich hooków pod 0x660bb0.
- Drugi parametr time.slw ma już kod zegara modelu i zapisu. Odbiór
  naturalnego zaklęcia i sceny Q306 pozostają warunkiem uznania działania w grze.

- Pakiet voices oraz torchbindings z 2026-10-24 skompilował oba EXE kodem 0.

- Strefy muzyki: oba EXE skompilowane; najnowsze ręczne komendy nadal bez odbioru.

## STOP użytkownika — stan końcowy tej sesji

Nowe prace zatrzymane na polecenie użytkownika. Postęp nadal 24/27 grup.
Muzyka menu oraz wygląd craftingu potwierdzone przez użytkownika. Pozostały
odbiór z listy powyżej nie jest zaliczony na podstawie samej kompilacji.
Pakiet dźwięków ZEN i armor/spelllevel skompilował oba EXE kodem 0.
Nie wykonano prób gry, testów automatycznych ani komend comcheck przez agenta.

---

# Kroniki: szczegółowy pozostały zakres — 24.10.2026

Postęp: **24/27 grup**, nie procent zgodności gry. To pełny znany backlog z kodu,
aktualnego DAT i wcześniejszych raportów; nie dowód wykrycia każdego błędu kampanii.
Agent nie uruchamia gry, testów ani komend comcheck. Użytkownik zezwolił na okresowe
kompilacje przed STOP. Nowy pakiet wymaga odbioru w grze. Muzyka menu i wygląd
panelu craftingu mają osobne potwierdzenie użytkownika.

## Priorytet: fabuła, awarie i zapis

- [x] Kod synchronizacji TRIA_FINISH i blokady przedwczesnego zamknięcia pustego menu Q102.
- [x] Kod DIACAM_UPDATE/StartDialogCam i odtworzenia kadru po kamerze scenki.
- [ ] Odbiór Q102: Jorn/Fabio, pomijanie, kolejna rozmowa, rutyny i dziennik.
- [x] Most OCNPC__GETNEXTENEMY_REPLACEHOOK: drużyna i widoczność, także bieżący cel.
- [x] Descriptor: wejściowe ARYDAMAGE, bonus pierścienia przed ochroną zamiast
  dodatkowej utraty HP z FDAMAGETOTAL. Dane zaklęcia i ponowny odczyt dontKill.
- [ ] Odbiór wyboru przeciwnika oraz obrażeń: pancerz/penetracja/pierścienie,
  strzały, magia, upadek i walki bez zabijania.
- [x] Nastawienie tymczasowe NPC działa w odczycie i decyzjach AI; permanentny
  odczyt pozostaje oddzielny. Odbiór Q205/Q306/Q602 i uspokojenia po walce.
- [ ] Kolejki AI przy długich scenach, anulowaniu, zniknięciu NPC i zmianie świata;
  limity awaryjne nie są dowodem prawidłowego wykonania sceny.
- [x] Kod fazy przed zapisem: _BW_SAVEGAME przed snapshotem świata/Daedalusa,
  TORCHLOADFIX_CHECKTORCH i końcowy zapis gotowych plików wirtualnych LeGo.
- [ ] PermMem/FF/destruktory, callbacki, napisy, widoki, referencje do NPC/przedmiotów.
- [ ] Restart procesu, wiele loadów, sloty/quicksave, zmiana świata i powrót.
- [ ] Aktywne efekty czasowe po zapisie/wczytaniu.
- [x] Stabilne referencje napisów memory_instance, walidacja pustych symboli
  i NPC teleportacji; timer świata synchronizowany przed callbackami FF.
- [x] ScreenFX wybiera klatkę modulo liczby tekstur, bez dostępu poza tablicę.
- [x] Zabezpieczenia SB_TOSTRING przed pustą strukturą i nieprawidłowym buforem;
  STR_FROMCHAR sprawdza granicę przed odczytem znaku.
- [ ] Dalszy przegląd dereferencji i synchronizacji surowych zapisów z silnikiem.

## Zaklęcia, zamki i postacie

- [x] MAG_BOOK/SetOwner dla oryginalnego SPELL_CAST_SLOWDOWN.
- [x] Zegar modelu time.slw dla animacji, root motion, zdarzeń i okien walki;
  weak owner efektu, przywrócenie skali, zapis zegara i aktywnych time.slw.
- [ ] Odbiór spowolnienia: koniec, ponowne rzucenie, load i Otis Q306, także
  zmiana świata/wyjście postaci; zwykły czar 60 s, Otis 2 s, efekt narkotyku 30 s.
- [x] Pozostały czas SPL_LIGHT zapisuje się osobno i odtwarza bez ponownego rzutu.
- [ ] IsSpellActive/światło Q501: przed, w trakcie, po wygaśnięciu i po loadzie;
  nadal brak ogólnej serializacji wszystkich efektów i faz pojedynczych cząstek.
- [x] Most SPELL_PICKLOCK_FOCUS i filtr TARGET_TYPE_MOB zgodny z callbackiem.
- [x] Postęp otwierania należy do skrzyni/drzwi, z opcjonalnym compat-lock w zapisie;
  obrót 256 tymczasowych slotów nie zeruje już odgadniętych znaków.
- [ ] Odbiór zaklęcia zamka, ClearKeyBuffer, wytrychy i magia: talent/bonus,
  klucze, kod, zapis kodu, cel i poprawne zużycie zwoju.
- [x] Kod wodnego podnoszenia i DoTakeVob dla przedmiotów.
- [x] BUFF_DIVE10_TICK: limit i pozostały tlen 30000/31000 ms, faktyczne obrażenia,
  HUD, regeneracja poza wodą i opcjonalny wpis zapisu compat-breath.
- [ ] Odbiór podnoszenia i buffa pod wodą, wygaśnięcia i zapisu/wczytania.
- [x] BODY_VISUALNAME/HEAD_VISUALNAME oraz osobne BITFIELD z numerami tekstur dla
  NPC_CHANGEFACE i B_REFRESHATINSERT; TELEPORTHEROTOPOS zmienia realną pozycję.
- [x] SPELL_LOGIC_TELEKINESIS, NEW_TELEKINESIS, SPELL_CAST_TELEKINESIS oraz
  SPELL_TELEKINESIS_FOCUS_REMOVER: rzeczywisty przedmiot, mana, ruch i fizyka.
- [ ] Odbiór telekinezy: przedmiot, koszt, dokończenie/anulowanie, zapis i load.
- [x] Kody klawiszy Ikarusa: poprawna konwersja z ustawień INI, cyfry/F1–F15,
  mysz, puszczenie klawiszy przy utracie fokusu i ClearKeyBuffer.
- [x] MEM_SetKeys zapisuje oba przypisania i odświeża sterowanie; INIT_TORCHKEY
  może ustawić T/przecinek przy nieistniejącej opcji keyTorch.
- [x] MEM_GetKey/SecondaryKey: aktualne dwa przypisania; logiczne GetState float
  i IsBindedToggled dla wymaganej akcji broni, BindKeys i __ftol int64.
- [ ] Odbiór map LeGo (Escape/Enter/PPM), zaklęcia zamka i skrótów CoM.

## Pozostała semantyka bibliotek

- [x] QSORT_G2: domyślny komparator unsigned pierwszego pola i całe rekordy,
  w tym osiem bajtów tabeli offset/symbol. Nieznany komparator zgłasza brak obsługi.
- [ ] Używane niestandardowe komparatory: udowodnić cele i zaimplementować potrzebne.
- [ ] Finalizacja/destruktory LeGo podczas delete/clear/load, bez mylenia z FINAL.
  FINAL jest optymalizatorem bytecode i ma zero bezpośrednich wywołań w tym DAT.
- [ ] Dynamiczne adresy/callbacki: ustalić konkretne cele, argumenty, wyniki i kontekst.
- [x] MEM_Call, MEM_GetFuncID i MEM_PushIntParam/StringParam/InstParam: zachowanie
  argumentów/wyniku oraz rozwiązywanie zmiennych func; invoke_function bez
  dodatkowej ramki dla naked overrides. Dotyczy komparatorów list LeGo.
- [x] MEM_CallByString zachowuje stos i wynik; ReadByte/WriteByte działają na
  jednym bajcie, CompareBytes waliduje zakresy; ujemny copybytes nie kopiuje.
- [x] STOPALLSOUNDS obejmuje również osobny kanał głosu DialogMenu.
- [ ] Faktycznie potrzebne instrukcje x86 i kontrakty flag/rejestrów/stosu.
- [ ] Alias VM kontra surowa pamięć: poprawny layout, odczyt i zapis do realnego obiektu.
- [x] SetPositionWorld sprawdza cały bufor vec3, a zapisy pozycji odrzucają NaN/Inf.
- [x] Usuwanie starego przedmiotu nie zeruje symbolu przepiętego na nowy obiekt;
  brak symbolu dla przejściowej instancji nie powoduje dereferencji nullptr.
- [x] CHANGEVOBCOLLISION zmienia również fizykę drzwi/obiektów interaktywnych;
  odtworzona fizyka zachowuje właściciela obiektu. Odbiór przejść fabularnych.
- [x] OCNPC.BITFIELD oddzielone od C_NPC.MISSION; zgodne maski tekstur.
- [ ] Pozostałe flagi OCNPC.BITFIELD: skutki zapisów dla realnego obiektu i trwałość.
- [ ] WLD_ISRAINING: wciąż false; 5 miejsc DAT, powitania/zaklęcia/staruszek NS06.
  Wymaga spójnego systemu pogody, którego renderer obecnie nie posiada.
- [x] MDL_APPLYRANDOMANI/FREQ (26.10): w kodzie, 3+1 miejsca ZS_MM_RTN_EATGROUND. Zegar per NPC w `Npc::tickRandomAni`,
  gra tylko animacje, których `next` wraca do bazowej (S_EAT), zapis w `compat-random-ani`. Do odbioru w grze.
- [x] Zweryfikowano istniejący adapter _AI_TeleportAlways: realny NPC/WP
  w callbacku LeGo; 95 miejsc AI_TELEPORTALWAYS w 53 funkcjach DAT.
- [ ] Potrzebne ścieżki waynetu i list vobów; nie pełna emulacja silnika na zapas.
- [ ] KmLib InitializeMenu/Gamestart/Always i eksporty usług gry; LoadLibrary/
  GetProcAddress są zastępcze. GetStat/IncrementStat/UnlockAchievement oraz trwałość.

## Natywny interfejs, prezentacja i odbiór

- [ ] CATINV: kategorie/strony, stosy/ostatnia sztuka, kontenery, zwłoki, handel,
  blokady i decyzje skryptowe. Wykazać pokrycie natywnym UI dla każdego starego hooka.
- [ ] Crafting: warunki, grupy składników, wynik, liczby, koszty, brak zasobów,
  ilości/Wszystko, zakładki, anulowanie i normalne wykonanie.
- [x] Panel craftingu: skala HPBAR, tekstura i rozmiary modelu, kolory i zgodny
  pomiar tekstu, odświeżanie po resize i sprzątanie receptury po zmianie zakładki.
- [x] Użytkownik potwierdził poprawny wygląd panelu względem drugiego zrzutu.
- [ ] Odbiór kosztów i ilości przy naturalnym wytwarzaniu oraz zmiany rozmiaru okna.
- [ ] InformationManager: imię psa/nazwa wina, polskie znaki, kasowanie/Escape/powrót;
  pełny prywatny widget Gothica nie jest emulowany.
- [ ] Scenki: menu/HUD/sterowanie/napisy podczas kamery i plansz końcowych.
- [x] Mosty CutsceneMenu 0x42abe7, CutsceneStatus 0x6c31d6 i DialogMessage
  0x7a9240: blokada menu, ukrywanie HUD, nazwa, kolor i docelowe pole napisu.
- [ ] Odbiór scenek dłuższych niż dwie minuty, napisów kamery i plansz końcowych.
- [ ] Sprite/Render: gradienty, dodatkowe blend modes, zagnieżdżone widoki,
  animacje okien, kolor/alpha tekstu, rotacja modeli i kolejność warstw 2D/3D.
- [x] ENABLEMUSICZONE: realne enabled, priorytety/ellipsoidy ZEN i opcjonalny
  zapis stanu stref na świat; brak strefy wycisza tylko tło, z zachowaniem override.
- [x] Trigger/Untrigger dźwięków ZEN: zatrzymanie kanału oraz opcjonalny zapis
  aktywności i pozostałego opóźnienia. Q101 Smalltalk/Scared i SQ125 piła.
- [ ] Muzyka świata: strefy, dzień/noc/walka, nadpisania, powrót po scenie i loadzie.
- [ ] Dziennik: naturalne ukończenie zadań, w tym W gorącej wodzie kąpany, i load.
- [x] NPC_CHANGEARMOR: końcowe AIVAR[48], NO_ARMOR=-1, idempotentny equip;
  MDL_SetVisualBody również nie przełącza już założonej zbroi. 0 zachowuje zbroję.
- [ ] Wyposażenie: warunki, idempotencja, kołczany, pochodnie, ręce, transfer/ostatnia sztuka.
- [ ] Dom: wzory ścian i odtworzenie po loadzie.
- [ ] Ustawienia zasięgu/skalowania, alternatywne loadingi i metadane zapisów. 26.10: nazwa obszaru w nagłówku zapisu
  (PATCHSAVEGAMEWORLD) i losowe ekrany ładowania `LOA_*` w kodzie — do odbioru; wersja zapisu i skalowanie widoczności bez zmian.
- [ ] Diagnostyka/konsola/schowek/telemetria; niższy priorytet od fabuły i zapisu. 26.10: schowek i `WriteToClipboard`
  mają 0 wywołań w DAT, telemetria to puste funkcje, `_CC_Hook` to tylko konsola.
- [x] Okresowe buildy i poprawki kompilatora; oba EXE zlinkowane, logi zachowane.
- [x] Po STOP końcowy build aktualnego stanu kodem 0 i lista odbioru.
  Nowe prace zatrzymane na polecenie użytkownika; kampania nadal niezweryfikowana.
- [ ] Odbiór scen Q204/Q205/Q304/Q306/Q406/Q501/Q602/GQ001, przedmiotów
  KQ402/SQ202 oraz efektów SQ509.
- [ ] Przejście rozdziałów, dróg gildyjnych i zakończeń.

## Wszystkie rozpoznane pozostałe adresy

Poniższe tabele opisują brak ogólnego handlera/mostu albo atrapę. Zamienniki natywne
mogą już pokrywać część zachowania: crafting, wejście tekstowe, nurkowanie, focus,
CATINV i HUD. Wymagają przeglądu semantyki. Liczby są statyczne, nie pomiarami gry.

### Adresy bez handlera: 29

| Adres | Wywołujący/rejestrujący | Callback | Miejsca wywołań wrappera |
| --- | --- | --- | --- |
| 0x5d3cf0 | ZCRENDERER_DRAWTILE |  | 0 |
| 0x606ad0 | ZCVISUAL_LOADVISUAL |  | 1 |
| 0x64d8e0 | ZRND_D3D_DRAWLINE |  | 0 |
| 0x6b6490 | HIDEFOCUSFORPARTYMEMBERS_UPDATESTATUSREPLACEHOOK2 |  | 0 |
| 0x6d0000 | ZCVOB_SETCOLL |  | 2 |
| 0x709630 | CATINV_DELETELISTSORTFROMPOOL |  | 4 |
| 0x70a150 | CATINV_SWITCHCONTAINER |  | 2 |
| 0x70ade0 | CATINV_RESET |  | 2 |
| 0x70b570 | CATINV_RESET |  | 2 |
| 0x7110a0 | CATINV_CONTAINERINSERT;CATINV_NPCINVINSERT |  | 0 |
| 0x711120 | CATINV_RIGHT;CATINV_SETMAXOFFSET |  | 1 |
| 0x762b40 | CATINV_PREVENTCLOSEDEADINV |  | 0 |
| 0x7813f0 | CATINV_CONTAINERREMOVE;CATINV_NPCINVREMOVE |  | 2 |
| 0x7a5e80 | CATINV_DRAWCATEGORY |  | 0 |
| 0x7a6060 | CATINV_DRAWCATEGORY |  | 0 |
| 0x7a6070 | CATINV_DRAWCATEGORY |  | 0 |
| 0x7a63c0 | CATINV_DRAWCATEGORY |  | 0 |
| 0x7a75b0 | CATINV_DRAWCATEGORY |  | 0 |
| 0x7a9a10 | CATINV_DRAWCATEGORY |  | 0 |
| 0x7aa030 | CATINV_DRAWCATEGORY |  | 0 |
| 0x7aa140 | CATINV_DRAWCATEGORY |  | 0 |
| 0x7abad0 | CATINV_DRAWCATEGORY |  | 0 |
| 0x7abd10 | CATINV_DRAWCATEGORY |  | 0 |
| 0x7b4870 | GLOBALLOCK |  | 2 |
| 0x7b4876 | GLOBALALLOC |  | 1 |
| 0x7b4a5c | CLOSECLIPBOARD |  | 2 |
| 0x7b4a62 | SETCLIPBOARDDATA |  | 1 |
| 0x7b4a68 | EMPTYCLIPBOARD |  | 1 |
| 0x7b4a6e | OPENCLIPBOARD |  | 2 |

### Hooki bez ogólnego mostu: 40

| Adres | Wywołujący/rejestrujący | Callback | Miejsca wywołań wrappera |
| --- | --- | --- | --- |
| 0x4276b0 | CATINV_INIT | APPLYSOMESETTINGS_HOOK | 1 |
| 0x42926a | INIT_SIGHTSCALING | APPLYSOMESETTINGSEND_HOOK | 1 |
| 0x42a2d0 | INITALTERNATIVELOADINGSCREENS | UPDATELOADINGTEXTURE | 1 |
| 0x4344a3 | INIT_FEATURES_GAMESTART | HANDLERESULTSTRING_MARVINMODE_REPLACEHOOK | 1 |
| 0x437d9f | PATCHSAVEGAMEWORLD | PATCHSAVEGAMEWORLD_HOOK | 1 |
| 0x437e55 | PATCHSAVEGAMEVERSION | PATCHSAVEGAMEVERSION_HOOK | 1 |
| 0x44e056 | STARTLOGFILE | ZERROR_SETTARGET_HOOK | 3 |
| 0x660bb0 | INIT_CRAFTINGVIEW_GAMESTART;INIT_FEATURES_GAMESTART | CRAFTINGVIEW_UPDATE;OCINFORMATIONMANAGER__UPDATE_HOOK | 2 |
| 0x68b6ce | CATINV_INIT | CATINV_HANDLEEVENTEBX | 1 |
| 0x68b9d0 | INIT_ITEMIMPORTANTFLAG | TELEMETRY_ONTRADELEFTHOOK | 1 |
| 0x68bba6 | INIT_ITEMIMPORTANTFLAG | TELEMETRY_ONTRADERIGHTHOOK | 1 |
| 0x68eba0 | INIT_FEATURES_GAMESTART | ZCVIEWDIALOGCHOICE__HANDLEEVENT_HOOK | 1 |
| 0x699c60 | INIT_GATHERINGITEMSUNDERWATER | OCAIHUMAN__PC_DIVING_HOOK | 1 |
| 0x69b947 | INIT_HIDEFOCUSFORPARTYMEMBERS | HIDEFOCUSFORPARTYMEMBERS_UPDATESTATUSREPLACEHOOK2 | 1 |
| 0x69d415 | CATINV_INIT | CATINV_DELAYMOBCAMERA | 1 |
| 0x6cfdf8 | LEGO_INITGAMESTART | _CC_HOOK | 1 |
| 0x706e03 | CATINV_INIT | CATINV_DRAWCATEGORY | 1 |
| 0x7086d0 | CATINV_INIT | CATINV_OPEN | 1 |
| 0x708f4d | CATINV_INIT | CATINV_CLOSE | 1 |
| 0x7093d6 | CATINV_INIT | CATINV_CONTAINERINSERT | 1 |
| 0x709487 | CATINV_INIT | CATINV_CONTAINERREMOVE | 1 |
| 0x7096e9 | CATINV_INIT | CATINV_CLAMPCATEGORY | 1 |
| 0x709763 | CATINV_INIT | CATINV_RIGHT | 1 |
| 0x709b26 | CATINV_INIT | CATINV_LEFT | 1 |
| 0x70aa5f | CATINV_INIT | CATINV_HANDLEEVENTEDI | 1 |
| 0x70af44 | CATINV_INIT | CATINV_MANIPULATECREATELIST | 1 |
| 0x70b228 | CATINV_INIT | CATINV_HANDLEEVENTEDI | 1 |
| 0x70b6b5 | CATINV_INIT | CATINV_MANIPULATECREATELIST | 1 |
| 0x70b8ad | CATINV_INIT | CATINV_HANDLEEVENTEDI | 1 |
| 0x70b9bd | CATINV_INIT | CATINV_PREVENTCLOSEDEADINV | 1 |
| 0x70cc37 | CATINV_INIT | CATINV_NPCINVREMOVEITEM | 1 |
| 0x70cd22 | CATINV_INIT | CATINV_NPCINVREMOVEBYPTR | 1 |
| 0x70cfa8 | CATINV_INIT | CATINV_NPCINVREMOVE | 1 |
| 0x70f251 | CATINV_INIT | CATINV_HANDLEEVENTNPCINVENTORY | 1 |
| 0x733837 | INIT_HIDEFOCUSFORPARTYMEMBERS | HIDEFOCUSFORPARTYMEMBERS_UPDATESTATUSREPLACEHOOK3 | 1 |
| 0x733ea4 | INIT_HIDEFOCUSFORPARTYMEMBERS | HIDEFOCUSFORPARTYMEMBERS_UPDATESTATUSREPLACEHOOK | 1 |
| 0x733eb7 | INIT_HIDEFOCUSFORPARTYMEMBERS | HIDEFOCUSFORPARTYMEMBERS_UPDATESTATUSREPLACEHOOK | 1 |
| 0x762a18 | CATINV_INIT | CATINV_DEADRESETCATEGORY | 1 |
| 0x76e0f3 | INIT_OCNPCSTATES_CHANGEROUTINE_ERROR | OCNPCSTATES_CHANGEROUTINE_HOOK | 0 |
| 0x7a6060 | INIT_INVTRANSPARENCYHACK | INVTRANSPARENCYHACK_HOOK | 1 |

### Proste atrapy: 2

| Adres | Wywołujący/rejestrujący | Callback | Miejsca wywołań wrappera |
| --- | --- | --- | --- |
| 0x44df50 | STARTLOGFILE |  | 3 |
| 0x7375e0 | OCNPC__DROPUNCONSCIOUS_HOOK |  | 0 |

## Pola natywnych obiektów: 20 kandydatów do przeglądu

Brak jawnego powiązania nie jest samodzielnym dowodem awarii: kontekst może być
transient memory_instance albo cały wywołujący może mieć działający override.

| Pole | Odniesienia operandów w DAT | Wywołujący |
| --- | --- | --- |
| OCITEM._ZCVOB_TRAFOOBJTOWORLD | 11 | NEW_TELEKINESIS;SPELL_LOGIC_TELEKINESIS |
| OCITEM._ZCVOB_BBOX3D_MINS | 9 | NEW_TELEKINESIS;SPELL_LOGIC_TELEKINESIS |
| OCITEM._ZCVOB_BBOX3D_MAXS | 9 | NEW_TELEKINESIS;SPELL_LOGIC_TELEKINESIS |
| OCNPC._ZCVOB_BITFIELD | 6 | MAKEHEROGHOST;_TRIA_COPY |
| OCNPC._ZCVOB_TRAFOOBJTOWORLD | 6 | SPELL_LOGIC_TELEKINESIS;TELEPORTHEROTOPOS |
| OCITEM._ZCVOB_BITFIELD | 4 | SPELL_CAST_TELEKINESIS |
| OCNPC._ZCVOB_HOMEWORLD | 3 | MEM_INSERTVOB |
| OCNPC._ZCVOB_VISUALALPHA | 3 | MAKEHEROGHOST;_TRIA_COPY |
| OCNPC.ENEMY | 2 | MEMINT_OLDWRITEINT;MEMINT_PREPAREASSIGNMENTS |
| OCNPC.MODEL_SCALE | 2 | _TRIA_COPY |
| OCNPC.MDS_NAME | 2 | _TRIA_COPY |
| OCNPC.INSTANZ | 2 | TELEMETRY_ONTRADELEFTHOOK;TELEMETRY_ONTRADERIGHTHOOK |
| OCITEM.INSTANZ | 2 | TELEMETRY_ONTRADELEFTHOOK;TELEMETRY_ONTRADERIGHTHOOK |
| OCNPC.INVENTORY2_VTBL | 1 | CATINV_OPEN |
| OCNPC.INVENTORY2_INVENTORY_COMPARE | 1 | CATINV_NPCCONTAINERISEMPTY |
| OCNPC._VTBL | 1 | PARSERSYMBOLISNPC |
| OCNPC.INVENTORY2_OCITEMCONTAINER_CONTENTS | 1 | ITEMIMPORTANTFLAG_CHECKSELLINGITEMS |
| OCNPC.INVENTORY2_OCITEMCONTAINER_SELECTEDITEM | 1 | ITEMIMPORTANTFLAG_CHECKSELLINGITEMS |
| OCNPC.DIVETIME | 1 | BUFF_DIVE10_TICK |
| OCNPC.DIVECTR | 1 | BUFF_DIVE10_TICK |

## Wszystkie dynamiczne miejsca DAT: 41

| Wywołujący | Mechanizm | Adres bytecode |
| --- | --- | --- |
| CALL__THISCALL | CALL__STDCALL | 0x242d |
| CALL__FASTCALL | CALL__THISCALL | 0x2482 |
| MEM_SWAPBYTES | CALL__CDECL | 0x25d7 |
| MEM_CLEAR | CALL__CDECL | 0x26a3 |
| LOADLIBRARY | CALL__STDCALL | 0x2973 |
| GETPROCADDRESS | CALL__STDCALL | 0x2a13 |
| VIRTUALPROTECT | CALL__STDCALL | 0x2af4 |
| MEM_MESSAGEBOX | CALL__STDCALL | 0x2b81 |
| MEMINT_QSORT | CALL__CDECL | 0x32a0 |
| STR_FROMCHAR | CALL__THISCALL | 0x35fe |
| STR_COMPARE | CALL__CDECL | 0x390a |
| STR_LOWER | CALL__THISCALL | 0x40cc |
| MEM_FINDPARSERSYMBOL | CALL__THISCALL | 0x415b |
| MEM_GETBUFFERCRC32 | CALL__CDECL | 0x66b4 |
| MEM_SEARCHVOBBYNAME | CALL__THISCALL | 0x678d |
| MEM_SEARCHALLVOBSBYNAME | CALL__THISCALL | 0x6826 |
| MEM_RENAMEVOB | CALL__THISCALL | 0x68af |
| MEMINT_VOBGETEM | CALL__FASTCALL | 0x6907 |
| MEM_TRIGGERVOB | CALL__THISCALL | 0x699b |
| MEM_UNTRIGGERVOB | CALL__THISCALL | 0x6a28 |
| MEM_APPLYGOTHOPT | CALL__THISCALL | 0x6fc1 |
| MEM_GETSYSTEMTIME | CALL__CDECL | 0x7473 |
| MEM_GETPERFORMANCECOUNTER | CALL__STDCALL | 0x74d2 |
| MEMINT_SENDTOSPY_IMPLEMENTATION | CALL__THISCALL | 0x789e |
| MEM_ALLOC_ | CALL__CDECL | 0x84ab |
| MEM_FREE_ | CALL__CDECL | 0x8521 |
| _FLT_BUILD_FUNC | CALL__CDECL | 0x874d |
| HOOKENGINEF | HOOKENGINEI | 0xd675 |
| HOOKENGINE | HOOKENGINEI | 0xd6a6 |
| HOOKENGINES | HOOKENGINE | 0xd6cd |
| REPLACEENGINEFUNCI | HOOKENGINEI | 0xd9fa |
| KMLIB_INITIALIZEMENU | CALL__CDECL | 0x200a5 |
| KMLIB_INITIALIZEGAMESTART | CALL__CDECL | 0x200c0 |
| KMLIB_INITIALIZEALWAYS | CALL__CDECL | 0x200db |
| PC_SCOPE | CALL__CDECL | 0x2012d |
| GETCLIPBOARDDATA | CALL__STDCALL | 0x23f4b |
| GETKEYSTATE | CALL__STDCALL | 0x24613 |
| OCINFOMANAGER_GETINFOUNIMPORTANT | CALL__THISCALL | 0x25a7b |
| GAMESERVICES_UNLOCKACHIEVEMENT | CALL__CDECL | 0x297f0 |
| GAMESERVICES_GETSTAT | CALL__CDECL | 0x2981f |
| GAMESERVICES_INCREMENTSTAT | CALL__CDECL | 0x29864 |

## Deklaracje externali bez bezpośrednich wywołań: 76

Nie są automatycznie wymaganymi implementacjami Kronik. Potrzeba musi wynikać
z rozpoznanego wywołania pośredniego; brak krawędzi BE nie wyklucza takiego użycia.

| Deklaracja | Miejsca wywołania |
| --- | --- |
| PRINTMULTI | 0 |
| SETPERCENTDONE | 0 |
| NPC_MEMORYENTRY | 0 |
| NPC_MEMORYENTRYGUILD | 0 |
| NPC_HASNEWS | 0 |
| NPC_GETNEWSWITNESS | 0 |
| NPC_GETNEWSOFFENDER | 0 |
| NPC_GETNEWSVICTIM | 0 |
| NPC_ISNEWSGOSSIP | 0 |
| NPC_DELETENEWS | 0 |
| NPC_GETGUILDATTITUDE | 0 |
| WLD_DETECTPLAYER | 0 |
| WLD_DETECTNPCEXATT | 0 |
| NPC_CREATESPELL | 0 |
| NPC_LEARNSPELL | 0 |
| NPC_GIVEITEM | 0 |
| NPC_HASDETECTEDNPC | 0 |
| NPC_GETCOMRADES | 0 |
| NPC_ISNEAR | 0 |
| NPC_KNOWSPLAYER | 0 |
| NPC_SETKNOWSPLAYER | 0 |
| NPC_ISAIMING | 0 |
| NPC_STARTITEMREACTMODULES | 0 |
| NPC_HASOFFERED | 0 |
| AI_PLAYCUTSCENE | 0 |
| AI_CANSEENPC | 0 |
| AI_GOTOSOUND | 0 |
| NPC_AREWESTRONGER | 0 |
| NPC_ISNEXTTARGETAVAILABLE | 0 |
| NPC_ISVOICEACTIVE | 0 |
| NPC_GIVEINFO | 0 |
| TAL_CONFIGURE | 0 |
| NPC_ISWAYBLOCKED | 0 |
| AI_WHIRLAROUNDTOSOURCE | 0 |
| AI_TURNTOSOUND | 0 |
| AI_QUICKLOOK | 0 |
| AI_DEFEND | 0 |
| AI_COMBATREACTTODAMAGE | 0 |
| AI_ASK | 0 |
| AI_ASKTEXT | 0 |
| AI_WAITFORQUESTION | 0 |
| AI_SND_PLAY3D | 0 |
| SND_GETDISTTOSOURCE | 0 |
| SND_ISSOURCENPC | 0 |
| SND_ISSOURCEITEM | 0 |
| MIS_GETSTATUS | 0 |
| MIS_SETSTATUS | 0 |
| MIS_ONTIME | 0 |
| MIS_ADDMISSIONENTRY | 0 |
| MIS_REMOVEMISSION | 0 |
| NPC_CHECKAVAILABLEMISSION | 0 |
| NPC_CHECKRUNNINGMISSION | 0 |
| NPC_CHECKOFFERMISSION | 0 |
| AI_PLAYFX | 0 |
| WLD_INSERTNPCANDRESPAWN | 0 |
| NPC_OWNEDBYGUILD | 0 |
| AI_TAKEMOB | 0 |
| AI_DROPMOB | 0 |
| DOC_OPEN | 0 |
| DOC_FONT | 0 |
| DOC_PRINT | 0 |
| DOC_MAPCOORDINATES | 0 |
| TA | 0 |
| TA_CS | 0 |
| TA_BEGINOVERLAY | 0 |
| TA_ENDOVERLAY | 0 |
| TA_REMOVEOVERLAY | 0 |
| RTN_EXCHANGE | 0 |
| WLD_ASSIGNROOMTONPC | 0 |
| WLD_GETPLAYERPORTALOWNER | 0 |
| WLD_GETFORMERPLAYERPORTALOWNER | 0 |
| NPC_ISPLAYERINMYROOM | 0 |
| NPC_WASPLAYERINMYROOM | 0 |
| NPC_GETPORTALOWNER | 0 |
| HLP_CUTSCENEPLAYED | 0 |
| GAME_INITENGINTL | 0 |

## Dane i ograniczenia

DAT SHA256: `7fba1af724ae528836a2537573b0d36024cd8a92d493e15a11c8e159ef44d54e`. Przejrzano 206709 symboli
i 240563 statycznych wywołań BL/BE. Nierozpoznane bezpośrednie
cele bytecode: 0. To nie rozstrzyga semantyki handlerów
ani przechodniości gry. Szczegóły wykonanych zmian: ARCHOLOS_AUDIT_2026-10-24.md.
