# Installation: setup.exe per användare med NSIS

**Datum:** 2026-11-20 · **Status:** implementerad och verifierad enligt avsnitt 8, granskning och användartest återstår · **Version:** oförändrad (0.1.0). Versionen höjs bara när användaren säger till.

Planen beskriver `Kalkyl-<version>-setup.exe`. Den installerar Kalkyl för den inloggade användaren utan administratörsrätt. Utgivaren syns i Appar och funktioner och i exe-filernas versionsinformation. Avinstallationen rör aldrig användarens data. Beteendet i avsnitt 5 är bindande. NSIS-syntaxen får justeras om makensis kräver det, men inte beteendet.

> **Undantag i arbetsflödet (användarens beslut):** granskningen görs direkt efter implementeringen, före användartestet i avsnitt 9.

---

## 1. Mål och avgränsning

### Ingår
1. **NSIS-skript** med svenska sidor (MUI2): välkomst, licens, mapp, installation och slut med "Starta Kalkyl".
2. **Installation per användare** i `%LOCALAPPDATA%\Programs\Kalkyl` med `RequestExecutionLevel user`.
3. **Filer och genvägar:** `kalkyl.exe`, `assets.bin`, `avinstallera.exe`, en genväg i Startmenyn och en valfri genväg på skrivbordet.
4. **Uninstall-nyckel** under HKCU med utgivaren `SWECO Sverige AB, Erik Holmbom`.
5. **Filtypen** `.kalkyl` registreras med `kalkyl.exe --registrera` och tas bort med `--avregistrera`.
6. **Avinstallation** som bara tar bort de egna filerna. Mappen tas bort bara om den är tom, och `%LOCALAPPDATA%\Kalkyl\` rörs aldrig.
7. **Uppgradering, ominstallation, flytt och nedgradering**, och en kontroll av att Kalkyl inte körs.
8. **Versionsinformation** i `setup.exe` (VIAddVersionKey) och i `kalkyl.exe` (en ny versionsresurs via windres).
9. **Tyst installation** (`/S`, `/D=`) för IT-utrullning.
10. **CMake-målet `installer`** och build-presetet `installer`. Versionen läses ur `project(VERSION)`.

### Ingår inte
- Kodsignering (se avsnitt 10), MSI/MSIX och installation för alla användare (HKLM, Program Files).
- Automatiska uppdateringar, Intune-paket (`.intunewin`) och egna bilder till guiden. NSIS standardbild används.
- Ett val för att ta bort användardata vid avinstallation. Det är medvetet.
- Ändringar i `main.cpp` eller `libs/platform`. Flaggorna finns redan och används som de är.

---

## 2. Fakta (kontrollerat 2026-11-20)

- **makensis:** `C:/msys64/ucrt64/bin/makensis.exe` v3.13 från paketet `mingw-w64-ucrt-x86_64-nsis` 3.13-2, med `NSISDIR=C:\msys64\ucrt64\share\nsis`.
  - **Stubbar:** bara `*-amd64-unicode`. setup.exe blir alltså en 64-bitarsexe, som `kalkyl.exe`. Skriptet anger `Target amd64-unicode`.
  - **Include:** `MUI2.nsh`, `LogicLib.nsh`, `WordFunc.nsh` (`${VersionCompare}`), `FileFunc.nsh`, `nsDialogs.nsh`, `WinVer.nsh` och `x64.nsh` finns.
  - **Språk:** `Swedish.nsh` och `Swedish.nlf` finns, båda UTF-8 med BOM.
  - **Plugins:** System, nsDialogs, nsExec, UserInfo m.fl. finns, men **nsProcess finns inte**. Skriptet använder inga plugins alls.
  - **Flaggor:** makensis tar dem också med `-` (`-DVERSION=…`, `-V2`, `-WX`, `-INPUTCHARSET UTF8`). I Git Bash gjordes `/HELP` om till `C:/Program Files/Git/HELP`. Samma sak händer med setup.exe:s `/S`, så kör setup.exe från PowerShell eller med `MSYS_NO_PATHCONV=1`.
- **`kalkyl.exe` (Release) i dag:**
  - Resurserna är RT_ICON och RT_GROUP_ICON (`glaze_app_icon`) och RT_MANIFEST med `asInvoker` (MinGW:s förval). **Ingen VERSIONINFO.**
  - Filen är 12,9 MB, statiskt länkad och har inga debug-sektioner.
- **windres 2.47** avkodar UTF-8 rätt med `#pragma code_page(65001)`. Provat via stdin: "ä" blev `\344` och "–" blev `\x2013`.
- **GlazeUI:**
  - Alla GlazeUI-program, även GlazeBuilder, registrerar fönsterklassen `"GlazeWindow"`. `FindWindow` kan alltså inte skilja Kalkyl från andra program och hittar inte heller den dolda väljaren.
  - `gfx::log` skriver till OutputDebugString och stderr, aldrig till en fil i exe-katalogen. Installationsmappen innehåller därför bara de tre egna filerna.
- **`--registrera` och `--avregistrera`** körs i `main()` före `glaze::loadAssets()` och systemdatabasen. De visar inget fönster, rör inte `%LOCALAPPDATA%\Kalkyl` och avslutar med 0 eller 1.
- **NSIS avinstallerare** kopierar sig själv till `%TEMP%` och startar om därifrån (`_?=`). Processen man startar avslutas alltså direkt.
- **Miljön:** CMake 4.4.2, Ninja och `core.autocrlf=true`.

---

## 3. Beslut med motivering

### 3.1 Katalogen `installer/` (godkänd av användaren)
| Fil | Innehåll |
|---|---|
| `installer/CMakeLists.txt` | Målet `installer` (makensis) |
| `installer/kalkyl.nsi` | NSIS-skriptet |
| `installer/licens.txt` | Licenstexten |
| `installer/CLAUDE.md` | Katalogens beskrivning |

- setup.exe hamnar i `build-release/installer/Kalkyl-<version>-setup.exe` och checkas aldrig in. `/build*/` i `.gitignore` täcker den redan.

