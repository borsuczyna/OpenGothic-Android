# Handoff dla Claude — Kroniki Myrtany, Ikarus i LeGo

Stan przekazania: **26.10.2026**. Dokument przygotowany z aktualnego kodu, historii Git i raportów z 24.10.2026. W tej turze wykonano tylko odczyty i dokumentację, bez implementacji, kompilacji ani uruchamiania gry.

## 1. Cel i najważniejsze ustalenia

Cel użytkownika: doprowadzić Kroniki Myrtany do poprawnego działania w OpenGothic. Najpierw brakujące lub częściowo zaimplementowane funkcje potrzebne rzeczywistym skryptom CoM, zwłaszcza Ikarus/LeGo, zapis i sceny fabularne. Sprawdzać również TODO, puste nadpisania i istniejący kod bez skutku w silniku.

**Postęp ostatniej listy: 24/27 grup. To nie procent zgodności ani 24 z 27 funkcji.** Otwarte grupy: pozostałe hooki, pozostała semantyka wywołań natywnych/dynamicznych, odbiór w grze. Nie ma dowodu pełnej przechodniości kampanii ani gwarancji braku critical error.

Użytkownik zatrzymał implementacje poleceniem STOP, po czym zażądał kompilacji, commitu i push — wykonane 24.10.2026. Obecne polecenie dotyczy handoffu. Kontynuację implementacji prowadź po poleceniu użytkownika w swojej rozmowie; nie traktuj tego pliku jako polecenia wznowienia pracy w tle.

Zasady dalszej pracy wynikające z dotychczasowej sesji:

- Pracuj nad rzeczami potrzebnymi Kronikom. Nie rozszerzaj zakresu na dowolne DLL/x86/renderery bez wykazanego użycia.
- Użytkownik chce większe sensowne pakiety, a nie zatrzymywanie pracy po jednej drobnej funkcji. Po każdej funkcji/pakiecie napisz, co zmieniłeś; każda wiadomość o pracy powinna zawierać postęp.
- Nie dodawaj pustych handlerów, nie ukrywaj logów i nie oznaczaj adresu jako obsłużonego bez właściwego skutku.
- **Nie uruchamiaj sam gry, automatycznych testów, VM-testów, scen ani komend comcheck.** Dotychczas użytkownik wykonuje próby w grze; analiza kodu, DAT i istniejących logów oraz kompilacje są dozwolone. Nowe wyraźne polecenie użytkownika może zmienić tę zasadę.
- Okresowe kompilacje są dozwolone. Gdy gra blokuje EXE, poproś o zapis/zamknięcie; nie zabijaj procesu.
- Użytkownik wcześniej zlecał commit/push wszystkich zmian. Stan z 24.10 jest już wysłany; nie uznawaj nowego, zastanego pliku za własną zmianę i nie usuwaj go.
- Przy kolejnym STOP zakończ nowe implementacje, skompiluj bieżący stan i przygotuj listę odbioru; commit/push zgodnie z poleceniem użytkownika.

## 2. Bieżący checkout i historia

Repo: `C:/Users/Ernest/Desktop/opengothic`, PowerShell, gałąź **touch-ui**.
Remote: `https://github.com/borsuczyna/OpenGothic-Android`.
HEAD sprawdzony 26.10: **7ba334d2**, wersja projektu Android z serii 0.8.4.

Ważne commity zakończonej pracy CoM:

| Commit | Zakres |
| --- | --- |
| ec09a5fb | Duży checkpoint zgodności Archolos oraz wszystkich ówczesnych zmian grafiki/mobile, zgodnie z poleceniem commitowania wszystkiego. |
| c0cb8c6f | Crafting, stos wywołań Ikarusa/ZenKit, hooki scenek, wejście, MEM_SetKeys i strefy muzyki. |
| 5af71f9a | Trigger/Untrigger dźwięków ZEN, stan zbroi, poziom czaru, analizator callbacków i checkpoint STOP. Wysłany na origin/touch-ui. |

Po 5af71f9a doszło osiem commitów: 118afdc1, bb5a0ddd, f4ef496c, 3cf285c1, ee36a72d, 96ddb583, 7b94b6e1, 7ba334d2. Dotyczą m.in. Adreno/Mali, Safe GPU, diagnostyki save/load, pakowania Androida, wersji 0.8.4 i walidacji uszkodzonych plików gry. **Nie cofaj ich do checkpointu CoM.** W katalogu zgodności różnica względem 5af71f9a to m.in. jawne rzutowania uint32_t przy masce ITM_TORCH wymagane przez Android/clang.

