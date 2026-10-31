# Safe GPU Mode, shader chmur na Mali i diagnostyka zapisów

## Crash `clouds_lut.comp` na Mali-G57

`vkCreateComputePipelines` kończyło się `SIGSEGV` w `libGLES_mali.so` przy `clouds_lut.comp.sprv`.
Oryginalny shader wywołuje `textureQueryLevels()` na samplerach przekazanych do funkcji jako argumenty
(`in sampler2D`). To jedyne zapytanie o stan samplera w tym shaderze i najbardziej prawdopodobny winowajca; **nie
potwierdzono tego na urządzeniu** (to wymaga Mali-G57), więc dodano obejście niezależne od potwierdzenia przyczyny.

- `shader/sky/clouds_lut_safe.comp` ma te same bindingi (0, 5, 6, 7, 8), ten sam rozmiar grupy i ten sam wynik.
  `textureQueryLevels()` zwraca liczbę poziomów, więc `textureLod(..., liczba_poziomów)` zawsze trafia poza ostatni
  poziom i sampler ogranicza go do ostatniego. Stała `20.0` (więcej poziomów niż ma jakakolwiek tekstura) daje ten sam poziom.
  Samplery nie są przekazywane do funkcji.
- `Shaders::cloudsLutShader()` wybiera wariant: każdy GPU z „Mali” w nazwie oraz tryb Safe GPU używają wariantu
  bezpiecznego, pozostałe GPU zostają przy oryginale. Jeśli sterownik odrzuci oryginał wyjątkiem, używany jest wariant bezpieczny.
- `try/catch` nie łapie `SIGSEGV`, dlatego crash sterownika jest zapamiętywany przez pliki (niżej).

## Safe GPU Mode (`common/graphics/safegpu.*`)

Pliki w katalogu roboczym gry (na Androidzie `getFilesDir()`, obok `log.txt` i `crash.log`):

| plik | znaczenie |
| --- | --- |
| `safegpu.lock` | istnieje, gdy sterownik kompiluje shadery; zawiera nazwę GPU i **nazwę ostatniego shadera** przekazanego sterownikowi |
| `safegpu.mode` | poziom trybu (`level=`), GPU (`gpu=`) i shader, który zabił poprzedni start (`shader=`) |

Przebieg:

1. Start: `SafeGpu::init()` czyta oba pliki. Jeśli `safegpu.lock` został z poprzedniego startu dla tego samego GPU, poprzedni
   start zginął w kompilatorze sterownika: poziom rośnie z 0 do 1 (warianty bezpieczne) i jest zapisywany w `safegpu.mode`.
2. Poziom 2 (`Minimal`): kolejny crash przy włączonym poziomie 1, potwierdzony świeżym `crash.log`, wyłącza też opcjonalne
   pipeline'y (bindless, ray tracing, mesh shadery, virtual shadows, software shadows).
3. Po udanej kompilacji wszystkich shaderów `safegpu.lock` jest usuwany. Kompilacja pipeline'ów materiałów w trakcie gry
   także zakłada blokadę.
4. Tryb jest przypisany do nazwy GPU. Inny GPU startuje w trybie normalnym.

Uwaga: zabicie aplikacji przez użytkownika w trakcie pierwszej kompilacji też zostawia blokadę i włącza poziom 1. Poziom 1 nie
zmienia obrazu (wariant `clouds_lut_safe` daje ten sam wynik), a poziom 2 wymaga potwierdzenia przez `crash.log`.

Parametr `-safegpu auto|off|on|minimal` (domyślnie `auto`). `off` usuwa oba pliki. Ręcznie: usuń `safegpu.mode`.

## Zapisy: diagnostyka i walidacja

Komunikat `[zenkit] 1 bytes overflowed in section f590` **nie dotyczy zapisów**. `f590` to `MODEL_TAG` w binarnym skrypcie
modelu (`.MSB`) czytanym przez ZenKit (`ModelScript.cc`); parser zeruje kursor na koniec sekcji, więc to ostrzeżenie
jest nieszkodliwe. Taki sam wiersz jest w `docs/archolos-audit/runtime-2026-10-20/log.txt`, z grą działającą poprawnie.

