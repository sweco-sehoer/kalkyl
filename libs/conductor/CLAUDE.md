# libs/conductor

Linan som typ och reglerna för den. Biblioteket heter `kalkyl_conductor`, namnrymden `kalkyl::conductor` och includes
skrivs `"conductor/conductor.h"`. Det beror bara på standardbiblioteket, så att både `systemdb` (linregistret)
och `projectdb` (projektets linkopior) kan använda det.
Reglerna står i den arkiverade planen `doc/plans/archive/2026-11-23_Lindata.md` (2.9). Linkopiorna i projektet och
flytten hit står i `doc/plans/archive/2026-11-23_Normalspann.md` (3.9–3.10).

| Fil | Innehåll |
|---|---|
| `CMakeLists.txt` | Biblioteket och testet `conductor` |
| `conductor.h` | `Conductor`, `ConductorOrigin`, `ConductorField`, `ConductorIssue`, `kCreepTempTolerance` och funktionerna nedan |
| `conductor.cpp` | `validateConductor`, `derivedCreepTempC`, `creepTempDeviates`, `sameConductorName`, `sameConductorValues`, `sameConductorData` och `compareNames` |
| `conductor_test.cpp` | Headless-test: reglerna, `sameConductorData` och `compareNames` |

## Regler

- **Validering:** `validateConductor` ger en tom lista för en giltig lina. Namnets unikhet kontrolleras inte här.
- **Namn:** `sameConductorName` jämför som SQLite NOCASE (bara ASCII-versaler viks).
- **Samma lina:** `sameConductorValues` jämför namn, typ och värden. `sameConductorData` jämför bara typ och värden (D–N),
  och används för att se om projektets kopia skiljer sig från registret.
- **Sortering:** `compareNames` är den naturliga jämförelsen som tabellernas sortering i GlazeUI: tal jämförs som tal.
