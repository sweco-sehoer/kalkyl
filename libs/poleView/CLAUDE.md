# libs/poleView

Vyn av Stolptabell efter som rena funktioner, utan databas och GUI: filtret Förändrade ±2, det ändrade området, de grå raderna för dolda stolpar,
rullningsmålet för Visa förändrade och infotexten.
Biblioteket heter `kalkyl_poleview`, namnrymden `kalkyl::poleview` och includes skrivs `"poleView/pole_view.h"`.
Det beror på `poleTable` (PUBLIC) och `autoMode` och `tableText` (PRIVATE).
`libs/poleGrid` (tabellen i fönstret) och huvudfönstret (rullistan och infotexten) använder samma funktioner, så tabellen och infotexten alltid stämmer.
`libs/lineHeight` (fliken Linhöjder) delar märkningen (`filterMarks`) och området (`changedArea`) men har sin egen marginal, `lineheight::kBlockMargin`.
Planerna är `doc/plans/archive/2026-11-27_Filter_stolptabell_efter.md`, avsnitt 5 och 6.1, och `doc/plans/archive/2026-11-27_Linhojder.md`, avsnitt 8.1 och H1.

| Fil | Innehåll |
|---|---|
| `CMakeLists.txt` | Biblioteket `kalkyl_poleview` (testet `poleview` ligger i `tests/`) |
| `pole_view.h` | `Filter`, `kMargin`, `ViewKind`, `ViewRow`, `PoleView`, `removedAnchors`, `removedCount`, `filterMarks`, `MarkedRange`, `markedRange`, `changedArea`, `identityView`, `buildView`, `resizeView`, `rowCount`, `modelRow`, `viewRowOf`, `visibleViewRow`, `isHiddenRow`, `rowLabel`, `hiddenText`, `hiddenToolTip`, `scrollTop`, `Summary`, `summarize` och `summaryText` |
| `pole_view.cpp` | Implementationen |

## Regler

- **Rena funktioner:** inget tillstånd, ingen I/O, inget fönster.
- **Vyn** är tabellens rader: `Pole` (en rad i modellen), `Hidden` (en grå rad för minst två dolda rader) och `Blank` (den tomma raden sist, alltid med).
  Filtret All, eller ingen märkt rad, ger identiteten.
- **Märkt rad** (`filterMarks`): varje rad i efter som inte är Same mot före (Changed, New, Empty och Ambiguous), och raderna närmast en borttagen stolpe (`removedAnchors`).
- **Det ändrade området** (`changedArea(count, marks, margin)`): från den första märkta raden minus `margin` till den sista plus `margin`, klampat till tabellen, räknat på radernas plats (aldrig på siffrorna i Stolpnr). `{-1, -1}` utan märkt rad.
- **Förändrade ±2** (`buildView`): det synliga är `changedArea` med `kMargin` (2), ett enda område.
  Två rader, så att alla stolpar som spannen i fliken Linhöjder når syns (blocken ±1, spannen ett steg till).
  Allt mellan den första och den sista märkta raden syns, också när de ligger långt isär.
  Raderna före området och raderna efter det blir var sin `Hidden`-rad när de är minst två, och en ensam rad visas som vanligt.
- **Borttagna stolpar** (`removedAnchors`): en rad i före är borttagen när dess trimmade Stolpnr inte är tom och saknas i efter. För varje sådan rad används närmaste rad ovanför och nedanför i före vars Stolpnr finns på exakt en rad i efter, och den raden i efter märks. Två genomgångar, O(n).
- **Index** (`modelRow`, `viewRowOf`, `visibleViewRow`): `Hidden` har ingen modellrad. Raderna under `Blank` ger nya rader (för inklistring), och en grå rad ersätts av nästa synliga rad (eller den före, med steg -1).
- **`resizeView`** behåller formen när antalet rader ändras, så att markeringen i tabellen stämmer tills vyn byggs om.
- **`scrollTop`:** mitten av området mitt i synfältet när det ryms, annars första raden överst, klampat till 0..(rader × radhöjd − synfält).
- **Texterna** (`hiddenText`, `hiddenToolTip`, `summaryText`) är ordagrant de i planen 7.1 och 7.2. Strecket i den grå raden är U+2013.
