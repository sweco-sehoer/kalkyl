# libs/projectdb

Projektdatabasen: en `.kalkyl`-fil per projekt, med lokal arbetskopia, låsfil och säker publicering.
Biblioteket heter `kalkyl_projectdb`, namnrymden `kalkyl::projectdb` och includes skrivs `"projectdb/project_db.h"`.
Arbetsmodellen (2.3 till 2.7), schemat (2.10) och API:t (5.5) står i planen `doc/plans/2026-11-20_Grunder.md`.

| Fil | Innehåll |
|---|---|
| `CMakeLists.txt` | Biblioteket (länkar `kalkyl_db`, `kalkyl_platform` och `kalkyl_version`) och testet `projectdb` |
| `project_db.h` | `ProjectDb`, `OpenOptions`, `Snapshot`, `Access`, `defaultWorkDir`, `withExtension` och `cleanWorkDir` |
| `project_db.cpp` | `create`, `open`, lås, städning vid fel, `snapshot`/`publish`/`published` och `close` |
| `projectdb_test.cpp` | Headless-test med egen mapp (åäö) och egen arbetsmapp. Lämnar `Prov åäö.kalkyl` i `KALKYL_PROV_DIR` |

## Regler

- **SQLite öppnar aldrig originalet.** `open` kopierar filen med `CopyFileExW` till `workDir`, och allt arbete går mot arbetskopian (`local()`).
- **Originalet ändras bara genom namnbyte.** `publish` kopierar en ögonblicksbild till den dolda `~<namn>.<pid>-<nr>.tmp` bredvid originalet, visar den igen och byter namn med `moveReplace` (vid `create` `moveNew`). Efter ett lyckat byte är resultatet alltid lyckat; går stämpeln inte att läsa används den temporära filens.
- **Skrivskyddat original:** arbetskopian får skrivskyddet borttaget, så att SQLite kan skriva och filen kan tas bort.
- **Ingen skrivrätt i mappen:** låset går inte att ta, och `open` ger `AccessDenied`. Väljaren erbjuder då skrivskyddat läge, som vid `Locked`.
- **Konflikt:** `publish` jämför originalets `FileStamp` med `Snapshot::base` och ger `Conflict` om den ändrats.
- **Lås:** `<projektfil>.lock`. Är låset vårt eget (samma dator och pid) väntar `open` upp till 2 s på att det släpps.
  Är det övergivet tas det bort, och annars ger `open` felet `Locked` med vem, var och när.
- **Städning:** varje fel efter att låset tagits släpper låset och tar bort arbetskopian (och ögonblicksbilden vid `create`).
- **Trådar:** `create` och `open` körs i en arbetstråd. `snapshot` och `published` körs på UI-tråden, `publish` i en arbetstråd.
- **Arbetsmappen:** `cleanWorkDir` tar bort filer som inte är öppna, och körs vid programstart.
- **Test:** tar 2 s extra, eftersom en andra öppning i samma process väntar ut den egna lås-regeln.
