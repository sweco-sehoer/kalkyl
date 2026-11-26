# libs/projectdb

Projektdatabasen: en `.kalkyl`-fil per projekt, med lokal arbetskopia, låsfil och säker publicering.
Biblioteket heter `kalkyl_projectdb`, namnrymden `kalkyl::projectdb` och includes skrivs `"projectdb/project_db.h"`.
Arbetsmodellen (2.3 till 2.7), schemat (2.10) och API:t (5.5) står i planen `doc/plans/archive/2026-11-20_Grunder.md`.
Dokumentationens värden, linkopiorna och normalspannsberäkningarna står i planen `doc/plans/archive/2026-11-23_Normalspann.md` (3.3 till 3.12, 4.1 och 5.5 till 5.6).
Linberäkningarna, kopplingen till Dokumentation och upplösningen står i planen `doc/plans/archive/2026-11-25_Linberakning_fas_2.md` (avsnitt 2.1 till 2.3, 3 och 4).
Stolptabell före står i planen `doc/plans/archive/2026-11-26_Stolptabell_fore.md` (4 och 5.4).

| Fil | Innehåll |
|---|---|
| `CMakeLists.txt` | Biblioteket (länkar `kalkyl_db`, `kalkyl_platform`, `kalkyl_ancalc`, `kalkyl_conductor`, `kalkyl_linecalc`, `kalkyl_poletable` (PUBLIC, eftersom `project_data.h` inkluderar den), `kalkyl_tabletext` och `kalkyl_version`). Testet `projectdb` ligger i `tests/` |
| `project_db.h` | `ProjectDb`, `OpenOptions`, `Snapshot`, `Access`, `defaultWorkDir`, `withExtension` och `cleanWorkDir` |
| `dev_migration.h` | Utvecklingsmigreringen: stegen efter v0.1.0 (`dev::kStep2` med linkopiorna, Dokumentation och normalspannet, `dev::kStep3` med linberäkningarna, `dev::kStep4` med Stolptabell före). Töms vid 1.0.0 |
| `project_db.cpp` | Schemat (steg 1, v0.1.0, och stegen ur `dev_migration.h`), `create`, `open` med schemakontroll, lås, städning vid fel, `snapshot`/`publish`/`published`, revisionen och `close` |
| `project_data.h` | Dataskrivningarna och läsningarna: `DocValue`, `NormalSpan`, `LineCalc`, `Wire`, `Stage`, `docValues`, `setDocValue`, linkopiorna, beräkningarna, kopplingarna, upplösningen av linberäkningarna och Stolptabell före (`polesBefore`, `setPolesBefore`) |
| `project_data.cpp` | Implementationen av `project_data.h`: SQL i råa strängliteraler, en transaktion per skrivning |
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
- **Nytt projekt:** `created()` är sann för ett projekt från `create` (inte `open`), och följer med vid flytt. Huvudfönstret skriver då Dokumentationens förval.
- **Test:** tar 2 s extra, eftersom en andra öppning i samma process väntar ut den egna lås-regeln.

## Schemat och schemakontrollen

- **Steg 1** (`kSchemaV1`, v0.1.0) har bara `project` och ändras aldrig.
- **Steg 2** (`dev::kStep2` i `dev_migration.h`) lägger till `project_conductor`, `doc_value`, `normal_span`, `normal_span_pole` och `doc_normal_span`.
  Steg 2 är committat i v0.2.0 och ändras aldrig.
- **Steg 3** (`dev::kStep3`) lägger till `line_calc`, `line_calc_case` (15 rader per beräkning) och `doc_line_calc`.
  Steg 3 är committat i v0.3.0 och ändras aldrig.
- **Steg 4** (`dev::kStep4`) lägger till `pole_before`, Stolptabell före. `kSchemaVersion` = 4. Steget kan ändras fritt tills det committas.
  Ett projekt från en äldre version migreras när det öppnas och sparas med det nya schemat vid nästa publicering.
  Migreringstesten är `testMigrateFromV010`, `testMigrateFromV020` och `testMigrateFromV030` (filen byggd som i v0.3.2, user_version 3).
- **Schemakontroll:** `open` räknar tabellerna i `kExpectedTablesSql` efter `migrate` och före `quickCheck`. Saknas någon (skadad fil) blir felet `NotADatabase`
  med texten "Projektfilen saknar delar som den här versionen av Kalkyl behöver. Filen är skadad:" följd av en radbrytning och sökvägen.
  Låset och arbetskopian städas som vid andra fel, och texten går inte genom `relabel`. Listan hålls lika med schemat.
