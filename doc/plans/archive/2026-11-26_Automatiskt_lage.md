# Automatiskt läge och Stolptabell efter: sträckor i stolptabellerna, automatiska normalspanns- och linberäkningar och Stolptabell efter

- **Datum:** 2026-11-26.
- **Status:** användarens svar från 2026-11-26 är inarbetade.
  - Alla förslag i avsnitt 1 gäller, och biblioteket `libs/poleGrid` gäller.
  - Ett undantag: de automatiska beräkningarna sparas i projektet (4.1).
  - Planen är klar för implementering.
- **Version:** 0.5.0, efter användarens beslut (minor+1).
  - Samordnaren ändrar `project(Kalkyl VERSION 0.4.0` till `0.5.0` i rotens `CMakeLists.txt`.
  - Agenterna rör aldrig versionen.
- **Underlag:**
  - Koden: `src/windows/main_window.*`, `normal_span_dialog.*` och `line_calc_dialog.*`.
  - Biblioteken: `libs/poleTable`, `libs/anCalc`, `libs/lineCalc`, `libs/projectdb` (`project_data.*`, `dev_migration.h`), `libs/sheetGrid` och `libs/tableText`.
  - Testerna i `tests/`.
  - GlazeUI v0.11.1, avsnitten i `build/_deps/glazeui-src/MANUAL.md`: `widget-data-grid`, `widget-list-view`, `widget-checkbox`, `widget-dropdown`, `widget-group-box`, `widget-tab-control`, `message-box` och `stock-icons`.
  - Planerna i `doc/plans/archive/`: Normalspann, Linberäkning fas 2 och Stolptabell före.

Planen är kontraktet för fyra implementeringspaket och för granskningen.
- **Bindande:** headrarna i 6.1, 6.3 och 6.5, schemat i 6.2 och texterna i avsnitt 7. Namn, signaturer och texter följs ordagrant.
- **Ändrade signaturer:** behöver ett paket ändra en signatur säger det till i rapporten, och samordnaren beslutar.

**Användarens svar 2026-11-26** (inarbetade i avsnitten):
1. Förslagen i fråga 1–14 gäller.
2. `libs/poleGrid` gäller (4.8).
3. **De automatiska beräkningarna sparas i projektet** som vanliga normalspanns- och linberäkningar och hålls aktuella när något de bygger på ändras.
   - Skälet: utskrifter kommer senare och ska kunna läsa dem som lagrade beräkningar.
   - Varje skede och lina har ett stabilt id (4.1, 4.4).
   - Läget av tar bort dem, och läget på skapar dem igen med samma id och nya värden.
4. **De automatiska beräkningarna ska synas tydligt** på raderna i flikarna Normalspann och Linberäkning (9.2).

---

## 1. Öppna frågor

Alla frågor är besvarade. Under varje fråga står förslaget och sedan användarens svar.

1. **Vad är "Automatiskt läge", och var slås det på?**
   - Förslag: en kryssruta per stolptabell, överst i fliken. Stolptabell före och Stolptabell efter har var sin ruta, och valet sparas i projektet.
     - På: Dokumentationens beräkningsrader för skedet (lila för före, rosa för efter) visar de automatiska beräkningarna för faslinan och topplinorna.
     - Av: allt fungerar som i dag.
     - Före kan vara automatisk medan efter är manuell, och tvärtom.
   - Alternativ: en ruta för hela projektet, eller en ruta per lina.

   **Svar:** förslaget gäller.

2. **Hur väljs start och slut?**
   - Förslag: en ram "Automatiskt läge" ovanför tabellen, med två rullistor per lina, Från och Till.
     - Rullistorna visar tabellens Stolpplatser i tabellens ordning. Överst står "–", som betyder inte vald.
     - Snabbmenyn i tabellen får undermenyn "Sträcka" med "Faslinan börjar här", "Faslinan slutar här" och samma för topplinorna.
     - Start- och slutraderna får en flagga i Stolpplats med en tooltip.
     - Tabellen får ingen ny kolumn.

   **Svar:** förslaget gäller.

3. **Vad händer med Dokumentationens nuvarande val i automatiskt läge?**
   - Förslag: raderna för skedet visar de automatiska beräkningarna och är låsta. Det är de sex lila eller rosa raderna, eller fyra när topplinorna är samma.
     - Snabbmenyn har bara "Visa…" och "Gå till Stolptabell före".
     - Kopplingarna och de manuella värdena som raderna hade tas inte bort. De vilar och kommer tillbaka när läget stängs av.
     - De beräkningar som var kopplade står kvar i flikarna Normalspann och Linberäkning. Så länge läget är på visar kolumnen Dokumentation "–" för dem.

   **Svar:** förslaget gäller.

4. **Vad händer när automatiskt läge stängs av?**
   - Förslag: de automatiska posterna försvinner ur flikarna, och raderna i Dokumentation visar det de visade innan.
     - Start och slut sparas, så samma beräkningar kommer tillbaka när läget slås på igen.
     - Rutan ställer ingen fråga.
     - Den som vill behålla en automatisk beräkning som vanlig gör Kopiera… först.
   - Alternativ: beräkningarna blir vanliga beräkningar ("fryses").

   **Svar:** förslaget gäller. Eftersom beräkningarna nu sparas (4.1) tas raderna bort ur projektet när läget stängs av. När läget slås på skapas de igen med samma id och med värden ur tabellen som den är då (4.4).

5. **Topplinorna: samma eller olika?**
   - Förslag: valet följer Dokumentationens Topplina höger.
     - "Samma" ger en sträcka, Topplinor, som räknas till raderna "Topplinor …".
     - En egen lina på Topplina höger ger två sträckor, Topplina vänster och Topplina höger, med var sina rader.
     - Sträckan för höger sparas också när Samma är valt, men den används inte då.
   - Alternativ: en egen ruta "Topplinorna har olika sträckor".

   **Svar:** förslaget gäller.

6. **Vad gäller för en lina utan vald sträcka när läget är på?**
   - Förslag: när läget är på är alla linor i skedet automatiska.
     - En lina utan start eller slut ger felet "Välj var faslinan börjar och slutar." i ramen.
     - Raden i Dokumentation är tom och har felet som tooltip.
   - Alternativ: en lina utan sträcka förblir manuell.

   **Svar:** förslaget gäller.

7. **Vilka gränser och vilka indata har de automatiska beräkningarna?**
   - Förslag:
     - Normalspannet har Excels förvalda gränser: 1,5, 0,5, 2,5, 0,95 och 1,15.
     - Linberäkningen har förvalen för en ny beräkning: läget Ny, standardmallen, T₀ 0 °C och konstanterna.
     - Linan, aₙ, σ₀, min och max kommer ur Dokumentation, som för en kopplad beräkning.
     - Inget av det går att ändra. Behövs något annat görs Kopiera…, och läget stängs av för skedet.
   - Alternativ: gränserna och läget går att ändra per beräkning.

   **Svar:** förslaget gäller.

8. **Vilka kolumner används till normalspannet?**
   - Förslag:
     - Stpnr är Stolpplats.
     - Spannlängden är Spannlängd föreg.
     - H är Koordinat H.
     - Av, Ah, Koordinat N och E och LKL används inte.

   **Svar:** förslaget gäller.

9. **Vilka färger har Stolptabell efter?**
   - Förslag:
     - En rad som är som i före är gul.
     - En ändrad rad är rosa. Cellerna som skiljer sig är starkare rosa och har tooltipen "I Stolptabell före: 350".
     - En ny rad är grön.
     - Förklaringen "■ Som i före ■ Ändrad ■ Ny" står ovanför tabellen.
     - Talen jämförs som tal, så 44,10 och 44,1 räknas som lika.

   **Svar:** förslaget gäller.

10. **Vad händer vid en ny kopiering när efter har egna ändringar?**
    - Förslag: "Kopiera från Stolptabell före…" ersätter allt i efter: raderna, sträckorna och läget.
      - Frågan säger hur många rader i efter som är ändrade eller nya.
      - Ingen sammanslagning görs.
      - Är efter tom kopieras allt utan fråga.

    **Svar:** förslaget gäller.

11. **Följer efters sträckor sträckorna i före?**
    - Förslag: nej. Efter har egna sträckor, som kopieras med tabellen. Knappen "Sträckorna som i före" hämtar dem igen utan att röra raderna.
    - Alternativ: en ruta "Samma sträckor som i före".

    **Svar:** förslaget gäller.

12. **Påverkar stolparna utanför sträckan något?**
    - Förslag: nej. Startstolpens Spannlängd föreg. ingår inte heller, som första raden i formuläret Normalspann.

    **Svar:** förslaget gäller.

13. **Hur matchas efter mot före, och vad gäller när Stolpplats ändras eller finns två gånger?**
    - Förslag: bara exakt samma Stolpplats matchar. Texten trimmas, och stora och små bokstäver skiljs.
      - En rad som har bytt Stolpplats blir Ny.
      - Ingen matchning görs på radnummer eller koordinater.
      - Skrivs en Stolpplats om i en cell följer start och slut med.
      - En Stolpplats som finns på flera rader i samma tabell markeras röd.
      - En sträcka som börjar eller slutar på en sådan Stolpplats får ett fel.
      - En rad i efter som matchar en Stolpplats som finns flera gånger i före markeras som osäker.
    - Alternativ: koordinaterna N och E som reserv.

    **Svar:** förslaget gäller.

14. **Hur märks de automatiska beräkningarna i Dokumentation och flikarna?**
    - Förslag:
      - Bilden `StockIcon::Sync` i stället för Σ.
      - Tooltipen "Automatisk ur Stolptabell före: …".
      - De automatiska posterna står först i flikarna, med lila eller rosa ton och Indata "Automatisk".
      - Ta bort är släckt, Ändra… heter Visa…, och Kopiera… fungerar.
      - Referens lämnas som användaren skrev den.

    **Svar:** förslaget gäller, med starkare märkning i flikarna. Varje automatisk post har:
    - bilden `Sync` i radens bildkolumn,
    - texten "Automatisk" i kolumnen Indata, som är ny i Normalspann,
    - anteckningen "Automatisk · Stolptabell före · faslinan ST0102–ST0107",
    - lila eller rosa ton,
    - formulärets titel "(automatisk)" (9.2, 10).

---

## 2. Mål och avgränsning

### Kraven
| # | Krav (användaren) | Var |
|---|---|---|
| K1 | När Stolptabell före har alla stolpar som behövs går Automatiskt läge att välja. | 1.1, 8.5 |
| K2 | Bland stolparna anges var faslinan börjar och slutar, till exempel STO150–STO350 i en tabell med STO100–STO400. | 1.2, 5.1, 8.5 |
| K3 | Sträckan ger automatiskt en normalspannsberäkning och en linberäkning. | 5.1, 5.2, 6.4 |
| K4 | Beräkningarna blir poster i flikarna Normalspann och Linberäkning. De syns tydligt men går inte att ändra eller ta bort. | 6.4, 9.2, 10 |
| K5 | De hör till stolptabellen, och resultatet visas i Dokumentation. | 5.4, 9.1 |
| K6 | Är topplinorna samma anges en sträcka, är de olika anges en per topplina. | 1.5, 5.3 |
| K7 | Stolptabell efter kan kopiera Stolptabell före. Det som fanns i efter rensas, sträckorna följer med och efter räknas automatiskt. | 6.3, 8.6 |
| K8 | Sträckorna i efter går att ändra. | 8.5 |
| K9 | Ändras stolpar i efter uppdateras normalspannet och linberäkningen i Dokumentation automatiskt. | 6.4, 11 |
| K10 | En annan färg visar om en rad i efter finns i före, matchad på Stolpplats. | 1.9, 1.13, 8.4 |
| K11 | Fliken Stolptabell efter står till höger om Stolptabell före. | 8.2 |
| K12 | Det manuella sättet med normalspann och linberäkning fungerar som förut. | 4.2, 9 |
| K13 | De automatiska beräkningarna sparas i projektet och hålls aktuella, så att utskrifter senare kan läsa dem. | 4.1, 6.4 |
| K14 | Versionen blir 0.5.0. | 15 |

### Ingår
- **Fliken Stolptabell efter** är en egen tabell i projektet med samma redigering som Stolptabell före. Fliken har dessutom:
  - knappen "Kopiera från Stolptabell före…",
  - knappen "Sträckorna som i före",
  - färgerna mot före.
- **Automatiskt läge per stolptabell:**
  - en sträcka per lina,
  - automatiska normalspanns- och linberäkningar, som sparas i projektet som vanliga beräkningar märkta som automatiska och hålls aktuella i samma transaktion som varje ändring de bygger på,
  - visning i Dokumentation och i flikarna, skrivskyddat och tydligt märkt,
  - markeringar i tabellen.
- **Två nya bibliotek:**
  - `libs/autoMode`: rena funktioner.
  - `libs/poleGrid`: stolptabellen i en DataGrid. Huvudfönstrets kod för Stolptabell före flyttas dit.
- **Projektdatabasen:** steg 5 med migreringstest från 0.4.0.
- **Formulären** Normalspann och Linberäkning visar de automatiska beräkningarna skrivskyddade.
- **Tester** i `tests/`.

### Ingår inte
- Utskrifterna. De kommer senare och läser de lagrade beräkningarna.
- "Hämta från stolptabellen" i normalspannsformuläret.
- Beräkningar ur koordinaterna eller ur Av och Ah.
- Gränser och läge som går att ändra i de automatiska beräkningarna (fråga 7).
- Sammanslagning vid en ny kopiering (fråga 10) och matchning på koordinater (fråga 13).
- Ångra.

---

## 3. Nuläge

- **`main_window.cpp`** har 2293 rader.
  - Stolptabellens kod står på rad 513–802: `loadPoleTable`, `fillPoleGrid`, `decoratePoleRow`, `poleTone`, `stolpGrid…`-händelserna, `PoleSheetHost`, `applyPoleEdits`, `savePoleTable`, `insertPoleRowAbove`, `removePoleRowsAsked` och `setPoleStatus`.
  - Medlemmarna är `poleRows_`, `poleSaved_`, `poleHost_`, `poleSheet_` och `alive_`.
  - `postIfAlive` används bara av stolptabellen.
- **Dokumentation:**
  - Raderna står i `kByggPmRows`. Normalspannsraderna (`ns`) och linberäkningsraderna (`lc`) har var sin `Wire` och `Stage`.
  - `showNormalSpanRows` och `showLineCalcRows` hittar den visade beräkningen genom `docKeys` (`linkedSpanIndex`, `linkedLineCalcIndex`).
  - `byggPmSplit_` är falsk när Topplina höger är "Samma".
- **Kopplade linberäkningar:**
  - `docSource` är radnyckeln.
  - `resolveLineInput` hämtar linan, aₙ, σ₀, min och max ur Dokumentation.
  - `docLineSources` räknar aₙ för varje kopplad normalspannsrad.
  - `addLinkedLineCalc` skapar en kopplad beräkning med `newLineInput()`.
  - En kopplad beräkning lagrar NULL i `cond_*`, `normal_span_m` och `tension_mpa`.
