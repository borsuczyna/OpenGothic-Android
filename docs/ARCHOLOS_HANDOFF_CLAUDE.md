> **Archiwum z 20.10.2026. Aktualny handoff z 26.10.2026:** [ARCHOLOS_HANDOFF_CLAUDE_2026-10-26.md](C:/Users/Ernest/Desktop/opengothic/docs/ARCHOLOS_HANDOFF_CLAUDE_2026-10-26.md). Poniższe opisy braków, HEAD, niecommitowanych zmian, OTHER i ASCII są historyczne; nie stosuj ich zamiast aktualnego kodu i nowego handoffu.

# Handoff dla Claude: Kroniki Myrtany w OpenGothic

Stan przekazania: **20.10.2026**, po potwierdzeniu użytkownika „oba działają”, odnoszącym się do ostatnio przygotowywanych prób strzelania i sceny palaczy. To lista wykonanej pracy i wszystkich znanych pozostałych obszarów z audytu. Pełna przechodniość Kronik nie została jeszcze potwierdzona.

## 1. Polecenie i zasady użytkownika

Cel: doprowadzić Kroniki Myrtany do pełnej grywalności w OpenGothic, implementując potrzebną semantykę Gothica, Ikarusa, LeGo i KmLib. Pracować kolejno, w małych pakietach, z czytelnymi próbami ręcznymi.

**Użytkownik wyraźnie zabronił agentowi samodzielnego testowania:** nie uruchamiaj gry, testów automatycznych, komend `comcheck` ani scen. Czytaj kod/logi, implementuj i rób buildy. Po każdym pakiecie napisz dokładnie, co użytkownik ma wpisać, gdzie stanąć, co ma zobaczyć oraz jaki wynik zgłosić. Nie zastępuj implementacji wyciszeniem logów, pustymi handlerami ani samym dopisaniem adresu do listy „obsługiwanych”.

Ta tura przygotowała dokumentację i zachowała logi; nie wykonywała kolejnego buildu ani nowych prób. Ostatni build kodu zakończył się powodzeniem, zlinkował oba EXE. Próby w grze wykonywał użytkownik.

Workspace: `C:/Users/Ernest/Desktop/opengothic`.

Komenda build podana przez użytkownika, PowerShell w katalogu workspace:

```powershell
$env:PATH = "C:\msys64\mingw64\bin;" + $env:PATH; cmake --build build-touch 2>&1 | Select-String -Pattern "error|Linking|FAILED" -Context 0,4 | Select-Object -First 30
```

Pliki wynikowe:

- `C:/Users/Ernest/Desktop/opengothic/build-touch/opengothic/Gothic2Notr.exe`
- `C:/Users/Ernest/Desktop/opengothic/build-touch/opengothic/Spacer.exe`

Filtr wyjścia zawiera także nieszkodliwe `QJS_BUILD_WERROR: OFF`. Potwierdzaj końcowe linkowanie i rzeczywiste błędy, nie sam kod wyjścia końca potoku. Jeśli gra blokuje podmianę EXE, poproś użytkownika o zapis i zamknięcie; nie zabijaj procesu.

Uruchomienie do przekazania użytkownikowi:

```powershell
cd C:/Users/Ernest/Desktop/opengothic/build-touch/opengothic
.\Gothic2Notr.exe -g "C:/Program Files (x86)/Steam/steamapps/common/TheChroniclesOfMyrtana" -game:TheChroniclesOfMyrtana.ini
```

## 2. Repozytorium i źródła: nie zgub istniejących zmian

HEAD: `501b5c117a9ceb200c39fa3e485046fa4200b593`, `Dialogs: typed answers (CoM '?Name' choices), text prompt for keyboard and touch keyboard`.

**Cała praca opisana niżej pozostaje niecommitowana.** Są też zmiany zastane w checkoutcie, w tym diagnostyka animacji i oczekiwanie na kolejki w `DialogMenu`. Nie resetuj repozytorium ani submodułu. Przeczytaj `git diff` i `git status --short` przed edycją. Nie przypisuj wszystkich zastanych zmian obecnemu pakietowi.

W submodule `C:/Users/Ernest/Desktop/opengothic/lib/ZenKit` zmodyfikowane są:

- `include/zenkit/DaedalusVm.hh`
- `src/DaedalusVm.cc`

To potrzebna lokalna zmiana VM. Sam diff głównego repo pokazuje jedynie brudny submoduł; zachowaj również jego zawartość przy przenoszeniu pracy.

Źródła moda od użytkownika:

`C:/Users/Ernest/AppData/Local/Temp/claude/C--Users-Ernest-Desktop-opengothic/b483b2a9-e243-4cde-b63e-7d3878f44c49/scratchpad/tcom/tcom-decompilation-main/Source`

Dekompilacja ma błędy i brakujące fragmenty. Pełny lokalny DAT odczytano z:

`C:/Program Files (x86)/Steam/steamapps/common/TheChroniclesOfMyrtana/Data/KM_ScriptsPL.mod`

