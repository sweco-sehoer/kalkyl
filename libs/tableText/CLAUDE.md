# libs/tableText

Tal med decimalkomma, tabbseparerat urklipp och inklistring som rena funktioner. Biblioteket heter `kalkyl_tabletext`,
namnrymden `kalkyl::tabletext` och includes skrivs `"tableText/table_text.h"`. Det beror bara på standardbiblioteket.

| Fil | Innehåll |
|---|---|
| `CMakeLists.txt` | Biblioteket `kalkyl_tabletext` (testet `tabletext` ligger i `tests/`) |
| `table_text.h` | `parseDecimal`, `formatDecimal`, `normalizeDecimal`, `formatDecimalText`, `trimmed`, `parseTsv`, `formatTsv`, `planPaste`, `PasteCell` och `PastePlan` för tal med decimalkomma, Excel-urklipp och inklistring |
| `table_text.cpp` | Implementationen av tabelltexten |

## Regler

- **Rena funktioner:** inget tillstånd, ingen I/O.
- **Talform:** `parseDecimal` läser komma och punkt (inte båda). `formatDecimal` skriver med komma för visning och urklipp
  och med punkt för DataGrids kanoniska celltext.
- **Kanonisk decimaltext:** `normalizeDecimal` gör inmatad text till punkt som decimaltecken, utan tusentalsavgränsare och med exakt de decimaler som skrevs.
  Exponent och flera decimaltecken avvisas, och tom text ger `""`. `formatDecimalText` skriver den med valfritt decimaltecken, och med `group`
  grupperas heltalsdelen med hårt mellanslag. Ingen av dem går via ett double. Normalspannets resultatpanel grupperar också med `formatDecimalText`.
- **Trimning:** `trimmed` tar bort ASCII-blanktecken i början och slutet (textfälten i `poleTable`).
- **Urklipp:** `parseTsv` läser Excels tabbseparerade text med citattecken och `formatTsv` skriver den. `planPaste` ger cellerna som en inklistring fyller.
- **Specialtecken** (U+00A0, U+202F, U+2212) byggs med `char`-värden i källan.