### 3.2 Installationsmapp: `%LOCALAPPDATA%\Programs\Kalkyl` (bekräftas)
- Det är FOLDERID_UserProgramFiles, Windows standardmapp för program per användare. NSIS `MultiUser.nsh` använder samma mapp som reserv. Ingen administratörsrätt behövs.
- Mappen är skild från datamappen `%LOCALAPPDATA%\Kalkyl`. En avinstallation når alltså aldrig `system.db` eller `arbete\`.
- Den ligger i Local och inte i Roaming: en exe på 13 MB ska inte följa med profilen, och Roaming omdirigeras ofta.
- **Mappsidan visas.** Förvalet är mappen ovan. Vid uppgradering är förvalet den nuvarande mappen (`InstallDirRegKey` läser InstallLocation).
- **Datamappen spärras.** Väljer användaren `%LOCALAPPDATA%\Kalkyl` eller en mapp inuti den, avvisas valet:
  - i guiden med ett besked, och sidan ligger kvar
  - i tyst läge med felkod 2
- **Valfri mapp är ofarlig.** Avinstallationen tar bara bort de egna filerna och mappen bara om den är tom. Även en mapp som `Dokument` blir alltså orörd.

### 3.3 Bygget: målet `installer` och presetet
- **Målet:** `installer/CMakeLists.txt` skapar `add_custom_target(installer)`, som inte ingår i ALL.
  - Ett vanligt `cmake --build --preset release` kräver alltså varken makensis eller tid.
  - Målet finns bara när `CMAKE_BUILD_TYPE` är `Release`. Debug registrerar aldrig filtypen och ska inte paketeras.
  - Saknas makensis skrivs ett `message(STATUS …)`, och målet skapas inte.
- **Presetet:** `{ "name": "installer", "configurePreset": "release", "targets": ["installer"] }`.
- **Användning:** `cmake --preset release && cmake --build --preset installer`.
- **Värden från CMake** skickas med `-D`: `VERSION=${PROJECT_VERSION}`, `PUBLISHER=${KALKYL_PUBLISHER}`, samt sökvägarna EXE, ASSETS, ICON och OUTFILE.
  - Versionen står aldrig i skriptet.
  - Sökvägarna skickas genom `$<SHELL_PATH:…>`, så att makensis får omvända snedstreck.
- **Utgivaren** står på ett ställe: `set(KALKYL_PUBLISHER "SWECO Sverige AB, Erik Holmbom")` i rotens `CMakeLists.txt`. Både versionsresursen och installationen läser den därifrån.
- **makensis-flaggor:** `-V2` (bara varningar och fel), `-WX` (varningar stoppar bygget) och `-INPUTCHARSET UTF8`.
- **Beroenden:** setup.exe byggs om när `kalkyl` (målet), `kalkyl.nsi`, `licens.txt`, `res/assets.bin` eller `res/kalkyl.ico` ändras.

### 3.4 Sidor, språk och ikon
- **Språk och ikon:** svenska via `MUI_LANGUAGE "Swedish"`. Ikonen `res/kalkyl.ico` används både för setup.exe och för avinstalleraren (`MUI_ICON`, `MUI_UNICON`).
- **Installationens sidor:**
  - Välkomst med egen text: ingen administratörsbehörighet behövs, stäng Kalkyl först, data påverkas inte.
  - Licens.
  - Mapp med kontroll i sidans leave-funktion.
  - Installation.
  - Slut med två kryssrutor: "Starta Kalkyl" (ikryssad) och "Skapa en genväg på skrivbordet" (inte ikryssad).
- **Avinstallationens sidor:** bekräftelse med texten "Projektfilerna (.kalkyl) och dina inställningar i …\Kalkyl ligger kvar", sedan förlopp.
- **Övrigt:**
  - `Name "Kalkyl ${VERSION}"`, så att guiden visar versionen.
  - `BrandingText "${PUBLISHER}"` ersätter "Nullsoft Install System".
  - `MUI_ABORTWARNING`, `ManifestDPIAware true` och `XPStyle on`.

### 3.5 Vad installeras
- **Filer:** `kalkyl.exe` och `assets.bin`, som ur Grunder 2.8 är allt som behövs, samt avinstalleraren `avinstallera.exe`. Det svenska namnet följer `--registrera` och `--avregistrera`.
- **Startmenyn:** `$SMPROGRAMS\Kalkyl.lnk`, direkt i Program utan undermapp, som Windows 11 rekommenderar för ett enda program.
- **Skrivbordet:** inte som förval, eftersom Windows 11 avråder och användaren kan fästa programmet själv.
  - Slutsidans kryssruta skapar genvägen.
  - Fanns `$DESKTOP\Kalkyl.lnk` redan, skrivs den om vid uppgradering och flytt så att den pekar på den nya exe:n.
  - Tyst läge skapar ingen ny skrivbordsgenväg.
- **Uninstall-nyckeln:** `HKCU\Software\Microsoft\Windows\CurrentVersion\Uninstall\Kalkyl`, skriven sist i en egen dold sektion:

| Värde | Typ | Innehåll |
|---|---|---|
| DisplayName | SZ | `Kalkyl` |
| DisplayVersion | SZ | `${VERSION}` |
| Publisher | SZ | `SWECO Sverige AB, Erik Holmbom` |
| DisplayIcon | SZ | `<mapp>\kalkyl.exe,0` |
| InstallLocation | SZ | `<mapp>` |
| UninstallString | SZ | `"<mapp>\avinstallera.exe"` |
| QuietUninstallString | SZ | `"<mapp>\avinstallera.exe" /S` |
| NoModify, NoRepair | DWORD | 1 |
| EstimatedSize | DWORD | kB enligt `SectionGetSize` för huvudsektionen |

### 3.6 Filtypen: `kalkyl.exe --registrera` och inte NSIS-nycklar
- Registervärdena, ProgID och `SHChangeNotify` finns i dag på ett ställe: `libs/platform/association.cpp`. Samma nycklar i NSIS skulle bli en andra kopia som kan glida isär, och `SHChangeNotify` skulle kräva System-pluginet.
- `--avregistrera` lämnar redan en registrering som pekar på en annan befintlig kopia orörd. Den logiken vore svår att göra om i NSIS.
- `kalkyl.exe` är statiskt länkad och går att köra direkt efter `File`.
- **Misslyckas `--registrera`** skrivs en rad i detaljerna, men installationen avbryts inte. Release registrerar ändå filtypen själv vid start när den saknas (`Missing`) eller pekar på en exe som inte finns (`Dead`).
- **Installationen tar över** en befintlig registrering, t.ex. ett utvecklingsbygge. `registerAssociation` skriver alltid över, och det är rätt för en installation.

### 3.7 Avinstallation
I den här ordningen:
1. Kontrollen att Kalkyl inte körs (3.9).
2. `ExecWait '"$INSTDIR\kalkyl.exe" --avregistrera'`, om exe:n finns.
3. `Delete` av `kalkyl.exe`, `assets.bin` och `avinstallera.exe`. Det fungerar eftersom avinstalleraren körs som kopia i `%TEMP%`.
4. `RMDir "$INSTDIR"` utan `/r`. Mappen tas alltså bara bort om den är tom.
5. `Delete` av `$SMPROGRAMS\Kalkyl.lnk` och `$DESKTOP\Kalkyl.lnk`.
6. `DeleteRegKey HKCU "${UNINST_KEY}"`.

- `%LOCALAPPDATA%\Kalkyl\` och projektfilerna nämns inte alls i avinstallationens kod, bara i bekräftelsesidans text.

### 3.8 Uppgradering, ominstallation, flytt och nedgradering
- **`.onInit`** läser `InstallLocation` till `$OldDir` och `DisplayVersion` till `$OldVersion`.
- **Samma eller äldre version installerad:** setup.exe installerar ovanpå utan fråga och skriver över filerna (`SetOverwrite on`, `AllowSkipFiles off`).
- **Nyare version installerad (nedgradering):**
  - `${VersionCompare}` ger 2, och guiden frågar Ja/Nej. Nej avbryter.
  - I tyst läge blir svaret Ja (`/SD IDYES`), så att IT kan gå tillbaka till en äldre version.
  - Risken är liten: en äldre Kalkyl som möter en nyare `system.db` får `TooNew` och kör med reserven i minnet (Grunder 2.10). Inga data förstörs.
- **Flytt:** användaren väljer en annan mapp än `$OldDir`, eller IT anger `/D=`. Om `$OldDir\avinstallera.exe` finns händer följande:
  1. Kontrollen att Kalkyl inte körs, för den gamla mappen.
  2. `ExecWait '"$OldDir\avinstallera.exe" /S _?=$OldDir'`, som kör den gamla avinstalleraren på plats och väntar.
  3. `Delete "$OldDir\avinstallera.exe"` och `RMDir "$OldDir"`.
  4. Sedan installeras allt på nytt i den nya mappen: genvägar, nyckel och filtyp.
- **Saknas den gamla avinstalleraren** blir de gamla filerna kvar, och nyckeln skrivs över. Det är accepterat.

### 3.9 Kalkyl körs: provet utan plugin
- **Provet:** `FileOpen $R1 "<mapp>\kalkyl.exe" a`. Windows nekar skrivåtkomst till en exe som körs (delningsfel). Att öppna filen ändrar inget i den.
- **Varför inte namnbyte:** en exe som körs går att döpa om, så det provet ger fel svar.
- **Varför inte FindWindow:** se avsnitt 2. Provet gäller exakt mappen som ska skrivas och hittar också dolda fönster. En Kalkyl som körs från en annan mapp, t.ex. ett utvecklingsbygge, stoppar ingenting.
- **Vid träff:** `MB_RETRYCANCEL` "Kalkyl körs. Stäng alla Kalkyl-fönster och klicka på Försök igen."
  - Avbryt, eller tyst läge (`/SD IDCANCEL`), ger `Abort`: installationen avbryts med kod 2 och har inte rört något.
- **När:** först i installationssektionen (för `$INSTDIR` och vid flytt för `$OldDir`) och först i avinstallationen.
- Funktionen finns i två varianter, `CheckNotRunning` och `un.CheckNotRunning`, via ett makro.

### 3.10 Versionsinformation
- **setup.exe:**
  - `VIProductVersion` och `VIFileVersion` är `"${VERSION}.0"`.
  - `VIAddVersionKey /LANG=${LANG_SWEDISH}` för ProductName `Kalkyl`, CompanyName `${PUBLISHER}`, FileDescription `Installation av Kalkyl`, FileVersion, ProductVersion, LegalCopyright `© SWECO Sverige AB. Internt program.`, Comments `Luftledningsberäkningar` och OriginalFilename `Kalkyl-${VERSION}-setup.exe`.
- **avinstallera.exe:** om NSIS ger den samma versionsinformation kontrolleras i 8.2. Saknas den är det accepterat, eftersom Publisher ändå syns i Appar och funktioner.
- **kalkyl.exe:** får en versionsresurs. Kravet gäller exe-filerna, och i dag har filen ingen.
  - **Mallen** `libs/version/version.rc.in` passar där, eftersom katalogen redan gör om `project(VERSION)` till `version.h`. `src/` får bara innehålla `main.cpp` och `CMakeLists.txt`, och `res/` är ikonen.
  - **Generering:** `configure_file(… version.rc @ONLY)` i `libs/version/CMakeLists.txt` skriver `${CMAKE_BINARY_DIR}/generated/version/version.rc`.
  - **Kompilering:** en `add_custom_command` med windres i `src/CMakeLists.txt` gör ett objekt som läggs till `kalkyl` med `EXTERNAL_OBJECT`. Det är samma väg som `glaze_app_icon`.
  - **Ingen `enable_language(RC)`.** Då kan målets kompileringsflaggor (`-Wall -Wextra` från `kalkyl_warnings`) nå windres, som inte känner till `-W`-flaggor.
  - **Innehåll:** CompanyName `@KALKYL_PUBLISHER@`, FileDescription `Kalkyl`, ProductName `Kalkyl`, version, LegalCopyright, OriginalFilename `kalkyl.exe` och InternalName `kalkyl`. Språkblocket är svenska och Unicode (`041D04B0`).
  - **FileDescription är `Kalkyl`**, eftersom Aktivitetshanteraren och "Öppna med" visar det som programmets namn.
  - **Copyright-texten** står ordagrant i både `version.rc.in` och `kalkyl.nsi`. Den innehåller "©" och går därför inte via kommandoraden.
  - Resursobjekten (ikon, manifest och version) slås ihop av ld. Ikonen och manifestet ligger redan i två olika objekt i dag.

### 3.11 Tyst installation och IT
- **Installera:** `Kalkyl-<version>-setup.exe /S [/D=<mapp>]`. `/D=` ska stå sist och utan citattecken, också med mellanslag.
- **Avinstallera:** QuietUninstallString, alltså `"<mapp>\avinstallera.exe" /S`.
- **Felkoder:** 0 klar, 1 avbruten av användaren (bara i guiden), 2 avbruten av skriptet (Kalkyl körs, datamappen vald eller nedgradering nekad).
- **Måste köras i användarens kontext** och aldrig som SYSTEM: i Intune "Install behavior: User". Annars hamnar programmet i SYSTEM-profilen.
- `/D=` stöds, och flytten följer 3.8.

### 3.12 Licenstexten
- Texten står i 5.5. En rad per stycke utan hårda radbrytningar, eftersom licensrutan bryter raderna själv.
- Licenssidans förvalda knapp "Jag godkänner" behålls.

### 3.13 Teckenkodning
- **`kalkyl.nsi`:** UTF-8. BOM behövs inte, eftersom CMake skickar `-INPUTCHARSET UTF8`.
- **`licens.txt`:** **UTF-8 med BOM och CRLF**. NSIS läser en licensfil utan BOM som ANSI, och då blir å, ä och ö fel.
  - Write-verktyget skriver utan BOM. Lägg därför till BOM en gång med PowerShell. Det är ingen Python.
    ```powershell
    $p = (Resolve-Path installer/licens.txt).Path
    $t = [IO.File]::ReadAllText($p, [Text.Encoding]::UTF8)
    [IO.File]::WriteAllText($p, ($t -replace "`r?`n", "`r`n"), (New-Object Text.UTF8Encoding $true))
    ```
    Kontrollera med `head -c 3 installer/licens.txt | od -An -tx1`, som ska ge `ef bb bf`.
  - **Reserv** om åäö ändå blir fel på licenssidan: spara filen som UTF-16LE med BOM.
- **`version.rc.in`:** UTF-8 med `#pragma code_page(65001)` först.

