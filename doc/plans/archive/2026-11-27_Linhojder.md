# Linhöjder: ny flik med linhöjder per spann för det ändrade området

- **Datum:** 2026-11-27.
- **Status:** klar och arkiverad 2026-11-27 (version 0.7.0), handtestad och granskad. Ändringarna efter handtestet (H1–H6 i avsnitt 1) går före de äldre avsnitten: filtret i Stolptabell efter står kvar på Förändrade ±2.
- **Version:** 0.7.0. Samordnaren sätter `project(Kalkyl VERSION 0.7.0` i rotens `CMakeLists.txt` innan paketen startar. Agenterna rör den aldrig.
- **Underlag:**
  - Förlagan: `ref/Beräkningar och indata.xlsx`, bladet "Data för beräkningar CL7 S1-3", rad 38–98 (blocken STP 379, STP 380 och STP 381). Format, sammanslagningar, bredder och formler står i avsnitt 3.
  - Koden: `src/windows/main_window.*`, `libs/poleView`, `libs/poleGrid`, `libs/poleTable`, `libs/tableText`, `libs/autoMode`, `libs/projectdb` och `libs/systemdb`.
  - GlazeUI v0.11.1, `build/_deps/glazeui-src/MANUAL.md`, avsnitten:
    - `custom-widget`, `widget-panel`, `widget-text-input`, `testing`, `glaze-render` och `glazebuilder`;
    - källan `property_grid.h` och `property_grid.cpp` (förebilden för redigeringen) och `panel.h` (`contentExtent`).
  - De arkiverade planerna `2026-11-27_Filter_stolptabell_efter.md` (filtret och vyn) och `2026-11-26_Automatiskt_lage.md`.
- **Bindande:**
  - API:t i avsnitt 8,
  - blockmallen och färgerna i avsnitt 9,
  - raderna i `.glazeui` i 10.4,
  - texterna i avsnitt 12.

  Namn, signaturer och texter följs ordagrant. Behöver ett paket ändra en signatur säger det till i rapporten, och samordnaren beslutar.

---

## 1. Beslut av användaren 2026-11-27

- **B1, linhöjden hör till spannet:** användaren räknar ut linhöjden per spann och skriver in den. Ett spann ligger mellan två stolpar, visas i båda grannblocken och lagras en gång. Varje spann har tre linhöjder: ytterfaser, mittfas och topplina.
- **B2, medelvärdet av intilliggande spann:** (spann bak + spann fram) / 2.
- **B3, linhöjd faslina för linberäkning:** (ytterfasernas medel + mittfasens medel) / 2.
- **B4, linhöjd faslina för beräkning av sammanslagning:** mittfasens spannvärden bak och fram, utan medling.
- **B5, Av:**
  - Av för mittfasen är Av Fas2 i Stolptabell efter.
  - Av för ytterfaserna är det största av Av Fas1 och Av Fas3 i Stolptabell efter.
  - Topplinans Av skriver användaren per stolpe, och den sparas i projektet.
- **B6, området:** fliken visar alltid bara det ändrade området ±1 rad (B14). Ändrat är samma märkning som filtret i Stolptabell efter (`poleview::filterMarks`). Inget filterval finns. Är inget ändrat visas inga block, bara en text.
- **B7, filtret blir ±1:** filtret i Stolptabell efter ändras från "Förändrade ±2" till "Förändrade ±1", med `kMargin`, alla texter, tester och dokumentation.
- **B8, lagring:** linhöjderna (spannvärdena per del och topplinans Av) sparas i projektdatabasen i ett nytt migreringssteg med migreringstest.
- **B9, layout:** hyfsat lik Excel-förlagan, med samma färger, rubriker och ordning. Lösningen följer GlazeUI och 3-filsmetoden, och en egen widget är tillåten. Fönstret ska gå att förhandsgranska.
- **B10, version:** 0.7.0.
- **B11, rena funktioner:** uträkningarna ligger som rena funktioner i ett eget bibliotek, med tester i `tests/`.
- **Fråga 3, spannen i kanten och tomma värden** (kan justeras efter handtestet):
  - (a) Blocken visas för det sammanhängande området ±1 rad. Spannen i ytterblocken går ut till grannraderna, alltså ±2 rader, så att varje block har båda sina spann och medlen blir rätt.
  - (b) Där tabellen tar slut finns bara ett spann, och medlet är då det enda värdet. Ett tomt spann ger ett tomt medel tills värdet finns, och när bara ett spann finns är medlet det enda ifyllda värdet.
- **Fråga 4, området:** ett sammanhängande område från den första till den sista märkta raden ±1, som filtret i Stolptabell efter.
- **Fråga 5, decimaler:**
  - Medlen (medelvärdet av intilliggande spann och linhöjd faslina för linberäkning) visas med två decimaler, till exempel 16,75.
  - Inmatade värden visas med de decimaler som skrevs.
  - Av visas som i stolptabellen.
- **Fråga 6, Linberäkningen:** ingen koppling till Linberäkningens fält "Linhöjd, snitt (m)" nu. Den läggs som 💡 i `TODO.md`.
- **Fråga 7, sammanslagningen:** värdena visas bara tills vidare, utan koppling till någon modul.
- **B12, bara Stolptabell efter:** "Vi gör efter Stolptabell efter, borttagna stolpar är borta. Det är ett arbete där stolpar kan försvinna, flytta eller tillkomma. Exakt som de står i Stolptabell efter. Aldrig FÖRE."
  - Blocken, spannen, Stolpnr och värdena tas ur raderna i Stolptabell efter. Linhöjderna hör till raderna där.
  - En borttagen rad tar sina värden med sig, och ett nytt grannpar börjar tomt.
  - Bara märkningen av vilka rader som är ändrade jämför med före (`poleview::filterMarks`), som filtret.
- **B13, namnet är inget id:** "Namnet är inte ID, det är ett namn. Två stolpar kan ha samma namn."
  - Stolpnr används aldrig som nyckel för ett spann eller för topplinans Av.
  - Dubbletter är tillåtna och låser ingenting. En tom Stolpnr låser inte heller.
  - Stolpnr visas som det står. En rad utan Stolpnr heter "rad {n}" i blockets texter (`poleview::rowLabel`).
- **B14, radernas ordning:** "Vi tar de förändrade stolparnas ordning ±2 st, oavsett vad det står där."
  - Området och grannarna räknas på radernas ordning i Stolptabell efter, aldrig på namn.
  - Tolkningen tills användaren säger annat: block för det sammanhängande området ±1 rad kring de märkta raderna (som filtret), och ytterblockens spann når grannraderna, alltså ±2 rader.
  - Varje rad i området som inte är helt tom får ett block, oavsett Stolpnr.
  - Marginalen för blocken är en egen konstant, `lineheight::kBlockMargin = 1`, som kan bli 2 efter handtestet (16, punkt 5, och Ö7).
- **Ö1, stolpbeteckningen, beslutad:** Stolpnr som det står i Stolptabell efter, överallt: "ST0379", "Linhöjd ST0379 ytterfaser" och "ST0378-ST0379".
- **Texterna:** rubrikerna och etiketterna i blocken ska vara ordagrant som i Excel-förlagan. Stolpnumret byts mot radens Stolpnr i Stolptabell efter.
  - Rubrikerna: "Linhöjd STP379 ytterfaser", "Linhöjd STP379 mittfas", "Linhöjd faslina för linberäkning", "Linhöjd STP379 Topplina" och "Linhöjd faslina för beräkning av sammanslagning".
  - Etiketterna: "meter", "medelvärdet av intilliggande spann" och "Av".

### Ändringar efter handtestet 2026-11-27

Dessa går före B7, K13, K18, avsnitt 9.2, 9.3 (färgtabellen) och 11 där de skiljer sig.

- **H1, filtret står kvar på ±2:** filtret i Stolptabell efter är Förändrade ±2 (`poleview::kMargin = 2`), som före 0.7.0. Då syns alla stolpar som fliken Linhöjder tar med: blocken ±1 och ytterblockens spann ±2. Blocken står kvar på `kBlockMargin = 1`.
- **H2, en tom rad mellan delarna:** den dubbla tomraden mellan "Linhöjd faslina för linberäkning" och Topplina blir en. Blocket är 19 rader (`kBlockRows = 19`, `kBlockStride = 20`), och mallen i 9.2 flyttar raderna 13–19 upp ett steg till 12–18.
- **H3, blockens bakgrund växlar:** blockets yta är stolptabellernas gula ton (220, 160, 40) med alfa 64 för jämna och 52 för udda block, lagd på vitt (`lineheight::blockFill`), så att det syns var ett nytt block börjar.
- **H4, "meter" och "Av" i delens färg:** stilarna `OuterMeter`/`MiddleMeter`/`TopMeter` och `OuterAvLabel`/`MiddleAvLabel`/`TopAvLabel` (i stället för `ColumnHeader` och `AvLabel`) har ytterfasernas `#FFE699`, mittfasens `#F8CBAD` och topplinans `#92D050`. Båda är feta; "meter" har kraftig ram och "Av" tunn.
- **H5, inmatningen är vit:** `Input` har fyllningen `#FFFFFF`, texten `#006100`.
- **H6, länkade och beräknade fält är gröna med glyf:** `Computed` (medlen och linhöjden för linberäkning) och den nya `Linked` (sammanslagningens värden, som hämtas ur mittfasens spann) har `#C5E0B4` och orange fet text. `AvValue` (Av ur Stolptabell efter) har `#C5E0B4` och svart text. `CellLook::mark` ger glyfen i cellens vänsterkant, som i Dokumentation: `Mark::Sum` (`StockIcon::Sum`) för beräknade och `Mark::Sync` (`StockIcon::Sync`) för länkade. Glyfen ritas med temats ljusa variant, eftersom bladet har Excel-färgerna också i mörkt läge.

---

## 2. Mål och avgränsning

### Användarens krav
| # | Krav | Var |
|---|---|---|
| A1 | En ny flik "Linhöjder" står mellan Stolptabell efter och Normalspann. | 10.4 |
| A2 | Ett block per rad som inte är helt tom i det ändrade området ±1, uppställt som förlagan. | 6.1, 9 |
| A3 | Linhöjderna skrivs per spann och del. Ett spann visas i båda grannblocken och lagras en gång, på radernas id. | 6.2, 7 |
| A4 | Medelvärdena, linhöjden för linberäkning och sammanslagningsvärdena räknas enligt B2–B4 och fråga 3. | 6.3–6.5 |
| A5 | Av visas för ytterfaser och mittfas, och topplinans Av skrivs per stolpe. | 6.6 |
| A6 | Allt sparas i projektet (steg 8). | 7 |
| A7 | Filtret i Stolptabell efter blir Förändrade ±1. | 11 |
| A8 | Fönstret går att förhandsgranska. | 10.5 |

### Ingår
- **Fliken Linhöjder** i huvudfönstret: rubrik, infotext, områdestext och linhöjdsbladet.
- **Ett nytt rent bibliotek, `libs/lineHeight`:** spannen, blocken, uträkningarna, den exakta decimalräkningen, bladets cellmodell, förflyttningen mellan cellerna, färgerna och texterna.
- **Ett nytt widgetbibliotek, `libs/lineHeightSheet`:** linhöjdsbladet som en egen GlazeUI-widget, ett OBJECT-bibliotek.
- **Radernas id i Stolptabell efter (7.1):** `PoleRow::id` och `assignRowIds` i `libs/poleTable`, `setRowIds` i `libs/poleGrid` och kolumnen `row_id` i `pole_after`.
- **Projektdatabasen:** steg 8 (`row_id`, `line_height_span`, `line_height_pole`), läsning, skrivning och städningen som håller linhöjderna lika med raderna i efter.
- **`libs/poleView`:** `kMargin = 1`, det gemensamma området `changedArea` och texterna.
- **Huvudfönstret:** texterna för filtret blir ±1.
- **Tester** i `tests/`.

### Ingår inte
- En koppling till Linberäkningens "Linhöjd, snitt (m)" (fråga 6, 💡 i `TODO.md`).
- Någon modul för sammanslagning (fråga 7).
- Linhöjder för Stolptabell före.
- Inklistring av flera celler, områdesmarkering och ångra i bladet.
- En snabbmeny i bladet, till exempel Gå till Stolptabell efter (💡).
- Id för raderna i Stolptabell före. `pole_before` får ingen kolumn, och `PoleRow::id` är alltid 0 där.
- Läget Excel och något PM (Ö4).

---

## 3. Förlagan (ref-arbetsboken)

Bladet "Data för beräkningar CL7 S1-3", kolumnerna P–X. Blocket STP 379 står på rad 38–57, STP 380 börjar på rad 59 och STP 381 på rad 80. Varje block är 20 rader följt av en tom rad, och blocken står under varandra.

### 3.1 Blocket rad för rad (relativ rad 0 = rad 38)
| Rad | Celler (sammanslagning) | Innehåll |
|---|---|---|
| 0 | P–X | "STP 379", grå `#BFBFBF`, fet, centrerad, kraftig ram |
| 1 | P–Q · S · T (rinner ut över U) | "Linhöjd STP379 ytterfaser" gul `#FFE699` fet · "meter" fet utan fyllning · "medelvärdet av intilliggande spann" fet, vänsterställd, utan fyllning |
| 2 | P–Q · S · T (rad 2–3) · V · W | "378-379" ljusgrön · 12,33 inmatning · `=SUM(S40:S41)/2` beräkning · "Av" · `=P27` (Av) |
| 3 | P–Q · S | "379-380" · 21,2 inmatning |
| 4 | | tom |
| 5 | P–Q · S · T | "Linhöjd STP379 mittfas" laxrosa `#F8CBAD` fet · "meter" · "medelvärdet av intilliggande spann" |
| 6 | P–Q · S · T (rad 6–7) · V · W | "378-379" · 12,33 · `=SUM(S44:S45)/2` · "Av" · `=W40` |
| 7 | P–Q · S | "379-380" · 21,2 |
| 8 | | tom |
| 9 | P–S | "Linhöjd faslina för linberäkning", blå `#00B0F0`, fet |
| 10 | P · Q–S | "STP379" ljusgrön · `=(T40+T44)/2` beräkning |
| 11–12 | | tomma |
| 13 | P–Q · S · T | "Linhöjd STP379 Topplina" grön `#92D050` fet · "meter" · "medelvärdet av intilliggande spann" |
| 14 | P–Q · S · T · V · W | "378-379" · 22,63 · `=SUM(S52:S53)/2` · "Av" · 265,9332 (inskrivet) |
| 15 | P–Q · S | "379-380" · 31,17 |
| 16 | | tom |
| 17 | P–S | "Linhöjd faslina för beräkning av sammanslagning", blå `#00B0F0`, fet |
| 18 | P · Q–S | "378-379" · `=S44` beräkning |
| 19 | P · Q–S | "379-380" · `=S45` beräkning |
| 20 | | tom, sedan nästa block |

### 3.2 Format
- **Färgerna** är räknade ur temat: ljusgrön `#C5E0B4` (Grön, dekorfärg 6, ljusare 60 %), gul `#FFE699` (Guld, dekorfärg 4, ljusare 60 %), laxrosa `#F8CBAD` (Orange, dekorfärg 2, ljusare 60 %) och grå `#BFBFBF` (Vit, mörkare 25 %).
- **Inmatningarna** har texten `#006100`, och talformatet 0.0 visar 12,33 som 12,3.
- **Beräkningscellerna** har fyllningen `#F2F2F2`, fet text `#FA7D00` och talformatet 0.0, så 16,765 visas som 16,8.
- **Topplinans avsnitt** har ljusgrön fyllning och svart text också på medlet och Av, utan beräkningsstil.
- **Ramarna:** rubrikcellerna har kraftig ram och datacellerna tunn, och hela blocket har en kraftig ram runt P–X.
- **Kolumnbredderna (tecken):** P 15,11 · Q 15,44 · R 13,66 · S 18,11 · T 25 · U 17 · V 13 · W 12,11 · X 17.

### 3.3 Iakttagelser (återges inte)
- Blocket STP 380 har kopierade rubriker: linberäkningsraden heter "STP381", och topplinan heter "Linhöjd STP187A Topplina". Kalkyl bygger rubrikerna ur Stolpnr.
- Blocket STP 380 räknar sammanslagningen som `=(S61+S65)/2`, alltså (ytter + mitt) / 2 per spann. Blocket STP 379 tar mittfasens värden. Kalkyl följer B4 (Ö4).
- Båda faserna tar Av ur Fas2 (`=P27`). Kalkyl följer B5.

---

## 4. Nuläge

- **Huvudfönstret:**
  - Flikarna adopteras i ordningen `byggPmSida`, `stolpSida`, `efterSida`, `nsSida` och `lcSida`.
  - `showPoleTab` väljer flik med `flikar.pageIndex(...)`, så en ny flik i mitten flyttar inget index.
  - `refreshAutomatic()` körs i `onLoad`, efter varje sparad ändring i stolptabellerna (`PoleHost::polesChanged`), efter Kopiera från Stolptabell före… och efter en import.
  - `saveClicked` och `onClosing` kör `commitEdits()`. Den automatiska sparningen gör det inte, med flit.
  - `PoleHost::poleRenamed(from, to)` körs efter en sparad cell i Stolpnr när både den gamla och den nya texten finns, och före `polesChanged`. Den används av sträckorna (`renameInRanges`) och rörs inte av den här planen.
  - `onLoad` skapar `afterGrid_` och slår på `setComparesWithBefore(true)` före `loadPoleTables`.
- **`libs/poleView`:**
  - `kMargin = 2`. `buildView` räknar området och regeln att en ensam rad visas.
  - `filterMarks` och `markedRange` är publika.
  - "±2" står i `pole_view.h`, `pole_view.cpp` (kommentarer och `hiddenToolTip`), `CMakeLists.txt` och `CLAUDE.md`.
