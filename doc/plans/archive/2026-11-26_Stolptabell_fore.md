# Stolptabell före: Excel-inklistring, redigering, radborttagning, användarens decimaler och sparning

- **Datum:** 2026-11-26
- **Status:** ✅ klar 2026-11-26 i version 0.4.0. Handtestad och granskad. Användarens svar står i avsnitt 16.
- **Version:** 0.4.0 står redan i `project(VERSION …)` i rotens `CMakeLists.txt`. Samordnaren har höjt den, och ändringen är inte committad. Planen och agenterna rör den aldrig.
- **Underlag:** `src/windows/main_window.*`, `src/windows/normal_span_dialog.*`, `libs/sheetGrid`, `libs/tableText` och `libs/projectdb`. Dessutom GlazeUI v0.11.1 (`build/_deps/glazeui-src/MANUAL.md` och källan) och `doc/plans/archive/2026-11-23_Normalspann.md` (3.11, 6.3 och 14).

---

## 1. Mål och avgränsning

### Kraven
| # | Krav (användaren) | Var |
|---|---|---|
| K1 | Data in från Excel som i normalspannsberäkningen: rader, kolumner eller båda. Markören eller markeringen avgör var det klistras in. | 3.3, 5.3, 6 |
| K2 | Varje rad går att redigera för hand i alla fält. | 7.1, 7.2 |
| K3 | En radera-glyf längst till höger på varje rad. Klick ger "Är du säker?" i en dialog som är centrerad. | 3.5, 3.6, 7.4 |
| K4 | Både punkt och komma fungerar som decimaltecken vid inmatning och inklistring, och värdena går att använda oavsett vilket som skrevs. | 4.2, 5.1 |
| K5 | Antalet decimaler följer användaren, per värde: inga decimaler ger inga, 10 decimaler ger 10. | 3.1, 4.2, 4.3 |

### Ingår
- Den mockade tabellen i fliken Stolptabell före blir en riktig, redigerbar tabell med samma åtta kolumner. En nionde smal kolumn längst till höger har radera-knappen.
- Talen lagras som kanonisk decimaltext, så att varje värde behåller exakt sina decimaler.
- Excel-markering, urklipp och inklistring via `libs/sheetGrid`, som får stöd för att växa med nya rader.
- Tabellen sparas i projektdatabasen med ett nytt migreringssteg 4, med migreringstest. Varje ändring skrivs direkt till arbetskopian, som i Dokumentation, och publiceras med Ctrl+S eller automatisk sparning. Alla rader sparas, också tomma rader mitt i tabellen.
- Ett nytt rent bibliotek `libs/poleTable` för radmodellen.
- Skrivskyddat läge.
- Tester i `tests/`.

### Ingår inte
- "Hämta från stolptabellen" i normalspannsformuläret. Stolptabell före är bara ett urval av stolparna, så den ska inte göras (svar 9).
- Stolptabell efter. Den får en egen tabell och ett eget migreringssteg när den görs (svar 8).
- Beräkningar ur koordinaterna, till exempel spannlängd eller vinklar.
- Att normalspannsformuläret byts till `libs/sheetGrid`. Det blir möjligt efter den här planen och står kvar som idé i `TODO.md`.
- Ångra, dragmarkering med musen och fyllning av en markering med ett enda värde (risker i 15).

---

## 2. Nuläge

- **`main_window.glazeui`:** `stolpGrid` har åtta kolumner: Stolpplats (Autosize), Av, Ah, "Spannlängd föreg.", Koordinat N, E och H, och LKL.
  - Kolumn 1–6 har `Editor::Number`, och 4–6 har dessutom `decimals = 2`.
  - Raderna är sex mockrader skrivna med `addRow`.
  - Sorteringen är på (förvalet), och det finns inga händelser.
- **`main_window.cpp`:** `shadeStolpRows()` ger raderna gul växlande ton (`kAmber`, alfa 64 och 52). Ingenting sparas.
- **DataGrids talredigerare kan inte användas:** `Editor::Number` sparar kanonisk text i kortaste form (`numbertext::format` med `decimals = -1`). 44,10 blir 44,1, och `decimals = 2` avrundar. Det bryter mot K5.
- **`normal_span_dialog.*`** emulerar Excel-markeringen själv (ankare och ände, `setCellColor`, Ctrl+C/X/V/A, Delete och snabbmenyn).
  - En överhoppad rubrikrad tar ingen rad, så datat börjar på ankarraden.
  - Tabellen har en tom rad sist för nya rader.
- **`libs/sheetGrid`** har samma beteende som ett bibliotek, men med tre begränsningar:
  - Antalet rader är fast, så fält under sista raden ignoreras.
  - En överhoppad rubrikrad tar en tabellrad, och datat börjar raden under ankaret (`testHeaderRow`).
  - Det finns inget sätt att meddela att rader har lagts till eller tagits bort.
- **`libs/tableText`:**
  - `parseDecimal` läser komma eller punkt (inte båda), mellanslag som tusentalsavgränsare, U+2212 och exponent.
  - `formatDecimal` skriver från ett `double`, så decimalerna som användaren skrev går förlorade.
- **`libs/projectdb`:**
  - Schemat har steg 1–3, och `kSchemaVersion` = 3.
  - Steg 3 är committat i v0.3.0 och ändras aldrig. 0.3.1 och 0.3.2 ändrade inte schemat.
  - Ändringen här blir alltså ett nytt steg 4 (`dev::kStep4`).
  - Det finns inga stolptabellsdata.
- **GlazeUI v0.11.1:**
  - `DataGrid::Editor::Button` ritar en knapp med bilden ur `setCellImage`. Ett tryck som släpps över cellen, Enter eller Space ger `onCellClick` (`CellClickArgs` med `row` och `column`).
  - `glaze::messageBox(owner, …)` öppnas med `StartPosition::CenterOwner`. Startplatsen räknas efter rutans `onLoad`, så den faktiska storleken centreras (`app_window.cpp`, `revealHidden`).

---

## 3. Beslut med motivering

### 3.1 Talen lagras som kanonisk decimaltext, inte som double
- K5 kräver att 44,10 visas som 44,10, och ett double kan inte bära avslutande nollor.
- En koordinat med 10 decimaler, till exempel 6512240,8500000000, har 17 gällande siffror, vilket är mer än ett double säkert återger.
- Därför lagras texten i kanonisk form: punkt som decimaltecken, inga tusentalsavgränsare och exakt de decimaler som användaren skrev (4.2).
- Visning och urklipp skrivs ur texten. Talvärdet för beräkningar tas fram med `parseDecimal` när det behövs (`poletable::numberValue`).

### 3.2 Talkolumnerna blir textceller med egen tolkning
- Kolumn 1–6 får `Editor::Text` med högerjustering.
- Vid `onCommit` tolkas texten med `poletable::acceptField`:
  - Ett giltigt tal skrivs om till visningsformen.
  - Ett ogiltigt tal, till exempel bokstäver, ger ingen ruta. Cellen får bara tillbaka sitt förra värde (7.1, svar 10).
- Talkolumnerna får `ColumnSizing::AutoMin`, så att de växer med antalet decimaler.
- **Varför inte `Editor::Number`, som sväljer bokstäver redan vid tangenttrycket?** Talcellen visar värdet via ett double (`valuecell::displayNumber`, `value_cell.cpp` rad 136). 44,10 visas då som 44,1, och 10 decimaler går förlorade, vilket bryter mot K5. Ett val i GlazeUI för att visa talet som det skrevs står som idé i `TODO.md` (14).

### 3.3 `libs/sheetGrid` utökas och används, och inget tredje exemplar av markeringen skrivs
- Linberäkningen använder biblioteket, och `TODO.md` har redan idén att det ska kunna lägga till och ta bort rader.
- Tre tillägg, där förvalen behåller dagens beteende för Linberäkning:
  - `SheetShape::growRows`: inklistringen får gå förbi sista raden.
  - `SheetShape::headerTakesRow`: om en överhoppad rubrikrad tar en tabellrad.
  - `SheetGrid::rowsChanged()`: fönstret meddelar att rader har lagts till eller tagits bort.
- Normalspannsformuläret byts inte nu, men kan byta till biblioteket senare.

### 3.4 Nytt rent bibliotek `libs/poleTable`
- Radmodellen samlas på ett ställe: kolumnerna, vilka som är tal, tolkningen av ett fält, visningstexten, ändringarna med nya rader och reglerna för tomma rader.
- Den delas av `projectdb` (lagringen och valideringen) och huvudfönstret, och senare av "Hämta från stolptabellen".
- Mönstret är `libs/conductor`, som är gemensamt för systemdb och projectdb.
- Biblioteket är rent och testas utan fönster. `main_window.cpp` är redan nära 2000 rader.

### 3.5 Radera-glyfen: en knappcell sist i raden
- **Lösningen som fungerar i v0.11.1:**
  - Kolumn 8 har `Editor::Button`, tom text och bilden `glaze::stockIcon(glaze::StockIcon::Trash)` per cell (`setCellImage`).
  - Klicket kommer i `onCellClick`, och Enter och Space på cellen gör samma sak.
  - Tab hoppar över knappcellerna, men piltangenterna når dem.
  - Den tomma raden sist har `Editor::None` och ingen bild.
  - Skrivskyddat får inga knappar.
