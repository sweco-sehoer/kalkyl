# Normalspann: beräkningen, formuläret, fliken Normalspann, linorna och Dokumentationen i projektet

**Datum:** 2026-11-23, uppdaterad 2026-11-24 med användarens svar · **Status:** klar (handtestad och granskad 2026-11-25) · **Version:** 0.2.0 (höjd på användarens uppmaning)

Planen är kontraktet för fyra implementeringspaket (A–D) och för granskningen. Header-skisserna i avsnitt 5 och schemat i avsnitt 4 är bindande. Namn, signaturer, felkoder och texter följs ordagrant. Behöver ett paket ändra en signatur meddelar det samordnaren i sin rapport, och samordnaren beslutar.

Underlag från planeringen:
- **Arket har inga formler.** `ref/Normalspannberäkning.xlsm` (bladet "Normalspann", v. 1.1) räknas helt av makrot `KALKYLERA` i bladets modul `Blad1`, som startas av en knapp. Modulen fanns inte i `ref/vba/`. Den lästes ut ur `xl/vbaProject.bin` med skriptet som `ref/vba/CLAUDE.md` beskriver. Formlerna i avsnitt 2 är återgivna ur den koden. `ref/vba/5420_Verktyg/Normalspann_20210401.bas` och `form_Info_an*.frm` innehåller bara två informationsrutor utan beräkning.
- **Arkets data är Kalkyls mockup.** Arket innehåller ST0252–ST0532 (281 stolpar, 280 spann). Resultatet aₙ = 343,1 m är exakt mockupens "Normalspannsberäkning faslina efter ombyggnation" (343.1, referens STO252-532). Arket har ST0380 = 332,247 m och ST0381 = 358,154 m, alltså "380 flyttad".
- **Spannlängderna står som text** i de flesta cellerna (inklistrade). VBA gör om dem till tal. Ett urklipp ur Excel är text i vilket fall som helst.
- **Allt i arket är kontrollerat mot en egen omräkning.** aₙ, medelspannet, totallängden och antalet röda celler i kontrollerna 1–5 stämmer exakt med arkets cachade värden och cellfärger (2.7).
- **GlazeUI v0.11.1:** DataGrid saknar områdesmarkering, dragmarkering, urklipp och Delete ("Ingen områdesmarkering; Delete tömmer inte en cell", MANUAL `widget-data-grid`). Snabbmenyn, tangenthändelserna och urklippet finns som byggstenar. Kalkyl bygger Excel-beteendet själv (3.11). Det som saknas står i avsnitt 14.

**Användarens beslut 2026-11-24** (inarbetade i avsnitten):
- **Fliken Normalspann:** planerade kolumner, anpassade till besluten om koppling och linor (7.1).
- **Medelspannet räknas som i makrot** (2.3). En kontrollfråga med siffror står i avsnitt 15.
- **Avvikelser från Excel som rättas:** punkt 3 i båda riktningarna, aₙ,ΔH bara med H på alla stolpar, och tomma rader hoppas över (2.6).
- **Dokumentation visar aₙ.**
- **Koppling:** en beräkning kan visas på flera rader, men en rad visar högst en beräkning (3.3).
- **Manuellt värde:** det ersätts utan fråga när en beräkning kopplas (3.4).
- **Koppling bara i Dokumentation:** en beräkning kopplas bara via högerklicksmenyn i Dokumentation, med "Koppla befintlig" och "Koppla bort" (3.3).
- **Formuläret** buffrar ändringarna, och Avbryt frågar. Ingen ångra.
- **Sparning:** Ctrl+S och automatisk sparning med inställbar timer, förval 5 minuter, bara när något ändrats (3.6–3.7).
- **Linorna** som används kopieras till projektet (3.9), och `Conductor` flyttas till `libs/conductor` (3.10).
- **Tabellen i formuläret:** Excel-markeringen emuleras (3.11).
- **Inklistring:** en rubrikrad hoppas över, och annan text avbryter inklistringen (6.3).
- **Testdata:** arkets spannlängder får checkas in i testet (bilaga A).
- **Nya projekt** startar tomma.
- **Stolptabell före** får en egen plan.

**Ändringar under handtestet 2026-11-24** (gäller före texten längre ned):
- **Medelspannet** delas med antalet spann, inte med antalet stolpar som i makrot (2.3, 2.7).
- **Migrering:** steg 1 är schemat i v0.1.0, och de nya tabellerna ligger i steg 2 i `libs/projectdb/dev_migration.h` (3.12, 4.1).
- **Normalspannsberäkningen har ingen lina.** aₙ beror bara på stolparna. Därför gäller:
  - `normal_span` har ingen kolumn `conductor`, och `NormalSpan` har inget fält `conductor`.
  - Formuläret har ingen rullista Lina, och `setSystemDb` och `conductorName` i `setNew` finns inte.
  - Fliken Normalspann har inte kolumnerna Lina och Lintyp. Det blir nio kolumner.
  - Menyn Koppla befintlig visar ingen lina.
  - Linorna i projektet är bara linradernas, och de rensas bara av `setDocValue` och `setDocConductor`.

---

## 1. Mål och avgränsning

### Ingår
1. **Beräkningen** enligt `ref/Normalspannberäkning.xlsm` i ett nytt bibliotek `libs/calc` med rena funktioner och headless-test:
   - normalspannet aₙ och aₙ,ΔH
   - medelspann och totallängd
   - kontrollerna 1–5 med redigerbara gränser
2. **Linorna i ett eget bibliotek `libs/conductor`.** `Conductor` och valideringen flyttas dit ur `libs/systemdb`, eftersom både systemdatabasen och projektdatabasen behöver typen.
3. **Projektdatabasen** (`.kalkyl`, schemats steg 1 ändras direkt, se 4):
   - fliken Dokumentations alla värden, med referenser
   - kopior av linorna som används
   - normalspannsberäkningarna med stolpar och gränser
   - kopplingen mellan raderna i Dokumentation och beräkningarna
4. **Sparning:**
   - Arkiv > Spara (Ctrl+S).
   - Automatisk sparning med en inställbar timer (Inställningar > Automatisk sparning, per användare i systemdatabasen).
   - `*` i titeln vid osparade ändringar och en fråga vid stängning.
   - Skrivskyddat läge spärrar alla ändringar.
5. **Dokumentation:**
   - **Normalspannsraderna** (lila och rosa "Normalspannsberäkning …") får en högerklicksmeny:
     - Utan beräkning: Lägg till…, Koppla befintlig och Manuellt värde.
     - Med beräkning: Ändra…, Koppla bort och Ta bort….
   - **Linraderna** får menyn "Uppdatera från linregistret".
   - **Värde** visar `-`, ett manuellt värde eller aₙ.
6. **Formuläret Normalspannsberäkning** (`src/windows/normal_span_dialog.*`), utformat efter arket, utan lina. Tabellen beter sig som i Excel:
   - inklistring från cellen och åt höger och nedåt
   - kopiera, klipp ut och klistra in med tabbseparerad text och decimalkomma
   - Ctrl+A och Delete
   - rektangulär markering med Shift+piltangenter och Shift+klick
7. **Fliken Normalspann** i huvudfönstret, med en rad per beräkning i gul, växlande radton. Knapparna är Ny beräkning…, Ändra…, Kopiera… och Ta bort.
8. **Tester** i `libs/calc/calc_test.cpp` (ny), `libs/conductor/conductor_test.cpp` (ny, med de flyttade lintesterna), `libs/systemdb/systemdb_test.cpp` och `libs/projectdb/projectdb_test.cpp`.

### Ingår uttryckligen inte
- **Linberäkningen.** Raderna "Linberäkning …" i Dokumentation förblir manuella talfält, men sparas nu i projektet. Gränssnittet mot en senare linberäkning är bara detta:
  - aₙ hämtas med `calc::calculateNormalSpan` på beräkningen som raden "Normalspannsberäkning …" för samma lina och skede visar (3.3).
  - Linans data hämtas ur projektets kopia (3.9).
- **Stolptabell före** är fortfarande en mockup och sparas inte. Den får en egen plan, och då kan formuläret få "Hämta från stolptabellen".
- **Ångra (Ctrl+Z)** i formulärets tabell. Avbryt i formuläret ångrar allt.
- **Markering genom att dra med musen** i formulärets tabell. Den går inte att bygga på GlazeUI v0.11.1 (3.11 och avsnitt 14).
- **Återställning av osparade arbetskopior efter krasch** (idé i `TODO.md`).
- **Versionen ändras inte.** Inga commits.

---

## 2. Beräkningen enligt Normalspannberäkning.xlsm

### 2.1 Arket
Arbetsboken har bladet "Normalspann" och ett dolt blad "Konvertering Lat och Long", som inte rör normalspannet. Det finns inga definierade namn och inga formler. Knapparna i arket:
- `cmd_Run` kör `KALKYLERA`.
- `Töm` kör `RADERA`, som tömmer C7:E499 och resultaten.
- `cmd_Info_an` och `cmd_Info_an_delta` visar två informationsrutor ur tillägget 5420_Verktyg. Deras layout är inte utläst.

| Cell | Innehåll |
|---|---|
| B2 | Rubriken "Kontroll av spannlängdernas lämplighet utifrån normalspannsmetoden", V2 "v. 1.1" |
| T3 | Infotext: "Indata: Spannlängd = Horisontell spannlängd (föregående spann). H = Plushöjd (mark/infästning). Ev. lutande spannlängd finns dold i kolumn G som vit text. Kontrollerna görs utifrån horisontell normalspann/spannlängd då skillnad inte blir så stor." |
| B4, H4 | "INDATA *valfritt" och "KONTROLL" |
| C6:E6 | `Stpnr*`, `Spannlängd`, `H*` (asterisken betyder valfritt) |
| I6:M6 | 1, 2, 3, 4, 5 (kontrollernas nummer) |
| C7:E499 | Indata. Rad 7 är **första stolpen**. |
| G8:G499 | Lutande spannlängd, avrundad till 0,1 m, dold som vit text |
| I8:M499 | Kontrollerna. Grå (242, 242, 242), röd (200, 0, 0) eller grön (0, 150, 0) bakgrund |
| R9, R10, S10 | "Normalspann, an", värdet, "m" |
| T9, T10, U10 | "Normalspann, an, ΔH", värdet eller "-", "m" |
| R12, R13 | "Medelspann" och värdet |
| T12, T13 | "Totallängd" och värdet |
| R15, P16, R16, S16 | "Längsta tillåtna spannlängd", "1:", **1,5**, "x an" |
| R18, P19, R19, S19 | "Kortast tillåtna spannlängd", "2:", **0,5**, "x an" |
| R21, P22, Q22, R22 | "Maximala förhållandet mellan två intilliggande spann", "3:", "1:", **2,5** |
| R24, P26, Q26, R26, S26 | "Normalspannet för 10 på varandra följande spann, an,10, får ej understiga", "4:", "an,10>", **0,95**, "x an" |
| R28, P30, Q30, R30, S30 | "… får ej överstiga", "5:", "an,10<", **1,15**, "x an" |
| P33–P35 | "Punkt 3 indikerar aktuell rad och föregående rad." "Punkt 4 och 5 kontrolleras endast vid fler än 20 spann." "Värdet visar normalspannet för de 10 föregående spannen och relationen till an." |
| P39 | Metoden: "Normalspannsmetoden bygger på fri rörlighet av infästningspunkterna mellan avspännings-/stolparna. Vid för stora avvikelser av spannen i förhållande till normalspannet begränsas detta och nedhänget kan anta en annan linbåge än normalspannets linbåge." |
| P43–P44 | "Effekt av punkt 4 – en sträcka av korta spann": det lokala normalspannet kan ge större nedhäng, vilket ska beaktas vid kontroll av fri höjd mot mark och korsande objekt. |
| P48–P49 | "Effekt av punkt 5 – en sträcka av långa spann": mindre nedhäng ger högre laster, vilket ska beaktas för stolpe, isolatorkedja och isolatorutslag. |
| P54 och bilden | En tabell över nedhäng b vid olika aₙ och a, för Al-59 910 mm², σ0 = 45 MPa och +60 °C. Den är bara information. |

Gränscellerna R16, R19, R22, R26 och R30 är ljusgrå inmatningsceller, och användaren kan ändra dem.

### 2.2 Indata
- **En rad per stolpe.** Stpnr (valfritt), Spannlängd och H (valfritt, plushöjd för mark eller infästning, m).
- **Spannlängden är det horisontella spannet före stolpen.** Spannet på första raden (rad 7) ligger före sträckan och **ingår inte** i summorna. Makrot börjar på rad 8. Med n + 1 ifyllda rader finns alltså n spann i summorna. Arkets exempel har D7 = 274, och det värdet ingår aldrig.
- **H på första raden används** som starthöjd för ΔH i första spannet.
- **Stpnr används inte i beräkningen.** Kalkyl visar första och sista stolpens Stpnr som sträckans från och till.

### 2.3 Formlerna (makrot `KALKYLERA`)
Beteckningar: spann i = 1 … n står på arkrad 7 + i, aᵢ = D(7+i), H₀ = E7 och Hᵢ = E(7+i).

1. **Slingan** går från rad 8 till 499 och slutar vid första tomma D (`GoTo Tom_Rad`). `ii` är då den första tomma raden, alltså ii = 8 + n.
2. **Summor:** Σa = Σ aᵢ och Σa³ = Σ aᵢ³, för i = 1 … n.
3. **Normalspannet** skrivs i R10:
   ```
   normalspann = Round(Sqr(Σa³ / Σa), 1)
   ```
   aₙ = √(Σaᵢ³ / Σaᵢ), avrundat till 0,1 m.
4. **Medelspannet** skrivs i R13:
   ```
   medelspann = Round(Σa / (ii − 7), 1)
   ```
   Nämnaren ii − 7 är antalet stolpar (rad 7 till sista raden), alltså n + 1, men summan har bara n spann. I arket ger det 89 160,401 / 281 = 317,3. Kalkyl delar med antalet spann (användarens beslut 2026-11-24, se 2.6): `meanM = Σa / spanCount` = 89 160,401 / 280 = **318,4**.
5. **Totallängden** skrivs i T13: Σa, oavrundat.
6. **ΔH**, på varje rad där E inte är tom:
   ```
   Δhᵢ = E(ii) − E(ii−1)            (en tom E räknas som 0)
   lᵢ  = Sqr(aᵢ² + Δhᵢ²)            (lutande spannlängd; G = Round(lᵢ, 1))
   Σl      += lᵢ
   Σ(a⁴/l) += aᵢ⁴ / lᵢ
   ```
   Flaggan `bool_normalspann_deltaH` sätts om för varje rad och gäller alltså bara den **sista** raden.
7. **Normalspannet med ΔH** skrivs i T10:
   ```
   normalspann_deltaH = Round(Sqr(Σ(a⁴/l) / Σl), 1)
   ```
   aₙ,ΔH = √(Σ(aᵢ⁴/lᵢ) / Σlᵢ), avrundat till 0,1 m. T10 får "-" om sista raden saknar H.

### 2.4 Kontrollerna 1–5 (andra slingan, en gång per spann)
aₙ är det **avrundade** värdet i R10. Det syns i I8 = 338 / 343,1 = 0,98514. Med oavrundat aₙ hade det blivit 0,98503.

| Nr | Kolumn | Värde | Rött när | Annars |
|---|---|---|---|---|
| 1 | I | aᵢ / aₙ | aᵢ / aₙ > R16 (1,5) | grått |
| 2 | J | aᵢ / aₙ | aᵢ / aₙ < R19 (0,5) | grått |
| 3 | K | "1 : " & Round(q, 1), där q = större / mindre av aᵢ₋₁ och aᵢ | q > R22 (2,5), raden och raden före | grönt |
| 4 | L | Round(aₙ,₁₀, 0) & " / " & Round(aₙ,₁₀ / aₙ, 2) | aₙ,₁₀ / aₙ < R26 (0,95) | grönt |
| 5 | M | samma text som L | aₙ,₁₀ / aₙ > R30 (1,15) | grönt |

- **Punkt 3** gäller från spann 2. Första spannet (K8) får "-". Makrot har två grenar:
  - aᵢ₋₁ < aᵢ: q = aᵢ / aᵢ₋₁ jämförs med R22.
  - Annars: q = aᵢ₋₁ / aᵢ jämförs med **Q22**, som innehåller texten "1:". I VBA är ett tal alltid mindre än en sträng när båda är Variant (`spannlangd_fore` är odeklarerad), så jämförelsen är alltid falsk. Ett kort spann efter ett långt blir därför aldrig rött (2.6).
- **Punkt 4 och 5** gäller från rad 18 (`If ii > 17`, ändrat i v. 1.1), alltså från spann 11. aₙ,₁₀ är normalspannet för de **10 föregående** spannen. Det aktuella spannet ingår inte:
  ```
  aₙ,₁₀(i) = Sqr(Σ_{j=1..10} aᵢ₋ⱼ³ / Σ_{j=1..10} aᵢ₋ⱼ)      (oavrundat i jämförelsen)
  ```
  Tidigare rader får "-". Texten i P34 ("fler än 20 spann") stämmer inte med koden i v. 1.1, och Kalkyl skriver rätt text (6.1).
- **Kalkyl** visar kontroll 1 och 2 med två decimaler ("0,45"). Excels format "0.0" visar 0,4547 som en röd "0,5", vilket ser motsägelsefullt ut. Texterna i kontroll 3–5 följer Excel: `formatDecimal(roundTenth(q))` ger "1 : 1,9" eller "1 : 1", och `formatDecimal(std::round(x))` + " / " + `formatDecimal(std::round(x / aₙ · 100) / 100)` ger "356 / 1,04".

### 2.5 Avrundning
- VBA:s `Round` avrundar till jämnt vid exakt mittvärde. Kalkyl avrundar halva bort från noll: `roundTenth(x) = std::round(x · 10) / 10`.
- Rötterna blir i praktiken aldrig exakt ett mittvärde, så resultaten blir lika. Testet visar det på arkets data (2.7).
- aₙ och aₙ,ΔH lagras aldrig. De räknas från stolparna varje gång en beräkning läses (3.3).

### 2.6 Avvikelser från Excel (beslutade)
| Excel | Kalkyl | Skäl |
|---|---|---|
| Punkt 3 blir aldrig röd när aᵢ₋₁ > aᵢ (Q22 i stället för R22) | Båda riktningarna jämförs med gränsen | Felet är uppenbart, och texten "1 : q" visas redan i båda riktningarna. |
| aₙ,ΔH räknas om sista raden har H. Saknad H räknas som 0, och rader utan H hoppas över i summorna. | aₙ,ΔH räknas bara när **alla** stolpar har H, också den första. Annars visas "-" med en varning. | En saknad höjd som räknas som 0 ger ett ΔH på hela plushöjden. |
| Medelspannet delas med antalet stolpar (ii − 7 = n + 1) | Medelspannet delas med antalet spann n | Summan har n spann. Användarens beslut 2026-11-24. |
| Slingan slutar vid första tomma spannlängd | Helt tomma rader hoppas över. En rad med Stpnr eller H men utan spannlängd är ett fel (utom första stolpen). | Tabellen tillåter tomma rader efter Delete (3.11). Ett tyst stopp mitt i sträckan vore farligt. |

aₙ, aₙ,ΔH och kontrollerna 1, 2, 4 och 5 är oförändrade mot Excel (de använder inget antal).

### 2.7 Testexempel
Talen nedan är räknade under planeringen och kontrollerade mot arkets cachade värden och cellfärger.

**Exempel A – arkets data (Excel):** ST0252–ST0532, 281 stolpar och 280 spann (rad 8–287). Spannen för testet tas fram enligt bilaga A.

