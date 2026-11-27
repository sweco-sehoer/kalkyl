# Filter i Stolptabell efter: Förändrade ±2, Visa förändrade och Kopiera från Stolptabell före per cell

- **Datum:** 2026-11-27.
- **Status:** godkänd efter användarens svar 2026-11-27. Svaren står i avsnitt 1, och planen är klar för implementering.
- **Version:** 0.6.0 står redan i `project(Kalkyl VERSION 0.6.0` i rotens `CMakeLists.txt`. Samordnaren har satt den. Planen och agenterna rör den aldrig.
- **Underlag:**
  - Koden: `src/windows/main_window.*`.
  - Biblioteken: `libs/poleGrid`, `libs/sheetGrid`, `libs/poleTable`, `libs/autoMode` och `libs/systemdb`, och för kontrollen `libs/projectdb`.
  - GlazeUI v0.11.1: `build/_deps/glazeui-src/MANUAL.md`, avsnitten `widget-data-grid`, `widget-list-view`, `widget-dropdown`, `widget-button` och `widget-label`, och källan `table.h`, `table.cpp`, `data_grid.cpp`, `panel.h` och `label.h`.
  - De arkiverade planerna `2026-11-26_Automatiskt_lage.md` och `2026-11-26_Stolptabell_fore.md`.
- **Bindande:**
  - headrarna och tilläggen i avsnitt 6,
  - texterna i avsnitt 7,
  - raderna i `.glazeui` i 8.2.

  Namn, signaturer och texter följs ordagrant. Behöver ett paket ändra en signatur säger det till i rapporten, och samordnaren beslutar.

---

## 1. Beslut av användaren 2026-11-27

- **F1, var valet sparas:** per användare i systemdatabasen, nyckeln `pole_after_filter` i tabellen `setting` (K7). Ingen schemaändring och ingen migrering behövs.
- **F2, inga förändringar:** Förändrade ±2 är förvalet. Finns inga ändringar visas hela listan automatiskt, som med Alla (K5).
- **F3, borttagna stolpar:** platsen räknas som förändrad och syns med ±2, och infotexten räknar de borttagna. Tabellen märker inte platsen (K2).
- **F4, klick på en grå rad:** ett klick gör ingenting. Snabbmenyn på en grå rad har bara "Visa alla rader" (K17).
- **F5, Gå till en dold stolpe:** filtret blir Alla och sparas (K18). "Gå till Stolptabell efter" finns i snabbmenyn på de automatiska beräkningsraderna i Dokumentation och i flikarna Normalspann och Linberäkning.
- **F6, inklistring över grå rader:** ändrat mot förslaget.
  - Blocket skrivs i modellens rader i följd, från ankarets rad och nedåt, också i de dolda raderna, som om alla rader syntes.
  - De dolda rader som skrivs blir förändrade och syns efter ombyggnaden.
  - Ingen ruta visas, och inklistringen avbryts inte (K13, 5.8, 6.3).
- **F7, Kopiera från Stolptabell före:** ändrat mot förslaget. Alternativet gäller bara den högerklickade cellen, inte markeringen (K21, K22, 5.7).
- **F8, flera ändringar:** det synliga är ett enda område, från den första förändrade raden −2 till den sista +2. Allt däremellan syns, också när ändringarna ligger långt isär, och grå rader står bara före och efter området (5.2).

---

## 2. Mål och avgränsning

### Användarens krav
| # | Krav | Var |
|---|---|---|
| A1 | Efter förklaringen "■ Som i före ■ Ändrad ■ Ny" står ett filter: en rullista med Förändrade ±2 och Alla. Förvalet är Förändrade ±2. | 8.2 |
| A2 | Förändrade ±2 visar en ändrad rad och två rader före och efter den. De andra raderna står ihop i grå rader, till exempel "Dolda STO100–STO447". Med flera ändringar syns allt mellan dem och två rader runt varje. | 5, K1–K5 |
| A3 | Alla visar alla rader. | K10 |
| A4 | Knappen "Visa förändrade" rullar till det ändrade området, med mitten av området mitt i vyn. Ryms det inte står den första ändrade raden överst. | 5.6, K9 |
| A5 | Valen sparas. | K7, K8 |
| A6 | Varje cell i Stolptabell efter har snabbmenyns "Kopiera från Stolptabell före", som återställer cellen till värdet i före. Raden matchas på Stolpnr, som färgerna. | 5.7, K21–K25 |
| A7 | Stolptabell före påverkas inte, fast `PoleGrid` är delad. | K6 |

### Ingår
- **Filtret** i fliken Stolptabell efter, med grå rader för dolda stolpar och en infotext.
- **Knappen Visa förändrade.**
- **Valet sparas** per användare i systemdatabasen.
- **Snabbmenyns "Kopiera från Stolptabell före"** i Stolptabell efter.
- **Ett nytt rent bibliotek, `libs/poleView`:** vyn, de grå raderna, rullningsmålet och infotexten.
- **Tillägg i fyra bibliotek:**
  - `libs/autoMode`: återställningen (`restoreFromBefore`).
  - `libs/sheetGrid`: dolda rader (`hiddenRow`) och värdens egen inklistring (`SheetHost::pasteBlock`).
  - `libs/systemdb`: valet.
  - `libs/poleGrid`: vyn i tabellen.
- **Tester** i `tests/`.

### Ingår inte
- Filter i Stolptabell före.
- En marginal som går att ändra (±2 är fast).
- Att upprepade tryck på Visa förändrade går från område till område.
- Att utfälla en enskild grå rad (F4).
- Att märka platsen för en borttagen stolpe i tabellen (F3).
- Kopiera från Stolptabell före för flera celler på en gång (F7).
- Ångra.
- Ändringar i projektfilen.

---

## 3. Nuläge

- **`libs/poleGrid` (`pole_grid.h`/`.cpp`, 538 rader):**
  - Tabellens rad är alltid modellens rad: `rows_` plus den tomma raden sist.
  - Alla vägar använder samma index: `committed`, `cellClicked`, `Adapter::copyText`, `baseColor`, `applyEdits`, `addMenuItems` (Infoga rad ovanför, Radera rader…), `insertRowAbove(int)`, `removeRowsAsked(int, int)`, `decorateRow`, `applyLooks`, `showRow` och `host_.addMenuItems`/`removeNote`.
  - `fill()` bygger om med `grid_.setRows`, som nollar cellinställningarna och den aktuella cellen.
  - `PoleLooks` har `compare`, `before`, `marks` och `coordinateSpans`.
  - Snabbmenyn har sheetGrids poster, Infoga rad ovanför, Radera rader… och värdens poster (Sträcka).
- **`libs/sheetGrid`:**
  - `SheetShape::skipRow` ger tomrader som aldrig markeras, målas eller tar emot.
  - Tomrader räknas med vid inklistring, så ett fält för en tomrad försvinner tyst.
  - Kopiering skriver en tom rad för dem.
  - Linberäkningen använder `skipRow` för lastfallstabellens tomrad.
  - Ett högerklick på en rad utanför markeringen markerar raden i den aktuella kolumnen, inte kolumnen under pekaren (`contextMenu`).
- **`libs/autoMode`:**
  - `compareAfter(before, after)` ger `RowState` per rad i efter: Empty, Same, Changed, New eller Ambiguous.
  - Den ger också `beforeRow` och `changed[8]`.
  - Talfält jämförs som tal och text exakt, och raden matchas på trimmad Stolpnr.
  - Stolpar som bara finns i före märks inte.
- **`main_window`:**
  - `updatePoleLooks()` ger efter `compare`, `before` och `marks` via `setLooks`. Den körs av `refreshAutomatic()` efter varje sparad ändring (`PoleHost::polesChanged`).
  - `showPoleTab(stage, row)` anropar `PoleGrid::showRow(row)` med modellens rad (`auto_calc.first_row`).
  - Förklaringen `efterSammaRuta`…`efterNyText` står i `.glazeui` på rad 391–431 med `anchorTop = {"efterAutoGrp", 8}`.
  - `efterGrid` har `anchorTop = {"efterSammaRuta", 8}` (rad 436).
- **`libs/systemdb`:** tabellen `setting(key, value)` finns sedan steg 1. `autosave_minutes` och `last_import_folder` är nycklar utan schemaändring.
- **`libs/projectdb`:** steg 1–6 är committade i 66b338e (0.5.0). Planen rör inte projektdatabasen, så inget nytt steg behövs.
- **GlazeUI v0.11.1, det planen använder:**
  - **`DataGrid`:**
    - Rader och celler: `setRows`, `rowCount`, `setCell`, `addRow`.
    - Cellinställningar: `setCellEditor(rad, kolumn, {glaze::DataGrid::Editor::None})`, `clearCellEditor`, `setCellColor(rad, kolumn, bakgrund, text)`, `setCellToolTip`, `setCellImage`.
    - Aktuell cell och redigering: `setCurrentCell`, `currentRow`, `currentColumn`, `isEditing`, `commitEdit`.
    - Rader och rullning: `rowAt(localX, localY)`, `ensureVisible(rad)`, och från `Panel` `setScroll(x, y)`, `scrollX()` och `scrollY()` (logiska px).
  - **Widget:** `fromWindow`, `focus`, `enable` och `disable`.
  - **`AppWindow`:** `style().gridRowH` (radhöjden i DataGrid, eftersom `setIconSize` inte används) och `commitEdits()`.
  - **Övrigt:**
    - `Menu::addItem` och `popupAt(host, x, y)`.
    - `Dropdown::setItems`, `setSelectedIndex`, `selectedIndex` och `onChange`. `onChange` kommer bara från användaren.
    - `Button::setImg` med `glaze::stockIcon(glaze::StockIcon::GoTo)`.
    - `Label::setBuddy`, `setRole`, `setElide` och `setText`.
  - **Beteenden som planen bygger på:**
    - `setRows` behåller rullningen men klampar den, och nollar den aktuella cellen.
    - `onCommit` kommer synkront vid Enter och Tab.
    - Tab flyttar cellen utan att börja redigera.
    - En cell med `Editor::None` ritas på `fieldReadOnly` med appens bakgrund och text ovanpå. En cell med `setCellEnabled(false)` får ingen appfärg, och därför används den inte här.
    - `RowKind::Category` drar in raderna under sig och döljer dem när den fälls ihop. `RowKind::Blank` ritar ingen text. Ingen av dem duger som grå rad.
    - DataGrid har ingen publik fråga om kolumnen under en punkt, och ett högertryck flyttar inte den aktuella cellen. Den skyddade `Table::columnAt(const Ctx&, float mx) const` ger kolumnen, och `Widget::behaviorCtx()` är publik.

---

## 4. Beslut med motivering

