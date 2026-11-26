# Import av stolptabell från Excel: knappen Importera stolptabell… i Stolptabell före, kolumnigenkänning och rådata i projektet

- **Datum:** 2026-11-26.
- **Status:** klar för implementering. Användarens beslut, de tre tilläggen (intelligent kolumnigenkänning, importen rör bara raderna i före, rådata i projektdatabasen) och svaren på frågorna i avsnitt 17 (2026-11-26) är inarbetade. Dialogen har inget val av blad: bara bladet som `bestSheet` väljer läses.
- **Version:** 0.5.0, som redan står i rotens `CMakeLists.txt`. Versionen höjs inte och ingen rör den.
- **Underlag:**
  - Koden: `libs/poleTable/pole_table.*`, `libs/poleGrid/pole_grid.h`, `libs/autoMode/auto_mode.h`, `libs/projectdb/project_data.*`, `project_db.cpp` och `dev_migration.h`, `libs/systemdb/system_db.*`, `libs/platform/files.h`, `libs/tableText/table_text.h`, `src/windows/main_window.*` och `conductor_dialog.*`, `src/CMakeLists.txt`, rotens `CMakeLists.txt` och `tests/CMakeLists.txt`.
  - Planen `doc/plans/archive/2026-11-26_Automatiskt_lage.md`, särskilt avsnitt 8, 14 och 19.
  - GlazeUI v0.11.1, avsnitten `file-dialog`, `widget-file-picker`, `app-window`, `widget-dropdown`, `widget-data-grid`, `message-box`, `threads` och `stock-icons` i `build/_deps/glazeui-src/MANUAL.md`.
  - Exempelfilen `ref/Stolptabell.xlsm` (gitignorerad, en fejkad rad). Den används aldrig i tester.
  - miniz på GitHub: senaste taggen är `3.1.2` (kontrollerat med `git ls-remote`), MIT-licens.

Planen är kontraktet för fyra implementeringspaket och för granskningen.
- **Bindande:** headrarna i 4.1, 5.1, 6.2, 7 och 8.2, schemat i 6.1 och texterna i avsnitt 9. Namn, signaturer och texter följs ordagrant.
- **Ändrade signaturer:** behöver ett paket ändra en signatur säger det till i rapporten, och samordnaren beslutar.

**Användarens beslut (bindande):**
1. Knappen "Importera stolptabell…" finns bara i Stolptabell före. Efter får sina rader med "Kopiera från Stolptabell före…" som i dag.
2. Av hämtas ur W (Max temp). Avstånd föregående hämtas ur U; är U tom lämnas cellen tom, och Kalkyl räknar spannet ur koordinaterna (Σ, avsnitt 19.1 i planen för automatiskt läge).
3. Importen ersätter raderna i Stolptabell före, med en fråga när tabellen inte är tom.
4. Användaren väljer alla stolpar (förval) eller en sträcka Från–Till bland stolparna i filen.
5. Dokumentationens uppgifter (faslina, uppspänning, temperatur, topplina) importeras inte. De kommer ur ByggPM.
6. miniz får användas, hämtat med FetchContent på en fast tagg och statiskt länkat.
7. **Intelligent import:** kolumnerna känns igen på sammansatta rubriker med normalisering, synonymer och poäng, också när de har flyttats eller döpts om. Osäkerhet syns i dialogen och rättas med en rullista per fält, och förhandsvisningen visar raderna med de valda kolumnerna.
8. **Importen rör bara raderna i Stolptabell före.** Sträckorna (Från och Till per lina) och läget står kvar, eftersom de sparas som Stolpnr-text. En sträcka vars stolpe saknas i den nya filen behålls och visar felet som i dag, och frågan före importen säger vilka sträckor det gäller. Stolptabell efter, Dokumentation, linkopiorna och de lagrade beräkningarna rörs inte, utom synkningen av de automatiska beräkningarna som `setPoles` alltid kör.
9. **Rådata sparas i projektet:** hela raden (alla kolumner) för varje importerad stolpe, kolumnernas rubriker, filnamnet, bladet, versionen, tidpunkten och vilka kolumner som gav de åtta fälten. Det läggs i det pågående steg 5. En ny import ersätter rådata i samma transaktion, och handändringar rör dem inte. Inget i gränssnittet visar dem nu.

---

## 1. Mål och avgränsning

### Kraven
| # | Krav | Var |
|---|---|---|
| K1 | Knappen Importera stolptabell… står bara i Stolptabell före. | 8.3 |
| K2 | Filen är en .xlsx eller .xlsm ("Stolptabell i Banken Ver. 6"). Den kan vara öppen i Excel, och makron körs aldrig. | 3.1, 4 |
| K3 | Kolumnerna känns igen på sina sammansatta rubriker, också när de har flyttats eller döpts om. Fält som inte är säkra syns och går att rätta per fält. | 3.3, 5 |
| K4 | Stolpnr = R (Slutgiltigt), N, E och H = I, J och K, Avstånd föregående = U, Ah = V, Av = W (Max temp) och LKL = AG. De beräknade kolumnerna L och N och Min temp (X) används inte. | 5.5, bilaga A |
| K5 | Alla stolpar (förval) eller en sträcka Från–Till. | 8.2 |
| K6 | Importen ersätter raderna i före och frågar när före har rader. | 8.2, 9.4 |
| K7 | Importen rör bara raderna i före (beslut 8). | 3.8, 6.3 |
| K8 | Frågan nämner sträckor som tappar sin start eller sitt slut. | 5.9, 9.4 |
| K9 | Dokumentationens uppgifter importeras inte. | 1, Ingår inte |
| K10 | Rådata sparas i projektet, ersätts vid ny import och rörs inte av handändringar. | 3.9, 6 |
| K11 | Ett val som användaren gör minns till nästa fil med samma rubrik. | 3.10, 7 |
| K12 | miniz hämtas på en fast tagg och länkas statiskt, och bygget är fritt från varningar. | 3.1, 11 |
| K13 | Testerna använder aldrig `ref/`. | 10 |
| K14 | Versionen är 0.5.0 och ändras inte. | – |

### Ingår
- **`libs/xlsx`** (nytt bibliotek): läser celler, delade strängar, talformat och sammanslagningar ur .xlsx och .xlsm med miniz och en egen liten XML-läsare.
- **`libs/poleImport`** (nytt bibliotek): rena funktioner för rubrikområdet, de sammansatta rubrikerna, normaliseringen, poängen, valet av kolumner, utdraget av stolparna, rådataposten och sträckorna som tappar en stolpe.
- **`libs/poleTable/import_record.h`**: typerna för rådata, som `poleImport`, `projectdb` och fönstret delar.
- **`libs/platform`**: `readFileShared`, läsning med delad åtkomst.
- **`libs/projectdb`**: fyra nya tabeller i steg 5, `importPolesBefore` och `lastPoleImport`.
- **`libs/systemdb`**: senaste importmappen och de inlärda rubrikerna, utan schemaändring.
- **Dialogen** `src/windows/pole_import_dialog.*` och knappen i huvudfönstret.
- **Tester** i `tests/`, med xlsx-filer som testerna bygger själva i minnet.

### Ingår inte
- Äldre .xls, .csv och .ods. En .xls ger ett tydligt fel (9.1).
- Import till Stolptabell efter och import av Dokumentationens uppgifter.
- Att visa rådata i gränssnittet (en idé i `TODO.md`).
- Sammanslagning med befintliga rader och ångra.
- Att starta Excel eller köra makron.

---

## 2. Nuläge och exempelfilen

- **Huvudfönstret** har `PoleHost::savePoles` → `projectdb::setPoles`, `PoleGrid::setRows`, `refreshAutomatic` och `setPoleStatus`. `copyBeforeToAfterAsked` är förebilden för fråga och ersättning.
- **Systemdatabasen** har redan `lastFolder`/`setLastFolder` (nyckeln `last_folder` i tabellen `setting`). `main.cpp` sätter den till projektets mapp, och projektväljaren använder den. Tabellen `setting` är nyckel och värde, så nya inställningar kräver ingen migrering.
- **Projektdatabasen** har steg 1–5. Steg 5 (`pole_after`, `auto_mode`, `auto_calc`) är inte committat och får ändras. `kExpectedTablesSql` räknar 13 tabeller.
- **`tabletext::normalizeDecimal`** tar "7047024,75", "7 047 024,75" och "422" och ger kanonisk decimaltext. **`poletable::acceptField`** trimmar Stolpnr och gör LKL med `normalizeLkl`.
- **Exempelfilen** (läst med zipfile):
  - Delarna är deflate-komprimerade. `xl/vbaProject.bin` finns (makron) och läses aldrig.
  - `xl/workbook.xml` har ett blad, "Stolptabell", via `r:id` och `xl/_rels/workbook.xml.rels`. Bladet är `xl/worksheets/sheet1.xml`, 2,1 MB uppackat, dimension A1:CK1112.
  - Nästan alla celler är delade strängar (`t="s"`). Talen i dataraden är text med decimalkomma ("7047024,75"). Tomma celler pekar ofta på en tom delad sträng (`<t/>`), och raderna 20–1112 är tomma rader med bara format.
  - Kolumnnummerraden 16 har riktiga tal (`<v>2</v>` i B16 … `<v>89</v>` i CK16), alltså kolumnens nummer.
  - Radbrytningar i celler står som CR LF i texten ("T373", CR LF, CR LF, "T373"). Två delade strängar är rik text med flera `<r>`.
  - 82 sammanslagningar i rubrikraderna, till exempel I11:O11 "Koordinater", I12:I14 "N", W12:X12 "Av", R12:S12 "Stpl nr" och U12:U14 "Avst föreg stolpe".
  - Rad 1 (dold) B1: "   Stolptabell i Banken Ver. 6 GRUNDLÄGGNING & JORDNING". B2 är rik text: "Stolptabell med data hämtad från IFS (Linjär tillgång). Mall Ver. 11.0 (svk)". Rad 10 är tom.
  - Bladet har bladskydd och villkorsstyrd formatering, som inte påverkar läsningen.
- **De sammansatta rubrikerna** i exempelfilen (rad 11–15, sammanslagningar ifyllda, upprepningar borttagna) står i bilaga A. De som avgör valet:
  - I "Koordinater › N › SWEREF 99 TM", J "… › E › …", K "Koordinater › H › RH 2000".
  - L "Koordinater › Beräknat från koordinater › Avst. föreg. › m" och N "Koordinater › Beräknat från koordinater › Ah › m" (används inte).
  - R "Profil › Stpl nr › Slutgiltigt" och S "Profil › Stpl nr › Byggnads".
  - U "Profil › Avst föreg stolpe", V "Profil › Ah › m", W "Profil › Av › Max temp › m" och X "Profil › Av › Min temp › m".
  - AG "Stolpe › Längdklass", och fällorna BH "Stag / Stagförankring › Stagförankring › Mått från typritning › E › cm" och BP "Linor / Isolation › Faslinor › Max temperatur › °C".

---

## 3. Beslut med motivering

### 3.1 Zip med miniz, bara källorna, eget mål
- **miniz 3.1.2** hämtas med FetchContent (`GIT_TAG 3.1.2`, `GIT_SHALLOW`), men minizs egen `CMakeLists.txt` används inte (`SOURCE_SUBDIR` pekar på en katalog som inte finns). Skälen:
  - Den kräver `cmake_minimum_required(VERSION 3.5)`, och CMake 4.4 varnar då vid konfigureringen.
  - Den lägger `-Wall -pedantic` i `CMAKE_C_FLAGS` och skapar cachevariabler med allmänna namn (`BUILD_TESTS`, `BUILD_EXAMPLES`, `BUILD_SHARED_LIBS`).
- **`libs/xlsx/CMakeLists.txt` bygger `kalkyl_miniz`** (STATIC) av `miniz.c`, `miniz_zip.c`, `miniz_tinfl.c` och `miniz_tdef.c`. `miniz_export.h` skrivs av CMake. Målet har `-w` (tredjepartskod, som `SYSTEM` för GlazeUI), headrarna är `SYSTEM`, och definitionerna `MINIZ_NO_STDIO`, `MINIZ_NO_TIME` och `MINIZ_NO_ZLIB_COMPATIBLE_NAMES` hindrar att miniz krockar med något annat.
- **Varför inte zlib ur MSYS2:** zlib packar bara upp. Zip-containern (central katalog, zip64) skulle behöva skrivas själv. miniz har båda, och testerna behöver också skriva zip.
- **Varför inte Windows-skalet:** COM-zip packar upp till disk, är långsamt och går inte att testa headless.

### 3.2 XML med en egen liten läsare
- En pull-läsare på cirka 300 rader (`libs/xlsx/xml_reader.*`) räcker, eftersom bara några delar läses: `.rels`, `workbook.xml`, `sharedStrings.xml`, `styles.xml` och bladen (rader, celler, värden och sammanslagningar).
- Den känner till taggar, attribut, namnrymdsprefix (jämför lokala namn, så att `x:c` och `c` är samma), entiteter (`&amp;`, `&#228;`, `&#xE4;`), CDATA, kommentarer och processinstruktioner. En DOCTYPE vägras, så inga entitetsdefinitioner kan expanderas.
- **Varför inte pugixml eller expat:** det vore ett beroende till som inte är godkänt. Vi behöver ändå egen kod för OOXML:s särdrag (`_xHHHH_`, radslut och fonetiska körningar), och läsaren testas genom `xlsx_test`.

### 3.3 Kolumnerna känns igen på rubrikernas innehåll, inte på bokstäverna
- **En kolumns rubrik** är rubrikradernas text uppifrån (grupp, rubrik, underrubrik, enhet). En sammanslagen cell ger sin text åt alla celler den täcker.
- **Normaliseringen** gör gemener, viker å, ä och ö till a och o, tar bort skiljetecken och byter förkortningar mot hela ord (5.4).
- **Poängen** (5.5): varje fält har grundregler, plus och minus. Reglerna skiljer de nära varianterna åt: Max temp och Min temp, Slutgiltigt och Byggnads, Profil och Beräknat från koordinater, och N, E och H under Koordinater.
- **Dataprovet** sänker poängen för en talkolumn som mest har text.
- **Valet** fördelar kolumnerna giriga efter poäng, så att varje kolumn ger högst ett fält. Säkerheten är Säker, Trolig, Välj (tvetydig), Saknas eller Ditt val (5.6).
- **Rubrikområdet och första dataraden söks:** kolumnnummerraden (2, 3, 4 …) är den starka signalen, och utan den används raden med flest träffar på fältens ord (5.2).
- **Användaren ser och rättar:** dialogen visar Excelkolumnen (bokstav och rubrik), säkerheten och skälet per fält, och en rullista per fält byter kolumn.

### 3.4 Filens version visas men stoppar inget
- Raderna ovanför rubrikerna söks efter en text med "Stolptabell" och "Ver. N". Den första träffen ger titeln och versionen ("Ver. 6").
- **Ver. 6 är provad** (`kKnownVersions`). En annan version eller en fil utan titel importeras ändå, med texten "versionen är inte provad, kontrollera kolumnerna".
- **Skälet:** användaren vill kunna läsa nya versioner och andra projekts tabeller, och det är kolumnigenkänningen som avgör. Stolpnr måste ha en kolumn, annars går det inte att importera.

### 3.5 Ett ogiltigt tal lämnas tomt och syns
- En talcell som inte är ett tal ("ca 300", "?", "422 m") lämnas tom, och importen fortsätter. Cellen är röd i förhandsvisningen med texten i tooltipen, och antalet står under förhandsvisningen och i statusraden efteråt.
- **En cell med bara ett streck** ("-", "–", "—" eller "/") är tom utan anmärkning, eftersom Banken använder streck för "ingen uppgift".
- **Skälet:** att vägra hela importen för en anteckning i en cell vore hårt. Spannet räknas ur koordinaterna när Avstånd föregående blir tomt, och användaren ser exakt vilka celler som påverkas.