| Storhet | Värde |
|---|---|
| Σa | 89 160,401 m (= T13) |
| Σa³ | 10 498 021 180,00667 |
| aₙ oavrundat | 343,1371310 |
| aₙ | **343,1** (= R10) |
| aₙ,ΔH | "-" (ingen H) |
| Medelspann | **318,4** (Σa/280 = 318,4300; Excels R13 visar 317,3, se 2.6) |
| Kontroll 1 | 0 spann |
| Kontroll 2 | 5 spann: ST0325 (156 m, 0,4547), ST0375 (165), ST0393 (152), ST0451 (150), ST0459 (149) |
| Kontroll 3 | 0 par. Största q är 2,316 vid ST0404 (190 → 440). Största q åt andra hållet är 2,013 vid ST0459 (300 → 149). |
| Kontroll 4 | 64 spann, första ST0300 och sista ST0503. Hela listan står i bilaga B. |
| Kontroll 5 | 1 spann: ST0283, där aₙ,₁₀ = 395,250 och kvoten 1,15200 ger "395 / 1,15" |
| aₙ,₁₀ | Första vid ST0263 (spann 11): 356,496, "356 / 1,04" (= L18). Sista vid ST0532: 332,253, "332 / 0,97" (= L287). |
| Kontroll 1–2, första spannet | ST0253: 338 / 343,1 = 0,98514 (= I8) |
| Gräns 2 ändrad till 0,45 | 3 spann: ST0393, ST0451 och ST0459 |

**Exempel B – syntetiskt, kontrollerna och rättelsen av punkt 3:** stolparna P0–P11. P0 har spannlängden 999, som inte ingår i summorna. Spannen för P1–P11 är 100, 100, 100, 100, 100, **400**, 100, 100, 100, 100, 100.

| Storhet | Värde |
|---|---|
| Σa, antal spann och antal stolpar | 1 400, 11 och 12 |
| aₙ | √(74 000 000 / 1 400) = 229,9068134, alltså **229,9** |
| Medelspann | 1 400 / 11 = 127,3 |
| Kontroll 1 | P6 (400 / 229,9 = 1,7399) |
| Kontroll 2 | P1–P5 och P7–P11 (100 / 229,9 = 0,4350), 10 spann |
| Kontroll 3 | P1 "-". P2–P5 "1 : 1" grönt. P6 "1 : 4" (P5 och P6 röda). P7 "1 : 4" (P6 och P7 röda, Excel: grönt). P8–P11 "1 : 1". Röda rader: P5, P6 och P7. |
| Kontroll 4–5 | P1–P10 "-". P11: aₙ,₁₀ = √(73 000 000 / 1 300) = 236,968, kvot 1,0307, "237 / 1,03", grönt. |

**Exempel C – ΔH:** stolparna P0–P3, H = 10, 20, 15 och 30 m. Spannlängderna är P0 250 (ingår inte), P1 300, P2 200 och P3 400.

| Storhet | Värde |
|---|---|
| Δh | 10, −5, 15 |
| l | 300,166620; 200,062490; 400,281151 (Σl = 900,510261) |
| a⁴/l | 26 985 012,488; 7 997 501,171; 63 955 047,405 (Σ = 98 937 561,065) |
| aₙ | √(99 000 000 / 900) = 331,6624790, alltså **331,7** |
| aₙ,ΔH | √(98 937 561,065 / 900,510261) = 331,4639237, alltså **331,5** |
| Medelspann och totallängd | 900 / 3 = 300,0 och 900 |
| Utan H på P2 | aₙ = 331,7, aₙ,ΔH tomt, varningen "H saknas på 1 av 4 stolpar. aₙ,ΔH räknas bara när alla stolpar har H." |

**Kantfall:**
| Indata | Väntat |
|---|---|
| Inga stolpar, eller en stolpe | Inget resultat. Felet (rad −1) "Minst två stolpar (ett spann) behövs." |
| P2 med Stpnr men utan spannlängd | Fel på P2, kolumn Length: "Spannlängd saknas." |
| P2 med spannlängden 0 | Fel på P2: "Spannlängden måste vara större än 0." |
| En helt tom rad mellan P1 och P2 i exempel C | Samma resultat som utan raden, också medelspannet (300,0). poleIndex i `spans` pekar på de ursprungliga raderna. |
| Gräns 0 eller NaN | Fel (rad −1): "Gränserna måste vara tal större än 0." |

---

## 3. Beslut med motivering

### 3.1 Normalspannsraderna i Dokumentation och vilka rader som får menyn
- **Den gröna gruppen** ("Normalspann faslina", "Normalspann topplina …") är uppgifter ur ByggPM, alltså andra dokument, precis som blått och gult. De förblir vanliga talfält utan meny.
- **Normalspannsmenyn finns bara på de lila och rosa raderna "Normalspannsberäkning …"**, sex rader i `kByggPmRows`:

| Rad (text när topplinorna är egna) | Text när Topplina höger är "Samma" | Lina | Skede | Nyckel |
|---|---|---|---|---|
| Normalspannsberäkning faslina före ombyggnation | samma | Faslina | Före | `ns_calc_phase_before` |
| Normalspannsberäkning faslina efter ombyggnation | samma | Faslina | Efter | `ns_calc_phase_after` |
| Normalspannsberäkning topplina vänster före ombyggnation | Normalspannsberäkning topplina före ombyggnation | Topplina vänster | Före | `ns_calc_top_left_before` |
| Normalspannsberäkning topplina vänster efter ombyggnation | Normalspannsberäkning topplina efter ombyggnation | Topplina vänster | Efter | `ns_calc_top_left_after` |
| Normalspannsberäkning topplina höger före ombyggnation | döljs | Topplina höger | Före | `ns_calc_top_right_before` |
| Normalspannsberäkning topplina höger efter ombyggnation | döljs | Topplina höger | Efter | `ns_calc_top_right_after` |

- När topplinorna är "Samma" syns alltså fyra rader med normalspannsmeny, och annars sex.
- **De tre linraderna** (Faslina, Topplina vänster, Topplina höger) får en egen meny med "Uppdatera från linregistret" (3.9).
- Raderna "Linberäkning …" får ingen meny. De förblir manuella talfält.

### 3.2 Beräkningskoden i ett nytt bibliotek `libs/calc`
- **Namnet:** `kalkyl_calc`, namnrymden `kalkyl::calc` och includes `"calc/normal_span.h"`. Det beror bara på standardbiblioteket, och linberäkningen kan senare ligga i samma bibliotek.
- **Filerna:**
  - `normal_span.*`: beräkningen.
  - `table_text.*`: tal med decimalkomma, tabbseparerat urklipp och var ett inklistrat block hamnar. Formuläret använder dem, och senare även stolptabellen.
- **Varför inte i `projectdb` eller i fönstret:** beräkningen ska gå att testa headless mot Excel-talen, utan databas och utan GUI.
- **Beroenden:** `projectdb` länkar `kalkyl_calc` och `kalkyl_conductor` PUBLIC, eftersom `projectdb::NormalSpan` bär `calc::SpanPole`, `calc::SpanLimits` och `conductor::Conductor`.

### 3.3 Kopplingen: en beräkning på flera rader, en beräkning per rad
- **En beräkning har ingen egen lina-och-skede-plats.** Den har stolpar, gränser, en lina (3.9) och en anteckning. Var den visas bestäms av kopplingarna.
- **Kopplingstabellen `doc_normal_span(doc_key, span_id)`:**
  - `doc_key` är primärnyckel, så en rad i Dokumentation visar högst en beräkning.
  - `span_id` får förekomma på flera rader, så en beräkning kan visas på flera rader, till exempel faslina före och efter när ombyggnaden inte rör sträckan, eller fas- och topplina på samma sträcka.
  - `ON DELETE CASCADE` tar bort kopplingarna när beräkningen tas bort.
- **Kopplingen görs bara i Dokumentationens högerklicksmeny** (användarens beslut):
  - **Lägg till…** öppnar formuläret för en ny beräkning, som kopplas till raden när användaren trycker OK.
  - **Koppla befintlig** har en undermeny över alla beräkningar i projektet, också de som redan visas på andra rader.
  - **Koppla bort** tar bort kopplingen på den raden. Beräkningen och dess andra rader påverkas inte.
  - Formuläret har ingen kryssruta för koppling, och listan i fliken Normalspann har ingen kopplingspost i sin meny.
- **Ta bort… tar bort beräkningen** med alla dess kopplingar. Frågan listar raderna den visas på (7.4).
- **Kopiera…** finns i fliken Normalspann, eftersom samma beräkning nu kan visas på flera rader och därför sällan behöver kopieras. Den skapar en ny, lös beräkning med samma stolpar och gränser. Användaren behåller den (2026-11-24), eftersom man ibland utgår från en befintlig beräkning. Det passar när "efter ombyggnation" bara skiljer några spann från "före", som arkets ST0380 och ST0381.
- **Resultatet lagras inte.** aₙ räknas från stolparna när projektet läses och efter varje ändring. Det tar mindre än en millisekund för några hundra spann. Då kan ett lagrat värde aldrig hamna i otakt med stolparna.
- **Gränssnittet mot linberäkningen:** beräkningen som raden `normalSpanDocKey(wire, stage)` visar ger aₙ via `calculateNormalSpan`. Finns ingen används det manuella värdet i Dokumentation.

### 3.4 Värde-cellen: `-`, manuellt värde eller beräknat
- **Ingen beräkning kopplad:** talfält (`Editor::Number`, `allowEmpty`, `emptyText = "-"`, suffix " m").
  - Tomt visar `-`.
  - Ett skrivet tal är det manuella värdet och sparas i `doc_value`.
- **Beräkning kopplad:** cellen visar aₙ (avrundat till 0,1, kanonisk text med punkt) och låses med `readOnly = true` och suffix " m".
  - Cellen får bilden `glaze::stockIcon(glaze::StockIcon::Sum)` via `setCellImage`.
  - Tooltipen är "Beräknad: ST0252–ST0532, 280 spann. Högerklicka för att ändra." Med fel blir den "Beräkningen har fel: <första felet>", och Värde visar `-`.
- **När en beräkning kopplas** till en rad töms radens manuella värde i samma transaktion, utan fråga (användarens beslut). Referensen står kvar.
- **Ta bort och Koppla bort** ger därför `-` tills användaren skriver ett nytt värde.

### 3.5 Dokumentationen i projektet: en nyckel per rad
- **Nycklar:** varje rad i `kByggPmRows` får en stabil nyckel. Värdet sparas som text i `doc_value(key, value, reference)`, med tal i kanonisk form med punkt som cellen bär och val som text.
- **Fördelar:**
  - En ny rad i Dokumentation kräver ingen schemaändring.
  - Nyckeln är oberoende av radens text, som ändras med läget "Samma".
- **En tom uppgift lagras inte.** Ett tomt värde och en tom referens tar bort raden.
- **Tabell 3.5:**

| Rad | Nyckel | Värdet |
|---|---|---|
| Faslina | `phase_conductor` | linans namn, som finns i `project_conductor` (3.9) |
| Konfiguration | `configuration` | "Singel", "Duplex" eller "Triplex" |
| Topplina vänster | `top_conductor_left` | linans namn |
| Topplina höger | `top_conductor_right` | linans namn eller "Samma" (= samma lina som vänster) |
| Uppspänning fas | `tension_phase` | tal |
| Uppspänning topp vänster | `tension_top_left` | tal |
| Uppspänning topp höger | `tension_top_right` | tal |
| Normalspann faslina | `normal_span_phase` | tal |
| Normalspann topplina vänster | `normal_span_top_left` | tal |
| Normalspann topplina höger | `normal_span_top_right` | tal |
| Max temp | `temp_max` | tal |
| Min temp | `temp_min` | tal |
| Normalspannsberäkning … (sex rader) | `normalSpanDocKey(wire, stage)`, se 3.1 | manuellt värde. Den kopplade beräkningen står i `doc_normal_span`. |
| Linberäkning … (sex rader) | `line_calc_phase_before`, `line_calc_phase_after`, `line_calc_top_left_before`, `line_calc_top_left_after`, `line_calc_top_right_before`, `line_calc_top_right_after` | tal |

- **Nya projekt startar tomma** (användarens beslut). Förval när nyckeln saknas: Konfiguration "Singel", Topplina höger "Samma" och allt annat tomt. Mockupens förval i `kByggPmRows` tas bort.

### 3.6 Sparning: arbetskopian direkt, publicering med Ctrl+S och automatiskt
- **Arbetskopian först.** Varje ändring skrivs direkt i den lokala arbetskopian, i en liten transaktion:
  - en sparad cell i Dokumentation
  - ett linval
  - OK i formuläret
  - Ta bort, Koppla befintlig, Koppla bort och Uppdatera från linregistret

  Det är arbetsmodellen i Grunder-planen 2.3. Arbetskopian är lokal, så det går snabbt.
- **Publicering:** projektet publiceras till projektfilen med Arkiv > Spara (Ctrl+S) eller med den automatiska sparningen (3.7). Båda gör `snapshot()` på UI-tråden och `ProjectDb::publish` i samma `glaze::threads::Worker`, och sedan `published(...)`.
- **Osparat syns och frågas efter.** `ProjectDb` räknar en revision för varje skrivning och minns revisionen i den senaste publicerade bilden. Skiljer de sig visar titeln `*`, och en stängning frågar Ja/Nej/Avbryt (7.6).
- **Nej vid stängningen** kastar ändringarna sedan senaste sparningen. Arbetskopian tas bort som i dag, och originalet är orört.

### 3.7 Automatisk sparning
- **Inställningen** är per användare och gäller alla projekt. Den sparas i systemdatabasen som nyckeln `autosave_minutes` i den befintliga tabellen `setting` (nyckel och värde).
  - En ny nyckel kräver ingen schemaändring. Steg 1 och `kExpectedTables` i systemdatabasen är oförändrade.
  - Värdena är 0 (av), 1, 5, 10, 15 och 30 minuter.
  - Förvalet är 5, också när värdet saknas eller är okänt.
- **Var den ställs in:** Inställningar > Automatisk sparning, en undermeny med radioposterna "Av", "1 minut", "5 minuter", "10 minuter", "15 minuter" och "30 minuter".
  - Den valda posten är markerad.
  - Ett val sparas direkt och startar om timern.
  - Det är det enklaste: inget nytt fönster, och valet syns när menyn öppnas.
- **Timern:** `autosaveTimer` (`glaze::Timer`, upprepad) i huvudfönstret, med intervallet minuter · 60 000 ms. Den startar i `onLoad` när inställningen inte är Av och projektet inte är skrivskyddat.
- **Vid varje tick** sparas projektet bara när alla villkor gäller:
  - projektet är inte skrivskyddat (timern startas aldrig då)
  - ingen publicering pågår (`saving_`)
  - projektet är ändrat (`isModified()`)
  - den automatiska sparningen är inte avstängd efter en konflikt

  Annars hoppas ticket över.
- **Vad som sparas:** det som finns i arbetskopian. Den automatiska sparningen anropar inte `commitEdits()`, så en cell som redigeras och ett öppet formulär med osparade ändringar rörs inte. Deras ändringar kommer med i nästa sparning.
- **Ticket kan komma medan en modal dialog är öppen**, eftersom timern tickar i dialogens pump. Det går bra, för bilden av arbetskopian är alltid konsekvent.
- **Fel stör aldrig med en dialog.** Utfallet visas bara i sparpanelen i statusraden:
  - Lyckat: "Sparat automatiskt hh:mm".
  - Fel: "Automatisk sparning misslyckades hh:mm: <message>". Nästa tick försöker igen.
- **Konflikt** (projektfilen har ändrats av någon annan sedan den öppnades):
  - Statusraden visar "Automatisk sparning avstängd: projektfilen har ändrats av någon annan. Spara med Ctrl+S för mer information."
  - Den automatiska sparningen är sedan avstängd tills projektet stängs, eftersom varje nytt försök skulle ge samma konflikt.
  - Ctrl+S visar konflikten i en meddelanderuta, som tidigare.
- **Skrivskyddat läge:** timern startas inte, och menyn Automatisk sparning går fortfarande att ändra, eftersom den gäller alla projekt.
- **Ctrl+S medan en automatisk sparning pågår** ger statusraden "Sparning pågår…". Är projektet fortfarande ändrat efteråt kan användaren spara igen.
- **Stängning under en sparning** väntar på sparningen (7.6).

### 3.8 Skrivskyddat läge
- **Inga skrivningar:** dataskrivningarna i `projectdb` ger `AccessDenied` när projektet är öppnat skrivskyddat. UI:t låter dem aldrig nås.
- **Dokumentation:**
  - Varje Värde- och Referenscell har `readOnly = true` (rullistorna också).
  - Menyn på normalspannsraderna har bara "Visa…" när en beräkning är kopplad, och annars ingen meny.
  - Linraderna har ingen meny.
- **Formuläret** öppnas med `setReadOnly(true)`:
  - Allt syns, Ctrl+C och Ctrl+A fungerar och inget går att ändra.
  - OK är dold och Avbryt heter Stäng.
- **Fliken Normalspann:** Ny beräkning, Kopiera och Ta bort är inaktiva, och Ändra… heter Visa….
- **Arkiv > Spara** är inaktiv, och den automatiska sparningen startar inte.

### 3.9 Linorna kopieras till projektet
- **Kopior per projekt:** projektet sparar en kopia av varje lina som används, med alla fält i linregistret (namn, typ och B–N), i tabellen `project_conductor`. Det gäller:
  - de tre linraderna i Dokumentation (fas, topp vänster, topp höger)
  - linan i varje normalspannsberäkning
- **Skälet:** en beräkning ska ge samma svar oavsett vems dator som öppnar projektet. Linregistret är per användare (Lindata-planen 2.10).
- **Namnet identifierar linan i projektet.** Det är unikt utan hänsyn till ASCII-versaler (`COLLATE NOCASE`), som i registret. Kopior byter aldrig namn. Därför refererar raderna i Dokumentation (`doc_value.value`) och beräkningarna (`normal_span.conductor`) till kopian med namnet.
- **Val i en rullista:**
  - Rullistan visar linregistret och dessutom de linor i projektet som saknas i registret, sorterade efter namn fallande som i dag.
  - **Finns en lina med samma namn redan i projektet återanvänds den.** Kopian ändras inte, och värdena i registret används inte.
  - Annars kopieras linan ur registret in.
- **Linor som inget använder tas bort** i samma transaktion som ändringen som gjorde dem oanvända. Projektet innehåller alltså exakt de linor som används. Väljs samma namn senare kopieras registrets aktuella värden in.
- **Linan saknas i registret men finns i projektet** (en kollegas egen lina, en lina som tagits bort ur registret eller fått nytt namn i Lindata):
  - Linan visas som vanligt, med projektets kopia.
  - Tooltipen är "Linan finns inte i ditt linregister. Projektets kopia används."
  - Den finns kvar i rullistan, så den går att välja igen.
  - Alla beräkningar använder kopian.
- **Kopian skiljer sig från registret** (samma namn, men någon annan typ eller något annat värde enligt `conductor::sameConductorData`):
  - Tooltipen är "Projektets kopia skiljer sig från ditt linregister. Högerklicka för att uppdatera."
  - Cellen får bilden `glaze::stockIcon(glaze::StockIcon::Warning)`.
- **Uppdatera från linregistret** tas med, eftersom den är liten: en menypost, en funktion i `projectdb` och ett test.
  - Posten finns i linradernas högerklicksmeny och är aktiv bara när registret har linan och kopian skiljer sig.
  - Den ersätter kopians typ och värden med registrets och behåller namnet och id.
  - Eftersom kopian delas ändras linan för alla rader och beräkningar som använder den. Statusraden säger "Linan <namn> uppdaterad från linregistret."
- **Lindata:** när fönstret Lindata stängs läses registret om, och tooltipsen räknas om. Projektets kopior ändras aldrig av Lindata. Valt id per rad i sessionen (`selectedConductor_`) behövs inte längre och tas bort.