- **Reserven**, om knappramen inte godtas eller knappcellen inte fungerar:
  - Kolumn 8 blir en `Editor::None`-cell med bara bilden.
  - I `onMouseUp` postas en kontroll av `currentRow()` och `currentColumn()`, eftersom trycket gör cellen aktuell.
  - Står den aktuella cellen i kolumn 8 ställs frågan. Det är samma teknik som markeringen synkas med i dag.
- Frågan postas alltid med `glaze::app().post`, bakom en livsspärr, så att tabellen inte ändras mitt i sin egen händelse.

### 3.6 "Är du säker?" är meddelanderutan, centrerad över huvudfönstret
- Anropet är `glaze::messageBox(*this, text, rubrik, MessageBoxButtons::YesNo, MessageBoxType::Warning)`.
- Rutan har `CenterOwner`: mitt över huvudfönstrets klientyta, på den skärm där fönstrets mitt ligger, och klämd in i skärmens arbetsyta.
  - Ligger fönstret på skärm 2 visas rutan där.
  - Är fönstret minimerat eller dolt centreras rutan på skärmen.
- Det blir ingen egen dialog. GlazeUI:s ruta står mitt över fönstret och annars mitt på skärmen, vilket är vad användaren vill (svar 4).
- I YesNo är Ja förvalt (Enter), som användaren vill (svar 5).

### 3.7 Sparning: varje ändring direkt i arbetskopian, hela tabellen per skrivning
- Varje sparad cell, inklistring, Delete och radborttagning skriver hela tabellen med `setPolesBefore` i en transaktion, som Dokumentation gör.
- Publiceringen sker som i dag, med Ctrl+S och den automatiska sparningen.
- Ingen skrivning görs när det som ska sparas är lika med det senast sparade (`poleSaved_`), till exempel när en cell sparas med samma värde.
- Några hundra rader per transaktion mot den lokala arbetskopian tar millisekunder.

### 3.8 En tom rad sist för nya rader
- Som i normalspannsformuläret skriver man i den tomma raden sist för att lägga till en stolpe, och en ny tom rad kommer då.
- Snabbmenyn har "Infoga rad ovanför".

### 3.9 Sorteringen stängs av
- `stolpGrid.setSortable(false)`. Ett klick i rubriken hade ändrat visningsordningen, och markeringen och inklistringen räknar med radernas index.

### 3.10 En tabell bara för Stolptabell före
- Tabellen `pole_before` har ingen kolumn för skede. Stolptabell efter får en egen tabell i ett senare steg (svar 8).

### 3.11 Tusentalsavgränsare i visningen prövas
- Talen visas med heltalsdelen grupperad i tretal med hårt mellanslag (U+00A0), till exempel 6 512 240,85 (svar 3: "vi kan testa").
- Urklippet till Excel skrivs utan gruppering, så att Excel alltid läser talet.
- Grupperingen styrs av en enda konstant, `kGroupThousands` i `pole_table.cpp`, så att den kan stängas av efter handtestet.

---

## 4. Datamodell

### 4.1 Kolumnerna
| Index | Rubrik i tabellen | Slag | `PoleRow`-fält | Databaskolumn | Storlek |
|---|---|---|---|---|---|
| 0 | Stolpplats | text | `pole` | `pole TEXT NOT NULL DEFAULT ''` | Autosize, bredd 120 |
| 1 | Av | tal | `av` | `av TEXT` | AutoMin |
| 2 | Ah | tal | `ah` | `ah TEXT` | AutoMin |
| 3 | Spannlängd föreg. (två rader) | tal | `spanM` | `span_m TEXT` | AutoMin |
| 4 | Koordinat N (två rader) | tal | `north` | `coord_n TEXT` | AutoMin |
| 5 | Koordinat E (två rader) | tal | `east` | `coord_e TEXT` | AutoMin |
| 6 | Koordinat H (två rader) | tal | `height` | `coord_h TEXT` | AutoMin |
| 7 | LKL | text | `lkl` | `lkl TEXT NOT NULL DEFAULT ''` | AutoMin |
| 8 | (tom rubrik, tooltip "Radera raden") | knapp | – | – | AutoMin |

### 4.2 Kanonisk decimaltext
`tabletext::normalizeDecimal(text)` gör om det användaren skrev eller klistrade in till kanonisk form:

1. Blanktecken tas bort överallt, eftersom de är tusentalsavgränsare: mellanslag, tabb, CR, LF, U+00A0 (hårt mellanslag, Excel på svenska) och U+202F.
2. U+2212 (riktigt minustecken) blir `-`.
3. Blir texten tom svarar funktionen med `""`, alltså en tom cell.
4. Ett tecken först, `+` eller `-`, är tillåtet. `+` stryks.
5. Resten består bara av siffror 0–9 och högst ett decimaltecken totalt, `,` eller `.`, och minst en siffra.
   - Allt annat ger tomt svar (`std::nullopt`, inte ett tal): bokstäver, exponent (`e` eller `E`), flera decimaltecken och både komma och punkt.
6. Heltalsdelen förlorar sina inledande nollor, och minst `0` blir kvar.
7. Decimalerna behålls exakt, med avslutande nollor. Utan decimalsiffror blir det ingen punkt.
8. Är alla siffror noll stryks minustecknet.

| Inmatat | Kanoniskt | Visas |
|---|---|---|
| `6512240,8500000000` | `6512240.8500000000` | `6512240,8500000000` |
| `6512240.85` | `6512240.85` | `6512240,85` |
| `6 512 240,85` (mellanslag eller hårt mellanslag) | `6512240.85` | `6512240,85` |
| `44` | `44` | `44` |
| `44,10` | `44.10` | `44,10` |
| `-12,5` och `−12,5` | `-12.5` | `-12,5` |
| `+3` | `3` | `3` |
| `-0,00` | `0.00` | `0,00` |
| `,5` | `0.5` | `0,5` |
| `5,` | `5` | `5` |
| `007,50` | `7.50` | `7,50` |
| tomt eller bara blanksteg | `""` | tomt |
| `abc`, `1,2,3`, `1.234,5`, `1e5`, `6,51E+06`, `-`, `,` | ogiltigt | – |

- **Visning och urklipp:** decimaltecknet är alltid komma (svar 2). Kolumnen Visas ovan är utan gruppering.
  - **Urklipp:** `tabletext::formatDecimalText(kanoniskt, ',')` byter bara punkten mot komma. Ingen gruppering och ASCII-minus, så att Excel läser talet tillbaka.
  - **Tabellen:** `tabletext::formatDecimalText(kanoniskt, ',', true)` grupperar dessutom heltalsdelen (3.11), till exempel 6 512 240,8500000000. Decimalerna grupperas inte.
- **Ett decimaltecken är alltid ett decimaltecken.** `6,512` är 6,512 och inte 6512. Två avgränsare (amerikanskt `6,512,240.85` eller `6.512.240,85`) avvisas med ett meddelande i stället för att gissas.

### 4.3 Tio decimaler och fler
- Displayen, urklippet och databasen läser bara texten. Inget steg går via ett double, så det blir inget avrundningsfel i visningen oavsett antalet decimaler.
- Talvärdet för beräkningar (`poletable::numberValue`) är `parseDecimal` på texten. Det har cirka 15–16 gällande siffror, alltså cirka 1e-9 m för en N-koordinat, vilket räcker för beräkningar.
- Excel håller själv 15 gällande siffror, så ett tal med fler decimaler som kopieras ut ur Kalkyl avrundas av Excel.

### 4.4 Tomma celler, negativa tal och exponent
- **Tom cell:** tom text i `PoleRow`, NULL i talkolumnerna i databasen och `''` i textkolumnerna. Ett tomt fält i en inklistring tömmer cellen, som i Excel.
- **Negativa tal** är tillåtna i alla talkolumner, eftersom till exempel Av och H kan vara negativa.
- **Exponent avvisas.** Excel kopierar den visade texten, och en för smal kolumn i Excel ger `6,5E+06`, vilket hade tappat precision utan att märkas. Meddelandet får användaren att bredda kolumnen i Excel (svar 13).

### 4.5 Textkolumnerna
- Stolpplats och LKL (till exempel `255/262`) lagras trimmade (ASCII-blanktecken i början och slutet) och visas som de är. Inga andra regler.
- LKL är längdklassen, alltså en fackverkstolpes längd i decimeter (262 = 26,2 m). Den kan ha två värden med snedstreck, så den är text.

### 4.6 Var sanningen finns
- `MainWindow::poleRows_` (`std::vector<poletable::PoleRow>`) är modellen: datarader utan den tomma raden sist.
- Tabellen visar `displayText` för varje cell. Rad `poleRows_.size()` är den tomma raden sist.
- Databasen har `rowsToSave(poleRows_)`, och `poleSaved_` är det senast sparade.
  - Alla rader sparas, också helt tomma rader mitt i tabellen (svar 11).
  - Bara tomma rader sist tas bort, eftersom de inte går att skilja från den tomma raden för nya stolpar.

---

