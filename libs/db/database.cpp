#include "db/database.h"

#include <sqlite3.h>

#include <utility>

#include "platform/files.h"

namespace kalkyl::db {

namespace {

// SQLite-resultatkoden till felets slag. Den primära koden (de lägsta 8 bitarna) avgör,
// eftersom utökade koder är påslagna. CANTOPEN hanteras av open() som kan fråga filsystemet.
core::ErrorCode codeFor(int rc)
{
    switch (rc & 0xFF) {
    case SQLITE_NOTADB:
    case SQLITE_CORRUPT:
        return core::ErrorCode::NotADatabase;
    case SQLITE_BUSY:
    case SQLITE_LOCKED:
        return core::ErrorCode::Busy;
    case SQLITE_READONLY:
    case SQLITE_PERM:
        return core::ErrorCode::AccessDenied;
    case SQLITE_INTERRUPT:
        return core::ErrorCode::Cancelled;
    case SQLITE_CANTOPEN:
        return core::ErrorCode::Io;
    default:
        return core::ErrorCode::Sql;
    }
}

// Användartexten för ett databasfel efter koden. Sökvägen tas med när den är känd.
std::string messageFor(core::ErrorCode code, const std::string& path, const std::string& detail)
{
    const std::string nl = "\n";
    const std::string where = path.empty() ? std::string() : nl + path;
    switch (code) {
    case core::ErrorCode::NotADatabase:
        return "Filen är inte en databas eller är skadad:" + where;
    case core::ErrorCode::Busy:
        return "Databasen används av någon annan:" + where;
    case core::ErrorCode::AccessDenied:
        return "Databasen får inte skrivas:" + where;
    case core::ErrorCode::Cancelled:
        return "Avbrutet.";
    case core::ErrorCode::Io:
        return "Databasen kunde inte öppnas:" + where;
    default:
        return "Databasfel: " + detail;
    }
}

// Bygger ett Error ur en SQLite-resultatkod. detail får SQLite:s egen text, och
// native koden behåller den utökade resultatkoden för loggen.
core::Error makeError(sqlite3* db, int rc, const std::string& path)
{
    core::Error e;
    e.code = codeFor(rc);
    e.nativeCode = rc;
    e.detail = db ? sqlite3_errmsg(db) : sqlite3_errstr(rc);
    e.message = messageFor(e.code, path, e.detail);
    return e;
}

// Kör en sats utan parametrar och utan resultat. Används för pragmas vid öppning.
int execRaw(sqlite3* db, const char* sql)
{
    return sqlite3_exec(db, sql, nullptr, nullptr, nullptr);
}

} // namespace

// ---- Statement ----

// Flytten tömmer källan så att bara en sats avslutar sqlite3_stmt.
Statement::Statement(Statement&& other) noexcept
    : db_(std::exchange(other.db_, nullptr)), stmt_(std::exchange(other.stmt_, nullptr))
{
}

// Flytttilldelning: den egna satsen avslutas innan den andras tas över.
Statement& Statement::operator=(Statement&& other) noexcept
{
    if (this != &other) {
        sqlite3_finalize(stmt_);
        db_ = std::exchange(other.db_, nullptr);
        stmt_ = std::exchange(other.stmt_, nullptr);
    }
    return *this;
}

// Avslutar satsen. sqlite3_finalize på en tom pekare är ofarligt.
Statement::~Statement()
{
    sqlite3_finalize(stmt_);
}

// Binder en text. SQLITE_TRANSIENT gör att SQLite tar en egen kopia. En tom text får en giltig
// pekare, eftersom en nollpekare skulle bindas som NULL.
Statement& Statement::bind(int index, std::string_view text)
{
    sqlite3_bind_text(stmt_, index, text.empty() ? "" : text.data(), static_cast<int>(text.size()),
                      SQLITE_TRANSIENT);
    return *this;
}

// Binder ett heltal.
Statement& Statement::bind(int index, std::int64_t value)
{
    sqlite3_bind_int64(stmt_, index, value);
    return *this;
}

// Binder ett flyttal.
Statement& Statement::bind(int index, double value)
{
    sqlite3_bind_double(stmt_, index, value);
    return *this;
}

// Binder NULL.
Statement& Statement::bindNull(int index)
{
    sqlite3_bind_null(stmt_, index);
    return *this;
}

// Tar ett steg. true betyder att en rad finns att läsa, false att satsen är klar.
core::Result<bool> Statement::step()
{
    const int rc = sqlite3_step(stmt_);
    if (rc == SQLITE_ROW)
        return true;
    if (rc == SQLITE_DONE)
        return false;
    return makeError(db_, rc, {});
}

// Stegar till slutet. För INSERT, UPDATE och DELETE, där raderna inte läses.
core::Result<void> Statement::run()
{
    for (;;) {
        auto r = step();
        if (!r)
            return r.error();
        if (!r.value())
            return {};
    }
}

// Texten i kolumnen. NULL ger en tom sträng.
std::string Statement::text(int column) const
{
    const unsigned char* p = sqlite3_column_text(stmt_, column);
    if (!p)
        return {};
    return std::string(reinterpret_cast<const char*>(p), static_cast<size_t>(sqlite3_column_bytes(stmt_, column)));
}

// Heltalet i kolumnen.
std::int64_t Statement::integer(int column) const
{
    return sqlite3_column_int64(stmt_, column);
}

// Flyttalet i kolumnen.
double Statement::real(int column) const
{
    return sqlite3_column_double(stmt_, column);
}

// Sant om kolumnen är NULL.
bool Statement::isNull(int column) const
{
    return sqlite3_column_type(stmt_, column) == SQLITE_NULL;
}

// Gör satsen redo för nya bindningar och en ny körning.
void Statement::reset()
{
    sqlite3_reset(stmt_);
    sqlite3_clear_bindings(stmt_);
}

// ---- Database ----

// Öppnar en fil. Först kontrolleras att SQLite är trådsäkert, sedan att filen finns (eller inte
// finns vid CreateNew), därefter sätts anslutningens pragmas enligt planen.
core::Result<Database> Database::open(const std::string& utf8Path, const OpenOptions& options)
{
    if (sqlite3_threadsafe() == 0) {
        core::Error e;
        e.code = core::ErrorCode::Sql;
        e.message = "Databasfel: SQLite är byggt utan trådstöd.";
        return e;
    }
    if (options.mode == OpenMode::CreateNew && platform::stamp(utf8Path).ok()) {
        core::Error e;
        e.code = core::ErrorCode::AlreadyExists;
        e.message = "Det finns redan en fil med namnet:" "\n" + utf8Path;
        return e;
    }
    int flags = SQLITE_OPEN_FULLMUTEX;
    if (options.readOnly)
        flags |= SQLITE_OPEN_READONLY;
    else
        flags |= SQLITE_OPEN_READWRITE | (options.mode == OpenMode::Existing ? 0 : SQLITE_OPEN_CREATE);

    sqlite3* raw = nullptr;
    int rc = sqlite3_open_v2(utf8Path.c_str(), &raw, flags, nullptr);
    if (rc != SQLITE_OK) {
        core::Error e = makeError(raw, rc, utf8Path);
        sqlite3_close_v2(raw);
        if ((rc & 0xFF) == SQLITE_CANTOPEN) {
            // Filsystemet vet varför filen inte gick att öppna.
            auto s = platform::stamp(utf8Path);
            if (!s)
                e = s.error();
        }
        return e;
    }
    Database db;
    db.db_ = raw;
    db.path_ = utf8Path;
    sqlite3_extended_result_codes(raw, 1);
    sqlite3_busy_timeout(raw, options.busyTimeoutMs);

    // Anslutningens pragmas. journal_mode kräver en skrivbar fil, och på en textfil ger den
    // NOTADB redan här, vilket är ett riktigt svar.
    rc = execRaw(raw, "PRAGMA foreign_keys=ON");
    if (rc == SQLITE_OK && !options.readOnly) {
        rc = execRaw(raw, "PRAGMA journal_mode=DELETE");
        if (rc == SQLITE_OK)
            rc = execRaw(raw, "PRAGMA synchronous=FULL");
    }
    if (rc == SQLITE_OK && options.exclusive)
        rc = execRaw(raw, "PRAGMA locking_mode=EXCLUSIVE");
    if (rc != SQLITE_OK)
        return makeError(raw, rc, utf8Path);
    return db;
}

// En databas i minnet, för reserver och tester. Samma pragmas som en fil, men utan sökväg.
core::Result<Database> Database::openMemory()
{
    sqlite3* raw = nullptr;
    int rc = sqlite3_open_v2(":memory:", &raw, SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE | SQLITE_OPEN_FULLMUTEX,
                             nullptr);
    if (rc != SQLITE_OK) {
        core::Error e = makeError(raw, rc, {});
        sqlite3_close_v2(raw);
        return e;
    }
    Database db;
    db.db_ = raw;
    sqlite3_extended_result_codes(raw, 1);
    rc = execRaw(raw, "PRAGMA foreign_keys=ON");
    if (rc != SQLITE_OK)
        return makeError(raw, rc, {});
    return db;
}

// Flytten tömmer källan, så att anslutningen bara stängs av en ägare.
Database::Database(Database&& other) noexcept
    : db_(std::exchange(other.db_, nullptr)), path_(std::move(other.path_))
{
    other.path_.clear();
}

// Flytttilldelning: den egna anslutningen stängs innan den andras tas över.
Database& Database::operator=(Database&& other) noexcept
{
    if (this != &other) {
        close();
        db_ = std::exchange(other.db_, nullptr);
        path_ = std::move(other.path_);
        other.path_.clear();
    }
    return *this;
}

// Stänger anslutningen.
Database::~Database()
{
    close();
}

// Filens sökväg, eller en tom sträng för en databas i minnet eller en stängd.
const std::string& Database::path() const
{
    return path_;
}

// Kör en eller flera satser utan parametrar. Satserna måste vara avslutade med noll,
// så texten kopieras.
core::Result<void> Database::exec(std::string_view sql)
{
    const std::string text(sql);
    const int rc = execRaw(db_, text.c_str());
    if (rc != SQLITE_OK)
        return makeError(db_, rc, path_);
    return {};
}

// Förbereder en sats med sqlite3_prepare_v2. En tom sats (bara kommentarer) räknas som fel.
core::Result<Statement> Database::prepare(std::string_view sql)
{
    sqlite3_stmt* stmt = nullptr;
    const int rc = sqlite3_prepare_v2(db_, sql.data(), static_cast<int>(sql.size()), &stmt, nullptr);
    if (rc != SQLITE_OK)
        return makeError(db_, rc, path_);
    if (!stmt) {
        core::Error e;
        e.code = core::ErrorCode::Sql;
        e.message = "Databasfel: tom sats.";
        return e;
    }
    return Statement(db_, stmt);
}

// BEGIN IMMEDIATE tar skrivlåset direkt, så att inget annat skrivfel kommer mitt i.
// Ger body ett fel rullas allt tillbaka, och felet från body returneras oförändrat.
core::Result<void> Database::transaction(const std::function<core::Result<void>()>& body)
{
    if (auto r = exec("BEGIN IMMEDIATE"); !r)
        return r;
    core::Result<void> result = body();
    if (!result) {
        (void)exec("ROLLBACK");
        return result;
    }
    if (auto r = exec("COMMIT"); !r) {
        (void)exec("ROLLBACK");
        return r;
    }
    return {};
}

// Läser ett heltals-pragma, t.ex. user_version. Namnet är programmets eget och inte indata.
core::Result<std::int64_t> Database::pragmaInt(std::string_view name)
{
    auto stmt = prepare("PRAGMA " + std::string(name));
    if (!stmt)
        return stmt.error();
    auto row = stmt.value().step();
    if (!row)
        return row.error();
    return row.value() ? stmt.value().integer(0) : std::int64_t{0};
}

// Tar en konsekvent och kompakt ögonblicksbild med VACUUM INTO. Målet får inte finnas,
// och det kontrolleras först så att felet blir AlreadyExists i stället för ett SQL-fel.
core::Result<void> Database::vacuumInto(const std::string& utf8Target)
{
    if (platform::stamp(utf8Target).ok()) {
        core::Error e;
        e.code = core::ErrorCode::AlreadyExists;
        e.message = "Det finns redan en fil med namnet:" "\n" + utf8Target;
        return e;
    }
    auto stmt = prepare("VACUUM INTO ?1");
    if (!stmt)
        return stmt.error();
    stmt.value().bind(1, std::string_view(utf8Target));
    return stmt.value().run();
}

// PRAGMA quick_check(1) ger "ok" för en hel fil. Allt annat betyder trunkerad eller skadad.
core::Result<void> Database::quickCheck()
{
    auto stmt = prepare("PRAGMA quick_check(1)");
    if (!stmt)
        return stmt.error();
    auto row = stmt.value().step();
    if (!row)
        return row.error();
    if (row.value() && stmt.value().text(0) == "ok")
        return {};
    core::Error e;
    e.code = core::ErrorCode::NotADatabase;
    e.message = "Filen är inte en databas eller är skadad:" "\n" + path_;
    e.detail = row.value() ? stmt.value().text(0) : "quick_check gav ingen rad";
    return e;
}

// Stänger anslutningen. sqlite3_close_v2 gör att satser som lever kvar inte hindrar stängningen.
void Database::close()
{
    if (db_) {
        sqlite3_close_v2(db_);
        db_ = nullptr;
    }
    path_.clear();
}

} // namespace kalkyl::db
