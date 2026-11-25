# Linberäkning: modulen, fliken Linberäkning och kopplingen till Dokumentation

**Datum:** 2026-11-25, uppdaterad samma dag med användarens svar · **Status:** klar (handtestad och granskad, D10–D11 tillagda) · **Version:** 0.3.0

Planen gäller arket Linberäkning i `ref/Linberäkning.xlsm` och delar arbetet i tre faser. Fas 1 är planerad i detalj: namnbytet av `libs/calc` och linberäkningen som eget bibliotek med tester. Den är kontraktet för paketen N, A, B och C och för granskningen. Mallarna i 2.5, avvikelserna i 4.2, filerna i 7.3 och header-skisserna i 5.3–5.4 är bindande. Namn, signaturer och texter följs ordagrant. Behöver ett paket ändra en signatur meddelar det samordnaren i sin rapport, och samordnaren beslutar. Fas 2 och 3 är skisser (avsnitt 9 och 10) och får en gemensam plan när fas 1 är klar.

Underlag från planeringen:
- **Källan är arkets XML och VBA-koden.** Planen bygger på arkets celler, sparade värden, talformat och skydd i `xl/worksheets/sheet1.xml` och på VBA-koden i tillägget 5420_Verktyg.xlam. Skärmbilden från användaren är bara en översikt.
- **Arket har inga formler.** Allt räknas av tilläggets moduler `Linber_Modul1_20151109` (indata), `Linber_Modul2_20151109` (laster, tillståndsekvationen, transponeringen och utskriften i arket) och `Linber_Modul3_20151109` (tredjegradsekvationen och Eiv). Arkets händelser och knappar ligger i `Linber_Blad_20151109`. Arkmodulen `ref/vba/Linberäkning/Blad1.cls` anropar dem med `Application.Run`.
- **Exemplet i arket är ett komplett fall.** I skärmbilden ser rullistan Lina tom ut. I XML innehåller J2 ändå namnet "124 mm² OPGW AFL 1x48 SMF 14230/20-01 (Dotterel)" och O2 rullistans index 104. Rullistan är en ActiveX-kontroll ovanpå J2:O2, och den fylls först när arket aktiveras med tillägget laddat. Lindatat i J9–J18 är exakt samma lina i linregistrets startdata (`libs/systemdb/conductor_seed_v1.h`, sort_order 1090, rad 109 i bladet Lindata = index 104 + 5). Testfall T1 använder linan och talen, och talen räcker också utan namnet (2.2).
- **Beräkningen är kontrollerad.** Under planeringen räknades VBA-koden om formel för formel (Python, i scratchpad). Omräkningen ger arkets alla sparade resultat för de 15 lastfallen med en relativ skillnad under 1e-15. Det gäller också X29 = 90,000000001010662, där Cardanos formel inte ger exakt σ₀ = 90. Kalkyl ska alltså räkna med samma formler i samma ordning, inte med en "bättre" lösare, för att stämma med Excel (5.6).
- **2015 och 2016:** Linberäkning.xlsm använder modulerna `…_20151109`. Kärnan i `…_20160421` (Sammanslagning.xlsm) räknar likadant (3.8).

**Användarens beslut 2026-11-25** (inarbetade i avsnitten):
1. **Omfång:** bara arket Linberäkning. Isolatorutslag, lokal islast, regleringstabell, nedhäng, slackutslag, sammanslagning och utskriftsarket med "Linbrott" blir idéer i `TODO.md` (1.3).
2. **Tre faser:** 1) modulen i `libs/` med tester, 2) en egen flik med lista och ett fönster som liknar arket, sparat i projektdatabasen, 3) koppling från Dokumentation med automatiska värden och manuella beräkningar. Fas 2 och 3 får en gemensam plan.
3. **Läget Excel eller Ny** i varje beräkning (`CalcMode`, 4.2). Användarens ord: "Jag vill ha en toggle, excel/ny där excel är exakt som i excel, och ny där felen är rättade. Dels för granskningen att vi verkligen fått det rätt, men sen så vi vet vi gör rätt. Intressant att kunna se skillnaden. Sen kan det hända vi tar bort excel varianten."
   - Excel räknar som arket med alla egenheter och fel. Facittesterna körs i läget Excel.
   - Ny rättar felen. Varje avvikelse har ett eget test som visar skillnaden mellan lägena.
   - Läget sparas per beräkning, och formuläret i fas 2 får en växelknapp. Excel-läget hålls isolerat så att det kan tas bort (5.6).
   - Förvalet i en ny beräkning är Ny (planens val, eftersom Excel-läget kan tas bort).
4. **π** är indata som i arket (O10, förval 3,1416) och används i båda lägena.
5. **Formuläret följer arket.** Användarens ord: "Ändra inte upplägget, det är ett stort plus om vi kommer nära orginalet med uppställningen och hur det funkar." Indata, lindata, konstanter och lastfallstabellen går att ändra som i Excel. Att välja en lina fyller i lindatat, som sedan kan ändras för hand.
6. **En ny beräkning:** T₀ = 0, min −50 °C och max +50 °C. Linan och uppspänningen är tomma, och därmed också lindatat. Planen låter också normalspannet vara tomt. Tomma indata ger ingen beräkning (5.5, 9.2).
7. **Mall till Dokumentation** är stakningsmallen: Mall (Z) i lastfall 15, vid maxtemperatur. Den är inte valbar.
8. **Bibliotek med en uppgift var** (5.1, 7.3): `libs/calc` byter namn till `libs/anCalc` och innehåller bara normalspannsberäkningen. Tabelltexten flyttar till `libs/tableText`. Linberäkningen och lastfallsmallarna blir `libs/lineCalc`. Användarens ord: "håll dem separerade, libs skall vara specieliserade på sin uppgift." Namnbytet är ett eget paket N före de andra.
9. **Planens övriga förslag gäller:**
   - Utnyttjandegraden: röd färg och en varning i fas 1, högsta utnyttjandegrad som kolumn i fliken i fas 2.
   - Manuell last: en flagga per lastfall som tas bort med en snabbmeny eller en mall.
   - Lastfall utan benämning räknas och visas dämpade.
   - En kopplad beräkning hämtar lina, uppspänning och normalspann ur Dokumentation. aₙ kommer från normalspannsraden för samma lina och skede, annars från den gröna raden "Normalspann …", annars blir det ett fel.
   - En manuell beräkning lagrar sin egen kopia av lindatat.
   - En beräkning med indata ur Dokumentation kopplas till en rad, en manuell kan visas på flera.
   - Användaren tar fram facit för T2–T15. Talen checkas in i testet, och testets utskrift är handtestet i fas 1.

Det finns inga öppna frågor (avsnitt 12).

---

## 1. Mål, faser och avgränsning

### 1.1 Faserna
1. **Fas 1, modulen** (den här planen i detalj):
   - `libs/calc` byter namn till `libs/anCalc`, och tabelltexten flyttar till `libs/tableText`. Inget beteende ändras.
   - Arkets beräkning i ett nytt bibliotek `libs/lineCalc`, med lägena Excel och Ny, lastfallsmallarna (knapparna i arket) och tester mot facit ur Excel.
2. **Fas 2, fliken Linberäkning** (skiss i avsnitt 9): en flik i huvudfönstret som fliken Normalspann, med Ny beräkning…, Ändra…, Kopiera… och Ta bort. Beräkningarna sparas i projektdatabasen i ett nytt dev-migreringssteg. Ett nytt fönster visar beräkningen med arkets uppställning och växelknappen Excel/Ny.
3. **Fas 3, kopplingen från Dokumentation** (skiss i avsnitt 10): högerklick på raderna "Linberäkning …" för att lägga till eller koppla en beräkning, som för normalspann. En kopplad beräkning hämtar lina, normalspann, uppspänning och temperaturer ur Dokumentation. Manuella beräkningar har egna indata.

### 1.2 Ingår i fas 1
- **Namnbytet** (paket N, 7.3): `libs/anCalc` och `libs/tableText` med alla berörda filer, utan ändrat beteende.
- `libs/lineCalc/line_calc.h/.cpp`: indata, lastfall, laster, tillståndsekvationen, Eiv, transponeringen, alla resultatkolumner i arket, lägena Excel och Ny, och valideringen med svenska texter (avsnitt 3–5).
- `libs/lineCalc/load_cases.h/.cpp`: en ny beräkning, arkets tre knappar "Återställ indata enl. SS-EN 50341-2-18", "Övre linan" och "Nedre linan" (2.5), och arkets följdändringar när min, max eller vindtrycken ändras (2.4).
- Tester: `linecalc_test.cpp`, `load_cases_test.cpp` och `linecalc_facit_test.cpp` med facit ur Excel (avsnitt 6).
- CLAUDE.md i alla berörda kataloger (avsnitt 11).

### 1.3 Ingår inte
- **Andra ark** i Linberäkning.xlsm: Isolatorutslag, Lokal islast, Regleringstabell, Nedhäng, Slackutslag ("EJ KLART" i originalet), Data till lastfallsdiagram och Linberäkning utskrift, med raderna "Linbrott lastfall ID 5" för häng- och spännkedja (Int(0,3 resp. 0,6 · (1,43 · (F₅ − F₇) + 1,1 · F₇))). Sammanslagning.xlsm ingår inte heller. Samordnaren lägger dem som idéer i `TODO.md`.
- **Fönster och projektdatabas för linberäkningen** i fas 1. Paket N ändrar bara namn och includes i `src/` och `libs/projectdb`.
- **Versionen** står på 0.3.0 och ändras inte. Inga commits.

---

## 2. Arket Linberäkning

### 2.1 Kartläggning av arket
Arket är `sheet1.xml` ("Linberäkning"). Använt område A1:AO64, utskriftsområde A1:AA38, 36 sammanslagna områden. Det finns inga formler, ingen datavalidering och ingen villkorsstyrd formatering. Kolumnerna B–D är dolda, och arket är skyddat. De olåsta cellerna står i tabellen.

| Område | Innehåll | Låst |
|---|---|---|
| Rad 1 | Menyraden med ActiveX-knappar till de andra arken: Isolatorutslag, Sammanslagning, Linberäkning, Nedhäng, Skriv ut, Slackutslag, Lindata, Lokal islast, Regleringstabell och Lastfallsdiagram | – |
| E2:E7 | Rubriken "Indata" | ja |
| F2, J2:O2 | "Lina" och rullistan `Linberakning_Lindata` (ActiveX ovanpå J2:O2). J2 = linans namn, O2 = rullistans index | J2 via rullistan |
| F3:J7 | aₙ (J3, m), σ₀ (J4, MPa), T₀ (J5, °C), minimumtemperatur (J6, °C), maximumtemperatur (J7, °C) | olåsta |
| K3:O5 | Linhöjd, snitt (O3, m), karakteristiskt vindtryck Normal vind (O4) och Hög vind (O5), N/m² | olåsta |
| U3 | Infotext "Fetmarkerade siffror triggar igång linberäkningen." (fet stil på J2–J7 och O3) | – |
| E9:E18 | Rubriken "Lindata / Konstanter" | ja |
| F9:J18 | Lindata: A (J9, mm²), d (J10, mm), Eik (J11), Ep (J12), σp (J13, MPa), k (J14), α (J15, 10⁻⁶/°C), Tp (J16, °C, format 0.0), qe (J17, N/m), fu (J18, kN) | olåsta |
| K9:O17 | Konstanter: isdensitet ρ (O9, 917,4 kg/m³), π (O10, 3,1416), g (O11, 9,81), Gq (O12, 1), Gc (O13, 0,5), Cc (O14, 1), minsta islast vid vindstilla (O15, 20 N/m), högsta tillåtna utnyttjandegrad (O16, 55 %), lastfall för transponering till Ep-stadiet (O17, 5) | olåsta |
| U10 och knapparna | "Läs in korsningslastfall enligt Table 5.9.6/SE.1 (50341-2-18)" med "Övre linan" och "Nedre linan", och knappen "Återställ indata enl. SS-EN 50341-2-18" | – |
| E21/F21, U21, AC21 | Grupprubrikerna "Lastfall", "Resultat" och "Data till SAG (3D)" | ja |
| F22:AE22 | Kolumnrubrikerna (tabellen nedan) | ja |
| Rad 23–29 | Lastfall 1–7, Ei-stadiet | se nedan |
| Rad 30 | Tom avskiljare. S30 och W30:AA30 är olåsta, S30 fet | – |
| Rad 31–38 | Lastfall 8–15, Ep-stadiet | se nedan |
| B21, B22 (dolda) | B21: temperaturflagga (0 eller 2), används bara i bortkommenterad kod. B22: 0 under utskriften i arket och 1 efteråt | – |
| B23:B38 (dolda) | Manuell last per lastfall: 0 = räknas ur vind och is, 1 = I och Qwc är inskrivna för hand (3.2) | – |
| AH42, AI48:AO64 | Rester: en tom fet cell och celler med bara mellanslag. Används inte | – |

Lastfallskolumnerna (rad 23–29 och 31–38). Format: "0" = heltal, "0.0" och "0.00" = en och två decimaler, Allmänt = Excels standard.

| Kolumn | Rubrik | Enhet | Slag | Format | Låst |
|---|---|---|---|---|---|
| F | Lastfall ID (1–15) | – | fast | Allmänt | ja |
| G | Temp T | °C | indata | Allmänt | nej |
| H | Lastfall benämning | – | text | Allmänt | nej |
| I | Stadium (Ei eller Ep) | – | fast | Allmänt | ja |
| J | SS-EN 50341 Standard load cases | – | text | Allmänt | nej |
| K | SS-EN 50341 Load cases for calculation of clearences | – | text | Allmänt | nej |
| L | Vid is och vindstilla använd vindparametrar från lastfall-ID | – | indata | Allmänt | nej |
| M | Höjd h | m | beräknad visning | Allmänt | nej |
| N | Magnitud vindlast | % | indata | Allmänt | nej |
| O | Karakteristiska vindtrycket qh | N/m² | indata | Allmänt | nej |
| P | Referenshöjd href | m | indata | Allmänt | nej |
| Q | dqh | N/m³ | indata | Allmänt | nej |
| R | Vindtryck qh | N/m² | beräknad | 0 | nej |
| S | Istjocklek vid normal vind | mm | indata | 0.0 | nej |
| T | Motsvarande istjocklek vid vindstilla | mm | beräknad | 0.0 | nej |
| U | Islast I | N/m | beräknad, eller indata vid manuell last | 0.00 | nej |
| V | Vindlast Qwc | N/m | beräknad, eller indata vid manuell last | 0.00 | nej |
| W | Nedhäng vertikal projektion b | m | resultat | 0.00 | nej |
| X | Spänning σ | MPa | resultat | 0.00 | nej |
| Y | Linkraft F | N | resultat | 0 | nej |
| Z | Mall | nr | resultat | 0 | nej |
| AA | Utnyttjandegrad av brottlast | % | resultat, röd över O16 | 0 | nej |
| AC | Nedhäng b (i den resulterande lastens plan) | m | resultat | 0.00 | ja |
| AD | Utsvängningsvinkel α | ° | resultat | 0.00 | ja |
| AE | Mall | nr | resultat, avrundad | 0 | ja |