- **Texter med ±2 eller "två rader" utanför poleView:**
  - `main_window.glazeui`: `efterInfo`, och posterna och tooltipen i `efterFilterDrop`;
  - kommentarer i `main_window.h` och `main_window.cpp`;
  - `libs/systemdb/system_db.h`, `system_db.cpp` och `CLAUDE.md`;
  - `libs/poleGrid/CLAUDE.md`, `libs/CLAUDE.md`, rotens `CLAUDE.md` och `src/windows/CLAUDE.md`;
  - `tests/poleview_test.cpp` och `tests/CLAUDE.md`.
  - De arkiverade planerna ändras inte.
- **`libs/projectdb`:**
  - Steg 1–7 är committade i cfa72f6 (0.6.0). `kSchemaVersion` är 7 i `project_db.h`, och schemat har 17 tabeller (`kExpectedTablesSql`, `kExpectedTableCount`).
  - Testerna räknar tabellerna med `kSeventeenTablesSql`.
  - `libs/projectdb/CLAUDE.md` säger felaktigt att steg 7 inte är committat.
  - Skrivningarna går genom `write(p, body)`, som kontrollerar skrivskyddet, kör en transaktion och anropar `markModified()`.
- **`libs/tableText`:**
  - `normalizeDecimal` ger kanonisk decimaltext.
  - `formatDecimalText(canonical, ',', group)` visar texten utan att gå via ett double.
  - `formatDecimal(double, 2)` avrundar det binära talet. 16,775 blir då 16,77, men Excel visar 16,78. Därför räknar planen exakt i decimaltext (6.7).
- **`libs/poleTable`:** `displayText`, `numberValue`, `isEmptyRow` och fälten `kAv`, `kAv2` och `kAv3`.
  - `PoleRow` har bara de tio fälten och ingen identitet. `isEmptyRow` prövar fälten med `field`, och `applyEdits` lägger till `PoleRow{}` bortom slutet.
- **Hur raderna i Stolptabell efter flödar i dag:**
  - `PoleGrid` håller `rows_` som `std::vector<PoleRow>`. Raderna skapas med `rows_.insert(…, PoleRow{})` (Infoga rad ovanför), `rows_.emplace_back()` (den tomma raden sist) och `pt::applyEdits` (inklistring och Delete bortom slutet), och tas bort med `erase` (Radera rader…) och `trimTrailingEmptyRows`.
  - Klipp ut är kopiera och tömma cellerna. Raden står kvar, tom. Inklistringen skriver fälten i befintliga eller nya rader. Ingen väg flyttar en `PoleRow`.
  - Urklippet skrivs per fält (`copyText`), och inklistringen läser per fält (`acceptField`).
  - Varje ändring sparas direkt med `save()` → `PoleGridHost::savePoles(rows_)` → `projectdb::setPoles`, som tömmer `pole_after` och skriver alla rader med `position` 0, 1, 2 … (`writePoleRows`). Raderna har alltså ingen beständig identitet utöver platsen.
  - `copyBeforeToAfter` ersätter `pole_after` med en kopia av `pole_before` i SQL, och `readPoles` läser i `position`-ordning.
  - "Kopiera från Stolptabell före" per cell (`restoreCell`) skriver ett fält i raden med `pt::applyEdits`.
- **GlazeUI v0.11.1:**
  - En egen widget ärver `Widget` eller `Panel` och registreras med `GLAZE_REGISTER_WIDGET(typnamn, Klass)`.
    - Makrot klistrar ihop klassnamnet till ett variabelnamn (`glazeWidgetReg_##Class`). Därför skrivs det med det okvalificerade namnet inne i namnrymden.
    - `typeid` ger det fulla namnet, som verktygen matchar mot medlemsraden.
  - Registreringen stryks av länkaren i ett statiskt bibliotek, så widgeten måste ligga i ett OBJECT-bibliotek. Med `GLAZE_TOOLS=ON` länkas det in i `glaze-render` och `GlazeBuilder` (`target_link_libraries(glaze-render PRIVATE <OBJECT-bibliotek>)`).
    - Utan det, till exempel i VS Code-tillägget, ritas widgeten som en ruta med klassnamnet.
  - `PropertyGrid` är en `Panel` där raderna är data, med ett eget, ägt textfält som flyttas till den aktiva cellen. Fältet placeras i logiska px i `childArea` och följer därmed rullningen.
    - `contentExtent` överskuggas så att rullningen täcker allt.
    - Fokusförlustens händelse postas, och `commitEdit()` nås av `AppWindow::commitEdits()`, som går från den fokuserade widgeten uppåt.
  - `setOffscreen(true)`, `renderFrame` och `captureFrame` ritar ett dolt fönster till pixlar i ett test, med returkoden 77 utan skärm.

---

## 5. Beslut med motivering

| # | Beslut | Motivering | Var |
|---|---|---|---|
| K1 | Fliken "Linhöjder" (`linSida`) adopteras mellan `efterSida` och `nsSida`. | A1 | 10.4 |
| K2 | Området är `poleview::changedArea(after.size(), filterMarks(before, after), margin)`: från den första märkta raden − margin till den sista + margin, klampat och räknat på radernas plats. Filtret använder samma funktion med `poleview::kMargin`, och fliken med `lineheight::kBlockMargin`. Regeln om en ensam rad gäller bara de grå raderna i tabellen. | B6, B14, fråga 4. En regel på ett ställe. | 6.1, 8.1 |
| K3 | Varje rad i området som inte är helt tom får ett block, oavsett Stolpnr. Helt tomma rader får inget block. | B13, B14 | 6.1 |
| K4 | Ett blocks grannar är närmaste rad ovanför och nedanför som inte är helt tom, också utanför området (±2). | Fråga 3a, B14 | 6.2 |
| K5 | Spannets nyckel är de två radernas id i stigande ordning (`a < b`). Topplinans Av har radens id som nyckel. Stolpnr ingår aldrig i en nyckel. | B1, B13. Ingen dubbellagring, och namnet är inget id. | 6.2, 7 |
| K6 | Ett spann går att skriva i när grannen finns och båda raderna har ett id. Dubbletter och tomma Stolpnr låser inget. Finns ingen granne (tabellens ände) visas raden tom och låst. | B13 | 6.2 |
| K7 | Medlet räknas enligt 6.3, och linberäkningen och sammanslagningen enligt 6.4 och 6.5. Allt räknas exakt i decimaltext. | B2–B4, fråga 3b | 6.3–6.7 |
| K8 | Medlen och linberäkningen visas med två decimaler, avrundade halvt bort från noll på det exakta värdet, som Excel visar dem. Inmatningar visas som de skrevs, och Av som i stolptabellen. | Fråga 5. Excel visar 16,775 som 16,78, men `formatDecimal` ger 16,77. | 6.7 |
| K9 | En linhöjd är ett tal ≥ 0 (komma eller punkt). Topplinans Av får vara negativ. Tom text tar bort värdet. Ogiltig text ger en text i statusraden, och värdet ändras inte. | En linhöjd är aldrig negativ, men Av kan vara det. | 6.8 |
| K10 | Lagringen är steg 8 med `row_id` i `pole_after`, `line_height_span` och `line_height_pole`. Varje ändring skrivs direkt i arbetskopian, som stolptabellerna. | B8 | 7 |
| K11 | Linhöjderna följer raderna i Stolptabell efter exakt: spann vars rader inte längre är grannar och topplinans Av för rader som inte längre finns eller är helt tomma tas bort i samma transaktion som ändringen i efter. Ett nytt Stolpnr på en rad ändrar inget. | B12, B13 | 7.6, 7.7 |
| K12 | Texterna är ordagrant som i förlagan, och "STP379" i rubrikerna byts mot radens Stolpnr exakt som det står i Stolptabell efter, eller "rad {n}" när det är tomt. Spannen heter "{bak}-{fram}" med radernas namn. | B12, B13, Ö1 | 9, 12 |
| K13 | Layouten följer förlagan: blocken under varandra, 20 rader och en tom rad, kolumnerna P–X med Excels bredder (gånger siffrornas bredd), sammanslagna celler, färger och ramar. | B9 | 9 |
| K14 | Bladet är en egen widget, `LineHeightSheet`, som en `Panel` i `PropertyGrid`s form, i OBJECT-biblioteket `libs/lineHeightSheet`. DataGrid är reserv. | Utvärderingen i 10.1 | 10 |
| K15 | Redigeringen: ett ägt textfält över den aktuella inmatningscellen. Tab, Enter och pilarna sparar och flyttar, och Escape avbryter. Fokusförlusten sparar (postat), och `commitEdit()` sparar direkt. | Som Excel och `PropertyGrid` | 10.3 |
| K16 | Skrivskyddat syns värdena, textfältet är skrivskyddat och inget sparas. | Som stolptabellerna | 10.3 |
| K17 | Uträknade värden har beräkningsstilen och inmatningar inmatningsstilen, också i topplinans avsnitt. Bladet behåller Excel-färgerna i mörkt läge. | Tydligt vad som går att skriva i (Ö5) | 9.3 |
| K18 | Filtret blir Förändrade ±1, med `kMargin = 1` och texterna i 11. Värdena i systemdatabasen (`changed`, `all`) är oförändrade. | B7 | 11 |
| K19 | Ingen koppling till Linberäkningen eller Sammanslagning. Inget läge Excel och inget PM. | Fråga 6 och 7, Ö4 | 2 |
| K20 | Varje rad i Stolptabell efter har ett stabilt id (`PoleRow::id`, `pole_after.row_id`) som följer raden vid redigering, inklistring, infogning och radering, men aldrig hamnar i urklippet. `PoleGrid` ger nya rader id före sparningen. | B12, B13. Utvärderingen i 7.1 | 7.1, 8.5, 8.6 |
| K21 | Kopiera från Stolptabell före… ersätter raderna i efter med nya rader och tar bort alla linhöjder i samma transaktion. Frågan säger det när det finns linhöjder. | B12: en ny tabell har nya rader (Ö6) | 7.6, 12.6 |
| K22 | Marginalen för blocken är `lineheight::kBlockMargin = 1`, skild från filtrets `poleview::kMargin`. | B14: kan bli 2 efter handtestet | 6.1, 8.2 |

---

## 6. Uträkningar och regler (`libs/lineHeight`)

### 6.1 Området och blocken
1. `marks = poleview::filterMarks(before, after)` och `area = poleview::changedArea(int(after.size()), marks, kBlockMargin)`.
   - Märkningen är filtrets och jämför med före på Stolpnr. Allt annat i fliken tas ur raderna i efter (B12).
2. `area.first < 0` ger inga block.
3. Block skapas i tabellens ordning för varje rad `area.first..area.last` som inte är helt tom (`poletable::isEmptyRow`), oavsett Stolpnr. Dubbletter och tomma Stolpnr får block som alla andra (B13).
4. **Blockets namn** (`PoleBlock::label`) är `poleview::rowLabel(after, m)`: den trimmade Stolpnr, eller "rad {m + 1}" när den är tom.
5. **Exempel:** STO100–STO900 där bara STO450 (rad 350) är ändrad ger området 349–351 och blocken STO449, STO450 och STO451.
6. **Två ändringar långt isär** ger ett enda område med allt emellan (fråga 4).
7. **En borttagen stolpe** märker raderna närmast platsen (`removedAnchors`), och området blir dem ±1.
8. **En helt tom rad** är märkt (`RowState::Empty`), så raderna runt den får block. Den själv får inget.
9. **Är Stolptabell före tom** är alla rader nya, och alla rader får block (risk 7).
10. **Marginalen** `kBlockMargin` är 1 och kan bli 2 efter handtestet. Då blir området ±2 och ytterblockens spann ±3. Texterna i 12.1 och 12.2 följer med (12.2 räknar ut ordet ur konstanten).

### 6.2 Grannar, spann och nyckel
- **Grannarna:** `back` är närmaste rad ovanför blockets rad som inte är helt tom (`poletable::isEmptyRow`), och `ahead` närmaste nedanför. Grannen får ligga utanför området (fråga 3a). Platsen räknas, aldrig namnet (B14).
- **`SpanRef::exists`** är sant när grannen finns.
- **`label`** är `"{bak}-{fram}"` i tabellens ordning med radernas namn (`rowLabel`), till exempel "ST0378-ST0379" eller "rad 6-ST0380", och tom när spannet inte finns.
- **`key`** är `spanKey(blockets id, grannens id)`, alltså de två radernas `PoleRow::id` i stigande ordning.
- **`usable`** är sant när grannen finns och båda raderna har ett id (> 0). Annars är `issue` en av texterna i 12.4.
  - Ett id 0 förekommer bara för en rad som ännu inte har sparats, och `PoleGrid` sparar direkt. Det är ett skydd, inget vanligt läge.
- **`heights`** är de sparade värdena för `key` (tomma när inget är sparat eller när spannet inte är användbart).
- **Topplinans Av** har blockets id som nyckel och går att skriva i när id:t är > 0.
- **Dubbletter:** två rader med samma Stolpnr är två block med var sitt id och var sina spann. Inget låses.

### 6.3 Medelvärdet av intilliggande spann (`spanMean`), per del
| Läge | Medel |
|---|---|
| Båda spannen finns och båda har värden | `halfSum(bak, fram)` |
| Båda spannen finns och minst ett är tomt | tom |
| Bara ett spann finns (stolpen är först eller sist bland raderna som inte är helt tomma) | det spannets värde, eller tom |
| Inget spann finns | tom |

Ett spann som finns men är låst räknas som tomt.

### 6.4 Linhöjd faslina för linberäkning (`lineCalcHeight`)
- `halfSum(outerMean, middleMean)` när båda finns, annars tom.
- Räkningen görs på de exakta medlen, inte på de avrundade. Excel räknar likadant.

### 6.5 Linhöjd faslina för beräkning av sammanslagning
- Mittfasens linhöjd i bakre och främre spannet, visad som den skrevs.
- Finns inte spannet är både etiketten och värdet tomma.

### 6.6 Av
- **Ytterfaserna** (`outerAvField`):
  - Det fält av `kAv` (Av Fas1) och `kAv3` (Av Fas3) som är störst som tal (`poletable::numberValue`).
  - Lika tal ger `kAv`, och ett tomt fält ger det andra. Är båda tomma blir svaret -1 och texten tom.
  - Texten är `poletable::displayText(rad, fält)`.
- **Mittfasen:** `displayText(rad, kAv2)`.
- **Topplinan:**
  - Den sparade texten för blockets rad-id, visad med `tabletext::formatDecimalText(text, ',', true)`.
  - Redigeringstexten är `formatDecimalText(text, ',', false)`.

### 6.7 Exakt räkning och avrundning
- **`halfSum(a, b)`** räknar (a + b) / 2 för två icke-negativa kanoniska decimaltexter med siffraritmetik på strängar:
  - decimalerna jämkas, siffrorna adderas, summan multipliceras med 5 och får en decimal till, och avslutande nollor tas bort;
  - ingen väg via double, och inget spill;
  - tom text när någon av dem är tom.
- **`roundedText(text, d)`** avrundar den kanoniska texten till `d` decimaler, halvt bort från noll, och skriver den med decimalkomma utan gruppering. Tom text ger tom text.
- **Exempel:**

| Indata | Exakt | Visas |
|---|---|---|
| 12,33 och 21,2 | 16,765 | 16,77 |
| 12,3 och 21,2 | 16,75 | 16,75 |
| 12,3 och 21,25 | 16,775 | 16,78 |
| 22,63 och 31,17 | 26,9 | 26,90 |
| linberäkning 16,765 och 16,775 | 16,77 | 16,77 |
| ett enda spann 10 | 10 | 10,00 |
| 9,995 (avrundning) | 9,995 | 10,00 |

### 6.8 Inmatning
- **`acceptHeight(text)`** är `tabletext::normalizeDecimal(text)` men vägrar negativa tal. Tom text ger "", och ett tomt svar betyder att texten inte är ett tal eller är negativ.
- **`acceptAv(text)`** är `normalizeDecimal(text)`.
- **Visning:**
  - En inmatad linhöjd visas med `formatDecimalText(text, ',', false)`, så 12.33 visas "12,33" och 7 visas "7".
  - Medlen visas med `roundedText(text, kMeanDecimals)`, där `kMeanDecimals = 2`.

### 6.9 Användarens exempel (förlagans STP 379 med syntetiska Stolpnr, rad-id 1–12 för ST0374–ST0385)
| Spann | Nyckel | Ytterfaser | Mittfas | Topplina |
|---|---|---|---|---|
| ST0378-ST0379 | {5, 6} | 12,33 | 12,33 | 22,63 |
| ST0379-ST0380 | {6, 7} | 21,2 | 21,2 | 31,17 |

Blocket ST0379 (id 6) med Av Fas1 273,29, Av Fas2 273,29, Av Fas3 270,1 och topplinans Av 265,9332:
- ytterfasernas medel visas 16,77 och mittfasens medel 16,77;
- linberäkningen visas 16,77, topplinans medel 26,90 och sammanslagningen 12,33 och 21,2;
- Av för ytterfaserna och mittfasen är 273,29, och topplinans Av 265,9332.

---

## 7. Datamodell och lagring

### 7.1 Radernas identitet (utvärdering)
Linhöjderna behöver en nyckel för raderna i Stolptabell efter som överlever redigering, inklistring, infogning och radering i `PoleGrid` men inte bygger på Stolpnr (B13). I dag har raderna ingen sådan nyckel (4, "Hur raderna i Stolptabell efter flödar").

**Kraven:**
- En raderad rad tar sina värden med sig.
- En ny stolpe mellan två ger två nya, tomma spann.
- En flyttad stolpe (klipp ut och klistra in) får nya spann.
- Ett nytt Stolpnr, dubbletter och tomma Stolpnr ändrar ingenting.
- Excel-inklistring över befintliga rader ska ge ett förutsägbart svar.
- Id:t får aldrig hamna i urklippet.
- Kopiera från Stolptabell före… och migreringen från 0.6.0 ska ha ett klart svar.