### 3.10 `Conductor` flyttas till `libs/conductor`
- **Var typen ligger:** `libs/systemdb/conductor.{h,cpp}` flyttas till `libs/conductor/conductor.{h,cpp}`, med biblioteket `kalkyl_conductor`, namnrymden `kalkyl::conductor` och includes `"conductor/conductor.h"`. Det var redan förutsett i Lindata-planen 2.12, och idén i `TODO.md`.
- **Skälet:** `projectdb` behöver typen och valideringen, men ska inte bero på systemdatabasen.
- **Innehållet** är oförändrat utom namnrymden, plus två funktioner:
  - `sameConductorData` (typ och värden lika, namn, id och ursprung ignoreras)
  - `compareNames` (den naturliga jämförelsen som i dag ligger i `main_window.cpp`, så att huvudfönstret och formuläret sorterar lika)
- **Biblioteket** beror bara på standardbiblioteket.
- **Användare som byter namnrymd:** `systemdb` (header, implementation och test), `conductor_dialog.*` och `main_window.*`.
- **`kConductorSeedV1`** och registrets tabeller ligger kvar i `systemdb`. Lintesternas regeldel (`testConductorRules`) flyttas till `libs/conductor/conductor_test.cpp`.
- **Flytten görs så:** innehållet skrivs i de nya filerna med Write, och de gamla filerna tas bort (`rm libs/systemdb/conductor.h libs/systemdb/conductor.cpp`). Det är en borttagning, ingen omskrivning med skal.

### 3.11 Tabellen i formuläret som i Excel: vad GlazeUI har och vad Kalkyl bygger
**Kontrollerat i MANUAL.md och headrarna för v0.11.1:**

| Behov | GlazeUI v0.11.1 | Kalkyl bygger |
|---|---|---|
| Rektangulär cellmarkering | **Saknas.** "Ingen områdesmarkering" (`widget-data-grid`). I `Cell`-läget finns bara den aktuella cellen. `FullRow` + `setMultiSelect` ger radval med Shift/Ctrl-klick, Shift+Upp/Ned och Ctrl+A. | Egen markering: ankare och ände, ritad med `setCellColor` (6.3) |
| Markera genom att dra med musen | **Saknas.** Widgets har inget `onMouseMove` (bara ritytan), och `columnAt` är protected. | Går inte. Shift+klick ersätter. |
| Urklipp | `gfx::clipboardText()` och `gfx::setClipboardText()` finns (`gfx`). CRLF blir LF vid läsning och LF blir CRLF vid skrivning. DataGrid kopierar och klistrar inte själv. | Ctrl+C, Ctrl+X och Ctrl+V i `onKeyDown`, med `calc::parseTsv`, `formatTsv` och `planPaste` |
| Tangenthändelser | `onKeyDown` finns på varje widget och når fokusenheten (den fokuserade widgeten och datagriden) **före** widgetens eget beteende. `e.handled = true` tar tangenten. | Ctrl+A, Ctrl+C, Ctrl+X, Ctrl+V, Delete, Shift+piltangenter, Shift+Home/End och Ctrl+Shift+Home/End |
| Ctrl+A | Bara som radval i `FullRow` med flerval | Egen: alla celler |
| Delete | **Saknas** ("Delete tömmer inte en cell") | Egen: tömmer indatacellerna i markeringen |
| Aktuell cell | `currentRow()`, `currentColumn()`, `setCurrentCell(r, c)` och `ensureVisible(r)`. Inget event när den aktuella cellen flyttas i `Cell`-läget. | Synkas efter tangent och klick med `glaze::app().post` (6.3) |
| Snabbmeny | `onContextMenu` (routad) och `glaze::Menu::popupAt`. DataGrid har ingen `contextRow()` (bara ListView). | Raden med `fromWindow({e.x, e.y})` + `rowAt(p.x, p.y)`. Kolumnen går inte att få fram. |
| Infoga och ta bort rader | `addRow`, `removeRow` och `setRows`. Ingen `insertRow`. | Infoga med `setRows` på hela tabellen |
| Ångra | Saknas i DataGrid | Ingenting |

**Beslut (användaren godkände rekommendationen):**
- Kalkyl emulerar Excels rektangulära markering med tangentbordet och Shift+klick, och allt urklipp.
- Markering genom att dra med musen ingår inte.
- Samordnaren lägger en idé i `TODO.md` om områdesmarkering i GlazeUI (drag, `selectedRange()` och ett event för aktuell cell). När den finns byts emuleringen ut utan att resten av formuläret ändras.

### 3.12 Migrering från v0.1.0 och schemakontroll
- **Beslut 2026-11-24:** v0.1.0 är taggad på commit 8f4f3d5, och versionen blir 0.2.0. Projekt från v0.1.0 (`user_version` 1, bara `project`) ska migreras.
- **Utvecklingsmigreringen** ligger i `libs/projectdb/dev_migration.h`: `dev::kStep2` skapar de nya tabellerna (`IF NOT EXISTS`). Steg 1 är v0.1.0:s och ändras aldrig. `kSchemaVersion` = 2. Vid 1.0.0 slås stegen ihop och filen töms (rotens CLAUDE.md).
- **Systemdatabasen** har samma schema som i v0.1.0; den automatiska sparningen är en nyckel i `setting`. Ingen migrering behövs.
- **Schemakontroll:** `ProjectDb::open` räknar tabellerna i `kExpectedTables` efter `migrate`. Saknas någon (skadad fil) blir felet `NotADatabase` med texten "Projektfilen saknar delar som den här versionen av Kalkyl behöver. Filen är skadad:" följd av en radbrytning och sökvägen.
  - Låset och arbetskopian städas som vid andra fel.
  - Texten går inte genom `relabel`, eftersom den annars skulle skrivas över.
- **Test:** `testMigrateFromV010` skapar en fil som i v0.1.0, öppnar, sparar och öppnar igen. `testMissingTablesRejected` provar schemakontrollen.

### 3.13 Gränserna i kontrollerna sparas per beräkning
- Gränserna är redigerbara inmatningsceller i Excel, så de får vara det även i formuläret.
- Förvalen är Excels: 1,5, 0,5, 2,5, 0,95 och 1,15.
- Varje beräkning sparar sina egna gränser. Det finns inga projektgemensamma förval.

---

## 4. Schema

### 4.1 Projektdatabasens steg 1 och 2
Steg 1 (`kSchemaV1` i `project_db.cpp`) är v0.1.0:s tabell `project` och ändras aldrig. Övriga tabeller nedan ligger i steg 2 (`dev::kStep2` i `dev_migration.h`, med `IF NOT EXISTS`), se 3.12. Tabellerna är inte `STRICT`, som `project`.

```sql
CREATE TABLE project (
    id           INTEGER PRIMARY KEY CHECK (id = 1),
    name         TEXT NOT NULL,
    created_utc  TEXT NOT NULL,
    created_with TEXT NOT NULL
);
-- Linorna som projektet använder: kopior av linregistrets linor (Lindata B-N), unika på namn.
-- Inga CHECK på värdena: reglerna ligger i conductor::validateConductor.
CREATE TABLE project_conductor (
    id                    INTEGER PRIMARY KEY AUTOINCREMENT,
    name                  TEXT NOT NULL COLLATE NOCASE UNIQUE,
    type                  TEXT NOT NULL DEFAULT '',
    area_mm2              REAL NOT NULL,
    diameter_mm           REAL NOT NULL,
    e_initial_mpa         REAL NOT NULL,
    e_permanent_mpa       REAL NOT NULL,
    e_initial_limit_mpa   REAL NOT NULL,
    eiv_constant          REAL NOT NULL,
    alpha_e6_per_c        REAL NOT NULL,
    creep_strain_permille REAL NOT NULL,
    creep_temp_c          REAL NOT NULL,
    weight_n_per_m        REAL NOT NULL,
    breaking_load_kn      REAL,
    copied_utc            TEXT NOT NULL
);
-- Fliken Dokumentation: en rad per uppgift med värde eller referens (nycklarna i planen 3.5).
-- value är kanonisk text: tal med punkt, val som text, linans namn för linraderna.
CREATE TABLE doc_value (
    key       TEXT PRIMARY KEY NOT NULL,
    value     TEXT NOT NULL DEFAULT '',
    reference TEXT NOT NULL DEFAULT ''
);
-- Normalspannsberäkningarna. conductor är linans namn i project_conductor, NULL = ingen lina.
-- Gränserna är kontrollerna 1-5 (Excel R16, R19, R22, R26, R30).
CREATE TABLE normal_span (
    id              INTEGER PRIMARY KEY AUTOINCREMENT,
    conductor       TEXT COLLATE NOCASE,
    note            TEXT NOT NULL DEFAULT '',
    limit_longest   REAL NOT NULL DEFAULT 1.5,
    limit_shortest  REAL NOT NULL DEFAULT 0.5,
    limit_adjacent  REAL NOT NULL DEFAULT 2.5,
    limit_local_min REAL NOT NULL DEFAULT 0.95,
    limit_local_max REAL NOT NULL DEFAULT 1.15,
    created_utc     TEXT NOT NULL,
    modified_utc    TEXT NOT NULL
);
-- Stolparna i tabellens ordning. length_m är spannet före stolpen; NULL = tom cell.
CREATE TABLE normal_span_pole (
    span_id  INTEGER NOT NULL REFERENCES normal_span(id) ON DELETE CASCADE,
    position INTEGER NOT NULL,
    pole     TEXT NOT NULL DEFAULT '',
    length_m REAL,
    height_m REAL,
    PRIMARY KEY (span_id, position)
);
-- Vilken beräkning en rad i Dokumentation visar: högst en per rad, en beräkning på flera rader.
CREATE TABLE doc_normal_span (
    doc_key TEXT PRIMARY KEY NOT NULL,
    span_id INTEGER NOT NULL REFERENCES normal_span(id) ON DELETE CASCADE
);
CREATE INDEX doc_normal_span_span ON doc_normal_span(span_id);
```

- **`kExpectedTables`** (3.12) är `'project', 'project_conductor', 'doc_value', 'normal_span', 'normal_span_pole', 'doc_normal_span'`, och `kExpectedTableCount` = 6.
- **`foreign_keys = ON`** sätts redan av `db::Database::open`, så `ON DELETE CASCADE` gäller.
- **Linornas namn** har ingen främmande nyckel, eftersom kollationen gör det osäkert. API:t garanterar att ett namn i `doc_value` eller `normal_span.conductor` alltid finns i `project_conductor` (5.6). Testet kontrollerar det.
- **Oanvända linor tas bort** med en sats i slutet av varje skrivning som kan släppa en lina. `IN` jämför med vänstra kolumnens kollation, alltså NOCASE:
  ```sql
  DELETE FROM project_conductor
   WHERE name NOT IN (SELECT value FROM doc_value
                       WHERE key IN ('phase_conductor', 'top_conductor_left', 'top_conductor_right'))
     AND name NOT IN (SELECT conductor FROM normal_span WHERE conductor IS NOT NULL);
  ```
- **`AUTOINCREMENT`:** ett id återanvänds aldrig, och ett gammalt id i en öppen dialog kan inte träffa en ny beräkning.
- **Tider:** `created_utc`, `modified_utc` och `copied_utc` är `platform::nowUtcIso()`. Det är bara spårbarhet.

### 4.2 Systemdatabasen
Ingen schemaändring. Nyckeln `autosave_minutes` läggs i den befintliga tabellen `setting` (3.7). Linregistrets tabeller är oförändrade. Bara C++-typen flyttar (3.10).

---

## 5. API (bindande)

### 5.1 `libs/calc/normal_span.h` (ny, beror bara på standardbiblioteket)
```cpp
#pragma once
#include <optional>
#include <string>
#include <vector>

namespace kalkyl::calc {

// En stolpe i normalspannsberäkningen: en rad i formulärets tabell (Excel C-E).
// lengthM är det horisontella spannet FÖRE stolpen; på första stolpen ingår det inte i summorna.
struct SpanPole {
    std::string pole;                 // Stpnr, får vara tom
    std::optional<double> lengthM;    // spannlängd, m
    std::optional<double> heightM;    // H, plushöjd för mark eller infästning, m
};

// Gränserna för kontrollerna 1-5 (Excel R16, R19, R22, R26, R30), med Excels förval.
struct SpanLimits {
    double longest = 1.5;     // 1: a <= longest * an
    double shortest = 0.5;    // 2: a >= shortest * an
    double adjacent = 2.5;    // 3: större / mindre av två intilliggande spann <= adjacent
    double localMin = 0.95;   // 4: an,10 >= localMin * an
    double localMax = 1.15;   // 5: an,10 <= localMax * an
};

// Kontrollerna för ett spann (Excel I-M). poleIndex är stolpen spannet slutar vid,
// som index i indatavektorn (också när tomma rader hoppats över).
struct SpanCheck {
    int poleIndex = -1;
    double lengthM = 0;
    double ratio = 0;                  // 1-2: a / an, med an avrundat till 0,1 m
    bool tooLong = false;              // 1
    bool tooShort = false;             // 2
    std::optional<double> adjacent;    // 3: >= 1; tomt för första spannet
    bool adjacentExceeded = false;     // 3: paret (föregående, detta) är över gränsen
    std::optional<double> local10;     // 4-5: an,10 för de 10 föregående spannen
    bool localTooLow = false;          // 4
    bool localTooHigh = false;         // 5
    std::optional<double> inclinedM;   // l = sqrt(a^2 + dH^2) när båda stolparna har H
};

// Resultatet. an och anDeltaH är avrundade till 0,1 m som i Excel; anExact är oavrundat.
struct SpanResult {
    double anExact = 0;
    double an = 0;                        // Excel R10
    std::optional<double> anDeltaH;       // Excel T10; tomt = "-"
    double meanM = 0;                     // Excel R13: summan / antal stolpar, som makrot
    double totalM = 0;                    // Excel T13
    int spanCount = 0;
    int poleCount = 0;                    // rader som inte är helt tomma
    std::string fromPole;                 // första stolpens Stpnr
    std::string toPole;                   // sista stolpens Stpnr
    std::vector<SpanCheck> spans;         // ett per spann, i ordning
    int tooLongCount = 0;                 // antal spann med kontroll 1
    int tooShortCount = 0;                // 2
    int adjacentCount = 0;                // 3: antal par över gränsen
    int localLowCount = 0;                // 4
    int localHighCount = 0;               // 5
};

enum class IssueKind { Error, Warning };
enum class SpanColumn { None, Pole, Length, Height };

// Ett fel eller en varning. poleIndex -1 och column None gäller hela beräkningen.
struct SpanIssue {
    IssueKind kind = IssueKind::Error;
    int poleIndex = -1;
    SpanColumn column = SpanColumn::None;
    std::string message;                  // svensk text till användaren
};

// Svaret: resultatet när inget fel finns, och alla fel och varningar i radordning.
struct SpanCalculation {
    std::optional<SpanResult> result;
    std::vector<SpanIssue> issues;
};

// Normalspannet och kontrollerna enligt planen avsnitt 2. Helt tomma stolpar hoppas över.
// Fel (inget resultat): färre än två stolpar, spannlängd saknas, spannlängd <= 0 eller inte
// ändlig, H inte ändlig, en gräns <= 0 eller inte ändlig. Varning: H saknas på några stolpar.
SpanCalculation calculateNormalSpan(const std::vector<SpanPole>& poles, const SpanLimits& limits);

// Avrundning till 0,1 som Excels Round(x, 1), fast halva bort från noll (planen 2.5).
double roundTenth(double value);

// Texterna i kontrollkolumnerna, som i Excel: "0,99", "1 : 1,9", "356 / 1,04".
// Kontroll 1 och 2 har två decimaler (planen 2.4).
std::string ratioText(double ratio);
std::string adjacentText(double adjacent);
std::string localText(double local10, double an);

} // namespace kalkyl::calc
```

**Texterna i `issues`** (ordagrant, de visas i formuläret):

| Fall | kind | poleIndex, column | message |
|---|---|---|---|
| Färre än två stolpar | Error | −1, None | "Minst två stolpar (ett spann) behövs." |
| Spannlängd saknas (inte första stolpen) | Error | i, Length | "Spannlängd saknas." |
| Spannlängd ≤ 0 | Error | i, Length | "Spannlängden måste vara större än 0." |
| Ett tal som inte är ändligt | Error | i, Length eller Height | "Ogiltigt tal." |
| En gräns ≤ 0 eller inte ändlig | Error | −1, None | "Gränserna måste vara tal större än 0." |
| H finns på några men inte alla stolpar | Warning | −1, None | "H saknas på N av M stolpar. aₙ,ΔH räknas bara när alla stolpar har H." |

**Implementation:**
- `ratioText(r)` = `formatDecimal(r, 2)`.
- `adjacentText(q)` = `"1 : " + formatDecimal(roundTenth(q))`.
- `localText(x, an)` = `formatDecimal(std::round(x)) + " / " + formatDecimal(std::round(x / an * 100) / 100)`.

### 5.2 `libs/calc/table_text.h` (ny)
```cpp
#pragma once
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace kalkyl::calc {

// Ett tal ur text som användaren skrivit eller klistrat in. Decimalkomma eller decimalpunkt,
// mellanslag (också hårt mellanslag U+00A0 och smalt U+202F) som tusentalsavgränsare, valfritt
// plus- eller minustecken (också U+2212) och exponent. Tomt, otolkbart, både komma och punkt,
// eller ett tal som inte är ändligt ger tomt svar.
std::optional<double> parseDecimal(std::string_view text);

// Talet i fast form utan exponent. decimals -1 = kortaste form som läses tillbaka exakt,
// annars så många decimaler (avrundat). separator ',' för visning och urklipp, '.' för
// DataGrids kanoniska celltext. -0 skrivs som 0.
std::string formatDecimal(double value, int decimals = -1, char separator = ',');

// Tabbseparerad text till rader och fält. Rader skiljs av LF eller CRLF, fält av tabb.
// En avslutande tom rad tas bort. Ett fält inom citattecken (Excel) får dem borttagna, och
// två citattecken i rad blir ett; där får fältet innehålla tabb och radbrytning.
std::vector<std::vector<std::string>> parseTsv(std::string_view text);

// Rader och fält till tabbseparerad text: tabb mellan fält, LF efter varje rad (som Excel).
// Ett fält med tabb, radbrytning eller citattecken skrivs inom citattecken.
std::string formatTsv(const std::vector<std::vector<std::string>>& rows);

// Var ett inklistrat block hamnar: från ankarcellen åt höger och nedåt. Fält som hamnar i
// kolumn >= columnCount ignoreras. rowsNeeded = anchorRow + antalet rader i blocket.
struct PasteCell { int row = 0; int column = 0; std::string text; };
struct PastePlan { std::vector<PasteCell> cells; int rowsNeeded = 0; };
PastePlan planPaste(int anchorRow, int anchorColumn, int columnCount,
                    const std::vector<std::vector<std::string>>& block);

} // namespace kalkyl::calc
```
- `parseDecimal` använder `std::from_chars` (`<charconv>`, double) på en rensad kopia och kräver att hela texten tolkas.
- `formatDecimal(v, -1)` använder `std::to_chars(…, std::chars_format::fixed)` utan precision, som Lindata.

### 5.3 `libs/conductor/conductor.h` (flyttad ur `libs/systemdb`)
Innehållet är dagens `libs/systemdb/conductor.h` ordagrant, med `namespace kalkyl::conductor` i stället för `kalkyl::systemdb`:
- `ConductorOrigin` och `Conductor`
- `ConductorField` och `ConductorIssue`
- `kCreepTempTolerance`
- `validateConductor`, `derivedCreepTempC`, `creepTempDeviates`, `sameConductorName` och `sameConductorValues`