## 5. API (bindande)

### 5.1 `libs/tableText/table_text.h`, nya funktioner
```cpp
// Ett tal ur inmatad eller inklistrad text som kanonisk decimaltext: valfritt minustecken,
// heltalssiffror utan inledande nollor och punkt följd av decimalerna exakt som de skrevs.
// Komma eller punkt (högst ett totalt), blanksteg (också U+00A0 och U+202F) som
// tusentalsavgränsare, + och U+2212 tillåtna, ingen exponent. Tomt eller bara blanktecken ger ""
// (tom cell); text som inte är ett tal ger tomt svar.
std::optional<std::string> normalizeDecimal(std::string_view text);

// Kanonisk decimaltext för visning och urklipp: punkten byts mot separator. Med group
// grupperas heltalsdelen i tretal med hårt mellanslag (U+00A0); decimalerna grupperas aldrig.
std::string formatDecimalText(std::string_view canonical, char separator = ',', bool group = false);

// Antalet decimaler i kanonisk decimaltext: "12.500" ger 3, "12" ger 0.
int decimalsOf(std::string_view canonical);
```
- `parseDecimal`, `formatDecimal`, `parseTsv`, `formatTsv` och `planPaste` är oförändrade.
- I källan skrivs U+00A0, U+202F och U+2212 som tecken byggda med `char` (som `groupedText` i `normal_span_dialog.cpp`), utan escape-sekvenser.

### 5.2 `libs/poleTable/pole_table.h` (nytt bibliotek `kalkyl_poletable`, namnrymden `kalkyl::poletable`)
```cpp
#pragma once
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace kalkyl::poletable {

// Kolumnerna i stolptabellen i tabellens ordning. Kolumn 1-6 är tal, 0 och 7 är text.
// Radera-kolumnen hör till fönstret och räknas inte här.
inline constexpr int kPole = 0;       // Stolpplats
inline constexpr int kAv = 1;         // Av
inline constexpr int kAh = 2;         // Ah
inline constexpr int kSpan = 3;       // Spannlängd föreg.
inline constexpr int kNorth = 4;      // Koordinat N
inline constexpr int kEast = 5;       // Koordinat E
inline constexpr int kHeight = 6;     // Koordinat H
inline constexpr int kLkl = 7;        // LKL
inline constexpr int kColumnCount = 8;

// En rad i stolptabellen. Talen är kanonisk decimaltext (tabletext::normalizeDecimal): punkt
// som decimaltecken, inga tusentalsavgränsare och exakt de decimaler användaren skrev.
// Tom text = tom cell. Textfälten är trimmade.
struct PoleRow {
    std::string pole;
    std::string av;
    std::string ah;
    std::string spanM;
    std::string north;
    std::string east;
    std::string height;
    std::string lkl;
    bool operator==(const PoleRow&) const = default;
};

// Sant för talkolumnerna 1-6.
bool isNumberColumn(int column);

// Kolumnens namn i meddelanden, t.ex. "Koordinat N". Tom text utanför 0-7.
std::string columnName(int column);

// Fältet för kolumnen 0-7.
const std::string& field(const PoleRow& row, int column);
std::string& field(PoleRow& row, int column);

// Texten som den lagras: text trimmas, tal blir kanonisk decimaltext ("" för tomt).
// Tomt svar = passar inte: inte ett tal i en talkolumn, eller kolumnen finns inte.
std::optional<std::string> acceptField(int column, std::string_view text);

// Cellens text i tabellen: tal med decimalkomma, de lagrade decimalerna och heltalsdelen
// grupperad när kGroupThousands är på; text som den är.
std::string displayText(const PoleRow& row, int column);

// Cellens text i urklippet: som displayText men alltid utan gruppering, så att Excel läser talet.
std::string copyText(const PoleRow& row, int column);

// Talet i en talkolumn, för beräkningar. Tomt för tom cell, textkolumn eller ogiltig text.
std::optional<double> numberValue(const PoleRow& row, int column);

// Sant när alla åtta fält är tomma.
bool isEmptyRow(const PoleRow& row);

// Första talkolumnen vars text varken är tom eller kanonisk decimaltext, annars tomt svar.
std::optional<int> firstInvalidColumn(const PoleRow& row);

// En ändrad cell: rad, kolumn 0-7 och texten som acceptField gav.
struct Edit { int row = 0; int column = 0; std::string text; };

// Skriver ändringarna. En rad bortom slutet ger nya tomma rader fram till och med den.
// Ändringar med negativ rad eller kolumn utanför 0-7 ignoreras.
void applyEdits(std::vector<PoleRow>& rows, const std::vector<Edit>& edits);

// Tar bort helt tomma rader sist i listan.
void trimTrailingEmptyRows(std::vector<PoleRow>& rows);

// Raderna som sparas: alla rader i ordning, också tomma rader mitt i. Bara helt tomma rader
// sist tas bort.
std::vector<PoleRow> rowsToSave(const std::vector<PoleRow>& rows);

} // namespace kalkyl::poletable
```

`libs/poleTable/CMakeLists.txt`:
```cmake
# Stolptabellens rader: kolumnerna, talen som decimaltext och ändringarna. Beror på tableText.
add_library(kalkyl_poletable STATIC pole_table.cpp)
target_include_directories(kalkyl_poletable PUBLIC ${PROJECT_SOURCE_DIR}/libs)
target_link_libraries(kalkyl_poletable PRIVATE kalkyl_tabletext kalkyl_warnings)
```
- `libs/CMakeLists.txt` får `add_subdirectory(poleTable)` direkt efter `tableText`, och kommentaren om beroendena uppdateras.

### 5.3 `libs/sheetGrid/sheet_grid.h`, tillägg
Nya fält läggs **sist** i structarna, så att befintlig kod inte påverkas:
```cpp
struct SheetShape {
    // ... som i dag ...
    // Inklistringen får gå förbi sista raden: fält under tabellen blir ändringar i nya rader,
    // som fönstret lägger till i applyEdits. SheetGrid läser då radantalet ur tabellen före
    // varje åtgärd, och rowCount är bara startvärdet.
    bool growRows = false;
    // En överhoppad rubrikrad tar en rad i tabellen (true, som arket Linberäkning) eller ingen,
    // så att datat börjar på ankarraden (false, som normalspannet och stolptabellen).
    bool headerTakesRow = true;
};

struct SheetPaste {
    // ... som i dag ...
    // Radantalet tabellen behöver för ändringarna: max(rowCount, sista ändrade raden + 1).
    int rowsNeeded = 0;
};

class SheetGrid {
public:
    // ... som i dag ...
    // Fönstret har lagt till eller tagit bort rader: radantalet läses om, markeringen klampas
    // och alla celler målas om.
    void rowsChanged();
private:
    // Med growRows: shape_.rowCount = grid_.rowCount(). Körs först i varje åtgärd.
    void syncRowCount();
};
```

**`planSheetPaste`:**
- Med `growRows` landar ett fält också på rad >= `rowCount`. `skipRow` och `isInput` frågas för de nya raderna, och fönstret svarar efter kolumnen.
- Med `growRows` klampas `area.row1` inte till sista raden.
- Med `headerTakesRow == false` och en överhoppad rubrikrad landar blockets andra rad på ankarraden, och `area.row1` blir ankaret + antalet datarader − 1.
- Radnumret i avbrottsmeddelandet räknas som i dag i urklippets rader, med rubrikraden inräknad.
- `rowsNeeded` sätts alltid.
- Utan de nya fälten är allt som i dag, och de befintliga sheetgrid-testerna går igenom oförändrade.

**`SheetGrid`:**
- `syncRowCount()` körs först i `keyDown`, `mouseUp`, `contextMenu`, `paintAll`, `paintCell`, `select`, `copySelection` och `pasteAtSelection`.
- `syncRowCount()` körs igen direkt efter `host_.applyEdits` i `pasteAtSelection` och `clearSelectionCells`.
- `rowsChanged()` = `syncRowCount()`, klampning av ankare och ände (`clampPos`) och `paintAll()`.
- Statustexten efter en inklistring blir "Klistrade in n rader (rubrikraden hoppades över)" när `skippedHeader` är sant, annars som i dag. Det gäller också Linberäkning.

### 5.4 `libs/projectdb`

**Steg 4 sist i `dev_migration.h`:**
```cpp
// Steg 4 (user_version 3 -> 4): stolptabellen i fliken Stolptabell före. IF NOT EXISTS som i
// steg 2. Talen är TEXT, så att användarens decimaler bevaras exakt.
inline constexpr std::string_view kStep4 = R"sql(
-- Fliken Stolptabell före: en rad per stolpe i tabellens ordning (position 0, 1, 2 ...),
-- också tomma rader mitt i tabellen.
-- Talen är kanonisk decimaltext (punkt, inga tusentalsavgränsare, användarens decimaler kvar).
-- TEXT-affiniteten gör att SQLite aldrig gör om dem till tal. NULL = tom cell.
CREATE TABLE IF NOT EXISTS pole_before (
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
)sql";
```