| Kriterium | A: rad-id i `PoleRow` och `pole_after` (rekommenderas) | B: linhöjderna som dolda fält i `PoleRow` (spannet fram och topplinans Av) | C: radens plats, med en logg över ändringarna från `PoleGrid` | D: koordinaterna N och E som nyckel |
|---|---|---|---|---|
| Radera | Id:t försvinner, och städningen (7.6) tar värdena | Raden tar sina fält med sig, men raden ovanför har kvar "spannet fram", som nu gäller en ny granne. `PoleGrid` måste tömma det | Alla platser efter raden flyttas, och varje väg måste logga det | Rätt |
| Infoga mellan | Den nya raden får ett nytt id, och nycklarna mot den är nya | Raden ovanför behåller spannet fram mot fel granne | Som radera | Rätt |
| Flytta (klipp och klistra) | Källraden blir tom och tappar sina spann. Målraden har sitt eget id och får nya spann | De dolda fälten stannar i den tömda raden och kommer tillbaka om den fylls igen | Syns inte som en flytt | Rätt |
| Stolpnr, dubbletter, tomma | Rör inte id:t | Rätt | Rätt | Rätt, men samma koordinater två gånger krockar, och nya stolpar saknar ofta koordinater |
| Inklistring över rader | Fälten skrivs och id:t står kvar, så värdena stannar på raden | Som A | Som A | En rättad koordinat tappar värdena |
| Urklipp | Id:t är inget fält och nås inte av `field`, `copyText` eller `acceptField` | Fälten är inga kolumner, men följer med varje kopia av en `PoleRow` | Inget id | Inget id |
| Lagring | En kolumn i `pole_after` och två små tabeller | Fyra kolumner till i `pole_after`, och varje linhöjd sparar hela tabellen | Platserna skrivs om vid varje ändring | Textpar som nyckel |
| Arbete och risk | Litet: `PoleRow::id`, `assignRowIds`, `PoleGrid::setRowIds` och städningen. `SheetGrid` rörs inte | Sex vägar i `PoleGrid` måste tömma rätt fält | Alla vägar måste logga, och en missad väg flyttar värden till fel spann | Litet, men skört i praktiken |

- **Rekommendation och val: A.** Id:t följer `PoleRow`-objektet genom `insert`, `erase`, `applyEdits` och `trimTrailingEmptyRows` utan särskild kod, eftersom ingen väg i `PoleGrid` kopierar eller flyttar en rad. Dessutom ger en väg som glöms bort inga fel värden, bara ett nytt, tomt spann (risk 6). B och C ger fel värden när en väg glöms, och D tappar värden när en koordinat rättas och kan inte skilja stolpar utan koordinater.

### 7.2 Radernas id, reglerna
- **`PoleRow::id`** (`std::int64_t`, 0 = inget id än) står sist i `PoleRow` och är inget fält. `field`, `kFieldCount`, `isEmptyRow`, `acceptField`, `displayText`, `copyText`, `firstInvalidColumn` och `applyEdits` rör det aldrig.
  - Urklippet, inklistringen och jämförelsen med före ser det därför aldrig.
  - `applyEdits` skriver fälten i raderna och står för de nya raderna bortom slutet (`PoleRow{}`, id 0).
- **I Stolptabell efter** är id:t > 0 och unikt i tabellen. **I Stolptabell före** är det alltid 0, och `pole_before` får ingen kolumn.
- **Tilldelningen** är `poletable::assignRowIds(rows)`: varje rad med id ≤ 0, eller med ett id som redan stått tidigare i listan, får nästa lediga id (högsta id + 1) i tabellens ordning. Inget annat ändras.
- **`PoleGrid::setRowIds(true)`** sätts bara på `afterGrid_` i `onLoad`. Då kör `save()` `assignRowIds(rows_)` först, före jämförelsen med det senast sparade och före `savePoles`. Raderna i fönstret och i projektet har därmed samma id.
- **`setPoles(p, Stage::After, rows)`** kör `assignRowIds` på en kopia innan den skriver, som skydd för anropare som inte ger id (till exempel testerna). Id:na läses tillbaka med `poles`.
- **Återanvändning:** raderas raden med det högsta id:t kan id:t delas ut igen. Det är säkert, eftersom städningen tar bort id:ts linhöjder i samma transaktion som raderingen (7.6).
- **Vägarna i `PoleGrid`:**
  - Infoga rad ovanför, den tomma raden sist och inklistring bortom slutet ger rader med id 0, som får id vid sparningen direkt efter.
  - Radera rader… och papperskorgen tar bort raden och id:t.
  - Delete och Klipp ut tömmer fälten. Raden står kvar, tom, med sitt id.
  - Inklistringen (också över grå rader) och Kopiera från Stolptabell före per cell skriver fälten. Id:t står kvar.
  - `setRows` och omläsningen efter ett fel tar id:na ur projektet.

### 7.3 Steg 8 (`dev::kStep8` sist i `libs/projectdb/dev_migration.h`)
```sql
-- Radernas id i Stolptabell efter: ett tal > 0, unikt i tabellen, som följer raden när rader
-- infogas, raderas och ändras, och som linhöjderna hänger på. Det är inget fält i tabellen utan
-- skrivs av Kalkyl (poletable::assignRowIds). De befintliga raderna får position + 1.
ALTER TABLE pole_after ADD COLUMN row_id INTEGER NOT NULL DEFAULT 0;
UPDATE pole_after SET row_id = position + 1;
CREATE UNIQUE INDEX IF NOT EXISTS pole_after_row_id ON pole_after(row_id);
-- Linhöjderna i fliken Linhöjder: en rad per spann mellan två grannrader i Stolptabell efter.
-- row_a och row_b är radernas row_id med row_a < row_b. Värdena är kanonisk decimaltext och NULL
-- när de inte är ifyllda. En rad utan värden tas bort. Ett spann vars rader inte längre är
-- grannar tas bort i samma transaktion som ändringen i efter (pruneLineHeights).
CREATE TABLE IF NOT EXISTS line_height_span (
    row_a    INTEGER NOT NULL CHECK (row_a > 0),
    row_b    INTEGER NOT NULL,
    outer_m  TEXT,
    middle_m TEXT,
    top_m    TEXT,
    PRIMARY KEY (row_a, row_b),
    CHECK (row_a < row_b)
) WITHOUT ROWID;
-- Topplinans Av per rad i Stolptabell efter (row_id), kanonisk decimaltext skriven av användaren.
CREATE TABLE IF NOT EXISTS line_height_pole (
    row_id INTEGER PRIMARY KEY CHECK (row_id > 0),
    top_av TEXT NOT NULL
);
```
- `project_db.cpp`: `s.steps` får `dev::kStep8`, `kExpectedTablesSql` får `'line_height_span'` och `'line_height_pole'`, och `kExpectedTableCount` blir 19. Indexet räknas inte.
- `project_db.h`: `kSchemaVersion = 8`.
- `ADD COLUMN` får ingen CHECK, eftersom SQLite prövar den mot de befintliga raderna innan `UPDATE` har gett dem id. Det unika indexet och `assignRowIds` håller id:na > 0 och unika.
- Steg 8 kan ändras fritt tills det committas (rotens CLAUDE.md).

### 7.4 Migreringen
- En fil från 0.6.0 (user_version 7) får `row_id` = position + 1 på varje rad i `pole_after`, det unika indexet och de två tomma tabellerna. Allt annat är orört. Inga startdata behövs.
- Positionerna är 0, 1, 2 … (`writePoleRows`), och `position` är primärnyckel, så id:na blir unika också om en äldre fil skulle ha luckor.
- Testet `testMigrateFromStep7` står i 13.3.

### 7.5 API:t (`project_data.h`, avsnitt 8.3)
- **`poles(p, Stage::After)`** ger `PoleRow::id` ur `row_id`. Före ger alltid 0.
- **`setPoles(p, Stage::After, rows)`:** talen valideras, en kopia får id (`assignRowIds`), `writePoleRows` skriver `row_id`, och sedan körs `pruneLineHeights` och `syncAutoCalcs`, allt i EN transaktion.
- **`copyBeforeToAfter`** skriver `row_id = position + 1` och tömmer `line_height_span` och `line_height_pole` i samma transaktion (K21).
- **`lineHeights(p)`** läser båda tabellerna till `lineheight::StoredHeights`. NULL blir tom text.
- **`setSpanHeight(p, key, part, value)`:**
  - Kontrollerna, i ordning:
    - `isValidKey(key)`, annars Invalid "Spannet för linhöjden är ogiltigt.";
    - `lineheight::acceptHeight(value) == value`, annars Invalid "Linhöjden är inget giltigt tal.";
    - nyckeln finns i `lineheight::afterSpans(readPoles(d, Stage::After))`, annars NotFound "Spannet finns inte längre i Stolptabell efter.".
  - Raden skrivs med `INSERT … ON CONFLICT(row_a, row_b) DO UPDATE SET <kolumn> = excluded.<kolumn>`, och en tom text blir NULL. Sedan körs `DELETE` om alla tre är NULL.
- **`setTopAv(p, row, value)`:**
  - Kontrollerna, i ordning:
    - `row > 0`, annars Invalid "Raden för topplinans Av är ogiltig.";
    - `lineheight::acceptAv(value) == value`, annars Invalid "Topplinans Av är inget giltigt tal.";
    - raden finns i efter och är inte helt tom, annars NotFound "Raden finns inte längre i Stolptabell efter.".
  - En tom text tar bort raden.
- Båda går genom `write(p, …)`: AccessDenied skrivskyddat, en transaktion och `markModified()`. Ingen synkning av de automatiska beräkningarna behövs, eftersom den inte läser tabellerna.
- Kontrollerna mot efter gör att en postad sparning från bladet aldrig skriver ett spann som hunnit försvinna.

### 7.6 Linhöjderna följer raderna i Stolptabell efter (`pruneLineHeights`, B12)
Linhöjderna gäller exakt raderna i Stolptabell efter, aldrig före. Det som inte längre finns i efter tas bort.
1. **Var:** en intern funktion `pruneLineHeights(db::Database& d)` i `project_data.cpp` körs i samma transaktion som `setPoles(p, Stage::After, …)`, efter `writePoleRows`. Ordningen mot `syncAutoCalcs` spelar ingen roll. `copyBeforeToAfter` tömmer tabellerna i stället.
2. **Vad:** raderna i efter (`readPoles(d, Stage::After)`) och det sparade läses, och `lineheight::keptHeights(stored, rows)` ger det som får stå kvar:
   - spannen vars nyckel finns i `afterSpans(rows)`, alltså två grannrader enligt 6.2 med id > 0;
   - topplinans Av för rader som finns och inte är helt tomma.
3. **Resten tas bort** med `DELETE`. Städningen skriver aldrig om ett värde.
4. **Följden:** efter varje skrivning hör allt i `line_height_*` till spann och rader som finns i efter. Värden för spann utanför området står kvar och syns igen när området når dem.
5. **Ingen namnföljning behövs:** ett nytt Stolpnr ändrar inte id:t. Det finns ingen `renameLineHeightPole`, ingen `renameLineHeights` och ingen namnbaserad städning.

### 7.7 Vad som händer vid varje ändring
| Ändring | Id | Linhöjderna |
|---|---|---|
| Nytt Stolpnr eller annat fält i en cell (också inklistrat) | står kvar | står kvar; rubrikerna visar det nya namnet |
| Två rader får samma Stolpnr, eller en rad blir utan Stolpnr | står kvar | står kvar och går att skriva i |
| Radera rader… eller papperskorgen | raden och id:t försvinner | radens spann och topplinans Av tas bort; grannarna runt den blir ett nytt, tomt spann |
| Infoga rad ovanför | den tomma raden får ett nytt id | inget ändras medan raden är tom: den är ingen stolpe, och spannet förbi den står kvar |
| Skriva i den infogade raden | står kvar | spannet förbi raden tas bort; två nya, tomma spann |
| Delete eller Klipp ut på hela raden | raden står kvar, tom, med sitt id | radens spann och topplinans Av tas bort; grannarna blir ett nytt, tomt spann |
| Flytta: klipp ut och klistra in på en annan rad | källraden töms, målraden har sitt eget eller ett nytt id | källradens värden tas bort; nya, tomma spann på den nya platsen |
| Excel-inklistring över befintliga rader | står kvar | står kvar på raderna, eftersom raden är densamma och bara fälten är nya (risk 4) |
| Ny stolpe under tabellen (inklistring eller den tomma raden) | nytt id | ett nytt, tomt spann mot den förra sista raden |
| Kopiera från Stolptabell före (snabbmenyn, en cell) | står kvar | står kvar |
| Kopiera från Stolptabell före… (knappen) | nya id, position + 1 | alla tas bort (K21, Ö6) |
| Stolptabell före, importen, sträckorna och läget | rörs inte | rörs inte; bara märkningen och därmed området kan ändras |

### 7.8 Det som inte ändras
- `importPolesBefore`, `setPolesBefore` och `setAutoSettings` rör aldrig linhöjderna eller `row_id`.
- `pole_before` och `PoleRow::id` i före är oförändrade (0).
- `PoleGridHost::poleRenamed` och `renameInRanges` (sträckorna) är oförändrade.

---

## 8. API (bindande)

### 8.1 `libs/poleView/pole_view.h`, ändringar
```cpp
// Filtret i Stolptabell efter. Changed är Förändrade ±1: de förändrade raderna och kMargin rader
// runt dem syns, resten står ihop i grå rader. All visar alla rader.
enum class Filter { Changed, All };

// Antalet rader ovanför och nedanför en förändrad rad som också syns (±1).
inline constexpr int kMargin = 1;

// Det ändrade området: från första sanna posten bland de count första i marks minus margin till
// sista plus margin, räknat på radernas plats och klampat till 0..count-1. {-1, -1} utan sann
// post. buildView använder kMargin och fliken Linhöjder lineheight::kBlockMargin.
MarkedRange changedArea(int count, const std::vector<bool>& marks, int margin);
```
- `buildView` använder `changedArea(count, marks, kMargin)`. Regeln att en ensam rad före eller efter området visas som `Pole` står kvar.
- `hiddenToolTip` får texten i 12.7.
- Kommentarerna som säger ±2 eller "två rader" ändras till ±1 och "en rad".