`unable to read save-game file` oznaczało, że odczyt wyszedł poza dane bieżącego wpisu ZIP albo wpisu w ogóle nie było.
Stary kod nie sprawdzał wyniku `mz_zip_reader_extract_file_to_mem` (uszkodzony wpis zamieniał się w zera lub w
niezainicjowany bufor), a brak wpisu dawał pusty bufor i anonimowy wyjątek. Teraz:

- `Serialize::implSetEntry()` sprawdza rozmiar, rozpakowanie i CRC; błąd zawiera nazwę wpisu, indeks, rozmiar,
  rozmiar spakowany i błąd miniz;
- `readBytes()` i walidacja liczników (`checkCount`) podają etap (`setStage`), wpis, offset, ile bajtów żądano i ile zostało,
  a zapis loguje to jako `save-game: ...`;
- `requireEntry()` dla wpisów, które zawsze są zapisywane (`header`, `game/session`, `game/quests`, `game/daedalus`,
  `game/camera`, `worlds/<świat>/{world,version,items,triggerEvents,routines}`, `npc/N/{data,visual}`);
- wersja zapisu spoza `Serialize::MinVersion..Current` jest odrzucana z komunikatem (zapis z nowszego programu);
- błąd ładowania trafia do `Gothic::loadError()`, a okno pokazuje go na ErrorScreen i wraca do menu głównego;
  wątek ładowania łapie także wyjątki spoza `std::exception`.

## Zamknięcie gry (`SIGABRT` w `MainWindow::~MainWindow`)

`setupUi()` dodaje `errorScreen` (składowa okna, nie obiekt na stercie) jako dziecko, a destruktor nie odpinał go przed
`removeAllWidgets()`. Ta funkcja woła `delete` na każdym pozostałym dziecku, więc `delete` na składowej kończył się
`SIGABRT` (nieprawidłowe zwolnienie pamięci). Destruktor odpina teraz `errorScreen`, jak pozostałe widgety.

## Crash kompilatora Adreno (610, 619, 650, 730) w `drawHiZ`

Logi z telefonów: `SIGSEGV` w `libllvm-glnext.so` / `libllvm-qgl.so` (`CreateQGLCShader`, `QGLCCompileToIRShader`) w
`vkCreateGraphicsPipelines`, wołanym z `DrawCommands::drawHiZ` → `VPipeline::instance`, przy wejściu do świata. Pipeline'y
materiałów są kompilowane przez sterownik dopiero przy pierwszym rysowaniu, a nie przy starcie.

- Wyłączenie bindless (poprzednia próba) **nie pomogło** i zostało cofnięte.
- Kolejność kompilacji w `drawHiZ` (najpierw bez alpha-testu, potem z nim) i numer padającej pary (3. w G1, 4. w G2) wskazują na wariant
  głębi **z alpha-testem** (`main_*_d_at`). **Nie potwierdzono tego na urządzeniu.**
- `shader/materials/main.vert`: wczesny `return` (lane poza zakresem meshletu) zostawiał `bucketIdOut` i `shOut` niezapisane na jednej ścieżce.
  Teraz każda ścieżka zapisuje wszystkie wyjścia, a lane poza zakresem czyta prymityw 0 i dostaje `gl_Position = NaN` (ten sam efekt).
- Log pokazuje teraz pełny wariant: `vkCreateGraphicsPipelines: main.vert [MESH_TYPE=1 DEPTH_ONLY ATEST] main.frag [...]`, a po kompilacji
  `vkCreateGraphicsPipelines: done` (makra czyta Tempest z informacji debugowej SPIR-V).
