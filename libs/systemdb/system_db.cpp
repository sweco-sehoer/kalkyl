#include "systemdb/system_db.h"

#include <charconv>
#include <filesystem>
#include <string>
#include <system_error>
#include <utility>

#include "db/migrate.h"
#include "platform/files.h"
#include "platform/system.h"
#include "platform/text.h"
#include "systemdb/conductor_seed_v1.h"

namespace kalkyl::systemdb {

using conductor::Conductor;
using conductor::ConductorOrigin;
using conductor::sameConductorValues;
using conductor::validateConductor;

namespace {

using core::Error;
using core::ErrorCode;
using core::Result;

// Schema version 1: inställningar (nyckel och värde), listan över senaste projekt och linregistret.
// Se planen 2.10 (Grunder) och 3 (Lindata). path_key är jämförelsenyckeln, path den sökväg som
// visas och öppnas. Linregistrets regler ligger i C++ och inte som CHECK, bortsett från flaggan.
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
-- Startdatan, orörd. Bara migreringsstegen skriver här. Återställningen läser härifrån.
CREATE TABLE conductor_seed (
    seed_key              TEXT PRIMARY KEY NOT NULL,
    name                  TEXT NOT NULL,
    type                  TEXT NOT NULL DEFAULT '',
    area_mm2              REAL NOT NULL,
    diameter_mm           REAL NOT NULL,
    e_initial_mpa         REAL NOT NULL,
    e_permanent_mpa       REAL NOT NULL,
    e_initial_limit_mpa   REAL NOT NULL,
    eiv_constant          REAL NOT NULL,
    alpha_e6_per_c        REAL NOT NULL,
    creep_strain_permille REAL NOT NULL,
    creep_temp_c          REAL NOT NULL,
    weight_n_per_m        REAL NOT NULL,
    breaking_load_kn      REAL,
    sort_order            INTEGER NOT NULL
);
-- Linregistret som användaren redigerar. id återanvänds aldrig.
CREATE TABLE conductor (
    id                    INTEGER PRIMARY KEY AUTOINCREMENT,
    name                  TEXT NOT NULL COLLATE NOCASE UNIQUE,
    type                  TEXT NOT NULL DEFAULT '',
    area_mm2              REAL NOT NULL,
    diameter_mm           REAL NOT NULL,
    e_initial_mpa         REAL NOT NULL,
    e_permanent_mpa       REAL NOT NULL,
    e_initial_limit_mpa   REAL NOT NULL,
    eiv_constant          REAL NOT NULL,
    alpha_e6_per_c        REAL NOT NULL,
    creep_strain_permille REAL NOT NULL,
    creep_temp_c          REAL NOT NULL,
    weight_n_per_m        REAL NOT NULL,
    breaking_load_kn      REAL,
    sort_order            INTEGER NOT NULL,
    seed_key              TEXT UNIQUE,
    user_modified         INTEGER NOT NULL DEFAULT 0 CHECK (user_modified IN (0, 1))
);
CREATE INDEX conductor_order ON conductor(sort_order, id);
-- Startdatalinor som användaren tagit bort. En senare migrering lägger inte tillbaka dem.
CREATE TABLE conductor_seed_removed (
    seed_key TEXT PRIMARY KEY NOT NULL
);
)sql";

// Tabellerna som schemat ska innehålla. Schemakontrollen räknar dem, så listan hålls lika med
// schemat: ett nytt steg med en ny tabell lägger till den här.
constexpr std::string_view kExpectedTables =
    "'setting', 'recent_project', 'conductor_seed', 'conductor', 'conductor_seed_removed'";
constexpr int kExpectedTableCount = 5;

// Schemat för systemdatabasen. En tom fil får initieras. Steg 1 är tabellerna och startdatan,
// ihopfogade en gång eftersom db::Schema kräver att stegen lever så länge programmet.
db::Schema schema()
{
    static const std::string step1 = std::string(kSchemaV1) + std::string(kConductorSeedV1);
    db::Schema s;
    s.applicationId = kApplicationId;
    s.steps = {step1};
    s.allowInitialize = true;
    return s;
}

// Ett fel med en kod, en svensk text till användaren och en teknisk detalj till loggen.
Error makeError(ErrorCode code, std::string message, std::string detail = {})
{
    Error e;
    e.code = code;
    e.message = std::move(message);
    e.detail = std::move(detail);
    return e;
}

// Tecknet för radbrytning i meddelandena.
const char kNewline = static_cast<char>(10);

// Kontrollerar att alla tabeller i schemat finns. En fil vars tabeller inte stämmer med dess
// version är skadad (planen 2.8) och flyttas undan av open. Ingenting repareras här.
Result<void> checkSchema(db::Database& database)
{
    auto stmt = database.prepare(
        "SELECT count(*) FROM sqlite_master WHERE type = 'table' AND name IN (" +
        std::string(kExpectedTables) + ")");
    if (!stmt)
        return stmt.error();
    auto row = stmt.value().step();
    if (!row)
        return row.error();
    const std::int64_t found = row.value() ? stmt.value().integer(0) : 0;
    if (found != kExpectedTableCount)
        return makeError(ErrorCode::NotADatabase, "Systemdatabasen saknar tabeller.",
                         std::to_string(found) + " av " + std::to_string(kExpectedTableCount) +
                             " tabeller finns");
    return {};
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
    if (auto checked = checkSchema(database.value()); !checked)
        return checked.error();
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

// Minuter mellan automatiska sparningar. Saknat, otolkbart eller ogiltigt värde ger förvalet.
// Bara värdena i kAutosaveChoices godtas, så ett skräpvärde i filen aldrig stänger av sparningen.
int SystemDb::autosaveMinutes()
{
    const std::string value = setting("autosave_minutes");
    int minutes = 0;
    const auto r = std::from_chars(value.data(), value.data() + value.size(), minutes);
    if (value.empty() || r.ec != std::errc() || r.ptr != value.data() + value.size())
        return kDefaultAutosaveMinutes;
    for (int choice : kAutosaveChoices)
        if (choice == minutes)
            return minutes;
    return kDefaultAutosaveMinutes;
}

// Sparar minuterna för automatisk sparning. Värdet måste vara ett av kAutosaveChoices.
Result<void> SystemDb::setAutosaveMinutes(int minutes)
{
    bool allowed = false;
    for (int choice : kAutosaveChoices)
        allowed = allowed || choice == minutes;
    if (!allowed)
        return makeError(ErrorCode::Invalid, "Ogiltigt värde för automatisk sparning.",
                         "autosave_minutes " + std::to_string(minutes));
    return setSetting("autosave_minutes", std::to_string(minutes));
}

// Mappen där Importera stolptabell… öppnar filväljaren, tom när inget är sparat.
// Den är skild från last_folder, eftersom Excelfilerna ofta ligger i en annan mapp än projekten.
std::string SystemDb::lastImportFolder()
{
    return setting("last_import_folder");
}

// Sparar importmappen, filens mapp efter en lyckad import.
Result<void> SystemDb::setLastImportFolder(const std::string& folder)
{
    return setSetting("last_import_folder", folder);
}

// Filtret i Stolptabell efter. Bara texten all ger All, så att ett saknat eller okänt värde
// och ett fel vid läsningen alltid ger förvalet Förändrade ±2.
AfterFilter SystemDb::afterFilter()
{
    return setting("pole_after_filter") == "all" ? AfterFilter::All : AfterFilter::Changed;
}

// Sparar filtret som text: changed eller all. Texten gör värdet läsbart i ett vanligt SQLite-verktyg.
Result<void> SystemDb::setAfterFilter(AfterFilter filter)
{
    return setSetting("pole_after_filter", filter == AfterFilter::All ? "all" : "changed");
}

namespace {

// Högsta antal inlärda rubriker per fält.
constexpr int kMaxLearnedPerField = 10;
constexpr int kLearnedFieldCount = 8;

// Tolkar värdet i pole_import_learned: en rad per rubrik, fältet och rubriken skilda med en tabb.
// Rader som inte går att tolka hoppas över.
std::vector<LearnedHeader> parseLearned(const std::string& value)
{
    std::vector<LearnedHeader> list;
    std::size_t start = 0;
    while (start <= value.size()) {
        std::size_t end = value.find(char{10}, start);
        if (end == std::string::npos)
            end = value.size();
        const std::string line = value.substr(start, end - start);
        start = end + 1;
        const std::size_t tab = line.find(char{9});
        if (tab == std::string::npos || tab == 0 || tab + 1 >= line.size())
            continue;
        int field = 0;
        const auto r = std::from_chars(line.data(), line.data() + tab, field);
        if (r.ec != std::errc() || r.ptr != line.data() + tab || field < 0 || field >= kLearnedFieldCount)
            continue;
        list.push_back({field, line.substr(tab + 1)});
    }
    return list;
}

// Skriver listan som värdet i pole_import_learned, en rad per rubrik.
std::string formatLearned(const std::vector<LearnedHeader>& list)
{
    std::string value;
    for (const LearnedHeader& h : list) {
        if (!value.empty())
            value += char{10};
        value += std::to_string(h.field) + char{9} + h.key;
    }
    return value;
}

} // namespace

// De inlärda rubrikerna, senast först. Fel vid läsning ger en tom lista.
std::vector<LearnedHeader> SystemDb::learnedHeaders()
{
    return parseLearned(setting("pole_import_learned"));
}

// Lägger rubriken först för fältet och tar bort en tidigare likadan. Högst kMaxLearnedPerField
// rubriker per fält behålls, de senaste.
Result<void> SystemDb::learnHeader(int field, const std::string& key)
{
    if (field < 0 || field >= kLearnedFieldCount)
        return makeError(ErrorCode::Invalid, "Ogiltigt fält för den inlärda rubriken.",
                         "field " + std::to_string(field));
    if (key.empty() || key.find(char{9}) != std::string::npos || key.find(char{10}) != std::string::npos ||
        key.find(char{13}) != std::string::npos)
        return makeError(ErrorCode::Invalid, "Ogiltig rubrik att lära in.");

    std::vector<LearnedHeader> list;
    list.push_back({field, key});
    for (const LearnedHeader& h : learnedHeaders())
        if (!(h.field == field && h.key == key))
            list.push_back(h);

    std::vector<LearnedHeader> kept;
    int perField[kLearnedFieldCount] = {};
    for (const LearnedHeader& h : list)
        if (perField[h.field]++ < kMaxLearnedPerField)
            kept.push_back(h);
    return setSetting("pole_import_learned", formatLearned(kept));
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

namespace {

// Kolumnerna som läses för en lina. Ordningen är den som readConductor förutsätter.
constexpr std::string_view kConductorColumns =
    "id, name, type, area_mm2, diameter_mm, e_initial_mpa, e_permanent_mpa, "
    "e_initial_limit_mpa, eiv_constant, alpha_e6_per_c, creep_strain_permille, creep_temp_c, "
    "weight_n_per_m, breaking_load_kn, seed_key, user_modified";

// Bygger en Conductor ur den aktuella raden. Ursprunget härleds ur seed_key och user_modified.
Conductor readConductor(const db::Statement& s)
{
    Conductor c;
    c.id = s.integer(0);
    c.name = s.text(1);
    c.type = s.text(2);
    c.areaMm2 = s.real(3);
    c.diameterMm = s.real(4);
    c.eInitialMpa = s.real(5);
    c.ePermanentMpa = s.real(6);
    c.eInitialLimitMpa = s.real(7);
    c.eivConstant = s.real(8);
    c.alphaE6PerC = s.real(9);
    c.creepStrainPermille = s.real(10);
    c.creepTempC = s.real(11);
    c.weightNPerM = s.real(12);
    if (!s.isNull(13))
        c.breakingLoadKn = s.real(13);
    if (s.isNull(14))
        c.origin = ConductorOrigin::Own;
    else
        c.origin = s.integer(15) == 1 ? ConductorOrigin::SeedModified : ConductorOrigin::Seed;
    return c;
}

// Sant för blank, tabb, vagnretur och radbrytning.
bool isSpace(char c)
{
    return c == 32 || c == 9 || c == 13 || c == 10;
}

// Tar bort blanktecken i början och slutet av en text.
std::string trimmed(const std::string& text)
{
    std::size_t first = 0;
    std::size_t last = text.size();
    while (first < last && isSpace(text[first]))
        ++first;
    while (last > first && isSpace(text[last - 1]))
        --last;
    return text.substr(first, last - first);
}

// Läser en lina på id. Svaret är tomt om id saknas.
Result<std::optional<Conductor>> loadConductor(db::Database& database, std::int64_t id)
{
    auto stmt = database.prepare("SELECT " + std::string(kConductorColumns) +
                                 " FROM conductor WHERE id = ?1");
    if (!stmt)
        return stmt.error();
    stmt.value().bind(1, id);
    auto row = stmt.value().step();
    if (!row)
        return row.error();
    if (!row.value())
        return std::optional<Conductor>{};
    return std::optional<Conductor>{readConductor(stmt.value())};
}

// Binder namn, typ och värden på plats 1-13 i INSERT- och UPDATE-satserna nedan.
// Saknad brottlast binds som NULL.
void bindConductorValues(db::Statement& s, const Conductor& c)
{
    s.bind(1, c.name).bind(2, c.type);
    s.bind(3, c.areaMm2).bind(4, c.diameterMm).bind(5, c.eInitialMpa).bind(6, c.ePermanentMpa);
    s.bind(7, c.eInitialLimitMpa).bind(8, c.eivConstant).bind(9, c.alphaE6PerC);
    s.bind(10, c.creepStrainPermille).bind(11, c.creepTempC).bind(12, c.weightNPerM);
    if (c.breakingLoadKn)
        s.bind(13, *c.breakingLoadKn);
    else
        s.bindNull(13);
}

// Tar bort linorna och skriver startdatalinornas nycklar i conductor_seed_removed.
// Ett id som saknas är inget fel. Anroparen har en transaktion.
Result<void> removeConductors(db::Database& database, const std::vector<std::int64_t>& removals)
{
    for (const std::int64_t id : removals) {
        std::vector<std::string> seedKeys;
        {
            auto del = database.prepare("DELETE FROM conductor WHERE id = ?1 RETURNING seed_key");
            if (!del)
                return del.error();
            del.value().bind(1, id);
            while (true) {
                auto row = del.value().step();
                if (!row)
                    return row.error();
                if (!row.value())
                    break;
                if (!del.value().isNull(0))
                    seedKeys.push_back(del.value().text(0));
            }
        }
        for (const std::string& key : seedKeys) {
            auto mark = database.prepare("INSERT OR IGNORE INTO conductor_seed_removed(seed_key) VALUES(?1)");
            if (!mark)
                return mark.error();
            mark.value().bind(1, key);
            if (auto r = mark.value().run(); !r)
                return r;
        }
    }
    return {};
}

// Sant om en annan rad än den med id redan har namnet (NOCASE, kolumnens jämförelse).
Result<bool> nameTaken(db::Database& database, const std::string& name, std::int64_t id)
{
    auto stmt = database.prepare("SELECT id FROM conductor WHERE name = ?1 AND id <> ?2");
    if (!stmt)
        return stmt.error();
    stmt.value().bind(1, name).bind(2, id);
    auto row = stmt.value().step();
    if (!row)
        return row.error();
    return row.value();
}

// Skriver en ändrad rad. Flaggan user_modified sätts för en startdatarad och nollas för en egen.
Result<void> updateConductor(db::Database& database, const Conductor& c)
{
    auto stmt = database.prepare(
        "UPDATE conductor SET name = ?1, type = ?2, area_mm2 = ?3, diameter_mm = ?4, "
        "e_initial_mpa = ?5, e_permanent_mpa = ?6, e_initial_limit_mpa = ?7, eiv_constant = ?8, "
        "alpha_e6_per_c = ?9, creep_strain_permille = ?10, creep_temp_c = ?11, "
        "weight_n_per_m = ?12, breaking_load_kn = ?13, "
        "user_modified = CASE WHEN seed_key IS NULL THEN 0 ELSE 1 END WHERE id = ?14");
    if (!stmt)
        return stmt.error();
    bindConductorValues(stmt.value(), c);
    stmt.value().bind(14, c.id);
    return stmt.value().run();
}

// Lägger in en egen lina sist i ordningen och svarar med det nya id:t.
Result<std::int64_t> insertConductor(db::Database& database, const Conductor& c)
{
    auto stmt = database.prepare(
        "INSERT INTO conductor(name, type, area_mm2, diameter_mm, e_initial_mpa, e_permanent_mpa, "
        "e_initial_limit_mpa, eiv_constant, alpha_e6_per_c, creep_strain_permille, creep_temp_c, "
        "weight_n_per_m, breaking_load_kn, sort_order, seed_key, user_modified) "
        "VALUES(?1, ?2, ?3, ?4, ?5, ?6, ?7, ?8, ?9, ?10, ?11, ?12, ?13, "
        "(SELECT coalesce(max(sort_order), 0) + 10 FROM conductor), NULL, 0) RETURNING id");
    if (!stmt)
        return stmt.error();
    bindConductorValues(stmt.value(), c);
    std::int64_t id = 0;
    while (true) {
        auto row = stmt.value().step();
        if (!row)
            return row.error();
        if (!row.value())
            break;
        id = stmt.value().integer(0);
    }
    return id;
}

// Sparningens gemensamma del, utan egen transaktion (planen 4.2). Borttagningarna körs först.
// Raderna valideras och trimmas, oförändrade hoppas över, omdöpta får först ett tillfälligt namn
// så att namnbyten mellan två rader inte krockar, och sedan skrivs resten. Svarar med id per rad.
Result<std::vector<std::int64_t>> applyConductorChanges(db::Database& database,
                                                        const std::vector<Conductor>& upserts,
                                                        const std::vector<std::int64_t>& removals)
{
    if (auto r = removeConductors(database, removals); !r)
        return r.error();

    // Validering och trimning av alla rader innan något skrivs.
    std::vector<Conductor> rows = upserts;
    for (Conductor& row : rows) {
        row.name = trimmed(row.name);
        row.type = trimmed(row.type);
        const auto issues = validateConductor(row);
        if (!issues.empty())
            return makeError(ErrorCode::Invalid, issues.front().message, "lina " + row.name);
    }

    // Raderna som redan finns läses som de står, för att se vilka som ändrats och vilka som byter namn.
    std::vector<bool> skip(rows.size(), false);
    std::vector<bool> renamed(rows.size(), false);
    for (std::size_t i = 0; i < rows.size(); ++i) {
        if (rows[i].id == 0)
            continue;
        auto existing = loadConductor(database, rows[i].id);
        if (!existing)
            return existing.error();
        if (!existing.value())
            return makeError(ErrorCode::NotFound,
                             "Linan finns inte i linregistret:" + std::string(1, kNewline) + rows[i].name,
                             "id " + std::to_string(rows[i].id));
        skip[i] = sameConductorValues(*existing.value(), rows[i]);
        renamed[i] = !skip[i] && existing.value()->name != rows[i].name;
    }
    for (std::size_t i = 0; i < rows.size(); ++i) {
        if (!renamed[i])
            continue;
        auto temp = database.prepare("UPDATE conductor SET name = char(1) || id WHERE id = ?1");
        if (!temp)
            return temp.error();
        temp.value().bind(1, rows[i].id);
        if (auto r = temp.value().run(); !r)
            return r.error();
    }

    // Namnkontroll och skrivning, rad för rad i den givna ordningen.
    std::vector<std::int64_t> ids(rows.size(), 0);
    for (std::size_t i = 0; i < rows.size(); ++i) {
        ids[i] = rows[i].id;
        if (skip[i])
            continue;
        auto taken = nameTaken(database, rows[i].name, rows[i].id);
        if (!taken)
            return taken.error();
        if (taken.value())
            return makeError(ErrorCode::AlreadyExists,
                             "Det finns redan en lina med namnet:" + std::string(1, kNewline) + rows[i].name,
                             "namnet finns i conductor");
        if (rows[i].id == 0) {
            auto id = insertConductor(database, rows[i]);
            if (!id)
                return id.error();
            ids[i] = id.value();
        } else if (auto r = updateConductor(database, rows[i]); !r) {
            return r.error();
        }
    }
    return ids;
}

} // namespace

// Hela linregistret i registrets ordning: sort_order och därefter id.
Result<std::vector<Conductor>> SystemDb::conductors()
{
    auto stmt = db_.prepare("SELECT " + std::string(kConductorColumns) +
                            " FROM conductor ORDER BY sort_order, id");
    if (!stmt)
        return stmt.error();
    std::vector<Conductor> list;
    while (true) {
        auto row = stmt.value().step();
        if (!row)
            return row.error();
        if (!row.value())
            break;
        list.push_back(readConductor(stmt.value()));
    }
    return list;
}

// En lina på id. Svaret är tomt när id saknas, och det är inget fel.
Result<std::optional<Conductor>> SystemDb::conductor(std::int64_t id)
{
    return loadConductor(db_, id);
}

// Lägger till (id 0) eller uppdaterar en lina i en egen transaktion och svarar med id.
// Valideringen, trimningen och namnkontrollen står i applyConductorChanges.
Result<std::int64_t> SystemDb::saveConductor(const Conductor& c)
{
    std::int64_t id = 0;
    auto done = db_.transaction([&]() -> Result<void> {
        auto ids = applyConductorChanges(db_, {c}, {});
        if (!ids)
            return ids.error();
        id = ids.value().front();
        return {};
    });
    if (!done)
        return done.error();
    return id;
}

// Tar bort en lina. En startdatalinas nyckel skrivs i conductor_seed_removed, så att en
// senare migrering inte lägger tillbaka den. Ett id som saknas är inget fel.
Result<void> SystemDb::removeConductor(std::int64_t id)
{
    return db_.transaction([&]() -> Result<void> {
        auto r = applyConductorChanges(db_, {}, {id});
        if (!r)
            return r.error();
        return {};
    });
}

// Fönstrets sparning: borttagningar, uppdateringar och nya linor i EN transaktion.
// Vid fel rullas allt tillbaka, så ingenting sparas.
Result<void> SystemDb::saveConductors(const std::vector<Conductor>& upserts,
                                      const std::vector<std::int64_t>& removals)
{
    return db_.transaction([&]() -> Result<void> {
        auto r = applyConductorChanges(db_, upserts, removals);
        if (!r)
            return r.error();
        return {};
    });
}

// Startdatan tillbaka (planen 2.7) i en transaktion. En egen lina med ett startdatanamn stoppar
// allt. Startdatarader uppdateras på plats, så deras id finns kvar, och saknade läggs in igen.
Result<void> SystemDb::restoreSeedConductors()
{
    return db_.transaction([&]() -> Result<void> {
        // Stopp om en egen lina har ett namn ur startdatan.
        {
            auto clash = db_.prepare(
                "SELECT c.name FROM conductor c JOIN conductor_seed s "
                "ON c.name = s.name COLLATE NOCASE WHERE c.seed_key IS NULL ORDER BY c.id LIMIT 1");
            if (!clash)
                return clash.error();
            auto row = clash.value().step();
            if (!row)
                return row.error();
            if (row.value())
                return makeError(ErrorCode::AlreadyExists,
                                 "Din egen lina heter som en lina i startdatan:" +
                                     std::string(1, kNewline) + clash.value().text(0) +
                                     std::string(2, kNewline) + "Byt namn på den och försök igen.",
                                 "egen lina med startdatans namn");
        }
        // Tillfälliga namn först, så att namnbyten mellan startdatarader inte krockar.
        if (auto r = db_.exec("UPDATE conductor SET name = char(1) || id "
                              "WHERE seed_key IN (SELECT seed_key FROM conductor_seed)");
            !r)
            return r;
        // Startdatarader får startdatans värden och user_modified = 0, med oförändrat id.
        if (auto r = db_.exec(
                "UPDATE conductor SET (name, type, area_mm2, diameter_mm, e_initial_mpa, "
                "e_permanent_mpa, e_initial_limit_mpa, eiv_constant, alpha_e6_per_c, "
                "creep_strain_permille, creep_temp_c, weight_n_per_m, breaking_load_kn, "
                "sort_order, user_modified) = "
                "(SELECT s.name, s.type, s.area_mm2, s.diameter_mm, s.e_initial_mpa, "
                "s.e_permanent_mpa, s.e_initial_limit_mpa, s.eiv_constant, s.alpha_e6_per_c, "
                "s.creep_strain_permille, s.creep_temp_c, s.weight_n_per_m, s.breaking_load_kn, "
                "s.sort_order, 0 FROM conductor_seed s WHERE s.seed_key = conductor.seed_key) "
                "WHERE seed_key IN (SELECT seed_key FROM conductor_seed)");
            !r)
            return r;
        // Saknade startdatalinor läggs in igen.
        if (auto r = db_.exec(
                "INSERT INTO conductor (name, type, area_mm2, diameter_mm, e_initial_mpa, "
                "e_permanent_mpa, e_initial_limit_mpa, eiv_constant, alpha_e6_per_c, "
                "creep_strain_permille, creep_temp_c, weight_n_per_m, breaking_load_kn, "
                "sort_order, seed_key, user_modified) "
                "SELECT name, type, area_mm2, diameter_mm, e_initial_mpa, e_permanent_mpa, "
                "e_initial_limit_mpa, eiv_constant, alpha_e6_per_c, creep_strain_permille, "
                "creep_temp_c, weight_n_per_m, breaking_load_kn, sort_order, seed_key, 0 "
                "FROM conductor_seed WHERE seed_key NOT IN "
                "(SELECT seed_key FROM conductor WHERE seed_key IS NOT NULL) ORDER BY sort_order");
            !r)
            return r;
        return db_.exec("DELETE FROM conductor_seed_removed");
    });
}

} // namespace kalkyl::systemdb
