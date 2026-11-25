# libs/sheetGrid

Excel-beteendet i en GlazeUI DataGrid: rektangulär markering med Shift och Shift+klick, Ctrl+C, X, V, A, Delete och snabbmenyn.
Biblioteket heter `kalkyl_sheetgrid`, namnrymden `kalkyl::sheetgrid` och includes skrivs `"sheetGrid/sheet_grid.h"`.
Det beror på `glaze` (PUBLIC) och `kalkyl_tabletext` (PRIVATE).

| Fil | Innehåll |
|---|---|
| `CMakeLists.txt` | Biblioteket `kalkyl_sheetgrid` (testet `sheetgrid` ligger i `tests/`) |
| `sheet_grid.h` | `SheetShape` (tabellens form), `SheetHost` (det fönstret svarar på), `SheetGrid` (markering, urklipp, snabbmeny) och de rena funktionerna `planSheetPaste`, `rectOf`, `clampPos` och `isBlock` |
| `sheet_grid.cpp` | Implementationen: de rena funktionerna och `SheetGrid` med tangenter, mus, snabbmeny, kopiering, inklistring och målning |
## Regler

- **Fönstret äger innehållet.** Biblioteket frågar via `SheetHost` vad en cell kopieras som och vilken färg den har,
  och via `SheetShape` vilka celler som är indata och vilka fält som passar. Ändringarna går tillbaka som en lista (`applyEdits`).
- **Inga rader läggs till eller tas bort.** Tabellen har ett fast antal rader, så biblioteket passar inte normalspannsformuläret.
- **Tomrader hoppas över.** `skipRow` pekar ut dem. De markeras, målas och tar emot inget, men raderna räknas med vid inklistring, så ett block ur Excel passar rad för rad.
- **Kolumnerna före `firstColumn`** (radrubriken) är aldrig markerade och får aldrig någon cellfärg.
- **Rena funktioner** (`planSheetPaste`, `rectOf`, `clampPos`, `isBlock`) har ingen I/O och testas utan fönster.
- **Fönstret** kopplar tabellens `onKeyDown`, `onMouseUp` och `onContextMenu` till `SheetGrid` och anropar `paintAll` efter varje omräkning.