| # | Beslut | Motivering | Var |
|---|---|---|---|
| K1 | **Förändrad** är varje rad i efter som inte är Same: Changed (rosa), New (grön), Empty (helt tom) och Ambiguous (Stolpnr flera gånger i före). | En tom rad mitt i tabellen är nyss infogad eller ett fel, och en osäker rad behöver ses. Bara en rad som är som i före döljs. | 5.2, 6.1 |
| K2 | **Borttagna stolpar** (Stolpnr i före som inte finns i efter) får ingen egen rad. Närmaste rad ovanför och nedanför platsen, i efter, räknas som förändrade. Infotexten räknar de borttagna. | En borttagning är en ändring. Utan detta försvinner platsen in i en grå rad (F3). | 5.2, 6.1 |
| K3 | **±2 räknas på radpositioner** i efter, aldrig på siffrorna i Stolpnr. | Stolpnr är text och kan ha luckor. | 5.2 |
| K4 | **En följd av minst två dolda rader blir en grå rad** med texten "Dolda STO100–STO447 (348 stolpar som i före)" och en tooltip. En ensam dold rad visas som vanligt. | En grå rad i stället för en rad sparar ingenting. | 5.2, 7.1 |
| K5 | **Inga förändrade rader:** Förändrade ±2 visar alla rader, och infotexten säger det. | Annars kan man inte börja arbeta efter en kopiering (F2). | 5.2, 7.2 |
| K6 | **Filtret finns bara i efter.** `PoleGrid` har förvalet utan jämförelse (`setComparesWithBefore(false)`), och då är vyn alltid tabellens rader i ordning. Stolptabell före och Linberäkningens `sheetGrid` beter sig som i 0.5.0. | Bakåtkompatibelt. | 6.3, 6.5 |
| K7 | **Valet sparas per användare** i `system.db`, nyckeln `pole_after_filter` med texten `changed` eller `all`. Ett saknat eller okänt värde ger Förändrade ±2. Ett fel vid sparningen visas i statusraden, och valet gäller ändå under sessionen. | Vyinställning, ingen schemaändring (F1). | 6.4, 8.3 |
| K8 | **Knappen sparar inget.** Den är tänd när minst en rad är förändrad (K1, K2), också skrivskyddat, och släckt annars. | Knappen är en navigering. | 8.3 |
| K9 | **Visa förändrade:** området är vyns rader från den första till den sista förändrade raden, och en grå rad räknas som en rad. Ryms området mitt i vyn läggs dess mitt mitt i vyn, klampat mot början och slutet. Ryms det inte står den första förändrade raden överst. Den första förändrade raden blir aktuell (Stolpnr), och tabellen får fokus. | Som användaren beskrev. Den aktuella raden gör att tangentbordet fortsätter där. | 5.6 |
| K10 | **Byte i rullistan:** till Förändrade ±2 körs Visa förändrade. Till Alla står den aktuella raden kvar på samma höjd i tabellen. | Efter ihopfällningen är området det man vill se. | 8.3 |
| K11 | **Vyn byggs om** när filtret, raderna eller jämförelsen ändras. Det händer aldrig under en öppen redigering och aldrig mitt i tabellens egen händelse: där postas ombyggnaden. Den aktuella cellen följer sin stolpe. Är stolpen dold hamnar den på nästa synliga rad. Den aktuella raden står kvar på samma höjd, och markeringen följer. | Tabellen får inte byggas om under användarens tangent. Det befintliga mönstret postar också (`postIfAlive`). | 5.5 |
| K12 | **Grå rader är passiva.** Varje cell har `Editor::None`. `sheetGrid` markerar och målar dem aldrig och skriver aldrig i dem (`hiddenRow`). Pilarna kan ställa den aktuella cellen på en grå rad, men ingenting går att ändra där. En inklistring skriver ändå i de dolda raderna bakom en grå rad (K13). | Den grå raden är bara visning. | 6.3, 6.5 |
| K13 | **En inklistring skriver i modellens rader i följd**, från ankarets rad och nedåt, också i dolda rader, som om alla rader syntes. Har vyn grå rader planerar `PoleGrid` inklistringen själv mot modellen (`SheetHost::pasteBlock`, och `planSheetPaste` med modellens form). Vyn byggs om direkt, så de dolda rader som får nya värden blir förändrade och syns. En skriven rad som blir lik före kan stå kvar dold. Markeringen blir det inklistrade området. Ingen ruta visas, och inklistringen avbryts inte. En inklistring som skapar nya rader under tabellen fungerar som förut. Utan grå rader går inklistringen som förut genom `sheetGrid`. | Ett block ur Excel är stolpar i följd, och Excel klistrar in i dolda rader på samma sätt. Användarens beslut (F6). | 5.8, 6.3, 6.5 |
| K14 | **Kopiering hoppar över grå rader helt**, och Delete och Ctrl+X tömmer bara synliga celler, som Excel med ett filtrerat område. | Kopian är det man ser, och en tömning får inte ändra data som inte syns. Inklistringen skiljer sig med flit, eftersom ett block måste landa i följd (K13). Därför hamnar en kopia över en grå rad förskjuten om den klistras tillbaka på samma ställe (risk 11). | 6.3 |
| K15 | **Radera rader… och papperskorgen tar bara synliga rader.** Går markeringen över en grå rad säger frågan det (7.4). | Dolda rader får aldrig raderas utan att synas. | 6.5 |
| K16 | **Infoga rad ovanför** infogar i modellen ovanför den valda stolpen. Den tomma raden är förändrad (K1) och syns. | Man infogar för att skriva i raden. | 6.5 |
| K17 | **En grå rad gör ingenting vid klick.** Högerklick på den visar en egen meny med bara "Visa alla rader": filtret blir Alla och sparas. | Enkelt, och inget eget tillstånd (F4). | 6.5 |
| K18 | **`showRow` på en dold rad** sätter filtret till Alla, säger det till värden (sparas och rullistan) och visar statusraden. Det gäller "Gå till Stolptabell efter". | Målet ska alltid synas (F5). | 6.5 |
| K19 | **Automatiskt läge påverkas inte:** sträckorna, rullistorna, synkningen och `auto_calc` använder modellens rader. Flaggor och fel syns bara på synliga rader. | Filtret är bara visning. | 5.4 |
| K20 | **Logiken bor i ett nytt rent bibliotek, `libs/poleView`.** `PoleGrid` och huvudfönstret använder samma funktioner, så vyn och infotexten alltid stämmer. | Testbart utan fönster. `autoMode` handlar om automatiskt läge och jämförelsen, inte om vyn. | 6.1 |
| K21 | **"Kopiera från Stolptabell före"** står i snabbmenyn i Stolptabell efter, efter "Radera rader…". Det gäller bara cellen under pekaren vid högerklicket, aldrig markeringen. Ett högerklick utanför markeringen markerar just den cellen. Inne i en markering står markeringen kvar, men alternativet gäller ändå bara den högerklickade cellen. Cellen får före-cellens text. | Användarens beslut (F7): det gäller cellen man pekar på. | 5.7, 6.2, 6.5 |
| K22 | **Alternativet är tänt** bara när projektet är skrivbart och den högerklickade cellen skiljer sig från före i en Changed-rad. Det är släckt på en cell som är lika, på Same-, New-, Empty- och Ambiguous-rader, på Stolpnr, på den tomma raden, i radera-kolumnen och när pekaren inte står på en cell. Det finns inte i Stolptabell före och inte i den grå radens meny. | Ett släckt alternativ visar att det finns men inte gäller här. | 5.7, 6.5 |
| K23 | **Stolpnr återställs aldrig.** Raden matchas på den, så i en Changed-rad är den redan lika. En rad med bytt Stolpnr är New och kan inte återställas: man skriver tillbaka Stolpnr för hand, och då jämförs raden igen. | Nyckeln får inte gissas. | 6.2 |
| K24 | **Återställningen är en vanlig ändring:** den sparas direkt via värden (`savePoles`), och färgerna, de automatiska beräkningarna och filtret räknas om. Blir raden som i före kan den försvinna in i en grå rad, och då hamnar den aktuella cellen på nästa synliga rad (K11). Statusraden säger vilken cell som fick vilket värde (7.5). | Samma väg som en redigerad cell. | 5.7, 6.5 |
| K25 | **Texten skrivs som den står i före:** kanonisk decimaltext med före-tabellens decimaler, och en tom cell blir tom. | Det är "värdet i före". | 6.2 |
| K26 | **En infotext** står efter knappen, till exempel "Förändrade: 3 av 801 rader · Borttagna: 1 · Visas: 15 av 801". | Man ser direkt hur mycket som är dolt. | 7.2 |

---

## 5. Vyn

### 5.1 Modellen och vyn
- **Modellen** är `PoleGrid::rows_`, raderna i efter i tabellens ordning. Den ändras inte av filtret.
- **Vyn** (`poleview::PoleView`) är tabellens rader i DataGrid. Varje vyrad är en av tre sorter:
  - `Pole`: en rad i modellen.
  - `Hidden`: en grå rad för modellens rader `first..last`, minst två.
  - `Blank`: den tomma raden sist, med `first = last = rows_.size()`.
- **Med filtret Alla, eller utan jämförelse,** är vyn identiteten: en `Pole` per rad och `Blank` sist. Det är exakt dagens tabell.

### 5.2 Regler för Förändrade ±2 (`poleview::buildView`)
1. `marks = poleview::filterMarks(before, after)`. En rad är märkt när den inte är Same (K1) eller när den står närmast en borttagen stolpe (K2).
2. Ingen märkt rad ger identiteten (K5).
3. Det synliga är ett enda område: från den första märkta raden minus `kMargin = 2` till den sista märkta raden plus `kMargin`, klampat till tabellen. Allt mellan den första och den sista märkta raden syns, också när de ligger långt isär (K3, F8).
4. Raderna före området och raderna efter det blir var sin `Hidden`-rad när de är minst två. En ensam rad blir en `Pole` (K4).
5. `Blank` står sist.

**Närmaste rad vid en borttagen stolpe (`removedAnchors`):**
- En rad i före är borttagen när dess trimmade Stolpnr inte är tom och inte finns på någon rad i efter.
- För varje borttagen rad söks närmaste rad ovanför den i före vars Stolpnr finns på exakt en rad i efter. Den raden i efter är en ankarrad. Samma sökning görs nedanför.
- Räkningen görs i O(n) med två genomgångar: för varje rad i före hålls närmaste ankarrad ovanför och nedanför.
- Svaret är radindex i efter, sorterade och utan dubbletter.

### 5.3 Användarens exempel
- **Tabellen:** STO100–STO900, alltså 801 rader med index 0–800. STO450 är index 350, och bara STO450 är ändrad.
- **Vyn:** `[Hidden 0–347, Pole 348, 349, 350, 351, 352, Hidden 353–800, Blank 801]`.
- **Så visas den:**
  - "Dolda STO100–STO447 (348 stolpar som i före)",
  - STO448–STO452,
  - "Dolda STO453–STO900 (448 stolpar som i före)",
  - den tomma raden.
- **Även STO460 ändrad** (index 360): STO448–STO462 syns, och de grå raderna är STO100–STO447 och STO463–STO900.
- **STO120 och STO880 ändrade:** STO118–STO882 syns, och de grå raderna är STO100–STO117 och STO883–STO900.

### 5.4 Index i vyn och modellen i `PoleGrid`
| Väg | Vyns rad in | Modellens rad |
|---|---|---|
| `committed(e)` | `e.row` | `Pole`: raden. `Blank`: ny rad sist. `Hidden`: ingenting |
| `cellClicked(e)` (papperskorgen) | `e.row` | Bara `Pole` |
| `Adapter::copyText` | rad | `Pole`: `cellText(m, …)`. Annars tom (grå rader frågas aldrig, K14) |
| `Adapter::baseColor` | rad | `Pole`/`Blank`: `cellLook(looks_, m, …)` med `m = rows_.size()` för `Blank`. `Hidden`: `kHiddenRow` |
| `Adapter::applyEdits` (Delete, Ctrl+X och inklistring utan grå rader) | `edit.row` | `poleview::modelRow`. `Blank` och raderna under den ger nya rader (`rows_.size() + k`) |
| `Adapter::pasteBlock` (inklistring med grå rader) | `anchor.row` | `modelRow(view_, anchor.row)`, och blocket i följd i modellens rader därifrån, också dolda (K13, 5.8) |
| Infoga rad ovanför | `selection.row0` | Bara `Pole` |
| Radera rader… | `row0..row1` | Alla `Pole` i intervallet (K15) |
| Kopiera från Stolptabell före | cellen under pekaren (`menuCell_`) | Modellens rad för en `Pole` och kolumnen, när den är 1–7 (K21, 5.7) |
| `host_.addMenuItems(menu, rad)` | `row0` | Modellens rad för en `Pole`, annars -1 |
| `host_.removeNote(first, last)` | | Modellens rader, en gång per sammanhängande följd, med noterna skilda av en radbrytning |
| `showRow(m)` | | Modellens rad, och vyns rad via `viewRowOf` (K18) |
| `cellLook`, `marks`, `compare`, `coordinateSpans` | | Alltid modellens rad. Radtonens växling följer modellens rad |

### 5.5 Ombyggnaden av vyn
- **`desiredView()`:**
  - Utan jämförelse eller med filtret All: `identityView(rows_.size())`.
  - Annars: `buildView(rows_.size(), filterMarks(looks_.before, rows_), Filter::Changed)`.
  - Jämförelsen räknas alltid om ur `rows_`, så att den aldrig ligger efter.