Två funktioner läggs till sist:
```cpp
// Samma typ och samma tal (D-N). Namn, id och ursprung ignoreras. Används för att se om
// projektets kopia skiljer sig från linregistret (planen 3.9).
bool sameConductorData(const Conductor& a, const Conductor& b);
// Naturlig jämförelse som tabellernas sortering i GlazeUI: en följd siffror jämförs som ett tal
// ("910" efter "185"), allt annat tecken för tecken med ASCII-versaler vikta. Svarar <0, 0 eller >0.
int compareNames(std::string_view a, std::string_view b);
```
- `compareNames` är `compareNatural` ur `main_window.cpp`, flyttad hit.
- **CMake:** `add_library(kalkyl_conductor STATIC conductor.cpp)` med `libs/` som PUBLIC include-katalog och `kalkyl_warnings` PRIVATE. Testet heter `conductor`.
- `kalkyl_systemdb` länkar `kalkyl_conductor` PUBLIC.

### 5.4 `libs/systemdb/system_db.h` (ändringar)
- `#include "systemdb/conductor.h"` blir `#include "conductor/conductor.h"`. `Conductor` skrivs `conductor::Conductor` i deklarationerna.
- **Nytt** före klassen:
```cpp
// Automatisk sparning i minuter: 0 = av. Bara dessa värden är tillåtna (planen 3.7).
inline constexpr int kAutosaveChoices[] = {0, 1, 5, 10, 15, 30};
inline constexpr int kDefaultAutosaveMinutes = 5;
```
- **Nytt** i `SystemDb`, efter `setLastFolder`:
```cpp
    // Automatisk sparning i minuter, ett av kAutosaveChoices. Förvalet 5 vid fel, saknat
    // eller okänt värde. Nyckeln autosave_minutes i tabellen setting.
    int autosaveMinutes();
    // Fel: Invalid (inte ett av kAutosaveChoices), Sql.
    core::Result<void> setAutosaveMinutes(int minutes);
```

### 5.5 `libs/projectdb/project_db.h` (ändringar)
- **`Snapshot`** får `std::uint64_t revision = 0;   // arbetskopians revision när bilden togs`.
- **`published`** ersätts av `void published(const Snapshot& s, const platform::FileStamp& newBase);`. Den sätter basen och den sparade revisionen till `s.revision`. `create` och testerna anpassas.
- **Nytt i `ProjectDb`**, efter `local()`:
```cpp
    // Ändringsräknaren. Varje skrivning i arbetskopian (project_data.h) räknar upp revision();
    // isModified() är sann när revisionen skiljer sig från den senast publicerade bilden.
    std::uint64_t revision() const;
    bool isModified() const;
    // Anropas av dataskrivningarna efter en lyckad transaktion.
    void markModified();
```
- **Medlemmar:** `std::uint64_t revision_ = 0;` och `std::uint64_t savedRevision_ = 0;`. De flyttas i flyttkonstruktorn och flyttilldelningen.
- **`snapshot()`** sätter `s.revision = revision_`.
- **`open`** kör schemakontrollen (3.12) direkt efter `migrate`, före `quickCheck`. Felet går genom `fail(...)` utan `relabel`.

### 5.6 `libs/projectdb/project_data.h` (ny) och `project_data.cpp`
```cpp
#pragma once
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "calc/normal_span.h"
#include "conductor/conductor.h"
#include "core/result.h"
#include "projectdb/project_db.h"

namespace kalkyl::projectdb {

// Dokumentationens nycklar för linraderna (planen 3.5). top_conductor_right kan vara "Samma".
inline constexpr std::string_view kPhaseConductorKey    = "phase_conductor";
inline constexpr std::string_view kTopLeftConductorKey  = "top_conductor_left";
inline constexpr std::string_view kTopRightConductorKey = "top_conductor_right";
inline constexpr std::string_view kSameConductor        = "Samma";

// En uppgift i fliken Dokumentation (planen 3.5). value är kanonisk text: tal med punkt,
// val som text, linans namn för linraderna. key är radens stabila nyckel.
struct DocValue { std::string key; std::string value; std::string reference; };

// Lina och skede för normalspannsraderna i Dokumentation: faslina, topplina vänster och
// höger, före (lila) och efter (rosa) ombyggnation.
enum class Wire { Phase, TopLeft, TopRight };
enum class Stage { Before, After };

// Nyckeln för normalspannsraden, t.ex. "ns_calc_top_left_after".
std::string normalSpanDocKey(Wire wire, Stage stage);

// En normalspannsberäkning: indata, gränser och lina. Resultatet räknas med
// calc::calculateNormalSpan och lagras aldrig.
struct NormalSpan {
    std::int64_t id = 0;                            // 0 = ny, inte sparad
    std::optional<conductor::Conductor> conductor;  // linan; vid läsning projektets kopia
    std::string note;                               // anteckning
    calc::SpanLimits limits;
    std::vector<calc::SpanPole> poles;              // i tabellens ordning
    std::vector<std::string> docKeys;               // raderna som visar den; bara vid läsning
    std::string createdUtc;                         // sätts vid sparning
    std::string modifiedUtc;
};

// Alla uppgifter i Dokumentation, i nyckelordning.
core::Result<std::vector<DocValue>> docValues(ProjectDb& p);
// Lägger in eller ersätter. Tomt värde och tom referens tar bort raden. På en linrad får
// värdet bara vara tomt, "Samma" (höger) eller namnet på en lina som finns i projektet.
// Fel: AccessDenied (skrivskyddat), Invalid (tom nyckel, okänt linnamn), Sql.
core::Result<void> setDocValue(ProjectDb& p, const DocValue& v);

// Projektets linor, efter namn.
core::Result<std::vector<conductor::Conductor>> projectConductors(ProjectDb& p);
// Sätter linan på en linrad. Finns en lina med samma namn (NOCASE) i projektet återanvänds
// den oförändrad; annars kopieras chosen in. Radens referens står kvar. Linor som inget
// använder tas bort. Fel: AccessDenied, Invalid (inte en linnyckel, ogiltig lina), Sql.
core::Result<void> setDocConductor(ProjectDb& p, std::string_view key,
                                   const conductor::Conductor& chosen);
// Uppdatera från linregistret: kopian med samma namn (NOCASE) får fresh:s typ och värden.
// Namn och id behålls. Fel: AccessDenied, NotFound (ingen sådan lina), Invalid, Sql.
core::Result<void> refreshProjectConductor(ProjectDb& p, const conductor::Conductor& fresh);

// Alla beräkningar med stolpar, lina och rader, efter id.
core::Result<std::vector<NormalSpan>> normalSpans(ProjectDb& p);
// En beräkning. Tomt svar om id saknas.
core::Result<std::optional<NormalSpan>> normalSpan(ProjectDb& p, std::int64_t id);
// Ny (id 0) eller ersätter beräkningen och alla dess stolpar, i EN transaktion, och svarar
// med id. Helt tomma stolpar sparas inte; positionerna blir 0, 1, 2 ... Linan återanvänds
// eller kopieras som i setDocConductor. Är linkDocKey satt kopplas beräkningen till den raden
// (som linkNormalSpan) i samma transaktion. Linor som inget använder tas bort.
// Fel: AccessDenied, NotFound (id saknas), Invalid (tal inte ändligt, gräns <= 0, ogiltig
// lina, okänd radnyckel), Sql.
core::Result<std::int64_t> saveNormalSpan(ProjectDb& p, const NormalSpan& ns,
                                          std::string_view linkDocKey = {});
// Raden docKey visar beräkningen id. En annan beräkning på raden kopplas bort, och radens
// manuella värde i doc_value töms (referensen står kvar). Fel: AccessDenied, NotFound,
// Invalid (inte en normalspannsnyckel), Sql.
core::Result<void> linkNormalSpan(ProjectDb& p, std::int64_t id, std::string_view docKey);
// Raden docKey visar ingen beräkning längre. Beräkningen finns kvar. Fel: AccessDenied, Sql.
core::Result<void> unlinkDocRow(ProjectDb& p, std::string_view docKey);
// Tar bort beräkningen, dess stolpar och alla dess kopplingar. Linor som inget använder
// tas bort. Saknat id är inget fel. Fel: AccessDenied, Sql.
core::Result<void> removeNormalSpan(ProjectDb& p, std::int64_t id);

} // namespace kalkyl::projectdb
```
- **Skrivningarna** kontrollerar först `p.access() == Access::ReadOnly`. Det ger `AccessDenied` med texten "Projektet är öppnat skrivskyddat och kan inte ändras.".
- **Efter en lyckad transaktion** anropas `p.markModified()`.
- **Giltiga nycklar:**
  - Normalspannsnycklarna är de sex i 3.1.
  - Linnycklarna är de tre konstanterna.
  - `setDocValue` med en normalspannsnyckel påverkar bara det manuella värdet, inte kopplingen.
- **Linorna valideras** med `conductor::validateConductor` innan de kopieras eller uppdateras. Ogiltig ger `Invalid` med texten "Linan <namn> har ogiltiga värden: <första meddelandet>".
- **Linans id** vid läsning är `project_conductor.id`. `origin` är `Own`, och båda ignoreras vid sparning.
- **`NotFound`:**
  - För en beräkning: "Normalspannsberäkningen finns inte längre i projektet."
  - För en lina: "Linan <namn> finns inte i projektet."
- **Rensningen** av oanvända linor (4.1) körs sist i `setDocValue` (när nyckeln är en linnyckel), `setDocConductor`, `saveNormalSpan` och `removeNormalSpan`.
- **SQL** skrivs som råa strängliteraler `R"sql( … )sql"`.

### 5.7 `src/windows/normal_span_dialog.h` (publikt gränssnitt)
Klassen `NormalSpanDialog : public glaze::AppWindow` ligger utanför namespace. Regionen ägs av GlazeBuilder (6.1). Efter `#pragma endregion`:
```cpp
public:
    // Projektet som formuläret läser och sparar i. Anropas före run(owner); ägs av main.
    void setProject(kalkyl::projectdb::ProjectDb& project);
    // Linregistret för rullistan Lina. Anropas före run(owner); ägs av main.
    void setSystemDb(kalkyl::systemdb::SystemDb& sys);
    // En ny beräkning. linkDocKey = raden i Dokumentation som ska visa den när användaren
    // trycker OK (Lägg till...), tom = lös. conductorName = förvald lina, tom = ingen.
    void setNew(std::string linkDocKey, std::string conductorName);
    // En befintlig beräkning, som läses ur projektet i onLoad.
    void setExisting(std::int64_t id);
    // En ny, lös beräkning med en kopia av beräkningen id (Kopiera...).
    void setCopyOf(std::int64_t id);
    // Radtexterna i Dokumentation per nyckel, för upplysningen om var beräkningen visas.
    void setDocRowLabels(std::vector<std::pair<std::string, std::string>> labels);
    // Skrivskyddat: allt visas, inget kan ändras, OK döljs och Avbryt heter Stäng.
    void setReadOnly(bool readOnly);
    // Efter run(): id för den sparade beräkningen, tomt om inget sparades.
    std::optional<std::int64_t> savedId() const;
```
Dialogen sparar själv med `saveNormalSpan` när användaren trycker OK, som `ConductorDialog` gör med systemdatabasen. Huvudfönstret läser om allt efter `run`.

---

## 6. Formuläret `NormalSpanDialog` (paket C)

Filerna är `src/windows/normal_span_dialog.{h,glazeui,cpp}` och läggs till i `glaze_add_window` i `src/CMakeLists.txt`. Dialogen är modal och synkron på UI-tråden. Projektets arbetskopia och systemdatabasen är lokala och behöver ingen `Worker`.

### 6.1 Layout (`.glazeui`, i grammatikens form)
- **Fönstret:**
  - `setTitle("Normalspannsberäkning")`, `setContentSize(1200, 720)`, `setMinimumContentSize(920, 520)`, `setStartPosition(glaze::StartPosition::CenterOwner)` och `setModal(true)`.
  - `setAcceptButton(okBtn)`, `setCancelButton(cancelBtn)`, `statusBar.show()` och `statusBar.addPanel("", 0, true)`.
  - Designraden `// glazeui-design: ui=Slate theme=slate(Windows) style=0 lang=sv`.
- **Översta raden** (y = 12, x = 16, syskonankare åt höger, 8 px mellan kontrollerna och 24 px mellan grupperna):
  - `conductorLbl` "Lina" och `conductorDd`, bredd 320. Posterna sätts i kod.
  - `noteLbl` "Anteckning" och `noteTxt`, ankrad höger 16.
- **`docLbl`** (`Role::Dim`, under raden, ankrad vänster och höger) visar var beräkningen visas (6.2).
- **`infoLbl`** (`Role::Dim`, `setWrap(true)`, under `docLbl`): "Klistra in Stpnr, spannlängd och H från Excel med Ctrl+V i den cell där inklistringen ska börja. Spannlängd är det horisontella spannet före stolpen. Spannet före första stolpen ingår inte i summorna. H är plushöjd för mark eller infästning och är valfri."
- **Knappraden** längst ned, adopterad före tabellen eftersom syskonankare bara pekar bakåt:
  - `clearBtn` "Töm…" till vänster, med `glaze::stockIcon(glaze::StockIcon::Clear)`.
  - `cancelBtn` "Avbryt" (`setDialogResult(Cancel)`) och `okBtn` "OK" till höger. okBtn har inget dialogresultat (OK validerar, se 6.2).
- **`resultPanel`** till höger, en Panel med bredd 380, ankrad uppe, nere och höger, under `infoLbl` och över knapparna. Den innehåller:
  - **GroupBox `resultGrp` "Resultat":** etikettpar `anLbl` "Normalspann, aₙ" + `anValue` (`Role::Heading`), `anDhLbl` "Normalspann, aₙ,ΔH" + `anDhValue`, `meanLbl` "Medelspann" + `meanValue`, `totalLbl` "Totallängd" + `totalValue`, `countLbl` "Antal spann" + `countValue` och `sectionLbl` "Sträcka" + `sectionValue`.
  - **GroupBox `limitGrp` "Kontroller":** fem rader `lim1Lbl` … `lim5Lbl`, `lim1Spin` … `lim5Spin` och `lim1Count` … `lim5Count`:
    - "1  Längsta tillåtna spannlängd", SpinBox 1,50, "× aₙ"
    - "2  Kortaste tillåtna spannlängd", 0,50, "× aₙ"
    - "3  Största förhållande mellan intilliggande spann, 1 :", 2,50
    - "4  aₙ,10 får inte understiga", 0,95, "× aₙ"
    - "5  aₙ,10 får inte överstiga", 1,15, "× aₙ"

    Varje SpinBox har `setDecimals(2)`, `setMin(0.01)`, `setMax(100)` och `setStep(0.05)`. Antalet avvikelser står efter varje rad ("0 avvikelser" eller "5 avvikelser").
  - **`methodLbl`** (`Role::Dim`, `setWrap(true)`): "Punkt 3 markerar raden och raden före. Punkt 4 och 5 kontrolleras från det elfte spannet: värdet är normalspannet för de 10 föregående spannen, aₙ,10, och kvoten mot aₙ. Normalspannsmetoden bygger på fri rörlighet av infästningspunkterna mellan avspänningsstolparna. Vid för stora avvikelser kan nedhänget anta en annan linbåge än normalspannets."
  - **Tooltips:** `lim4Lbl` får texten om punkt 4 ur P44 och `lim5Lbl` texten om punkt 5 ur P49.
- **`poleGrid`** (DataGrid) till vänster:
  - Ankrad uppe, nere och vänster, med `anchorRight = {"resultPanel", 16}`, under `infoLbl` och över knapparna.
  - `setHeaderLines(2)`, `setSortable(false)` och `setEditOnClick(false)`.
  - **Kolumnerna** (tvåradiga rubriker skrivs med radbrytningen i strängen, som i `conductor_dialog.glazeui`):

| Kol | Rubrik (rad 1 / rad 2) | Redigerare | Bredd, justering |
|---|---|---|---|
| 0 | Stpnr / (tom) | `Text` | 110, Autosize |
| 1 | Spannlängd / m | `Number`, `allowEmpty = true` | 100, höger |
| 2 | H / m | `Number`, `allowEmpty = true` | 80, höger |
| 3 | 1 / a/aₙ | `None` | AutoMin, höger |
| 4 | 2 / a/aₙ | `None` | AutoMin, höger |
| 5 | 3 / grannspann | `None` | AutoMin, höger |
| 6 | 4 / aₙ,10 / kvot | `None` | AutoMin, höger |
| 7 | 5 / aₙ,10 / kvot | `None` | AutoMin, höger |

  - **Kolumntooltips:**
    - 1 "Horisontellt spann före stolpen. Spannet före första stolpen ingår inte i summorna."
    - 2 "Plushöjd för mark eller infästning (valfri)"
    - 3–7: kontrollernas texter ur 2.4.
  - **Inga rader i `.glazeui`.** Tabellen fylls i `onLoad`.
- **Kopplingarna** i fullform:
  - `okBtn.onClick`, `clearBtn.onClick`
  - `poleGrid.onCommit`, `poleGrid.onKeyDown`, `poleGrid.onMouseUp`, `poleGrid.onContextMenu`
  - `conductorDd.onChange`, `noteTxt.onChange`
  - `lim1Spin.onCommit` … `lim5Spin.onCommit`
  - `glaze::AppWindow::onLoad`, `glaze::AppWindow::onClose`
- **Ingen `appearance::apply`.** Dialogen är inget toppfönster.
- **Kontroll:** glaze-render efter varje handredigering ska ge exit 0 (avsnitt 11).

### 6.2 Beteende (`.cpp`)
- **Medlemmar efter regionen** (förutom 5.7):
  - `project_`, `sys_`, `existingId_`, `copyOfId_`, `linkDocKey_`, `defaultConductor_`, `readOnly_`, `docLabels_`
  - `conductorChoices_` (namn och lina per post i `conductorDd`)
  - `dirty_`, `saved_`, `savedId_`
  - `calc_` (senaste `calc::SpanCalculation`)
  - markeringen `anchor_` och `extent_` (6.3)
  - `glaze::Menu gridMenu_`
- **`onLoad`:**
  1. **Rullistan Lina:**
     - Posterna är "(ingen)" först, sedan registrets linor (`sys_->conductors()`) och projektets linor som saknas i registret (`projectConductors`).
     - De står efter namn fallande med `conductor::compareNames`, som i Dokumentation.
     - En post som bara finns i projektet får samma namn i listan, och dess lina är projektets kopia.
  2. **Befintlig beräkning** (`setExisting`) eller kopia (`setCopyOf`): `normalSpan(*project_, id)`.
     - Saknas den visas "Normalspannsberäkningen finns inte längre i projektet.", och dialogen stängs med Cancel.
     - Annars fylls linan, anteckningen och gränserna, och tabellen fylls med `setRows`: en rad per stolpe och sist en tom rad.
     - En kopia får anteckningen "Kopia av <från> – <till>" (eller "Kopia av <anteckning>" när den har en anteckning) och id 0.
  3. **Ny beräkning:** linan `defaultConductor_` (eller "(ingen)"), förvalsgränserna och en tom rad.
  4. **`docLbl`:**
     - För en befintlig beräkning: "Visas i Dokumentation på: <radtexter>" (ur `docKeys` och `docLabels_`). Utan rader: "Visas inte i Dokumentation. Koppla den med högerklick på en normalspannsrad i fliken Dokumentation."
     - För en ny med `linkDocKey_`: "Visas på raden <radtext> när du trycker OK."
     - För en ny utan rad och för en kopia: "Blir en lös beräkning i fliken Normalspann."
  5. **`setReadOnly`:**
     - Alla redigerare blir `readOnly = true`, och `conductorDd`, `noteTxt` och SpinBoxarna inaktiveras (`disable()`).
     - `okBtn.hide()` och `clearBtn.hide()`, och `cancelBtn` får texten "Stäng".
  6. **`recalc()`.**