- **Normalspannet:** `calculateNormalSpan`.
  - Spannet på första stolpen ingår inte.
  - Helt tomma rader hoppas över.
  - `saveNormalSpan` sparar inte helt tomma stolpar.
- **`libs/projectdb`:**
  - Schemat har steg 1–4, `kSchemaVersion` = 4 och tio tabeller.
  - Steg 4 (`pole_before`) är committat i 0.4.0 (47c6c74) och ändras aldrig.
  - `normal_span` och `line_calc` har `AUTOINCREMENT`, så SQLite återanvänder aldrig ett borttaget id.
  - `storeLineCalc` kan skriva med ett givet id (`ON CONFLICT(id)`).
  - `setPolesBefore` svarar "Stolptabellen har ett ogiltigt tal …".
- **Formulären:**
  - `NormalSpanDialog` och `LineCalcDialog` läser med `normalSpan(id)` respektive `lineCalc(id)` och `docLineSources`.
  - Båda har ett skrivskyddat läge.
  - Skrivskyddat fungerar `diffChk` och i-knappen fortfarande i Linberäkning.
- **GlazeUI v0.11.1:**
  - `Checkbox` och `Dropdown` fyrar `onChange` bara för användarens ändringar.
  - En `Dropdown` går inte att skriva i.
  - `GroupBox` tar sin storlek ur barnen.
  - DataGrid har `setCellColor`, `setCellToolTip` och `setCellImage`.
  - ListView har radens bild i kolumn 0 (`Row::img`).
  - `setRows` nollar cellinställningarna.
  - `messageBox` har ingen förvald knapp: första knappen, Ja, är förvald.
  - `AppWindow::commitEdits()` är publik.

---

## 4. Beslut med motivering

### 4.1 De automatiska beräkningarna sparas och hålls aktuella av `projectdb`
- **Lagringen:** varje automatisk beräkning är en vanlig rad.
  - Normalspannet ligger i `normal_span` med stolparna i `normal_span_pole`.
  - Linberäkningen ligger i `line_calc` med lastfallen i `line_calc_case`.
  - Tabellen `auto_calc` (6.2) märker raderna som automatiska.
- **Synkningen:** en enda intern funktion, `syncAutoCalcs`, skapar, uppdaterar och tar bort raderna så att de stämmer med stolptabellerna, sträckorna, läget och Topplina höger.
  - Den körs i samma transaktion av varje skrivning som kan ändra dem (6.4).
  - Det som lagras är alltså aldrig ur takt med det som räknas ur tabellen.
- **Utskrifterna** läser dem som andra lagrade beräkningar. Märkningen i `auto_calc` och anteckningen (7.7) säger vad de är.
- **Resultatet lagras inte**, som för alla beräkningar. Indata lagras, alltså stolparna och linberäkningens förval.
  - aₙ räknas ur de lagrade stolparna.
  - Linberäkningens lina, aₙ, σ₀, min och max löses upp vid läsningen, som för en kopplad beräkning.
  - Utskrifterna gör likadant. Att lagra de upplösta värdena skulle göra den automatiska linberäkningen till ett undantag bland de kopplade och kräva synkning också vid ändringar i Dokumentation.
- **Bortvalt alternativ:** beräkningar som bara räknas vid läsningen (det förra utkastet). Utskrifterna behöver lagrade beräkningar.

### 4.2 Raderna visar de automatiska beräkningarna, och lagrade kopplingar vilar
- **Vad raderna visar:** när läget är på för ett skede visar varje aktiv lina (5.3) sin automatiska beräkning på normalspanns- och linberäkningsraden.
- **Var kopplingen finns:** den automatiska kopplingen lagras inte i `doc_normal_span` eller `doc_line_calc`. Den följer av `auto_calc`.
  - Där har `doc_key` primärnyckel, och den lagrade kopplingen på samma rad ska vila och finnas kvar.
  - Lagrade kopplingar och manuella värden i `doc_value` ändras aldrig av läget.
- **Läsningarna:** de lagrade beräkningarnas `docKeys` saknar en rad så länge raden visar en automatisk beräkning.
  - Varje rad finns alltså bara i en beräknings `docKeys`.
  - Fönstrets kod (`linkedSpanIndex`, `linkedLineCalcIndex`, `shortRowsText`) fungerar oförändrad.

### 4.3 Den automatiska linberäkningen är en kopplad linberäkning med förvalen
- Den har `docSource` = linberäkningsraden och `input = linecalc::newLineInput()` när den skapas. Indata ändras inte därefter.
- Upplösningen är densamma som för "Lägg till" i dag. Skillnaden är att aₙ kommer ur den automatiska normalspannsberäkningen, eftersom `docLineSources` ger den på normalspannsraden (6.4).

### 4.4 Stabila id per skede och lina
- **Reserveringen:** första gången en lina i ett skede blir automatisk får normalspannet och linberäkningen nya id med `AUTOINCREMENT`. De sparas i `auto_calc`, en rad per skede och lina.
- **Ändringar uppdaterar på plats:** samma id står kvar, och `created_utc` står kvar.
- **Läget av** tar bort raderna i `normal_span` och `line_calc`, men raden i `auto_calc` står kvar med id:na.
- **Läget på** skapar raderna igen med samma id, uttryckligen angivet i `INSERT`, och med värden ur tabellen som den är då.
- **Inga krockar:** `AUTOINCREMENT` återanvänder aldrig ett id, så en vanlig beräkning som skapades medan läget var av kan inte ha fått det.
- **Bedömningen:** id:t består också över av och på. Då kan en utskrift eller en mall som pekar på "faslinan före" behålla sitt id, och det kostar bara en tabell utan främmande nycklar.

### 4.5 Inställningen ligger i `auto_mode`, märkningen i `auto_calc`
- **`auto_mode`** har en rad per skede: läget och tre sträckor som Stolpplats-text. Texten följer Stolpplats och inte radnumret.
- **`auto_calc`** har en rad per skede och lina:
  - id:na,
  - sträckans rader och fel som de senast lästes (`first_row`, `last_row`, `issue`).
- **Främmande nycklar** saknas med avsikt, eftersom raderna i `normal_span` och `line_calc` tas bort när läget är av.

### 4.6 Stolptabell efter har en egen tabell
- Tabellen `pole_after` är en kopia av `pole_before`, som det beslutades i planen för Stolptabell före (svar 8).
- API:t får `poles(p, stage)` och `setPoles(p, stage, rows)`. `polesBefore` och `setPolesBefore` finns kvar.

### 4.7 Jämförelsen mellan efter och före räknas och lagras inte
- Jämförelsen matchar på Stolpplats (fråga 13).
- Den räknas om efter varje ändring i någon av tabellerna.

### 4.8 Två nya bibliotek
- **`libs/autoMode` (`kalkyl_automode`, rent):**
  - sträckan i tabellen och normalspannets stolpar,
  - panelens text och dubbletterna,
  - jämförelsen och markeringarna.
  - `projectdb` (synkningen) och huvudfönstret (panelen och tabellen) använder samma funktioner, så de visar alltid samma sak.
- **`libs/poleGrid` (`kalkyl_polegrid`, GlazeUI):** stolptabellen i en DataGrid.
  - Den innehåller radmodellen i fönstret, Excel-markeringen via `sheetGrid`, papperskorgen, snabbmenyn och färgerna.
  - Huvudfönstrets kod för Stolptabell före flyttas hit och används av båda tabellerna. Det följer förebilden `sheetGrid`.
  - Inga layouter skrivs i kod.

### 4.9 Omräkningen sker direkt
- Synkningen körs i skrivningens transaktion på UI-tråden.
- Därefter läser fönstret om beräkningarna (avsnitt 11).
- Med 1000 stolpar tar det millisekunder, och stolparna skrivs bara om när de har ändrats.

### 4.10 Spärrarna ligger i `projectdb`
- `projectdb` vägrar ändra, ta bort eller koppla om de automatiska beräkningarna och raderna de visar (7.5).
- Bara synkningen skriver dem.

---

## 5. Indata till de automatiska beräkningarna

### 5.1 Normalspannet ur stolptabellen
Stolparna är raderna från raden med Stolpplats `fromPole` till raden med `toPole`, båda medräknade, i tabellens ordning (`automode::rangeSpan`).

| Normalspannet (`ancalc::SpanPole`) | Stolptabellen (`poletable::PoleRow`) | Anmärkning |
|---|---|---|
| `pole` (Stpnr) | `pole` (Stolpplats) | |
| `lengthM` (spannet före stolpen) | `spanM` (Spannlängd föreg.) | Startstolpens spann ingår inte (fråga 12) |
| `heightM` (H) | `height` (Koordinat H) | Valfri. aₙ,ΔH räknas bara när alla stolpar har H |
| gränserna | `ancalc::SpanLimits{}` (Excels förval) | Går inte att ändra (fråga 7) |
| – | Av, Ah, Koordinat N och E, LKL | Används inte (fråga 8) |

- **Talen** läses med `poletable::numberValue`. Tom text blir ett tomt tal.
- **Vad som lagras:** de lagrade stolparna är `rangeSpan().poles` utan helt tomma stolpar, med samma regel som `saveNormalSpan`.
  - Det ger samma beräkning som i tabellen, eftersom `calculateNormalSpan` också hoppar över tomma rader.
  - En rad med bara Av, Ah, N, E eller LKL blir en tom stolpe och sparas inte.
- **Felen** är `calculateNormalSpan`s egna.
  - Har sträckan ett eget fel (7.1) sparas inga stolpar.
  - Felet sparas då i `auto_calc.issue`.

### 5.2 Linberäkningen ur Dokumentation
| Indata | Varifrån | Rad i Dokumentation |
|---|---|---|
| Linan med lindata | Projektets kopia av linan på linraden | Faslina, Topplina vänster eller Topplina höger. Höger med Samma löses upp som vänster |
| aₙ | Den automatiska normalspannsberäkningen för samma lina och skede, avrundad till 0,1 m | Normalspannsraden |
| σ₀ | Uppspänningsraden | Uppspänning fas, topp (vänster) eller topp höger |
| Min och max | Min temp och Max temp, med arkets följdändringar | De gula raderna |
| T₀, linhöjd, vind, konstanter, lastfall och läge | `linecalc::newLineInput()`, lagrat när beräkningen skapas | – |

- **Värden som saknas** ger samma upplysning som för en kopplad beräkning i dag.
- **När normalspannsberäkningen har fel** gäller texten i 7.5.

### 5.3 Vilka linor som är automatiska
- **Aktiva linor:** för ett skede med läget på är `Wire::Phase` och `Wire::TopLeft` alltid aktiva. `Wire::TopRight` är aktiv bara när topplina höger är en egen lina.
- **"Samma"** betyder att `doc_value.top_conductor_right` är tomt eller "Samma", som i `resolveLineInput`.
- **Topplinorna med Samma:** `TopLeft` är Topplinor och räknas till raderna "Topplinor …".
- **Linans namn:**

  | Var | Faslinan | Topplinor (Samma) | Vänster | Höger | Funktion |
  |---|---|---|---|---|---|
  | Panelen | Faslina | Topplinor | Topplina vänster | Topplina höger | `projectdb::autoLineLabel` |
  | Texterna | faslinan | topplinorna | topplina vänster | topplina höger | `projectdb::autoLineWord` |

### 5.4 Dokumentationens rader i automatiskt läge
| Skede och lina | Normalspannsrad | Linberäkningsrad |
|---|---|---|
| Före, Phase | `ns_calc_phase_before` | `line_calc_phase_before` |
| Före, TopLeft | `ns_calc_top_left_before` | `line_calc_top_left_before` |
| Före, TopRight | `ns_calc_top_right_before` | `line_calc_top_right_before` |
| Efter, Phase | `ns_calc_phase_after` | `line_calc_phase_after` |
| Efter, TopLeft | `ns_calc_top_left_after` | `line_calc_top_left_after` |
| Efter, TopRight | `ns_calc_top_right_after` | `line_calc_top_right_after` |

- **Tagen rad:** en rad som visar en automatisk beräkning kallas tagen. Det gäller när beräkningens rader finns i `normal_span` och `line_calc`.
- **Id:na** står i `auto_calc` (4.4).
- **Uppgifterna enligt ByggPM** påverkas inte, till exempel den gröna raden "Normalspann faslina".

---

## 6. API (bindande)

