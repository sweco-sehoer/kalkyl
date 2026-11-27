# libs/lineHeightSheet

Linhöjdsbladet i fliken Linhöjder som en egen GlazeUI-widget, `kalkyl::lineheightsheet::LineHeightSheet` (`kalkyl_lineheightsheet`).
Den ritar `lineheight::Sheet` (libs/lineHeight) som Excel-förlagan och redigerar inmatningscellerna.
Bladet sparar inget självt: huvudfönstret läser `lastCommit()` i `onCommit` och skriver i projektet.

| Fil | Innehåll |
|---|---|
| `CMakeLists.txt` | OBJECT-biblioteket `kalkyl_lineheightsheet` och länkningen in i `glaze-render` och `GlazeBuilder` |
| `line_height_sheet.h` | `LineHeightSheet : glaze::Panel`: `setSheet`, `setReadOnly`, `beginEdit`, `currentCell`, `lastCommit`, `onCommit`, `commitEdit`, `toolTipAt` |
| `line_height_sheet.cpp` | Registreringen, måtten, ritningen, träffen, textfältet (`Editor`) och redigeringen |

## OBJECT-bibliotek och registrering

- Biblioteket är ett OBJECT-bibliotek. Länkaren stryker annars registreringen `GLAZE_REGISTER_WIDGET` ur ett statiskt bibliotek.
- Registreringen står i `line_height_sheet.cpp` inne i namnrymden med det okvalificerade klassnamnet:
  `GLAZE_REGISTER_WIDGET("lineheightsheet", LineHeightSheet)`. Makrot klistrar ihop klassnamnet till ett variabelnamn.
- Medlemsraden i `.glazeui`-regionen skriver klassen med hela namnrymden (`kalkyl::lineheightsheet::LineHeightSheet linBlad;`),
  så att verktygen hittar den i registret.
- Med `-DGLAZE_TOOLS=ON` länkar den egna `CMakeLists.txt` in biblioteket i `glaze-render` och `GlazeBuilder` (de mål som finns).
  Då ritas bladet på riktigt vid förhandsgranskningen. Presetet `tools` gör det.
- VS Code-tillägget och den medföljande GlazeBuilder känner inte widgeten och ritar en ruta med klassnamnet. Det är GlazeUI:s regel för egna typer.

## Ritningen

- Måtten räknas ur Ctx:t i logiska px: kolumnbredden är `lineheight::kColumnChars[i]` gånger bredden av siffran "0" plus `2 × gridPadX`, radhöjden är `gridRowH` och luften runt bladet är 8 px.
- `Panel::draw` ritar ytan och ramen. Därefter ritas bara de block som syns, med rullningen avdragen: blockets gula yta (`lineheight::blockFill`, stolptabellernas ton som växlar mellan blocken), cellerna (fyllning, text, ram) och blockets kraftiga ram.
- Färger, fet stil, justering, ram och glyf kommer ur `lineheight::cellLook`. Bladet behåller Excel-färgerna i mörkt läge.
- Glyfen (`StockIcon::Sum` för beräknade och `StockIcon::Sync` för länkade värden, högst 16 px) står i cellens vänsterkant och ritas med fönstrets tema i den ljusa varianten, så att den syns på de gröna cellerna också i mörkt läge.
- Fet text ritas med `Draw::withFont(c, UI, Bold)`, och en text som inte ryms kortas med "…".
- Den aktuella cellen får en ram i temats accentfärg när bladet eller textfältet har fokus.
- `contentExtent` svarar med bladets bredd och höjd plus luften i fysiska px, så rullningen täcker allt. Ett smalt fönster rullar i sidled.
- Träffen (`cellAtPoint`) räknar i samma rum som ritningen och använder `lineheight::cellAt`, så den fungerar också inne i en sammanslagen cell.

## Redigeringen

- Textfältet `Editor` (en `glaze::TextInput`) är ett ägt barn som skapas i konstruktorn och göms när det inte används.
  Det läggs över den aktuella cellen med 1 px insteg, relativt barnytan, så det följer rullningen. `canReleaseChild` vägrar att släppa det.
- Ett klick på en inmatningscell sparar en öppen cell och öppnar den nya med hela texten markerad. Enter och F2 öppnar den aktuella cellen när bladet har fokus.
- Tangenterna i textfältet (Ctrl, Alt och Cmd släpps orörda till fältet, så Ctrl+Tab går till flikarna):

| Tangent | Gör |
|---|---|
| Tab · Shift+Tab | sparar och öppnar nästa respektive förra inmatningen (`moveInput`) |
| Enter · Ner | sparar och öppnar cellen nedanför i samma kolumn, också i nästa block |
| Shift+Enter · Upp | sparar och öppnar cellen ovanför |
| Escape | kastar texten, stänger fältet och ger fokus till bladet |

- Sparningen (`endEdit`) fyrar inget när texten är oförändrad mot cellens `editText`. Annars sätts `lastCommit()` (inmatningen, blockets rad-id och texten) och `onCommit` fyras.
  - Enter, Tab, pilarna, `commitEdit()` och ett klick fyrar direkt. Svarar handlern att bladet rivits returnerar koden direkt.
  - Fokusförlusten postar händelsen (`postEvent`), som bär sin egen nyckel och text och därför aldrig slopas.
- Efter en direkt sparning hittas målcellen igen med `lineheight::findInput` på radens id och inmatningen, eftersom handlern kan ha bytt bladet med `setSheet`.
- `setSheet` följer den aktuella cellen på radens id (två block med samma Stolpnr skiljs åt). En öppen redigering står kvar med det som skrivits.
  Finns cellen inte kvar stängs fältet utan händelse.
- Skrivskyddat (`setReadOnly(true)`) öppnas textfältet skrivskyddat, så att det går att markera och kopiera i, och `onCommit` fyras aldrig.
- Tooltipen (`toolTipAt`) är cellens `toolTip`.

## Tester

`tests/lineheightsheet_test.cpp` (ctest `lineheightsheet`) provar bladet utan fönster (`beginEdit`, aktuell cell som följer sin rad, skrivskydd)
och i ett dolt fönster: färgerna i planen räknas i bilden, som skrivs till `KALKYL_PROV_DIR/linhojder.png`. Utan skärm svarar testet 77.
