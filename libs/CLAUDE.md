# libs

Stödbibliotek. En katalog per bibliotek, som heter `kalkyl_<namn>`.
Fönster hör inte hit utan till `src/windows/`.

Varje bibliotek har en enda uppgift. En ny beräkning eller ett nytt hjälpmedel får ett eget bibliotek,
och kod som flera bibliotek använder ligger i sitt eget bibliotek.

Beroendena går åt ett håll: `core` ← `platform` ← `db` ← `systemdb`/`projectdb` ← `appearance`.
`conductor` och `tableText` beror bara på standardbiblioteket, `anCalc` beror på `tableText`,
`lineCalc` beror på `conductor` och `tableText`, `systemdb` beror på `db` och `conductor`, och `projectdb` beror på `db`, `anCalc`, `conductor`, `lineCalc` och `tableText`.
`sheetGrid` beror på `glaze` och `tableText`.
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
| `anCalc/` | Normalspannsberäkningen som rena funktioner (`kalkyl_ancalc`) |
| `lineCalc/` | Linberäkningen enligt arket Linberäkning och lastfallsmallarna som rena funktioner (`kalkyl_linecalc`) |
| `platform/` | Win32-nära hjälpmedel: text, system, filer, låsfil och filassociation (`kalkyl_platform`) |
| `db/` | SQLite-lagret: `Database`, `Statement` och `migrate` (`kalkyl_db`) |
| `systemdb/` | Systemdatabasen med inställningar, senaste projekt och linregistret med startdata (`kalkyl_systemdb`) |
| `projectdb/` | Projektdatabasen med arbetskopia, lås och publicering (`kalkyl_projectdb`) |
| `appearance/` | Utseende och lägesknapp för GlazeUI-fönstren (`kalkyl_appearance`) |
| `sheetGrid/` | Excel-beteendet i en DataGrid: markering, urklipp och inklistring (`kalkyl_sheetgrid`) |

Inga tester ligger här. Alla tester ligger i `tests/` i roten, byggs med `-DKALKYL_TESTS=ON` och körs med `ctest`.