### 6.1 `libs/autoMode/auto_mode.h` (nytt bibliotek `kalkyl_automode`, namnrymden `kalkyl::automode`)
```cpp
#pragma once
#include <array>
#include <string>
#include <string_view>
#include <vector>

#include "anCalc/normal_span.h"
#include "poleTable/pole_table.h"

namespace kalkyl::automode {

// En sträcka i en stolptabell: Stolpplats för första och sista stolpen, trimmade.
// Tom text = stolpen är inte vald.
struct PoleRange {
    std::string fromPole;
    std::string toPole;
    bool operator==(const PoleRange&) const = default;
};

// Var sträckan ligger i tabellen: första och sista raden, 0-baserade och båda med. issue är
// felet när sträckan inte går att använda (planen 7.1), och då är first och last -1.
struct RangeRows {
    int first = -1;
    int last = -1;
    std::string issue;
};

// Söker sträckan bland raderna. word är linans ord i texterna ("faslinan", "topplinorna",
// "topplina vänster", "topplina höger") och table tabellens namn ("Stolptabell före").
// Stolpplats jämförs exakt efter trimning. Felen prövas i ordningen i planen 7.1.
RangeRows findRange(const std::vector<poletable::PoleRow>& rows, const PoleRange& range,
                    std::string_view word, std::string_view table);

// Raderna first..last som normalspannets stolpar, i tabellens ordning: Stolpplats som Stpnr,
// Spannlängd föreg. som spannlängd och Koordinat H som H (poletable::numberValue). Tomma tal
// blir tomma. first < 0, last utanför tabellen eller first > last ger en tom lista.
std::vector<ancalc::SpanPole> spanPoles(const std::vector<poletable::PoleRow>& rows, int first, int last);

// Normalspannet för en sträcka: var den ligger, stolparna och beräkningen. Har sträckan fel är
// poles tom, och calc har inget resultat och bara sträckans fel (Error, poleIndex -1).
struct RangeSpan {
    RangeRows rows;
    std::vector<ancalc::SpanPole> poles;
    ancalc::SpanCalculation calc;
};
RangeSpan rangeSpan(const std::vector<poletable::PoleRow>& rows, const PoleRange& range,
                    std::string_view word, std::string_view table,
                    const ancalc::SpanLimits& limits = {});

// Panelens text för en sträcka (planen 7.2): "5 spann · aₙ 315,2 m" med aₙ,ΔH och avvikelser
// när de finns, eller det första felet med stolpens Stolpplats ("ST0105: Spannlängd saknas.").
struct RangeStatus {
    bool ok = false;
    std::string text;
};
RangeStatus rangeStatus(const RangeSpan& span, const std::vector<poletable::PoleRow>& rows);

// Stolpplatserna som finns på mer än en rad, i första förekomstens ordning. Tom Stolpplats räknas inte.
std::vector<std::string> duplicatePoles(const std::vector<poletable::PoleRow>& rows);

// Hur en rad i Stolptabell efter förhåller sig till Stolptabell före, matchad på Stolpplats.
enum class RowState {
    Empty,       // helt tom rad
    Same,        // Stolpplats finns på en rad i före, och alla fält är lika
    Changed,     // Stolpplats finns på en rad i före, men minst ett fält skiljer sig
    New,         // Stolpplats finns inte i före, eller raden saknar Stolpplats
    Ambiguous,   // Stolpplats finns på flera rader i före
};

// Jämförelsen för en rad: läget, raden i före (-1 utan en enda träff) och fälten som skiljer sig.
struct RowCompare {
    RowState state = RowState::Empty;
    int beforeRow = -1;
    std::array<bool, poletable::kColumnCount> changed{};
    bool operator==(const RowCompare&) const = default;
};

// En jämförelse per rad i after. Talfälten jämförs som tal (44,10 och 44,1 är lika, och tomt är
// bara lika med tomt), textfälten exakt.
std::vector<RowCompare> compareAfter(const std::vector<poletable::PoleRow>& before,
                                     const std::vector<poletable::PoleRow>& after);

// En sträcka med linans ord, till markeringarna i tabellen. word börjar med en ASCII-bokstav,
// som tableMarks gör versal i början av en mening ("Faslinan börjar här.").
struct NamedRange {
    std::string word;
    PoleRange range;
};

// Ett fel i en cell: raden, kolumnen 0-7 och texten.
struct CellIssue {
    int row = -1;
    int column = -1;
    std::string message;
    bool operator==(const CellIssue&) const = default;
};

// Markeringarna i en stolptabell: per rad en text om de sträckor som börjar eller slutar där
// (tom = ingen), och felen per cell, alltså dubbletter av Stolpplats och normalspannets fel i en sträcka.
struct TableMarks {
    std::vector<std::string> rowNotes;
    std::vector<CellIssue> issues;
};

// Markeringarna för raderna och sträckorna (planen 7.3). Utan sträckor blir det bara dubbletterna.
// En sträcka med fel ger inga markeringar, eftersom felet visas i panelen.
TableMarks tableMarks(const std::vector<poletable::PoleRow>& rows, const std::vector<NamedRange>& ranges,
                      std::string_view table);

} // namespace kalkyl::automode
```

`libs/autoMode/CMakeLists.txt`:
```cmake
# Automatiskt läge som rena funktioner: sträckorna i stolptabellen, normalspannets stolpar och
# jämförelsen mellan Stolptabell före och efter. Includes skrivs som "autoMode/auto_mode.h".
add_library(kalkyl_automode STATIC auto_mode.cpp)
target_include_directories(kalkyl_automode PUBLIC ${PROJECT_SOURCE_DIR}/libs)
target_link_libraries(kalkyl_automode PUBLIC kalkyl_poletable kalkyl_ancalc
                      PRIVATE kalkyl_tabletext kalkyl_warnings)
```

**Om implementationen:**
- **`libs/CMakeLists.txt`** får `add_subdirectory(autoMode)` direkt efter `anCalc` och `add_subdirectory(poleGrid)` sist, efter `sheetGrid`.
- **`rangeSpan`** anropar `findRange` och sedan `spanPoles` och `ancalc::calculateNormalSpan`. Vid fel byggs `calc` för hand: `{std::nullopt, {{IssueKind::Error, -1, SpanColumn::None, issue}}}`.
- **`compareAfter`** jämför talfält med `poletable::numberValue` och textfält med `==`.
- **Talen i texterna** skrivs med `tabletext::formatDecimal(v, 1, ',')`.

### 6.2 `libs/projectdb`: steg 5
Steget läggs sist i `dev_migration.h`, efter `kStep4`. Steg 4 rörs inte.
```cpp
// Steg 5 (user_version 4 -> 5): Stolptabell efter, automatiskt läge och märkningen av de
// automatiska beräkningarna. IF NOT EXISTS som i steg 2. De automatiska beräkningarna är vanliga
// rader i normal_span och line_calc; auto_calc säger vilka och reserverar deras id.
inline constexpr std::string_view kStep5 = R"sql(
-- Fliken Stolptabell efter: som pole_before, en rad per stolpe i tabellens ordning
-- (position 0, 1, 2 ...), också tomma rader mitt i tabellen. Talen är kanonisk decimaltext.
-- NULL = tom cell.
CREATE TABLE IF NOT EXISTS pole_after (
    position INTEGER PRIMARY KEY,
    pole     TEXT    NOT NULL DEFAULT '',
    av       TEXT,
    ah       TEXT,
    span_m   TEXT,
    coord_n  TEXT,
    coord_e  TEXT,
    coord_h  TEXT,
    lkl      TEXT    NOT NULL DEFAULT ''
);
-- Automatiskt läge per stolptabell: på eller av, och var faslinan och topplinorna börjar och
-- slutar (Stolpplats, '' = inte vald). top_left är Topplinor när topplina höger är Samma.
-- Saknas raden är läget av och inga sträckor valda.
CREATE TABLE IF NOT EXISTS auto_mode (
    stage          TEXT    PRIMARY KEY NOT NULL CHECK (stage IN ('before', 'after')),
    enabled        INTEGER NOT NULL DEFAULT 0 CHECK (enabled IN (0, 1)),
    phase_from     TEXT    NOT NULL DEFAULT '',
    phase_to       TEXT    NOT NULL DEFAULT '',
    top_left_from  TEXT    NOT NULL DEFAULT '',
    top_left_to    TEXT    NOT NULL DEFAULT '',
    top_right_from TEXT    NOT NULL DEFAULT '',
    top_right_to   TEXT    NOT NULL DEFAULT ''
);
-- De automatiska beräkningarna: en rad per skede och lina som någon gång har varit automatisk.
-- span_id och calc_id är id i normal_span och line_calc. De är reserverade för skedet och linan:
-- raderna där finns bara när linan är automatisk och skapas igen med samma id. Ingen främmande
-- nyckel, eftersom raderna tas bort när läget är av. first_row, last_row och issue är sträckan
-- som den senast lästes ur stolptabellen (-1 och felets text när den inte går att använda).
CREATE TABLE IF NOT EXISTS auto_calc (
    stage     TEXT    NOT NULL CHECK (stage IN ('before', 'after')),
    wire      TEXT    NOT NULL CHECK (wire IN ('phase', 'top_left', 'top_right')),
    span_id   INTEGER NOT NULL UNIQUE,
    calc_id   INTEGER NOT NULL UNIQUE,
    first_row INTEGER NOT NULL DEFAULT -1,
    last_row  INTEGER NOT NULL DEFAULT -1,
    issue     TEXT    NOT NULL DEFAULT '',
    PRIMARY KEY (stage, wire)
);
)sql";
```

- **`project_db.h`:** `kSchemaVersion = 5`.
- **`project_db.cpp`:**
  - `s.steps = {kSchemaV1, dev::kStep2, dev::kStep3, dev::kStep4, dev::kStep5}`.
  - `kExpectedTablesSql` får `'pole_after', 'auto_mode', 'auto_calc'`, och `kExpectedTableCount = 13`.
- **`CMakeLists.txt`:** `kalkyl_automode` läggs till som PUBLIC.

### 6.3 `libs/projectdb/project_data.h`, tillägg
Tilläggen görs med `#include <array>` och `#include "autoMode/auto_mode.h"`.
- `AutoOrigin` står före `NormalSpan`.
- `automatic` läggs sist i `NormalSpan` och `LineCalc`, och `autoSpanKeys` sist i `DocLineSources`.
- Kommentarerna på `normalSpans` och `lineCalcs` ändras till ordningen i 6.4.

```cpp
// Var en automatisk beräkning kommer ifrån (planen Automatiskt läge, 4.1). Satt bara på de
// automatiska, som projectdb skapar, håller aktuella och tar bort (syncAutoCalcs).
struct AutoOrigin {
    Stage stage = Stage::Before;
    Wire wire = Wire::Phase;
    automode::PoleRange range;
    int firstRow = -1;      // sträckans första rad i stolptabellen, -1 när sträckan har fel
    int lastRow = -1;       // sträckans sista rad, -1 när sträckan har fel
    std::string issue;      // sträckans fel (automode::findRange), tomt när den gick att läsa
};

// I NormalSpan och LineCalc, sist:
    std::optional<AutoOrigin> automatic;        // satt för en automatisk beräkning

// I DocLineSources, sist:
    // Normalspannsnycklarna vars aₙ kommer ur en automatisk beräkning (för upplysningens text)
    std::vector<std::string> autoSpanKeys;

// Räknar normalspannet för en beräkning: sträckans fel för en automatisk beräkning vars sträcka
// inte går att använda, annars ancalc::calculateNormalSpan på stolparna och gränserna.
ancalc::SpanCalculation calculateSpan(const NormalSpan& ns);

// Tabellens namn i texterna: "Stolptabell före" eller "Stolptabell efter".
std::string_view poleTableName(Stage stage);

// Linans ord i automatiska lägets texter: "faslinan", "topplinorna" (same: topplina höger är
// Samma), "topplina vänster" eller "topplina höger".
std::string autoLineWord(Wire wire, bool same);

// Linans rubrik i panelen: "Faslina", "Topplinor" (same), "Topplina vänster" eller "Topplina höger".
std::string autoLineLabel(Wire wire, bool same);

// Stolptabellen för skedet i tabellens ordning. polesBefore(p) är poles(p, Stage::Before).
core::Result<std::vector<poletable::PoleRow>> poles(ProjectDb& p, Stage stage);
// Ersätter hela stolptabellen för skedet i EN transaktion, med samma regler som setPolesBefore,
// som är setPoles(p, Stage::Before, rows). De automatiska beräkningarna följer med i samma
// transaktion. Fel: AccessDenied, Invalid ("Stolptabell före har ett ogiltigt tal på rad <n> i
// kolumnen <namn>." respektive "Stolptabell efter …"), Sql.
core::Result<void> setPoles(ProjectDb& p, Stage stage, const std::vector<poletable::PoleRow>& rows);

// Automatiskt läge för en stolptabell: på eller av, och en sträcka per lina i Wire-ordningen
// (Phase, TopLeft, TopRight). TopLeft är Topplinor när topplina höger är Samma. TopRight används
// bara när topplina höger är en egen lina, men sparas ändå.
struct AutoSettings {
    bool enabled = false;
    std::array<automode::PoleRange, 3> ranges;
    bool operator==(const AutoSettings&) const = default;
};
// Inställningen för skedet. Utan rad i auto_mode: av och inga sträckor.
core::Result<AutoSettings> autoSettings(ProjectDb& p, Stage stage);
// Sparar inställningen (Stolpplats trimmas), och de automatiska beräkningarna följer med i samma
// transaktion. Fel: AccessDenied, Sql.
core::Result<void> setAutoSettings(ProjectDb& p, Stage stage, const AutoSettings& s);
// Stolptabell efter blir en kopia av Stolptabell före: alla rader, sträckorna och läget, i EN
// transaktion med de automatiska beräkningarna. Det som fanns i efter försvinner.
// Fel: AccessDenied, Sql.
core::Result<void> copyBeforeToAfter(ProjectDb& p);
```

### 6.4 `libs/projectdb`: beteendet

**Vilka beräkningar som är automatiska**
- En normalspannsberäkning är automatisk när dess id är `span_id` i `auto_calc`.
- En linberäkning är automatisk när dess id är `calc_id`.
- Raderna i `normal_span` och `line_calc` finns bara när linan är aktiv (5.3).
- En rad är tagen när dess skede och lina har en automatisk rad i `normal_span`.

**Synkningen `syncAutoCalcs(db::Database& d, const std::string& now)`**
- Den är intern i `project_data.cpp` och har en kommentar på 2–5 rader.
- Den körs sist i samma transaktion, efter skrivningens egen ändring, av:
  - `setPoles`, och därmed `setPolesBefore`,
  - `setAutoSettings`,
  - `copyBeforeToAfter`,
  - `setDocValue` och `setDocConductor` när nyckeln är `top_conductor_right`, eftersom aktiva linor beror på Samma (5.3).
- **Den läser:**
  - `auto_mode` för båda skedena,
  - båda stolptabellerna,
  - `doc_value.top_conductor_right`,
  - `auto_calc`.
- **För varje skede och lina (sex fall):**
  1. **Inte aktiv:** finns en rad i `auto_calc` tas `normal_span` med `span_id` och `line_calc` med `calc_id` bort. Kaskaden tar stolparna och lastfallen. Raden i `auto_calc` står kvar (4.4).
  2. **Aktiv:** räkna `span = automode::rangeSpan(rader, sträcka, autoLineWord(wire, same), poleTableName(stage))`. Stolparna är `span.poles` utan helt tomma stolpar (5.1), och anteckningen är texten i 7.7.
  3. **Utan rad i `auto_calc`:**
     - `INSERT` i `normal_span` (id ur `AUTOINCREMENT`) med förvalda gränser, anteckningen och `created_utc = modified_utc = now`.
     - `storeLineCalc` med `id = 0`, `input = newLineInput()`, `docSource = lineCalcDocKey(wire, stage)` och anteckningen.
     - `INSERT` i `auto_calc` med de två nya id:na.
  4. **Med rad i `auto_calc` men utan rad i `normal_span` eller `line_calc`** (läget har slagits på igen): samma skrivningar som i 3, men med id:na ur `auto_calc` angivna uttryckligen (`INSERT INTO normal_span(id, …)` respektive `storeLineCalc` med `lc.id = calc_id`).
  5. **Jämför och skriv bara det som skiljer sig:**
     - Stolparna i `normal_span_pole`: hela listan ersätts när Stpnr, spannlängd eller H skiljer sig i någon stolpe, eller när antalet skiljer sig.
     - Anteckningen och gränserna i `normal_span`. `modified_utc = now` sätts bara när något i raden eller stolparna ändrades.
     - Anteckningen i `line_calc`, med `modified_utc` på samma sätt. Linberäkningens indata ändras inte efter att den skapats.
     - `first_row`, `last_row` och `issue` i `auto_calc`.
- **Regel:** en framtida skrivning som ändrar något som synkningen läser ska också köra den. Regeln står i `libs/projectdb/CLAUDE.md`.

**Läsningarna**
- **`normalSpans(p)`:**
  - Först de automatiska, för före och sedan efter, och i varje skede Phase, TopLeft och TopRight.
  - Sedan de övriga efter id.
  - En automatisk beräkning har `docKeys = {normalSpanDocKey(wire, stage)}` och `automatic` ur `auto_calc` och `auto_mode`.
  - De övrigas `docKeys` saknar tagna rader.