### 8.2 `libs/lineHeight/line_height.h` (nytt bibliotek `kalkyl_lineheight`, namnrymden `kalkyl::lineheight`)
```cpp
#pragma once
#include <array>
#include <cstdint>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "poleTable/pole_table.h"

namespace kalkyl::lineheight {

// Linhöjdens del: ytterfaserna, mittfasen eller topplinan. Varje spann har en linhöjd per del.
enum class Part { Outer, Middle, Top };

// Antalet rader före och efter de ändrade raderna som får block (±1, planen B14). Skild från
// filtrets poleview::kMargin, så att den kan prövas med 2 i handtestet.
inline constexpr int kBlockMargin = 1;

// Ett spann mellan två grannrader i Stolptabell efter: radernas id (poletable::PoleRow::id) med
// a < b. Stolpnr ingår aldrig. Lagras så i projektet.
struct SpanKey {
    std::int64_t a = 0;
    std::int64_t b = 0;
    auto operator<=>(const SpanKey&) const = default;
};

// Nyckeln för spannet mellan raderna med id x och y, ordnade.
SpanKey spanKey(std::int64_t x, std::int64_t y);

// Sant när 0 < a < b.
bool isValidKey(const SpanKey& key);

// Ett spanns tre linhöjder som kanonisk decimaltext. Tom text betyder att värdet inte är ifyllt.
struct SpanHeights {
    std::string outer;
    std::string middle;
    std::string top;
    bool operator==(const SpanHeights&) const = default;
};

// Delens linhöjd i spannet.
const std::string& heightOf(const SpanHeights& heights, Part part);
std::string& heightOf(SpanHeights& heights, Part part);

// Det som är sparat i projektet: linhöjderna per spann och topplinans Av per rad (radens id till
// kanonisk decimaltext). projectdb håller det lika med raderna i Stolptabell efter (keptHeights).
struct StoredHeights {
    std::map<SpanKey, SpanHeights> spans;
    std::map<std::int64_t, std::string> topAv;
    bool operator==(const StoredHeights&) const = default;
};

// Spannen i Stolptabell efter: nyckeln för varje par av grannrader som inte är helt tomma
// (planen 6.2), i tabellens ordning. Ett par där en rad saknar id (0) ger inget spann.
std::vector<SpanKey> afterSpans(const std::vector<poletable::PoleRow>& after);

// Det av stored som hör till raderna i after (planen 7.6): spannen i afterSpans och topplinans Av
// för rader som finns och inte är helt tomma. projectdb tar bort resten.
StoredHeights keptHeights(const StoredHeights& stored, const std::vector<poletable::PoleRow>& after);

// En linhöjd ur inmatad text som kanonisk decimaltext (komma eller punkt). "" för tom text. Tomt
// svar när texten inte är ett tal eller talet är negativt.
std::optional<std::string> acceptHeight(std::string_view text);

// Topplinans Av ur inmatad text: som acceptHeight, men negativa tal är tillåtna.
std::optional<std::string> acceptAv(std::string_view text);

// Exakt (a + b) / 2 av två icke-negativa kanoniska decimaltexter, räknat på siffrorna utan väg
// via double och utan avslutande nollor. Tom text när någon av dem är tom.
std::string halfSum(std::string_view a, std::string_view b);

// Kanonisk decimaltext avrundad till decimals decimaler, halva bort från noll, med decimalkomma
// och utan gruppering: "16.765" ger "16,77". Tom text ger tom text.
std::string roundedText(std::string_view canonical, int decimals);

// Antalet decimaler i medelvärdena och i linhöjden för linberäkning.
inline constexpr int kMeanDecimals = 2;

// Ett spann sett från ett block: om grannen finns, om spannet går att skriva i, grannens rad
// (platsen i after), etiketten i tabellens ordning med radernas namn ("ST0378-ST0379"), nyckeln,
// de sparade värdena och varför spannet är låst (tooltip).
struct SpanRef {
    bool exists = false;
    bool usable = false;
    int neighbourRow = -1;
    std::string label;
    SpanKey key;
    SpanHeights heights;
    std::string issue;
    bool operator==(const SpanRef&) const = default;
};

// Ett block i fliken: raden (platsen i after och id:t), Stolpnr som det står (trimmat, kan vara
// tomt), radens namn i texterna (poleview::rowLabel), dess två spann, medelvärdena och linhöjden
// för linberäkning som exakt kanonisk text (tom när de inte går att räkna), Av per fas som i
// stolptabellen och topplinans Av som den sparats.
struct PoleBlock {
    int row = -1;
    std::int64_t id = 0;
    std::string pole;
    std::string label;
    SpanRef back;
    SpanRef ahead;
    std::string outerMean;
    std::string middleMean;
    std::string topMean;
    std::string lineCalc;
    std::string av1;
    std::string av2;
    std::string av3;
    std::string outerAv;
    std::string topAv;
    bool operator==(const PoleBlock&) const = default;
};

// Medelvärdet av intilliggande spann för en del (planen 6.3): båda spannen finns och har värden
// ger (bak + fram) / 2, bara ett spann ger dess värde, annars tom text.
std::string spanMean(const SpanRef& back, const SpanRef& ahead, Part part);

// Linhöjd faslina för linberäkning: (ytterfasernas medel + mittfasens medel) / 2, tom när något
// av dem saknas.
std::string lineCalcHeight(std::string_view outerMean, std::string_view middleMean);

// Fältet för ytterfasernas Av: kAv eller kAv3, det som är störst som tal (kAv vid lika, det andra
// när ett är tomt). -1 när båda är tomma.
int outerAvField(const poletable::PoleRow& row);

// Blocken för fliken (planen 6.1 och 6.2): raderna som inte är helt tomma i det ändrade området
// ±kBlockMargin, oavsett Stolpnr, med spannen till grannarna, de sparade värdena, medelvärdena och
// Av. Tom lista när inget är ändrat.
std::vector<PoleBlock> buildBlocks(const std::vector<poletable::PoleRow>& before,
                                   const std::vector<poletable::PoleRow>& after, const StoredHeights& stored);

// Bladets mått i celler (H2 ändrade till 19 och 20): kolumnerna P–X i förlagan, 20 rader per block och ett tomt varv emellan.
inline constexpr int kSheetColumns = 9;
inline constexpr int kBlockRows = 20;
inline constexpr int kBlockStride = 21;

// Kolumnbredderna i förlagan i tecken (Excels mått). Bladet ritar dem gånger siffrans bredd.
inline constexpr std::array<float, kSheetColumns> kColumnChars = {15.11f, 15.44f, 13.66f, 18.11f, 25.0f,
                                                                  17.0f, 13.0f, 12.11f, 17.0f};

// Cellens stil, som ger färg, fet stil, justering och ram (cellLook).
enum class CellStyle {
    PoleHeader, OuterHeader, MiddleHeader, LineCalcHeader, TopHeader, MergeHeader,
    ColumnHeader, MeanHeader, Label, Input, Locked, Computed, AvLabel, AvValue
};

// Vad en inmatningscell skriver: en del av ett spann, eller topplinans Av för raden med id row.
// Kind None för alla andra celler, också låsta.
struct InputRef {
    enum class Kind { None, Span, TopAv };
    Kind kind = Kind::None;
    SpanKey span;
    Part part = Part::Outer;
    std::int64_t row = 0;
    bool operator==(const InputRef&) const = default;
};

// En cell i bladet: blocket, raden och kolumnen i bladet (0-baserade, kolumn 0 är P), hur många
// rader och kolumner den täcker, stilen, texten, redigeringstexten (decimalkomma utan gruppering,
// bara för inmatning), tooltipen och vad den skriver.
struct SheetCell {
    int block = 0;
    int row = 0;
    int column = 0;
    int rows = 1;
    int columns = 1;
    CellStyle style = CellStyle::Label;
    std::string text;
    std::string editText;
    std::string toolTip;
    InputRef input;
    bool operator==(const SheetCell&) const = default;
};

// Bladet: cellerna block för block i rad- och kolumnordning, antalet rader, första cellen och
// radens id per block, och inmatningscellerna i läsordning (Tab).
struct Sheet {
    std::vector<SheetCell> cells;
    int rows = 0;
    std::vector<int> blockStart;
    std::vector<std::int64_t> blockRows;
    std::vector<int> inputs;
    bool operator==(const Sheet&) const = default;
};

// Bladet för blocken enligt blockmallen (planen 9.2), med texterna i planen 12.
Sheet buildSheet(const std::vector<PoleBlock>& blocks);

// Cellen som täcker raden och kolumnen i bladet, också inne i en sammanslagen cell, eller -1.
int cellAt(const Sheet& sheet, int row, int column);

// Förflyttningen mellan inmatningscellerna: Next och Previous i läsordning (Tab och Shift+Tab),
// Down och Up till närmaste inmatningscell i samma kolumn nedanför och ovanför, också i nästa
// block. Svarar med cellen, eller from när det inte finns någon åt det hållet.
enum class Move { Next, Previous, Down, Up };
int moveInput(const Sheet& sheet, int from, Move move);

// Inmatningscellen i blocket för raden med id blockRow som skriver input, eller -1. Två block med
// samma Stolpnr skiljs på id:t.
int findInput(const Sheet& sheet, std::int64_t blockRow, const InputRef& input);

// Färgerna och stilen för en cellstil (planen 9.3).
struct Rgb {
    std::uint8_t r = 0;
    std::uint8_t g = 0;
    std::uint8_t b = 0;
    bool operator==(const Rgb&) const = default;
};
enum class Align { Left, Center };
enum class Frame { None, Thin, Thick };
struct CellLook {
    bool filled = false;
    Rgb fill;
    Rgb text;
    bool bold = false;
    Align align = Align::Center;
    Frame frame = Frame::Thin;
    bool operator==(const CellLook&) const = default;
};
CellLook cellLook(CellStyle style);

// Texten ovanför bladet (planen 12.2): vilka rader som visas, med blockens namn, eller varför
// inga visas.
std::string areaText(const std::vector<poletable::PoleRow>& before, const std::vector<poletable::PoleRow>& after,
                     const std::vector<PoleBlock>& blocks);

} // namespace kalkyl::lineheight
```
`libs/lineHeight/CMakeLists.txt`:
```cmake
# Fliken Linhöjder som rena funktioner: spannen på radernas id, blocken, uträkningarna, bladets
# celler och texterna. Includes skrivs som "lineHeight/line_height.h".
add_library(kalkyl_lineheight STATIC line_height.cpp)
target_include_directories(kalkyl_lineheight PUBLIC ${PROJECT_SOURCE_DIR}/libs)
target_link_libraries(kalkyl_lineheight PUBLIC kalkyl_poletable
                      PRIVATE kalkyl_poleview kalkyl_automode kalkyl_tabletext kalkyl_warnings)
```

### 8.3 `libs/projectdb/project_data.h`, tillägg sist i namnrymden
```cpp
// Linhöjderna i fliken Linhöjder: spannen (radernas id) och topplinans Av per rad som användaren
// har skrivit. Allt hör till rader som finns i Stolptabell efter (planen 7.6). Tomt svar när inget
// är sparat.
core::Result<lineheight::StoredHeights> lineHeights(ProjectDb& p);

// Skriver en del av ett spann som kanonisk decimaltext ≥ 0. Tom text tar bort värdet, och ett
// spann utan värden tas bort. Fel: AccessDenied, Invalid för ogiltig nyckel eller ogiltigt tal,
// och NotFound för ett spann som inte är två grannrader i Stolptabell efter.
core::Result<void> setSpanHeight(ProjectDb& p, const lineheight::SpanKey& span, lineheight::Part part,
                                 const std::string& value);

// Skriver topplinans Av för raden med id row i Stolptabell efter som kanonisk decimaltext. Tom
// text tar bort värdet. Fel: AccessDenied, Invalid för id ≤ 0 eller ogiltigt tal, och NotFound
// för en rad som inte finns eller är helt tom.
core::Result<void> setTopAv(ProjectDb& p, std::int64_t row, const std::string& value);
```
- `#include "lineHeight/line_height.h"` och `<cstdint>`.
- `libs/projectdb/CMakeLists.txt` får `kalkyl_lineheight` i PUBLIC.
- **Kommentarerna i `project_data.h`** ändras, utan nya signaturer:
  - `poles`: Stolptabell efter ger radernas id (`PoleRow::id`), före alltid 0.
  - `setPoles`: i efter får rader utan id ett (`poletable::assignRowIds`), och linhöjderna som inte längre hör till raderna tas bort i samma transaktion.
  - `copyBeforeToAfter`: raderna i efter får nya id (position + 1), och alla linhöjder tas bort.
- **Internt i `project_data.cpp`:** `readPoles` läser `row_id` i efter, `writePoleRows` skriver det, `readLineHeights(d)` läser tabellerna (delas av `lineHeights` och städningen), och `pruneLineHeights(d)` städar enligt 7.6. Ingen `renameLineHeightPole`.

### 8.4 `libs/lineHeightSheet/line_height_sheet.h` (nytt OBJECT-bibliotek `kalkyl_lineheightsheet`, namnrymden `kalkyl::lineheightsheet`)
```cpp
#pragma once
#include <glaze/glaze.h>

#include <cstdint>
#include <string>

#include "lineHeight/line_height.h"

namespace kalkyl::lineheightsheet {

// Linhöjdsbladet i fliken Linhöjder: blocken ur lineheight::Sheet ritade som Excel-förlagan, med
// färger, sammanslagna celler och ramar, i en Panel som rullar. Ett eget textfält redigerar den
// aktuella inmatningscellen. Bladet sparar inget självt: värden läser lastCommit() i onCommit.
class LineHeightSheet : public glaze::Panel {
public:
    LineHeightSheet();
    ~LineHeightSheet() override;

    // Byter bladet. Den aktuella cellen följer sin rad (id) och inmatning (lineheight::findInput),
    // och en öppen redigering står kvar med det som skrivits. Finns cellen inte kvar stängs
    // redigeringen utan att något fyras.
    void setSheet(lineheight::Sheet sheet);
    const lineheight::Sheet& sheet() const;

    // Skrivskyddat: textfältet går att markera och kopiera i men inte ändra, och onCommit fyras aldrig.
    void setReadOnly(bool on);
    bool isReadOnly() const;

    // Börjar redigera cellen (index i sheet().cells, en inmatning) och rullar fram den. Svarar
    // false för en cell som inte går att redigera.
    bool beginEdit(int cell);

    // Den aktuella cellen, eller -1.
    int currentCell() const;

    // Den senaste sparade inmatningen: vad cellen skriver, blockets rad-id och texten som den
    // skrevs. Värden läser den i onCommit.
    struct Commit {
        lineheight::InputRef input;
        std::int64_t blockRow = 0;
        std::string text;
    };
    const Commit& lastCommit() const;

    // Användaren är klar med en cell vars text ändrades: Enter, Tab, pilarna, ett klick på en annan
    // cell, fokus bort (postat) eller commitEdit.
    glaze::Handler<glaze::ChangeArgs> onCommit;

    // Sparar en öppen redigering nu och stänger den (AppWindow::commitEdits når den).
    void commitEdit() override;

    // Tooltipen för cellen under punkten.
    glaze::TrText toolTipAt(const glaze::Ctx& c, float x, float y) const override;

protected:
    void draw(const glaze::Ctx& c) override;
    gfx::Sizef contentExtent(const glaze::Ctx& c) const override;
    void mouseDown(const glaze::Ctx& c, float mx, float my) override;
    bool keyDown(gfx::Key key, std::uint32_t mods) override;
    bool focusable() const override;

private:
    class Editor;   // glaze::TextInput som tar Enter, Tab, Escape, Upp och Ner (som PropertyGrids TextEditor)
    // Resten är implementationens: cellernas och kolumnernas mått, ritningen, träffen, öppna och
    // stänga redigeringen, förflyttningen och rullningen till cellen.
};

} // namespace kalkyl::lineheightsheet
```
- Implementeringsagenten kontrollerar signaturerna för `keyDown`, `mouseDown`, `focusable`, `toolTipAt`, `contentExtent` och `canReleaseChild` i `widget.h` och `panel.h` och anpassar dem. En ändring står i rapporten.
- Registreringen står i `line_height_sheet.cpp`, inne i namnrymden:
  ```cpp
  namespace kalkyl::lineheightsheet {
  GLAZE_REGISTER_WIDGET("lineheightsheet", LineHeightSheet)
  ```
- `libs/lineHeightSheet/CMakeLists.txt`:
  ```cmake
  # Linhöjdsbladet, fliken Linhöjders egen GlazeUI-widget. Ett OBJECT-bibliotek, så att
  # registreringen (GLAZE_REGISTER_WIDGET) följer med in i kalkyl.exe, och med GLAZE_TOOLS=ON
  # också i glaze-render och GlazeBuilder, som då ritar bladet i stället för en ruta med klassnamnet.
  add_library(kalkyl_lineheightsheet OBJECT line_height_sheet.cpp)
  target_include_directories(kalkyl_lineheightsheet PUBLIC ${PROJECT_SOURCE_DIR}/libs)
  target_link_libraries(kalkyl_lineheightsheet PUBLIC glaze kalkyl_lineheight PRIVATE kalkyl_warnings)
  foreach(tool glaze-render GlazeBuilder)
      if(TARGET ${tool})
          target_link_libraries(${tool} PRIVATE kalkyl_lineheightsheet)
      endif()
  endforeach()
  ```
- `libs/CMakeLists.txt`:
  - `add_subdirectory(lineHeight)` direkt efter `add_subdirectory(poleView)`;
  - `add_subdirectory(lineHeightSheet)` sist, efter `poleGrid`;
  - kommentaren överst får "poleView ← lineHeight ← projectdb och lineHeightSheet".

### 8.5 `libs/poleTable/pole_table.h`, ändringar
```cpp
#include <cstdint>

// En rad i stolptabellen. Talen är kanonisk decimaltext (tabletext::normalizeDecimal): punkt
// som decimaltecken, inga tusentalsavgränsare och exakt de decimaler användaren skrev.
// Tom text = tom cell. Textfälten är trimmade. av2 och av3 används bara i Stolptabell efter.
// id är radens id i Stolptabell efter (> 0, unikt i tabellen, 0 = inget id än) och inget fält:
// det syns aldrig i tabellen, urklippet eller jämförelsen, och i Stolptabell före är det alltid 0.
struct PoleRow {
    std::string pole;
    std::string av;
    std::string ah;
    std::string spanM;
    std::string north;
    std::string east;
    std::string height;
    std::string lkl;
    std::string av2;
    std::string av3;
    std::int64_t id = 0;
    bool operator==(const PoleRow&) const = default;
};

// Ger varje rad med id ≤ 0, eller med ett id som redan stått tidigare i listan, nästa lediga id
// (högsta id + 1) i tabellens ordning. Andra rader och alla fält är orörda.
void assignRowIds(std::vector<PoleRow>& rows);
```
- `id` står sist, så att alla befintliga initieringar av `PoleRow` (i ordning och med namn) kompilerar oförändrade.
- Kommentarerna till `isEmptyRow` ("alla tio fält", id räknas inte), `applyEdits` (nya rader har id 0, id:t i en befintlig rad står kvar) och `rowsToSave` (id:na följer med) förtydligas.
- **Jämförelserna:** `operator==` tar med id:t. Det används i `PoleGrid::dirty` och `save` och i testerna. Paketet går igenom alla jämförelser av `PoleRow` i `libs`, `src` och `tests` och rättar de tester som jämför lästa rader i efter med rader utan id (13.3).

### 8.6 `libs/poleGrid/pole_grid.h`, tillägg i `PoleGrid`
```cpp
    // Raderna har id (Stolptabell efter): före varje sparning får nya rader ett id med
    // poletable::assignRowIds, så att raderna i tabellen och i projektet har samma id. Förval av.
    void setRowIds(bool on);
```
- Medlemmen `bool rowIds_ = false;` med en kort kommentar.
- `save()` kör `if (rowIds_) pt::assignRowIds(rows_);` först, före `rowsToSave` och jämförelsen med `saved_`. Inget annat i `PoleGrid` ändras: id:t följer raderna genom `insert`, `erase`, `applyEdits` och `trimTrailingEmptyRows`.
- Huvudfönstret kör `afterGrid_->setRowIds(true);` i `onLoad` direkt efter `afterGrid_->setComparesWithBefore(true);`.

---

## 9. Layouten

### 9.1 Skiss av ett block (kolumnerna P–X är 0–8)
```
      P             Q          R       S        T                          U      V      W         X
 0  [                                   ST0379  (grå, fet)                                               ]
 1  [ Linhöjd ST0379 ytterfaser (gul) ]       [meter] medelvärdet av intilliggande spann
 2  [ ST0378-ST0379                   ]       [12,33] [ 16,77 ]                         [ Av ][273,29]
 3  [ ST0379-ST0380                   ]       [21,2 ] [       ]
 4
 5  [ Linhöjd ST0379 mittfas (laxrosa)]       [meter] medelvärdet av intilliggande spann
 6  [ ST0378-ST0379                   ]       [12,33] [ 16,77 ]                         [ Av ][273,29]
 7  [ ST0379-ST0380                   ]       [21,2 ] [       ]
 8
 9  [ Linhöjd faslina för linberäkning (blå)            ]
10  [ ST0379     ][ 16,77                                ]
11
12
13  [ Linhöjd ST0379 Topplina (grön)  ]       [meter] medelvärdet av intilliggande spann
14  [ ST0378-ST0379                   ]       [22,63] [ 26,90 ]                         [ Av ][265,9332]
15  [ ST0379-ST0380                   ]       [31,17] [       ]
16
17  [ Linhöjd faslina för beräkning av sammanslagning (blå) ]
18  [ ST0378-ST0379 ][ 12,33                              ]
19  [ ST0379-ST0380 ][ 21,2                               ]
20  (tom rad, sedan nästa block)
```

