# libs/poleGrid

Stolptabellen i en GlazeUI DataGrid: raderna som kanonisk decimaltext med den tomma raden sist, Excel-markeringen,
papperskorgen med frågan, snabbmenyn och färgerna. Delas av Stolptabell före och Stolptabell efter.
Biblioteket heter `kalkyl_polegrid`, namnrymden `kalkyl::polegrid` och includes skrivs `"poleGrid/pole_grid.h"`.
Det beror på `glaze`, `kalkyl_sheetgrid`, `kalkyl_poletable` och `kalkyl_automode` (PUBLIC) och `kalkyl_tabletext` (PRIVATE).

| Fil | Innehåll |
|---|---|
| `CMakeLists.txt` | Biblioteket `kalkyl_polegrid` (testet `polegrid` ligger i `tests/`) |
| `pole_grid.h` | `PoleGridHost` (det fönstret svarar på), `PoleLooks` och `CellLook`, de rena funktionerna `cellLook` och `computedSpanText` och klassen `PoleGrid` |
| `pole_grid.cpp` | Implementationen: `cellLook`, adaptern till `SheetGrid`, tabellens händelser, sparning, Infoga rad och Radera rader |

## Regler

- **Fönstret äger projektet.** `PoleGrid` håller raderna (`rows_`) och det senast sparade (`saved_`) och frågar `PoleGridHost`
  om sparning (`savePoles`), omläsning efter fel (`reloadPoles`), omräkning (`polesChanged`), omdöpt Stolpnr (`poleRenamed`),
  egna menyposter (`addMenuItems`), notisen i frågan Radera (`removeNote`) och statusraden (`showStatus`).
  `polesChanged` körs efter varje lyckad sparning som skrev något, och `poleRenamed` före den.
- **Inga layouter i koden.** Tabellen, kolumnerna och rullningslisterna står i `.glazeui`. `PoleGrid` styr bara en befintlig `DataGrid`:
  den kopplas med `committed`, `cellClicked`, `keyDown`, `mouseUp` och `contextMenu`.
- **Färgerna** kommer ur `cellLook`, som är en ren funktion utan fönster (testas i `tests/polegrid_test.cpp`).
  - Radens ton är gul (`{220,160,40}`), rosa för en ändrad rad och grön för en ny rad, med alfa 64 på jämna och 52 på udda rader.
  - Den ändrade cellen har `{210,90,140,130}` och tooltipen "I Stolptabell före: …". Ett fel eller en osäker Stolpnr har `{220,60,60,80}` och vinner över jämförelsen.
  - Tooltipens delar skiljs med radbrytning i ordningen fel, jämförelse, not. En not på raden ger Stolpnren flaggan (`StockIcon::Flag`).
  - Radera-kolumnen har radens ton och ingen tooltip i `cellLook`; "Radera raden" sätts av `PoleGrid` på datarader i ett skrivbart projekt.
- **Spannlängd ur koordinaterna:** en tom Spannlängd föreg. visar värdet ur `automode::coordinateSpans` med två decimaler, Σ-bilden (`StockIcon::Sum`) och en tooltip, som de uträknade värdena i Dokumentation.
  `PoleGrid` räknar värdena själv ur `rows_` (`looks_.coordinateSpans` i `fill`, `applyLooks` och `committed`), och `cellText` ger cellens text för visning och urklipp.
  Modellen har kvar den tomma texten: en inskriven spannlängd ersätter värdet, en tömd cell räknas igen, och ett uträknat värde som bekräftas oförändrat förblir uträknat.
- **`setLooks`** målar om och sätter tooltips och flaggor utan att bygga om raderna. `setRows` och varje strukturell ändring (`fill`) sätter dem tillbaka,
  eftersom `DataGrid::setRows` nollar cellinställningarna.
- **Sparningen** sker direkt vid varje ändring och bara när `poletable::rowsToSave` skiljer sig från det senast sparade.
  Misslyckas den postas en omläsning med `reloadPoles` (bakom `alive_`, så att tabellen inte byggs om mitt i sin egen händelse).
- **Skrivskyddat** (`setReadOnly`) låser cellerna, tar bort papperskorgarna och låter markeringen bara kopiera.
- **Används av** huvudfönstret (`src/windows/main_window.*`) för Stolptabell före och efter. Excel-markeringen kommer ur `libs/sheetGrid`.