---

## 4. Filer

| Fil | Ny/ändras | Paket |
|---|---|---|
| `installer/CMakeLists.txt` | ny | I |
| `installer/kalkyl.nsi` | ny | I |
| `installer/licens.txt` | ny (UTF-8 med BOM, CRLF) | I |
| `installer/CLAUDE.md` | ny | I |
| `CMakeLists.txt` (roten) | `KALKYL_PUBLISHER` och `add_subdirectory(installer)` | I |
| `CMakePresets.json` | build-presetet `installer` | I |
| `CLAUDE.md` (roten) | Delar, Teknik, Bygga och köra, Driftsättning, Katalogstruktur, Data, Versioner | I |
| `libs/version/version.rc.in` | ny | V |
| `libs/version/CMakeLists.txt` | `configure_file` för `version.rc` | V |
| `libs/version/CLAUDE.md` | ny rad och ny beskrivning | V |
| `libs/CLAUDE.md` | raden för `version/` | V |
| `src/CMakeLists.txt` | windres för versionsresursen | V |
| `src/CLAUDE.md` | raden för `CMakeLists.txt` | V |
| `TODO.md`, `doc/plans/CLAUDE.md`, planen | | samordnaren |

`.gitignore`, `main.cpp` och `libs/platform` ändras inte.

---

## 5. Skisser (bindande beteende)

Varje funktion, sektion och logisk del får en kommentar på 2–5 rader, enligt CLAUDE.md, också i `.nsi` och CMake.

### 5.1 Rotens `CMakeLists.txt` (efter `project(...)`, versionen orörd)
```cmake
# Utgivaren i exe-filernas versionsinformation och i installationens Uninstall-nyckel.
set(KALKYL_PUBLISHER "SWECO Sverige AB, Erik Holmbom")
```
Längst ned efter `add_subdirectory(src)`, och kommentaren ovanför byggs ut med "och sist installationen":
```cmake
add_subdirectory(installer)
```

### 5.2 `CMakePresets.json`, `buildPresets`
```json
{ "name": "installer", "configurePreset": "release", "targets": ["installer"] }
```