### 3.6 Dubbletter av Stolpnr importeras och varnas
Stolptabellerna tillåter redan dubbletter och markerar dem röda (`automode::duplicatePoles`). Dialogen nämner dem, och de syns röda i tabellen efter importen.

### 3.7 Raderna hittas till bladets slut
- **Dataraderna** är raderna efter rubrikområdet till bladets sista rad. Tomma rader mitt i hoppas över, så importen slutar inte vid första tomma rad.
- **En rad utan Stolpnr** men med andra värden i de valda kolumnerna hoppas över och listas i dialogen. Helt tomma rader hoppas över tyst.
- **En upprepad rubrikrad** (Stolpnr-cellen har samma text som en av kolumnens rubrikdelar, till exempel "Stpl nr") hoppas över tyst.
- **Skälet:** Banken-filen har tusen formaterade tomma rader, och avdelande tomma rader förekommer. Att läsa allt kostar ingenting, och Till väljer bort det som inte ska med.

### 3.8 Bara raderna i före byts, i en transaktion
- `projectdb::importPolesBefore(p, rows, record)` skriver raderna som `setPoles(p, Stage::Before, rows)` och ersätter rådata. Synkningen av de automatiska beräkningarna körs sist, allt i EN transaktion.
- `auto_mode`, `pole_after`, `doc_value`, linkopiorna och de manuella beräkningarna rörs inte. Sträckorna är Stolpnr-text och hittar sina nya rader själva.
- Fönstret läser inte om inställningen för automatiskt läge, eftersom den inte ändras.

### 3.9 Rådata i fyra tabeller
- **Tabellerna:** `pole_import` (en rad), `pole_import_column`, `pole_import_row` och `pole_import_cell` (rad, kolumn, slag och text), se 6.1.
- **Skälen till schemat:**
  - Det är enkelt att fråga i SQL per stolpe, kolumn eller fält, med vanliga join (exemplet i 6.3) och utan att tolka text.
  - Cellens slag (text, tal, sant/falskt, fel) följer med, så att ett datum eller ett tal går att tolka senare.
  - `WITHOUT ROWID` håller cellerna kompakta.
  - En text per rad (tabbseparerad) vore mindre men måste tolkas för varje fråga och tappar slaget.
- **Bara de importerade raderna sparas**, alltså sträckan Från–Till. De motsvarar raderna i Stolptabell före ett till ett (`position`), och data utanför sträckan hör inte till projektet. Behövs en större sträcka görs en ny import, som ersätter rådata.
- **Bara filnamnet sparas**, inte hela sökvägen. Projektet ligger på en share och öppnas av kollegor, för vilka "C:/Users/<namn>/Downloads/…" saknar mening och visar ett användarnamn.
- **Texten sparas som den står i filen:** strängen (radbrytningar som LF, inte trimmad), talet som i `<v>` med punkt och alla decimaler ("422.10000000000002"), sant/falskt som "1"/"0" och felet som "#N/A".
- **Storleken:** 1 000 stolpar med 60 ifyllda kolumner ger cirka 60 000 celler och 2–3 MB i projektfilen. Skrivningen använder en förberedd sats per tabell och tar under en halv sekund.

### 3.10 Inlärda rubriker ingår, i systemdatabasen
- Väljer användaren en kolumn själv och importerar, sparas kolumnens normaliserade rubrik för fältet (`systemdb::learnHeader`). Vid nästa import får en kolumn med exakt den rubriken 100 poäng och säkerheten "Ditt val".
- **Per fält och rubrik, inte per hel rubrikuppsättning:** då gäller valet också när andra kolumner har lagts till eller flyttats.
- **Per användare** (systemdatabasen), högst tio rubriker per fält, den senaste först. Nyckeln är `pole_import_learned` i tabellen `setting`, så ingen migrering behövs.
- **Skälet att det ingår:** arbetet är litet (två funktioner i `systemdb`, en parameter till `recognize` och några rader i dialogen), och nästa fil av samma slag går då direkt.

### 3.11 Filen läses i bakgrunden
Läsningen och igenkänningen körs i en `glaze::threads::Worker` i dialogen, eftersom filen kan ligga på en share. Dialogen visar "Läser …" och väntepekaren, och Importera är släckt tills svaret kommit.

### 3.12 Flödet: filväljaren först, frågan i dialogen
- **Knappen** öppnar huvudfönstrets filväljare, och därefter dialogen med filen. I dialogen går det att byta fil. Bladet väljs av `bestSheet` (bladet "Stolptabell", annars det som mest liknar en stolptabell) och visas bara med namn; andra blad i filen syns inte (användarens beslut).
- **Frågan om ersättning** ställs när användaren trycker Importera i dialogen, så att ett Nej lämnar dialogen öppen med valen kvar.
- **Huvudfönstret skriver** med `importPolesBefore`, eftersom det äger tabellen, statusraden och omräkningen.

### 3.13 Rådataposten bor i `libs/poleTable`
- `poletable::ImportRecord` (ny header `poleTable/import_record.h`, bara typer) delas av `poleImport` (bygger posten), `projectdb` (sparar och läser den) och fönstret (skickar den vidare).
- `poleTable` beror bara på standardbiblioteket och delas redan av alla tre. Läge i `projectdb` skulle tvinga `poleImport` att bero på databasen, och läge i `poleImport` skulle tvinga `projectdb` att bero på xlsx och miniz.

---

## 4. Biblioteket `libs/xlsx`

Biblioteket heter `kalkyl_xlsx`, namnrymden `kalkyl::xlsx` och includes skrivs `"xlsx/xlsx.h"`. Det beror på `core` (PUBLIC) och `platform` och `kalkyl_miniz` (PRIVATE).

### 4.1 `libs/xlsx/xlsx.h` (bindande)
```cpp
#pragma once
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "core/result.h"

namespace kalkyl::xlsx {

// Cellens slag. En tom cell (inget värde eller en tom text) finns inte i Sheet::cells.
enum class CellKind { Text, Number, Bool, Error };

// En cell med värde. row och column är 1-baserade (A = 1). text är värdet som det står i filen:
// strängen (delad eller inline, rik text ihopfogad, radbrytningar som LF), talet som i <v> med
// punkt ("7047024.75", "422.10000000000002"), Bool som "1" eller "0" och Error som "#N/A". En
// formel ger sitt sparade resultat. decimals är decimalerna i cellens talformat ("0.00" ger 2),
// -1 för Allmänt eller ett format som inte är ett enkelt talformat.
struct Cell {
    int row = 0;
    int column = 0;
    CellKind kind = CellKind::Text;
    std::string text;
    int decimals = -1;
    bool operator==(const Cell&) const = default;
};

// En sammanslagning, till exempel W12:X12. Bara den övre vänstra cellen har värdet.
struct Merge {
    int firstRow = 0;
    int firstColumn = 0;
    int lastRow = 0;
    int lastColumn = 0;
    bool operator==(const Merge&) const = default;
};

// Ett kalkylblad: cellerna med värde i rad- och kolumnordning och sammanslagningarna.
struct Sheet {
    std::string name;
    bool hidden = false;            // state "hidden" eller "veryHidden" i workbook.xml
    std::vector<Cell> cells;
    std::vector<Merge> merges;
    int lastRow = 0;                // största rad med en cell, 0 = tomt blad
    int lastColumn = 0;             // största kolumn med en cell
    // Cellen på raden och kolumnen, eller nullptr när den är tom. Binärsökning i cells.
    const Cell* find(int row, int column) const;
};

// Arbetsboken: kalkylbladen i bokens ordning. Diagramblad och makroblad tas inte med.
struct Workbook {
    std::vector<Sheet> sheets;
};

// Läser en .xlsx eller .xlsm ur minnet. fileName står i felens text. Makron och allt annat än
// relationerna, bladen, de delade strängarna, talformaten och sammanslagningarna läses aldrig.
// Fel (Invalid, texterna i planen 9.1): inte en zip, en äldre .xls eller lösenordsskyddad fil,
// skadad zip eller XML, inga kalkylblad, en del som är för stor uppackad.
core::Result<Workbook> readWorkbook(std::string_view bytes, std::string_view fileName);

// Läser filen med delad läsning (platform::readFileShared, högst kMaxFileBytes), så att den kan
// vara öppen i Excel, och sedan readWorkbook. Felen har filens namn i texten (planen 9.1).
core::Result<Workbook> openWorkbook(const std::string& utf8Path);

// Gränserna: filen på disk och varje uppackad del.
inline constexpr std::uint64_t kMaxFileBytes = 64ull * 1024 * 1024;
inline constexpr std::uint64_t kMaxPartBytes = 256ull * 1024 * 1024;

// Kolumnens bokstäver: 1 = "A", 27 = "AA", 89 = "CK". Tom text utanför 1-16384.
std::string columnLetters(int column);

// En cellreferens som "AB12" eller "$AB$12" som {rad, kolumn}. Tomt svar när den inte är giltig
// (rad 1-1048576, kolumn 1-16384).
std::optional<std::pair<int, int>> parseCellRef(std::string_view ref);

// Decimalerna i ett talformat: "0.00" ger 2 och "#,##0" ger 0. Allmänt, procent, exponent, datum,
// tid, text och andra format ger -1.
int formatDecimals(std::string_view formatCode);

// Talet som Excel visar det: med decimals decimaler, eller med decimals -1 som formatet Allmänt
// (högst 15 värdesiffror, utan nollor sist). Punkt som decimaltecken och ingen exponent.
// Tomt svar när text inte är ett tal.
std::optional<std::string> numberText(std::string_view text, int decimals);

} // namespace kalkyl::xlsx
```
`<cstdint>` inkluderas för `std::uint64_t`.

### 4.2 `libs/platform/files.h`, tillägg (bindande)
```cpp
// Läser hela filen i minnet med delad åtkomst (GENERIC_READ och FILE_SHARE_READ, WRITE och DELETE),
// så att en fil som är öppen i Excel går att läsa. En fil som är större än maxBytes ger Invalid.
// Övriga fel som win32Error (NotFound, AccessDenied, Busy, Unreachable, Io).
core::Result<std::string> readFileShared(const std::string& path, std::uint64_t maxBytes);
```

### 4.3 Läsreglerna
1. **Filens typ** avgörs av de första byten:
   - "PK" följt av 3 och 4, eller 5 och 6, är en zip.
   - D0 CF 11 E0 A1 B1 1A E1 är en OLE-fil: en .xls eller en lösenordsskyddad .xlsx.
   - Allt annat är inte en Excelfil.
2. **Zip:** `mz_zip_reader_init_mem`. Varje del kontrolleras med `mz_zip_reader_file_stat` mot `kMaxPartBytes` innan den packas upp med `mz_zip_reader_extract_to_heap`, och minnet släpps med `mz_free`. Delnamn jämförs utan skillnad på stora och små bokstäver.
3. **Huvuddelen:** `_rels/.rels`, den relation vars Type slutar på "/officeDocument". Saknas den används "xl/workbook.xml".
4. **`workbook.xml`:** elementen `sheet` med `name`, `state` och relationens id (attributets lokala namn är "id", skilt från `sheetId`).
5. **Arbetsbokens relationer** (`xl/_rels/workbook.xml.rels`): Target tolkas relativt arbetsbokens mapp, eller från roten när den börjar med "/". "../" löses upp. Type som slutar på "/worksheet" är ett blad, "/sharedStrings" och "/styles" är de delade strängarna och formaten. Diagramblad och makroblad hoppas över.
6. **Delade strängar:** varje `si` är texten i `t` direkt under `si` och i `t` under varje `r`, ihopfogad. Fonetiska körningar (`rPh`) och `phoneticPr` hoppas över med allt de innehåller. Delen får saknas.
7. **Talformat** (`styles.xml`):
   - `numFmts/numFmt` ger `numFmtId` och `formatCode`. `cellXfs/xf` i ordning ger `numFmtId` per formatindex (cellens `s`).
   - De inbyggda id:na 1 och 3 ger 0 decimaler och 2 och 4 ger 2. Id 0 och övriga inbyggda ger -1, och egna format går genom `formatDecimals`.
   - Delen får saknas, och då är alla decimaler -1.
8. **`formatDecimals`:**
   - Första avsnittet före ";" används. Text inom citattecken, tecken efter ett bakvänt snedstreck och hakparenteser (färg, villkor) tas bort.
   - Finns något av "%", "E", "e", "@", "/" eller ett datum- eller tidstecken (y, m, d, h, s) kvar, eller är formatet "General", blir svaret -1.
   - Annars räknas nollorna direkt efter punkten. Utan punkt blir svaret 0.
9. **Bladet:**
   - `sheetData/row` har `r` (saknas den är det förra radnumret plus 1), och `c` har `r` (saknas den är det kolumnen efter förra cellen).
   - `t` styr slaget:
     - "s" är ett index i de delade strängarna; ett index utanför är en skadad fil.
     - "inlineStr" läser `is`, med `t` och `r/t` som i regel 6.
     - "str" (formelns text) och "d" (ISO-datum) ger Text.
     - "b" ger Bool och "e" ger Error.
     - "n" eller inget `t` ger Number.
   - Värdet står i `v`, och `f` (formeln) läses inte.
   - En cell utan värde eller med tom text sparas inte, och `s` ger `decimals` för ett tal.
10. **Sammanslagningar:** `mergeCells/mergeCell ref="W12:X12"`. En referens med bara en cell hoppas över.
11. **Text:**
    - Entiteterna avkodas först, sedan `_xHHHH_` (UTF-16, surrogatpar slås ihop), där `_x005F_` ger ett understreck så att "_x005F_x000D_" blir texten "_x000D_".
    - Sist blir CR LF och ensamt CR till LF.
12. **Ordning:** cellerna sorteras på rad och kolumn efter läsningen, och `lastRow` och `lastColumn` räknas.
13. **Inget annat läses:** `vbaProject.bin`, customXml, ritningar och skrivarinställningar öppnas aldrig.
14. **`numberText`:**
    - Med decimals ≥ 0 skrivs talet med så många decimaler.
    - Med -1 avrundas det till 15 värdesiffror och skrivs i kortaste form.
    - Svaret har aldrig exponent.
    - Exempel: "422.10000000000002" ger "422.1", "0.30000000000000004" ger "0.3", "1E-3" ger "0.001", "1.5E+20" ger "150000000000000000000", "-0" ger "0", och "6512000" med 2 ger "6512000.00".

---

## 5. Biblioteket `libs/poleImport`

Biblioteket heter `kalkyl_poleimport`, namnrymden `kalkyl::poleimport` och includes skrivs `"poleImport/pole_import.h"`. Det beror på `xlsx`, `poleTable` och `autoMode` (PUBLIC) och `tableText` (PRIVATE). Bara rena funktioner: inget tillstånd, ingen I/O och inget fönster.