- **`refreshView()`:**
  1. Är `desiredView() == view_` sätts `viewPending_ = false`, och inget mer görs.
  2. Annars sätts `viewPending_ = true`.
  3. Är `inEvent_ > 0` eller `grid_.isEditing()` körs `postRebuild()`. Annars körs `rebuildView()` direkt.
- **`inEvent_`** räknas upp och ner (RAII, `EventScope`) i `committed`, `cellClicked`, `keyDown`, `mouseUp`, `contextMenu`, `Adapter::applyEdits` och `pasteIntoModel`.
  - Där får tabellen inte byggas om mitt i händelsen, eftersom `sheetGrid` markerar blocket i vyns rader efter `applyEdits`.
  - `pasteIntoModel` bygger själv om vyn och sätter markeringen (5.8).
- **`postRebuild()`:** postar högst en gång (`rebuildPosted_`) en lambda bakom `alive_`. Lambdan nollar `rebuildPosted_` och kör `rebuildView()` om `viewPending_` är sann.
- **Slutet av `committed`, `keyDown` och `mouseUp`:** `if (viewPending_) postRebuild();`. Det fångar en ombyggnad som väntade på att en redigering skulle sluta, till exempel efter Escape.
- **`rebuildView()`:**
  1. Är `grid_.isEditing()` sann görs ingenting, och `viewPending_` står kvar.
  2. `next = desiredView()` och `viewPending_ = false`. Är `next == view_` görs inget mer.
  3. Dessa sparas: den aktuella cellen (`curV`, `curC`), markeringen (`sel = sheet_->selection()`), `h = owner_.style().gridRowH` och `scroll = grid_.scrollY()`.
  4. Ankaret: `anchorV = curV` om `curV >= 0`, annars `int(scroll / h)` klampat till vyn. `anchorM` är modellens rad för `anchorV` (`Pole`: raden, `Hidden`: `first`, `Blank`: `rows_.size()`). `offset = anchorV * h - scroll`.
  5. `m0` och `m1` är modellens rader för `sel.row0` och `sel.row1`, på samma sätt.
  6. `view_ = next`, och sedan körs `fill()`.
  7. Fanns en aktuell cell:
     - `n0 = visibleViewRow(view_, m0, +1)` och `n1 = max(n0, visibleViewRow(view_, m1, -1))`.
     - En markering på en cell ger `sheet_->select({n0, sel.column0}, {n0, sel.column0})`.
     - Annars `sheet_->select({n0, sel.column0}, {n1, sel.column1})`.
  8. Rullningen: `grid_.setScroll(grid_.scrollX(), max(0, visibleViewRow(view_, anchorM, +1) * h - offset))`.
- **Strukturella ändringar** sätter vyn själva, eftersom modellens index flyttas:
  - `setRows`: `view_ = desiredView()` före `fill()`.
  - `insertRowAbove`: samma sak efter infogningen.
  - `removeRowsAsked`: samma sak efter borttagningen.
  - `Adapter::applyEdits` och `committed` på den tomma raden: `view_ = resizeView(view_, rows_.size())` behåller vyns form, så att `sheetGrid`s markering stämmer. Sedan körs `refreshView()`, som postar.
  - `pasteIntoModel` (vyn har grå rader): `view_ = desiredView()` direkt efter ändringarna och `fill()`. Markeringen blir det inklistrade området (5.8).
- **`fill()`** räknar aldrig om filtret. Den bygger tabellen ur `view_`. Det gäller alltid att `view_.rows.back()` är `Blank` med `first == rows_.size()`, och att `grid_.rowCount() == view_.rows.size()` efter `fill()`.

### 5.6 Visa förändrade (`PoleGrid::showChanged`)
1. Utan jämförelse svarar funktionen false.
2. `marks = filterMarks(looks_.before, rows_)` och `range = markedRange(marks)`. Utan märkt rad svarar den false.
3. Är `viewPending_` sann körs `rebuildView()`.
4. `vf = viewRowOf(view_, range.first)` och `vl = viewRowOf(view_, range.last)`.
5. `sheet_->select({vf, 0}, {vf, 0})` och `grid_.focus()`.
6. Vyns höjd mäts. Ett öppet fönster behövs, och det är öppet vid ett knapptryck:
   ```cpp
   const float h = owner_.style().gridRowH;
   const int n = grid_.rowCount();
   grid_.setScroll(grid_.scrollX(), 0.0f);
   grid_.ensureVisible(n - 1);
   const float s = grid_.scrollY();
   const float viewport = s > 0.0f ? float(n) * h - s : float(n) * h;   // s == 0: allt ryms
   ```
7. `grid_.setScroll(grid_.scrollX(), poleview::scrollTop(vf, vl, n, h, viewport))`, och funktionen svarar true.

### 5.7 Kopiera från Stolptabell före (den högerklickade cellen)
- **Cellen under pekaren** (`cellUnderPointer(e)`) tas fram i `PoleGrid::contextMenu(e)` innan `sheetGrid` bygger menyn.
  - **Raden:** `v = grid_.rowAt(p->x, p->y)` med `p = grid_.fromWindow(gfx::Pointf{e.x, e.y})`.
  - **Kolumnen:** `c = GridProbe::columnUnder(grid_, e.x)`.
    - DataGrid har ingen publik fråga om kolumnen i v0.11.1, och ett högertryck flyttar inte den aktuella cellen.
    - `GridProbe` når den skyddade `glaze::Table::columnAt(const glaze::Ctx&, float mx) const` genom en pekare till medlem som bildas i en härledd klass. Det är tillåtet i C++, och klassen skapas aldrig.
    - Den står i den anonyma namnrymden i `pole_grid.cpp`:
    ```cpp
    // Kolumnen under en punkt i tabellen. GlazeUI v0.11.1 har bara den skyddade Table::columnAt, och en
    // pekare till medlem som bildas här, i en härledd klass, får anropas på vilken DataGrid som helst.
    // Klassen skapas aldrig. windowX är fönstrets fysiska px (ContextMenuArgs::x). -1 utanför kolumnerna.
    struct GridProbe : glaze::DataGrid {
        static int columnUnder(glaze::DataGrid& grid, float windowX)
        {
            int (glaze::Table::*hit)(const glaze::Ctx&, float) const = &GridProbe::columnAt;
            return (grid.*hit)(grid.behaviorCtx(), windowX);
        }
    };
    ```
    - Implementeringsagenten kontrollerar i GlazeUI-källan vilket koordinatsystem `columnAt` väntar sig (fönstrets eller tabellens) och justerar argumentet därefter.
  - **`cellUnderPointer`** svarar `{v, c}` som de är. `menuCell_` blir `{v, c}`, eller `{-1, -1}` när någon av dem är -1.
- **Markeringen:**
  - Med `compares_`, när `menuCell_` är en cell utanför `sheet_->selection()`, körs `sheet_->select(menuCell_, menuCell_)`. Den högerklickade cellen blir då markerad och aktuell, och `sheet_->contextMenu(e)` flyttar inte markeringen igen.
  - Inne i markeringen står den kvar.
  - Utan `compares_` (Stolptabell före) görs inget av detta, och högerklicket beter sig som förut.
- **Posten** (`Adapter::addMenuItems`, bara med `compares_`):
  - `m = modelRow(view_, menuCell_.row)` när vyns rad är en `Pole`, annars -1.
  - `edit = m >= 0 ? automode::restoreFromBefore(looks_.before, automode::compareAfter(looks_.before, rows_), m, menuCell_.column) : std::nullopt`.
  - Posten är aktiv när `!readOnly_ && edit`.
  - Ett klick kör `restoreCell(m, menuCell_.column)`.
- **`restoreCell(m, c)`:**
  1. Gör ingenting skrivskyddat.
  2. Kör `owner_.commitEdits()`.
  3. Räknar `edit` igen. Saknas den görs inget mer.
  4. Kör `pt::applyEdits(rows_, {*edit})` och `fill()`. Vyns form är oförändrad.
  5. Kör `save()`, och `host_.polesChanged()` när något skrevs.
  6. Visar statusraden (7.5) och kör `refreshView()`. Det är utanför tabellens händelse, så ombyggnaden sker direkt.

### 5.8 Inklistringen när vyn har grå rader (`PoleGrid::pasteIntoModel`)
`sheetGrid` frågar värden först (`SheetHost::pasteBlock`, 6.3), och `Adapter::pasteBlock` svarar med `pasteIntoModel(block, anchor)`.
1. Har vyn ingen `Hidden`-rad (`hasHiddenRows()`) svarar funktionen false. Då klistrar `sheetGrid` in som förut, eftersom vyns rader då är modellens.
2. `EventScope`. Skrivskyddat svarar funktionen true och gör ingenting.
3. `m = modelRow(view_, anchor.row)`.
   - Ankaret är aldrig en grå rad, eftersom `sheetGrid` klampar markeringen förbi dem.
   - Den tomma raden ger `rows_.size()`.
   - Är `m` -1 svarar funktionen true och gör inget mer.
4. `paste = sheetgrid::planSheetPaste({m, anchor.column}, block, shape)`.
   - `shape` är `baseShape()` med `rowCount = rows_.size() + 1` och utan `hiddenRow`.
   - Blocket landar alltså i modellens rader m, m + 1 och så vidare i följd, också i dolda rader. Rader under tabellen blir nya rader.
5. `paste.rejected` ger rutan med texten och titeln "Klistra in" (`MessageBoxButtons::OK`, `MessageBoxType::Warning`), som i `sheetGrid`. Svaret är true.
6. Tom `paste.edits` ger statusraden i 7.3, och svaret är true.
7. Ändringarna skrivs med `pt::applyEdits(rows_, …)` med modellens rader som de står, och sedan körs `pt::trimTrailingEmptyRows(rows_)`.
8. `view_ = desiredView()`, `viewPending_ = false` och `fill()`. De skrivna rader som blev förändrade syns nu.
9. Markeringen blir det inklistrade området:
   - `n0 = visibleViewRow(view_, paste.area.row0, +1)`,
   - `n1 = max(n0, visibleViewRow(view_, paste.area.row1, -1))`,
   - `sheet_->select({n0, paste.area.column0}, {n1, paste.area.column1})`.
10. Sparningen: `const bool wrote = dirty(); if (save() && wrote) host_.polesChanged();`.
11. Statusraden enligt 7.3, och svaret true.

---

## 6. API (bindande)