### 5.3 `installer/CMakeLists.txt`
```cmake
# setup.exe för Kalkyl med NSIS (makensis ur MSYS2-paketet mingw-w64-ucrt-x86_64-nsis).
# Målet installer ingår inte i ALL och byggs med: cmake --build --preset installer.
# Bara Release paketeras, eftersom Debug aldrig registrerar filtypen och är långsam.
if(NOT CMAKE_BUILD_TYPE STREQUAL "Release")
    return()
endif()
get_filename_component(_bindir "${CMAKE_CXX_COMPILER}" DIRECTORY)
find_program(KALKYL_MAKENSIS makensis HINTS "${_bindir}")
if(NOT KALKYL_MAKENSIS)
    message(STATUS "makensis saknas, målet installer skapas inte (pacman -S mingw-w64-ucrt-x86_64-nsis)")
    return()
endif()

# Version och utgivare kommer ur rotens CMakeLists.txt, sökvägarna med omvända snedstreck.
# -WX gör varningar till fel, och -INPUTCHARSET UTF8 läser skriptet som UTF-8.
set(_setup "${CMAKE_CURRENT_BINARY_DIR}/Kalkyl-${PROJECT_VERSION}-setup.exe")
add_custom_command(
    OUTPUT "${_setup}"
    COMMAND "${KALKYL_MAKENSIS}" -V2 -WX -INPUTCHARSET UTF8
            "-DVERSION=${PROJECT_VERSION}"
            "-DPUBLISHER=${KALKYL_PUBLISHER}"
            "-DEXE=$<SHELL_PATH:$<TARGET_FILE:kalkyl>>"
            "-DASSETS=$<SHELL_PATH:${PROJECT_SOURCE_DIR}/res/assets.bin>"
            "-DICON=$<SHELL_PATH:${PROJECT_SOURCE_DIR}/res/kalkyl.ico>"
            "-DOUTFILE=$<SHELL_PATH:${_setup}>"
            "$<SHELL_PATH:${CMAKE_CURRENT_SOURCE_DIR}/kalkyl.nsi>"
    DEPENDS kalkyl
            "${CMAKE_CURRENT_SOURCE_DIR}/kalkyl.nsi"
            "${CMAKE_CURRENT_SOURCE_DIR}/licens.txt"
            "${PROJECT_SOURCE_DIR}/res/assets.bin"
            "${PROJECT_SOURCE_DIR}/res/kalkyl.ico"
    COMMENT "Bygger Kalkyl-${PROJECT_VERSION}-setup.exe (NSIS)"
    VERBATIM)
add_custom_target(installer DEPENDS "${_setup}")
```

