# libs/tableText

Tal med decimalkomma, tabbseparerat urklipp och inklistring som rena funktioner. Biblioteket heter `kalkyl_tabletext`,
namnrymden `kalkyl::tabletext` och includes skrivs `"tableText/table_text.h"`. Det beror bara på standardbiblioteket.

| Fil | Innehåll |
|---|---|
| `CMakeLists.txt` | Biblioteket `kalkyl_tabletext` (testet `tabletext` ligger i `tests/`) |
| `table_text.h` | `parseDecimal`, `formatDecimal`, `parseTsv`, `formatTsv`, `planPaste`, `PasteCell` och `PastePlan` för tal med decimalkomma, Excel-urklipp och inklistring |
| `table_text.cpp` | Implementationen av tabelltexten |
## Regler

- **Rena funktioner:** inget tillstånd, ingen I/O.
- **Talform:** `parseDecimal` läser komma och punkt (inte båda). `formatDecimal` skriver med komma för visning och urklipp
  och med punkt för DataGrids kanoniska celltext.
- **Urklipp:** `parseTsv` läser Excels tabbseparerade text med citattecken och `formatTsv` skriver den. `planPaste` ger cellerna som en inklistring fyller.