### 6.1 `libs/poleView/pole_view.h` (nytt bibliotek `kalkyl_poleview`, namnrymden `kalkyl::poleview`)
```cpp
#pragma once
#include <string>
#include <vector>

#include "poleTable/pole_table.h"

namespace kalkyl::poleview {

// Filtret i Stolptabell efter. Changed är Förändrade ±2: de förändrade raderna och kMargin rader
// runt dem syns, resten står ihop i grå rader. All visar alla rader.
enum class Filter { Changed, All };

// Antalet rader ovanför och nedanför en förändrad rad som också syns (±2).
inline constexpr int kMargin = 2;

// Vad en rad i vyn står för: en rad i tabellen, en grå rad för dolda rader eller den tomma raden sist.
enum class ViewKind { Pole, Hidden, Blank };

// En rad i vyn. Pole: first = last = raden i tabellen. Hidden: de dolda raderna first..last, minst
// två. Blank: first = last = antalet rader i tabellen.
struct ViewRow {
    ViewKind kind = ViewKind::Pole;
    int first = 0;
    int last = 0;
    bool operator==(const ViewRow&) const = default;
};

// Vyn: raderna i tabellens ordning och alltid en Blank sist.
struct PoleView {
    std::vector<ViewRow> rows;
    bool operator==(const PoleView&) const = default;
};

// Raderna i after närmast platsen där en stolpe i before saknas i after (planen 5.2): för varje
// rad i before med Stolpnr som inte finns i after, närmaste rad ovanför och nedanför i before vars
// Stolpnr finns på exakt en rad i after, och den raden i after. Sorterade, utan dubbletter.
std::vector<int> removedAnchors(const std::vector<poletable::PoleRow>& before,
                                const std::vector<poletable::PoleRow>& after);

// Antalet rader i before med Stolpnr (trimmad, inte tom) som inte finns på någon rad i after.
int removedCount(const std::vector<poletable::PoleRow>& before, const std::vector<poletable::PoleRow>& after);

// En post per rad i after: sant när raden inte är Same i automode::compareAfter (Changed, New,
// Empty och Ambiguous) eller står i removedAnchors.
std::vector<bool> filterMarks(const std::vector<poletable::PoleRow>& before,
                              const std::vector<poletable::PoleRow>& after);

// Första och sista sanna posten i marks, eller {-1, -1}.
struct MarkedRange {
    int first = -1;
    int last = -1;
    bool operator==(const MarkedRange&) const = default;
};
MarkedRange markedRange(const std::vector<bool>& marks);

// Vyn med en Pole per rad och Blank sist.
PoleView identityView(int count);

// Vyn för count rader (planen 5.2). All, eller ingen sann post i marks, ger identityView. Poster
// bortom count ignoreras och saknade poster är falska.
PoleView buildView(int count, const std::vector<bool>& marks, Filter filter);

// Samma vy för en tabell med count rader: rader bortom count tas bort, en Hidden klipps (en enda
// rad kvar blir Pole), nya rader blir Pole före Blank. Formen bevaras, så att markeringen stämmer.
PoleView resizeView(const PoleView& view, int count);

// Antalet rader i tabellen: Blank.first (0 för en tom vy).
int rowCount(const PoleView& view);

// Tabellens rad för vyns rad v. Pole ger raden, Hidden och v < 0 ger -1. Blank och raderna under
// den ger rowCount + (v - blankindex), alltså nya rader vid en inklistring.
int modelRow(const PoleView& view, int v);

// Vyns rad för tabellens rad m: Pole-raden, Hidden-raden som döljer den, eller Blank för m >= rowCount
// och m < 0 ger -1.
int viewRowOf(const PoleView& view, int m);

// Vyns rad för m som inte är Hidden: Pole-raden, annars raden efter den grå (step +1) eller före den
// (step -1, och efter när den grå står först). -1 för m < 0.
int visibleViewRow(const PoleView& view, int m, int step);

// Sant när vyns rad v är en Hidden-rad. Falskt utanför vyn.
bool isHiddenRow(const PoleView& view, int v);

// Radens namn i texterna: trimmad Stolpnr, eller "rad {m + 1}" när den är tom.
std::string rowLabel(const std::vector<poletable::PoleRow>& rows, int m);

// Den grå radens text och tooltip (planen 7.1).
std::string hiddenText(const std::vector<poletable::PoleRow>& rows, const ViewRow& hidden);
std::string hiddenToolTip(const ViewRow& hidden);

// Var vyn ska rullas, i logiska px, för att visa vyns rader first..last (planen K9): mitten av
// området mitt i synfältet när det ryms, annars first överst. Klampat till 0..(viewRows * rowHeight
// - viewportHeight). rowHeight <= 0 eller viewportHeight <= 0 ger 0.
float scrollTop(int first, int last, int viewRows, float rowHeight, float viewportHeight);

// Siffrorna i infotexten. rows: raderna i after. changed: raderna som inte är Same. marked: sanna
// poster i filterMarks. removed: removedCount. shown: Pole-raderna i buildView med filtret.
struct Summary {
    int rows = 0;
    int changed = 0;
    int marked = 0;
    int removed = 0;
    int shown = 0;
    bool operator==(const Summary&) const = default;
};
Summary summarize(const std::vector<poletable::PoleRow>& before, const std::vector<poletable::PoleRow>& after,
                  Filter filter);

// Infotexten bredvid Visa förändrade (planen 7.2).
std::string summaryText(const Summary& summary, Filter filter);

} // namespace kalkyl::poleview
```

`libs/poleView/CMakeLists.txt`:
```cmake
# Vyn av Stolptabell efter som rena funktioner: filtret Förändrade ±2, de grå raderna, rullningsmålet
# för Visa förändrade och infotexten. Includes skrivs som "poleView/pole_view.h".
add_library(kalkyl_poleview STATIC pole_view.cpp)
target_include_directories(kalkyl_poleview PUBLIC ${PROJECT_SOURCE_DIR}/libs)
target_link_libraries(kalkyl_poleview PUBLIC kalkyl_poletable
                      PRIVATE kalkyl_automode kalkyl_tabletext kalkyl_warnings)
```
`libs/CMakeLists.txt` får `add_subdirectory(poleView)` direkt efter `add_subdirectory(autoMode)`. Kommentaren överst kompletteras med "autoMode ← poleView ← poleGrid".

### 6.2 `libs/autoMode/auto_mode.h`, tillägg (sist i namnrymden)
```cpp
// Ändringen som skriver tillbaka värdet ur Stolptabell före i cellen row, column i Stolptabell efter:
// bara när raden är Changed och kolumnen skiljer sig (compare[row].changed), med före-cellens text som
// den står. Tomt svar för Stolpnr (kPole), för en kolumn utanför 0-7, för en rad utanför compare och
// när cellen inte skiljer sig. compare är compareAfter(before, efter).
std::optional<poletable::Edit> restoreFromBefore(const std::vector<poletable::PoleRow>& before,
                                                 const std::vector<RowCompare>& compare, int row, int column);
```

### 6.3 `libs/sheetGrid/sheet_grid.h`, tillägg i `SheetShape` och `SheetHost`
I `SheetShape`, sist i structen:
```cpp
    // Rader som står för dolda rader (filtrets grå rader). Som skipRow markeras, målas och tar de
    // emot inget, och kopieringen hoppar över dem helt. En värd som vill klistra in i raderna bakom
    // dem tar inklistringen själv (SheetHost::pasteBlock). Tom = inga.
    std::function<bool(int row)> hiddenRow;
```
I `SheetHost`, efter `addMenuItems`:
```cpp
    // Inklistringen av ett block vid ankaret, markeringens övre vänstra cell. Svarar true när värden
    // har tagit hand om den själv (planeringen, ändringarna, markeringen och statusraden), och då gör
    // SheetGrid inget mer. Förvalet svarar false, och SheetGrid klistrar in som förut.
    virtual bool pasteBlock(const std::vector<std::vector<std::string>>& block, CellPos anchor);
```
**Beteendet i `sheet_grid.cpp`:**
- En intern hjälpfunktion `isPassiveRow(shape, row)` är `isSkipRow || (shape.hiddenRow && shape.hiddenRow(row))`. Den ersätter `isSkipRow` i:
  - `paintCellOf`,
  - `lands` i `planSheetPaste`,
  - `clampPos`,
  - `contextMenu` (högerklick på raden flyttar inte markeringen),
  - `clearSelectionCells`.
- **`pasteAtSelection`:** direkt efter raden `const CellRect rect = rectOf(anchor_, extent_);`, och före `planSheetPaste`, körs `if (host_.pasteBlock(block, {rect.row0, rect.column0})) return;`.
- **`SheetHost::pasteBlock`:** förvalet `return false;` står i `sheet_grid.cpp` bredvid `SheetHost::addMenuItems`.
- **`planSheetPaste`** avbryter aldrig på grund av `hiddenRow`. En dold rad tar emot inget men räknas med, som en `skipRow`.
- **`copySelection`:** en rad med `hiddenRow` hoppas över helt. En `skipRow` ger som förut en tom rad. Statusraden räknar de rader som kopierades.
- **`skipRow` beter sig som förut.** Linberäkningen använder varken `hiddenRow` eller `pasteBlock`.

### 6.4 `libs/systemdb/system_db.h`, tillägg
```cpp
// Filtret i Stolptabell efter: Förändrade ±2 (Changed) eller Alla (All). Speglar poleview::Filter
// utan att bero på poleView.
enum class AfterFilter { Changed, All };
```
I `SystemDb`, efter `setLastImportFolder`:
```cpp
    // Filtret i Stolptabell efter. Changed vid fel, saknat eller okänt värde. Nyckeln pole_after_filter.
    AfterFilter afterFilter();
    // Sparar filtret som text: changed eller all.
    core::Result<void> setAfterFilter(AfterFilter filter);
```
Implementationen följer `themeMode`/`setThemeMode`: `setting("pole_after_filter")` och `setSetting`. Det behövs ingen schemaändring och inget migreringssteg, eftersom tabellen `setting` finns sedan steg 1.

### 6.5 `libs/poleGrid/pole_grid.h`, tillägg och ändringar
- **`#include "poleView/pole_view.h"`.**
- **I `PoleGridHost`**, sist. Den är inte ren virtuell, så att den är bakåtkompatibel. Förvalet definieras i `pole_grid.cpp`.
  ```cpp
      // PoleGrid bytte filtret själv: Visa alla rader i den grå radens snabbmeny, eller showRow på en
      // dold rad. Värden sparar valet och visar det i rullistan. Förvalet gör ingenting.
      virtual void filterChanged(poleview::Filter filter);
  ```
- **I `PoleGrid`, publikt**, efter `setLooks`:
  ```cpp
      // Tabellen jämförs med Stolptabell före (Stolptabell efter): filtret Changed går att använda och
      // snabbmenyn får Kopiera från Stolptabell före. Förval av, och då är filtret alltid All.
      void setComparesWithBefore(bool on);
      // Filtret. Sparar en öppen cell, bygger om vyn och behåller den aktuella raden på samma höjd (5.5).
      void setFilter(poleview::Filter filter);
      poleview::Filter filter() const;
      // Visa förändrade (5.6): den första förändrade raden blir aktuell och området rullas fram. Svarar
      // false och gör ingenting när ingen rad är förändrad eller tabellen inte jämförs med före.
      bool showChanged();
  ```
- **Kommentaren till `showRow`** blir: "Gör tabellens rad aktuell och synlig och markerar dess Stolpnr. Är raden dold av filtret blir filtret All, värden får `filterChanged` och statusraden förklarar."
- **Privat, nytt eller ändrat.** Varje del får en kommentar på 2–5 rader.
  - `struct EventScope;`: räknar `inEvent_` upp i konstruktorn och ner i destruktorn.
  - `poleview::PoleView desiredView() const;`, `void refreshView();`, `void postRebuild();` och `void rebuildView();` (5.5).
  - `std::vector<int> selectedModelRows(const sheetgrid::CellRect& rect) const;`: modellens rader för `Pole` i `rect.row0..row1`, stigande.
  - `void insertRowAbove(int modelRow);` (som förut, men med modellens rad).
  - `void removeRowsAsked(std::vector<int> modelRows);` ersätter `removeRowsAsked(int first, int last)`.
  - `void restoreCell(int modelRow, int column);` (5.7).
  - `sheetgrid::CellPos cellUnderPointer(const glaze::ContextMenuArgs& e);`: raden och kolumnen under pekaren, var för sig -1 utanför (5.7).
  - `bool pasteIntoModel(const std::vector<std::vector<std::string>>& block, sheetgrid::CellPos anchor);` (5.8).
  - `sheetgrid::SheetShape baseShape() const;`: formen utan `rowCount` och `hiddenRow`, till konstruktorn och `pasteIntoModel`.
  - `bool hasHiddenRows() const;`: sant när vyn har en `Hidden`-rad.
  - **Medlemmar:**
    - `bool compares_ = false;`
    - `poleview::Filter filter_ = poleview::Filter::All;`
    - `poleview::PoleView view_ = poleview::identityView(0);`
    - `bool viewPending_ = false;`
    - `bool rebuildPosted_ = false;`
    - `int inEvent_ = 0;`
    - `glaze::Menu hiddenMenu_;` (den grå radens snabbmeny).
    - `sheetgrid::CellPos menuCell_{-1, -1};` (den högerklickade cellen, 5.7).
- **Konstruktorn:**
  - Den tar formen ur `baseShape()`, som är samma form som i dag.
  - Den sätter `shape.rowCount = grid_.rowCount()` och `shape.hiddenRow = [this](int row) { return poleview::isHiddenRow(view_, row); };`.
  - `GridProbe` (5.7) står i den anonyma namnrymden i `pole_grid.cpp`.
- **Färgerna** för en grå rad, nya konstanter i `pole_grid.cpp`:
  - `kHiddenRow{128, 128, 128, 48}` (bakgrund),
  - `kHiddenText{128, 128, 128, 255}` (text).
