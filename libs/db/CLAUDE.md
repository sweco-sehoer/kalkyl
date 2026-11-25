# libs/db

SQLite-lagret. Biblioteket heter `kalkyl_db` (STATIC). SQLite tas statiskt ur MSYS2 (`libsqlite3.a`)
via `find_library` före `find_package`, så att exe-filen inte behöver någon DLL.

| Fil | Innehåll |
|---|---|
| `CMakeLists.txt` | Biblioteket och SQLite (testet `db` ligger i `tests/`) |
| `database.h`, `database.cpp` | `Database` (RAII runt `sqlite3*`), `Statement` och `OpenOptions` |
| `migrate.h`, `migrate.cpp` | `Schema` och `migrate`: id- och versionskontroll samt migreringssteg |
- `Database::open` sätter `journal_mode=DELETE`, `synchronous=FULL`, `foreign_keys=ON`, `busy_timeout`
  och vid behov `locking_mode=EXCLUSIVE`. En anslutning får flyttas mellan trådar men aldrig användas av två samtidigt.
- Alla fel är `core::Result` med kod enligt tabellen i `database.h`. Inga SQLite-typer syns i headern
  utöver framåtdeklarationerna `sqlite3` och `sqlite3_stmt`.
- `migrate` kör de steg som saknas, tillsammans med `application_id` och `user_version`, i en enda
  `BEGIN IMMEDIATE`-transaktion. Stegen är SQL-literaler med statisk livstid.
- `Database` har en publik förvald konstruktor som ger ett tomt flyttat-från-läge. `ProjectDb` kräver den.
- Anslutningen får aldrig användas mot en fil på en nätverksresurs. Projektfilen arbetas på i en lokal kopia (se `projectdb/`).