Stan przed zapisaniem tego handoffu: główny checkout bez niezacommitowanych zmian kodu, ale `lib/Tempest` i `lib/ZenKit` oznaczone jako zmodyfikowane oraz nieśledzony `crashes/1.log`.

Zmiany bibliotek są utrzymywane jako `patches/tempest-opengothic.patch` i `patches/zenkit-opengothic.patch`, stosowane przez `cmake/tempestpatch.cmake` i `cmake/zenkitpatch.cmake`. Brudny submoduł jest spodziewany. Nie resetuj go i nie commituj do obcych upstreamów zamiast zachować patch w repo głównym.

`crashes/1.log` identyfikuje **Gothic1, Android 13, OpenGothic 0.8.3, Adreno 619**, a stos kończy się tworzeniem pipeline GPU. Nie jest dowodem crasha Ikarusa/Kronik; sąsiedztwo logu „not implemented” nie dowodzi przyczyny. Zachowaj plik; nie przenoś tego od razu do priorytetów CoM.

## 3. Co faktycznie potwierdzono

**Potwierdzone przez użytkownika w tej rozmowie:**

- Muzyka menu: po kilku poprawkach gra wyłącznie właściwy motyw Kronik. Przyczyną nakładania był również startowy GAMESTART.WAV, a nie tylko odczyt VDF.
- Wygląd panelu craftingu zgodny z drugim zrzutem: tło, model przedmiotu, odstępy i kolorowe składniki.

Nie rozszerzaj potwierdzenia wyglądu na koszty, ilości, „Wszystko”, wykonanie receptury, resize ani zapis. Nie rozszerzaj muzyki menu na wszystkie strefy świata i koncert.

Archiwalny handoff z 20.10 opisuje dodatkowe wcześniejsze potwierdzenia użytkownika: czekanie/anulowanie kolejki, zapis podczas czekania, AI_DropItem, skrzynia, przygotowany strzelec i wymuszona scena palaczy. To wąskie próby, nie naturalne przejście kampanii. Nie uruchamiano ich ponownie przy obecnym przekazaniu.

Próby z listy w [PROGRESS.md](C:/Users/Ernest/Desktop/opengothic/docs/PROGRESS.md) pozostają do odbioru, jeśli nie ma osobnego wyniku użytkownika. `[x]` oznacza tam głównie kod, nie test kampanii.

## 4. Źródła i autorytatywny DAT

Wszystkie poniższe katalogi nadal istnieją — sprawdzono 26.10:

```powershell
$repo = 'C:/Users/Ernest/Desktop/opengothic'
$coMSourceRoot = 'C:/Users/Ernest/AppData/Local/Temp/claude/C--Users-Ernest-Desktop-opengothic/b483b2a9-e243-4cde-b63e-7d3878f44c49/scratchpad'
$coMMod = "$coMSourceRoot/tcom/tcom-decompilation-main/Source"
$coMIkarus = "$coMSourceRoot/Ikarus/Ikarus-master"
$coMLeGo = "$coMSourceRoot/LeGo/LeGo-dev"
```

- Repo skryptów: `https://github.com/lmichaelis/tcom-decompilation`.
- Ikarus: `https://github.com/Lehona/Ikarus`, master.
- LeGo: `https://github.com/Lehona/LeGo`, dev.
- Dekompilacja: około 4999 plików .d, Latin-1, nazwy wielkimi literami, funkcje bywają połączone w obcym pliku. Przeczytaj GothicMOD-License.txt przed rozpowszechnianiem źródeł.
- **Dekompilacja jest przybliżona. Nie ufaj brakującym fragmentom, numerom symboli ani pozornemu przepływowi if/return. Sprawdzaj aktualny DAT.** Wersje bibliotek z Git mogą różnić się od wkompilowanych.
- `C:/Users/Ernest/Desktop/opengothic/build-touch/archolos-input/GOTHIC.DAT` — lokalny, ignorowany plik, SHA256 sprawdzony ponownie 26.10: `7fba1af724ae528836a2537573b0d36024cd8a92d493e15a11c8e159ef44d54e`.
- Obok: MENU.DAT oraz KM_Scripts.mod.VISUALFX.DAT. Nie dodawaj DAT ani całej dekompilacji do Git.
- Instalacja gry i oryginalny EXE do statycznego porównania ABI: `C:/Program Files (x86)/Steam/steamapps/common/TheChroniclesOfMyrtana`, EXE w System/Gothic2.exe.
- Nie skanuj całego scratchpad rekurencyjnie: istnieje problematyczna duża ścieżka/junction. Ogranicz wyszukiwanie do podanych podkatalogów.