- **`setComparesWithBefore(on)`:** `compares_ = on`. Vid `!on` blir `filter_ = All`. Sedan körs `refreshView()`.
- **`setFilter(f)`:**
  1. Utan `compares_` blir `f = All`.
  2. Är `f == filter_` görs ingenting.
  3. `owner_.commitEdits()`, `filter_ = f`, `viewPending_ = true` och `rebuildView()`. Den anropas aldrig ur tabellens egen händelse.
- **`setRows`:** `rows_` och `saved_` sätts som förut. Sedan `view_ = desiredView()`, `viewPending_ = false` och `fill()`.
- **`setLooks(looks)`:** `looks_ = move(looks)`, `applyLooks()` och `refreshView()`.
- **`fill()`:** varje `ViewRow` ger en tabellrad.
  - `Pole`: cellerna `cellText(m, c, true)` och en tom radera-cell.
  - `Hidden`: nio celler, `hiddenText(rows_, vr)` i kolumn 0 och resten tomma.
  - `Blank`: `blankRow()`.
  - Därefter körs `decorateRow(v)` på varje rad, `applyLooks()`, den aktuella cellen sätts tillbaka klampad som förut, och `sheet_->rowsChanged()` körs.
- **`decorateRow(v)`:**
  - `Pole` och `Blank` som förut, med `rows_.size()` som gräns.
  - `Hidden`: `setCellEditor(v, c, {glaze::DataGrid::Editor::None})` för `c = 0..kDeleteColumn`.
- **`applyLooks()`:**
  - `Pole` (modellens rad `m`) och `Blank` (`m = rows_.size()`): som förut, med `m` till `cellLook`, `cellText`, `rows_` och `coordinateSpans`, och `v` till alla `grid_`-anrop.
  - `Hidden`:
    - `setCellToolTip(v, c, hiddenToolTip(vr))` för `c = 0..7`.
    - Inga bilder i kolumn 0 och `kSpan`.
    - `setCellColor(v, c, kHiddenRow, kHiddenText)` för `c = 0..kDeleteColumn`.
    - Tom tooltip i radera-kolumnen.
  - `sheet_->paintAll()` sist. Den målar aldrig grå rader.
- **`committed(e)`:** `EventScope`. Vyns rad tolkas enligt 5.4.
  - `Hidden`: ingenting.
  - `Blank`: den nya raden läggs till som i dag. Efter `grid_.addRow(...)` sätts `view_ = resizeView(view_, rows_.size())`.
  - Efter sparningen körs `refreshView()`, och sist `if (viewPending_) postRebuild();`.
- **`cellClicked(e)`:** `EventScope`. Bara för `Pole` postas `removeRowsAsked({m})` bakom `alive_`.
- **`keyDown` och `mouseUp`:** `EventScope`, sedan `sheet_` som förut, och sist `if (viewPending_) postRebuild();`.
- **`contextMenu(e)`:** `EventScope`, och `cell = cellUnderPointer(e)` (5.7).
  - Är `cell.row` en `Hidden`-rad:
    1. `hiddenMenu_.items.clear()`.
    2. En enda post, "Visa alla rader". Ett klick kör `setFilter(All)`, `host_.filterChanged(All)` och `host_.showStatus` med 7.5.
    3. `hiddenMenu_.popupAt(grid_, e.x, e.y)`, och sedan return.
  - Annars:
    1. `menuCell_` sätts enligt 5.7.
    2. Med `compares_` markeras cellen när den ligger utanför markeringen (5.7).
    3. Sist körs `sheet_->contextMenu(e)`.
- **`Adapter::addMenuItems(menu, selection)`:**
  - `selected = selectedModelRows(selection)`, och `m0` är modellens rad för `selection.row0` om den är `Pole`, annars -1.
  - "Infoga rad ovanför": aktiv när den är skrivbar och `m0 >= 0`. Klick kör `insertRowAbove(m0)`.
  - "Radera rader…": aktiv när den är skrivbar och `!selected.empty()`. Klick postar `removeRowsAsked(selected)`.
  - Bara med `compares_`: "Kopiera från Stolptabell före" enligt 5.7.
  - Sist `host_.addMenuItems(menu, m0)`.
- **`Adapter::applyEdits(edits)`:** `EventScope`. Den används för Delete, Ctrl+X och en inklistring utan grå rader.
  1. Varje ändring får modellens rad med `modelRow`. Rader med -1 hoppas över.
  2. `pt::applyEdits` och `pt::trimTrailingEmptyRows`.
  3. `view_ = resizeView(view_, rows_.size())` och `fill()`.
  4. Sparningen och `polesChanged` som förut, och sist `refreshView()`.
- **`Adapter::pasteBlock(block, anchor)`:** `return grid_.pasteIntoModel(block, anchor);` (5.8).
- **`insertRowAbove(m)`:**
  1. `owner_.commitEdits()` och infogningen i `rows_`.
  2. `view_ = desiredView()` och `fill()`.
  3. `v = visibleViewRow(view_, m, +1)` och `sheet_->select({v, 0}, {v, 0})`.
  4. Sparningen som förut.
- **`removeRowsAsked(rows)`:**
  1. `owner_.commitEdits()`. Raderna sorteras, dubbletter och rader utanför tas bort, och är listan tom görs inget mer.
  2. Frågan:
     - Är raderna en följd (`rows.back() - rows.front() + 1 == rows.size()`) gäller dagens text med `first = rows.front()` och `last = rows.back()`.
     - Annars gäller texten i 7.4.
  3. Noten är `host_.removeNote(a, b)` per sammanhängande följd, med de icke-tomma noterna skilda av `char{10}`. Den läggs efter två `char{10}`.
  4. Vid Ja raderas raderna bakifrån. Sedan `view_ = desiredView()`, `fill()` och `select` på `visibleViewRow(view_, min(rows.front(), rows_.size()), +1)`.
  5. Sparningen och statusraden som förut.
- **`showRow(m)`:**
  1. Utanför `rows_` görs ingenting.
  2. Är `viewPending_` sann körs `rebuildView()`.
  3. Är `isHiddenRow(view_, viewRowOf(view_, m))` sann körs `setFilter(All)`, `host_.filterChanged(All)` och `host_.showStatus` med texten i 7.5.
  4. `v = viewRowOf(view_, m)` och `sheet_->select({v, 0}, {v, 0})`.
- **`showChanged()`:** enligt 5.6.
- **`libs/poleGrid/CMakeLists.txt`:** `kalkyl_poleview` läggs till i PUBLIC.

---

## 7. Texter (ordagrant)

I texterna står `{A}` och `{B}` för `rowLabel` och `{n}` för ett antal. Radbrytning skrivs `char{10}` i koden.

### 7.1 Den grå raden (`hiddenText`, `hiddenToolTip`)
- **Text:** "Dolda {A}–{B} ({n} stolpar som i före)". `{A}` är den första dolda raden och `{B}` den sista. Strecket är U+2013.
- **Tooltip:** "{n} stolpar som är som i Stolptabell före är dolda av filtret Förändrade ±2. Välj Alla i filtret ovanför tabellen, eller högerklicka här och välj Visa alla rader."

### 7.2 Infotexten (`summaryText`)
- `rows == 0 && removed == 0`: tom text.
- `changed == 0 && removed == 0`:
  - med Changed: "Inga förändringar mot Stolptabell före. Alla rader visas."
  - med All: "Inga förändringar mot Stolptabell före."
- Annars: "Förändrade: {changed} av {rows} rader".
  - Med `removed > 0` läggs " · Borttagna: {removed}" till.
  - Med Changed läggs dessutom " · Visas: {shown} av {rows}" till.
  - Exempel: "Förändrade: 2 av 801 rader · Borttagna: 1 · Visas: 19 av 801".

### 7.3 Inklistringen i `PoleGrid` (5.8)
Texterna är desamma som i `sheetGrid`:
- **Statusraden:** "Klistrade in {n} rader", där `{n}` är blockets rader utan en överhoppad rubrikrad. Hoppades en rubrikrad över läggs " (rubrikraden hoppades över)" till.
- **Ingenting landade:** "Inget att klistra in här. Markera en inmatningscell."
- **Ett fält passar inte i sin kolumn:** `planSheetPaste`s egen text i `rejected`, i rutan "Klistra in".

### 7.4 Frågan Radera när markeringen går över en grå rad
- **Med Stolpnr på båda raderna:** "Radera {n} synliga rader mellan stolpnr {A} och {B}? Dolda rader däremellan raderas inte."
  - `{A}` och `{B}` är trimmad Stolpnr på den första och den sista valda raden.
- **Saknas en av dem:** "Radera {n} synliga rader mellan rad {a} och rad {b}? Dolda rader däremellan raderas inte."
  - `{a}` och `{b}` är 1-baserade radnummer i tabellen.

### 7.5 Statusraden och menyerna
- **Snabbmenyn i efter:** "Kopiera från Stolptabell före".
- **Den grå radens meny:** "Visa alla rader".
- **Efter Visa alla rader:** "Filtret är nu Alla."
- **Efter `showRow` på en dold rad:** "Filtret är nu Alla, så att {A} syns."
- **Efter återställningen:** "{Kolumn} för {A} är nu som i Stolptabell före: {värde}."
  - `{Kolumn}` är `poletable::columnName(c)` och `{A}` är `rowLabel`.
  - `{värde}` är `poletable::displayText(före-raden, c)`, eller "tom".
  - Exempel: "Ah för STO450 är nu som i Stolptabell före: 300."
- **Visa förändrade utan förändringar:** "Stolptabell efter har inga förändringar mot Stolptabell före."
- **Fel vid sparning av valet:** "Filtret kunde inte sparas: " följt av `error().message`.

### 7.6 Fönstret
- **`efterFilterLbl`:** "Filter".
- **`efterFilterDrop`:**
  - Posterna: "Förändrade ±2" och "Alla".
  - Tooltip: "Förändrade ±2 visar de stolpar som är ändrade, nya eller borttagna jämfört med Stolptabell före och två rader runt dem. De andra står ihop i grå rader. Alla visar alla stolpar. Valet sparas."
- **`efterVisaBtn`:**
  - Text: "Visa förändrade".
  - Tooltip: "Rullar tabellen till de förändrade stolparna och lägger området mitt i tabellen. Ryms det inte står den första förändrade stolpen överst."
- **`efterInfo`, ny text:** "Kopiera Stolptabell före hit och ändra sedan de stolpar som byggs om. Raderna jämförs med Stolptabell före på Stolpnr: färgen visar om raden är som i före, ändrad eller ny, och tooltipen på en ändrad cell visar värdet i före. Filtret Förändrade ±2 visar bara de förändrade stolparna och två rader runt dem, och resten står ihop i grå rader. Redigering, inklistring och papperskorgen fungerar som i Stolptabell före."

---

## 8. Fönstret

Regionen GlazeBuilder ändras för hand i grammatikens form. `.glazeui` hålls fri från logik, och designraden står kvar.

### 8.1 `main_window.h`
- **`#include "poleView/pole_view.h"`.**
- **Regionen**, efter `glaze::Label efterNyText;`: `glaze::Label efterFilterLbl;`, `glaze::Dropdown efterFilterDrop;`, `glaze::Button efterVisaBtn;` och `glaze::Label efterFilterInfo;`.
- **Handlers**, efter `efterSomForeBtnClicked`:
  ```cpp
      // Filtret i Stolptabell efter: Förändrade ±2 eller Alla. Valet sparas i systemdatabasen, och
      // Förändrade ±2 rullar till de förändrade stolparna.
      void efterFilterDropChanged(glaze::Widget* sender, glaze::ChangeArgs& e);

      // Visa förändrade: rullar Stolptabell efter till de förändrade stolparna (PoleGrid::showChanged).
      void efterVisaBtnClicked(glaze::Widget* sender, glaze::ClickArgs& e);
  ```
- **`PoleHost`:** `void filterChanged(kalkyl::poleview::Filter filter) override;`.
- **Hjälpfunktioner**, efter `updatePoleLooks`:
  ```cpp
      // Sätter filtret i Stolptabell efter: rullistan, PoleGrid och, med save, systemdatabasen. center
      // rullar till de förändrade stolparna. Ett fel vid sparningen visas i statusraden.
      void applyAfterFilter(kalkyl::poleview::Filter filter, bool save, bool center);

      // Infotexten bredvid Visa förändrade och om knappen är tänd (poleview::summarize).
      void updateAfterFilterInfo();
  ```
