# libs/poleTable

Radmodellen för stolptabellerna i flikarna Stolptabell före och efter: kolumnerna, talen som kanonisk decimaltext och ändringarna med nya rader.
Biblioteket heter `kalkyl_poletable`, namnrymden `kalkyl::poletable` och includes skrivs `"poleTable/pole_table.h"`.
Det beror på `tableText` och delas av `projectdb` (lagringen och valideringen), `autoMode` (sträckorna och jämförelsen),
`poleGrid` (tabellen i fönstret), `poleImport` (importen från Excel, med typerna i `import_record.h`) och huvudfönstret.

| Fil | Innehåll |
|---|---|
| `CMakeLists.txt` | Biblioteket `kalkyl_poletable` (testet `poletable` ligger i `tests/`) |
| `pole_table.h` | Kolumnkonstanterna `kPole`…`kLkl`, `PoleRow`, `isNumberColumn`, `columnName`, `field`, `isEmptyRow`, `normalizeLkl`, `acceptField`, `displayText`, `copyText`, `numberValue`, `firstInvalidColumn`, `Edit`, `applyEdits`, `trimTrailingEmptyRows` och `rowsToSave` |
| `pole_table.cpp` | Implementationen, och konstanten `kGroupThousands` för tusentalsgrupperingen i visningen |
| `import_record.h` | Bara typer: rådata för en import av en stolptabell (`ImportCellKind`, `ImportCell`, `ImportRow`, `ImportColumn` och `ImportRecord`). `poleImport` bygger posten, `projectdb` sparar och läser den och fönstret skickar den vidare |

## Regler

- **Rena funktioner:** inget tillstånd, ingen I/O, inget fönster.
- **Kolumnerna:** 0 Stolpnr (text), 1 Koordinat N, 2 E, 3 H, 4 Avstånd föregående, 5 Ah, 6 Av (tal) och 7 LKL (text), samma ordning som i stolptabellerna som klistras in.
  Radera-kolumnen hör till fönstret och räknas inte här. Kod som läser eller skriver fälten använder konstanterna eller fältnamnen, aldrig kolumnnummer.
- **LKL** (`normalizeLkl`, via `acceptField`): delarna mellan radbrytningar och snedstreck trimmas och skrivs med ett snedstreck, så "227", två rader eller "227/262" blir "227" respektive "227/262".
- **Talen är kanonisk decimaltext** (`tabletext::normalizeDecimal`): punkt, inga tusentalsavgränsare och exakt de decimaler användaren skrev.
  Visning och urklipp skrivs ur texten, aldrig via ett double. `numberValue` ger talet för beräkningar.
- **Visning och urklipp:** `displayText` har decimalkomma och grupperar heltalsdelen med hårt mellanslag när `kGroupThousands` är sant.
  `copyText` grupperar aldrig, så att Excel läser talet.
- **Stolpnr** trimmas med `tabletext::trimmed` (ASCII-blanktecken i början och slutet), annars inga regler.
- **Tomma rader:** `rowsToSave` behåller tomma rader mitt i tabellen och tar bara bort tomma rader sist.
  `applyEdits` lägger till tomma rader fram till en ändrad rad bortom slutet.
