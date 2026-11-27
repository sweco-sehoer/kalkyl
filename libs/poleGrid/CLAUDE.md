# libs/poleGrid

Stolptabellen i en GlazeUI DataGrid: raderna som kanonisk decimaltext med den tomma raden sist, Excel-markeringen,
papperskorgen med frågan, snabbmenyn och färgerna. Delas av Stolptabell före och Stolptabell efter.
I Stolptabell efter har den dessutom filtret Förändrade ±2/Alla med grå rader, Visa förändrade och snabbmenyns Kopiera från Stolptabell före.
Biblioteket heter `kalkyl_polegrid`, namnrymden `kalkyl::polegrid` och includes skrivs `"poleGrid/pole_grid.h"`.
Det beror på `glaze`, `kalkyl_sheetgrid`, `kalkyl_poletable`, `kalkyl_automode` och `kalkyl_poleview` (PUBLIC) och `kalkyl_tabletext` (PRIVATE).

| Fil | Innehåll |
|---|---|
| `CMakeLists.txt` | Biblioteket `kalkyl_polegrid` (testet `polegrid` ligger i `tests/`) |
| `pole_grid.h` | `PoleGridHost` (det fönstret svarar på, med `filterChanged`), `PoleLooks` och `CellLook`, de rena funktionerna `cellLook` och `computedSpanText` och klassen `PoleGrid` |
| `pole_grid.cpp` | Implementationen: `cellLook`, adaptern till `SheetGrid`, vyn och ombyggnaden, tabellens händelser, sparning, Infoga rad, Radera rader, inklistringen mot modellen och `GridProbe` |

## Regler

- **Fönstret äger projektet.** `PoleGrid` håller raderna (`rows_`) och det senast sparade (`saved_`) och frågar `PoleGridHost`
  om sparning (`savePoles`), omläsning efter fel (`reloadPoles`), omräkning (`polesChanged`), omdöpt Stolpnr (`poleRenamed`),
  egna menyposter (`addMenuItems`), notisen i frågan Radera (`removeNote`), statusraden (`showStatus`) och filterbytet (`filterChanged`, förval: ingenting).
  `polesChanged` körs efter varje lyckad sparning som skrev något, och `poleRenamed` före den.
- **Kolumnerna** är fält i en ordning som ges till konstruktorn (`columns_`): `poletable::beforeColumns()` i före och `afterColumns()` i efter, med Av Fas1, Fas2 och Fas3 före LKL.
  Radera-kolumnen står efter dem (`deleteColumn()`). Allt som DataGrid och `SheetGrid` talar om är tabellens kolumner, och `PoleGrid` översätter med `fieldOf` och `columnOf`.
  `cellText`, `cellLook` och `restoreCell` tar fältet; `cellLook` med -1 (radera-kolumnen) ger bara radens ton. I efter heter Av "Av Fas1" i meddelandena (`fieldName`).
- **Inga layouter i koden.** Tabellen, kolumnerna och rullningslisterna står i `.glazeui`. `PoleGrid` styr bara en befintlig `DataGrid`:
  den kopplas med `committed`, `cellClicked`, `keyDown`, `mouseUp` och `contextMenu`.
- **Färgerna** kommer ur `cellLook`, som är en ren funktion utan fönster (testas i `tests/polegrid_test.cpp`).
  - Radens ton är gul (`{220,160,40}`), rosa för en ändrad rad och grön för en ny rad, med alfa 64 på jämna och 52 på udda rader (modellens rad).
  - Den ändrade cellen har `{210,90,140,130}` och tooltipen "I Stolptabell före: …". Ett fel eller en osäker Stolpnr har `{220,60,60,80}` och vinner över jämförelsen.
  - Tooltipens delar skiljs med radbrytning i ordningen fel, jämförelse, not. En not på raden ger Stolpnren flaggan (`StockIcon::Flag`).
  - Radera-kolumnen har radens ton och ingen tooltip i `cellLook`; "Radera raden" sätts av `PoleGrid` på stolpar i ett skrivbart projekt.
  - En grå rad har bakgrunden `kHiddenRow` `{128,128,128,48}` och texten `kHiddenText` `{128,128,128,255}` i alla kolumner, också radera-kolumnen.