Wersja badanych danych: `GAME_VERSION=23`, `BUILD_VERSION=142565`, Ikarus `10202`, LeGo `2.7.0`. Archiwum SHA-256: `B4900CC7DD109DA79017F6A2F2E12020CCDE79886F9DFE0573CBB159887BF5F6`.

Pomocnicze metadane DAT:

`C:/Users/Ernest/AppData/Local/Temp/claude/C--Users-Ernest-Desktop-opengothic/b483b2a9-e243-4cde-b63e-7d3878f44c49/scratchpad/archolos-audit-2026-10-19/dat-symbols.json`

W tym samym katalogu jest `functions.json`. Oba pliki czytaj selektywnie parserem JSON; nie wypisuj ich w całości przez `rg`. Odwołuj się do nazw symboli z aktualnego DAT, a nie numerów z dekompilacji. Sygnatury externali sprawdzaj w DAT.

Dokumenty w workspace:

- `C:/Users/Ernest/Desktop/opengothic/docs/ARCHOLOS_AUDIT.md` — pełny audyt statyczny stanu wyjściowego.
- `C:/Users/Ernest/Desktop/opengothic/docs/ARCHOLOS_TESTING.md` — aktualne szczegółowe próby A–H.
- `C:/Users/Ernest/Desktop/opengothic/docs/ARCHOLOS_HANDOFF_BACKLOG.md` — pełne tabele historycznie brakujących hooków/adresów/externali i ich znaczenie dla kontynuacji.
- `C:/Users/Ernest/Desktop/opengothic/docs/archolos-audit/` — źródłowe CSV i `summary.json`.
- `C:/Users/Ernest/Desktop/opengothic/docs/MODS.md` — API modów JS i dodane narzędzia.

CSV i pierwotny audyt **nie zostały przeliczone po implementacji pierwszego pakietu**. Ich dziewięć nowych externali i opis starego `AI_Function` nie są już aktualnymi brakami. Listy hooków/adresów nadal służą jako backlog do weryfikacji. Liczba miejsc wywołania nie oznacza procentu zgodności.

## 3. Co jest potwierdzone w grze

| Obszar | Dowód / ograniczenie |
|---|---|
| Nowa gra wchodzi do świata | Początkowy powrót z loadingu do menu naprawiony. Użytkownik następnie dochodził do rozmów i wykonywał komendy. Nie jest to pełne przejście intra. |
| B: synchronizacja czekania | Użytkownik potwierdził blokadę bohatera na około 4 s i odblokowanie. Początkowo nie widział komunikatu; poprawiono jego pozycję, ale brak osobnego potwierdzenia ponownej próby B. |
| C: anulowanie oczekiwanej kolejki | Użytkownik: działa. |
| D: zapis w trakcie czekania i restart | Użytkownik: działa według przekazanej procedury. Nie rozszerzać tego na kompletność PermMem/FF/wszystkich slotów. |
| E: wyrzucenie przedmiotu przez AI | Użytkownik: działa. |
| F: dodanie przedmiotów do skrzyni | Użytkownik: działa. Nie mamy protokołu wszystkich wariantów kontenerów/questów. |
| G: przygotowany strzelec | Ostatnie ogólne potwierdzenie użytkownika obejmuje tę próbę. Zachowany log pokazuje kuszę: `ammo=19, projectiles=1`, `created=1, ammo spent=1`, `PROJECTILE CONFIRMED`. Nie potwierdza trafienia, balansu ani widoczności modelu. Wcześniejsze próby z tego uruchomienia pokazywały także 0/0 i wyjątki VM. Osobny pełny wynik łuku nie został opisany. |
| H: oryginalna scena palaczy | Po poprawkach przygotowania i lądowania użytkownik potwierdził, że działa. Obejmuje scenę uruchomioną komendą z wymuszonymi warunkami; nie potwierdza naturalnej dostępności na wszystkich etapach kampanii. |

Niepotwierdzone osobno:

- A: `comcheck text` po poprawce położenia i numerowaniu kolejnych prób. Pierwotny wynik był niepewny; „oba działają” interpretujemy jako G/H, nie wszystkie A–H.
- Wpisywanie imienia psa po poprawce Ctrl; brak wyraźnego wyniku ponownej próby. Polskie znaki pozostają nieobsługiwane w tym polu.
- Wszystkie sceny Q205/Q304/Q406, trialogi z trzema i większą liczbą NPC, naturalne questowe `AI_Snd_Play`, `AI_StopFX`, `Wld_RemoveItem`.
- Pełna rozgrywka, wszystkie rozdziały, gildie, zakończenia i zadania poboczne.

## 4. Co zostało zaimplementowane

### 4.1. Dziewięć externali i kolejki

W `common/game/gamescript.*`, `common/world/aiqueue.*`, `common/world/objects/npc.*`, `common/game/constants.h`:

- `AI_WaitTillEnd(npc, other)` — znacznik synchronizacji w kolejce drugiej postaci zamiast naiwnego czekania na całkowite jej opróżnienie. Obsługuje wzajemne oczekiwanie; `clear()` kończy wydane znaczniki, zwalniając czekających.
- `AI_DropItem`, `AI_AimAt`, `AI_ShootAt`, `AI_StopAim`, `AI_Snd_Play`, `AI_StopFX` — rzeczywiste akcje kolejki.
- `Mob_CreateItems(tag, item, amount)` — dodawanie do kontenerów o pasującym tagu, porównanie bez rozróżniania wielkości liter.
- `Wld_RemoveItem(item)` — usuwa luźny przedmiot ze świata, nie element inwentarza; zwraca `int`: 1 po usunięciu, 0 bez usunięcia.
- Celowanie/strzał wymagają wyciągniętej broni dystansowej; strzał tworzy pocisk i zużywa amunicję. Poprawiono odwrócony warunek końca `implTurnTo`. `AI_StopAim` czyści także cel, aby po opróżnieniu kolejki nie uruchomić samoczynnej walki.

Pułapka: pierwsza rejestracja `Wld_RemoveItem` jako `void` powodowała `external WLD_REMOVEITEM has illegal return type 'void', expected 'int'` i powrót do menu. Jest już naprawiona. Nie przywracaj `void`.

Nowe identyfikatory akcji **dopisano na końcu**, zachowując numery starych. `Serialize::Current` zmieniono z 55 na **56**; zapis przechowuje liczniki synchronizacji. Starszy format jest obsługiwany warunkowo. Nie otwieraj nowych zapisów starym EXE; nie zmieniaj numeracji istniejących akcji.

Z dziesięciu wywoływanych externali wykrytych w audycie nadal pozostał `WLD_INSERTOBJECT`.

### 4.2. `AI_Function` i VM

`GameScript::eventPlayAni` przekazuje wykonującego `Npc&` do `DirectMemory::eventPlayAni`.

Parser `CALL ...` obsługuje formaty `I/N/S`, także `NSII`, puste stringi, ucieczki `STR_Escape`, kodowanie bitowe floatów według sygnatury. Waliduje argumenty przed modyfikowaniem stosu. Ustawia `SELF` na wykonującego NPC, zeruje `OTHER`/`ITEM`, ustawia kontekst instancji VM i odtwarza poprzednie wartości przez RAII. Wynik funkcji jest zdejmowany zgodnie z typem.

W ZenKit dodano `DaedalusVm::invoke_function(sym)` dla argumentów już na stosie: dispatch externali, nadpisanych funkcji lub normalnego bytecode'u. Używają go `AI_Function` i `debug.call`. **Istniejącego `unsafe_call` nie zmieniano globalnie.** Przy diagnozie obecnych wyjątków stosu sprawdź także kontrakt nowej metody, ale nie zakładaj z góry, że to ona jest przyczyną.

### 4.3. Pole tekstowe i dotyk

- Obsługa odpowiedzi `?Name` i `INFOMANAGERANSWER` była już w HEAD.
- `Controls::cancel()` zachowuje oczekujące zdarzenia zwolnienia klawiszy. Wcześniejsze kolejne zmiany układu mogły zgubić puszczenie akcji/Ctrl, a pole tekstowe odrzucało litery.
- Syntetyczne klawisze dotykowe używają odrębnych identyfikatorów zamiast kolidować ze scancode'ami fizycznej klawiatury.
- Zamknięcie dialogu resetuje prompt; log pokazuje modyfikatory i długość tekstu.
- Nadal tylko ASCII 32–126, limit 24 bajtów, Backspace usuwa bajt. Potrzebna właściwa obsługa polskich znaków i kodowania tekstu Gothica.
- W checkoutcie istnieje blokowanie wyborów do końca kolejek NPC z limitem `QUEUE_WAIT_MAX=10000`. To dodatkowy mechanizm UI do oceny: arbitralne 10 s może być niewłaściwe w długich scenach. Nie utożsamiaj go z poprawną semantyką `AI_WaitTillEnd`.

### 4.4. Narzędzia ręcznych prób i teleport

`mods/devtools/lib/archoloschecks.js`, rejestracja w `mods/devtools/index.js`:

```text
comcheck text
comcheck wait [sekundy]
comcheck cancel
comcheck drop [instancja]
comcheck chest [vob] [przedmiot] [liczba]
comcheck ranged [bow|crossbow|aim|shoot|stop|status|clear]
comcheck scene [smokers]
```

Komendy nie wykonują się automatycznie przy starcie moda. Teksty A–C są teraz centrowane na 35% wysokości, 10 s, z numerem próby; wcześniej `100,100` umieszczało je poza ekranem.

Strzelec: oryginalny `NONE_34813481_TREETESTER`, bez rutyny/percepcji, z wyposażeniem, amunicją i włączonym god mode gracza do `ranged clear`. Używa zapamiętanego NPC, nie najbliższego przechodnia. Po aim/shoot dodaje 120 s czekania, aby zwykła walka nie dopisywała strzałów. Nowe polecenie czyści poprzednią kolejkę — trzeba zamknąć F10 i dać wykonać się poprzedniej akcji.

