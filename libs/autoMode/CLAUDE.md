# libs/autoMode

Automatiskt läge som rena funktioner, utan databas och GUI: sträckorna i stolptabellen, normalspannets stolpar,
panelens text, dubbletterna, markeringarna i tabellen och jämförelsen mellan Stolptabell efter och före.
Biblioteket heter `kalkyl_automode`, namnrymden `kalkyl::automode` och includes skrivs `"autoMode/auto_mode.h"`.
Det beror på `poleTable` och `anCalc` (PUBLIC) och `tableText` (PRIVATE). `projectdb` (synkningen av de automatiska
beräkningarna) och `libs/poleGrid` och huvudfönstret (panelen och tabellen) använder samma funktioner, så de visar alltid samma sak.
Planen är `doc/plans/archive/2026-11-26_Automatiskt_lage.md`, avsnitt 5–7.

| Fil | Innehåll |
|---|---|
| `CMakeLists.txt` | Biblioteket `kalkyl_automode` (testet `automode` ligger i `tests/`) |
| `auto_mode.h` | `PoleRange`, `RangeRows`, `findRange`, `coordinateSpans`, `spanPoles`, `RangeSpan`, `rangeSpan`, `RangeStatus`, `rangeStatus`, `duplicatePoles`, `RowState`, `RowCompare`, `compareAfter`, `NamedRange`, `CellIssue`, `TableMarks` och `tableMarks` |
| `auto_mode.cpp` | Implementationen |

## Regler

- **Rena funktioner:** inget tillstånd, ingen I/O, inget fönster.
- **Stolpnr** jämförs exakt efter trimning (`tabletext::trimmed`). Stora och små bokstäver skiljs.
- **Sträckans fel** prövas i ordningen i planen 7.1 och har exakt de texterna; `rangeSpan` ger då inga stolpar och ett svar med bara sträckans fel.
- **Normalspannets stolpar** är raderna från första till sista stolpen, båda med, i tabellens ordning: Stolpnr, Spannlängd föreg. och Koordinat H (`poletable::numberValue`).
  Av, Ah och LKL används inte. Gränserna är `ancalc::SpanLimits{}`.
- **Spannlängd ur koordinaterna** (`coordinateSpans`): en tom Spannlängd föreg. blir det vågräta avståndet √(ΔN² + ΔE²) till närmast föregående rad som inte är helt tom, när båda har N och E.
  En ifylld spannlängd gäller alltid. Tabellens första stolpe och avståndet 0 ger inget värde. `spanPoles` använder värdet, och `libs/poleGrid` visar det i cellen.
- **Jämförelsen** matchar bara på Stolpnr. Talfälten jämförs som tal (44,10 = 44,1, tomt bara mot tomt) och textfälten exakt.
  En Stolpnr som finns flera gånger i före ger `Ambiguous`.
- **Markeringarna** (`tableMarks`): en sträcka med fel ger inga markeringar, eftersom felet visas i panelen. Dubbletter markeras alltid.
