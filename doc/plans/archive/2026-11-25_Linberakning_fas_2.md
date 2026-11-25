# Linberäkning fas 2 och 3: fliken Linberäkning, formuläret och kopplingen till Dokumentation

**Datum:** 2026-11-25, uppdaterad samma dag med användarens svar · **Status:** klar (handtestad och granskad; ändringarna under handtestet står i rotens och katalogernas CLAUDE.md) · **Version:** 0.3.0

Planen bygger vidare på fas 1 (`doc/plans/archive/2026-11-25_Linberakning.md`, modulen `libs/lineCalc`) och gör skisserna i dess avsnitt 9 och 10 till en fullständig plan. Den är kontraktet för paketen A–D och för granskningen. Schemat i 3, header-skisserna i 4.1 och 5.1, formulärets publika gränssnitt i 6.1, kolumnerna i 6.3 och texterna i 4.4, 6.6 och 8.2–8.5 är bindande. Namn, signaturer och texter följs ordagrant. Behöver ett paket ändra en signatur meddelar det samordnaren i sin rapport, och samordnaren beslutar.

**Användarens beslut som gäller** (ur fas 1 och uppdraget):
1. **Uppställningen följer arket.** Formuläret har blocken Indata, Lindata/Konstanter och Lastfall med Resultat och Data till SAG (3D), samma rader och kolumner som arket och knapparna Återställ, Övre linan och Nedre linan.
2. **Redigering som i Excel.** Indata, lindata, konstanter och lastfall går att ändra. Att välja en lina fyller i lindatat, som sedan kan ändras för hand.
3. **Läget Excel eller Ny** per beräkning, med en växelknapp. Förval Ny. Skillnaden mellan lägena ska gå att se (förslaget i 6.12).
4. **En ny beräkning** har T₀ = 0, min −50 och max +50. Linan, σ₀ och aₙ är tomma.
5. **Mallen till Dokumentation** är stakningsmallen, Mall (Z) i lastfall 15 (`stakingMall`).
6. **En kopplad beräkning** hämtar linan, aₙ och σ₀ ur Dokumentation: aₙ från normalspannsraden för samma lina och skede, annars från den gröna raden "Normalspann …", annars saknas den.
7. **En manuell beräkning** har en egen kopia av linan.
8. **Koppling:** en kopplad beräkning hör till en rad. En manuell beräkning kan visas på flera rader.
9. **Fliken** har samma funktioner som Normalspann och högsta utnyttjandegrad som kolumn.
10. **Bibliotek:** varje bibliotek har en uppgift. `projectdb` får länka `lineCalc` för lagringen.
11. **Notis från granskningen av fas 1:** ett lastfall med manuell last visar kolumnerna J, K och M–T tomma, också O, som Excel (6.4).

**Användarens svar 2026-11-25** (inarbetade i avsnitten):
12. **En kopplad beräkning använder Dokumentationens värden, inte en kopia.** Användarens ord: "När man kör en lägg till där igenom, så ska den inte bara ta o kopiera de värderna, utan det är de värderna vi använder. så uppdateras dem så uppdateras mallen vi ser också. Automatiskt. Så dessa kan vi inte ändra, utan det är de värden som vi har. Manuella däremot, där för vi in det manuellt."
    - Linan med lindata, aₙ, σ₀ och min- och maxtemperaturen hämtas ur Dokumentation varje gång beräkningen räknas (2.3, 4.2).
    - Ändras Dokumentation räknas beräkningen om, och mallen på raden ändras direkt (8.4).
    - Fälten ur Dokumentation är låsta i formuläret och visas som hämtade, med en tooltip om varifrån (6.6). T₀, linhöjden, vindtrycken, konstanterna, lastfallen och läget går att ändra i beräkningen.
    - Saknas ett värde i Dokumentation får beräkningen inget resultat, och upplysningen säger var värdet fylls i (4.4). Det finns inget förval för temperaturerna (2.3).
13. **"Lägg till manuell…"** börjar med en kopia av Dokumentationens värden, och sedan går allt att ändra fritt (8.3).
14. **"Koppla bort" på en kopplad beräkning** gör den manuell. Den behåller sina senaste värden som egna och finns kvar i fliken (2.2).
15. **Skillnaden mellan Excel och Ny** visas med kryssrutan i formuläret, och det räcker (6.12).
16. **OK vid fel** sparar efter en fråga, och listan i fliken visar att beräkningen har fel (2.7, 7.2).

Det finns inga öppna frågor (avsnitt 16).

---

## 1. Mål och avgränsning

### 1.1 Fas 2: fliken och formuläret
- **Projektdatabasen** sparar linberäkningarna i ett nytt dev-steg 3, med 15 lastfall per beräkning och kopplingen till Dokumentation (avsnitt 3 och 4).
- **Formuläret `LineCalcDialog`** visar en beräkning med arkets uppställning, räknar om vid varje ändring och har Excel-markering och urklipp som normalspannsformuläret (avsnitt 6).
- **Biblioteket `libs/sheetGrid`** ger Excel-beteendet (markering, urklipp, inklistring) i en DataGrid. Formuläret har tre sådana tabeller (avsnitt 5).
- **Fliken Linberäkning** i huvudfönstret, efter fliken Normalspann, med en lista och knapparna Ny beräkning…, Ändra…, Kopiera… och Ta bort (avsnitt 7).

### 1.2 Fas 3: kopplingen till Dokumentation
- **De sex raderna "Linberäkning …"** i Dokumentation får en högerklicksmeny som normalspannsraderna och visar stakningsmallen när en beräkning är kopplad (avsnitt 8).
- **Kopplad beräkning:** linan, aₙ, σ₀, min och max är Dokumentationens värden. De hämtas varje gång beräkningen räknas och kan bara ändras i Dokumentation (beslut 12).
- **Manuell beräkning:** egna indata och en egen kopia av linan. Den kan visas på flera rader eller vara lös i fliken.

Fas 2 och 3 byggs i samma omgång. Schemat i steg 3 har därför redan kolumnen för fas 3 (`doc_source`), så att inget steg 4 behövs om fas 2 skulle committas för sig.

### 1.3 Ingår inte
- Arken utanför Linberäkning (fas 1, 1.3).
- Ångra (Ctrl+Z) i formulärets tabeller. Avbryt ångrar allt, som i normalspannsformuläret.
- Markering genom att dra med musen (GlazeUI saknar den, normalspannsplanen 3.11).
- Normalspannsformuläret byter inte till `libs/sheetGrid`. Det fungerar och är handtestat. Samordnaren lägger en idé i `TODO.md` om att flytta det senare.
- Utskrift av linberäkningen.
- **Versionen** står kvar på 0.3.0. Inga commits.

---

## 2. Beslut med motivering

### 2.1 Lagringen: en rad per beräkning och en rad per lastfall
- **Tabellen `line_calc`** har en kolumn per fält i `LineInput` (arkets J2–J18 och O3–O17), läget, anteckningen och källan i Dokumentation. **`line_calc_case`** har 15 rader per beräkning, en per lastfall, med primärnyckeln (calc_id, case_id). Det följer `normal_span` och `normal_span_pole`.
- **Lindatat lagras i beräkningen** (`cond_*`-kolumnerna), inte som en referens till `project_conductor`. Skälen:
  - En manuell beräkning har en egen kopia (beslut 7), och lindatat får ändras för hand efter att en lina valts (beslut 2). En delad kopia hade ändrat linan för Dokumentation och alla andra beräkningar.
  - Rensningen av oanvända linor i `project_conductor` (`pruneConductors`) ändras inte, eftersom linberäkningarna inte refererar till den tabellen.
- **Källorna** (temperatur och vindtryck per lastfall) och läget lagras som text: `fixed`, `min`, `min_plus_20`, `max`, `short_circuit`, `short_circuit_rise`; `fixed`, `normal`, `high`; `excel`, `new`. Text är läsbar i en SQLite-läsare och tål att en enum växer.
- **Manuell last** är två kolumner som båda är NULL (ingen manuell last) eller båda har ett tal. Då kan flaggan och talen aldrig gå isär.
- **Inga CHECK-villkor**, som i steg 2. Reglerna ligger i API:t (4.3), och en okänd text vid läsning ger felet `Invalid` (en fil från en nyare version vägras redan av `TooNew`).
- **Resultatet lagras aldrig.** `calculateLine` tar under en millisekund. Ett lagrat resultat hade kunnat hamna i otakt med indata eller med Dokumentation.

### 2.2 Kopplad och manuell beräkning
- **`doc_source`** i `line_calc` är nyckeln för den linberäkningsrad som beräkningen hämtar sina indata ur (`line_calc_phase_before` …). NULL betyder manuell.
- **Ingen kopia (beslut 12).** En kopplad beräkning lagrar inte linan, aₙ eller σ₀: `cond_*`, `normal_span_m` och `tension_mpa` är NULL. `min_temp_c` och `max_temp_c` lagras bara som det senast hämtade värdet, för att se när Dokumentation ändrats (4.3). Räkningen använder alltid Dokumentationens värden.
- **Regeln (beslut 8)**, som API:t håller: en kopplad beräkning är kopplad till exakt en rad, och det är raden i `doc_source`. En manuell beräkning kan vara kopplad till noll, en eller flera rader.
- **En kopplad beräkning som förlorar sin rad blir manuell** (beslut 14). Dokumentationens värden i det ögonblicket skrivs in som beräkningens egna, i samma transaktion som ändringen. Det händer när användaren väljer Koppla bort på raden och när en annan beräkning kopplas till raden. Inget försvinner då. Beräkningen finns kvar i fliken som manuell.
- **Formuläret kan byta** mellan kopplad och manuell med kryssrutan "Hämta lina, aₙ, σ₀ och temperaturer ur Dokumentation" (6.6). Bytet sparas med OK.
- **Kopplingstabellen `doc_line_calc`** har samma form som `doc_normal_span`: `doc_key` är primärnyckel, så en rad visar högst en beräkning.

### 2.3 Var Dokumentationens värden hämtas: i `projectdb`
- **Upplösningen** (vilken lina, vilket aₙ, vilket σ₀ och vilka temperaturer en kopplad beräkning får) gäller Dokumentationens nycklar och normalspannsberäkningarna. Det är projektdatabasens begrepp, så den ligger i `project_data` och inte i `lineCalc`, som är ren beräkning.
- **Två steg:** `docLineSources(p)` läser det som behövs en gång (Dokumentationens värden, projektets linor och aₙ för varje kopplad normalspannsrad), och den rena funktionen `resolveLineInput(input, docSource, sources)` sätter in värdena. Huvudfönstret läser en gång och löser upp alla beräkningar. Formuläret gör samma sak. Testerna provar den rena funktionen utan fil.
- **Min och max** sätts med `linecalc::applyInputChange`, bara när värdet skiljer sig från det senast hämtade. Då följer lastfallen 1, 2 och 15 med i läget Excel, som arkets händelser, och läget Ny följer källorna (fas 1, 4.2, D7).
- **Saknade värden ger inget resultat.** Saknas linan, aₙ, σ₀, min eller max i Dokumentation ger upplösningen en Missing-upplysning, och `calculateResolved` ger inget resultat. Raden i Dokumentation visar då "-", och tooltipen säger vad som ska fyllas i.
- **Inget förval för temperaturerna.** Mallen är nedhänget vid maxtemperaturen, så ett förval som +50 °C när Dokumentation har +60 °C hade gett en fel mall utan att det syns. Det är tryggare att raden visar "-" tills Dokumentation är ifylld.

### 2.4 Formuläret i GlazeUI: tre tabeller som arkets block
Kontrollerat i `MANUAL.md` (`widget-data-grid`, `widget-list-view`) och headrarna `table.h`, `column_panel.h` och `data_grid.h` för v0.11.1:

| Arket har | GlazeUI v0.11.1 | Formuläret gör |
|---|---|---|
| Celler med rutnät, grå etikettceller och vita inmatningsceller | DataGrid med redigerare per cell (`setCellEditor`), skrivskydd per cell (`readOnly`), bakgrund och textfärg per cell (`setCellColor`) och tooltip per cell | Blocken Indata och Lindata/Konstanter blir var sin DataGrid med fyra kolumner: etikett, värde, etikett, värde |
| Blockens namn lodrätt till vänster ("Indata", "Lindata / Konstanter", "Lastfall") | **Ingen lodrät text** | Namnet står i tabellens rubrikrad, över etikettkolumnen |
| Rullistan Lina över J2:O2 (sammanslagna celler) | **Inga sammanslagna celler**. En val-cell är bara så bred som sin kolumn | En egen Dropdown `conductorDd` ovanför tabellen Indata, lika bred som J2:O2 i arket |
| Rubriker på flera rader, delvis lodräta (J, K, L, F) | Rubriker på flera rader (`setHeaderLines`, radbrytning i texten, `setHeaderVAlign`). Ingen lodrät text | Rubrikerna skrivs vågrätt på upp till sex rader. Arkets lodräta rubriker bryts i ord (6.3) |
| Grupprubrikerna Lastfall, Resultat och Data till SAG (3D) över flera kolumner | **Ingen rubrik över flera kolumner** | Gruppens namn står på rubrikens översta rad i gruppens första kolumn (ID, U och AC) |
| Tom rad 30 mellan lastfall 7 och 8 | `RowKind::Blank`: en tomrad utan rutnät som aldrig väljs | Rad 7 i tabellen är en tomrad, så att ett block ur Excel (F23:AE38) passar rad för rad |
| Kolumn F (ID) står kvar när man rullar | Radrubriker (`setRowHeaders`): kolumn 0 står kvar vid rullning i sidled och går inte att ändra | ID är radrubrik |
| Tom kolumn AB mellan Resultat och SAG | Inga mellanrum mellan kolumner | En smal tom kolumn utan rubrik (12 px) |
| Fet stil på J2–J7 och O3 ("triggar beräkningen") | **Ingen fet stil per cell** | Behövs inte: Kalkyl räknar om vid varje ändring. Infotexten säger det |
| Röd utnyttjandegrad | Cellfärg | Röd bakgrund som i normalspannsformuläret |
| Områdesmarkering, urklipp, Delete | **Saknas i DataGrid** (normalspannsplanen 3.11) | Biblioteket `libs/sheetGrid` (2.5) |

- **Rullning:** formulärets innehåll ligger i en rullande panel (`sheetPanel`), som arket rullar i Excel. På en skärm med 1920 × 1080 och 100 % skala ryms nästan allt. Med 125 % skala rullar panelen i höjdled. Lastfallstabellen rullar själv i sidled, och ID står kvar.
- **Stavningen** i arkets etiketter rättas där det är uppenbara fel ("Karekteristiskt" blir "Karakteristiskt", "Mpa" blir "MPa", "clearences" blir "clearances"). Orden är annars arkets.
- **Kortslutningstemperaturen** (Tks) syns som indata i den tomma cellen O6. I arket finns den bara i frågan vid Övre linan, men läget Ny räknar med den (källorna ShortCircuit och ShortCircuitRise), så den måste gå att se och ändra.

### 2.5 Excel-beteendet i ett eget bibliotek `libs/sheetGrid`
- **Behovet:** formuläret har tre tabeller som ska bete sig som Excel: rektangulär markering med Shift och Shift+klick, Ctrl+C, X, V, A, Delete och snabbmenyn. Normalspannsformuläret har samma sak inbyggd för en tabell (cirka 400 rader). Tre kopior vore fel.
- **Biblioteket** `kalkyl_sheetgrid` (namnrymd `kalkyl::sheetgrid`, include `"sheetGrid/sheet_grid.h"`) har en enda uppgift: Excel-beteendet i en GlazeUI DataGrid. Det beror på `glaze` (PUBLIC, som `appearance`) och `tableText` (PRIVATE).
- **Fönstret äger innehållet.** Biblioteket frågar fönstret vilka celler som är indata, vad en cell kopieras som, om ett inklistrat fält passar och vilken färg en cell har, och lämnar ändringarna till fönstret som en lista (5.1).
- **Planeringen av en inklistring** är en ren funktion (`planSheetPaste`) med test utan fönster.
- **Tabellerna i formuläret har ett fast antal rader.** Biblioteket lägger aldrig till eller tar bort rader. Därför passar det inte normalspannsformuläret rakt av (1.3).

### 2.6 Läget och skillnaden mellan lägena
- **Växelknappen** är två radioknappar i gruppen "Läge": "Ny (felen i arket rättade)" och "Excel (räknar exakt som arket)". Bytet räknar om direkt och sparas med beräkningen.
- **Skillnaden** visas med kryssrutan "Markera skillnaden mot läget Excel" (eller "… läget Ny" när Excel är valt). Formuläret räknar då båda lägena, och varje cell vars visade värde skiljer sig får en egen färg och en tooltip med båda värdena (6.12). Statusraden säger hur många celler som skiljer sig. Kryssrutan sparas inte och är av när formuläret öppnas.
- **Excel-läget går att ta bort senare** utan att formuläret ändras mer än att radioknapparna och kryssrutan tas bort.

