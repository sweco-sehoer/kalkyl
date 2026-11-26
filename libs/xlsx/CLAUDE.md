# libs/xlsx

Läser celler, delade strängar, talformat och sammanslagningar ur .xlsx och .xlsm. Biblioteket heter
`kalkyl_xlsx` (STATIC), namnrymden är `kalkyl::xlsx` och includes skrivs `"xlsx/xlsx.h"`. Det beror på `core`
(PUBLIC) och `platform` och `kalkyl_miniz` (PRIVATE). **Makron läses aldrig**: `vbaProject.bin`, customXml,
ritningar och skrivarinställningar öppnas inte.

| Fil | Innehåll |
|---|---|
| `CMakeLists.txt` | `kalkyl_miniz` (zip) och `kalkyl_xlsx` |
| `xlsx.h`, `xlsx.cpp` | `readWorkbook` (ur minnet), `openWorkbook` (ur fil, delad läsning), `Sheet`, `Cell`, `Merge`, `columnLetters`, `parseCellRef`, `formatDecimals` och `numberText` |
| `xml_reader.h`, `xml_reader.cpp` | `XmlReader`, en liten pull-läsare för XML, och `decodeOoxmlText` (`_xHHHH_` och radbrytningar) |

## kalkyl_miniz

- Källorna (`miniz.c`, `miniz_zip.c`, `miniz_tinfl.c`, `miniz_tdef.c`) kommer ur FetchContent i rotens `CMakeLists.txt`
  (taggen `3.1.2`, `SOURCE_SUBDIR` pekar på en katalog som inte finns, så minizs egen `CMakeLists.txt` används aldrig).
  Lokal utcheckning: `-DFETCHCONTENT_SOURCE_DIR_MINIZ=<sökväg>`.
- Målet har `-w` (tredjepartskod), headrarna är `SYSTEM`, och definitionerna `MINIZ_NO_STDIO`, `MINIZ_NO_TIME` och
  `MINIZ_NO_ZLIB_COMPATIBLE_NAMES` följer med PUBLIC. `miniz_export.h` skrivs av `CMakeLists.txt` i byggkatalogen.
- miniz inkluderas bara i `xlsx.cpp` och `tests/xlsx_maker.h`.

## Läsreglerna (planen 4.3)

1. **Filtypen** avgörs av de första byten: "PK" med 3 och 4 eller 5 och 6 är en zip, D0 CF 11 E0 A1 B1 1A E1 är en
   OLE-fil (.xls eller lösenord), allt annat är inte en Excelfil.
2. **Zip** öppnas ur minnet. Delnamn jämförs utan skillnad på stora och små bokstäver. Varje del kontrolleras mot
   `kMaxPartBytes` före uppackningen.
3. **Huvuddelen** är relationen med Type som slutar på "/officeDocument" i `_rels/.rels`, annars `xl/workbook.xml`.
   Saknas den ger det "inte en Excelfil". Target löses mot arbetsbokens mapp, eller från roten med snedstreck först,
   och "../" löses upp.
4. **Bladen** i `workbook.xml` (namn, `state`, relationens id) pekar via arbetsbokens relationer på delarna.
   Bara Type som slutar på "/worksheet" är kalkylblad: diagram-, makro- och dialogblad hoppas över.
5. **Delade strängar** är texten i `t` direkt under `si` och under varje `r`, ihopfogad. `rPh` och `phoneticPr`
   hoppas över. Delarna `sharedStrings.xml` och `styles.xml` får saknas.
6. **Talformat:** `numFmts` och `cellXfs/xf` ger decimaler per formatindex. Inbyggda id 1 och 3 ger 0, id 2 och 4
   ger 2, övriga inbyggda -1, och egna format går genom `formatDecimals`.
7. **Celler:** rad och kolumn utan `r` är förra radnumret plus 1 och kolumnen efter förra cellen. `t` styr slaget
   (`s`, `inlineStr`, `str`, `d`, `b`, `e`, annars tal). Formler (`f`) läses inte, bara det sparade värdet. En cell
   utan värde eller med tom text sparas inte.
8. **Text:** entiteterna avkodas först, sedan `_xHHHH_` (surrogatpar ihop, `_x005F_` ger ett understreck), sist blir
   CR LF och ensamt CR till LF. Talet står som i `<v>` med punkt och alla decimaler.
9. **Sammanslagningar** (`mergeCell`) med en enda cell hoppas över.
10. **`numberText`** avrundar till 15 värdesiffror som Excel och skriver sedan med cellens decimaler (avrundat uppåt
    vid 5) eller, med -1, i kortaste form. Aldrig exponent, och ett negativt tal som blir noll får inget minustecken.

## Gränser och fel

- `kMaxFileBytes` (64 MB) gäller filen på disk, `kMaxPartBytes` (256 MB) varje uppackad del.
- Alla fel i innehållet är `Invalid` med texterna i planen 9.1. `openWorkbook` översätter läsfelen från `readFileShared`
  (`NotFound`, `Busy`, `AccessDenied`, `Unreachable`, `Io` och en för stor fil) till planens texter med filens namn.
- `XmlReader` vägrar en DOCTYPE, så inga entitetsdefinitioner kan expanderas. Prefix jämförs som lokala namn, så
  `x:c` och `c` är samma element, och `xmlns`-attribut hoppas över.
- `detail::setPartLimitForTest` (deklareras av testet, inte i headern) sänker gränsen för en del, så att felet
  "för stor del" går att prova utan en 256 MB stor fil.