- **Medlem**, efter `autoAfter_`: `kalkyl::poleview::Filter afterFilter_ = kalkyl::poleview::Filter::Changed;` med kommentaren `// filtret i Stolptabell efter`.

### 8.2 `main_window.glazeui`
- **Förklaringens sex etiketter** (`efterSammaRuta` … `efterNyText`) får var sin rad `.setBuddy("efterFilterDrop");`. Då får de rullistans höjd och står mitt för den.
- **Efter `efterNyText`**, före `efterGrid`:
  ```cpp
      efterFilterLbl.name = "efterFilterLbl";
      efterFilterLbl.setText("Filter");
      efterFilterLbl.setRole(glaze::Label::Role::Dim);
      efterFilterLbl.setBuddy("efterFilterDrop");
      efterFilterLbl.anchorLeft = {"efterNyText", 24};
      efterFilterLbl.anchorTop = {"efterAutoGrp", 8};

      efterFilterDrop.name = "efterFilterDrop";
      efterFilterDrop.setItems({"Förändrade ±2", "Alla"});
      efterFilterDrop.setSelectedIndex(0);
      efterFilterDrop.setToolTip("<texten i 7.6>");
      efterFilterDrop.width = 150;
      efterFilterDrop.anchorLeft = {"efterFilterLbl", 8};
      efterFilterDrop.anchorTop = {"efterAutoGrp", 8};

      efterVisaBtn.name = "efterVisaBtn";
      efterVisaBtn.setText("Visa förändrade");
      efterVisaBtn.setImg(glaze::stockIcon(glaze::StockIcon::GoTo));
      efterVisaBtn.setToolTip("<texten i 7.6>");
      efterVisaBtn.anchorLeft = {"efterFilterDrop", 8};
      efterVisaBtn.anchorTop = {"efterAutoGrp", 8};

      efterFilterInfo.name = "efterFilterInfo";
      efterFilterInfo.setRole(glaze::Label::Role::Dim);
      efterFilterInfo.setBuddy("efterFilterDrop");
      efterFilterInfo.setElide(true);
      efterFilterInfo.anchors = {true, true, true, false};
      efterFilterInfo.anchorLeft = {"efterVisaBtn", 12};
      efterFilterInfo.anchorTop = {"efterAutoGrp", 8};
      efterFilterInfo.right = 16;
  ```
  Raderna "<texten i 7.6>" skrivs med texten ur 7.6.
- **`efterGrid.anchorTop`** blir `{"efterFilterDrop", 8}`.
  - Med glaze-render kontrolleras att knappen inte går längre ner än rullistan.
  - Gör den det blir ankaret `{"efterVisaBtn", 8}`, och det står i rapporten.
- **`efterInfo.setText`** får den nya texten i 7.6.
- **Händelserna**, efter `efterGrid.onContextMenu`:
  ```cpp
      efterFilterDrop.onChange = [this](glaze::Widget* s, glaze::ChangeArgs& e) { efterFilterDropChanged(s, e); };
      efterVisaBtn.onClick = [this](glaze::Widget* s, glaze::ClickArgs& e) { efterVisaBtnClicked(s, e); };
  ```
- **Trädet:** `efterPanel.adopt(efterFilterLbl);`, `adopt(efterFilterDrop)`, `adopt(efterVisaBtn)` och `adopt(efterFilterInfo)` direkt efter `efterPanel.adopt(efterNyText);` och före `efterPanel.adopt(efterGrid);`.
- **Raderna med `setColumns` (kolumnrubrikerna)** rörs inte. Ingen `Edit` får ha dem i `old_string`.

### 8.3 `main_window.cpp`
- **Anonym namnrymd:** `kalkyl::poleview::Filter viewFilterOf(kalkyl::systemdb::AfterFilter)` och `kalkyl::systemdb::AfterFilter sysFilterOf(kalkyl::poleview::Filter)`. Rullistans post 0 är Changed och post 1 är All.
- **`onLoad`:** direkt efter att `afterGrid_` skapats, och före `loadPoleTables()`:
  ```cpp
  afterGrid_->setComparesWithBefore(true);
  afterFilter_ = viewFilterOf(sys_->afterFilter());
  efterFilterDrop.setSelectedIndex(afterFilter_ == kalkyl::poleview::Filter::Changed ? 0 : 1);
  afterGrid_->setFilter(afterFilter_);
  ```
  Rullistan och knappen släcks inte skrivskyddat.
- **`updatePoleLooks()`** kör `updateAfterFilterInfo()` sist.
- **`updateAfterFilterInfo()`:**
  1. `s = poleview::summarize(beforeGrid_->rows(), afterGrid_->rows(), afterFilter_)`.
  2. `efterFilterInfo.setText(poleview::summaryText(s, afterFilter_))`.
  3. `s.marked > 0` ger `efterVisaBtn.enable()`, annars `disable()`.
- **`efterFilterDropChanged`:**
  - `f` = All för post 1, annars Changed.
  - Är `f == afterFilter_` görs ingenting.
  - Annars `applyAfterFilter(f, true, f == Changed)`.
- **`applyAfterFilter(f, save, center)`:**
  1. `commitEdits()`, `afterFilter_ = f` och rullistans post.
  2. `afterGrid_->setFilter(f)`.
  3. Med `save`: `sys_->setAfterFilter(sysFilterOf(f))`. Vid fel `gfx::log` och statusraden i 7.5.
  4. Med `center`: `afterGrid_->showChanged()`.
  5. `updateAfterFilterInfo()`.
- **`PoleHost::filterChanged(f)`:** `window_.applyAfterFilter(f, true, false)`.
- **`efterVisaBtnClicked`:** `commitEdits()`. Svarar `afterGrid_->showChanged()` false visas statusraden i 7.5.
- **`showPoleTab`, `copyBeforeToAfterAsked` och `importPolesAsked`** ändras inte. `showRow` och `setLooks` sköter filtret.

---

## 9. Persistens och migrering
- **Projektfilen ändras inte.** Ingen fil i `libs/projectdb` rörs, och inget steg 7 behövs. Steg 5 och 6 är committade i 66b338e.
- **Systemdatabasen** får nyckeln `pole_after_filter` i tabellen `setting`. Det är ingen schemaändring, och `kExpectedTables` är oförändrad.
  - En `system.db` från 0.5.0 saknar nyckeln, och då gäller Förändrade ±2.
  - Det behövs inget migreringstest. `testAfterFilter` provar förvalet.
- **Väljer användaren F1-alternativet (per projekt)** skrivs ett nytt avsnitt med steg 7 och migreringstestet in i planen före implementeringen.

---

## 10. Tester (ctest i `tests/`)

### 10.1 `tests/poleview_test.cpp` (ny, ctest `poleview`)
**Provtabellen** `stoRows()`: 801 rader STO100–STO900 (`PoleRow{"STO" + to_string(100 + i), "300", "300", "", ...}`).

**`testBuildView`:**
| Märkta rader (count 801) | Vyn |
|---|---|
| {350} | `[Hidden 0–347, Pole 348–352, Hidden 353–800, Blank 801]` |
| {350, 360} | `[Hidden 0–347, Pole 348–362, Hidden 363–800, Blank]` |
| {350, 356} | 353 visas som Pole: `[Hidden 0–347, Pole 348–358, Hidden 359–800, Blank]` |
| {350, 357} | `[Hidden 0–347, Pole 348–359, Hidden 360–800, Blank]` |
| {20, 780} | `[Hidden 0–17, Pole 18–782, Hidden 783–800, Blank]` (allt emellan syns, F8) |
| {0} | `[Pole 0–2, Hidden 3–800, Blank]` |
| {3} | rad 0 ensam och visas: `[Pole 0–5, Hidden 6–800, Blank]` |
| {800} | `[Hidden 0–797, Pole 798–800, Blank]` |

- Inga märkta rader ger `identityView(801)`, och filtret All ger också identiteten.
- `count` 0 ger `[Blank 0]`.
- `count` 3 med {1} ger tre Pole och Blank.
- Poster bortom `count` ignoreras.

**`testFilterMarks`** (bilaga A med tio rader ST0100–ST0109):
- En ändrad spannlängd, en ny Stolpnr, en tom rad och en osäker rad (dubblett i före) är märkta. Same är inte märkt.

**`testRemovedAnchors`:**
| Efter | Ankarrader | `removedCount` |
|---|---|---|
| utan ST0104 | {3, 4} | 1 |
| utan ST0100 | {0} | |
| utan ST0109 | {8} | |
| utan ST0104 och ST0105 | ST0103 och ST0106 i efter | |
| tom | {} | 10 |

- En Stolpnr som finns två gånger i efter hoppas över, och sökningen går vidare.
- En tom Stolpnr i före räknas aldrig som borttagen.

**`testIndex`**, på vyn `V` för {350}:
- `modelRow`: `(V, 0)` ger -1, `(V, 1)` 348, `(V, 5)` 352, `(V, 6)` -1, `(V, 7)` 801, `(V, 9)` 803 och `(V, -1)` -1.
- `viewRowOf`: rad 0 och 347 ger 0, 348 ger 1, 400 ger 6, och 801 och 805 ger 7.
- `visibleViewRow`:
  - `(V, 100, +1)` och `(V, 100, -1)` ger 1, eftersom den grå raden står först.
  - `(V, 400, -1)` ger 5 och `(V, 400, +1)` ger 7.
  - `(V, 350, ±1)` ger 3.
- `isHiddenRow` och `rowCount(V) == 801`.

**`testResizeView`**, från V:
| `count` | Vyn |
|---|---|
| 803 | `[…, Hidden 353–800, Pole 801, Pole 802, Blank 803]` |
| 354 | Hidden 353 blir Pole 353 |
| 355 | Hidden 353–354 |
| 200 | `[Hidden 0–199, Blank 200]` |
| 1 | `[Pole 0, Blank 1]` |
| 0 | `[Blank 0]` |

**`testTexts`:**
- `hiddenText` för Hidden 0–347 är "Dolda STO100–STO447 (348 stolpar som i före)".
- En tom Stolpnr ger "rad 5".
- `hiddenToolTip` ordagrant.
- `rowLabel`.

**`testScrollTop`:**
| first, last | viewRows | rowHeight | viewport | Svar |
|---|---|---|---|---|
| 1, 5 | 8 | 20 | 100 | 20 |
| 3, 4 | 20 | 20 | 200 | 0 |
| 15, 16 | 20 | 20 | 200 | 200 |
| 2, 30 | 50 | 20 | 200 | 40 |
| | 5 rader | | 200 | 0 |
| | | rowHeight 0 | | 0 |

**`testSummary`:**
- `summarize` med två ändrade, en borttagen och filtret Changed ger rätt `rows`, `changed`, `marked`, `removed` och `shown`.
- `summaryText` för alla grenarna i 7.2, ordagrant.
- `markedRange`.

### 10.2 `tests/automode_test.cpp`, `testRestoreFromBefore` (ny)
Efter är bilaga A, med ändringarna i tabellen:

| Rad i efter | Ändring |
|---|---|
| 2 | `spanM = "999"` och `height = "45.00"` (lika som tal) |
| 4 | `av = "301"` |
| 0 | `spanM = "0"` (före har tom) |
| 8 | `pole = "ST0200"` (ny) |

| `row`, `column` | Svar |
|---|---|
| 2, kSpan | `{2, kSpan, "320"}` |
| 2, kHeight | tomt (lika som tal) |
| 4, kAv | `{4, kAv, "300"}` |
| 4, kSpan | tomt (kolumnen skiljer sig inte) |
| 0, kSpan | `{0, kSpan, ""}` |
| 8, kSpan | tomt (New) |
| 1, kAh | tomt (Same) |
| 2, -1 och 2, 8 | tomt (kolumn utanför 0-7) |
| 99, kSpan | tomt (rad utanför compare) |

- En handbyggd `RowCompare` med `state = Changed`, `beforeRow = 0` och `changed[kPole] = true` ger tomt svar för `0, kPole`.
- En Ambiguous-rad ger tomt svar.