- **`normalSpan(p, id)`:** som den lagrade beräkningen, med `automatic` och `docKeys` enligt samma regel.
- **`lineCalcs(p)` och `lineCalc(p, id)`:** på samma sätt, med `docKeys = {lineCalcDocKey(wire, stage)}` för de automatiska.
- **Filtreringen** görs i de publika läsningarna, inte i de interna läsningarna inne i skrivningarna.
- **`calculateSpan(ns)`:** är `ns.automatic` satt och `issue` inte tom blir svaret `{nullopt, {{Error, -1, None, issue}}}`. Annars `calculateNormalSpan(ns.poles, ns.limits)`.
- **`docLineSources(p)`:**
  - För varje tagen normalspannsrad ersätts posten i `normalSpans` av den automatiska beräkningens aₙ, ur `calculateSpan`: `result->an`, eller tomt vid fel.
  - En lagrad koppling på samma rad räknas inte.
  - Nyckeln läggs i `autoSpanKeys`.
  - Interna `readLineSources` använder `calculateSpan`.
- **`resolveLineInput`:** saknar den kopplade normalspannsraden aₙ och finns nyckeln i `docs.autoSpanKeys`, blir texten den i 7.5. Annars är allt som i dag.

**De andra skrivningarna**
- **`setPoles`** är koden i `setPolesBefore` med tabellnamnet efter skedet. `setPolesBefore` anropar `setPoles(p, Stage::Before, rows)`.
- **`setAutoSettings`:** en upsert av raden för skedet. Texterna trimmas med `tabletext::trimmed`. Stolpplats valideras inte, eftersom felen hamnar i `auto_calc.issue`.
- **`copyBeforeToAfter`:** i en transaktion `DELETE FROM pole_after` och sedan `INSERT INTO pole_after(...) SELECT ... FROM pole_before`. Därefter `DELETE FROM auto_mode WHERE stage = 'after'` och `INSERT INTO auto_mode SELECT 'after', enabled, ... FROM auto_mode WHERE stage = 'before'`. Sist körs synkningen.

**Spärrarna** (texterna i 7.5) ger `Invalid` och ändrar ingenting:
- **T1:** `saveNormalSpan` och `linkNormalSpan` med ett automatiskt id, och `saveLineCalc`, `linkLineCalc` och `detachLineCalc` med ett automatiskt id.
- **T2:** `removeNormalSpan` och `removeLineCalc` med ett automatiskt id.
- **T3:**
  - `saveNormalSpan` med en tagen `linkDocKey`.
  - `linkNormalSpan` och `unlinkDocRow` på en tagen rad.
  - `saveLineCalc` med en tagen `linkDocKey`, eller med `lc.id == 0` och en tagen `docSource`.
  - `linkLineCalc` på en tagen rad.
- **Tillåtet:**
  - Att spara en lagrad kopplad beräkning vars rad är tagen, när `lc.id` inte är automatiskt. Kopplingen vilar.
  - `setDocValue` på en tagen rad, eftersom värdet vilar.

### 6.5 `libs/poleGrid/pole_grid.h` (nytt bibliotek `kalkyl_polegrid`, namnrymden `kalkyl::polegrid`)
```cpp
#pragma once
#include <glaze/glaze.h>

#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "autoMode/auto_mode.h"
#include "poleTable/pole_table.h"

namespace kalkyl::polegrid {

// Radera-kolumnen: kolumnen efter stolptabellens åtta datakolumner.
inline constexpr int kDeleteColumn = poletable::kColumnCount;

// Det PoleGrid frågar fönstret om. Fönstret äger projektet, sparar och räknar om.
class PoleGridHost {
public:
    virtual ~PoleGridHost() = default;
    // Sparar hela tabellen i projektet. Svarar om det lyckades. Vid fel har värden visat felet,
    // och PoleGrid läser om tabellen med reloadPoles (postat).
    virtual bool savePoles(const std::vector<poletable::PoleRow>& rows) = 0;
    // Raderna som de står i projektet, efter ett fel vid sparningen.
    virtual std::vector<poletable::PoleRow> reloadPoles() = 0;
    // Tabellen är sparad med en ändring: värden räknar om och sätter nya färger (setLooks).
    virtual void polesChanged() = 0;
    // En Stolpplats skrevs om i en cell, från en text till en annan (båda icke-tomma). Körs efter
    // sparningen och före polesChanged, så att sträckorna kan följa med.
    virtual void poleRenamed(const std::string& from, const std::string& to) = 0;
    // Egna poster sist i snabbmenyn för markeringens första rad (-1 = ingen datarad).
    virtual void addMenuItems(glaze::Menu& menu, int row) = 0;
    // En rad till i frågan "Radera …?" för raderna first..last. Tom = ingen.
    virtual std::string removeNote(int first, int last) const = 0;
    // En kort text i statusraden.
    virtual void showStatus(const std::string& text) = 0;
};

// Färgerna och märkena i tabellen. compare och before finns bara i Stolptabell efter.
struct PoleLooks {
    std::vector<automode::RowCompare> compare;    // per rad i efter; tom i före
    std::vector<poletable::PoleRow> before;       // före-raderna, till tooltipen på en ändrad cell
    automode::TableMarks marks;                   // sträckornas början och slut och cellernas fel
};

// En cells utseende utan markering: bakgrunden, tooltipen och om Stolpplats får flaggan.
struct CellLook {
    std::optional<gfx::Color> background;
    std::string toolTip;
    bool flag = false;
};

// Cellens utseende ur märkena och jämförelsen (planen 8.4). dataRows är antalet datarader;
// raden dataRows är den tomma raden sist. Ren funktion som testas utan fönster.
CellLook cellLook(const PoleLooks& looks, int row, int column, int dataRows);

// Stolptabellen i en DataGrid: raderna som kanonisk decimaltext med den tomma raden sist,
// Excel-markeringen (sheetGrid med växande rader), papperskorgen med frågan, snabbmenyn och
// färgerna. Varje ändring sparas direkt via värden. Fönstret kopplar tabellens händelser hit.
class PoleGrid {
public:
    // owner: fönstret, för frågorna och commitEdits. tableName står i meddelandena ("Stolptabell före").
    PoleGrid(glaze::AppWindow& owner, glaze::DataGrid& grid, PoleGridHost& host, std::string tableName);
    ~PoleGrid();
    // Skrivskyddat: låsta celler, inga papperskorgar, skrivskyddad markering. Bygger om tabellen.
    void setReadOnly(bool readOnly);
    // Raderna som de lästes ur projektet. De blir också det senast sparade. Bygger om tabellen.
    void setRows(std::vector<poletable::PoleRow> rows);
    // Raderna utan den tomma raden sist.
    const std::vector<poletable::PoleRow>& rows() const;
    // Nya färger och märken. Målar om och sätter tooltips och flaggor utan att bygga om raderna.
    void setLooks(PoleLooks looks);
    // Gör raden aktuell och synlig och markerar dess Stolpplats.
    void showRow(int row);
    // Tabellens händelser.
    void committed(glaze::CellChangeArgs& e);
    void cellClicked(glaze::CellClickArgs& e);
    void keyDown(glaze::KeyArgs& e);
    void mouseUp(glaze::MouseArgs& e);
    void contextMenu(glaze::ContextMenuArgs& e);

private:
    class Adapter;   // sheetgrid::SheetHost för tabellen, definieras i pole_grid.cpp
    // ... medlemmarna och hjälpfunktionerna nedan
};

} // namespace kalkyl::polegrid
```

**De privata delarna** har en kommentar på 2–5 rader var.
- **Medlemmar:**
  - `glaze::AppWindow& owner_`, `glaze::DataGrid& grid_`, `PoleGridHost& host_` och `std::string tableName_`.
  - `bool readOnly_`.
  - `std::vector<poletable::PoleRow> rows_` och `saved_`.
  - `PoleLooks looks_`.
  - `std::unique_ptr<Adapter> adapter_` och `std::unique_ptr<sheetgrid::SheetGrid> sheet_`.
  - `std::shared_ptr<bool> alive_`.
- **Funktioner:** `fill()`, `decorateRow(int)`, `applyLooks()`, `applyEdits(...)`, `bool save()`, `insertRowAbove(int)` och `removeRowsAsked(int, int)`.

`libs/poleGrid/CMakeLists.txt`:
```cmake
# Stolptabellen i en GlazeUI DataGrid: raderna, Excel-markeringen, papperskorgen, färgerna och
# snabbmenyn. Delas av Stolptabell före och efter. Includes skrivs som "poleGrid/pole_grid.h".
add_library(kalkyl_polegrid STATIC pole_grid.cpp)
target_include_directories(kalkyl_polegrid PUBLIC ${PROJECT_SOURCE_DIR}/libs)
target_link_libraries(kalkyl_polegrid PUBLIC glaze kalkyl_sheetgrid kalkyl_poletable kalkyl_automode
                      PRIVATE kalkyl_tabletext kalkyl_warnings)
```

**Beteendet** är dagens Stolptabell före (planen för Stolptabell före, avsnitt 6 och 7), flyttat ur `main_window.cpp`.
- **Konstruktorn** skapar `SheetGrid` med samma form som i dag:
  - `columnCount = kColumnCount` och `firstColumn = 0`.
  - `isInput` för kolumnerna 0–7.
  - `accept = poletable::acceptField` och `columnName = poletable::columnName`.
  - `growRows = true` och `headerTakesRow = false`.
- **`committed`** är `stolpGridCommitted`.
  - Byter kolumn 0 text på en datarad och båda texterna är icke-tomma, anropas `host_.poleRenamed(gammal, ny)` efter en lyckad sparning.
  - Sedan anropas `host_.polesChanged()`.
- **`cellClicked`:** radera-knappen postar `removeRowsAsked` bakom `alive_`.
- **`keyDown`, `mouseUp` och `contextMenu`** går till `sheet_`.
- **Snabbmenyn:**
  1. sheetGrids poster,
  2. en separator,
  3. "Infoga rad ovanför" och "Radera rader…",
  4. `host_.addMenuItems(menu, rad)`, med markeringens första rad när den är en datarad, annars -1.
- **`save()`:**
  - Gör ingenting när `rowsToSave(rows_) == saved_`.
  - Annars anropas `host_.savePoles(rows_)`.
  - Vid fel postas `setRows(host_.reloadPoles())` bakom `alive_`.
  - Annars sätts `saved_`.
  - `polesChanged()` körs av den som anropade `save()`, och bara när något skrevs.
- **`applyEdits`, `insertRowAbove` och `removeRowsAsked`** fungerar som i dag och kör `polesChanged()` efter en lyckad sparning.
- **Frågan** i `removeRowsAsked` är dagens text. Ger `host_.removeNote` en text, läggs två `char{10}` och den texten till efter frågan.
- **`fill()`** är `fillPoleGrid`:
  1. `setRows`.
  2. `decorateRow` på varje rad.
  3. `applyLooks()`.
  4. Den aktuella cellen sätts tillbaka.
  5. `sheet_->rowsChanged()`.
- **`applyLooks()`:**
  - För varje rad och kolumnerna 0–7 körs `setCellToolTip(r, c, look.toolTip)`.
  - I kolumn 0 dessutom `setCellImage(r, 0, look.flag ? glaze::stockIcon(glaze::StockIcon::Flag) : ImageSource{})`.
  - Radera-kolumnen får bakgrunden ur `cellLook` och tooltipen "Radera raden" på datarader i ett skrivbart projekt.
  - Sist körs `sheet_->paintAll()`.
- **`Adapter::baseColor`** är `cellLook(looks_, row, column, rows_.size()).background`. `copyText` är `poletable::copyText`.
- **`postIfAlive`** finns som lokal hjälpfunktion i `pole_grid.cpp`.

---

## 7. Validering och texter (ordagrant)

Platshållarna i texterna:
- `{word}` är `autoLineWord`, till exempel "faslinan".
- `{Word}` är samma ord med versal, "Faslinan".
- `{table}` är `poleTableName`, till exempel "Stolptabell före".
- `{from}` och `{to}` är sträckans Stolpplatser.

### 7.1 Sträckans fel (`automode::findRange`)
Felen prövas i den här ordningen.
1. Start eller slut är inte valt: "Välj var {word} börjar och slutar."
2. `{from}` finns inte: "Stolpplats {from} finns inte i {table}."
3. `{to}` finns inte: "Stolpplats {to} finns inte i {table}."
4. `{from}` finns flera gånger: "Stolpplats {from} finns flera gånger i {table}."
5. `{to}` finns flera gånger: "Stolpplats {to} finns flera gånger i {table}."
6. Samma stolpe: "{Word} börjar och slutar på samma stolpe."
7. Slutet står före starten: "Slutet {to} står före starten {from} i {table}. Byt start och slut."

### 7.2 Panelens text (`automode::rangeStatus`)
- **Utan fel:** "{n} spann · aₙ {aₙ} m".
  - Finns aₙ,ΔH läggs " · aₙ,ΔH {x} m" till.
  - Finns avvikelser läggs " · 1 avvikelse" eller " · {k} avvikelser" till.
- **Med fel:** det första felet visas.
  - Har felet `poleIndex >= 0` blir texten "{Stolpplats}: {meddelande}", eller "Rad {n}: {meddelande}" utan Stolpplats.
  - Annars visas meddelandet som det är.
- **Färgen:** röd text (`gfx::Color{200, 0, 0}`) när `ok` är falsk, annars temats.

### 7.3 Markeringarna i tabellen (`automode::tableMarks`)
- **Start- och slutrader:** "{Word} börjar här." och "{Word} slutar här.". Flera texter på samma rad skiljs med `char{10}`.
- **Normalspannets fel** i en giltig sträcka ger `CellIssue{first + poleIndex, kolumn, "{Word}: {meddelande}"}`.
  - Kolumnen är `kSpan` för `Length`, `kHeight` för `Height` och `kPole` annars.
  - Fel med `poleIndex < 0` och varningar markeras inte.
- **Dubbletter:** `CellIssue{rad, kPole, "Stolpplats {p} finns flera gånger i {table}."}`, oavsett sträckorna.

### 7.4 Jämförelsen i Stolptabell efter (tooltips i `cellLook`)
- **Ändrad cell:** "I Stolptabell före: {visningstext}", eller "I Stolptabell före: tom".
- **Ny rad, i kolumn 0:** "Finns inte i Stolptabell före."
  - Utan Stolpplats: "Raden saknar Stolpplats och kan inte jämföras med Stolptabell före."
- **Osäker rad, i kolumn 0:** "Stolpplats {p} finns flera gånger i Stolptabell före och kan inte jämföras."

### 7.5 Spärrarna och upplösningen i `projectdb`
- **T1:** "Den automatiska beräkningen räknas ur {table} och kan inte ändras."
- **T2:** "Den automatiska beräkningen kan inte tas bort. Stäng av automatiskt läge i {table}."
- **T3:** "Raden visar en automatisk beräkning ur {table}. Stäng av automatiskt läge där för att koppla en annan beräkning."
- **Upplösningen**, när den automatiska aₙ saknas: "{radtext} har fel och ger inget aₙ. Rätta sträckan eller stolparna i {table}."