- **`recalc()`:**
  1. Läs tabellen till `std::vector<calc::SpanPole>`, utan den sista tomma raden:
     - Stpnr trimmas.
     - Talen läses med `poleGrid.numberValue(r, c)`.
     - En cell med `poleGrid.hasError(r, c)` ger ett fel "Ogiltigt tal." på raden innan beräkningen körs, och cellen räknas som tom.
  2. Läs gränserna ur SpinBoxarna (`value()`).
  3. `calc_ = calc::calculateNormalSpan(poles, limits)`.
  4. Kontrollkolumnerna 3–7 skrivs med `setCell` för varje rad:
     - Första stolpen och tomma rader får "".
     - Spannrader får `ratioText` (3 och 4), `adjacentText` eller "-" (5) och `localText` eller "-" (6 och 7).
  5. Cellfärgerna sätts om (6.3, "färgerna").
  6. Tooltips:
     - Felceller får felets text.
     - Spannlängden på första stolpen får "Spannet före första stolpen ingår inte i summorna."
     - En spannlängd med `inclinedM` får "Lutande längd <l> m".
  7. Resultatpanelen:
     - aₙ visas som "343,1 m" (`formatDecimal(an, 1)`).
     - aₙ,ΔH visas som värde eller "–".
     - Medel och totallängd har en decimal, antal spann som heltal och sträckan som "ST0252 – ST0532".
     - Utan resultat visar alla "–".
     - Varje kontrollrads antal blir "N avvikelser", med röd text (`setColor`) när N > 0.
  8. Statusraden:
     - Det första felet: "Rad 12: Spannlängd saknas." eller "Minst två stolpar (ett spann) behövs.".
     - Annars den första varningen.
     - Annars "<n> spann · <från> – <till>".
- **`gridCommitted(e)`:** `dirty_ = true`. Är `e.row` den sista raden läggs en tom rad till med `addRow`. Sedan `recalc()`.
- **Linan, anteckningen och SpinBoxarna (`onChange`/`onCommit`):** `dirty_ = true`. Gränserna kör `recalc()`.
- **OK (`okBtnClicked`):**
  1. `commitEdits()`.
  2. Finns ett fel i `calc_` (eller ett "Ogiltigt tal."):
     - Visa `messageBox(Warning)`: "Beräkningen kan inte sparas: rad <n>: <text>", eller utan rad bara texten, plus "(och N fel till)".
     - Sätt `setCurrentCell` på felets cell, `ensureVisible`, och stanna.
  3. Bygg `projectdb::NormalSpan`:
     - id: 0 för ny och kopia.
     - lina: postens lina, eller tom för "(ingen)".
     - anteckning, gränser och stolpar utan helt tomma rader.
  4. `saveNormalSpan(*project_, ns, linkDocKey_)`. Det manuella värdet på raden ersätts utan fråga (3.4).
  5. Vid fel visas `messageBox(Error, message)` och dialogen stannar. Annars `saved_ = true`, `savedId_ = id` och `closeWithResult(OK)`.
- **Töm… (`clearBtnClicked`):** frågan "Töm hela tabellen?" (Ja/Nej). Vid Ja: `setRows` med en tom rad, `dirty_ = true` och `recalc()`.
- **Krysset, Avbryt och Escape (`onClosing`):**
  1. Ingenting görs om `saved_`, `readOnly_` eller `e.reason != UserClosing`.
  2. Annars `commitEdits()`. Är `dirty_` falsk görs inget mer.
  3. Frågan "Du har ändrat normalspannsberäkningen. Vill du spara ändringarna?" visas med `YesNoCancel` och `Warning`:
     - **Ja:** `okBtnClicked`-logiken. Misslyckas den blir `e.cancel = true`.
     - **Nej:** stäng.
     - **Avbryt:** `e.cancel = true`.
  - Fungerar `messageBox` dåligt inom `onClose` tas Avbryt-knappens `setDialogResult` bort, och frågan ställs i dess `onClick`, som i Lindata-planen 6.3.

### 6.3 Tabellen som i Excel
**Modellen:**
- `anchor_` och `extent_` är två celler (`struct CellPos { int row = 0; int column = 0; };`).
- Markeringen är rektangeln mellan dem.
- **Den aktuella cellen i DataGrid är alltid `extent_`.**
- Utan utökad markering är `anchor_ == extent_`, alltså bara den aktuella cellen.
- **Indatakolumnerna** är 0–2. **Alla kolumner** är 0–7.
- **"Den sista raden"** är den tomma raden sist. Den finns alltid och räknas inte som data.

**Tangenterna (`poleGridKeyDown(e)`).** `ctrl` är Ctrl utan Alt (AltGr är Ctrl+Alt) och `shift` är Shift.
- **Medan en cell redigeras** (`poleGrid.isEditing()`) får redigeraren tangenterna, utom i ett fall:
  - Ctrl+V när urklippet innehåller tabb eller radbrytning. Då blir `e.handled = true`, `poleGrid.cancelEdit()` körs och blocket klistras in från den redigerade cellen.
  - Ctrl+V med ett enda värde klistras in i redigeraren som vanligt.
- **Utan redigering** gäller tabellen nedan. Allt som inte står där lämnas åt DataGrid, och sedan postas `syncSelection(false)` med `glaze::app().post`.

| Tangent | Åtgärd | handled |
|---|---|---|
| Ctrl+C | `copySelection()` | ja |
| Ctrl+X | `copySelection()` och sedan `clearSelectionCells()` (bara när det inte är skrivskyddat) | ja |
| Ctrl+V | `pasteAt(övre vänstra cellen i markeringen)` (inte skrivskyddat) | ja |
| Ctrl+A | `selectAll()` | ja |
| Delete | `clearSelectionCells()` (inte skrivskyddat) | ja |
| Shift+Vänster/Höger/Upp/Ned | `extent_` ± 1 kolumn eller rad | ja |
| Shift+Home / Shift+End | `extent_.column` = 0 / 7 | ja |
| Ctrl+Shift+Home / Ctrl+Shift+End | `extent_` = (0, 0) / (sista dataraden, 7) | ja |
| Shift+PageUp / Shift+PageDown | `extent_.row` ∓ 20 | ja |

När `extent_` ändras: klampa till tabellen, `poleGrid.setCurrentCell(extent_)`, `poleGrid.ensureVisible(extent_.row)` och `paintSelection()`.

**Musen (`poleGridMouseUp(e)`, vänster knapp):**
- `e.mods` sparas, och `syncSelection(shift)` postas. När släppet kommer har trycket redan gjort cellen aktuell.
- `syncSelection(shift)` läser `currentRow()` och `currentColumn()`.
  - Med Shift blir `extent_` den cellen, så att Shift+klick utökar markeringen.
  - Annars blir både `anchor_` och `extent_` den cellen.
- Sedan körs `paintSelection()`.
- Visar handtestet att den aktuella cellen inte hunnit flyttas läggs uppdateringen i `onMouseUp` i stället för i `post`, eller tvärtom. Samordnaren väljer efter handtestet.

**Snabbmenyn (`poleGridContextMenu(e)`):**
1. Raden: `if (auto p = poleGrid.fromWindow({e.x, e.y})) row = poleGrid.rowAt(p->x, p->y);`. Kolumnen går inte att få fram (3.11).
2. Ligger raden utanför markeringen flyttas markeringen dit: `(row, currentColumn())`.
3. `gridMenu_.items.clear()` och sedan posterna, med genvägen visad i `MenuItem::shortcut`:
   - "Klipp ut" (Ctrl+X), "Kopiera" (Ctrl+C), "Klistra in" (Ctrl+V), "Radera innehåll" (Delete), separator, "Markera alla" (Ctrl+A), separator, "Infoga rad ovanför", "Ta bort rader"
   - I skrivskyddat läge är bara Kopiera och Markera alla aktiva (`enabled`).
4. `gridMenu_.popupAt(poleGrid, e.x, e.y)`.

**`copySelection()`:**
- Rektangeln blir rader av fält:
  - Stpnr som det står.
  - Spannlängd och H som `formatDecimal(värde, -1, ',')`, där tom är "".
  - Kontrollkolumnerna med sin visade text.
- `gfx::setClipboardText(calc::formatTsv(rader))`.
- Statusraden: "Kopierade <rader> × <kolumner> celler".
- Den sista tomma raden tas aldrig med.

**`pasteAt(cell)`:**
1. `text = gfx::clipboardText()`. Är den tom visar statusraden "Urklippet är tomt.", och inget ändras.
2. `block = calc::parseTsv(text)`.
3. **Rubrikrad:** har blocket mer än en rad, och har första raden ett fält som inte är tomt och inte ett tal (`parseDecimal`) där det landar i kolumn 1 eller 2, hoppas första raden över.
4. **Kontroll före ändring:** varje fält som landar i kolumn 1 eller 2 och inte är tomt måste vara ett tal. Annars visas `messageBox(Warning)` "Inklistringen avbröts: "<text>" på rad <n> i urklippet är inte ett tal (kolumnen <Spannlängd|H>). Inget ändrades.", och inget ändras.
5. `plan = calc::planPaste(cell.row, cell.column, 3, block)`. Fält i kolumn 3–7 ignoreras, eftersom de inte är indata (kolumner utanför tabellen ignoreras).
6. Lägg till rader med `addRow` tills `rowCount() == plan.rowsNeeded + 1`, så att det finns en tom rad sist.
7. Skriv cellerna med `setCell`:
   - Stpnr trimmat.
   - Talen i kanonisk form `formatDecimal(v, -1, '.')`.
   - Ett tomt fält tömmer cellen, som i Excel.
8. Markeringen blir det inklistrade: `anchor_` = cellen och `extent_` = (sista raden i blocket, min(cell.column + blockets bredd − 1, 2)).
9. `dirty_ = true`, `recalc()` och `paintSelection()`. Statusraden: "Klistrade in <n> rader".
- **Det inklistrade tolkas inte som en fast ordning** Stpnr, spannlängd, H. Varje fält hamnar i den kolumn där det landar.

**`clearSelectionCells()`:**
- `setCell(r, c, "")` för indatakolumnerna i rektangeln.
- Sedan `trimTrailingRows()`: tomma rader sist tas bort med `removeRow`, tills exakt en tom rad finns sist.
- `dirty_ = true` och `recalc()`.
- Efter Ctrl+A och Delete finns alltså en enda tom rad. Resultaten visar "–", och användaren kan börja om från början.

**`selectAll()`:**
- `anchor_` = (sista dataraden, 7) och `extent_` = (0, 0), så att den aktuella cellen hamnar överst.
- Utan datarader blir markeringen (0, 0)–(0, 7).

**Infoga rad ovanför och Ta bort rader** (snabbmenyn):
- Tabellen byggs om med `setRows` från en kopia av cellerna, med en tom rad infogad vid `extent_.row` respektive raderna i rektangeln borttagna.
- Minst en tom rad finns kvar sist.
- Därefter `recalc()` och markeringen på (rad, 0).

**Färgerna (`paintSelection()` och `paintCell(r, c)`):**
- **Grundfärgen** för en cell:
  - Fel (`Error`-issue eller `hasError`): `gfx::Color{220, 60, 60, 80}` med tooltip.
  - Spannlängden på första stolpen: `{128, 128, 128, 48}`.
  - Kontroll 1 och 2 röd: `{200, 0, 0, 90}`, och annars ingen.
  - Kontroll 3–5 röd: `{200, 0, 0, 90}`. Grön: `{0, 150, 0, 64}`. "-" och tomt får ingen färg.
- **Markeringen** ritas med `{60, 130, 210, 90}` över grundfärgen och ersätter den, utom på den aktuella cellen, som DataGrid ritar med markeringens färger.
- `paintSelection()` målar bara om cellerna i den gamla och den nya rektangeln.
- `recalc()` målar om alla.

**Tomma rader:**
- Helt tomma rader mitt i tabellen står kvar, som i Excel, och hoppas över av beräkningen (2.6).
- De sparas inte, så när formuläret öppnas igen är de borta.

---

## 7. Huvudfönstret (paket D)

### 7.1 `main_window.glazeui`
- **Arkiv:** `menuBar.addItem("Arkiv", "Spara", "Ctrl+S");` och en `addSeparator("Arkiv")` före "Stäng projekt".
- **Inställningar:** efter "Lindata…" kommer `menuBar.addSeparator("Inställningar");` och `menuBar.addSubmenu("Inställningar", "Automatisk sparning");`. Därefter sex rader `menuBar.addRadio({"Inställningar", "Automatisk sparning"}, "<text>");` med texterna "Av", "1 minut", "5 minuter", "10 minuter", "15 minuter" och "30 minuter".
- **Timern:** `glaze::Timer autosaveTimer;` i regionen (aldrig adopterad), `autosaveTimer.name = "autosaveTimer";`, `autosaveTimer.setInterval(300000);` och kopplingen `autosaveTimer.onTick = [this](glaze::Timer* s, glaze::TimerArgs& e) { autosaveTimerTick(s, e); };`. Den startas i `onLoad`, aldrig i `.glazeui`.
- **Fliken `nsSida`** "Normalspann" adopteras sist i `flikar`, efter `stolpSida`. Den har panelen `nsPanel` (`Dock::Fill`) med:
  - `nsRubrik` "Normalspannsberäkningar" (`Role::Heading`, x 16, y 8)
  - `nsNyBtn` "Ny beräkning…" (`StockIcon::Add`)
  - `nsAndraBtn` "Ändra…" (`StockIcon::Edit`)
  - `nsKopieraBtn` "Kopiera…" (`StockIcon::Duplicate`)
  - `nsTaBortBtn` "Ta bort" (`StockIcon::Remove`)

  Knapparna står i en rad under rubriken med 8 px mellanrum. Sist kommer `nsLista` (`glaze::ListView`), ankrad i alla fyra kanter under knapparna, med 16 px marginal och `setHeaderLines(2)`.
- **Kolumnerna i `nsLista`.** Förslaget med tolv kolumner gällde. Eftersom en beräkning kan visas på flera rader och inte har någon lina blir det nio kolumner:
  - "Lina" (fas/topp) och "Skede" ersätts av kolumnen Dokumentation, som visar raderna.
  - Linans namn och lintyp tas bort (handtestet 2026-11-24): normalspannet beror inte på linan.

| Kol | Rubrik | Innehåll |
|---|---|---|
| 0 | Från | första stolpens Stpnr ("–" om tom) |
| 1 | Till | sista stolpens Stpnr |
| 2 | Spann | antal spann |
| 3 | Totallängd / m | en decimal |
| 4 | aₙ / m | en decimal, sorteringsnyckel med `setSortKey` |
| 5 | aₙ,ΔH / m | en decimal eller "–" |
| 6 | Avvikelser | summan av kontrollernas antal. Tooltip "1: 0 · 2: 5 · 3: 0 · 4: 64 · 5: 1". "Fel" vid fel. |
| 7 | Dokumentation | de kopplade raderna i kort form (tabellen nedan), skilda med komma. "–" utan rader. Tooltip: radernas hela texter, en per rad. |
| 8 | Anteckning | anteckningen, `ColumnSizing::Autosize` |

- **Korta radnamn:**

| Nyckel | Kort form, topplinor "Samma" | Kort form, egna topplinor |
|---|---|---|
| `ns_calc_phase_before` / `_after` | Fas före / Fas efter | Fas före / Fas efter |
| `ns_calc_top_left_before` / `_after` | Topp före / Topp efter | Topp v. före / Topp v. efter |
| `ns_calc_top_right_before` / `_after` | Topp h. före (dold) / Topp h. efter (dold) | Topp h. före / Topp h. efter |

- **Kopplingar** i fullform:
  - `nsNyBtn.onClick`, `nsAndraBtn.onClick`, `nsKopieraBtn.onClick`, `nsTaBortBtn.onClick`
  - `nsLista.onActivate`, `nsLista.onChange`, `nsLista.onContextMenu`
  - `byggPmGrid.onContextMenu`
  - `glaze::AppWindow::onClose`

### 7.2 `main_window.h`, efter regionen
- **Nya handlers:**
  - `saveClicked()`, `autosaveTimerTick(glaze::Timer*, glaze::TimerArgs&)`, `autosaveChosen(int minutes)`, `onClosing(glaze::Widget*, glaze::CloseArgs&)`
  - `byggPmGridContextMenu(glaze::Widget*, glaze::ContextMenuArgs&)`
  - `nsNyBtnClicked`, `nsAndraBtnClicked`, `nsKopieraBtnClicked`, `nsTaBortBtnClicked` (`ClickArgs`)
  - `nsListaActivated(glaze::Widget*, glaze::ActivateArgs&)`, `nsListaChanged(glaze::Widget*, glaze::ChangeArgs&)`, `nsListaContextMenu(glaze::Widget*, glaze::ContextMenuArgs&)`
- **Hjälpfunktioner:**
  - `loadProjectData()` (dokumentationen och linorna), `saveDocValue(std::size_t def)`, `chooseConductor(std::size_t conductorRow, const std::string& name)`
  - `refreshNormalSpans()`, `showNormalSpanRows()`, `fillNormalSpanList()`
  - `openNormalSpanDialog(…)`, `removeNormalSpanAsked(std::int64_t id)`
  - `docRowLabels()` (nyckel och radtext för de synliga och dolda normalspannsraderna), `shortDocLabel(key)`
  - `startSave(SaveKind kind)` med `enum class SaveKind { Manual, Auto, Closing };`, `restartAutosave()`, `updateTitle()`, `readOnly() const`
- **Medlemmar:**
  - `std::vector<kalkyl::conductor::Conductor> projectConductors_` (projektets kopior). `selectedConductor_` tas bort.
  - `std::vector<kalkyl::projectdb::NormalSpan> normalSpans_` och `std::vector<kalkyl::calc::SpanCalculation> nsCalcs_` (samma ordning)
  - `std::vector<std::int64_t> nsListIds_` (id per rad i listan)
  - `glaze::Menu byggPmMenu_` och `glaze::Menu nsMenu_` (medlemmar, aldrig lokala)
  - `bool saving_`, `bool closeAfterSave_`, `bool closingConfirmed_` och `bool autosaveBlocked_`
  - `int savePanel_` (index för sparpanelen i statusraden)
  - **sist** `glaze::threads::Worker saveWorker_`, så att den rivs först (som i `ProjectPicker`)
- **`#include <glaze_threads/threads.h>`**, `"calc/normal_span.h"`, `"conductor/conductor.h"` och `"projectdb/project_data.h"`.
- **Varje deklaration** får en kommentar på 2–5 rader.

### 7.3 Dokumentation och linorna i projektet (`main_window.cpp`)
- **Namnrymden:** `kalkyl::systemdb::Conductor` blir `kalkyl::conductor::Conductor`. `compareNatural` tas bort, och `kalkyl::conductor::compareNames` används.
- **`ByggPmRowDef`** får fältet `key` (nyckeln ur 3.5, `nullptr` för normalspannsraderna) och `std::optional<std::pair<Wire, Stage>> ns` för de sex normalspannsraderna.
  - Deras nyckel är `normalSpanDocKey(wire, stage)`.
  - Fältet `value` blir förvalet: `""`, utom "Singel" (Konfiguration) och "Samma" (Topplina höger).
- **`loadProjectData()`** körs i `onLoad` före `rebuildByggPm()`:
  - `docValues(*project_)` läses in i `byggPmValues_`, och `projectConductors(*project_)` i `projectConductors_`. Saknade nycklar får förvalet.
  - Vid fel visas `messageBox(Error)` med "Projektets uppgifter kunde inte läsas:" och `message`, och fönstret fortsätter med förvalen.