- **Spannlängd ur koordinaterna:** en tom Spannlängd föreg. visar värdet ur `automode::coordinateSpans` med två decimaler, Σ-bilden (`StockIcon::Sum`) och en tooltip, som de uträknade värdena i Dokumentation.
  `PoleGrid` räknar värdena själv ur `rows_` (`looks_.coordinateSpans` i `fill`, `applyLooks` och `committed`), och `cellText` ger cellens text för visning och urklipp.
  Modellen har kvar den tomma texten: en inskriven spannlängd ersätter värdet, en tömd cell räknas igen, och ett uträknat värde som bekräftas oförändrat förblir uträknat.
- **`setLooks`** målar om och sätter tooltips och flaggor, och bygger om vyn när den ändrade jämförelsen ger en annan. `setRows` och varje strukturell ändring (`fill`) sätter dem tillbaka,
  eftersom `DataGrid::setRows` nollar cellinställningarna.
- **Sparningen** sker direkt vid varje ändring och bara när `poletable::rowsToSave` skiljer sig från det senast sparade.
  Misslyckas den postas en omläsning med `reloadPoles` (bakom `alive_`, så att tabellen inte byggs om mitt i sin egen händelse).
- **Skrivskyddat** (`setReadOnly`) låser cellerna, tar bort papperskorgarna och låter markeringen bara kopiera. Filtret och Visa alla rader fungerar ändå.
- **Används av** huvudfönstret (`src/windows/main_window.*`) för Stolptabell före och efter. Excel-markeringen kommer ur `libs/sheetGrid`, vyn ur `libs/poleView`.

## Vyn och mappningen mellan vy och modell

- **Modellen** är `rows_`, raderna i efter i tabellens ordning. Filtret ändrar den aldrig.
- **Vyn** (`view_`, `poleview::PoleView`) är tabellens rader i DataGrid. Varje vyrad är `Pole` (en rad i modellen), `Hidden` (en grå rad för modellens rader `first..last`, minst två) eller `Blank` (den tomma raden sist, `first = rows_.size()`).
  `grid_.rowCount() == view_.rows.size()` efter `fill()`. Allt `SheetGrid` och DataGrid talar om är vyns rader, och `PoleGrid` översätter med `poleview::modelRow`, `viewRowOf` och `visibleViewRow`.
- **Utan jämförelse** (`setComparesWithBefore(false)`, Stolptabell före och förvalet) är filtret alltid `All` och vyn identiteten, så tabellen beter sig exakt som utan filter.
- **`desiredView()`** ger identiteten utan jämförelse eller med `All`, annars `buildView(…, filterMarks(looks_.before, rows_), Changed)`. Jämförelsen räknas alltid om ur `rows_`, så vyn ligger aldrig efter.
- **Strukturella ändringar** sätter vyn själva, eftersom modellens index flyttas: `setRows`, `insertRowAbove`, `removeRowsAsked` och `pasteIntoModel` sätter `view_ = desiredView()`,
  och `applyEdits` och en ny stolpe i den tomma raden behåller vyns form med `resizeView`, så att `SheetGrid`s markering stämmer, och kör sedan `refreshView`.
- **Vägarna** i tabellen: `committed` och `cellClicked` (bara `Pole` ger ändring eller papperskorg), `Adapter::copyText` och `baseColor` (modellens rad), `Adapter::addMenuItems`
  (Infoga rad ovanför, Radera rader… och Kopiera från Stolptabell före), `host_.addMenuItems` (`Pole` ger modellens rad, annars -1) och `host_.removeNote` (modellens rader, en gång per följd).

## De grå raderna

- En grå rad är en vanlig rad av dataceller med `Editor::None` i alla kolumner, också radera-kolumnen, text i kolumn 0 (`poleview::hiddenText`), tooltip (`hiddenToolTip`) och grå färger.
  `RowKind::Category` och `RowKind::Blank` duger inte, eftersom de fäller ihop eller inte ritar text.
- `SheetShape::hiddenRow` pekar ut dem för `SheetGrid`, som aldrig markerar, målar eller skriver i dem och hoppar över dem vid kopiering, Delete och Ctrl+X.
  Pilarna kan ställa den aktuella cellen på en grå rad, men ingenting går att ändra där. Ett klick gör ingenting.
- Högerklick på en grå rad visar en egen meny (`hiddenMenu_`) med bara "Visa alla rader": filtret blir `All`, värden får `filterChanged(All)` och statusraden säger "Filtret är nu Alla.".
- `showRow` på en dold rad gör samma sak och säger "Filtret är nu Alla, så att {Stolpnr} syns.".
- Radera rader… och papperskorgen tar bara synliga stolpar. Grå rader står bara före och efter det synliga området, så de valda stolparna är nästan alltid i följd. Är de inte det (en ny stolpe efter den sista grå raden innan vyn byggts om) säger frågan att dolda rader däremellan inte raderas.