### 2.2 Utanför skärmbilden
Följande finns i arket men syns inte i bilden, eller syns som tomt:
1. **Menyraden i rad 1** med knapparna till de andra arken. De leder utanför omfånget och ingår inte.
2. **Dolda kolumner B–D.** B håller flaggorna i 2.1. C och D är tomma. Flaggan för manuell last motsvaras av `LoadCase::manual` (5.3). De andra flaggorna behövs inte.
3. **Linans namn i J2 och index i O2**, bakom rullistan. Rullistan ser tom ut i bilden, men linan är vald. I Kalkyl visas namnet i rullistan.
4. **Utskriftsområdet** slutar vid AA. "Data till SAG (3D)" skrivs bara om AC21 inte är tom (äldre versioner av arket saknar rubriken). Kalkyl räknar alltid kolumnerna.
5. **Resultatcellerna U–AA är olåsta.** Användaren kan skriva över ett resultat i Excel, men nästa omräkning skriver tillbaka det. Bara U och V har en betydelse som indata (manuell last).
6. **Resterna** AH42 och AI48:AO64 används inte.

Inga andra fält i exemplet är kvarlämnade. Tomma celler i J, K och L är tomma i mallen. Alla lastfallsvärden i exemplet följer standardmallen (2.5.1) med J6 = −50 och J7 = 50.

### 2.3 När en lina väljs (`Linber_Blad_20151109.blad1_Linberakning_Lindata`)
1. O2 får rullistans index och J2 linans namn.
2. Bladet Lindata gås igenom från rad 5 till 201. Den **sista** raden vars namn i kolumn B är lika med J2 används.
3. J9–J18 fylls ur raden: D (area), E (diameter), F (Eik), G (Ep), H (σp), I (k), J (α), **L** (Tp), M (qe) och N (brottlast, kN). Kolumn K (εc) används inte. Tp kommer alltså direkt ur L, inte ur K/J.
4. Kontrollerna efteråt visar ett meddelande, men beräkningen körs ändå (`Call blad1_Worksheet_Change(Range("$J$3"))` står efter etiketten `Avsluta`):
   - en tom cell ger "Fyll i samtliga lindata i kalkylblad Lindata", och resten av J9–J18 skrivs inte
   - areans kontroll testar slingvariabeln `a` (= 11 efter slingan), inte arean, och slår aldrig till
   - övriga kontroller: d, Eik, Ep, σp, qe och fu > 0, k, εc och Tp ≥ 0
5. Brottlast "-" i Lindata ger "-" i J18. Indata gör då brottlasten till 0, och utnyttjandegraden visas som "-".
6. J9–J18 är olåsta. Lindatat kan skrivas för hand, med eller utan vald lina.

**I Kalkyl** kommer linan från linregistret (systemdatabasen) eller från projektets linkopior, som `kalkyl::conductor::Conductor`. Att välja en lina fyller i lindatat, som sedan kan ändras för hand (beslut 5). Mappningen står i 5.2. Typen har alla fält som beräkningen behöver, och εc används inte, som i Excel.

### 2.4 Händelser: vad som räknas när (`blad1_Worksheet_Change`)
- **Omräkning** sker när någon av dessa celler ändras: J2–J7, O3–O5, O9–O17, G23–G38, M, N, P, Q, R, S, U och V i lastfallsraderna. **Inte** vid ändring i J9–J18 (lindata), O23–O38 (vindtrycket per lastfall), H, J, K, L eller T. Den nya siffran används vid nästa omräkning.
- **Följdändringar före omräkningen:**
  - J6 ändrad: G23 = J6 och G24 = J6 + 20 (lastfall 1 och 2).
  - J7 ändrad: G38 = J7 (lastfall 15).
  - O4 eller O5 ändrad: O-kolumnen skrivs om på alla 15 rader enligt standardmallens mönster. Lastfall 2, 3, 4, 8, 13 och 14 får O4, lastfall 6 och 10 får O5, och övriga får 0. Det gäller också när en korsningsmall är inläst (4.2, D7–D9).
  - U eller V ändrad: kolumn B i raden blir 1, alltså manuell last.
- **Omräkningen:** `exeLin`, `lastfall` och `berakning` för Ei, sedan `calcEP`, `lastfall` och `berakning` för Ep (3.7).
- **Kontrollen av tomma indataceller** (`Kontrollera`, `Check_2`) räknas ut men används aldrig (`GoTo Avsluta_2` är bortkommenterad). Den pekar dessutom på de tomma cellerna O6 och O7.
- **Fel i indata:** `indata` har `On Error GoTo FEL` med "FELAKTIG INDATA FÖRMODLIGEN ÄR NÅGOT VÄRDE SATT TILL 0", men fortsätter sedan. Division med noll i `berakning` stoppar makrot.

I Kalkyl gör `applyInputChange` (5.4) följdändringarna för J6, J7, O4 och O5. Fas 2 räknar om vid varje ändring.

### 2.5 Lastfallsmallarna (knapparna)
Mallarna är bindande för paket B.
- **Kolumnerna Temp och O** anger källan (5.3): Min = J6, Max = J7, Tks = kortslutningstemperaturen, Normal = O4 och Hög = O5. Ett tal betyder källan Fixed.
- **Talen** `tempC` och `charPressureNPerM2` får det värde som makrot skriver i cellen: källans värde när knappen trycks, och 500 där Excel skriver 500. Läget Excel räknar med talen, läget Ny med källorna (4.2, D7–D9).
- Tomma celler i J, K och L är tomma.
- Alla mallar tar bort manuell last (B = 0) på alla rader. M, R och T räknas alltid om (3.2) och lagras inte.
- Benämningarna skrivs ordagrant, med mellanslagen inom backticks.

#### 2.5.1 Standard: "Återställ indata enl. SS-EN 50341-2-18" (`reset_Swedish_standard`)
Sätter O3 = 25, O4 = 500, O5 = 800, J6 = −40, J7 = 60, O9 = 917,4, O10 = 3,1416, O11 = 9,81, O12 = 1, O13 = 0,5, O14 = 1, O16 = 55 och O17 = 5, som makrot och i båda lägena. **Behålls:** läget, linan, J3–J5 och O15 (minsta islast). En ny beräkning har ändå min −50 och max +50 (beslut 6, 5.4).

| ID | Temp | Benämning (H) | J | K | L | N % | O | P | Q | S mm |
|---|---|---|---|---|---|---|---|---|---|---|
| 1 | Min | `Vindstilla` | | H | | 0 | 0 | 0 | 0 | 0 |
| 2 | Min + 20 | `Normal Vind` | | D | | 100 | Normal | 25 | 6 | 0 |
| 3 | 15 | `30% Normal Vind` | | E | | 30 | Normal | 25 | 6 | 0 |
| 4 | 0 | `Islast och Normal Vind` | | A | | 100 | Normal | 25 | 6 | 18 |
| 5 | 0 | `Islast och Vindstilla` | 2a | | 4 | 0 | 0 | 0 | 0 | 18 |
| 6 | 0 | `Hög Vind` | 1a | | | 100 | Hög | 10 | 6 | 0 |
| 7 | 0 | `Vindstilla` | 4 | | | 0 | 0 | 0 | 0 | 0 |
| 8 | 0 | `Islast och Normal Vind` | 3 | B | | 100 | Normal | 25 | 6 | 18 |
| 9 | 0 | `Islast och Vindstilla` | 2a | | 8 | 0 | 0 | 0 | 0 | 18 |
| 10 | 0 | `Hög Vind` | 1a | | | 100 | Hög | 10 | 6 | 0 |
| 11 | 0 | `Vindstilla` | | | | 0 | 0 | 0 | 0 | 0 |
| 12 | 15 | `Vindstilla` | | G | | 0 | 0 | 0 | 0 | 0 |
| 13 | 15 | `Normal Vind` | | C | | 100 | Normal | 25 | 6 | 0 |
| 14 | 15 | `30% Normal Vind` | | F | | 30 | Normal | 25 | 6 | 0 |
| 15 | Max | `Vindstilla` | | | | 0 | 0 | 0 | 0 | 0 |

Med standardmallen ger källorna samma värden som arkets följdändringar i 2.4.

#### 2.5.2 Korsningslastfall, nedre linan (`blad1_Korsningslastfall_Nedre`)
Sätter O17 = 5. Övriga indata och konstanter behålls.

| ID | Temp | Benämning (H) | J | K | L | N % | O | P | Q | S mm |
|---|---|---|---|---|---|---|---|---|---|---|
| 1 | 35 | `I  (+35 vindstilla)` | | | | 0 | 0 | 0 | 0 | 0 |
| 2 | 0 | `II-IV  (vindstilla)` | 4 | | | 0 | 0 | 0 | 0 | 0 |
| 3 | Max | `V  (maxtemp och vindstilla)` | | | | 0 | 0 | 0 | 0 | 0 |
| 4 | Min | `VI  (mintemp och vindstilla)` | | H | | 0 | 0 | 0 | 0 | 0 |
| 5 | 0 | `Islast och Vindstilla` | 2a | | 6 | 0 | 0 | 0 | 0 | 18 |
| 6 | 0 | `Islast och Normal Vind` | | A | | 100 | Normal, talet 500 | 25 | 6 | 18 |
| 7 | 0 | (tom) | | | | 0 | 0 | 0 | 0 | 0 |
| 8 | 35 | `I  (+35 vindstilla)` | | | | 0 | 0 | 0 | 0 | 0 |
| 9 | 0 | `II-IV  (vindstilla)` | | | | 0 | 0 | 0 | 0 | 0 |
| 10 | Max | `V  (maxtemp och vindstilla)` | | | | 0 | 0 | 0 | 0 | 0 |
| 11 | Min | `VI  (mintemp och vindstilla)` | | | | 0 | 0 | 0 | 0 | 0 |
| 12–15 | 0 | (tom) | | | | 0 | 0 | 0 | 0 | 0 |

#### 2.5.3 Korsningslastfall, övre linan (`blad1_Korsningslastfall_Ovre`)
Excel frågar först "Lastfall III (lokal islast) måste beräknas separat! … Ange din kortslutningstemperatur för linan:" med förvalet 100. Sätter O17 = 3 och kortslutningstemperaturen. Övriga indata och konstanter behålls.

| ID | Temp | Benämning (H) | J | K | L | N % | O | P | Q | S mm |
|---|---|---|---|---|---|---|---|---|---|---|
| 1 | 0 | `Islast och Normal vind` | | A | | 100 | Normal, talet 500 | 25 | 6 | 18 |
| 2 | Max | `I   (maxtemp och vindstilla)` | | | | 0 | 0 | 0 | 0 | 0 |
| 3 | 0 | `II  (islast och vindstilla)` | | | 1 | 0 | 0 | 0 | 0 | 18 |
| 4 | 0 | `IV (vindstilla)` | | | | 0 | 0 | 0 | 0 | 0 |
| 5 | Tks | `V  (kortslutningstemp.)` | | | | 0 | 0 | 0 | 0 | 0 |
| 6 | Min + (Tks − Max) | `VI (kortslutningstemp.)` | | | | 0 | 0 | 0 | 0 | 0 |
| 7 | 0 | (tom) | | | | 0 | 0 | 0 | 0 | 0 |
| 8 | Max | `I   (maxtemp och vindstilla)` | | | | 0 | 0 | 0 | 0 | 0 |
| 9 | 0 | `II  (islast och vindstilla)` | | | 13 | 0 | 0 | 0 | 0 | 18 |
| 10 | 0 | `IV (vindstilla)` | | | | 0 | 0 | 0 | 0 | 0 |
| 11 | Tks | `V  (kortslutningstemp.)` | | | | 0 | 0 | 0 | 0 | 0 |
| 12 | Min + (Tks − Max) | `VI (kortslutningstemp.)` | | | | 0 | 0 | 0 | 0 | 0 |
| 13 | 0 | `Islast och Normal vind` | 3 | B | | 100 | Normal, talet 500 | 25 | 6 | 18 |
| 14–15 | 0 | (tom) | | | | 0 | 0 | 0 | 0 | 0 |

Excel skriver R = 0 på alla rader. Lastfall 9 hämtar vindtrycket från lastfall 13, som räknas senare i samma omräkning. Makrot sätter därför S32 = 18 två gånger, så att arket räknas två gånger (4.1, punkt 9).

---

## 3. Beräkningen (VBA)

### 3.1 Beteckningar och enheter
| Tecken | Cell | Betydelse | Enhet |
|---|---|---|---|
| aₙ | J3 | normalspann | m |
| σ₀, T₀ | J4, J5 | uppspänning vid referenstemperaturen | MPa, °C |
| A, d | J9, J10 | tvärsnittsarea, diameter | mm², mm |
| Ei, Ep | J11, J12 | initiell och permanent E-modul | MPa |
| σp, k | J13, J14 | högsta spänning där Ei gäller, konstant för Eiv | MPa, – |
| α | J15 · 10⁻⁶ | längdutvidgning | 1/°C |
| Tp | J16 | permanent förlängning genom krypning, som temperaturhöjning | °C |
| qe, fu | J17, J18 · 1000 | egentyngd, brottlast | N/m, N |
| h₀ | O3 | linhöjd, snitt | m |
| ρ, π, g | O9, O10, O11 | isdensitet, arkets π, tyngdacceleration | kg/m³, –, m/s² |
| Gq, Gc, Cc | O12, O13, O14 | gust response, structural resonance och drag factor | – |
| Imin | O15 | minsta islast vid vindstilla | N/m |
| tr | O17 | lastfallet för transponering (1–7) | – |
| γ | ρ/1000 · g | isens tunghet (VBA `ISDENS`) | – |

Lastfall i har Tᵢ (G), Nᵢ (N), Oᵢ (O), Pᵢ (P), Qᵢ (Q), Sᵢ (S) och Lᵢ (L).

