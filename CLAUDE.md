# Kalkyl

Ett internt kalkylprogram för luftledningsberäkningar.

## Delar

Fönstren ligger i `src/windows/`, stödbiblioteken i `libs/`.

| Del | Katalog | Innehåll |
|---|---|---|
| Projektväljare | `src/windows/project_picker.*` | Startfönstret: de 10 senaste projekten, Öppna, Öppna annat projekt…, Nytt projekt… och Ta bort ur listan |
| Huvudfönster | `src/windows/main_window.*` | Programikon (stor med verktygsfält), menyer och lägesknapp i namnlisten, flikarna Dokumentation (uppgifter enligt ByggPM i blått, grönt och gult, beräkningar före i lila och efter i rosa; fas- och topplina ur linregistret) och Stolptabell före och statusrad |
| Lindata | `src/windows/conductor_dialog.*` | Inställningar > Lindata…: redigera linregistret (lägg till, ta bort, återställ startdata) |
| Version | `libs/version` | Programmets version som header och versionsresurs för kalkyl.exe |
| Felmodell | `libs/core` | `Result<T>` och `Error`, som alla bibliotek använder |
| Plattform | `libs/platform` | Text (UTF-8/UTF-16), system, filer, låsfil och filassociation (Win32) |
| Databas | `libs/db` | SQLite-anslutning, satser och migrering |
| Systemdatabas | `libs/systemdb` | Utseendeläge, senaste mapp, senaste projekt och linregistret, lokalt per användare |
| Projektdatabas | `libs/projectdb` | Projektfilen `.kalkyl`: lokal arbetskopia, lås och säker publicering |
| Utseende | `libs/appearance` | Slate, temat Windows och lägesknappen, gemensamt för alla toppfönster |
| Resurser | `res` | Programikonen (källa, `assets.bin`, `kalkyl.ico`) |
| Installation | `installer` | setup.exe per användare (NSIS) |

## Teknik

- C++20, MSYS2 UCRT64 (g++), CMake + Ninja.
- GUI: [GlazeUI](https://github.com/holmbom/glazeui), hämtas med FetchContent på en fast tagg (nu `v0.11.1`) i rotens `CMakeLists.txt`.
  Uppgradera genom att byta taggen, aldrig `main`.
- GlazeUI-dokumentation: `MANUAL.md` i GlazeUI-källan, `build/_deps/glazeui-src/`.
- SQLite tas statiskt ur MSYS2 (`libsqlite3.a`), se `libs/db`.
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
  - `test` bygger testerna (`KALKYL_TESTS=ON`) i `build-test/`. Kör dem med `ctest --preset test`.
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
| `res` | Resurser som checkas in: programikonen och skriptet som ritar den |
| `installer` | Installationen: NSIS-skriptet, licenstexten och målet installer |

## Data

- **Systemdatabasen** `system.db` ligger lokalt i `%LOCALAPPDATA%\Kalkyl` (eller `KALKYL_DATA_DIR`) och sparar utseendeläge, senaste mapp, senaste projekt och linregistret. Linregistrets startdata (SWECO:s 129 linor ur Linberäkning.xlsm) ligger i `libs/systemdb/conductor_seed_v1.h`.
- **Projekt** är en SQLite-fil med ändelsen `.kalkyl`, som kan ligga på en nätverksshare.
  Programmet kopierar den till en lokal arbetskopia vid öppning och skriver tillbaka den bara med atomär publicering. En låsfil visar att projektet är öppet.
- Avinstallationen rör aldrig systemdatabasen, arbetskopiorna eller projektfilerna.
- `.kalkyl` associeras per användare med Kalkyl (`kalkyl.exe --registrera` och `--avregistrera`).
- Detaljer, felmodell och API:er står i `doc/plans/archive/2026-11-20_Grunder.md`.

### Migrering mellan versioner

Varje taggad version (t.ex. `v0.1.0`) till nästa taggade version ska alltid migrera både systemdatabasen och projektdatabasen korrekt. Inga data får gå förlorade. Migrering gäller bara mellan taggade versioner.

- **Före första taggade versionen** finns ingen migrering: schemat ändras direkt i steg 1. Lokala utvecklingsdatabaser (`system.db`, testprojekt) tas bort och skapas om.
- Schemat versioneras med `PRAGMA user_version`. Efter en taggad version är varje ändring ett nytt steg sist i stegen (`db::Schema::steps`), och de stegen körs av `db::migrate`.
- Ändringar mellan två taggar samlas i ETT steg per databas, som kan ändras fritt tills nästa tagg sätts.
- Ett steg som har funnits i en taggad version ändras aldrig. Fel rättas med ett nytt steg.
- Varje schemaändring har ett test som skapar databasen som den såg ut i den förra taggade versionen, öppnar den med den nya koden och kontrollerar att alla data finns kvar och att de nya delarna är på plats.
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
