# libs/poleTable

Radmodellen för stolptabellen i fliken Stolptabell före: kolumnerna, talen som kanonisk decimaltext och ändringarna med nya rader.
Biblioteket heter `kalkyl_poletable`, namnrymden `kalkyl::poletable` och includes skrivs `"poleTable/pole_table.h"`.
Det beror på `tableText` och delas av `projectdb` (lagringen och valideringen) och huvudfönstret.

| Fil | Innehåll |
|---|---|
| `CMakeLists.txt` | Biblioteket `kalkyl_poletable` (testet `poletable` ligger i `tests/`) |
| `pole_table.h` | Kolumnkonstanterna `kPole`…`kLkl`, `PoleRow`, `isNumberColumn`, `columnName`, `field`, `isEmptyRow`, `acceptField`, `displayText`, `copyText`, `numberValue`, `firstInvalidColumn`, `Edit`, `applyEdits`, `trimTrailingEmptyRows` och `rowsToSave` |
| `pole_table.cpp` | Implementationen, och konstanten `kGroupThousands` för tusentalsgrupperingen i visningen |

## Regler

- **Rena funktioner:** inget tillstånd, ingen I/O, inget fönster.
- **Kolumnerna:** 0 Stolpplats (text), 1 Av, 2 Ah, 3 Spannlängd föreg., 4 Koordinat N, 5 E, 6 H (tal) och 7 LKL (text). Radera-kolumnen hör till fönstret och räknas inte här.
- **Talen är kanonisk decimaltext** (`tabletext::normalizeDecimal`): punkt, inga tusentalsavgränsare och exakt de decimaler användaren skrev.
  Visning och urklipp skrivs ur texten, aldrig via ett double. `numberValue` ger talet för beräkningar.
- **Visning och urklipp:** `displayText` har decimalkomma och grupperar heltalsdelen med hårt mellanslag när `kGroupThousands` är sant.
  `copyText` grupperar aldrig, så att Excel läser talet.
- **Textfälten** trimmas med `tabletext::trimmed` (ASCII-blanktecken i början och slutet), annars inga regler.
- **Tomma rader:** `rowsToSave` behåller tomma rader mitt i tabellen och tar bara bort tomma rader sist.
  `applyEdits` lägger till tomma rader fram till en ändrad rad bortom slutet.