### 7.6 Frågor och statusrader i fönstret
- **Frågan vid kopiering, när efter har rader:**
  - Rubrik: "Kopiera från Stolptabell före".
  - Text: "Ersätta Stolptabell efter med en kopia av Stolptabell före?", två radbrytningar och sedan "Stolptabell efter har {n} rader, varav {x} ändrade och {y} nya jämfört med före. De ersätts av före-tabellens {m} rader, och start och slut och automatiskt läge blir som i före."
  - Knappar Ja och Nej, `MessageBoxType::Warning`.
- **Före är tom:** "Stolptabell före är tom. Det finns inget att kopiera." (OK, Information).
- **Statusraden:**
  - Efter kopiering: "Kopierade {m} rader från Stolptabell före."
  - Efter "Sträckorna som i före": "Start och slut är nu som i Stolptabell före."
  - Läget på: "Automatiskt läge är på för {table}. Raderna {före|efter} ombyggnad i Dokumentation visar de automatiska beräkningarna."
  - Läget av: "Automatiskt läge är av för {table}. Raderna i Dokumentation visar det de visade innan."
- **Notisen i frågan Radera:** "{p} är start för {word} och behöver väljas om efteråt.", eller "slut". En rad per träff.
- **Rullistan**, när den sparade Stolpplatsen saknas i tabellen: posten "{p} (finns inte)".

### 7.7 Anteckningen på de automatiska beräkningarna
- **Sträckan vald:** "Automatisk · {table} · {word} {from}–{to}". Exempel: "Automatisk · Stolptabell före · faslinan ST0102–ST0107".
- **Start eller slut saknas:** "Automatisk · {table} · {word}, sträckan är inte vald".
- **Samma text** står på normalspannet och linberäkningen.
- **Synkningen** skriver om texten när sträckan eller Samma ändras.

---

## 8. Fönstret: flikarna Stolptabell före och efter

Regionen GlazeBuilder ändras för hand i grammatikens form. `.glazeui` hålls fri från logik, och designraden står kvar.

### 8.1 `main_window.h`
- **Regionen**, nya medlemmar i ordning efter `stolpGrid`:
  - **Före:**
    - `glaze::GroupBox stolpAutoGrp;`, `glaze::Checkbox stolpAutoChk;` och `glaze::Label stolpFranLbl, stolpTillLbl;`.
    - Per lina `glaze::Label stolpFasLbl; glaze::Dropdown stolpFasFran, stolpFasTill; glaze::Label stolpFasStatus;`, och på samma sätt `stolpToppV…` och `stolpToppH…`.
  - **Efter:**
    - `glaze::TabPage efterSida;` direkt efter `stolpSida`, och `glaze::Panel efterPanel;` efter `stolpPanel`.
    - `glaze::Label efterRubrik; glaze::Button efterKopieraBtn; glaze::Label efterInfo;`.
    - `glaze::GroupBox efterAutoGrp; glaze::Checkbox efterAutoChk; glaze::Button efterSomForeBtn; glaze::Label efterFranLbl, efterTillLbl;`.
    - `efterFas…`, `efterToppV…` och `efterToppH…` som för före.
    - `glaze::Label efterSammaRuta, efterSammaText, efterAndradRuta, efterAndradText, efterNyRuta, efterNyText;`.
    - `glaze::DataGrid efterGrid;`.
- **Handlers:**
  - `stolpAutoChkChanged` och `efterAutoChkChanged`.
  - `rangeChanged(glaze::Widget* sender, glaze::ChangeArgs& e)`, gemensam för de tolv rullistorna.
  - `efterKopieraBtnClicked` och `efterSomForeBtnClicked`.
  - `efterGridCommitted`, `efterGridCellClicked`, `efterGridKeyDown`, `efterGridMouseUp` och `efterGridContextMenu`.
  - `stolpGrid…` står kvar och skickar vidare till `beforeGrid_`.
- **Nästlade typer:**
  - `class PoleHost : public kalkyl::polegrid::PoleGridHost` med `MainWindow& window_` och `kalkyl::projectdb::Stage stage_`.
  - `struct AutoPanel` med pekare till rutan, de tre namnetiketterna, de sex rullistorna och de tre statusetiketterna, och `std::array<std::vector<std::string>, 6> itemPoles`. Där har varje post i en rullista sin Stolpplats, och post 0 är tom.
- **Hjälpfunktioner:**
  - `loadPoleTables()`, `savePoles(Stage, rows)` och `reloadPoles(Stage)`.
  - `refreshAutomatic()`, `updateAutoPanels()`, `fillAutoPanel(Stage)` och `updatePoleLooks()`.
  - `saveAutoSettings(Stage, const AutoSettings&)`.
  - `std::vector<Wire> activeWires() const` och `std::vector<automode::NamedRange> namedRanges(Stage) const`.
  - `addRangeMenuItems(Stage, glaze::Menu&, int row)`, `renameInRanges(Stage, from, to)` och `std::string rangeNote(Stage, int first, int last) const`.
  - `copyBeforeToAfterAsked()` och `showPoleTab(Stage, int row)`.
  - `kalkyl::polegrid::PoleGrid& poleGridOf(Stage)` och `AutoSettings& settingsOf(Stage)`.
- **Medlemmar:** `beforeHost_`, `afterHost_`, `beforeGrid_`, `afterGrid_` (`unique_ptr`), `autoBefore_`, `autoAfter_`, `beforePanel_` och `afterPanel_`.
- **Tas bort:**
  - `PoleSheetHost`, `loadPoleTable`, `fillPoleGrid`, `decoratePoleRow`, `poleTone`, `applyPoleEdits`, `savePoleTable`, `insertPoleRowAbove` och `removePoleRowsAsked`.
  - `poleRows_`, `poleSaved_`, `poleHost_` och `poleSheet_`.
  - `alive_` och `postIfAlive`, om de inte används längre.
  - `setPoleStatus` och `gridStatusPanel_` står kvar.

### 8.2 `main_window.glazeui`
- **Flikarna:** `flikar.adopt` i ordningen `byggPmSida`, `stolpSida`, `efterSida`, `nsSida` och `lcSida` (K11). `efterSida.setText("Stolptabell efter")`, och `efterPanel` har `dock = Fill`.
- **Före**
  - **`stolpInfo`:** samma text som i dag plus meningen "Med automatiskt läge räknas normalspann och linberäkning för faslinan och topplinorna ur stolparna mellan start och slut, och resultaten visas i Dokumentation."
  - **`stolpAutoGrp`:** "Automatiskt läge", `x = 16`, `anchorTop = {"stolpInfo", 8}`.
  - **`stolpAutoChk`:** "Räkna faslinans och topplinornas normalspann och linberäkning ur tabellen", på x 0 och y 0.
    - Tooltip: "De automatiska beräkningarna sparas i projektet och visas på raderna före ombyggnad i Dokumentation. Kopplingar och värden som raderna hade vilar så länge och kommer tillbaka när rutan slås av."
  - **`stolpFranLbl`:** "Från", `Dim`, `x = 140`. **`stolpTillLbl`:** "Till", `Dim`, `x = 310`. Båda har `anchorTop = {"stolpAutoChk", 8}`.
  - **Faslinans rad**, alla med `anchorTop = {"stolpFranLbl", 4}` (justeras med glaze-render):
    - `stolpFasLbl`: "Faslina", på x 0.
    - `stolpFasFran` och `stolpFasTill`: `width = 160`, på x 140 och 310, med tooltipen "Stolpen där linan börjar" respektive "Stolpen där linan slutar".
    - `stolpFasStatus`: på x 480.
  - **Topplinornas rader:** `stolpToppV…` ("Topplinor") under `stolpFasFran`, och `stolpToppH…` ("Topplina höger") under `stolpToppVFran`.
  - **`stolpGrid`:** `anchorTop = {"stolpAutoGrp", 8}`.
- **Efter**
  - **`efterRubrik`:** "Data efter ombyggnad", `Heading`, x 16, y 8.
  - **`efterKopieraBtn`:** "Kopiera från Stolptabell före…" med `glaze::stockIcon(glaze::StockIcon::Copy)`, `anchorLeft = {"efterRubrik", 16}`, y 4.
    - Tooltip: "Ersätter Stolptabell efter med en kopia av Stolptabell före, med start, slut och automatiskt läge."
  - **`efterInfo`:** `Dim`, `setWrap(true)`.
    - Text: "Kopiera Stolptabell före hit och ändra sedan de stolpar som byggs om. Raderna jämförs med Stolptabell före på Stolpplats: färgen visar om raden är som i före, ändrad eller ny, och tooltipen på en ändrad cell visar värdet i före. Redigering, inklistring och papperskorgen fungerar som i Stolptabell före."
  - **`efterAutoGrp`:** som `stolpAutoGrp`, men `efterAutoChk` har "efter ombyggnad" i tooltipen.
  - **`efterSomForeBtn`:** "Sträckorna som i före", `anchorLeft = {"efterAutoChk", 16}`, y 0.
    - Tooltip: "Hämtar start och slut ur Stolptabell före. Raderna ändras inte."
  - **Förklaringen:** en rad med `x = 16` och `anchorTop = {"efterAutoGrp", 8}`, i kedja med `anchorLeft`. Varje ruta har samma tooltip som sin text.
    - `efterSammaRuta`: "■" i `gfx::Color{220, 160, 40, 255}`, och `efterSammaText`: "Som i före" (`Dim`).
    - `efterAndradRuta`: "■" i `{210, 90, 140, 255}`, och `efterAndradText`: "Ändrad".
    - `efterNyRuta`: "■" i `{70, 170, 90, 255}`, och `efterNyText`: "Ny".
  - **`efterGrid`:** `anchorTop = {"efterSammaRuta", 8}`. Kolumnerna, redigerarna, tooltipsen, `setSortable(false)`, `setEditOnClick(false)` och `setHeaderLines(2)` skrivs ordagrant som för `stolpGrid`.
- **`nsLista`** får kolumnen Indata först, se 9.2: `{.text = "Indata", .width = 110}` före "Från".
- **Händelserna** kopplas i fullform:
  - `stolpAutoChk.onChange` och `efterAutoChk.onChange`.
  - De tolv rullistornas `onChange` till `rangeChanged`.
  - `efterKopieraBtn.onClick` och `efterSomForeBtn.onClick`.
  - `efterGrid.onCommit`, `onCellClick`, `onKeyDown`, `onMouseUp` och `onContextMenu`.
- **Trädet** byggs med `adopt` i layoutordning. Syskonankaren pekar bara bakåt.

### 8.3 `main_window.cpp`: tabellerna
- **`onLoad`:**
  1. `loadProjectData()` och `rebuildByggPm()`.
  2. Värdarna `beforeHost_` och `afterHost_` skapas, och sedan `beforeGrid_ = PoleGrid(*this, stolpGrid, *beforeHost_, "Stolptabell före")` och `afterGrid_` på samma sätt.
  3. Skrivskyddat enligt 8.7.
  4. Pekarna i panelerna sätts.
  5. `loadPoleTables()` och `refreshAutomatic()`.
  6. Resten som i dag.
- **`loadPoleTables()`:**
  - Läser `poles` och `autoSettings` för båda skedena och kör `setRows` på båda tabellerna.
  - Ett fel ger rutan "Stolptabell före kunde inte läsas." med en radbrytning och felet. Tabellen blir då tom och läget av.
- **`PoleHost`:**
  - **`savePoles`:** `setPoles(*project_, stage_, rows)`.
    - Vid fel loggas felet, rutan "{table} kunde inte sparas." med en radbrytning och felet visas, och svaret är false.
    - Annars körs `updateTitle()`, och svaret är true.
  - **`reloadPoles`:** läser `poles`. Vid fel svarar den med en tom lista.
  - **`polesChanged`:** `refreshAutomatic()`.
  - **`poleRenamed`:** `renameInRanges`.
  - **`addMenuItems`:** `addRangeMenuItems`.
  - **`removeNote`:** `rangeNote`.
  - **`showStatus`:** `setPoleStatus`.
- **`renameInRanges`:** varje sträcka i skedet med `fromPole == from` eller `toPole == from` får `to`. Har något ändrats körs `saveAutoSettings`.
- **`addRangeMenuItems`:** undermenyn "Sträcka" med "{Word} börjar här" och "{Word} slutar här" för varje aktiv lina.
  - Posterna är aktiva när projektet är skrivbart, `row >= 0` och raden har Stolpplats.
  - Ett klick sätter start eller slut och kör `saveAutoSettings`.
- **`saveAutoSettings`:** `setAutoSettings`.
  - Vid fel visas en ruta, och inställningen läses om.
  - Annars sätts `settingsOf(stage) = s` och körs `updateTitle()` och `refreshAutomatic()`.
- **`refreshAutomatic()`:** `refreshNormalSpans()`, `refreshLineCalcs()`, `updateAutoPanels()` och `updatePoleLooks()`.
- **`updatePoleLooks()`:**
  - Före: `marks = automode::tableMarks(före-rader, autoBefore_.enabled ? namedRanges(Before) : {}, "Stolptabell före")`.
  - Efter: `compare = automode::compareAfter(före, efter)`, `before = före-rader` och `marks` på samma sätt.
  - Sedan `setLooks` på båda tabellerna.

### 8.4 Färgerna (`polegrid::cellLook`)
- **Tonen:** `tone(färg, rad)` ger alfa 64 på jämna rader och 52 på udda.
- **Färgerna:**

  | Namn | Värde | Används till |
  |---|---|---|
  | `kAmber` | `{220,160,40}` | Som i före |
  | `kRose` | `{210,90,140}` | Ändrad rad |
  | `kGreen` | `{70,170,90}` | Ny rad |
  | `kChangedCell` | `{210,90,140,130}` | Ändrad cell |
  | `kErrorColor` | `{220,60,60,80}` | Fel |

- **Radens grundton:**
  - Före: `tone(kAmber)`.
  - Efter: `Empty`, `Same` och `Ambiguous` ger `tone(kAmber)`, `Changed` ger `tone(kRose)` och `New` ger `tone(kGreen)`.
  - Den tomma raden sist: `tone(kAmber)`.
- **Cellen, kolumnerna 0–7**, i den här ordningen:
  1. `Changed` och `changed[kolumn]`: `kChangedCell` och tooltipen i 7.4.
  2. `New`, kolumn 0: tooltipen i 7.4.
  3. `Ambiguous`, kolumn 0: `kErrorColor` och tooltipen i 7.4.
  4. En `CellIssue` för cellen: `kErrorColor`, som vinner över punkt 1–3. Felets meddelanden står först i tooltipen.
  5. Kolumn 0 med en not: `flag = true`, och noten läggs sist i tooltipen.
