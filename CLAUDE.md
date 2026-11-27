# Kalkyl

Ett internt kalkylprogram för luftledningsberäkningar.

## Delar

Fönstren ligger i `src/windows/`, stödbiblioteken i `libs/`.

| Del | Katalog | Innehåll |
|---|---|---|
| Projektväljare | `src/windows/project_picker.*` | Startfönstret: de 10 senaste projekten, Öppna, Öppna annat projekt…, Nytt projekt… och Ta bort ur listan, menyerna Arkiv > Avsluta och Hjälp > Om Kalkyl i namnlisten |
| Huvudfönster | `src/windows/main_window.*` | Programikon (stor med verktygsfält), menyer och lägesknapp i namnlisten, flikarna Dokumentation (uppgifter enligt ByggPM i blått, grönt och gult, beräkningar före i lila och efter i rosa; fas- och topplina kopieras ur linregistret till projektet; högerklick på normalspanns- och linberäkningsraderna; ett nytt projekt får förval), Stolptabell före (redigerbar, Excel-inklistring, decimaler som de skrevs, papperskorg med fråga, sparas i projektet), Stolptabell efter (kopia av före, färger mot före matchade på Stolpnr, filtret Förändrade ±2/Alla med grå rader för dolda stolpar, Visa förändrade och Kopiera från Stolptabell före per cell i snabbmenyn), automatiskt läge i båda stolptabellerna (start och slut per lina, automatiska normalspanns- och linberäkningar i Dokumentation, Gå till tabellen), Normalspann och Linberäkning (alla beräkningar, även lösa; de automatiska först, märkta och låsta), Arkiv > Spara (Ctrl+S), automatisk sparning och statusrad |
| Lindata | `src/windows/conductor_dialog.*` | Inställningar > Lindata…: redigera linregistret (lägg till, ta bort, återställ startdata) |
| Normalspann | `src/windows/normal_span_dialog.*` | Formuläret Normalspannsberäkning enligt Normalspannberäkning.xlsm: tabell med Stpnr, spannlängd och H som i Excel (urklipp, markering), resultat och kontrollerna 1–5; visar en automatisk beräkning skrivskyddad |
| Linberäkning | `src/windows/line_calc_dialog.*` | Formuläret Linberäkning som arket: Indata, Lindata/Konstanter och Lastfall med Resultat och Data till SAG, korsningsmallarna, lägena Ny och Excel med skillnadsmarkering, kopplad (indata ur Dokumentation) eller manuell; visar en automatisk beräkning skrivskyddad |
| Om Kalkyl | `src/windows/about_dialog.*` | Hjälp > Om Kalkyl: programikonen i stort format, version, underlaget för beräkningarna, utvecklaren och copyright |
| PM Excel-arket | `src/windows/line_calc_info_dialog.*` | Visar PM:et om felen i Excel-arket (`res/linberakning_excel.md`), öppnas med i-knappen vid läget Ny |
| Normalspann | `libs/anCalc` | Normalspannsberäkningen som rena funktioner |
| Tabelltext | `libs/tableText` | Tal med decimalkomma, kanonisk decimaltext (decimalerna som de skrevs), tabbseparerat urklipp och inklistring |
| Stolptabell | `libs/poleTable` | Stolptabellens radmodell: kolumnerna, talen som decimaltext, visning, urklipp och ändringar |
| Automatiskt läge | `libs/autoMode` | Sträckorna i stolptabellen, normalspannets stolpar, panelens text, dubbletter, jämförelsen mellan Stolptabell före och efter och återställningen av en cell ur före som rena funktioner |
| Vyn av Stolptabell efter | `libs/poleView` | Filtret Förändrade ±2, de grå raderna, rullningsmålet för Visa förändrade och infotexten som rena funktioner |
| Importera stolptabell | `src/windows/pole_import_dialog.*` | Stolptabell före > Importera stolptabell…: läser en stolptabell från Banken (.xlsx/.xlsm), visar kolumnvalet per fält med säkerhet, Från–Till och förhandsvisning, och ersätter bara raderna i Stolptabell före |
| Excelfiler | `libs/xlsx` | Celler, delade strängar, talformat och sammanslagningar ur .xlsx och .xlsm med miniz och en egen XML-läsare, utan makron |
| Stolptabellsimport | `libs/poleImport` | Rubrikområdet, kolumnigenkänningen på rubrikerna (poäng, nära varianter, inlärda val), utdraget och rådataposten som rena funktioner |
| Stolptabellen i fönstret | `libs/poleGrid` | `PoleGrid`: stolptabellen i en GlazeUI DataGrid med Excel-markering, papperskorg, snabbmeny och färger, delad av Stolptabell före och efter |
| Linberäkning | `libs/lineCalc` | Arket Linberäkning (15 lastfall, Ei och Ep) och lastfallsmallarna som rena funktioner, i lägena Excel (som arket) och Ny (felen rättade) |
| Excel-tabell | `libs/sheetGrid` | Excel-markering, urklipp och inklistring i en GlazeUI DataGrid med fast antal rader |
| Linor | `libs/conductor` | Typen `Conductor`, validering och jämförelser, gemensam för systemdb och projectdb |
| Version | `libs/version` | Programmets version som header och versionsresurs för kalkyl.exe |
| Felmodell | `libs/core` | `Result<T>` och `Error`, som alla bibliotek använder |
| Plattform | `libs/platform` | Text (UTF-8/UTF-16), system, filer, låsfil och filassociation (Win32) |
| Databas | `libs/db` | SQLite-anslutning, satser och migrering |
| Systemdatabas | `libs/systemdb` | Utseendeläge, senaste mapp, senaste projekt, linregistret, automatisk sparning, importmappen, de inlärda kolumnvalen för importen och filtret i Stolptabell efter, lokalt per användare |
| Projektdatabas | `libs/projectdb` | Projektfilen `.kalkyl`: lokal arbetskopia, lås och säker publicering; Dokumentation, linkopior, Stolptabell före och efter, automatiskt läge med de lagrade automatiska beräkningarna (`auto_calc`, synkade i samma transaktion), importens rådata (`pole_import*`), normalspanns- och linberäkningar och upplösningen av kopplade linberäkningar (`project_data.*`) |
| Utseende | `libs/appearance` | Slate, temat Windows och lägesknappen, gemensamt för alla toppfönster |
| Resurser | `res` | Programikonen (källa, `assets.bin`, `kalkyl.ico`) och PM:et `linberakning_excel.md`, som byggs in i kalkyl.exe |
| Tester | `tests` | Alla tester (ctest) och deras facit |
| Installation | `installer` | setup.exe per användare (NSIS) |