## Ombyggnaden av vyn (`refreshView`, `postRebuild`, `rebuildView`)

- **Aldrig mitt i en händelse eller en redigering.** `inEvent_` räknas upp och ner av `EventScope` i `committed`, `cellClicked`, `keyDown`, `mouseUp`, `contextMenu`, `applyEdits` och `pasteIntoModel`.
  `refreshView` bygger om direkt när varken en händelse pågår eller en cell redigeras, annars postas ombyggnaden (`postRebuild`, högst en åt gången, bakom `alive_`).
  Sist i `committed`, `keyDown` och `mouseUp` postas en väntande ombyggnad, så att en ombyggnad som väntade på att en redigering skulle sluta (till exempel efter Escape) kommer.
- **`rebuildView`** gör ingenting under en öppen redigering. Annars sparar den den aktuella cellen, markeringen, `style().gridRowH` och `scrollY()`, bygger om med `fill()`
  och sätter tillbaka: den aktuella cellen följer sin stolpe (en dold stolpe ger nästa synliga rad), markeringen följer med, och den aktuella raden står kvar på samma höjd.
- **Radhöjden** är `owner_.style().gridRowH`, eftersom `setIconSize` inte används. Sätts en ikonstorlek på tabellen blir rullningen fel.
- **`setFilter`** sparar en öppen cell (`commitEdits`) och bygger om direkt. Den får aldrig anropas ur tabellens egen händelse.

## Visa förändrade (`showChanged`)

Den första förändrade raden blir aktuell och tabellen får fokus. Vyns höjd mäts genom att rulla längst ned (`setScroll(0)`, `ensureVisible(sista raden)`, och rullningen där ger innehållet minus synfältet), vilket kräver ett öppet fönster.
Rullningsmålet kommer ur `poleview::scrollTop`: området mitt i synfältet när det ryms, annars första raden överst. Svarar false när ingen rad är förändrad eller tabellen inte jämförs med före.

## Kopiera från Stolptabell före (den högerklickade cellen)

- Gäller bara cellen under pekaren vid högerklicket (`menuCell_`), aldrig markeringen. Ett högerklick utanför markeringen markerar just den cellen, och inne i en markering står den kvar.
  Utan jämförelse (Stolptabell före) görs inget av detta.
- Posten är tänd när projektet är skrivbart och `automode::restoreFromBefore` svarar med en ändring för cellen. `restoreCell` skriver ändringen, sparar direkt (`savePoles` och `polesChanged`)
  och visar "{Kolumn} för {Stolpnr} är nu som i Stolptabell före: {värde}.".
- **`GridProbe`** (anonym namnrymd i `pole_grid.cpp`) tar fram kolumnen under pekaren, eftersom DataGrid i GlazeUI v0.11.1 saknar en publik fråga om det.
  Den når den skyddade `glaze::Table::columnAt(const Ctx&, float mx) const` genom en pekare till medlem som bildas i en härledd klass (tillåtet i C++, klassen skapas aldrig).
  `mx` är fönstrets fysiska px, alltså `ContextMenuArgs::x` som den är; raden tas separat ur `rowAt` med `fromWindow`. -1 utanför kolumnerna.
  Det bygger på en skyddad signatur som kan ändras när GlazeUI uppgraderas, och kan tas bort om GlazeUI får en publik fråga.

## Inklistringen när vyn har grå rader (`pasteIntoModel`)

`SheetGrid` frågar först värden (`SheetHost::pasteBlock`). Har vyn ingen grå rad svarar `PoleGrid` false och `SheetGrid` klistrar in själv (vyns rader är då modellens).
Annars planerar `PoleGrid` själv med `sheetgrid::planSheetPaste` mot modellens form (`baseShape()` med `rowCount = rows_.size() + 1` och utan `hiddenRow`):
blocket skrivs i modellens rader i följd från ankarets rad, också i dolda rader, och rader under tabellen blir nya rader. Ingen ruta visas och inklistringen avbryts inte
(utom när ett fält inte passar i sin kolumn, då visas rutan "Klistra in" som i `SheetGrid`). Vyn byggs om direkt, så de rader som blev förändrade syns, och markeringen blir det inklistrade området.
Kopiering och inklistring är med flit inte symmetriska: kopieringen hoppar över grå rader (som Excel med ett filtrerat område).