### 3.2 Laster per lastfall (`Linber_Modul2_20151109.lastfall`)
För varje lastfall som **inte** har manuell last (B = 0):
1. **Höjd:** hᵢ = h₀, men hᵢ = Pᵢ om h₀ < Pᵢ.
2. **Vindtryck** (R): Rᵢ = Oᵢ + Qᵢ · (hᵢ − Pᵢ).
3. **Reducerat tryck:** qᵢ = Rᵢ · Gq · Gc · Cc.
4. **Diameter med is:** dᵢ = d + 2 · Sᵢ (mm).
5. **Vindlast** (V, Qwc): Vᵢ = qᵢ · (Nᵢ / 100) · dᵢ / 1000.
6. **Islast vid vind:** Iᵥ = γ · (dᵢ² − d²) · π / 4 / 1000, med arkets π.
7. **Is vid vindstilla, likvärdig last:**
   - basen är R för lastfallet i L när qᵢ = 0 och Sᵢ ≠ 0, annars Oᵢ, plus Qᵢ · (hᵢ − Pᵢ)
   - Iₛ = √((bas · Gc · dᵢ / 1000)² + (qe + Iᵥ)²) − qe
   - bara Gc används här, inte Gq och Cc, och inte magnituden Nᵢ
   - VBA har `* 10) / 10` på två ställen, som inte ändrar något
8. **Islast** (U, I):
   - qᵢ = 0 och Sᵢ ≠ 0: Uᵢ = max(Iₛ, Imin)
   - Sᵢ = 0: Uᵢ = 0
   - annars: Uᵢ = Iᵥ
9. **Motsvarande istjocklek vid vindstilla** (T): Tᵢ = (√(4 · Iₛ / (g · π · ρ) + (d/1000)²) · 1000 − d) / 2 när Sᵢ ≠ 0, annars 0. Iₛ är här värdet efter max(…, Imin) i vindstillegrenen.
10. **Höjd för visning** (M): 0 när Nᵢ = 0, annars h₀ (också när h₀ < Pᵢ).

Villkoret "vindstilla" är qᵢ = 0, alltså trycket efter faktorerna. Ett lastfall med Nᵢ = 0 men Oᵢ > 0 räknas som vind och får Uᵢ = Iᵥ.

Med **manuell last** (B = 1) är Uᵢ och Vᵢ de inskrivna talen. Excel tömmer då J, K och M–T i raden.

Referensen i L läses som `Cells(22 + L, 18)` i Ei och `Cells(23 + L, 18)` i Ep. Med ID 1–7 i Ei och 8–15 i Ep blir raden rätt. Pekar L till det andra stadiet hamnar den en rad fel (4.2, D2).

### 3.3 Tillståndsekvationen (`Linber_Modul2_20151109.berakning`, `Linber_Modul3_20151109.TREDJEGRAD`)
Ett stadium har en referens (σᵣ, Tᵣ) och en E-modul E. Så här räknas lastfall i.
```
K1 = qe² · aₙ² / A² / 24                          (MPa²)
Qᵢ = √(Vᵢ² + (qe + Uᵢ)²)                          (resulterande last, N/m)
c  = K1 / σᵣ² · E − σᵣ + α · (Tᵢ − Tᵣ) · E
D  = K1 · E · (Qᵢ / qe)²
σᵢ = roten till σ³ + c · σ² − D = 0
```
Det är tillståndsekvationen för en parabel, K1 · (Qᵢ/qe)² / σᵢ² − σᵢ/E = K1/σᵣ² − σᵣ/E + α · (Tᵢ − Tᵣ).

`TREDJEGRAD` löser x³ + c·x² − D = 0 med P = −c²/3, Q = 2c³/27 − D och Z = Q²/4 + P³/27:
- **Z ≥ 0 (Cardano):** u = |√Z − Q/2|^(1/3) · sgn(√Z − Q/2), v = |−√Z − Q/2|^(1/3) · sgn(−√Z − Q/2) och x = u + v − c/3.
- **Z < 0 (trigonometriskt):**
  - R = 2 · (√(−P³/27))^(1/3)
  - F = ACOS(−Q / 2 / √(−P³/27)), där `ACOS(y) = −Atn(y / √(1 − y²)) + pi/2` med **arkets π**, och `ACOS(±1) = pi/2` med meddelandet "Division med noll."
  - X1 = R · cos(F/3) − c/3, X2 = R · cos(F/3 + 2π/3) − c/3, X3 = R · cos(F/3 + 4π/3) − c/3
  - negativa rötter blir −1e25, och den rot som ligger närmast σᵣ väljs

Med D > 0 har ekvationen alltid **exakt en positiv rot** (f(0) = −D < 0, ett teckenbyte i koefficienterna). Den är den största roten och alltså X1, och rotvalet mot σᵣ ger samma rot. Med D = 0 (bara i 4.2, D4) är rötterna 0, 0 och −c, och svaret beror på avrundningen.

### 3.4 Eiv (`Linber_Modul3_20151109.EIV`)
Bara i Ei-stadiet (`If e = ei`), när σᵢ > σp, så här.
```
E3 = Ei
gör om:
    E4 = Ei − k · (σᵢ − σp)
    om |E4 − E3| < 10: sluta
    E3 = E4
    σᵢ = lös 3.3 med E = E4
lös 3.3 med E = E4 en gång till
```
Referensen (σᵣ, Tᵣ) är densamma. Slingan har ingen gräns för antalet varv. E4 är en global variabel och står kvar efter lastfallet (3.5).

### 3.5 Transponeringen till Ep (`Linber_Modul2_20151109.calcEP` och `transp`)
Efter Ei-lastfallen 1–7 körs `transp` en gång. tr = O17, T_tr = transponeringslastfallets temperatur och Q_tr dess resulterande last ur 3.3.
1. **Utan tilläggslast vid T_tr, Ei-stadiet:** x₀ = roten med c = K1 · Ei / σ₀² − σ₀ + α · (T_tr − T₀) · Ei och D = K1 · Ei.
   Är x₀ > σp anropas `EIV`, men med de globala `i` = 8 och `j` = 1 kvar från slingan. Temperaturen och lasten i lastfall 8 i Ei är tomma, alltså 0 (4.2, D4). E4 och x₀ blir resultatet av den räkningen.
2. **E4** är den globala variabeln: Ei, eller E4 från det **sista** Ei-lastfallet (1–7, i ordning) där Eiv räknades, eller E4 från punkt 1. Den behöver inte höra till transponeringslastfallet (4.2, D1).
3. **Relativ längd:** L = K1 / x₀² − x₀ / E4.
4. **Transponeringslasten:** y = roten med c = L · E4 · A / (Q_tr · aₙ) och D = E4 · A / (24 · Q_tr · aₙ). y = σ_tr · A / (Q_tr · aₙ), där σ_tr är spänningen i transponeringslastfallet.
5. **Ep-stadiet:** K2 = (Ep − E4) · Q_tr · aₙ / (Ep · E4 · A). σ_p0 = roten med c = Ep · (L + α · Tp + K2 · y) och D = K1 · Ep.
   K2 · y = σ_tr/E4 − σ_tr/Ep är skillnaden i elastisk töjning vid σ_tr, och α · Tp är krypningen.
6. **Ep-stadiets referens** är (σᵣ, Tᵣ, E) = (σ_p0, T_tr, Ep). Lastfall 8–15 räknas med 3.3.
   Eiv räknas i Ep bara om Ep = Ei, eftersom villkoret är `If e = ei` (4.2, D5).

### 3.6 Utdata (`Linber_Modul2_20151109.RESUT`)
För varje lastfall, med σᵢ ur 3.3–3.5:

| Kolumn | Värde |
|---|---|
| U, V | Uᵢ och Vᵢ |
| W (b) | (qe + Uᵢ) · aₙ² / (8 · σᵢ · A) |
| X (σ) | σᵢ |
| Y (F) | σᵢ · A |
| Z (Mall) | (2000 / aₙ)² · b, alltså nedhänget omräknat till ett spann på 2000 m |
| AA | F / (fu · 1000) · 100, eller "-" när brottlasten är 0. Röd när värdet > O16 |
| AC (b) | Qᵢ · aₙ² / (8 · σᵢ · A) |
| AD (α) | Atn(Vᵢ / (Uᵢ + qe)) · 180 / π, med arkets π |
| AE (Mall) | Round((2000 / aₙ)² · AC), VBA:s avrundning till jämnt |

Raden skrivs bara om F ≠ 0. `indata` räknar också B0, F0 och MALL0 (nedhäng, linkraft och mall vid σ₀ med egentyngd), men de visas inte.

Z i lastfall 15 är **stakningsmallen**. Tillägget skickar den vidare som "stakmall" till Isolatorutslag och avrundad till Lastfallsdiagram. Den visas i Dokumentation (beslut 7).

### 3.7 Ordningen i en omräkning
1. `exeLin`/`indata`: läser J3–J18 och O9–O11. Ep = 0 blir 1. T₀ tom blir 0. Brottlasten "-" eller tom blir 0. E = Ei och E4 = Ei.
2. `lastfall` för Ei (rad 23–29), i ordning 1–7.
3. `berakning` för Ei: lastfall 1–7 med (σ₀, T₀, Ei) och Eiv.
4. `calcEP`: `transp` (3.5).
5. `lastfall` för Ep (rad 31–38), i ordning 8–15.
6. `berakning` för Ep.

Ett lastfall som i L pekar på en **senare** rad får R från förra omräkningen (4.1, punkt 9).

### 3.8 2015 och 2016
Linberäkning.xlsm använder `…_20151109` och Sammanslagning.xlsm `…_20160421`. Jämförelsen:
- `lastfall` och `berakning` är identiska utom modulnamnen.
- `transp`, `EIV` och `TREDJEGRAD` räknar likadant.
- `Public i As Integer, j As Integer` deklareras i `Linber_Modul1_20160421`, och 2015-modulerna använder samma globala `i` och `j` (de saknar egen deklaration).
- 2016 hanterar inte brottlasten "-" i `indata` (`CDbl` ger körfel), och `indata_smsl` saknar `If ep = 0 Then ep = 1`.
- Sammanslagningen har en egen lastfallsmatris (`lastfallsamman_160421`, 24 lastfall, 48 i Ep). Den ingår inte.

---

## 4. Fel och egenheter i VBA-koden

### 4.1 Lika i båda lägena
| Nr | Egenhet | VBA | Kalkyl |
|---|---|---|---|
| 1 | Cardano ger inte exakt roten | X29 = 90,000000001010662 i stället för 90 | Samma formler och samma ordning (5.6). Ingen Newton-polering |
| 2 | Arkets π = 3,1416 i ACOS-steget | Vid Z < 0 blir F fel med (3,1416 − π)/2 ≈ 3,7e-6 rad. Slaka linor (K1 · E > ca 2,6 · σ₀³) får upp till 1,6e-5 relativt fel i σ | π ur O10, förval 3,1416, i samma formler som VBA: Iᵥ, T, AD och ACOS (beslut 4). T14 och T15 visar effekten |
| 3 | Eiv utan gräns | Ingen gräns för antalet varv | Högst 100 varv, sedan fel. E4 ≤ 0 är fel |
| 4 | L saknas på en rad med is utan vind | Typfel, makrot stannar | Fel med text (5.5) |
| 5 | Isvikt vid vindstilla | Bara Gc, inte Gq, Cc eller N. Basen är R i lastfallet L plus egna Qᵢ · (hᵢ − Pᵢ) | Samma |
| 6 | Villkoret för vindstilla | qᵢ = 0 efter faktorerna, inte Nᵢ = 0 | Samma |
| 7 | Motsvarande istjocklek | Räknas ur Iₛ också på rader med vind och is | Samma |
| 8 | Fel i lindata eller indata | Meddelande men räknar ändå, eller division med noll | Validering (5.5). Fel ger inget resultat |
| 9 | Ordningsberoende referens | L som pekar framåt läser förra omräkningens R. Övre-mallen räknar därför två gånger | R räknas för alla lastfall före lasterna. Resultatet blir som efter Excels andra omräkning. Excels första omräkning beror på historiken och går inte att återskapa |
| 10 | Brottlast saknas | "-" i AA | `utilizationPercent` tom och en varning |
| 11 | Mall i AE | VBA `Round` avrundar till jämnt | Biblioteket ger det oavrundade talet. Visningen avrundar |
| 12 | Tom benämning | Raden räknas ändå. Utskriften slutar vid första tomma benämning | Raden räknas. Fas 2 visar den dämpad |
| 13 | Höjd M | Visar h₀ också när h₀ < Pᵢ | Samma |
| 14 | Lindata Tp | J16 är kolumn L i Lindata, inte K/J | `creepTempC`, inte `creepStrainPermille` |
| 15 | Återställ | Sätter J6 = −40 och J7 = 60. Återställer inte O15 och inte J3–J5 | Samma |
| 16 | Manuell last | Tas bort bara med mallknapparna. J, K och M–T töms | Flagga per lastfall. Koder och vindindata finns kvar men används inte. Tas bort med en snabbmeny (fas 2) eller en mall |
| 17 | Övre linan | Avbryt i frågan ger 0 °C | Fönstret avbryter vid Avbryt (fas 2) |

### 4.2 Avvikelser mellan lägena Excel och Ny
Läget Excel räknar som arket. Läget Ny rättar felen. Varje rad har ett eget test som visar skillnaden (6.1 och 6.2).

| Nr | Läget Excel (som VBA) | Läget Ny | Test |
|---|---|---|---|
| D1 | E4 i transponeringen är E4 från det sista Ei-lastfallet med Eiv, inte nödvändigtvis transponeringslastfallets (3.5, punkt 2). Varning | E4 från transponeringslastfallet tr om det fick Eiv, annars Ei (utom D4) | A, T2-data |
| D2 | L som pekar till det andra stadiet läser arkets rad 22 + L (Ei) eller 23 + L (Ep). Från Ei blir ID 8 en tom rad (R = 0) och ID 9–15 lastfall L − 1. Från Ep blir ID 7 en tom rad och ID 1–6 lastfall L + 1. Varning | L är lastfallets ID, 1–15 | A |
| D3 | L som pekar på ett lastfall med manuell last ger R = 0 (Excel har tömt R). Varning | R räknas ur det lastfallets vindindata, som Kalkyl behåller | A |
| D4 | x₀ > σp i transponeringen: Eiv räknas med temperaturen 0 och lasten 0 (tomma värden), och x₀ och E4 blir resultatet. D = 0 i ekvationen, och svaret beror på avrundningen. Varning, eller fel om Eiv inte konvergerar | Eiv räknas för det rätta tillståndet: T_tr och lasten qe, med referensen (σ₀, T₀). x₀ och E4 blir resultatet. Varning | A |
| D5 | Ep = Ei: Eiv räknas också i Ep-stadiet. Varning när det händer | Ingen Eiv i Ep-stadiet | A, T5-data |
| D6 | `ACOS(±1)` = π/2 | `ACOS(1)` = 0 och `ACOS(−1)` = π (O10) | A |
| D7 | Min, max och vindtryck kopieras som tal till fasta rader när J6, J7, O4 eller O5 ändras (2.4), också när en korsningsmall är inläst. Lastfall i en korsningsmall som följer min eller max behåller värdet från när knappen trycktes | Varje lastfall följer sin källa (5.3). Mallens rader följer J6, J7, Tks, O4 och O5 | B |
| D8 | Korsningsmallarna skriver talet 500 N/m² i lastfall 6 (nedre) och 1 och 13 (övre), inte O4 | Källan Normal (O4) | B |
| D9 | En temperatur som ändrats för hand i lastfall 1, 2 eller 15 skrivs över när J6 eller J7 ändras, och O-kolumnen skrivs över när O4 eller O5 ändras | En ändrad cell blir Fixed och skrivs inte över | B |
| D10 | Arkets π (O10, 3,1416) i Iᵥ, T, AD och ACOS | Exakt π (`std::numbers::pi`). O10 läses inte, och cellen visas låst som π. Tillagt 2026-11-25 på användarens beslut under handtestet | A (`checkT1New`) |
| D11 | Övre linan avrundar kortslutningstemperaturen till heltal (VBA `Integer`, till jämnt) | Temperaturen behålls. Tillagt efter granskningen av fas 1 | B (`testModeD11`) |

