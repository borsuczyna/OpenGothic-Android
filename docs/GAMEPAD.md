# Pad (kontroler) na Androidzie

Model: `common/ui/gamepadmodel.{h,cpp}` (bez zależności od Androida), podpięty w `common/utils/androidinput.cpp`.
Układ wzorowany na zGamePad (Gratt-5r2/zGamePad, domyślny plik `Utils/controls.gamepad`), z kamerą na prawej gałce.

## Gra

| Wejście | Akcja |
| --- | --- |
| lewa gałka góra / dół | do przodu / do tyłu |
| lewa gałka w bok | **obrót** postaci jak na lewej gałce ekranu dotykowego (bez kroku w bok, 100°/s przy pełnym wychyleniu); razem z przodem to marsz łukiem (ruch po skosie); przy namierzonym celu obrót daje krok w bok |
| prawa gałka | kamera, mocniej niż obrót lewej (150°/s, obrót i pochylenie, krzywa 1,6; czułość z ustawień „sensitivity”) |
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

## Windows (wersja pulpitowa z interfejsem dotykowym)

`common/utils/winpad.cpp`, wołane co klatkę z `Controller::update`. Najpierw XInput (pady Xbox i wszystko, co Steam Input albo DS4Windows
pokazuje jako XInput), potem DirectInput: **DualShock 4 i DualSense** są tam widoczne bez sterownika (kwadrat 0, krzyżyk 1, kółko 2,
trójkąt 3, L1 4, R1 5, L2 6, R2 7, Share 8, Options 9, L3 10, R3 11; lewa gałka X/Y, prawa Z/Rz, spusty z przycisków 6 i 7). Pad jest
czytany tylko, gdy okno gry jest aktywne. Inne pady DirectInput dostają układ „stary Xbox” (A0 B1 X2 Y3 LB4 RB5 Back6 Start7 L3 8 R3 9, prawa gałka Rx/Ry,
spusty jako oś Z). Nie sprawdzone na prawdziwym DualShocku.

## Osie na Androidzie

Lewa gałka: `AXIS_X/Y`, prawa: `AXIS_Z/RZ`, spusty: `LTRIGGER/RTRIGGER` albo `BRAKE/GAS` (lub przyciski L2/R2), D-pad:
`AXIS_HAT_X/Y` albo klawisze `DPAD_*` ze źródła pada. `AXIS_RX/RY` pomijam, bo na części padów (DualShock) to spusty.
Zdarzenia ruchu z pada nie trafiają już do obsługi dotyku, wcześniej gałki były czytane jak palec.

## Testy

```
c++ -std=c++17 -I common android/tests/gamepad_test.cpp common/ui/gamepadmodel.cpp common/ui/mobilecontrolsmodel.cpp && ./a.out
```

Sprawdzone: obrót lewej gałki, marsz łukiem, kamera i martwa strefa, przyciski i spusty, bumpery z bronią i bez, menu z powtarzaniem,
zmiana ekranu z trzymanym przyciskiem, ekwipunek, wytrych, zgodność z klawiaturą i dotykiem, wejście NaN / poza zakresem.
Kod odczytu zdarzeń Androida (`onPadKey`, `onPadMotion`) sprawdzony tylko składniowo z atrapami nagłówków NDK.
**Nie sprawdzony na prawdziwym padzie.**
