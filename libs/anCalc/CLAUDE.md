# libs/anCalc

Normalspannsberäkningen som rena funktioner, utan databas och GUI. Biblioteket heter `kalkyl_ancalc`, namnrymden `kalkyl::ancalc`
och includes skrivs `"anCalc/normal_span.h"`. Det beror på `tableText` (`kalkyl_tabletext`, PRIVATE) för talen i kontrolltexterna.
Normalspannsberäkningen följer `ref/Normalspannberäkning.xlsm` enligt `doc/plans/archive/2026-11-23_Normalspann.md` avsnitt 2 och 5.1–5.2.

| Fil | Innehåll |
|---|---|
| `CMakeLists.txt` | Biblioteket `kalkyl_ancalc` (testet `ancalc` ligger i `tests/`) |
| `normal_span.h` | `SpanPole`, `SpanLimits`, `SpanCheck`, `SpanResult`, `SpanIssue`, `SpanCalculation`, `calculateNormalSpan`, `roundTenth` och texterna `ratioText`, `adjacentText`, `localText` |
| `normal_span.cpp` | Valideringen, aₙ, aₙ,ΔH, medelspannet, totallängden och kontrollerna 1–5 |
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