Med standardmallen, utan korsningsmallar och utan manuella ändringar ger lägena samma resultat, utom vid D1, D4 och D5. T1 ger samma resultat i båda lägena.

---

## 5. Biblioteken (fas 1)

### 5.1 Plats, namn och beroenden
**Regel (beslut 8):** varje bibliotek har en enda uppgift. Det blir tre bibliotek där i dag `libs/calc` finns:

| Katalog | Mål | Namnrymd | Include | Uppgift | Beror på |
|---|---|---|---|---|---|
| `libs/anCalc` | `kalkyl_ancalc` | `kalkyl::ancalc` | `"anCalc/normal_span.h"` | Normalspannsberäkningen | `tableText` |
| `libs/tableText` | `kalkyl_tabletext` | `kalkyl::tabletext` | `"tableText/table_text.h"` | Tal med decimalkomma, tabbseparerat urklipp och inklistring | standardbiblioteket |
| `libs/lineCalc` | `kalkyl_linecalc` | `kalkyl::linecalc` | `"lineCalc/line_calc.h"` | Linberäkningen och lastfallsmallarna | `conductor`, `tableText` |

- **Katalognamnen** skrivs exakt så, med versal i mitten (användarens mönster). Målen och namnrymderna skrivs med gemener, som `kalkyl_systemdb` och `kalkyl::systemdb`.
- **Beroendena går åt ett håll:** `tableText` och `conductor` beror bara på standardbiblioteket. `anCalc` använder `formatDecimal` i kontrolltexterna och länkar `kalkyl_tabletext` PRIVATE. `lineCalc` länkar `kalkyl_conductor` PUBLIC, eftersom `LineInput` innehåller en `conductor::Conductor`, och `kalkyl_tabletext` PRIVATE för talen i felmeddelandena.
- **`libs/CMakeLists.txt`** får ordningen `version`, `core`, `conductor`, `tableText`, `anCalc`, `lineCalc`, `platform`, `db`, `systemdb`, `projectdb`, `appearance`.
- **Ingen `core`:** som `calculateNormalSpan` svarar `calculateLine` med resultat och en lista med fel och varningar (`LineCalculation`), inte med `Result<T>`. Fönstret behöver alla fel med lastfall och fält på en gång för att markera celler.
- **Inget delat issue-slag:** `lineCalc` har sin egen `LineIssueKind` och beror inte på `anCalc`.
- **Dokumentationens nycklar** `ns_calc_*` och `line_calc_*` i projektfilerna ändras inte. De är data, inte kodnamn.

### 5.2 Linan: `Conductor` → arkets J9–J18
| Arket | `Conductor` | I beräkningen |
|---|---|---|
| J2 | `name` | visas, får vara tom när lindatat är inskrivet för hand |
| J9 A | `areaMm2` | mm² |
| J10 d | `diameterMm` | mm |
| J11 Eik | `eInitialMpa` | MPa |
| J12 Ep | `ePermanentMpa` | MPa |
| J13 σp | `eInitialLimitMpa` | MPa |
| J14 k | `eivConstant` | – |
| J15 α | `alphaE6PerC` | · 10⁻⁶ |
| J16 Tp | `creepTempC` | °C |
| J17 qe | `weightNPerM` | N/m |
| J18 fu | `breakingLoadKn` | · 1000 N, tom = "-" |
| (används inte) | `type`, `creepStrainPermille`, `id`, `origin` | – |

Typen saknar inget fält. Lindatat ligger i `LineInput::conductor` och kan ändras fritt efter att en lina valts (beslut 5). Är det tomt (`nullopt`) finns varken lina eller lindata. `validateConductor` används, utom regeln om namnet. Den kräver redan Ep > 0, så Excels `If ep = 0 Then ep = 1` behövs inte.

### 5.3 `libs/lineCalc/line_calc.h` (ny, bindande)
```cpp
#pragma once
#include <array>
#include <optional>
#include <string>
#include <vector>

#include "conductor/conductor.h"

namespace kalkyl::linecalc {

// Antalet lastfall i arket Linberäkning: ID 1-7 i Ei-stadiet (rad 23-29) och
// ID 8-15 i Ep-stadiet (rad 31-38). LineInput::cases[id - 1] är lastfall id.
inline constexpr int kLoadCaseCount = 15;
inline constexpr int kEiCaseCount = 7;

// Hur beräkningen räknar (planen 4.2). Excel räknar som arket med dess fel, New rättar
// felen. Läget sparas per beräkning. Excel ska kunna tas bort utan att New ändras.
enum class CalcMode { Excel, New };

// Stadiet för ett lastfall (kolumn I): Ei före transponeringen och Ep efter.
// Stadiet följer av ID och kan inte ändras.
enum class LineStage { Ei, Ep };

// Var lastfallets temperatur kommer ifrån i läget New (planen 2.5). Fixed är talet i
// LoadCase::tempC. De andra följer indata: Min = J6, MinPlus20 = J6 + 20, Max = J7,
// ShortCircuit = kortslutningstemperaturen och ShortCircuitRise = J6 + (Tks - J7).
enum class TempSource { Fixed, Min, MinPlus20, Max, ShortCircuit, ShortCircuitRise };

// Var det karakteristiska vindtrycket (kolumn O) kommer ifrån i läget New: talet i
// LoadCase::charPressureNPerM2, Normal vind (O4) eller Hög vind (O5).
enum class PressureSource { Fixed, Normal, High };

// Is- och vindlast inskrivna för hand (arkets kolumn B = 1 med U och V).
// Lastfallet räknas då inte ur vind och is.
struct ManualLoad {
    double iceNPerM = 0;    // U, I
    double windNPerM = 0;   // V, Qwc
};

// Ett lastfall: en rad i arket, kolumn G-S. Resultaten står i LoadCaseResult.
// tempC och charPressureNPerM2 är cellernas tal, som läget Excel räknar med.
// Läget New räknar med källorna och använder talen bara när källan är Fixed.
struct LoadCase {
    TempSource tempSource = TempSource::Fixed;
    double tempC = 0;                            // G
    std::string name;                            // H, tom = lastfallet används inte
    std::string standardCase;                    // J, SS-EN 50341 Standard load case
    std::string clearanceCase;                   // K, Load case for clearances
    std::optional<int> windFromCase;             // L, lastfall-ID 1-15
    double windMagnitudePercent = 0;             // N
    PressureSource pressureSource = PressureSource::Fixed;
    double charPressureNPerM2 = 0;               // O
    double refHeightM = 0;                       // P, href
    double pressureGradientNPerM3 = 0;           // Q, dqh
    double iceThicknessMm = 0;                   // S
    std::optional<ManualLoad> manual;            // B = 1
};

// Konstanterna i arket (O9-O17) med arkets förval. pi används i båda lägena där VBA
// använder arkets pi (planen 4.1, punkt 2).
struct LineConstants {
    double iceDensityKgM3 = 917.4;       // O9
    double pi = 3.1416;                  // O10
    double gravity = 9.81;               // O11
    double gustFactor = 1;               // O12, Gq
    double resonanceFactor = 0.5;        // O13, Gc
    double dragFactor = 1;               // O14, Cc
    double minIceNPerM = 20;             // O15
    double maxUtilizationPercent = 55;   // O16
    int transpositionCase = 5;           // O17, lastfall-ID 1-7
};

// Indata till en linberäkning: läget, arkets J2-J18, O3-O17 och de 15 lastfallen.
// Tomt lindata, normalspann eller uppspänning ger ingen beräkning (planen 5.5).
// En ny beräkning skapas med newLineInput() i load_cases.h.
struct LineInput {
    CalcMode mode = CalcMode::New;
    std::optional<conductor::Conductor> conductor;   // J2, J9-J18; tomt = inget lindata
    std::optional<double> normalSpanM;               // J3, an
    std::optional<double> tensionMpa;                // J4, sigma0
    double refTempC = 0;                             // J5, T0
    double minTempC = -50;                           // J6
    double maxTempC = 50;                            // J7
    double shortCircuitTempC = 100;                  // korsningsmallen Övre linan
    double lineHeightM = 25;                         // O3
    double normalWindNPerM2 = 500;                   // O4
    double highWindNPerM2 = 800;                     // O5
    LineConstants constants;
    std::array<LoadCase, kLoadCaseCount> cases;
};

// Resultatet för ett lastfall: arkets beräknade kolumner M, R, T, U-AA och AC-AE.
// Talen är oavrundade. Visningen avrundar enligt arkets format (planen 2.1).
struct LoadCaseResult {
    int id = 0;                                  // 1-15
    LineStage stage = LineStage::Ei;
    double tempC = 0;                            // G som räkningen använde
    double charPressureNPerM2 = 0;               // O som räkningen använde
    std::optional<double> heightM;               // M; tom vid manuell last
    std::optional<double> pressureNPerM2;        // R; tom vid manuell last
    std::optional<double> iceEquivalentMm;       // T; tom vid manuell last
    double iceNPerM = 0;                         // U, I
    double windNPerM = 0;                        // V, Qwc
    double resultantNPerM = 0;                   // Q i planen 3.3 (inte i arket)
    double modulusMpa = 0;                       // E i lösningen: Ei, Eiv eller Ep
    double sagM = 0;                             // W, b
    double stressMpa = 0;                        // X, sigma
    double forceN = 0;                           // Y, F
    double mall = 0;                             // Z
    std::optional<double> utilizationPercent;    // AA; tom = "-"
    bool overUtilization = false;                // AA röd: över O16
    double inclinedSagM = 0;                     // AC
    double swingAngleDeg = 0;                    // AD
    double inclinedMall = 0;                     // AE, oavrundad
};

// Resultatet för alla lastfall och transponeringens mellanvärden (planen 3.5).
// cases[id - 1] är lastfall id. stakingMall är Z i lastfall 15 (beslut 7).
struct LineResult {
    CalcMode mode = CalcMode::New;
    std::array<LoadCaseResult, kLoadCaseCount> cases;
    double k1 = 0;                               // K1, MPa²
    double unloadedTranspositionStressMpa = 0;   // x0 som L räknades med
    int transpositionEivCase = 0;                // lastfallet som gav E4; 0 = Ei eller D4
    double transpositionModulusMpa = 0;          // E4
    double transpositionStressMpa = 0;           // sigma_tr = y * Q_tr * an / A
    double epReferenceStressMpa = 0;             // sigma_p0
    double epReferenceTempC = 0;                 // T_tr
    double stakingMall = 0;                      // Z i lastfall 15
};

// Fältet som en upplysning gäller. Fönstret översätter fältet och lastfallet till
// en cell. Result gäller själva räkningen i ett lastfall.
enum class LineField {
    None, Conductor, NormalSpan, Tension, RefTemp, MinTemp, MaxTemp, ShortCircuitTemp,
    LineHeight, NormalWind, HighWind, IceDensity, Pi, Gravity, GustFactor, ResonanceFactor,
    DragFactor, MinIce, MaxUtilization, TranspositionCase, CaseTemp, CaseWindFrom,
    CaseMagnitude, CasePressure, CaseRefHeight, CaseGradient, CaseIce, CaseManualIce,
    CaseManualWind, Result
};

// Slaget av upplysning. Missing: indata saknas, inget resultat, visas neutralt.
// Error: fel, inget resultat. Warning: resultatet finns men något bör ses över.
enum class LineIssueKind { Missing, Error, Warning };

// En upplysning. caseId 0 gäller hela beräkningen, annars lastfall 1-15.
// message är svensk text till användaren (planen 5.5).
struct LineIssue {
    LineIssueKind kind = LineIssueKind::Error;
    int caseId = 0;
    LineField field = LineField::None;
    std::string message;
};

// Svaret från calculateLine: resultatet när varken Missing eller Error finns, och alla
// upplysningar i ordningen hela beräkningen, sedan lastfall 1-15.
struct LineCalculation {
    std::optional<LineResult> result;
    std::vector<LineIssue> issues;
};

// Stadiet för lastfall id (1-15): Ei för 1-7, Ep för 8-15.
// id utanför 1-15 är ett programfel och ger Ep.
LineStage stageOf(int id);

// Lastfallets temperatur och karakteristiska vindtryck i input.mode: talet i läget
// Excel, källan i läget New (planen 2.5 och 4.2, D7-D9).
double caseTempC(const LineInput& input, int id);
double caseCharPressureNPerM2(const LineInput& input, int id);

// ACOS i Modul3 med arkets pi: -atan(y / sqrt(1 - y²)) + pi / 2. Vid y = 1 eller -1
// ger läget Excel pi / 2 och läget New 0 eller pi (planen 4.2, D6). |y| > 1 ger NaN.
double vbaAcos(double y, double pi, CalcMode mode);

// En rot till x^3 + c*x^2 - d = 0, räknad som VBA:s TREDJEGRAD (planen 3.3 och 5.6).
// pi och mode gäller ACOS-steget. Den icke-negativa rot som ligger närmast sigmaRef väljs
// i den trigonometriska grenen. Svarar NaN om svaret inte är ändligt.
double solveStateCubic(double c, double d, double pi, double sigmaRef, CalcMode mode);

// Linberäkningen enligt arket Linberäkning (planen avsnitt 3) i input.mode, med
// avvikelserna i 4.2. Missing och Error ger inget resultat, Warning ger resultat.
// Funktionen är ren och kastar inte.
LineCalculation calculateLine(const LineInput& input);

} // namespace kalkyl::linecalc
```