### 5.1 `libs/poleImport/pole_import.h` (bindande)
```cpp
#pragma once
#include <array>
#include <string>
#include <string_view>
#include <vector>

#include "autoMode/auto_mode.h"
#include "poleTable/import_record.h"
#include "poleTable/pole_table.h"
#include "xlsx/xlsx.h"

namespace kalkyl::poleimport {

// Gränserna för valet (planen 5.6): lägsta poäng för en kandidat, säker och trolig.
inline constexpr int kMinScore = 40;
inline constexpr int kSureScore = 70;
inline constexpr int kSureMargin = 20;
inline constexpr int kLikelyMargin = 10;
// Poängen för en rubrik som användaren har valt förut.
inline constexpr int kLearnedScore = 100;
// Versionerna av "Stolptabell i Banken" som är provade.
inline constexpr std::string_view kKnownVersions[] = {"6"};

// Excelkolumn per fält i Stolptabell före (poletable::kPole … kLkl): kolumnens nummer (1 = A),
// 0 = hämtas inte, -1 = inte valt än (ett tvetydigt fält, eller Stolpnr som saknas).
using Mapping = std::array<int, poletable::kColumnCount>;
// Inlärda rubriker per fält: normaliserade rubriker (SourceColumn::key) som användaren valt förut.
using Learned = std::array<std::vector<std::string>, poletable::kColumnCount>;

// Rubrikområdet, 1-baserade rader. firstRow 0 = inget hittades. numberRow 0 = ingen kolumnnummerrad.
struct HeaderArea {
    int firstRow = 0;
    int lastRow = 0;
    int numberRow = 0;
    bool operator==(const HeaderArea&) const = default;
};

// En kolumn i bladet med sin sammansatta rubrik: rubrikraderna uppifrån, sammanslagna celler
// ifyllda, varje del trimmad med radbrytningar och blanktecken ihop till ett blanksteg, och tomma
// delar och upprepningar borttagna. label är delarna med " › " emellan ("" utan rubrik), key är
// normalizeHeader av varje del med " | " emellan.
struct SourceColumn {
    int column = 0;
    std::string letter;
    std::vector<std::string> parts;
    std::string label;
    std::string key;
};

// Hur säkert ett fält har sin kolumn: Säker, Trolig, Välj (tvetydig), Saknas och Ditt val.
enum class Confidence { Sure, Likely, Ambiguous, Missing, Chosen };

// En kolumn som passar ett fält: poängen och skälet ("avstånd föregående 60, stolpe +10, profil +10").
struct Candidate {
    int column = 0;
    int score = 0;
    std::string reason;
};

// Valet för ett fält: kolumnen (som i Mapping), säkerheten och de bästa kandidaterna (högst fem med
// poäng över 0, bäst först, också under kMinScore, så att tooltipen kan visa näst bäst).
struct FieldMatch {
    int column = 0;
    Confidence confidence = Confidence::Missing;
    std::vector<Candidate> candidates;
};

// Igenkänningen av ett blad.
struct Recognition {
    HeaderArea header;
    std::string title;                      // "Stolptabell i Banken Ver. 6 GRUNDLÄGGNING & JORDNING", "" utan
    std::string version;                    // "Ver. 6", "" utan
    bool knownVersion = false;              // versionen finns i kKnownVersions
    std::vector<SourceColumn> columns;      // kolumnerna med rubrik eller data, i kolumnordning
    std::array<FieldMatch, poletable::kColumnCount> fields;
    bool looksLikePoleTable = false;        // Stolpnr är Säker, Trolig eller Ditt val
};

// Rubriktext i normaliserad form (planen 5.4): gemener, å/ä/ö vikta, skiljetecken bort och
// förkortningar utskrivna. "Avst. föreg." ger "avstand foregaende".
std::string normalizeHeader(std::string_view text);

// Rubrikområdet (planen 5.2): kolumnnummerraden, annars raden med flest träffar på fältens ord.
HeaderArea findHeaderArea(const xlsx::Sheet& sheet);

// Kolumnerna med sammansatta rubriker ur rubrikområdet och sammanslagningarna (planen 5.3).
std::vector<SourceColumn> sourceColumns(const xlsx::Sheet& sheet, const HeaderArea& header);

// Regelpoängen 0-100 för ett fält och en kolumn, utan dataprov och inlärning (planen 5.5).
Candidate scoreField(int field, const SourceColumn& column);

// Hela igenkänningen: rubrikområdet, titeln, kolumnerna, poängen med dataprov och inlärning, och valet.
Recognition recognize(const xlsx::Sheet& sheet, const Learned& learned = {});

// Bladet som ser mest ut som en stolptabell: ett synligt blad som heter "Stolptabell" (normaliserat),
// annars det med flest fält som är säkra eller troliga. 0 för en bok utan blad.
int bestSheet(const xlsx::Workbook& book, const Learned& learned = {});

// Kolumnerna ur valet: fields[f].column per fält.
Mapping mappingOf(const Recognition& rec);

// "Säker", "Trolig", "Välj", "Saknas" och "Ditt val".
std::string confidenceText(Confidence confidence);

// En stolpe ur filen: Excels radnummer, raden som acceptField gav (ett ogiltigt tal är tomt) och
// den ogiltiga texten per fält ("" = giltig eller tom).
struct SourceRow {
    int excelRow = 0;
    poletable::PoleRow pole;
    std::array<std::string, poletable::kColumnCount> invalid;
};

// Stolparna i bladet med de valda kolumnerna (planen 5.7).
struct Extraction {
    int firstDataRow = 0;                // första raden med Stolpnr, 0 = inga stolpar
    std::vector<SourceRow> rows;         // raderna med Stolpnr, i filens ordning
    std::vector<int> skippedRows;        // rader utan Stolpnr men med andra värden
};
Extraction extract(const xlsx::Sheet& sheet, const HeaderArea& header, const Mapping& mapping);

// Raderna first..last (index i ex.rows, båda med) som rader i Stolptabell före.
std::vector<poletable::PoleRow> poleRows(const Extraction& ex, int first, int last);

// Importens rådata för raderna first..last: alla celler med värde på raderna, kolumnerna med
// rubrik eller data på raderna, fälten ur mapping (bara kolumner > 0), titel, version,
// rubrikområdet, bladets namn och fileName (bara namnet). importedUtc lämnas tom.
poletable::ImportRecord importRecord(const xlsx::Sheet& sheet, const Recognition& rec, const Mapping& mapping,
                                     const Extraction& ex, int first, int last, std::string fileName);

// Ett problem med importen. blocking = Importera går inte att trycka.
struct Issue {
    bool blocking = false;
    std::string text;
};
// Problemen i den ordning dialogen visar dem (planen 9.3): först de som stoppar, sedan de övriga.
std::vector<Issue> importIssues(const Recognition& rec, const Mapping& mapping, const Extraction& ex,
                                int first, int last);

// Sträckor som har en giltig start och ett giltigt slut i current men inte i imported, en rad per
// sträcka: "{Word}: " och felet ur automode::findRange med tabellen "filen", till exempel
// "Faslinan: Stolpnr ST0107 finns inte i filen.". Sträckor utan start eller slut räknas inte.
std::vector<std::string> lostRanges(const std::vector<automode::NamedRange>& ranges,
                                    const std::vector<poletable::PoleRow>& current,
                                    const std::vector<poletable::PoleRow>& imported);

} // namespace kalkyl::poleimport
```

### 5.2 Rubrikområdet (`findHeaderArea`)
1. **Kolumnnummerraden:** bland raderna 1–100 är det den första rad där minst 8 celler har ett heltal (tal, eller text med bara siffror) som är kolumnens nummer plus samma k, och de är minst 80 % av radens talceller.
   - Exempelfilen har rad 16 med B16 = 2 … CK16 = 89, alltså k = 0.
2. **Med kolumnnummerrad:** `lastRow` är raden ovanför, eller ovanför en eller två tomma rader. Uppåt hör raderna till området så länge de har minst en textcell, men högst 8 rader. En tom rad stoppar.
   - Exempelfilen ger 11–15, eftersom rad 10 är tom.
3. **Utan kolumnnummerrad:**
   - Ankarraden är den rad bland 1–100 som har flest celler vars normaliserade text ensam ger minst 50 poäng för något fält (`scoreField` med en kolumn som bara har den delen). Det krävs minst 2 träffar, och lika många ger den första raden.
   - Området växer uppåt över rader som inte är tomma, högst 3 rader. Nedåt växer det över rader som inte är tomma och inte ser ut som data, högst 3 rader. Data betyder minst 3 talceller, eller att minst hälften av cellerna är tal.
4. **Utan ankare:** området är bladets första rad som inte är tom, och `looksLikePoleTable` blir falsk om inte Stolpnr ändå känns igen.

### 5.3 De sammansatta rubrikerna (`sourceColumns`)
- **Kolumnerna** är alla som har en text i rubrikområdet eller en cell nedanför det.
- **Delarna:** för varje rubrikrad uppifrån används cellens text, eller texten i den övre vänstra cellen när cellen ingår i en sammanslagning som börjar i rubrikområdet. En sammanslagning ger alltså sin text åt alla kolumner och rader den täcker.
- **Rensning:** delarna trimmas, radbrytningar och blanktecken blir ett blanksteg, tomma delar tas bort och en del som är lik den närmast ovanför tas bort.
  - AC11 "Stolpe" och AC12:AJ12 "Stolpe" ger alltså en del.
- **Exempel:** W ger "Profil › Av › Max temp › m" och I ger "Koordinater › N › SWEREF 99 TM".
- **Utan sammanslagning** sprids en grupptext inte åt höger. Grupptexten är bara ett plus i poängen, så "N" känns ändå igen.
- **Kolumnnummerraden** ingår inte i rubriken.

### 5.4 Normaliseringen (`normalizeHeader`)
1. ASCII-bokstäver blir gemener.
   - Å, å, Ä, ä, À, à, Á och á blir a; Ö, ö, Ø, ø, Ó och ó blir o; É, é, È, è, Ë och ë blir e; Ü och ü blir u.
   - Andra tecken utanför ASCII blir blanksteg.
2. Allt som inte är en bokstav eller siffra blir blanksteg, också punkt, komma, snedstreck, parenteser, bindestreck, citattecken, °, ², hårt mellanslag och radbrytning.
3. Bokstäver och siffror skiljs åt ("rh2000" blir "rh 2000"). Blanksteg slås ihop och trimmas.
4. **Förkortningarna** byts ord för ord (tabellen `kAbbreviations` i `recognize.cpp`):

   | Ord | Blir |
   |---|---|
   | avst, avstand | avstand |
   | foreg, forega, foregaende | foregaende |
   | stpl, stolp, stolpe, stolpar, stolpens | stolpe |
   | nr, no, num, numr, nummer | nummer |
   | stolpnr, stplnr, stpnr, stolpnummer | stolpe nummer |
   | stolpplats | stolpe plats |
   | ordningsnr | ordning nummer |
   | temp, temperatur | temperatur |
   | koord, koordinat, koordinater | koordinat |
   | berakn, beraknat, beraknad, beraknade | berakna |
   | spannlangd, spannl | spann langd |
   | lkl, langdklass | langdklass |
   | slutgiltig, slutgiltigt, slutlig, slutligt | slutgiltig |
   | byggnad, byggnads | byggnad |
   | norr, nord, northing | n |
   | ost, east, easting | e |

5. **Varje del** normaliseras för sig. En dels ord är en mängd, och kolumnens ord är alla delars ord tillsammans.

### 5.5 Poängen (`scoreField` och dataprovet)
- **Grunden** är den högsta grundregel som passar. Plus och minus läggs till för varje ord som finns bland kolumnens ord, och summan hålls inom 0–100.
- **"En del = {…}"** betyder att någon dels ordmängd är exakt den. **"Alla {…}"** betyder att kolumnens ord innehåller alla.

| Fält | Grund | Plus | Minus |
|---|---|---|---|
| Stolpnr | alla {stolpe, nummer} 60; en del = {stolpe, plats} 50 | slutgiltig +30, profil +5 | byggnad −40, ordning −40, omvand −40 |
| Koordinat N | en del = {koordinat, n} 60; en del = {n} 50; en del = {x} 30 | koordinat +30, sweref eller (rt och 90) +15 | berakna −40 |
| Koordinat E | en del = {koordinat, e} 60; en del = {e} 50; en del = {y} 30 | som N | berakna −40 |
| Koordinat H | en del = {koordinat, h} 60; en del = {h} 50; en del = {hojd} 40; en del = {z} 30 | koordinat +30, rh +15 | berakna −40, till −30 |
| Avstånd föregående | alla {avstand, foregaende} 60; alla {spann, langd} 50; en del = {spann} 35 | stolpe +10, profil +10 | berakna −40, koordinat −20, total −40, station −30 |
| Ah | en del = {ah} 60 | profil +10, m +5 | berakna −40, koordinat −20 |
| Av | en del = {av} 60 | max +20, profil +10, m +5 | min −50, berakna −40 |
| LKL | alla {langdklass} 70; en del = {langd, klass} 70 | stolpe +10 | – |

- **Skälet** skrivs som grunden och varje plus och minus med poäng, till exempel "avstånd föregående 60, stolpe +10, profil +10" och "Av 60, max +20, profil +10, m +5". Orden visas med å, ä och ö (tabellen `kRuleTexts`).
- **Dataprovet** (i `recognize`) görs på raderna efter rubrikområdet till bladets slut:
  - En talkolumn (N, E, H, Avstånd, Ah och Av) med värden där färre än hälften är tal (text som `tabletext::normalizeDecimal` tar efter streckregeln i 3.5, eller ett tal) får −40, med skälet "mest text i kolumnen −40".
  - Stolpnr i en kolumn utan värden får −30, med skälet "inga värden −30".
- **Inlärningen:** en kolumn vars `key` finns i `learned[f]` får `kLearnedScore` för fältet, med skälet "ditt tidigare val".
- **Exempelfilen** ger:
  - Stolpnr R 95 och S 25.
  - N, E och H i I, J och K 95 vardera; BH (E) 10 efter dataprovet.
  - Avstånd föregående U 80 och L 0.
  - Ah V 75 och N 5.
  - Av W 95 och X 25.
  - LKL AG 80.
  - Alla blir Säker.

### 5.6 Valet (`recognize`)
1. **Paren:** alla par (fält, kolumn) med poäng ≥ `kMinScore` sorteras på poäng (lika: fältets ordning, sedan kolumnens). Varje par tas när varken fältet eller kolumnen är tagna.
2. **Säkerheten** för ett fält med kolumn c och poäng s räknas mot näst bäst, den högsta poängen för fältet bland kolumner som inte har tagits av ett annat fält (0 om ingen):
   - s = `kLearnedScore` från inlärningen ger Ditt val.
   - s ≥ `kSureScore` och s − näst bäst ≥ `kSureMargin` ger Säker.
   - s − näst bäst ≥ `kLikelyMargin` ger Trolig.
   - Annars Välj: `column` = −1, och kandidaterna står kvar.
3. **Ett fält utan par** får Saknas, med `column` = 0 för de valfria och −1 för Stolpnr.
4. **`looksLikePoleTable`** är sann när Stolpnr har Säker, Trolig eller Ditt val.
5. **Titel och version:**
   - Cellerna ovanför rubrikområdet söks i radordning efter en text vars normaliserade form innehåller "stolptabell" och "ver" följt av ett tal.
   - `title` är cellens text, trimmad och med blanktecken ihop. `version` är "Ver. " och talet, och `knownVersion` säger om talet finns i `kKnownVersions`.
   - Exempelfilen ger B1, eftersom rad 1 kommer före B2.

### 5.7 Utdraget (`extract`)
1. **Dataraderna** är raderna efter max(`header.lastRow`, `header.numberRow`) till `sheet.lastRow`. Kolumnen −1 räknas som 0.
2. **Stolpnr:** text trimmas, och ett tal skrivs med `xlsx::numberText(text, -1)` (1234 blir "1234"). Sedan används `poletable::acceptField(kPole, …)`.
3. **En rad hoppas över:**
   - utan Stolpnr: listas i `skippedRows` när någon annan vald kolumn har ett värde, annars tyst,
   - med en Stolpnr som är lik en av Stolpnr-kolumnens rubrikdelar: tyst (upprepad rubrik).
4. **Talfälten:**
   - Text trimmas. Bara streck ger tomt (3.5), och annars används `tabletext::normalizeDecimal`.
   - Ett tal skrivs med `xlsx::numberText(text, cell.decimals)` och normaliseras.
   - Bool, Error och text som inte är ett tal ger tomt fält och texten i `invalid[f]` (Bool och Error med sin text).
