# Kalkyl

Ett internt kalkylprogram för luftledningsberäkningar.

## Delar

Fönstren ligger i `src/windows/`, stödbiblioteken i `libs/`.

| Del | Katalog | Innehåll |
|---|---|---|
| Projektväljare | `src/windows/project_picker.*` | Startfönstret: de 10 senaste projekten, Öppna, Öppna annat projekt…, Nytt projekt… och Ta bort ur listan |
| Huvudfönster | `src/windows/main_window.*` | Programikon (stor med verktygsfält), menyer och lägesknapp i namnlisten, arbetsyta och statusrad |
| Version | `libs/version` | Programmets version som header och versionsresurs för kalkyl.exe |
| Felmodell | `libs/core` | `Result<T>` och `Error`, som alla bibliotek använder |
| Plattform | `libs/platform` | Text (UTF-8/UTF-16), system, filer, låsfil och filassociation (Win32) |
| Databas | `libs/db` | SQLite-anslutning, satser och migrering |
| Systemdatabas | `libs/systemdb` | Utseendeläge, senaste mapp och senaste projekt, lokalt per användare |
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

- **Systemdatabasen** `system.db` ligger lokalt i `%LOCALAPPDATA%\Kalkyl` (eller `KALKYL_DATA_DIR`) och sparar utseendeläge, senaste mapp och senaste projekt.
- **Projekt** är en SQLite-fil med ändelsen `.kalkyl`, som kan ligga på en nätverksshare.
  Programmet kopierar den till en lokal arbetskopia vid öppning och skriver tillbaka den bara med atomär publicering. En låsfil visar att projektet är öppet.
- Avinstallationen rör aldrig systemdatabasen, arbetskopiorna eller projektfilerna.
- `.kalkyl` associeras per användare med Kalkyl (`kalkyl.exe --registrera` och `--avregistrera`).
- Detaljer, felmodell och API:er står i `doc/plans/2026-11-20_Grunder.md`.

## Planer

- Filnamnet är alltid `ÅÅÅÅ-MM-DD_<Namn>.md`, utan mellanslag. Ord i namnet skiljs med `_`, t.ex. `2026-11-20_Layout.md` och `2026-11-20_Layout_fas_1.md`.
- Pågående planer ligger i `doc/plans/`.
- En plan flyttas till `doc/plans/archive/` när ingen öppen punkt i `TODO.md` länkar till den längre.

## TODO.md

`TODO.md` i roten är listan över allt arbete, alla buggar och alla idéer.

- **Status:** varje punkt börjar med en statusglyf. Förklaringen står överst i filen.
  - ⬜ ej startad
  - 📝 planeras
  - 🔨 utförs
  - 🔍 granskas
  - ⏸️ väntar
  - ✅ klar
  - 🐞 bugg
  - 💡 idé
- **Längd:** en punkt är högst 3 rader.
- **Länk:** arbetspunkter länkar till sin planfil.
- **Utan planfil:** buggar (🐞) och idéer (💡) har ingen planfil. En idé som blir arbete får en planfil och status ⬜.
- **Klara punkter:** en punkt som blir ✅ tas bort från listan. Är den sista punkten för en plan klar arkiveras planfilen.
- **Ordning:** punkterna står under rubrikerna Arbete, Buggar och Idéer, i prioritetsordning.

## CLAUDE.md

- Varje katalog har en `CLAUDE.md`.
- Den här filen beskriver projektet i stort.
- I underkatalogerna beskriver `CLAUDE.md` bara den katalogens egna filer.
- Uppdatera katalogens `CLAUDE.md` när filer läggs till, tas bort eller byter syfte.

## Kommentarer i källkod

- Varje funktion, klass och logisk del har en kommentar på 2–5 rader.
- Inline-kommentarer är korta och säger bara vad som händer.
- Ingen historik i kommentarer (vad som ändrats, vem, när, tidigare beteende). Sådant hör hemma i git.

## Fönster

- Alla fönster och dialoger skrivs med GlazeUI:s 3-filsmetod (MANUAL-avsnittet `three-files`):
  - `<namn>.h`: klassen, där `#pragma region GlazeBuilder` tillhör editorn.
  - `<namn>.glazeui`: genererad layout, `initializeComponent()`.
  - `<namn>.cpp`: egen kod, en medlemsfunktion per event.
- Filerna ligger i `src/windows/` och läggs till i `src/CMakeLists.txt` med `glaze_add_window`.
- Fönsterklasserna ligger utanför namespace. GlazeBuilder läser bara okvalificerade klassnamn (`MainWindow::MainWindow()`).
- Utseendet är Slate med temat Windows. Det sätts i varje toppfönsters `onLoad` via `kalkyl::appearance::apply`, och varje `.glazeui` har designraden `// glazeui-design: ui=Slate theme=slate(Windows) style=0 lang=sv`.
- Fönster ska gå att öppna i GlazeBuilder och förhandsgranskas. Inga handkodade layouter utanför `.glazeui`.

## Arbetsflöde med agenter

Huvudagenten är samordnare: den fördelar arbetet, startar agenterna, följer upp och rapporterar till användaren.

| Roll | Modell | Effort | Antal |
|---|---|---|---|
| Planering | Opus | xhigh | högst 1 |
| Implementering | Sonnet | high | högst 4 samtidigt |
| Granskning | Opus | high | 1 |

- Planeringsagenten skriver planen i `doc/plans/`. Implementeringen följer planen.
- Implementeringsagenterna får egna filer att äga, så att de inte skriver i samma filer samtidigt.
- Efter implementeringen testar användaren först. Granskningen startar först när användaren säger till, så att ändringar från testet kommer med.
- Granskningsagenten granskar det som implementerats mot planen och reglerna i den här filen.
- Små fixar efter granskningen gör samordnaren själv.
  Större fixar går tillbaka till den Sonnet-agent som skrev koden.
- Ingen commit, varken lokalt eller till remoten, förrän användaren säger till.

## Versioner

Formatet är `major.minor.patch`. Versionen står bara i `project(VERSION ...)` i rotens `CMakeLists.txt`.

**Versionen ändras bara när användaren säger till.** Planer och agenter höjer den aldrig själva.
setup.exe och versionsresursen i kalkyl.exe får versionen ur `project(VERSION)`.

- **major**: höjs när programmet är klart (1.0.0).
- **minor**: nya funktioner och API-brytande ändringar.
- **patch**: fixar och små ändringar, t.ex. i layout.
