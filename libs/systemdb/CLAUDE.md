# libs/systemdb

Systemdatabasen `system.db`: lokal per användare, alltid synkron på UI-tråden. Biblioteket heter
`kalkyl_systemdb`, namnrymden `kalkyl::systemdb` och includes skrivs `"systemdb/system_db.h"`.
Den sparar utseendeläget (`theme_mode`), senaste mapp (`last_folder`), de 10 senaste projekten och linregistret.
Plats, schema och felhantering står i den arkiverade planen `doc/plans/archive/2026-11-20_Grunder.md` (2.2, 2.10 och 5.4).
Linregistret står i den arkiverade planen `doc/plans/archive/2026-11-23_Lindata.md`.

| Fil | Innehåll |
|---|---|
| `CMakeLists.txt` | Biblioteket (länkar `kalkyl_db` och `kalkyl_platform`) och testet `systemdb` |
| `system_db.h` | `SystemDb`, `ThemeMode`, `RecentProject`, `kMaxRecent`, `kApplicationId` och `defaultPath()` |
| `system_db.cpp` | Öppning, migrering, schemakontroll, undanflyttning av skadad fil, inställningar, senaste-listan och linregistret |
| `conductor.h` | `Conductor`, `ConductorOrigin`, `ConductorField`, `ConductorIssue` och valideringen. Beror bara på standardbiblioteket |
| `conductor.cpp` | `validateConductor`, `derivedCreepTempC`, `creepTempDeviates`, `sameConductorName` och `sameConductorValues` |
| `conductor_seed_v1.h` | Startdatan: SWECO:s 129 linor som SQL (`kConductorSeedV1`), en del av schemats steg 1 |
| `systemdb_test.cpp` | Headless-test i en temporär mapp med åäö i namnet (ctest-namn `systemdb`) |

## Regler

- **Plats:** `defaultPath()` ger `system.db` i `platform::kalkylDataDir()`. `KALKYL_DATA_DIR` ersätter mappen.
- **Skadad fil:** `open` flyttar en fil som inte är SQLite, har fel `application_id` eller saknar tabeller i schemat till
  `system-trasig-<ÅÅÅÅMMDD-HHMMSS>.db` och skapar en ny. En kvarglömd `-journal` tas bort så att den inte spelas upp mot den nya filen.
- **Schemakontroll:** `openMigrated` räknar tabellerna i `kExpectedTables` efter `migrate`. Saknas någon blir felet `NotADatabase`.
  Listan hålls lika med schemat: ett nytt steg med en ny tabell lägger till den.
- **Fel som returneras:** `TooNew` och åtkomstfel. Anroparen faller då tillbaka på `openInMemory(orsak)`.
- **Inställningar:** `themeMode()` och `lastFolder()` ger förval vid fel och returnerar inget `Result`.
- **Senaste projekt:** raden identifieras av `platform::pathKey`, så samma fil med andra versaler ger en rad.
  `touchRecentProject` lägger in och trimmar till `kMaxRecent` i en transaktion.
- **Ordning:** `last_opened` med millisekunder, och `rowid` avgör vid lika tid.

## Linregistret

- **Tabeller:** `conductor_seed` (orörd startdata, bara migreringsstegen skriver där), `conductor` (det användaren redigerar)
  och `conductor_seed_removed` (borttagna startdatalinor). Allt skapas och fylls av steg 1.
- **Startdatan** ligger som SQL i `conductor_seed_v1.h` och ändras aldrig efter första taggen. Ny startdata blir ett nytt migreringssteg.
  Enheterna är bladets egna (mm², mm, MPa, 10⁻⁶/°C, ‰, °C, N/m, kN), inte SI.
- **Ursprung:** `seed_key` är startdatans nyckel (`NULL` för egna linor) och ändras aldrig. `user_modified` sätts när en sparning ändrar en startdatarad.
  `ConductorOrigin` härleds vid läsning och ignoreras vid sparning.
- **Ordning:** `sort_order, id`. En ny lina får `max(sort_order) + 10`. `id` återanvänds aldrig (`AUTOINCREMENT`).
- **Namn:** unika utan hänsyn till ASCII-versaler (`COLLATE NOCASE`), och trimmas vid sparning.
- **Validering:** `validateConductor` är gemensam för fönstret och sparningen. Sparningen ger `Invalid` vid ogiltiga värden,
  `AlreadyExists` vid namnkrock och `NotFound` för ett id som saknas.
- **Sparning:** `saveConductors` gör borttagningar, uppdateringar och nya linor i en transaktion, och två linor får byta namn med varandra.
  En oförändrad rad hoppas över, så att `user_modified` inte sätts i onödan.
- **Borttagning:** en startdatalinas nyckel skrivs i `conductor_seed_removed`.
- **Återställning:** `restoreSeedConductors` uppdaterar startdataraderna på plats (id behålls), lägger in saknade och tömmer `conductor_seed_removed`.
  Egna linor behålls. Har en egen lina ett namn ur startdatan blir det `AlreadyExists` och ingenting ändras.
- **Krypningstemperaturen** (kolumn L) lagras. `derivedCreepTempC` är bara en kontroll, och `creepTempDeviates` markerar avvikelser över `kCreepTempTolerance`.
