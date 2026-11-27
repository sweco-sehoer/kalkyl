# libs/lineHeight

Fliken Linhöjder som rena funktioner, utan databas och GUI: spannen på radernas id, blocken, medelvärdena (exakt i decimaltext),
det som ska stå kvar när Stolptabell efter ändras, bladets celler, färger och texter.
Biblioteket heter `kalkyl_lineheight`, namnrymden `kalkyl::lineheight` och includes skrivs `"lineHeight/line_height.h"`.
Det beror på `poleTable` (PUBLIC) och `poleView` och `tableText` (PRIVATE).
`libs/projectdb` (lagringen och städningen), `libs/lineHeightSheet` (widgeten) och huvudfönstret använder det.
Planen är `doc/plans/archive/2026-11-27_Linhojder.md`, avsnitt 6, 8.2, 9 och 12, med ändringarna efter handtestet (H1–H6) i avsnitt 1.

| Fil | Innehåll |
|---|---|
| `CMakeLists.txt` | Biblioteket `kalkyl_lineheight` (testet `lineheight` ligger i `tests/`) |
| `line_height.h` | `Part`, `kBlockMargin`, `SpanKey`, `spanKey`, `isValidKey`, `SpanHeights`, `heightOf`, `StoredHeights`, `afterSpans`, `keptHeights`, `acceptHeight`, `acceptAv`, `halfSum`, `roundedText`, `kMeanDecimals`, `SpanRef`, `PoleBlock`, `spanMean`, `lineCalcHeight`, `outerAvField`, `buildBlocks`, bladets mått (`kSheetColumns`, `kBlockRows`, `kBlockStride`, `kColumnChars`), `CellStyle`, `InputRef`, `SheetCell`, `Sheet`, `buildSheet`, `cellAt`, `Move`, `moveInput`, `findInput`, `Rgb`, `Align`, `Frame`, `Mark`, `CellLook`, `cellLook`, `blockFill` och `areaText` |
| `line_height.cpp` | Implementationen |

## Regler

- **Rena funktioner:** inget tillstånd, ingen I/O, inget fönster.
- **Raderna i Stolptabell efter är allt.** Blocken, spannen och Stolpnr tas ur efter. Bara märkningen av vilka rader som är ändrade jämför med före (`poleview::filterMarks`).
  Stolpnr är ett namn och aldrig en nyckel: dubbletter och tomma Stolpnr låser ingenting, och en rad utan Stolpnr heter "rad {n}" (`poleview::rowLabel`).
- **Området** är `poleview::changedArea(antal, märkningen, kBlockMargin)`, ett sammanhängande område från första till sista märkta raden ±`kBlockMargin` (1). Filtret i Stolptabell efter är ±2, så att det visar alla stolpar som ytterblockens spann når.
  Varje rad i området som inte är helt tom (`poletable::isEmptyRow`) får ett block, oavsett Stolpnr.
- **Grannar och spann:** grannarna är närmaste rad ovanför och nedanför som inte är helt tom, också utanför området (ytterblockens spann når alltså två rader från de ändrade raderna).
  Spannets nyckel är de två radernas `PoleRow::id` i stigande ordning (`SpanKey`), och topplinans Av har radens id som nyckel.
  Ett spann går att skriva i (`usable`) när grannen finns och båda raderna har id > 0. Annars är `issue` skälet och tooltipen.
- **`afterSpans` och `keptHeights`:** spannen i efter, och det av det sparade som får stå kvar (spann i `afterSpans` och topplinans Av för rader som finns och inte är tomma).
  `projectdb` anropar dem när efter skrivs och tar bort resten.
- **Medelvärdet** (`spanMean`): båda spannen med värden ger (bak + fram) / 2, bara ett spann ger dess värde, ett tomt eller låst värde när båda finns ger tomt.
  Linhöjd faslina för linberäkning (`lineCalcHeight`) är (ytterfasernas medel + mittfasens medel) / 2, räknat på de exakta medlen.
  Sammanslagningen visar mittfasens spannvärden som de skrevs.
- **Av:** ytterfaserna visar det största av Av Fas1 och Fas3 som tal (`outerAvField`, Fas1 vid lika), mittfasen Av Fas2, och topplinans Av skrivs av användaren.
- **Exakt räkning:** `halfSum` räknar på siffrorna i decimaltexterna med strängaritmetik (aldrig double), och `roundedText` avrundar halvt bort från noll på det exakta värdet,
  som Excel visar det (16,775 blir 16,78, vilket `formatDecimal` på ett double inte ger).
- **Inmatning:** `acceptHeight` är `normalizeDecimal` men vägrar negativa tal, `acceptAv` tillåter dem. En inmatad linhöjd visas som den skrevs (`formatDecimalText`), medlen med två decimaler.
- **Bladet** (`buildSheet`): blockmallen i planen 9.2, 19 rader per block med en tom rad mellan delarna och en tom rad mellan blocken, kolumnerna P–X som 0–8.
  Blockets bakgrund (`blockFill`) är stolptabellernas gula ton med alfa 64 och 52 på vitt, växelvis per block.
  `Sheet::inputs` är inmatningscellerna i läsordning, och `moveInput` och `findInput` är förflyttningen (Tab, Enter, pilarna) och sökningen på blockets rad-id.
  `cellLook` ger färger, fet stil, justering, ram och glyf (`Mark`) per `CellStyle` (planen 9.3 och H4–H6):
  "meter" och "Av" har delens rubrikfärg, inmatningen är vit, och beräknade (`Computed`, Σ) och länkade fält (`Linked` och `AvValue`, Sync) är gröna.
- **Texterna** (rubrikerna, tooltips och `areaText`) är ordagrant de i planen 12. Strecket i `areaText` är U+2013, och spannen skrivs med bindestreck.