### 5.4 `libs/lineCalc/load_cases.h` (ny, bindande)
```cpp
#pragma once
#include "lineCalc/line_calc.h"

namespace kalkyl::linecalc {

// En ny beräkning i läget mode (beslut 6): ingen lina och inget lindata, inget normalspann
// och ingen uppspänning, T0 = 0, min -50 °C, max +50 °C, kortslutningstemperaturen 100 °C,
// konstanterna med arkets förval och lastfallen enligt planen 2.5.1 med min och max ovan.
LineInput newLineInput(CalcMode mode = CalcMode::New);

// Knappen "Återställ indata enl. SS-EN 50341-2-18" (reset_Swedish_standard): linhöjd,
// vindtryck, J6 = -40, J7 = 60, konstanterna utom O15 och alla 15 lastfall enligt planen
// 2.5.1, utan manuell last. Läget, lindatat, an, sigma0, T0 och O15 behålls.
void resetToStandard(LineInput& input);

// Knappen "Nedre linan" (blad1_Korsningslastfall_Nedre): O17 = 5 och lastfallen enligt
// planen 2.5.2, utan manuell last. Övriga indata och konstanter behålls.
void loadCrossingLower(LineInput& input);

// Knappen "Övre linan" (blad1_Korsningslastfall_Ovre): O17 = 3, kortslutningstemperaturen
// och lastfallen enligt planen 2.5.3, utan manuell last. Fönstret frågar efter
// temperaturen (förval 100 °C) och anropar inte funktionen vid Avbryt.
void loadCrossingUpper(LineInput& input, double shortCircuitTempC);

// Det indata som ändrats i applyInputChange: J6, J7, kortslutningstemperaturen, O4 eller O5.
// Fönstret anropar funktionen efter att ha skrivit det nya värdet i input.
enum class InputChange { MinTemp, MaxTemp, ShortCircuitTemp, NormalWind, HighWind };

// Arkets följdändringar (planen 2.4). Läget Excel skriver talen som arkets händelser:
// J6 till lastfall 1 och 2, J7 till lastfall 15 och O4/O5 till hela O-kolumnen.
// Läget New skriver det nya värdet i talen på de lastfall vars källa följer det.
void applyInputChange(LineInput& input, InputChange change);

} // namespace kalkyl::linecalc
```

### 5.5 Upplysningar (texter ordagrant)
Talen i texterna skrivs med `tabletext::formatDecimal(x, 1)` (decimalkomma). N, M och K står för lastfallens ID. Läge "båda" betyder att upplysningen gäller i båda lägena.

| Fall | Läge | kind | caseId, field | message |
|---|---|---|---|---|
| Inget lindata (`conductor` tom) | båda | Missing | 0, Conductor | "Lina eller lindata saknas." |
| aₙ saknas | båda | Missing | 0, NormalSpan | "Normalspannet saknas." |
| σ₀ saknas | båda | Missing | 0, Tension | "Uppspänningen saknas." |
| `validateConductor` ger fel, utom om namnet (ett per fel) | båda | Error | 0, Conductor | "Lindata: " + felets text |
| Brottlast saknas | båda | Warning | 0, Conductor | "Linan saknar brottlast. Utnyttjandegraden visas som -." |
| aₙ ≤ 0 | båda | Error | 0, NormalSpan | "Normalspannet måste vara större än 0." |
| σ₀ ≤ 0 | båda | Error | 0, Tension | "Uppspänningen måste vara större än 0." |
| Ett tal som inte är ändligt (alla fält, också källornas temperaturer) | båda | Error | 0 eller id, fältet | "Ogiltigt tal." |
| h₀, O4, O5, ρ, Gq, Gc, Cc eller Imin < 0 | båda | Error | 0, fältet | "Värdet måste vara 0 eller större." |
| π, g eller O16 ≤ 0 | båda | Error | 0, fältet | "Värdet måste vara större än 0." |
| O17 inte 1–7 | båda | Error | 0, TranspositionCase | "Lastfallet för transponering måste vara ett av lastfallen 1–7." |
| Nᵢ, Oᵢ, Pᵢ, Sᵢ eller manuell last < 0 | båda | Error | id, fältet | "Värdet måste vara 0 eller större." |
| L utanför 1–15 | båda | Error | id, CaseWindFrom | "Lastfall-ID för vindparametrarna måste vara 1–15." |
| L saknas där qᵢ = 0 och Sᵢ ≠ 0 | båda | Error | id, CaseWindFrom | "Lastfallet har is utan vind och behöver ett lastfall-ID för vindparametrarna." |
| L pekar till det andra stadiet (D2) | Excel | Warning | id, CaseWindFrom | "Lastfall-ID M pekar till det andra stadiet. Excel läser då lastfall K." eller, vid den tomma raden, "… Excel läser då en tom rad, och vindtrycket blir 0." |
| L pekar på ett lastfall med manuell last (D3) | Excel | Warning | id, CaseWindFrom | "Lastfall M har manuell last. Excel läser då inget vindtryck, och vindtrycket blir 0." |
| Eiv konvergerar inte på 100 varv | båda | Error | id, Result | "Eiv konvergerar inte." |
| E4 ≤ 0 | båda | Error | id, Result | "Eiv blir 0 eller negativ vid σ = x MPa." |
| Ingen ändlig positiv rot i ett lastfall, eller x₀ ≤ 0 | båda | Error | id, Result | "Tillståndsekvationen saknar lösning." |
| x₀ > σp (D4) | Excel | Warning | tr, Result | "Spänningen utan tilläggslast vid transponeringstemperaturen blir x MPa, över σp. Excel räknar då Eiv med tomma värden (temperatur 0 och last 0), och resultatet beror på avrundningen." |
| x₀ > σp (D4) | New | Warning | tr, Result | "Spänningen utan tilläggslast vid transponeringstemperaturen blir x MPa, över σp. Eiv räknas för det tillståndet." |
| E4 kommer från ett annat lastfall än tr (D1) | Excel | Warning | tr, Result | "Transponeringen använder Eiv från lastfall M, som Excel, inte från lastfall N." |
| Eiv räknades i Ep-stadiet (D5) | Excel | Warning | 0, Result | "Ep = Ei, och Excel räknar då Eiv också i Ep-stadiet (lastfall N, M)." |
| Utnyttjandegraden över O16 | båda | Warning | 0, Result | "Utnyttjandegraden är över x % i lastfall N, M." |

- Missing och valideringens fel samlas alla innan något räknas. Fel ur själva räkningen (Eiv, rot) stoppar beräkningen vid första felet.
- Listor med lastfall skrivs i ordning, skilda med komma och mellanslag.

### 5.6 Implementation (paket A och B)
**Lägena isoleras** i `line_calc.cpp` med en regelstruktur i den anonyma namnrymden. Den är det enda stället som läser `CalcMode`, utom `caseTempC`, `caseCharPressureNPerM2`, `vbaAcos` och `applyInputChange`:
```cpp
// Excels egenheter som flaggor, en per avvikelse i planen 4.2. rulesFor(CalcMode::New)
// ger alla false. När läget Excel tas bort försvinner strukturen och grenarna den styr.
struct ModeRules {
    bool excelWindRows = false;          // D2: L läses som arkets rad
    bool manualRefZero = false;          // D3: L till manuell last ger R = 0
    bool lastEivModulus = false;         // D1: E4 från sista Ei-lastfallet med Eiv
    bool emptyTranspositionEiv = false;  // D4: Eiv med temperatur 0 och last 0
    bool eivWhenEpEqualsEi = false;      // D5: Eiv också i Ep när Ep == Ei
};
ModeRules rulesFor(CalcMode mode);
```
- D6 ligger i `vbaAcos`, D7–D9 i `caseTempC`/`caseCharPressureNPerM2` (talen eller källan) och i `applyInputChange`.
- Varje gren som gäller läget Excel har en kort kommentar med avvikelsens nummer, till exempel `// D2: Excel läser arkets rad 22 + L`.
- **Att ta bort läget Excel** blir då: ta bort `CalcMode::Excel`, `ModeRules`, grenarna med D-nummer, Excel-grenen i `applyInputChange` och facittestets jämförelse med Excel. Fälten `tempC` och `charPressureNPerM2` blir kvar för Fixed.

**`calculateLine`** (A), i den här ordningen:
1. Missing och validering enligt 5.5. Finns Missing eller Error: svara med upplysningarna och utan resultat.
2. Lös upp Tᵢ och Oᵢ för alla 15 lastfall med `caseTempC` och `caseCharPressureNPerM2`.
3. **Första varvet:** Rᵢ för alla lastfall utan manuell last (3.2 punkt 1–2). För ett lastfall med manuell last ger D3 R = 0 i läget Excel och R ur dess vindindata i läget New.
4. **Andra varvet:** Vᵢ, Uᵢ, Tᵢ och M för alla lastfall (3.2 punkt 3–10). Lastfallet som L pekar på väljs enligt D2.
5. K1, sedan lastfall 1–7 med (σ₀, T₀, Ei) och Eiv (3.3–3.4). Spara E4 och lastfallets ID varje gång Eiv räknas.
6. Transponeringen (3.5) med E4 enligt D1 och x₀ > σp enligt D4.
7. Lastfall 8–15 med (σ_p0, T_tr, Ep). Eiv i Ep enligt D5.
8. Utdata (3.6), `stakingMall` och varningarna. Ett lastfall med σᵢ ≤ 0 eller NaN ger felet "Tillståndsekvationen saknar lösning."

**`solveStateCubic`** (A) följer `TREDJEGRAD` ordagrant:
- `p = -c * c / 3`, `q = 2 * std::pow(c, 3) / 27 - d` och `z = q * q / 4 + std::pow(p, 3) / 27`, i VBA:s ordning.
- Kubikroten är `std::pow(std::fabs(v), 1.0 / 3.0)` med tecknet från v, och sgn(0) = 0. Inte `std::cbrt`, så att talen blir som VBA:s `^ (1 / 3)`.
- Vid z ≥ 0 är svaret u + v − c/3, också när det är negativt (D = 0 i D4).
- Vid z < 0: y = −q / 2 / √(−p³/27) och F = `vbaAcos(y, pi, mode)`. X1, X2 och X3 räknas som i 3.3. Negativa blir −1e25, och den rot som ligger närmast `sigmaRef` väljs. Är avstånden lika eller finns ingen rot ≥ 0 svarar funktionen NaN.
- Med d > 0 blir svaret alltid X1 (3.3), så `sigmaRef` påverkar bara D4.

**Uttrycken** skrivs i samma ordning som VBA, till exempel `K1 / (sr * sr) * e - sr + alpha * (t - tr) * e`. Skillnader i sista siffran ryms i toleranserna (6.5).

**Källorna** (A) i `caseTempC`: läget Excel ger `tempC`. Läget New ger Fixed → `tempC`, Min → J6, MinPlus20 → J6 + 20, Max → J7, ShortCircuit → Tks och ShortCircuitRise → J6 + (Tks − J7). `caseCharPressureNPerM2` på samma sätt: läget Excel ger talet, och läget New ger Fixed → talet, Normal → O4 och High → O5.

**Mallarna** (B):
- Skriver tabellerna i 2.5 rad för rad, med källorna och med talen som makrot skriver (2.5).
- Benämningarna och koderna är ordagranna, också mellanslagen. `windFromCase` är tom där L är tom, och `manual` blir tom på alla rader.
- Gör samma sak i båda lägena.

**`applyInputChange`** (B):
- **Läget Excel:**
  - MinTemp: lastfall 1 får J6 och lastfall 2 får J6 + 20.
  - MaxTemp: lastfall 15 får J7.
  - NormalWind och HighWind: alla 15 lastfall får O-talet enligt 2.4.
  - ShortCircuitTemp: ingenting.
  - Källorna ändras inte.
- **Läget New:** de lastfall vars källa följer det ändrade värdet får det upplösta värdet i talet, så att talen stämmer med källorna. Fixed ändras inte.

---

## 6. Tester

Testerna följer `ancalc_test.cpp` (i dag `calc_test.cpp`): egen `CHECK`, `near(a, b, tol)`, ingen testram, utskrift till `std::cerr` och exit-kod 1 vid fel. Data ligger i testfilerna eller i `linecalc_facit.h`, aldrig i `ref/` (som är gitignorerad).

### 6.1 `libs/lineCalc/linecalc_test.cpp` (ctest `linecalc`, paket A)
Testet bygger T1:s indata själv (standardmallens 15 rader skrivs ut i en hjälpfunktion i testet), så att A inte väntar på B.
1. **`solveStateCubic`, Cardano:** K1 = 7,647955258454736 (T1), E = 89000, c = K1 · E / 90² − 90, d = K1 · E, pi = 3,1416 och sigmaRef = 90 ger 90,000000001010662 med relativ tolerans 1e-15.
2. **Trigonometrisk gren:** c = 291,13235456912173, d = 680668,0180024715 och sigmaRef = 45 (σ₀ = 45) ger 44,999838055875486 med pi = 3,1416 och 45 ± 1e-12 med `std::numbers::pi`.
3. **Negativt c** och d > 0 ger en positiv rot som uppfyller ekvationen (residual < 1e-9 · d).
4. **T1 i båda lägena:** σ för alla 15 lastfall och U, V, R och T för lastfall 4, 5, 8 och 9 mot bilaga A, relativ tolerans 1e-9. AD för lastfall 2 och 6. `stakingMall` = Z38.
5. **Missing:** `newLineInput`-liknande indata utan lindata, aₙ och σ₀ ger de tre Missing-upplysningarna och inget resultat. Lindata utan namn räknas.
6. **Manuell last:** T1 med lastfall 4 manuellt (I = 16,743530380610878, Qwc = 12,725) ger X26 som i T1. M, R och T är tomma i lastfall 4.
7. **L framåt:** T1 med L = 6 i lastfall 5 ger U i lastfall 5 = √((890 · 0,5 · 50,9 / 1000)² + (5,34 + 16,743530380610878)²) − 5,34 i båda lägena.
8. **Fel:** varje fel i 5.5 minst en gång, bland annat aₙ = 0, σ₀ = 0, O17 = 8, L saknas, L = 16, Sᵢ = −1, ett NaN och k = 1e6 (Eiv-fel i lastfall 4).
9. **Brottlast saknas:** `utilizationPercent` tom och varningen finns.
10. **Utnyttjandegrad:** T4:s indata ger `overUtilization` i lastfall 4, 5, 8 och 9 och varningen.
11. **Källorna:** `caseTempC` för alla sex källor och `caseCharPressureNPerM2` för alla tre, i båda lägena (läget Excel ger talen).