### 2.7 OK med fel i beräkningen
- En linberäkning kan få fel av värden i Dokumentation, som användaren inte kan ändra i formuläret. Att spärra OK som i normalspannsformuläret hade gjort formuläret till en fälla.
- **OK sparar därför också en beräkning med upplysningar.** Med Missing sparas den utan fråga. Med Error frågar formuläret "Linberäkningen har fel: <text>. Vill du spara den ändå?" (beslut 16). Listan i fliken visar sedan "Fel" i röd text (7.2), och raden i Dokumentation visar "-" med felet som tooltip (8.2).
- Ogiltiga tal i en cell ("abc") sparas aldrig. De stoppar OK som i normalspannsformuläret.

---

## 3. Schemat: dev-steg 3

### 3.1 `libs/projectdb/dev_migration.h`
Steg 1 (v0.1.0) och steg 2 (v0.2.0, committat) ändras aldrig. Nytt sist i filen, med en kommentar på 2–5 rader:

```cpp
// Steg 3 (user_version 2 -> 3): linberäkningarna med sina 15 lastfall och kopplingen till
// raderna "Linberäkning ..." i Dokumentation. IF NOT EXISTS som i steg 2.
inline constexpr std::string_view kStep3 = R"sql(
-- Linberäkningarna enligt arket Linberäkning. doc_source NULL = manuell, med lindatat som egen
-- kopia (cond_name NULL = inget lindata, '' = lindata utan namn). Annars nyckeln för raden som
-- linan, an, sigma0, min och max hämtas ur; då är cond_*, normal_span_m och tension_mpa NULL.
CREATE TABLE IF NOT EXISTS line_calc (
    id                         INTEGER PRIMARY KEY AUTOINCREMENT,
    note                       TEXT NOT NULL DEFAULT '',
    mode                       TEXT NOT NULL DEFAULT 'new',
    doc_source                 TEXT,
    cond_name                  TEXT,
    cond_type                  TEXT NOT NULL DEFAULT '',
    cond_area_mm2              REAL,
    cond_diameter_mm           REAL,
    cond_e_initial_mpa         REAL,
    cond_e_permanent_mpa       REAL,
    cond_e_initial_limit_mpa   REAL,
    cond_eiv_constant          REAL,
    cond_alpha_e6_per_c        REAL,
    cond_creep_strain_permille REAL,
    cond_creep_temp_c          REAL,
    cond_weight_n_per_m        REAL,
    cond_breaking_load_kn      REAL,
    normal_span_m              REAL,
    tension_mpa                REAL,
    ref_temp_c                 REAL NOT NULL DEFAULT 0,
    min_temp_c                 REAL NOT NULL DEFAULT -50,
    max_temp_c                 REAL NOT NULL DEFAULT 50,
    short_circuit_temp_c       REAL NOT NULL DEFAULT 100,
    line_height_m              REAL NOT NULL DEFAULT 25,
    normal_wind_n_per_m2       REAL NOT NULL DEFAULT 500,
    high_wind_n_per_m2         REAL NOT NULL DEFAULT 800,
    ice_density_kg_m3          REAL NOT NULL DEFAULT 917.4,
    pi                         REAL NOT NULL DEFAULT 3.1416,
    gravity                    REAL NOT NULL DEFAULT 9.81,
    gust_factor                REAL NOT NULL DEFAULT 1,
    resonance_factor           REAL NOT NULL DEFAULT 0.5,
    drag_factor                REAL NOT NULL DEFAULT 1,
    min_ice_n_per_m            REAL NOT NULL DEFAULT 20,
    max_utilization_percent    REAL NOT NULL DEFAULT 55,
    transposition_case         INTEGER NOT NULL DEFAULT 5,
    created_utc                TEXT NOT NULL,
    modified_utc               TEXT NOT NULL
);
-- Lastfallen 1-15 (arkets rad 23-29 och 31-38). Källorna som text. manual_* båda NULL = ingen
-- manuell last, båda satta = I och Qwc inskrivna för hand.
CREATE TABLE IF NOT EXISTS line_calc_case (
    calc_id                    INTEGER NOT NULL REFERENCES line_calc(id) ON DELETE CASCADE,
    case_id                    INTEGER NOT NULL,
    temp_source                TEXT NOT NULL DEFAULT 'fixed',
    temp_c                     REAL NOT NULL DEFAULT 0,
    name                       TEXT NOT NULL DEFAULT '',
    standard_case              TEXT NOT NULL DEFAULT '',
    clearance_case             TEXT NOT NULL DEFAULT '',
    wind_from_case             INTEGER,
    wind_magnitude_percent     REAL NOT NULL DEFAULT 0,
    pressure_source            TEXT NOT NULL DEFAULT 'fixed',
    char_pressure_n_per_m2     REAL NOT NULL DEFAULT 0,
    ref_height_m               REAL NOT NULL DEFAULT 0,
    pressure_gradient_n_per_m3 REAL NOT NULL DEFAULT 0,
    ice_thickness_mm           REAL NOT NULL DEFAULT 0,
    manual_ice_n_per_m         REAL,
    manual_wind_n_per_m        REAL,
    PRIMARY KEY (calc_id, case_id)
);
-- Vilken linberäkning en rad "Linberäkning ..." i Dokumentation visar: högst en per rad.
CREATE TABLE IF NOT EXISTS doc_line_calc (
    doc_key TEXT PRIMARY KEY NOT NULL,
    calc_id INTEGER NOT NULL REFERENCES line_calc(id) ON DELETE CASCADE
);
CREATE INDEX IF NOT EXISTS doc_line_calc_calc ON doc_line_calc(calc_id);
)sql";
```

- **Kolumnerna** motsvarar `LineInput`, `LineConstants`, `LoadCase` och `conductor::Conductor` fält för fält. Förvalen är `LineInput`s och `LineConstants` förval (fas 1, 5.3).
- **`cond_*`:** när `cond_name` inte är NULL har alla `cond_*`-tal utom `cond_breaking_load_kn` ett värde. API:t skriver dem tillsammans. `cond_breaking_load_kn` NULL = brottlast saknas ("-").
- **`case_id`** är 1–15. Varje beräkning har alltid alla 15 raderna. API:t skriver dem i samma transaktion som beräkningen.

### 3.2 `libs/projectdb/project_db.h` och `project_db.cpp`
- `kSchemaVersion = 3` (kommentaren: "antal steg: 1 (v0.1.0) + dev_migration.h").
- `schema()`: `s.steps = {kSchemaV1, dev::kStep2, dev::kStep3};`
- `kExpectedTablesSql` får `'line_calc', 'line_calc_case', 'doc_line_calc'` sist, och `kExpectedTableCount = 9`.
- `PRAGMA foreign_keys = ON` sätts redan av `db::Database::open`, så kaskaden gäller.

### 3.3 Migreringstest från v0.2.0
**`testMigrateFromV020`** i `projectdb_test.cpp` (paket B):
1. **Skapa filen som v0.2.0** med `db::Database` (hjälpfunktionen `writeV020Project(path)`):
   - application_id KLKP och `user_version` 2
   - steg 1:s tabell `project` ordagrant som i `writeV010Project`, och därefter SQL:en i `projectdb::dev::kStep2` (committad, ändras aldrig)
   - data: projektraden "åäö-v020", CURLEW i `project_conductor`, `doc_value` med `phase_conductor` = CURLEW, `tension_phase` = "90" (referens "Ref åäö"), `temp_max` = "50" och `line_calc_phase_before` = "354" (manuellt värde med referensen "Excel"), en normalspannsberäkning med exempel C:s stolpar och egna gränser, kopplad till `ns_calc_phase_before`
2. **Öppna med den nya koden:** `user_version` = 3 och de nio tabellerna finns.
3. **Allt från v0.2.0 finns kvar:** projektraden, linkopian med alla värden, alla `doc_value`-rader med referenser (också det manuella värdet 354 på linberäkningsraden), normalspannsberäkningen med stolpar, gränser och koppling.
4. **De nya delarna:** `lineCalcs(p)` är tom. En linberäkning kan sparas, kopplas till `line_calc_phase_after` och läsas tillbaka.
5. **Publicera, stäng och öppna originalet igen:** `user_version` = 3, och både de gamla uppgifterna och linberäkningen finns.

`testMissingTablesRejected` använder `kSchemaVersion` och fungerar oförändrat: en fil med `user_version` 3 men bara `project` vägras.

---

## 4. API i `project_data` (paket B)

### 4.1 `libs/projectdb/project_data.h` (tillägg, bindande)
Nya includes `<utility>` och `"lineCalc/line_calc.h"`. Efter `removeNormalSpan`, före namnrymdens slut:

```cpp
// Nyckeln för linberäkningsraden i Dokumentation, t.ex. "line_calc_top_left_after".
// Raderna finns redan i v0.2.0 med dessa nycklar och manuella värden (planen fas 2, 8.1).
std::string lineCalcDocKey(Wire wire, Stage stage);

// Lina och skede för en linberäkningsnyckel. Tomt svar för en annan nyckel.
std::optional<std::pair<Wire, Stage>> lineCalcRowOf(std::string_view docKey);

// En linberäkning i projektet. input är indata som de lagras. En kopplad beräkning (docSource
// satt) lagrar ingen lina, inget an och inget sigma0: de och min/max är Dokumentationens och
// sätts in av resolveLineInput. Resultatet räknas med calculateResolved och lagras aldrig.
struct LineCalc {
    std::int64_t id = 0;                     // 0 = ny, inte sparad
    std::string note;                        // anteckning
    std::string docSource;                   // linberäkningsradens nyckel; tom = manuell
    linecalc::LineInput input;
    std::vector<std::string> docKeys;        // raderna som visar den; bara vid läsning
    std::string createdUtc;                  // sätts vid sparning
    std::string modifiedUtc;
};

// Det i Dokumentation som en kopplad linberäkning hämtar: uppgifterna, projektets linor och
// aₙ för varje normalspannsrad med en kopplad beräkning. Läses en gång per omräkning.
struct DocLineSources {
    std::vector<DocValue> values;                  // som docValues
    std::vector<conductor::Conductor> conductors;  // som projectConductors
    // aₙ (avrundat till 0,1 m) per normalspannsnyckel med kopplad beräkning; tomt = fel
    std::vector<std::pair<std::string, std::optional<double>>> normalSpans;
};
core::Result<DocLineSources> docLineSources(ProjectDb& p);

// Fältet i LineInput som ett hämtat värde gäller, och nyckeln i Dokumentation det kom ur.
enum class LineSourceField { Conductor, NormalSpan, Tension, MinTemp, MaxTemp };
struct LineSourceRef { LineSourceField field; std::string docKey; };

// Indata efter Dokumentation (planen fas 2, 4.2). Med tom docSource kommer input tillbaka
// orörd. Annars sätts linan (projektets kopia), an, sigma0, min och max, med
// linecalc::applyInputChange för min och max. Det som saknas ger en Missing-upplysning.
struct ResolvedLineInput {
    linecalc::LineInput input;
    std::vector<linecalc::LineIssue> issues;   // Missing, texterna i planen fas 2, 4.4
    std::vector<LineSourceRef> sources;        // de fält som kom ur Dokumentation
};
ResolvedLineInput resolveLineInput(const linecalc::LineInput& input, std::string_view docSource,
                                   const DocLineSources& docs);

// Räknar en upplöst beräkning: linecalc::calculateLine på input, med upplösningens upplysningar
// först. Finns en Missing ur upplösningen blir det inget resultat, också när calculateLine kunde
// räkna, och calculateLine:s Missing för samma fält tas bort.
linecalc::LineCalculation calculateResolved(const ResolvedLineInput& resolved);

// Alla linberäkningar med lastfall och rader, efter id.
core::Result<std::vector<LineCalc>> lineCalcs(ProjectDb& p);
// En linberäkning. Tomt svar om id saknas.
core::Result<std::optional<LineCalc>> lineCalc(ProjectDb& p, std::int64_t id);
// Ny (id 0) eller ersätter beräkningen och dess 15 lastfall, i EN transaktion, och svarar med id.
// linkDocKey kopplar den till raden (som linkLineCalc). En kopplad beräkning kopplas till
// docSource och bort från alla andra rader. Fel: AccessDenied, NotFound (id saknas), Invalid
// (tal inte ändligt, okänd nyckel, docSource och linkDocKey olika), Sql.
core::Result<std::int64_t> saveLineCalc(ProjectDb& p, const LineCalc& lc,
                                        std::string_view linkDocKey = {});
// Raden docKey visar linberäkningen id, och radens manuella värde töms (referensen står kvar).
// En kopplad beräkning som visades på raden blir manuell (detach). Fel: AccessDenied, NotFound,
// Invalid (inte en linberäkningsnyckel, id är kopplad till en annan rad), Sql.
core::Result<void> linkLineCalc(ProjectDb& p, std::int64_t id, std::string_view docKey);
// Gör en kopplad beräkning manuell: värdena ur Dokumentation skrivs in som egna, och kopplingen
// till raden står kvar. En manuell beräkning lämnas orörd. Fel: AccessDenied, NotFound, Sql.
core::Result<void> detachLineCalc(ProjectDb& p, std::int64_t id);
// Tar bort beräkningen, dess lastfall och alla dess kopplingar. Saknat id är inget fel.
// Fel: AccessDenied, Sql.
core::Result<void> removeLineCalc(ProjectDb& p, std::int64_t id);
```

**`unlinkDocRow`** ändras utan ny signatur. Kommentaren blir: "Raden docKey visar ingen beräkning längre, varken normalspann eller linberäkning. Beräkningen finns kvar. En kopplad linberäkning blir manuell (detach). Fel: AccessDenied, Sql."

### 4.2 Reglerna för upplösningen (`resolveLineInput`)
Wire och Stage ur `lineCalcRowOf(docSource)`. En okänd `docSource` ger en Missing-upplysning (texten i 4.4) och input orörd.

**Topplina höger "Samma":** när `top_conductor_right` är "Samma" löses TopRight upp exakt som TopLeft, för linan, σ₀ och aₙ. Höger-raderna syns då inte i Dokumentation, men en beräkning som redan är kopplad där räknas ändå.

| Fält | Hämtas ur, i den här ordningen | Saknas |
|---|---|---|
| Linan (`conductor`) | linradens namn (`phase_conductor`, `top_conductor_left`, `top_conductor_right`), och projektets kopia med det namnet (`conductor::sameConductorName`) | `conductor` blir tom, Missing (Conductor) |
| aₙ (`normalSpanM`) | 1. den kopplade normalspannsberäkningen på `normalSpanDocKey(wire, stage)`: dess aₙ avrundat till 0,1 m. Har den fel blir aₙ tomt med Missing, utan att gå vidare. 2. normalspannsradens manuella värde i `doc_value`. 3. den gröna raden `normal_span_phase`, `normal_span_top_left` eller `normal_span_top_right` | tomt, Missing (NormalSpan) |
| σ₀ (`tensionMpa`) | `tension_phase`, `tension_top_left` eller `tension_top_right` | tomt, Missing (Tension) |
| Min (`minTempC`) | `temp_min` | talet står kvar, Missing (MinTemp) |
| Max (`maxTempC`) | `temp_max` | talet står kvar, Missing (MaxTemp) |

- **Talen** i `doc_value` är kanonisk text med punkt och läses med `tabletext::parseDecimal`. En text som inte är ett tal räknas som saknad.
- **Min och max:** skiljer sig värdet ur Dokumentation från `input.minTempC` (det senast hämtade) sätts det, och `linecalc::applyInputChange(input, InputChange::MinTemp)` körs. Likadant för max med `MaxTemp`. Saknas värdet står det senast hämtade kvar i `input`, men Missing-upplysningen gör att `calculateResolved` inte ger något resultat (2.3).
- **Ett fält som saknas** ger alltid en upplysning och aldrig ett reservvärde ur beräkningen själv (beslut 12).
- **`sources`** får en post per fält som faktiskt kom ur Dokumentation, med nyckeln det kom ur (för aₙ den normalspannsnyckel eller gröna nyckel som gav värdet). Formuläret låser och färgar de fälten.
- **Funktionen är ren:** ingen fil, ingen I/O, ingen `Result`.

### 4.3 Reglerna för skrivningarna
Som för normalspannet (normalspannsplanen 5.6): varje skrivning kontrollerar först skrivskyddet, är en egen transaktion och anropar `markModified()` efter en lyckad transaktion. SQL skrivs som råa strängliteraler `R"sql( … )sql"`.