- Tryb Safe GPU obejmuje teraz także te pipeline'y: `SafeGpu::onLog()` zakłada `safegpu.lock` z nazwą wariantu na czas kompilacji w
  trakcie gry. Po crashu: poziom 1 pomija w `drawHiZ` obiekty z alpha-testem, poziom 2 pomija cały `drawHiZ`. Wynik to mniej okluderów w
  HiZ (mniej odrzuceń, nigdy błędne), nie zmiana obrazu.

## Zapisy: animacje postaci nie blokują wczytania

Log z CoM: `npc/384/visual` — długość napisu 1398755913 to bajty tekstu `IR_S`, czyli odczyt zaczął się w środku napisu. Dokładnego pola
bez pliku `.sav` nie wskazano. Zmiany:

- `AnimationSolver::addOverlay` nie dubluje już tego samego overlayu (skrypty CoM wołają `Mdl_ApplyOverlayMds` przy każdej zmianie ekwipunku;
  nadmiar rósł w zapisie, a jedno `Mdl_RemoveOverlayMDS` zdejmowało tylko jeden). Starsze zapisy są oczyszczane przy wczytaniu.
- `Pose::save` zapisuje liczbę warstw jako bajt: powyżej 255 liczba i dane się rozjeżdżały. Teraz zapisuje najwyżej 255 i liczba jest prawdziwa.
- `AnimationSolver::load` i `Pose::load` czytają wszystko do zmiennych tymczasowych i przejmują dopiero po całym wpisie.
- `MdlVisual::load` łapie błąd odczytu animacji jednej postaci, loguje `save-game: the animations of '<npc>' ... skipped` i wczytuje dalej
  (postać startuje bez animacji, AI/rutyny zagrają następną) zamiast odrzucać cały zapis.

### Adreno 619, log z 0.8.3: crash przeszedł z `drawHiZ` do `drawGBuffer`

`safegpu.lock` z telefonu (OPPO CPH2197, Adreno 619) wskazał `main.vert [MESH_TYPE=0 FLAT_NORMAL] main.frag [FLAT_NORMAL GBUFFER SIMPLE_MAT]`, czyli
`main_lnd_g_s` (G-buffer landszaftu). Przebieg HiZ (wszystkie 4 warianty głębi) po usunięciu wczesnego `return` z vertex shadera **już się kompiluje**, więc
ta poprawka zadziałała. Pozostałe różnice tego pipeline'u: `isFlat()` (dwa `return`, po pochodnych `dFdx/dFdy`) i struktura `Varyings` z trzema
członkami (`vec2`, `vec3`, `vec3`) jako interfejs vertex → fragment. **Nie potwierdzono na urządzeniu**, która z nich jest przyczyną, więc zmieniono obie
(bez zmiany obrazu):

- jedno wyjście z każdej funkcji materiałów: `isFlat`, `waterShading`, `fresnel`, `calcShadow` (dwa przeciążenia), `mapViewport`, `morphOffset`;
  przy NaN zachowanie jest takie samo jak przy wczesnych `return` (porównania zapisane jako `!(x>=1)` i `!(x<=0)`);
- varyingi vertex/fragment jako zwykłe zmienne (`shOutUv`, `shOutNormal`, `shOutPos`, `shOutColor`, `shInUv`...), nie struktura: lokacje są takie, jakie
  zajmowały członkowie struktury (`VLOC_*` w `materials_common.glsl`), więc shadery mesh i tesselacji, które zostają przy strukturze, są zgodne.
  Fragment dostaje `shInp` jako zmienną globalną wypełnianą w `loadVaryings()`, reszta kodu się nie zmienia.

Sprawdzone na hoście: `spirv-val` dla wszystkich 162 shaderów `main_*`, a skrypt porównujący lokacje i typy wyjść vertex z wejściami fragment: 82 pary,
0 niezgodności.

### Cztery logi 0.8.3 z Adreno (610/619/650/730): crash tylko w pipeline'ach G-buffera

Wszystkie cztery logi pochodzą z natywnej biblioteki sprzed poprawki shaderów `3cf285c1` (te same offsety w `CrashLog::dumpStack`). Nazwy z `vkCreateGraphicsPipelines:`
w logach:

| GPU | gra | ostatni pipeline przed crashem |
| --- | --- | --- |
| Adreno 650 (poziom 1) | G1 | `main.vert [MESH_TYPE=0 FLAT_NORMAL]` + `main.frag [FLAT_NORMAL GBUFFER SIMPLE_MAT]` (`main_lnd_g_s`), po dwóch pipeline'ach głębi, które przeszły |
| Adreno 619 (poziom 2) | G1 | `main_lnd_g_s` |
| Adreno 730 (poziom 2) | G2 | `main.vert [MESH_TYPE=1]` + `main.frag [GBUFFER]` (`main_obj_g`) |
| Adreno 619 (poziom 2) | G2 | ucięty log, ten sam typ stosu |

`main_obj_g` nie ma `isFlat()`, więc to nie samo `isFlat` zabija sterownik. Wspólne dla pipeline'ów G-buffera (a niewystępujące w tych, które się kompilują: głębia,
niebo, post-efekty): struktura varyingów z `vec3`, dwa wyjścia koloru (w tym `uint` do R32U), `PackSnorm2x16` w `octahedral_32` i wyjście `uint`.
`3cf285c1` usuwa pierwsze dwie rzeczy (wielokrotne `return` i strukturę); ten krok zastępuje instrukcję `PackSnorm2x16` ręcznym kodem
(`packSnorm2x16_` w `common.glsl`: `roundEven(clamp(v,-1,1)*32767)`, dwa uzupełnienia do 16 bitów, x w dolnej połowie, ten sam wynik). **Nie potwierdzono
na urządzeniu**, która z tych zmian jest przyczyną.

Tryb Safe GPU nic tu nie zmienia, bo nie ma czego pominąć: G-buffer jest potrzebny. Poziom 1 i 2 tylko pomijają HiZ.

### Sześć logów z 31.10.2026 (`Desktop/logs`): wszystkie sprzed poprawek `3cf285c1` i `96ddb583`

| Raport | Wersja | GPU | Miejsce awarii |
| --- | --- | --- | --- |
| `opengothic-discord-raport-1` | 0.8.3 | Adreno 619 (G1) | `vkCreateGraphicsPipelines`, `main.vert [MESH_TYPE=0 FLAT_NORMAL]` + `main.frag [FLAT_NORMAL GBUFFER SIMPLE_MAT]` |
| `OpenGothic log (1)` | 0.8.3 | Adreno 710 (G2) | `main.vert [MESH_TYPE=1]` + `main.frag [GBUFFER]` |
| `OpenGothic logi` | 0.8.3 | Adreno 610 (G1) | `main.vert [MESH_TYPE=3 BINDLESS]` + `main.frag [GBUFFER BINDLESS]`; w logu też `[MESH_TYPE=2] [GBUFFER]` |
| `GOTHIC LOG.pdf` | 0.8.3 | Adreno 660 (G2) | ten sam stos `CreateQGLCShader` w `vkCreateGraphicsPipelines` |
| `GothicLog` | 0.8.3 | Adreno 618 (G2) | **nie kompilator**: `vkCmdDispatch` w `Renderer::buildHiZ`, już w poziomie 2 |
| `gothic2` | 0.8.4 | Mali-G720 (G2) | zamknięcie procesu: `~Resources` → `zenkit::Vfs::~Vfs` (po normalnej rozgrywce) |

- Pięć logów z Adreno to 0.8.3, czyli **przed** zmianami shadera G-buffera (`3cf285c1`, `96ddb583`). Nic w nich nie obala tych poprawek, ale też ich
  nie potwierdza. Do sprawdzenia potrzebny jest nowy APK (0.8.4 ma obie zmiany) na tych samych urządzeniach.
- Mali: crash przy zamykaniu, naprawiony w `7ba334d2` (`std::_Exit` po opróżnieniu logu); log 0.8.4 jest sprzed tej zmiany.
- Adreno 618 pokazuje lukę trybu Safe: blokada (`safegpu.lock`) pilnuje tylko kompilacji pipeline'ów, a awaria w rozkazie `vkCmdDispatch` budowy HiZ jej
  nie zostawia. Poziom 2 pomija tylko rysowanie okluderów, ale budował piramidę HiZ, więc gra padała w kółko.