- **Resultatet lagras aldrig.** aₙ räknas med `ancalc::calculateNormalSpan` från stolparna som `normalSpan` läser.

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
- **Nycklar:** de sex normalspannsnycklarna ges av `normalSpanDocKey(wire, stage)` och de sex linberäkningsnycklarna av `lineCalcDocKey(wire, stage)`
  (`lineCalcRowOf` är motsatsen). Övriga nycklar i `doc_value` är fria.

## Stolptabell före

- **Lagring:** `pole_before(position, pole, av, ah, span_m, coord_n, coord_e, coord_h, lkl)` har en rad per stolpe i tabellens ordning (position 0, 1, 2 ...). Tabellen har ingen kolumn för skede.
- **Talen är TEXT:** Av, Ah, spannlängd och koordinaterna lagras som kanonisk decimaltext (punkt, inga tusentalsavgränsare, användarens decimaler kvar, `poletable::acceptField`).
  Aldrig REAL, eftersom decimalerna annars går förlorade. En tom cell är NULL i talkolumnerna, och `pole` och `lkl` är text med `''` som tom.
- **Läsning:** `polesBefore` ger raderna i ordning. NULL blir tom text. En tom tabell ger en tom lista.
- **Skrivning:** `setPolesBefore` ersätter hela tabellen i EN transaktion med `poletable::rowsToSave(rows)`. Tomma rader mitt i sparas, tomma rader sist sparas inte.
  `poletable::firstInvalidColumn` kontrollerar varje talfält före allt annat: ett tal som inte är kanonisk text (till exempel `6512240,85`, `abc` eller `1e5`) ger `Invalid`
  ("Stolptabellen har ett ogiltigt tal på rad <n> i kolumnen <namn>.", raden 1-baserad i indatat) och inget ändras. Skrivskyddat ger `AccessDenied`.

## Linberäkningarna

- **Lagring:** `line_calc` har en rad per beräkning och `line_calc_case` 15 rader (case_id 1–15). Källor och läge lagras som text, manuell last som två kolumner som båda är NULL eller båda har ett tal.
  Resultatet lagras aldrig, det räknas med `calculateResolved`. En okänd text eller färre än 15 lastfall ger `Invalid` ("Linberäkningen <id> i projektfilen är skadad.").
- **Eget lindata:** beräkningen lagrar sin egen kopia av linan (`cond_*`) och rör aldrig `project_conductor`, så `pruneConductors` är oförändrad.
- **Kopplad och manuell:** `doc_source` är nyckeln för raden som beräkningen hämtar sina indata ur, och NULL betyder manuell.
  En kopplad beräkning lagrar NULL i `cond_*`, `normal_span_m` och `tension_mpa` (vad `input` än innehåller) och läses tillbaka med dem tomma.
  `min_temp_c` och `max_temp_c` är det senast hämtade värdet och används aldrig som reserv. Den hör till exakt en rad, en manuell kan visas på flera.
- **Detach:** en kopplad beräkning som förlorar sin rad blir manuell med Dokumentationens värden i det ögonblicket som egna (`detachLineCalc`, `linkLineCalc` på en rad med en kopplad beräkning och `unlinkDocRow`).
  `detachLineCalc` låter kopplingen stå kvar, `unlinkDocRow` tar bort den.
- **Kopplingar:** `doc_line_calc(doc_key, calc_id)` har `doc_key` som primärnyckel. En koppling tömmer radens manuella värde i `doc_value` (referensen står kvar).
  `saveLineCalc` skriver beräkningen och lastfallen i en transaktion och kopplar den, och `removeLineCalc` tar bort lastfall och kopplingar med kaskaden.
- **Upplösningen:** `docLineSources` läser Dokumentationens värden, projektets linor och aₙ för varje kopplad normalspannsrad en gång.
  `resolveLineInput` är ren och sätter in lina, aₙ, σ₀, min och max (aₙ: kopplad beräkning, radens manuella värde, gröna raden). Det som saknas ger en Missing-upplysning med en text om var värdet fylls i.
  Topplina höger utan rad eller med "Samma" löses upp som vänster. `calculateResolved` ger inget resultat när upplösningen har en upplysning.
