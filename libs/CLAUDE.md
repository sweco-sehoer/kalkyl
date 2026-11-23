# libs

Stödbibliotek. En katalog per bibliotek, som heter `kalkyl_<namn>`.
Fönster hör inte hit utan till `src/windows/`.

Beroendena går åt ett håll: `core` ← `platform` ← `db` ← `systemdb`/`projectdb` ← `appearance`.
Includes skrivs som `"core/result.h"`, `"platform/files.h"` och så vidare, och varje bibliotek
exporterar `libs/` som include-katalog. Namnrymderna heter `kalkyl::core`, `kalkyl::platform` osv.

| Fil/katalog | Innehåll |
|---|---|
| `CMakeLists.txt` | Tar in varje bibliotek med `add_subdirectory`, i beroendeordning |
| `version/` | Programmets version som header och versionsresurs för kalkyl.exe (`kalkyl_version`) |
| `core/` | Felmodellen `Result<T>` och `Error` (`kalkyl_core`, INTERFACE) |
| `platform/` | Win32-nära hjälpmedel: text, system, filer, låsfil och filassociation (`kalkyl_platform`) |
| `db/` | SQLite-lagret: `Database`, `Statement` och `migrate` (`kalkyl_db`) |
| `systemdb/` | Systemdatabasen med inställningar, senaste projekt och linregistret med startdata (`kalkyl_systemdb`) |
| `projectdb/` | Projektdatabasen med arbetskopia, lås och publicering (`kalkyl_projectdb`) |
| `appearance/` | Utseende och lägesknapp för GlazeUI-fönstren (`kalkyl_appearance`) |

Varje bibliotek har sin testfil i sin katalog. Den byggs med `-DKALKYL_TESTS=ON` och körs med `ctest`.