`Npc::rangedShotCount` / JS `npc.rangedShots`: licznik rośnie po faktycznym utworzeniu pocisku. Nie jest serializowany; nie mierzy trafień i nie pobiera wspólnego `world.takeShots()` używanego przez co-op.

Scena H: `BAU_13324_SMOKER`, `BAU_13323_SMOKER`, `DIA_13324_SMOKER_OHFUCK`, opóźnione `DIA_13324_SMOKER_RUN`. Przygotowuje Silbach, 22:00, historię rozmów, flagi i rutyny; tworzy brakujących NPC. **Zmienia stan świata i nie ma pełnego undo** — po próbie użytkownik wraca do pierwotnego zapisu.

Dodane API debug: `debug.symbol().index`, `debug.containers`, `debug.parkNpc`, `debug.setInfoKnown`, `debug.placeOnGround`. Ten ostatni wyznacza podłoże, sprawdza nachylenie i kolizje, szuka miejsca w promieniu 2 m, zeruje kolejkę i spadanie, resetuje kamerę. Zwraca miejsce lądowania albo null. Komenda wypisuje `Ground landing`.

Naprawiono dwie pułapki przygotowania sceny:

1. `Npc::setPosition` zwracał false również dla „już stoi tutaj”; bindingi JS `setPosition`/`teleport` traktują teraz to jako powodzenie. Właściwa metoda silnika zachowała swój kontrakt.
2. Przesuwanie X/Z z pozostawioną wysokością waypointa powodowało teleport pod mapę. Scena korzysta teraz z `placeOnGround`, resetuje ruch i kamerę. Użytkownik potwierdził poprawkę.

Build osadza Developer tools w `builtin_mods.cpp` i kopiuje `mods` obok EXE. Po ostatnim buildzie kopia `archoloschecks.js` miała ten sam hash co źródło.

## 5. Działające elementy zastane, których nie implementować ponownie od zera

Statycznie istnieją: emulowana pamięć 32-bitowa, mapowanie symboli/instancji, pętle Ikarusa, dynamiczne wywołania skryptów, część matematyki i stringów, LeGo FrameFunctions, podstawowe widoki/fonty/teksty, movery, część hooków gry. To nie jest dowód kompletnej poprawności tych podsystemów.

Mosty obejmują m.in. podnoszenie przedmiotu, zmianę atrybutów, obrażenia/upadek, klawisze, strefę muzyki, zmianę świata, część zamków/magii, statystyki i kamerę.

**Pliki LeGo już istnieją:** `CreateFileA`, `ReadFile`, `WriteFile`, `CloseHandle`, `GetFileSize`, `GetLastError` działają na plikach wirtualnych osadzonych w zapisie `game/dma`. Nie zgłaszaj ich ponownie jako brakujących na podstawie heurystycznego skanera.

`tools/ikarus/analyze.py` i `docs/IKARUS.md` są pomocnicze, częściowo nieaktualne. Skaner nie rozpoznaje wszystkich rejestracji przez wrappery i nie odróżnia pustego handlera od poprawnej implementacji.

## 6. Najpierw: faktyczne problemy z ostatnich logów

Zachowane kopie, aby start gry ich nie nadpisał:

- `C:/Users/Ernest/Desktop/opengothic/docs/archolos-audit/runtime-2026-10-20/log.txt`
- `C:/Users/Ernest/Desktop/opengothic/docs/archolos-audit/runtime-2026-10-20/crash.log`

Oryginały miały modyfikację odpowiednio 20.10.2026 00:35:34 i 00:35:35; EXE 00:32:08 (czas lokalny odczytany z plików). Agent ich nie wygenerował własnym uruchomieniem.

### 6.1. Wyjątki VM przy pasku doświadczenia / LeGo

Powtarza się:

```text
Internal Exception: tried to pop_int but frame does not contain a int.
BAR_DELETE at 1acd8
FREE at e392
BAR_CREATE at 1bd83
STATUSSCREEN_CREATEEXPBAR at 2b157
FRAMEFUNCTIONS at 14b16
FOREACHHNDL at e151
_FF_HOOK at 14a68
stack: [REFERENCE] FREE.H[0] = ...
```

To rzeczywisty otwarty błąd, nawet gdy strzał później dochodzi do skutku. Zdiagnozuj stos, wywołania/destruktory PermMem i typy uchwytów. Nie maskuj błędu zerowym wynikiem ani wyłączeniem całego paska. Późniejsze pojedyncze wyjątki bez stosu występują także między próbami G; nie przypisuj im automatycznie przyczyny w strzelaniu.

### 6.2. Crash przy zamykaniu

Ostatni `crash.log`: SIGSEGV, `AndroidInput::Controller::~Controller()` → `MainWindow::~MainWindow()` → `main`. Do sprawdzenia kolejność niszczenia zasobów, callbacków, urządzenia i kontrolera. Log nie dowodzi jeszcze konkretnej przyczyny. Nie ignoruj tego tylko dlatego, że rozgrywka działa do zamknięcia okna.

## 7. Wszystko znane, co pozostaje do pełnej grywalności