- **Tooltipens delar** skiljs med `char{10}`.
- **Radera-kolumnen** har radens grundton och ingen tooltip.

### 8.5 Panelen Automatiskt läge
- **`fillAutoPanel(stage)`:**
  - **Namnetiketterna:** `autoLineLabel(wire, !byggPmSplit_)`. Höger-raden döljs när topplinorna är samma.
  - **Rullistorna:** "–" och sedan varje Stolpplats som inte är tom, i tabellens ordning och en gång var.
    - En sparad text som inte finns läggs till som "{p} (finns inte)" och väljs.
    - `itemPoles` fylls samtidigt.
  - **Statusen:** `automode::rangeStatus(automode::rangeSpan(...), rader)` med färgen i 7.2. Den visas också när läget är av.
  - **Rutan:** `setChecked(settings.enabled)`.
- **`rangeChanged(sender)`:** avsändaren hittas bland pekarna, och postens Stolpplats skrivs in i inställningen. En post "(finns inte)" ändrar ingenting. Sedan körs `saveAutoSettings`.
- **`stolpAutoChkChanged` och `efterAutoChkChanged`:** sätter `enabled`, kör `saveAutoSettings` och visar statusraden i 7.6.

### 8.6 Kopiera från före och Sträckorna som i före
- **`copyBeforeToAfterAsked()`:**
  1. Är före tom visas rutan i 7.6, och ingenting kopieras.
  2. Har efter rader visas frågan i 7.6. Antalen räknas med `compareAfter`.
  3. Vid Ja, eller när efter är tom, körs `copyBeforeToAfter`. Ett fel ger en ruta.
  4. `afterGrid_->setRows(poles(After))`, `autoAfter_ = autoSettings(After)`, `refreshAutomatic()` och statusraden.
- **`efterSomForeBtnClicked`:** `s = autoAfter_` och `s.ranges = autoBefore_.ranges`. Sedan `saveAutoSettings(After, s)` och statusraden.
- **`showPoleTab(stage, row)`:** väljer fliken med `flikar.setSelectedIndex(flikar.pageIndex(...))` och kör `showRow(row)` när `row >= 0`.

### 8.7 Skrivskyddat
- Rutorna, rullistorna, `efterKopieraBtn` och `efterSomForeBtn` stängs av med `disable()`.
- `PoleGrid::setReadOnly(true)` körs på båda tabellerna.
- Menyposterna under Sträcka är inaktiva.
- De automatiska beräkningarna syns och går att visa.

---

## 9. Dokumentation och flikarna Normalspann och Linberäkning

### 9.1 Dokumentation
- **`refreshNormalSpans`:** `nsCalcs_` räknas med `projectdb::calculateSpan(ns)`.
- **Felet för en automatisk beräkning:** felet med `poleIndex >= 0` skrivs "{Stpnr}: {meddelande}", där Stpnr är `ns.poles[poleIndex].pole`, eller "rad {n}: …" när Stpnr är tom.
- **`showNormalSpanRows`**, när den visade beräkningen har `automatic`:
  - Bilden är `StockIcon::Sync`.
  - Tooltipen vid resultat: "Automatisk ur {table}: {from} – {to}, {n} spann. Den ändras i {table}."
  - Tooltipen vid fel: "Automatisk ur {table}: {fel}".
- **`showLineCalcRows`** för en automatisk beräkning:
  - Bilden är `Sync`, eller `Warning` när utnyttjandegraden är för hög.
  - Tooltip: "Stakningsmall (lastfall 15, maxtemperatur) ur den automatiska linberäkningen: {lina}, aₙ {aₙ} m ur {table}, σ₀ {σ₀} MPa. Den ändras i {table} och Dokumentation."
  - Utnyttjandetexten läggs till som i dag.
- **`byggPmGridContextMenu`** på en automatisk rad har bara två poster:
  - "Visa…", som öppnar formuläret.
  - "Gå till {table}", som kör `showPoleTab(stage, automatic->firstRow)`.
  - Undermenyn Koppla befintlig för normalspann visar inga automatiska beräkningar.
- **Byte till eller från Samma:** den postade lambdan kör `rebuildByggPm(); refreshAutomatic();`.

### 9.2 Flikarna Normalspann och Linberäkning
Varje automatisk post märks på fyra sätt, i båda listorna (svar 14):
1. **Bilden:** `glaze::stockIcon(glaze::StockIcon::Sync)` som radens bild (`Row::img`, i kolumn 0). Vanliga poster har ingen bild.
2. **Kolumnen Indata:** "Automatisk".
   - I `nsLista` är kolumnen ny och står först. Vanliga normalspann har "Manuell".
   - I `lcLista` finns kolumnen redan och har "Manuell", "Dokumentation" eller "Automatisk".
3. **Kolumnen Anteckning:** den lagrade anteckningen i 7.7, till exempel "Automatisk · Stolptabell före · faslinan ST0102–ST0107".
4. **Tonen:** `kPurple` för före och `kRose` för efter, med alfa 64/52. Vanliga poster är gula.

De automatiska posterna står först, i projektdatabasens ordning (6.4).

**Övrigt:**
- **`fillNormalSpanList`:**
  - Index för talkolumnernas `setSortKey` flyttas ett steg, eftersom Indata är ny kolumn 0.
  - Från och Till är sträckans Stolpplatser när beräkningen saknar resultat.
- **`updateNsButtons` och `updateLcButtons`** för en automatisk post:
  - Ändra… heter "Visa…".
  - Ta bort är släckt.
  - Kopiera… är aktiv när projektet är skrivbart.
  - Tooltipen börjar med anteckningen och har raden "Automatisk: den ändras i {table} och kan inte tas bort."
- **`nsListaContextMenu` och `lcListaContextMenu`** för en automatisk post: "Visa…", "Kopiera…", en separator och "Gå till {table}".
- **`removeNormalSpanAsked` och `removeLineCalcAsked`** gör ingenting för en automatisk post.

---

## 10. Formulären Normalspann och Linberäkning

**`NormalSpanDialog`** (`.h` och `.cpp`):
- **Ny medlem:** `std::optional<kalkyl::projectdb::AutoOrigin> automatic_` efter regionen, med `#include "projectdb/project_data.h"`.
- **`loadCalculation`:** med `existingId_ != 0` och `ns.automatic` sätts `automatic_` och `readOnly_ = true`.
- **`updateDocLabel`** med `automatic_`:
  - "Automatisk beräkning ur {table}: {from} – {to}. Den följer tabellen och ändras där, och den kan inte tas bort."
  - Har sträckan fel läggs " Sträckan går inte att använda: {issue}" till.
  - Sedan " Visas i Dokumentation på: …".
- **Titeln:** "Normalspannsberäkning (automatisk)".
- **En kopia** med `setCopyOf` blir en vanlig, ny och lös beräkning med stolparna. Anteckningen blir "Kopia av Automatisk · …".

**`LineCalcDialog`** (`.h` och `.cpp`):
- **Ny medlem:** samma `automatic_`.
- **`loadCalculation`:** med `existingId_ != 0` och `lc_.automatic` sätts `automatic_` och `readOnly_ = true`. I grenen för en kopia nollas `lc_.automatic`.
- **`onLoad`:** med `automatic_` döljs `docInputChk`.
- **`updateDocLabel`:** "Automatisk linberäkning ur {table}. Linan, σ₀ och temperaturerna är Dokumentationens, och aₙ kommer ur den automatiska normalspannsberäkningen ({from} – {to}). Den kan inte ändras eller tas bort."
- **Titeln:** "Linberäkning (automatisk)".
- **Skrivskyddat** fungerar skillnadsrutan och i-knappen.

---

## 11. Omräkning och lägena Excel och Ny

- **De lagrade beräkningarna:**
  - `syncAutoCalcs` håller dem aktuella i samma transaktion som varje skrivning de bygger på (6.4).
  - Stolparna skrivs bara om när de har ändrats. En ändring i Av eller LKL rör alltså inga beräkningar.
- **Visningen:** fönstret kör `refreshAutomatic()` direkt efter varje sparad ändring, på UI-tråden:
  - en ändring i en stolptabell (`PoleGridHost::polesChanged`),
  - en ändrad sträcka eller ändrat läge,
  - en kopiering,
  - ett byte till eller från Samma.
- **`refreshAutomatic()`** läser beräkningarna ur projektet, räknar aₙ och mallen och räknar panelerna och färgerna med `autoMode` på raderna i minnet. Raderna i minnet är lika med det sparade.
- **Ändringar i Dokumentation** kör `refreshLineCalcs` som i dag. Linan, σ₀ och temperaturerna löses upp vid läsningen, så inget lagrat behöver ändras.
- **Resultaten lagras inte**, som för alla beräkningar.
  - aₙ räknas ur de lagrade stolparna och mallen ur den upplösta linberäkningen.
  - Utskrifterna gör senare likadant.
- **Lägena:**
  - Normalspannet har inga lägen, och avstegen i planen för Normalspann (2.6) gäller.
  - Linberäkningen är i läget Ny.
  - Formuläret visar skillnaden mot Excel med kryssrutan, och i-knappen öppnar PM:et.
- **Inget nytt avsteg** från Excel tillkommer, så `res/linberakning_excel.md` ändras inte.

---

## 12. Projektdatabasen: steg 5 och migrering

- **Steg 5** (6.2) lägger till `pole_after`, `auto_mode` och `auto_calc`.
  - Inga befintliga tabeller ändras, och inga data flyttas eller tas bort.
  - Steg 1–4 är orörda.
- **Projekt från v0.1.0–v0.4.0** (`user_version` 1–4) migreras när de öppnas.
  - Läget är av, och det finns inga automatiska beräkningar.
  - Migreringen kör ingen synkning.
- **Ett nytt projekt** har läget av och en tom Stolptabell efter.
- **Steg 5 kan ändras fritt tills det committas.**
- **Migreringstestet `testMigrateFromV040`** (13.2) bygger filen som i 47c6c74 (v0.4.0) och kontrollerar att alla data finns kvar och att de nya delarna fungerar.

---

## 13. Tester (ctest i `tests/`)

### 13.1 `tests/automode_test.cpp` (ny, ctest `automode`)
Testet länkar `kalkyl_automode kalkyl_poletable kalkyl_ancalc`, och data är bilaga A.
- **`findRange`:**
  - Varje fel i 7.1 med exakt text och i rätt ordning.
  - Trimning.
  - En tom rad mitt i.
  - ST0102–ST0107 ger `first` 2 och `last` 7.
- **`spanPoles`:** fälten, tomt tal blir `nullopt`, och index utanför tabellen eller `first > last` ger en tom lista.
- **`rangeSpan`:**
  - Fas ST0102–ST0107 före: 5 spann, `totalM` 1560 och aₙ 315,2 (`anExact` ≈ 315,1557, tolerans 1e-3).
  - Topp ST0101–ST0108: aₙ 313,8.
  - Hela tabellen: aₙ 308,3.
  - Efter, med ST0104 = 365 och ST0105 = 295: fas 316,8 och topp 315,0.
  - En sträcka med fel ger inget resultat och sträckans fel.
- **`rangeStatus`:**
  - "5 spann · aₙ 315,2 m" och " · aₙ,ΔH ".
  - "ST0105: Spannlängd saknas." när spannlängden saknas.
  - "Rad {n}: …" utan Stolpplats.
- **`duplicatePoles`:** ordningen, och tom Stolpplats räknas inte.
- **`compareAfter`:**
  - Alla fem lägen.
  - 44.10 mot 44.1 är lika, och "" mot "0" skiljer sig.
  - Texten jämförs exakt, och en dubblett i före ger `Ambiguous`.
- **`tableMarks`:**
  - Noter, och två noter på samma rad skiljs med `char{10}`.
  - `CellIssue` på rätt rad i `kSpan` med texten "Faslinan: Spannlängd saknas.".
  - Dubbletter utan sträckor.
  - En sträcka med fel ger inga markeringar.

### 13.2 `tests/projectdb_test.cpp` (utökas)
**Hjälpfunktionen `checkAutoMatches(p)`** jämför för varje aktiv automatisk beräkning:
- de lagrade stolparna (`normalSpan(id).poles`, fält för fält) med `automode::rangeSpan(poles(stage), …).poles` utan helt tomma stolpar,
- `automatic->issue`, `firstRow` och `lastRow` med `findRange`,
- anteckningen med 7.7.

Den kontrollerar också att det inte finns några automatiska rader för inaktiva linor (`SELECT count(*)` mot de reserverade id:na).

**Schemat**
- `kTenTablesSql` blir `kThirteenTablesSql` med `'pole_after', 'auto_mode', 'auto_calc'`.
- `testSchemaTables` kontrollerar 13 tabeller och `kSchemaVersion == 5`.
- Den hårdkodade 4:an i `testMigrateFromV030` byts mot `kSchemaVersion`.

**Stolptabeller och inställningar**
- **`testPoleTableInvalid`:** texterna "Stolptabell före har …" och samma fall för efter.
- **`testPolesAfterRoundTrip`:** som för före, och före påverkas inte.
- **`testAutoSettingsRoundTrip`:** förvalet, trimning och åäö, och skedena är oberoende.
- **`testCopyBeforeToAfter`:**
  - Efter blir lika med före i rader och inställning, det gamla i efter är borta och före är orört.
  - De automatiska beräkningarna i efter skapas.
  - `checkAutoMatches`.

**`testAutoCalcsSync`** (ny). Utgångsläget är bilaga A i före, läget på, fas ST0102–ST0107 och topp ST0101–ST0108.
- **`normalSpans`:**
  - Börjar med två automatiska beräkningar med positiva id, `automatic`, anteckningen, `docKeys` och 6 respektive 8 lagrade stolpar.
  - `calculateSpan` ger 315,2 och 313,8.
  - `checkAutoMatches`.
- **Synkningen efter varje sorts ändring.** Efter varje steg körs `checkAutoMatches`, och id:na är desamma som förut.
  - `setPoles` som ändrar ST0104 till 365 ger aₙ 316,8 och ett nytt `modified_utc`.
  - `setPoles` som bara ändrar Av eller LKL ger samma `modified_utc`.
  - `setAutoSettings` med en ny sträcka ger nya stolpar och en ny anteckning.
  - Ett fel i sträckan ger `issue`, inga stolpar och felet ur `calculateSpan`. Rättas sträckan kommer stolparna tillbaka.
  - En egen lina på Topplina höger med `setDocConductor` ger en tredje automatisk beräkning med felet "Välj var topplina höger börjar och slutar.". Anteckningen för TopLeft blir "topplina vänster …".
  - `setDocValue(top_conductor_right, "Samma")` tar bort den tredje.
- **Läget av:**
  - Raderna i `normal_span` och `line_calc` för id:na finns inte längre, och `normalSpans` har inga automatiska.
  - Raderna i `auto_calc` finns kvar.
