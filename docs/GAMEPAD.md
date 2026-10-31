# Pad (kontroler) na Androidzie

Model: `common/ui/gamepadmodel.{h,cpp}` (bez zależności od Androida), podpięty w `common/utils/androidinput.cpp`.
Układ wzorowany na zGamePad (Gratt-5r2/zGamePad, domyślny plik `Utils/controls.gamepad`), z kamerą na prawej gałce.

## Gra

| Wejście | Akcja |
| --- | --- |
| lewa gałka góra / dół | do przodu / do tyłu |
| lewa gałka w bok | **strafe** (krok w bok, postać się nie obraca; razem z przodem to ruch po skosie) |
| prawa gałka | kamera (obrót i pochylenie, krzywa 1,6; czułość z ustawień „sensitivity”) |
| A albo RT | akcja: rozmowa, wzięcie, atak, strzał, czar (trzymane) |
| B albo LT | blok (trzymany) |
| X | skok (trzymany) |
| Y | wyjmij / schowaj broń |
| LB / RB | z bronią w ręku ciosy w lewo / w prawo; bez broni LB przełącza chód i bieg |
| L3 | skradanie |
| R3 | cel (następny) |
| D-pad góra / lewo / prawo / dół | mapa / postać / dziennik / czary |
| Select | ekwipunek |
| Start | menu |
| A w przemianie | powrót do ludzkiej postaci (klawisz Enter, który gra daje tylko w przemianie) |

## Menu, dialogi, ekwipunek, wytrych, wideo

- gałka lub D-pad wybierają (powtarzanie po 380 ms co 110 ms), A zatwierdza, B cofa, Start zamyka;
- ekwipunek: X stos, Y wyrzuć, LB / RB poprzednia / następna kategoria, Select zamyka;
- menu: Y to Backspace (pole tekstowe);
- wytrych: lewo / prawo obracają wytrych, B, D-pad dół albo Select anulują;
- wideo: A, B lub Start pomijają.

Przycisk trzymany w chwili zmiany ekranu należy do poprzedniego ekranu i nic nie robi, dopóki go nie puścisz.

## Osie na Androidzie

Lewa gałka: `AXIS_X/Y`, prawa: `AXIS_Z/RZ`, spusty: `LTRIGGER/RTRIGGER` albo `BRAKE/GAS` (lub przyciski L2/R2), D-pad:
`AXIS_HAT_X/Y` albo klawisze `DPAD_*` ze źródła pada. `AXIS_RX/RY` pomijam, bo na części padów (DualShock) to spusty.
Zdarzenia ruchu z pada nie trafiają już do obsługi dotyku, wcześniej gałki były czytane jak palec.

## Testy

```
c++ -std=c++17 -I common android/tests/gamepad_test.cpp common/ui/gamepadmodel.cpp common/ui/mobilecontrolsmodel.cpp && ./a.out
```

Sprawdzone: strafe, ruch po skosie, kamera i martwa strefa, przyciski i spusty, bumpery z bronią i bez, menu z powtarzaniem,
zmiana ekranu z trzymanym przyciskiem, ekwipunek, wytrych, zgodność z klawiaturą i dotykiem, wejście NaN / poza zakresem.
Kod odczytu zdarzeń Androida (`onPadKey`, `onPadMotion`) sprawdzony tylko składniowo z atrapami nagłówków NDK.
**Nie sprawdzony na prawdziwym padzie.**