- **`saveLineCalc`:**
  1. Varje tal i `input` (också lindata, konstanter, lastfall och manuell last) ska vara ändligt, annars `Invalid` "Ogiltigt tal.".
  2. `docSource` tom eller en linberäkningsnyckel, `linkDocKey` tom eller en linberäkningsnyckel, annars `Invalid` "Raden <nyckel> kan inte visa en linberäkning.".
  3. Är båda satta måste de vara lika, annars `Invalid` "En linberäkning med indata ur Dokumentation hör till en rad.".
  4. INSERT eller UPDATE av `line_calc`, och sedan DELETE och INSERT av de 15 raderna i `line_calc_case`.
  5. Är `docSource` satt: koppla till `docSource` (som `linkLineCalc`) och ta bort beräkningens andra rader i `doc_line_calc`.
  6. Annars, är `linkDocKey` satt: koppla till raden.
- **En kopplad beräkning lagras utan Dokumentationens värden** (beslut 12): med `docSource` satt skriver `saveLineCalc` NULL i `cond_*` (och '' i `cond_type`), `normal_span_m` och `tension_mpa`, vad `input` än innehåller. `min_temp_c` och `max_temp_c` får `input`s tal, som är det senast hämtade värdet. Det används bara för att se när Dokumentation ändrats (4.2) och aldrig som reserv i räkningen. Vid läsning har en kopplad beräkning `conductor`, `normalSpanM` och `tensionMpa` tomma.
- **En manuell beräkning** lagras som `input` står.
- **`linkLineCalc`:**
  - Är `id` kopplad med `docSource` till en annan rad: `Invalid` "En linberäkning med indata ur Dokumentation hör till en rad.".
  - Visade raden en kopplad beräkning med `docSource` = raden, blir den manuell först (detach), i samma transaktion.
  - Sedan kopplingen (`ON CONFLICT(doc_key) DO UPDATE`) och tömningen av radens manuella värde, som `linkInside` för normalspannet.
- **Detach** (inre hjälpfunktion, används av `detachLineCalc`, `linkLineCalc` och `unlinkDocRow`): läs beräkningen och `docLineSources` i samma transaktion, `resolveLineInput`, skriv det upplösta som egna värden och sätt `doc_source` = NULL. Det är beräkningens senaste värden (beslut 14). Det som saknades i Dokumentation blir tomt (lina, aₙ, σ₀) eller behåller det senast hämtade talet (min, max).
- **`removeLineCalc`:** `DELETE FROM line_calc WHERE id = ?1`. Lastfallen och kopplingarna följer med kaskaden.
- **Läsningen:** en okänd text i `mode`, `temp_source` eller `pressure_source`, eller färre än 15 lastfall, ger `Invalid` "Linberäkningen <id> i projektfilen är skadad.". `conductor.id` är 0 och `origin` Own.
- **`NotFound`:** "Linberäkningen finns inte längre i projektet.".
- **Länkning:** `kalkyl_projectdb` länkar `kalkyl_linecalc` PUBLIC (`LineCalc` bär en `LineInput`) och `kalkyl_tabletext` PRIVATE.

### 4.4 Upplysningarna ur upplösningen (texter ordagrant)
Alla har `kind` Missing och `caseId` 0. Varje text säger var värdet fylls i (beslut 12). Radnamnen är radernas texter i Dokumentation och följer läget: med "Samma" heter uppspänningen "Uppspänning topp" och normalspannet "Normalspann topplina".

| Fall | field | message |
|---|---|---|
| Okänd `docSource` | None | "Raden <nyckel> finns inte i Dokumentation." |
| Linraden tom | Conductor | "Fyll i Faslina i fliken Dokumentation." (eller "Topplina vänster", "Topplina höger") |
| Linans namn finns inte i projektet | Conductor | "Linan <namn> finns inte i projektet. Välj linan igen i fliken Dokumentation." |
| Den kopplade normalspannsberäkningen har fel | NormalSpan | "Normalspannsberäkning <lina> <skede> ombyggnation har fel och ger inget aₙ. Rätta den i fliken Normalspann." |
| aₙ saknas helt | NormalSpan | "Fyll i Normalspann <lina> eller Normalspannsberäkning <lina> <skede> ombyggnation i fliken Dokumentation." |
| σ₀ saknas | Tension | "Fyll i Uppspänning fas i fliken Dokumentation." (eller "Uppspänning topp", "Uppspänning topp vänster", "Uppspänning topp höger") |
| Min saknas | MinTemp | "Fyll i Min temp i fliken Dokumentation." |
| Max saknas | MaxTemp | "Fyll i Max temp i fliken Dokumentation." |

I texterna är `<lina>` "faslina", "topplina vänster", "topplina höger" eller, med "Samma", "topplina", och `<skede>` är "före" eller "efter".

`calculateLine` lägger till sina egna Missing ("Uppspänningen saknas." …) när ett värde är tomt. `calculateResolved` sätter upplösningens upplysningar först och tar bort beräkningens Missing för samma fält, så att användaren bara ser texten om Dokumentation.

---

## 5. Biblioteket `libs/sheetGrid` (paket A)

### 5.1 `libs/sheetGrid/sheet_grid.h` (ny, bindande)
```cpp
#pragma once
#include <glaze/glaze.h>

#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace kalkyl::sheetgrid {

// En cell i tabellen och en rektangel av celler, båda ändar inräknade.
struct CellPos { int row = 0; int column = 0; };
struct CellRect { int row0 = 0; int column0 = 0; int row1 = 0; int column1 = 0; };

// En ändring som fönstret ska göra: texten i cellen, med tal i kanonisk form med punkt.
// Tom text = töm cellen.
struct CellEdit { int row = 0; int column = 0; std::string text; };

// Tabellens form för markeringen och inklistringen. Fönstret svarar på frågorna.
// skipRow: tomraden, som aldrig markeras och vars fält ignoreras. isInput: cellen tar emot
// inklistring och Delete. accept: fältet som cellens text, tomt svar = passar inte (text i en
// talcell). columnName: kolumnens namn i meddelandet när en inklistring avbryts.
struct SheetShape {
    int rowCount = 0;
    int columnCount = 0;
    int firstColumn = 0;                        // 1 när kolumn 0 är radrubrik
    std::function<bool(int row)> skipRow;
    std::function<bool(int row, int column)> isInput;
    std::function<std::optional<std::string>(int row, int column, const std::string& field)> accept;
    std::function<std::string(int column)> columnName;
};

// Ett planerat inklistrat block: ändringarna, om en rubrikrad hoppades över, och blockets
// storlek i tabellen. rejected är meddelandet när inklistringen avbryts; då är edits tom.
struct SheetPaste {
    std::vector<CellEdit> edits;
    bool skippedHeader = false;
    CellRect area;
    std::optional<std::string> rejected;
};

// Var ett inklistrat block hamnar (planen fas 2, 5.2). Ren funktion utan fönster.
SheetPaste planSheetPaste(CellPos anchor, const std::vector<std::vector<std::string>>& block,
                          const SheetShape& shape);

// Rektangeln mellan två celler, med rad0 <= rad1 och kolumn0 <= kolumn1.
CellRect rectOf(CellPos a, CellPos b);

// Cellen klampad till tabellen. En tomrad flyttas till nästa rad i riktningen step (+1 eller -1),
// och åt andra hållet när det inte finns någon.
CellPos clampPos(CellPos pos, const SheetShape& shape, int step);

// Sant när urklippet är ett block: flera rader eller flera fält. En enda cell från Excel
// (med avslutande radbrytning) räknas inte.
bool isBlock(const std::string& clip);

// Det SheetGrid frågar fönstret om. Fönstret äger innehållet och räknar om.
class SheetHost {
public:
    virtual ~SheetHost() = default;
    // Texten som kopieras för cellen (som den visas, tal med decimalkomma).
    virtual std::string copyText(int row, int column) const = 0;
    // Cellens färg utan markering.
    virtual std::optional<gfx::Color> baseColor(int row, int column) const = 0;
    // Skriver ändringarna som EN ändring (inklistring, Delete, Ctrl+X) och räknar om.
    virtual void applyEdits(const std::vector<CellEdit>& edits) = 0;
    // Egna poster sist i snabbmenyn, efter en separator. Tomt som förval.
    virtual void addMenuItems(glaze::Menu& menu, const CellRect& selection);
    // En kort text i statusraden ("Kopierade 3 × 2 celler").
    virtual void showStatus(const std::string& text) = 0;
};

// Excel-beteendet i en DataGrid (planen fas 2, 5.2): rektangulär markering med Shift och
// Shift+klick, Ctrl+C, X, V, A, Delete och snabbmenyn. Fönstret kopplar tabellens onKeyDown,
// onMouseUp och onContextMenu hit. Tabellen och värden måste leva längre än objektet.
class SheetGrid {
public:
    SheetGrid(glaze::DataGrid& grid, SheetShape shape, SheetHost& host);
    // Skrivskyddat: bara Ctrl+C, Ctrl+A och markeringen fungerar.
    void setReadOnly(bool readOnly);
    void keyDown(glaze::KeyArgs& e);
    void mouseUp(glaze::MouseArgs& e);
    void contextMenu(glaze::ContextMenuArgs& e);
    // Målar om alla celler eller en cell: markeringens färg i en utökad markering, annars
    // värdens baseColor.
    void paintAll();
    void paintCell(int row, int column);
    // Markeringen och den aktuella cellen.
    CellRect selection() const;
    void select(CellPos anchor, CellPos extent);
    // Kopierar markeringen till urklippet och klistrar in urklippet vid markeringen.
    void copySelection();
    void pasteAtSelection();

private:
    void syncSelection(bool extend);
    void clearSelectionCells();
    void selectAll();

    glaze::DataGrid& grid_;
    SheetShape shape_;
    SheetHost& host_;
    bool readOnly_ = false;
    CellPos anchor_;
    CellPos extent_;
    glaze::Menu menu_;
    std::shared_ptr<bool> alive_ = std::make_shared<bool>(true);   // spärr för postade anrop
};

} // namespace kalkyl::sheetgrid
```

### 5.2 Beteendet (`sheet_grid.cpp`)
Det är normalspannsformulärets beteende (normalspannsplanen 6.3), utan raderna som läggs till och tas bort.

- **Tangenterna** (`keyDown`), med `ctrl` = Ctrl utan Alt och `shift` = Shift:
  - Under redigering tas bara Ctrl+V med ett block (`isBlock`): `e.handled = true`, `cancelEdit()` och inklistring från den redigerade cellen. Annars postas `syncSelection(false)`.
  - Utan redigering: Ctrl+C kopierar, Ctrl+X kopierar och tömmer (inte skrivskyddat), Ctrl+V klistrar in vid markeringens övre vänstra cell (inte skrivskyddat), Ctrl+A markerar alla, Delete tömmer (inte skrivskyddat).
  - Shift+Vänster/Höger/Upp/Ned, Shift+Home/End (kolumnen `firstColumn` eller sista), Ctrl+Shift+Home/End (första eller sista cellen) och Shift+PageUp/PageDown (20 rader) flyttar `extent_` med `clampPos`, som hoppar över tomrader.
  - Övriga tangenter lämnas åt DataGrid, och `syncSelection(false)` postas med `glaze::app().post` och `alive_`.
- **Musen** (`mouseUp`, vänster knapp): `syncSelection(shift)` postas. Med Shift utökas markeringen från ankaret.
- **Snabbmenyn** (`contextMenu`): raden ur `grid_.fromWindow` + `rowAt`. Ligger den utanför markeringen markeras (rad, aktuell kolumn). Posterna är "Klipp ut" (Ctrl+X), "Kopiera" (Ctrl+C), "Klistra in" (Ctrl+V), "Radera innehåll" (Delete), separator och "Markera alla" (Ctrl+A), sedan värdens `addMenuItems`. Skrivskyddat är bara Kopiera och Markera alla aktiva.
- **Kopiera:** rektangeln blir rader av `host_.copyText`. En tomrad blir en tom rad, som arkets rad 30. Texten går till `gfx::setClipboardText(tabletext::formatTsv(rader))`. Statusraden: "Kopierade <rader> × <kolumner> celler".
- **Klistra in:** `tabletext::parseTsv(gfx::clipboardText())`, sedan `planSheetPaste`. Är `rejected` satt visas `glaze::messageBox(Warning)` med texten, och inget ändras. Annars `host_.applyEdits(edits)`, markeringen blir `area` och statusraden "Klistrade in <n> rader".
- **Töm** (Delete, Ctrl+X, menyn): en `CellEdit` med tom text för varje indatacell i rektangeln, i ett anrop till `applyEdits`.
- **Färgerna:** markeringen ritas med `{60, 130, 210, 90}` över grundfärgen, utom på den aktuella cellen, som DataGrid ritar själv. `paintAll` målar alla celler. Markeringsbyten målar bara om den gamla och den nya rektangeln.

**`planSheetPaste`:**
1. Fältet i blockets rad r och kolumn c landar i (anchor.row + r, anchor.column + c). Kolumnerna före `firstColumn` och utanför tabellen ignoreras, liksom rader utanför tabellen. Tabellen växer aldrig.
2. Fält som landar på en tomrad (`skipRow`) ignoreras, men raderna räknas ändå. Ett block ur Excel F23:T38 passar alltså rad för rad med tomraden i arkets rad 30.
3. Fält som landar i en cell som inte är indata (`isInput` falsk) ignoreras.
4. **Rubrikrad:** har blocket mer än en rad, och har första raden minst ett fält som inte är tomt och som `accept` avvisar, hoppas första raden över (`skippedHeader`). Raderna efter den flyttas inte upp: blocket börjar ändå vid ankaret.
5. **Kontroll före ändring:** avvisar `accept` ett fält efter rubrikraden blir `rejected` "Inklistringen avbröts: "<fält>" på rad <n> i urklippet passar inte i kolumnen <columnName>. Inget ändrades." och `edits` tom.
6. Annars blir varje fält en `CellEdit` med texten ur `accept`. Ett tomt fält blir en tom text (töm cellen, som Excel).
7. `area` är rektangeln från ankaret till blockets sista rad och kolumn, klampad till tabellen.

### 5.3 Filer och mål
- `libs/sheetGrid/CMakeLists.txt`: `kalkyl_sheetgrid` (STATIC, `sheet_grid.cpp`, `libs/` PUBLIC, länkar `glaze` PUBLIC och `kalkyl_tabletext` och `kalkyl_warnings` PRIVATE), och testet `kalkyl_sheetgrid_test` (ctest `sheetgrid`).
- `libs/sheetGrid/sheetgrid_test.cpp`: testerna i 10.2. De anropar bara de rena funktionerna och skapar inga fönster.
- `libs/sheetGrid/CLAUDE.md`: biblioteket, filerna och reglerna (fönstret äger innehållet, inga rader läggs till, tomrader hoppas över).
- `libs/CMakeLists.txt`: `add_subdirectory(sheetGrid)` sist, efter `appearance` (GUI-biblioteken sist), och kommentaren om ordningen.

---

## 6. Formuläret `LineCalcDialog` (paket C)

Filerna är `src/windows/line_calc_dialog.{h,glazeui,cpp}` och läggs till i `glaze_add_window` i `src/CMakeLists.txt`. Dialogen är modal och synkron på UI-tråden, som `NormalSpanDialog`. Projektets arbetskopia och systemdatabasen är lokala och behöver ingen `Worker`.

### 6.1 Publikt gränssnitt (`line_calc_dialog.h`, bindande)
Klassen `LineCalcDialog : public glaze::AppWindow` ligger utanför namespace. Regionen ägs av GlazeBuilder. Efter `#pragma endregion`:

```cpp
public:
    // Projektet som formuläret läser och sparar i. Anropas före run(owner); ägs av main.
    void setProject(kalkyl::projectdb::ProjectDb& project);
    // Linregistret för rullistan Lina. Anropas före run(owner); ägs av main.
    void setSystemDb(kalkyl::systemdb::SystemDb& sys);
    // En ny beräkning. linkDocKey = raden i Dokumentation som visar den efter OK, tom = lös.
    // fromDoc = indata ur Dokumentation (kopplad, kräver linkDocKey). En manuell beräkning med
    // linkDocKey börjar med Dokumentationens värden.
    void setNew(std::string linkDocKey, bool fromDoc);
    // En befintlig beräkning, som läses ur projektet i onLoad.
    void setExisting(std::int64_t id);
    // En ny, lös och manuell beräkning med en kopia av beräkningen id (Kopiera...).
    void setCopyOf(std::int64_t id);
    // Radtexterna i Dokumentation per nyckel, för upplysningen om var beräkningen visas.
    void setDocRowLabels(std::vector<std::pair<std::string, std::string>> labels);
    // Skrivskyddat: allt visas, inget kan ändras, OK döljs och Avbryt heter Stäng.
    void setReadOnly(bool readOnly);
    // Efter run(): id för den sparade beräkningen, tomt om inget sparades.
    std::optional<std::int64_t> savedId() const;
```