### 9.2 Blockmallen (`buildSheet`, bindande)
Raden i bladet är `block * kBlockStride + r`. `{P}` är blockets namn (`PoleBlock::label`: Stolpnr som det står, eller "rad {n}"). "bak" och "fram" är spannen. `blockRows` får blockets `id`.

| r | kolumn, rader × kolumner | Stil | Text | Inmatning |
|---|---|---|---|---|
| 0 | 0, 1×9 | PoleHeader | `{P}` | |
| 1 | 0, 1×2 | OuterHeader | `Linhöjd {P} ytterfaser` | |
| 1 | 3 | ColumnHeader | `meter` | |
| 1 | 4, 1×2 | MeanHeader | `medelvärdet av intilliggande spann` | |
| 2 | 0, 1×2 | Label | bak `label` | |
| 2 | 3 | Input eller Locked | ytterfasernas bak | Span, Outer, bak `key` |
| 2 | 4, 2×1 | Computed | `roundedText(outerMean, 2)` | |
| 2 | 6 | AvLabel | `Av` | |
| 2 | 7 | AvValue | `outerAv` | |
| 3 | 0, 1×2 | Label | fram `label` | |
| 3 | 3 | Input eller Locked | ytterfasernas fram | Span, Outer, fram `key` |
| 5 | 0, 1×2 | MiddleHeader | `Linhöjd {P} mittfas` | |
| 5 | 3 · 4 (1×2) | ColumnHeader · MeanHeader | `meter` · `medelvärdet av intilliggande spann` | |
| 6 | 0 (1×2) · 3 · 4 (2×1) · 6 · 7 | Label · Input/Locked · Computed · AvLabel · AvValue | bak `label` · mittfas bak · `roundedText(middleMean, 2)` · `Av` · `av2` | Span, Middle, bak |
| 7 | 0 (1×2) · 3 | Label · Input/Locked | fram `label` · mittfas fram | Span, Middle, fram |
| 9 | 0, 1×4 | LineCalcHeader | `Linhöjd faslina för linberäkning` | |
| 10 | 0 · 1 (1×3) | Label · Computed | `{P}` · `roundedText(lineCalc, 2)` | |
| 12 | 0, 1×2 | TopHeader | `Linhöjd {P} Topplina` | |
| 12 | 3 · 4 (1×2) | ColumnHeader · MeanHeader | `meter` · `medelvärdet av intilliggande spann` | |
| 13 | 0 (1×2) · 3 · 4 (2×1) · 6 · 7 | Label · Input/Locked · Computed · AvLabel · Input/Locked | bak `label` · topplina bak · `roundedText(topMean, 2)` · `Av` · topplinans Av | Span, Top, bak; TopAv, blockets `id` |
| 14 | 0 (1×2) · 3 | Label · Input/Locked | fram `label` · topplina fram | Span, Top, fram |
| 16 | 0, 1×4 | MergeHeader | `Linhöjd faslina för beräkning av sammanslagning` | |
| 17 | 0 · 1 (1×3) | Label · Computed | bak `label` · mittfasens bak som den skrevs | |
| 18 | 0 · 1 (1×3) | Label · Computed | fram `label` · mittfasens fram som den skrevs | |

- Raderna 4, 8, 11 och 15 har inga celler: en tom rad mellan delarna (H2).
- **En spanncell är Input** med `editText` och `text` lika med `formatDecimalText(värde, ',', false)` när spannet är `usable`. Annars är den Locked med tom text, `InputRef::Kind::None` och `issue` som tooltip.
- **Topplinans Av** är Input när blockets `id` är > 0, annars Locked med tooltipen för en rad som inte är sparad (12.4). Dubbletter och tomma Stolpnr låser inget.
- **Läsordningen (`inputs`)** är rad för rad och kolumn för kolumn: ytter bak, ytter fram, mitt bak, mitt fram, topp bak, topplinans Av och topp fram, block för block.
- **`rows`** är `blocks.size() * kBlockStride - 1`, och 0 utan block.

### 9.3 Färgerna (`cellLook`)
| Stil | Fyllning | Text | Fet | Justering | Ram |
|---|---|---|---|---|---|
| PoleHeader | `#BFBFBF` | `#000000` | ja | mitten | Thick |
| OuterHeader | `#FFE699` | `#000000` | ja | mitten | Thick |
| MiddleHeader | `#F8CBAD` | `#000000` | ja | mitten | Thick |
| LineCalcHeader | `#00B0F0` | `#000000` | ja | mitten | Thick |
| TopHeader | `#92D050` | `#000000` | ja | mitten | Thick |
| MergeHeader | `#00B0F0` | `#000000` | ja | mitten | Thick |
| ColumnHeader | ingen | `#000000` | ja | mitten | Thick |
| MeanHeader | ingen | `#000000` | ja | vänster | None |
| Label | `#C5E0B4` | `#000000` | nej | mitten | Thin |
| Input | `#C5E0B4` | `#006100` | nej | mitten | Thin |
| Locked | `#E7E6E6` | `#808080` | nej | mitten | Thin |
| Computed | `#F2F2F2` | `#FA7D00` | ja | mitten | Thin |
| AvLabel | `#C5E0B4` | `#000000` | nej | mitten | Thin |
| AvValue | `#C5E0B4` | `#000000` | nej | mitten | Thin |

- Blockets yta, raderna 0–18 i kolumnerna P–X, fylls med `blockFill(block)` (H3): stolptabellernas gula ton med alfa 64 och 52 på vitt, växelvis, `#F6E7C9` och `#F8ECD3`. Den får en kraftig ram runt sig. Det gäller också i mörkt läge (K17).
- Mellan blocken och utanför dem syns panelens bakgrund.
- Ramarna är svarta: Thin är 1 logisk px och Thick 2 logiska px.

### 9.4 Måtten i widgeten
- **Kolumnbredden** är `kColumnChars[i]` gånger bredden av siffran "0" i UI-fonten plus cellens luft (`c.style`).
- **Radhöjden** är `c.style->gridRowH`, och bladet har 8 logiska px luft till vänster och ovanför.
- **Texten:** fet text ritas med `glaze::Draw::withFont(c, gfx::FontFamily::UI, gfx::FontStyle::Bold)`, och en text som inte ryms kortas med "…".
- **Hela bladet** är ungefär 1 070 logiska px brett. I ett smalare fönster rullar panelen i sidled.

---

## 10. Widgeten och fönstret

### 10.1 Utvärdering
| Kriterium | A: egen widget `LineHeightSheet` (rekommenderas) | B: DataGrid styrd från koden, som `PoleGrid` | C: en widget per block, skapade i koden |
|---|---|---|---|
| Likhet med förlagan | Hög: sammanslagna celler, fet stil, ramar, vitt blad, ingen kolumnrubrik | Medel: färger per cell men ingen sammanslagning, en gemensam kolumnrubrik, markeringens färg och rutnätet | Hög |
| Redigering | Ett eget textfält med `PropertyGrid` som förebild; medelstor risk | Färdig (DataGrid och sheetGrid); låg risk | Ett fält per cell, fokus mellan block; hög risk |
| Förhandsgranskning | Riktig i verktygsbygget (`tools`, OBJECT-biblioteket länkat), en ruta med klassnamnet i VS Code | Riktig överallt (tom tabell) | Barnen skapas i koden och syns aldrig |
| 3-filsmetoden | En widget i `.glazeui` | En DataGrid i `.glazeui` | Barn skapas i koden (gränsfall) |
| Testbarhet | Samma rena modell (`Sheet`), och ett test i ett dolt fönster | Samma rena modell | Svår |
| Arbete | Cirka 900 rader | Cirka 450 rader | Mest |

- **Rekommendation:** A. Det är äldre användares igenkänning som väger tyngst.
- **Risken minskas på tre sätt:**
  - All logik, alltså celler, texter, färger, förflyttning och träff i celler, ligger i den rena modellen och testas utan fönster.
  - Redigeringen följer `PropertyGrid`, som redan klarar fokus, rullning och postad sparning.
  - **Reserv:** misslyckas paket 3 kan samordnaren byta till B på samma `Sheet`. `SheetCell::row` och `column` blir DataGrid-celler, en sammanslagen cell skrivs i sin första kolumn, och resten blir tomma med samma färg.

### 10.2 Ritningen
1. `Panel::draw` ritas först, för ytan och ramen.
2. Bara de block som syns ritas. Vilka det är räknas ur rullningen, radhöjden och `kBlockStride`, och `blockStart` ger deras celler.
3. **För varje synligt block:**
   - den vita ytan;
   - cellerna: fyllning, text med stilens färg och justering, och ramen;
   - till sist blockets kraftiga ram.
4. **Den aktuella cellen** får en ram i temats accentfärg (`c.theme->colors.accent`) när bladet eller textfältet har fokus.
5. **`contentExtent`** är bladets bredd och höjd plus luften i fysiska px, som i `PropertyGrid`.
6. Rullningslisterna ritas av `Panel`.

### 10.3 Redigeringen
- **Textfältet** (`Editor`, en subklass av `glaze::TextInput`) är ett ägt barn som skapas med `add<>` i konstruktorn och är dolt när det inte används.
  - Det placeras över den aktuella cellen i logiska px i `childArea`, med 1 px insteg, och följer då rullningen.
  - `canReleaseChild` vägrar för det, som i `PropertyGrid`.
- **Börja:**
  - Ett klick på en Input-cell sparar en öppen cell och öppnar den nya med hela texten markerad, så att det man skriver ersätter den.
  - Ett klick på en annan cell sparar och stänger.
  - `beginEdit(cell)` gör samma sak från koden.
  - Har bladet fokus utan öppen redigering öppnar Enter och F2 den aktuella cellen, eller den första i `inputs`.
- **Tangenterna i textfältet** (`Editor::keyDown`, som tar dem före fönstret):

| Tangent | Gör |
|---|---|
| Tab · Shift+Tab | sparar och öppnar `moveInput(Next)` · `moveInput(Previous)` |
| Enter · Ner | sparar och öppnar `moveInput(Down)` |
| Shift+Enter · Upp | sparar och öppnar `moveInput(Up)` |
| Escape | återställer texten och stänger. Fokus går till bladet, och den aktuella cellen står kvar |
| Vänster, Höger, Home, End, Ctrl+C, X, V, A, Z | som i ett vanligt textfält |

  Ctrl+Tab går vidare till flikarna.
- **Spara (`endEdit`):**
  - Är texten oförändrad mot cellens `editText` fyras inget.
  - Annars sätts `lastCommit_ = {cellens input, blockets rad-id (blockRows), texten}`.
    - Vid Enter, Tab, pilarna, ett klick och `commitEdit()` fyras `onCommit` direkt med `fireEvent`. Svarar den false (handlern rev widgeten) returnerar koden direkt.
    - Därefter öppnas målcellen, som har tagits fram med `findInput` på målets rad-id och `InputRef` före händelsen. Handlern kan ha bytt bladet med `setSheet`.
  - **Fokusförlusten** sparar `lastCommit_` och postar `onCommit` med `postEvent`. Händelsen slopas aldrig, eftersom den bär sin egen nyckel och text. Har spannet eller raden hunnit försvinna vägrar `projectdb` (7.5), och värden visar statusraden.
- **Skrivskyddat** öppnas textfältet med `setReadOnly(true)`, så att det går att markera och kopiera, och `onCommit` fyras aldrig.
- **Rullningen:** när den aktuella cellen byts från tangentbordet rullas bladet så att cellen syns, med `setScroll`.
- **`setSheet` under en öppen redigering:**
  - Finns cellen kvar (`findInput`) flyttas textfältet dit, och texten står kvar.
  - Annars stängs det utan händelse.
- **Tooltipen:** `toolTipAt` ger cellens `toolTip`.

### 10.4 `main_window.h` och `main_window.glazeui`
**Regionen:**
- efter `glaze::TabPage efterSida;`: `glaze::TabPage linSida;`;
- efter `glaze::DataGrid efterGrid;`: `glaze::Panel linPanel;`, `glaze::Label linRubrik;`, `glaze::Label linInfo;`, `glaze::Label linOmrade;` och `kalkyl::lineheightsheet::LineHeightSheet linBlad;`.

Medlemsraden skriver klassen med hela namnrymden, så att verktygen hittar den i registret.

**Inkluderingarna** överst, utanför regionen: `"lineHeight/line_height.h"` och `"lineHeightSheet/line_height_sheet.h"`.

**`.glazeui`:** efter satserna för `efterSida`:
```cpp
    linSida.name = "linSida";
    linSida.setText("Linhöjder");
```
efter satserna för `efterPanel`:
```cpp
    linPanel.name = "linPanel";
    linPanel.dock = glaze::Dock::Fill;
```
och efter satserna för `efterGrid`, före `nsRubrik`:
```cpp
    linRubrik.name = "linRubrik";
    linRubrik.setText("Linhöjder");
    linRubrik.setRole(glaze::Label::Role::Heading);
    linRubrik.x = 16;
    linRubrik.y = 8;

    linInfo.name = "linInfo";
    linInfo.setText("<texten i 12.1>");
    linInfo.setRole(glaze::Label::Role::Dim);
    linInfo.setWrap(true);
    linInfo.anchors = {true, true, true, false};
    linInfo.x = 16;
    linInfo.anchorTop = {"linRubrik", 6};
    linInfo.right = 16;

    linOmrade.name = "linOmrade";
    linOmrade.setText("<texten Inga ändringar i 12.2>");
    linOmrade.setWrap(true);
    linOmrade.anchors = {true, true, true, false};
    linOmrade.x = 16;
    linOmrade.anchorTop = {"linInfo", 8};
    linOmrade.right = 16;

    linBlad.name = "linBlad";
    linBlad.setBorder(true);
    linBlad.anchors = {true, true, true, true};
    linBlad.x = 16;
    linBlad.anchorTop = {"linOmrade", 8};
    linBlad.right = 16;
    linBlad.bottom = 16;
```
- Raderna `<texten …>` skrivs med texterna ur avsnitt 12.
- **Händelsen**, efter `efterVisaBtn.onClick`:
  ```cpp
      linBlad.onCommit = [this](glaze::Widget* s, glaze::ChangeArgs& e) { linBladCommitted(s, e); };
  ```
- **Trädet:**
  - `flikar.adopt(linSida);` direkt efter `flikar.adopt(efterSida);`;
  - `linSida.adopt(linPanel);` direkt efter `efterSida.adopt(efterPanel);`;
  - `linPanel.adopt(linRubrik);`, `adopt(linInfo)`, `adopt(linOmrade)` och `adopt(linBlad)` direkt efter `efterPanel.adopt(efterGrid);`.
- **Raderna med `setColumns`** rörs inte. Ingen Edit får ha dem i `old_string`.

**`main_window.h`, efter regionen:**
```cpp
    // Linhöjdsbladet sparade en cell: talet kontrolleras (lineheight::acceptHeight eller acceptAv)
    // och skrivs i projektet, och bladet räknas om. Ogiltig text visas i statusraden.
    void linBladCommitted(glaze::Widget* sender, glaze::ChangeArgs& e);

    // Bygger fliken Linhöjder ur stolptabellerna och de sparade linhöjderna: blocken, bladet och
    // områdestexten. Utan block döljs bladet.
    void refreshLineHeights();
```
- Ingen `renameLineHeights`: ett nytt Stolpnr ändrar inte radens id (7.6).

### 10.5 Förhandsgranskningen
- **Verktygsbygget** (presetet `tools`, eller en byggkatalog med `-DGLAZE_TOOLS=ON` och målet `glaze-render`) länkar `kalkyl_lineheightsheet` in i glaze-render. Då ritas bladet på riktigt: tomt, med panelens ram.
- **VS Code-tillägget och den medföljande GlazeBuilder** ritar en ruta med klassnamnet, vilket är regel 4 i GlazeUI.
- **Fliken Linhöjder i bilden:** kopiera tripletten till scratchpad-mappen, lägg in `flikar.setSelectedIndex(3);` efter trädbygget i kopians `.glazeui` och rendera kopian.
  - glaze-render ska ge exit 0, och stderr får inte nämna någon stand-in för `LineHeightSheet`.
- **Ett bild-test med provdata** står i 13.4.

### 10.6 `main_window.cpp`
- **`onLoad`:**
  - `afterGrid_->setRowIds(true);` direkt efter `afterGrid_->setComparesWithBefore(true);` (8.6).
  - Skrivskyddat körs `linBlad.setReadOnly(true)`. `refreshAutomatic()` körs redan i slutet.
- **`refreshAutomatic()`** kör `refreshLineHeights()` sist.
- **`refreshLineHeights()`:**
  1. `stored = projectdb::lineHeights(*project_)`. Vid fel loggas det, statusraden får "Linhöjderna kunde inte läsas: " följt av `error().message`, och `stored` blir tom.
  2. `blocks = lineheight::buildBlocks(beforeGrid_->rows(), afterGrid_->rows(), stored)`.
  3. `linOmrade.setText(lineheight::areaText(before, after, blocks))`.
  4. `linBlad.setSheet(lineheight::buildSheet(blocks))`. Med block körs `linBlad.show()`, annars `linBlad.hide()`.
- **`linBladCommitted`:**
  1. Skrivskyddat görs ingenting.
  2. `const auto commit = linBlad.lastCommit();` (en kopia).
  3. `value` är `acceptAv(commit.text)` för `TopAv` och annars `acceptHeight(commit.text)`.
  4. Tomt svar ger statusraden i 12.6 (`setPoleStatus`), `refreshLineHeights()` och return.
  5. `setSpanHeight(…, commit.input.span, commit.input.part, *value)` eller `setTopAv(…, commit.input.row, *value)`.
     - `ErrorCode::NotFound` (spannet eller raden har hunnit försvinna, 7.5) ger `error().message` i statusraden, `refreshLineHeights()` och return.
     - Andra fel loggas, och rutan i 12.6 visas.
  6. `updateTitle()` och `refreshLineHeights()`.
