# libs/core

Felmodellen som alla bibliotek delar. Biblioteket heter `kalkyl_core` (INTERFACE, bara header).

| Fil | Innehåll |
|---|---|
| `CMakeLists.txt` | Definierar `kalkyl_core` och exporterar `libs/` som include-katalog |
| `result.h` | `ErrorCode`, `Error` och `Result<T>` (med en specialisering för `Result<void>`) |

- `Error` har en kod, en svensk text till användaren (`message`, med sökväg), en teknisk text till loggen
  (`detail`) och den ursprungliga SQLite- eller Win32-koden (`nativeCode`).
- Fönstren väljer åtgärd efter `code` och visar `message`.
- Inga undantag används för fel som kan förväntas. Se planen `doc/plans/2026-11-20_Grunder.md`, avsnitt 2.9.