- **En ny vanlig beräkning** medan läget är av får ett id som är större än alla reserverade.
- **Läget på igen:** samma id som förut och värden ur tabellen som den är nu.
- **`lineCalcs`:**
  - De automatiska har `docSource` = raden och indata lika med `newLineInput()` i de fält som går att jämföra.
  - `docLineSources` har `("ns_calc_phase_before", 315.2)` och nyckeln i `autoSpanKeys`.
  - `resolveLineInput` ger `normalSpanM` 315,2.

**Kopplingar och spärrar**
- **`testAutoDormantLinks`:**
  - Utgångsläget: en lagrad normalspannsberäkning och en kopplad linberäkning på fasraderna före, och ett manuellt värde på `ns_calc_top_left_before`.
  - Med läget på har de lagrade beräkningarna tomma `docKeys`.
  - Med läget av är kopplingarna och värdet tillbaka, oförändrade.
- **`testAutoGuards`:** varje spärr i 6.4 ger `Invalid` med texten i 7.5. Revisionen och de lagrade raderna är oförändrade.
- **`testAutoIssue`:**
  - En sträcka till ST0199 ger `issue` "Stolpplats ST0199 finns inte i Stolptabell före.".
  - `docLineSources` ger tomt aₙ.
  - Upplösningen ger texten i 7.5.

**Skrivskydd och revision**
- **`testReadOnlyWrites`:** `setPoles(After)`, `setAutoSettings` och `copyBeforeToAfter` ger `AccessDenied`. De lagrade automatiska beräkningarna går att läsa.
- **`testRevision`:** de tre skrivningarna räknar upp revisionen. En synkning i dem ger ingen extra uppräkning.

**Migrering och provprojekt**
- **`writeV040Project` och `testMigrateFromV040`** (nya):
  - Filen byggs som v0.4.0: `user_version = 4`, `kStep2`–`kStep4`, projektraden `'åäö-v040'`, `'Kalkyl 0.4.0'`, två `doc_value`, en normalspannsberäkning med koppling, en linberäkning med 15 lastfall och koppling, och tre rader i `pole_before` med tio decimaler.
  - Efter öppningen: `user_version` är 5, det finns 13 tabeller och allt gamla finns kvar.
  - `poles(After)` är tom, inställningarna har förvalet och det finns inga automatiska beräkningar.
  - Sedan slås läget på med en sträcka.
  - Efter publicering, stängning och ny öppning finns allt kvar, också de automatiska beräkningarna med samma id. `checkAutoMatches` går igenom.
- **`testProvProject`:**
  - Provprojektet får bilaga A i före med fas ST0102–ST0107, topp ST0101–ST0108 och läget på.
  - Efter blir en kopia där ST0104 har spannlängden 365 och ST0105 har 295, med raden ST0110 (spannlängd 260) tillagd.
  - De lagrade beräkningarna som projektet redan har står kvar och vilar.

### 13.3 `tests/polegrid_test.cpp` (ny, ctest `polegrid`)
Testet länkar `kalkyl_polegrid kalkyl_automode kalkyl_poletable` och testar bara `cellLook`.
- **Före:** gul med alfa 64 och 52, också på den tomma raden.
- **`Changed`:** rosa rad. Den ändrade cellen har alfa 130 och tooltipen "I Stolptabell före: 350".
- **`New`:** grön rad, med båda tooltiptexterna för kolumn 0.
- **`Ambiguous`:** fel-färgen på Stolpplats.
- **`CellIssue`** vinner över färgen på en ändrad cell, och tooltipen får båda texterna i rätt ordning.
- **En not** ger `flag` och tooltip.
- **Radera-kolumnen** har radens ton.

### 13.4 Det som kräver fönster
- `PoleGrid` i ett fönster, huvudfönstret och formulären provas med glaze-render, röktest och handtest.
- De befintliga testerna går igenom oförändrade.

---

## 14. Arbetspaket

Fyra Sonnet-agenter (high) arbetar samtidigt. Varje agent äger sina filer, och ingen fil delas mellan paketen.

### Gemensamt för alla paket
- **Byggkatalog:** varje paket har en egen katalog i repots rot. Den konfigureras med MSYS2:s cmake (`C:/msys64/ucrt64/bin/cmake.exe`):
  ```sh
  cmake -S . -B build-<paket> -G Ninja -DCMAKE_BUILD_TYPE=Debug -DCMAKE_C_COMPILER=C:/msys64/ucrt64/bin/gcc.exe -DCMAKE_CXX_COMPILER=C:/msys64/ucrt64/bin/g++.exe -DKALKYL_TESTS=ON -DFETCHCONTENT_SOURCE_DIR_GLAZEUI=C:/Users/sehoer/Source/Kalkyl/build/_deps/glazeui-src
  ```
- **Städning:** `build-<paket>` tas alltid bort när agenten är klar (`rm -rf build-<paket>`). `build/` rörs aldrig: den byggs inte, rensas inte och konfigureras inte om.
- **Fel i ett annat pakets fil:** faller bygget i en fil som ett annat paket äger, vänta och försök igen. Ändra aldrig filen.
- **Steg 0:** inom några minuter skriver varje paket sina kontraktsfiler ordagrant ur avsnitt 6, med kroppar som kompilerar. Ordningen är:
  1. paket 1, `auto_mode.h`,
  2. paket 2, `project_data.h` och schemat,
  3. paket 3, `pole_grid.h`,
  4. paket 4.

### Paket 1: autoMode och formulären (`build-auto`)
- **Äger:**
  - `libs/autoMode/` (ny), `libs/CMakeLists.txt`, `libs/CLAUDE.md` och `libs/poleTable/CLAUDE.md` (bara texten).
  - `tests/automode_test.cpp` (ny) och `tests/CMakeLists.txt`.
  - `src/windows/normal_span_dialog.h` och `.cpp`, och `src/windows/line_calc_dialog.h` och `.cpp`.
- **Steg 0:**
  - 6.1 med stubbar.
  - `libs/CMakeLists.txt` med `add_subdirectory(autoMode)` efter `anCalc` och `add_subdirectory(poleGrid)` sist.
  - I `tests/CMakeLists.txt`:
    ```
    kalkyl_add_test(automode automode_test.cpp kalkyl_automode kalkyl_poletable kalkyl_ancalc)
    kalkyl_add_test(polegrid polegrid_test.cpp kalkyl_polegrid kalkyl_automode kalkyl_poletable)
    ```
  - En stubb av `automode_test.cpp`.
- **Arbete:** 6.1, 7.1–7.3, 13.1 och formulären enligt 10. Formulären väntar på paket 2:s steg 0.
- **Bygg och kör:**
  - `ctest -R automode` och `kalkyl`.
  - glaze-render på `normal_span_dialog.h` och `line_calc_dialog.h` (`-DGLAZE_TOOLS=ON`, `--target glaze-render`). Exit 0 krävs.
- **CLAUDE.md:**
  - `libs/CLAUDE.md`, `libs/autoMode/CLAUDE.md` och `libs/poleTable/CLAUDE.md`.
  - Rapporten innehåller texten till `tests/CLAUDE.md` och till formulärens rader i `src/windows/CLAUDE.md`.

### Paket 2: projektdatabasen (`build-db`)
- **Äger:** `libs/projectdb/dev_migration.h`, `project_db.h`, `project_db.cpp`, `project_data.h`, `project_data.cpp`, `CMakeLists.txt` och `CLAUDE.md`, och `tests/projectdb_test.cpp`.
- **Steg 0:** 6.2 och 6.3 med kroppar som kompilerar, och `kalkyl_automode` som PUBLIC.
- **Arbete:** 6.2–6.4, 7.5, 7.7, 12 och 13.2.
  - `syncAutoCalcs` och dess anrop i alla fem skrivningarna är kärnan.
  - Gå igenom alla skrivningar i `project_data.cpp` och bekräfta i rapporten att ingen annan påverkar det som synkningen läser.
  - Slutkörningen av testerna görs när paket 1 har rapporterat.
- **Bygg och kör:** `ctest -R projectdb`.
- **CLAUDE.md:** `libs/projectdb/CLAUDE.md`. Den beskriver:
  - steg 5 och tabellerna,
  - `poles`, `setPoles` och automatiskt läge,
  - de lagrade automatiska beräkningarna med `auto_calc`, de reserverade id:na, synkningen och regeln om nya skrivningar,
  - de vilande kopplingarna och spärrarna,
  - `testMigrateFromV040`.

  Rapporten innehåller texten till `tests/CLAUDE.md`.

### Paket 3: poleGrid (`build-grid`)
- **Äger:** `libs/poleGrid/` (ny), `tests/polegrid_test.cpp` (ny) och `libs/sheetGrid/CLAUDE.md` (bara texten).
- **Steg 0:** 6.5 med stubbar och en stubb av `polegrid_test.cpp`.
- **Arbete:** 6.5, 8.4 och 13.3.
  - Koden flyttas ur `main_window.cpp`, rad 513–802, och görs generell. Läs den först.
  - Stolptabell före ska bete sig som i 0.4.0, utom färgerna och notisen i frågan.
- **Bygg och kör:** `ctest -R polegrid`, och `kalkyl` när paket 4 har sina stubbar.
- **CLAUDE.md:** `libs/poleGrid/CLAUDE.md`. `libs/sheetGrid/CLAUDE.md` får meningen "Används av Linberäkning och `libs/poleGrid` (Stolptabell före och efter)". Rapporten innehåller texten till `tests/CLAUDE.md`.

### Paket 4: huvudfönstret (`build-main`)
- **Äger:** `src/windows/main_window.h`, `main_window.glazeui`, `main_window.cpp`, `src/windows/CLAUDE.md`, `src/CMakeLists.txt` och `src/CLAUDE.md`.
- **Steg 0:**
  - Regionen och `.glazeui` (8.1–8.2, med Indata i `nsLista`) och tomma handlers.
  - `kalkyl_automode` och `kalkyl_polegrid` i `src/CMakeLists.txt`.
  - Den gamla stolptabellskoden står kvar tills `pole_grid.h` finns.
- **Arbete:** 8.1–8.7 och 9. Den gamla koden tas bort.
- **Förhandsgranskning:** `-DGLAZE_TOOLS=ON` och `--target glaze-render`, sedan
  ```sh
  build-main/_deps/glazeui-build/apps/glaze-render/glaze-render.exe src/windows/main_window.h -o <scratchpad>/main_window.png
  ```
  Exit 0 krävs, och bilden granskas med Read.
- **Röktest:**
  - `KALKYL_DATA_DIR=build-main/smoke timeout -k 2 3 ./build-main/src/kalkyl.exe; echo $?` ger 124 eller 137.
  - Med provprojektet ur `build-main/prov` (efter `ctest -R projectdb`) visas huvudfönstret.
- **Kontroller:**
  - `grep -nE "poleRows_|PoleSheetHost|fillPoleGrid" src/windows/main_window.*` ger ingen träff.
  - Flikarna står i ordningen i 8.2.
- **CLAUDE.md:** `src/windows/CLAUDE.md` (raderna för `main_window.*`) och `src/CLAUDE.md`.

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
- **Filredigering:**
  - Filer ändras bara med verktygen Edit och Write, aldrig med Python- eller sed-skript.
  - Skalet används bara för bygge, körning, test och läsning.
- **Hooken `~/.claude/hooks/only-c-drive.sh`** granskar varje verktygsanrop, också filinnehåll. Den stoppar:
  - två bakvända snedstreck i rad,
  - ett bakvänt snedstreck och en bokstav efter skiljetecken eller blanksteg och före citattecken eller slut,
  - andra enhetsbokstäver än C följda av kolon. `old_string` och `new_string` får aldrig sluta med en bokstav och ett kolon.

  **Därför:**
  - råa strängliteraler: `R"sql( … )sql"` och `R"( … )"`,
  - teckenklasser som `[.]`,
  - `char{10}`, `char{9}` och `char{34}`,
  - snedstreck framåt i sökvägar,
  - bracket-argument och `string(ASCII 10 NL)` i CMake.
- **Varningar:** bygget är fritt från varningar (`-Wall -Wextra`).
- **Versionen** rörs inte.
- **Inga commits.**

---

## 15. Integration (samordnaren)

1. **Versionen:** `project(Kalkyl VERSION 0.5.0` i rotens `CMakeLists.txt`.
2. **Bygget utan varningar** i `build/`: `cmake --build --preset debug 2>&1 | grep -i warning` ger ingen utdata.
3. **Testerna** i en tillfällig `build-verify`, konfigurerad som i 14 och med `-DGLAZE_TOOLS=ON`:
   ```sh
   cmake --build build-verify && ctest --test-dir build-verify --output-on-failure
   ```
   Alla tester går igenom. Katalogen tas bort efteråt.
4. **glaze-render** ger exit 0 på `main_window.h`, `normal_span_dialog.h` och `line_calc_dialog.h`.
5. **Röktest med provprojektet:**
   - Flikordningen stämmer.
   - Färgerna syns i efter.
   - Dokumentation har Sync-bilder.
   - Flikarna Normalspann och Linberäkning har de automatiska posterna först, med Sync-bild, "Automatisk" och anteckningen.
6. **Kontroller:**
   - `grep -rn "poleRows_" src` ger ingen träff.
   - `kSchemaVersion = 5`.
   - `git diff` visar att steg 1–4 är oförändrade.
7. **Städning:** inga `build-*` utom `build/` finns kvar.
8. **CLAUDE.md och `TODO.md`** enligt 17. Användaren får testfallen i 16.

---

## 16. Handtest (användaren)

Kör `build/src/kalkyl.exe` och ha Excel öppet med bilaga A. Provprojektet `build-test/prov/Prov åäö.kalkyl` skapas av `ctest --preset test` och har bilaga A i före och efter.

**Projektet**
1. **Ett projekt från 0.4.0** öppnas utan fel.
   - Stolptabell före är som förut.
   - Stolptabell efter är tom.
   - Läget är av.
   - Dokumentation, Normalspann och Linberäkning är orörda.
2. **Flikarna** står i ordningen Dokumentation, Stolptabell före, Stolptabell efter, Normalspann, Linberäkning.

**Stolptabell före och automatiskt läge**

3. **Klistra in bilaga A.** Välj Faslina från ST0102 till ST0107. Statusen visar "5 spann · aₙ 315,2 m" och aₙ,ΔH.
4. **Slå på Automatiskt läge.**
   - "Faslina normalspannsberäkning före ombyggnad" visar 315,2 m med Sync-bilden.
   - Linberäkningsraden visar mallen när Faslina, Uppspänning fas och temperaturerna är ifyllda.
5. **Fliken Normalspann:** den automatiska posten står först och har:
   - Sync-bilden,
   - Indata "Automatisk",
   - anteckningen "Automatisk · Stolptabell före · faslinan ST0102–ST0107",
   - lila ton.

   Ta bort är släckt. Visa… visar formuläret "Normalspannsberäkning (automatisk)" skrivskyddat med ST0102–ST0107 och 5 spann. Kopiera… ger en vanlig beräkning.