**Zmiana (31.10):** `SafeGpu::noHiZ()`. Przy starcie `safegpu.cpp` czyta `crash.log`; jeśli zawiera `buildHiZ` i `GPU: <nazwa tego GPU>`, zapisuje `nohiz=1`
w `safegpu.mode`. Wtedy `Renderer` nie rysuje okluderów ani nie buduje piramidy, `DrawCommands` pomija widok `V_HiZ`, a widok główny używa nowego shadera
`visibility_pass_main.comp` (`-DMAIN_VIEW`, bez testu HiZ). Efekt: nieco więcej rysowania, ale bez rozkazu, który zabijał sterownik. `-safegpu off` wyłącza to na
jedno uruchomienie, usunięcie `safegpu.mode` kasuje decyzję (crash.log nadal ją odtworzy, jeśli awaria była w HiZ). **Nie potwierdzono na urządzeniu.**

Nie zmieniałem shaderów G-buffera po raz trzeci: bez urządzenia z Adreno każda kolejna zmiana byłaby zgadywaniem. Jeśli nowy APK nadal padnie w `main.frag [GBUFFER]`,
kolejnym krokiem jest wariant G-buffera bez wyjścia `uint` (R32U) i bez osobnych wariantów vertex, wybierany z poziomu trybu Safe po kolejnej awarii.

### Cztery logi z 31.10.2026 wieczorem (`Desktop/logs/log1..4`): wszystkie wersji 0.8.3

| Log | GPU | Miejsce awarii | Co z tym zrobiono |
| --- | --- | --- | --- |
| `log1` (G1) | Adreno 710 | kompilator, `main.vert [MESH_TYPE=2]` + `main.frag [GBUFFER]` | zmiany shaderów `3cf285c1`, `96ddb583` (są w 0.8.4 i 0.9), nie potwierdzone na urządzeniu |
| `log2` (G2) | Adreno 650, poziom 2 | `vkCmdDispatch` w `Renderer::buildHiZ` | poziom 2 nie buduje już piramidy HiZ (niżej) |
| `log3` (G2) | Adreno 710 | kompilator, `main.vert [MESH_TYPE=2]` + `main.frag [GBUFFER]` | jak `log1` |
| `log4` (Kroniki) | Samsung A57 | `~Resources` → `zenkit::Vfs::~Vfs` przy zamykaniu | `std::_Exit` po opróżnieniu logu (`7ba334d2`, po 0.8.4, jest w 0.9) |

Żaden z tych logów nie pochodzi z 0.8.4 ani nowszej wersji, więc nic w nich nie obala poprawek z 0.8.4 i nic ich nie potwierdza.

**HiZ na poziomie 2.** Dwa urządzenia (Adreno 618 i 650) padły w `buildHiZ` dokładnie na poziomie 2, a w pierwszym starcie (poziom 0) ta sama klatka
przeszła `buildHiZ` bez awarii i padła dopiero później, na pipeline'ie G-buffera. Na poziomie 2 `DrawCommands::drawHiZ` nie rysuje żadnych okluderów
(`SafeGpu::isMinimal()`), więc piramida była budowana z pustego bufora głębi. Teraz `SafeGpu::noHiZ()` jest prawdą także na poziomie ≥2, bez czekania na
`crash.log`: nie rysujemy okluderów, nie budujemy piramidy, widok główny idzie przez `visibility_pass_main.comp`. Poprzednia wersja (zapis `nohiz=1` po
awarii w `buildHiZ`) zostaje jako zabezpieczenie dla poziomu 0 i 1.

Sprawdzone na hoście: `spirv-val` dla 168 shaderów `main_*`, `hiz_*`, `visibility_pass*` — wszystkie poprawne. **Nie potwierdzono na urządzeniu.**