- **`PoleHost::poleRenamed`** och `renameInRanges` är oförändrade.
- **`copyBeforeToAfterAsked`:** när Stolptabell efter har rader läser frågan också `projectdb::lineHeights`. Är svaret inte tomt får frågan meningen i 12.6 sist. Ett fel vid läsningen loggas, och frågan ställs utan meningen.
- **±1-texterna** i 11.
- **`src/CMakeLists.txt`:** `kalkyl_lineheight` efter `kalkyl_poleview` och `kalkyl_lineheightsheet` efter `kalkyl_polegrid`.

---

## 11. Filtret ±2 → ±1

| Fil | Ändring |
|---|---|
| `libs/poleView/pole_view.h` | `kMargin = 1`, `changedArea` och kommentarerna (8.1) |
| `libs/poleView/pole_view.cpp` | `buildView` använder `changedArea`, `hiddenToolTip` (12.7) och kommentarerna |
| `libs/poleView/CMakeLists.txt`, `CLAUDE.md` | "Förändrade ±1", `changedArea` och fliken Linhöjder som användare |
| `src/windows/main_window.glazeui` | `efterFilterDrop.setItems({"Förändrade ±1", "Alla"})`, tooltipen och `efterInfo` (12.7) |
| `src/windows/main_window.h`, `.cpp` | Kommentarerna |
| `libs/systemdb/system_db.h`, `.cpp`, `CLAUDE.md` | Kommentarerna och texten "Förändrade ±1". Värdena `changed` och `all` är oförändrade, och ingen migrering behövs |
| `libs/poleGrid/CLAUDE.md`, `libs/CLAUDE.md`, `src/windows/CLAUDE.md`, rotens `CLAUDE.md`, `tests/CLAUDE.md` | "Förändrade ±1" |
| `tests/poleview_test.cpp` | Förväntningarna i 13.2 |

- **Kontroll:** `grep -rn "±2" libs src tests res` och `grep -rn "två rader runt" libs src` ger inga träffar. De arkiverade planerna rörs inte.

---

## 12. Texter (ordagrant)

`{P}` är blockets namn (`PoleBlock::label`: Stolpnr som det står i Stolptabell efter, eller "rad {n}" när det är tomt), `{A}` och `{B}` är radernas namn på samma sätt, och `{x}` och `{y}` är tal med decimalkomma som de skrevs. Ett namn som börjar en mening får versal först ("Rad 6 är …"). Strecket i områdestexten är U+2013. Radbrytning skrivs `char{10}` i koden.

### 12.1 `linInfo`
"Linhöjderna för de ändrade raderna i Stolptabell efter och en rad före och efter dem, uppställda som i Excel. Skriv linhöjden per spann i de vita cellerna under meter, och topplinans Av per stolpe. Ett spann hör till två grannrader och syns i båda blocken, men sparas en gång. Linhöjderna hör till raderna, inte till Stolpnr: en raderad eller tömd rad tar sina värden med sig, och en ny stolpe emellan ger nya, tomma spann. Medelvärdena räknas ut och visas med två decimaler. Allt sparas i projektet."

- Blir `kBlockMargin` 2 efter handtestet ändras "en rad före och efter dem" till "två rader före och efter dem" här och i `.glazeui`.

### 12.2 Områdestexten (`areaText`)
`{marg}` räknas ur `kBlockMargin`: "en rad" för 1, "två rader" för 2 och annars "{k} rader".

| Läge | Text |
|---|---|
| Stolptabell efter saknar rader att spara | "Stolptabell efter är tom. Kopiera Stolptabell före dit och ändra de stolpar som byggs om, så visas deras linhöjder här." |
| Ingen rad är märkt | "Inga stolpar i Stolptabell efter är ändrade mot Stolptabell före, så det finns inga linhöjder att visa." |
| Märkta rader men inga block | "De ändrade raderna i Stolptabell efter är tomma, så det finns inga linhöjder att visa." |
| Ett block | "Visar 1 stolpe: {P}." |
| Flera block | "Visar {n} stolpar: {A}–{B}, de ändrade raderna och {marg} före och efter dem." |

- `{A}` och `{B}` är det första och det sista blockets namn. Dubbletter visas som de står.

### 12.3 Blockens texter
"{P}", "Linhöjd {P} ytterfaser", "Linhöjd {P} mittfas", "Linhöjd {P} Topplina", "Linhöjd faslina för linberäkning", "Linhöjd faslina för beräkning av sammanslagning", "meter", "medelvärdet av intilliggande spann", "Av" och spannen "{A}-{B}" (bindestreck U+002D, som i förlagan).

### 12.4 Tooltips
- **Inmatat spann:** "Linhöjd för {del} i spannet {A}-{B}, i meter. Spannet visas också i blocket för {granne} och sparas en gång."
  - `{del}` är ytterfaserna, mittfasen eller topplinan.
- **Medel, båda finns:** "Medelvärdet av intilliggande spann: ({x} + {y}) / 2 = {exakt}."
  - `{exakt}` är `halfSum` med decimalkomma.
- **Medel, ett spann:** "{P} har bara ett spann, så medelvärdet är spannets linhöjd."
- **Medel, tomt:** "Medelvärdet räknas när båda spannen har en linhöjd."
- **Linberäkningen:** "Medelvärdet av ytterfasernas och mittfasens medelvärden: ({x} + {y}) / 2 = {exakt}." När något saknas: "Räknas när ytterfasernas och mittfasens medelvärden finns."
- **Av för ytterfaserna:** "Det största av Av Fas1 ({x}) och Av Fas3 ({y}) i Stolptabell efter." En tom fas skrivs "tom".
- **Av för mittfasen:** "Av Fas2 i Stolptabell efter."
- **Topplinans Av:** "Topplinans Av för {P}, i meter. Skrivs här och sparas i projektet."
- **Sammanslagningen:** "Mittfasens linhöjd i spannet {A}-{B}."
- **Låst, först:** "{P} är den första stolpen i Stolptabell efter och har inget spann bakåt."
- **Låst, sist:** "{P} är den sista stolpen i Stolptabell efter och har inget spann framåt."
- **Låst, rad utan id** (skydd, 6.2): spannet "Spannet {A}-{B} hör till en rad som inte är sparad i projektet än, så linhöjden kan inte skrivas.", och topplinans Av "{P} är inte sparad i projektet än, så topplinans Av kan inte skrivas."
- Det finns inga låsta celler för dubbletter eller tomma Stolpnr (B13).

### 12.5 Widgeten
- Inga egna texter utöver cellernas.

### 12.6 Statusraden och rutor
- **Ogiltig linhöjd:** "Linhöjden måste vara ett tal som är 0 eller större, till exempel 12,3. Värdet ändrades inte."
- **Ogiltigt Av:** "Topplinans Av måste vara ett tal, till exempel 265,9. Värdet ändrades inte."
- **Fel vid sparning:**
  - rutan har titeln "Kalkyl", knappen OK och typen Error;
  - texten är "Linhöjden kunde inte sparas." följt av `char{10}` och `error().message`.
- **Spannet eller raden har försvunnit** (NotFound, 10.6): `error().message` i statusraden, alltså "Spannet finns inte längre i Stolptabell efter." eller "Raden finns inte längre i Stolptabell efter.".
- **Kopiera från Stolptabell före…, frågan:** när projektet har linhöjder får frågan meningen " Linhöjderna i fliken Linhöjder tas bort." sist, direkt efter "… och start och slut och automatiskt läge blir som i före.".

### 12.7 Filtret
- **`efterFilterDrop`, posterna:** "Förändrade ±1" och "Alla".
- **`efterFilterDrop`, tooltip:** "Förändrade ±1 visar de stolpar som är ändrade, nya eller borttagna jämfört med Stolptabell före och en rad före och efter dem. De andra står ihop i grå rader. Alla visar alla stolpar. Valet sparas."
- **`efterInfo`:** "Kopiera Stolptabell före hit och ändra sedan de stolpar som byggs om. Raderna jämförs med Stolptabell före på Stolpnr: färgen visar om raden är som i före, ändrad eller ny, och tooltipen på en ändrad cell visar värdet i före. Filtret Förändrade ±1 visar bara de förändrade stolparna och en rad före och efter dem, och resten står ihop i grå rader. Redigering, inklistring och papperskorgen fungerar som i Stolptabell före."
- **`hiddenToolTip`:** "{n} stolpar som är som i Stolptabell före är dolda av filtret Förändrade ±1. Välj Alla i filtret ovanför tabellen, eller högerklicka här och välj Visa alla rader."

---

## 13. Tester (ctest i `tests/`)

### 13.1 `tests/lineheight_test.cpp` (ny, ctest `lineheight`)
**Provtabellen:** `twelve()` är tolv rader ST0374–ST0385 med id 1–12, Av, Av Fas2 och Av Fas3 "300" och H "140". `asChanged()` är en kopia där ST0379 (rad 5, id 6) har Av "273.29", Av2 "273.29" och Av3 "270.1".

- **`testKeys`:**
  - `spanKey(7, 6)` och `spanKey(6, 7)` ger båda `{6, 7}`.
  - `isValidKey` är falsk för `a == 0`, `a < 0`, `a == b` och `a > b`.
- **`testAfterSpans`:**
  - `twelve()` ger elva spann, {1, 2} … {11, 12}, i tabellens ordning.
  - En helt tom rad mitt i hoppas över: spannet går förbi den.
  - En rad med id 0 ger inget spann mot sina grannar.
  - Dubbletter och tomma Stolpnr ger spann som alla andra rader.
- **`testKeptHeights`:**
  - Allt står kvar när raderna bara har fått nya Stolpnr, dubbletter eller en tom Stolpnr.
  - Raden med id 6 borttagen: {5, 6}, {6, 7} och topplinans Av för 6 tas bort, och {5, 7} finns inte.
  - Raden med id 6 tömd (alla fält tomma, id kvar): samma svar.
  - En tom rad (id 13) infogad mellan id 5 och 6: {5, 6} står kvar. Får raden ett Stolpnr tas {5, 6} bort.
  - En rad som inte finns i stored ändrar inget, och en tom stored ger en tom.
- **`testAccept`:**
  - `acceptHeight`: "12,33" ger "12.33", " 7 " ger "7" och "" ger "".
  - `acceptHeight` vägrar "-1", "abc" och "1e5".
  - `acceptAv("-3,5")` ger "-3.5".
- **`testHalfSumAndRounding`:**
  - `halfSum`: ("12.33", "21.2") ger "16.765", ("22.63", "31.17") ger "26.9", ("0.1", "0.2") ger "0.15", ("100", "0.001") ger "50.0005", ("0", "0") ger "0", och ("", "1") ger "".
  - `roundedText` med 2 decimaler: "16.765" ger "16,77", "16.775" ger "16,78", "26.9" ger "26,90", "9.995" ger "10,00", "0.004" ger "0,00", "12" ger "12,00" och "" ger "".
- **`testMeans`:**
  - `spanMean` för alla raderna i 6.3, också ett låst spann.
  - `lineCalcHeight("16.765", "16.775")` ger "16.77", och en tom text ger "".
- **`testOuterAv`:** Fas1 "273.29" och Fas3 "270.1" ger `kAv`; "300" och "1000" ger `kAv3` (talet avgör, inte texten); "" och "5" ger `kAv3`; lika ger `kAv`; båda tomma ger -1.
- **`testBuildBlocks`:**
  - **Användarens exempel** (6.9) med sparade värden på {5, 6} och {6, 7} ger blocken ST0378, ST0379 och ST0380:
    - ST0378 har spannen ST0377-ST0378 ({4, 5}) och ST0378-ST0379 ({5, 6}) (±2);
    - ST0379 (id 6) har medlen och Av i 6.9;
    - ST0378:s ytterfasmedel är tomt, eftersom ST0377-ST0378 är tomt.
  - **Ändring på rad 0:** blocken ST0374 och ST0375. ST0374 har `back.exists == false`, och ytterfasmedlet är värdet i ST0374-ST0375.
  - **Ändringar på rad 1 och 9:** elva block, ST0374–ST0384.
  - **Nytt Stolpnr:** ST0379 heter ST0379A i efter. Blocket får namnet ST0379A, och de sparade värdena på {5, 6} och {6, 7} visas.
  - **Dubblettnamn:** ST0380 heter också ST0379. Två block heter ST0379, med id 6 och 7, och spannen {5, 6}, {6, 7} och {7, 8} går att skriva i. Topplinans Av är skrivbar i båda.
  - **En rad med data men utan Stolpnr:** blocket heter "rad {n}", spannen mot den går att skriva i, och etiketten är till exempel "rad 6-ST0380".
  - **En helt tom rad infogad efter ST0378 (id 13):** raden får inget block, ST0378:s främre granne är ST0379, och värdena på {5, 6} visas.
  - **En infogad rad med data efter ST0378 (id 13):** blocket får spannen {5, 13} och {6, 13}, tomma, och {5, 6} används inte.
  - **ST0379 borttagen:** blocken ST0377, ST0378, ST0380 och ST0381. ST0378-ST0380 ({5, 7}) är ett nytt, tomt spann.
  - **Flyttad rad:** fälten i ST0379 (id 6) är tömda, och en ny rad sist (id 13) har ST0379 med ändrat Av. Rad 5 får inget block, ST0378-ST0380 är {5, 7}, och den nya raden har spannet {12, 13} mot ST0385, tomt.
  - **Rader med id 0:** spannen mot dem och topplinans Av är låsta med texterna i 12.4.
  - **Inga ändringar:** inga block.
  - **Stolptabell före tom:** ett block per rad som inte är helt tom.
  - **`kBlockMargin`:** området är `changedArea(…, kBlockMargin)`, och testet räknar med konstanten, så att det håller också när den blir 2.
- **`testBuildSheet`:**
  - Ett block ger cellerna i 9.2 exakt: rad, kolumn, storlek, stil och text.
  - `rows` är 20 för ett block och 41 för två, och `blockStart` och `blockRows` stämmer.
  - `inputs` har läsordningen i 9.2, och topplinans Av har `InputRef::row` = blockets id.
  - Den saknade bakre cellen är Locked med `Kind::None` och tooltipen.
- **`testNavigation`:**
  - `cellAt` inne i den sammanslagna medelcellen (rad 2 och 3, kolumn 4) och i den grå rubriken ger cellen, och en tom rad ger -1.
  - `moveInput`:
    - Next från topp bak ger topplinans Av, och Down från topp bak ger topp fram;
    - Down från topp fram i block 0 ger ytter bak i block 1, och Down från topplinans Av i block 0 ger topplinans Av i block 1;
    - Up från block 1:s ytter bak ger block 0:s topp fram;
    - kanterna ger `from`.
  - `findInput` hittar samma spann i två block och skiljer dem på radens id, också när blocken har samma Stolpnr.
- **`testLooks`:** `cellLook` för alla stilar i 9.3.
- **`testAreaText`:** alla rader i 12.2, också en tabell med en enda rad utan Stolpnr ("Visar 1 stolpe: rad 1.") och ordet för `{marg}` ur `kBlockMargin`.

### 13.2 `tests/poleview_test.cpp`, nya förväntningar (801 rader, ±1)
| Märkta | `buildView` |
|---|---|
| {350} | `[H 0–348, P 349–351, H 352–800]` |
| {350, 360} | `[H 0–348, P 349–361, H 362–800]` |
| {350, 353} | `[H 0–348, P 349–354, H 355–800]` |
| {350, 354} | `[H 0–348, P 349–355, H 356–800]` |
| {20, 780} | `[H 0–18, P 19–781, H 782–800]` |
| {0} | `[P 0–1, H 2–800]` |
| {2} | rad 0 är ensam och visas: `[P 0–3, H 4–800]` |
| {800} | `[H 0–798, P 799–800]` |
| count 3, {1} | `[P 0–2]` |
| count 10, marks för 2 med {1} | `[P 0–2, H 3–9]` |

- **`testChangedArea` (ny):**
  - Med margin 1: {350} ger {349, 351}, {0} ger {0, 1}, {800} ger {799, 800}, {20, 780} ger {19, 781}, och inga märkta ger {-1, -1}.
  - Med margin 2: {350} ger {348, 352} och {0} ger {0, 2}. Med margin 0: {350} ger {350, 350}.
- **`testIndex`** på vyn för {350} (sex rader):
  - `modelRow`: (0) -1, (1) 349, (3) 351, (4) -1, (5) 801, (7) 803.
  - `viewRowOf`: 0 och 348 ger 0, 349 ger 1, 400 ger 4, och 801 och 805 ger 5.
  - `visibleViewRow`: (100, ±1) ger 1, (400, -1) ger 3, (400, +1) ger 5 och (350, ±1) ger 2.
  - `isHiddenRow` för vyns rad 0 och 4.
- **`testResizeView`:**
  - 803 ger `[H 0–348, P 349–351, H 352–800, P 801, P 802]`.
  - 353 ger `[H 0–348, P 349–352]`, och 354 ger `[H 0–348, P 349–351, H 352–353]`.
  - 200, 1 och 0 som förut.
- **`testTexts`:**
  - `hiddenText` för {H 0–348} är "Dolda STO100–STO448 (349 stolpar som i före)", och för {H 352–800} "Dolda STO452–STO900 (449 stolpar som i före)".
  - `hiddenToolTip` enligt 12.7.
- **`testSummary`:**
  - Exemplet med en borttagen stolpe ger `shown == 403` (raderna 98–500) och "Förändrade: 2 av 800 rader · Borttagna: 1 · Visas: 403 av 800".
  - Två ändringar ger `Summary({801, 2, 2, 0, 13})` och "Visas: 13 av 801".

