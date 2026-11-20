#include "systemdb/system_db.h"

#include <filesystem>
#include <utility>

#include "db/migrate.h"
#include "platform/files.h"
#include "platform/system.h"
#include "platform/text.h"

namespace kalkyl::systemdb {

namespace {

using core::Error;
using core::ErrorCode;
using core::Result;

// Schema version 1: inställningar (nyckel och värde) och listan över senaste projekt.
// Se planen 2.10. path_key är jämförelsenyckeln, path den sökväg som visas och öppnas.
constexpr std::string_view kSchemaV1 = R"sql(
CREATE TABLE setting (
    key   TEXT PRIMARY KEY NOT NULL,
    value TEXT NOT NULL
);
CREATE TABLE recent_project (
    path_key    TEXT PRIMARY KEY NOT NULL,
    path        TEXT NOT NULL,
    name        TEXT NOT NULL,
    last_opened TEXT NOT NULL
);
CREATE INDEX recent_project_last_opened ON recent_project(last_opened);
)sql";

// Schemat för systemdatabasen. En tom fil får initieras.
db::Schema schema()
{
    db::Schema s;
    s.applicationId = kApplicationId;
    s.steps = {kSchemaV1};
    s.allowInitialize = true;
    return s;
}

// Fogar ihop en mapp och ett filnamn med std::filesystem, så att UTF-8 och avgränsare blir rätt.
std::string joinPath(const std::string& dir, const std::string& name)
{
    return platform::pathToUtf8(platform::pathFromUtf8(dir) / platform::pathFromUtf8(name));
}

// Öppnar filen (skapar den vid behov) och migrerar den. Databasen stängs igen vid fel,
// så att filen går att flytta undan.
Result<db::Database> openMigrated(const std::string& path)
{
    db::OpenOptions options;
    options.mode = db::OpenMode::OpenOrCreate;
    auto database = db::Database::open(path, options);
    if (!database)
        return database.error();
    auto migrated = db::migrate(database.value(), schema());
    if (!migrated)
        return migrated.error();
    return std::move(database).value();
}

// Bygger tidsdelen i namnet på en undanflyttad fil ur nowUtcIso: ÅÅÅÅMMDD-HHMMSS.
std::string brokenStamp()
{
    const std::string iso = platform::nowUtcIso();   // 2026-11-20T13:45:07.123Z
    std::string out;
    for (std::size_t i = 0; i < iso.size() && i < 19; ++i) {
        const char c = iso[i];
        if (c >= '0' && c <= '9')
            out += c;
        else if (c == 'T')
            out += '-';
    }
    return out;
}

// Flyttar en skadad systemdatabas undan till system-trasig-<tid>.db i samma mapp.
// En kvarglömd journal tas bort, så att den inte spelas upp mot den nya filen.
Result<void> moveAside(const std::string& path)
{
    const std::string target =
        joinPath(platform::parentDir(path), "system-trasig-" + brokenStamp() + ".db");
    auto moved = platform::moveNew(path, target);
    if (!moved)
        return moved.error();
    (void)platform::removeFile(path + "-journal");
    return {};
}

} // namespace

// Systemdatabasens förvalda plats: system.db i Kalkyls datamapp.
// KALKYL_DATA_DIR flyttar den, till exempel i tester och röktester.
std::string defaultPath()
{
    return joinPath(platform::kalkylDataDir(), "system.db");
}

// Tar över en öppnad och migrerad anslutning. En tom orsak betyder att filen används,
// annars är det reserven i minnet och orsaken visas för användaren.
SystemDb::SystemDb(db::Database database, std::string fallbackReason)
    : db_(std::move(database)), fallbackReason_(std::move(fallbackReason))
{
}

// Flytt och rivning sköts av medlemmarna: anslutningen stängs när objektet förstörs.
SystemDb::SystemDb(SystemDb&&) noexcept = default;
SystemDb& SystemDb::operator=(SystemDb&&) noexcept = default;
SystemDb::~SystemDb() = default;

// Öppnar systemdatabasen. Fel id eller en skadad fil flyttas undan och ersätts med en ny.
// Övriga fel, till exempel TooNew och åtkomstfel, returneras till anroparen.
Result<SystemDb> SystemDb::open(const std::string& utf8Path)
{
    const std::string path = platform::absolutePath(utf8Path);
    auto dir = platform::ensureDirectory(platform::parentDir(path));
    if (!dir)
        return dir.error();

    auto database = openMigrated(path);
    if (!database && (database.error().code == ErrorCode::NotADatabase ||
                      database.error().code == ErrorCode::WrongApplication)) {
        auto moved = moveAside(path);
        if (!moved)
            return moved.error();
        database = openMigrated(path);
    }
    if (!database)
        return database.error();
    return SystemDb(std::move(database).value(), "");
}

// Reserv i minnet när filen inte går att använda. Allt fungerar men inget sparas.
Result<SystemDb> SystemDb::openInMemory(std::string reason)
{
    auto database = db::Database::openMemory();
    if (!database)
        return database.error();
    auto migrated = db::migrate(database.value(), schema());
    if (!migrated)
        return migrated.error();
    return SystemDb(std::move(database).value(), std::move(reason));
}