### 5.4 `installer/kalkyl.nsi`
```nsis
; Kalkyl: installation per användare med NSIS 3 och Modern UI 2, på svenska.
; Målet installer (installer/CMakeLists.txt) skickar in VERSION, PUBLISHER, EXE, ASSETS,
; ICON och OUTFILE med -D. Versionen står aldrig här utan kommer ur project(VERSION).
; Allt skrivs i användarens profil och under HKCU, så ingen administratörsrätt behövs.
Target amd64-unicode
RequestExecutionLevel user
ManifestDPIAware true
XPStyle on
SetCompressor /SOLID lzma

; Stoppar bygget med ett tydligt fel om ett värde från CMake saknas.
!macro RequireDefine NAME
  !ifndef ${NAME}
    !error "${NAME} saknas. Bygg med: cmake --build --preset installer"
  !endif
!macroend
!insertmacro RequireDefine VERSION
!insertmacro RequireDefine PUBLISHER
!insertmacro RequireDefine EXE
!insertmacro RequireDefine ASSETS
!insertmacro RequireDefine ICON
!insertmacro RequireDefine OUTFILE

; Uninstall-nyckeln under HKCU och Kalkyls datamapp. Datamappen (system.db, arbete\)
; tillhör användaren: den får aldrig bli installationsmapp och rörs aldrig.
!define UNINST_KEY "Software\Microsoft\Windows\CurrentVersion\Uninstall\Kalkyl"
!define DATA_DIR "$LOCALAPPDATA\Kalkyl"

Name "Kalkyl ${VERSION}"
OutFile "${OUTFILE}"
InstallDir "$LOCALAPPDATA\Programs\Kalkyl"
InstallDirRegKey HKCU "${UNINST_KEY}" "InstallLocation"
BrandingText "${PUBLISHER}"
AllowSkipFiles off

!include "MUI2.nsh"
!include "LogicLib.nsh"
!include "WordFunc.nsh"

; En tidigare installation enligt Uninstall-nyckeln, och om skrivbordsgenvägen fanns.
Var OldDir
Var OldVersion
Var HadDesktop

; Ikonen ur res/kalkyl.ico för setup och avinstallation, och en fråga innan man avbryter.
!define MUI_ICON "${ICON}"
!define MUI_UNICON "${ICON}"
!define MUI_ABORTWARNING

; Installationens sidor: välkomst, licens, mapp (med kontroll), installation och slut.
!define MUI_WELCOMEPAGE_TEXT "Guiden installerar Kalkyl ${VERSION} för dig. Ingen administratörsbehörighet behövs.$\r$\n$\r$\nStäng Kalkyl innan du fortsätter, om programmet är öppet. Dina projekt och inställningar påverkas inte.$\r$\n$\r$\n$_CLICK"
!insertmacro MUI_PAGE_WELCOME
!insertmacro MUI_PAGE_LICENSE "${__FILEDIR__}\licens.txt"
!define MUI_DIRECTORYPAGE_TEXT_TOP "Kalkyl installeras i mappen nedan, bara för dig. Väljer du en annan mapp än en tidigare installation flyttas Kalkyl dit."
!define MUI_PAGE_CUSTOMFUNCTION_LEAVE DirectoryLeave
!insertmacro MUI_PAGE_DIRECTORY
!insertmacro MUI_PAGE_INSTFILES
!define MUI_FINISHPAGE_RUN "$INSTDIR\kalkyl.exe"
!define MUI_FINISHPAGE_RUN_TEXT "Starta Kalkyl"
!define MUI_FINISHPAGE_SHOWREADME ""
!define MUI_FINISHPAGE_SHOWREADME_NOTCHECKED
!define MUI_FINISHPAGE_SHOWREADME_TEXT "Skapa en genväg på skrivbordet"
!define MUI_FINISHPAGE_SHOWREADME_FUNCTION CreateDesktopShortcut
!insertmacro MUI_PAGE_FINISH

; Avinstallationens sidor: bekräftelse med besked om att data ligger kvar, och förlopp.
!define MUI_UNCONFIRMPAGE_TEXT_TOP "Kalkyl tas bort från datorn. Projektfilerna (.kalkyl) och dina inställningar i ${DATA_DIR} ligger kvar."
!insertmacro MUI_UNPAGE_CONFIRM
!insertmacro MUI_UNPAGE_INSTFILES

!insertmacro MUI_LANGUAGE "Swedish"

; setup.exe:s versionsinformation. Den fjärde siffran är alltid 0.
VIProductVersion "${VERSION}.0"
VIFileVersion "${VERSION}.0"
VIAddVersionKey /LANG=${LANG_SWEDISH} "ProductName" "Kalkyl"
VIAddVersionKey /LANG=${LANG_SWEDISH} "CompanyName" "${PUBLISHER}"
VIAddVersionKey /LANG=${LANG_SWEDISH} "FileDescription" "Installation av Kalkyl"
VIAddVersionKey /LANG=${LANG_SWEDISH} "FileVersion" "${VERSION}"
VIAddVersionKey /LANG=${LANG_SWEDISH} "ProductVersion" "${VERSION}"
VIAddVersionKey /LANG=${LANG_SWEDISH} "LegalCopyright" "© SWECO Sverige AB. Internt program."
VIAddVersionKey /LANG=${LANG_SWEDISH} "Comments" "Luftledningsberäkningar"
VIAddVersionKey /LANG=${LANG_SWEDISH} "OriginalFilename" "Kalkyl-${VERSION}-setup.exe"

; Läser en tidigare installation ur Uninstall-nyckeln: mappen (för flytt) och versionen.
; Är den installerade versionen nyare frågar guiden först. Tyst läge svarar ja,
; så att IT kan gå tillbaka till en äldre version.
Function .onInit
  SetShellVarContext current
  ReadRegStr $OldDir HKCU "${UNINST_KEY}" "InstallLocation"
  ReadRegStr $OldVersion HKCU "${UNINST_KEY}" "DisplayVersion"
  ${If} $OldVersion != ""
    ${VersionCompare} "${VERSION}" "$OldVersion" $0
    ${If} $0 == 2
      MessageBox MB_YESNO|MB_ICONQUESTION "En nyare version av Kalkyl ($OldVersion) är redan installerad.$\r$\nVill du ersätta den med version ${VERSION}?" /SD IDYES IDYES keep
      Abort
      keep:
    ${EndIf}
  ${EndIf}
FunctionEnd

; Svarar i $0 med 1 om $INSTDIR är Kalkyls datamapp eller ligger i den, annars 0.
; StrCmp jämför utan hänsyn till skiftläge. Funktionen använder $1–$3.
Function IsDataDir
  StrCpy $0 0
  StrLen $1 "${DATA_DIR}"
  StrCpy $2 "$INSTDIR" $1
  ${If} $2 == "${DATA_DIR}"
    StrCpy $3 "$INSTDIR" 1 $1
    ${If} $3 == ""
    ${OrIf} $3 == "\"
      StrCpy $0 1
    ${EndIf}
  ${EndIf}
FunctionEnd

; Mappsidans kontroll: datamappen avvisas med ett besked, och sidan ligger kvar.
Function DirectoryLeave
  Call IsDataDir
  ${If} $0 == 1
    MessageBox MB_OK|MB_ICONEXCLAMATION "Kalkyl kan inte installeras i ${DATA_DIR}.$\r$\nDär ligger dina inställningar och arbetskopior. Välj en annan mapp."
    Abort
  ${EndIf}
FunctionEnd

; Väntar tills kalkyl.exe i mappen överst på stacken inte körs. Provet öppnar exe:n för
; skrivning, vilket Windows nekar så länge programmet körs. Användaren stänger Kalkyl och
; försöker igen. Avbryt, eller tyst läge, avbryter allt med kod 2 innan något ändrats.
!macro CheckNotRunningFunc PREFIX
Function ${PREFIX}CheckNotRunning
  Exch $R0
  Push $R1
  retry:
  ${If} ${FileExists} "$R0\kalkyl.exe"
    ClearErrors
    FileOpen $R1 "$R0\kalkyl.exe" a
    ${If} ${Errors}
      MessageBox MB_RETRYCANCEL|MB_ICONEXCLAMATION "Kalkyl körs. Stäng alla Kalkyl-fönster och klicka på Försök igen." /SD IDCANCEL IDRETRY retry
      Abort
    ${EndIf}
    FileClose $R1
  ${EndIf}
  Pop $R1
  Pop $R0
FunctionEnd
!macroend
!insertmacro CheckNotRunningFunc ""
!insertmacro CheckNotRunningFunc "un."

; Genvägen på skrivbordet: slutsidans kryssruta, och vid uppgradering om den fanns förut.
Function CreateDesktopShortcut
  CreateShortcut "$DESKTOP\Kalkyl.lnk" "$INSTDIR\kalkyl.exe" "" "$INSTDIR\kalkyl.exe" 0 SW_SHOWNORMAL "" "Luftledningsberäkningar"
FunctionEnd

; Installerar programmet. Först kontrolleras mappen och att Kalkyl inte körs. En tidigare
; installation i en annan mapp tas bort med sin egen avinstallerare (flytt). Filtypen
; registreras av kalkyl.exe själv, så att registervärdena bara finns i association.cpp.
Section "Kalkyl" SecKalkyl
  Call IsDataDir
  ${If} $0 == 1
    MessageBox MB_OK|MB_ICONSTOP "Kalkyl kan inte installeras i ${DATA_DIR}." /SD IDOK
    Abort
  ${EndIf}
  Push $INSTDIR
  Call CheckNotRunning

  StrCpy $HadDesktop 0
  ${If} ${FileExists} "$DESKTOP\Kalkyl.lnk"
    StrCpy $HadDesktop 1
  ${EndIf}

  ; Flytt: den gamla installationen avinstalleras tyst och på plats (_?=), sedan städas den.
  ${If} $OldDir != ""
  ${AndIf} $OldDir != $INSTDIR
  ${AndIf} ${FileExists} "$OldDir\avinstallera.exe"
    Push $OldDir
    Call CheckNotRunning
    DetailPrint "Tar bort den tidigare installationen i $OldDir"
    ExecWait '"$OldDir\avinstallera.exe" /S _?=$OldDir'
    Delete "$OldDir\avinstallera.exe"
    RMDir "$OldDir"
  ${EndIf}

  SetOutPath "$INSTDIR"
  File "/oname=kalkyl.exe" "${EXE}"
  File "/oname=assets.bin" "${ASSETS}"
  WriteUninstaller "$INSTDIR\avinstallera.exe"

  CreateShortcut "$SMPROGRAMS\Kalkyl.lnk" "$INSTDIR\kalkyl.exe" "" "$INSTDIR\kalkyl.exe" 0 SW_SHOWNORMAL "" "Luftledningsberäkningar"
  ${If} $HadDesktop == 1
    Call CreateDesktopShortcut
  ${EndIf}

  ClearErrors
  ExecWait '"$INSTDIR\kalkyl.exe" --registrera' $0
  ${If} ${Errors}
  ${OrIf} $0 != 0
    DetailPrint "Filtypen .kalkyl kunde inte registreras ($0). Kalkyl registrerar den själv vid start."
  ${EndIf}
SectionEnd

; Raden i Appar och funktioner skrivs sist, när allt annat lyckats. Storleken (kB) är
; huvudsektionens filer. NoModify och NoRepair döljer Ändra och Reparera.
Section "-Appar och funktioner"
  SectionGetSize ${SecKalkyl} $0
  WriteRegStr   HKCU "${UNINST_KEY}" "DisplayName"          "Kalkyl"
  WriteRegStr   HKCU "${UNINST_KEY}" "DisplayVersion"       "${VERSION}"
  WriteRegStr   HKCU "${UNINST_KEY}" "Publisher"            "${PUBLISHER}"
  WriteRegStr   HKCU "${UNINST_KEY}" "DisplayIcon"          "$INSTDIR\kalkyl.exe,0"
  WriteRegStr   HKCU "${UNINST_KEY}" "InstallLocation"      "$INSTDIR"
  WriteRegStr   HKCU "${UNINST_KEY}" "UninstallString"      '"$INSTDIR\avinstallera.exe"'
  WriteRegStr   HKCU "${UNINST_KEY}" "QuietUninstallString" '"$INSTDIR\avinstallera.exe" /S'
  WriteRegDWORD HKCU "${UNINST_KEY}" "NoModify"             1
  WriteRegDWORD HKCU "${UNINST_KEY}" "NoRepair"             1
  WriteRegDWORD HKCU "${UNINST_KEY}" "EstimatedSize"        $0
SectionEnd

; Avinstallationen arbetar i användarens profil, som installationen.
Function un.onInit
  SetShellVarContext current
FunctionEnd

; Tar bort programmet. Filtypen avregistreras först, av kalkyl.exe själv, som lämnar en
; annan befintlig kopias registrering orörd. Sedan tas bara de egna filerna bort, och mappen
; bara om den är tom (aldrig RMDir /r). Användarens datamapp nämns inte och rörs aldrig.
Section "Uninstall"
  Push $INSTDIR
  Call un.CheckNotRunning
  ${If} ${FileExists} "$INSTDIR\kalkyl.exe"
    ExecWait '"$INSTDIR\kalkyl.exe" --avregistrera'
  ${EndIf}
  Delete "$INSTDIR\kalkyl.exe"
  Delete "$INSTDIR\assets.bin"
  Delete "$INSTDIR\avinstallera.exe"
  RMDir "$INSTDIR"
  Delete "$SMPROGRAMS\Kalkyl.lnk"
  Delete "$DESKTOP\Kalkyl.lnk"
  DeleteRegKey HKCU "${UNINST_KEY}"
SectionEnd
```