- **`rebuildByggPm()`** läser inte längre tillbaka ur tabellen. `byggPmValues_` är sanningen och uppdateras i `byggPmGridCommitted` och av `loadProjectData`.
  - Sist anropas `fillConductorChoices()` och `showNormalSpanRows()`, eftersom `setRows` nollar cellernas redigerare, bilder och tooltips.
- **`fillConductorChoices()`** (3.9):
  1. Läser registret (`sys_->conductors()`).
  2. Rullistan för varje linrad är registrets namn plus projektets linor som saknas i registret (`sameConductorName`), sorterade med `compareNames` fallande. Topplina höger har "Samma" först.
  3. Tooltip och bild per linrad efter tillståndet:

| Tillstånd | Tooltip | Bild |
|---|---|---|
| Ingen lina | – | – |
| "Samma" | "Samma lina som Topplina vänster" | – |
| Kopian finns i registret och är lika (`sameConductorData`) | – | – |
| Kopian finns i registret men skiljer sig | "Projektets kopia skiljer sig från ditt linregister. Högerklicka för att uppdatera." | `StockIcon::Warning` |
| Kopian finns inte i registret | "Linan finns inte i ditt linregister. Projektets kopia används." | – |
| Registret är tomt och ingen lina är vald | "Linregistret är tomt" | – |

- **`byggPmGridCommitted(e)`:**
  - **På en linrad med ett namn:** `chooseConductor(row, namn)`:
    - Finns namnet bland projektets kopior används den kopian, annars registrets lina.
    - `setDocConductor(*project_, key, lina)` körs, och sedan läses `projectConductors_` om.
  - **"Samma" på Topplina höger:** `setDocValue(key, "Samma", referens)`.
  - **På alla andra rader:** `saveDocValue(def)` med nyckeln, värdet och referensen.
  - Vid fel visas `messageBox(Error, message)`, och cellen får tillbaka värdet ur `byggPmValues_`.
  - Därefter `updateTitle()` och `fillConductorChoices()`.
  - Byte mellan "Samma" och en egen lina bygger om raderna som i dag, i pump-svansen.
- **`lindataClicked()`:** efter dialogen körs `fillConductorChoices()`. Projektets kopior ändras inte.
- **Skrivskyddat:** alla Värde- och Referensceller får sina redigerare med `readOnly = true` (talfält och rullistor).

### 7.4 Högerklick i Dokumentation
- **`byggPmGridContextMenu(e)`:**
  1. Raden via `byggPmGrid.fromWindow({e.x, e.y})` och `byggPmGrid.rowAt(p->x, p->y)`. Är raden varken en linrad eller en normalspannsrad öppnas ingen meny. I skrivskyddat läge gäller tabellen nedan.
  2. `commitEdits()` och `byggPmGrid.setCurrentCell(row, kValueColumn)`, så att raden syns som vald.
  3. `byggPmMenu_.items.clear()` och sedan posterna:

| Rad och läge | Poster |
|---|---|
| Linrad med en lina, inte skrivskyddat | "Uppdatera från linregistret" (aktiv bara när registret har linan och kopian skiljer sig) |
| Normalspannsrad utan kopplad beräkning | "Lägg till…" · "Koppla befintlig" (undermeny, se nedan; inaktiv utan beräkningar) · separator · "Manuellt värde" |
| Normalspannsrad med kopplad beräkning | "Ändra…" · "Koppla bort" · separator · "Ta bort…" |
| Skrivskyddat, normalspannsrad med beräkning | "Visa…" |
| Skrivskyddat, övriga | ingen meny |

  4. `byggPmMenu_.popupAt(byggPmGrid, e.x, e.y)`.
- **Undermenyn "Koppla befintlig"** har en post per beräkning i projektet, också de som redan visas på andra rader. Texten är "<från> – <till> · <aₙ> m · <korta radnamn eller "ej kopplad">", till exempel "ST0252 – ST0532 · 343,1 m · Fas efter".
- **Åtgärderna:**
  - **Lägg till…:** `openNormalSpanDialog(new, linkDocKey = radens nyckel, conductorName = linan för radens lina)`. Linan är Faslina, Topplina vänster eller Topplina höger ur `byggPmValues_`, där "Samma" ger vänsterns namn.
  - **Ändra… och Visa…:** `openNormalSpanDialog(existing id)`.
  - **Koppla befintlig → post:** `linkNormalSpan(id, radens nyckel)`. Det manuella värdet ersätts utan fråga.
  - **Manuellt värde:** `byggPmGrid.setCurrentCell(row, kValueColumn)` och `byggPmGrid.beginEdit()`.
  - **Koppla bort:** `unlinkDocRow(radens nyckel)`.
  - **Ta bort…:** `removeNormalSpanAsked(id)`.
  - **Uppdatera från linregistret:** `refreshProjectConductor(*project_, registrets lina)`, och statusraden säger "Linan <namn> uppdaterad från linregistret."
  - Efter varje åtgärd körs `loadProjectData()` (när linorna ändrats), `refreshNormalSpans()`, `fillConductorChoices()` och `updateTitle()`. Fel visas med `messageBox(Error, message)`.
- **`removeNormalSpanAsked(id)`** frågar Ja/Nej (`Warning`). Texten är "Ta bort normalspannsberäkningen <från> – <till> (<n> spann, aₙ <aₙ> m) ur projektet?", följd av en tom rad och:
  - utan rader: "Den visas inte i Dokumentation."
  - en rad: "Den visas på raden <radtext>, som visar - efteråt."
  - flera rader: "Den visas på <n> rader i Dokumentation, som visar - efteråt:" följt av radtexterna, en per rad.

  Vid Ja körs `removeNormalSpan(id)`.
- **`refreshNormalSpans()`:**
  1. `normalSpans(*project_)` läses in i `normalSpans_`, och `calculateNormalSpan` körs för var och en in i `nsCalcs_`.
  2. Sedan `showNormalSpanRows()` och `fillNormalSpanList()`.
  3. Anropas i `onLoad`, efter dialogen, efter varje menyåtgärd och efter att linläget bytts.
- **`showNormalSpanRows()`**, för varje synlig rad med `ns`:
  - **Med en beräkning vars `docKeys` innehåller radens nyckel**, och ett resultat:
    - `setCell(r, kValueColumn, formatDecimal(an, -1, '.'))`.
    - `setCellEditor` med `{.editor = Number, .suffix = " m", .allowEmpty = true, .emptyText = "-", .readOnly = true}`.
    - `setCellImage(r, kValueColumn, glaze::stockIcon(glaze::StockIcon::Sum))` och tooltipen i 3.4.
  - **Kopplad men med fel:** cellen är tom (`-`) och tooltipen är "Beräkningen har fel: <första felet>".
  - **Utan kopplad beräkning:**
    - Cellen får det manuella värdet ur `byggPmValues_`.
    - Redigeraren är talfältet med `emptyText = "-"` (låst när det är skrivskyddat).
    - `setCellImage(r, kValueColumn, {})`.
    - Tooltipen är "Högerklicka för att lägga till eller koppla en normalspannsberäkning".
- **`openNormalSpanDialog`:**
  1. `commitEdits()`.
  2. `NormalSpanDialog dlg;` och därefter `setProject`, `setSystemDb`, `setNew`, `setExisting` eller `setCopyOf`, `setDocRowLabels(docRowLabels())`, `setReadOnly(readOnly())` och `dlg.run(*this)`.
  3. Sedan `loadProjectData()`, `refreshNormalSpans()`, `fillConductorChoices()` och `updateTitle()`, eftersom formuläret kan ha kopierat in en lina.
  4. Är beräkningen ny och sparad väljs dess rad i listan.

### 7.5 Fliken Normalspann
- **`fillNormalSpanList()`:**
  - `nsLista.setRows(...)` byggs ur `normalSpans_` och `nsCalcs_` med kolumnerna i 7.1, och `nsListIds_` byggs.
  - Därefter: sorteringsnycklar för talkolumnerna, gul växlande radton (`kAmber` med alfa 64/52 per rad, som `shadeStolpRows`), tooltips för Avvikelser och Dokumentation, och det tidigare valda id:t väljs igen.
- **Knapparna:**
  - **Ny beräkning…:** `openNormalSpanDialog(new, linkDocKey = "", conductorName = "")`, alltså en lös beräkning.
  - **Ändra…** och dubbelklick eller Enter (`onActivate`): den valda raden.
  - **Kopiera…:** `openNormalSpanDialog(copyOf = vald id)`.
  - **Ta bort:** `removeNormalSpanAsked(id)`.
  - `nsListaChanged` aktiverar Ändra…, Kopiera… och Ta bort bara när en rad är vald.
  - Skrivskyddat: Ny beräkning, Kopiera och Ta bort är inaktiva, och Ändra… heter "Visa…".
- **Snabbmenyn (`nsListaContextMenu`)** använder `nsLista.contextRow()`, eftersom högertrycket har valt raden. Posterna är "Ändra…" (eller "Visa…"), "Kopiera…", separator och "Ta bort…". Det finns ingen kopplingspost här (användarens beslut).

### 7.6 Spara, automatisk sparning, stängning, titel och statusrad
- **`onLoad`:**
  - `menuBar.setHandler({"Arkiv", "Spara"}, [this] { saveClicked(); });`
  - en `setHandler` per post under {"Inställningar", "Automatisk sparning"}, som anropar `autosaveChosen(minuter)`
  - `menuBar.setItemChecked` på posten för `sys_->autosaveMinutes()`
  - Sparpanelen läggs till efter sökvägen, och dess index sparas i `savePanel_`.
  - Skrivskyddat: `menuBar.setItemEnabled({"Arkiv", "Spara"}, false)`.
  - Därefter `loadProjectData()`, `rebuildByggPm()`, `refreshNormalSpans()`, `shadeStolpRows()`, `updateTitle()` och `restartAutosave()`.
- **`restartAutosave()`:** `autosaveTimer.stop()`. Är inställningen inte Av, projektet inte skrivskyddat och `autosaveBlocked_` falsk körs `autosaveTimer.setInterval(minuter * 60000)` och `autosaveTimer.start()`.
- **`autosaveChosen(m)`:** `sys_->setAutosaveMinutes(m)`. Fel visas i statusraden, och valet gäller ändå under sessionen. Sedan `restartAutosave()`.
- **`autosaveTimerTick`:** hoppa över ticket om det är skrivskyddat, om `saving_`, `autosaveBlocked_` eller `!project_->isModified()`. Annars `startSave(SaveKind::Auto)`.
- **`updateTitle()`:** "<namn>" + (`isModified()` ? " *" : "") + " – Kalkyl " + version + (skrivskyddat ? " (skrivskyddat)" : ""). Sparpanelen visar "Ej sparat" när projektet är ändrat och ingen sparning pågår, och annars den senaste texten.
- **`saveClicked()`:**
  - Gör ingenting om det är skrivskyddat.
  - Om `saving_` visar statusraden "Sparning pågår…".
  - Om inget är ändrat visar statusraden "Inget att spara."
  - Annars `commitEdits()` och `startSave(SaveKind::Manual)`.
- **`startSave(kind)`:**
  1. `auto snap = project_->snapshot();`. Vid fel visas en `messageBox(Error)` vid Manual och Closing, och bara statusraden vid Auto.
  2. `saving_ = true`, sparpanelen "Sparar…" och Arkiv > Spara inaktiv.
  3. `saveWorker_.run([snap](std::stop_token stop) { return ProjectDb::publish(snap, stop); }, …)`.
  4. Fortsättningen på UI-tråden:
     - **I alla fall:** `saving_ = false` och Arkiv > Spara aktiv igen.
     - **Lyckat:** `project_->published(snap, stamp)` och `updateTitle()`. Sparpanelen visar "Sparat hh:mm" vid Manual och Closing och "Sparat automatiskt hh:mm" vid Auto.
       - Är `closeAfterSave_` satt (Closing, eller ett kryss under sparningen) och projektet inte längre ändrat: `closingConfirmed_ = true` och `close()`.
       - Är det fortfarande ändrat körs `close()` utan `closingConfirmed_`, så att frågan ställs igen.
     - **Fel:**
       - `platform::removeFile(snap.localFile)`.
       - Vid Manual och Closing visas `messageBox(Error, "Projektet kunde inte sparas." + radbrytning + message)`, `closeAfterSave_ = false` och `outcome_ = Outcome::Exit`.
       - Vid Auto visas bara statusraden "Automatisk sparning misslyckades hh:mm: <message>".
       - Är felet `Conflict` blir dessutom `autosaveBlocked_ = true`, timern stoppas och statusraden visar texten i 3.7.
       - Ett kryss under en automatisk sparning som misslyckas (`closeAfterSave_`): frågan i `onClosing` ställs igen genom `close()`.
     - **Avbrutet:** ingenting.
- **`onClosing(e)`** (krysset, Arkiv > Stäng projekt och Arkiv > Avsluta går alla hit):
  1. Är `closingConfirmed_` satt, skrivskyddat eller inget ändrat: `autosaveTimer.stop()` och stäng.
  2. Om `saving_`: `e.cancel = true` och `closeAfterSave_ = true`. Statusraden: "Stänger när sparningen är klar…".
  3. Annars frågan "Projektet har ändringar som inte är sparade. Vill du spara dem innan projektet stängs?" (`YesNoCancel`, `Warning`):
     - **Ja:** `e.cancel = true`, `closeAfterSave_ = true` och `startSave(SaveKind::Closing)`. Fönstret stängs när sparningen är klar.
     - **Nej:** `autosaveTimer.stop()` och stäng. Ändringarna försvinner med arbetskopian.
     - **Avbryt:** `e.cancel = true` och `outcome_ = Outcome::Exit`, så att ett senare kryss avslutar.
  - `e.reason == AppQuit` behandlas som `UserClosing`.

---

## 8. Fel och validering

| Var | Fel | Visas |
|---|---|---|
| Formulärets tabell | ogiltigt tal i en cell (DataGrid: "Ogiltigt värde") | röd cell och statusraden "Rad n: Ogiltigt tal." |
| Formuläret, beräkningen | issues ur `calculateNormalSpan` (5.1) | röda celler med tooltip, statusraden och listan vid OK |
| Formuläret, varning | H saknas på några stolpar | statusraden. Sparning tillåten. |
| Formuläret, inklistring | text i en talkolumn (inte rubrikraden) | `messageBox(Warning)`, inget ändras (6.3) |
| Formuläret, OK | fel i beräkningen | `messageBox(Warning)` och den första felcellen blir aktuell |
| Projektdatabasen | `AccessDenied`, `NotFound`, `Invalid`, `Sql` | `messageBox(Error, message)`. `detail` loggas med `gfx::log`. |
| Linval | ogiltig lina i registret (borde inte hända, registret validerar) | `messageBox(Error, message)`, och cellen får tillbaka sitt värde |
| Ctrl+S och stängning | publiceringen misslyckas (`Unreachable`, `Conflict`, `Busy`, `Io`) | `messageBox(Error)`. Projektet förblir ändrat och kan sparas igen. |
| Automatisk sparning | samma fel | bara statusraden. `Conflict` stänger av den automatiska sparningen för sessionen (3.7). |
| Inställningen | `setAutosaveMinutes` misslyckas (systemdatabasen i minnet eller fel) | statusraden. Valet gäller ändå under sessionen. |
| Öppna | projekt med gammalt schema | väljaren visar texten i 3.12 |

- **Validering i två lager:**
  - `calculateNormalSpan` och `saveNormalSpan` delar reglerna för tal (ändliga), och sparningen kontrollerar gränserna (> 0).
  - En sparning med fel i stolparna stoppas redan i formuläret, eftersom OK kräver ett resultat.
- **`ErrorCode::Invalid`** finns redan (Lindata). Inga nya felkoder.

---

## 9. Tester

Alla tester har samma form som de befintliga: en `report`-funktion, en funktion per del och felkoden är antalet misslyckade kontroller. Tal jämförs med `near(a, b, tol)`.

### 9.1 `libs/calc/calc_test.cpp` (ny, ctest-namn `calc`, paket A)
1. **`testExampleA()`:** de 280 spannen ur bilaga A som `constexpr double kSpansA[]`, och Stpnr "ST0252" … "ST0532" framräknade. Stolpe 0 (ST0252) har spannlängden 274, som inte ingår i summorna. Kontrollera allt i tabellen för exempel A i 2.7:
   - aₙ, anExact (1e-6), medel 318,4300 (1e-3), total (1e-6) och `poleCount` 281
   - antalen 0, 5, 0, 64 och 1
   - de exakta Stpnr för kontroll 2 och 5, och för kontroll 4 den första, den sista och antalet
   - `localText` vid ST0263 = "356 / 1,04" och vid ST0532 = "332 / 0,97"
   - `ratioText` vid ST0253 = "0,99"
   - gräns 2 = 0,45 ger 3
2. **`testExampleB()`:** exempel B, alla rader i tabellen, också medelspannet 116,667, att P7 blir rött i kontroll 3 och att P0:s 999 inte ingår i summorna.
3. **`testExampleC()`:** exempel C med och utan H på P2 (varningens text ordagrant), medel 300,0. `inclinedM` på P1 = 300,166620.
4. **`testEdgeCases()`:** kantfallen i 2.7, med texterna i 5.1 ordagrant och `poleIndex` för den tomma raden.
5. **`testTableText()`:**
   - `parseDecimal`:
     - "332,247" och "332.247" ger 332.247.
     - "1 234,5" med vanligt, hårt (U+00A0) och smalt (U+202F) mellanslag ger 1234.5.
     - "−3" (U+2212) ger −3, och "1e3" ger 1000.
     - "1.234,5", "abc", "" och "inf" ger tomt.
   - `formatDecimal`: (332.247, −1, ',') = "332,247". (343.1, 1) = "343,1". (0.98514, 2) = "0,99". (1, −1) = "1". (−0.0, −1) = "0". (89160.401, −1, '.') = "89160.401".
   - `parseTsv`:
     - Ett Excel-urklipp med CRLF och en avslutande radbrytning ger 3 × 3 fält.
     - Ett fält inom citattecken med tabb och dubbla citattecken tolkas rätt.
     - En enda kolumn och en enda rad fungerar.
     - Tomma fält mitt i en rad behålls.
   - `formatTsv`: tillbaka till samma text, med LF och en avslutande LF.
   - `planPaste`:
     - Ankare (5, 1) med ett 2 × 3-block ger 4 celler (kolumn 1–2) och `rowsNeeded` 7.
     - Ankare (0, 0) med 1 × 5 ger 3 celler.
     - Ankare (0, 2) med en kolumn ger celler bara i kolumn 2.

### 9.2 `libs/conductor/conductor_test.cpp` (ny, ctest-namn `conductor`, paket A)
1. **`testConductorRules()`:** flyttad ordagrant ur `systemdb_test.cpp`, med den nya namnrymden.
2. **`testSameConductorData()`:** samma värden med olika namn och id ger sant. En annan diameter, en annan typ eller brottlast tom mot 165 ger falskt.
3. **`testCompareNames()`:** "910 mm² FeAl FALCON" är större än "185 mm² Fe60" (talet jämförs som tal). "abc" och "ABC" är lika. "a2" är mindre än "a10".

### 9.3 `libs/systemdb/systemdb_test.cpp` (paket A)
1. **De befintliga testerna** använder `conductor::Conductor`. `testConductorRules` är flyttad (9.2).
2. **`testAutosaveSetting(dir)`:**
   - En ny fil ger 5.
   - `setAutosaveMinutes(10)` ger 10, också efter stängning och ny öppning.
   - `setAutosaveMinutes(7)` ger `Invalid`, och värdet är fortfarande 10.
   - Ett skräpvärde i `setting` ("abc") ger 5.
   - `openInMemory` ger 5.