**Ett test per avvikelse** (namnen är funktionerna i testfilen):
- **`testModeD1`:** T2:s indata. Excel: `transpositionEivCase` = 5, E4 = 85875,27 ± 0,01, varningen. New: `transpositionEivCase` = 4, E4 ≈ 85655 (avrundat till heltal). σ i lastfall 15 skiljer sig.
- **`testModeD2`:** T1 med L = 6 i lastfall 9. Excel läser lastfall 7 (R = 0), så U9 = max(Iₛ med bas 0, 20) = 20 med varningen. New ger U9 = √((890 · 0,5 · 50,9 / 1000)² + (5,34 + 16,743530380610878)²) − 5,34. Dessutom L = 9 i lastfall 5: Excel läser lastfall 8 (R = 500), New lastfall 9 (R = 0).
- **`testModeD3`:** T1 med lastfall 4 manuellt (som punkt 6) och L = 4 i lastfall 5. Excel: R = 0, U5 = 20 och varningen. New: R = 500, U5 = 20,147407460770967.
- **`testModeD4`:** T1:s lina med σ₀ = 170 och T₀ = 20 (x₀ = 194,8 MPa > σp). New: resultat, varningen, och E4 = Ei − k · (x₀ − σp) inom 10 MPa. Excel: varningen och antingen ett resultat som skiljer sig från New eller Eiv-felet.
- **`testModeD5`:** T5:s lina och indata. Excel: `modulusMpa` < Ep i lastfall 8 och varningen. New: `modulusMpa` = Ep i lastfall 8–15.
- **`testModeD6`:** `vbaAcos(1, 3.1416, Excel)` = 1,5708, `vbaAcos(1, 3.1416, New)` = 0, `vbaAcos(-1, 3.1416, New)` = 3,1416 och `vbaAcos(0.5, 3.1416, …)` lika i båda lägena.

### 6.2 `libs/lineCalc/load_cases_test.cpp` (ctest `load_cases`, paket B)
1. **`newLineInput()`:** läget New, inget lindata, aₙ och σ₀ tomma, T₀ = 0, min −50, max 50, konstanterna och alla 15 lastfall enligt 2.5.1, med talen −50, −30 och 50 i lastfall 1, 2 och 15. `newLineInput(CalcMode::Excel)` har läget Excel.
2. **`resetToStandard`:** behåller läget, lindatat, aₙ, σ₀, T₀ och O15. Allt annat som i 2.5.1, med J6 = −40 och J7 = 60, också efter ändrade värden och manuell last.
3. **`loadCrossingLower` och `loadCrossingUpper(…, 100)`:** lastfallen enligt 2.5.2 och 2.5.3 fält för fält, med källor, tal, O17 och `shortCircuitTempC`. J6, J7, O3–O5 och konstanterna utom O17 är oförändrade.
4. Ingen mall lämnar manuell last kvar.

**Ett test per avvikelse:**
- **`testModeD7`:** T1:s indata (J6 = −50, J7 = 50), Nedre linan, sedan J7 = 40 och `applyInputChange(MaxTemp)`.
  - Excel: talet i lastfall 15 blir 40, och lastfall 3 och 10 har kvar 50.
  - New: talen i lastfall 3 och 10 blir 40, och lastfall 15 (Fixed 0) är oförändrat.
  - Övre linan, sedan O4 = 600 och `applyInputChange(NormalWind)`. Excel: O-talet i lastfall 1 blir 0 och i lastfall 6 blir det O5. New: lastfall 1 och 13 har 600, och lastfall 6 är oförändrat.
- **`testModeD8`:** O4 = 600, sedan Nedre linan. Lastfall 6 har talet 500 och källan Normal i båda lägena. `caseCharPressureNPerM2(…, 6)` ger 500 i läget Excel och 600 i läget New. Testet länkar `kalkyl_linecalc` och använder A:s funktion.
- **`testModeD9`:** standardmallen, lastfall 1 ändras för hand till −10 med källan Fixed, sedan J6 = −45 och `applyInputChange(MinTemp)`. Excel: lastfall 1 blir −45. New: lastfall 1 är kvar −10, och lastfall 2 blir −25.

### 6.3 `libs/lineCalc/linecalc_facit_test.cpp` och `linecalc_facit.h` (ctest `linecalc_facit`, paket C)
1. `linecalc_facit.h` har ett fall per testfall i 6.4: linans tio värden och namn, indata, ändringarna, vilken mall, och facit för lastfall 1–15 (M, R, T, U, V, W, X, Y, Z, AA eller tomt, AC, AD och AE).
2. Testet bygger varje fall i läget Excel, i samma ordning som användaren i Excel:
   1. `newLineInput(CalcMode::Excel)`, lindatat och `resetToStandard`.
   2. J3–J5. Sedan J6, J7, O4 och O5, var och en följd av `applyInputChange`. O3 utan följdändring.
   3. Ändringarna i kolumnen Övrigt, mallen och den manuella lasten.
3. Testet kör `calculateLine` och jämför alla kolumner med toleranserna i 6.5.
4. Testet räknar också varje fall i läget New och skriver skillnaden mot Excel utan att jämföra med facit, till exempel "T2 New: 8 lastfall skiljer sig, största skillnad 0,25 % (X38)". Det är underlaget för att se skillnaden mellan lägena.
5. Testet skriver en rad per fall ("T3: 15 lastfall, största relativa skillnad 3,1e-16 (X38)") och vid fel en tabell med Kalkyl, facit och skillnad per cell. Utskriften är handtestet i fas 1 (avsnitt 8).
6. T1 ur bilaga A finns redan. T2–T15 läggs in när användaren lämnat facit. T16 och T17 är frivilliga.

### 6.4 Testfallen att räkna i Excel
**Arbetsgång för varje fall** i en ny kopia av `ref/Linberäkning.xlsm`, med tillägget 5420_Verktyg laddat:
1. Välj linan i Lina. Namnen är linregistrets (startdatan) och Lindata-bladets.
2. Klicka "Återställ indata enl. SS-EN 50341-2-18".
3. Skriv J3 (aₙ), J4 (σ₀) och J5 (T₀), sedan J6 och J7, sedan O3, O4 och O5.
4. Gör ändringarna i kolumnen Övrigt, i den ordning de står.
5. Kontrollera att resultattabellen ändrats. Om inte: skriv om J3 med samma värde.
6. Arkiv > Spara som `ref/facit/Linberakning_T<nr>.xlsm` (Excel-arbetsbok med makron).

Kontrollvärdena är σ i lastfall 4 (X26) och 15 (X38) ur planeringens omräkning i läget Excel. Facit från Excel gäller före dem. Kolumnen New anger om läget New ska ge ett annat resultat.

| Nr | Lina | aₙ | σ₀ | T₀ | Min | Max | h₀ | O4 | O5 | Övrigt | Täcker | X26 / X38 | New |
|---|---|---|---|---|---|---|---|---|---|---|---|---|---|
| T1 | 124 mm² OPGW AFL 1x48 SMF 14230/20-01 (Dotterel) | 314,6 | 90 | 0 | −50 | 50 | 25 | 500 | 800 | – | Cardano, Eiv i Ei (4, 5), transponering i 5, krypning. **Facit finns** (bilaga A) | 249,6346 / 60,8744 | lika |
| T2 | som T1 | 314,6 | 90 | 0 | −50 | 50 | 25 | 500 | 800 | O13 = 0,6, O14 = 1,1, O17 = 4 | E4 från lastfall 5 vid transponering i 4 (D1), transponering med vind, isvikt med bara Gc | 263,6143 / 60,5811 | D1 |
| T3 | 157 mm² FeAl PARTRIDGE | 250 | 40 | 0 | −40 | 60 | 25 | 500 | 800 | – | Trigonometriska grenen i Ep och transponeringen, k = 160 | 140,9280 / 28,4947 | lika |
| T4 | 157 mm² FeAl PARTRIDGE | 400 | 50 | 0 | −40 | 60 | 25 | 500 | 800 | – | Utnyttjandegrad över 55 % (röd), trigonometrisk gren i Ei (lastfall 3) | 180,9283 / 38,0307 | lika |
| T5 | `106mm²  OPGW 78N94z` (två mellanslag) | 350 | 420 | 0 | −40 | 60 | 25 | 500 | 800 | – | Ep = Ei, Eiv i Ep (8, 9) | 575,3221 / 277,7226 | D5 |
| T6 | 185 mm² Fe60 | 200 | 100 | 0 | −40 | 60 | 25 | 500 | 800 | – | Brottlast "-", k = 0, Tp = 0, ingen Eiv | 194,8735 / 79,0322 | lika |
| T7 | 95 mm² Cu | 120 | 60 | 10 | −40 | 60 | 25 | 500 | 800 | – | T₀ ≠ T_tr, minsta islast O15 (lastfall 5 och 9), Tp = 0 | 152,4503 / 46,5679 | lika |
| T8 | 209 mm² ADSS, SD-nDJ6/31 | 150 | 20 | 0 | −40 | 60 | 25 | 500 | 800 | – | Negativ α, låg E | 68,8599 / 20,0345 | lika |
| T9 | 910 mm² Al | 350 | 45 | 15 | −40 | 60 | 12 | 500 | 800 | – | Eiv i lastfall 1 (kyla utan is), h₀ under href, T₀ = 15 | 85,3953 / 30,9710 | lika |
| T10 | 910 mm² FeAl FALCON | 450 | 80 | 0 | −40 | 80 | 40 | 600 | 900 | – | Vindtrycken sprids till lastfallen, dqh över href, max 80 °C | 131,5029 / 52,8292 | lika |
| T11 | som T1 | 314,6 | 90 | 0 | −50 | 50 | 25 | 500 | 800 | Klicka Nedre linan | Mallen 2.5.2, L framåt (5 → 6) | 125,3688 / 75,0451 | lika |
| T12 | som T1 | 314,6 | 90 | 0 | −50 | 50 | 25 | 500 | 800 | Klicka Övre linan, svara 100 | Mallen 2.5.3, transponering i 3, L framåt (9 → 13) | 90,0000 / 75,0451 | lika |
| T13 | som T1 | 314,6 | 90 | 0 | −50 | 50 | 25 | 500 | 800 | Skriv 25 i U25, sedan 5 i V25 | Manuell last i lastfall 3, Eiv i 3 | 249,6346 / 60,8744 (X25 = 274,4946) | lika |
| T14 | som T1 | 314,6 | 40 | 0 | −50 | 50 | 25 | 500 | 800 | – | Trigonometriska grenen i nästan alla lastfall, ingen Eiv | 167,4689 / 35,4724 | lika |
| T15 | som T1 | 314,6 | 40 | 0 | −50 | 50 | 25 | 500 | 800 | O10 = 3,14159265358979 | Effekten av arkets π (4.1, punkt 2) | 167,4687 / 35,4732 | lika |
| T16 | som T1 | 314,6 | 170 | 20 | −50 | 50 | 25 | 500 | 800 | – | Frivilligt: x₀ = 194,8 > σp (D4). Vad visar Excel? | – | D4 |
| T17 | som T1 | 314,6 | 90 | 0 | −50 | 50 | 25 | 500 | 800 | Klicka Nedre linan, sedan J7 = 40 och O4 = 600 | Frivilligt: arkets följdändringar efter en korsningsmall (D7, D8) | – | D7, D8 |

### 6.5 Facit: format och toleranser
**Format A (föredras):** den sparade kopian. Paket C läser cellernas sparade värden med full precision med läskommandot i bilaga B: J2–J18, O2–O17, B21–B38 och F23:AE38.

**Format B (om en kopia inte går att spara):** markera F23:AE38, Ctrl+C, och klistra in i en textfil `ref/facit/T<nr>.tsv`. Gör likadant med J2:J18 och O2:O17 i en andra fil. Talen blir då de visade, med arkets format.

**Toleranser:**

| Kolumner | Format A | Format B |
|---|---|---|
| R, T, U, V, W, X, Y, Z, AA, AC, AD | \|Kalkyl − facit\| ≤ 1e-9 · max(1, \|facit\|) | en halv enhet i sista visade siffran: R, Y, Z, AA 0,5; T 0,05; U, V, W, X, AC, AD 0,005 |
| AE | \|Kalkyl − facit\| ≤ 0,5 + 1e-9 | samma |
| M | exakt | exakt |
| AA "-" | `utilizationPercent` tom | samma |

1e-9 räcker med marginal: omräkningen i planeringen träffar T1 på 1e-15. Toleransen tål skillnader i sista siffran mellan `std::pow` och VBA:s `^`, men inte en annan ekvationslösare eller ett annat antal Eiv-varv. T16 (D4) jämförs bara om Excel och Kalkyl följer samma gren (3.3).

---

## 7. Implementeringspaket (fas 1)

Paket N körs först och ensamt. Därefter arbetar A, B och C parallellt. Alla är Sonnet-agenter (high). Ingen fil skrivs av två paket samtidigt.

### 7.1 Byggkataloger
- **En katalog per paket:** `build-n`, `build-a`, `build-b` och `build-c`. Paketet tar bort sin katalog när det är klart (`rm -rf build-<x>`).
- **`build/` är användarens.** Den tas aldrig bort, rensas inte och konfigureras inte om av en agent.
- **MSYS2:s cmake** (`C:/msys64/ucrt64/bin/cmake.exe`), aldrig `C:/Program Files/CMake`.
- **Konfiguration** (med paketets katalog):
  ```sh
  cmake -S . -B build-a -G Ninja -DCMAKE_BUILD_TYPE=Debug -DCMAKE_C_COMPILER=C:/msys64/ucrt64/bin/gcc.exe -DCMAKE_CXX_COMPILER=C:/msys64/ucrt64/bin/g++.exe -DKALKYL_TESTS=ON -DFETCHCONTENT_SOURCE_DIR_GLAZEUI=C:/Users/sehoer/Source/Kalkyl/build/_deps/glazeui-src
  ```
- **Bygg bara de egna målen:**
  - N: allt (`cmake --build build-n`), också `kalkyl`, och `ctest --test-dir build-n --output-on-failure`
  - A: `kalkyl_linecalc_test` och `ctest -R "^linecalc$"`
  - B: `kalkyl_load_cases_test` och `ctest -R load_cases`
  - C: `kalkyl_linecalc_facit_test` och `ctest -R linecalc_facit`
- **Faller bygget i en fil som ett annat paket äger:** vänta en stund och försök igen. Rör aldrig den filen.