### 5.5 `installer/licens.txt` (UTF-8 med BOM och CRLF, ett stycke per rad)
```
Kalkyl – licensvillkor

Kalkyl är ett internt program för luftledningsberäkningar, utvecklat för SWECO Sverige AB. Programmet får bara användas i SWECO Sverige AB:s verksamhet.

Programmet får inte spridas, säljas eller lämnas ut utanför SWECO utan utgivarens tillstånd.

Programmet levereras i befintligt skick. Beräkningsresultat ska granskas enligt SWECO:s rutiner innan de används i ett uppdrag.

Programmet installeras bara för den inloggade användaren. Projektfiler och inställningar ligger kvar om programmet avinstalleras.

Utgivare: SWECO Sverige AB, Erik Holmbom
```

### 5.6 `libs/version`
`version.rc.in`:
```rc
// Versionsresursen för kalkyl.exe, ifylld av CMake från project(VERSION) och KALKYL_PUBLISHER.
// Egenskaper > Information visar den. Aktivitetshanteraren och "Öppna med" visar
// FileDescription som programmets namn. Filen är UTF-8, därav code_page.
#pragma code_page(65001)

1 VERSIONINFO
FILEVERSION     @PROJECT_VERSION_MAJOR@,@PROJECT_VERSION_MINOR@,@PROJECT_VERSION_PATCH@,0
PRODUCTVERSION  @PROJECT_VERSION_MAJOR@,@PROJECT_VERSION_MINOR@,@PROJECT_VERSION_PATCH@,0
FILEFLAGSMASK   0x3f
FILEFLAGS       0x0
FILEOS          0x40004
FILETYPE        0x1
FILESUBTYPE     0x0
BEGIN
  BLOCK "StringFileInfo"
  BEGIN
    BLOCK "041D04B0"
    BEGIN
      VALUE "CompanyName",      "@KALKYL_PUBLISHER@"
      VALUE "FileDescription",  "Kalkyl"
      VALUE "FileVersion",      "@PROJECT_VERSION@"
      VALUE "InternalName",     "kalkyl"
      VALUE "LegalCopyright",   "© SWECO Sverige AB. Internt program."
      VALUE "OriginalFilename", "kalkyl.exe"
      VALUE "ProductName",      "Kalkyl"
      VALUE "ProductVersion",   "@PROJECT_VERSION@"
      VALUE "Comments",         "Luftledningsberäkningar"
    END
  END
  BLOCK "VarFileInfo"
  BEGIN
    VALUE "Translation", 0x041D, 1200
  END
END
```
Till `libs/version/CMakeLists.txt`:
```cmake
# Versionsresursen för kalkyl.exe med samma version och KALKYL_PUBLISHER ur rotens
# CMakeLists.txt. Den kompileras med windres i src/CMakeLists.txt och checkas aldrig in.
configure_file(version.rc.in ${CMAKE_BINARY_DIR}/generated/version/version.rc @ONLY)
```

### 5.7 `src/CMakeLists.txt` (efter `glaze_app_icon`)
```cmake
# Versionsresursen (utgivare, produkt och version) ur libs/version, kompilerad med windres
# som glaze_app_icon gör. Ingen enable_language(RC): då kan målets C++-flaggor
# (-Wall -Wextra) nå windres, som inte känner till dem.
get_filename_component(_bindir "${CMAKE_CXX_COMPILER}" DIRECTORY)
find_program(KALKYL_WINDRES windres HINTS "${_bindir}" REQUIRED)
set(_rc "${CMAKE_BINARY_DIR}/generated/version/version.rc")
set(_rcobj "${CMAKE_CURRENT_BINARY_DIR}/kalkyl_version_rc.o")
add_custom_command(
    OUTPUT "${_rcobj}"
    COMMAND "${KALKYL_WINDRES}" --input-format=rc --output-format=coff -i "${_rc}" -o "${_rcobj}"
    DEPENDS "${_rc}"
    COMMENT "Versionsresursen för kalkyl (windres)"
    VERBATIM)
set_source_files_properties("${_rcobj}" PROPERTIES EXTERNAL_OBJECT TRUE GENERATED TRUE)
target_sources(kalkyl PRIVATE "${_rcobj}")
```

---

## 6. CLAUDE.md

- **`installer/CLAUDE.md` (ny):**
  - Inledning: setup.exe per användare utan administratörsrätt. Bygget genererar ingenting här, och setup.exe hamnar i `build-release/installer/`.
  - Filtabell för `CMakeLists.txt`, `kalkyl.nsi` och `licens.txt`. Ange att licensfilen är UTF-8 med BOM och CRLF.
  - Byggkommandon.
  - Regler:
    - Versionen står aldrig i skriptet.
    - Avinstallationen tar bara bort egna filer och aldrig med `RMDir /r`.
    - Filtypen hanteras av `kalkyl.exe --registrera` och `--avregistrera`.
    - En ny fil i installationen läggs till på tre ställen: `File`, `Delete` i Uninstall och `DEPENDS`.
    - Copyright-texten står också i `libs/version/version.rc.in`.
    - Tyst läge: `/S` och `/D=` sist utan citattecken. Felkoder 0, 1 och 2.
    - Körs i användarens kontext, aldrig som SYSTEM.
    - Från Git Bash krävs `MSYS_NO_PATHCONV=1`.
- **Rotens `CLAUDE.md`:**
  - **Delar:** ny rad `Installation | installer | setup.exe per användare (NSIS)`. Raden Version blir "Programmets version som header och versionsresurs för kalkyl.exe".
  - **Teknik:** NSIS 3 (`makensis` ur `mingw-w64-ucrt-x86_64-nsis`). Utgivaren står i `KALKYL_PUBLISHER` i rotens `CMakeLists.txt`.
  - **Bygga och köra:** presetet `installer` (`cmake --preset release && cmake --build --preset installer`) ger `build-release/installer/Kalkyl-<version>-setup.exe`.
  - **Driftsättning** skrivs om:
    - Normalfallet är setup.exe: per användare i `%LOCALAPPDATA%\Programs\Kalkyl`, utan administratörsrätt, med genväg i Startmenyn, filtypen och en rad i Appar och funktioner.
    - Tyst: `/S [/D=<mapp>]`, och avinstallation med `avinstallera.exe /S`.
    - Avinstallationen tar aldrig bort `%LOCALAPPDATA%\Kalkyl`.
    - Utan installation fungerar det fortfarande att kopiera `kalkyl.exe` och `assets.bin`.
  - **Katalogstruktur:** ny rad `installer | Installationen: NSIS-skriptet, licenstexten och målet installer`.
  - **Data:** avinstallationen rör aldrig systemdatabasen, arbetskopiorna eller projektfilerna.
  - **Versioner:** setup.exe och versionsresursen i kalkyl.exe får versionen ur `project(VERSION)`.
- **`libs/version/CLAUDE.md`:** "Programmets version som header och som versionsresurs för kalkyl.exe". Rad för `version.rc.in`. Raden för CMakeLists blir "genererar `version.h` och `version.rc`".
- **`libs/CLAUDE.md`:** raden för `version/` på samma sätt.
- **`src/CLAUDE.md`:** raden för `CMakeLists.txt` får "versionsresursen (windres ur `libs/version/version.rc.in`)".
- **Samordnaren:** `doc/plans/CLAUDE.md` listar den här planen, och `TODO.md` uppdateras.

