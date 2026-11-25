# tests

Alla Kalkyls headless-tester (ctest). Inga tester ligger bland källkoden i `src` eller `libs`.
Katalogen tas med från rotens `CMakeLists.txt` när `KALKYL_TESTS` är på (presetet `test`, `ctest --preset test`).
Varje testfil blir ett program `kalkyl_<namn>_test` och ett ctest-test `<namn>`, via `kalkyl_add_test` i `CMakeLists.txt`.

| Fil | ctest | Innehåll |
|---|---|---|
| `CMakeLists.txt` | | `kalkyl_add_test` och ett anrop per testfil, med de bibliotek testet provar |
| `conductor_test.cpp` | `conductor` | Linans regler, `sameConductorData` och `compareNames` (`libs/conductor`) |
| `tabletext_test.cpp` | `tabletext` | Talen, den tabbseparerade texten och inklistringen (`libs/tableText`) |
| `ancalc_test.cpp` | `ancalc` | Normalspannet med arkets 280 spannlängder (exempel A), de syntetiska exemplen B och C och kantfall (`libs/anCalc`) |
| `linecalc_test.cpp` | `linecalc` | Linberäkningen: ekvationen, T1 i läget Excel och läget Ny (Excel med exakt π), felen, källorna och ett test per avvikelse D1–D6 och D10 (`libs/lineCalc`) |
| `load_cases_test.cpp` | `load_cases` | Lastfallsmallarna och följdändringarna, ett test per avvikelse D7–D9 och D11 (`libs/lineCalc`) |
| `linecalc_facit.h` | | Facit ur arket Linberäkning (sparade värden med full precision): `FacitCase`, `FacitRow`, `FacitConductor` och `kFacitCases`. Ett nytt testfall är en ny post sist |
| `linecalc_excel_facit.md` | | Användarens testfall ur Excel (visade tal): E1–E8 med standardmallen och åtta linor, E9 med Övre linan och E10 med Nedre linan |
| `linecalc_excel_test.cpp` | `linecalc_excel` | E1–E10 ur md-filen: läget Excel mot arkets visade X, Z och AE i alla lastfall, och skillnaden mot läget Ny per fall |
| `linecalc_facit_test.cpp` | `linecalc_facit` | Bygger varje facitfall som i Excel, jämför i läget Excel och skriver skillnaden mot läget Ny |
| `platform_test.cpp` | `platform` | Text, system, filer och `LockFile` mot en lokal mapp med åäö (`libs/platform`) |
| `association_test.cpp` | `association` | Filassociationen med en testtyp i `HKCU` (`libs/platform`) |
| `db_test.cpp` | `db` | SQLite-lagret mot en lokal mapp med åäö (`libs/db`) |
| `systemdb_test.cpp` | `systemdb` | Systemdatabasen i en temporär mapp med åäö (`libs/systemdb`) |
| `projectdb_test.cpp` | `projectdb` | Projektdatabasen med migreringstesten från v0.1.0 och v0.2.0. Lämnar `Prov åäö.kalkyl` i `KALKYL_PROV_DIR` (`<build>/prov`) åt röktestet |
| `sheetgrid_test.cpp` | `sheetgrid` | De rena funktionerna i `libs/sheetGrid`, utan fönster |

- Ett nytt test läggs här med en rad `kalkyl_add_test(<namn> <fil> <bibliotek>...)` och en rad i tabellen ovan.
- Testerna skriver bara i temporära mappar och i byggkatalogen, aldrig i `%LOCALAPPDATA%\Kalkyl`.