### 7.2 Steg 0 för A, B och C: kontraktsfilerna först
Efter paket N skriver A, B och C först, inom några minuter, sina kontraktsfiler, så att de andra kan kompilera:
- **A:**
  - `line_calc.h` ordagrant ur 5.3 och en `line_calc.cpp` som kompilerar (tomma kroppar)
  - raden `add_subdirectory(lineCalc)` efter `anCalc` i `libs/CMakeLists.txt`
  - `libs/lineCalc/CMakeLists.txt`:
  ```cmake
  add_library(kalkyl_linecalc STATIC line_calc.cpp load_cases.cpp)
  target_include_directories(kalkyl_linecalc PUBLIC ${PROJECT_SOURCE_DIR}/libs)
  target_link_libraries(kalkyl_linecalc PUBLIC kalkyl_conductor PRIVATE kalkyl_tabletext kalkyl_warnings)
  ```
  plus testmålen `kalkyl_linecalc_test` (`linecalc`), `kalkyl_load_cases_test` (`load_cases`) och `kalkyl_linecalc_facit_test` (`linecalc_facit`), alla med `kalkyl_linecalc` och `kalkyl_warnings`.
- **B:** `load_cases.h` ordagrant ur 5.4, en `load_cases.cpp` som kompilerar och en `load_cases_test.cpp` med en `main` som svarar 0.
- **C:** `linecalc_facit.h` med typerna för facit och T1, och en `linecalc_facit_test.cpp` med en `main` som svarar 0.

Ingen väntar på ett meddelande. Den som behöver en annans fil bygger när filen finns.

### 7.3 Paket N: namnbytet (före de andra)
Ett rent namnbyte. Inget beteende ändras, och alla befintliga tester ska gå igenom efteråt. Filer flyttas med `git mv` (innehållet orört), och innehållet ändras sedan med Edit och Write.

**Flyttas:**
- `libs/calc/normal_span.h`, `normal_span.cpp`, `CMakeLists.txt` och `CLAUDE.md` → `libs/anCalc/`
- `libs/calc/calc_test.cpp` → `libs/anCalc/ancalc_test.cpp`
- `libs/calc/table_text.h` och `table_text.cpp` → `libs/tableText/`
- Katalogen `libs/calc` finns inte efteråt.

**Nya filer:**
- `libs/tableText/CMakeLists.txt`: `kalkyl_tabletext` (STATIC, `table_text.cpp`, `libs/` PUBLIC, `kalkyl_warnings` PRIVATE) och testet `kalkyl_tabletext_test` (ctest `tabletext`).
- `libs/tableText/tabletext_test.cpp`: `testTableText` ur `calc_test.cpp`, flyttad oförändrad med samma `CHECK`, `report` och `near`.
- `libs/tableText/CLAUDE.md`: tabelltextens rader och regler ur `libs/calc/CLAUDE.md` (talform, urklipp, inklistring).

**Ändras:**
| Fil | Ändring |
|---|---|
| `libs/anCalc/CMakeLists.txt` | `kalkyl_ancalc` (bara `normal_span.cpp`, länkar `kalkyl_tabletext` PRIVATE), testet `kalkyl_ancalc_test` (ctest `ancalc`, länkar `kalkyl_tabletext` om testet behöver det) |
| `libs/anCalc/normal_span.h`, `normal_span.cpp` | namnrymden `kalkyl::ancalc`, include `"anCalc/normal_span.h"` och `"tableText/table_text.h"`, `formatDecimal` som `tabletext::formatDecimal` |
| `libs/anCalc/ancalc_test.cpp` | utan `testTableText`, includes och namnrymder enligt ovan |
| `libs/tableText/table_text.h`, `table_text.cpp` | namnrymden `kalkyl::tabletext`, include `"tableText/table_text.h"` |
| `libs/anCalc/CLAUDE.md` | rubriken `libs/anCalc`, bara normalspannet, mål, namnrymd och include |
| `libs/CMakeLists.txt` | `add_subdirectory(tableText)` och `add_subdirectory(anCalc)` i stället för `calc`, i ordningen i 5.1, och kommentaren om ordningen |
| `libs/CLAUDE.md` | tabellen, beroendena och regeln om en uppgift per bibliotek (avsnitt 11) |
| `libs/projectdb/CMakeLists.txt` | `kalkyl_calc` → `kalkyl_ancalc` |
| `libs/projectdb/project_data.h`, `project_data.cpp`, `projectdb_test.cpp` | include `"anCalc/normal_span.h"`, `calc::` → `ancalc::` |
| `libs/projectdb/CLAUDE.md` | `kalkyl_calc` → `kalkyl_ancalc`, `calc::calculateNormalSpan` → `ancalc::calculateNormalSpan` |
| `src/CMakeLists.txt` | `kalkyl_calc` → `kalkyl_ancalc` och `kalkyl_tabletext` |
| `src/CLAUDE.md` | länkade bibliotek |
| `src/windows/normal_span_dialog.h`, `normal_span_dialog.cpp` | includes, aliaset `namespace calc = kalkyl::calc;` blir `namespace ancalc = kalkyl::ancalc;` och `namespace tabletext = kalkyl::tabletext;`, och varje `calc::` byts efter symbolen |
| `src/windows/main_window.h`, `main_window.cpp` | includes och `kalkyl::calc::` efter symbolen |
| `src/windows/CLAUDE.md` | omnämnanden av `calc::` och `libs/calc` |
| Rotens `CLAUDE.md` | tabellen Delar: raden Beräkningar ersätts av Normalspann (`libs/anCalc`) och Tabelltext (`libs/tableText`), samordnaren lägger till Linberäkning (`libs/lineCalc`) efter A |

- **Symbolerna** som hör till `tabletext`: `parseDecimal`, `formatDecimal`, `parseTsv`, `formatTsv`, `planPaste`, `PasteCell` och `PastePlan`.
- **Symbolerna** som hör till `ancalc`: `SpanPole`, `SpanLimits`, `SpanCheck`, `SpanResult`, `IssueKind`, `SpanColumn`, `SpanIssue`, `SpanCalculation`, `calculateNormalSpan`, `roundTenth`, `ratioText`, `adjacentText` och `localText`.
- **Ändras inte:**
  - de arkiverade planerna i `doc/plans/archive/`, som är historik
  - Dokumentationens nycklar `ns_calc_*` och `line_calc_*`
  - kod- och textnamn som `nsCalcs_`
- **Kontroll efteråt:** `grep -rnE 'kalkyl_calc|kalkyl::calc|"calc/|libs/calc' CMakeLists.txt libs src CLAUDE.md` ger ingen träff. `ctest` i `build-n` visar `ancalc`, `tabletext`, `conductor`, `platform`, `db`, `systemdb` och `projectdb`, alla godkända. glaze-render behövs inte. Ett röktest av `kalkyl.exe`:
  ```sh
  KALKYL_DATA_DIR=build-n/smoke timeout -k 2 3 ./build-n/src/kalkyl.exe; echo $?
  ```
  Svaret ska vara 124 eller 137.

**Äger under sin tid:** alla filer i tabellen ovan och de flyttade och nya filerna.

### 7.4 Paket A: beräkningen
**Äger:**
- `libs/lineCalc/CMakeLists.txt`, `line_calc.h`, `line_calc.cpp`, `linecalc_test.cpp` och `libs/lineCalc/CLAUDE.md`
- `libs/CMakeLists.txt` och `libs/CLAUDE.md` (bara raden för `lineCalc`)

**Arbetsgång:**
1. Steg 0.
2. `stageOf`, källorna, `vbaAcos`, `solveStateCubic`, `ModeRules` och `calculateLine` enligt avsnitt 3–5.
3. Testerna enligt 6.1.
4. CLAUDE.md enligt avsnitt 11, med B:s och C:s rader ur deras rapporter.

### 7.5 Paket B: lastfallsmallarna
**Äger:** `libs/lineCalc/load_cases.h`, `load_cases.cpp` och `load_cases_test.cpp`.

**Arbetsgång:**
1. Steg 0.
2. `newLineInput`, `resetToStandard`, `loadCrossingLower`, `loadCrossingUpper` och `applyInputChange` enligt 2.4, 2.5, 5.4 och 5.6.
3. Testerna enligt 6.2.
4. Rapporten innehåller texten till B:s rader i `libs/lineCalc/CLAUDE.md`.

### 7.6 Paket C: facittestet
**Äger:** `libs/lineCalc/linecalc_facit.h` och `linecalc_facit_test.cpp`.

**Arbetsgång:**
1. Steg 0, med T1 ur bilaga A.
2. Testet enligt 6.3. Det byggs och körs när A och B är klara.
3. När användaren lämnat facit: läs varje kopia med bilaga B (eller TSV-filen) och skriv in talen i `linecalc_facit.h` med Write eller Edit. Inga skript skriver filen.
4. Rapporten innehåller texten till C:s rader i `libs/lineCalc/CLAUDE.md` och utskriften från testet.

### 7.7 Samordnaren
**Äger:** `TODO.md`, `doc/plans/CLAUDE.md` och efter paket N rotens `CLAUDE.md`.

**Ordning:**
1. Planen och användarens svar (klart).
2. Användaren tar fram facit T2–T15. Det kan ske samtidigt som 3–4. C:s steg 3 väntar på facit.
3. Paket N. Samordnaren bygger i `build/` efteråt och kontrollerar att allt fungerar som förut.
4. A, B och C parallellt.
5. Integration (avsnitt 8).
6. Handtest (avsnitt 8).
7. Granskning (Opus).

### 7.8 Regler för alla implementeringsagenter
- **Filer redigeras med Edit och Write.** Inga Python- eller sed-skript ändrar filer. Skal används bara för bygge, körning, test, läsning och `git mv` i paket N. Bilaga B är ett läskommando.
- **Hooken `~/.claude/hooks/only-c-drive.sh`** granskar varje verktygsanrop, också filinnehåll, och stoppar:
  - två bakvända snedstreck i rad
  - ett bakvänt snedstreck och en bokstav (escape-sekvens) efter skiljetecken eller blanksteg och före citattecken eller radslut
  - andra enhetsbokstäver än C följda av kolon, också i slutet av en rad eller precis före ett citattecken. Låt aldrig en Edit-sträng sluta med en ensam bokstav och kolon
- **Därför:**
  - råa strängliteraler `R"( … )"` om reguljära uttryck behövs
  - teckenklasser som `[.]` och `[0-9]` i stället för escape
  - `char{10}` för radbrytning och `char{9}` för tabb
  - snedstreck framåt i sökvägar, och i CMake bracket-argument och `string(ASCII 10 NL)`
  - inga escape-sekvenser för radbrytning i strängar; utskrift med `std::endl` eller `char{10}`
- **Kommentarer:** varje funktion, klass och logisk del har en kommentar på 2–5 rader. Inline-kommentarer är korta och säger bara vad som händer. Ingen historik i kommentarer ("flyttad från", "hette förut").
- **Varningar:** bygget ska vara utan varningar (`kalkyl_warnings`, `-Wall -Wextra`).
- **Versionen** står kvar på 0.3.0. Inga commits.
- **CLAUDE.md:** uppdatera katalogens CLAUDE.md när filer läggs till eller byter syfte.

---

## 8. Integration och verifiering (fas 1, samordnaren)
1. **Bygget** i `build/` utan varningar: `cmake --build --preset debug 2>&1 | grep -i warning` ger ingen utdata. Namnbytet får CMake att köras om av sig självt.
2. **Testerna** i en tillfällig katalog:
   ```sh
   cmake -S . -B build-verify -G Ninja -DCMAKE_BUILD_TYPE=Debug -DCMAKE_C_COMPILER=C:/msys64/ucrt64/bin/gcc.exe -DCMAKE_CXX_COMPILER=C:/msys64/ucrt64/bin/g++.exe -DKALKYL_TESTS=ON -DFETCHCONTENT_SOURCE_DIR_GLAZEUI=C:/Users/sehoer/Source/Kalkyl/build/_deps/glazeui-src
   cmake --build build-verify && ctest --test-dir build-verify --output-on-failure
   ```
   Alla tester ska gå igenom: `ancalc`, `tabletext`, `linecalc`, `load_cases`, `linecalc_facit` och de gamla.
3. **Röktest:** `KALKYL_DATA_DIR=build/smoke timeout -k 2 3 ./build/src/kalkyl.exe; echo $?` ger 124 eller 137. Normalspannsformuläret och fliken Normalspann fungerar som förut.
4. **Reglerna:**
   - CLAUDE.md är uppdaterad i varje berörd katalog.
   - Kommentarerna har 2–5 rader.
   - `grep -rn "ref/" libs/lineCalc/CMakeLists.txt` ger ingen träff.
   - Kontrollen ur 7.3 ger ingen träff.
5. **Versionen:** `project(Kalkyl VERSION 0.3.0` är oförändrad.
6. **Städning:** `rm -rf build-verify`. `build-n`, `build-a`, `build-b` och `build-c` är borta.
7. **Handtest:** fas 1 har inget nytt fönster.
   - Användaren får utskriften från `linecalc_facit`: en rad per testfall med största skillnad mot Excel och skillnaden mellan lägena.
   - Användaren provar att normalspannet fungerar som förut efter namnbytet.

---

## 9. Fas 2: fliken Linberäkning och formuläret (skiss)

### 9.1 Projektdatabasen
- **Nytt steg 3** i `libs/projectdb/dev_migration.h` (`dev::kStep3`, user_version 2 → 3). Steg 2 är committat i v0.2.0 och ändras inte. `kSchemaVersion` blir 3, och `kExpectedTablesSql` får de nya tabellerna. `kalkyl_projectdb` länkar `kalkyl_linecalc`.
- **Tabeller (utkast):**
  - `line_calc`:
    - id, anteckning, läget (`excel` eller `new`)
    - lindatat som kopia (namn och de tio talen, alla NULL när lindatat är tomt)
    - aₙ och σ₀ (NULL när de är tomma), T₀, min, max och kortslutningstemperaturen
    - h₀, O4, O5 och konstanterna O9–O17
    - flaggan "indata ur Dokumentation" (fas 3), skapad och ändrad
  - `line_calc_case`: calc_id, case_id 1–15, temperaturkälla och tal, benämning, J, K, L, N, vindtryckskälla och tal, P, Q, S, manuell (0/1), I och Qwc. Primärnyckel (calc_id, case_id), `ON DELETE CASCADE`.
  - `doc_line_calc`: doc_key som primärnyckel och calc_id, som `doc_normal_span`.
  - Källorna lagras som text (`fixed`, `min`, `min_plus_20`, `max`, `short_circuit`, `short_circuit_rise`; `fixed`, `normal`, `high`).
- **API i `project_data.h` (utkast):**
  - `LineCalc` (id, anteckning, `linecalc::LineInput`, docKeys, tider)
  - `lineCalcs`, `lineCalc`, `saveLineCalc(p, lc, linkDocKey)`, `linkLineCalc` och `removeLineCalc`
  - `unlinkDocRow` kopplar bort i båda kopplingstabellerna
  - resultatet lagras aldrig utan räknas med `linecalc::calculateLine`
