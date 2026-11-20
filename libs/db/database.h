#pragma once
#include <cstdint>
#include <functional>
#include <string>
#include <string_view>

#include "core/result.h"

struct sqlite3;
struct sqlite3_stmt;

namespace kalkyl::db {

enum class OpenMode { Existing, CreateNew, OpenOrCreate };   // CreateNew: AlreadyExists om filen finns

// Hur en databasfil öppnas: skapande, skrivskydd, exklusiv låsning och väntetid vid upptaget.
struct OpenOptions {
    OpenMode mode = OpenMode::Existing;
    bool readOnly = false;          // SQLITE_OPEN_READONLY
    bool exclusive = false;         // PRAGMA locking_mode=EXCLUSIVE (arbetskopian)
    int  busyTimeoutMs = 5000;
};

class Database;

// En förberedd sats (RAII runt sqlite3_stmt). Får inte överleva sin Database.
// Bind-anropen ger tillbaka satsen så att de kan kedjas. Index är 1-baserade, kolumner 0-baserade.
class Statement {
public:
    Statement(Statement&&) noexcept; Statement& operator=(Statement&&) noexcept; ~Statement();
    Statement& bind(int index, std::string_view text);   // 1-baserat, SQLITE_TRANSIENT
    Statement& bind(int index, std::int64_t value);
    Statement& bind(int index, double value);
    Statement& bindNull(int index);
    core::Result<bool> step();        // true = en rad finns, false = klar
    core::Result<void> run();         // stega till slutet (INSERT/UPDATE/DELETE)
    std::string  text(int column) const;      // 0-baserat; NULL ger ""
    std::int64_t integer(int column) const;
    double       real(int column) const;
    bool         isNull(int column) const;
    void reset();                     // sqlite3_reset + clear_bindings
private:
    friend class Database;
    Statement(sqlite3* db, sqlite3_stmt* stmt) : db_(db), stmt_(stmt) {}
    sqlite3*      db_ = nullptr;
    sqlite3_stmt* stmt_ = nullptr;
};

// En anslutning (RAII runt sqlite3*). Sätter vid öppning extended result codes,
// busy_timeout, foreign_keys=ON, journal_mode=DELETE, synchronous=FULL och vid behov
// locking_mode=EXCLUSIVE. Kräver sqlite3_threadsafe() != 0: en anslutning får flyttas
// mellan trådar men aldrig användas av två samtidigt.
class Database {
public:
    Database() = default;                                // tomt läge, som efter en flytt
    static core::Result<Database> open(const std::string& utf8Path, const OpenOptions& options);
    static core::Result<Database> openMemory();
    Database(Database&&) noexcept; Database& operator=(Database&&) noexcept; ~Database();
    const std::string& path() const;                     // "" för en databas i minnet
    core::Result<void> exec(std::string_view sql);       // en eller flera satser, utan parametrar
    core::Result<Statement> prepare(std::string_view sql);
    // BEGIN IMMEDIATE; body(); COMMIT, eller ROLLBACK om body ger fel.
    core::Result<void> transaction(const std::function<core::Result<void>()>& body);
    core::Result<std::int64_t> pragmaInt(std::string_view name);   // "user_version", "application_id"
    core::Result<void> vacuumInto(const std::string& utf8Target);  // ögonblicksbild; målet får inte finnas
    core::Result<void> quickCheck();                     // PRAGMA quick_check(1) == "ok", annars NotADatabase
    void close();
private:
    sqlite3*    db_ = nullptr;
    std::string path_;
};
// SQLite-koder → ErrorCode: CANTOPEN → NotFound/AccessDenied/Io, NOTADB/CORRUPT → NotADatabase,
// BUSY/LOCKED → Busy, READONLY/PERM → AccessDenied, INTERRUPT → Cancelled, övrigt → Sql.

} // namespace kalkyl::db