Dialogen sparar själv med `saveLineCalc` när användaren trycker OK. Huvudfönstret läser om allt efter `run`.

### 6.2 Layout (`.glazeui`, i grammatikens form)
**Fönstret:**
- `setTitle("Linberäkning")`, `setContentSize(1480, 940)`, `setMinimumContentSize(1000, 600)`, `setStartPosition(glaze::StartPosition::CenterOwner)` och `setModal(true)`. Ett för stort fönster kläms in i skärmens arbetsyta vid start.
- `setAcceptButton(okBtn)`, `setCancelButton(cancelBtn)`, `statusBar.show()` och `statusBar.addPanel("", 0, true)`.
- Designraden `// glazeui-design: ui=Slate theme=slate(Windows) style=0 lang=sv`. Ingen `appearance::apply`: dialogen är inget toppfönster.

**Roten**, i adopteringsordning (syskonankare pekar bara bakåt):
1. `cancelBtn` "Avbryt" (`setDialogResult(Cancel)`, bredd 96, ankrad höger 16 och nere 16).
2. `okBtn` "OK" (bredd 96, `anchorRight = {"cancelBtn", 8}`, nere 16, tooltip "Sparar linberäkningen i projektet och stänger"). Inget dialogresultat: OK sparar först (6.10).
3. `noteLbl` "Anteckning" (x 16, y 16) och `noteTxt` (`anchorLeft = {"noteLbl", 8}`, y 12, ankrad höger 16).
4. `docLbl` (`Role::Dim`, x 16, `anchorTop = {"noteTxt", 8}`, ankrad höger 16): var beräkningen visas (6.6).
5. `sheetPanel` (Panel, `setScroll(true)`, ankrad i alla kanter, x 0, `anchorTop = {"docLbl", 8}`, höger 0, `anchorBottom = {"cancelBtn", 12}`): arket. Barnen nedan står i panelens koordinater.

**I `sheetPanel`**, i adopteringsordning:
1. **Lina** (arkets F2 och J2:O2):
   - `conductorLbl` "Lina" (x 16, y 8)
   - `conductorDd` (Dropdown, `anchorLeft = {"conductorLbl", 8}`, y 4, bredd 480). Posterna sätts i kod (6.8).
   - `conductorInfo` (`Role::Dim`, `anchorLeft = {"conductorDd", 12}`, y 8): linans ursprung (6.8).
2. **`inputGrid`** (DataGrid, arkets Indata, x 16, `anchorTop = {"conductorDd", 8}`, bredd 844):
   - `setSortable(false)`, `setEditOnClick(false)` och fyra kolumner: `{.text = "Indata", .width = 330}`, `{.text = "", .width = 90, .align = Right}`, `{.text = "", .width = 330}`, `{.text = "", .width = 90, .align = Right}`. Höjden kommer ur raderna.
   - Raderna (etikett, värde, etikett, värde) sätts i `onLoad` (6.3).
3. **`constGrid`** (DataGrid, arkets Lindata / Konstanter, x 16, `anchorTop = {"inputGrid", 12}`, bredd 844): som `inputGrid`, med rubriken "Lindata / Konstanter" i kolumn 0.
4. **Knapparna till höger** (arkets ruta vid U10):
   - `crossingGrp` (GroupBox "Läs in korsningslastfall enligt Table 5.9.6/SE.1 (50341-2-18)", `anchorLeft = {"inputGrid", 32}`, `anchorTop = {"conductorDd", 8}`, bredd 420) med `upperBtn` "Övre linan" (x 0, y 0, bredd 160) och `lowerBtn` "Nedre linan" (`anchorLeft = {"upperBtn", 16}`, y 0, bredd 160).
   - `resetBtn` "Återställ indata enl. SS-EN 50341-2-18" (`anchorLeft = {"inputGrid", 32}`, `anchorTop = {"crossingGrp", 12}`).
5. **Läget:** `modeGrp` (GroupBox "Läge", `anchorLeft = {"inputGrid", 32}`, `anchorTop = {"resetBtn", 16}`, bredd 420) med:
   - `modeNewRb` (Radio, gruppen "mode", "Ny (felen i arket rättade)", vald)
   - `modeExcelRb` (Radio, gruppen "mode", "Excel (räknar exakt som arket)", `anchorTop = {"modeNewRb", 4}`)
   - `diffChk` (Checkbox "Markera skillnaden mot läget Excel", `anchorTop = {"modeExcelRb", 8}`)
6. **`docInputChk`** (Checkbox "Hämta lina, aₙ, σ₀ och temperaturer ur Dokumentation", `anchorLeft = {"inputGrid", 32}`, `anchorTop = {"modeGrp", 12}`). Dold som förval (6.6).
7. **`infoLbl`** (`Role::Dim`, `setWrap(true)`, `anchorLeft = {"inputGrid", 32}`, `anchorTop = {"docInputChk", 12}`, bredd 420): "Allt räknas om när ett värde ändras. Klistra in från Excel med Ctrl+V i den cell där inklistringen ska börja. Resultatkolumnerna går inte att ändra, utom islast och vindlast: ett tal där gör lastfallet manuellt."
8. **`caseGrid`** (DataGrid, arkets Lastfall, Resultat och Data till SAG (3D), x 16, `anchorTop = {"constGrid", 16}`, ankrad vänster, uppe och höger, höger 16):
   - `setRowHeaders(true)`, `setHeaderLines(7)`, `setHeaderVAlign(glaze::VAlign::Bottom)`, `setSortable(false)` och `setEditOnClick(false)`. Höjden kommer ur raderna.
   - Kolumnerna, redigerarna och kolumntooltipsen enligt 6.3.

**Kopplingarna** i fullform:
- `okBtn.onClick`, `upperBtn.onClick`, `lowerBtn.onClick`, `resetBtn.onClick`
- `noteTxt.onChange`, `conductorDd.onChange`, `modeNewRb.onChange`, `modeExcelRb.onChange`, `diffChk.onChange`, `docInputChk.onChange`
- för var och en av `inputGrid`, `constGrid` och `caseGrid`: `onCommit`, `onKeyDown`, `onMouseUp` och `onContextMenu`
- `glaze::AppWindow::onLoad`, `glaze::AppWindow::onClose`

**Kontroll:** glaze-render efter varje handredigering ska ge exit 0, och bilden granskas med Read.

### 6.3 Tabellerna rad för rad och kolumn för kolumn
**`inputGrid`** (arkets F3:O7). Etiketterna är skrivskyddade (`Editor::None`). Värdena är talfält (`Editor::Number`, `allowEmpty = true`). Tomt aₙ och σ₀ visas tomma.

| Rad | Etikett (kolumn 0) | Värde (1) | Etikett (2) | Värde (3) |
|---|---|---|---|---|
| 0 | Normalspann, aₙ (m) | `normalSpanM` | Linhöjd, snitt (m) | `lineHeightM` |
| 1 | Uppspänning, σ₀ (MPa) | `tensionMpa` | Karakteristiskt vindtryck Normal Vind (N/m²) | `normalWindNPerM2` |
| 2 | Referenstemperatur, T₀ (°C) | `refTempC` | Karakteristiskt vindtryck Hög Vind (N/m²) | `highWindNPerM2` |
| 3 | Minimumtemperatur (°C) | `minTempC` | Kortslutningstemperatur, Tks (°C) | `shortCircuitTempC` |
| 4 | Maximumtemperatur (°C) | `maxTempC` | (tom) | (tom) |

**`constGrid`** (arkets F9:O18):

| Rad | Etikett (0) | Värde (1) | Etikett (2) | Värde (3) |
|---|---|---|---|---|
| 0 | Tvärsnittsarea, A (mm²) | `areaMm2` | Isdensitet, ρ (kg/m³) | `iceDensityKgM3` |
| 1 | Diameter, d (mm) | `diameterMm` | Pi | `pi` |
| 2 | Initiell E-modul, Eᵢₖ (MPa) | `eInitialMpa` | Tyngdaccelerationen (m/s²) | `gravity` |
| 3 | Permanent E-modul, Eₚ (MPa) | `ePermanentMpa` | Gust response factor för linan, Gq | `gustFactor` |
| 4 | Högsta spänningen då Eᵢₖ är giltig, σₚ (MPa) | `eInitialLimitMpa` | Structural resonance factor för linan, Gc | `resonanceFactor` |
| 5 | Konstant för beräkning av Eᵢᵥ, k | `eivConstant` | Drag factor för linan, Cc | `dragFactor` |
| 6 | Utvidgningskoefficient, α (10⁻⁶/°C) | `alphaE6PerC` | Minimum islast vid vindstilla, I (N/m) | `minIceNPerM` |
| 7 | Permanent förlängning pga krypning, Tₚ (°C) | `creepTempC` | Maximal tillåten utnyttjandegrad av brottlast (%) | `maxUtilizationPercent` |
| 8 | Egentyngd, qₑ (N/m) | `weightNPerM` | Lastfall för transponering till Ep-stadiet (nr) | `transpositionCase` |
| 9 | Brottlast, fᵤ (kN) | `breakingLoadKn` | (tom) | (tom) |

- Kolumn 1 är lindatat (linans fält, `Conductor`), kolumn 3 konstanterna (`LineConstants`).
- Rad 7:s etikett har arkets hela text som tooltip: "Permanent förlängning pga krypning uttryckt i temperaturhöjning, Tₚ (°C)".
- Brottlasten har `emptyText = "-"`: tom betyder att brottlast saknas, som arkets "-".
- Transponeringens lastfall har `decimals = 0`.

**`caseGrid`** (arkets F22:AE38): 16 rader. Rad 0–6 är lastfall 1–7, rad 7 är en tomrad (`RowKind::Blank`, arkets rad 30) och rad 8–15 är lastfall 8–15. Rubrikerna står nederst i ett huvud med 7 rader. I de tre gruppkolumnerna (0, 15 och 23) börjar rubriken med gruppens namn och fylls ut med tomma rader till 7, så att namnet står överst som i arket. **En rubrik får inte börja med en tom rad**: två radbrytningar direkt efter citattecknet stoppas av hooken. Raderna i rubrikerna skiljs nedan med " · ".

| Kol | Arket | Rubrik (rader) | Redigerare | Format | Bredd |
|---|---|---|---|---|---|
| 0 | F | Lastfall · 5 tomma · ID | radrubrik | heltal | AutoMin |
| 1 | G | Temp · T · (°C) | Number | kortaste form | AutoMin |
| 2 | H | Lastfall benämning | Text | text | 170 |
| 3 | I | Stadium | None | Ei eller Ep | AutoMin |
| 4 | J | SS-EN 50341 · Standard · load cases | Text | text | AutoMin |
| 5 | K | SS-EN 50341 · Load cases · for calculation · of clearances | Text | text | AutoMin |
| 6 | L | Vid is och · vindstilla · använd vind- · parametrar från · lastfall-ID | Number, `decimals = 0`, tomt tillåtet | heltal | AutoMin |
| 7 | M | Höjd · h · (m) | None | kortaste form | AutoMin |
| 8 | N | Magnitud · vindlast · (%) | Number | kortaste form | AutoMin |
| 9 | O | Karakter- · istiska · vindtrycket · qₕ · (N/m²) | Number | kortaste form | AutoMin |
| 10 | P | Referens- · höjd · href · (m) | Number | kortaste form | AutoMin |
| 11 | Q | dqₕ · (N/m³) | Number | kortaste form | AutoMin |
| 12 | R | Vindtryck · qₕ · (N/m²) | None | 0 decimaler | AutoMin |
| 13 | S | Istjocklek · vid · normal · vind · (mm) | Number | 1 decimal | AutoMin |
| 14 | T | Motsvar- · ande · istjocklek · vid · vindstilla · (mm) | None | 1 decimal | AutoMin |
| 15 | U | Resultat · 3 tomma · Islast · I · (N/m) | Number (manuell last) | 2 decimaler | AutoMin |
| 16 | V | Vindlast · Qwc · (N/m) | Number (manuell last) | 2 decimaler | AutoMin |
| 17 | W | Nedhäng · vertikal · projektion · b · (m) | None | 2 decimaler | AutoMin |
| 18 | X | Spänning · σ · (MPa) | None | 2 decimaler | AutoMin |
| 19 | Y | Linkraft · F · (N) | None | 0 decimaler | AutoMin |
| 20 | Z | Mall · (nr) | None | 0 decimaler | AutoMin |
| 21 | AA | Utnyttjan- · degrad · av · brottlast · (%) | None | 0 decimaler eller "-" | AutoMin |
| 22 | AB | (tom) | None | tom | 12 |
| 23 | AC | Data till SAG (3D) · 2 tomma · Nedhäng · b · (m) | None | 2 decimaler | AutoMin |
| 24 | AD | Utsvängnings- · vinkel · α · (°) | None | 2 decimaler | AutoMin |
| 25 | AE | Mall · (nr) | None | 0 decimaler, avrundat till jämnt | AutoMin |

- **Kolumntooltips** med arkets hela rubrik, till exempel "Vid is och vindstilla används vindparametrarna från lastfall-ID" (L), och för U och V: "Ett tal här gör lastfallet manuellt (arkets kolumn B). Högerklicka och välj Räkna last ur vind och is för att ta bort det."
- **Talfälten** i alla tre tabellerna har varken `min`, `max` eller `decimals`, utom L och transponeringens lastfall (`decimals = 0`). Ogiltiga värden visas som fel ur `calculateLine` i stället för att klampas tyst.

### 6.4 Visning och format
- **Modellen är sanningen.** Formuläret håller beräkningen i `lc_.input` med fullt antal decimaler. Cellerna visar talen med arkets format. Ett värde som visas avrundat har det fullständiga talet som tooltip ("Värde: 23,391812865497073").
- **Talfälten** får texten `formatDecimal(v, d, '.')` (kanonisk form, DataGrid visar decimalkomma). **Skrivskyddade celler** får `formatDecimal(v, d, ',')`. `d` är formatets decimaler, −1 för kortaste form. En cell som inte redigeras skriver aldrig tillbaka sitt avrundade värde: DataGrid fyrar `onCommit` bara när texten ändrats.
- **AE** avrundas till jämnt som VBA:s `Round` (`std::nearbyint` i standardläget). Övriga kolumner med 0 decimaler avrundar som Excels visning.
- **G och O** visar talet som räkningen använder: `caseTempC` och `caseCharPressureNPerM2` i beräkningens läge. I läget Ny har ett lastfall med en källa tooltipen "Följer minimumtemperaturen (J6)", "Följer minimumtemperaturen + 20", "Följer maximumtemperaturen (J7)", "Följer kortslutningstemperaturen", "Följer minimumtemperaturen + (kortslutningstemperaturen − maximumtemperaturen)", "Följer Normal vind (O4)" eller "Följer Hög vind (O5)".
- **Utan resultat** (Missing eller Error) är M, R, T och W–AE tomma. U och V visar då bara manuell last.
- **AA** visar "-" när brottlast saknas (`utilizationPercent` tom).
- **Lastfall med manuell last** (granskningens notis):
  - J, K, M, N, O, P, Q, R, S och T visas **tomma** och är låsta (`Editor::None`), som när Excel tömmer dem.
  - Tooltipen i de cellerna: "Lastfallet har manuell last. Högerklicka och välj Räkna last ur vind och is för att räkna med vind och is igen."
  - Värdena finns kvar i modellen (fas 1, 4.1 punkt 16) och syns igen när den manuella lasten tas bort.
  - U och V visar de manuella talen med färgen för manuell last. L går att ändra som i arket.
  - Kopiering ger tom text i de tomma cellerna, och inklistring hoppar över dem.
- **Lastfall utan benämning** (H tom) räknas och visas dämpade: textfärgen `{128, 128, 128, 255}` i hela raden.
- **Stadiet** (I) är "Ei" för lastfall 1–7 och "Ep" för 8–15.
- **Tomma talceller** i lastfallen och i de icke-valfria indata räknas som 0 och visas som 0, som VBA läser en tom cell. Bara aₙ, σ₀, lindatat (brottlasten "-") och L kan vara tomma.

### 6.5 Färgerna
Halvgenomskinliga, så att de fungerar i ljust och mörkt läge. En cell får det första märket som gäller, i tabellens ordning. Markeringen (`{60, 130, 210, 90}`, ritad av `SheetGrid`) går före allt.