- **Migreringstest:** ett projekt från v0.2.0 (steg 1–2 med normalspann och Dokumentation) öppnas, alla data finns kvar och de nya tabellerna finns.

### 9.2 Formuläret `LineCalcDialog` (`src/windows/line_calc_dialog.*`)
- **Arkets uppställning** (beslut 5):
  - Indata: rullistan Lina (linregistret och projektets linor), aₙ, σ₀, T₀, min och max. Till höger linhöjd och de två vindtrycken.
  - Lindata/Konstanter: lindatat fylls i när en lina väljs och kan sedan ändras. Konstanterna går att ändra.
  - Knapparna "Övre linan" och "Nedre linan" under rubriken "Läs in korsningslastfall enligt Table 5.9.6/SE.1 (50341-2-18)", och "Återställ indata enl. SS-EN 50341-2-18".
  - Tabellen med 15 lastfall och en tom avskiljare mellan 7 och 8. Kolumngrupperna Lastfall, Resultat och Data till SAG (3D), med arkets rubriker och format.
  - Ljusorange (252, 213, 180) och vit växling som i arket, röd utnyttjandegrad över O16, låsta resultatkolumner och dämpade lastfall utan benämning.
  - Anteckning, statusrad med upplysningarna, OK och Avbryt.
- **Växelknappen Excel/Ny:** byter `LineInput::mode` och räknar om. En möjlighet är att formuläret räknar båda lägena och visar de celler som skiljer sig med en markering och det andra lägets värde som tooltip.
- **Tomma indata** (beslut 6):
  - En ny beräkning har tom lina, tomt lindata, tomt aₙ och tomt σ₀. Cellerna visas tomma, med samma inmatningsfärg som i arket.
  - Resultatkolumnerna är tomma, och statusraden visar upplysningarna av slaget Missing, till exempel "Uppspänningen saknas.", utan felfärg.
  - En lindatacell som töms gör lindatat ofullständigt. Formuläret skickar då inget lindata, markerar de tomma lindatacellerna och visar "Lina eller lindata saknas."
  - Fel av slaget Error markerar cellen rött med texten som tooltip.
- **Beteende:**
  - Omräkning vid varje ändring.
  - Ändras J6, J7, kortslutningstemperaturen, O4 eller O5 anropas `applyInputChange`.
  - En ändrad temperatur eller ett ändrat vindtryck i ett lastfall blir Fixed.
  - Ett tal i I eller Qwc gör lastfallet manuellt, och snabbmenyn "Räkna last ur vind och is" tar bort det.
  - Övre linan frågar efter kortslutningstemperaturen.
  - Formuläret buffrar ändringarna och sparar själv vid OK, som `NormalSpanDialog`.

### 9.3 Fliken Linberäkning i huvudfönstret
- Efter fliken Normalspann, med knapparna Ny beräkning…, Ändra…, Kopiera… och Ta bort och en lista i gul växlande radton.
- **Kolumner (utkast):** Lina, Läge, aₙ, σ₀, T₀, Min/Max, Mall (lastfall 15), Högsta utnyttjandegrad, Dokumentation och Anteckning. En beräkning som saknar indata visar "-" och en med fel "fel", med upplysningen som tooltip.

### 9.4 Paket (utkast)
B projektdatabasen, C formuläret och D huvudfönstret, högst fyra med ett eventuellt A för små tillägg i `libs/lineCalc`.

---

## 10. Fas 3: kopplingen till Dokumentation (skiss)

### 10.1 Raderna
De sex raderna "Linberäkning … före/efter ombyggnation" (nycklarna `line_calc_phase_before` … `line_calc_top_right_after`, enhet Mall) får en snabbmeny som normalspannsraderna:
- **Utan beräkning:** Lägg till… (indata ur Dokumentation), Lägg till manuell…, Koppla befintlig och Manuellt värde.
- **Med beräkning:** Ändra…, Koppla bort och Ta bort…. Skrivskyddat bara Visa….
- **Värde:** stakningsmallen, Mall i lastfall 15 (`stakingMall`) avrundad till heltal, i beräkningens läge, i en låst cell med Σ-bild (beslut 7). Saknas indata eller finns fel: tom cell och upplysningen som tooltip. En koppling tömmer radens manuella värde, som för normalspann.

### 10.2 Automatiska värden
| Indata | Faslina | Topplina vänster | Topplina höger |
|---|---|---|---|
| Lina och lindata | `phase_conductor` | `top_conductor_left` | `top_conductor_right`, "Samma" = vänster |
| σ₀ | `tension_phase` | `tension_top_left` | `tension_top_right`, "Samma" = vänster |
| aₙ | normalspannsraden för samma lina och skede: kopplad beräkning eller manuellt värde, annars den gröna raden `normal_span_phase` / `normal_span_top_left` / `normal_span_top_right`, annars fel | | |
| Min, max | `temp_min`, `temp_max` | | |

- Linan är projektets kopia för linraden.
- Värdena läses varje gång beräkningen räknas. Formuläret visar dem låsta med texten "ur Dokumentation".
- Ett värde som saknas ger en upplysning av slaget Missing, till exempel "Dokumentation saknar Uppspänning fas."
- T₀, h₀, vindtrycken, konstanterna, lastfallen och läget finns inte i Dokumentation. De lagras i beräkningen med förvalen i 5.4.
- När Dokumentation eller en kopplad normalspannsberäkning ändras räknas de kopplade linberäkningarna om. Ändrade min och max går genom `applyInputChange`.

### 10.3 Manuella beräkningar
Har egna indata och en egen kopia av lindatat. De kan kopplas till flera rader. En beräkning med indata ur Dokumentation kopplas till en rad.

### 10.4 Paket (utkast)
D huvudfönstret (snabbmenyn, värdet, omräkningen), C formuläret (låsta fält) och B projektdatabasen om flaggan och kopplingen kräver det.

---

## 11. CLAUDE.md och TODO.md
- **`libs/CLAUDE.md`** (N, sedan A):
  - **Regeln:** "Varje bibliotek har en enda uppgift. En ny beräkning eller ett nytt hjälpmedel får ett eget bibliotek, och kod som flera bibliotek använder ligger i sitt eget bibliotek."
  - **Beroendena:** `core` ← `platform` ← `db` ← `systemdb`/`projectdb` ← `appearance`. `conductor` och `tableText` beror bara på standardbiblioteket. `anCalc` beror på `tableText`, och `lineCalc` på `conductor` och `tableText`. `systemdb` beror på `db` och `conductor`, och `projectdb` på `db`, `anCalc` och `conductor` (och `lineCalc` från fas 2).
  - **Namnen:** katalogen får gärna versal i mitten (`anCalc`), och målet och namnrymden skrivs med gemener (`kalkyl_ancalc`, `kalkyl::ancalc`).
  - **Tabellen:** raderna `anCalc/`, `tableText/` och (A) `lineCalc/` i stället för `calc/`.
  - **Testerna:** "Varje bibliotek har sina testfiler i sin katalog."
- **`libs/anCalc/CLAUDE.md`** (N): bara normalspannet. **`libs/tableText/CLAUDE.md`** (N): tabelltexten med reglerna om talform och urklipp.
- **`libs/lineCalc/CLAUDE.md`** (A, med B:s och C:s rader): filerna, och reglerna:
  - beräkningen följer VBA formel för formel (planen 3 och 5.6)
  - arkets π ur O10
  - lägena Excel och Ny med avvikelserna D1–D9
  - `ModeRules` som enda stället för Excels grenar
  - mallarna och följdändringarna i `load_cases`
- **`libs/projectdb/CLAUDE.md`, `src/CLAUDE.md` och `src/windows/CLAUDE.md`** (N): de nya namnen.
- **Rotens `CLAUDE.md`** (N, sedan samordnaren): tabellen Delar med Normalspann (`libs/anCalc`), Tabelltext (`libs/tableText`) och Linberäkning (`libs/lineCalc`).
- **`doc/plans/CLAUDE.md`** (samordnaren): raden för den här planen.
- **`TODO.md`** (samordnaren): punkten Linberäkning får status efter fas, och idéerna i 1.3 läggs till.

---

## 12. Öppna frågor
Inga. Frågorna i utkastet är besvarade, och svaren står under "Användarens beslut 2026-11-25" överst.

---

## Bilaga A: facit T1 ur arbetsbokens sparade värden

**Indata** (`ref/Linberäkning.xlsm`, bladet Linberäkning): lina "124 mm² OPGW AFL 1x48 SMF 14230/20-01 (Dotterel)" med A = 124, d = 14.9, Eik = 89000, Ep = 95000, σp = 180, k = 40, α = 17.1, Tp = 23.391812865497073, qe = 5.34, fu = 77.7. aₙ = 314.6, σ₀ = 90, T₀ = 0, J6 = −50, J7 = 50, O3 = 25, O4 = 500, O5 = 800, O9 = 917.4, O10 = 3.1416, O11 = 9.81, O12 = 1, O13 = 0.5, O14 = 1, O15 = 20, O16 = 55, O17 = 5. Standardmallen (2.5.1), B23:B38 = 0. Läget Excel. Läget Ny ger samma resultat.

**M** (höjd): 25 i lastfall 2, 3, 4, 6, 8, 10, 13 och 14, annars 0.

| ID | R | T | U | V | W | X | Y | Z | AA | AC | AD | AE |
|---|---|---|---|---|---|---|---|---|---|---|---|---|
| 1 | 0 | 0 | 0 | 0 | 4.2496941021932928 | 125.36876604134662 | 15545.726989126981 | 171.75137591618949 | 20.007370642377069 | 4.2496941021932928 | 0 | 172 |
| 2 | 500 | 0 | 0 | 3.7250000000000001 | 4.40760567141868 | 120.8771712723737 | 14988.769237774339 | 178.1333715693701 | 19.290565299580873 | 5.3740222421589543 | 34.898217991235953 | 217 |
| 3 | 500 | 0 | 0 | 1.1174999999999999 | 6.346392939596508 | 83.949876838075923 | 10409.784727921415 | 256.489453892207 | 13.397406342241203 | 6.4838702103883383 | 11.819667798760729 | 262 |
| 4 | 500 | 20.264505893041264 | 16.743530380610878 | 12.725 | 8.8261113418727888 | 249.63464951586255 | 30954.696539966957 | 356.70726657096884 | 39.838734285671762 | 10.186536852909702 | 29.951391228421414 | 412 |
| 5 | 0 | 20.264505893041264 | 20.147407460770967 | 0 | 10.186536852909702 | 249.63464951586255 | 30954.696539966957 | 411.68886000647854 | 39.838734285671762 | 10.186536852909702 | 0 | 412 |
| 6 | 890 | 0 | 0 | 6.6304999999999996 | 4.3645223271432467 | 122.07038152417618 | 15136.727308997846 | 176.39215832426675 | 19.480987527667757 | 6.9582807749031597 | 51.152984533267528 | 281 |
| 7 | 0 | 0 | 0 | 0 | 5.9197656182130949 | 90.000000001010662 | 11160.000000125323 | 239.24731182527037 | 14.362934363095652 | 5.9197656182130949 | 0 | 239 |
| 8 | 500 | 20.264505893041264 | 16.743530380610878 | 12.725 | 9.2549072702898822 | 238.06864262048742 | 29520.51168494044 | 374.0370528854441 | 37.99293653145488 | 10.681425865523444 | 29.951391228421414 | 432 |
| 9 | 0 | 20.264505893041264 | 20.147407460770967 | 0 | 10.681425865523444 | 238.06864262048742 | 29520.51168494044 | 431.68979814420163 | 37.99293653145488 | 10.681425865523444 | 0 | 432 |
| 10 | 890 | 0 | 0 | 6.6304999999999996 | 4.9753734856005751 | 107.0831983140759 | 13278.316590945411 | 201.07970627998841 | 17.089210541757286 | 7.9321499760725471 | 51.152984533267528 | 321 |
| 11 | 0 | 0 | 0 | 0 | 7.0994529510360538 | 75.045064643665356 | 9305.5880158145046 | 286.92437226561435 | 11.976303752657021 | 7.0994529510360538 | 0 | 287 |
| 12 | 0 | 0 | 0 | 0 | 7.6122928145944426 | 69.989281629275595 | 8678.6709220301746 | 307.65079399685499 | 11.169460646113482 | 7.6122928145944426 | 0 | 308 |
| 13 | 500 | 0 | 0 | 3.7250000000000001 | 6.4924670891806118 | 82.061086845247502 | 10175.574768810689 | 262.39304026184919 | 13.095977823437179 | 7.9160127163806191 | 34.898217991235953 | 320 |
| 14 | 500 | 0 | 0 | 1.1174999999999999 | 7.4815477321236594 | 71.212391435739804 | 8830.3365380317355 | 302.3667318341117 | 11.364654489101332 | 7.6436150313439564 | 11.819667798760729 | 309 |
| 15 | 0 | 0 | 0 | 0 | 8.7521002514106119 | 60.874406181452422 | 7548.4263665000999 | 353.71610854541211 | 9.7148344485200759 | 8.7521002514106119 | 0 | 354 |

Mellanvärden ur planeringens omräkning: K1 = 7.647955258454736, E4 i transponeringen = 86214.56857120684 (från lastfall 5, samma som tr), σ_tr = 249.63464979330303, σ_p0 = 75.04506464366474 vid T_tr = 0. Stakningsmallen = Z38 = 353.71610854541211.

## Bilaga B: läskommando för facit ur en sparad kopia
Kommandot läser bara. Det skriver cellernas sparade värden som `cell=värde`, en per rad. Arket Linberäkning är `sheet1.xml`. Kontrollera att J3 är aₙ.
```sh
python -I -c "
import sys, zipfile, re
import xml.etree.ElementTree as ET
sys.stdout.reconfigure(encoding='utf-8')
m = '{http://schemas.openxmlformats.org/spreadsheetml/2006/main}'
z = zipfile.ZipFile(sys.argv[1])
ss = [''.join(t.text or '' for t in si.iter(m + 't')) for si in ET.fromstring(z.read('xl/sharedStrings.xml'))]
want = re.compile('^(J([2-9]|1[0-8])|O([2-9]|1[0-7])|B(2[1-9]|3[0-8])|([F-Z]|A[A-E])(2[3-9]|3[0-8]))$')
for c in ET.fromstring(z.read('xl/worksheets/sheet1.xml')).iter(m + 'c'):
    v = c.find(m + 'v')
    if v is not None and want.match(c.get('r')):
        print(c.get('r'), ss[int(v.text)] if c.get('t') == 's' else v.text, sep='=')
" ref/facit/Linberakning_T02.xlsm
```