5. **LKL:** text går genom `acceptField(kLkl, …)`, så "T373", tom rad, "T373" blir "T373/T373". Ett tal skrivs med `numberText(text, -1)`.
6. **`firstDataRow`** är första radens `excelRow`, och 0 utan rader.
7. **Sammanslagningar och dolda rader** i dataområdet påverkar inte utdraget: bara den övre vänstra cellen har ett värde, och dolda rader läses som andra.

### 5.8 Rådataposten (`importRecord`)
- **Raderna:** en `ImportRow` per rad first..last med `excelRow`, `pole` (samma Stolpnr som `poleRows`) och alla celler med värde på Excelraden, i kolumnordning.
- **Cellerna:** `ImportCell` har kolumnen, slaget (`xlsx::CellKind` till `ImportCellKind`) och texten som i `xlsx::Cell::text`.
- **Kolumnerna:** de som har en rubrik eller en cell på raderna, med `letter`, `header` = `label` (delarna med " › ") och `field` = fältet vars `mapping` pekar på kolumnen, annars −1.
- **Övrigt:** `fileName`, `sheetName`, `title`, `version`, `headerFirstRow`, `headerLastRow`, `firstDataRow` (den första importerade raden), `fromPole` och `toPole` (första och sista importerade Stolpnr). `importedUtc` är tom.

### 5.9 Sträckorna som tappar en stolpe (`lostRanges`)
- **Prövningen:** för varje sträcka med både start och slut körs `automode::findRange(current, range, word, "Stolptabell före")`. Är den utan fel körs `findRange(imported, range, word, "filen")`, och har den då ett fel blir raden "{Word}: {felet}".
- **{Word}** är `word` med ASCII-versal först, som i `automode::tableMarks`.
- **Exempel:** "Faslinan: Stolpnr ST0107 finns inte i filen." och "Topplinorna: Slutet ST0101 står före starten ST0108 i filen. Byt start och slut."

---

## 6. Rådata i projektdatabasen

### 6.1 Schemat: tillägg sist i `dev::kStep5` (bindande)

> **Ändrat under handtestet (2026-11-26):** tabellerna ligger i ett eget steg, `dev::kStep6`, och `kSchemaVersion` är 6. Användaren hade redan projekt som skapats med steg 5 utan de här tabellerna (user_version 5, 13 tabeller), och de vägrades som skadade. `testMigrateFromStep5` prövar den vägen.
Steg 1–4 ändras inte. Kommentarerna står i SQL-texten som i de andra tabellerna.
```sql
-- Den senaste importen av en stolptabell till Stolptabell före: en rad (id = 1) med filens namn
-- (inte sökvägen), bladet, titeln och versionen ("Ver. 6"), tidpunkten (UTC), rubrikområdet,
-- första dataraden och sträckan som importerades. Saknas raden har ingen import gjorts.
CREATE TABLE IF NOT EXISTS pole_import (
    id               INTEGER PRIMARY KEY CHECK (id = 1),
    file_name        TEXT    NOT NULL,
    sheet_name       TEXT    NOT NULL,
    title            TEXT    NOT NULL DEFAULT '',
    version          TEXT    NOT NULL DEFAULT '',
    imported_utc     TEXT    NOT NULL,
    header_first_row INTEGER NOT NULL,
    header_last_row  INTEGER NOT NULL,
    first_data_row   INTEGER NOT NULL,
    from_pole        TEXT    NOT NULL DEFAULT '',
    to_pole          TEXT    NOT NULL DEFAULT ''
);
-- Kolumnerna i den importerade filen: numret (1 = A), bokstäverna, rubriken (rubrikraderna
-- uppifrån med " › " emellan, sammanslagna celler ifyllda) och fältet i Stolptabell före som
-- kolumnen gav, med pole_before-kolumnens namn. NULL för kolumner som inte gav något fält.
CREATE TABLE IF NOT EXISTS pole_import_column (
    col    INTEGER PRIMARY KEY,
    letter TEXT    NOT NULL,
    header TEXT    NOT NULL DEFAULT '',
    field  TEXT    UNIQUE CHECK (field IN ('pole', 'coord_n', 'coord_e', 'coord_h', 'span_m', 'ah', 'av', 'lkl'))
);
-- De importerade raderna: Excels radnummer, platsen i Stolptabell före vid importen (0, 1, 2 ...)
-- och Stolpnr som det blev där.
CREATE TABLE IF NOT EXISTS pole_import_row (
    excel_row INTEGER PRIMARY KEY,
    position  INTEGER NOT NULL UNIQUE,
    pole      TEXT    NOT NULL
);
-- Cellerna på de importerade raderna, alla kolumner som har ett värde. text är värdet som det står
-- i filen: texten med radbrytningar, talet med punkt och alla decimaler, sant/falskt som 1/0 och
-- felet som #N/A. kind är cellens slag.
CREATE TABLE IF NOT EXISTS pole_import_cell (
    excel_row INTEGER NOT NULL REFERENCES pole_import_row(excel_row) ON DELETE CASCADE,
    col       INTEGER NOT NULL REFERENCES pole_import_column(col) ON DELETE CASCADE,
    kind      TEXT    NOT NULL CHECK (kind IN ('text', 'number', 'bool', 'error')),
    text      TEXT    NOT NULL,
    PRIMARY KEY (excel_row, col)
) WITHOUT ROWID;
```
- **`project_db.cpp`:** `kExpectedTablesSql` får de fyra namnen, och `kExpectedTableCount` blir 17. `kSchemaVersion` förblir 5.
- **Fältnamnen** i `field` är pole_before-kolumnernas namn: `pole` (kPole), `coord_n` (kNorth), `coord_e` (kEast), `coord_h` (kHeight), `span_m` (kSpan), `ah` (kAh), `av` (kAv) och `lkl` (kLkl).

### 6.2 `libs/projectdb/project_data.h`, tillägg (bindande)
```cpp
#include "poleTable/import_record.h"

// Stolptabell före ur en importerad fil: raderna som setPoles(p, Stage::Before, rows) och importens
// rådata (pole_import, pole_import_column, pole_import_row och pole_import_cell), som ersätter den
// förra importens. Allt sker i EN transaktion, med synkningen av de automatiska beräkningarna sist.
// record.rows har en post per rad i rows med samma Stolpnr, och rows har inga tomma rader.
// importedUtc sätts här. Inget annat ändras: Stolptabell efter, automatiskt läge med sträckorna,
// Dokumentation, linkopiorna och beräkningarna står kvar, utom det synkningen skriver.
// Fel: AccessDenied, Invalid (ogiltigt tal som i setPoles, eller "Importen av stolptabellen är
// ofullständig och sparades inte."), Sql.
core::Result<void> importPolesBefore(ProjectDb& p, const std::vector<poletable::PoleRow>& rows,
                                     const poletable::ImportRecord& record);

// Den senaste importens rådata, eller tomt svar när ingen import har gjorts. Kolumnerna i
// kolumnordning, raderna i platsordning och cellerna i kolumnordning. Fel: Sql.
core::Result<std::optional<poletable::ImportRecord>> lastPoleImport(ProjectDb& p);
```

### 6.3 Beteendet
- **`importPolesBefore`**, i `write`:
  1. Kontrollerna, före allt annat:
     - Antalet rader är lika i `rows` och `record.rows`, och `rows[i].pole` är lika med `record.rows[i].pole`.
     - Ingen rad är tom (`poletable::isEmptyRow`).
     - Kolumnnumren och fälten (≥ 0) är unika, och varje cell pekar på en kolumn i `record.columns`.

     Ett fel ger Invalid "Importen av stolptabellen är ofullständig och sparades inte." med detaljen i `detail`. Sedan kontrolleras talen med `firstInvalidColumn` som i `setPoles`.
  2. Raderna skrivs med samma kod som `setPoles`. Den bryts ut till en intern `writePoleRows(d, stage, rows)`, som båda anropar.
  3. `DELETE FROM pole_import_cell`, `pole_import_row`, `pole_import_column` och `pole_import`, i den ordningen.
  4. Posten skrivs: `pole_import` med `imported_utc = platform::nowUtcIso()`, kolumnerna, raderna (position = index) och cellerna (`kind` 'text', 'number', 'bool' eller 'error').
     - Använd en förberedd sats per tabell (`bind`, `step`, `reset`), inte `run` per cell.
  5. `syncAutoCalcs(d, platform::nowUtcIso())`.
- **Inget annat rör rådata:** `setPoles` (båda skedena), `copyBeforeToAfter`, `setAutoSettings` och de andra skrivningarna gör det inte. En tömd Stolptabell före lämnar rådata kvar.
- **`lastPoleImport`** läser de fyra tabellerna. Utan rad i `pole_import` blir svaret tomt, och en okänd `field` eller `kind` ger Invalid "Importens rådata i projektfilen är skadad.".
- **Exempel på en fråga** (står i `libs/projectdb/CLAUDE.md`):
  ```sql
  SELECT c.letter, c.header, x.text
  FROM pole_import_row r
  JOIN pole_import_cell x ON x.excel_row = r.excel_row
  JOIN pole_import_column c ON c.col = x.col
  WHERE r.pole = 'ST9002'
  ORDER BY x.col;
  ```

---

## 7. Systemdatabasen (`libs/systemdb/system_db.h`, tillägg, bindande)
```cpp
// En rubrik som användaren valt själv för ett fält vid importen av en stolptabell: fältet (0-7,
// poletable-kolumnen) och den normaliserade rubriken (poleimport::SourceColumn::key).
struct LearnedHeader {
    int field = 0;
    std::string key;
    bool operator==(const LearnedHeader&) const = default;
};

// I klassen SystemDb:
// Mappen där Importera stolptabell… öppnar filväljaren, "" om inget är sparat. Nyckeln last_import_folder.
std::string lastImportFolder();
core::Result<void> setLastImportFolder(const std::string& folder);
// De inlärda rubrikerna, senast först. Tom lista vid fel eller när inget är sparat.
// Nyckeln pole_import_learned: en rad per rubrik, fältet och rubriken skilda med en tabb.
std::vector<LearnedHeader> learnedHeaders();
// Lägger rubriken först för fältet. Finns den redan flyttas den först, och fler än tio per fält
// tas bort från slutet. Fel: Invalid (fältet utanför 0-7, eller en tom rubrik eller en rubrik med
// tabb eller radbrytning), Sql.
core::Result<void> learnHeader(int field, const std::string& key);
```
- Ingen schemaändring och ingen migrering: allt ligger i tabellen `setting`.
- Tabben och radbrytningen skrivs `char{9}` och `char{10}` i koden.
- **Senaste mappen** är en egen nyckel och inte `last_folder`. Excelfilerna ligger oftast i en leveransmapp eller i Hämtade filer, och då skulle projektväljaren öppna där.
- **Förvalet** för importmappen är projektfilens mapp (`platform::parentDir(project_->path())`), och sedan Dokument.

---

## 8. Fönstren

### 8.1 Huvudfönstret
- **`main_window.h`, regionen:** `glaze::Button stolpImportBtn;` efter `stolpRubrik`.
- **Handlern:** `stolpImportBtnClicked(glaze::Widget* sender, glaze::ClickArgs& e)`, med en kommentar på 2–5 rader.
- **Efter regionen:**
  - Hjälpfunktionen `importPolesAsked()`, med kommentar.
  - Medlemmen `glaze::FileDialog importFileDlg_{glaze::FileDialogMode::Open};` före `saveWorker_`.
- **`main_window.glazeui`:**
  - `stolpImportBtn`:
    - Text "Importera stolptabell…", bild `glaze::stockIcon(glaze::StockIcon::Import)`, `anchorLeft = {"stolpRubrik", 16}` och `y = 10`, som `efterKopieraBtn`.
    - Tooltip: "Ersätter raderna i Stolptabell före med stolparna ur en stolptabell från Banken (.xlsx eller .xlsm). Start och slut, automatiskt läge och Stolptabell efter ändras inte."
    - `stolpImportBtn.onClick` kopplas i fullform, och `stolpPanel.adopt(stolpImportBtn)` står direkt efter `stolpPanel.adopt(stolpRubrik)`.
  - `stolpInfo` får texten "Klistra in från Excel med Ctrl+V i den cell där inklistringen ska börja: en cell, rader, kolumner eller ett block. Importera stolptabell… hämtar i stället stolparna ur en stolptabell från Banken. Komma och punkt fungerar båda som decimaltecken, och talen visas med så många decimaler som du skriver. Skriv i den tomma raden sist för att lägga till en stolpe, och ta bort en rad med papperskorgen längst till höger. Med automatiskt läge räknas normalspann och linberäkning för faslinan och topplinorna ur stolparna mellan start och slut, och resultaten visas i Dokumentation."
- **Skrivskyddat:** `stolpImportBtn.disable()` i `onLoad`, bredvid `efterKopieraBtn.disable()`.
- **`importPolesAsked()`:**
  1. `commitEdits()`.
  2. Mappen är `sys_->lastImportFolder()`, annars projektfilens mapp, annars `platform::documentsDir()`.
  3. `importFileDlg_` får titeln "Importera stolptabell", filtren `{{"Excel-filer", "*.xlsx;*.xlsm"}, {glaze::tr(glaze::str::FileAllFiles), "*.*"}}` och mappen. Svarar den inte OK händer ingenting.
  4. `sys_->setLastImportFolder(platform::parentDir(path))`. Ett fel loggas bara.
  5. `PoleImportDialog dlg;` får `setSystemDb(*sys_)`, `setFile(path)` och `setCurrentTable(beforeGrid_->rows(), namedRanges(Stage::Before))`. Svarar `dlg.run(*this)` inte OK händer ingenting.
  6. `projectdb::importPolesBefore(*project_, dlg.importedRows(), dlg.importRecord())`.
     - Vid fel loggas `detail`, och rutan "Stolptabellen kunde inte importeras." visas med en radbrytning och `message` ("Kalkyl", OK, Error). Inget annat ändras.
  7. Vid lyckad import:
     - `updateTitle()`, `beforeGrid_->setRows(reloadPoles(Stage::Before))` och `refreshAutomatic()`.
     - `autoBefore_` läses inte om, eftersom inställningen inte ändras.
     - Panelernas rullistor visar de nya Stolpnr, och en sträcka vars stolpe saknas visar "(finns inte)" och felet som i dag.
  8. Statusraden får texten i 9.5.
- **`stolpImportBtnClicked`** anropar `importPolesAsked()`.

### 8.2 Dialogen `src/windows/pole_import_dialog.*` (klassen `PoleImportDialog`)
**Efter regionen i `.h` (bindande):**
```cpp
public:
    // Systemdatabasen, för de inlärda rubrikerna och importmappen. Anropas före run(owner).
    void setSystemDb(kalkyl::systemdb::SystemDb& sys);
    // Filen som läses när dialogen öppnas. Anropas före run(owner).
    void setFile(const std::string& path);
    // Raderna i Stolptabell före och dess sträckor, till frågan om ersättning. Anropas före run(owner).
    void setCurrentTable(std::vector<kalkyl::poletable::PoleRow> rows,
                         std::vector<kalkyl::automode::NamedRange> ranges);
    // Efter run(owner) == OK: raderna att importera och deras rådata.
    const std::vector<kalkyl::poletable::PoleRow>& importedRows() const;
    const kalkyl::poletable::ImportRecord& importRecord() const;
    // Antalet ogiltiga tal som lämnades tomma i de importerade raderna.
    int invalidCount() const;
```
Därefter kommer handlers, hjälpfunktioner och medlemmar, var och en med en kommentar:
- **Handlers:** `onLoad`, `fileBoxCommitted`, `fieldGridCommitted`, `rangeChanged` (Från och Till), `allBtnClicked` och `okBtnClicked`.
- **Hjälpfunktioner:** `startLoad(path)`, `applySheet(index)`, `fillFields()`, `fillRange()`, `refreshPreview()` och `buildResult()`.
- **Medlemmar:** `sys_`, `path_`, `loadedPath_`, `currentRows_`, `ranges_`, `book_` (`std::optional<xlsx::Workbook>`), `sheetIndex_`, `learned_`, `rec_`, `mapping_`, `chosen_` (`std::array<bool, 8>`, fält som användaren valt), `choiceColumns_` (kolumnen per post i rullistan, 0 = hämtas inte, −1 = Välj kolumn…), `ex_`, `first_`, `last_`, `rows_`, `record_`, `invalid_` och sist `glaze::threads::Worker loadWorker_`.