Analiza i dezasemblacja nie wykonują skryptów gry:

```powershell
Set-Location -LiteralPath $repo
python tools/ikarus/disasm.py build-touch/archolos-input/GOTHIC.DAT TRIA_FINISH NPC_CHANGEARMOR
python tools/ikarus/analyze.py --ikarus $coMIkarus --lego $coMLeGo --mod $coMMod --dat build-touch/archolos-input/GOTHIC.DAT --repo . --out build-touch/archolos-audit-2026-10-26
```

Przy własnym parserze Python ustaw `sys.stdout.reconfigure(errors='backslashreplace')`: ukryte nazwy symboli zawierają `\xff` i potrafią wysypać konsolę cp1250. Do tekstu źródeł używaj Latin-1. Na Windows `rg` z `-g '*.cpp'`, zamiast globów typu `common/gothic.*` w argumencie ścieżki.

## 5. Co już istnieje — nie implementuj drugi raz

| Obszar | Implementacja i istotny kontrakt |
| --- | --- |
| Pamięć/uchwyty | Mem32: granice, overflow, free/realloc; stabilne uchwyty NPC/item, 8192 sloty bez nadpisania żywego obiektu przy pełnej puli; user_ptr zerowany po usunięciu. Stabilne napisy memory_instance per adres. |
| Wywołania Ikarusa | MEM_Call, GetFuncID, PushInt/String/InstParam, CallByID/Ptr/String oraz invoke_function respektują nadpisania. Naked adaptery zachowują argumenty i wyniki; dodatkowa ramka potrafi je zniszczyć. |
| Bajty/stringi | MEM_ReadByte/WriteByte rzeczywiście jednobajtowe, CompareBytes waliduje cały zakres, ujemny CopyBytes nie kopiuje. MEM_WriteString obsługuje pożyczany bufor. SB_ToString i STR_FromChar mają walidację. |
| AI_Function | Parser CALL I/N/S, walidacja typów, floaty bitowe, escape stringów, RAII kontekstu. W aktualnym kodzie OTHER jest ustawiany na gracza, gdy jest dostępny — nie przywracaj starego opisu „zawsze null”. |
| Pliki LeGo/zapis | Wirtualne CreateFile/ReadFile/WriteFile/CloseHandle/GetFileSize, zapis game/dma. _BW_SAVEGAME i torch przed snapshotem, zapis gotowego VFS na końcu. Pełne PermMem/FF nadal wymaga przeglądu. |
| Trialog Q102 | Synchronizacja TRIA_FINISH z kolejkami i blokada przedwczesnego zamknięcia dialogu. Skip zatrzymuje głos również trzeciego NPC i gesty. Naturalne „Idę z Jornem/Fabio” nie ma końcowego odbioru. |
| Scenki | DIACAM_UPDATE/StartDialogCam; mosty menu 0x42abe7, HUD 0x6c31d6, napisów 0x7a9240, kamery i plansz końcowych. Jawna blokada nie wygasa po heurystycznych 120 s. |
| Dźwięk/muzyka | Menu CoM, nagrane OGG, mapowanie stref i override track; ENABLEMUSICZONE, enabled/priority/ellipsoid ZEN, zapis music-zones. STOPALLSOUNDS zatrzymuje świat/globalne/głos DialogMenu, nie muzykę. |
| Dźwięki ZEN | Trigger startuje, Untrigger zatrzymuje współdzielony slot i wznowienia; opcjonalny zapis aktywności/opóźnienia w sound-zones. Konkretne użycia Q101 Smalltalk/Scared i SQ125 piła. |
| Crafting/LeGo UI | Niezerowa skala HPBAR, panel DLG_CHOICE.TGA, model, tint i pomiar czcionki, odświeżenie po resize/zmianie zakładki, sprzątanie C_RECIPE, komparator listy przez MEM_Call. Wygląd potwierdzony. |
| Sprite/Render | UV/obrót/jednolity kolor, blend 1/2/3; Render preview używa natywnego InventoryRenderer, własnych itemów i zasobów na klatkę GPU. To nie pełny renderer Gothica. |
| Ekwipunek/wyposażenie | Natywne kategorie CATINV, maski itemów, idempotentne Equip/Unequip, BestWeaponByType i hook, GetSlotItem, DropFromSlot, kołczany, pochodnie i chronione ogłuszenie. Pełna semantyka CATINV wymaga przeglądu. |
| Zbroje i wygląd | NPC_CHANGEARMOR zapisuje AIVAR[48], -1 zdejmuje zbroję, 0 zachowuje, dodatnia wartość idempotentnie wyposaża. MDL_SetVisualBody analogicznie. BODY/HEAD oraz BITFIELD mają osobne pola, nie C_NPC.MISSION. |
| Wejście | Kody DIK, cyfry/F1–F15/mysz, zwolnienie przy utracie fokusu; MEM_GetKey/SecondaryKey/SetKeys. INIT_TORCHKEY może ustawić T i przecinek. GetState/IsBindedToggled obsługują wykazaną akcję broni 8; __ftol zwraca int64 w EDX:EAX. |
| Dialog tekstowy | Prompt psa/wina istnieje. W bieżącym kodzie jest już mapowanie polskich liter Unicode do cp1250; stary opis „tylko ASCII” jest nieaktualny. Nadal potrzebny odbiór w grze, modifierów, Escape i limitu 24 bajtów. |
| Walka/drużyna | NextEnemy 0x734f4d, widoczność i party, tymczasowe nastawienia oddzielne od stałych; descriptor obrażeń przed pancerzem, pierścień/penetracja/dontKill. Wymaga naturalnych walk do odbioru. |
| Magia/zamek | Focus moba, DoTakeVob pod wodą, trwały postęp zamka, warunkowe zużycie zwoju; realna telekineza i fizyka przy zakończeniu/anulowaniu/zapisie. |
| Efekty czasowe | time.slw: zegar modelu, ruch/animacje/zdarzenia, wygaśnięcie i zapis; SPL_LIGHT: zapis pozostałego czasu; buff nurkowania 30000/31000 ms, tlen/obrażenia/HUD i zapis. To nie serializacja wszystkich FX. |
| Świat/questy | Wld_InsertObject dla obsługiwanych wizualizacji z zapisem, CHANGEVOBCOLLISION na realnym obiekcie, zmiana tekstury ścian, load/save locks, dziennik i Log_MoveToTop. |
| Pozostałe | Timer świata przed FF, ScreenFX modulo liczby klatek, QSORT_G2 całych rekordów z domyślnym unsigned komparatorem; invokeSpell przekazuje poziom fazy emisji zamiast stałego 0. |