### 10.3 `tests/sheetgrid_test.cpp`, `testHiddenRows` (ny)
**Formen:** sex rader och tre kolumner, `isInput` alla, `accept` identitet, `growRows = true`, `headerTakesRow = false` och `hiddenRow = (row == 2)`.

| Ankare | Block | Väntat |
|---|---|---|
| {0, 0} | 2 rader | ändringar i rad 0–1 |
| {0, 0} | 3 rader | ändringar i rad 0–1 och ingen i rad 2, `rejected` tom |
| {1, 0} | ett enda värde | ändring i rad 1 |
| {3, 0} | 5 rader | ändringar i rad 3–7, `rowsNeeded` 8 |
| {1, 0} | rubrikrad och två datarader | data i rad 1–2: ändring bara i rad 1, `rejected` tom |

- `clampPos({2, 0}, shape, 1)` ger rad 3, och `clampPos({2, 0}, shape, -1)` ger rad 1.
- En form med både `skipRow = (row == 1)` och `hiddenRow = (row == 3)` och ett block på fyra rader vid {0, 0} ger ändringar bara i rad 0 och 2.
- `SheetHost::pasteBlock`: en testvärd som bara implementerar `copyText`, `baseColor`, `applyEdits` och `showStatus` svarar false.
- De befintliga testerna går igenom oförändrade.

### 10.4 `tests/systemdb_test.cpp`, `testAfterFilter` (ny)
- Förvalet är Changed.
- `setAfterFilter(All)` ger All, också efter omöppning.
- Skräpvärdet `'x'` i `setting` ger Changed.
- `openInMemory` ger Changed, och sparningen fungerar där under sessionen.

### 10.5 Det som kräver fönster
- `PoleGrid` i fönstret, huvudfönstret och rullningen provas med glaze-render, röktest och handtest.
- `tests/polegrid_test.cpp` (`cellLook`) går igenom oförändrat.

`tests/CMakeLists.txt` får raden direkt efter `automode`:
```
kalkyl_add_test(poleview poleview_test.cpp kalkyl_poleview kalkyl_automode kalkyl_poletable)
```

---

## 11. Arbetspaket

Fyra Sonnet-agenter (high) arbetar samtidigt. Varje agent äger sina filer, och ingen fil delas mellan paketen.

### Gemensamt för alla paket
- **Byggkatalogen** ligger i repots rot och konfigureras med MSYS2:s cmake (`C:/msys64/ucrt64/bin/cmake.exe`):
  ```sh
  cmake -S . -B build-<paket> -G Ninja -DCMAKE_BUILD_TYPE=Debug -DCMAKE_C_COMPILER=C:/msys64/ucrt64/bin/gcc.exe -DCMAKE_CXX_COMPILER=C:/msys64/ucrt64/bin/g++.exe -DKALKYL_TESTS=ON -DFETCHCONTENT_SOURCE_DIR_GLAZEUI=C:/Users/sehoer/Source/Kalkyl/build/_deps/glazeui-src
  ```
- **Städning:** `build-<paket>` tas alltid bort när agenten är klar (`rm -rf build-<paket>`).
  - `build/` rörs aldrig: den byggs inte, rensas inte och konfigureras inte om.
- **Fel i ett annat pakets fil:** faller bygget i en fil som ett annat paket äger väntar agenten och försöker igen. Den ändrar aldrig filen.
- **Steg 0:** inom några minuter skriver varje paket sina kontraktsfiler ordagrant ur avsnitt 6, med kroppar som kompilerar. Ordningen:
  1. paket 1: `pole_view.h` och dess `CMakeLists.txt`, och `restoreFromBefore`,
  2. paket 2: `hiddenRow`, `SheetHost::pasteBlock` och `AfterFilter`,
  3. paket 3: `pole_grid.h`,
  4. paket 4: regionen, `.glazeui` och tomma handlers.

### Paket 1: poleView och autoMode (`build-vy`)
- **Äger:**
  - `libs/poleView/` (ny: `CMakeLists.txt`, `pole_view.h`, `pole_view.cpp`, `CLAUDE.md`),
  - `libs/CMakeLists.txt` och `libs/CLAUDE.md`,
  - `libs/autoMode/auto_mode.h`, `auto_mode.cpp` och `CLAUDE.md`,
  - `tests/poleview_test.cpp` (ny), `tests/automode_test.cpp` och `tests/CMakeLists.txt`.
- **Arbete:** 5.2, 6.1, 6.2, 7.1, 7.2, 10.1 och 10.2.
- **Bygg och kör:** `ctest -R "poleview|automode"`.
- **CLAUDE.md:**
  - `libs/poleView/CLAUDE.md`: filerna, reglerna i 5.2 och vem som använder biblioteket.
  - `libs/CLAUDE.md`: raden `poleView/` och beroendet "`poleView` beror på `poleTable`, `autoMode` och `tableText`; `poleGrid` beror också på `poleView`".
  - `libs/autoMode/CLAUDE.md`: `restoreFromBefore`.
  - Rapporten innehåller texten till `tests/CLAUDE.md` (raden för `poleview` och tillägget för `automode`).

### Paket 2: sheetGrid och systemdb (`build-sys`)
- **Äger:**
  - `libs/sheetGrid/sheet_grid.h`, `sheet_grid.cpp` och `CLAUDE.md`, och `tests/sheetgrid_test.cpp`,
  - `libs/systemdb/system_db.h`, `system_db.cpp` och `CLAUDE.md`, och `tests/systemdb_test.cpp`.
- **Arbete:** 6.3, 6.4, 10.3 och 10.4.
- **Bygg och kör:** `ctest -R "sheetgrid|systemdb"`.
- **CLAUDE.md:**
  - `libs/sheetGrid/CLAUDE.md`: `hiddenRow` och skillnaden mot `skipRow`, och `SheetHost::pasteBlock`.
  - `libs/systemdb/CLAUDE.md`: nyckeln `pole_after_filter`.
  - Rapporten innehåller texten till `tests/CLAUDE.md`.

### Paket 3: poleGrid (`build-grid`)
- **Äger:** `libs/poleGrid/pole_grid.h`, `pole_grid.cpp`, `CMakeLists.txt` och `CLAUDE.md`, och `tests/polegrid_test.cpp` (bara om ett test behöver ändras).
- **Arbete:** 5.4–5.8, 6.5 och 7.3–7.5.
  - Varje väg i tabellen 5.4 gås igenom.
  - Med `setComparesWithBefore(false)` ska Stolptabell före bete sig som i 0.5.0.
- **Bygg och kör:** `ctest -R polegrid`, och `kalkyl` när paket 4 har sina stubbar.
- **CLAUDE.md:** `libs/poleGrid/CLAUDE.md`. Den beskriver:
  - vyn och mappningen mellan vy och modell,
  - de grå raderna,
  - ombyggnaden och dess regler (K11),
  - `setComparesWithBefore`, `setFilter`, `showChanged`, `filterChanged` och Kopiera från Stolptabell före för den högerklickade cellen (`GridProbe`),
  - inklistringen mot modellen (`pasteIntoModel`).

### Paket 4: huvudfönstret (`build-main`)
- **Äger:** `src/windows/main_window.h`, `main_window.glazeui`, `main_window.cpp` och `src/windows/CLAUDE.md`, och `src/CMakeLists.txt` (`kalkyl_poleview` efter `kalkyl_automode`) och `src/CLAUDE.md`.
- **Arbete:** 7.6 och avsnitt 8.
- **Förhandsgranskning:** konfigurera med `-DGLAZE_TOOLS=ON` och bygg `--target glaze-render`, sedan
  ```sh
  build-main/_deps/glazeui-build/apps/glaze-render/glaze-render.exe src/windows/main_window.h -o <scratchpad>/main_window.png
  ```
  - Exit 0 krävs, och bilden granskas med Read.
  - Fliken Stolptabell efter ska ha förklaringen, filtret, knappen och infotexten på en rad ovanför tabellen.
- **Röktest:** `KALKYL_DATA_DIR=build-main/smoke timeout -k 2 3 ./build-main/src/kalkyl.exe; echo $?` ger 124 eller 137.
- **CLAUDE.md:** raden för `main_window.*` i `src/windows/CLAUDE.md`: filtret, knappen, infotexten, `applyAfterFilter`, `updateAfterFilterInfo` och `afterFilter_`. Dessutom `src/CLAUDE.md` (länklistan).

### Regler för alla implementeringsagenter
- **Kommentarer:**
  - Varje funktion, klass och logisk del har en kommentar på 2–5 rader.
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
- **Hooken `~/.claude/hooks/only-c-drive.sh`** granskar varje verktygsanrop, också filinnehållet. Den stoppar:
  - två bakvända snedstreck i rad,
  - ett bakvänt snedstreck och en bokstav efter skiljetecken eller blanksteg och före citattecken eller slut,
  - andra enhetsbokstäver än C följda av kolon. Ett `old_string` eller `new_string` får aldrig sluta med en ensam bokstav och ett kolon.

  **Därför:**
  - råa strängliteraler `R"sql( … )sql"` och `R"( … )"` för SQL och reguljära uttryck,
  - teckenklasser som `[.]`,
  - `char{10}`, `char{9}` och `char{34}`,
  - snedstreck framåt i sökvägar,
  - bracket-argument och `string(ASCII 10 NL)` i CMake.

  Raderna med kolumnrubrikerna i `main_window.glazeui` (`setColumns`) tas aldrig med i en Edit.
- **Varningar:** bygget är fritt från varningar (`-Wall -Wextra`).
- **Versionen (0.6.0)** rörs inte.
- **Inga commits.**

---

## 12. Integration (samordnaren)
1. **Bygget utan varningar** i `build/`: `cmake --build --preset debug 2>&1 | grep -i warning` ger ingen utdata.
2. **Testerna** i en tillfällig `build-verify`, konfigurerad som i 11 och med `-DGLAZE_TOOLS=ON`:
   ```sh
   cmake --build build-verify && ctest --test-dir build-verify --output-on-failure
   ```
   Alla tester går igenom, också de nya `poleview`, `testRestoreFromBefore`, `testHiddenRows` och `testAfterFilter`.
3. **glaze-render** ger exit 0 på `main_window.h`, och raden ovanför `efterGrid` är rätt.
4. **Kontroller:**
   - `git diff --stat libs/projectdb` är tom.
   - `grep -n "removeRowsAsked(int" libs/poleGrid` ger ingen träff.
   - Versionen är 0.6.0.
5. **Röktest** med provprojektet `build-verify/prov/Prov åäö.kalkyl`: filtret och knappen syns, och efter har en ändrad rad med grå rader runt.
6. **Städning:** inga `build-*` utom `build/` finns kvar.
7. **CLAUDE.md och `TODO.md`** enligt avsnitt 14. Användaren får handtestet i avsnitt 13.

---

## 13. Handtest (användaren)

**Provdata:** 801 stolpar ur Excel.
- Skriv i A1 `="STO"&(ROW()+99)`, i B1 `=6600000-ROW()*300`, i C1 `=400000` och i D1 `=50`, och fyll ner till rad 801. Det ger STO100–STO900 med 300 m mellan stolparna.
- Kopiera A1:D801 och klistra in på den tomma raden i Stolptabell före. Avstånd föregående räknas då ur koordinaterna.
- Välj "Kopiera från Stolptabell före…" i Stolptabell efter.

**Filtret och vyn**
1. **Direkt efter kopieringen:**
   - Filtret är Förändrade ±2, och alla 801 rader syns.
   - Infotexten är "Inga förändringar mot Stolptabell före. Alla rader visas.".
   - Visa förändrade är släckt.
2. **Ändra Ah på STO450 och tryck Enter.**
   - Tabellen blir "Dolda STO100–STO447 (348 stolpar som i före)", STO448–STO452 (STO450 rosa med starkare rosa cell), "Dolda STO453–STO900 (448 stolpar som i före)" och den tomma raden.
   - Den aktuella cellen står på STO451 på samma höjd som förut.
   - Infotexten är "Förändrade: 1 av 801 rader · Visas: 5 av 801".