**`project_db.h` och `project_db.cpp`:**
- `kSchemaVersion = 4`.
- `s.steps = {kSchemaV1, dev::kStep2, dev::kStep3, dev::kStep4}`.
- `kExpectedTablesSql` får `'pole_before'`, och `kExpectedTableCount = 10`.

**`project_data.h`**, efter linberäkningarna, med `#include "poleTable/pole_table.h"`:
```cpp
// Stolptabellen i fliken Stolptabell före, i tabellens ordning. En tom tabell ger en tom lista.
core::Result<std::vector<poletable::PoleRow>> polesBefore(ProjectDb& p);
// Ersätter hela Stolptabell före i EN transaktion med poletable::rowsToSave(rows): tomma rader
// mitt i sparas, tomma rader sist inte. Positionerna blir 0, 1, 2 ... Talen ska vara kanonisk
// decimaltext (poletable::acceptField).
// Fel: AccessDenied, Invalid ("Stolptabellen har ett ogiltigt tal på rad <n> i kolumnen <namn>."), Sql.
core::Result<void> setPolesBefore(ProjectDb& p, const std::vector<poletable::PoleRow>& rows);
```

**`project_data.cpp`:**
- `setPolesBefore` går genom `write(p, …)`:
  1. Validering med `firstInvalidColumn` före allt annat. Radnumret i meddelandet är 1-baserat i indatat.
  2. `DELETE FROM pole_before`.
  3. En `INSERT` per rad i `rowsToSave(rows)`. Tomma tal binds som NULL.
- `polesBefore` läser med `ORDER BY position`. NULL blir tom text (`Statement::text`).
- **`CMakeLists.txt`:** `kalkyl_poletable` läggs till som PUBLIC, eftersom headern inkluderar den.

---

## 6. Inklistringsregler

Inklistringen går alltid från markeringens övre vänstra cell (`SheetGrid::pasteAtSelection`). Det är den aktuella cellen när inget block är markerat.

| Fall | Vad som händer |
|---|---|
| En cell, utan redigering | Värdet tolkas med `acceptField` och hamnar i cellen. |
| En cell, under redigering | DataGrids redigerare klistrar in vid textmarkören. Tolkningen sker när cellen sparas (7.1). |
| Ett block under redigering | Redigeringen avbryts, och blocket klistras in från den redigerade cellen. |
| Rad, kolumn eller block | Från ankaret åt höger och nedåt. Varje fält landar i den kolumn det hamnar i. Det finns ingen fast ordning. |
| Förbi sista kolumnen (LKL) | Fälten ignoreras. Radera-kolumnen tar aldrig emot något. |
| Förbi sista raden | Nya rader skapas (`growRows`), och en ny tom rad kommer sist. Gäller också inklistring som börjar i den tomma raden. |
| Rubrikrad | Har blocket mer än en rad, och har första raden ett fält som inte är tomt och som en talkolumn avvisar, hoppas raden över. Datat börjar på ankarraden (`headerTakesRow = false`), och statusraden säger att rubrikraden hoppades över. Täcker blocket bara textkolumner kan ingen rubrik kännas igen, och den klistras in som data (risk 15.9). |
| Ogiltigt tal | Hela inklistringen avbryts innan något ändras. Rutan säger fältet, raden i urklippet och kolumnen (sheetGrids meddelande). |
| Komma och punkt i samma urklipp | Varje fält tolkas för sig, så båda fungerar sida vid sida. Inom ett fält är bara ett decimaltecken tillåtet. |
| Tomt fält | Tömmer cellen, som i Excel. |
| Skrivskyddat | Ingen inklistring. Ctrl+C och Ctrl+A fungerar. |

Efter inklistringen blir markeringen det inklistrade (`select(area)`), tabellen sparas och titeln får `*`.

---

## 7. Redigering, nya rader och radborttagning

### 7.1 Redigera en cell (`stolpGridCommitted`)
- Alla celler i kolumn 0–7 är textceller: dubbelklick, F2, Enter eller att börja skriva. `setEditOnClick(false)`, som i formuläret.
- Vid `onCommit` gäller kolumn 0–7.
  - **Talkolumn med ogiltig text**, till exempel bokstäver:
    - `setCell` med det förra värdets visningstext.
    - Ingen ruta och inget meddelande. Inget sparas (svar 10).
  - **Giltig text:**
    - `field(poleRows_[r], c) = text`.
    - `setCell(r, c, displayText(…))`, så att till exempel `6512240.85` visas som `6 512 240,85` (med gruppering) och `44.10` som `44,10`.
    - `savePoleTable()`.

### 7.2 Nya rader
- **Skriver användaren i den tomma raden sist:**
  - Är det sparade värdet tomt händer inget.
  - Annars `poleRows_.push_back({})`, ändringen skrivs, och en ny tom rad läggs till med `addRow`.
  - `decoratePoleRow` körs för båda raderna, så att den förra tomma raden får sin knapp.
  - `poleSheet_->rowsChanged()` och `savePoleTable()`.
- Ingen `setRows` körs inne i `onCommit`.
- **"Infoga rad ovanför"** i snabbmenyn infogar en tom rad i `poleRows_` vid markeringens första rad (inte vid den tomma raden sist) och bygger om tabellen med `fillPoleGrid()`.
  - Markeringen hamnar på (raden, 0).
  - `savePoleTable()`, eftersom tomma rader mitt i sparas.

### 7.3 Delete, Ctrl+X, Ctrl+C och Ctrl+A
- **sheetGrid** sköter tangenterna.
  - Delete och Ctrl+X ger tomma ändringar till `applyPoleEdits`.
  - Ctrl+C kopierar `copyText`, alltså tal med decimalkomma och alla decimaler, utan gruppering.
  - Ctrl+A markerar allt, också den tomma raden sist.
- **`applyPoleEdits`:**
  1. Gör om ändringarna till `poletable::Edit`.
  2. Kör `applyEdits` och `trimTrailingEmptyRows`.
  3. Bygger om med `fillPoleGrid()` och kör `savePoleTable()`.
- Ctrl+A och Delete tömmer alltså hela tabellen så att bara den tomma raden finns kvar, som i formuläret.
- Det finns ingen fråga och ingen ångra (svar 12).

### 7.4 Radera-glyfen och "Är du säker?"
1. `stolpGridCellClicked(e)`: om `e.column == kPoleDeleteColumn`, projektet inte är skrivskyddat och `0 <= e.row < poleRows_.size()` postas `removePoleRowsAsked(e.row, e.row)` bakom `alive_`.
2. `removePoleRowsAsked(first, last)`:
   1. `commitEdits()`. Raderna klampas till dataraderna, och utan rader händer inget.
   2. **Text vid en rad:** "Radera stolpplats ST0381?" (svar 5).
      - Utan stolpplats: "Radera rad 4?"
   3. **Text vid flera rader:** "Radera stolpplats ST0378 – ST0380 (3 rader)?"
   4. **Rubrik:** "Radera".
   5. **Ruta:** `glaze::messageBox(*this, text, rubrik, glaze::MessageBoxButtons::YesNo, glaze::MessageBoxType::Warning)`, centrerad över huvudfönstret (3.6).
   6. **Om svaret är Ja:**
      - Raderna tas bort ur `poleRows_`, och tabellen byggs om med `fillPoleGrid()`.
      - Markeringen hamnar på (min(first, antalet rader), 0).
      - `savePoleTable()`, och statusraden visar "Raderade n rader."
3. Ja är förvalt (Enter), och Nej gör ingenting.

### 7.5 Snabbmenyn
- sheetGrids poster: Klipp ut, Kopiera, Klistra in, Radera innehåll och Markera alla.
- Efter en separator kommer två egna poster ur `PoleSheetHost::addMenuItems`:
  - **"Infoga rad ovanför":** aktiv när projektet inte är skrivskyddat och markeringens första rad är en datarad.
  - **"Radera rader…":** `removePoleRowsAsked(sel.row0, min(sel.row1, antalet rader − 1))`. Aktiv när projektet inte är skrivskyddat och minst en datarad är markerad.

### 7.6 Skrivskyddat
- Kolumn 0–7 får `setColumnEditor(c, {.editor = glaze::DataGrid::Editor::Text, .readOnly = true})`.
- Kolumn 8 får `{glaze::DataGrid::Editor::None}` utan bilder.
- `poleSheet_->setReadOnly(true)`.
- `stolpGridCellClicked` och `stolpGridCommitted` gör ingenting.
- Menyposterna är inaktiva.

---

## 8. Fönstret: tre filer

Regionen GlazeBuilder ändras för hand i grammatikens form, och `.glazeui` hålls fri från logik (inga if, loopar eller lokala variabler, en sats per rad). Designraden `// glazeui-design: ui=Slate theme=slate(Windows) style=0 lang=sv` står kvar.