---

## 7. Arbetsfördelning

Arbetet är litet. **En Sonnet-agent** kan ta båda paketen i ordningen I1 → V → I2. Med två agenter gäller ägarskapet nedan. Ingen commit görs, och versionen ändras inte.

### Paket I: installationen
**Äger:** `installer/*`, rotens `CMakeLists.txt`, `CMakePresets.json` och rotens `CLAUDE.md`.
1. **I1 (först, några minuter):**
   - Rotens `CMakeLists.txt` enligt 5.1.
   - Presetet enligt 5.2.
   - `installer/CMakeLists.txt` enligt 5.3.
   - Meddela V att `KALKYL_PUBLISHER` finns.
2. **I2:**
   - `kalkyl.nsi` enligt 5.4.
   - `licens.txt` enligt 5.5. Lägg till BOM enligt 3.13.
   - `installer/CLAUDE.md` och rotens `CLAUDE.md` enligt 6.
3. Bygg i `build-release` med `cmake --preset release && cmake --build --preset installer`. Bygget ska gå utan varningar (`-WX`).
4. Gör 8.2 och 8.4 som en egen kontroll. Avinstallera och återställ enligt 8.9–8.10 innan rapporten.

### Paket V: versionsresursen
**Äger:** `libs/version/*`, `libs/CLAUDE.md`, `src/CMakeLists.txt` och `src/CLAUDE.md`.
1. Skriv `version.rc.in` (5.6). Det går före I1.
2. **Efter I1**, i den här ordningen så att konfigureringen aldrig saknar `version.rc`:
   1. `libs/version/CMakeLists.txt`.
   2. `src/CMakeLists.txt` (5.7).
3. Bygg i `build` (presetet `debug`), inte i `build-release`, så att agenterna inte delar Ninja-katalog. Gör 8.1 för debug och testerna.
4. Kontrollera resursen:
   - `windres -i build/src/kalkyl.exe -O rc | grep -A25 VERSIONINFO`
   - PowerShell `(Get-Item build\src\kalkyl.exe).VersionInfo`
5. CLAUDE.md-filerna enligt 6.

### Ordning
1. I1, under tiden V1.
2. I2 och V2–V5 parallellt.
3. Samordnaren bygger allt om i `build-release` och gör avsnitt 8.

---

## 8. Verifiering (samordnaren)

Byggkommandona körs i Git Bash och resten i PowerShell. Installationen sker i den egna profilen, och 8.3 och 8.10 återställer den.

### 8.1 Bygge
```sh
cmake --preset release && cmake --build --preset release 2>&1 | grep -i warning   # tomt
cmake --build --preset installer                                                  # -WX: varningar stoppar bygget
ls -la build-release/installer/Kalkyl-0.1.0-setup.exe
cmake --preset debug && cmake --build --preset debug 2>&1 | grep -i warning       # tomt
cmake --preset test && cmake --build --preset test && ctest --preset test         # alla tester går igenom
```

### 8.2 Ingen administratörsrätt och versionsinformation
```sh
grep -ao 'requestedExecutionLevel level="[a-zA-Z]*"' build-release/installer/Kalkyl-0.1.0-setup.exe   # level="asInvoker"
```
```powershell
foreach ($f in 'build-release\installer\Kalkyl-0.1.0-setup.exe', 'build-release\src\kalkyl.exe') {
  (Get-Item $f).VersionInfo | Format-List CompanyName, ProductName, FileDescription, FileVersion, ProductVersion, LegalCopyright
}
```
- Väntat:
  - CompanyName `SWECO Sverige AB, Erik Holmbom`
  - ProductVersion `0.1.0`
  - FileDescription `Installation av Kalkyl` respektive `Kalkyl`
  - LegalCopyright med ©
- Utforskaren visar ingen UAC-sköld på setup.exe.
- Utforskarens flik Information visar inte CompanyName. Kontrollera den därför med PowerShell.
- Efter 8.4 kontrolleras samma sak för `avinstallera.exe`. Saknas versionsinformationen där är det accepterat (3.10).

### 8.3 Förberedelse och återställningspunkt
```powershell
$s = 'C:\Temp\kalkyl-installtest'; New-Item -ItemType Directory -Force $s | Out-Null
reg export 'HKCU\Software\Classes\.kalkyl' "$s\ext.reg" /y              # fel om nyckeln saknas, det är ok
reg export 'HKCU\Software\Classes\Kalkyl.Projekt' "$s\progid.reg" /y
Test-Path "$env:LOCALAPPDATA\Kalkyl\system.db"                          # True; annars starta build-release\src\kalkyl.exe en gång och stäng
Test-Path 'HKCU:\Software\Microsoft\Windows\CurrentVersion\Uninstall\Kalkyl'   # False
$setup = (Resolve-Path 'build-release\installer\Kalkyl-0.1.0-setup.exe').Path
$dir   = "$env:LOCALAPPDATA\Programs\Kalkyl"
$key   = 'HKCU:\Software\Microsoft\Windows\CurrentVersion\Uninstall\Kalkyl'
$start = Join-Path ([Environment]::GetFolderPath('Programs')) 'Kalkyl.lnk'
$desk  = Join-Path ([Environment]::GetFolderPath('Desktop')) 'Kalkyl.lnk'
```

### 8.4 Tyst installation
```powershell
(Start-Process $setup -ArgumentList '/S' -Wait -PassThru).ExitCode      # 0
Get-ChildItem $dir | Select-Object -ExpandProperty Name                 # exakt avinstallera.exe, assets.bin, kalkyl.exe
(New-Object -ComObject WScript.Shell).CreateShortcut($start).TargetPath # $dir\kalkyl.exe
Test-Path $desk                                                         # False (tyst läge skapar ingen)
Get-ItemProperty $key | Format-List DisplayName, DisplayVersion, Publisher, DisplayIcon, InstallLocation, UninstallString, QuietUninstallString, NoModify, NoRepair, EstimatedSize
reg query 'HKCU\Software\Classes\.kalkyl' /ve                           # Kalkyl.Projekt
reg query 'HKCU\Software\Classes\Kalkyl.Projekt\shell\open\command' /ve # "<dir>\kalkyl.exe" "%1"
Test-Path 'HKLM:\Software\Microsoft\Windows\CurrentVersion\Uninstall\Kalkyl'   # False
```
- EstimatedSize är ungefär 12 600 (kB).

### 8.5 Ominstallation av samma version
- Kör `/S` igen. Koden ska vara 0, de tre filerna finnas kvar och nyckeln vara oförändrad.

### 8.6 Kalkyl körs
```powershell
$env:KALKYL_DATA_DIR = 'C:\Temp\kalkyl-installtest\data'                # rör inte den riktiga system.db
$p = Start-Process "$dir\kalkyl.exe" -PassThru; Start-Sleep 2
(Start-Process $setup -ArgumentList '/S' -Wait -PassThru).ExitCode      # 2
Start-Process "$dir\avinstallera.exe" -ArgumentList '/S' -Wait; Start-Sleep 5
Test-Path "$dir\kalkyl.exe"                                             # True: inget borttaget
$p | Stop-Process; Remove-Item Env:KALKYL_DATA_DIR
```

### 8.7 Datamappen avvisas
```powershell
(Start-Process $setup -ArgumentList '/S', "/D=$env:LOCALAPPDATA\Kalkyl" -Wait -PassThru).ExitCode   # 2
Test-Path "$env:LOCALAPPDATA\Kalkyl\kalkyl.exe"                         # False
```

