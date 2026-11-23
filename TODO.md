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

- 💡 Spara flikarna Dokumentation och Stolptabell före i projektdatabasen. I dag är värdena mockupens förval
  i `main_window.cpp` och gäller bara medan projektet är öppet.
- 💡 Spara fas- och topplinan i projektet som en kopia av linans data (inte id), med "Uppdatera från linregistret".
  Se doc/plans/archive/2026-11-23_Lindata.md, avsnitt 2.10.
- 💡 Flytta `Conductor` ur systemdb till ett eget bibliotek när projectdb eller beräkningarna behöver den.
- 💡 Kodsignering av setup.exe, avinstallera.exe och kalkyl.exe med SWECO:s certifikat (SmartScreen, AppLocker).
  Se doc/plans/2026-11-20_Installation.md, avsnitt 10.
- 💡 Nytt och Öppna projekt direkt i huvudfönstret, utan att gå via väljaren.
- 💡 Återställ osparade arbetskopior efter en krasch, när projekten får redigerbara data.
- 💡 En instans per projekt: skicka en dubbelklickad fil till en Kalkyl som redan är öppen.
- 💡 Avbryt-knapp under "Öppnar…" och städning av gamla ~*.tmp i projektmappar.
- 💡 Överge en öppning som hänger på en share som slutat svara (lås, stämpel, kopiering i frikopplad tråd),
  så att väljaren alltid går att stänga direkt. Se doc/plans/archive/2026-11-20_Grunder.md, avsnitt 12.