### 13.3 `tests/projectdb_test.cpp`
- **`kSeventeenTablesSql`** byts mot `kNineteenTablesSql` med de två nya tabellerna, och alla kontroller räknar 19.
- **`testSchemaTables`** kontrollerar `kSchemaVersion == 8`.
- **De befintliga testerna** som jämför lästa rader i efter med rader utan id (till exempel kring rad 1289, 1531, 1645, 2423 och 2431 i dag) jämför med de id som efter ska ha (1, 2, 3 … eller de givna) eller med en hjälpfunktion `withoutIds(rows)` i testet. Före jämförs som förut, med id 0.
- **`testRowIds` (ny):**
  - `setPoles(After)` med id 5, 0, 9 och 0 ger 5, 10, 9 och 11 tillbaka (`assignRowIds` i tabellens ordning). Ett dubblerat id får ett nytt.
  - Givna id står kvar efter omöppning. `poles(Before)` ger alltid 0.
  - Det unika indexet `pole_after_row_id` finns.
  - `copyBeforeToAfter` ger id position + 1 och tömmer linhöjderna.
- **`testLineHeights` (ny):**
  - Tur och retur för alla tre delarna och topplinans Av, också efter omöppning.
  - En tom text tar bort värdet, och ett spann utan värden försvinner ur tabellen.
  - Invalid och ingen ändring för: nyckel med `a == 0`, `a == b` eller `a > b`; "12,3" (inte kanonisk); "-1" som linhöjd; topplinans Av för id 0.
  - NotFound och ingen ändring för ett spann vars rader inte är grannar, och för topplinans Av på ett id som inte finns eller på en helt tom rad.
  - "-3.5" som topplinans Av går bra.
- **`testLineHeightsFollowAfter` (ny):** tolv rader med id 1–12 och värden på {5, 6}, {6, 7} och topplinans Av för 6. Varje steg skrivs med `setPoles(After)`:
  1. ST0379 (id 6) får Stolpnr ST0379A: allt står kvar.
  2. ST0380 (id 7) får Stolpnr ST0379, en dubblett, och rad 8 blir utan Stolpnr: allt står kvar, och `setSpanHeight` på {6, 7} och {7, 8} går bra.
  3. En tom rad (id 0) infogas mellan id 5 och 6: den får id 13, och {5, 6} står kvar. Raden får ett Stolpnr: {5, 6} tas bort, och {5, 13} och {6, 13} går att skriva.
  4. Raden med id 6 tas bort: {6, 7}, {6, 13} och topplinans Av för 6 tas bort, och {7, 13} är ett nytt spann.
  5. Alla fält i raden med id 7 töms (id kvar): dess spann och topplinans Av tas bort.
  6. `setPolesBefore`, `importPolesBefore` och `setAutoSettings` lämnar linhöjderna orörda.
  7. `copyBeforeToAfter` tar bort alla.
- **`testReadOnlyWrites` och `testRevision`** får `setSpanHeight` och `setTopAv`: AccessDenied respektive en uppräknad revision.
- **`testMigrateFromStep7` (ny):**
  1. `writeV040Project`, sedan `kStep5`, `kStep6` och `kStep7`, `PRAGMA user_version = 7` och tre rader i `pole_after` (den mittersta helt tom) med Av Fas2 och Fas3.
  2. Öppningen ger user_version 8, 19 tabeller, `row_id` 1, 2 och 3, det unika indexet, tomma `line_height_span` och `line_height_pole`, ett tomt svar från `lineHeights` och oförändrade data (`checkV040Data`, faserna).
  3. `setSpanHeight` på {1, 3} (förbi den tomma raden) fungerar och finns kvar efter omöppning. En ny rad sist får id 4.
- **Provprojektet** (`Prov åäö.kalkyl` i `KALKYL_PROV_DIR`) får linhöjder för spannen runt den ändrade raden i efter, på radernas id, åt röktestet.

### 13.4 `tests/lineheightsheet_test.cpp` (ny, ctest `lineheightsheet`)
1. **Ett testfönster** (en subklass av `glaze::AppWindow` i testet) har `LineHeightSheet` med `Dock::Fill` och bladet ur användarens exempel (tre block).
2. **Utan fönster:**
   - `sheet() == buildSheet(...)`.
   - `beginEdit` på en Input-cell svarar true och på en Computed-cell false.
   - `currentCell()` stämmer.
   - `setSheet` med ett blad där två block heter ST0379 (id 6 och 7) och ett block har tillkommit före dem: den aktuella cellen följer blocket med id 7.
3. **Bilden:**
   - `setOffscreen(true)`, `show()` och returkoden 77 om `!isOpen()`.
   - Sedan `renderFrame()` och `captureFrame()`.
   - Färgerna `#BFBFBF`, `#FFE699`, `#F8CBAD`, `#00B0F0`, `#92D050`, `#C5E0B4` och `#F2F2F2` finns på minst 50 bildpunkter var.
   - Bilden skrivs till `KALKYL_PROV_DIR/linhojder.png` (`gfx::encodePng`), och agenten och samordnaren granskar den med Read.

### 13.5 `tests/CMakeLists.txt`
```
kalkyl_add_test(lineheight lineheight_test.cpp kalkyl_lineheight kalkyl_poleview kalkyl_automode kalkyl_poletable kalkyl_tabletext)
kalkyl_add_test(lineheightsheet lineheightsheet_test.cpp kalkyl_lineheightsheet kalkyl_lineheight)
target_compile_definitions(kalkyl_lineheightsheet_test PRIVATE KALKYL_PROV_DIR="${CMAKE_BINARY_DIR}/prov")
set_tests_properties(lineheightsheet PROPERTIES TIMEOUT 30 SKIP_RETURN_CODE 77)
```
- Den första raden står direkt efter `poleview`.
- De tre sista står sist, under en kommentar om linhöjdsbladet i ett dolt fönster.
- Testerna `poletable` och `projectdb` har redan sina rader och ändras inte här.

### 13.6 `tests/poletable_test.cpp`
- **`testRowIds` (ny):**
  - `assignRowIds` ger {0, 3, 0, 3, -1} id {4, 3, 5, 6, 7}: rader med 0 eller negativt id och den senare dubbletten får nya id i tabellens ordning.
  - En tom lista och en lista med bara giltiga id ändras inte.
  - `isEmptyRow` är sant för en rad med bara id.
  - `applyEdits` behåller id:t i en befintlig rad och ger nya rader id 0. `trimTrailingEmptyRows` och `rowsToSave` behåller id:na på raderna som står kvar.
  - `copyText`, `displayText` och `field` för fälten 0–9 visar aldrig id:t, och `acceptField(10, …)` ger tomt svar.

### 13.7 `PoleGrid` och id:na
- `PoleGrid` har inget fönsterfritt test i dag (`polegrid_test.cpp` prövar bara `cellLook`). `setRowIds` och `save()` provas därför i handtestet (16) och genom `testLineHeightsFollowAfter`, som skriver raderna som `PoleGrid` gör.
- Paketet kontrollerar i koden att varje väg i 7.2 når `save()` och att ingen väg kopierar en `PoleRow` till en annan plats.

---

## 14. Arbetspaket

Fyra Sonnet-agenter (high) arbetar samtidigt. Varje agent äger sina filer, och ingen fil delas mellan paketen.

### Gemensamt för alla paket
- **Byggkatalogen** ligger i repots rot och konfigureras med MSYS2:s cmake (`C:/msys64/ucrt64/bin/cmake.exe`):
  ```sh
  cmake -S . -B build-<paket> -G Ninja -DCMAKE_BUILD_TYPE=Debug -DCMAKE_C_COMPILER=C:/msys64/ucrt64/bin/gcc.exe -DCMAKE_CXX_COMPILER=C:/msys64/ucrt64/bin/g++.exe -DKALKYL_TESTS=ON -DFETCHCONTENT_SOURCE_DIR_GLAZEUI=C:/Users/sehoer/Source/Kalkyl/build/_deps/glazeui-src -DFETCHCONTENT_SOURCE_DIR_MINIZ=C:/Users/sehoer/Source/Kalkyl/build/_deps/miniz-src
  ```
  Paket 3 och 4 lägger till `-DGLAZE_TOOLS=ON`.
- **Städning:** `build-<paket>` tas alltid bort när agenten är klar (`rm -rf build-<paket>`).
  - `build/` rörs aldrig: den byggs inte, rensas inte och konfigureras inte om.
- **Fel i ett annat pakets fil:** faller bygget i en fil som ett annat paket äger väntar agenten och försöker igen. Den ändrar aldrig filen.
- **Steg 0:** inom några minuter skriver varje paket sina kontraktsfiler ordagrant ur avsnitt 8, med kroppar som kompilerar. Ordningen:
  1. paket 2: `PoleRow::id` och `assignRowIds` i `libs/poleTable` (8.5) och `setRowIds` med medlemmen i `libs/poleGrid` (8.6), först, eftersom alla andra paket bygger på dem;
  2. paket 3: `libs/lineHeightSheet/CMakeLists.txt`, headern, en `.cpp` med stubbar och en stubb av `tests/lineheightsheet_test.cpp` som svarar 77;
  3. paket 1: `line_height.h` och `libs/lineHeight/CMakeLists.txt`, `changedArea` och raderna i `libs/CMakeLists.txt` och `tests/CMakeLists.txt`. `add_subdirectory(lineHeightSheet)` läggs in när paket 3:s `CMakeLists.txt` finns;
  4. paket 2: tilläggen i `project_data.h` och `kalkyl_lineheight` i `libs/projectdb/CMakeLists.txt`;
  5. paket 4: regionen, `.glazeui` och tomma handlers.
- **Versionen 0.7.0** är satt av samordnaren innan paketen startar.
- **Ägarskapet:** varje fil har exakt en ägare. `libs/poleTable` och `libs/poleGrid` ägs av paket 2, också deras `CLAUDE.md`. Ingen annan rör dem.

### Paket 1: lineHeight och poleView (`build-rakna`)
- **Äger:**
  - `libs/lineHeight/` (ny: `CMakeLists.txt`, `line_height.h`, `line_height.cpp`, `CLAUDE.md`);
  - `libs/poleView/pole_view.h`, `pole_view.cpp`, `CMakeLists.txt` och `CLAUDE.md`;
  - `libs/CMakeLists.txt` och `libs/CLAUDE.md`;
  - `tests/lineheight_test.cpp` (ny), `tests/poleview_test.cpp` och `tests/CMakeLists.txt`.
- **Arbete:** avsnitt 6, 8.1, 8.2, 9.2, 9.3, 11 (poleView-raderna), 12.1–12.4, 12.7 (`hiddenToolTip`), 13.1, 13.2 och 13.5.
  - `afterSpans` och `keptHeights` (7.6) är paketets. Paket 2 anropar dem.
- **Bygg och kör:** `ctest -R "lineheight$|poleview"`.
- **CLAUDE.md:**
  - `libs/lineHeight/CLAUDE.md`: filerna, reglerna i 6 (området med `kBlockMargin`, grannarna, nyckeln på radernas id, medlen, den exakta räkningen och avrundningen), `afterSpans` och `keptHeights`, bladets modell och vem som använder biblioteket.
  - `libs/poleView/CLAUDE.md`: ±1, `changedArea` med marginalen och att fliken Linhöjder delar området med sin egen marginal.
  - `libs/CLAUDE.md`: raderna `lineHeight/` och `lineHeightSheet/`, "Förändrade ±1" på raden `poleView/`, "och radernas id i Stolptabell efter" på raden `poleTable/`, och beroendena. `lineHeight` beror på `poleTable` (PUBLIC) och `poleView`, `autoMode` och `tableText`. `lineHeightSheet` beror på `glaze` och `lineHeight`, och `projectdb` beror också på `lineHeight`.
  - Rapporten innehåller texten till `tests/CLAUDE.md`.

### Paket 2: radernas id, projectdb och systemdb (`build-db`)
- **Äger:**
  - `libs/poleTable/pole_table.h`, `pole_table.cpp` och `CLAUDE.md`, och `tests/poletable_test.cpp`;
  - `libs/poleGrid/pole_grid.h`, `pole_grid.cpp` och `CLAUDE.md`;
  - `libs/projectdb/dev_migration.h`, `project_db.h`, `project_db.cpp`, `project_data.h`, `project_data.cpp`, `CMakeLists.txt` och `CLAUDE.md`;
  - `tests/projectdb_test.cpp`;
  - `libs/systemdb/system_db.h`, `system_db.cpp` och `CLAUDE.md` (bara texterna ±1).
- **Arbete:** avsnitt 7, 8.3, 8.5, 8.6, 11 (systemdb- och poleGrid-raderna), 13.3, 13.6 och 13.7.
  - Genomgången av alla jämförelser av `PoleRow` (8.5) och av vägarna i `PoleGrid` (7.2, 13.7) står i rapporten.
- **Bygg och kör:** `ctest -R "poletable|polegrid|projectdb|systemdb|poleimport_flow|automode"`.
- **CLAUDE.md:**
  - `libs/poleTable/CLAUDE.md`: `PoleRow::id` (inget fält, bara i efter, aldrig i urklippet) och `assignRowIds`.
  - `libs/poleGrid/CLAUDE.md`: "Förändrade ±1", `setRowIds` och att `save()` ger nya rader id, och att id:t följer raderna genom alla vägar (7.2).
  - `libs/projectdb/CLAUDE.md` får:
    - steg 8: `row_id`, det unika indexet, tabellerna och API:t;
    - `pruneLineHeights` och regeln att varje skrivning av `pole_after` städar linhöjderna, och att `copyBeforeToAfter` tömmer dem;
    - de 19 tabellerna och `kNineteenTablesSql`;
    - testerna;
    - en rättelse: steg 7 är committat i 0.6.0.
  - `libs/systemdb/CLAUDE.md` får "Förändrade ±1".
  - Rapporten innehåller texten till `tests/CLAUDE.md`.

### Paket 3: linhöjdsbladet (`build-blad`, med `-DGLAZE_TOOLS=ON`)
- **Äger:** `libs/lineHeightSheet/` (ny: `CMakeLists.txt`, `line_height_sheet.h`, `line_height_sheet.cpp`, `CLAUDE.md`) och `tests/lineheightsheet_test.cpp` (ny).
- **Arbete:** 8.4, 9.3, 9.4, 10.1–10.3 och 13.4.
  - Förebilden är `property_grid.h` och `property_grid.cpp` i GlazeUI-källan: textfältet, `contentExtent`, rullningen, `commitEdit`, den postade fokusförlusten och `canReleaseChild`.
- **Bygg och kör:**
  - `ctest -R lineheightsheet`, och granska PNG-bilden med Read.
  - Bygg `--target glaze-render` och kontrollera med en scratch-triplett att bladet inte ritas som en ruta med klassnamnet.
- **CLAUDE.md:** `libs/lineHeightSheet/CLAUDE.md` med:
  - OBJECT-biblioteket och varför;
  - registreringen med det okvalificerade namnet;
  - länkningen till glaze-render och GlazeBuilder;
  - ritningen, redigeringen och tangenterna;
  - skrivskyddet;
  - att VS Code visar en ruta med klassnamnet.

### Paket 4: huvudfönstret (`build-fonster`, med `-DGLAZE_TOOLS=ON`)
- **Äger:** `src/windows/main_window.h`, `main_window.glazeui`, `main_window.cpp` och `src/windows/CLAUDE.md`, samt `src/CMakeLists.txt` och `src/CLAUDE.md`.
- **Arbete:** 10.4–10.6, 11 och 12.7 (fönstrets texter), och 12.6 (frågan i Kopiera från Stolptabell före… och NotFound i statusraden).
  - `afterGrid_->setRowIds(true);` i `onLoad` (8.6).
- **Förhandsgranskning:**
  ```sh
  build-fonster/_deps/glazeui-build/apps/glaze-render/glaze-render.exe src/windows/main_window.h -o <scratchpad>/main_window.png
  ```
  - Exit 0 krävs.
  - Fliken syns i bilden via kopian med `flikar.setSelectedIndex(3);` (10.5): rubriken, infotexten, områdestexten och bladets ram.
- **Röktest:** `KALKYL_DATA_DIR=build-fonster/smoke timeout -k 2 3 ./build-fonster/src/kalkyl.exe; echo $?` ger 124 eller 137.
- **CLAUDE.md:**
  - Raden för `main_window.*` i `src/windows/CLAUDE.md`: fliken `linSida`, `linPanel`, `linRubrik`, `linInfo`, `linOmrade` och `linBlad`, `refreshLineHeights`, `linBladCommitted`, `setRowIds` på `afterGrid_`, frågan i Kopiera från Stolptabell före… och ±1.
  - `src/CLAUDE.md`: länklistan.

### Regler för alla implementeringsagenter
- **Kommentarer:**
  - Varje funktion och klass har en kommentar på 2–5 rader.
  - Inline-kommentarer är korta.
  - Ingen historik står i kommentarerna.
- **Fönster:**
  - Fönster görs bara med GlazeUI:s 3-filsmetod.
  - Layouten står bara i `.glazeui`, och `.glazeui` har ingen logik.
  - Klassen ligger utanför namespace, och designraden `// glazeui-design: ui=Slate theme=slate(Windows) style=0 lang=sv` står kvar.
  - Från GlazeUI inkluderas bara `<glaze/glaze.h>` och `<glaze_threads/threads.h>`.
- **Filredigering:**
  - Filer ändras bara med verktygen Edit och Write, aldrig med Python- eller sed-skript.
  - Skalet används bara för bygge, körning, test och läsning.
- **Hooken** granskar varje verktygsanrop, också filinnehållet. Den stoppar:
  - två bakvända snedstreck i rad,
  - ett bakvänt snedstreck och en bokstav efter skiljetecken eller blanksteg och före citattecken eller slut,
  - andra enhetsbokstäver än C följda av kolon.

  **Därför:**
  - råa strängliteraler för SQL,
  - `char{10}` och `char{34}`,
  - snedstreck framåt i sökvägar.

  Raderna med `setColumns` i `main_window.glazeui` tas aldrig med i en Edit.
- **Varningar:** bygget är fritt från varningar (`-Wall -Wextra`).
- **Versionen** rörs inte.
- **Inga commits.**

---