**`.glazeui`** (designraden står kvar):
- **Fönstret:** titel "Importera stolptabell", `setContentSize(1100, 760)`, `setMinimumContentSize(860, 600)`, `CenterOwner`, modal, `setAcceptButton(okBtn)` och `setCancelButton(cancelBtn)`. Alla rader har `x = 16` och `right = 16` där de sträcker sig.
- **`hintLbl`** (`Dim`, `setWrap(true)`, y 12, ankrad vänster, uppe och höger): "Stolparna hämtas ur en stolptabell från Banken (.xlsx eller .xlsm). Kalkyl läser filen som den senast sparades och kör aldrig makron. Varje fält i Stolptabell före hämtas ur kolumnen som står vald nedan. Kontrollera dem, särskilt de som inte är säkra."
- **Filraden:**
  - `fileLbl`: "Fil".
  - `fileBox` (`FilePicker`): `Open`, filtren `addFilter("Excel-filer", "*.xlsx;*.xlsm")` och `addFilter(glaze::tr(43), "*.*")`, `setDialogTitle("Importera stolptabell")`, x 80, ankrad vänster, uppe och höger, och `onCommit` till `fileBoxCommitted`.
- **Inforaden:** `infoLbl` (`setWrap(true)`) under filraden, x 80, ankrad vänster och höger. Inget val av blad.
- **`fieldsRubrik`** (`Heading`): "Kolumner".
- **`fieldGrid`:**
  - Höjd 250, ankrad vänster, uppe och höger, `setSortable(false)`.
  - Kolumnerna "Fält" (170), "Kolumn i filen" (460), "Säkerhet" (110) och "Första värdet" (Autosize).
  - Redigerarna: kolumn 0, 2 och 3 `None`, kolumn 1 `Choice` (valen sätts per cell i koden).
  - Rubrikernas tooltips: kolumn 1 "Kolumnen i Excelfilen som fältet hämtas ur. Klicka för att välja en annan.", kolumn 2 "Hur säkert rubriken känns igen. Peka på cellen för skälet." och kolumn 3 "Värdet på första raden som importeras, som det står i filen".
  - `onCommit` till `fieldGridCommitted`.
- **`rangeRubrik`** (`Heading`): "Stolpar". Under den står en rad i kedja med `anchorLeft`:
  - `fromLbl` "Från", `fromDrop` (bredd 220) med tooltipen "Första stolpen som importeras".
  - `toLbl` "till", `toDrop` (bredd 220) med tooltipen "Sista stolpen som importeras".
  - `allBtn` "Alla stolpar" med tooltipen "Från första till sista stolpen i filen".
  - `countLbl`.
  - Båda rullistorna har `onChange` till `rangeChanged`, och `allBtn.onClick` går till `allBtnClicked`.
- **`previewRubrik`** (`Heading`): "Förhandsvisning".
- **`previewGrid`:**
  - Ankrad på alla sidor, med `anchorBottom = {"issuesLbl", 8}`, `setSortable(false)` och `setHeaderLines(2)`.
  - Nio kolumner, alla `None`: "Rad" (höger, AutoMin, tooltip "Raden i Excelfilen"), och sedan "Stolpnr", "Koordinat N", "Koordinat E", "Koordinat H", "Avstånd föregående", "Ah", "Av" och "LKL" med samma rubriker på två rader, bredder och justering som `stolpGrid`.
- **`issuesLbl`** (`setWrap(true)`): ankrad vänster, höger och nere, `anchorBottom = {"cancelBtn", 12}`.
- **Knapparna:**
  - `cancelBtn` "Avbryt": `DialogResult::Cancel`, bredd 96, höger 16, nere 16.
  - `okBtn` "Importera": inget `dialogResult`, bredd 110, `anchorRight = {"cancelBtn", 8}`, nere 16, tooltipen "Ersätter raderna i Stolptabell före med stolparna i förhandsvisningen" och `onClick` till `okBtnClicked`.

**`.cpp`, beteendet:**
1. **`onLoad`:** `fileBox.setPath(path_)`, `okBtn.disable()`, `learned_` ur `sys_->learnedHeaders()` och `startLoad(path_)`.
2. **`startLoad(path)`:**
   - `infoLbl` får "Läser {namn}…", och `setUseWaitCursor(true)`.
   - `loadWorker_.cancel()` och sedan `loadWorker_.run(...)`. Arbetet fångar bara värden (sökvägen och `learned_`) och gör `xlsx::openWorkbook`, `poleimport::bestSheet` och `recognize`.
   - Fortsättningen sätter tillbaka pekaren. Vid fel visar `infoLbl` felets `message` i rött (`gfx::Color{200, 0, 0}`), tabellerna töms och Importera är släckt.
   - Annars sätts `sheetIndex_` till `bestSheet`, och `applySheet` körs.
3. **`fileBoxCommitted`:** en ny sökväg (inte lik `loadedPath_`) ger `startLoad`.
4. **Bladet:** bara bladet ur `bestSheet` läses. Dialogen har inget val av blad, och andra blad i filen syns inte.
5. **`applySheet`:**
   - `mapping_ = mappingOf(rec_)` och `chosen_` nollställs.
   - `infoLbl` får texten i 9.2.
   - Sedan `fillFields()`, `ex_ = extract(...)`, `fillRange()` med Från = första och Till = sista stolpen, och `refreshPreview()`.
6. **`fillFields()`:** en rad per fält i `poletable`-ordning.
   - **Fält:** `poletable::columnName(f)`. Stolpnr-cellen har tooltipen "Stolpnr krävs. De andra fälten kan lämnas tomma."
   - **Kolumn i filen:** valen är "– (hämtas inte)" för de valfria fälten, och sedan varje `SourceColumn` som "{L} · {label}" eller "{L} · (ingen rubrik)". För Stolpnr och för ett fält med `mapping_` −1 står "Välj kolumn…" först.
     - Cellens text är posten för `mapping_[f]`, och `setCellEditor(rad, 1, {Choice, val})` sätts efter `setRows`.
   - **Säkerhet:** `confidenceText`, med cellfärgen grön `{70,170,90,64}` för Säker, bärnsten `{220,160,40,64}` för Trolig och Saknas, röd `{220,60,60,80}` för Välj och blå `{60,130,210,56}` för Ditt val. Tooltipen står i 9.3.
   - **Första värdet:** cellens text på första importerade raden, annars "–".
7. **`fieldGridCommitted`:**
   - Den valda posten ger kolumnen (`choiceColumns_`), och `mapping_[f]` och `chosen_[f] = true` sätts.
   - Säkerheten visas som Ditt val, utom för "– (hämtas inte)", som visar Saknas.
   - Sedan `ex_ = extract(...)`. Ändrades Stolpnr körs `fillRange()`. Sist `refreshPreview()`.
8. **`fillRange()`:** Från och Till får "{Stolpnr} · rad {r}" för varje rad i `ex_.rows`. Är Från och Till fortfarande giltiga behålls de, annars väljs första och sista.
9. **`refreshPreview()`:**
   - `first_` och `last_` hämtas ur rullistorna, och `countLbl` får texten i 9.2.
   - `previewGrid` får raderna first..last: Excelraden och fälten med `poletable::displayText`.
     - En ogiltig cell är röd `{220,60,60,80}` med tooltipen i 9.3. En Stolpnr som finns flera gånger i sträckan är röd med tooltipen i 9.3.
   - `issuesLbl` får `importIssues(...)` och `lostRanges(ranges_, currentRows_, poleRows(...))` (inte stoppande), en per rad. Texten är röd när något stoppar, annars `Dim`.
   - Importera är tänd bara när inget problem stoppar.
10. **`allBtnClicked`:** Från blir första och Till sista stolpen, och `refreshPreview()` körs.
11. **`okBtnClicked`:**
    1. `commitEdits()`, `buildResult()` (`rows_ = poleRows(...)`, `record_ = importRecord(...)` med `platform::fileName(loadedPath_)`, `invalid_` = antalet ogiltiga fält i sträckan).
    2. Har `rowsToSave(currentRows_)` rader visas frågan i 9.4, och Nej lämnar dialogen öppen.
    3. Varje fält med `chosen_` och kolumn > 0 lärs in med `sys_->learnHeader(f, kolumnens key)`. Ett fel loggas bara.
    4. `sys_->setLastImportFolder(platform::parentDir(loadedPath_))` och `closeWithResult(glaze::DialogResult::OK)`.
- **Två fält på samma kolumn:** det stoppas av `importIssues`, så användaren får välja om.

### 8.3 Vad som händer efteråt
- **Stolptabell före** har de nya raderna (`setRows`). Det senast sparade är de nya raderna, så ingen extra sparning görs.
- **Automatiskt läge:** sträckorna och läget är som förut. `importPolesBefore` har synkat de lagrade automatiska beräkningarna, och `refreshAutomatic()` visar dem i Dokumentation, flikarna, panelerna och färgerna.
- **Stolptabell efter** är orörd, men färgerna jämförs nu mot de nya raderna i före. Rader kan därför bli Ändrad eller Ny utan att efter har ändrats.
- **Titeln** får `*`, och projektet publiceras med Ctrl+S eller den automatiska sparningen.

---

## 9. Texter (ordagrant)

{namn} är filens namn utan mapp, {L} kolumnens bokstäver, {label} den sammansatta rubriken och {fält} `poletable::columnName`.

### 9.1 Filen (`xlsx::openWorkbook` och `readWorkbook`)
| Fall | Kod | Text |
|---|---|---|
| Finns inte | NotFound | "Filen {namn} finns inte." |
| Används av ett annat program | Busy | "{namn} används av ett annat program och kan inte läsas just nu. Stäng filen där, eller spara en kopia och importera den." |
| Ingen behörighet | AccessDenied | "Du har inte behörighet att läsa {namn}." |
| Nätverket svarar inte | Unreachable | "Mappen med {namn} svarar inte. Kontrollera nätverket och försök igen." |
| Annat läsfel | Io | "{namn} kunde inte läsas." |
| För stor fil | Invalid | "{namn} är för stor för att importeras (mer än 64 MB)." |
| Inte en zip | Invalid | "{namn} är inte en Excelfil (.xlsx eller .xlsm)." |
| OLE (.xls eller lösenord) | Invalid | "{namn} är en äldre Excelfil (.xls) eller lösenordsskyddad. Spara den som .xlsx i Excel utan lösenord och importera den." |
| Skadad zip eller XML | Invalid | "{namn} är skadad och kan inte läsas som en Excelfil." (vilken del och var i `detail`) |
| Inga kalkylblad | Invalid | "{namn} har inga kalkylblad." |
| För stor del | Invalid | "{namn} har en del som är för stor uppackad (mer än 256 MB)." |

### 9.2 Dialogens rader
- **`infoLbl`:**
  - Under läsningen: "Läser {namn}…".
  - Med titel: "{title} · rubriker på rad {a}–{b} · data från rad {c}". Är versionen inte provad läggs " · versionen är inte provad, kontrollera kolumnerna" till.
  - Utan titel: "Blad {blad} · rubriker på rad {a}–{b} · data från rad {c}".
  - Utan stolpar står "data från rad {c}" inte med.
  - När `looksLikePoleTable` är falsk (röd): "Hittar ingen stolptabell på bladet {blad}. Välj kolumnerna själv eller en annan fil."
- **`countLbl`:** "Alla {n} stolpar", "{k} av {n} stolpar", "1 stolpe" (filen har en stolpe) eller "Inga stolpar".
- **Rullistornas poster:** "– (hämtas inte)", "Välj kolumn…", "{L} · {label}", "{L} · (ingen rubrik)" och "{Stolpnr} · rad {r}".

### 9.3 Problemen (`importIssues`) och tooltips
**Stoppar**, i den här ordningen:
1. "Välj kolumnen för Stolpnr." (Stolpnr har −1 eller 0.)
2. Per tvetydigt fält (−1): "Välj kolumn för {fält}: {L1} och {L2} passar båda."
3. Per kolumn som valts för två fält: "Kolumn {L} är vald för både {fält1} och {fält2}. Välj olika kolumner."
4. "Inga stolpar under rubrikerna i kolumn {L} ({label})." (Stolpnr har en kolumn men `ex.rows` är tom.)
5. "Till står före Från i filen. Byt Från och Till." (first > last.)

**Stoppar inte**, i den här ordningen:
1. "1 ogiltigt tal lämnas tomt (rött i förhandsvisningen)." eller "{k} ogiltiga tal lämnas tomma (röda i förhandsvisningen)." (inom first..last).
2. "1 rad utan Stolpnr hoppas över: rad {r}." eller "{k} rader utan Stolpnr hoppas över: rad {r1}, {r2} …". Högst fem nummer visas, och " …" bara när det finns fler.
3. Per dubblett i sträckan (högst tre): "Stolpnr {p} finns flera gånger. Dubbletterna markeras i tabellen efter importen." Fler ger en rad till, "… och {k} till.".
4. "Avstånd föregående hämtas inte och räknas ur koordinaterna." (Avstånd föregående har 0.)
5. Raderna ur `lostRanges` (läggs till av dialogen).

**Tooltips i `fieldGrid`, kolumnen Säkerhet:**
- **Säker och Trolig:** "{s} poäng: {skäl}." Finns en näst bäst kandidat läggs " Näst bäst: {L} ”{label}”, {s2} poäng." till.
- **Välj:** "{L1} ”{label1}” och {L2} ”{label2}” passar båda ({s1} och {s2} poäng). Välj den rätta."
- **Saknas, Stolpnr:** "Stolpnr behövs. Välj kolumnen med stolparnas nummer."
- **Saknas, valfritt fält:** "Ingen kolumn känns igen som {fält}. Fältet lämnas tomt, eller välj en kolumn."
- **Ditt val:** "Ditt val. Kalkyl minns det för filer med samma rubrik."

**Tooltips i `previewGrid`:**
- Ogiltig cell: "Ogiltigt tal i {L}{rad}: ”{text}”. Cellen lämnas tom."
- Dubblett: "Stolpnr {p} finns flera gånger i filen."

### 9.4 Frågan före ersättningen (i dialogen, när Stolptabell före har rader)
- **Rubrik:** "Importera stolptabell".
- **Text:** "Ersätta raderna i Stolptabell före med {m} stolpar från {namn}?", två radbrytningar och sedan "Stolptabell före har {n} rader nu. Bara raderna byts: start och slut för linorna, automatiskt läge, Stolptabell efter och Dokumentation ändras inte."
  - Med `lostRanges` följer två radbrytningar, "De här sträckorna hittar inte sin stolpe i filen. De står kvar och visar felet tills de väljs om:", och sedan en rad per sträcka.
  - Ental: "1 stolpe" och "1 rad".
- **Knappar:** Ja och Nej, `MessageBoxType::Warning`.