### 8.1 `main_window.h`
- **Regionen:** `glaze::Label stolpInfo;` direkt efter `glaze::Label stolpRubrik;`.
- **Efter regionen:**
  - `#include "poleTable/pole_table.h"`, `#include "sheetGrid/sheet_grid.h"` och `<memory>`.
  - Handlers:
    - `void stolpGridCommitted(glaze::Widget* sender, glaze::CellChangeArgs& e);`
    - `void stolpGridCellClicked(glaze::Widget* sender, glaze::CellClickArgs& e);`
    - `void stolpGridKeyDown(glaze::Widget* sender, glaze::KeyArgs& e);`
    - `void stolpGridMouseUp(glaze::Widget* sender, glaze::MouseArgs& e);`
    - `void stolpGridContextMenu(glaze::Widget* sender, glaze::ContextMenuArgs& e);`
  - Den nästlade adaptern, med mönstret från `LineCalcDialog::GridHost`:
    ```cpp
    // Adaptern mellan stolptabellen och SheetGrid: vad en cell kopieras som (poletable::copyText),
    // radtonen, inklistrade ändringar och snabbmenyns egna poster (Infoga rad ovanför, Radera rader…).
    class PoleSheetHost : public kalkyl::sheetgrid::SheetHost {
    public:
        explicit PoleSheetHost(MainWindow& window) : window_(window) {}
        std::string copyText(int row, int column) const override;
        std::optional<gfx::Color> baseColor(int row, int column) const override;
        void applyEdits(const std::vector<kalkyl::sheetgrid::CellEdit>& edits) override;
        void addMenuItems(glaze::Menu& menu, const kalkyl::sheetgrid::CellRect& selection) override;
        void showStatus(const std::string& text) override;
    private:
        MainWindow& window_;
    };
    ```
  - Hjälpfunktioner med 2–5 raders kommentarer:
    - `loadPoleTable()`
    - `fillPoleGrid()`
    - `decoratePoleRow(int row)`
    - `gfx::Color poleTone(int row) const`
    - `applyPoleEdits(const std::vector<kalkyl::sheetgrid::CellEdit>& edits)`
    - `bool savePoleTable()`
    - `insertPoleRowAbove(int row)`
    - `removePoleRowsAsked(int first, int last)`
    - `setPoleStatus(const std::string& text)`
  - Medlemmar, före `saveWorker_` (som står sist):
    - `std::vector<kalkyl::poletable::PoleRow> poleRows_;` — raderna, utan den tomma raden sist.
    - `std::vector<kalkyl::poletable::PoleRow> poleSaved_;` — det senast sparade.
    - `std::unique_ptr<PoleSheetHost> poleHost_;`
    - `std::unique_ptr<kalkyl::sheetgrid::SheetGrid> poleSheet_;`
    - `int gridStatusPanel_ = -1;` — statusradens panel för tabellens meddelanden.
    - `std::shared_ptr<bool> alive_ = std::make_shared<bool>(true);` — spärr för postade anrop. `MainWindow` är en lokal i `main`-slingan, och en postad lambda får inte nå ett rivet fönster.
- **Tas bort:** `shadeStolpRows()`.

### 8.2 `main_window.glazeui`
- **`stolpInfo`** (ny), efter `stolpRubrik`:
  - `setText` med texten nedan.
  - `setRole(glaze::Label::Role::Dim)`, `setWrap(true)`, `x = 16`, `anchorTop = {"stolpRubrik", 4}`, `anchors = {true, true, true, false}`, `right = 16`.
  - Text: "Klistra in från Excel med Ctrl+V i den cell där inklistringen ska börja: en cell, rader, kolumner eller ett block. Komma och punkt fungerar båda som decimaltecken, och talen visas med så många decimaler som du skriver. Skriv i den tomma raden sist för att lägga till en stolpe, och ta bort en rad med papperskorgen längst till höger."
- **`stolpGrid`:**
  - `anchorTop = {"stolpInfo", 8}`, `setSortable(false)`, `setEditOnClick(false)` och `setHeaderLines(2)` (som i dag).
  - `setColumns` med nio kolumner enligt 4.1:
    - Talkolumnerna `.align = glaze::Align::Right, .sizing = glaze::ColumnSizing::AutoMin`.
    - Rubrikerna med två rader skrivs som i dag.
    - Kolumn 8 är `{.text = "", .align = glaze::Align::Center, .sizing = glaze::ColumnSizing::AutoMin}`.
  - `setColumnEditor(1..6, {glaze::DataGrid::Editor::Text})` ersätter `Number`, och `decimals = 2` försvinner.
  - `setColumnEditor(8, {glaze::DataGrid::Editor::Button})`.
  - Tooltips på kolumnrubrikerna (svar 15). Talkolumnerna får dessutom meningen "Komma eller punkt som decimaltecken, och decimalerna visas som du skriver dem."
    - Av: "Vertikal linlängd (m) som belastar stolpen."
    - Ah: "Horisontell linlängd (m), för vindberäkningar."
    - Spannlängd föreg.: "Spannlängden (m) till föregående stolpe."
    - Koordinat N, E och H: "Koordinat N (m)" och så vidare.
    - LKL: "Längdklass: fackverkstolpens längd i decimeter, till exempel 262 = 26,2 m."
    - Kolumn 8: "Radera raden".
  - **De sex `addRow`-raderna tas bort.** Raderna byggs i kod, som i Dokumentation.
- **Händelser:**
  - `stolpGrid.onCommit` → `stolpGridCommitted`
  - `onCellClick` → `stolpGridCellClicked`
  - `onKeyDown` → `stolpGridKeyDown`
  - `onMouseUp` → `stolpGridMouseUp`
  - `onContextMenu` → `stolpGridContextMenu`
  - Fullformen `[this](glaze::Widget* s, glaze::CellClickArgs& e) { … }`.
- **Trädet:** `stolpPanel.adopt(stolpRubrik); stolpPanel.adopt(stolpInfo); stolpPanel.adopt(stolpGrid);`. Syskonankaren pekar bara bakåt.

### 8.3 `main_window.cpp`
- **Konstant** i den anonyma namnrymden: `constexpr int kPoleDeleteColumn = kalkyl::poletable::kColumnCount;`.
- **`onLoad`**, där anropet av `shadeStolpRows()` ersätts:
  1. Lägg till `statusBar.addPanel("")` och `gridStatusPanel_` direkt efter `savePanel_`.
  2. `loadPoleTable()`.
  3. Skapa `poleHost_` och `poleSheet_` med formen:
     - `rowCount = stolpGrid.rowCount()`, `columnCount = poletable::kColumnCount`, `firstColumn = 0`.
     - `isInput`: kolumn 0–7.
     - `accept`: `poletable::acceptField`.
     - `columnName`: `poletable::columnName`.
     - `growRows = true`, `headerTakesRow = false`.
  4. Skrivskyddat: kör 7.6.
  5. `fillPoleGrid()`.
- **`loadPoleTable()`:** `projectdb::polesBefore(*project_)`.
  - Vid fel: loggning, `messageBox` med "Stolptabellen kunde inte läsas." + radbrytning + felet, och en tom tabell.
  - Sätter `poleRows_` och `poleSaved_`.
- **`fillPoleGrid()`:**
  1. Sparar den aktuella cellen.
  2. `setRows` med en rad per `poleRows_` (`displayText` för 0–7 och `""` för 8) och den tomma raden sist.
  3. `decoratePoleRow` för varje rad.
  4. Sätter tillbaka den aktuella cellen (klampad), eftersom `setRows` nollar den.
  5. `poleSheet_->rowsChanged()`, som målar kolumn 0–7 med `baseColor`.
- **`decoratePoleRow(r)`:**
  - Kolumn 8 får `poleTone(r)`.
  - En datarad i ett skrivbart projekt får `setCellImage(r, 8, glaze::stockIcon(glaze::StockIcon::Trash))` och `setCellToolTip(r, 8, "Radera raden")`.
  - Glyfen tas ur GlazeUI:s stock-ikoner (svar 6). Papperskorgen är förvalet. Implementeraren går igenom `StockIcon` i manualen och väljer en annan bara om den passar bättre.
  - Annars `setCellEditor(r, 8, {glaze::DataGrid::Editor::None})` och ingen bild.
- **`poleTone(r)`:** `kAmber` med alfa 64 på jämna rader och 52 på udda, som `shadeStolpRows` i dag. `PoleSheetHost::baseColor` svarar med den.
- **`savePoleTable()`:**
  - `toSave = rowsToSave(poleRows_)`. Är den lika med `poleSaved_` svarar funktionen true.
  - Annars `setPolesBefore`.
  - Vid fel: loggning, `messageBox` och omläsning (`loadPoleTable()` + `fillPoleGrid()`), och svaret false.
  - Annars sätts `poleSaved_`, `updateTitle()` körs och svaret är true.
- **`stolpGridKeyDown`, `stolpGridMouseUp` och `stolpGridContextMenu`:** skickar vidare till `poleSheet_` (`keyDown`, `mouseUp`, `contextMenu`).
- **Postade anrop** går genom en `postIfAlive(alive_, fn)` som i `normal_span_dialog.cpp`.
- **`setPoleStatus`** skriver i `gridStatusPanel_`. `PoleSheetHost::showStatus` anropar den.

---

## 9. Sparning och migrering