**Ważna pułapka:** `_AI_TELEPORTALWAYS` już ma adapter w setupQuiverFunctions, w directmemory_events.cpp. AI_TELEPORTALWAYS ma 95 miejsc wywołania w 53 funkcjach DAT, a callback idzie pośrednio przez AI_Function_IS. Pod koniec poprzedniej pracy błędnie dodano drugi adapter, po czym go usunięto. Nie dodawaj go ponownie ani nie zgłaszaj jako nowej naprawy.

## 6. Gdzie czytać kod

Ścieżki poniżej są względem `C:/Users/Ernest/Desktop/opengothic`:

- `common/game/compatibility/directmemory.cpp`: pamięć VM, symbole/instancje, wywołania, aliasy, WinAPI, wyszukiwanie moverów, AI_Function i tick FF.
- `directmemory_engine.cpp` w tym samym katalogu: natywne adresy, wejście, magia, muzyka, tekstury.
- `directmemory_events.cpp`: mosty zdarzeń, descriptor, moby, telekineza, wyposażenie, trialog i istniejący teleport.
- `directmemory_ui.cpp`: widoki/fonty/sprite/Render/crafting; `directmemory_files.cpp`: VFS i save/load.
- `hookaddresses.h`, `directmemory_hooks.cpp`: adresy, kontrakty rejestrów/stosu, emisja fireHook. isHookAnswered to deklaracja, nie samodzielny dowód poprawnej emisji.
- `cpu32.cpp`, `mem32.cpp`, `instancehandles.cpp`: interpreter, pamięć i uchwyty.
- `lib/ZenKit/src/DaedalusVm.cc` i patch ZenKit: stos/ramki. call_function potrafi celowo omijać override; invoke_function respektuje override. W craftingu oryginalne body jest wywoływane celowo.
- `common/game/gamescript.cpp`, `common/world/objects/npc.cpp`, `common/game/inventory.cpp`: rzeczywiste skutki w silniku.
- `common/ui/dialogmenu.cpp`, `common/mainwindow.cpp`: dialog, głos, napisy, UI i blokady scenek.
- `common/gamemusic.cpp`, `common/world/worldsound.cpp`: warstwy muzyki, strefy, SFX i ich zapis.
- `mods/devtools/lib/archoloschecks.js`: ręczne komendy. Build osadza/kopiuje JS; samo edytowanie źródła nie zmienia wcześniej zbudowanego EXE.

