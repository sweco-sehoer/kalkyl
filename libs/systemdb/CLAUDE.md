# libs/systemdb

Systemdatabasen `system.db`: lokal per användare, alltid synkron på UI-tråden. Biblioteket heter
`kalkyl_systemdb`, namnrymden `kalkyl::systemdb` och includes skrivs `"systemdb/system_db.h"`.
Den sparar utseendeläget (`theme_mode`), senaste mapp (`last_folder`) och de 10 senaste projekten.
Plats, schema och felhantering står i planen `doc/plans/2026-11-20_Grunder.md` (2.2, 2.10 och 5.4).

| Fil | Innehåll |
|---|---|
| `CMakeLists.txt` | Biblioteket (länkar `kalkyl_db` och `kalkyl_platform`) och testet `systemdb` |
| `system_db.h` | `SystemDb`, `ThemeMode`, `RecentProject`, `kMaxRecent`, `kApplicationId` och `defaultPath()` |
| `system_db.cpp` | Öppning, migrering, undanflyttning av skadad fil, inställningar och senaste-listan |
| `systemdb_test.cpp` | Headless-test i en temporär mapp med åäö i namnet (ctest-namn `systemdb`) |

## Regler

- **Plats:** `defaultPath()` ger `system.db` i `platform::kalkylDataDir()`. `KALKYL_DATA_DIR` ersätter mappen.
- **Skadad fil:** `open` flyttar en fil som inte är SQLite, eller har fel `application_id`, till
  `system-trasig-<ÅÅÅÅMMDD-HHMMSS>.db` och skapar en ny. En kvarglömd `-journal` tas bort så att den inte spelas upp mot den nya filen.
- **Fel som returneras:** `TooNew` och åtkomstfel. Anroparen faller då tillbaka på `openInMemory(orsak)`.
- **Inställningar:** `themeMode()` och `lastFolder()` ger förval vid fel och returnerar inget `Result`.
- **Senaste projekt:** raden identifieras av `platform::pathKey`, så samma fil med andra versaler ger en rad.
  `touchRecentProject` lägger in och trimmar till `kMaxRecent` i en transaktion.
- **Ordning:** `last_opened` med millisekunder, och `rowid` avgör vid lika tid.