// En databas i minnet har ingen sökväg, och då sparas inget.
bool SystemDb::isPersistent() const
{
    return !db_.path().empty();
}

// Varför reserven i minnet används, tom när filen används.
// Väljaren visar texten i statusraden.
const std::string& SystemDb::fallbackReason() const
{
    return fallbackReason_;
}

// Läser en inställning. Saknas den eller går det inte att läsa blir svaret en tom sträng.
std::string SystemDb::setting(const char* key)
{
    auto stmt = db_.prepare("SELECT value FROM setting WHERE key = ?1");
    if (!stmt)
        return {};
    stmt.value().bind(1, key);
    auto row = stmt.value().step();
    if (!row || !row.value())
        return {};
    return stmt.value().text(0);
}

// Skriver en inställning, och ersätter ett tidigare värde för samma nyckel.
Result<void> SystemDb::setSetting(const char* key, const std::string& value)
{
    auto stmt = db_.prepare("INSERT INTO setting(key, value) VALUES(?1, ?2) "
                            "ON CONFLICT(key) DO UPDATE SET value = excluded.value");
    if (!stmt)
        return stmt.error();
    stmt.value().bind(1, key).bind(2, value);
    return stmt.value().run();
}

// Det sparade utseendeläget. Ett okänt eller saknat värde ger System,
// så att ett fel aldrig hindrar starten.
ThemeMode SystemDb::themeMode()
{
    const std::string value = setting("theme_mode");
    if (value == "light")
        return ThemeMode::Light;
    if (value == "dark")
        return ThemeMode::Dark;
    return ThemeMode::System;
}

// Sparar utseendeläget som text: system, light eller dark.
// Texten gör värdet läsbart också i ett vanligt SQLite-verktyg.
Result<void> SystemDb::setThemeMode(ThemeMode mode)
{
    const char* value = mode == ThemeMode::Light ? "light"
                      : mode == ThemeMode::Dark  ? "dark"
                                                 : "system";
    return setSetting("theme_mode", value);
}

// Mappen som användes senast, startmapp för filväljarna.
// Tom om ingen mapp sparats eller om den inte gick att läsa.
std::string SystemDb::lastFolder()
{
    return setting("last_folder");
}

// Sparar mappen som användes senast. Anropas när ett projekt har öppnats
// eller skapats, med projektets mapp.
Result<void> SystemDb::setLastFolder(const std::string& folder)
{
    return setSetting("last_folder", folder);
}

// De senaste projekten, senast öppnade först. Vid lika tid avgör radordningen.
Result<std::vector<RecentProject>> SystemDb::recentProjects()
{
    auto stmt = db_.prepare("SELECT path, name, last_opened FROM recent_project "
                            "ORDER BY last_opened DESC, rowid DESC LIMIT ?1");
    if (!stmt)
        return stmt.error();
    stmt.value().bind(1, static_cast<std::int64_t>(kMaxRecent));
    std::vector<RecentProject> list;
    while (true) {
        auto row = stmt.value().step();
        if (!row)
            return row.error();
        if (!row.value())
            break;
        list.push_back({stmt.value().text(0), stmt.value().text(1), stmt.value().text(2)});
    }
    return list;
}

// Lägger in eller uppdaterar projektet och trimmar listan till kMaxRecent rader.
// Båda stegen sker i en transaktion. Samma fil under olika skrivsätt ger en rad via pathKey.
Result<void> SystemDb::touchRecentProject(const std::string& path, const std::string& name)
{
    const std::string shown = platform::absolutePath(path);
    const std::string key = platform::pathKey(path);
    const std::string now = platform::nowUtcIso();
    return db_.transaction([&]() -> Result<void> {
        auto upsert = db_.prepare(
            "INSERT INTO recent_project(path_key, path, name, last_opened) "
            "VALUES(?1, ?2, ?3, ?4) "
            "ON CONFLICT(path_key) DO UPDATE SET path = excluded.path, "
            "name = excluded.name, last_opened = excluded.last_opened");
        if (!upsert)
            return upsert.error();
        upsert.value().bind(1, key).bind(2, shown).bind(3, name).bind(4, now);
        if (auto r = upsert.value().run(); !r)
            return r;
        auto trim = db_.prepare(
            "DELETE FROM recent_project WHERE path_key NOT IN "
            "(SELECT path_key FROM recent_project ORDER BY last_opened DESC, rowid DESC LIMIT ?1)");
        if (!trim)
            return trim.error();
        trim.value().bind(1, static_cast<std::int64_t>(kMaxRecent));
        return trim.value().run();
    });
}

// Tar bort projektet ur listan. Filen rörs aldrig, och en rad som saknas är inget fel.
Result<void> SystemDb::removeRecentProject(const std::string& path)
{
    auto stmt = db_.prepare("DELETE FROM recent_project WHERE path_key = ?1");
    if (!stmt)
        return stmt.error();
    stmt.value().bind(1, platform::pathKey(path));
    return stmt.value().run();
}

} // namespace kalkyl::systemdb
