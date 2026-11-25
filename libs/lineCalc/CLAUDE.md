# libs/lineCalc

Linberäkningen enligt arket Linberäkning i `ref/Linberäkning.xlsm` och lastfallsmallarna (knapparna i arket), som rena funktioner
utan databas och GUI. Biblioteket heter `kalkyl_linecalc`, namnrymden `kalkyl::linecalc` och includes skrivs `"lineCalc/line_calc.h"`.
Det beror på `conductor` (`kalkyl_conductor`, PUBLIC, eftersom `LineInput` innehåller en `Conductor`) och `tableText`
(`kalkyl_tabletext`, PRIVATE, för talen i upplysningstexterna). Planen är `doc/plans/archive/2026-11-25_Linberakning.md`.

| Fil | Innehåll |
|---|---|
| `CMakeLists.txt` | Biblioteket `kalkyl_linecalc` (testerna `linecalc`, `load_cases`, `linecalc_facit` och `linecalc_excel` och facit ligger i `tests/`) |
| `line_calc.h` | Indata (`LineInput`, `LoadCase`, `LineConstants`), resultat (`LineResult`, `LoadCaseResult`), upplysningar (`LineIssue`), `CalcMode` och funktionerna `calculateLine`, `solveStateCubic`, `vbaAcos`, `stageOf`, `caseTempC` och `caseCharPressureNPerM2` |
| `line_calc.cpp` | Valideringen, lasterna, tillståndsekvationen, Eiv, transponeringen, utdatan och `ModeRules` |
| `load_cases.h`, `load_cases.cpp` | Lastfallsmallarna `newLineInput`, `resetToStandard`, `loadCrossingLower` och `loadCrossingUpper` (med D11), och arkets följdändringar `applyInputChange` |

## Regler

- **Beräkningen följer VBA formel för formel** (planen avsnitt 3 och 5.6): samma uttryck i samma ordning, ingen bättre lösare och
  ingen Newton-polering. `solveStateCubic` använder `std::pow(x, 1.0 / 3.0)` med tecken, inte `std::cbrt`.
  X29 i T1 blir därför 90,000000001010662 som i Excel.
- **π (D10):** läget Excel räknar med arkets π (O10, förval 3,1416) i Iᵥ, T, AD och ACOS. Läget New räknar med `std::numbers::pi` och läser inte O10.
- **Lägena Excel och New** (planen 4.2): Excel räknar som arket med dess fel, New rättar dem. Avvikelserna D1–D11 har varsitt test.
  Facittesterna (`linecalc_facit` med T1 och `linecalc_excel` med E1–E10) körs i läget Excel.
- **Varje avsteg från arket** beskrivs i PM:et `res/linberakning_excel.md` (vad Excel gör, vad New gör och varför), som visas i programmet.
  Ett nytt eller ändrat avsteg uppdaterar PM:et i samma ändring.
- **`ModeRules` är enda stället för Excels grenar** i `line_calc.cpp`, utom `caseTempC`, `caseCharPressureNPerM2`, `vbaAcos` och
  `applyInputChange`. Varje gren har en kommentar med avvikelsens nummer. Att ta bort Excel är att ta bort `CalcMode::Excel`,
  `ModeRules` och grenarna med D-nummer.
- **Ordningen i en omräkning:** R för alla lastfall, sedan laster, Ei-stadiet med Eiv, transponeringen och Ep-stadiet.
  Ett lastfall med L som pekar framåt får R ur första varvet.
- **Upplysningar:** Missing och Error ger inget resultat, Warning ger resultat. Valideringen samlar alla innan något räknas.
  Fel ur räkningen (Eiv, rot) stoppar vid första felet. Texterna står i planen 5.5.
- **Mallarna och följdändringarna** ligger i `load_cases`. Tal i mallarna är de som makrot skriver, källorna är de som läget New följer.
- **Rena funktioner:** inget tillstånd, ingen I/O. Resultatet lagras aldrig utan räknas om från indata.