| Märke | Bakgrund | När |
|---|---|---|
| Fel | `{220, 60, 60, 80}` | en Error-upplysning på cellen, eller ett ogiltigt tal |
| Saknas | `{220, 160, 40, 72}` | aₙ, σ₀ eller en lindatacell som behövs är tom, eller en Missing-upplysning ur Dokumentation |
| Över gränsen | `{200, 0, 0, 90}` | AA när `overUtilization` (arkets röda utnyttjandegrad) |
| Skillnad | `{40, 170, 170, 72}` | cellen visar ett annat värde i det andra läget (6.12) |
| Manuell last | `{150, 100, 210, 64}` | U och V i ett lastfall med manuell last |
| Ur Dokumentation | `{70, 170, 90, 56}` | låsta celler i en kopplad beräkning (Dokumentationens gröna ton) |
| Ändrat lindata | `{200, 120, 40, 56}` | en lindatacell som skiljer sig från den valda linan i linregistret (6.8) |
| Etikett | `{128, 128, 128, 40}` | etikettcellerna i `inputGrid` och `constGrid` (arkets grå) |
| Jämnt lastfall | `{245, 160, 90, 64}` | lastfall 2, 4, 6, 8, 10, 12 och 14 (arkets ljusorange 252, 213, 180) |

Cellen med märket Fel, Saknas, Över gränsen eller Skillnad får också en tooltip med upplysningens text. En varning utan egen färg visas som tooltip på sin cell och i statusraden.

**Upplysningarnas celler** (`LineIssue::field` och `caseId`):
- Indata och konstanter: varje `LineField` har sin cell i tabellerna ovan (MinTemp är `inputGrid` rad 3 kolumn 1, TranspositionCase är `constGrid` rad 8 kolumn 3 och så vidare).
- Conductor: Missing markerar de tomma lindatacellerna. Error ("Lindata: …") markerar cellerna som `conductor::validateConductor` på lindatat pekar ut (`ConductorField` till rad i `constGrid`). Tooltipen på `conductorDd` får texten.
- Lastfallens fält: CaseTemp G, CaseWindFrom L, CaseMagnitude N, CasePressure O, CaseRefHeight P, CaseGradient Q, CaseIce S, CaseManualIce U, CaseManualWind V, och Result X i lastfallets rad.
- `caseId` 0 med Result, och None: bara statusraden.

### 6.6 Start, händelser och omräkning
**Medlemmar efter regionen** (förutom 6.1):
- `project_`, `sys_`, `existingId_`, `copyOfId_`, `linkDocKey_`, `newFromDoc_`, `readOnly_`, `docLabels_`
- `lc_` (`projectdb::LineCalc`: beräkningen som redigeras), `docs_` (`DocLineSources`), `sources_` och `docIssues_` (ur senaste upplösningen)
- lindatat som celler: `std::array<std::optional<double>, 10> lindata_`, `conductorName_`, `conductorType_` och `creepStrain_` (ett ofullständigt lindata går inte att lagra i `std::optional<Conductor>`)
- `conductorChoices_` (namn, lina och ursprung per post i `conductorDd`)
- `calc_` (senaste `LineCalculation`) och `other_` (det andra läget, bara med `diffChk`)
- `dirty_`, `saved_`, `savedId_`
- tre värdar (`SheetHost`-adaptrar, en per tabell, som en nästlad klass `GridHost` med en referens till dialogen och tabellens nummer) och tre `std::unique_ptr<kalkyl::sheetgrid::SheetGrid>`
- `alive_` (`std::shared_ptr<bool>`, spärr för postade anrop)

**`onLoad`:**
1. **Rullistan Lina** (6.8).
2. **`docs_ = docLineSources(*project_)`.** Vid fel visas "Dokumentationens uppgifter kunde inte läsas." och felet, och formuläret fortsätter utan dem.
3. **Beräkningen:**
   - **Befintlig** (`setExisting`) eller **kopia** (`setCopyOf`): `lineCalc(*project_, id)`. Saknas den visas "Linberäkningen finns inte längre i projektet.", och dialogen stängs med Cancel.
   - **Kopia:** id 0, inga `docKeys`, och en kopplad beräkning blir manuell med sina upplösta värden (`resolveLineInput`). Anteckningen blir "Kopia av <anteckning>", annars "Kopia av <linans namn>", annars "Kopia".
   - **Ny:** `linecalc::newLineInput()`.
   - **Ny kopplad** (`setNew(key, true)`): `docSource` = key.
   - **Ny manuell med rad** (`setNew(key, false)`): `input = resolveLineInput(newLineInput(), key, docs_).input`, och `docSource` förblir tom. `docLbl` får meningen " Indata är kopierade ur Dokumentation och följer inte ändringar där. Allt går att ändra." (beslut 13). Det som saknas i Dokumentation blir tomt, och min och max får `newLineInput`s −50 och +50, som en ny beräkning (beslut 4).
4. **En kopplad beräkning** löses upp: `lc_.input = resolveLineInput(lc_.input, lc_.docSource, docs_).input`, och `sources_` och `docIssues_` sparas. Formuläret arbetar sedan med de upplösta värdena.
5. **Lindatat** i cellerna fylls ur `lc_.input.conductor`.
6. **Läget:** `modeNewRb` eller `modeExcelRb` väljs efter `input.mode`.
7. **`docInputChk`** syns när beräkningen är kopplad, när den är kopplad till exakt en linberäkningsrad, eller när den är ny med en rad. Den är ikryssad när `docSource` är satt.
8. **`docLbl`:**
   - Befintlig: "Visas i Dokumentation på: <radtexter>", eller "Visas inte i Dokumentation. Koppla den med högerklick på en rad Linberäkning … i fliken Dokumentation."
   - Ny med rad: "Visas på raden <radtext> när du trycker OK."
   - Ny utan rad och kopia: "Blir en lös beräkning i fliken Linberäkning."
   - En kopplad beräkning får dessutom meningen " Linan, aₙ, σ₀ och temperaturerna är Dokumentationens och ändras där." (beslut 12).
9. **Tabellerna** byggs med `setRows` (5, 10 och 16 rader, rad 7 i `caseGrid` som tomrad), cellernas redigerare och etiketterna. Därefter skapas de tre `SheetGrid`.
10. **Skrivskydd** (6.9), sedan `recalc()`, och den första värdecellen i `inputGrid` blir aktuell.

**`recalc()`**, efter varje ändring:
1. **Lindatat:** alla nio tal och namnet ger en `Conductor` (brottlasten kan vara tom). Saknas något av de nio talen är lindatat ofullständigt, `conductor` blir tom och de tomma cellerna får märket Saknas. Är alla tomma finns inget lindata.
2. **En kopplad beräkning löses upp igen**, så att Återställ och mallarna inte kan skriva över värdena ur Dokumentation (Återställ sätter min −40 och max 60, men Dokumentationens temperaturer gäller).
3. `calc_ = calculateResolved(upplöst)`, och `other_` på samma sätt med det andra läget när `diffChk` är ikryssad. För en manuell beräkning är upplösningen tom, och `calculateResolved` blir `calculateLine`.
4. **Cellerna** skrivs med `setCell`: indata ur modellen och resultaten ur `calc_` (6.4).
5. **Märken, tooltips och redigerare** (låsta celler i en kopplad beräkning och i manuella lastfall), sedan `paintAll()` i de tre `SheetGrid`.
6. **Statusraden** (6.11).

**Användarens ändringar.** Alla sätter `dirty_ = true` och kör `recalc()`:
- **En cell** (`inputGridCommitted`, `constGridCommitted`, `caseGridCommitted`): värdet läses med `numberValue` eller `cell` och skrivs i modellen. En cell med `hasError` ändrar inte modellen. Dessutom:
  - min, max, Tks, O4 och O5: `linecalc::applyInputChange` med rätt `InputChange` (fas 1, 2.4 och 5.4)
  - G i ett lastfall: källan blir Fixed och talet det skrivna (fas 1, 9.2). O likadant med vindtryckets källa
  - U eller V: lastfallet blir manuellt med det skrivna talet och det andra fältets visade värde (0 utan resultat), som arkets kolumn B = 1
  - en lindatacell: lindatat ändras, namnet står kvar (6.8)
- **Inklistring och Delete** (`applyEdits` från `SheetGrid`): samma skrivningar för varje cell, sedan `applyInputChange` en gång för varje ändrat av min, max, Tks, O4 och O5, i den ordningen, och en enda `recalc()`.
- **Rullistan Lina** (`conductorChosen`, 6.8).
- **Övre linan:** `glaze::inputBox(*this, "Lastfall III (lokal islast) måste beräknas separat. Ange kortslutningstemperaturen för linan (°C).", "Övre linan", <nuvarande Tks med decimalkomma>)`.
  - Avbryt: inget görs (fas 1, 4.1 punkt 17).
  - En text som inte är ett tal: "Kortslutningstemperaturen måste vara ett tal." och inget görs.
  - Annars `loadCrossingUpper(input, tks)`.
- **Nedre linan:** `loadCrossingLower(input)`. **Återställ:** `resetToStandard(input)`. Ingen fråga, som i arket. Avbryt i formuläret ångrar.
- **Läget** (`modeChanged`, bara från radion som blev vald): `input.mode` byts, och `diffChk` får texten "Markera skillnaden mot läget Excel" eller "… läget Ny".
- **`docInputChk`:**
  - På: `docSource` = beräkningens enda rad (eller den nya radens nyckel), upplösning och låsta fält.
  - Av: `docSource` töms, och värdena står kvar som egna och går att ändra.
  - `docLbl` skrivs om i båda fallen, så att meningen om Dokumentation följer rutan.
- **Anteckningen:** bara `dirty_ = true`.
- **`diffChk`:** bara `recalc()`. Den ändrar inte beräkningen.
- **Snabbmenyn i `caseGrid`** får posten "Räkna last ur vind och is" efter `SheetGrid`s poster (värdens `addMenuItems`). Den är aktiv när något lastfall i markeringen har manuell last och tar bort den manuella lasten på dem (fas 1, 4.1 punkt 16).

**Låsta celler i en kopplad beräkning** (beslut 12):
- `conductorDd` är inaktiv och visar linans namn. Lindatacellerna, aₙ, σ₀, min och max är alltid låsta (`readOnly`).
- **Ett hämtat värde** har märket Ur Dokumentation och tooltipen "Hämtas ur Dokumentation: <radens text>. Ändra det där." (för aₙ den rad som gav värdet, normalspannsberäkningen eller den gröna raden, ur `sources_`).
- **Ett värde som saknas i Dokumentation** visas tomt, också min och max, med märket Saknas och upplysningens text ur 4.4 som tooltip ("Fyll i Max temp i fliken Dokumentation."). Beräkningen har då inget resultat.
- **Det som går att ändra** är T₀, linhöjden, vindtrycken, Tks, konstanterna, lastfallen och läget, som i en manuell beräkning.
- **Knapparna** Återställ, Övre linan och Nedre linan fungerar, men de låsta fälten behåller Dokumentationens värden (recalc, steg 2).

### 6.7 Urklipp och markering
Varje tabell har en `SheetGrid` (avsnitt 5), och tabellens fyra händelser går direkt till den. Beteendet är normalspannsformulärets: rektangulär markering med Shift+piltangenter och Shift+klick, Ctrl+C, Ctrl+X, Ctrl+V, Ctrl+A, Delete och snabbmenyn.

| | `inputGrid` | `constGrid` | `caseGrid` |
|---|---|---|---|
| Rader, kolumner | 5, 4 | 10, 4 | 16, 26 |
| `firstColumn` | 0 | 0 | 1 (ID är radrubrik) |
| Tomrad | ingen | ingen | rad 7 |
| Indata (`isInput`) | kolumn 1 rad 0–4 och kolumn 3 rad 0–3 | kolumn 1 rad 0–9 och kolumn 3 rad 0–8 | G, H, J, K, L, N, O, P, Q, S, U och V |
| Inte indata | etiketterna, låsta celler i en kopplad beräkning, skrivskyddat | som `inputGrid` | resultatkolumnerna, AB, låsta celler i manuella lastfall, skrivskyddat |

- **`accept`:** talceller tar `tabletext::parseDecimal`, med texten `formatDecimal(v, -1, '.')`. Ett tomt fält töms. L tar bara heltal 1–15 eller tomt. Textkolumnerna (H, J, K) tar all text, trimmad.
- **`copyText`:** som cellen visas. Talfält kopieras med fullt antal decimaler och decimalkomma (`formatDecimal(v, -1, ',')`), som i normalspannsformuläret. Tomma celler, låsta manuella celler och tomraden ger tom text.
- **Ett block ur Excel** passar rad för rad: G23:S38 från arket klistras in i G på lastfall 1 och hamnar på rätt lastfall, eftersom tomraden ligger där arkets rad 30 ligger. J9:J18 från arket hamnar i lindatat när det klistras in i A.
- **Kolumnnamnet** i meddelandet är rubrikens rader med mellanslag, till exempel "Temp T (°C)", eller etiketten i `inputGrid` och `constGrid`.

### 6.8 Linan och lindatat
- **Rullistans poster:** först "(utan namn)", sedan linregistrets linor (`sys_->conductors()`), projektets linor som saknas i registret (`projectConductors`) och, om beräkningens lina finns i ingetdera, dess namn. Namnen står fallande med `conductor::compareNames`, som i Dokumentation.
- **Att välja en lina** fyller lindatat med projektets kopia om den finns, annars registrets lina, annars beräkningens egen kopia. Det är samma regel som linraderna i Dokumentation (normalspannsplanen 3.9), så samma namn ger samma data i projektet. Projektets kopior ändras aldrig av formuläret.
- **"(utan namn)"** tar bort namnet och behåller lindatat (lindata som skrivits för hand).
- **Lindatat kan ändras för hand** efter valet (beslut 2). En cell som skiljer sig från samma lina i linregistret får märket Ändrat lindata och tooltipen "Linregistret: <värde>". Finns linan inte i registret jämförs den med projektets kopia.
- **`conductorInfo`:**
  - kopplad: "Ur Dokumentation: <radens text>"
  - lindatat skiljer sig: "Ändrat för hand. Linregistret har andra värden."
  - linan finns bara i projektet: "Projektets kopia. Linan finns inte i ditt linregister."
  - linan finns bara i beräkningen: "Beräkningens egen kopia. Linan finns inte i linregistret eller i projektet."
  - lindata utan namn: "Lindata utan lina"
  - annars tom
- **Typen** och εc (`creepStrainPermille`) följer med den valda linan och visas inte. Med lindata utan lina är typen tom och εc 0.

### 6.9 Skrivskydd
`setReadOnly(true)`:
- alla tabellers redigerare låsta (`readOnly = true`) och de tre `SheetGrid` skrivskyddade (Ctrl+C, Ctrl+A och markeringen fungerar)
- `conductorDd`, `noteTxt`, radioknapparna, `docInputChk`, `upperBtn`, `lowerBtn` och `resetBtn` inaktiva
- `diffChk` går att använda, eftersom den bara visar
- `okBtn.hide()`, och `cancelBtn` heter "Stäng"

### 6.10 Sparning och osparade ändringar
**OK (`okBtnClicked`) och `saveChanges()`:**
1. `commitEdits()`.
2. En cell med ogiltigt tal: `messageBox(Warning)` "Linberäkningen kan inte sparas: <cellens namn> innehåller inget tal.", och cellen blir aktuell. Stanna.
3. **Ofullständigt lindata:** frågan "Lindatat är ofullständigt (<n> värden saknas) och sparas inte. Vill du spara beräkningen ändå?" (Ja/Nej). Nej stannar.
4. **Fel i beräkningen** (Error): frågan "Linberäkningen har fel: <första felet> (och <n> fel till). Vill du spara den ändå?" (Ja/Nej, beslut 16). Nej stannar, och felets cell blir aktuell. Missing sparas utan fråga.
5. Bygg `LineCalc`: id (0 för ny och kopia), anteckningen trimmad, `docSource` och `input` med lindatat ur cellerna. För en kopplad beräkning lagrar `saveLineCalc` inte Dokumentationens värden (4.3).
6. `saveLineCalc(*project_, lc, linkDocKey_)`. Vid fel visas `messageBox(Error, message)`, och dialogen står kvar. Annars `saved_ = true`, `savedId_` och `closeWithResult(OK)`.

**Krysset, Avbryt och Escape (`onClosing`)**, som i normalspannsformuläret:
- Ingenting görs om `saved_`, `readOnly_` eller `e.reason` inte är `UserClosing`.
- Annars `commitEdits()`. Är `dirty_` falsk stängs dialogen.
- Annars frågan "Du har ändrat linberäkningen. Vill du spara ändringarna?" (`YesNoCancel`, `Warning`). Ja kör `saveChanges()` och stannar om den misslyckas, Nej stänger och Avbryt stannar (`e.cancel = true`).

### 6.11 Statusraden
- **Texten:** det första felet (Error), annars den första Missing (Dokumentationens först, 4.4), annars den första varningen. Ett lastfall skrivs först: "Lastfall 4: Eiv konvergerar inte.". Finns fler upplysningar läggs " (och <n> till)" till.
- **Utan upplysningar:** "Stakningsmall <Z i lastfall 15, heltal> · Högsta utnyttjandegrad <x> % (lastfall <a, b>) · Läge Ny". Med `diffChk` läggs " · <n> celler skiljer sig mot läget Excel" till.
- **Tooltipen** på statusraden listar alla upplysningar, en per rad.

