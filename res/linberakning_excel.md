# PM: Linberäkningen och fel i Excel-arket

## Sammanfattning

Kalkyl räknar som arket Linberäkning i `Linberäkning.xlsm`, med makrona i `5420_Verktyg.xlam`. Arket och makrona har elva fel, D1–D11. Kalkyl har därför två lägen:

- **Excel** räknar exakt som arket, också felen. Det är kontrollerat mot arket: ett fall stämmer på 15 decimaler, och tio fall stämmer med arkets visade värden.
- **Ny** rättar felen och är förvalt.

Med standardlastfallen skiljer lägena oftast bara i femte eller sjätte värdesiffran, eftersom läget Ny räknar med exakt π (D10). Större skillnader uppstår när D1, D4 eller D5 slår till, vilket beror på linan, normalspannet och uppspänningen. Felen D2, D3, D6–D9 och D11 märks bara när man ändrar lastfallen, använder en korsningsmall eller hamnar i ett gränsfall.

## D1. E-modulen i transponeringen

- **Excel:** använder E-modulen från det sista Ei-lastfallet där Eᵢᵥ räknades, inte transponeringslastfallets.
- **Ny:** använder transponeringslastfallets E-modul.
- **Varför:** transponeringen gäller tillståndet i transponeringslastfallet.

## D2. Lastfall-ID för vindparametrar till det andra stadiet

- **Excel:** läser en rad fel när ID:t pekar från Ei till Ep eller tvärtom. ID 7 och 8 hamnar på tomraden, så vindtrycket blir 0.
- **Ny:** läser lastfallet med det angivna ID:t.
- **Varför:** kolumnen anger ett lastfall-ID. Felet kommer av hur makrot räknar fram raden.

## D3. Vindparametrar från ett lastfall med manuell last

- **Excel:** vindtrycket blir 0, eftersom arket tömmer raden vid manuell last.
- **Ny:** räknar vindtrycket ur det lastfallets vindindata.
- **Varför:** den manuella lasten ersätter bara det lastfallets is- och vindlast, inte dess vindparametrar.

## D4. Spänning över σₚ utan tilläggslast vid transponeringen

- **Excel:** räknar Eᵢᵥ med temperaturen 0 och lasten 0, alltså med en viktlös lina, eftersom makrot läser en tom rad.
- **Ny:** räknar Eᵢᵥ vid transponeringstemperaturen med linans egentyngd.
- **Varför:** spänningen gäller linan med egentyngd. E-modulen måste räknas för samma tillstånd.

## D5. Eᵢᵥ i Ep-stadiet när Eₚ = Eᵢₖ

- **Excel:** räknar Eᵢᵥ också i Ep-stadiet. Makrot skiljer stadierna åt genom att jämföra E-modulerna, och när de är lika går det inte.
- **Ny:** räknar aldrig Eᵢᵥ i Ep-stadiet.
- **Varför:** i Ep-stadiet är linan permanent förlängd och räknas med Eₚ.

## D6. Arccos av 1 och −1

- **Excel:** ger π/2, eftersom makrot delar med noll.
- **Ny:** ger 0 respektive π.
- **Varför:** det är de matematiskt rätta värdena.

## D7. Min, max och vindtryck kopieras till fasta rader

- **Excel:** skriver in talen på fasta rader (min i lastfall 1–2, max i 15, vindtrycket i hela kolumnen), också efter en korsningsmall där andra rader ska följa.
- **Ny:** varje lastfall följer sin källa: min, max, kortslutningstemperatur, Normal vind, Hög vind eller eget tal.
- **Varför:** samma indata ska ge samma resultat, oberoende av i vilken ordning de skrevs in.

## D8. Korsningsmallarnas vindtryck

- **Excel:** skriver in 500 N/m² som tal.
- **Ny:** lastfallen följer Normal vind.
- **Varför:** lastfallen avser normal vind, inte talet 500.

## D9. Ändringar för hand skrivs över

- **Excel:** skriver över en temperatur eller ett vindtryck som ändrats för hand, när min, max eller vindtrycket i indata ändras.
- **Ny:** behåller värden som ändrats för hand.
- **Varför:** användarens värden ska inte ändras utan att det syns.

## D10. π

- **Excel:** räknar med konstanten Pi = 3,1416 i islasten, istjockleken, utsvängningsvinkeln och tillståndsekvationen.
- **Ny:** räknar med exakt π. Konstanten Pi visas som π och kan inte ändras.
- **Varför:** 3,1416 är ett avrundat värde, och felet blir upp till 0,0016 % i spänningen.

## D11. Kortslutningstemperaturen vid Övre linan

- **Excel:** avrundar den inskrivna temperaturen till heltal, med avrundning till jämnt tal (100,5 blir 100).
- **Ny:** behåller temperaturen som den skrevs.
- **Varför:** makrot lagrar svaret i en heltalsvariabel. Avrundningen är inte avsiktlig.

## Avsteg i båda lägena

Kalkyl gör detta annorlunda än arket i båda lägena. Inget av det ändrar ett resultat som arket räknar rätt.

- **Fel i indata eller lindata:** felet visas och ingen beräkning görs. Arket räknar vidare eller stannar med ett körfel. Ett resultat ur fel indata ska inte se giltigt ut.
- **Eᵢᵥ:** högst 100 varv, sedan ett fel. Arket har ingen gräns och kan hänga.
- **Hänvisning till en senare rad:** alla vindtryck räknas före lasterna. I arket beror resultatet på förra omräkningen.
- **Avbryt vid Övre linan:** ingenting ändras. Arket sätter då kortslutningstemperaturen till 0 °C.

## Behållet som i arket

Följande räknar Kalkyl som arket i båda lägena. Det är inte prövat mot standarden.

- Isvikten vid vindstilla räknas bara med Gc.
- Vindstilla avgörs av vindtrycket efter faktorerna, inte av magnituden.
- Tillståndsekvationen löses med makrots formler och i samma ordning.
- Stakningsmallen är mallen i lastfall 15, också efter en korsningsmall.