## 15. Integration (samordnaren)
1. **Före start:** versionen 0.7.0 i rotens `CMakeLists.txt`.
2. **Bygget utan varningar** i `build/`: `cmake --build --preset debug 2>&1 | grep -i warning` ger ingen utdata.
3. **Testerna** i en tillfällig `build-verify`, konfigurerad som i 14 och med `-DGLAZE_TOOLS=ON`:
   ```sh
   cmake --build build-verify && ctest --test-dir build-verify --output-on-failure
   ```
   Alla tester går igenom, också de nya `lineheight`, `lineheightsheet`, `testRowIds` (poletable och projectdb), `testMigrateFromStep7`, `testLineHeights` och `testLineHeightsFollowAfter`.
4. **Bilderna:**
   - `build-verify/prov/linhojder.png` granskas mot avsnitt 9.
   - glaze-render på `main_window.h` ger exit 0, och kopian med fliken vald visar fliken utan rutan med klassnamnet.
5. **Kontroller:**
   - `grep -rn "±2" libs src tests res` ger inga träffar.
   - `grep -rn "renameLineHeight" libs src tests` ger inga träffar.
   - `kSchemaVersion` är 8.
   - Versionen är 0.7.0.
6. **Röktest med provprojektet** `build-verify/prov/Prov åäö.kalkyl`: fliken Linhöjder visar blocken med de sparade linhöjderna.
7. **Städning:** inga `build-*` utom `build/` finns kvar.
8. **CLAUDE.md och `TODO.md`** enligt avsnitt 17. Användaren får handtestet i avsnitt 16.

---

## 16. Handtest (användaren)

**Provdata:** tolv stolpar ur Excel.
1. Skriv på rad 1 i ett tomt blad:
   - `="ST0"&(ROW()+373)` i A1,
   - `=6934000-ROW()*300` i B1,
   - `=548000` i C1,
   - `=140` i D1,
   - ingenting i E1,
   - `=300` i F1,
   - `=270+ROW()` i G1.
2. Fyll ner till rad 12. Det ger ST0374–ST0385.
3. Kopiera A1:G12 och klistra in på den tomma raden i Stolptabell före. Välj sedan "Kopiera från Stolptabell före…" i Stolptabell efter.

**Fliken och området**
1. Fliken Linhöjder står mellan Stolptabell efter och Normalspann. Utan ändringar visas bara texten "Inga stolpar i Stolptabell efter är ändrade …".
2. **Ändra Av Fas1 på ST0379 till 273,29, Av Fas2 till 273,29 och Av Fas3 till 270,1 i Stolptabell efter.**
   - Linhöjder visar tre block under varandra: ST0378, ST0379 och ST0380.
   - Texten ovanför är "Visar 3 stolpar: ST0378–ST0380, …".
   - Färgerna och rubrikerna är som i Excel: grå, gul, laxrosa, blå, grön och blå.
3. **Kanterna:** blocket ST0378 har spannen ST0377-ST0378 och ST0378-ST0379, och blocket ST0380 har ST0379-ST0380 och ST0380-ST0381.
4. **Filtret i Stolptabell efter** heter Förändrade ±1. Det visar ST0378–ST0380 med grå rader runt, och tooltipen och infotexten säger en rad.
5. **Marginalen, ±1 eller ±2 (B14):** blocken är ST0378–ST0380 (±1), och ytterblockens spann når ST0377 och ST0381 (±2). Bedöm om det räcker eller om blocken ska vara ±2 (ST0377–ST0381, spannen ±3). Säg till, så ändrar samordnaren `kBlockMargin` och texten i 12.1.

**Inmatning och uträkning**

6. **Ytterfaser i blocket ST0379:**
   - Skriv 12,33 i ST0378-ST0379 och tryck Enter. Samma värde syns i blocket ST0378, och titeln får `*`.
   - Skriv 21,2 i ST0379-ST0380. Medlet blir 16,77 i fet orange text på ljusgrått, och tooltipen visar (12,33 + 21,2) / 2 = 16,765.
7. **Mittfas:** samma värden ger 16,77. Linberäkningen blir 16,77, och sammanslagningen visar 12,33 och 21,2.
8. **Avrundningen:** mittfas 12,3 och 21,25 ger 16,78, och linberäkningen blir 16,77.
9. **Topplina:** 22,63 och 31,17 ger 26,90. Skriv 265,9332 i topplinans Av: det visas som det skrevs.
10. **Av:**
    - Av för ytterfaserna är 273,29.
    - Ändra Av Fas3 på ST0379 till 280 i Stolptabell efter. Linhöjder visar då 280 för ytterfaserna, och 273,29 för mittfasen. Linhöjderna står kvar.
11. **Ett spann tomt:** medlet är tomt, och tooltipen förklarar.
12. **Felaktig text:**
    - "abc" ger texten i statusraden, och värdet står kvar.
    - "12.5" visas som 12,5.
    - "-1" vägras som linhöjd men går som topplinans Av.
13. **Töm en cell och tryck Enter:** värdet försvinner, och medlet blir tomt. Skriv tillbaka värdet.
14. **Tangenterna:**
    - Tab och Shift+Tab går i läsordning.
    - Enter och Ner går nedåt i samma kolumn, också till nästa block.
    - Escape ångrar det skrivna.
15. **Ctrl+S, stäng och öppna igen:** alla värden finns kvar. Ctrl+S medan en cell redigeras sparar också den.

**Kanterna**

16. **Första stolpen:** ändra ST0374. Blocket ST0374 har en låst rad utan spann bakåt och tooltipen "… den första stolpen …", och medlet är det främre spannets värde.
17. **Två ändringar långt isär** (ST0376 och ST0383) ger ett sammanhängande område ST0375–ST0384. Återställ ST0374, ST0376 och ST0383 med "Kopiera från Stolptabell före" i snabbmenyn, cell för cell. Linhöjderna står kvar.

**Ändringar i Stolptabell efter: linhöjderna hör till raderna, inte till Stolpnr**

Punkterna görs i ordning, med värdena från punkt 6–9 i ST0378-ST0379 och ST0379-ST0380.

18. **Nytt Stolpnr:**
    - Döp om ST0379 till ST0379A i en cell. Värdena står kvar, rubriken blir "Linhöjd ST0379A ytterfaser" och spannen "ST0378-ST0379A" och "ST0379A-ST0380".
    - Klistra in "ST0379B" från Excel i samma cell. Värdena står kvar också då.
    - Döp tillbaka till ST0379.
19. **Dubblettnamn:**
    - Döp om ST0380 till ST0379. Två block heter ST0379, inget är låst, och spannet mellan dem heter "ST0379-ST0379".
    - Skriv 5 i spannet ST0379-ST0379. Det syns i båda blocken med det namnet och inte i något annat spann.
    - Döp tillbaka till ST0380. Värdena står kvar.
20. **Tom Stolpnr:** töm bara Stolpnr på ST0380. Blocket heter "rad 7", spannen heter "ST0379-rad 7" och "rad 7-ST0381", de går att skriva i, och värdena står kvar. Skriv tillbaka ST0380.
21. **Infogad rad:**
    - Högerklicka på ST0380 och välj Infoga rad ovanför. Den tomma raden får inget block, och ST0379-ST0380 visar fortfarande sina värden.
    - Skriv ST0379C som Stolpnr i den nya raden. Blocket ST0379C kommer, ST0379-ST0379C och ST0379C-ST0380 är nya och tomma, och värdena i ST0379-ST0380 är borta.
22. **Raderad rad:**
    - Radera ST0379C med papperskorgen. ST0379-ST0380 är tomt, eftersom de gamla värdena togs bort när ST0379C kom emellan.
    - Radera ST0379. Blocken blir ST0377, ST0378, ST0380 och ST0381, ST0378-ST0380 är tomt, och topplinans Av för ST0379 är borta.
23. **Flyttad rad:**
    - Ändra Av Fas2 på ST0383 till 301 och skriv 5 i ST0382-ST0383 under ytterfaser.
    - Välj Alla i filtret, markera hela raden ST0383 (Stolpnr till LKL), Ctrl+X, och klistra in med Ctrl+V på den tomma raden sist.
    - Den tömda raden får inget block, ST0382-ST0384 är ett nytt, tomt spann, och 5:an är borta. ST0383 sist har ett block med det nya, tomma spannet ST0385-ST0383.
24. **Inklistring över rader:** kopiera tre tal ur Excel och klistra in dem i Av Fas1 på ST0380–ST0382. Linhöjderna i de blocken står kvar.
25. **Kopiera från Stolptabell före…:**
    - Frågan säger "… Linhöjderna i fliken Linhöjder tas bort.". Svara Ja.
    - Linhöjder visar "Inga stolpar i Stolptabell efter är ändrade …". Ändra ST0379 igen: alla spann är tomma.
26. **Skrivskyddat projekt:** värdena syns och går att markera och kopiera men inte ändra.

**Övrigt**

27. **Mörkt och ljust läge:** bladet behåller Excel-färgerna och är läsbart i båda.
28. **Smalt fönster:** bladet rullar i sidled.
29. **Prestanda:** med många block (till exempel en tom Stolptabell före och alla stolpar nya) rullar bladet mjukt, och en ändring i Stolptabell efter går lika fort som förut.
30. **Ett äldre projekt från 0.6.0** öppnas och sparas. Inget försvinner, fliken fungerar, och rader i Stolptabell efter går att infoga, radera och klistra in som förut.

---

## 17. CLAUDE.md och TODO.md
| Fil | Vem | Ändring |
|---|---|---|
| Rotens `CLAUDE.md` | samordnaren | Se nedan |
| `libs/CLAUDE.md`, `libs/lineHeight/CLAUDE.md`, `libs/poleView/CLAUDE.md` | paket 1 | 14, paket 1 |
| `libs/poleTable/CLAUDE.md`, `libs/poleGrid/CLAUDE.md`, `libs/projectdb/CLAUDE.md`, `libs/systemdb/CLAUDE.md` | paket 2 | 14, paket 2 |
| `libs/lineHeightSheet/CLAUDE.md` | paket 3 | 14, paket 3 |
| `src/windows/CLAUDE.md`, `src/CLAUDE.md` | paket 4 | 14, paket 4 |
| `tests/CLAUDE.md` | samordnaren | Raderna för `lineheight` och `lineheightsheet` och tilläggen för `poleview`, `poletable` och `projectdb`, med paketens text |
| `doc/plans/CLAUDE.md` | samordnaren | Raden för den här planen |
| `TODO.md` | samordnaren | 📝 nu, ⬜ när planen är godkänd och 🔨 under implementeringen, och 💡-raderna nedan |

**Rotens `CLAUDE.md`:**
- Huvudfönstrets rad: flikarna får "Linhöjder (ett block per rad i det ändrade området ±1 som Excel-förlagan, linhöjder per spann och topplinans Av på radernas id i Stolptabell efter, sparas i projektet)" mellan Stolptabell efter och Normalspann. Filtret heter "Förändrade ±1/Alla".
- Raden för `libs/poleView` får "Filtret Förändrade ±1 och det ändrade området".
- Raden för `libs/poleTable` får "och radernas id i Stolptabell efter (`PoleRow::id`, `assignRowIds`)".
- Nya rader:
  - `libs/lineHeight`: "Fliken Linhöjder som rena funktioner: spannen på radernas id, blocken, medelvärdena (exakt i decimaltext), det som ska stå kvar när efter ändras, bladets celler, färger och texter".
  - `libs/lineHeightSheet`: "`LineHeightSheet`: linhöjdsbladet som egen GlazeUI-widget (OBJECT-bibliotek, ritas också av glaze-render i verktygsbygget)".
- Raden för `libs/projectdb` och avsnittet Data: "… radernas id i Stolptabell efter (`row_id`) och linhöjderna per spann och topplinans Av på radernas id (`line_height_*`), som följer raderna i efter och tas bort med dem".
- Under Bygga och köra: linhöjdsbladet ritas på riktigt bara av glaze-render ur presetet `tools`, och VS Code visar en ruta med klassnamnet.

**💡 i `TODO.md`:**
- Linhöjd faslina för linberäkning som indata till Linberäkningens fält "Linhöjd, snitt (m)" (fråga 6).
- Inklistring av flera celler och en snabbmeny med Gå till Stolptabell efter i fliken Linhöjder.

---

## 18. Risker och luckor
1. **En egen widget är nytt i Kalkyl:** registreringen, OBJECT-biblioteket, verktygsbygget och redigeringen med fokus och postad sparning.
   - Risken minskas med `PropertyGrid` som förebild, den rena modellen och bild-testet.
   - Reserven är DataGrid på samma `Sheet` (10.1).
2. **`GLAZE_REGISTER_WIDGET`** klistrar ihop klassnamnet. Därför skrivs det med det okvalificerade namnet inne i namnrymden, och medlemsraden skrivs med hela namnet. Fel form ger en ruta med klassnamnet i glaze-render.
3. **Excel-färgerna i mörkt läge:** bladet är ljust som ett papper (Ö5).
4. **Inklistring över befintliga rader behåller linhöjderna.** Raden är densamma och bara fälten är nya (7.7). Klistrar man in en helt annan stolptabell över raderna i efter står linhöjderna kvar på raderna. Att börja om görs med Kopiera från Stolptabell före…, som tar bort dem och säger det i frågan.
5. **Klipp ut och klistra in på samma rad tar bort linhöjderna.** Raden är tom och sparad emellan, och städningen tar då dess spann. Samma sak gäller Delete på hela raden följt av ny text. Det följer B12 (en tömd rad är borta), men kan överraska.
6. **Id:t vilar på att `PoleGrid` ger nya rader id före varje sparning** (`setRowIds`). En ny väg i `PoleGrid` som skapar rader utan att gå genom `save()` skulle få id först vid nästa sparning. Den får då inget fel värde, bara ett nytt, tomt spann. `setPoles` ger dessutom id själv som skydd.
7. **En tom Stolptabell före** gör alla rader nya, och då får alla rader block (801 rader ger cirka 16 800 rader i bladet). Bladet ritar bara de synliga blocken.
8. **Avrundningen** görs på det exakta decimalvärdet. Den stämmer med Excels visning i alla vanliga fall, men Excel räknar internt med 15 siffror.
9. **Förlagans inkonsekvenser återges inte** (3.3, Ö4).
10. **Förhandsgranskningen i VS Code** visar en ruta med klassnamnet. Det är GlazeUI:s regel för egna typer utan verktygsbygge.
11. **Den automatiska sparningen** sparar inte en cell som redigeras just då, med flit som i resten av programmet. Ctrl+S och stängningen gör det.
12. **`libs/projectdb/CLAUDE.md` är fel om steg 7.** Paket 2 rättar texten.
13. **Varje ändring i Stolptabell efter läser nu tabellen och linhöjderna en gång till** i samma transaktion (`pruneLineHeights`). Med 800 rader är det några millisekunder, i samma storlek som `syncAutoCalcs`, som redan gör det. Handtestet prövar det (16, punkt 29).
14. **Märkningen jämför fortfarande med före på Stolpnr** (filtret). En flyttad rad som är lika med sin rad i före är därför inte märkt och får inget block förrän något i den ändras (16, punkt 23). Det gäller filtret på samma sätt.
15. **`PoleRow::operator==` tar med id:t.** Tester som jämför lästa rader i efter med rader utan id måste rättas (13.3), och nya jämförelser i koden måste ta ställning till id:t.

---

## 19. Öppna frågor (förslagen är förval)
- **Användaren sa "kör" 2026-11-27:** förslagen i Ö4–Ö7 gäller som beslut. Ändringar tas efter handtestet.
- **Beslutade 2026-11-27:**
  - Ö1, stolpbeteckningen: Stolpnr som det står i Stolptabell efter, överallt (avsnitt 1).
  - Ö2 och Ö3 ersätts av B12–B14: linhöjderna hör till raderna i efter genom radernas id, en borttagen rad tar sina värden med sig, och dubbletter och tomma Stolpnr låser inget.
- **Ö4, förlagans block STP 380:**
  - Blocket räknar sammanslagningen som (ytter + mitt) / 2 per spann, har kopierade rubriker och tar Av ur Fas2 för båda faserna.
  - **Förslag:** följ besluten B4 och B5, och inför varken läget Excel eller något PM, eftersom fliken inte återger en beräkning i ett Excel-ark.
- **Ö5, formateringen:**
  - **Förslag:**
    - Alla uträknade värden får beräkningsstilen (ljusgrå, fet orange), också topplinans medel, som är grönt i förlagan.
    - Alla inmatningar får inmatningsstilen (ljusgrön, mörkgrön text).
    - Bladet behåller Excel-färgerna i mörkt läge.
  - Detaljerna kan justeras efter handtestet.
- **Ö6, linhöjderna vid Kopiera från Stolptabell före…:**
  - Knappen ersätter alla rader i efter med en kopia av före, alltså nya rader.
  - **Förslag:** alla linhöjder tas bort i samma transaktion, och frågan säger det (K21). Det följer B12: linhöjderna hör till raderna i efter, och de raderna finns inte längre.
  - **Alternativ:** behålla värdena för raderna på samma plats (id = position + 1 som förut). Det skulle knyta värden till platser och inte till stolpar, och ge fel värden när före har en annan ordning.
- **Ö7, marginalen för blocken:** ±1 rad (`kBlockMargin = 1`) tills handtestet (16, punkt 5) visar om ±2 behövs (B14).

---

### Kritiska filer för implementeringen
- C:/Users/sehoer/Source/Kalkyl/libs/lineHeight/line_height.h (ny)
- C:/Users/sehoer/Source/Kalkyl/libs/lineHeightSheet/line_height_sheet.cpp (ny)
- C:/Users/sehoer/Source/Kalkyl/libs/poleView/pole_view.cpp
- C:/Users/sehoer/Source/Kalkyl/libs/poleTable/pole_table.h
- C:/Users/sehoer/Source/Kalkyl/libs/poleGrid/pole_grid.cpp
- C:/Users/sehoer/Source/Kalkyl/libs/projectdb/project_data.cpp
- C:/Users/sehoer/Source/Kalkyl/libs/projectdb/dev_migration.h
- C:/Users/sehoer/Source/Kalkyl/src/windows/main_window.cpp