- Projekt från v0.1.0, v0.2.0 och v0.3.x (`user_version` 1–3) migreras med de steg som saknas. Steg 4 skapar den tomma tabellen.
- Inga data går förlorade. Mockraderna sparades aldrig, så det finns inget att flytta.
- `kSchemaVersion` = 4. Steg 4 kan ändras fritt tills det committas. Därefter rättas fel med ett nytt steg.
- Schemakontrollen räknar tio tabeller.
- **Migreringstestet `testMigrateFromV030`** (10.3) skapar filen som den såg ut i den senaste commiten (d598beb, v0.3.2), öppnar den med den nya koden och kontrollerar att allt finns kvar och att stolptabellen fungerar.

---

## 10. Tester (ctest i `tests/`)

### 10.1 `tabletext_test.cpp` (utökas)
- `normalizeDecimal` med alla rader i tabellen i 4.2, också hårt och smalt mellanslag och U+2212 (byggda med `char`).
- `formatDecimalText("6512240.8500000000") == "6512240,8500000000"` och med separatorn `'.'`.
- Med `group`: `"6512240.8500000000"` ger 6, hårt mellanslag, 512, hårt mellanslag, 240,8500000000. `"-1234.5"` ger -1, hårt mellanslag, 234,5. `"999"` och `"0.123456"` grupperas inte.
- `decimalsOf`: "44.10" ger 2, "44" ger 0, "0.0000000001" ger 10.
- `parseDecimal(normalizeDecimal(x))` ger samma tal som `parseDecimal(x)` för giltiga x utan exponent.

### 10.2 `poletable_test.cpp` (ny, ctest `poletable`)
- `acceptField`:
  - Text trimmas.
  - Tal normaliseras, och tomt ger `""`.
  - `abc` och exponent i en talkolumn ger tomt svar, och kolumn 8 ger tomt svar.
- `displayText`: decimalkomma med de lagrade decimalerna, grupperat när `kGroupThousands` är på, och text som den är.
- `copyText`: aldrig grupperat.
- `numberValue` för tal, tom cell och textkolumn.
- `field` når alla åtta fält i rätt ordning, och `columnName` ger alla åtta namnen.
- `applyEdits`:
  - En ändring på rad 5 i en lista med två rader ger sex rader med tomma rader 2–4.
  - Negativ rad och kolumn 8 ignoreras.
- `trimTrailingEmptyRows` och `rowsToSave`: tomma rader mitt i behålls, och tomma rader sist tas bort.
- `firstInvalidColumn`: `"6512240,85"` (inte kanonisk) och `"abc"` ger kolumnen, och kanonisk text ger tomt svar.

### 10.3 `projectdb_test.cpp` (utökas)
- **`kNineTablesSql` blir `kTenTablesSql`** med `'pole_before'`.
  - `testSchemaTables`, `testMigrateFromV010` och `testMigrateFromV020` kontrollerar 10 tabeller och `kSchemaVersion`.
  - `testMigrateFromV020` har i dag `3` hårdkodat, och det byts mot `kSchemaVersion`.
- **`testPoleTableRoundTrip` (ny):**
  - En ny tabell är tom.
  - Spara rader med `"6512240.8500000000"`, `"44.10"`, `"-12.5"`, `"0"`, tomma tal och text med åäö, en helt tom rad mitt i och två helt tomma rader sist.
  - Läs tillbaka: den tomma raden mitt i finns kvar, de tomma raderna sist är borta, och alla fält är exakt som sparade.
  - `SELECT typeof(coord_n) …` ger `'text'`, och ett tomt tal ger `'null'`.
  - En ny sparning ersätter allt.
- **`testInvalid`:** `setPolesBefore` med `"6512240,85"` eller `"abc"` i en talkolumn ger `Invalid` och meddelandet med rad och kolumn, och inget ändras.
- **`testReadOnlyWrites`:** `setPolesBefore` ger `AccessDenied`.
- **`testRevision`:** `setPolesBefore` räknar upp revisionen.
- **`writeV030Project` + `testMigrateFromV030` (ny):**
  - Filen skapas som i v0.3.2:
    - Huvudet som `writeV020Project` med `PRAGMA user_version = 3`, plus `dev::kStep2` och `dev::kStep3`.
    - Data:
      - Projektraden `'åäö-v030'`, `'Kalkyl 0.3.2'`.
      - Två `doc_value`.
      - En normalspannsberäkning med fyra stolpar och koppling.
      - En manuell `line_calc` med bara `id`, `note`, `created_utc` och `modified_utc` (resten förval).
      - Femton `line_calc_case` (case_id 1–15, förval). Implementeraren kontrollerar mot läsningen i `project_data.cpp` att förvalen läses utan fel.
      - En `doc_line_calc`.
  - Öppna: `user_version` = 4, tio tabeller och allt kvar (värden, stolpar, gränser, kopplingar och linberäkningen med sin rad). `polesBefore` är tom.
  - Spara en stolptabell med 10 decimaler, publicera, stäng, öppna igen och kontrollera allt en gång till.
- **`testProvProject`:** provprojektet får de sex mockraderna (bilaga A) i Stolptabell före, så att röktestet och handtestet visar en fylld tabell.

### 10.4 `sheetgrid_test.cpp` (utökas)
- **`testGrowRows`:** formen har `rowCount` 3, `growRows` och `headerTakesRow = false`.
  - Ett block med fem rader från rad 2 ger ändringar på rad 2–6, `rowsNeeded == 7` och `area.row1 == 6`.
  - Ett ogiltigt tal under tabellen avbryter fortfarande allt.
- **`testHeaderNoRow`:** rubrikrad med `headerTakesRow = false`. Datat börjar på ankarraden, och `area` stämmer.
- De befintliga testerna (fast radantal, rubrikraden tar en rad) går igenom oförändrade, och `rowsNeeded` kontrolleras i `testOutside`.

`SheetGrid::rowsChanged` och `syncRowCount` kräver ett fönster och provas i handtestet.

---

## 11. Arbetspaket

Fyra Sonnet-agenter (high) arbetar samtidigt. Varje agent äger sina filer, och ingen fil delas.

### Gemensamt för alla paket
- **Byggkatalog** per paket i repots rot, konfigurerad med MSYS2:s cmake (`C:/msys64/ucrt64/bin/cmake.exe`):
  ```sh
  cmake -S . -B build-<paket> -G Ninja -DCMAKE_BUILD_TYPE=Debug -DCMAKE_C_COMPILER=C:/msys64/ucrt64/bin/gcc.exe -DCMAKE_CXX_COMPILER=C:/msys64/ucrt64/bin/g++.exe -DKALKYL_TESTS=ON -DFETCHCONTENT_SOURCE_DIR_GLAZEUI=C:/Users/sehoer/Source/Kalkyl/build/_deps/glazeui-src
  ```
- **Städning:** `build-<paket>` tas bort när agenten är klar (`rm -rf build-<paket>`). `build/` rörs aldrig: den byggs inte, rensas inte och konfigureras inte om.
- **Bygget faller i en fil som ett annat paket äger:** vänta och försök igen. Rör aldrig den filen.
- **Steg 0:** inom några minuter skriver varje paket sina kontraktsfiler ordagrant ur avsnitt 5, med kroppar som kompilerar (gärna tomma), så att de andra kan bygga. Ingen väntar på ett meddelande.

### Paket 1: tal och radmodell (`build-tal`)
- **Äger:**
  - `libs/tableText/table_text.h`, `table_text.cpp` och `CLAUDE.md`
  - `libs/poleTable/` (ny): `CMakeLists.txt`, `pole_table.h`, `pole_table.cpp` och `CLAUDE.md`
  - `libs/CMakeLists.txt` och `libs/CLAUDE.md`
  - `tests/tabletext_test.cpp`, `tests/poletable_test.cpp` (ny) och `tests/CMakeLists.txt`
- **Steg 0:** 5.1, 5.2, `libs/CMakeLists.txt` och raden `kalkyl_add_test(poletable poletable_test.cpp kalkyl_poletable kalkyl_tabletext)`.
- **Arbete:** 4.2 och 5.1–5.2, och testerna 10.1–10.2.
- **Bygg och kör:** `kalkyl_tabletext_test`, `kalkyl_poletable_test` och `ctest -R "tabletext|poletable"`.
- **CLAUDE.md:**
  - `libs/CLAUDE.md`: raden `poleTable/` och beroendet "`poleTable` beror på `tableText`, och `projectdb` beror också på `poleTable`".
  - `libs/tableText/CLAUDE.md` och `libs/poleTable/CLAUDE.md`.
  - Rapporten innehåller texten till `tests/CLAUDE.md`.

### Paket 2: projektdatabasen (`build-db`)
- **Äger:** `libs/projectdb/dev_migration.h`, `project_db.h`, `project_db.cpp`, `project_data.h`, `project_data.cpp`, `CMakeLists.txt` och `CLAUDE.md`, och `tests/projectdb_test.cpp`.
- **Steg 0:** deklarationerna i 5.4, kroppar som kompilerar och `kalkyl_poletable` PUBLIC i CMake. Bygget kräver paket 1:s steg 0.
- **Arbete:** 5.4, 9 och 10.3. Testerna med decimaler kräver paket 1:s riktiga `normalizeDecimal`, så slutkörningen görs när paket 1 rapporterat.
- **Bygg och kör:** `kalkyl_projectdb_test` och `ctest -R projectdb`.
- **CLAUDE.md:** `libs/projectdb/CLAUDE.md` (steg 4, tabellen `pole_before`, `polesBefore` och `setPolesBefore`, regeln om TEXT-tal och `testMigrateFromV030`). Rapporten innehåller texten till `tests/CLAUDE.md`.

