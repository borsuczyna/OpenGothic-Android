# Import zapisów z oryginalnego Gothica II

Zapis z oryginalnej gry (Gothic II / Noc Kruka, folder `Saves/savegameN`) jest wczytywany w silniku
bezpośrednio, bez konwertera na PC. Silnik czyta pliki zapisu przez ZenKit i nakłada ich stan na świeżą
Nową grę, więc wynik zawsze pasuje do skryptów, które ma gracz (`GOTHIC.DAT` z jego instalacji).

## Jak tego używa gracz (Android)

1. Launcher: **Zapisy → Zarządzanie zapisami → Importuj zapisy**, wybór gry (Gothic II lub Noc Kruka).
2. **Wybór źródła zapisów**: *OpenGothic* (ZIP z eksportu, jak dotąd) albo *Oryginał*.
3. *Oryginał*: wybór jednego folderu zapisu (np. `Saves/savegame1` skopiowanego z PC). Launcher
   (`OriginalSaveImport.java`) kopiuje pliki `.SAV` do `<Gothic II>/Saves/import`; wymagane są
   `SAVEDAT.SAV` i `SAVEINFO.SAV`.
4. Następna **Nowa gra** wczytuje ten zapis. Zaraz po wczytaniu gra zapisuje go jako zwykły slot
   „Gothic II: <tytuł lub data>”, a folder zmienia nazwę na `import-done` (albo na `import-failed`
   po błędzie; wtedy powód jest na ekranie, a kolejna Nowa gra jest zwykła).

Na desktopie wystarczy samemu skopiować folder zapisu do `<Gothic II>/Saves/import`.

## Co jest przenoszone

| Część | Skąd | Uwagi |
| --- | --- | --- |
| Wszystkie odwiedzone światy | `<ŚWIAT>.SAV` | każdy świat, który ma `.zen` w danych gry; poprzednie są zachowane jak przy zmianie poziomu |
| Postacie | `oCNpc` świata | dopasowanie po instancji i najbliższej pozycji, brakujące są dodawane, nadmiarowe usuwane; martwe zostają martwe |
| Bohater | jak postacie, także gdy jest zapisany wewnątrz lecącej strzały (`oCAIArrow`) | |
| Ekwipunek, założone rzeczy, skróty 1–0 | `packed`, `items`, sloty, `shortKey` | skróty oryginału 3..0 → sloty 3..10 |
| Atrybuty, poziom, PD, PN, talenty, ochrona, `aivar`, gildia, postawy, rutyna | `oCNpc` | |
| Zmienne skryptów, poznane dialogi, dziennik | `SAVEDAT.SAV` | |
| Postawy gildii | `guildTable` | Gothic II zapisuje tablicę 66×66, nie 42×42 (`SaveState::guild_table_raw`) |
| Skrzynie i drzwi | `oCMobContainer`, `oCMobDoor` | zawartość i zamki, dopasowanie po nazwie i pozycji (1 m) |
| Przedmioty na ziemi | `oCItem` | zostają, dochodzą albo znikają tak, jak w zapisie |
| Mechanizmy (bramy, kraty, tajne przejścia, łodzie, wiatraki) | `zCMover` | stan i klatka kluczowa; ruchome ruszają dalej od swojej klatki |
| Triggery | `zCTrigger` i pochodne | pozostała liczba uruchomień i włączenie |
| Czas gry, czas grania | `SAVEINFO.SAV` | |

Dźwignie same nie mają zapisywanego stanu w oryginale, zapisany jest stan mechanizmów, które poruszają.

Triggery startu świata odpalają się po imporcie tak jak przy wczytaniu zapisu (tylko te, które odpalają się
za każdym razem). Skutki pierwszego startu są już w zapisie.

## Czego nie ma

- Dokładnego stanu AI (bieżąca akcja, cel walki): postacie zaczynają od swojej rutyny. Zapis zrobiony w trakcie
  walki zaczyna się więc od nowa w tej samej sytuacji.
- Ciała zabitych postaci znikają przy ponownym wejściu do świata, jak zwykle w silniku.
- Sprawdzone na zapisach Gothic II Noc Kruka z Nowego Świata i Górniczej Doliny; zapisy z Jarkendaru i Irdorath
  powinny działać tak samo (świat z `.zen`), ale nie były testowane.

## Zmiany w ZenKit (`patches/zenkit-opengothic.patch`)

- `oCAIArrow`: obiekt AI lecącej strzały jest czytany razem z jej właścicielem, więc bohater zapisany w trakcie
  strzału jest znajdowany.
- `VNpc::item_short_keys`: skróty przedmiotów (`shortKey<n>`) są czytane i zapisywane, zamiast `0`.
- `VNpc::packed`: poprawione dzielenie listy przedmiotów po `;`.
- `SaveState::guild_table_raw`: cała tablica postaw gildii, niezależnie od rozmiaru.

## Sprawdzanie

Na telefonie (Redmi Note 8) zapisy `savegame1`–`savegame3` (dwa w Nowym Świecie, jeden w Górniczej Dolinie):
wszystkie skrzynie (159 + 65) i wszystkie mechanizmy (56 + 9) mają stan z zapisu, także po zapisaniu i
ponownym wczytaniu slotu; liczby przedmiotów na ziemi zgadzają się z zapisem; mieszkańcy miasta nie walczą ze sobą.