### 6.12 Skillnaden mellan lägena
- `diffChk` räknar det andra läget (`other_`) med samma indata och bara läget bytt.
- **Cellerna som jämförs** är G och O (som räknas olika i lägena, D7–D9) och resultatkolumnerna M, R, T, U, V, W–AA och AC–AE. Jämförelsen gäller den **visade** texten, så skillnader i femtonde decimalen syns inte.
- **En cell som skiljer sig** får märket Skillnad och tooltipen "Ny: 60,87 · Excel: 60,58".
- **Har bara det ena läget ett resultat** säger statusraden till exempel "Läget Excel ger ingen beräkning: <upplysningen>.".
- **Excel-lägets varningar** (D1–D5) syns i statusradens tooltip under rubriken "Läget Excel:" när Ny är valt.

---

## 7. Fliken Linberäkning i huvudfönstret (paket D, fas 2)

### 7.1 `main_window.glazeui`
- **Fliken `lcSida`** "Linberäkning" adopteras sist i `flikar`, efter `nsSida`. Den har panelen `lcPanel` (`Dock::Fill`) med samma uppbyggnad som `nsPanel`:
  - `lcRubrik` "Linberäkningar" (`Role::Heading`, x 16, y 8)
  - `lcNyBtn` "Ny beräkning…" (`StockIcon::Add`), `lcAndraBtn` "Ändra…" (`StockIcon::Edit`), `lcKopieraBtn` "Kopiera…" (`StockIcon::Duplicate`) och `lcTaBortBtn` "Ta bort" (`StockIcon::Remove`) i en rad under rubriken, 8 px isär
  - `lcLista` (ListView, ankrad i alla fyra kanter under knapparna, 16 px marginal, `setHeaderLines(2)`)
- **Kopplingarna:** `lcNyBtn.onClick`, `lcAndraBtn.onClick`, `lcKopieraBtn.onClick`, `lcTaBortBtn.onClick`, `lcLista.onActivate`, `lcLista.onChange` och `lcLista.onContextMenu`.

### 7.2 Kolumnerna i `lcLista`
| Kol | Rubrik (rad 1 · rad 2) | Innehåll | Bredd |
|---|---|---|---|
| 0 | Lina | linans namn, "(utan namn)" för lindata utan lina, "–" utan lindata | 260 |
| 1 | Indata | "Dokumentation" (kopplad) eller "Manuell" | 100 |
| 2 | Läge | "Ny" eller "Excel" | 60 |
| 3 | aₙ · m | en decimal, "–" när tomt | 70, höger |
| 4 | σ₀ · MPa | kortaste form, "–" när tomt | 70, höger |
| 5 | T₀ · °C | kortaste form | 60, höger |
| 6 | Min / max · °C | "−50 / 50" | 90, höger |
| 7 | Mall · lastfall 15 | stakningsmallen som heltal, "–" vid Missing, "Fel" i röd text vid Error | 80, höger |
| 8 | Högsta utnyttj. · % | "40 (4, 5)": högsta utnyttjandegraden som heltal och lastfallen med det visade värdet, "–" utan brottlast eller resultat | 110, höger |
| 9 | Dokumentation | de kopplade raderna i kort form (7.4), "–" utan rader | 170 |
| 10 | Anteckning | anteckningen | `Autosize` |

- **Upplösta värden:** kolumnerna 0 och 3–8 visar värdena efter `resolveLineInput`, alltså de som räkningen använde.
- **Sorteringsnycklar** (`setSortKey`) för kolumnerna 3–8 (Min / max sorteras på min).
- **Radtonen** är den gula växlande som i fliken Normalspann (`kAmber`, alfa 64 och 52).
- **Över gränsen:** kolumn 8 får röd text (`setCellColor(rad, 8, ton, {200, 0, 0, 255})`) när något lastfall har `overUtilization`.
- **Fel** (beslut 16): en beräkning som sparats med fel visar "Fel" i röd text i kolumn 7 (samma färg), och tooltipen för den valda raden är "Linberäkningen har fel: <första felet>". Missing visar "–" och tooltipen "Linberäkningen saknar indata: <första upplysningen>".
- **Listans tooltip** för den valda raden (som `updateNsButtons`): den första upplysningen eller "Stakningsmall <n> · Högsta utnyttjandegrad <x> %", och sedan "Visas i Dokumentation på" med radtexterna, eller "Visas inte i Dokumentation.".

### 7.3 Knapparna och snabbmenyn
- **Ny beräkning…:** `openLineCalcDialog(LcOpen::New, 0, "")`, en lös och manuell beräkning.
- **Ändra…**, dubbelklick och Enter (`onActivate`): den valda raden.
- **Kopiera…:** `openLineCalcDialog(LcOpen::Copy, id, "")`. Kopian är lös och manuell (6.6).
- **Ta bort:** `removeLineCalcAsked(id)` (8.5).
- `lcListaChanged` aktiverar Ändra…, Kopiera… och Ta bort bara när en rad är vald.
- **Skrivskyddat:** Ny beräkning, Kopiera och Ta bort är inaktiva, och Ändra… heter "Visa…".
- **Snabbmenyn** (`lcListaContextMenu`, `lcLista.contextRow()`): "Ändra…" (eller "Visa…"), "Kopiera…", separator och "Ta bort…". Ingen kopplingspost: kopplingar görs bara i Dokumentation, som för normalspannet.

### 7.4 `main_window.h` och korta radnamn
**`ByggPmRowDef`** får fältet `std::optional<Ns> lc` sist. De sex raderna "Linberäkning …" får lina och skede där och `key = nullptr`, och `rowKey` svarar med `kalkyl::projectdb::lineCalcDocKey(wire, stage)`. Nycklarna blir exakt de som står i dag (`line_calc_phase_before` …), så de manuella värdena från v0.2.0 står kvar.

**Nya handlers:** `lcNyBtnClicked`, `lcAndraBtnClicked`, `lcKopieraBtnClicked`, `lcTaBortBtnClicked` (`ClickArgs`), `lcListaActivated` (`ActivateArgs`), `lcListaChanged` (`ChangeArgs`) och `lcListaContextMenu` (`ContextMenuArgs`).

**Nya hjälpfunktioner**, var och en med en kommentar på 2–5 rader:
- `enum class LcOpen { New, NewFromDoc, NewManualForRow, Existing, Copy };`
- `refreshLineCalcs()`: läser `lineCalcs` och `docLineSources`, löser upp varje beräkning (`resolveLineInput`) och räknar den (`calculateResolved`), och kör `showLineCalcRows()` och `fillLineCalcList()`
- `showLineCalcRows()` (8.2), `fillLineCalcList()` (7.2) och `updateLcButtons()`
- `openLineCalcDialog(LcOpen mode, std::int64_t id, const std::string& rowKey)` (8.3)
- `removeLineCalcAsked(std::int64_t id)` (8.5)
- `linkedLineCalcIndex(const std::string& key) const` (platsen i `lineCalcs_`, eller −1)
- `lineCalcSummary(std::size_t index) const` (texten i undermenyn Koppla befintlig, 8.3)
- `lineCalcRowLabels() const` (nyckel och radtext för de sex linberäkningsraderna, till formuläret)

**`shortDocLabel`, `fullDocLabel` och `shortRowsText`** gäller också linberäkningsraderna. De korta namnen är normalspannets: "Fas före", "Topp efter", "Topp v. före", "Topp h. efter (dold)". `shortRowsText` får en överlagring för en `LineCalc`.

**Nya medlemmar**, före `saveWorker_`, som ska stå sist:
- `std::vector<kalkyl::projectdb::LineCalc> lineCalcs_`
- `std::vector<kalkyl::projectdb::ResolvedLineInput> lcResolved_` och `std::vector<kalkyl::linecalc::LineCalculation> lcCalcs_` (samma ordning)
- `std::vector<std::int64_t> lcListIds_`
- `glaze::Menu lcMenu_`

**Includes:** `"lineCalc/line_calc.h"` och `"line_calc_dialog.h"` (i `.cpp`).

---

## 8. Fas 3: kopplingen från Dokumentation (paket D, med C:s formulär)

### 8.1 Raderna
De sex raderna "Linberäkning …" i `kByggPmRows` (lila före, rosa efter ombyggnation, enheten Mall):

| Rad (egna topplinor) | Med "Samma" | Lina | Skede | Nyckel |
|---|---|---|---|---|
| Linberäkning faslina före ombyggnation | samma | Faslina | Före | `line_calc_phase_before` |
| Linberäkning faslina efter ombyggnation | samma | Faslina | Efter | `line_calc_phase_after` |
| Linberäkning topplina vänster före ombyggnation | Linberäkning topplina före ombyggnation | Topplina vänster | Före | `line_calc_top_left_before` |
| Linberäkning topplina vänster efter ombyggnation | Linberäkning topplina efter ombyggnation | Topplina vänster | Efter | `line_calc_top_left_after` |
| Linberäkning topplina höger före ombyggnation | döljs | Topplina höger | Före | `line_calc_top_right_before` |
| Linberäkning topplina höger efter ombyggnation | döljs | Topplina höger | Efter | `line_calc_top_right_after` |

En beräkning som är kopplad till en dold höger-rad finns kvar och räknas med vänster-värdena (4.2). I listan heter raden "Topp h. före (dold)".

### 8.2 Värdecellen
**`showLineCalcRows()`**, för varje synlig rad med `lc` (anropas sist i `rebuildByggPm()`, eftersom `setRows` nollar cellernas redigerare, bilder och tooltips, och i `refreshLineCalcs()`):
- **Med en kopplad beräkning som har resultat:**
  - värdet `formatDecimal(std::round(stakingMall), 0, '.')` i en låst cell: `numberEditor(Unit::Mall, true, true)`
  - bilden `glaze::stockIcon(glaze::StockIcon::Sum)`, eller `StockIcon::Warning` när något lastfall har `overUtilization`
  - tooltipen: "Stakningsmall (lastfall 15, maxtemperatur) ur en linberäkning med indata ur Dokumentation: <lina>, aₙ <x> m, σ₀ <y> MPa. Högerklicka för att ändra." För en manuell beräkning börjar den "… ur en manuell linberäkning: …". Över gränsen läggs "Utnyttjandegraden är över <O16> % i lastfall <n, m>." till.
- **Kopplad men utan resultat:** tom cell (`-`). Tooltipen är "Linberäkningen saknar indata: <första Missing>" eller "Linberäkningen har fel: <första Error>".
- **Utan kopplad beräkning:**
  - det manuella värdet ur `byggPmValues_` i talfältet (låst när det är skrivskyddat) och ingen bild
  - tooltipen "Högerklicka för att lägga till eller koppla en linberäkning"

### 8.3 Högerklicksmenyn
`byggPmGridContextMenu` får en tredje gren för raderna med `lc`, efter linraderna och normalspannsraderna:

| Rad och läge | Poster |
|---|---|
| Utan kopplad beräkning | "Lägg till…" · "Lägg till manuell…" · "Koppla befintlig" (undermeny, inaktiv utan manuella beräkningar) · separator · "Manuellt värde" |
| Med kopplad beräkning | "Ändra…" · "Koppla bort" · separator · "Ta bort…" |
| Skrivskyddat, med beräkning | "Visa…" |
| Skrivskyddat, utan beräkning | ingen meny |

- **Lägg till…:** `openLineCalcDialog(LcOpen::NewFromDoc, 0, nyckel)`, alltså `dlg.setNew(nyckel, true)`. Beräkningen använder Dokumentationens lina, aₙ, σ₀, min och max, och de kan bara ändras där (beslut 12).
- **Lägg till manuell…:** `openLineCalcDialog(LcOpen::NewManualForRow, 0, nyckel)`, alltså `dlg.setNew(nyckel, false)`. Den börjar med en kopia av Dokumentationens värden, allt går att ändra (beslut 13), och den kopplas till raden vid OK.
- **Koppla befintlig:** en post per **manuell** beräkning, också de som visas på andra rader. En kopplad beräkning hör till sin rad och står inte med. Texten är `lineCalcSummary`: "<lina> · aₙ 314,6 m · σ₀ 90 MPa · Mall 354 · <korta radnamn eller "ej kopplad">". Ett val kör `linkLineCalc(id, nyckel)`, och det manuella värdet ersätts utan fråga, som för normalspannet.
- **Manuellt värde:** `byggPmGrid.setCurrentCell(rad, kValueColumn)` och `beginEdit()`.
- **Ändra… och Visa…:** `openLineCalcDialog(LcOpen::Existing, id, "")`.
- **Koppla bort:** `unlinkDocRow(nyckel)`. En kopplad beräkning blir manuell med sina senaste värden som egna och finns kvar i fliken (beslut 14). Statusraden säger då "Linberäkningen är bortkopplad och har nu egna indata. Den finns kvar i fliken Linberäkning.".
- **Ta bort…:** `removeLineCalcAsked(id)`.
- **Efter varje åtgärd:** `reloadAfterChange()`. Fel visas med `messageBox(Error, message)`.

**`openLineCalcDialog`**, som `openNormalSpanDialog`:
1. `commitEdits()`.
2. `LineCalcDialog dlg;`, sedan `setProject`, `setSystemDb`, `setNew`, `setExisting` eller `setCopyOf`, `setDocRowLabels(lineCalcRowLabels())`, `setReadOnly(readOnly())` och `dlg.run(*this)`.
3. `reloadAfterChange()`. En ny sparad beräkning väljs i listan.

### 8.4 Värdena hålls aktuella
- **`reloadAfterChange()`** anropar också `refreshLineCalcs()`, efter `refreshNormalSpans()`. Den körs redan efter formulären, menyåtgärderna och Uppdatera från linregistret.
- **`byggPmGridCommitted`** anropar `refreshLineCalcs()` efter en lyckad sparning av en cell, så att en ändrad uppspänning, lina, normalspann eller temperatur syns direkt på linberäkningsraderna och i listan.
- **När läget byts** ("Samma" eller egna topplinor) och `rebuildByggPm` körs i pump-svansen följer `fillLineCalcList()` med, som `fillNormalSpanList()`.
- **`onLoad`** anropar `refreshLineCalcs()` efter `refreshNormalSpans()`.
- **Kostnaden:** en omräkning är under en millisekund per beräkning. Ett projekt har några få.
- **Ingenting lagras:** en ändring i Dokumentation skriver aldrig i `line_calc`. Den kopplade beräkningen läser värdena varje gång den räknas (2.3).

### 8.5 Ta bort
**`removeLineCalcAsked(id)`** frågar Ja/Nej (`Warning`). Texten är "Ta bort linberäkningen <lina> (<Manuell eller Indata ur Dokumentation>, Mall <n>) ur projektet?" ("beräkningen har fel" i stället för Mall), följd av en tom rad och:
- utan rader: "Den visas inte i Dokumentation."
- en rad: "Den visas på raden <radtext>, som visar - efteråt."
- flera rader: "Den visas på <n> rader i Dokumentation, som visar - efteråt:" följt av radtexterna, en per rad.

Vid Ja körs `removeLineCalc(id)` och `reloadAfterChange()`.

### 8.6 Manuella beräkningar
- **Skapas** med Ny beräkning… och Kopiera… i fliken (lösa), med Lägg till manuell… på en rad, eller genom att `docInputChk` slås av i formuläret.
- **Har** en egen kopia av lindatat och egna värden för aₙ, σ₀ och temperaturerna (beslut 7). Allt går att ändra, och ändringar i Dokumentation påverkar den inte.
- **Skillnaden mot en kopplad beräkning** (beslut 12): den kopplade har ingen kopia. Den räknar alltid med Dokumentationens värden, och de går bara att ändra i Dokumentation.
- **Kopplas** till fler rader med Koppla befintlig. Samma beräkning visar då samma mall på alla raderna, till exempel faslina före och efter när ombyggnaden inte rör linan.
- **Blir kopplad** genom att `docInputChk` slås på i formuläret, när den visas på exakt en rad.

---

## 9. Fel och validering

| Var | Fel | Visas |
|---|---|---|
| Formulärets tabeller | ogiltigt tal i en cell (DataGrid: "Ogiltigt värde") | röd cell och statusraden |
| Formuläret, beräkningen | upplysningar ur `calculateLine` och `resolveLineInput` | färg och tooltip på cellen (6.5) och statusraden (6.11) |
| Formuläret, inklistring | text i en talcell, ett L som inte är 1–15 | `messageBox(Warning)` med texten i 5.2, inget ändras |
| Formuläret, OK | ogiltigt tal, ofullständigt lindata, fel i beräkningen | 6.10 |
| Formuläret, Övre linan | kortslutningstemperaturen är inget tal | `messageBox(Warning)` |
| Projektdatabasen | `AccessDenied`, `NotFound`, `Invalid`, `Sql` | `messageBox(Error, message)`, och `detail` loggas med `gfx::log` |
| Dokumentation | en kopplad beräkning saknar indata eller har fel | tom cell med tooltip (8.2) |
| Fliken | samma | "–" eller "Fel" i kolumnen Mall, och tooltipen |