### Paket 3: sheetGrid (`build-sheet`)
- **Äger:** `libs/sheetGrid/sheet_grid.h`, `sheet_grid.cpp` och `CLAUDE.md`, och `tests/sheetgrid_test.cpp`.
- **Steg 0:** tilläggen i 5.3 med kroppar som kompilerar.
- **Arbete:** 5.3 och 10.4. Linberäkningen ska bete sig exakt som förut, utom statustexten när en rubrikrad hoppats över.
- **Bygg och kör:** `kalkyl_sheetgrid_test`, `ctest -R sheetgrid` och `kalkyl`, för att visa att `line_calc_dialog` kompilerar.
- **CLAUDE.md:** `libs/sheetGrid/CLAUDE.md`. Regeln "Inga rader läggs till eller tas bort" ersätts med `growRows`, `headerTakesRow` och `rowsChanged`. Rapporten innehåller texten till `tests/CLAUDE.md`.

### Paket 4: huvudfönstret (`build-main`)
- **Äger:** `src/windows/main_window.h`, `main_window.glazeui`, `main_window.cpp` och `CLAUDE.md`, och `src/CMakeLists.txt` och `src/CLAUDE.md`.
- **Steg 0:** regionen och `.glazeui` (8.1–8.2) och tomma handlers som kompilerar. `kalkyl_poletable` läggs till i `src/CMakeLists.txt`.
- **Arbete:** 7 och 8.3. Byggs fullt när paket 1–3 har sina kontraktsfiler, och slutbygget görs när alla har rapporterat.
- **Förhandsgranskning:** konfigurera också med `-DGLAZE_TOOLS=ON`, bygg `--target glaze-render` och kör:
  ```sh
  build-main/_deps/glazeui-build/apps/glaze-render/glaze-render.exe src/windows/main_window.h -o <scratchpad>/main_window.png
  ```
  Exit 0 krävs, och bilden granskas med Read.
- **Röktest:** `KALKYL_DATA_DIR=build-main/smoke timeout -k 2 3 ./build-main/src/kalkyl.exe; echo $?` ger 124 eller 137. Med provprojektet ur `build-main/prov` som argument visas huvudfönstret.
- **CLAUDE.md:** `src/windows/CLAUDE.md` med `main_window.*`-raderna. Stolptabellen sparas nu i projektet, `shadeStolpRows` är borta och de nya funktionerna och medlemmarna tillkommer. `src/CLAUDE.md` får `kalkyl_poletable` i länklistan.

### Regler för alla implementeringsagenter
- **Filer redigeras med verktygen Edit och Write.** Inga Python- eller sed-skript ändrar filer. Skal används bara för bygge, körning, test och läsning.
- **Hooken `~/.claude/hooks/only-c-drive.sh`** granskar varje verktygsanrop, också filinnehåll, och stoppar:
  - två bakvända snedstreck i rad (räknas som UNC-sökväg)
  - ett bakvänt snedstreck och en bokstav (escape-sekvens) efter skiljetecken eller blanksteg och före citattecken eller slut
  - andra enhetsbokstäver än C följda av kolon. Låt aldrig `old_string` eller `new_string` sluta med en bokstav och ett kolon.
- **Därför:**
  - råa strängliteraler `R"sql( … )sql"` för SQL och `R"( … )"` för reguljära uttryck
  - teckenklasser som `[.]`
  - `char{10}` för radbrytning, `char{9}` för tabb och `char{34}` för citattecken
  - Unicode-tecken (U+00A0, U+202F, U+2212) byggda med `char`
  - snedstreck framåt i sökvägar
  - i CMake bracket-argument och `string(ASCII 10 NL)`
- **Kommentarer:**
  - Varje funktion, klass och logisk del har en kommentar på 2–5 rader.
  - Inline-kommentarer är korta och säger bara vad som händer.
  - Ingen historik (vad som ändrats, vem, när, tidigare beteende).
- **Fönsterreglerna:**
  - Tre filer per fönster.
  - Klassen utanför namespace.
  - Designraden står kvar.
  - Ingen logik i `.glazeui`.
  - Startlogik i `onLoad`.
  - Bara `<glaze/glaze.h>` och `<glaze_threads/threads.h>` från GlazeUI.
- **Varningar:** bygget är fritt från varningar (`-Wall -Wextra`).
- **Versionen** (0.4.0) rörs inte. Rotens `CMakeLists.txt` hör inte till något paket.
- **Inga commits.**

---

## 12. Integration (samordnaren)

1. **Bygget utan varningar** i `build/`: `cmake --build --preset debug 2>&1 | grep -i warning` ger ingen utdata.
2. **Testerna** i en tillfällig `build-verify` (konfigurerad som i 11, med `-DGLAZE_TOOLS=ON`):
   ```sh
   cmake --build build-verify && ctest --test-dir build-verify --output-on-failure
   ```
   Alla tester går igenom. Katalogen tas bort efteråt.
3. **glaze-render** på `main_window.h`, `normal_span_dialog.h` och `line_calc_dialog.h` ger exit 0.
4. **Röktest** med provprojektet: fliken Stolptabell före visar sex rader, papperskorgar och den tomma raden sist.
5. **Kontroller:**
   - `grep -n "shadeStolpRows" src` ger ingen träff.
   - `main_window.glazeui` har inga `stolpGrid.addRow`.
   - `project(Kalkyl VERSION 0.4.0` är oförändrad.
6. **Städning:** inga `build-*` utom `build/` finns kvar.
7. **CLAUDE.md och `TODO.md`** enligt 14. Användaren får testfallen i 13.

---

## 13. Handtest (användaren)

Kör `build/src/kalkyl.exe`. Ha Excel öppet med raderna i bilaga A.

**Projektet och sparningen**
1. **Projekt från v0.3.2:** ett projekt som skapats före ändringen öppnas utan fel. Stolptabell före är tom med en tom rad, och Dokumentation, normalspann och linberäkningar är orörda.
2. **Nytt projekt:** tabellen är tom med en tom rad och ingen papperskorg.
3. **Skriv en stolpe för hand** i den tomma raden: ST0378, 363, 362, 348, 6512240,85, 412181,98, 44,19 och 255/262.
   - En ny tom rad kommer, och raden får en papperskorg.
   - Titeln får `*`.
   - Ctrl+S, stäng och öppna igen: raden är exakt likadan.

**Inklistring (K1)**

4. **Blocket i bilaga A (8 kolumner × 6 rader):** klicka i Stolpplats på den tomma raden och tryck Ctrl+V. Sex rader skapas, en tom rad står sist, och markeringen är blocket.
5. **Med rubrikraden:** rubriken hoppas över, datat börjar på den markerade raden, och statusraden säger att rubrikraden hoppades över.
6. **En kolumn:** bara Koordinat N klistras in i Koordinat N på rad 3. Värdena hamnar från rad 3 och nedåt.
7. **En rad:** en hel rad klistras in i Stolpplats på rad 2.
8. **En cell:** ett enda värde klistras in i en vald cell.
9. **Förbi slutet:** klistra in tio rader från näst sista dataraden. Nya rader skapas, och en tom rad står sist.
10. **Bredare än tabellen:** tre kolumner klistras in i Koordinat H. H och LKL får de två första, och den tredje ignoreras.
11. **Under redigering:**
    - Ett enda tal hamnar i redigeringen.
    - Ett block avbryter redigeringen och klistras in från cellen.
12. **Markeringen:** Shift+pil och Shift+klick ger en blå rektangel. Ctrl+V klistrar in i dess övre vänstra cell.

**Decimaler och decimaltecken (K4, K5)**

13. `6512240,8500000000` visas med alla tio decimaler, också efter att projektet öppnats igen. Kopiera cellen till Excel och se talet där.
14. `44` visas som 44. `44,10` och `44.10` visas båda som 44,10, inte 44,1.
15. **Tusentalsavgränsare (prövas):** `6512240,85` och `6 512 240,85` visas båda som 6 512 240,85. Ctrl+C till Excel ger 6512240,85 utan mellanslag. Säg till om grupperingen ska bort.
16. `-12,5` och `−12,5` visas båda som -12,5.
17. Ett urklipp med punkt i en kolumn och komma i en annan tas emot helt.
18. `1.234,5` klistrat in i en talkolumn avbryts med ett meddelande, och inget ändras.
19. `abc` i Koordinat N:
    - Inklistrat avbryts det med ett meddelande, och inget ändras.
    - Skrivet händer ingenting: ingen ruta, och det förra värdet kommer tillbaka.
20. En för smal Excel-kolumn som visar `6,5E+06` avbryts med ett meddelande.
21. Tomma fält i ett inklistrat block tömmer cellerna.

**Redigering (K2)**

22. Ändra Stolpplats och LKL (också med åäö). Varje ändring syns och sparas.
23. **Tangenterna:**
    - Delete tömmer de markerade cellerna.
    - Ctrl+X och Ctrl+V någon annanstans flyttar dem.
    - Ctrl+C till Excel ger tal med decimalkomma.
    - Ctrl+A och Delete ger en tom tabell.

