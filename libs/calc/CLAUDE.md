# libs/calc

Beräkningar som rena funktioner, utan databas och GUI. Biblioteket heter `kalkyl_calc`, namnrymden `kalkyl::calc`
och includes skrivs `"calc/normal_span.h"`. Det beror bara på standardbiblioteket.
Normalspannsberäkningen följer `ref/Normalspannberäkning.xlsm` enligt `doc/plans/archive/2026-11-23_Normalspann.md` avsnitt 2 och 5.1–5.2.

| Fil | Innehåll |
|---|---|
| `CMakeLists.txt` | Biblioteket och testet `calc` |
| `normal_span.h` | `SpanPole`, `SpanLimits`, `SpanCheck`, `SpanResult`, `SpanIssue`, `SpanCalculation`, `calculateNormalSpan`, `roundTenth` och texterna `ratioText`, `adjacentText`, `localText` |
| `normal_span.cpp` | Valideringen, aₙ, aₙ,ΔH, medelspannet, totallängden och kontrollerna 1–5 |
| `table_text.h` | `parseDecimal`, `formatDecimal`, `parseTsv`, `formatTsv` och `planPaste` för tal med decimalkomma, Excel-urklipp och inklistring |
| `table_text.cpp` | Implementationen av tabelltexten |
| `calc_test.cpp` | Headless-test med arkets 280 spannlängder (exempel A), de syntetiska exemplen B och C, kantfall och tabelltexten |

## Regler

- **Rena funktioner:** inget tillstånd, ingen I/O. Resultatet lagras aldrig utan räknas om från indata.
- **Excels formler** står i planens avsnitt 2: aₙ = √(Σa³ / Σa) avrundat till 0,1 m, och aₙ,ΔH med lutande spannlängd.
  Spannet på första raden ingår inte i summorna.
- **Avvikelser från Excel** (planen 2.6):
  - medelspannet är `Σa / spanCount` (makrot delar med antalet stolpar, ett för många),
  - punkt 3 kontrolleras i båda riktningarna,
  - aₙ,ΔH räknas bara när alla stolpar har H,
  - helt tomma rader hoppas över.
- **Avrundning:** `roundTenth` avrundar halva bort från noll. Kontrollerna jämförs mot det avrundade aₙ, som i Excel.
- **Talform:** `parseDecimal` läser komma och punkt (inte båda). `formatDecimal` skriver med komma för visning och urklipp
  och med punkt för DataGrids kanoniska celltext.