- **Två lager:** `saveLineCalc` stoppar bara tal som inte är ändliga och okända nycklar. Allt annat (negativa värden, O17 utanför 1–7, L utanför 1–15) är upplysningar ur `calculateLine`, och en sådan beräkning kan sparas efter frågan i 6.10.
- **Inga nya felkoder.** `Invalid` och `NotFound` finns redan.

---

## 10. Tester

Testerna har samma form som de befintliga: en `report`-funktion, en funktion per del, `near(a, b, tol)` och exit-kod 1 vid fel.

### 10.1 `libs/projectdb/projectdb_test.cpp` (paket B)
T1:s lina (Dotterel) och indata skrivs i testet, med talen ur fas 1, bilaga A.
1. **`testSchemaTables`:** ett nytt projekt har de nio tabellerna.
2. **`testMigrateFromV020`:** enligt 3.3.
3. **`testLineCalcKeys`:** `lineCalcDocKey` ger de sex nycklarna i 8.1, och `lineCalcRowOf` ger tillbaka lina och skede. En normalspannsnyckel och "temp_max" ger tomt.
4. **`testLineCalcRoundTrip`:**
   - T1 i läget Excel, med varje fält i `LineInput`, `LineConstants` och `LoadCase` satt till ett värde som skiljer sig från förvalet: alla sex temperaturkällor och tre vindtryckskällor, `windFromCase` satt och tomt, manuell last i ett lastfall, benämningar med åäö och dubbla mellanslag och anteckningen "Anteckning åäö".
   - Läs tillbaka: alla fält lika, `docKeys` tom, id > 0, `modifiedUtc` satt.
   - Lindata utan namn (namn "") och utan brottlast läses tillbaka så. Utan lindata (`conductor` tom) blir det tomt igen.
   - Spara igen med ändrade lastfall: de 15 raderna ersätts, och `line_calc_case` har 15 rader för beräkningen.
5. **`testLineCalcLinking`:**
   - `setDocValue(line_calc_phase_before = "354", ref "Excel")`, sedan `saveLineCalc(M, linkDocKey = line_calc_phase_before)`: värdet töms och referensen står kvar.
   - `linkLineCalc(M, line_calc_phase_after)`: M har två rader.
   - En kopplad beräkning K sparas med `docSource` = `line_calc_top_left_before`: den har bara den raden. `linkLineCalc(K, line_calc_phase_before)` ger `Invalid` med texten i 4.3.
   - `linkLineCalc(M, line_calc_top_left_before)`: K blir manuell (`docSource` tom) med de värden den hade, och raden visar M.
   - `unlinkDocRow` på en rad med en kopplad beräkning gör den manuell. På en rad med en manuell lämnas den orörd.
   - En okänd nyckel ("temp_max", "ns_calc_phase_before") ger `Invalid`. Primärnyckeln stoppar två beräkningar på samma rad (direkt `INSERT` som ska ge fel).
6. **`testLineCalcResolve`** (ren funktion, `DocLineSources` byggd i testet):
   - Faslina Dotterel, `tension_phase` "90", `normal_span_phase` "314.6", `temp_min` "-40" och `temp_max` "60": före ger linan, aₙ 314,6, σ₀ 90, min −40 och max 60, och `sources` med fem poster och rätt nycklar.
   - I läget Excel har lastfall 1, 2 och 15 talen −40, −20 och 60 efteråt. I läget Ny följer talen källorna.
   - En kopplad normalspannsberäkning med aₙ 331,7 på `ns_calc_phase_before` går före den gröna raden. Med fel ger den Missing med texten i 4.4, och aₙ blir tomt fast den gröna raden har ett tal.
   - Ett manuellt värde på normalspannsraden går före den gröna raden.
   - Tom `tension_phase` och tom linrad ger Missing med texterna i 4.4.
   - "Samma" på Topplina höger: `line_calc_top_right_after` löses upp med vänster-värdena.
   - Tom `docSource` ger input orörd och inga upplysningar.
   - Utan `temp_min` ger upplösningen Missing "Fyll i Min temp i fliken Dokumentation.", och `calculateResolved` ger inget resultat fast `calculateLine` hade kunnat räkna. Likadant utan `temp_max`.
   - **`calculateResolved`:** med en Missing ur upplösningen finns inget resultat, upplösningens upplysning står först, och `calculateLine`s "Uppspänningen saknas." finns inte med när Dokumentation saknar σ₀. Utan upplysningar ger den samma resultat som `calculateLine` (T1).
7. **`testLineCalcDetach`:** `detachLineCalc` på en kopplad beräkning skriver linan, aₙ, σ₀, min och max ur Dokumentation som egna och tömmer `docSource`. Kopplingen står kvar. Ändras Dokumentation efteråt ändras inte beräkningen.
   - **Ingen kopia:** en kopplad beräkning som sparas med lindata, aₙ och σ₀ i `input` läses tillbaka med dem tomma, och `line_calc` har NULL i `cond_name`, `normal_span_m` och `tension_mpa` (direkt `SELECT`). Ändras `tension_phase` i Dokumentation ger nästa `resolveLineInput` det nya värdet.
8. **`testLineCalcRemove`:** en beräkning med två rader tas bort. Lastfallen och kopplingarna försvinner (räkna i tabellerna), och ett id som saknas är inget fel.
9. **`testReadOnlyWrites`** (utökad): `saveLineCalc`, `linkLineCalc`, `detachLineCalc` och `removeLineCalc` ger `AccessDenied`, och `isModified()` förblir falsk.
10. **`testInvalid`** (utökad): NaN i ett lastfall, i lindatat och i en konstant ger `Invalid` "Ogiltigt tal.". `docSource` och `linkDocKey` olika ger `Invalid`. Ett id som saknas ger `NotFound`. En okänd text i `mode` (direkt `UPDATE`) ger `Invalid` vid läsning.
11. **`testProvProject`** (utökad): provprojektet får dessutom `tension_phase` "90", Normalspann faslina "314.6", en kopplad linberäkning på `line_calc_phase_before` och en manuell T1 i läget Excel kopplad till `line_calc_phase_after`, så att röktestet och handtestet har data.

### 10.2 `libs/sheetGrid/sheetgrid_test.cpp` (ctest `sheetgrid`, paket A)
En `SheetShape` som lastfallstabellen: 16 rader, 26 kolumner, `firstColumn` 1, tomrad 7, indata i kolumnerna 1, 2, 4, 5, 6, 8, 9, 10, 11, 13, 15 och 16, och `accept` som tar tal i talkolumnerna.
1. **Ett block som arkets G23:S38** (16 rader × 13 fält) klistrat i (0, 1): ändringar bara i indatakolumnerna, ingen ändring på rad 7, och rad 15 får blockets sista rad. Fälten i kolumnerna 3, 7 och 12 ignoreras.
2. **Rubrikrad:** ett block vars första rad har text i en talkolumn och vars andra rad har tal: första raden hoppas över (`skippedHeader`), och den andra raden landar på ankarraden + 1.
3. **Avbruten inklistring:** text i en talkolumn på rad 3 i blocket ger `rejected` med texten i 5.2 ordagrant och inga ändringar.
4. **Utanför tabellen:** fält till vänster om `firstColumn`, till höger om sista kolumnen och under sista raden ignoreras, och `area` är klampad.
5. **Tomt fält** ger en ändring med tom text.
6. **`rectOf`, `clampPos` och `isBlock`:**
   - `clampPos` på rad 7 med steg +1 ger rad 8, och med steg −1 rad 6. Utanför tabellen klampas till kanten.
   - `isBlock("5" + LF)` är falsk, "a" + tabb + "b" är sant, två rader är sant.

### 10.3 GUI
- Inga automatiska GUI-tester.
- glaze-render på `line_calc_dialog.h` och `main_window.h` ska ge exit 0, och bilderna granskas med Read: formuläret med blocken som i arket, och huvudfönstret med fliken Linberäkning.
- Handtestet i avsnitt 13 täcker beteendet.

---

## 11. Paket, filägarskap och byggregler

Fyra paket arbetar parallellt (Sonnet, high). Ingen fil skrivs av två paket.

### 11.1 Ägarskap
| Paket | Äger |
|---|---|
| A, Excel-tabellen | `libs/sheetGrid/*` (ny: `CMakeLists.txt`, `CLAUDE.md`, `sheet_grid.h`, `sheet_grid.cpp`, `sheetgrid_test.cpp`), `libs/CMakeLists.txt` och `libs/CLAUDE.md` |
| B, projektdatabasen | `libs/projectdb/*`: `dev_migration.h`, `project_db.h`, `project_db.cpp`, `project_data.h`, `project_data.cpp`, `projectdb_test.cpp`, `CMakeLists.txt` och `CLAUDE.md` |
| C, formuläret | `src/windows/line_calc_dialog.{h,glazeui,cpp}` (nya), `src/CMakeLists.txt` och `src/CLAUDE.md` |
| D, huvudfönstret | `src/windows/main_window.{h,glazeui,cpp}` och `src/windows/CLAUDE.md` |
| Samordnaren | rotens `CLAUDE.md`, `TODO.md` och `doc/plans/CLAUDE.md` |

- A skriver också B:s rad i `libs/CLAUDE.md` (projectdb beror på `lineCalc` och `tableText`).
- C:s tre rader i `src/windows/CLAUDE.md` står i C:s rapport, och samordnaren lägger in dem när D är klar.

### 11.2 Steg 0: kontraktsfilerna först
Inom några minuter skriver varje paket sina kontraktsfiler, så att de andra kan kompilera:
- **A:** `sheet_grid.h` ordagrant ur 5.1, en `sheet_grid.cpp` som kompilerar (tomma kroppar), `libs/sheetGrid/CMakeLists.txt` och `add_subdirectory(sheetGrid)` i `libs/CMakeLists.txt`.
- **B:** tilläggen i `project_data.h` ordagrant ur 4.1, kroppar i `project_data.cpp` som kompilerar, och `CMakeLists.txt` med `kalkyl_linecalc` PUBLIC och `kalkyl_tabletext` PRIVATE.
- **C:** `line_calc_dialog.h` med regionen och gränssnittet i 6.1, en minimal `.glazeui` och `.cpp` som kompilerar, raden i `glaze_add_window` och `kalkyl_linecalc` och `kalkyl_sheetgrid` i `target_link_libraries` i `src/CMakeLists.txt`.
- **D:** behöver inga kontraktsfiler. D börjar med `ByggPmRowDef::lc` och fliken.

Ingen väntar på ett meddelande. Den som behöver en annans fil bygger när filen finns.

### 11.3 Byggkataloger
- **En katalog per paket:** `build-a`, `build-b`, `build-c` och `build-d`. Paketet tar bort sin katalog när det är klart (`rm -rf build-<x>`).
- **`build/` är användarens.** Den tas aldrig bort, rensas inte och konfigureras inte om av en agent.
- **MSYS2:s cmake** (`C:/msys64/ucrt64/bin/cmake.exe`), aldrig `C:/Program Files/CMake`.
- **Konfiguration** (med paketets katalog; C och D lägger till `-DGLAZE_TOOLS=ON`):
  ```sh
  cmake -S . -B build-a -G Ninja -DCMAKE_BUILD_TYPE=Debug -DCMAKE_C_COMPILER=C:/msys64/ucrt64/bin/gcc.exe -DCMAKE_CXX_COMPILER=C:/msys64/ucrt64/bin/g++.exe -DKALKYL_TESTS=ON -DFETCHCONTENT_SOURCE_DIR_GLAZEUI=C:/Users/sehoer/Source/Kalkyl/build/_deps/glazeui-src
  ```
- **Bygg bara de egna målen:**
  - A: `kalkyl_sheetgrid_test` och `ctest -R sheetgrid`
  - B: `kalkyl_projectdb_test` och `ctest -R projectdb`
  - C och D: `kalkyl` och `glaze-render`
- **Förhandsgranskning** (C och D):
  ```sh
  build-c/_deps/glazeui-build/apps/glaze-render/glaze-render.exe src/windows/line_calc_dialog.h -o <scratchpad>/line_calc_dialog.png
  ```
  Exit 0 krävs, och bilden granskas med Read. D gör samma sak med `main_window.h`.
- **Röktest** (C och D, när allt kompilerar):
  ```sh
  KALKYL_DATA_DIR=build-c/smoke timeout -k 2 3 ./build-c/src/kalkyl.exe; echo $?
  ```
  Svaret ska vara 124 eller 137.
- **Faller bygget i en fil som ett annat paket äger:** vänta en stund och försök igen. Rör aldrig den filen.

### 11.4 Arbetsgång per paket
- **A:** steg 0, `planSheetPaste`, `rectOf`, `clampPos` och `isBlock`, testerna i 10.2, sedan `SheetGrid` enligt 5.2 (förebilden är `normal_span_dialog.cpp`, rad 273–1027), sist CLAUDE.md.
- **B:** steg 0, schemat (3.1–3.2), läsning och skrivning (4.3), upplösningen och `calculateResolved` (4.2 och 4.4), testerna i 10.1, sist CLAUDE.md.
- **C**, i den här ordningen så att varje del kan provas för sig:
  1. steg 0
  2. layouten (6.2) och glaze-render
  3. tabellerna, visningen och omräkningen (6.3–6.6)
  4. `SheetGrid` (6.7), när A:s header finns
  5. linan (6.8), knapparna, läget och skillnaden (6.12)
  6. kopplad beräkning: låsta fält och `docInputChk`
  7. skrivskydd, sparning och frågorna (6.9–6.10)
  8. fullt bygge, glaze-render och röktest
- **D**, i den här ordningen:
  1. `ByggPmRowDef::lc`, `rowKey` och de korta radnamnen (7.4)
  2. fliken Linberäkning (7.1–7.3)
  3. raderna i Dokumentation, menyn och värdena (8.2–8.5)
  4. omräkningen när Dokumentation ändras (8.4)
  5. fullt bygge när B:s och C:s steg 0 finns, och slutbygget när alla rapporterat klart
  6. glaze-render och röktest med provprojektet ur `build-d/prov`

### 11.5 Regler för alla implementeringsagenter
- **Filer redigeras med Edit och Write.** Inga Python- eller sed-skript ändrar filer. Skal används bara för bygge, körning, test och läsning.
- **Hooken `~/.claude/hooks/only-c-drive.sh`** granskar varje verktygsanrop, också filinnehåll, och stoppar:
  - två bakvända snedstreck i rad
  - ett bakvänt snedstreck och en bokstav (escape-sekvens) efter skiljetecken eller blanksteg, följt av ett nytt bakvänt snedstreck, ett citattecken eller slutet. **En kolumnrubrik i `.glazeui` får därför aldrig börja med två radbrytningar.** Fyll ut med tomma rader efter gruppnamnet, aldrig före rubriken (6.3)
  - andra enhetsbokstäver än C följda av kolon, också precis före ett citattecken. Låt aldrig en Edit-sträng sluta med en ensam bokstav och kolon
- **Därför:**
  - råa strängliteraler `R"sql( … )sql"` för SQL och `R"( … )"` för reguljära uttryck
  - teckenklasser som `[.]` och `[0-9]` i stället för escape
  - `char{10}` för radbrytning och `char{9}` för tabb i C++-strängar
  - snedstreck framåt i sökvägar, och i CMake bracket-argument och `string(ASCII 10 NL)`
  - i `.glazeui` skrivs radbrytningen i en kolumnrubrik direkt efter en bokstav, en siffra eller en parentes, som i `normal_span_dialog.glazeui`
- **Kommentarer:** varje funktion, klass och logisk del har en kommentar på 2–5 rader. Inline-kommentarer är korta och säger bara vad som händer. Ingen historik i kommentarer.
- **Fönsterreglerna:** tre filer per fönster, klassen utanför namespace, designraden, ingen logik i `.glazeui` (inga if, loopar eller lokala variabler), en sats per rad, startlogik i `onLoad` och bara `<glaze/glaze.h>` från GlazeUI.
- **Varningar:** bygget ska vara utan varningar (`kalkyl_warnings`, `-Wall -Wextra`).
- **Versionen** står kvar på 0.3.0. Inga commits.
- **CLAUDE.md:** uppdatera katalogens CLAUDE.md när filer läggs till eller byter syfte.

---