### 8.8 Flytt med /D= och tillbaka
```powershell
(Start-Process $setup -ArgumentList '/S', '/D=C:\Temp\Kalkyl-flytt' -Wait -PassThru).ExitCode   # 0
Test-Path $dir                                                          # False: den gamla mappen är borta
Get-ChildItem C:\Temp\Kalkyl-flytt | Select-Object -ExpandProperty Name # tre filer
(Get-ItemProperty $key).InstallLocation                                 # C:\Temp\Kalkyl-flytt
reg query 'HKCU\Software\Classes\Kalkyl.Projekt\shell\open\command' /ve # pekar på C:\Temp\Kalkyl-flytt
(Start-Process $setup -ArgumentList '/S', "/D=$dir" -Wait -PassThru).ExitCode   # 0, tillbaka
Test-Path C:\Temp\Kalkyl-flytt                                          # False
```

### 8.9 Avinstallation och användardata
```powershell
New-Item "$dir\egen.txt" -ItemType File | Out-Null                      # en främmande fil i mappen
$hash = (Get-FileHash "$env:LOCALAPPDATA\Kalkyl\system.db").Hash
Start-Process "$dir\avinstallera.exe" -ArgumentList '/S' -Wait
# Avinstalleraren fortsätter som kopia i %TEMP%: vänta högst 30 s på att den blir klar.
for ($i = 0; ($i -lt 60) -and (Test-Path "$dir\avinstallera.exe"); $i++) { Start-Sleep -Milliseconds 500 }
Get-ChildItem $dir | Select-Object -ExpandProperty Name                 # bara egen.txt: mappen fanns kvar, inget RMDir /r
Remove-Item "$dir\egen.txt"; Remove-Item $dir                           # städa för hand
```
Installera sedan igen med `/S` och avinstallera utan främmande fil:
```powershell
(Start-Process $setup -ArgumentList '/S' -Wait -PassThru).ExitCode     # 0
$hash = (Get-FileHash "$env:LOCALAPPDATA\Kalkyl\system.db").Hash
$q = (Get-ItemProperty $key).QuietUninstallString                      # det IT kör
Start-Process cmd.exe -ArgumentList '/c', $q -Wait
for ($i = 0; ($i -lt 60) -and (Test-Path $dir); $i++) { Start-Sleep -Milliseconds 500 }
Test-Path $dir                                                          # False: tom mapp borttagen
Test-Path $start; Test-Path $desk; Test-Path $key                       # False, False, False
reg query 'HKCU\Software\Classes\Kalkyl.Projekt'                        # finns inte
(Get-FileHash "$env:LOCALAPPDATA\Kalkyl\system.db").Hash -eq $hash      # True
Test-Path "$env:LOCALAPPDATA\Kalkyl\arbete"                             # True
```

### 8.10 Återställning
```powershell
if (Test-Path "$s\ext.reg")    { reg import "$s\ext.reg" }
if (Test-Path "$s\progid.reg") { reg import "$s\progid.reg" }
Remove-Item -Recurse -Force 'C:\Temp\kalkyl-installtest', 'C:\Temp\Kalkyl-flytt' -ErrorAction SilentlyContinue
Test-Path $dir; Test-Path $start; Test-Path $key                        # False, False, False
```
- Utan export räcker det att köra `build-release\src\kalkyl.exe --registrera`, eller att starta Release-bygget, som registrerar filtypen själv när den saknas.

### 8.11 Granskning mot reglerna
- Kommentarer på 2–5 rader också i `.nsi`, `.rc.in` och CMake.
- En `CLAUDE.md` i `installer/`.
- Ingen version i skriptet.
- Ingen `RMDir /r`, och ingen sökväg till `%LOCALAPPDATA%\Kalkyl` i någon borttagning.

---

## 9. Användartest (efter granskningen, enligt undantaget överst)

1. **Starta setup.exe** med dubbelklick.
   - Ingen UAC-fråga visas.
   - setup.exe och guidens fönster har tornikonen.
   - Laddades filen ned kan SmartScreen varna, eftersom filen inte är signerad (avsnitt 10).
2. **Välkomst och licens.** Välkomsttexten är på svenska. Licenstexten visar å, ä och ö rätt. Klicka "Jag godkänner".
3. **Mapp.** Förvalet är `…\AppData\Local\Programs\Kalkyl`. Välj `…\AppData\Local\Kalkyl`: guiden visar ett besked och stannar kvar på sidan.
4. **Slut.** "Starta Kalkyl" är ikryssad och "Skapa en genväg på skrivbordet" är inte ikryssad. Kryssa i båda: Kalkyl startar och genvägen skapas.
5. **Startmenyn** har Kalkyl med tornikonen.
6. **Inställningar > Appar > Installerade appar** visar Kalkyl med version 0.1.0, utgivaren `SWECO Sverige AB, Erik Holmbom`, storlek och ikon.
7. **Dubbelklicka på en `.kalkyl`.** Den installerade Kalkyl öppnar projektet.
8. **Kör setup.exe igen** med Kalkyl öppet. Beskedet "Kalkyl körs" visas. Stäng Kalkyl och klicka Försök igen: installationen blir klar, och skrivbordsgenvägen finns kvar.
9. **Egenskaper för `kalkyl.exe` > Information** visar filbeskrivningen Kalkyl, produktnamn, version 0.1.0 och copyright.
10. **Avinstallera från Appar.**
    - Bekräftelsesidan säger att projekt och inställningar ligger kvar.
    - Efteråt är genvägarna, mappen och raden i Appar borta.
    - `%LOCALAPPDATA%\Kalkyl\system.db` finns kvar, och senaste-listan är intakt i nästa Kalkyl.

---

## 10. Öppna punkter och kända gränser

- **Kodsignering ingår inte.**
  - Den kräver SWECO:s certifikat (Authenticode, gärna Azure Trusted Signing eller EV).
  - `kalkyl.exe` signeras före paketeringen och setup.exe efteråt. NSIS kan signera avinstalleraren under bygget med `!uninstfinalize` och setup.exe med `!finalize`.
  - Utan signering varnar SmartScreen för nedladdade filer, och Defender kan ibland ge falsklarm för NSIS-installationer.
- **AppLocker och WDAC.** Spärrar SWECO:s IT program i AppData, t.ex. med AppLockers förvalda regler, startar Kalkyl inte i `%LOCALAPPDATA%\Programs`. Då behövs signering och en utgivarregel, eller en installation för alla användare. Det bör kontrolleras med IT före utrullning.
- **IT-verktyg:** installationen måste köras i användarens kontext, aldrig som SYSTEM.
- **Avinstallerarens felkod.** Avinstalleraren startar om sig själv från `%TEMP%`, så processen IT väntar på avslutas direkt med 0. Vill IT vänta på riktigt: `avinstallera.exe /S _?=<mapp>`. Då blir `avinstallera.exe` och mappen kvar och tas bort för hand.
- **Raderad mapp.** Har användaren tagit bort installationsmappen för hand finns raden i Appar kvar. setup.exe lagar den, och Windows erbjuder sig att ta bort raden när avinstalleraren saknas.
- **Skrivbordsgenvägen** `Kalkyl.lnk` tas bort vid avinstallation även om användaren själv skapat den och den pekar på en annan kopia. NSIS kan inte läsa genvägens mål utan plugin.
- **Versionsinformationen i `avinstallera.exe`** beror på NSIS (3.10).
- **setup.exe är 64-bitars** (amd64-stubbar i MSYS2). Det fungerar på alla Windows 11-datorer, och på ARM64 via emulering.
- **Grunder 2.12 gäller fortfarande:** har användaren själv valt ett annat program för `.kalkyl` (UserChoice) vinner Windows val.