### 9.4 `libs/projectdb/projectdb_test.cpp` (paket B)
CURLEW-linan i testerna är ett `conductor::Conductor`-värde ur startdatan: namn "593 mm² FeAl CURLEW", typ "FeAl CURLEW", 593, 31.7, 52000, 72000, 120, 90, 19, 0.4, 21, 19.6 och 165.

1. **De befintliga testerna** anpassas till `published(snap, stamp)`.
2. **`testSchemaTables`:** ett nytt projekt har de sex tabellerna i `kExpectedTables`.
3. **`testOldSchemaRejected`:**
   - Skapa med `db::Database` en fil med application_id KLKP, `user_version` 1 och bara tabellen `project` med en rad.
   - `open` ger `NotADatabase` med texten i 3.12. Varken lås eller arbetskopia finns kvar.
4. **`testDocValues`:**
   - Sätt, ersätt och ta bort (tomt). Referensen får "åäö".
   - `isModified()` blir sann. Efter `snapshot` och `publish` och `published(snap, stamp)` är den falsk.
   - Öppna originalet igen: värdena finns.
5. **`testConductorCopies`:**
   - `setDocConductor(phase, CURLEW)` ger en kopia med alla värden.
   - `setDocConductor(top_conductor_left, CURLEW med diametern 32 och namnet i gemener)` återanvänder kopian: fortfarande en lina, diametern 31.7, och båda raderna har namnet.
   - `setDocValue(top_conductor_right, "Samma")` lyckas. `setDocValue(phase_conductor, "Okänd lina")` ger `Invalid`.
   - Byt Faslina till en annan lina: CURLEW finns kvar, eftersom Topplina vänster använder den. Byt även Topplina vänster: CURLEW tas bort.
   - `refreshProjectConductor` med en ändrad diameter: värdet ändras och id står kvar. Med ett namn som saknas blir det `NotFound`.
   - En lina med diametern 0 ger `Invalid`.
   - Efter varje steg gäller att varje namn i `doc_value` (linnycklarna) och i `normal_span.conductor` finns i `project_conductor`.
6. **`testNormalSpanRoundTrip`:**
   - Exempel C med Stpnr "P0" … "P3", linan CURLEW, anteckningen "Anteckning åäö" och egna gränser sparas, och en helt tom stolpe i mitten sparas inte.
   - Läs tillbaka: samma värden och ordning, `NULL` som tomt optional, linans data, `docKeys` tom, id > 0 och `modifiedUtc` satt.
   - Spara igen med en stolpe mindre: stolparna ersätts.
   - Spara utan lina: CURLEW tas bort om inget annat använder den.
7. **`testNormalSpanLinking`:**
   - `setDocValue(ns_calc_phase_before = "343.4", ref "STO252-532")`.
   - `saveNormalSpan(X, linkDocKey = ns_calc_phase_before)`: värdet töms och referensen står kvar.
   - `linkNormalSpan(X, ns_calc_phase_after)`: X har två `docKeys`.
   - Spara Y och `linkNormalSpan(Y, ns_calc_phase_before)`: raden visar Y, och X har bara `ns_calc_phase_after` kvar.
   - `unlinkDocRow(ns_calc_phase_after)`: X har inga rader.
   - En okänd nyckel ("temp_max" eller "x") ger `Invalid`.
   - Primärnyckeln stoppar två beräkningar på samma rad, kontrollerat med en direkt `INSERT` som ska ge fel.
8. **`testNormalSpanRemove`:** en beräkning med två rader och en lina tas bort. Stolparna och kopplingarna försvinner (räkna i tabellerna), linan tas bort om den inte används, och ett id som saknas är inget fel.
9. **`testReadOnlyWrites`:** öppna skrivskyddat. Alla skrivningar i 5.6 ger `AccessDenied`, och `isModified()` förblir falsk.
10. **`testInvalid`:** en gräns som är 0 eller NaN och en spannlängd som är NaN ger `Invalid`. Ett id som saknas ger `NotFound` i `saveNormalSpan` och `linkNormalSpan`.
11. **`testRevision`:** skriv (rev 1) och ta en bild (rev 1). Skriv igen (rev 2). `published(snap, …)`: `isModified()` är fortfarande sann.
12. **`testProvProject`:** provprojektet `Prov åäö.kalkyl` får Faslina CURLEW och exempel B med linan CURLEW, kopplad till både `ns_calc_phase_before` och `ns_calc_phase_after`, så att röktestet och handtestet har data.

### 9.5 GUI
- Inga automatiska GUI-tester.
- glaze-render på `normal_span_dialog.h`, `main_window.h` och `conductor_dialog.h` ska ge exit 0, och bilderna granskas (12).
- Handtestet i avsnitt 13 täcker beteendet.

---

## 10. CLAUDE.md-uppdateringar

| Fil | Vem | Ändring |
|---|---|---|
| `libs/calc/CLAUDE.md` (ny) | A | Biblioteket, filerna, reglerna: rena funktioner, Excels formler (hänvisning till planen avsnitt 2), avvikelserna (2.6), medelspannet som makrot och talformen (komma och punkt) |
| `libs/conductor/CLAUDE.md` (ny) | A | Biblioteket, filerna, valideringen och de två nya funktionerna. Hänvisning till Lindata-planen 2.9 och den här planen 3.9–3.10. |
| `libs/systemdb/CLAUDE.md` | A | `conductor.*` är flyttade. Nyckeln `autosave_minutes` och de tillåtna värdena. |
| `libs/CLAUDE.md` | A | Raderna `calc/` och `conductor/` och beroendena: `conductor` och `calc` beror bara på standardbiblioteket, `systemdb` beror på `db` och `conductor`, och `projectdb` beror på `db`, `calc` och `conductor`. |
| `libs/projectdb/CLAUDE.md` | B | `project_data.*`, tabellerna, linkopiorna och rensningen, kopplingstabellen, revisionen och `isModified`, schemakontrollen och skrivskyddet. Hänvisningen till Grunder-planen rättas till `doc/plans/archive/`. |
| `src/windows/CLAUDE.md` | D, och A:s och C:s rader via samordnaren | Tre rader för `normal_span_dialog.*`. `conductor_dialog.*` använder `kalkyl::conductor`. `main_window.*`: Arkiv > Spara, automatisk sparning med Inställningar-menyn och timern, fliken Normalspann, menyerna på normalspanns- och linraderna, linkopiorna, och att Dokumentation sparas i projektet. Meningen "Inget sparas i projektet" och `selectedConductor_` tas bort. |
| `src/CLAUDE.md` | C | `CMakeLists.txt` länkar också `kalkyl_calc` och `kalkyl_conductor` |
| Rotens `CLAUDE.md` | samordnaren | **Delar:** raderna Normalspann (`src/windows/normal_span_dialog.*`), Beräkningar (`libs/calc`) och Linor (`libs/conductor`), och huvudfönstrets rad (fliken Normalspann, Spara, automatisk sparning). **Data:** systemdatabasen sparar också den automatiska sparningen, och projektet sparar Dokumentation, linkopiorna och normalspannsberäkningarna. |
| `doc/plans/CLAUDE.md`, `TODO.md` | samordnaren | Planens rad och punkten. Idén om områdesmarkering i GlazeUI (3.11). Idéerna "Spara fas- och topplinan i projektet som en kopia" och "Flytta `Conductor` ur systemdb" är klara med den här planen. Idén "Spara flikarna Dokumentation och Stolptabell före" ändras till bara Stolptabell före (egen plan). |

---

## 11. Arbetsfördelning, bygg- och arbetsregler

### Byggkataloger
- **En katalog per agent:** `build-a`, `build-b`, `build-c` och `build-d`. Agenten tar bort sin katalog när den är klar (`rm -rf build-<x>`).
- **`build/` är samordnarens och användarens.** Den tas aldrig bort, rensas inte och konfigureras inte om av en agent.
- **Använd MSYS2:s cmake** (`C:/msys64/ucrt64/bin/cmake.exe`), aldrig `C:/Program Files/CMake`.
- **Konfiguration** (A och B):
  ```sh
  cmake -S . -B build-a -G Ninja -DCMAKE_BUILD_TYPE=Debug -DCMAKE_C_COMPILER=C:/msys64/ucrt64/bin/gcc.exe -DCMAKE_CXX_COMPILER=C:/msys64/ucrt64/bin/g++.exe -DKALKYL_TESTS=ON -DFETCHCONTENT_SOURCE_DIR_GLAZEUI=C:/Users/sehoer/Source/Kalkyl/build/_deps/glazeui-src
  ```
- **A, C och D** lägger till `-DGLAZE_TOOLS=ON` och bygger `--target glaze-render`. Förhandsgranskning:
  ```sh
  build-c/_deps/glazeui-build/apps/glaze-render/glaze-render.exe src/windows/normal_span_dialog.h -o <scratchpad>/normal_span_dialog.png
  ```
  Exit 0 krävs. A gör samma sak för `conductor_dialog.h` och D för `main_window.h`. Bilderna granskas med Read.
- **Bygg bara de egna målen:**
  - A: `kalkyl_calc_test`, `kalkyl_conductor_test` och `kalkyl_systemdb_test`, och `ctest -R "calc|conductor|systemdb"`
  - B: `kalkyl_projectdb_test` och `ctest -R projectdb`
  - C och D: `kalkyl`
- **Om bygget faller i en fil som ett annat paket äger:** vänta en stund och försök igen. Rör aldrig den filen.

### Steg 0: kontraktsfilerna först
Varje paket skriver först, inom några minuter, sina kontraktsfiler ordagrant ur planen, så att de andra kan kompilera mot dem:
- **A:**
  - `libs/conductor/` (header, `.cpp` och `CMakeLists.txt`, flyttad med de två nya funktionerna)
  - `libs/systemdb` bytt till `kalkyl::conductor` med de nya deklarationerna (5.4)
  - `conductor_dialog.*` bytt till den nya namnrymden
  - `libs/calc/normal_span.h`, `table_text.h` och `CMakeLists.txt` (`.cpp`-filer som kompilerar, gärna med tomma kroppar)
  - `libs/CMakeLists.txt`, med ordningen `version`, `core`, `conductor`, `calc`, `platform`, `db`, `systemdb`, `projectdb`, `appearance`

  Därefter kompilerar allt utom `main_window.cpp`, tills D har bytt namnrymd.
- **B:** ändringarna i `project_db.h` och `project_data.h` (5.5–5.6), en `project_data.cpp` som kompilerar och `CMakeLists.txt` (lägg till `project_data.cpp`, och `kalkyl_calc` och `kalkyl_conductor` PUBLIC).
- **C:** `normal_span_dialog.h` med regionen och det publika gränssnittet (5.7), en minimal `.glazeui` och `.cpp` som kompilerar, och raden i `glaze_add_window` plus `kalkyl_calc` och `kalkyl_conductor` i `src/CMakeLists.txt`.
- **D:** byter först `main_window.*` till `kalkyl::conductor` och `compareNames`.

Ingen väntar på ett meddelande. Den som behöver en annans fil bygger när filen finns.

### Paket A: rena bibliotek, linorna och systemdatabasen
**Äger:**
- `libs/calc/*`: `CMakeLists.txt`, `CLAUDE.md`, `normal_span.h/.cpp`, `table_text.h/.cpp` och `calc_test.cpp`
- `libs/conductor/*` (ny): `CMakeLists.txt`, `CLAUDE.md`, `conductor.h/.cpp` och `conductor_test.cpp`
- `libs/systemdb/*`: `system_db.h/.cpp`, `systemdb_test.cpp`, `CMakeLists.txt` och `CLAUDE.md`. `conductor.h/.cpp` tas bort. `conductor_seed_v1.h` är orörd.
- `src/windows/conductor_dialog.{h,glazeui,cpp}`: bara namnrymden
- `libs/CMakeLists.txt`
- `libs/CLAUDE.md`

**Arbetsgång:**
1. Steg 0, med flytten enligt 3.10.
2. `table_text.cpp` och `normal_span.cpp` enligt 2 och 5.1–5.2, och `sameConductorData` och `compareNames`.
3. `autosaveMinutes` och `setAutosaveMinutes` enligt 5.4.
4. Testdatan enligt bilaga A.
5. Testerna enligt 9.1–9.3. Alla tester i `build-a` ska gå igenom. glaze-render på `conductor_dialog.h`.
6. CLAUDE.md enligt 10. Rapporten innehåller texten till `conductor_dialog`-raderna i `src/windows/CLAUDE.md`.

### Paket B: projektdatabasen (`libs/projectdb`)
**Äger:** `libs/projectdb/*`: `project_db.h/.cpp`, `project_data.h/.cpp`, `projectdb_test.cpp`, `CMakeLists.txt` och `CLAUDE.md`.

**Arbetsgång:**
1. Steg 0.
2. Schemat (4.1), schemakontrollen (3.12), revisionen (5.5) och dataskrivningarna med linkopior, kopplingar och rensning (5.6), med SQL i råa strängliteraler.
3. Testerna enligt 9.4.
4. CLAUDE.md.

### Paket C: formuläret (`src/windows/normal_span_dialog.*`)
**Äger:**
- `src/windows/normal_span_dialog.{h,glazeui,cpp}`
- `src/CMakeLists.txt`
- `src/CLAUDE.md`

**Arbetsgång:**
1. Steg 0.
2. Layout enligt 6.1, beteende enligt 6.2 och tabellen enligt 6.3.
3. Fullt bygge när A och B är klara.
4. glaze-render och röktest:
   ```sh
   KALKYL_DATA_DIR=build-c/smoke timeout -k 2 3 ./build-c/src/kalkyl.exe; echo $?
   ```
   Svaret ska vara 124 eller 137.
5. Rapporten innehåller texten till de tre raderna i `src/windows/CLAUDE.md`.

### Paket D: huvudfönstret (`src/windows/main_window.*`)
**Äger:**
- `src/windows/main_window.{h,glazeui,cpp}`
- `src/windows/CLAUDE.md`

**Arbetsgång**, i den här ordningen, så att varje del kan provas för sig:
1. Namnrymden (steg 0).
2. Dokumentation i projektet och linkopiorna (7.3).
3. Spara, den automatiska sparningen och stängningen (7.6).
4. Högerklick i Dokumentation (7.4).
5. Fliken Normalspann (7.5).
6. Fullt bygge när A, B och C:s steg 0 finns. Slutbygget görs när alla rapporterat klart.
7. glaze-render och samma röktest i `build-d`.
8. CLAUDE.md, där A:s och C:s rader läggs till av samordnaren.

### Samordnaren
**Äger:** `TODO.md`, `doc/plans/CLAUDE.md` och rotens `CLAUDE.md` (avsnitt 10).

**Ordning:**
1. Planen.
2. Användarens svar, inarbetade 2026-11-24.
3. A, B, C och D parallellt (fyra Sonnet-agenter, high).
4. Integration (12).
5. Handtest (13).
6. Ändringarna från handtestet.
7. Granskning (Opus).

### Regler för alla implementeringsagenter
- **Filer redigeras med verktygen Edit och Write.** Inga Python- eller sed-skript ändrar filer. Skal (Bash, PowerShell) används bara för bygge, körning, test och läsning. Bilaga A är ett läskommando. Två filer tas bort med `rm` vid flytten (3.10).
- **Hooken `~/.claude/hooks/only-c-drive.sh`** granskar varje verktygsanrop, också filinnehåll, och stoppar:
  - två bakvända snedstreck i rad (räknas som UNC-sökväg)
  - ett bakvänt snedstreck och en bokstav (escape-sekvens) efter skiljetecken eller blanksteg och före citattecken eller radslut, till exempel ett ensamt radbrytnings-escape inom citattecken
  - andra enhetsbokstäver än C följda av kolon. Det gäller också text som slutar med en bokstav och kolon precis före citattecknet som avslutar en Edit-sträng, till exempel "bakåt:", så låt aldrig `old_string` eller `new_string` sluta så.
- **Därför:**
  - Råa strängliteraler `R"sql( … )sql"` för SQL och `R"( … )"` för reguljära uttryck.
  - Teckenklasser som `[.]` och `[0-9]` i stället för escape.
  - `char{10}` för radbrytning (`const std::string nl(1, char{10});`), `char{9}` för tabb och `char{13}` för CR.
  - Snedstreck framåt i sökvägar.
  - I CMake bracket-argument och `string(ASCII 10 NL)`.
  - I `.glazeui` skrivs tvåradiga kolumnrubriker med radbrytningen direkt efter en bokstav i strängen, som i `conductor_dialog.glazeui`.
- **Kommentarer:**
  - Varje funktion, klass och logisk del har en kommentar på 2–5 rader.
  - Inline-kommentarer är korta och säger bara vad som händer.
  - Ingen historik i kommentarer (vad som ändrats, vem, när, tidigare beteende, "flyttad från").
- **Versionen** står kvar på 0.1.0 i `project(VERSION …)`. Höj den aldrig.
- **Inga commits.**
- **Fönsterreglerna:**
  - Tre filer per fönster.
  - Klassen utanför namespace.
  - Designraden `// glazeui-design: ui=Slate theme=slate(Windows) style=0 lang=sv`.
  - Ingen logik i `.glazeui` (inga if, loopar eller lokala variabler).
  - En sats per rad och inga kommentarer efter en sats.
  - Startlogik i `onLoad`, aldrig i konstruktorn.
  - Bara `<glaze/glaze.h>` (och `<glaze_threads/threads.h>`) från GlazeUI.
- **Varningar:** bygget ska vara utan varningar (`kalkyl_warnings`, `-Wall -Wextra`).
- **CLAUDE.md:** uppdatera katalogens CLAUDE.md när filer läggs till eller byter syfte.

---

## 12. Integration och verifiering (samordnaren)

1. **Bygget utan varningar** i `build/`:
   ```sh
   cmake --build --preset debug 2>&1 | grep -i warning
   ```
   Ingen utdata. De nya CMake-filerna får CMake att köras om av sig självt.
2. **Testerna** i en tillfällig katalog som tas bort efteråt:
   ```sh
   cmake -S . -B build-verify -G Ninja -DCMAKE_BUILD_TYPE=Debug -DCMAKE_C_COMPILER=C:/msys64/ucrt64/bin/gcc.exe -DCMAKE_CXX_COMPILER=C:/msys64/ucrt64/bin/g++.exe -DKALKYL_TESTS=ON -DGLAZE_TOOLS=ON -DFETCHCONTENT_SOURCE_DIR_GLAZEUI=C:/Users/sehoer/Source/Kalkyl/build/_deps/glazeui-src
   cmake --build build-verify && ctest --test-dir build-verify --output-on-failure
   ```
   Alla tester ska gå igenom, också `calc` och `conductor`.
3. **glaze-render** (`build-verify`) på `normal_span_dialog.h`, `main_window.h` och `conductor_dialog.h`:
   - exit 0
   - Bilderna: formuläret med tabellen till vänster och resultat och kontroller till höger. Huvudfönstret med Arkiv > Spara, Inställningar > Automatisk sparning och fliken Normalspann.
4. **Röktest:**
   ```sh
   KALKYL_DATA_DIR=build/smoke timeout -k 2 3 ./build/src/kalkyl.exe; echo $?
   ```
   Svaret ska vara 124 eller 137. Med provprojektet ur `build-verify/prov` som argument visas huvudfönstret.
5. **Flytten:** `libs/systemdb/conductor.h` och `conductor.cpp` finns inte. `grep -rnE "systemdb::Conductor|systemdb/conductor[.]h" src libs` ger ingen träff.
6. **Gamla provprojekt:** tala om för användaren att projekt skapade före denna ändring inte går att öppna (3.12) och måste skapas om.
7. **Versionen:** `project(Kalkyl VERSION 0.1.0` är oförändrad.
8. **Reglerna:**
   - CLAUDE.md är uppdaterad i varje berörd katalog.
   - Kommentarerna har 2–5 rader.
   - Ingen logik finns i `.glazeui`.
   - `grep -rn "ref/" CMakeLists.txt src libs` ger ingen träff i CMake.