## 12. Integration och verifiering (samordnaren)
1. **Bygget** i `build/` utan varningar: `cmake --build --preset debug 2>&1 | grep -i warning` ger ingen utdata. De nya CMake-filerna får CMake att köras om av sig självt.
2. **Testerna** i en tillfällig katalog:
   ```sh
   cmake -S . -B build-verify -G Ninja -DCMAKE_BUILD_TYPE=Debug -DCMAKE_C_COMPILER=C:/msys64/ucrt64/bin/gcc.exe -DCMAKE_CXX_COMPILER=C:/msys64/ucrt64/bin/g++.exe -DKALKYL_TESTS=ON -DGLAZE_TOOLS=ON -DFETCHCONTENT_SOURCE_DIR_GLAZEUI=C:/Users/sehoer/Source/Kalkyl/build/_deps/glazeui-src
   cmake --build build-verify && ctest --test-dir build-verify --output-on-failure
   ```
   Alla tester ska gå igenom, också `sheetgrid`, `projectdb`, `linecalc`, `load_cases` och `linecalc_facit`.
3. **glaze-render** på `line_calc_dialog.h`, `main_window.h` och `normal_span_dialog.h`: exit 0, och bilderna granskas.
4. **Röktest:** `KALKYL_DATA_DIR=build/smoke timeout -k 2 3 ./build/src/kalkyl.exe; echo $?` ger 124 eller 137. Med provprojektet ur `build-verify/prov` som argument visas huvudfönstret.
5. **Reglerna:** CLAUDE.md i varje berörd katalog, kommentarerna, ingen logik i `.glazeui`, och `project(Kalkyl VERSION 0.3.0` oförändrad.
6. **Städning:** `rm -rf build-verify`. `build-a` till `build-d` är borta.
7. **Handtestet:** användaren får listan i avsnitt 13.

---

## 13. Handtest (användaren, före granskningen)

Kör `build/src/kalkyl.exe` (Debug). Ha Excel öppet med `ref/Linberäkning.xlsm` och tillägget 5420_Verktyg laddat. Talen nedan är facit ur fas 1 (bilaga A och tabellen i 6.4).

**Projektet**
1. **Projekt från v0.2.0:** öppna ett projekt som sparats med v0.2.0. Det öppnas utan fel, Dokumentation och fliken Normalspann är som förut, och manuella värden på raderna "Linberäkning …" står kvar. Fliken Linberäkning är tom.
2. **Provprojektet** (`build-verify/prov/Prov åäö.kalkyl`, före städningen): fliken Linberäkning har en kopplad och en manuell beräkning, och raderna "Linberäkning faslina före/efter ombyggnation" visar en mall.

**Fliken och formuläret**
3. **Ny beräkning…:**
   - Lina, aₙ och σ₀ är tomma. aₙ och σ₀ har den gula färgen för det som saknas.
   - T₀ 0, min −50, max 50 och Tks 100. Lastfallen följer standardmallen, med −50, −30 och 50 i lastfall 1, 2 och 15.
   - Resultatkolumnerna är tomma, och statusraden visar "Lina eller lindata saknas.".
   - Texten överst är "Blir en lös beräkning i fliken Linberäkning.".
4. **Uppställningen** jämförd med arket: Indata, Lindata / Konstanter och knapparna till höger, lastfallstabellen med grupperna Lastfall, Resultat och Data till SAG (3D) överst i rubrikerna, tomraden mellan lastfall 7 och 8, den ljusorange tonen på jämna lastfall och den smala tomma kolumnen före SAG. Rulla i sidled: ID står kvar.
5. **T1:** välj linan "124 mm² OPGW AFL 1x48 SMF 14230/20-01 (Dotterel)", skriv aₙ 314,6 och σ₀ 90.
   - Lindatat fylls i (A 124, d 14,9 … fᵤ 77,7). Tₚ visar 23,4, och tooltipen visar hela talet.
   - X i lastfall 4 är 249,63, i lastfall 7 90,00 och i lastfall 15 60,87. Z i lastfall 15 är 354, och AE i lastfall 4 är 412.
   - Jämför alla rader med arket. Statusraden: "Stakningsmall 354 · Högsta utnyttjandegrad 40 % (lastfall 4, 5) · Läge Ny".
6. **Lindata för hand:** ändra d till 15. Cellen får den bruna tonen och tooltipen "Linregistret: 14,9", texten bredvid Lina säger "Ändrat för hand …", och resultaten ändras. Skriv 14,9 igen: tonen försvinner.
7. **Min och max:** skriv min −40. Lastfall 1 och 2 blir −40 och −20, med tooltipen "Följer minimumtemperaturen …". Skriv max 60: lastfall 15 blir 60. Skriv tillbaka −50 och 50.
8. **Läget och skillnaden:**
   - Kryssa "Markera skillnaden mot läget Excel". Med T1 skiljer sig inga celler.
   - Skriv Gc 0,6, Cc 1,1 och transponering 4 (T2). Några celler i Ep-lastfallen får den turkosa tonen, och tooltipen visar båda värdena. Statusraden säger hur många.
   - Välj Excel: X i lastfall 4 är 263,61 och i lastfall 15 60,58, som arket. Statusraden visar varningen om Eiv (D1). Kryssrutan heter nu "… läget Ny".
   - Skriv tillbaka Gc 0,5, Cc 1 och transponering 5, och välj Ny.
9. **Övre linan:** frågan om kortslutningstemperaturen visas med 100. Avbryt: inget händer. OK: lastfallen blir korsningsmallen för övre linan och transponeringen 3. X i lastfall 4 är 90,00 och i lastfall 15 75,05 (T12).
10. **Nedre linan:** lastfallen blir mallen för nedre linan. X i lastfall 4 är 125,37 och i lastfall 15 75,05 (T11).
11. **Återställ:** standardmallen, min −40 och max 60. Linan, aₙ, σ₀ och T₀ står kvar.
12. **Manuell last (T13):** återställ, skriv min −50 och max 50, och skriv 25 i U i lastfall 3, sedan 5 i V.
    - Lastfall 3 blir manuellt: U och V får den lila tonen, och J, K och M–T är tomma i raden.
    - X i lastfall 3 är 274,49, och lastfall 4 och 15 är som i T1.
    - Högerklicka på raden och välj "Räkna last ur vind och is": vindvärdena kommer tillbaka och lastfallet räknas som förut.
13. **Lastfall utan benämning:** töm H i lastfall 7. Raden blir grå men räknas.
14. **Röd utnyttjandegrad (T4):** en ny beräkning med linan "157 mm² FeAl PARTRIDGE", Återställ, aₙ 400 och σ₀ 50. AA är röd i lastfall 4, 5, 8 och 9, och statusraden varnar. X i lastfall 4 är 180,93.
15. **Fel:** σ₀ 0 ger en röd cell och "Uppspänningen måste vara större än 0.". L 16 ger "Lastfall-ID för vindparametrarna måste vara 1–15.". "abc" i en talcell ger "Ogiltigt värde".

**Urklipp och markering**
16. **Klistra in från Excel:** gör Nedre linan i arket, kopiera G23:S38 (med rad 30) och klistra in i G på lastfall 1 i en beräkning med standardmallen. Lastfallen blir som arkets, tomraden hoppas över, och resultaten blir som i arket.
17. **Rubrikrad:** kopiera G22:S38 och klistra in i G på lastfall 1. Rubrikraden hoppas över.
18. **Text i talkolumn:** klistra in "abc" i N. Meddelandet visas, och inget ändras.
19. **Resultatkolumner:** klistra in ett block över X–AA. Inget ändras där.
20. **Lindata ur Excel:** kopiera J9:J18 i arket och klistra in i A. Lindatat fylls i.
21. **Till Excel:** markera X i lastfall 1 till AA i lastfall 15, Ctrl+C, och klistra in i ett tomt blad. Det blir 16 rader med en tom rad som rad 8, och talen har decimalkomma.
22. **Markeringen:** Shift+piltangenter, Shift+klick och Ctrl+A markerar. Delete tömmer bara indatacellerna, och Ctrl+X kopierar och tömmer. En pil utan Shift tar bort markeringen.

**Spara, lista och skrivskydd**
23. **OK:** listan visar Dotterel, Manuell, Ny, 314,6, 90, 0, −50 / 50, 354, 40 (4, 5), –, och anteckningen. Tooltipen visar stakningsmallen och "Visas inte i Dokumentation.".
24. **Avbryt med ändringar:** frågan visas. Nej kastar ändringarna, Ja sparar.
25. **OK med fel:** σ₀ 0 och OK ger frågan "Linberäkningen har fel …". Töm en lindatacell och tryck OK: frågan om ofullständigt lindata.
26. **Kopiera…:** formuläret visar "Kopia av …" och "Blir en lös beräkning …". Ändra något och tryck OK: listan har två rader, och originalet är oförändrat.
27. **Sortering:** klicka på rubrikerna aₙ, Mall och Högsta utnyttj.
28. **Ta bort:** frågan visas, och raden försvinner.
29. **Skrivskyddat:** öppna projektet i en andra Kalkyl och välj skrivskyddat. Fliken har Visa…, formuläret har Stäng, Ctrl+C och skillnadsrutan fungerar, men inget går att ändra.
30. **Utseende:** växla Mörkt och Ljust. Alla tonerna går att läsa i båda.

**Dokumentation**
31. **Rad utan beräkning:** högerklicka på "Linberäkning faslina före ombyggnation". Menyn har Lägg till…, Lägg till manuell…, Koppla befintlig och Manuellt värde. Skriv 354 som manuellt värde: cellen visar "354 Mall".
32. **Lägg till…:** fyll först i Faslina Dotterel, Uppspänning fas 90, Normalspann faslina 314,6, Min temp −50 och Max temp 50.
    - Formuläret visar linan, lindatat, aₙ, σ₀, min och max låsta med den gröna tonen och "Ur Dokumentation: Faslina". Tooltipen säger vilken rad varje värde kommer ur. "Hämta … ur Dokumentation" är ikryssad.
    - T₀, linhöjden, vindtrycken, konstanterna, lastfallen och läget går att ändra. Återställ ändrar inte min och max (de är Dokumentationens −50 och 50).
    - OK: raden visar "354 Mall" med Σ-bild och tooltip, och det manuella värdet är borta. Listan visar Indata "Dokumentation" och "Fas före".
33. **Ändring i Dokumentation:** skriv Uppspänning fas 100. Raden och listan ändras direkt, utan att formuläret öppnas. Skriv tillbaka 90.
34. **Temperatur i Dokumentation:** skriv Max temp 60. Mallen på raden ändras direkt (mallen gäller maxtemperaturen). Öppna beräkningen: max är 60 och låst, och lastfall 15 räknas vid 60. Skriv tillbaka 50.
35. **Normalspannsberäkning:** koppla en normalspannsberäkning till "Normalspannsberäkning faslina före ombyggnation". Linberäkningen räknar med dess aₙ. Ge normalspannsberäkningen ett fel: linberäkningsraden blir tom med tooltipen "… har fel och ger inget aₙ. Rätta den i fliken Normalspann.".
36. **Saknade värden:**
    - Töm Uppspänning fas. Raden visar "-" och tooltipen "Linberäkningen saknar indata: Fyll i Uppspänning fas i fliken Dokumentation.". I formuläret är σ₀ tom, låst och gul, och resultaten är tomma.
    - Töm Max temp. Raden visar "-" med "Fyll i Max temp i fliken Dokumentation.". Fyll i båda igen: mallen kommer tillbaka.
37. **Koppla bort:** på raden med den kopplade beräkningen. Raden visar "-", statusraden säger att beräkningen har egna indata, och listan visar den som Manuell med de senaste värdena. Ändra Uppspänning fas: den bortkopplade beräkningen ändras inte. Öppna den: allt går att ändra.
38. **Lägg till manuell…:** på "Linberäkning faslina efter ombyggnation". Formuläret har en kopia av Dokumentationens värden, olåsta. Ändra σ₀ och tryck OK: raden visar beräkningens mall, och listan visar Manuell. Ändra Uppspänning fas i Dokumentation: den manuella beräkningen ändras inte.
39. **Koppla befintlig:** koppla den manuella beräkningen också till "Linberäkning topplina före ombyggnation". Båda raderna visar samma mall, och listan visar "Fas efter, Topp före". Kopplade beräkningar står inte i undermenyn.
40. **Hämta ur Dokumentation:** öppna en manuell beräkning som visas på en rad och kryssa i rutan. Värdena hämtas och låses, och efter OK visar listan "Dokumentation". En beräkning som visas på två rader har ingen sådan ruta.
41. **Topplina höger:** sätt en egen lina på Topplina höger. Höger-raderna syns och har menyn. Koppla en beräkning till "Linberäkning topplina höger före ombyggnation" och sätt tillbaka "Samma": listan visar "Topp h. före (dold)", och beräkningen räknas med vänster-värdena.
42. **Ta bort… på en rad** med en beräkning som visas på två rader: frågan listar båda raderna.
43. **Skrivskyddat:** högerklick på en rad med beräkning ger bara Visa….
44. **Spara och öppna igen:** alla beräkningar, kopplingar, lägen, manuella laster och anteckningar finns kvar. En kopplad beräkning visar Dokumentationens aktuella värden.
45. **Normalspann:** fliken Normalspann och normalspannsraderna fungerar som förut.

---

## 14. CLAUDE.md och TODO.md

| Fil | Vem | Ändring |
|---|---|---|
| `libs/CLAUDE.md` | A | Raden `sheetGrid/`. Beroendena: `sheetGrid` beror på `glaze` och `tableText`, och `projectdb` på `db`, `anCalc`, `conductor`, `lineCalc` och `tableText` |
| `libs/sheetGrid/CLAUDE.md` (ny) | A | Biblioteket, filerna och reglerna: fönstret äger innehållet, tabellen växer aldrig, tomrader hoppas över, planeringen är ren och testad. Hänvisning till den här planen, avsnitt 5 |
| `libs/projectdb/CLAUDE.md` | B | Steg 3 och de tre tabellerna, `LineCalc`, regeln om kopplad och manuell beräkning, upplösningen (`docLineSources`, `resolveLineInput`), att linberäkningarna har eget lindata och inte rör `project_conductor`, och hänvisningen till den här planen |
| `src/CLAUDE.md` | C | `CMakeLists.txt` länkar också `kalkyl_linecalc` och `kalkyl_sheetgrid` |
| `src/windows/CLAUDE.md` | D, och C:s rader via samordnaren | Tre rader för `line_calc_dialog.*`. `main_window.*`: fliken Linberäkning (`lcSida`, `lcLista`), linberäkningsraderna i Dokumentation med meny och mall, och `ByggPmRowDef::lc` |
| Rotens `CLAUDE.md` | samordnaren | **Delar:** raderna Linberäkning (`src/windows/line_calc_dialog.*`) och Excel-tabell (`libs/sheetGrid`), och huvudfönstrets rad med fliken Linberäkning. **Data:** projektet sparar också linberäkningarna |
| `TODO.md` | samordnaren | Punkten Linberäkning får status efter fas. Idén "Normalspannsformuläret använder `libs/sheetGrid`" läggs till |

`doc/plans/CLAUDE.md` har redan raden för den här planen.

---

## 15. Risker och luckor i GlazeUI v0.11.1

1. **Tomma rader i en kolumnrubrik.** Gruppnamnen överst bygger på att DataGrid ritar tomma rader i en rubrik (6.3). C prövar det med glaze-render. Fallback: gruppnamnen står i kolumntooltipsen, och rubrikerna får bara kolumnens egna rader.
2. **Radrubriker, tomrad och rullning tillsammans.** Kolumn 0 som radrubrik, en tomrad och en tabell som rullar i sidled inne i en rullande panel är inte prövade ihop i Kalkyl. C provar mushjulet över tabellen (det ska rulla panelen) och Shift+mushjul (tabellen i sidled).
3. **Nedsänkta tecken** (Eᵢₖ, Eₚ, σₚ, Eᵢᵥ, qₑ, fᵤ, qₕ) ritas med GlazeUI:s reservfont om UI-fonten saknar dem. Syns ersättningstecken skrivs etiketterna utan nedsänkning (Eik, Ep).
4. **Ingen områdesmarkering i DataGrid.** `SheetGrid` emulerar den som normalspannsformuläret gör. Ingen dragmarkering.
5. **Inget event när den aktuella cellen flyttas.** Markeringen synkas med `glaze::app().post`, som i normalspannsformuläret.
6. **Paket D är störst**, eftersom fliken och kopplingen ligger i samma tre filer. Ordningen i 11.4 gör att fliken kan provas innan kopplingen finns.
7. **En kopplad beräkning i läget Excel** får lastfall 1, 2 och 15 överskrivna när min eller max i Dokumentation ändras, som arkets händelser (D9). Det är avsiktligt och visas med skillnadsrutan. Tills beräkningen sparas igen görs överskrivningen vid varje omräkning, med samma resultat varje gång.
8. **Rullistan Lina** har drygt 130 poster. Dropdown rullar och klarar det, men har ingen sökning.

---

## 16. Öppna frågor

Inga. De fem frågorna i utkastet är besvarade, och svaren står som beslut 12–16 överst.