## 7. Co zostało i w jakiej kolejności pracować

### A. Najpierw ustalenie rzeczywistych blockerów CoM

1. Przeczytaj git status/diff i niniejszy dokument; uruchom świeży analizator na obecnym HEAD i tym samym DAT. Snapshot z 24.10 nie jest nowym pomiarem z 26.10.
2. Dla brakującego adresu znajdź wywołujący skrypt, drogę od questa/callbacka, argumenty i wynik. Sprawdź też, czy cały wywołujący ma już override albo natywne UI pokrywa zachowanie.
3. Pierwszeństwo: awaria/pamięć, utrata stanu po zapisie, zakleszczenie sceny lub zadania. Nie zastępuj końca sceny upływem timeoutu.
4. Przy każdym rzeczywistym braku: zaimplementuj skutek, zapisz kontrakt/dowód i podaj ręczną próbę użytkownikowi. Nie uznawaj próby za zdaną samym buildem.

### B. Obszary nadal wymagające implementacji lub przeglądu semantyki

- [ ] **PermMem/FF i trwałość:** create/delete/clear/load, destruktory klas, callbacki po wielu loadach, lifetime napisów/widoków/referencji do NPC/item. Restart procesu i zmiana świata. Istniejące pliki VFS nie dowodzą kompletności obiektów biblioteki.
- [ ] **Kolejki/trialog/sceny:** naturalny Q102 Jorn/Fabio, skip, anulowanie, zniknięcie NPC, wyjście ze świata; kamera/menu/HUD/sterowanie po zakończeniu. Kod poprawki istnieje, wynik nadal niepotwierdzony.
- [ ] **CATINV:** dla każdego starego hooka wykazać odpowiednik w natywnym UI: kategorie/offsety/strony, transfery, ostatnia sztuka stosu, martwy NPC, handel, skrzynia, ważne przedmioty i blokady. Nie trzeba kopiować prywatnych linked list Gothica, ale decyzje skryptowe muszą działać.
- [ ] **Crafting:** grupy składników, prawdziwe owned/required, warunki, koszty, wiele sztuk/Wszystko, brak zasobów, wynik, zakładki, wyjście i resize. Nie podkładać liczb z referencyjnego screena.
- [ ] **Dynamiczne wywołania:** 39 nierozpoznanych callbacków i 41 miejsc dynamicznych adresów DAT w snapshotach. Rozstrzygać konkretne cele; nie zakładać „zero wywołań = nieużywane”.
- [ ] **Pamięć/x86:** pozostałe realnie potrzebne instrukcje i kontrakty, alias VM vs raw write, niepowiązane pola. MEMINT_StackPushVar nadal jest TODO; jego główne ścieżki mają adaptery, więc najpierw ustal, czy pozostała ścieżka faktycznie go wykonuje.
- [ ] **Listy vobów/waynet:** ogólne MEM_SearchVobByName w warstwie zgodności nadal przeszukuje movery. Niektóre wysokopoziomowe operacje omijają ten brak (kolizje po nazwie, EnableMusicZone, teleport). Ustal pozostałe faktyczne odczyty, zamiast tworzyć fikcyjne uchwyty.
- [ ] **WLD_ISRAINING:** nadal false, 5 miejsc DAT: B_SAY_GUILDGREETINGS, dwa C_CANNPCCOLLIDEWITHSPELL, DIA_OLDMAN_GOODWATER_CONDITION, EVENTSMANAGER_NS06. Brakuje spójnej pogody; nie dodawać losowego true odłączonego od świata. Warunki staruszka wymagają suchości, więc nie ogłaszaj ich zablokowanymi bez dowodu.
- [ ] **MDL_APPLYRANDOMANI/FREQ:** nadal puste, 3+1 miejsca ZS_MM_RTN_EATGROUND: S_EAT -> R_ROAM1/2/3, częstotliwość float 8.0 s. Była tylko analiza, bez implementacji. Potrzebne per-model powiązania, wybór tylko podczas właściwej animacji, powrót, reset i zapis. Nie nadpisywać globalnie współdzielonego zasobu animacji NPC.
- [ ] **KmLib:** InitializeMenu/Gamestart/Always i GameServices_GetStat/IncrementStat/UnlockAchievement nie mają pełnych usług. LoadLibrary/GetProcAddress zwracają zastępcze uchwyty. Nie przedstawiać lokalnego licznika jako integracji Steam. Muzyka ma oddzielne działające adaptery.
- [ ] **Render/GUI:** gradienty sprite, pozostałe blend modes, zagnieżdżone widoki/animacje okien, dowolna rotacja i kolejność 2D/3D. Implementować te używane przez CoM; istniejący preview nie jest pełną emulacją oCItem::Render.
- [ ] **Muzyka/strefy:** naturalne enabled/priority/ellipsoid, koncert HARBOUR_INX, override, powrót po scence, zapis i zmiana świata. Reverb/volumeLevel oraz wejściowe day/nightEntrance nie są pełne. Menu już odebrane.
- [ ] **Pozostałe FX:** pełna serializacja efektów/ich faz poza time.slw i SPL_LIGHT nie istnieje. Odbiór opisanych buffów i światła nie oznacza kompletnego FX.
- [ ] **Nastawienia/obrażenia:** przegląd skutków descriptorów, pancerza, penetracji, magii i dontKill w naturalnych walkach, także po loadzie. Kod istnieje.
- [ ] **Nazwy tekstowe:** ręczny odbiór imienia psa/nazwy wina, polskich liter cp1250, Backspace, Escape, Enter, dotyku i modifierów. Nie przywracać starej blokady ASCII.
- [ ] **Ustawienia/loading/save metadata:** mosty ApplySomeSettings/SightScaling, alternatywne loadingi, nazwa świata i wersja zapisu. Nie mylić z odczytem formatu samego save'a.
- [ ] **Niestandardowy QSORT komparator:** domyślny działa; dla innych ustalić adres i ABI. FINAL jest optymalizatorem bytecode z zerem bezpośrednich wywołań w tym DAT, nie „brakującym destruktorem PermMem”.
- [ ] **Schowek/konsola/telemetria:** konkretne brakujące adresy są niżej; priorytet po stanie kampanii i awariach.

