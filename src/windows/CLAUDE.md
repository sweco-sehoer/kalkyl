# src/windows

Programmets fönster och dialoger i GlazeUI:s tre-filsmodell (`<namn>.h`, `<namn>.glazeui`, `<namn>.cpp`).
Varje fönster läggs till i `glaze_add_window` i `src/CMakeLists.txt`.
Fönsterklasserna ligger utanför namespace, för GlazeBuilder läser bara okvalificerade klassnamn.
Utseendet sätts i varje toppfönsters `onLoad` med `kalkyl::appearance::apply`.

| Fil | Innehåll |
|---|---|
| `project_picker.h` | Klassen `ProjectPicker`: projektväljaren. Medlemmarna efter regionen: systemdatabasen, senaste-listan, det öppnade projektet, filväljarna och två `Worker` sist |
| `project_picker.glazeui` | Genererad layout: lista över senaste projekt, knapparna Nytt, Öppna annat, Öppna och Ta bort ur listan, tom-text, statusrad, lägesknapp i namnlisten och en timer som visar en dold väljare |
| `project_picker.cpp` | Egen kod: `onLoad`, listan och filkontrollen i bakgrunden, öppning och skapande i bakgrunden (`finishOpen` väljer fråga eller felmeddelande efter felkoden), `takeProject` |
| `main_window.h` | Klassen `MainWindow`; regionen GlazeBuilder ägs av editorn. `setContext` och `outcome` är gränssnittet mot `main` |
| `main_window.glazeui` | Genererad layout: titel, storlek, programikon (stor), menyerna (Arkiv med Stäng projekt och Avsluta, Hjälp) i namnlisten, statusrad och lägesknappen `themeBtn` till höger i namnlisten |
| `main_window.cpp` | Egen kod: `onLoad` (utseende, titel med projektnamn, sökväg och skrivskyddat i statusraden), lägesknappen via `kalkyl::appearance::cycle`, menyernas handlers |

- Ikonerna `glaze::img("app", "kalkyl")` och `glaze::img("app32", "kalkyl")` finns i `res/assets.bin`. Saknas filen visas GlazeUI:s förvalda ikon.
