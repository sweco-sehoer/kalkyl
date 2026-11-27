# TODO

## Status

| Glyf | Status | Betydelse |
|---|---|---|
| ⬜ | Ej startad | Planerad men inte påbörjad |
| 📝 | Planeras | Planfilen skrivs |
| 🔨 | Utförs | Implementeras |
| 🔍 | Granskas | Implementerad, granskas |
| ⏸️ | Väntar | Blockerad av annan punkt eller beslut |
| ✅ | Klar | Tas bort från listan, planfilen arkiveras |
| 🐞 | Bugg | Fel som ska rättas, ingen planfil |
| 💡 | Idé | Förslag, ingen planfil förrän den blir arbete |

## Arbete

- 🔍 **Installation (NSIS):** implementerad och verifierad (8.1–8.10), granskning och användartest återstår.
  setup.exe per användare i `%LOCALAPPDATA%\Programs\Kalkyl`, utgivare, licens, versionsresurs i kalkyl.exe.
  [Plan](doc/plans/2026-11-20_Installation.md)

## Buggar

- 🐞 Första tysta installationen (`/S`, ren profil) gav exit-kod 2 fast allt installerades. Gick inte att återskapa;
  senare körningar gav 0. Utred före IT-utrullning.

## Idéer

- 💡 Linberäkningens övriga ark ur Linberäkning.xlsm: Isolatorutslag, Lokal islast, Regleringstabell, Nedhäng,
  Slackutslag, lastfallsdiagrammet och utskriftens "Linbrott lastfall ID 5". Se doc/plans/archive/2026-11-25_Linberakning.md, 1.3.
- 💡 Fler Excel-facit för linberäkningen: testfallen T2 och T11–T16 i doc/plans/archive/2026-11-25_Linberakning.md, 6.4
  (D1, korsningsmall med ändrad min/max, manuell last) som tester i `tests/`, som E1–E10.
- 💡 Sammanslagning (Sammanslagning.xlsm) som egen beräkningsmodul.
- 💡 Linhöjd faslina för linberäkning (fliken Linhöjder) som indata till Linberäkningens fält "Linhöjd, snitt (m)".
  Se doc/plans/archive/2026-11-27_Linhojder.md, fråga 6.
- 💡 Fliken Linhöjder: inklistring av flera celler och en snabbmeny med Gå till Stolptabell efter.
- 💡 GlazeUI: ett val för talceller i DataGrid som visar talet med de decimaler som skrevs. Då kan Stolptabell före
  svälja bokstäver redan vid tangenttrycket. Se doc/plans/archive/2026-11-26_Stolptabell_fore.md, 3.2.
- 💡 GlazeUI: en publik fråga om cellen (rad och kolumn) under en punkt i DataGrid, så att `GridProbe` i `libs/poleGrid` kan tas bort.
  Se doc/plans/archive/2026-11-27_Filter_stolptabell_efter.md, risk 12.
- 💡 `postIfAlive` finns i fyra filer (Normalspann, Linberäkning, `libs/sheetGrid` och `libs/poleGrid`). Samla den på ett gemensamt ställe.
- 💡 Normalspannsformuläret använder `libs/sheetGrid` (`growRows`, `headerTakesRow = false`) för markering och urklipp,
  som Linberäkning och Stolptabell före.
- 💡 Områdesmarkering, dragmarkering, urklipp och ångra i GlazeUI:s DataGrid. Kalkyl emulerar i dag markeringen
  i normalspannsformuläret. Se doc/plans/archive/2026-11-23_Normalspann.md, avsnitt 3.11 och 14.
- 💡 Kodsignering av setup.exe, avinstallera.exe och kalkyl.exe med SWECO:s certifikat (SmartScreen, AppLocker).
  Se doc/plans/2026-11-20_Installation.md, avsnitt 10.
- 💡 Nytt och Öppna projekt direkt i huvudfönstret, utan att gå via väljaren.
- 💡 Återställ osparade arbetskopior efter en krasch, när projekten får redigerbara data.
- 💡 En instans per projekt: skicka en dubbelklickad fil till en Kalkyl som redan är öppen.
- 💡 Avbryt-knapp under "Öppnar…" och städning av gamla ~*.tmp i projektmappar.
- 💡 Överge en öppning som hänger på en share som slutat svara (lås, stämpel, kopiering i frikopplad tråd),
  så att väljaren alltid går att stänga direkt. Se doc/plans/archive/2026-11-20_Grunder.md, avsnitt 12.