MDL_ApplyRandomFaceAni i NPC_IsDetectedMobOwnedByGuild także mają niepełny kod, ale w zbadanym DAT zero bezpośrednich wywołań. Tak samo 76 niepowiązanych deklaracji externali nie jest automatycznie listą 76 blockerów. Sprawdzać użycie pośrednie.

### C. Odbiór ręczny nadal do wykonania

Pełna lista jest w [bieżącej liście odbioru](C:/Users/Ernest/Desktop/opengothic/docs/ARCHOLOS_PROGRESS_2026-10-24.md). Minimalny zestaw:

1. Q102 „Idę z Jornem” i „Idę z Fabio”, zwykła rozmowa i skip; potem kolejny dialog, rutyny, kamera, dziennik; TRIA_RUNNING/TRIA_CPTR wracają do 0.
2. Crafting: receptury/koszty/ilości/Wszystko/zakładki/resize, naturalne wytworzenie.
3. Dziennik: „W gorącej wodzie kąpany” i inne ukończone/niezaliczone zadania, ponowny load.
4. Scenki ponad 2 min, koniec kamery i plansze końcowe; HUD, menu, kolor/nazwa/pozycja napisów wracają.
5. Strefy muzyki, dzień/noc/walka, In Extremo 18–21, oba stany strefy po loadzie i zmianie świata.
6. Q101 Smalltalk/Scared i SQ125 piła: Untrigger wycisza i stan nie wraca sam po loadzie.
7. QA306 Bloodwyn NO_ARMOR, Q102/rozdziały zmiany munduru, ta sama zbroja dwa razy, zmiana twarzy, zapis.
8. T/przecinek pochodni, inne przypisania i drugi klawisz, ekwipunek/kołczan/ręce, zapis bez duplikatów.
9. Handel/kontener/zwłoki/ostatnia sztuka/ważny item, wszystkie kategorie i blokady.
10. Telekineza, anulowanie i fizyka po zapisie; zaklęcie zamka, klucze/kod/postęp/zużycie; wodne podnoszenie i buff nurkowania.
11. Spowolnienie i Otis Q306, SPL_LIGHT w Q501, pozostały czas po restarcie procesu.
12. Party/cele za przeszkodą, pierścień maga, penetracja, strzały, ogłuszenie bez zabijania i nastawienie po walce.
13. Mapy/model preview, ściany domu 0..11, nazwy psa/wina.
14. PermMem/FF po wielu loadach/quicksave/restartach/zmianie świata; sceny Q204/Q205/Q304/Q306/Q406/Q501/Q602/GQ001, KQ402/SQ202/SQ509, drogi gildyjne i zakończenia.