6. **Fliken Linberäkning:** samma märkning med Indata "Automatisk". Visa… är skrivskyddat, och skillnadsrutan mot Excel och i-knappen fungerar.
7. **Topplinor med Samma:** ST0101–ST0108 ger 313,8 m på raderna "Topplinor …", och anteckningen säger "topplinorna".
8. **Topplina höger:**
   - Välj en egen lina. Ramen visar vänster och höger.
   - Höger har felet "Välj var topplina höger börjar och slutar.", och raden är tom med felet i tooltipen.
   - Välj en sträcka, och ett värde kommer.
   - Välj Samma igen, och höger försvinner, också ur flikarna.
9. **Högerklick på ST0103**, sedan Sträcka och "Faslinan börjar här": Från blir ST0103. aₙ och anteckningen ändras direkt.
10. **Ändra spannlängden för ST0105.** Dokumentation, flikarna och statusen uppdateras direkt.
11. **Töm spannlängden för ST0105.**
    - Cellen blir röd med tooltipen "Faslinan: Spannlängd saknas.".
    - Statusen visar "ST0105: Spannlängd saknas.".
    - Raden i Dokumentation är tom och har felet "ST0105: Spannlängd saknas." i tooltipen.
12. **Fel ordning:** Från ST0107 och Till ST0102 ger "Slutet ST0102 står före starten ST0107 i Stolptabell före. Byt start och slut.".
13. **Radera ST0107.**
    - Frågan nämner att raden är slut för faslinan.
    - Efter Ja visas felet att ST0107 inte finns.
    - Rullistan visar "ST0107 (finns inte)".
14. **Dubblett:** ST0104 på två rader gör båda röda, och en sträcka som slutar där får ett fel.
15. **Byt namn:** ST0102 blir ST0102A, och Från följer med.
16. **Flaggorna** på start- och slutraderna har tooltips.
17. **Stäng av automatiskt läge.**
    - Posterna försvinner ur flikarna.
    - I provprojektet visar fasraderna Exempel B och de tidigare linberäkningarna igen.
    - Slå på läget igen. Posterna kommer tillbaka på samma plats med värden ur tabellen som den är nu.
18. **Högerklick på en automatisk rad i Dokumentation:** bara Visa… och "Gå till Stolptabell före", som väljer fliken och raden.

**Stolptabell efter**

19. **Kopiera från Stolptabell före…** till ett tomt efter.
    - Ingen fråga, och raderna är gula.
    - Sträckorna och läget är som i före.
    - De rosa raderna visar samma värden som de lila.
    - Flikarna har automatiska poster för efter, med rosa ton och anteckningen "… Stolptabell efter …".
20. **Ändra spannlängden** för ST0104 till 365 och för ST0105 till 295.
    - Raderna blir rosa, och cellerna har tooltipen "I Stolptabell före: 350" respektive 310.
    - Dokumentation efter visar fas 316,8 m och topp 315,0 m, och före är oförändrad.
21. **Skriv 44,190** där före har 44,19. Raden förblir gul.
22. **Lägg till ST0110** sist. Den blir grön med tooltipen "Finns inte i Stolptabell före.".
23. **Byt namn på ST0106 i efter.** Raden blir grön.
24. **Kopiera igen.** Frågan nämner ändrade och nya rader. Nej gör ingenting, och Ja ersätter allt.
25. **Ändra Från i efter.** Bara efter ändras. "Sträckorna som i före" tar tillbaka före-tabellens sträckor.
26. **Efter av och före på:** bara de lila raderna är automatiska.

**Manuellt, sparning och övrigt**

27. **Utan automatiskt läge** fungerar Lägg till…, Koppla befintlig, Manuellt värde, Koppla bort och Ta bort som i 0.4.0.
28. **Skrivskyddat:**
    - Rutorna och knapparna är släckta.
    - De automatiska posterna syns, eftersom de är lagrade, och Visa… fungerar.
29. **Stolptabell före som i 0.4.0:** gå kort igenom punkt 4–24 i den förra planens handtest, och samma i efter.
30. **Ctrl+S, stäng och öppna.**
    - Sträckorna, läget, efter och de automatiska posterna med samma värden finns kvar direkt.
    - Öppnas projektet skrivskyddat i en andra Kalkyl syns de också där.
31. **Prestanda:** klistra in 1000 rader och slå på läget med en sträcka över alla. En ändring i en cell ska kännas direkt.
32. **Mörkt och ljust läge:** färgerna, flaggorna, de röda cellerna och tonerna i flikarna syns i båda.

---

## 17. CLAUDE.md och TODO.md

| Fil | Vem | Ändring |
|---|---|---|
| Rotens `CLAUDE.md` | samordnaren | Se nedan |
| `libs/CLAUDE.md` | paket 1 | Raderna `autoMode/` och `poleGrid/` och beroendena: "`autoMode` beror på `poleTable` och `anCalc`; `projectdb` beror också på `autoMode`; `poleGrid` beror på `glaze`, `sheetGrid`, `poleTable` och `autoMode`" |
| `libs/autoMode/CLAUDE.md`, `libs/poleTable/CLAUDE.md` | paket 1 | Filerna och reglerna, och vem som delar `poleTable` |
| `libs/projectdb/CLAUDE.md` | paket 2 | Steg 5, tabellerna och API:t. De lagrade automatiska beräkningarna (`auto_calc`, reserverade id, `syncAutoCalcs` och regeln om nya skrivningar), de vilande kopplingarna, spärrarna och `testMigrateFromV040` |
| `libs/poleGrid/CLAUDE.md`, `libs/sheetGrid/CLAUDE.md` | paket 3 | Filerna, `PoleGridHost`, färgerna och vem som använder `sheetGrid` |
| `src/windows/CLAUDE.md`, `src/CLAUDE.md` | paket 4, och samordnaren för formulärens rader | `main_window.*` (Stolptabell efter, panelerna, `PoleGrid`, de automatiska raderna i Dokumentation, märkningen i flikarna och Indata i `nsLista`), formulären och länklistan |
| `tests/CLAUDE.md` | samordnaren | Raderna för `automode`, `polegrid` och `projectdb` med paketens text |
| `doc/plans/CLAUDE.md` | samordnaren | Raden för den här planen |
| `TODO.md` | samordnaren | Se nedan |

**Rotens `CLAUDE.md`**
- **Delar:**
  - Huvudfönstret: Stolptabell efter, automatiskt läge och "Gå till".
  - Nya rader för `libs/autoMode` och `libs/poleGrid`.
  - Projektdatabasen: Stolptabell efter, automatiskt läge och de lagrade automatiska beräkningarna.
  - Formulären: de automatiska beräkningarna visas skrivskyddade.
- **Data:** "Projektet sparar … Stolptabell före och efter, inställningen för automatiskt läge och de automatiska beräkningarna. De automatiska beräkningarna är vanliga normalspanns- och linberäkningar, märkta i `auto_calc` med ett fast id per skede och lina. `projectdb` håller dem aktuella i samma transaktion som varje ändring de bygger på, och tar bort dem när läget stängs av."

**`TODO.md`**
- **Arbetspunkten:** ⬜ när planen är klar och 🔨 under implementeringen.
- **Idén om `postIfAlive`:** "finns i tre filer (Normalspann, Linberäkning och `libs/poleGrid`)".
- **Ny idé:** "💡 Utskrift av beräkningarna. De automatiska beräkningarna är lagrade (`auto_calc`) och läses som andra."

---

## 18. Risker och luckor

1. **Långa rullistor.** En rullista med 1000 poster går inte att söka i. Snabbmenyn Sträcka är snabbvägen.
2. **`GroupBox` med dolda barn.** Att ramen krymper när höger-raden döljs kontrolleras med glaze-render. Reserven är fasta mått.
3. **Lagrat ur takt med tabellen.** Det kan hända om en ny skrivväg ändrar något som `syncAutoCalcs` läser utan att köra den.
   - Regeln står i `libs/projectdb/CLAUDE.md`.
   - `checkAutoMatches` i testerna täcker dagens fem vägar.
   - Paket 2 bekräftar genomgången i sin rapport.
4. **Synkningen körs på varje sparad celländring.** Stolparna skrivs bara om när de har ändrats. Att det känns direkt mäts i handtest 31.
5. **Reserverade id utan främmande nyckel.** Raden i `auto_calc` pekar på id som saknas när läget är av.
   - Spärrarna gäller id:na också då.
   - `AUTOINCREMENT` gör att de aldrig delas ut till andra beräkningar.
6. **Vilande kopplingar kan förvåna.** En lagrad beräkning visar "–" i kolumnen Dokumentation så länge läget är på.
7. **Koden för Stolptabell före flyttas.** Det finns risk för regressioner, och handtest 29 täcker den.
8. **`setRows` nollar tooltips och bilder.** `PoleGrid::fill` sätter tillbaka dem.
9. **Stolpplats jämförs exakt.** "st0104" och "ST0104" är olika (fråga 13).
10. **Ingen ångra.** Kopieringen frågar först. Det som senast publicerades går att få tillbaka genom att stänga utan att spara.
11. **Referens fylls inte i automatiskt.** Anteckningen på beräkningarna bär sträckan (7.7).

---

## 19. Ändringar under handtestet

### 19.1 Spannlängd ur koordinaterna (användarens beslut 2026-11-26)
Ersätter fråga 8 och "Beräkningar ur koordinaterna" under Ingår inte, för Spannlängd föreg.
- **Regeln:** en tom Spannlängd föreg. räknas som det vågräta avståndet √(ΔN² + ΔE²) till närmast föregående rad som inte är helt tom, när båda har Koordinat N och E.
  - En ifylld spannlängd gäller alltid.
  - Tabellens första stolpe har ingen föregående och får inget värde. Den används inte heller i normalspannet.
  - Avståndet 0 (samma punkt två gånger) ger inget värde.
  - Värdet räknas med full precision och visas med två decimaler.
- **`libs/autoMode`:** `coordinateSpans(rows)` ger värdet per rad, och `spanPoles` använder det för en tom spannlängd. Därmed får synkningen i `projectdb`, panelen och markeringarna samma värden.
- **`libs/poleGrid`:** cellen visar värdet med Σ-bilden (`StockIcon::Sum`) och en tooltip, som de uträknade värdena i Dokumentation.
  - Modellen har kvar den tomma texten, så det sparade projektet ändras inte.
  - En inskriven spannlängd ersätter värdet. En tömd cell räknas igen.
  - Ett uträknat värde som bekräftas oförändrat (F2 och Enter) förblir uträknat.
  - Urklippet ger det visade värdet.
- **Tester:** `testCoordinateSpans` i `automode_test.cpp` och `testComputedSpan` i `polegrid_test.cpp`. Bilaga A i `automode_test.cpp` har tomma N och E, så att testerna av "Spannlängd saknas." fortfarande prövar en saknad spannlängd.

### 19.2 Kolumnordning, Stolpnr och LKL (användarens beslut 2026-11-26)
- **Kolumnordningen** i båda stolptabellerna är den i de stolptabeller som klistras in: Stolpnr, Koordinat N, E, H, Avstånd föregående, Ah, Av och LKL. Då går flera kolumner att klistra in på en gång.
  - Konstanterna i `libs/poleTable` (`kPole`…`kLkl`) har den nya ordningen, och kolumnerna i `.glazeui` följer den.
  - Databasens kolumner ändras inte. `readPoles` i `projectdb` läser fälten med namn och inte med kolumnnummer.
- **Stolpplats heter Stolpnr** i alla texter i programmet: kolumnrubriken, felen, tooltips och frågan Radera. Kolumnen Spannlängd föreg. heter Avstånd föregående.
- **LKL** skrivs alltid "227/262" (`poletable::normalizeLkl` i `acceptField`). Delarna mellan radbrytningar, tomma rader och snedstreck trimmas och skrivs med ett snedstreck.
  Det gäller inmatning och inklistring. LKL som redan ligger i ett projekt ändras inte förrän cellen skrivs om.

---

## Bilaga A – provdata

Skriv raderna i Excel med rubrikraden i rad 1 och kopiera blocket. Koordinat N och E, Av, Ah och LKL används inte i beräkningarna.

| Stolpplats | Av | Ah | Spannlängd föreg. | Koordinat N | Koordinat E | Koordinat H | LKL |
|---|---|---|---|---|---|---|---|
| ST0100 | 300 | 300 | | 6512000,00 | 412000,00 | 40,0 | 255/262 |
| ST0101 | 310 | 310 | 300 | 6511700,00 | 411950,00 | 42,5 | |
| ST0102 | 300 | 300 | 320 | 6511380,00 | 411900,00 | 45,0 | 262/262 |
| ST0103 | 315 | 315 | 280 | 6511100,00 | 411860,00 | 43,0 | |
| ST0104 | 330 | 330 | 350 | 6510750,00 | 411810,00 | 47,5 | 262/262 |
| ST0105 | 300 | 300 | 310 | 6510440,00 | 411770,00 | 50,0 | |
| ST0106 | 310 | 310 | 290 | 6510150,00 | 411730,00 | 48,0 | 241/241 |
| ST0107 | 315 | 315 | 330 | 6509820,00 | 411690,00 | 46,5 | |
| ST0108 | 285 | 285 | 300 | 6509520,00 | 411650,00 | 44,0 | |
| ST0109 | 270 | 270 | 270 | 6509250,00 | 411610,00 | 41,0 | 255/262 |

**Väntade resultat.** aₙ = √(Σa³ / Σa), avrundat till 0,1 m. Startstolpens spann ingår inte.

| Tabell | Sträcka | Spann | Σa (m) | aₙ (m) |
|---|---|---|---|---|
| Före | Faslinan ST0102–ST0107 | 280, 350, 310, 290, 330 | 1560 | 315,2 |
| Före | Topplinorna ST0101–ST0108 | 320, 280, 350, 310, 290, 330, 300 | 2180 | 313,8 |
| Före | Hela tabellen ST0100–ST0109 | nio spann | 2750 | 308,3 |
| Efter (ST0104 = 365, ST0105 = 295) | Faslinan ST0102–ST0107 | 280, 365, 295, 290, 330 | 1560 | 316,8 |
| Efter (ST0104 = 365, ST0105 = 295) | Topplinorna ST0101–ST0108 | 320, 280, 365, 295, 290, 330, 300 | 2180 | 315,0 |

- Kontrollerna 1–3 ger inga avvikelser.
- Kontrollerna 4 och 5 börjar först vid det elfte spannet.
- Alla stolpar har H, så aₙ,ΔH visas.

---

### Kritiska filer för implementeringen
- C:/Users/sehoer/Source/Kalkyl/libs/projectdb/project_data.cpp
- C:/Users/sehoer/Source/Kalkyl/libs/projectdb/project_data.h
- C:/Users/sehoer/Source/Kalkyl/libs/projectdb/dev_migration.h
- C:/Users/sehoer/Source/Kalkyl/src/windows/main_window.cpp
- C:/Users/sehoer/Source/Kalkyl/src/windows/main_window.glazeui