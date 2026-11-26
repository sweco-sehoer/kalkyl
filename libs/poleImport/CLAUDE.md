# libs/poleImport

Importen av en stolptabell från Excel som rena funktioner, utan I/O, databas och fönster: rubrikområdet, de sammansatta rubrikerna,
poängen och valet av kolumner, utdraget av stolparna, rådataposten, problemen med importen och sträckorna som tappar en stolpe.
Biblioteket heter `kalkyl_poleimport`, namnrymden `kalkyl::poleimport` och includes skrivs `"poleImport/pole_import.h"`.
Det beror på `xlsx`, `poleTable` och `autoMode` (PUBLIC) och `tableText` (PRIVATE).
Dialogen `src/windows/pole_import_dialog.*` och huvudfönstret använder funktionerna. Planen är `doc/plans/archive/2026-11-26_Import_stolptabell.md`, avsnitt 3.3–3.7 och 5.

| Fil | Innehåll |
|---|---|
| `CMakeLists.txt` | Biblioteket `kalkyl_poleimport` (testet `poleimport` ligger i `tests/`) |
| `pole_import.h` | Gränssnittet: `Mapping`, `Learned`, `HeaderArea`, `SourceColumn`, `Confidence`, `Candidate`, `FieldMatch`, `Recognition`, `normalizeHeader`, `findHeaderArea`, `sourceColumns`, `scoreField`, `recognize`, `bestSheet`, `mappingOf`, `confidenceText`, `SourceRow`, `Extraction`, `extract`, `poleRows`, `importRecord`, `Issue`, `importIssues` och `lostRanges` |
| `recognize.cpp` | Normaliseringen, rubrikområdet, de sammansatta rubrikerna, poängtabellen, dataprovet, inlärningen, valet och `bestSheet` |
| `extract.cpp` | Utdraget, `poleRows`, rådataposten, `importIssues` och `lostRanges` |
| `internal.h` | Små hjälpfunktioner som bara `recognize.cpp` och `extract.cpp` delar (`cellAt`, `dataStartRow`, `rowCells`, `isDashOnly`, `collapseSpaces`, `valueKind`) |

Rådataposten `poletable::ImportRecord` ligger i `libs/poleTable/import_record.h`, eftersom `projectdb` och fönstret delar den.

## Regler

- **Rena funktioner:** inget tillstånd, ingen I/O och inget fönster. Kolumner är 1-baserade (A = 1), och `Mapping` har 0 = hämtas inte och -1 = inte valt än.
- **Kolumnerna känns igen på rubrikernas innehåll, aldrig på bokstäverna.** Rubrikerna är bara ord; Excelkolumnen är resultatet.
- **Rubrikområdet** (`findHeaderArea`): kolumnnummerraden (minst 8 heltal som är kolumnnumret plus samma k, och minst 80 % av radens talceller) är den starka signalen.
  Området är raderna ovanför den (en tom rad stoppar, högst 8 rader). Utan kolumnnummerrad används ankarraden, raden med flest celler som ensamma ger minst 50 poäng (minst 2),
  och området växer högst 3 rader uppåt och högst 3 rader nedåt över rader som inte ser ut som data. Utan ankare är området första raden som inte är tom.
- **De sammansatta rubrikerna** (`sourceColumns`): rubrikraderna uppifrån, sammanslagna celler ifyllda (sammanslagningen ger sin text åt alla kolumner och rader den täcker),
  delarna trimmade med blanktecken ihop till ett blanksteg, tomma delar och upprepningar borttagna. `label` skrivs med " › ", `key` är de normaliserade delarna med " | ". Kolumnnummerraden ingår inte.
- **Normaliseringen** (`normalizeHeader`): gemener, å/ä/ö viks till a/o, skiljetecken bort, bokstäver och siffror skiljs åt och förkortningarna byts ord för ord (tabellen `kAbbreviations`).
- **Poängen** (`scoreField`, tabellen i `fieldRules`): en grundregel ("en del = {…}" eller "alla {…}"), plus och minus för orden som finns, summan 0–100 och ett skäl med å, ä och ö.
  Reglerna skiljer de nära varianterna åt (Max och Min temp, Slutgiltigt och Byggnads, Profil och Beräknat från koordinater, N/E/H under Koordinater).
- **Dataprovet** (i `recognize`): en talkolumn med färre tal än hälften av värdena får −40, Stolpnr utan värden −30. Text med radbrytning eller tabb mitt i är aldrig ett tal (`tabletext::normalizeDecimal` vägrar den).
  En cell med bara ett streck ("-", "–", "—" eller "/") är tom.
- **Inlärningen:** en kolumn vars `key` finns i `Learned` för fältet får `kLearnedScore` och säkerheten Ditt val, utom när fler kolumner har samma inlärda rubrik (då Välj).
- **Valet:** giriga par efter poäng (minst `kMinScore`), varje fält och kolumn högst en gång. Säker kräver minst `kSureScore` och `kSureMargin` till näst bästa kolumn som ingen annan har tagit, Trolig kräver `kLikelyMargin`, annars Välj (kolumn -1).
  Ett fält utan par är Saknas (kolumn 0, men -1 för Stolpnr).
- **Utdraget** (`extract`): dataraderna till bladets slut. Rader utan Stolpnr listas i `skippedRows` när en annan vald kolumn har ett värde, annars tyst.
  En upprepad rubrikrad hoppas över tyst. Ett tal som inte går att läsa lämnar fältet tomt och texten i `invalid`; "-" är tomt utan anmärkning. Ingen Stolpnr-kolumn ger inga rader.
- **Rådataposten** (`importRecord`): alla celler på de importerade raderna (också de som inte används), kolumnerna med rubrik eller cell och fältet varje kolumn gav, titel, version, rubrikområdet och sträckan. `importedUtc` sätts av `projectdb`.
- **Problemen** (`importIssues`): texterna i planen 9.3 ordagrant, de som stoppar först. Överhoppade rader räknas bara inom den valda sträckan.
- **`lostRanges`:** bara sträckor som har start och slut och fungerar i nuvarande tabell, men tappar en stolpe i filen. Texten är `automode::findRange` med tabellen "filen".
- **En ny regel får ett test med en fälla** i `tests/poleimport_test.cpp`: en kolumn med nästan samma rubrik som ska väljas bort.