Komendy ręczne z JS: `comcheck trialog`, `crafting`, `cutscene`, `musiczone`, `telekinesis`, `breath`, `slowdown`, `attitude`, `spellactive`, `torch`, `category`, `map`, `preview`, `bestweapon`, `housewall`, `scenelock`. Składnię i skutki odczytaj w archoloschecks.js. Część tylko odczytuje stan; część zmienia świat. Nie wykonuj ich sam i nie polecaj mutujących prób na jedynym zapisie użytkownika.

## 8. Jak interpretować audyt

Snapshot: `C:/Users/Ernest/Desktop/opengothic/docs/archolos-audit/2026-10-24/`.

- 206709 symboli DAT, 240563 miejsc BL/BE; zero nierozpoznanych bezpośrednich celów bytecode.
- 2529 miejsc callbacków, 2490 rozwiązanych, 39 dynamicznych. Callbacki są osobną tabelą, nie doliczać do BL/BE.
- 29 różnych adresów bez handlera, 40 bez ogólnego mostu hooka, 2 proste atrapy. To snapshot, nie wynik gry i nie liczba potwierdzonych awarii.
- 41 dynamicznych miejsc DAT w native-and-hooks.csv to inna kategoria niż 39 callbacków.
- Liczby większe w summary.json mogą obejmować źródła Ikarus/LeGo/mod oraz wielokrotne miejsca tego samego adresu. Do unikalnych adresów bieżącego DAT używaj addresses.csv, nie sum źródeł.
- functions.csv: licznik miejsc w kodzie, nie liczba wykonań. W C++ część externali może być powiązana przez wrappery; registration != poprawna implementacja.
- callback-references.csv: stałe cele AI_Function, FF_Apply, CallAfterWorldLoaded, MEM_Call/ByID/ByString; normalizuje m.in. -(1). Bez pełnego dataflow; dynamicznych celów nie zgaduje.
- implementation-review.csv i markers.csv: atrapy/TODO do przeglądu, nie automatyczna lista wymaganych zmian.
- native-members-review.csv: 20 kandydatów bez jawnego aliasu; część należy do funkcji już zastąpionych. Tabela jest w pełnym raporcie pozostałych prac.

Ignorowany pomocnik `build-touch/write_remaining_report.py` generował PROGRESS/REMAINING i kopiował snapshoty. Ma na sztywno datę 24.10 i licznik 24/27; zaktualizuj generator albo zapisuj nowy raport, zamiast nadpisywać świeży handoff starą datą.

Dodatkowe wskazówki przed ewentualną implementacją RandomAni: dotychczas przejrzano referencję API, SDK i oryginalny EXE, bez uruchamiania gry. Częstotliwość to sekundy; GetCreateAniAttachment zapisuje domyślne float 1.0 w 0x583700, SetRandAniFreq jest pod 0x583ac0. Pose::processLayers obecnie szybko wraca przy hasTransitions==0, więc samo dodanie listy do solvera nie uruchomi losowej animacji S_EAT. Zegar powinien współgrać z modelTime/time.slw i nie przerywać dowolnej walki/ruchu. Nie ma jeszcze implementacji tych zmian.

Źródła kontraktu: https://auronen.cokoliv.eu/gmc/zengin/scripts/externals/mdl/#mdl_applyrandomanifreq oraz https://raw.githubusercontent.com/PhoenixTales/nyx-core/main/ZenGin/Gothic_II_Addon/API/zModel.h .