### P0: stabilna pamięć i obiekty

- `InstanceHandles` nadal ma pierścieniowe 4096 slotów: po zapełnieniu nadpisuje slot żywego obiektu. Zastąpić stabilną tożsamością, zarządzaniem życiem i obsługą nieaktualnych adresów; rozważyć trwałość między zapisami/światami.
- `Mem32::readInt/writeInt` sprawdzają start regionu, ale nie pełny zakres czterech bajtów. Poprawić granice, przepełnienia i semantykę callbacków. Zbadać także inne dereferencje/copy/free.
- `OCNPC.FOCUS_VOB` jest aliasem `C_NPC.voice_pitch`; eventFocus nadal nadpisuje prawdziwe pole skryptowe. Rozdzielić dane zgodności od VOICEPITCH.
- `OCNPC.INTERACTMOB` aliasuje `spawn_delay` i nie dostarcza rzeczywistego moba; `BITFIELD` aliasuje `mission` bez pełnego wpływu na ruch/stany. Potrzebne jawne adaptery.
- Odtworzyć używane pola `_ZCVOB_*`, `MAG_BOOK`, modelu, wyglądu, wyposażenia. Zapis macierzy rotacji ma obracać rzeczywisty obiekt, nie tylko pamięć emulowaną.
- Osobno uwzględnić odczyt członków VM i surowe `MEM_ReadInt(ptr+offset)` — jedno nie zapewnia drugiego. Ocenić również życie uchwytów mobów i odtwarzanie referencji po usunięciu obiektu.

### P0: czas, świat, wyszukiwanie i tworzenie obiektów

- `WLDTIMER`, `WAYNET` nadal `BAD_BUILTIN_PTR`; `TIMESTEP=1`, `zTimer` bez poprawnej aktualizacji klatkowej. Zaimplementować czas świata, klatki, pauzę, spowolnienie, zmianę czasu i poziomu.
- Obsłużyć `SCALETIME`/`CHANGETIMESPEED` w rzeczywistym silniku. Osobna obsługa `MEM_GetSystemTime` nie rozwiązuje tych pól.
- Potrzebne listy waypointów i ogólna lista vobów; obecnie odświeżana jest głównie lista NPC.
- `MEM_SearchVobByName` wyszukuje movery; rozszerzyć na używane NPC, przedmioty, skrzynie, dekoracje i inne obiekty, zgodnie z kontraktem.
- Zaimplementować `WLD_INSERTOBJECT`, rzeczywiste `RemoveVob`, `SetVobName`, poprawne utworzenie/usunięcie/przeniesienie questowego obiektu.

### P0: hooki questów, kompanów i działań gracza

- Ochrona ważnych przedmiotów przed upuszczeniem i sprzedażą: `OCNPC__EVDROPVOB_HOOK`, `ITEMIMPORTANTFLAG_CHECKSELLINGITEMS`.
- Wybór przeciwnika i kompanów: `OCNPC__GETNEXTENEMY_REPLACEHOOK`, pięć miejsc `HIDEFOCUSFORPARTYMEMBERS_*`.
- Utrata przytomności: `OCNPC__DROPUNCONSCIOUS_HOOK`, w tym `Q205_ONDROPUNCONSCIOUS`, `Q306_ONDROPUNCONSCIOUS`.
- Dobijanie: `EV_FINISHINGMOVE_HOOK`; blokada ekwipunku `LOCKINVENTORY`; zamknięcie inwentarza i stan ukończonej rozmowy `SETDIALOGUETOLDPROPERTY`.
- Hook zmiany rutyny `OCNPCSTATES_CHANGEROUTINE_HOOK` i semantyka odświeżania NPC.
- Zbieranie pod wodą `OCAIHUMAN__PC_DIVING_HOOK`.
- Odtwarzać skryptową decyzję **przed** sprzedażą, dropem, walką lub utratą przytomności, nie tylko emitować informację po operacji.

### P0/P1: zamki i zaklęcia

- Istnieją CanOpen i SpellSetup, ale brak `HANDLE_SETPROPERPICKLOCKSTRLEN`, `SPELL_PICKLOCK_FOCUS` i rzeczywistego `ClearKeyBuffer`.
- Losowanie kodów wymaga list vobów; zapis `PICKLOCKSTR` ma zmieniać prawdziwy zamek.
- Sprawdzić skracanie kodu przez talent/bonus, aktualnego moba, klucze, wymagania i cel zaklęcia. Osobne próby wytrychów i magii.
- Uzupełnić `OCNPC_ISSPELLACTIVE`, spowolnienie oraz adaptery stanów/bitów używanych przez czary, telekinezę i nurkowanie.

### P1: sloty, ręce i wyposażenie

- `GetSlotItem -> 0`, `EquipWeapon` nop nadal obecne. Brak PutInSlot, RemoveFromSlot, DropFromSlot, Equip, UnequipItem, EquipBestWeapon, DropAllInHand.
- `INIT_QUIVERS_ALWAYS` jest świadomie wyłączone; odtworzyć kołczany i hooki equip/unequip.
- Obsłużyć pochodnie, zajętość rąk, ich odtworzenie po wczytaniu, wybór najlepszej broni i rzeczy po utracie przytomności.
- Nie zakładać, że działający test kuszy dowodzi poprawności całego natywnego wyposażania i wszystkich wrapperów LeGo.