### 9.5 Huvudfönstret
- **Statusraden:** "Importerade {m} stolpar från {namn} till Stolptabell före." (ental "1 stolpe").
  - Med ogiltiga tal läggs " 1 ogiltigt tal lämnades tomt." eller " {k} ogiltiga tal lämnades tomma." till.
- **Felrutan:** "Stolptabellen kunde inte importeras.", en radbrytning och felets text.
- **Projektdatabasens texter:** "Importen av stolptabellen är ofullständig och sparades inte." och "Importens rådata i projektfilen är skadad.".

---

## 10. Tester

**Testfilerna byggs i minnet** av testerna själva. `ref/` används aldrig.
- **`tests/xlsx_maker.h`** (header, namnrymden `kalkyl::testxlsx`) skriver zip med minizs skrivdel (`mz_zip_writer_init_heap`, `mz_zip_writer_add_mem`, `mz_zip_writer_finalize_heap_archive`):
  - `std::string zip(const std::vector<Part>& parts)` gör en zip ur delar (namn och data).
  - `std::string workbook(sheets, extra)` gör `[Content_Types].xml`, `_rels/.rels`, `xl/workbook.xml` med relationer och ett blad per post, plus extra delar som `sharedStrings.xml` och `styles.xml`.
  - `std::string fromSheets(const std::vector<xlsx::Sheet>& sheets, bool shared = true)` skriver `xlsx::Sheet` som riktiga blad: text som delade eller inline-strängar (LF som tecken med `xml:space="preserve"`), tal, Bool, Error, sammanslagningar och dolda blad.
- **`tests/banken_fixture.h`** (namnrymden `kalkyl::testfixture`) bygger `xlsx::Sheet` med rubrikerna i bilaga A:
  - `bankenV6(rows)` lägger rubrikerna och en datarad per `PoleRow` från rad 19, med talen som text med decimalkomma som i Banken.
  - Fällorna har egna värden: L är spannet + 1, N är "999", S "A", X är Av − 100 och BH "256", radbrytning, "256".
  - `annexRows()` är bilaga A i planen för automatiskt läge, med koordinater och spann.

### 10.1 `tests/xlsx_test.cpp` (ny, ctest `xlsx`)
- **Rundtur:** `fromSheets` och sedan `readWorkbook` ger samma `Sheet`, både med delade och med inline-strängar.
- **Delade strängar:** rik text med flera `r`, `rPh` som hoppas över, `<t/>` som ger ingen cell och `xml:space="preserve"`.
- **Inline-strängar:** `is/t` och `is/r/t`.
- **Tal:**
  - "7047024.75" och "422.10000000000002" sparas som de står, och `numberText` ger "422.1".
  - Exponenterna, "-0" och decimalerna ur `styles.xml` (inbyggt id 2 och egna "0.000" och "#,##0.00").
  - `formatDecimals` för "General", "0%", "0.00E+00", "yyyy-mm-dd", "[Red]0.0" och "#,##0".
- **Övriga slag:** Bool, Error, formel med sparat värde (`t="str"` och utan `t`) och `t="d"`.
- **Radbrytningar:** CR LF och ensamt CR blir LF, `_x000D_` följt av LF blir LF, och `_x005F_x000D_` blir texten "_x000D_".
- **Entiteter:** `&amp;`, `&lt;`, `&#228;` och `&#xE4;`, och en DOCTYPE som vägras.
- **Celler och rader utan `r`**, och namnrymdsprefix (`x:c`, `x:row`).
- **`columnLetters` och `parseCellRef`:** A, Z, AA, AZ, BA, CK och XFD; "$AB$12"; ogiltiga "A0", "1A", "" och "XFE1".
- **Sammanslagningar**, och ett blad med en sammanslagning som bara har en cell.
- **Relationerna:** Target med "/" först och med "../", ett diagramblad som hoppas över, ett dolt blad och en bok utan `sharedStrings.xml` och `styles.xml`.
- **Fel med exakt text (9.1):** inte en zip, OLE-början, en avkortad zip, en zip utan `workbook.xml`, en bok utan blad, en del som är för stor (`kMaxPartBytes` prövas med ett falskt stat-värde eller en liten gräns i en intern variant) och ett delat index utanför.
- **`openWorkbook`** på en fil i en temporär mapp med åäö.

### 10.2 `tests/platform_test.cpp` (utökas)
- **`readFileShared`:**
  - Läser en fil som en annan handtag har öppen med `GENERIC_READ | GENERIC_WRITE` och bara `FILE_SHARE_READ`, som Excel.
  - En fil som saknas ger NotFound, och `maxBytes` mindre än filen ger Invalid.

### 10.3 `tests/poleimport_test.cpp` (ny, ctest `poleimport`)
Testerna bygger `xlsx::Sheet` direkt ur `banken_fixture.h` eller för hand.
- **`normalizeHeader`:**
  - "Avst föreg stolpe" ger "avstand foregaende stolpe", "Stpl nr" ger "stolpe nummer" och "Avst. föreg." ger "avstand foregaende".
  - "Längdklass" och "LKL" ger "langdklass", och "RH2000" ger "rh 2000".
  - Ett hårt mellanslag och en radbrytning ger ett blanksteg, och "Stolpnummer" ger "stolpe nummer".
- **Rubrikområdet:**
  - Banken Ver. 6 ger 11–15, kolumnnummerrad 16 och första dataraden 19.
  - En enkel tabell med rubriken i rad 1 och data från rad 2.
  - En tabell med rubriken i rad 4 under en titel, utan kolumnnummerrad.
- **Sammanslagna rubrikceller:** W har delarna "Profil", "Av", "Max temp" och "m", I har "Koordinater", "N" och "SWEREF 99 TM", och en grupp sammanslagen över tre kolumner ger sin text åt alla tre.
- **Banken Ver. 6:** alla åtta fält är Säker med R, I, J, K, U, V, W och AG. Titeln och "Ver. 6" känns igen, och `knownVersion` är sann.
- **Nära varianter:** X (Min temp), S (Byggnads), N (beräknad Ah), L (beräknad Avst. föreg.) och BH (E under Stagförankring) väljs aldrig. Poängen i 5.5 kontrolleras för dem.
- **Flyttade kolumner:** samma rubriker i en annan ordning (Stolpnr i B, LKL i C, N, E och H i X, Y och Z) ger rätt kolumner.
- **Omdöpta rubriker:**
  - "Stolpnummer", "Koordinat N" i en cell, "Avstånd föregående", "Spannlängd", "LKL" och "Av" med "Max temperatur" ger rätt fält, Säker eller Trolig.
  - "Ver. 7" ger `knownVersion` falsk men samma val.
- **Saknad kolumn:**
  - Utan AG blir LKL Saknas med kolumn 0.
  - Utan Stolpnr blir Stolpnr Saknas med −1, `looksLikePoleTable` falsk och "Välj kolumnen för Stolpnr." som stoppar.
- **Två kandidater:** två kolumner med "Profil › Ah" ger Välj med −1, båda kandidaterna och problemet "Välj kolumn för Ah: V och Y passar båda.".
- **Inlärning:** en okänd rubrik "Stolpbeteckning" ger Saknas, och med `learned[kPole] = {"stolpbeteckning"}` blir den Ditt val med 100 poäng.
- **Dataprovet:** en talkolumn med mest text får −40 och väljs inte.
- **Utdraget:**
  - "7047024,75" ger "7047024.75", talet "422.10000000000002" ger "422.1" och talet 6512000 med formatet "0.00" ger "6512000.00".
  - LKL "T373", tom rad, "T373" ger "T373/T373".
  - "ca 300" ger tomt fält och `invalid`, och "-" ger tomt utan `invalid`.
  - En Stolpnr som tal ger "1234".
  - En rad utan Stolpnr med värden hamnar i `skippedRows`, och en helt tom rad gör det inte.
  - En tom rad mitt i data, och en upprepad rubrikrad "Stpl nr" som hoppas över.
- **`importRecord`:** raderna 2..4 ger rätt Excelrader, alla celler (också de som inte används), kolumnernas rubriker och fält, titel, version, `fromPole` och `toPole`.
- **`importIssues`:** varje text i 9.3 exakt, i rätt ordning, och ental och flertal.
- **`lostRanges`:**
  - En start som saknas ger "Faslinan: Stolpnr ST0107 finns inte i filen.".
  - En sträcka som finns ger ingen rad, och inte heller en sträcka som redan har fel eller saknar start.
- **`bestSheet`:** "Blad1" med skräp och "Stolptabell" ger Stolptabell. Utan det namnet vinner bladet med flest säkra fält, och ett dolt blad väljs inte före ett synligt.

### 10.4 `tests/projectdb_test.cpp` (utökas)
- **`kThirteenTablesSql`** blir `kSeventeenTablesSql` med de fyra nya tabellerna. `testSchemaTables`, `testMigrateFromV010`, `testMigrateFromV020`, `testMigrateFromV030` och `testMigrateFromV040` räknar 17.
- **`testMigrateFromV040`:** dessutom är `pole_import` tom och `lastPoleImport` tomt svar efter öppningen.
- **`testPoleImportRoundTrip`:**
  - En post med tre rader, alla fyra cellslag, en text med radbrytning, ett tal med 17 siffror och kolumner med och utan fält.
  - `lastPoleImport` ger samma post utom `importedUtc`, som är satt.
  - `poles(Before)` är raderna, och SQL-exemplet i 6.3 ger rätt rader.
- **`testPoleImportReplaced`:** en andra import ersätter allt i de fyra tabellerna, och inga celler från den första finns kvar.
- **`testPoleImportUntouched`:** `setPoles(Before)` med ändrade rader, `setPoles(After)`, `copyBeforeToAfter` och `setAutoSettings` lämnar rådata lika.
- **`testPoleImportInvalid`:** färre poster än rader, fel Stolpnr, en tom rad, två kolumner med samma fält och ett ogiltigt tal ger Invalid med texten, och inget ändras.
- **`testReadOnlyWrites`** får `importPolesBefore`, som ger AccessDenied. **`testRevision`:** importen räknar upp revisionen.

### 10.5 `tests/systemdb_test.cpp` (utökas)
- **`lastImportFolder`:** rundtur, skild från `lastFolder`.
- **`learnHeader`:**
  - Ordningen är senast först, en dubblett flyttas först och den elfte tar bort den äldsta för fältet.
  - Andra fält påverkas inte.
  - Fältet 8, en tom rubrik och en rubrik med tabb ger Invalid.
  - Rubrikerna finns kvar efter en ny öppning, och reserven i minnet fungerar.

### 10.6 `tests/poleimport_flow_test.cpp` (ny, ctest `poleimport_flow`)
Hela vägen från fil till projekt, i en temporär mapp med åäö, som `projectdb_test`.
1. **Förberedelsen:**
   - Ett projekt får `annexRows()` i före, `copyBeforeToAfter`, några värden i Dokumentation, en manuell normalspannsberäkning och automatiskt läge på i före med faslinan ST0102–ST0107 och topplinorna ST0101–ST0108.
   - Ögonblicksbilden är `poles(After)`, `autoSettings` för båda skedena, `docValues`, de manuella beräkningarna och `projectConductors`.
2. **Filen:** Banken Ver. 6 med ST0098 och ST0099 före bilaga A och ST0110 efter, som riktig xlsx-text (`fromSheets`), läst med `readWorkbook`.
3. **Importen:** `bestSheet`, `recognize`, `extract`, `poleRows` och `importRecord` för alla rader, och sedan `importPolesBefore`.
4. **Kontrollerna:**
   - `poles(Before)` har 13 rader i filens ordning.
   - `autoSettings(Before)` är oförändrad (samma Stolpnr, läget på).
   - Allt i ögonblicksbilden är oförändrat.
   - `auto_calc` för före och faslinan har `first_row` 4 och `last_row` 9.
   - Den automatiska normalspannsberäkningen har stolparna ST0102–ST0107, och `calculateSpan` ger aₙ 315,2. Topplinorna ger 313,8.
   - `lastPoleImport` har 13 rader med Excelrad 19–31 och fälten på R, I, J, K, U, V, W och AG.
5. **En andra fil utan ST0107:**
   - `lostRanges` ger "Faslinan: Stolpnr ST0107 finns inte i filen.".
   - Efter importen är `autoSettings(Before)` fortfarande ST0102–ST0107, och `auto_calc` har felet "Stolpnr ST0107 finns inte i Stolptabell före.".
   - Rådata är ersatta, och efter, Dokumentation och beräkningarna är fortfarande oförändrade.
6. **Provfilen:** testet lämnar `Stolptabell prov.xlsx` (första filen) i `KALKYL_PROV_DIR` åt röktestet och handtestet.

### 10.7 Det som kräver fönster
Dialogen och knappen provas med glaze-render, röktest och handtest. De befintliga testerna går igenom oförändrade.

---

## 11. CMake och miniz

**Rotens `CMakeLists.txt`**, efter GlazeUI:
```cmake
# miniz (zip) hämtas på en fast tagg, aldrig master. Bara källorna hämtas (SOURCE_SUBDIR pekar på
# en katalog som inte finns): libs/xlsx bygger dem som kalkyl_miniz, eftersom minizs egen
# CMakeLists ändrar CMAKE_C_FLAGS och kräver CMake 3.5. Lokal utcheckning: -DFETCHCONTENT_SOURCE_DIR_MINIZ=<sökväg>.
FetchContent_Declare(miniz
    GIT_REPOSITORY https://github.com/richgel999/miniz.git
    GIT_TAG        3.1.2
    GIT_SHALLOW    TRUE
    SOURCE_SUBDIR  kalkyl-bara-kallor)
FetchContent_MakeAvailable(miniz)
```

**`libs/xlsx/CMakeLists.txt`:**
```cmake
# miniz som statiskt bibliotek ur källorna som rotens CMakeLists.txt hämtar. Tredjepartskod: inga
# varningar (-w), headrarna som SYSTEM, utan stdio, tid och zlib-namn. miniz_export.h skrivs här.
file(CONFIGURE OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/miniz/miniz_export.h" CONTENT [=[
// Genererad av libs/xlsx/CMakeLists.txt. miniz byggs statiskt i kalkyl_miniz, utan export.
#pragma once
#define MINIZ_EXPORT
#define MINIZ_NO_EXPORT
#define MINIZ_DEPRECATED
#define MINIZ_DEPRECATED_EXPORT
#define MINIZ_DEPRECATED_NO_EXPORT
]=])
add_library(kalkyl_miniz STATIC
    ${miniz_SOURCE_DIR}/miniz.c ${miniz_SOURCE_DIR}/miniz_zip.c
    ${miniz_SOURCE_DIR}/miniz_tinfl.c ${miniz_SOURCE_DIR}/miniz_tdef.c)
target_include_directories(kalkyl_miniz SYSTEM PUBLIC ${miniz_SOURCE_DIR} ${CMAKE_CURRENT_BINARY_DIR}/miniz)
target_compile_definitions(kalkyl_miniz PUBLIC MINIZ_NO_STDIO MINIZ_NO_TIME MINIZ_NO_ZLIB_COMPATIBLE_NAMES)
target_compile_options(kalkyl_miniz PRIVATE -w)

# Excelfiler: celler, delade strängar, talformat och sammanslagningar ur .xlsx och .xlsm.
add_library(kalkyl_xlsx STATIC xlsx.cpp xml_reader.cpp)
target_include_directories(kalkyl_xlsx PUBLIC ${PROJECT_SOURCE_DIR}/libs)
target_link_libraries(kalkyl_xlsx PUBLIC kalkyl_core PRIVATE kalkyl_platform kalkyl_miniz kalkyl_warnings)
```
- **Makronamnen:** paketet kontrollerar med `grep` i minizs källor att inga andra `MINIZ_…EXPORT`-namn används.