## 9. Kompilacja, logi i patche

```powershell
Set-Location -LiteralPath 'C:/Users/Ernest/Desktop/opengothic'
$env:PATH = 'C:/msys64/mingw64/bin;' + $env:PATH
cmake --build build-touch -j 6 *> build-touch/archolos-claude-build.log
$buildResult = $LASTEXITCODE
Get-Content build-touch/archolos-claude-build.log -Tail 35
if ($buildResult -ne 0) { throw "Build failed: $buildResult" }
```

Zachowaj zgodność zapisów: wcześniejszy pakiet kolejek używa Serialize::Current=56; nie przestawiaj istniejących numerów akcji AI. Nowy opcjonalny stan zapisywano w osobnych wpisach ZIP z fallbackiem dla starych zapisów.

Nie uruchamiaj dwóch buildów w tym samym katalogu równocześnie. Gdy plik zmieni się podczas kompilacji, po jej zakończeniu wykonaj kolejny incremental build, aby wyrównać zależności. Nie zastępuj kontroli exit code potokiem Select-String.

Ostatnie buildy sesji CoM z 24.10: `archolos-armor-spelllevel-build-2026-10-24.log` zlinkował oba EXE kodem 0; `archolos-stop-build-2026-10-24.log` zakończył się kodem 0 z „ninja: no work to do”. To nie dzisiejszy build HEAD. Obecny plik EXE ma czas 26.10.2026 14:56:12 i 697912716 bajtów, bez nowego potwierdzenia działania z tej tury. Duży rozmiar wiąże się z RelWithDebInfo; nie stripuj samowolnie.

Wyniki: `C:/Users/Ernest/Desktop/opengothic/build-touch/opengothic/Gothic2Notr.exe` i Spacer.exe.

Walidacja zmian:

```powershell
git diff --check -- . ':!patches/*.patch'
git -C lib/ZenKit apply --ignore-whitespace --check --reverse C:/Users/Ernest/Desktop/opengothic/patches/zenkit-opengothic.patch
git -C lib/Tempest apply --ignore-whitespace --check --reverse C:/Users/Ernest/Desktop/opengothic/patches/tempest-opengothic.patch
```

Patche eksportuj jako surowe bajty git diff --binary HEAD (np. subprocess.check_output + Path.write_bytes). Windowsowe write_text/obcinanie białych znaków już raz uszkodziły końce linii lub kontekst patcha. Nie usuwaj spacji z pustych linii kontekstu tylko po to, by uciszyć diff --check.

Logi czytaj selektywnie, np. Get-Content -Tail 100. Nie wypisuj całych ogromnych DAT/JSON i nie czytaj narastającego logu bez limitu.

## 10. Dokumenty i gotowy start

- [Szczegółowy aktualny backlog z tabelami](C:/Users/Ernest/Desktop/opengothic/docs/ARCHOLOS_REMAINING_2026-10-24.md).
- [Opis implementacji i pułapek](C:/Users/Ernest/Desktop/opengothic/docs/ARCHOLOS_AUDIT_2026-10-24.md).
- [Postęp i lista odbioru](C:/Users/Ernest/Desktop/opengothic/docs/PROGRESS.md).
- [Architektura zgodności](C:/Users/Ernest/Desktop/opengothic/docs/IKARUS.md).
- Starsze ARCHOLOS_HANDOFF_CLAUDE.md i ARCHOLOS_HANDOFF_BACKLOG.md: materiał historyczny z 20.10. Nie przywracaj ich list braków jako aktualnej.

Przy kontynuacji: sprawdź aktualny checkout i świeży audyt, wybierz wykazany brak na ścieżce CoM według sekcji 7, porównaj źródło z DAT i kodem C++, zaimplementuj rzeczywisty skutek, skompiluj i opisz próbę ręczną. Uaktualnij raport, a nie tylko komunikat na czacie. Pełnej zgodności nie deklaruj bez odbioru.

## 11. Pełna lista rozpoznanych adresów z 24.10

Poniżej zostaje dołączony dokładny snapshot 29 adresów, 40 hooków i 2 atrap z raportu pozostałych prac. Statusy należy zweryfikować na aktualnym HEAD. Część ma już równoważny adapter/UI, dlatego „bez ogólnego mostu” nie oznacza automatycznie zablokowanej kampanii.

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