### P1: sceny, trialogi, kamera i obrażenia

- Zweryfikować co najmniej scenę trzech NPC z TRIA_WAIT, przedmiotem i ruchem; długie/zagnieżdżone AI_Function, zanik NPC, anulowanie i przerwanie sceny.
- Uzupełnić StartDialogCam i sprawdzanie globalnej cutscenki przez HUD, ruch i menu. Blokady wejścia i powrót kamery muszą odpowiadać skryptom.
- Wyjaśnić natywną semantykę `0x688a70` w Q204/Q304, a następnie wdrożyć właściwe kolizje/chasm detection; nie wnioskować wyłącznie z nazw funkcji questowych.
- Sprawdzić i uzupełnić aktywne animacje, ładowanie visuali oraz zatrzymywanie aktywnych dźwięków.
- Descriptor obrażeń jest niepełny; `FDAMAGETOTAL` traktowane jako dodatni bonus wymaga porównania z kontraktem. Zweryfikować pancerz, penetrację, pierścienie, strzały, magię, upadek i nieśmiertelność.
- Sceny do ręcznego odbioru: Q204, Q205, Q304, Q306, Q406, Q602/GQ001; Wld_RemoveItem w KQ402/SQ202; AI_StopFX w SQ509; naturalne AI_Snd_Play. Potrzebne komendy przygotowujące sceny dla użytkownika, analogicznie do H, jeżeli nie ma zapisów.

### P1: crafting, ekwipunek, handel i wpisywane odpowiedzi

- Odtworzyć potrzebną semantykę `CATINV_*`, list kontenerów, kategorii, zmiany stron, przenoszenia/stosów, resetu po śmierci i blokad zamknięcia.
- Część hooków oryginalnego UI można zastąpić równoważnym zachowaniem natywnego UI OpenGothic; trzeba to udokumentować i sprawdzić decyzje skryptów.
- Uzupełnić `OCINFORMATIONMANAGER__UPDATE_HOOK`, `CRAFTINGVIEW_UPDATE`, `ZCVIEWDIALOGCHOICE__HANDLEEVENT_HOOK`, `OCITEMCONTAINER__DRAWITEMINFO_HOOK`.
- Receptury: warunki, wybór, składniki, wynik, liczba sztuk, koszt, brak zasobów i anulowanie. Handel: ważne przedmioty, ceny i transfer.
- Powtórzyć imię psa i nazwę wina; dodać polskie znaki z właściwym kodowaniem fontów/VM, poprawne kasowanie i limity. Sprawdzić klawiaturę fizyczną, ekranową, Escape i powrót do dialogu.
- Zweryfikować 10-sekundowy limit UI oczekiwania na kolejki; nie dopuszczać odpowiedzi przed zakończeniem czynności skryptowych.

### P0/P1: trwałość gry

- Rozszerzyć odbiór zapisu poza potwierdzony D: PermMem, destruktory, FF, widoki, ciągi, referencje NPC/przedmiotów i aktywne efekty czasowe.
- Pełny restart procesu oraz wielokrotne loady w tej samej sesji; wiele slotów i quicksave.
- Zmiana świata i powrót, usuwanie/odtwarzanie obiektów, brak wiszących wskaźników.
- `ReadSavegame/_BR_SETSELECTEDSLOT` jest deklarowany jako obsługiwany, lecz callback nie jest emitowany. Ustawienie slotu ma osobną drogę; ustalić brakującą semantykę i poprawić prawdomówność raportu.
- Oceniać zgodność starych zapisów i wersji 56 po każdej zmianie serializacji; nie dodawać pól bez wersjonowania. Wirtualne pliki LeGo mają pozostać częścią konkretnego slotu OpenGothic.

### P1/P2: KmLib, muzyka, statystyki i osiągnięcia

- `LoadLibrary/GetProcAddress` dają pozorne uchwyty; Cpu32 pomija wykonanie tych eksportów. Potrzebny jawny adapter używanych funkcji KmLib.
- `InitializeMenu`, `InitializeGamestart`, `InitializeAlways`.
- `MusicSystem_OverrideTrack`, `MusicSystem_DisableTrackOverride` i przywrócenie muzyki po scenie/loadzie.
- `GameServices_UnlockAchievement`, `GetStat`, `IncrementStat`, z deterministycznym wynikiem i trwałością. Osiągnięcia same nie muszą blokować fabuły, ale nie są już „pełną zgodnością”, dopóki nie działają.
- Obsługiwać potrzebne eksporty, nie ładować dowolnych Windows DLL ani nie budować pełnego procesu Gothic2.

### P2: rysowanie, wygląd świata i ustawienia