**`libs/poleImport/CMakeLists.txt`:**
```cmake
# Importen av en stolptabell: rubrikerna, valet av kolumner, utdraget och rådata som rena funktioner.
add_library(kalkyl_poleimport STATIC recognize.cpp extract.cpp)
target_include_directories(kalkyl_poleimport PUBLIC ${PROJECT_SOURCE_DIR}/libs)
target_link_libraries(kalkyl_poleimport PUBLIC kalkyl_xlsx kalkyl_poletable kalkyl_automode
                      PRIVATE kalkyl_tabletext kalkyl_warnings)
```

**`libs/CMakeLists.txt`:** `add_subdirectory(xlsx)` efter `platform` och `add_subdirectory(poleImport)` efter `xlsx`. Kommentaren om ordningen får "platform ← xlsx ← poleImport (med poleTable och autoMode)".

**`src/CMakeLists.txt`:** `windows/pole_import_dialog` i `glaze_add_window`, och `kalkyl_xlsx` och `kalkyl_poleimport` i länklistan.

**`tests/CMakeLists.txt`:**
```cmake
# Excelfiler och importen av stolptabellen.
kalkyl_add_test(xlsx xlsx_test.cpp kalkyl_xlsx kalkyl_miniz kalkyl_platform)
kalkyl_add_test(poleimport poleimport_test.cpp kalkyl_poleimport kalkyl_xlsx kalkyl_poletable kalkyl_automode kalkyl_tabletext)
kalkyl_add_test(poleimport_flow poleimport_flow_test.cpp kalkyl_poleimport kalkyl_xlsx kalkyl_miniz kalkyl_projectdb kalkyl_platform)
target_compile_definitions(kalkyl_poleimport_flow_test PRIVATE KALKYL_PROV_DIR="${CMAKE_BINARY_DIR}/prov")
```

**Krav:** bygget och konfigureringen är fria från varningar.
- `cmake --preset debug 2>&1 | grep -i warning` ger ingen utdata.
- Byggets utdata har inga varningar.
- `kalkyl.exe` länkas statiskt som förut (`-static`), och inga DLL:er tillkommer.

---

## 12. Arbetspaket

Fyra Sonnet-agenter (high) arbetar samtidigt. Varje agent äger sina filer, och ingen fil delas mellan paketen.

### Gemensamt för alla paket
- **Förberedelse (samordnaren, före agenterna):** miniz klonas en gång till en delad katalog:
  ```sh
  git clone --depth 1 --branch 3.1.2 https://github.com/richgel999/miniz.git C:/Users/sehoer/AppData/Local/Temp/claude/4074fa7132ac3ef7/a1fe004d-deaf-4f85-a3c0-d36997f28879/scratchpad/miniz-src
  ```
- **Byggkatalog:** varje paket har en egen katalog i repots rot, konfigurerad med MSYS2:s cmake (`C:/msys64/ucrt64/bin/cmake.exe`):
  ```sh
  cmake -S . -B build-<paket> -G Ninja -DCMAKE_BUILD_TYPE=Debug -DCMAKE_C_COMPILER=C:/msys64/ucrt64/bin/gcc.exe -DCMAKE_CXX_COMPILER=C:/msys64/ucrt64/bin/g++.exe -DKALKYL_TESTS=ON -DFETCHCONTENT_SOURCE_DIR_GLAZEUI=C:/Users/sehoer/Source/Kalkyl/build/_deps/glazeui-src -DFETCHCONTENT_SOURCE_DIR_MINIZ=C:/Users/sehoer/AppData/Local/Temp/claude/4074fa7132ac3ef7/a1fe004d-deaf-4f85-a3c0-d36997f28879/scratchpad/miniz-src
  ```
- **Städning:** `build-<paket>` tas alltid bort när agenten är klar (`rm -rf build-<paket>`). `build/` och miniz-klonen i scratchpad rörs aldrig: de byggs inte, rensas inte och konfigureras inte om.
- **Fel i ett annat pakets fil:** faller bygget i en fil som ett annat paket äger, vänta och försök igen. Ändra aldrig filen.
- **Steg 0:** inom några minuter skriver varje paket sina kontraktsfiler ordagrant ur planen, med kroppar som kompilerar, och de stubbar som CMake behöver. Ordningen är:
  1. paket 1: CMake-filerna i avsnitt 11, `xlsx.h`, stubbar för `xlsx.cpp` och `xml_reader.*`, `readFileShared` med stubb, en stubb av `xlsx_test.cpp` och hela `tests/xlsx_maker.h`,
  2. paket 2: `poleTable/import_record.h`, `pole_import.h`, `libs/poleImport/CMakeLists.txt`, stubbar för `recognize.cpp` och `extract.cpp`, en stubb av `poleimport_test.cpp` och `tests/banken_fixture.h`,
  3. paket 3: tilläggen i `project_data.h` och `system_db.h` med stubbar, tabellerna i steg 5 och en stubb av `poleimport_flow_test.cpp`,
  4. paket 4: regionen och `.glazeui` i huvudfönstret och dialogens tre filer med tomma handlers.

### Paket 1: xlsx och miniz (`build-xlsx`)
- **Äger:**
  - rotens `CMakeLists.txt` (bara miniz-blocket), `libs/CMakeLists.txt`, `libs/CLAUDE.md`,
  - `libs/xlsx/` (ny: `CMakeLists.txt`, `xlsx.h`, `xlsx.cpp`, `xml_reader.h`, `xml_reader.cpp` och `CLAUDE.md`),
  - `libs/platform/files.h`, `files.cpp` och `CLAUDE.md`,
  - `tests/CMakeLists.txt`, `tests/xlsx_test.cpp` (ny), `tests/xlsx_maker.h` (ny) och testet för `readFileShared` i `tests/platform_test.cpp`.
- **Arbete:** 3.1, 3.2, 4, 9.1, 10.1, 10.2 och 11.
- **Bygg och kör:** `ctest --test-dir build-xlsx -R "^(xlsx|platform)$"`.
- **Kontroll:** konfigureringen och bygget av `kalkyl_miniz` och `kalkyl_xlsx` har inga varningar.
- **CLAUDE.md:** `libs/CLAUDE.md` (raderna `xlsx/` och `poleImport/` och beroendena), `libs/xlsx/CLAUDE.md` och `libs/platform/CLAUDE.md`. Rapporten innehåller texten till `tests/CLAUDE.md`.

### Paket 2: igenkänningen (`build-recog`)
- **Äger:** `libs/poleImport/` (ny: `CMakeLists.txt`, `pole_import.h`, `recognize.cpp`, `extract.cpp` och `CLAUDE.md`), `libs/poleTable/import_record.h` (ny), `libs/poleTable/CLAUDE.md` (bara texten), `tests/poleimport_test.cpp` (ny) och `tests/banken_fixture.h` (ny).
- **Arbete:** 3.3–3.7, 5, 9.3 (`importIssues`), 10.3 och bilaga A. `import_record.h` skrivs ordagrant med typerna i 13.1.
- **Bygg och kör:** `ctest --test-dir build-recog -R "^poleimport$"`.
- **CLAUDE.md:** `libs/poleImport/CLAUDE.md` och `libs/poleTable/CLAUDE.md`. Rapporten innehåller texten till `tests/CLAUDE.md`.

### Paket 3: projektdatabasen och systemdatabasen (`build-db`)
- **Äger:**
  - `libs/projectdb/dev_migration.h` (bara tillägget sist i `kStep5`), `project_db.cpp` (tabellistan och antalet), `project_data.h`, `project_data.cpp` och `CLAUDE.md`,
  - `libs/systemdb/system_db.h`, `system_db.cpp` och `CLAUDE.md`,
  - `tests/projectdb_test.cpp`, `tests/systemdb_test.cpp` och `tests/poleimport_flow_test.cpp` (ny).
- **Arbete:** 3.8–3.10, 6, 7, 10.4, 10.5 och 10.6.
  - Bekräfta i rapporten att ingen annan skrivning än `importPolesBefore` rör `pole_import*`, och att `git diff` visar steg 1–4 oförändrade.
  - Flödestestet körs sist, när paket 1 och 2 har rapporterat.
- **Bygg och kör:** `ctest --test-dir build-db -R "^(projectdb|systemdb|poleimport_flow)$"`.
- **CLAUDE.md:** `libs/projectdb/CLAUDE.md` (steg 5 med de fyra tabellerna, `importPolesBefore`, `lastPoleImport`, regeln att handändringar inte rör rådata, SQL-exemplet och migreringstesten med 17 tabeller) och `libs/systemdb/CLAUDE.md`. Rapporten innehåller texten till `tests/CLAUDE.md`.

### Paket 4: fönstren (`build-ui`)
- **Äger:** `src/windows/pole_import_dialog.h`, `.glazeui` och `.cpp` (nya), `src/windows/main_window.h`, `.glazeui` och `.cpp`, `src/CMakeLists.txt`, `src/windows/CLAUDE.md` och `src/CLAUDE.md`.
- **Arbete:** 3.11, 3.12, 8 och 9.2–9.5.
  - Väntar på headrarna från paket 1–3. Funktionerna i `poleImport` kan vara stubbar tills paket 2 är klart.
- **Förhandsgranskning:** `-DGLAZE_TOOLS=ON` och `--target glaze-render`, sedan
  ```sh
  build-ui/_deps/glazeui-build/apps/glaze-render/glaze-render.exe src/windows/pole_import_dialog.h -o <scratchpad>/pole_import_dialog.png
  build-ui/_deps/glazeui-build/apps/glaze-render/glaze-render.exe src/windows/main_window.h -o <scratchpad>/main_window.png
  ```
  Exit 0 krävs, och bilderna granskas med Read: knappen bredvid rubriken och dialogens rader utan överlapp.
- **Röktest:**
  - `KALKYL_DATA_DIR=build-ui/smoke timeout -k 2 3 ./build-ui/src/kalkyl.exe; echo $?` ger 124 eller 137.
  - Med provprojektet och `Stolptabell prov.xlsx` ur `build-ui/prov` (efter `ctest -R "projectdb|poleimport_flow"`) visas huvudfönstret.
- **CLAUDE.md:** `src/windows/CLAUDE.md` (dialogens tre rader och huvudfönstrets knapp, `importPolesAsked` och `importFileDlg_`) och `src/CLAUDE.md` (länklistan).

### Regler för alla implementeringsagenter
- **Kommentarer:**
  - Varje funktion, klass och logisk del har en kommentar på 2–5 rader.
  - Inline-kommentarer är korta.
  - Ingen historik står i kommentarerna.
- **Fönster:**
  - Fönster görs bara med GlazeUI:s 3-filsmetod.
  - Inga layouter skrivs i kod utanför `.glazeui`, och `.glazeui` har ingen logik.
  - Klassen ligger utanför namespace, och designraden står kvar.
  - Startlogiken står i `onLoad`.
  - Från GlazeUI inkluderas bara `<glaze/glaze.h>` och `<glaze_threads/threads.h>`.
- **miniz** inkluderas bara i `libs/xlsx/xlsx.cpp` och `tests/xlsx_maker.h`.
- **Filredigering:**
  - Filer ändras bara med verktygen Edit och Write, aldrig med Python- eller sed-skript.
  - Skalet används bara för bygge, körning, test och läsning.
- **Hooken `~/.claude/hooks/only-c-drive.sh`** granskar varje verktygsanrop, också filinnehåll. Den stoppar:
  - två bakvända snedstreck i rad,
  - ett bakvänt snedstreck och en bokstav efter skiljetecken eller blanksteg och före citattecken eller slut,
  - andra enhetsbokstäver än C följda av kolon. `old_string` och `new_string` får aldrig sluta med en bokstav och ett kolon.

  **Därför:**
  - råa strängliteraler: `R"sql( … )sql"` och `R"( … )"`, också för XML i testerna,
  - teckenklasser som `[.]`,
  - `char{9}`, `char{10}`, `char{13}` och `char{34}`,
  - snedstreck framåt i sökvägar,
  - bracket-argument i CMake.
  - Testtexter med bakvänt snedstreck (formatkoder) byggs med `char{92}`.
- **Varningar:** bygget är fritt från varningar (`-Wall -Wextra`).
- **Versionen** rörs inte.
- **Inga commits.**

---

## 13. Integration (samordnaren)

### 13.1 Typerna i `libs/poleTable/import_record.h` (bindande, skrivs av paket 2 i steg 0)
```cpp
#pragma once
#include <string>
#include <vector>

namespace kalkyl::poletable {

// Cellens slag i en importerad fil.
enum class ImportCellKind { Text, Number, Bool, Error };

// En cell på en importerad rad: kolumnen (1 = A), slaget och värdet som det står i filen.
struct ImportCell {
    int column = 0;
    ImportCellKind kind = ImportCellKind::Text;
    std::string text;
    bool operator==(const ImportCell&) const = default;
};

// En importerad rad: Excels radnummer, Stolpnr som det blev i Stolptabell före och alla celler med
// värde i kolumnordning.
struct ImportRow {
    int excelRow = 0;
    std::string pole;
    std::vector<ImportCell> cells;
    bool operator==(const ImportRow&) const = default;
};

// En kolumn i filen: numret, bokstäverna, rubriken (rubrikraderna uppifrån med " › " emellan,
// sammanslagna celler ifyllda) och fältet i Stolptabell före (kPole … kLkl), -1 för inget.
struct ImportColumn {
    int column = 0;
    std::string letter;
    std::string header;
    int field = -1;
    bool operator==(const ImportColumn&) const = default;
};

// Rådata för en import till Stolptabell före. rows har en post per rad i tabellen, samma ordning.
struct ImportRecord {
    std::string fileName;      // bara namnet, "Stolptabell.xlsm"
    std::string sheetName;
    std::string title;         // "Stolptabell i Banken Ver. 6 …", "" utan titel
    std::string version;       // "Ver. 6", "" utan version
    std::string importedUtc;   // sätts av projectdb vid skrivningen
    int headerFirstRow = 0;
    int headerLastRow = 0;
    int firstDataRow = 0;
    std::string fromPole;
    std::string toPole;
    std::vector<ImportColumn> columns;
    std::vector<ImportRow> rows;
    bool operator==(const ImportRecord&) const = default;
};

} // namespace kalkyl::poletable
```

### 13.2 Stegen
1. **Före agenterna:** klona miniz till `miniz-src` i scratchpad (12, Förberedelse).
2. **Bygget i `build/`:**
   - `cmake --preset debug` konfigurerar om, och FetchContent klonar miniz till `build/_deps/miniz-src`.
   - `cmake --build --preset debug 2>&1 | grep -i warning` och konfigureringens utdata ger inga varningar.
3. **Testerna** i en tillfällig `build-verify`, konfigurerad som i 12 men med `-DFETCHCONTENT_SOURCE_DIR_MINIZ=C:/Users/sehoer/Source/Kalkyl/build/_deps/miniz-src` och `-DGLAZE_TOOLS=ON`:
   ```sh
   cmake --build build-verify && ctest --test-dir build-verify --output-on-failure
   ```
   Alla tester går igenom.
4. **glaze-render** ger exit 0 på `pole_import_dialog.h` och `main_window.h`.
5. **Röktest:**
   - Provprojektet och `build-verify/prov/Stolptabell prov.xlsx`: importen ger 13 stolpar.
   - Sträckorna i provprojektet står kvar.
   - Statusraden har texten i 9.5.
6. **Kontroller:**
   - `git diff libs/projectdb/dev_migration.h` visar bara tillägget sist i `kStep5`.
   - `kExpectedTableCount = 17`.
   - Rotens `CMakeLists.txt` har `GIT_TAG 3.1.2`.
   - `grep -rn "miniz.h" src libs tests` träffar bara `libs/xlsx/xlsx.cpp` och `tests/xlsx_maker.h`.
