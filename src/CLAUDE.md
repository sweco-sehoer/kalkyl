# src

Programmet `kalkyl`: `main.cpp` och fönstren. Stödbibliotek ligger i `libs/`.

| Fil/katalog | Innehåll |
|---|---|
| `CMakeLists.txt` | Bygger `kalkyl.exe`: `main.cpp` + fönstren via `glaze_add_window`, länkar GlazeUI, `glaze_threads_ui` och `libs/` (bland dem `kalkyl_ancalc`, `kalkyl_automode`, `kalkyl_poleview`, `kalkyl_linecalc`, `kalkyl_sheetgrid`, `kalkyl_polegrid`, `kalkyl_poletable`, `kalkyl_xlsx`, `kalkyl_poleimport`, `kalkyl_tabletext` och `kalkyl_conductor`). PM:et `res/linberakning_excel.md` som strängkonstant i `<build>/generated/res/linberakning_excel.h` (CMake konfigureras om när md-filen ändras). Programfilens ikon (`glaze_app_icon` ur `res/kalkyl.ico`), versionsresursen (windres ur `libs/version/version.rc.in`) och kopieringen av `res/assets.bin` till exe:ns katalog |
| `main.cpp` | `main()`: argument (UTF-8) och flaggorna `--registrera`/`--avregistrera`, systemdatabasen med reserv i minnet, städning av arbetskopior, automatisk filassociation i Release och slingan mellan projektväljaren och huvudfönstret |
| `windows/` | Alla fönster och dialoger, tre filer per fönster |

- En projektfil som argument öppnas direkt av en dold väljare. Väljaren visas först om öppningen drar ut på tiden eller misslyckas.
- Hjälpfunktionerna för argument och systemdatabas ligger i en anonym namnrymd i `main.cpp`.
