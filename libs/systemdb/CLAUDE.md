# libs/systemdb

Systemdatabasen `system.db`: lokal per användare, alltid synkron på UI-tråden. Biblioteket heter
`kalkyl_systemdb`, namnrymden `kalkyl::systemdb` och includes skrivs `"systemdb/system_db.h"`.
Den sparar utseendeläget (`theme_mode`), senaste mapp (`last_folder`), importmappen (`last_import_folder`), de inlärda rubrikerna (`pole_import_learned`), filtret i Stolptabell efter (`pole_after_filter`), de 10 senaste projekten och linregistret.
Plats, schema och felhantering står i den arkiverade planen `doc/plans/archive/2026-11-20_Grunder.md` (2.2, 2.10 och 5.4).
Linregistret står i den arkiverade planen `doc/plans/archive/2026-11-23_Lindata.md`.
Typen `Conductor` och valideringen ligger i `libs/conductor` (`kalkyl::conductor`), inte här.

| Fil | Innehåll |
|---|---|
| `CMakeLists.txt` | Biblioteket (länkar `kalkyl_db`, `kalkyl_platform` och `kalkyl_conductor`). Testet `systemdb` ligger i `tests/` |
| `system_db.h` | `SystemDb`, `ThemeMode`, `AfterFilter`, `RecentProject`, `LearnedHeader`, `kMaxRecent`, `kApplicationId`, `kAutosaveChoices`, `kDefaultAutosaveMinutes` och `defaultPath()` |
| `system_db.cpp` | Öppning, migrering, schemakontroll, undanflyttning av skadad fil, inställningar, senaste-listan och linregistret |
| `conductor_seed_v1.h` | Startdatan: SWECO:s 129 linor som SQL (`kConductorSeedV1`), en del av schemats steg 1 |
## Regler

- **Plats:** `defaultPath()` ger `system.db` i `platform::kalkylDataDir()`. `KALKYL_DATA_DIR` ersätter mappen.
- **Skadad fil:** `open` flyttar en fil som inte är SQLite, har fel `application_id` eller saknar tabeller i schemat till
  `system-trasig-<ÅÅÅÅMMDD-HHMMSS>.db` och skapar en ny. En kvarglömd `-journal` tas bort så att den inte spelas upp mot den nya filen.
- **Schemakontroll:** `openMigrated` räknar tabellerna i `kExpectedTables` efter `migrate`. Saknas någon blir felet `NotADatabase`.
  Listan hålls lika med schemat: ett nytt steg med en ny tabell lägger till den.
- **Fel som returneras:** `TooNew` och åtkomstfel. Anroparen faller då tillbaka på `openInMemory(orsak)`.
- **Inställningar:** `themeMode()` och `lastFolder()` ger förval vid fel och returnerar inget `Result`.
- **Automatisk sparning:** nyckeln `autosave_minutes` i tabellen `setting`, utan schemaändring. De tillåtna värdena är
  `kAutosaveChoices` (0 = av, 1, 5, 10, 15 och 30 minuter). `autosaveMinutes()` ger förvalet 5 när värdet saknas eller är okänt,
  och `setAutosaveMinutes` ger `Invalid` för ett annat värde.
- **Importmappen:** nyckeln `last_import_folder` i tabellen `setting`, utan schemaändring. `lastImportFolder()` ger tom text när inget är sparat, och `setLastImportFolder` sparar mappen.
  Den är skild från `last_folder`, eftersom Excelfilerna ofta ligger i en leveransmapp eller i Hämtade filer och projektväljaren då skulle öppna där.
  Förvalet för Importera stolptabell… är projektfilens mapp och sedan Dokument, men det väljer huvudfönstret.
- **Inlärda rubriker:** nyckeln `pole_import_learned` i tabellen `setting`, en rad per rubrik med fältet (0-7, poletable-kolumnen) och den normaliserade rubriken (`poleimport::SourceColumn::key`) skilda med en tabb.
  Raderna står senast först. `learnedHeaders()` ger en tom lista vid fel eller när inget är sparat, och rader som inte går att tolka hoppas över.
  `learnHeader(field, key)` lägger rubriken först för fältet, flyttar en likadan som redan finns och behåller högst tio per fält (de senaste). Andra fält rörs inte.
  Fältet utanför 0-7 och en tom rubrik eller en rubrik med tabb eller radbrytning ger `Invalid`. Tabben och radbrytningen skrivs `char{9}` och `char{10}` i koden.
- **Filtret i Stolptabell efter:** nyckeln `pole_after_filter` i tabellen `setting`, utan schemaändring och utan migreringssteg. Texten är `changed` (Förändrade ±2) eller `all` (Alla).
  `afterFilter()` ger `AfterFilter::Changed` vid fel och för ett saknat eller okänt värde, och `setAfterFilter` sparar valet. `AfterFilter` speglar `poleview::Filter` utan att systemdb beror på poleView.
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
