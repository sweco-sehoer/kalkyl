# libs/appearance

Programmets utseende och lägesknappen, delade av båda toppfönstren. Biblioteket heter `kalkyl_appearance`
och ligger i namnrymden `kalkyl::appearance`. Det länkar `glaze` och `kalkyl_systemdb`.

| Fil | Innehåll |
|---|---|
| `CMakeLists.txt` | Statiskt bibliotek `kalkyl_appearance` |
| `appearance.h` | `apply`, `cycle`, `syncButton` samt `toGlaze`/`fromGlaze` |
| `appearance.cpp` | Implementationen: Slate med temat Windows, läget ur systemdatabasen, lägesknappens glyfer |

- `apply(window, sys)` anropas i varje toppfönsters `onLoad`. Utseendet är app-globalt.
- `cycle(window, button, sys)` är lägesknappens klick: System, Mörkt, Ljust. Läget sparas i systemdatabasen.