3. **Ändra också STO460.** STO448–STO462 syns, och de grå raderna är STO100–STO447 och STO463–STO900.
4. **Ensam dold rad:** ändra bara STO103. STO100 visas fast den ligger utanför ±2, eftersom en ensam dold rad visas. Det blir ingen grå rad överst.
5. **Ändra STO450 i två steg med Tab.** Den aktuella cellen följer med, och ingen redigering avbryts.
6. **Den grå raden:**
   - Tooltipen förklarar.
   - Klick och dubbelklick ändrar ingenting.
   - Pilarna kan ställa cellen på den, men F2 och skrivning gör ingenting.
   - Högerklick visar bara "Visa alla rader". Den sätter filtret till Alla, rullistan visar Alla och statusraden säger "Filtret är nu Alla.".
7. **Alla:**
   - Alla rader syns, och den aktuella raden står kvar på samma höjd.
   - Stäng Kalkyl och öppna igen. Filtret är fortfarande Alla.
8. **Tillbaka till Förändrade ±2:** tabellen fälls ihop och rullar så att STO448–STO462 står mitt i tabellen.

**Visa förändrade**

9. **Med Alla och ändringar på STO450 och STO460:** området STO450–STO460 hamnar mitt i tabellen, och STO450 är aktuell.
10. **Med Alla och ändringar på STO120 och STO880:** området ryms inte, och STO120 står överst.
11. **Med Förändrade ±2 och samma ändringar:** STO118–STO882 syns mellan två grå rader. Området ryms inte, och STO120 står överst.
12. **Utan ändringar** är knappen släckt.

**Markering, urklipp och redigering med grå rader** (ändringar på STO450 och STO460, så att STO463–STO900 är en grå rad under det synliga området)

13. **Markera från STO458 till den tomma raden sist med Shift+klick.** Den grå raden blir inte blå. Ctrl+C och klistra in i Excel ger STO458–STO462 (5 rader) utan den grå.
14. **Klistra in ett block på 10 rader ur Excel på STO461**, med andra Ah-värden än i före.
    - Ingen ruta visas. Blocket skrivs i STO461–STO470 i följd, också i de dolda STO463–STO470.
    - Raderna blir rosa och syns, och den grå raden blir "Dolda STO473–STO900 (428 stolpar som i före)".
    - Markeringen är de inklistrade raderna.
    - Med Alla syns att STO461–STO470 har blockets värden i följd och att STO471 är orörd.
15. **Klistra in tre rader på den tomma raden sist.** De blir gröna, och STO899–STO900 syns ovanför dem.
16. **Delete på markeringen i punkt 13:**
    - Bara de synliga cellerna töms.
    - Med Alla är STO463–STO900 orörda.
    - Ångra genom att stänga utan att spara.
17. **Radera rader… på markeringen i punkt 13:**
    - Frågan gäller STO458–STO462, med dagens text.
    - Ja raderar bara dem. De dolda raderna under den grå raden raderas inte.
    - Platserna syns med raderna runt, och infotexten säger "Borttagna: 5".
18. **Papperskorgen på STO450:** raden raderas, STO448–STO452 utom STO450 står kvar synliga, och infotexten räknar en borttagen.
19. **Infoga rad ovanför STO451:** en tom rad kommer och står kvar synlig. Skriv en Stolpnr, och raden blir grön.

**Kopiera från Stolptabell före**

20. **Högerklicka direkt på den ändrade Ah-cellen på STO450**, utan att klicka på den först.
    - Cellen blir markerad, och "Kopiera från Stolptabell före" är tänd.
    - Ett klick ger värdet i före, och raden blir gul.
    - Statusraden säger "Ah för STO450 är nu som i Stolptabell före: tom.", eftersom provdatan inte har någon Ah.
    - Utan andra ändringar visas alla rader igen, och den aktuella raden står kvar.
21. **Bara den högerklickade cellen återställs:**
    - Ändra Ah och Av på STO450 och högerklicka på Av. Bara Av återställs, och Ah är fortfarande starkare rosa.
    - Markera STO448–STO452 i alla kolumner och högerklicka på Ah på STO450 inne i markeringen. Markeringen står kvar, och bara Ah återställs.
22. **Alternativet är släckt:**
    - på en cell som är lika i en rosa rad,
    - på en gul rad,
    - på en grön rad (ny eller bytt Stolpnr),
    - på Stolpnr,
    - på den tomma raden och i radera-kolumnen,
    - i ett skrivskyddat projekt.

    På en grå rad visar högerklicket bara "Visa alla rader".
23. **Stolptabell före:** snabbmenyn har inget "Kopiera från Stolptabell före". Där finns varken filter eller grå rader, och allt fungerar som i 0.5.0.
24. **Automatiska beräkningar:** med automatiskt läge på i efter och faslinan STO440–STO460 ändras normalspannet i Dokumentation efter en återställning.

**Automatiskt läge och Gå till**

25. **Välj faslinan STO440–STO460 i efter.** Rullistorna visar alla stolpar, och flaggorna syns på de synliga raderna.
26. **"Gå till Stolptabell efter" i Dokumentation när STO440 är dold:**
    - Filtret blir Alla och sparas.
    - STO440 blir aktuell.
    - Statusraden säger "Filtret är nu Alla, så att STO440 syns.".

**Övrigt**

27. **Skrivskyddat:** filtret, Visa förändrade och Visa alla rader fungerar. Redigering och återställning är släckta.
28. **Ctrl+S, stäng och öppna igen:** raderna är kvar, och filtret är det senast valda.
29. **"Kopiera från Stolptabell före…" (knappen):** efter blir lik före, och alla rader syns igen.
30. **Importera stolptabell… till före:** jämförelsen och vyn i efter uppdateras direkt.
31. **Prestanda:** med 801 rader ska en ändring och ett filterbyte kännas direkt.
32. **Mörkt och ljust läge:** de grå raderna och infotexten är läsbara i båda.

---

## 14. CLAUDE.md och TODO.md
| Fil | Vem | Ändring |
|---|---|---|
| Rotens `CLAUDE.md` | samordnaren | Se nedan |
| `libs/CLAUDE.md`, `libs/poleView/CLAUDE.md`, `libs/autoMode/CLAUDE.md` | paket 1 | Raden `poleView/`, beroendena och `restoreFromBefore` |
| `libs/sheetGrid/CLAUDE.md`, `libs/systemdb/CLAUDE.md` | paket 2 | `hiddenRow`, `SheetHost::pasteBlock` och `pole_after_filter` |
| `libs/poleGrid/CLAUDE.md` | paket 3 | Vyn, de grå raderna, ombyggnaden och det nya API:t |
| `src/windows/CLAUDE.md`, `src/CLAUDE.md` | paket 4 | `main_window.*` och länklistan |
| `tests/CLAUDE.md` | samordnaren | Raden för `poleview` och tilläggen för `automode`, `sheetgrid` och `systemdb`, med paketens text |
| `doc/plans/CLAUDE.md` | samordnaren | Raden för den här planen |
| `TODO.md` | samordnaren | ⬜ när planen är godkänd och 🔨 under implementeringen, och idén i risk 12 |

**Rotens `CLAUDE.md`:**
- Huvudfönstrets rad: "Stolptabell efter (kopia av före, färger mot före matchade på Stolpnr, filtret Förändrade ±2/Alla med grå rader för dolda stolpar, Visa förändrade och Kopiera från Stolptabell före per cell i snabbmenyn)".
- En ny rad för `libs/poleView`: "Vyn av Stolptabell efter: filtret Förändrade ±2, de grå raderna, rullningsmålet och infotexten som rena funktioner".
- Systemdatabasens rad och avsnittet Data: "… och filtret i Stolptabell efter".

---

## 15. Risker och luckor
1. **Radhöjden** tas ur `AppWindow::style().gridRowH`, eftersom DataGrid inte har `setIconSize`. Sätts en ikonstorlek senare blir rullningen fel. Det står i `libs/poleGrid/CLAUDE.md`.
2. **Vyns höjd mäts** med `setScroll` och `ensureVisible` på sista raden. Det kräver ett öppet fönster, och fönstret är öppet vid ett knapptryck och ett filterbyte.
3. **Grå rader är dataceller.** Pilarna och Enter kan ställa den aktuella cellen på dem, men ingenting går att ändra där. `RowKind::Category` och `Blank` duger inte (avsnitt 3).
4. **Smalt fönster:** texten i den grå raden och infotexten kortas med "…". Tooltipen har hela texten.
5. **Efter en sparad cell** postas ombyggnaden. Under en bild kan den gamla vyn synas.
6. **Fel och flaggor på dolda rader syns inte**, till exempel "Spannlängd saknas." på en rad som är som i före. Panelen Automatiskt läge visar felet.
7. **Ingen ångra.** Återställningen och raderingen sparas direkt. Det som senast publicerades går att få tillbaka genom att stänga utan att spara.
8. **Valet per användare:** två användare ser olika vyer av samma projekt (F1).
9. **`sheetGrid` ändras.**
   - Linberäkningen använder varken `hiddenRow` eller `pasteBlock`.
   - Stolptabell före har aldrig grå rader, så `pasteIntoModel` svarar false där.
   - De befintliga testerna täcker båda.
10. **`libs/projectdb/CLAUDE.md`** säger att steg 5 och 6 inte är committade, men de är committade i 66b338e. Planen rör inte filen. Samordnaren kan rätta meningen separat.
11. **Kopiering och inklistring är inte symmetriska** (K13, K14), precis som i Excel.
    - En markering över en grå rad kopieras utan de dolda raderna.
    - Inklistringen skriver i följd, också i dolda rader.
    - Klistras kopian tillbaka på samma ställe hamnar raderna efter den grå raden förskjutna. Välj Alla före en sådan kopiering.
12. **`GridProbe`** (5.7) når den skyddade `Table::columnAt` genom en pekare till medlem, eftersom v0.11.1 inte har någon publik fråga om kolumnen under en punkt.
    - Det är tillåten C++, men det bygger på en skyddad signatur som kan ändras när GlazeUI uppgraderas.
    - Ny idé i `TODO.md`: "💡 GlazeUI: en publik fråga om cellen (rad och kolumn) under en punkt i DataGrid, så att `GridProbe` i `libs/poleGrid` kan tas bort."
13. **En inklistring över grå rader skriver i data som inte syns.** Det är användarens beslut (F6), och de rader som ändras blir synliga direkt.

---

### Kritiska filer för implementeringen
- C:/Users/sehoer/Source/Kalkyl/libs/poleGrid/pole_grid.cpp
- C:/Users/sehoer/Source/Kalkyl/libs/poleGrid/pole_grid.h
- C:/Users/sehoer/Source/Kalkyl/libs/poleView/pole_view.h (ny)
- C:/Users/sehoer/Source/Kalkyl/libs/sheetGrid/sheet_grid.cpp
- C:/Users/sehoer/Source/Kalkyl/src/windows/main_window.cpp

---

## 16. Efter handtestet och granskningen

- **Av per fas i Stolptabell efter** (användarens önskemål, utan egen plan): Av blir Av Fas1, Av Fas2 och Av Fas3 före LKL.
  - `poletable` har `kAv2`, `kAv3`, `kFieldCount`, `beforeField`, `beforeColumns`, `afterColumns` och `afterColumnName`.
  - `PoleGrid` får tabellens kolumnordning i konstruktorn och översätter kolumn och fält med `fieldOf` och `columnOf`.
  - `compareAfter` och `restoreFromBefore` jämför och återställer Fas2 och Fas3 mot Av i före.
  - Projektdatabasen fick steg 7: `pole_after.av2` och `av3`, och befintliga rader får Av i alla tre faserna. `copyBeforeToAfter` gör likadant. Testet är `testMigrateFromStep6`.
- **Spannlängd ur koordinaterna** avrundas till två decimaler i `automode::coordinateSpans`, så att normalspannet räknar med det tal som visas.
- **Felet som fick fönstret att utebli** låg i byggkatalogen och inte i koden: fyra objektfiler utan registrerade beroenden byggdes inte om efter en ändring i `SheetShape`.
- **Granskningen** hittade inga fel. Dubbletterna samlades: inklistringens texter och ruta i `sheetGrid` (`pasteStatus`, `nothingToPasteText` och `showPasteRejected`), `afterColumnName`, `beforeText` i `PoleGrid`, `markedRange` i `buildView` och rullistans poster i huvudfönstret. Kommentarer med historik skrevs om.