7. **Licensen:** minizs upphovsrätt läggs i `installer/licens.txt`, om användaren säger ja (17.1).
8. **Städning:** `build-verify` tas bort, så att inga `build-*` utom `build/` finns kvar.
9. **CLAUDE.md och `TODO.md`** enligt 15, och användaren får handtestet i 14.

---

## 14. Handtest (användaren)

Kör `build/src/kalkyl.exe`. Exempelfilen är `ref/Stolptabell.xlsm`, och en fil med 13 stolpar är `build-test/prov/Stolptabell prov.xlsx` (skapas av `ctest --preset test`).

**Grunderna**
1. **Knappen** "Importera stolptabell…" står bredvid "Data enligt dokumentation" i Stolptabell före. Den finns inte i Stolptabell efter.
2. **Importera exempelfilen** till ett projekt med en tom Stolptabell före.
   - Filväljaren öppnar i projektets mapp första gången.
   - Dialogen visar "Stolptabell i Banken Ver. 6 GRUNDLÄGGNING & JORDNING · rubriker på rad 11–15 · data från rad 19".
   - Alla åtta fält är Säker: R, I, J, K, U, V, W och AG.
   - Förhandsvisningen har en rad: rad 19, ST9002, 7 047 024,75, 588 746,85, 186,46, 422, tom Ah, 485 och T373/T373.
3. **Tryck Importera.**
   - Ingen fråga visas, eftersom tabellen var tom.
   - Statusraden säger "Importerade 1 stolpe från Stolptabell.xlsm till Stolptabell före.", och titeln får `*`.
4. **Importera igen.**
   - Filväljaren öppnar i exempelfilens mapp.
   - Frågan säger "Stolptabell före har 1 rad nu. Bara raderna byts …".
   - Nej lämnar dialogen öppen med valen kvar.
5. **Excel:** ha filen öppen i Excel under importen, och det går. Ändra något i Excel utan att spara: importen visar det sparade.

**Igenkänningen** (gör ändringarna i en kopia av exempelfilen och spara)

6. **Flytta** kolumnen U till efter AG och döp om "Stpl nr" till "Stolpnummer". Båda känns igen.
7. **Ta bort** kolumnen AG. LKL är Saknas (bärnsten), och importen ger tom LKL.
8. **Kopiera** kolumnen V (Ah) till en ny kolumn under Profil. Ah visar Välj (röd), och Importera är släckt tills en kolumn är vald.
9. **Byt kolumn för Ah** till N (beräknad).
   - Säkerheten blir Ditt val, och förhandsvisningen ändras.
   - Importera, öppna samma fil igen, och Ah står direkt på N som Ditt val. Det gäller också efter en omstart av Kalkyl.
10. **Välj samma kolumn för två fält.** Texten "Kolumn … är vald för både …" visas, och Importera är släckt.
11. **Peka på Säkerhet:** tooltipen visar poängen, skälet och näst bäst (för Av: "W … Näst bäst: X ”Profil › Av › Min temp › m”, 25 poäng.").

**Data**

12. **Skriv "ca 300"** i U på en rad. Cellen är röd med tooltip, texten "1 ogiltigt tal lämnas tomt …" visas, och efter importen räknas spannet ur koordinaterna (Σ).
13. **Två rader med samma Stolpnr** ger varningen, och båda blir röda i tabellen efter importen.
14. **En rad utan Stolpnr** men med koordinater listas som överhoppad.
15. **Från och Till:** välj en delsträcka, och antalet och förhandsvisningen följer. Till före Från ger felet, och "Alla stolpar" återställer.

**Bara raderna byts**

16. **Med automatiskt läge på** och en sträcka i före, importera en fil där sträckans stolpar står på andra rader.
    - Sträckan och läget står kvar, och aₙ i Dokumentation är densamma.
    - Stolptabell efter är orörd. Dess färger kan ändras, eftersom de jämförs med den nya före.
    - Normalspann och Linberäkning har samma poster.
17. **Importera en fil som saknar sträckans slutstolpe.**
    - Frågan listar "Faslinan: Stolpnr … finns inte i filen.".
    - Efter Ja visar panelen felet och rullistan "… (finns inte)". Välj om sträckan, och felet försvinner.
18. **Ändra en cell för hand efter importen.** Det fungerar som förut. Rådata syns inte i programmet; den som vill kan öppna en kopia av `.kalkyl` i en SQLite-läsare och köra SQL-exemplet i 6.3.

**Fel och övrigt**

19. **Välj en .xls, en .docx eller en lösenordsskyddad .xlsx.** Texterna i 9.1 visas i dialogen, och en annan fil går att välja med "…".
20. **En arbetsbok med flera blad:** bladet Stolptabell används, och de andra bladen syns inte i dialogen.
21. **En fil på en nätverksshare** läses med "Läser …" och väntepekaren utan att fönstret fryser.
22. **Skrivskyddat projekt:** knappen är släckt.
23. **Avbryt eller Escape** i dialogen ändrar ingenting.
24. **Ctrl+S, stäng och öppna:** den importerade tabellen finns kvar.
25. **Mörkt och ljust läge:** säkerhetsfärgerna och de röda cellerna syns i båda.

---

## 15. CLAUDE.md och TODO.md

| Fil | Vem | Ändring |
|---|---|---|
| Rotens `CLAUDE.md` | samordnaren | Se nedan |
| `libs/CLAUDE.md` | paket 1 | Raderna `xlsx/` och `poleImport/`. Beroendena: "`xlsx` beror på `core`, `platform` och miniz (`kalkyl_miniz`); `poleImport` beror på `xlsx`, `poleTable`, `autoMode` och `tableText`" |
| `libs/xlsx/CLAUDE.md` (ny) | paket 1 | Filerna, `kalkyl_miniz` (källorna ur FetchContent, `-w`, `SYSTEM`, definitionerna), läsreglerna i 4.3, att makron aldrig läses och gränserna |
| `libs/platform/CLAUDE.md` | paket 1 | `readFileShared` med delad läsning |
| `libs/poleImport/CLAUDE.md` (ny) | paket 2 | Filerna, rubrikområdet, sammansatta rubriker, normaliseringen, poängtabellen, valet och säkerheten, utdraget, rådataposten, `importIssues` och `lostRanges`. Regeln att en ny regel får ett test med en fälla |
| `libs/poleTable/CLAUDE.md` | paket 2 | `import_record.h` och vem som delar den |
| `libs/projectdb/CLAUDE.md` | paket 3 | Steg 5 med `pole_import*`, `importPolesBefore`, `lastPoleImport`, regeln att handändringar inte rör rådata, SQL-exemplet och 17 tabeller |
| `libs/systemdb/CLAUDE.md` | paket 3 | `last_import_folder`, `pole_import_learned`, `learnedHeaders` och `learnHeader` |
| `src/windows/CLAUDE.md`, `src/CLAUDE.md` | paket 4 | Dialogens rader och huvudfönstrets knapp, flöde och länkar |
| `tests/CLAUDE.md` | samordnaren | Raderna `xlsx`, `poleimport`, `poleimport_flow`, `xlsx_maker.h` och `banken_fixture.h`, och tilläggen för `platform`, `projectdb` (17 tabeller, rådata) och `systemdb`, med paketens text |
| `doc/plans/CLAUDE.md` | samordnaren | Raden för den här planen |
| `TODO.md` | samordnaren | Se nedan |

**Rotens `CLAUDE.md`**
- **Delar:**
  - Huvudfönstret: "Importera stolptabell… i Stolptabell före (Banken-tabell ur Excel, bara raderna byts)".
  - Ny rad: Import av stolptabell, `src/windows/pole_import_dialog.*`, "Dialogen för Importera stolptabell…: fil och blad, kolumnerna med säkerhet och rullista per fält, Från och Till, förhandsvisning och frågan om ersättning".
  - Nya rader: `libs/xlsx` ("Excelfiler: celler, delade strängar, talformat och sammanslagningar ur .xlsx och .xlsm med miniz, utan makron") och `libs/poleImport` ("Importen av en stolptabell: rubrikerna, valet av kolumner, utdraget och rådata som rena funktioner").
  - Systemdatabasen: "… senaste importmapp och inlärda rubriker".
  - Projektdatabasen: "… den senaste importens rådata (`pole_import*`)".
- **Teknik:** "Zip: miniz hämtas med FetchContent på en fast tagg (nu `3.1.2`), bara källorna, och byggs statiskt som `kalkyl_miniz` i `libs/xlsx`. Uppgradera genom att byta taggen."
- **Byggkataloger** under Arbetssätt: agenternas kataloger får också `-DFETCHCONTENT_SOURCE_DIR_MINIZ=C:/Users/sehoer/Source/Kalkyl/build/_deps/miniz-src`.
- **Data:** "Projektet sparar … och den senaste importens rådata: filens namn, blad, version och tid, kolumnernas rubriker och fält och alla celler på de importerade raderna. En ny import ersätter dem, och handändringar rör dem inte."

**`TODO.md`**
- **Arbetspunkten:** ⬜ när planen är klar och 🔨 under implementeringen. Texten: "knappen Importera stolptabell… i Stolptabell före läser en stolptabell från Banken (.xlsx/.xlsm) med miniz, känner igen kolumnerna på rubrikerna, användaren väljer Från och Till, och rådata sparas i projektet".
- **Nya idéer:**
  - "💡 Visa importens rådata (alla Banken-kolumner) per stolpe, till exempel i en tooltip eller i utskrifterna."
  - "💡 Importera Dokumentationens uppgifter ur ByggPM."

---

## 16. Risker

1. **Fel kolumn med hög säkerhet.** En ny version kan ha en rubrik som liknar ett annat fält. Förhandsvisningen, Första värdet och tooltipen med näst bäst gör det synligt, och en fälla per regel finns i testerna.
2. **Inlärd rubrik som blir fel.** Det senaste valet vinner, och användaren byter i rullistan. Två kolumner med samma inlärda rubrik ger Välj.
3. **Excel låser filen helt.** Det är ovanligt, eftersom Excel tillåter delad läsning, men det ger texten om Busy.
4. **Stora filer.** Läsningen sker i bakgrunden, och 1 000 rader med 60 kolumner tar under en sekund att läsa och spara. Projektfilen växer med 2–3 MB.
5. **Talens precision.** Riktiga tal visas med högst 15 värdesiffror eller med cellens decimaler, som i Excel. Rådata behåller `<v>` exakt.
6. **Andra program som skriver xlsx** (LibreOffice, Google). Prefix, inline-strängar och rader utan `r` hanteras och testas. Strict OOXML läses också, eftersom lokala namn jämförs.
7. **Färgerna i efter ändras** efter en import, fast efter är orörd (8.3). Det står i handtestet.
8. **Sträckor som tappar en stolpe.** De visas i frågan och i panelen, och inget nollställs.
9. **Synkningen i samma transaktion** följer regeln i `libs/projectdb/CLAUDE.md`. `importPolesBefore` kör den, och flödestestet kontrollerar resultatet.

---

## 17. Öppna frågor

Besvarade av användaren 2026-11-26:

1. **Licensen för miniz.** Svar: ja. Ett stycke sist i `installer/licens.txt`: "Kalkyl innehåller miniz (MIT-licens). Copyright 2013-2014 RAD Game Tools and Valve Software. Copyright 2010-2014 Rich Geldreich and Tenacious Software LLC." följt av MIT-texten. Samordnaren skriver det vid integrationen.
2. **Dolda eller filtrerade rader i Excel.** Svar: alla rader på bladet importeras, också dolda och filtrerade, och Från–Till väljer. Bara bladet som `bestSheet` väljer läses; dialogen har inget val av blad (8.2).
3. **Inlärda val.** Svar: i systemdatabasen, per användare, som planen säger (3.10).

---

## Bilaga A – Banken Ver. 6 i testerna

`testfixture::bankenV6(rows)` bygger ett blad "Stolptabell" med de här cellerna. Texterna är mallens rubriker, utan data ur `ref/`.

**Raderna ovanför rubrikerna**
- B1: "   Stolptabell i Banken Ver. 6 GRUNDLÄGGNING & JORDNING" (raden dold är inte nödvändigt).
- B2: "Stolptabell med data hämtad från IFS (Linjär tillgång). Mall Ver. 11.0 (svk)".
- B9: "Provlinje L1". Rad 10 är tom.

**Rubrikerna** (kolumn, rad och text, radbrytning som `char{10}`)
| Kolumn | Rad 11 | Rad 12 | Rad 13 | Rad 14 | Rad 15 |
|---|---|---|---|---|---|
| A | | | | Internt Id | |
| B | Objekt ID | Ordningsnr | | | |
| I | Koordinater | N | | | SWEREF 99 TM |
| J | | E | | | SWEREF 99 TM |
| K | | H | | | RH 2000 |
| L | | Beräknat från koordinater | Avst. föreg. | | m |
| M | | | Beräknat, radbrytning, totalavst., radbrytning, från, radbrytning, station | | m |
| N | | | Ah | | m |
| R | Profil | Stpl nr | Slutgiltigt | | |
| S | | | Byggnads | | |
| T | | Sektion | | | km+m |
| U | | Avst föreg stolpe | | | |
| V | | Ah | | | m |
| W | | Av | Max temp | | m |
| X | | | Min temp | | m |
| AC | Stolpe | Stolpe | | | |
| AG | | | Längdklass | | |
| AP | Fundament | | | | |
| AW | | Höjd till underdel ö.k, radbrytning, Höjd till årtalsspik | | | m |
| BB | Stag / Stagförankring | | | | |
| BF | | Stagförankring | | | |
| BG | | | Mått från typritning | | |
| BH | | | | E | cm |
| BN | Linor / Isolation | Faslinor | | | |
| BP | | | Max temperatur | | °C |

**Rad 16** har kolumnens nummer som tal i varje kolumn från B till BP (B = 2 … BP = 68).

**Sammanslagningarna**
- Rad 11: B11:F11, I11:O11, R11:Z11, AC11:AM11, AP11:AY11, BB11:BK11 och BN11:CB11.
- Rad 12: B12:B14, I12:I14, J12:J14, K12:K14, L12:O12, R12:S12, T12:T14, U12:U14, V12:V14, W12:X12, AC12:AJ12, AW12:AW14, BF12:BJ12 och BN12:BR12.
- Rad 13: L13:L14, M13:M14, N13:N14, R13:R14, S13:S14, W13:W14, X13:X14, AG13:AG14, BG13:BI13 och BP13:BP14.

**Dataraderna** börjar på rad 19 (rad 17 och 18 är tomma), en per `PoleRow`:
- A är internt id som tal (214021 och uppåt) och B "L1:0001" och uppåt.
- I, J och K är N, E och H som text med decimalkomma.
- L är spannet + 1 som text, och N är "999".
- R är Stolpnr, S "A", U spannet, V Ah, W Av och X Av − 100.
- AG är LKL med delarna på egna rader och en tom rad emellan ("255", tom rad, "262").
- BH är "256", radbrytning, "256".
- Tomma fält ger ingen cell.

**Väntat val:** Stolpnr R, Koordinat N I, Koordinat E J, Koordinat H K, Avstånd föregående U, Ah V, Av W och LKL AG, alla Säker.

---

### Kritiska filer för implementeringen
- C:/Users/sehoer/Source/Kalkyl/libs/poleImport/pole_import.h (ny)
- C:/Users/sehoer/Source/Kalkyl/libs/xlsx/xlsx.h (ny)
- C:/Users/sehoer/Source/Kalkyl/libs/projectdb/project_data.cpp
- C:/Users/sehoer/Source/Kalkyl/libs/projectdb/dev_migration.h
- C:/Users/sehoer/Source/Kalkyl/src/windows/main_window.cpp
- C:/Users/sehoer/Source/Kalkyl/src/windows/pole_import_dialog.cpp (ny)