9. **Städning:** `rm -rf build-verify`. `build-a` till `build-d` är borta.
10. **Testfallen:** användaren får listan i avsnitt 13.

---

## 13. Handtest (användaren, före granskningen)

Kör `build/src/kalkyl.exe` (Debug). Ha Excel öppet med `ref/Normalspannberäkning.xlsm`.

**Projektet och Dokumentation**
1. **Projekt från v0.1.0:** öppna ett projekt som skapats med v0.1.0 (till exempel `git worktree` på taggen, eller ett befintligt provprojekt). Det öppnas utan fel, Dokumentation är tom och värden kan sparas. Öppna igen: värdena finns kvar.
2. **Nytt projekt:**
   - Dokumentation är tom, utom Konfiguration "Singel" och Topplina höger "Samma".
   - De fyra raderna "Normalspannsberäkning …" visar `-`.
3. **Fyll i** uppspänning, normalspann (grönt) och temperaturer.
   - Titeln får `*` och statusraden visar "Ej sparat".
   - Ctrl+S ger "Sparar…" och sedan "Sparat hh:mm", och `*` försvinner.
4. **Stäng och öppna igen:** alla värden och referenser finns kvar.
5. **Osparat vid stängning:** sätt automatisk sparning till Av, ändra något och prova krysset, Arkiv > Stäng projekt och Arkiv > Avsluta. Frågan visas med Ja/Nej/Avbryt:
   - **Avbryt:** fönstret står kvar.
   - **Nej:** ändringen är borta när projektet öppnas igen.
   - **Ja:** sparar och stänger.

**Automatisk sparning**
6. **Menyn:** Inställningar > Automatisk sparning visar Av, 1, 5, 10, 15 och 30 minuter, med 5 minuter markerat i en ny användarprofil.
7. **En minut:** välj 1 minut, ändra ett värde och vänta. Inom en minut visar statusraden "Sparat automatiskt hh:mm", och `*` försvinner. Utan ändring sparas inget (tiden i statusraden står still).
8. **Inställningen är per användare:** starta om Kalkyl. 1 minut är fortfarande markerat, också i ett annat projekt.
9. **Av:** välj Av, ändra något och vänta två minuter. `*` står kvar.
10. **Fel utan dialog:** sätt 1 minut. Lägg projektet på en share och koppla från nätverket (eller byt namn på mappen i Utforskaren medan projektet är öppet). Ändra något. Statusraden visar "Automatisk sparning misslyckades …", och ingen dialog visas. Koppla in igen: nästa tick sparar.
11. **Konflikt:** ersätt projektfilen i Utforskaren med en kopia av den medan projektet är öppet, och ändra något.
    - Nästa tick visar "Automatisk sparning avstängd: projektfilen har ändrats av någon annan …".
    - Fler ticks sparar inte.
    - Ctrl+S visar konflikten i en meddelanderuta.
12. **Under formuläret:** sätt 1 minut, ändra i Dokumentation och öppna en normalspannsberäkning. Låt den vara öppen i över en minut. Dokumentationens ändring sparas, och formuläret påverkas inte.

**Linorna i projektet**
13. **Välj Faslina:** välj "593 mm² FeAl CURLEW". Titeln får `*`.
14. **Samma lina på två rader:** välj CURLEW även som Topplina vänster. Båda visar CURLEW, och projektet har en kopia (syns inte, men 17 visar att den delas).
15. **Kopian skiljer sig:** öppna Inställningar > Lindata, ändra CURLEW:s diameter till 31,8 och tryck OK.
    - Faslina och Topplina vänster får varningsbild och tooltipen "Projektets kopia skiljer sig …".
    - Beräkningarna använder fortfarande 31,7.
16. **Uppdatera från linregistret:** högerklicka på Faslina och välj posten. Statusraden säger att linan uppdaterats. Varningen försvinner på båda raderna.
17. **Lina borttagen ur registret:** lägg till en egen lina "Testlina" i Lindata, välj den som Faslina och spara projektet. Ta sedan bort Testlina ur registret.
    - Faslina visar fortfarande "Testlina", med tooltipen "Linan finns inte i ditt linregister …".
    - Rullistan innehåller fortfarande Testlina.
    - Stäng och öppna projektet: Testlina finns kvar.
    - Byt Faslina till CURLEW, och sedan tillbaka till Testlina via rullistan (den finns kvar tills ingen använder den). Byt till CURLEW igen och spara. Nu finns Testlina inte längre i rullistan.
18. **Återställ startdata:** återställ startdata i Lindata. Den ändrade CURLEW-diametern försvinner ur registret. Projektets kopia visar varning bara om den skiljer sig.

**Högerklick på normalspannsraderna**
19. **Rader utan meny:** högerklicka på en grön rad och en Linberäkningsrad. Ingen meny visas.
20. **Rad utan beräkning:** högerklicka på "Normalspannsberäkning faslina före ombyggnation". Menyn visar Lägg till…, Koppla befintlig (inaktiv) och Manuellt värde.
21. **Manuellt värde:**
    - Cellen börjar redigeras. Skriv 343,4 och tryck Enter: cellen visar "343,4 m".
    - Töm cellen: den visar `-`.
    - Skriv 343,4 igen.
22. **Lägg till…:**
    - Formuläret öppnas utan lina, med texten "Visas på raden Normalspannsberäkning faslina före ombyggnation när du trycker OK."
    - Kopiera C7:E287 i Excel, klicka i Stpnr på rad 1 och tryck Ctrl+V. Det blir 281 rader och en tom rad sist.
    - Resultaten: aₙ 343,1 m, aₙ,ΔH –, medelspann 318,4 m, totallängd 89 160,4 m och 280 spann, ST0252 – ST0532.
    - Kontrollerna: 1: 0, 2: 5, 3: 0, 4: 64 och 5: 1 avvikelser. Kontroll 2 är röd på ST0325, ST0375, ST0393, ST0451 och ST0459. Kontroll 5 är röd på ST0283, med "395 / 1,15".
    - Spannlängden på ST0252 är grå, med tooltip.
    - OK: ingen fråga. Raden visar "343,1 m" med Σ-symbol och tooltip, och cellen går inte att skriva i. Det manuella värdet 343,4 är borta.
23. **En beräkning på två rader:** högerklicka på "Normalspannsberäkning faslina efter ombyggnation" och välj Koppla befintlig. Undermenyn visar "ST0252 – ST0532 · 343,1 m · Fas före". Välj den.
    - Båda raderna visar 343,1 m.
    - Listan visar Dokumentation "Fas före, Fas efter".
    - Formuläret (Ändra…) visar "Visas i Dokumentation på: …" med båda raderna.
24. **Ändra slår igenom på båda raderna:** öppna med Ändra…, ändra ST0380 till 332 och tryck OK. aₙ ändras på båda raderna och i listan.
25. **Gränserna och Avbryt:** sätt gräns 2 till 0,45. Kontroll 2 får 3 avvikelser. Avbryt och svara Nej. Gränsen är 0,5 när formuläret öppnas igen.
26. **Koppla bort:** på raden "efter". Den raden visar `-`, och raden "före" visar fortfarande 343,1 m.
27. **Ta bort med flera rader:** koppla "efter" igen och välj Ta bort… på raden "före". Frågan listar båda raderna. Ja gör att båda visar `-`, och beräkningen är borta ur listan.

**Fliken Normalspann**
28. **Ny beräkning…:**
    - Klistra in exempel B (bilaga C). Texten säger "Blir en lös beräkning …". Tryck OK.
    - Listan visar raden med gul radton, utan kolumnerna Lina och Lintyp, med Dokumentation "–" och avvikelserna med tooltip.
    - Medelspannet i formuläret är 127,3 m.
29. **Koppla den i Dokumentation:** högerklicka på "Normalspannsberäkning topplina efter ombyggnation" och välj Koppla befintlig och beräkningen. Raden visar 229,9 m, och listan visar "Topp efter".
30. **Kopiera…:**
    - Välj beräkningen och tryck Kopiera…. Formuläret visar samma stolpar, anteckningen "Kopia av P0 – P11" och "Blir en lös beräkning".
    - Ändra P6 till 300 och tryck OK. Listan har två rader, och originalet är oförändrat.
31. **Ändra:** dubbelklick, Enter och Ändra… öppnar formuläret.
32. **Snabbmenyn i listan:** Ändra…, Kopiera… och Ta bort… finns. Ingen kopplingspost.
33. **Topplina höger:**
    - Sätt Topplina höger till en egen lina. Höger-raderna syns och har menyn.
    - Koppla en beräkning till "topplina höger före" och sätt tillbaka "Samma". Listan visar "Topp h. före (dold)".
34. **Ta bort i listan:** frågan listar raderna, och raden försvinner.

**Tabellen i formuläret som i Excel**
35. **Klistra in mitt i tabellen:**
    - Markera Spannlängd på rad 5 och klistra in två kolumner (spannlängd och H) ur Excel. Värdena hamnar i Spannlängd och H från rad 5 och nedåt.
    - Klistra in tre kolumner i H: bara den första hamnar i H, och resten ignoreras.
    - Nya rader skapas när blocket är längre än tabellen.
36. **En kolumn och en rad:** klistra in bara spannlängder i Spannlängd, och sedan en enda rad (Stpnr, spannlängd, H) i Stpnr.
37. **Rubrikrad:** klistra in C6:E20 (med rubrikraden). Rubriken hoppas över.
38. **Text i talkolumn:** klistra in en cell med "abc" i Spannlängd. Meddelandet visas, och inget ändras.
39. **Markering med tangentbordet:** Shift+piltangenter markerar en rektangel (blå). Shift+End går till sista kolumnen och Ctrl+Shift+End till sista raden. En pil utan Shift tar bort markeringen.
40. **Markering med musen:** klicka i en cell och Shift+klicka i en annan. Rektangeln markeras. Prova att dra med musen: ingen rektangel blir markerad (känd gräns), och inget går sönder.
41. **Kopiera till Excel:**
    - Markera ett block med spannlängder med decimaler (ST0380–ST0381) och tryck Ctrl+C. Klistra in i ett tomt Excel-blad: talen blir tal med decimalkomma (332,247).
    - Kopiera kontrollkolumnerna: texterna följer med.
42. **Kopiera internt:** Ctrl+C i en beräkning och Ctrl+V i en annan beräkning och i samma tabell. Värdena är lika.
43. **Klipp ut:** Ctrl+X tömmer cellerna, och Ctrl+V någon annanstans klistrar in dem.
44. **Markera alla och radera:** Ctrl+A markerar alla celler, och Delete tömmer allt. En tom rad finns kvar, och resultaten visar "–". Klistra in från början igen.
45. **Delete på en cell:** tömmer bara den cellen. Beräkningen räknas om.
46. **Klistra in under redigering:**
    - Skriv i en cell och tryck Ctrl+V med ett enda tal i urklippet. Talet hamnar i redigeringen.
    - Gör samma sak med ett block. Redigeringen avbryts, och blocket klistras in från cellen.
47. **Snabbmenyn i tabellen:**
    - Klipp ut, Kopiera, Klistra in, Radera innehåll och Markera alla gör samma sak som tangenterna.
    - "Infoga rad ovanför" infogar en tom rad.
    - "Ta bort rader" tar bort de markerade raderna, och beräkningen räknas om.
48. **ΔH:** klistra in exempel C med H. aₙ,ΔH blir 331,5 m och medelspannet 300,0 m. Töm H på P2: aₙ,ΔH blir "–" och statusraden visar varningen.
49. **Fel:**
    - Töm en spannlängd mitt i: cellen blir röd med tooltip, och statusraden visar "Rad n: Spannlängd saknas."
    - OK ger meddelandet, och cellen blir aktuell.
    - Skriv "abc" i en talcell: "Ogiltigt värde".
50. **Töm…:** frågan visas, och tabellen blir tom.

**Skrivskyddat och utseende**
51. **Skrivskyddat:** öppna samma projekt i en andra Kalkyl och välj skrivskyddat.
    - Arkiv > Spara är inaktiv, och ingen automatisk sparning sker (statusraden ändras inte).
    - Dokumentation går inte att ändra, och linraderna har ingen meny.
    - Högerklick på en kopplad normalspannsrad ger "Visa…".
    - Formuläret visar allt, men går inte att ändra. Ctrl+C och Ctrl+A fungerar, men inte Ctrl+V, Delete eller Ctrl+X. Knappen heter Stäng.
    - Fliken Normalspann: Ny, Kopiera och Ta bort är inaktiva.
52. **Lindata efter flytten:** Inställningar > Lindata fungerar som förut: redigera, validera och återställ startdata.
53. **Utseende:** växla Mörkt och Ljust. Formuläret, de röda och gröna kontrollcellerna, markeringen och varningsbilden på linraderna syns i båda lägena.

---

## 14. Risker och luckor i GlazeUI v0.11.1

1. **Ingen områdesmarkering eller dragmarkering i DataGrid.** Kalkyl emulerar markeringen (3.11). Dragmarkering går inte, eftersom widgets inte har `onMouseMove` och `columnAt` är protected. En beställning till GlazeUI läggs som idé.
2. **Inget event när den aktuella cellen flyttas** i `Cell`-läget. Markeringen synkas med `glaze::app().post` efter tangent och klick. Om tidpunkten inte stämmer märks det i handtest 39–40.
3. **Ordningen för `onKeyDown` under redigering.** Manualen säger att fokusenheten (redigeraren och datagriden) får tangenten före widgetens beteende. Att datagridens handler hinner ta Ctrl+V innan redigeraren klistrar in verifieras i handtest 46. **Fallback:** blockinklistring bara utan redigering.
4. **DataGrid saknar `contextRow()`.** Raden tas fram med `fromWindow` + `rowAt`. Kolumnen går inte att få fram, så snabbmenyn i formuläret gäller markeringen.
5. **Färgerna:** den aktuella cellen ritas alltid med markeringens färger, så dess röda eller gröna kontrollfärg syns inte medan den är aktuell. Markeringens blå ton ersätter grundfärgen i markerade celler.
6. **Ingen ångra** i DataGrid. Avbryt i formuläret ångrar allt.
7. **Publicering mot en share som inte svarar:** `publish` går i en `Worker` och kan avbrytas. En stängning väntar på sparningen ("Stänger när sparningen är klar…"). Hänger sharen helt gäller samma gräns som i Grunder-planen 12. Den automatiska sparningen gör inget nytt försök medan en publicering pågår.
8. **Timern tickar i modala dialogers pump.** En automatisk sparning kan alltså starta medan formuläret eller Lindata är öppet. Det är avsiktligt och säkert (3.7), men `snapshot()` körs då på UI-tråden under dialogen. För en liten projektfil märks det inte.
9. **Paket D är störst**, eftersom alla ändringar i huvudfönstret ligger i samma tre filer. Ordningen i paket D gör att varje del kan provas för sig.
10. **Flytten av `Conductor`** gör att `main_window.cpp` inte kompilerar mellan A:s steg 0 och D:s steg 0. Båda görs först i respektive paket.
11. **Linornas namn har ingen främmande nyckel** (4.1). API:t och testerna garanterar att varje namn finns i `project_conductor`.
12. **Excels `Round` avrundar till jämnt.** Det ger ingen skillnad i praktiken (2.5). Testet på arkets data bekräftar det.
13. **Prestanda:** `setCellColor` och `setCell` på några tusen celler per omräkning. Det är litet. `paintSelection` målar bara om ändrade celler.

---

## 15. Kvarstående frågor

Inga. Medelspannet avgjordes 2026-11-24: antalet spann (2.3 och 2.6). Migreringen från v0.1.0: 3.12.

---

## Bilaga A: spannlängderna ur arket (paket A)

Kör i PowerShell-verktyget (Windows PowerShell 5.1). Kommandot läser bara `ref/` och skriver ingenting. Agenten skriver talen i `calc_test.cpp` med Write.

```powershell
Add-Type -AssemblyName System.IO.Compression.FileSystem
$ci = [Globalization.CultureInfo]::InvariantCulture
$zip = [IO.Compression.ZipFile]::OpenRead('C:/Users/sehoer/Source/Kalkyl/ref/Normalspannberäkning.xlsm')
function Read-Entry([string]$name) {
    $r = New-Object IO.StreamReader($zip.GetEntry($name).Open(), [Text.Encoding]::UTF8)
    $t = $r.ReadToEnd(); $r.Close(); $t
}
$sheet = Read-Entry 'xl/worksheets/sheet1.xml'
$shared = @(foreach ($m in [regex]::Matches((Read-Entry 'xl/sharedStrings.xml'), '(?s)<si>(.*?)</si>')) { $m.Groups[1].Value -replace '<[^>]+>', '' })
$zip.Dispose()
$spans = foreach ($m in [regex]::Matches($sheet, '<c r="D([0-9]+)"([^>]*)><v>([^<]*)</v></c>')) {
    $row = [int]$m.Groups[1].Value
    if ($row -lt 8 -or $row -gt 287) { continue }
    $v = $m.Groups[3].Value
    if ($m.Groups[2].Value -match 't="s"') { $v = $shared[[int]$v] }
    [double]::Parse($v, $ci).ToString('R', $ci)
}
"{0} spann" -f $spans.Count
$spans -join ', '
```

- **Kontroll före användning:** 280 spann, de tolv första `338, 175, 188, 355, 434, 346, 212, 453, 301, 384, 396, 338`, de tre sista `325, 284, 256`, och index 127 och 128 (räknat från 0, alltså ST0380 och ST0381) `332.247, 358.154`. Summan är 89160.401.
- **Spannlängderna står som text** i arket (delade strängar, `t="s"`). Därför slås de upp i `sharedStrings.xml`.
- **Stpnr** är ST0253 … ST0532 för spann 1 … 280. Den första stolpen ST0252 har spannlängden 274 (D7), som inte ingår i summorna och som testet sätter på stolpe 0.

## Bilaga B: kontroll 4 i exempel A (64 spann)

ST0300, ST0301, ST0303, ST0304, ST0307, ST0308, ST0309, ST0310, ST0311, ST0312, ST0313, ST0315, ST0316, ST0320, ST0321, ST0332, ST0333, ST0339, ST0340, ST0341, ST0342, ST0343, ST0344, ST0347, ST0348, ST0349, ST0350, ST0376, ST0377, ST0378, ST0379, ST0381, ST0382, ST0392, ST0393, ST0394, ST0395, ST0427, ST0428, ST0429, ST0430, ST0451, ST0452, ST0453, ST0454, ST0457, ST0458, ST0459, ST0460, ST0461, ST0462, ST0463, ST0464, ST0465, ST0466, ST0467, ST0468, ST0475, ST0477, ST0478, ST0479, ST0480, ST0481, ST0503.

Listan stämmer med de röda cellerna L55 … L258 i arket (rad = 7 + spannets nummer, Stpnr = ST(252 + rad − 7)).

## Bilaga C: exemplen B och C för handtestet (att skriva i Excel och klistra in i Stpnr)

Exempel B (P0 har 999, som inte ingår i summorna):

| Stpnr | Spannlängd | H |
|---|---|---|
| P0 | 999 | |
| P1 | 100 | |
| P2 | 100 | |
| P3 | 100 | |
| P4 | 100 | |
| P5 | 100 | |
| P6 | 400 | |
| P7 | 100 | |
| P8 | 100 | |
| P9 | 100 | |
| P10 | 100 | |
| P11 | 100 | |

Exempel C:

| Stpnr | Spannlängd | H |
|---|---|---|
| P0 | 250 | 10 |
| P1 | 300 | 20 |
| P2 | 200 | 15 |
| P3 | 400 | 30 |

Skriv tabellerna i Excel och kopiera därifrån, eller kopiera raderna ur Kalkyls egen tabell.