**Radera (K3)**

24. **Klicka på papperskorgen på rad 3:** "Radera stolpplats ST0380?" visas mitt över huvudfönstret.
    - Enter väljer Ja.
    - Nej: inget händer.
    - Ja: raden försvinner, radtonen växlar rätt och titeln får `*`.
25. **Placeringen:** flytta huvudfönstret till skärm 2 och prova igen, och prova också med fönstret litet i ett hörn och maximerat. Rutan står mitt över fönstret och inom skärmen.
26. **Tangentbordet:** gå med pilarna till papperskorgen och tryck Space. Frågan visas.
27. **Snabbmenyn:**
    - "Infoga rad ovanför" infogar en tom rad.
    - "Radera rader…" med tre markerade rader frågar med båda ändarnas stolpplats, och Ja tar bort dem.
28. **Tom rad mitt i:** infoga en tom rad, låt den vara tom, spara och öppna igen. Den tomma raden finns kvar.

**Övrigt**

29. **Skrivskyddat:**
    - Det finns inga papperskorgar, och cellerna går inte att redigera.
    - Ctrl+C och Ctrl+A fungerar, men Ctrl+V, Delete och Ctrl+X gör ingenting.
    - Menyposterna är inaktiva.
30. **Sortering:** ett klick i en kolumnrubrik sorterar inte.
31. **Automatisk sparning** (1 minut): en ändring i tabellen sparas, och `*` försvinner.
32. **Mörkt och ljust läge:** radtonen, markeringen och papperskorgen syns i båda.
33. **Linberäkning:** Excel-inklistringen i lastfallen fungerar som förut. En rubrikrad tar fortfarande sin egen rad.
34. **Normalspannsformuläret:** oförändrat.

---

## 14. CLAUDE.md och TODO.md

| Fil | Vem | Ändring |
|---|---|---|
| Rotens `CLAUDE.md` | samordnaren | Delar: huvudfönstrets Stolptabell före (Excel-urklipp, redigering, papperskorg, sparas), ny rad för `libs/poleTable`, och projektdatabasens rad med stolptabellen. Data: projektet sparar också Stolptabell före. |
| `libs/CLAUDE.md` | paket 1 | `poleTable/` och beroendena |
| `libs/tableText/CLAUDE.md`, `libs/poleTable/CLAUDE.md` | paket 1 | Filerna och reglerna |
| `libs/projectdb/CLAUDE.md` | paket 2 | Steg 4, tabellen, API:t och migreringstestet |
| `libs/sheetGrid/CLAUDE.md` | paket 3 | `growRows`, `headerTakesRow` och `rowsChanged` |
| `src/windows/CLAUDE.md`, `src/CLAUDE.md` | paket 4 | `main_window.*` och länklistan |
| `tests/CLAUDE.md` | samordnaren | Raden för `poletable` och ändrade beskrivningar för `tabletext`, `projectdb` (migrering från v0.3.x) och `sheetgrid`, med paketens text |
| `doc/plans/CLAUDE.md` | samordnaren | Raden för den här planen |
| `TODO.md` | samordnaren | Se nedan |

**`TODO.md`:**
- **Arbetspunkten Stolptabell före** har status ⬜, och 🔨 under implementeringen.
- **Idén "Spara Stolptabell före i projektdatabasen …" tas bort.** Sparningen ingår här, och "Hämta från stolptabellen" ska inte göras (svar 9).
- **Ny idé:** "💡 GlazeUI: ett val för talceller i DataGrid som visar talet med de decimaler som skrevs, så att Stolptabell före kan svälja bokstäver redan vid tangenttrycket. Se doc/plans/2026-11-26_Stolptabell_fore.md, 3.2."
- **Idén om normalspannsformuläret och sheetGrid** ändras till: "Normalspannsformuläret använder `libs/sheetGrid` (`growRows`, `headerTakesRow = false`) för markering och urklipp, som Linberäkning och Stolptabell före."

---

## 15. Risker och luckor i GlazeUI v0.11.1

1. **Ingen områdesmarkering eller dragmarkering** i DataGrid. Markeringen emuleras av sheetGrid med tangentbordet och Shift+klick, som i formulären.
2. **Inget event när den aktuella cellen flyttas.** sheetGrid synkar med en postad funktion efter tangent och klick, som i dag.
3. **Knappcellen har knappram och hover.** Den är ingen ren glyf. Godtas den inte används reserven i 3.5.
4. **YesNo-rutan:** Ja är förvald, så Enter raderar raden, som användaren vill. Escape och krysset gör ingenting.
5. **`setRows` nollar cellinställningarna, färgerna och den aktuella cellen.** `fillPoleGrid` sätter tillbaka dem, och `rowsChanged` målar om.
6. **Ingen ångra.** Delete, Ctrl+X, inklistring och radborttagning skrivs direkt till arbetskopian. Det som publicerats senast går att få tillbaka genom att stänga utan att spara, men den automatiska sparningen publicerar efter några minuter.
7. **Textredigeraren filtrerar inga tecken.** Bokstäver syns medan man skriver, och cellen återställs tyst när den sparas (7.1). Idén i `TODO.md` om ett val i GlazeUI löser det.
8. **Sorteringen är på som förval.** Den stängs av (3.9).
9. **Rubrikraden känns bara igen i talkolumnerna.** Två följder:
   - En första datarad med ett felskrivet tal tas för en rubrik och hoppas över. Statusraden säger det.
   - En rubrik över bara Stolpplats eller LKL klistras in som data, och raden raderas med papperskorgen.
10. **Hela tabellen skrivs vid varje ändring.** Det är billigt för några tusen rader. `poleSaved_` hindrar tomma skrivningar.
11. **Ett double har cirka 15–16 gällande siffror.** Visningen påverkas inte (4.3), men beräkningar och Excel avrundar.
12. **Postade anrop och fönstrets livstid.** `MainWindow` är en lokal i `main`, så de nya postningarna går bakom `alive_`.
13. **Kolumnbredderna med AutoMin** växer med långa decimaler, och tabellen rullar i sidled när de inte ryms.

---

## 16. Användarens svar (2026-11-26)

1. **Decimalerna som de skrevs** gäller Av, Ah, Spannlängd föreg. och Koordinat N, E och H. Stolpplats och LKL är vanlig text.
2. **Decimaltecknet** visas alltid som komma.
3. **Tusentalsavgränsare** prövas i visningen (3.11) och avgörs i handtestet.
4. **"Är du säker?"** använder GlazeUI:s meddelanderuta, utan egen kod. Den står mitt över fönstret och annars mitt på skärmen (3.6).
5. **Frågan** är till exempel "Radera stolpplats ST0381?" med Ja och Nej, och Ja är förvalt (7.4).
6. **Glyfen** tas ur GlazeUI:s stock-ikoner (8.3).
7. **Sparningen** i projektdatabasen ingår.
8. **Stolptabell efter** förbereds inte. Den får en egen tabell senare (3.10).
9. **"Hämta från stolptabellen"** görs inte, eftersom Stolptabell före bara är ett urval av stolparna.
10. **Bokstäver i en talcell** gör ingenting. Cellen återställs tyst (3.2, 7.1).
11. **Alla rader sparas**, också tomma rader mitt i tabellen (4.6).
12. **Delete, Ctrl+X och inklistring** skriver över utan fråga och utan ångra. Bara radborttagningen frågar.
13. **Exponentform** (`6,51E+06`) avvisas med ett meddelande.
14. **Ett värde i en markering med flera celler** fyller inte markeringen. Det hamnar bara i markeringens första cell.
15. **Kolumnerna** betyder följande (tooltips i 8.2):
    - Av: vertikal linlängd som belastar stolpen.
    - Ah: horisontell linlängd, som används i vindberäkningar.
    - LKL: längdklass, alltså fackverkstolpens längd, där 262 är 26,2 m.

---

## Bilaga A: provdata (mockens sex rader)

Skriv raderna i Excel (rubrikraden i rad 1) och kopiera blocket. I den andra varianten skrivs Koordinat N med punkt och tio decimaler.

| Stolpplats | Av | Ah | Spannlängd föreg. | Koordinat N | Koordinat E | Koordinat H | LKL |
|---|---|---|---|---|---|---|---|
| ST0378 | 363 | 362 | 348 | 6512240,85 | 412181,98 | 44,19 | 255/262 |
| ST0379 | 267 | 320 | 375 | 6511904,92 | 412013,87 | 28,05 | |
| ST0380 | 344 | 345 | 264 | 6511653,69 | 411933,25 | 22,27 | 262/262 |
| ST0381 | 344 | 374 | 427 | 6511247,08 | 411804,35 | 30,01 | 262/262 |
| ST0382 | 378 | 305 | 322 | 6510940,33 | 411706,06 | 48,54 | |
| ST0383 | 181 | 300 | 287 | 6510691,90 | 411562,70 | 42,00 | 241/241 |

Variant: Koordinat N som `6512240.8500000000`, `6511904.9200000000` och så vidare. H `42,00` ska visas med två decimaler.
