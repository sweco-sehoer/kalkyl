# res

Programikonen och texter som byggs in i programmet. Ikonen visar tre mörka fackverksstolpar i perspektiv mot en kvällshimmel över grön mark.
Ledningen vinklar vid mittstolpen, och varje stolpes sex linor går till samma fästpunkt på nästa stolpe.
Bygget genererar ingenting här. Filerna nedan checkas in och kopieras eller läses bara av bygget.

| Fil | Innehåll | Checkas in |
|---|---|---|
| `linberakning_excel.md` | PM:et om linberäkningen och felen i Excel-arket (D1–D10, avsteg och skäl). Byggs in i `kalkyl.exe` av `src/CMakeLists.txt` och visas med i-knappen i formuläret Linberäkning. Får inte innehålla `)md"` eller `@` | ja |
| `rita_ikon.ps1` | Ikonens källa. Ritar stolparna med System.Drawing (GDI+) och skriver `ikon-256.png` och `ikon-32.png`. 32 px utelämnar fackverket och tonar linorna | ja |
| `assets.txt` | Manifest för `glazeassets pack`: listorna `app` (256 px) och `app32` (32 px), nyckeln `kalkyl` | ja |
| `assets.bin` | Bilderna som fönstret och `appIcon` läser. Kopieras till `kalkyl.exe`:s katalog | ja |
| `kalkyl.ico` | Programfilens ikon (windres via `glaze_app_icon`) och ikon för `.kalkyl`-filer | ja |
| `ikon-256.png`, `ikon-32.png` | Mellanbilder ur skriptet | nej (`.gitignore`) |

## Göra om ikonen

Kommandona körs från projektets rot i Git Bash. Skriptet kräver bara Windows PowerShell 5.1.

```sh
cmake --preset tools && cmake --build --preset tools     # en gång, ger glazeassets
powershell -NoProfile -ExecutionPolicy Bypass -File res/rita_ikon.ps1
G=$(find build-tools -name glazeassets.exe | head -1)
$G pack res/assets.txt res/assets.bin
$G verify res/assets.bin
$G icon res/assets.bin app:kalkyl --small app32:kalkyl build-tools/ikon kalkyl
cp build-tools/ikon/kalkyl.ico res/kalkyl.ico
```

- Blockeras skript: `powershell -NoProfile -Command "Invoke-Expression (Get-Content -Raw res/rita_ikon.ps1)"`.
- Ändra ikonen genom att redigera `rita_ikon.ps1` och köra kommandona om. Checka in `assets.bin` och `kalkyl.ico`.
- Skriptet skriver i sin egen katalog och ändrar inga andra filer.
- GlazeBuilder och `glaze-render` letar `assets.bin` uppåt från fönstrets `.h`-fil, så förhandsgranskningen visar GlazeUI:s förvalda ikon.