## Teknik

- C++20, MSYS2 UCRT64 (g++), CMake + Ninja.
- GUI: [GlazeUI](https://github.com/holmbom/glazeui), hämtas med FetchContent på en fast tagg (nu `v0.11.1`) i rotens `CMakeLists.txt`.
  Uppgradera genom att byta taggen, aldrig `main`.
- GlazeUI-dokumentation: `MANUAL.md` i GlazeUI-källan, `build/_deps/glazeui-src/`.
- SQLite tas statiskt ur MSYS2 (`libsqlite3.a`), se `libs/db`.
- miniz (zip, MIT) hämtas med FetchContent på en fast tagg (nu `3.1.2`) i rotens `CMakeLists.txt`, bara källorna, och byggs som `kalkyl_miniz` i `libs/xlsx`. Licensen står i `installer/licens.txt`.
- Alla program länkas med `-static` (C++-runtime och winpthread), så `kalkyl.exe` går att köra utan MSYS2 i PATH.
- Bakgrundsarbete (projektöppning, kontroll av filer) körs med `glaze::threads::Worker`, målet `glaze_threads_ui`, som länkas direkt till `kalkyl`.
- Installation: NSIS 3 (`makensis` ur `mingw-w64-ucrt-x86_64-nsis`). Utgivaren står i `KALKYL_PUBLISHER` i rotens `CMakeLists.txt`.
- Varningar: `kalkyl_warnings` (`-Wall -Wextra`) länkas till alla egna mål. GlazeUI hämtas som `SYSTEM`.

## Bygga och köra

```sh
cmake --preset debug
cmake --build --preset debug
./build/src/kalkyl.exe
```

- `CMakePresets.json` låser generatorn till Ninja och kompilatorn till UCRT64. VS Code (CMake Tools) använder samma presets.
- Presets:
  - `debug` bygger i `build/`, `release` i `build-release/`.
  - `tools` bygger GlazeUI-verktygen (`glazeassets`, `glaze-render`) i `build-tools/`.
    Då rörs inte det vanliga bygget.
  - `installer` bygger setup.exe: `cmake --preset release && cmake --build --preset installer`.
    Resultatet är `build-release/installer/Kalkyl-<version>-setup.exe`.
  - `test` bygger testerna i `tests/` (`KALKYL_TESTS=ON`) i `build-test/`. Kör dem med `ctest --preset test`, som kör åtta tester parallellt. Varje test skriver i en egen tillfällig mapp, så de krockar inte.
- Byter man generator i en befintlig byggkatalog måste hela katalogen tas bort, även `_deps/`.
- Förhandsgranska ett fönster: `build-tools/_deps/glazeui-build/apps/glaze-render/glaze-render.exe src/windows/main_window.h -o bild.png`.

## Driftsättning

- Normalfallet är setup.exe: per användare i `%LOCALAPPDATA%\Programs\Kalkyl`, utan administratörsrätt,
  med genväg i Startmenyn, filtypen och en rad i Appar och funktioner.
- Tyst: `/S [/D=<mapp>]`, och avinstallation med `avinstallera.exe /S`.
- Avinstallationen tar aldrig bort `%LOCALAPPDATA%\Kalkyl`.
- Utan installation fungerar det fortfarande att kopiera `kalkyl.exe` och `assets.bin` till samma mapp.
  Inga andra filer behövs. Saknas `assets.bin` visas GlazeUI:s förvalda ikon i fönstret, och ett fel loggas.

## Katalogstruktur

Strukturen är fast. Nya filer läggs där de hör hemma.

| Katalog | Innehåll |
|---|---|
| `doc/plans` | Pågående planer |
| `doc/plans/archive` | Avklarade planer, flyttas hit när de är klara |
| `doc/research` | Forskningsrapporter |
| `src` | Programmet: bara `main.cpp` och `CMakeLists.txt` |
| `src/windows` | Alla fönster och dialoger, tre filer per fönster |
| `libs` | Stödbibliotek, en katalog per bibliotek (t.ex. systemdb, nätverk) |
| `tests` | Alla tester och deras facit. Inga tester ligger bland källkoden i `src` eller `libs` |
| `res` | Resurser som checkas in: programikonen, skriptet som ritar den och texter som byggs in i programmet (PM:et om Excel-arket) |
| `installer` | Installationen: NSIS-skriptet, licenstexten och målet installer |

## Data

- **Systemdatabasen** `system.db` ligger lokalt i `%LOCALAPPDATA%\Kalkyl` (eller `KALKYL_DATA_DIR`) och sparar utseendeläge, senaste mapp, senaste projekt, linregistret, intervallet för automatisk sparning och, för importen av stolptabeller, importmappen och de inlärda kolumnvalen, och filtret i Stolptabell efter. Linregistrets startdata (SWECO:s 129 linor ur Linberäkning.xlsm) ligger i `libs/systemdb/conductor_seed_v1.h`.
- **Projekt** är en SQLite-fil med ändelsen `.kalkyl`, som kan ligga på en nätverksshare.
  Programmet kopierar den till en lokal arbetskopia vid öppning och skriver tillbaka den bara med atomär publicering (Ctrl+S eller automatisk sparning). En låsfil visar att projektet är öppet.
  Projektet sparar Dokumentationens värden, en kopia av varje använd lina, Stolptabell före och efter (talen som text med användarens decimaler, och i efter Av Fas1, Fas2 och Fas3, som får Av ur före vid kopieringen), inställningen för automatiskt läge (läget och start och slut per lina och stolptabell), normalspannsberäkningarna och linberäkningarna.
  De automatiska beräkningarna sparas som vanliga beräkningar med stabila id, märkta i `auto_calc`. `projectdb` håller dem aktuella i samma transaktion som varje ändring de bygger på.
  En import av en stolptabell sparar också rådata (alla kolumner för de importerade raderna, rubrikerna och filens namn) i `pole_import*`. En ny import ersätter dem, och handändringar rör dem inte.
  En kopplad linberäkning sparar inte linan, aₙ, σ₀, min och max, utan hämtar dem ur Dokumentation vid varje omräkning.
- Avinstallationen rör aldrig systemdatabasen, arbetskopiorna eller projektfilerna.
- `.kalkyl` associeras per användare med Kalkyl (`kalkyl.exe --registrera` och `--avregistrera`).
- Detaljer, felmodell och API:er står i `doc/plans/archive/2026-11-20_Grunder.md`.

### Migrering mellan versioner

Databaser från den senaste commiten ska alltid gå att öppna med den nya koden, både systemdatabasen och projektdatabasen. Inga data får gå förlorade.
Det gäller hela vägen från `v0.1.0` (taggad på commit 8f4f3d5) och framåt.

- **Räknaren** är `PRAGMA user_version` i filen: varje steg i `db::Schema::steps` höjer den med ett, och `db::migrate` kör de steg som saknas i en transaktion.
  En fil med högre nummer än programmet känner till vägras (`TooNew`).
- **Steg 1** är schemat i v0.1.0 och ändras aldrig.
- **Utvecklingsmigreringen** ligger i en egen fil per databas, `libs/<databas>/dev_migration.h` (namnrymden `…::dev`), med stegen efter v0.1.0.
  Ändringar sedan den senaste commiten samlas i ETT nytt steg sist, som kan ändras fritt tills det committas. Ett committat steg ändras aldrig; fel rättas med ett nytt steg.
- **Vid 1.0.0** slås alla steg ihop till ett nytt steg 1, `dev_migration.h` töms och räknaren börjar om. Därefter gäller samma regel mellan släppta versioner.
- Varje schemaändring har ett test som skapar databasen som den såg ut i den förra commiten, öppnar den med den nya koden och kontrollerar att alla data finns kvar och att de nya delarna är på plats.
- Startdata som följer med ett steg, t.ex. lindata, läggs in av steget. Användarens egna ändringar skrivs aldrig över av en senare migrering.

## Fönster

- Alla fönster och dialoger skrivs med GlazeUI:s 3-filsmetod (MANUAL-avsnittet `three-files`):
  - `<namn>.h`: klassen, där `#pragma region GlazeBuilder` tillhör editorn.
  - `<namn>.glazeui`: genererad layout, `initializeComponent()`.
  - `<namn>.cpp`: egen kod, en medlemsfunktion per event.
- Filerna ligger i `src/windows/` och läggs till i `src/CMakeLists.txt` med `glaze_add_window`.
- Fönsterklasserna ligger utanför namespace. GlazeBuilder läser bara okvalificerade klassnamn (`MainWindow::MainWindow()`).
- Utseendet är Slate med temat Windows. Det sätts i varje toppfönsters `onLoad` via `kalkyl::appearance::apply`, och varje `.glazeui` har designraden `// glazeui-design: ui=Slate theme=slate(Windows) style=0 lang=sv`.
- Fönster ska gå att öppna i GlazeBuilder och förhandsgranskas. Inga handkodade layouter utanför `.glazeui`.

## Arbetssätt

Arbetsflödet med agenter, planer, `TODO.md`, CLAUDE.md per katalog, kommentarer, versioner, filredigering och git
står i den globala `~/.claude/CLAUDE.md`. Här står bara det som är särskilt för Kalkyl:

- **Byggkataloger:** huvudkatalogen är `build/` (presetet `debug`). Implementeringsagenternas egna kataloger
  konfigureras med `-DFETCHCONTENT_SOURCE_DIR_GLAZEUI=C:/Users/sehoer/Source/Kalkyl/build/_deps/glazeui-src`, så att GlazeUI inte klonas igen.
- **Versionen** står bara i `project(VERSION ...)` i rotens `CMakeLists.txt`. setup.exe och versionsresursen i kalkyl.exe får versionen därifrån.
- **Beräkningarna ska vara korrekta, och varje avsteg från Excel-underlaget dokumenteras.**
  - En beräkning som återger ett Excel-ark har läget Excel, som räknar exakt som arket, och läget Ny, där felen är rättade.
  - Varje avsteg beskrivs i ett PM i `res/` med vad Excel gör, vad Kalkyl gör och varför, kortfattat och utan exempel.
  - PM:et visas i programmet med en i-knapp (`StockIcon::Information`) vid läget Ny.
  - Ett nytt eller ändrat avsteg uppdaterar PM:et i samma ändring. Linberäkningens PM är `res/linberakning_excel.md`.
