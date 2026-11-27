# libs

Stödbibliotek. En katalog per bibliotek, som heter `kalkyl_<namn>`.
Fönster hör inte hit utan till `src/windows/`.

Varje bibliotek har en enda uppgift. En ny beräkning eller ett nytt hjälpmedel får ett eget bibliotek,
och kod som flera bibliotek använder ligger i sitt eget bibliotek.

Beroendena går åt ett håll: `core` ← `platform` ← `db` ← `systemdb`/`projectdb` ← `appearance`.
`conductor` och `tableText` beror bara på standardbiblioteket, `anCalc` beror på `tableText`,
`lineCalc` beror på `conductor` och `tableText`, `poleTable` beror på `tableText`, `systemdb` beror på `db` och `conductor`, och `projectdb` beror på `db`, `anCalc`, `autoMode`, `conductor`, `lineCalc`, `tableText` och `poleTable`.
`autoMode` beror på `poleTable` och `anCalc`; `poleView` beror på `poleTable`, `autoMode` och `tableText`; `poleGrid` beror på `glaze`, `sheetGrid`, `poleTable`, `autoMode`, `tableText` och `poleView`.
`lineHeight` beror på `poleTable` (PUBLIC) och `poleView` och `tableText`; `lineHeightSheet` beror på `glaze` och `lineHeight`, och `projectdb` beror också på `lineHeight`.
`sheetGrid` beror på `glaze` och `tableText`.
`xlsx` beror på `core`, `platform` och miniz (`kalkyl_miniz`); `poleImport` beror på `xlsx`, `poleTable`, `autoMode` och `tableText`.
Katalogen får gärna ha versal i mitten (`anCalc`), och målet och namnrymden skrivs med gemener (`kalkyl_ancalc`, `kalkyl::ancalc`).
Includes skrivs som `"core/result.h"`, `"platform/files.h"` och så vidare, och varje bibliotek
exporterar `libs/` som include-katalog. Namnrymderna heter `kalkyl::core`, `kalkyl::platform` osv.

| Fil/katalog | Innehåll |
|---|---|
| `CMakeLists.txt` | Tar in varje bibliotek med `add_subdirectory`, i beroendeordning |
| `version/` | Programmets version som header och versionsresurs för kalkyl.exe (`kalkyl_version`) |
| `core/` | Felmodellen `Result<T>` och `Error` (`kalkyl_core`, INTERFACE) |
| `conductor/` | Linan som typ (`Conductor`) och valideringen, gemensam för systemdatabasen och projektdatabasen (`kalkyl_conductor`) |
| `tableText/` | Tabelltext: tal med decimalkomma, tabbseparerat urklipp och inklistring som rena funktioner (`kalkyl_tabletext`) |
| `poleTable/` | Stolptabellens rader: kolumnerna, talen som kanonisk decimaltext, ändringarna och radernas id i Stolptabell efter som rena funktioner (`kalkyl_poletable`) |
| `anCalc/` | Normalspannsberäkningen som rena funktioner (`kalkyl_ancalc`) |
| `autoMode/` | Automatiskt läge som rena funktioner: sträckorna i stolptabellen, normalspannets stolpar och jämförelsen mellan Stolptabell före och efter (`kalkyl_automode`) |
| `poleView/` | Vyn av Stolptabell efter som rena funktioner: filtret Förändrade ±2, det ändrade området, de grå raderna, rullningsmålet och infotexten (`kalkyl_poleview`) |
| `lineHeight/` | Fliken Linhöjder som rena funktioner: spannen på radernas id, blocken, medelvärdena (exakt i decimaltext), det som ska stå kvar när efter ändras, bladets celler, färger och texter (`kalkyl_lineheight`) |
| `lineCalc/` | Linberäkningen enligt arket Linberäkning och lastfallsmallarna som rena funktioner (`kalkyl_linecalc`) |
| `platform/` | Win32-nära hjälpmedel: text, system, filer, låsfil och filassociation (`kalkyl_platform`) |
| `xlsx/` | Excelfiler: celler, delade strängar, talformat och sammanslagningar ur .xlsx och .xlsm med miniz, utan makron (`kalkyl_xlsx`, och `kalkyl_miniz` för zip-källorna) |
| `poleImport/` | Importen av en stolptabell: rubrikerna, valet av kolumner, utdraget och rådata som rena funktioner (`kalkyl_poleimport`) |
| `db/` | SQLite-lagret: `Database`, `Statement` och `migrate` (`kalkyl_db`) |
| `systemdb/` | Systemdatabasen med inställningar, senaste projekt och linregistret med startdata (`kalkyl_systemdb`) |
| `projectdb/` | Projektdatabasen med arbetskopia, lås och publicering (`kalkyl_projectdb`) |
| `appearance/` | Utseende och lägesknapp för GlazeUI-fönstren (`kalkyl_appearance`) |
| `sheetGrid/` | Excel-beteendet i en DataGrid: markering, urklipp och inklistring (`kalkyl_sheetgrid`) |
| `poleGrid/` | Stolptabellen i en DataGrid: raderna, Excel-markeringen, papperskorgen, färgerna och snabbmenyn, delad av Stolptabell före och efter (`kalkyl_polegrid`) |
| `lineHeightSheet/` | `LineHeightSheet`: linhöjdsbladet som egen GlazeUI-widget, ett OBJECT-bibliotek som också länkas in i glaze-render och GlazeBuilder (`kalkyl_lineheightsheet`) |

Inga tester ligger här. Alla tester ligger i `tests/` i roten, byggs med `-DKALKYL_TESTS=ON` och körs med `ctest`.
