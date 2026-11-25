# libs/projectdb

Projektdatabasen: en `.kalkyl`-fil per projekt, med lokal arbetskopia, låsfil och säker publicering.
Biblioteket heter `kalkyl_projectdb`, namnrymden `kalkyl::projectdb` och includes skrivs `"projectdb/project_db.h"`.
Arbetsmodellen (2.3 till 2.7), schemat (2.10) och API:t (5.5) står i planen `doc/plans/archive/2026-11-20_Grunder.md`.
Dokumentationens värden, linkopiorna och normalspannsberäkningarna står i planen `doc/plans/archive/2026-11-23_Normalspann.md` (3.3 till 3.12, 4.1 och 5.5 till 5.6).

| Fil | Innehåll |
|---|---|
| `CMakeLists.txt` | Biblioteket (länkar `kalkyl_db`, `kalkyl_platform`, `kalkyl_calc`, `kalkyl_conductor` och `kalkyl_version`) och testet `projectdb` |
| `project_db.h` | `ProjectDb`, `OpenOptions`, `Snapshot`, `Access`, `defaultWorkDir`, `withExtension` och `cleanWorkDir` |
| `dev_migration.h` | Utvecklingsmigreringen: stegen efter v0.1.0 (`dev::kStep2` med de nya tabellerna). Töms vid 1.0.0 |
| `project_db.cpp` | Schemat (steg 1, v0.1.0, och stegen ur `dev_migration.h`), `create`, `open` med schemakontroll, lås, städning vid fel, `snapshot`/`publish`/`published`, revisionen och `close` |
| `project_data.h` | Dataskrivningarna och läsningarna: `DocValue`, `NormalSpan`, `Wire`, `Stage`, `docValues`, `setDocValue`, linkopiorna, beräkningarna och kopplingarna |
| `project_data.cpp` | Implementationen av `project_data.h`: SQL i råa strängliteraler, en transaktion per skrivning |
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

## Schemat och schemakontrollen

- **Steg 1** (`kSchemaV1`, v0.1.0) har bara `project` och ändras aldrig.
- **Steg 2** (`dev::kStep2` i `dev_migration.h`) lägger till `project_conductor`, `doc_value`, `normal_span`, `normal_span_pole` och `doc_normal_span`.
  `kSchemaVersion` = 2. Ett projekt från v0.1.0 migreras när det öppnas och sparas med det nya schemat vid nästa publicering.
- **Schemakontroll:** `open` räknar tabellerna i `kExpectedTablesSql` efter `migrate` och före `quickCheck`. Saknas någon (skadad fil) blir felet `NotADatabase`
  med texten "Projektfilen saknar delar som den här versionen av Kalkyl behöver. Filen är skadad:" följd av en radbrytning och sökvägen.
  Låset och arbetskopian städas som vid andra fel, och texten går inte genom `relabel`. Listan hålls lika med schemat.
- **Resultatet lagras aldrig.** aₙ räknas med `calc::calculateNormalSpan` från stolparna som `normalSpan` läser.

## Dataskrivningarna (`project_data.h`)

- **Skrivskydd:** varje skrivning kontrollerar först `access()` och ger `AccessDenied` ("Projektet är öppnat skrivskyddat och kan inte ändras.").
- **Transaktion och revision:** varje skrivning är en egen transaktion mot arbetskopian. Efter en lyckad transaktion anropas `markModified()`.
  `ProjectDb::revision()` räknar upp, och `isModified()` är sann när revisionen skiljer sig från den i den senast publicerade bilden (`published(snapshot, stamp)`).
- **Dokumentation:** `doc_value(key, value, reference)` har en rad per uppgift. Tomt värde och tom referens tar bort raden.
- **Linkopior:** projektet sparar en kopia av varje lina som en linrad i Dokumentation använder i `project_conductor`, unik på namn (`COLLATE NOCASE`).
  Finns namnet redan återanvänds kopian oförändrad. Annars kopieras linan in. `refreshProjectConductor` ersätter kopians typ och värden och behåller namn och id.
  Linraderna refererar till kopian med namn.
- **Rensning:** linor som ingen linrad (`phase_conductor`, `top_conductor_left`, `top_conductor_right`) använder
  tas bort i slutet av skrivningen som kunde släppa dem (`setDocValue` på en linrad, `setDocConductor`).
- **Beräkningar:** har ingen lina, eftersom normalspannet bara beror på stolparna. `saveNormalSpan` ersätter beräkningen och alla stolpar i en transaktion. Helt tomma stolpar sparas inte, och positionerna blir 0, 1, 2 ...
  Gränser och tal valideras (ändliga, gränser > 0) och ger `Invalid`.
- **Kopplingar:** `doc_normal_span(doc_key, span_id)` har `doc_key` som primärnyckel, så en rad visar högst en beräkning, men en beräkning kan visas på flera rader.
  `ON DELETE CASCADE` tar bort kopplingarna och stolparna när beräkningen tas bort. En koppling tömmer radens manuella värde i `doc_value` (referensen står kvar).
- **Nycklar:** de sex normalspannsnycklarna ges av `normalSpanDocKey(wire, stage)`. Övriga nycklar i `doc_value` är fria.