- Sprite, Render/Draw3D, używane operacje renderera, zagnieżdżone widoki, animacje okien, alpha/kolor tekstu.
- Rozmiar ekranu widziany przez skrypty nadal 800×600, zdarzenie rozdzielczości tylko raz: poprawić dla zmiany okna/skali/HUD.
- `zCTexture::Load -> 0`; funkcje APPLYHOUSEWALLTEXTURE i zmiana wyglądu domu.
- GetFontName, informacje o przedmiotach, status, boss UI, crafting UI, dziennik `LOG_MOVETOTOP` (nadal logowany jako niezaimplementowany).
- ApplySomeSettings / skalowanie widzenia / przezroczystość ekwipunku / tekstury ładowania.
- Konsola CC, autouzupełnianie, clipboard, diagnostyka/telemetria: ocenić priorytet i potrzebne odpowiedniki, zamiast traktować każdy WinAPI call jako bloker głównej fabuły.

### P0/P2: Cpu32, PermMem i uczciwa diagnostyka

- Zaimplementować potrzebne `qsort` i `FINAL`; pełny zakres destructors/locals zbadać wraz z wyjątkiem FREE/BAR_DELETE.
- Brakujące instrukcje x86/adresy obsługiwać według rzeczywistego użycia DAT. Nieznane wywołanie nie powinno pozostawiać przypadkowego starego wyniku jako sukcesu.
- Nie wystarczy pamięć pod patch kodu oryginalnego EXE: efekt patcha trzeba odwzorować w OpenGothic.
- Raport „hook obsługiwany” ma oznaczać rzeczywistą semantykę i emisję zdarzenia, ewentualnie jawnie wskazany równoważny adapter.

## 8. Kolejność następnych pakietów

1. Zachowaj stan i logi; zdiagnozuj wyjątek `FREE/BAR_DELETE` oraz crash zamykania. Oddzielić te dwie przyczyny. Po poprawce dać użytkownikowi krótką próbę statusu/F10, powtarzanych strzałów i zamknięcia gry.
2. Stabilne uchwyty i granice Mem32, odseparowanie focusu/VOICEPITCH; to fundament dalszych adapterów i trwałych zapisów.
3. Czas świata, waypointy/listy vobów oraz faktyczne operacje obiektów, w tym Wld_InsertObject.
4. Hooki wpływające na postęp: ważne przedmioty, kompanów, nieprzytomność, dobijanie, blokady inwentarza i zamki.
5. Sloty/ręce/pochodnie/kołczany, magia i questowe kolizje; sceny trialogów i kamera. Z każdym pakietem konkretny scenariusz przygotowywany komendą, bez samodzielnego uruchamiania.
6. Pełna trwałość PermMem/FF/referencji i round-trip między światami; poprawki serializacji przed szerokim przejściem kampanii.
7. Crafting, handel, kategorie i polskie odpowiedzi tekstowe; następnie pozostałe UI, muzyka/KmLib, dom i prezentacja.
8. Ręczna regresja kampanii wszystkich rozdziałów, dróg gildyjnych, zakończeń i zadań pobocznych, z listą zapisów odbiorowych i porównaniem do oryginalnej gry w tej samej wersji danych.

Nie podawaj procentu ukończenia na podstawie liczby handlerów. Za „w pełni grywalne” można uznać dopiero grę bez znanych blockerów fabuły i utraty zapisów, po ręcznym sprawdzeniu mechanik i reprezentatywnych ścieżek kampanii; pełna zgodność prezentacji/usług wymaga także obszarów P2.

## 9. Minimalny prompt startowy dla Claude

> Kontynuuj implementację zgodności Kronik Myrtany w `C:/Users/Ernest/Desktop/opengothic`. Przeczytaj `docs/ARCHOLOS_HANDOFF_CLAUDE.md`, `docs/ARCHOLOS_HANDOFF_BACKLOG.md`, aktualny diff i lokalny diff submodułu ZenKit. Zachowaj wszystkie niecommitowane zmiany. Użytkownik sam wykonuje testy: nie uruchamiaj gry, testów ani comcheck; rób buildy podaną komendą i pisz dokładne próby ręczne. G/H są potwierdzone przez użytkownika. Najpierw zbadaj zapisany wyjątek VM BAR_DELETE/FREE/STATUSSCREEN_CREATEEXPBAR oraz osobny crash AndroidInput::Controller przy zamykaniu, potem realizuj backlog w małych pakietach. Historyczne CSV nie uwzględniają dziewięciu zaimplementowanych externali i poprawki AI_Function. Nie kończ na planie — wdrażaj kolejny uzasadniony pakiet, buduj i oddaj użytkownikowi do ręcznej weryfikacji.

## 10. Mapa plików do kontynuacji

Wszystkie ścieżki poniżej względem `C:/Users/Ernest/Desktop/opengothic`:

| Obszar | Pliki |
|---|---|
| Wyjątki stosu i dispatch VM | `lib/ZenKit/src/DaedalusVm.cc`, `lib/ZenKit/include/zenkit/DaedalusVm.hh`, `common/game/compatibility/directmemory.cpp`, `cpu32.cpp` |
| Życie uchwytów i granice pamięci | `common/game/compatibility/instancehandles.*`, `mem32.*` |
| Pola silnika, focus, movery, moby, zegary | `common/game/compatibility/directmemory_engine.cpp`, `directmemory_events.cpp`, `directmemory.cpp`, struktury w `compatibility/` |
| Widoki i tekst | `common/game/compatibility/directmemory_ui.cpp`, `common/mainwindow.cpp`, `common/utils/gthfont.cpp` |
| Externale, semantyka skryptów, historia dialogu | `common/game/gamescript.cpp`, `gamescript.h` |
| Kolejki, akcje i serializacja | `common/world/aiqueue.cpp`, `aiqueue.h`, `common/world/objects/npc.cpp`, `npc.h`, `common/game/constants.h`, `serialize.h` |
| Prompt, wybory i oczekiwanie UI | `common/ui/dialogmenu.cpp`, `dialogmenu.h` |
| Ctrl / dotyk / crash zamykania | `common/ui/mobilecontrolsmodel.cpp`, `common/utils/androidinput.cpp`, `common/mainwindow.cpp` i niszczenie jego pól |
| Przygotowanie scen i diagnostyka | `common/scripting/game/game_debug.cpp`, `game_npc.cpp`, `game_world.cpp`, `mods/devtools/lib/archoloschecks.js`, `mods/devtools/index.js` |
| Zastana diagnostyka animacji | `common/graphics/mesh/animationsolver.cpp`, dodatkowe logi w GameScript/Npc/DialogMenu — zachować cudze zmiany |
| Pakowanie modów do buildu | `cmake/embedmods.cmake`, `CMakeLists.txt`, wygenerowany `build-touch/builtin_mods.cpp` — nie edytować wygenerowanego pliku ręcznie |

Następny agent powinien porównywać imdzialaplementację z lokalnym DAT i kodem Ikarus/LeGo z dostarczonych materiałów. Nie rozszerzać zmiany na dowolne mechanizmy DLL/x86 bez związku z faktycznie używanymi ścieżkami Kronik.

## 11. Postęp Claude, pakiet 1 (20.10.2026, bez commitu, bez testów w grze)

Wykonane i zbudowane (touch + plain), czekają na ręczną weryfikację:

1. **Wyjątek `BAR_DELETE` / `FREE`** — przyczyna: `free(h, inst)` z LeGo wywołuje `<klasa>_DELETE` z instancją na stosie, a `Bar_Delete(var int bar)` ma parametr `int`. Oryginalny silnik przekazuje wtedy indeks symbolu instancji; ZenKit rzucał wyjątek w `pop_int` (zdejmując ramkę). Poprawka lokalna w submodule: `DaedalusVm::pop_int` zwraca `symbol_index()` instancji (0 dla pustej). To zmiana w ZenKit (niecommitowana, razem z `invoke_function`).
2. **Crash zamykania** (`AndroidInput::Controller::~Controller`) — nie znaleziono dowodu przyczyny (brak symboli). Wdrożone środki zapobiegawcze: `Controller::releaseIcons()` oraz oddanie kopii sprite'ów z `InventoryMenu` w `~MainWindow` przed niszczeniem pól (atlas żyje). Dodatkowo zlikwidowano realną korupcję sterty w `Mem32` (patrz 3). Jeżeli crash się powtórzy, potrzebny jest symbol z offsetem.
3. **`Mem32::readInt/writeInt`** — odczyt/zapis 4 bajtów na końcu regionu wychodził poza bufor (korupcja sterty). Teraz granica jest sprawdzana, a dostęp przechodzący przez koniec bloku idzie bajt po bajcie (`readByte/writeByte`); porównania `address+size` w `deref` i `implTranslate` w 64 bitach.
4. **`InstanceHandles`** — 8192 sloty zamiast 4096; wolny slot albo slot obiektu, który nikt już nie trzyma (`use_count()==1`), od najstarszego. Dopiero gdy wszystkie sloty należą do żywych obiektów, najstarszy jest nadpisywany (log błędu).
5. **`OCNPC.FOCUS_VOB` / `INTERACTMOB`** — mają własne pola `compat_focus_vob` i `compat_interact_mob` w `zenkit::INpc` (ZenKit), więc nie nadpisują `VOICEPITCH`/`SPAWNDELAY`. `INTERACTMOB` gracza jest odświeżany co tick z rzeczywistego moba.
6. **`OCNPC__EVDROPVOB_HOOK`** (0x7538C0) — `Npc::dropItem` gracza woła hook przed upuszczeniem; adres 0x763110 (`SetDeleted(1)` wiadomości) anuluje upuszczenie. Ważne przedmioty (flaga `ITEM_IMPORTANT`) nie wypadają, bohater gra „nie”.

Dalej według sekcji 8: czas świata/waypointy, pozostałe hooki postępu (sprzedaż ważnych przedmiotów wymaga bytecode'u DAT — brak źródła `ITEMIMPORTANTFLAG_CHECKSELLINGITEMS`), sloty/ręce/kołczany.
