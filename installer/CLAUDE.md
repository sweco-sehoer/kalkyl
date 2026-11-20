# installer

setup.exe för Kalkyl: installation per användare i `%LOCALAPPDATA%\Programs\Kalkyl` utan administratörsrätt.
Bygget genererar ingenting här. setup.exe hamnar i `build-release/installer/` och checkas aldrig in.

| Fil | Innehåll |
|---|---|
| `CMakeLists.txt` | Målet `installer` (makensis). Ingår inte i ALL och finns bara i Release och när makensis hittas |
| `kalkyl.nsi` | NSIS-skriptet (MUI2, svenska): sidor, installation, uppgradering och flytt, avinstallation |
| `licens.txt` | Licenstexten på licenssidan. UTF-8 med BOM och CRLF |

## Bygga

```sh
cmake --preset release && cmake --build --preset installer
```

Ger `build-release/installer/Kalkyl-<version>-setup.exe`.

## Regler

- Versionen står aldrig i skriptet. Den kommer ur `project(VERSION)` via `-DVERSION`, och utgivaren ur `KALKYL_PUBLISHER`.
- Avinstallationen tar bara bort egna filer och aldrig med `RMDir /r`. `%LOCALAPPDATA%\Kalkyl` rörs aldrig.
- Filtypen hanteras av `kalkyl.exe --registrera` och `--avregistrera`, inte av NSIS-nycklar.
- En ny fil i installationen läggs till på tre ställen: `File`, `Delete` i Uninstall och `DEPENDS` i `CMakeLists.txt`.
- Copyright-texten står också i `libs/version/version.rc.in`.
- Tyst läge: `/S` och `/D=` sist och utan citattecken. Felkoder: 0 klar, 1 avbruten av användaren, 2 avbruten av skriptet.
- Körs i användarens kontext, aldrig som SYSTEM.
- Från Git Bash krävs `MSYS_NO_PATHCONV=1`, annars görs `/S` om till en sökväg. PowerShell har inte problemet.
