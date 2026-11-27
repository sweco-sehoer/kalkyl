// Headless-test för projektdatabasen. Allt sker i en egen temporär mapp med åäö i namnet och
// en egen arbetsmapp, aldrig i användarens riktiga data. Sist lämnas ett provprojekt kvar i
// KALKYL_PROV_DIR åt röktestet. Felkoden är antalet misslyckade kontroller.
#include <process.h>

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <limits>
#include <optional>
#include <stop_token>
#include <string>
#include <vector>

#include "anCalc/normal_span.h"
#include "conductor/conductor.h"
#include "db/database.h"
#include "lineCalc/load_cases.h"
#include "platform/files.h"
#include "platform/lock_file.h"
#include "platform/system.h"
#include "platform/text.h"
#include "poleTable/pole_table.h"
#include "projectdb/dev_migration.h"
#include "projectdb/project_data.h"
#include "projectdb/project_db.h"

using namespace kalkyl;
using core::ErrorCode;
namespace fs = std::filesystem;

namespace {

int g_failures = 0;

// Räknar ett misslyckande och skriver var det hände.
void report(bool ok, const char* expr, const char* file, int line)
{
    if (ok)
        return;
    ++g_failures;
    std::cerr << file << ":" << line << ": CHECK misslyckades: " << expr << std::endl;
}
#define CHECK(cond) report(static_cast<bool>(cond), #cond, __FILE__, __LINE__)

std::string g_share;   // mappen med projektfilerna (åäö i namnet)
std::string g_work;    // arbetskopiornas mapp

// Fogar ihop mapp och namn till en UTF-8-sökväg.
std::string join(const std::string& dir, const std::string& name)
{
    return platform::pathToUtf8(platform::pathFromUtf8(dir) / platform::pathFromUtf8(name));
}

bool exists(const std::string& path)
{
    std::error_code ec;
    return fs::exists(platform::pathFromUtf8(path), ec);
}

// Namnen på filerna i en mapp.
std::vector<std::string> listNames(const std::string& dir)
{
    std::vector<std::string> names;
    std::error_code ec;
    for (const auto& entry : fs::directory_iterator(platform::pathFromUtf8(dir), ec))
        names.push_back(platform::pathToUtf8(entry.path().filename()));
    return names;
}

// Antal filer i en mapp vars namn börjar med prefixet.
int countStartingWith(const std::string& dir, const std::string& prefix)
{
    int n = 0;
    for (const std::string& name : listNames(dir))
        if (name.rfind(prefix, 0) == 0)
            ++n;
    return n;
}

// Standardinställningar för ett test, med den egna arbetsmappen.
projectdb::OpenOptions options(projectdb::Access access = projectdb::Access::ReadWrite)
{
    projectdb::OpenOptions o;
    o.access = access;
    o.workDir = g_work;
    return o;
}

// Skriver en textfil.
void writeText(const std::string& path, const std::string& text)
{
    std::ofstream out(platform::pathFromUtf8(path), std::ios::binary);
    out << text;
}

// Skapar ett projekt och stänger det direkt, så att filen ligger kvar utan lås.
bool makeProject(const std::string& path, const std::string& name)
{
    auto p = projectdb::ProjectDb::create(path, name, options());
    CHECK(p.ok());
    if (!p.ok())
        return false;
    p.value().close();
    return true;
}

// Läser ett heltal ur en SQL-fråga mot en fil, öppnad direkt och skrivskyddat.
std::int64_t scalar(const std::string& path, const std::string& sql)
{
    db::OpenOptions o;
    o.readOnly = true;
    auto d = db::Database::open(path, o);
    CHECK(d.ok());
    if (!d.ok())
        return -1;
    auto stmt = d.value().prepare(sql);
    CHECK(stmt.ok());
    if (!stmt.ok())
        return -1;
    auto row = stmt.value().step();
    CHECK(row.ok() && row.value());
    return row.ok() && row.value() ? stmt.value().integer(0) : -1;
}

// create ger en fil med rätt id och version, open läser samma uppgifter, och låsfilen följer
// projektets öppna tid.
void testCreateAndOpen()
{
    const std::string path = projectdb::withExtension(join(g_share, "Ny ledning"));
    CHECK(path == join(g_share, "Ny ledning.kalkyl"));
    auto created = projectdb::ProjectDb::create(path, "Ny ledning", options());
    CHECK(created.ok());
    if (!created.ok()) {
        std::cerr << created.error().message << " / " << created.error().detail << std::endl;
        return;
    }
    projectdb::ProjectDb& p = created.value();
    CHECK(p.path() == path);
    CHECK(p.info().name == "Ny ledning");
    CHECK(p.info().createdWith.rfind("Kalkyl ", 0) == 0);
    CHECK(!p.info().createdUtc.empty());
    CHECK(p.access() == projectdb::Access::ReadWrite);
    CHECK(!p.lockedBy().has_value());
    CHECK(p.created());
    CHECK(exists(path));
    CHECK(exists(path + ".lock"));
    CHECK(scalar(path, "PRAGMA application_id") == static_cast<std::int64_t>(projectdb::kApplicationId));
    CHECK(scalar(path, "PRAGMA user_version") == projectdb::kSchemaVersion);
    CHECK(scalar(path, "SELECT count(*) FROM project WHERE id = 1") == 1);
    CHECK(countStartingWith(g_share, "~") == 0);
    p.close();
    CHECK(!exists(path + ".lock"));
    CHECK(listNames(g_work).empty());

    auto opened = projectdb::ProjectDb::open(path, options());
    CHECK(opened.ok());
    if (!opened.ok())
        return;
    CHECK(opened.value().info().name == "Ny ledning");
    CHECK(!opened.value().created());
    CHECK(opened.value().info().createdUtc == p.info().createdUtc);
    CHECK(opened.value().info().createdWith == p.info().createdWith);
    CHECK(opened.value().path() == path);
    auto q = opened.value().local().prepare("SELECT name FROM project");
    CHECK(q.ok());
}

// Låset: en andra ReadWrite-öppning ger Locked, ReadOnly fungerar och ger lockedBy, och efter
// close går det att öppna igen. Create mot en befintlig fil ger AlreadyExists utan att röra låset.
void testLocking()
{
    const std::string path = join(g_share, "Lås åäö.kalkyl");
    CHECK(makeProject(path, "Lås"));

    auto first = projectdb::ProjectDb::open(path, options());
    CHECK(first.ok());
    if (!first.ok())
        return;
    CHECK(exists(path + ".lock"));

    // Samma process håller låset, så öppningen väntar 2 s innan den ger upp.
    auto second = projectdb::ProjectDb::open(path, options());
    CHECK(!second.ok() && second.error().code == ErrorCode::Locked);
    if (!second.ok()) {
        CHECK(second.error().message.rfind("Projektet är öppnat av ", 0) == 0);
        CHECK(second.error().message.find(platform::userName()) != std::string::npos);
    }
    CHECK(exists(path + ".lock"));

    auto readOnly = projectdb::ProjectDb::open(path, options(projectdb::Access::ReadOnly));
    CHECK(readOnly.ok());
    if (readOnly.ok()) {
        CHECK(readOnly.value().access() == projectdb::Access::ReadOnly);
        CHECK(readOnly.value().lockedBy().has_value());
        if (readOnly.value().lockedBy())
            CHECK(readOnly.value().lockedBy()->user == platform::userName());
        CHECK(readOnly.value().info().name == "Lås");
        auto snap = readOnly.value().snapshot();
        CHECK(!snap.ok() && snap.error().code == ErrorCode::AccessDenied);
        readOnly.value().close();
        CHECK(exists(path + ".lock"));
    }

    auto again = projectdb::ProjectDb::create(path, "Annat", options());
    CHECK(!again.ok() && again.error().code == ErrorCode::AlreadyExists);
    CHECK(exists(path + ".lock"));

    first.value().close();
    CHECK(!exists(path + ".lock"));
    CHECK(listNames(g_work).empty());

    auto reopened = projectdb::ProjectDb::open(path, options());
    CHECK(reopened.ok());
}

// Egen lås: en återöppning direkt efter ett släpp i bakgrunden väntar ut släppet och lyckas.
void testOwnLockWait()
{
    const std::string path = join(g_share, "Egen.kalkyl");
    CHECK(makeProject(path, "Egen"));
    for (int i = 0; i < 3; ++i) {
        auto p = projectdb::ProjectDb::open(path, options());
        CHECK(p.ok());
        if (!p.ok())
            return;
        p.value().close(projectdb::ProjectDb::CloseMode::Background);
    }
}

// Sparning: snapshot, publish och published, med nytt innehåll i originalet och utan kvarglömda
// temporära filer. En andra sparning efter published får ingen Conflict.
void testSnapshotPublish()
{
    const std::string path = join(g_share, "Spara.kalkyl");
    auto created = projectdb::ProjectDb::create(path, "Spara", options());
    CHECK(created.ok());
    if (!created.ok())
        return;
    projectdb::ProjectDb& p = created.value();

    CHECK(p.local().exec("CREATE TABLE t(a INTEGER); INSERT INTO t VALUES(7);").ok());
    CHECK(scalar(path, "SELECT count(*) FROM sqlite_master WHERE name = 't'") == 0);

    auto snap = p.snapshot();
    CHECK(snap.ok());
    if (!snap.ok())
        return;
    CHECK(exists(snap.value().localFile));
    CHECK(snap.value().target == path);
    CHECK(snap.value().replace);
    auto stamp = projectdb::ProjectDb::publish(snap.value(), std::stop_token{});
    CHECK(stamp.ok());
    if (!stamp.ok()) {
        std::cerr << stamp.error().message << " / " << stamp.error().detail << std::endl;
        return;
    }
    p.published(snap.value(), stamp.value());
    CHECK(!exists(snap.value().localFile));
    CHECK(countStartingWith(g_share, "~") == 0);
    CHECK(scalar(path, "SELECT a FROM t") == 7);
    CHECK(scalar(path, "PRAGMA application_id") == static_cast<std::int64_t>(projectdb::kApplicationId));
    auto onDisk = platform::stamp(path);
    CHECK(onDisk.ok() && onDisk.value() == stamp.value());

    CHECK(p.local().exec("INSERT INTO t VALUES(8);").ok());
    auto snap2 = p.snapshot();
    CHECK(snap2.ok());
    if (!snap2.ok())
        return;
    auto stamp2 = projectdb::ProjectDb::publish(snap2.value(), std::stop_token{});
    CHECK(stamp2.ok());
    if (stamp2.ok())
        p.published(snap2.value(), stamp2.value());
    CHECK(scalar(path, "SELECT count(*) FROM t") == 2);
}

// Konflikt: en extern ändring av originalet följd av publish ger Conflict och ingen ersättning.
void testConflict()
{
    const std::string path = join(g_share, "Konflikt.kalkyl");
    auto created = projectdb::ProjectDb::create(path, "Konflikt", options());
    CHECK(created.ok());
    if (!created.ok())
        return;
    projectdb::ProjectDb& p = created.value();
    auto snap = p.snapshot();
    CHECK(snap.ok());
    if (!snap.ok())
        return;

    const auto before = platform::stamp(path);
    CHECK(before.ok());
    {
        std::ofstream out(platform::pathFromUtf8(path), std::ios::binary | std::ios::app);
        out << "x";
    }
    const auto changed = platform::stamp(path);
    CHECK(changed.ok() && before.ok() && !(changed.value() == before.value()));

    auto stamp = projectdb::ProjectDb::publish(snap.value(), std::stop_token{});
    CHECK(!stamp.ok() && stamp.error().code == ErrorCode::Conflict);
    CHECK(countStartingWith(g_share, "~") == 0);
    const auto after = platform::stamp(path);
    CHECK(after.ok() && changed.ok() && after.value() == changed.value());
    (void)platform::removeFile(snap.value().localFile);
}

// Efter ett fel som kommer efter att låset tagits ska låset vara släppt och arbetsmappen tom.
void checkCleanAfterFailure(const std::string& path)
{
    CHECK(!exists(path + ".lock"));
    CHECK(listNames(g_work).empty());
}

// Felaktiga filer: TooNew, en vanlig SQLite-fil och en textfil. Alla städar efter sig.
void testBadFiles()
{
    const std::string good = join(g_share, "God.kalkyl");
    CHECK(makeProject(good, "God"));

    const std::string tooNew = join(g_share, "För ny.kalkyl");
    std::error_code ec;
    fs::copy_file(platform::pathFromUtf8(good), platform::pathFromUtf8(tooNew), ec);
    CHECK(!ec);
    {
        auto d = db::Database::open(tooNew, db::OpenOptions{});
        CHECK(d.ok());
        if (d.ok())
            CHECK(d.value().exec("PRAGMA user_version = 99").ok());
    }
    auto r1 = projectdb::ProjectDb::open(tooNew, options());
    CHECK(!r1.ok() && r1.error().code == ErrorCode::TooNew);
    checkCleanAfterFailure(tooNew);

    const std::string plain = join(g_share, "Vanlig.kalkyl");
    {
        db::OpenOptions o;
        o.mode = db::OpenMode::CreateNew;
        auto d = db::Database::open(plain, o);
        CHECK(d.ok());
        if (d.ok())
            CHECK(d.value().exec("CREATE TABLE x(a INTEGER)").ok());
    }
    auto r2 = projectdb::ProjectDb::open(plain, options());
    CHECK(!r2.ok() && r2.error().code == ErrorCode::WrongApplication);
    if (!r2.ok())
        CHECK(r2.error().message.rfind("Filen är inte ett Kalkylprojekt:", 0) == 0);
    checkCleanAfterFailure(plain);

    const std::string text = join(g_share, "Text.kalkyl");
    writeText(text, "det här är bara text och inte en databas, men tillräckligt lång för en sidhuvud");
    auto r3 = projectdb::ProjectDb::open(text, options());
    CHECK(!r3.ok() && r3.error().code == ErrorCode::NotADatabase);
    checkCleanAfterFailure(text);
}

// Saknade filer och mappar, och en stop-token som redan är begärd.
void testMissingAndCancelled()
{
    const std::string missing = join(g_share, "Saknas.kalkyl");
    auto r1 = projectdb::ProjectDb::open(missing, options());
    CHECK(!r1.ok() && r1.error().code == ErrorCode::NotFound);

    const std::string noFolder = join(join(g_share, "mapp som saknas"), "X.kalkyl");
    auto r2 = projectdb::ProjectDb::create(noFolder, "X", options());
    CHECK(!r2.ok() && r2.error().code == ErrorCode::NotFound);

    const std::string good = join(g_share, "Avbryt.kalkyl");
    CHECK(makeProject(good, "Avbryt"));
    std::stop_source source;
    source.request_stop();
    projectdb::OpenOptions o = options();
    o.stop = source.get_token();
    auto r3 = projectdb::ProjectDb::open(good, o);
    CHECK(!r3.ok() && r3.error().code == ErrorCode::Cancelled);
    checkCleanAfterFailure(good);

    const std::string fresh = join(g_share, "Avbruten ny.kalkyl");
    auto r4 = projectdb::ProjectDb::create(fresh, "Avbruten", o);
    CHECK(!r4.ok() && r4.error().code == ErrorCode::Cancelled);
    CHECK(!exists(fresh));
    checkCleanAfterFailure(fresh);
}

// cleanWorkDir tar bort lösa filer men lämnar en fil som en annan anslutning har öppen.
void testCleanWorkDir()
{
    const std::string dir = join(g_share, "arbete städ");
    CHECK(platform::ensureDirectory(dir).ok());
    writeText(join(dir, "lös.kalkyl"), "x");
    writeText(join(dir, "lös.snap"), "x");
    const std::string held = join(dir, "öppen.kalkyl");
    db::OpenOptions o;
    o.mode = db::OpenMode::CreateNew;
    o.exclusive = true;
    auto d = db::Database::open(held, o);
    CHECK(d.ok());
    CHECK(d.ok() && d.value().exec("CREATE TABLE x(a INTEGER)").ok());

    projectdb::cleanWorkDir(dir);
    CHECK(!exists(join(dir, "lös.kalkyl")));
    CHECK(!exists(join(dir, "lös.snap")));
    CHECK(exists(held));
    if (d.ok())
        d.value().close();
    projectdb::cleanWorkDir(dir);
    CHECK(!exists(held));
    projectdb::cleanWorkDir(join(dir, "finns inte"));   // får inte krascha
}

// withExtension lägger bara till ändelsen när den saknas.
void testWithExtension()
{
    CHECK(projectdb::withExtension("a") == "a.kalkyl");
    CHECK(projectdb::withExtension("a.kalkyl") == "a.kalkyl");
    CHECK(projectdb::withExtension(join(g_share, "Åsa Öberg")) == join(g_share, "Åsa Öberg") + ".kalkyl");
}

// CURLEW-linan ur startdatan.
conductor::Conductor curlew()
{
    conductor::Conductor c;
    c.name = "593 mm² FeAl CURLEW";
    c.type = "FeAl CURLEW";
    c.areaMm2 = 593;
    c.diameterMm = 31.7;
    c.eInitialMpa = 52000;
    c.ePermanentMpa = 72000;
    c.eInitialLimitMpa = 120;
    c.eivConstant = 90;
    c.alphaE6PerC = 19;
    c.creepStrainPermille = 0.4;
    c.creepTempC = 21;
    c.weightNPerM = 19.6;
    c.breakingLoadKn = 165;
    return c;
}

// En annan giltig lina, med ett annat namn och andra värden.
conductor::Conductor otherConductor()
{
    conductor::Conductor c = curlew();
    c.name = "Annan lina";
    c.type = "Al";
    c.areaMm2 = 100;
    c.diameterMm = 12.5;
    return c;
}

// Namnet med ASCII-versalerna omvandlade till gemener. Övriga tecken lämnas orörda.
std::string lowerAscii(std::string text)
{
    for (char& ch : text)
        if (ch >= 'A' && ch <= 'Z')
            ch = static_cast<char>(ch - 'A' + 'a');
    return text;
}

// Exempel C ur planen: P0 till P3 med spannlängderna 250, 300, 200 och 400 och H 10, 20, 15, 30.
std::vector<ancalc::SpanPole> exampleC()
{
    return {{"P0", 250.0, 10.0}, {"P1", 300.0, 20.0}, {"P2", 200.0, 15.0}, {"P3", 400.0, 30.0}};
}

// Exempel B ur planen: P0 med 999 och därefter 100 (5 st), 400 och 100 (5 st), utan H.
std::vector<ancalc::SpanPole> exampleB()
{
    std::vector<ancalc::SpanPole> poles;
    poles.push_back({"P0", 999.0, std::nullopt});
    const double lengths[] = {100, 100, 100, 100, 100, 400, 100, 100, 100, 100, 100};
    int n = 1;
    for (double length : lengths)
        poles.push_back({"P" + std::to_string(n++), length, std::nullopt});
    return poles;
}

// Antal rader som en SQL-fråga ger i första kolumnen, mot arbetskopian.
std::int64_t countIn(projectdb::ProjectDb& p, const std::string& sql)
{
    auto stmt = p.local().prepare(sql);
    CHECK(stmt.ok());
    if (!stmt.ok())
        return -1;
    auto row = stmt.value().step();
    CHECK(row.ok() && row.value());
    return row.ok() && row.value() ? stmt.value().integer(0) : -1;
}

// Varje linnamn på linraderna i doc_value finns i project_conductor, och varje lina i
// project_conductor används av en linrad.
void checkConductorsConsistent(projectdb::ProjectDb& p)
{
    CHECK(countIn(p, R"sql(SELECT count(*) FROM doc_value
        WHERE key IN ('phase_conductor', 'top_conductor_left', 'top_conductor_right')
          AND value <> '' AND value <> 'Samma'
          AND value NOT IN (SELECT name FROM project_conductor))sql") == 0);
    CHECK(countIn(p, R"sql(SELECT count(*) FROM project_conductor
        WHERE name NOT IN (SELECT value FROM doc_value
                            WHERE key IN ('phase_conductor', 'top_conductor_left', 'top_conductor_right')))sql") == 0);
}

// Sparar projektet: bild, publicering och bekräftelse. Svarar sant när det gick.
bool publishNow(projectdb::ProjectDb& p)
{
    auto snap = p.snapshot();
    CHECK(snap.ok());
    if (!snap.ok())
        return false;
    auto stamp = projectdb::ProjectDb::publish(snap.value(), std::stop_token{});
    CHECK(stamp.ok());
    if (!stamp.ok())
        return false;
    p.published(snap.value(), stamp.value());
    return true;
}

// SQL-frågan som räknar de nitton tabellerna i schemat.
const char* const kNineteenTablesSql =
    "SELECT count(*) FROM sqlite_master WHERE type = 'table' AND name IN ('project', "
    "'project_conductor', 'doc_value', 'normal_span', 'normal_span_pole', 'doc_normal_span', "
    "'line_calc', 'line_calc_case', 'doc_line_calc', 'pole_before', 'pole_after', 'auto_mode', "
    "'auto_calc', 'pole_import', 'pole_import_column', 'pole_import_row', 'pole_import_cell', "
    "'line_height_span', 'line_height_pole')";

// Ett nytt projekt har alla nitton tabellerna i schemat.
void testSchemaTables()
{
    auto created = projectdb::ProjectDb::create(join(g_share, "Tabeller.kalkyl"), "Tabeller", options());
    CHECK(created.ok());
    if (!created.ok())
        return;
    CHECK(countIn(created.value(), kNineteenTablesSql) == 19);
    CHECK(countIn(created.value(), "PRAGMA user_version") == projectdb::kSchemaVersion);
    CHECK(projectdb::kSchemaVersion == 8);
}

// Skapar en projektfil med bara tabellen project, som i v0.1.0, och angiven user_version.
// Raden har namnet åäö-Gammal, så att migreringstestet kan se att den finns kvar.
bool writeV010Project(const std::string& path, int userVersion)
{
    db::OpenOptions o;
    o.mode = db::OpenMode::CreateNew;
    auto d = db::Database::open(path, o);
    CHECK(d.ok());
    if (!d.ok())
        return false;
    const std::string sql =
        "PRAGMA application_id = " + std::to_string(projectdb::kApplicationId) +
        "; PRAGMA user_version = " + std::to_string(userVersion) + "; "
        "CREATE TABLE project (id INTEGER PRIMARY KEY CHECK (id = 1), name TEXT NOT NULL, "
        "created_utc TEXT NOT NULL, created_with TEXT NOT NULL); "
        "INSERT INTO project VALUES (1, 'åäö-Gammal', '2026-11-23T12:00:00Z', 'Kalkyl 0.1.0');";
    const bool ok = d.value().exec(sql).ok();
    CHECK(ok);
    return ok;
}

// Ett projekt från v0.1.0 (user_version 1) migreras: projektraden finns kvar, de nya tabellerna
// finns, data kan sparas och originalet har user_version 2 efter publicering och ny öppning.
void testMigrateFromV010()
{
    const std::string path = join(g_share, "Från 0.1.0.kalkyl");
    if (!writeV010Project(path, 1))
        return;
    {
        auto opened = projectdb::ProjectDb::open(path, options());
        CHECK(opened.ok());
        if (!opened.ok())
            return;
        projectdb::ProjectDb& p = opened.value();
        CHECK(p.info().name == "åäö-Gammal");
        CHECK(p.info().createdUtc == "2026-11-23T12:00:00Z");
        CHECK(p.info().createdWith == "Kalkyl 0.1.0");
        CHECK(countIn(p, "PRAGMA user_version") == projectdb::kSchemaVersion);
        CHECK(countIn(p, kNineteenTablesSql) == 19);
        CHECK(projectdb::setDocValue(p, {"temp_max", "45", "Migrerad"}).ok());
        CHECK(publishNow(p));
        p.close();
    }
    CHECK(scalar(path, "PRAGMA user_version") == projectdb::kSchemaVersion);
    CHECK(scalar(path, "SELECT count(*) FROM project WHERE name = 'åäö-Gammal'") == 1);
    auto reopened = projectdb::ProjectDb::open(path, options());
    CHECK(reopened.ok());
    if (!reopened.ok())
        return;
    auto values = projectdb::docValues(reopened.value());
    CHECK(values.ok() && values.value().size() == 1);
    if (values.ok() && values.value().size() == 1)
        CHECK(values.value()[0].reference == "Migrerad");
}

// En fil med aktuell user_version men utan de nya tabellerna (skadad) vägras med texten,
// och varken lås eller arbetskopia blir kvar.
void testMissingTablesRejected()
{
    const std::string path = join(g_share, "Saknar tabeller.kalkyl");
    if (!writeV010Project(path, projectdb::kSchemaVersion))
        return;
    auto opened = projectdb::ProjectDb::open(path, options());
    CHECK(!opened.ok() && opened.error().code == ErrorCode::NotADatabase);
    if (!opened.ok()) {
        const std::string start = "Projektfilen saknar delar som den här versionen av Kalkyl behöver.";
        CHECK(opened.error().message.rfind(start, 0) == 0);
        CHECK(opened.error().message.size() > path.size() &&
              opened.error().message.compare(opened.error().message.size() - path.size(), path.size(),
                                             path) == 0);
        CHECK(opened.error().message.find(std::string(1, char{10}) + path) != std::string::npos);
    }
    checkCleanAfterFailure(path);
}

// Dokumentationens värden: sätta, ersätta och ta bort, ändringsräknaren och att värdena
// finns kvar i originalet efter publicering och ny öppning.
void testDocValues()
{
    const std::string path = join(g_share, "Dokumentation.kalkyl");
    auto created = projectdb::ProjectDb::create(path, "Dokumentation", options());
    CHECK(created.ok());
    if (!created.ok())
        return;
    projectdb::ProjectDb& p = created.value();
    CHECK(!p.isModified());

    CHECK(projectdb::setDocValue(p, {"temp_max", "45", "Källa åäö"}).ok());
    CHECK(p.isModified());
    CHECK(projectdb::setDocValue(p, {"tension_phase", "50", ""}).ok());
    CHECK(projectdb::setDocValue(p, {"temp_max", "60", "Ny källa åäö"}).ok());
    CHECK(projectdb::setDocValue(p, {"temp_min", "-25", ""}).ok());
    CHECK(projectdb::setDocValue(p, {"temp_min", "", ""}).ok());
    CHECK(projectdb::setDocValue(p, {"", "x", ""}).error().code == ErrorCode::Invalid);

    auto values = projectdb::docValues(p);
    CHECK(values.ok());
    if (values.ok()) {
        CHECK(values.value().size() == 2);
        if (values.value().size() == 2) {
            CHECK(values.value()[0].key == "temp_max");
            CHECK(values.value()[0].value == "60");
            CHECK(values.value()[0].reference == "Ny källa åäö");
            CHECK(values.value()[1].key == "tension_phase");
            CHECK(values.value()[1].value == "50");
        }
    }

    CHECK(publishNow(p));
    CHECK(!p.isModified());
    p.close();

    auto reopened = projectdb::ProjectDb::open(path, options());
    CHECK(reopened.ok());
    if (!reopened.ok())
        return;
    auto again = projectdb::docValues(reopened.value());
    CHECK(again.ok() && again.value().size() == 2);
    if (again.ok() && again.value().size() == 2)
        CHECK(again.value()[0].reference == "Ny källa åäö");
}

// Linkopiorna: kopiering, återanvändning med andra versaler, Samma, okänd lina, rensning av
// oanvända linor, uppdatering från registret och ogiltiga linor.
void testConductorCopies()
{
    auto created = projectdb::ProjectDb::create(join(g_share, "Linor.kalkyl"), "Linor", options());
    CHECK(created.ok());
    if (!created.ok())
        return;
    projectdb::ProjectDb& p = created.value();
    const conductor::Conductor base = curlew();

    CHECK(projectdb::setDocConductor(p, projectdb::kPhaseConductorKey, base).ok());
    auto list = projectdb::projectConductors(p);
    CHECK(list.ok() && list.value().size() == 1);
    if (list.ok() && list.value().size() == 1) {
        CHECK(list.value()[0].id > 0);
        CHECK(list.value()[0].name == base.name);
        CHECK(conductor::sameConductorData(list.value()[0], base));
        CHECK(list.value()[0].breakingLoadKn && *list.value()[0].breakingLoadKn == 165);
    }
    checkConductorsConsistent(p);

    conductor::Conductor changed = base;
    changed.name = lowerAscii(base.name);
    changed.diameterMm = 32;
    CHECK(projectdb::setDocConductor(p, projectdb::kTopLeftConductorKey, changed).ok());
    list = projectdb::projectConductors(p);
    CHECK(list.ok() && list.value().size() == 1);
    if (list.ok() && list.value().size() == 1) {
        CHECK(list.value()[0].diameterMm == 31.7);
        CHECK(list.value()[0].name == base.name);
    }
    auto docs = projectdb::docValues(p);
    CHECK(docs.ok() && docs.value().size() == 2);
    if (docs.ok())
        for (const projectdb::DocValue& v : docs.value())
            CHECK(v.value == base.name);
    checkConductorsConsistent(p);

    CHECK(projectdb::setDocValue(p, {"top_conductor_right", "Samma", ""}).ok());
    CHECK(projectdb::setDocValue(p, {"phase_conductor", "Okänd lina", ""}).error().code ==
          ErrorCode::Invalid);
    CHECK(projectdb::setDocValue(p, {"phase_conductor", "Samma", ""}).error().code ==
          ErrorCode::Invalid);
    CHECK(projectdb::setDocConductor(p, "temp_max", base).error().code == ErrorCode::Invalid);
    checkConductorsConsistent(p);

    const conductor::Conductor other = otherConductor();
    CHECK(projectdb::setDocConductor(p, projectdb::kPhaseConductorKey, other).ok());
    list = projectdb::projectConductors(p);
    CHECK(list.ok() && list.value().size() == 2);
    checkConductorsConsistent(p);
    CHECK(projectdb::setDocConductor(p, projectdb::kTopLeftConductorKey, other).ok());
    list = projectdb::projectConductors(p);
    CHECK(list.ok() && list.value().size() == 1);
    if (list.ok() && list.value().size() == 1)
        CHECK(list.value()[0].name == other.name);
    checkConductorsConsistent(p);

    conductor::Conductor fresh = other;
    fresh.diameterMm = 33;
    fresh.type = "Ny typ";
    std::int64_t oldId = list.ok() && !list.value().empty() ? list.value()[0].id : -1;
    CHECK(projectdb::refreshProjectConductor(p, fresh).ok());
    list = projectdb::projectConductors(p);
    CHECK(list.ok() && list.value().size() == 1);
    if (list.ok() && list.value().size() == 1) {
        CHECK(list.value()[0].diameterMm == 33);
        CHECK(list.value()[0].type == "Ny typ");
        CHECK(list.value()[0].id == oldId);
    }
    conductor::Conductor missing = fresh;
    missing.name = "Finns inte";
    CHECK(projectdb::refreshProjectConductor(p, missing).error().code == ErrorCode::NotFound);

    conductor::Conductor bad = base;
    bad.diameterMm = 0;
    CHECK(projectdb::setDocConductor(p, projectdb::kPhaseConductorKey, bad).error().code ==
          ErrorCode::Invalid);
    CHECK(projectdb::refreshProjectConductor(p, bad).error().code == ErrorCode::Invalid);
    checkConductorsConsistent(p);
}

// En beräkning sparas och läses tillbaka med samma värden och ordning, stolpar ersätts vid ny
// sparning, och ingen lina kopieras in i projektet.
void testNormalSpanRoundTrip()
{
    auto created = projectdb::ProjectDb::create(join(g_share, "Beräkning.kalkyl"), "Beräkning", options());
    CHECK(created.ok());
    if (!created.ok())
        return;
    projectdb::ProjectDb& p = created.value();

    projectdb::NormalSpan ns;
    ns.note = "Anteckning åäö";
    ns.limits = {1.6, 0.4, 2.0, 0.9, 1.2};
    ns.poles = exampleC();
    ns.poles[0].heightM = std::nullopt;
    ns.poles.insert(ns.poles.begin() + 2, ancalc::SpanPole{});
    CHECK(ns.poles.size() == 5);

    auto id = projectdb::saveNormalSpan(p, ns);
    CHECK(id.ok());
    if (!id.ok())
        return;
    CHECK(id.value() > 0);

    auto read = projectdb::normalSpan(p, id.value());
    CHECK(read.ok() && read.value().has_value());
    if (!read.ok() || !read.value())
        return;
    const projectdb::NormalSpan& got = *read.value();
    CHECK(got.id == id.value());
    CHECK(got.note == "Anteckning åäö");
    CHECK(got.limits.longest == 1.6 && got.limits.shortest == 0.4 && got.limits.adjacent == 2.0);
    CHECK(got.limits.localMin == 0.9 && got.limits.localMax == 1.2);
    CHECK(got.docKeys.empty());
    CHECK(!got.createdUtc.empty() && !got.modifiedUtc.empty());
    CHECK(countIn(p, "SELECT count(*) FROM project_conductor") == 0);
    CHECK(got.poles.size() == 4);
    if (got.poles.size() == 4) {
        const std::vector<ancalc::SpanPole> want = exampleC();
        for (std::size_t i = 0; i < 4; ++i) {
            CHECK(got.poles[i].pole == want[i].pole);
            CHECK(got.poles[i].lengthM == want[i].lengthM);
            if (i == 0)
                CHECK(!got.poles[i].heightM.has_value());
            else
                CHECK(got.poles[i].heightM == want[i].heightM);
        }
    }

    auto all = projectdb::normalSpans(p);
    CHECK(all.ok() && all.value().size() == 1);
    checkConductorsConsistent(p);

    projectdb::NormalSpan again = got;
    again.poles.pop_back();
    auto id2 = projectdb::saveNormalSpan(p, again);
    CHECK(id2.ok() && id2.value() == id.value());
    read = projectdb::normalSpan(p, id.value());
    CHECK(read.ok() && read.value() && read.value()->poles.size() == 3);
    CHECK(countIn(p, "SELECT count(*) FROM normal_span_pole") == 3);
    CHECK(countIn(p, "SELECT count(*) FROM normal_span") == 1);

    auto none = projectdb::normalSpan(p, 9999);
    CHECK(none.ok() && !none.value().has_value());
    checkConductorsConsistent(p);
}

// Kopplingar mellan rader i Dokumentation och beräkningar: tömt manuellt värde, flera rader per
// beräkning, en beräkning per rad, bortkoppling, okända nycklar och primärnyckeln.
void testNormalSpanLinking()
{
    auto created = projectdb::ProjectDb::create(join(g_share, "Koppling.kalkyl"), "Koppling", options());
    CHECK(created.ok());
    if (!created.ok())
        return;
    projectdb::ProjectDb& p = created.value();
    const std::string before = projectdb::normalSpanDocKey(projectdb::Wire::Phase, projectdb::Stage::Before);
    const std::string after = projectdb::normalSpanDocKey(projectdb::Wire::Phase, projectdb::Stage::After);
    CHECK(before == "ns_calc_phase_before" && after == "ns_calc_phase_after");
    CHECK(projectdb::normalSpanDocKey(projectdb::Wire::TopLeft, projectdb::Stage::After) ==
          "ns_calc_top_left_after");
    CHECK(projectdb::normalSpanDocKey(projectdb::Wire::TopRight, projectdb::Stage::Before) ==
          "ns_calc_top_right_before");

    CHECK(projectdb::setDocValue(p, {before, "343.4", "STO252-532"}).ok());
    projectdb::NormalSpan ns;
    ns.poles = exampleC();
    auto x = projectdb::saveNormalSpan(p, ns, before);
    CHECK(x.ok());
    if (!x.ok())
        return;
    auto docs = projectdb::docValues(p);
    CHECK(docs.ok() && docs.value().size() == 1);
    if (docs.ok() && docs.value().size() == 1) {
        CHECK(docs.value()[0].key == before);
        CHECK(docs.value()[0].value.empty());
        CHECK(docs.value()[0].reference == "STO252-532");
    }

    CHECK(projectdb::linkNormalSpan(p, x.value(), after).ok());
    auto readX = projectdb::normalSpan(p, x.value());
    CHECK(readX.ok() && readX.value() && readX.value()->docKeys.size() == 2);

    auto y = projectdb::saveNormalSpan(p, ns);
    CHECK(y.ok());
    if (!y.ok())
        return;
    CHECK(projectdb::linkNormalSpan(p, y.value(), before).ok());
    auto readY = projectdb::normalSpan(p, y.value());
    CHECK(readY.ok() && readY.value() && readY.value()->docKeys == std::vector<std::string>{before});
    readX = projectdb::normalSpan(p, x.value());
    CHECK(readX.ok() && readX.value() && readX.value()->docKeys == std::vector<std::string>{after});

    CHECK(projectdb::unlinkDocRow(p, after).ok());
    readX = projectdb::normalSpan(p, x.value());
    CHECK(readX.ok() && readX.value() && readX.value()->docKeys.empty());

    CHECK(projectdb::linkNormalSpan(p, y.value(), "temp_max").error().code == ErrorCode::Invalid);
    CHECK(projectdb::linkNormalSpan(p, y.value(), "x").error().code == ErrorCode::Invalid);
    CHECK(projectdb::saveNormalSpan(p, ns, "x").error().code == ErrorCode::Invalid);
    CHECK(countIn(p, "SELECT count(*) FROM normal_span") == 2);

    auto direct = p.local().exec("INSERT INTO doc_normal_span(doc_key, span_id) VALUES('" + before +
                                 "', " + std::to_string(x.value()) + ")");
    CHECK(!direct.ok());
}

// Ta bort en beräkning: stolpar och kopplingar försvinner, saknat id är inget fel, och
// Dokumentationens linor rörs inte.
void testNormalSpanRemove()
{
    auto created = projectdb::ProjectDb::create(join(g_share, "Ta bort.kalkyl"), "Ta bort", options());
    CHECK(created.ok());
    if (!created.ok())
        return;
    projectdb::ProjectDb& p = created.value();
    projectdb::NormalSpan ns;
    ns.poles = exampleC();
    auto id = projectdb::saveNormalSpan(p, ns, "ns_calc_phase_before");
    CHECK(id.ok());
    if (!id.ok())
        return;
    CHECK(projectdb::linkNormalSpan(p, id.value(), "ns_calc_phase_after").ok());
    CHECK(countIn(p, "SELECT count(*) FROM doc_normal_span") == 2);
    CHECK(countIn(p, "SELECT count(*) FROM normal_span_pole") == 4);

    CHECK(projectdb::removeNormalSpan(p, id.value()).ok());
    CHECK(countIn(p, "SELECT count(*) FROM normal_span") == 0);
    CHECK(countIn(p, "SELECT count(*) FROM doc_normal_span") == 0);
    CHECK(countIn(p, "SELECT count(*) FROM normal_span_pole") == 0);
    CHECK(projectdb::removeNormalSpan(p, 9999).ok());

    // Faslinans lina blir kvar när en beräkning sparas och tas bort.
    CHECK(projectdb::setDocConductor(p, projectdb::kPhaseConductorKey, curlew()).ok());
    auto id2 = projectdb::saveNormalSpan(p, ns);
    CHECK(id2.ok());
    if (id2.ok())
        CHECK(projectdb::removeNormalSpan(p, id2.value()).ok());
    CHECK(countIn(p, "SELECT count(*) FROM project_conductor") == 1);
    checkConductorsConsistent(p);
}

// ---- Linberäkningarna ----

// Sant när två tal skiljer sig med högst tol relativt b.
bool relClose(double a, double b, double tol)
{
    return std::fabs(a - b) <= tol * std::fabs(b);
}

// Dotterel, linan i T1 (fas 1, bilaga A).
conductor::Conductor dotterel()
{
    conductor::Conductor c;
    c.name = "124 mm² OPGW AFL 1x48 SMF 14230/20-01 (Dotterel)";
    c.areaMm2 = 124;
    c.diameterMm = 14.9;
    c.eInitialMpa = 89000;
    c.ePermanentMpa = 95000;
    c.eInitialLimitMpa = 180;
    c.eivConstant = 40;
    c.alphaE6PerC = 17.1;
    c.creepTempC = 23.391812865497073;
    c.weightNPerM = 5.34;
    c.breakingLoadKn = 77.7;
    return c;
}

// T1: Dotterel, aₙ 314,6 m, σ₀ 90 MPa, T0 0, min -50 och max +50 med standardmallen.
linecalc::LineInput t1Input(linecalc::CalcMode mode)
{
    linecalc::LineInput in = linecalc::newLineInput(mode);
    in.conductor = dotterel();
    in.normalSpanM = 314.6;
    in.tensionMpa = 90;
    return in;
}

// Ett T1 där varje fält i LineInput, LineConstants och LoadCase har ett värde som skiljer sig
// från förvalet: alla temperaturkällor och vindtryckskällor, L satt och tomt, och manuell last.
linecalc::LineInput variedInput()
{
    using linecalc::PressureSource;
    using linecalc::TempSource;
    linecalc::LineInput in = t1Input(linecalc::CalcMode::Excel);
    in.conductor->type = "Typ åäö";
    in.refTempC = 5;
    in.minTempC = -45;
    in.maxTempC = 65;
    in.shortCircuitTempC = 120;
    in.lineHeightM = 30;
    in.normalWindNPerM2 = 550;
    in.highWindNPerM2 = 850;
    in.constants.iceDensityKgM3 = 900.5;
    in.constants.pi = 3.14159;
    in.constants.gravity = 9.80665;
    in.constants.gustFactor = 1.1;
    in.constants.resonanceFactor = 0.6;
    in.constants.dragFactor = 1.2;
    in.constants.minIceNPerM = 25;
    in.constants.maxUtilizationPercent = 60;
    in.constants.transpositionCase = 3;
    const TempSource temps[] = {TempSource::Fixed, TempSource::Min, TempSource::MinPlus20,
                                TempSource::Max, TempSource::ShortCircuit,
                                TempSource::ShortCircuitRise};
    const PressureSource pressures[] = {PressureSource::Fixed, PressureSource::Normal,
                                        PressureSource::High};
    for (std::size_t i = 0; i < in.cases.size(); ++i) {
        linecalc::LoadCase& c = in.cases[i];
        const double n = static_cast<double>(i);
        c.tempSource = temps[i % 6];
        c.tempC = n + 0.25;
        c.name = "Last  åäö " + std::to_string(i);
        c.standardCase = i % 2 == 0 ? "1a" : "2a";
        c.clearanceCase = "H" + std::to_string(i);
        if (i % 2 == 1)
            c.windFromCase = static_cast<int>(i) + 1;
        else
            c.windFromCase.reset();
        c.windMagnitudePercent = 10 + n;
        c.pressureSource = pressures[i % 3];
        c.charPressureNPerM2 = 100 + n;
        c.refHeightM = 7 + n;
        c.pressureGradientNPerM3 = 0.5 + n;
        c.iceThicknessMm = 3 + n * 0.5;
        c.manual.reset();
    }
    in.cases[3].manual = linecalc::ManualLoad{16.5, 12.25};
    return in;
}

// Kontrollerar att två indata är lika i varje fält.
void checkSameInput(const linecalc::LineInput& a, const linecalc::LineInput& b)
{
    CHECK(a.mode == b.mode);
    CHECK(a.conductor.has_value() == b.conductor.has_value());
    if (a.conductor && b.conductor)
        CHECK(conductor::sameConductorValues(*a.conductor, *b.conductor));
    CHECK(a.normalSpanM == b.normalSpanM);
    CHECK(a.tensionMpa == b.tensionMpa);
    CHECK(a.refTempC == b.refTempC);
    CHECK(a.minTempC == b.minTempC);
    CHECK(a.maxTempC == b.maxTempC);
    CHECK(a.shortCircuitTempC == b.shortCircuitTempC);
    CHECK(a.lineHeightM == b.lineHeightM);
    CHECK(a.normalWindNPerM2 == b.normalWindNPerM2);
    CHECK(a.highWindNPerM2 == b.highWindNPerM2);
    CHECK(a.constants.iceDensityKgM3 == b.constants.iceDensityKgM3);
    CHECK(a.constants.pi == b.constants.pi);
    CHECK(a.constants.gravity == b.constants.gravity);
    CHECK(a.constants.gustFactor == b.constants.gustFactor);
    CHECK(a.constants.resonanceFactor == b.constants.resonanceFactor);
    CHECK(a.constants.dragFactor == b.constants.dragFactor);
    CHECK(a.constants.minIceNPerM == b.constants.minIceNPerM);
    CHECK(a.constants.maxUtilizationPercent == b.constants.maxUtilizationPercent);
    CHECK(a.constants.transpositionCase == b.constants.transpositionCase);
    for (std::size_t i = 0; i < a.cases.size(); ++i) {
        const linecalc::LoadCase& x = a.cases[i];
        const linecalc::LoadCase& y = b.cases[i];
        CHECK(x.tempSource == y.tempSource);
        CHECK(x.tempC == y.tempC);
        CHECK(x.name == y.name);
        CHECK(x.standardCase == y.standardCase);
        CHECK(x.clearanceCase == y.clearanceCase);
        CHECK(x.windFromCase == y.windFromCase);
        CHECK(x.windMagnitudePercent == y.windMagnitudePercent);
        CHECK(x.pressureSource == y.pressureSource);
        CHECK(x.charPressureNPerM2 == y.charPressureNPerM2);
        CHECK(x.refHeightM == y.refHeightM);
        CHECK(x.pressureGradientNPerM3 == y.pressureGradientNPerM3);
        CHECK(x.iceThicknessMm == y.iceThicknessMm);
        CHECK(x.manual.has_value() == y.manual.has_value());
        if (x.manual && y.manual) {
            CHECK(x.manual->iceNPerM == y.manual->iceNPerM);
            CHECK(x.manual->windNPerM == y.manual->windNPerM);
        }
    }
}

// Läser linberäkningen id och svarar med den, eller en tom beräkning när den saknas.
projectdb::LineCalc readCalc(projectdb::ProjectDb& p, std::int64_t id)
{
    auto calc = projectdb::lineCalc(p, id);
    CHECK(calc.ok());
    CHECK(calc.ok() && calc.value().has_value());
    if (calc.ok() && calc.value())
        return *calc.value();
    return {};
}

// En manuell linberäkning av ett T1.
projectdb::LineCalc manualCalc(const linecalc::LineInput& input)
{
    projectdb::LineCalc lc;
    lc.input = input;
    return lc;
}

// En kopplad linberäkning på raden docSource. Indata har ingen lina, aₙ eller σ₀.
projectdb::LineCalc linkedCalc(const std::string& docSource)
{
    projectdb::LineCalc lc;
    lc.docSource = docSource;
    lc.input = linecalc::newLineInput(linecalc::CalcMode::New);
    return lc;
}

// Källor till upplösningen byggda för hand, med Dotterel som enda lina i projektet.
projectdb::DocLineSources makeDocs(std::initializer_list<projectdb::DocValue> values)
{
    projectdb::DocLineSources s;
    s.values = values;
    s.conductors = {dotterel()};
    return s;
}

// Upplysningen för ett fält, eller null.
const linecalc::LineIssue* issueFor(const std::vector<linecalc::LineIssue>& issues,
                                    linecalc::LineField field)
{
    for (const linecalc::LineIssue& issue : issues)
        if (issue.field == field)
            return &issue;
    return nullptr;
}

// Sant när upplösningens källor har fältet med nyckeln.
bool hasSource(const projectdb::ResolvedLineInput& r, projectdb::LineSourceField field,
               const std::string& key)
{
    for (const auto& s : r.sources)
        if (s.field == field && s.docKey == key)
            return true;
    return false;
}

// Skapar ett projekt för ett test och svarar med det, eller ett fel som gör att testet hoppas över.
bool openTestProject(const char* name, std::optional<projectdb::ProjectDb>& out)
{
    auto created = projectdb::ProjectDb::create(join(g_share, std::string(name) + ".kalkyl"), name, options());
    CHECK(created.ok());
    if (!created.ok())
        return false;
    out.emplace(std::move(created.value()));
    return true;
}

// Skapar projektfilen som v0.2.0 hade den: steg 1 och 2, user_version 2 och uppgifter i alla
// tabeller från steg 2. Namnet är åäö-v020, och normalspannet har egna gränser.
bool writeV020Project(const std::string& path)
{
    db::OpenOptions o;
    o.mode = db::OpenMode::CreateNew;
    auto d = db::Database::open(path, o);
    CHECK(d.ok());
    if (!d.ok())
        return false;
    const std::string head =
        "PRAGMA application_id = " + std::to_string(projectdb::kApplicationId) +
        "; PRAGMA user_version = 2; "
        "CREATE TABLE project (id INTEGER PRIMARY KEY CHECK (id = 1), name TEXT NOT NULL, "
        "created_utc TEXT NOT NULL, created_with TEXT NOT NULL); ";
    bool ok = d.value().exec(head).ok();
    ok = ok && d.value().exec(std::string(projectdb::dev::kStep2)).ok();
    const std::string data = R"sql(
INSERT INTO project VALUES (1, 'åäö-v020', '2026-11-24T12:00:00Z', 'Kalkyl 0.2.0');
INSERT INTO project_conductor(name, type, area_mm2, diameter_mm, e_initial_mpa, e_permanent_mpa,
                              e_initial_limit_mpa, eiv_constant, alpha_e6_per_c,
                              creep_strain_permille, creep_temp_c, weight_n_per_m,
                              breaking_load_kn, copied_utc)
VALUES ('593 mm² FeAl CURLEW', 'FeAl CURLEW', 593, 31.7, 52000, 72000, 120, 90, 19, 0.4, 21,
        19.6, 165, '2026-11-24T12:00:00Z');
INSERT INTO doc_value(key, value, reference) VALUES
    ('phase_conductor', '593 mm² FeAl CURLEW', ''),
    ('tension_phase', '90', 'Ref åäö'),
    ('temp_max', '50', ''),
    ('line_calc_phase_before', '354', 'Excel');
INSERT INTO normal_span(id, note, limit_longest, limit_shortest, limit_adjacent, limit_local_min,
                        limit_local_max, created_utc, modified_utc)
VALUES (1, 'Exempel C åäö', 1.6, 0.4, 2.2, 0.9, 1.2, '2026-11-24T12:00:00Z', '2026-11-24T12:00:00Z');
INSERT INTO normal_span_pole(span_id, position, pole, length_m, height_m) VALUES
    (1, 0, 'P0', 250, 10), (1, 1, 'P1', 300, 20), (1, 2, 'P2', 200, 15), (1, 3, 'P3', 400, 30);
INSERT INTO doc_normal_span(doc_key, span_id) VALUES ('ns_calc_phase_before', 1);
)sql";
    ok = ok && d.value().exec(data).ok();
    CHECK(ok);
    return ok;
}

// Kontrollerar att allt som v0.2.0-filen innehöll finns kvar i det öppnade projektet.
void checkV020Data(projectdb::ProjectDb& p)
{
    CHECK(p.info().name == "åäö-v020");
    CHECK(p.info().createdWith == "Kalkyl 0.2.0");
    auto conductors = projectdb::projectConductors(p);
    CHECK(conductors.ok() && conductors.value().size() == 1);
    if (conductors.ok() && conductors.value().size() == 1) {
        conductor::Conductor expected = curlew();
        CHECK(conductor::sameConductorValues(conductors.value()[0], expected));
    }
    auto values = projectdb::docValues(p);
    CHECK(values.ok() && values.value().size() == 4);
    if (values.ok()) {
        for (const auto& v : values.value()) {
            if (v.key == "phase_conductor")
                CHECK(v.value == curlew().name);
            else if (v.key == "tension_phase")
                CHECK(v.value == "90" && v.reference == "Ref åäö");
            else if (v.key == "temp_max")
                CHECK(v.value == "50");
            else if (v.key == "line_calc_phase_before")
                CHECK(v.value == "354" && v.reference == "Excel");
            else
                CHECK(false);
        }
    }
    auto spans = projectdb::normalSpans(p);
    CHECK(spans.ok() && spans.value().size() == 1);
    if (spans.ok() && spans.value().size() == 1) {
        const projectdb::NormalSpan& ns = spans.value()[0];
        CHECK(ns.note == "Exempel C åäö");
        CHECK(ns.poles.size() == 4);
        CHECK(ns.limits.longest == 1.6 && ns.limits.shortest == 0.4 && ns.limits.adjacent == 2.2);
        CHECK(ns.limits.localMin == 0.9 && ns.limits.localMax == 1.2);
        CHECK(ns.docKeys.size() == 1 && ns.docKeys[0] == "ns_calc_phase_before");
    }
}

// Ett projekt som v0.2.0 hade det migreras till det senaste steget: allt gammalt finns kvar, de
// nya delarna fungerar, och originalet har det nya user_version efter publicering och ny öppning.
void testMigrateFromV020()
{
    const std::string path = join(g_share, "Från 0.2.0.kalkyl");
    if (!writeV020Project(path))
        return;
    std::int64_t savedId = 0;
    {
        auto opened = projectdb::ProjectDb::open(path, options());
        CHECK(opened.ok());
        if (!opened.ok()) {
            std::cerr << opened.error().message << " / " << opened.error().detail << std::endl;
            return;
        }
        projectdb::ProjectDb& p = opened.value();
        CHECK(countIn(p, "PRAGMA user_version") == projectdb::kSchemaVersion);
        CHECK(countIn(p, kNineteenTablesSql) == 19);
        checkV020Data(p);

        auto none = projectdb::lineCalcs(p);
        CHECK(none.ok() && none.value().empty());
        auto id = projectdb::saveLineCalc(p, manualCalc(t1Input(linecalc::CalcMode::Excel)),
                                          "line_calc_phase_after");
        CHECK(id.ok());
        if (id.ok()) {
            savedId = id.value();
            const projectdb::LineCalc back = readCalc(p, savedId);
            checkSameInput(back.input, t1Input(linecalc::CalcMode::Excel));
            CHECK(back.docKeys.size() == 1 && back.docKeys[0] == "line_calc_phase_after");
        }
        CHECK(publishNow(p));
        p.close();
    }
    CHECK(scalar(path, "PRAGMA user_version") == projectdb::kSchemaVersion);
    CHECK(scalar(path, "SELECT count(*) FROM project WHERE name = 'åäö-v020'") == 1);
    auto reopened = projectdb::ProjectDb::open(path, options());
    CHECK(reopened.ok());
    if (!reopened.ok())
        return;
    checkV020Data(reopened.value());
    const projectdb::LineCalc back = readCalc(reopened.value(), savedId);
    checkSameInput(back.input, t1Input(linecalc::CalcMode::Excel));
    CHECK(back.docKeys.size() == 1 && back.docKeys[0] == "line_calc_phase_after");
}

// ---- Stolptabell före ----

// En rad i stolptabellen med de åtta fälten i tabellens ordning.
poletable::PoleRow poleRow(const char* pole, const char* av, const char* ah, const char* span,
                           const char* north, const char* east, const char* height, const char* lkl)
{
    poletable::PoleRow row;
    row.pole = pole;
    row.av = av;
    row.ah = ah;
    row.spanM = span;
    row.north = north;
    row.east = east;
    row.height = height;
    row.lkl = lkl;
    return row;
}

// Tre stolpar med tio decimaler i Koordinat N, 44.10 med avslutande nolla, negativt tal, noll,
// tomma tal och text med åäö. Raderna har ingen tom rad.
std::vector<poletable::PoleRow> tenDecimalRows()
{
    return {poleRow("ST0378", "363", "362", "348", "6512240.8500000000", "412181.98", "44.19", "255/262"),
            poleRow("Åkerö-ÄÖ", "", "-12.5", "0", "", "", "", ""),
            poleRow("ST0381", "44.10", "", "", "6511247.0800000000", "411804.35", "30.01", "262/262")};
}

// Raderna med id 1, 2, 3 ... i tabellens ordning: det Stolptabell efter ger tillbaka när alla rader
// saknade id vid sparningen.
std::vector<poletable::PoleRow> withIds(std::vector<poletable::PoleRow> rows)
{
    for (std::size_t i = 0; i < rows.size(); ++i)
        rows[i].id = static_cast<std::int64_t>(i) + 1;
    return rows;
}

// Raderna med id 0, för jämförelser av fälten.
std::vector<poletable::PoleRow> withoutIds(std::vector<poletable::PoleRow> rows)
{
    for (poletable::PoleRow& row : rows)
        row.id = 0;
    return rows;
}

// Sparar och läser tillbaka Stolptabell före: en ny tabell är tom, alla fält kommer tillbaka
// exakt, tomma rader mitt i finns kvar och tomma rader sist är borta, talen lagras som text,
// tomma tal som NULL, och en ny sparning ersätter allt.
void testPoleTableRoundTrip()
{
    std::optional<projectdb::ProjectDb> opened;
    if (!openTestProject("Stolptabell", opened))
        return;
    projectdb::ProjectDb& p = *opened;

    auto none = projectdb::polesBefore(p);
    CHECK(none.ok() && none.value().empty());

    std::vector<poletable::PoleRow> rows = tenDecimalRows();
    rows.insert(rows.begin() + 2, poletable::PoleRow{});   // tom rad mitt i
    rows.push_back({});                                    // två tomma rader sist
    rows.push_back({});
    CHECK(projectdb::setPolesBefore(p, rows).ok());

    auto back = projectdb::polesBefore(p);
    CHECK(back.ok());
    if (back.ok()) {
        const std::vector<poletable::PoleRow> expected = {rows[0], rows[1], rows[2], rows[3]};
        CHECK(back.value().size() == 4);
        CHECK(back.value() == expected);
        if (back.value().size() == 4) {
            CHECK(poletable::isEmptyRow(back.value()[2]));
            CHECK(back.value()[0].north == "6512240.8500000000");
            CHECK(back.value()[1].pole == "Åkerö-ÄÖ" && back.value()[1].ah == "-12.5");
            CHECK(back.value()[1].spanM == "0");
            CHECK(back.value()[3].av == "44.10");
        }
    }
    CHECK(countIn(p, "SELECT count(*) FROM pole_before") == 4);
    CHECK(countIn(p, "SELECT max(position) FROM pole_before") == 3);
    CHECK(countIn(p, "SELECT count(*) FROM pole_before WHERE position = 0 AND "
                     "coord_n = '6512240.8500000000' AND typeof(coord_n) = 'text'") == 1);
    CHECK(countIn(p, "SELECT count(*) FROM pole_before WHERE position = 1 AND "
                     "typeof(span_m) = 'text' AND span_m = '0' AND typeof(coord_n) = 'null'") == 1);
    CHECK(countIn(p, "SELECT count(*) FROM pole_before WHERE position = 3 AND av = '44.10' AND "
                     "typeof(av) = 'text' AND typeof(ah) = 'null'") == 1);
    CHECK(countIn(p, "SELECT count(*) FROM pole_before WHERE position = 2 AND pole = '' AND lkl = '' AND "
                     "av IS NULL AND ah IS NULL AND span_m IS NULL AND coord_n IS NULL AND "
                     "coord_e IS NULL AND coord_h IS NULL") == 1);

    // Publicering, stängning och ny öppning bevarar tabellen.
    CHECK(publishNow(p));
    const std::string path = p.path();
    p.close();
    auto reopened = projectdb::ProjectDb::open(path, options());
    CHECK(reopened.ok());
    if (reopened.ok()) {
        auto again = projectdb::polesBefore(reopened.value());
        CHECK(again.ok() && again.value().size() == 4);
        if (again.ok() && back.ok())
            CHECK(again.value() == back.value());

        // En ny sparning ersätter allt, och en tom lista tömmer tabellen.
        CHECK(projectdb::setPolesBefore(reopened.value(), {poleRow("ny", "1", "", "", "", "", "", "")}).ok());
        auto replaced = projectdb::polesBefore(reopened.value());
        CHECK(replaced.ok() && replaced.value().size() == 1 && replaced.value()[0].pole == "ny");
        CHECK(countIn(reopened.value(), "SELECT count(*) FROM pole_before") == 1);
        CHECK(projectdb::setPolesBefore(reopened.value(), {}).ok());
        CHECK(countIn(reopened.value(), "SELECT count(*) FROM pole_before") == 0);
        // Bara tomma rader sparas inte heller.
        CHECK(projectdb::setPolesBefore(reopened.value(), {poletable::PoleRow{}, poletable::PoleRow{}}).ok());
        CHECK(countIn(reopened.value(), "SELECT count(*) FROM pole_before") == 0);
    }
}

// ---- Automatiskt läge ----

using projectdb::Stage;
using projectdb::Wire;

// Bilaga A i planen: tio stolpar ST0100 till ST0109 med spannlängder, H och LKL.
std::vector<poletable::PoleRow> annexRows()
{
    return {poleRow("ST0100", "300", "300", "", "6512000.00", "412000.00", "40.0", "255/262"),
            poleRow("ST0101", "310", "310", "300", "6511700.00", "411950.00", "42.5", ""),
            poleRow("ST0102", "300", "300", "320", "6511380.00", "411900.00", "45.0", "262/262"),
            poleRow("ST0103", "315", "315", "280", "6511100.00", "411860.00", "43.0", ""),
            poleRow("ST0104", "330", "330", "350", "6510750.00", "411810.00", "47.5", "262/262"),
            poleRow("ST0105", "300", "300", "310", "6510440.00", "411770.00", "50.0", ""),
            poleRow("ST0106", "310", "310", "290", "6510150.00", "411730.00", "48.0", "241/241"),
            poleRow("ST0107", "315", "315", "330", "6509820.00", "411690.00", "46.5", ""),
            poleRow("ST0108", "285", "285", "300", "6509520.00", "411650.00", "44.0", ""),
            poleRow("ST0109", "270", "270", "270", "6509250.00", "411610.00", "41.0", "255/262")};
}

// Läget på med faslinan ST0102–ST0107 och topplinorna ST0101–ST0108.
projectdb::AutoSettings standardSettings()
{
    projectdb::AutoSettings s;
    s.enabled = true;
    s.ranges[0] = {"ST0102", "ST0107"};
    s.ranges[1] = {"ST0101", "ST0108"};
    return s;
}

// Lagringstexten för skedet och linan i auto_calc.
const char* stageName(Stage stage)
{
    return stage == Stage::Before ? "before" : "after";
}
const char* wireName(Wire wire)
{
    return wire == Wire::Phase ? "phase" : wire == Wire::TopLeft ? "top_left" : "top_right";
}

// Id:na som auto_calc har reserverat för skedet och linan.
struct Reserved {
    bool exists = false;
    std::int64_t spanId = 0;
    std::int64_t calcId = 0;
};
Reserved reservedIds(projectdb::ProjectDb& p, Stage stage, Wire wire)
{
    const std::string from = std::string(" FROM auto_calc WHERE stage = '") + stageName(stage) +
                             "' AND wire = '" + wireName(wire) + "'";
    Reserved r;
    r.exists = countIn(p, "SELECT count(*)" + from) == 1;
    if (r.exists) {
        r.spanId = countIn(p, "SELECT span_id" + from);
        r.calcId = countIn(p, "SELECT calc_id" + from);
    }
    return r;
}

// Sant när topplina höger är Samma (ingen rad, tom eller Samma).
bool topIsSame(projectdb::ProjectDb& p)
{
    auto values = projectdb::docValues(p);
    CHECK(values.ok());
    if (values.ok())
        for (const auto& v : values.value())
            if (v.key == projectdb::kTopRightConductorKey)
                return v.value.empty() || v.value == projectdb::kSameConductor;
    return true;
}

// Anteckningen på en automatisk beräkning enligt planen 7.7.
std::string expectedNote(Stage stage, Wire wire, bool same, const automode::PoleRange& range)
{
    const std::string word = wire == Wire::Phase     ? "faslinan"
                             : wire == Wire::TopLeft ? (same ? "topplinorna" : "topplina vänster")
                                                     : "topplina höger";
    const std::string table = stage == Stage::Before ? "Stolptabell före" : "Stolptabell efter";
    const std::string note = "Automatisk · " + table + " · " + word;
    if (range.fromPole.empty() || range.toPole.empty())
        return note + ", sträckan är inte vald";
    return note + " " + range.fromPole + "–" + range.toPole;
}

// Jämför de lagrade automatiska beräkningarna med det som stolptabellerna, sträckorna och
// Samma ger: stolpar, sträckans läsning och anteckningen för varje aktiv lina, och att inaktiva
// linor inte har några rader i normal_span och line_calc.
void checkAutoMatches(projectdb::ProjectDb& p)
{
    const bool same = topIsSame(p);
    for (Stage stage : {Stage::Before, Stage::After}) {
        auto settings = projectdb::autoSettings(p, stage);
        auto rows = projectdb::poles(p, stage);
        CHECK(settings.ok() && rows.ok());
        if (!settings.ok() || !rows.ok())
            return;
        for (Wire wire : {Wire::Phase, Wire::TopLeft, Wire::TopRight}) {
            const std::size_t index = wire == Wire::Phase ? 0 : wire == Wire::TopLeft ? 1 : 2;
            const bool active = settings.value().enabled && (wire != Wire::TopRight || !same);
            const Reserved r = reservedIds(p, stage, wire);
            if (!active) {
                if (r.exists) {
                    CHECK(countIn(p, "SELECT count(*) FROM normal_span WHERE id = " + std::to_string(r.spanId)) == 0);
                    CHECK(countIn(p, "SELECT count(*) FROM line_calc WHERE id = " + std::to_string(r.calcId)) == 0);
                }
                continue;
            }
            CHECK(r.exists);
            if (!r.exists)
                continue;
            const automode::PoleRange range = settings.value().ranges[index];
            const automode::RangeSpan span = automode::rangeSpan(
                rows.value(), range, projectdb::autoLineWord(wire, same), projectdb::poleTableName(stage));
            std::vector<ancalc::SpanPole> expected;
            for (const ancalc::SpanPole& pole : span.poles)
                if (!pole.pole.empty() || pole.lengthM || pole.heightM)
                    expected.push_back(pole);

            auto ns = projectdb::normalSpan(p, r.spanId);
            CHECK(ns.ok() && ns.value().has_value());
            if (ns.ok() && ns.value()) {
                const projectdb::NormalSpan& n = *ns.value();
                CHECK(n.automatic.has_value());
                CHECK(n.poles.size() == expected.size());
                for (std::size_t i = 0; i < n.poles.size() && i < expected.size(); ++i) {
                    CHECK(n.poles[i].pole == expected[i].pole);
                    CHECK(n.poles[i].lengthM == expected[i].lengthM);
                    CHECK(n.poles[i].heightM == expected[i].heightM);
                }
                if (n.automatic) {
                    CHECK(n.automatic->stage == stage && n.automatic->wire == wire);
                    CHECK(n.automatic->range == range);
                    CHECK(n.automatic->issue == span.rows.issue);
                    CHECK(n.automatic->firstRow == span.rows.first);
                    CHECK(n.automatic->lastRow == span.rows.last);
                }
                CHECK(n.note == expectedNote(stage, wire, same, range));
                CHECK(n.docKeys.size() == 1 && n.docKeys[0] == projectdb::normalSpanDocKey(wire, stage));
            }
            auto lc = projectdb::lineCalc(p, r.calcId);
            CHECK(lc.ok() && lc.value().has_value());
            if (lc.ok() && lc.value()) {
                CHECK(lc.value()->automatic.has_value());
                CHECK(lc.value()->note == expectedNote(stage, wire, same, range));
                CHECK(lc.value()->docSource == projectdb::lineCalcDocKey(wire, stage));
            }
        }
    }
}

// Sant när den första automatiska normalspannsberäkningen i listan hör till skedet och linan.
const projectdb::NormalSpan* findAutoSpan(const std::vector<projectdb::NormalSpan>& spans, Stage stage,
                                          Wire wire)
{
    for (const projectdb::NormalSpan& ns : spans)
        if (ns.automatic && ns.automatic->stage == stage && ns.automatic->wire == wire)
            return &ns;
    return nullptr;
}

// Antal automatiska beräkningar i listan.
int autoCount(const std::vector<projectdb::NormalSpan>& spans)
{
    int n = 0;
    for (const projectdb::NormalSpan& ns : spans)
        if (ns.automatic)
            ++n;
    return n;
}

// Sant när två tal är lika inom en tusendel.
bool closeTo(double a, double b)
{
    return std::fabs(a - b) < 1e-3;
}

// Stolptabell efter har egen lagring: en ny tabell är tom, alla fält kommer tillbaka exakt som
// för före, och de två tabellerna påverkar inte varandra.
void testPolesAfterRoundTrip()
{
    std::optional<projectdb::ProjectDb> opened;
    if (!openTestProject("Stolptabell efter", opened))
        return;
    projectdb::ProjectDb& p = *opened;

    auto none = projectdb::poles(p, Stage::After);
    CHECK(none.ok() && none.value().empty());

    std::vector<poletable::PoleRow> rows = tenDecimalRows();
    rows.insert(rows.begin() + 1, poletable::PoleRow{});   // tom rad mitt i
    rows.push_back({});                                    // tom rad sist
    CHECK(projectdb::setPoles(p, Stage::After, rows).ok());
    auto back = projectdb::poles(p, Stage::After);
    CHECK(back.ok());
    if (back.ok()) {
        const std::vector<poletable::PoleRow> expected = withIds({rows[0], rows[1], rows[2], rows[3]});
        CHECK(back.value() == expected);
        CHECK(withoutIds(back.value()) == std::vector<poletable::PoleRow>({rows[0], rows[1], rows[2], rows[3]}));
    }
    CHECK(countIn(p, "SELECT count(*) FROM pole_after") == 4);
    CHECK(countIn(p, "SELECT count(*) FROM pole_after WHERE position = 0 AND "
                     "coord_n = '6512240.8500000000' AND typeof(coord_n) = 'text'") == 1);
    CHECK(countIn(p, "SELECT count(*) FROM pole_after WHERE position = 1 AND pole = '' AND av IS NULL") == 1);
    auto before = projectdb::polesBefore(p);
    CHECK(before.ok() && before.value().empty());
    CHECK(countIn(p, "SELECT count(*) FROM pole_before") == 0);

    // Före ändras utan att efter rörs.
    CHECK(projectdb::setPolesBefore(p, {poleRow("ny", "1", "", "", "", "", "", "")}).ok());
    auto still = projectdb::poles(p, Stage::After);
    CHECK(still.ok() && back.ok() && still.value() == back.value());
    CHECK(projectdb::poles(p, Stage::Before).ok() && projectdb::poles(p, Stage::Before).value().size() == 1);

    CHECK(publishNow(p));
    const std::string path = p.path();
    p.close();
    auto reopened = projectdb::ProjectDb::open(path, options());
    CHECK(reopened.ok());
    if (reopened.ok()) {
        auto again = projectdb::poles(reopened.value(), Stage::After);
        CHECK(again.ok() && back.ok() && again.value() == back.value());
        CHECK(projectdb::setPoles(reopened.value(), Stage::After, {}).ok());
        CHECK(countIn(reopened.value(), "SELECT count(*) FROM pole_after") == 0);
        CHECK(countIn(reopened.value(), "SELECT count(*) FROM pole_before") == 1);
    }
}

// Inställningen för automatiskt läge: förvalet, trimning av Stolpnr, åäö, och att skedena är
// oberoende. Utan stolptabell ger ett läge på inga stolpar men en rad per aktiv lina.
void testAutoSettingsRoundTrip()
{
    std::optional<projectdb::ProjectDb> opened;
    if (!openTestProject("Inställning", opened))
        return;
    projectdb::ProjectDb& p = *opened;

    for (Stage stage : {Stage::Before, Stage::After}) {
        auto def = projectdb::autoSettings(p, stage);
        CHECK(def.ok() && def.value() == projectdb::AutoSettings{});
        CHECK(def.ok() && !def.value().enabled && def.value().ranges[0].fromPole.empty());
    }
    CHECK(countIn(p, "SELECT count(*) FROM auto_mode") == 0);

    projectdb::AutoSettings s;
    s.enabled = true;
    s.ranges[0] = {"  ST0102 ", "ST0107" + std::string(1, char{9})};
    s.ranges[1] = {"Åkerö-ÄÖ", " ST0108"};
    s.ranges[2] = {"ST0001", "ST0002"};
    CHECK(projectdb::setAutoSettings(p, Stage::Before, s).ok());
    auto back = projectdb::autoSettings(p, Stage::Before);
    CHECK(back.ok());
    if (back.ok()) {
        CHECK(back.value().enabled);
        CHECK(back.value().ranges[0].fromPole == "ST0102" && back.value().ranges[0].toPole == "ST0107");
        CHECK(back.value().ranges[1].fromPole == "Åkerö-ÄÖ" && back.value().ranges[1].toPole == "ST0108");
        CHECK(back.value().ranges[2].fromPole == "ST0001" && back.value().ranges[2].toPole == "ST0002");
    }
    auto after = projectdb::autoSettings(p, Stage::After);
    CHECK(after.ok() && after.value() == projectdb::AutoSettings{});

    projectdb::AutoSettings off;
    off.ranges[0] = {"A", "B"};
    CHECK(projectdb::setAutoSettings(p, Stage::After, off).ok());
    auto afterBack = projectdb::autoSettings(p, Stage::After);
    CHECK(afterBack.ok() && afterBack.value() == off && !afterBack.value().enabled);
    auto beforeAgain = projectdb::autoSettings(p, Stage::Before);
    CHECK(beforeAgain.ok() && back.ok() && beforeAgain.value() == back.value());
    CHECK(countIn(p, "SELECT count(*) FROM auto_mode") == 2);

    // Läget av ger inga automatiska beräkningar, läget på två rader (Samma) utan stolptabell.
    CHECK(countIn(p, "SELECT count(*) FROM normal_span") == 2);
    auto spans = projectdb::normalSpans(p);
    CHECK(spans.ok() && autoCount(spans.value()) == 2);
    if (spans.ok()) {
        CHECK(findAutoSpan(spans.value(), Stage::Before, Wire::Phase) != nullptr);
        const projectdb::NormalSpan* top = findAutoSpan(spans.value(), Stage::Before, Wire::TopLeft);
        CHECK(top != nullptr && top->poles.empty());
        if (top && top->automatic)
            CHECK(top->automatic->issue == "Stolpnr Åkerö-ÄÖ finns inte i Stolptabell före.");
    }
    checkAutoMatches(p);
}

// Stolptabell efter blir en kopia av före: rader, sträckor och läge ersätts, före är orört, och de
// automatiska beräkningarna i efter skapas med stabila id.
void testCopyBeforeToAfter()
{
    std::optional<projectdb::ProjectDb> opened;
    if (!openTestProject("Kopiera före", opened))
        return;
    projectdb::ProjectDb& p = *opened;

    CHECK(projectdb::setPoles(p, Stage::Before, annexRows()).ok());
    CHECK(projectdb::setAutoSettings(p, Stage::Before, standardSettings()).ok());
    // Efter har andra rader och en egen sträcka med läget på.
    CHECK(projectdb::setPoles(p, Stage::After, tenDecimalRows()).ok());
    projectdb::AutoSettings other;
    other.enabled = true;
    other.ranges[0] = {"ST0378", "ST0381"};
    CHECK(projectdb::setAutoSettings(p, Stage::After, other).ok());
    const Reserved oldPhase = reservedIds(p, Stage::After, Wire::Phase);
    CHECK(oldPhase.exists);
    checkAutoMatches(p);

    CHECK(projectdb::copyBeforeToAfter(p).ok());
    auto after = projectdb::poles(p, Stage::After);
    std::vector<poletable::PoleRow> copied = annexRows();   // Av i alla tre faserna
    for (poletable::PoleRow& row : copied) {
        row.av2 = row.av;
        row.av3 = row.av;
    }
    CHECK(after.ok() && after.value() == withIds(copied));
    auto before = projectdb::polesBefore(p);
    CHECK(before.ok() && before.value() == annexRows());
    auto afterSettings = projectdb::autoSettings(p, Stage::After);
    CHECK(afterSettings.ok() && afterSettings.value() == standardSettings());
    auto beforeSettings = projectdb::autoSettings(p, Stage::Before);
    CHECK(beforeSettings.ok() && beforeSettings.value() == standardSettings());
    CHECK(countIn(p, "SELECT count(*) FROM pole_after") == 10);
    CHECK(countIn(p, "SELECT count(*) FROM pole_after WHERE pole = 'Åkerö-ÄÖ'") == 0);

    auto spans = projectdb::normalSpans(p);
    CHECK(spans.ok() && autoCount(spans.value()) == 4);
    if (spans.ok()) {
        const projectdb::NormalSpan* phase = findAutoSpan(spans.value(), Stage::After, Wire::Phase);
        CHECK(phase != nullptr && phase->id == oldPhase.spanId);
        CHECK(phase != nullptr && phase->poles.size() == 6);
        if (phase)
            CHECK(phase->note == "Automatisk · Stolptabell efter · faslinan ST0102–ST0107");
    }
    checkAutoMatches(p);

    // Läget av i före följer med: efter blir av och beräkningarna tas bort, id:na är reserverade.
    projectdb::AutoSettings off = standardSettings();
    off.enabled = false;
    CHECK(projectdb::setAutoSettings(p, Stage::Before, off).ok());
    CHECK(projectdb::copyBeforeToAfter(p).ok());
    auto offAfter = projectdb::autoSettings(p, Stage::After);
    CHECK(offAfter.ok() && offAfter.value() == off);
    auto none = projectdb::normalSpans(p);
    CHECK(none.ok() && none.value().empty());
    CHECK(countIn(p, "SELECT count(*) FROM auto_calc WHERE stage = 'after'") == 2);
    checkAutoMatches(p);

    // Utan någon rad i auto_mode för före blir efter också utan rad.
    CHECK(p.local().exec("DELETE FROM auto_mode WHERE stage = 'before'").ok());
    CHECK(projectdb::copyBeforeToAfter(p).ok());
    CHECK(countIn(p, "SELECT count(*) FROM auto_mode WHERE stage = 'after'") == 0);
}

// Synkningen efter varje sorts ändring: stolpar, anteckning och sträckans läsning följer
// tabellen, id:na står kvar, och bara det som ändrats får ny modified_utc.
void testAutoCalcsSync()
{
    std::optional<projectdb::ProjectDb> opened;
    if (!openTestProject("Automatisk synk", opened))
        return;
    projectdb::ProjectDb& p = *opened;
    const std::string dummy = "2000-01-01T00:00:00Z";
    const auto freeze = [&]() {
        CHECK(p.local().exec("UPDATE normal_span SET modified_utc = '" + dummy + "'").ok());
        CHECK(p.local().exec("UPDATE line_calc SET modified_utc = '" + dummy + "'").ok());
    };

    CHECK(projectdb::setPoles(p, Stage::Before, annexRows()).ok());
    auto idle = projectdb::normalSpans(p);
    CHECK(idle.ok() && idle.value().empty());
    CHECK(countIn(p, "SELECT count(*) FROM auto_calc") == 0);
    CHECK(projectdb::setAutoSettings(p, Stage::Before, standardSettings()).ok());

    // Två automatiska beräkningar först i listan, med stolpar, anteckning och rader.
    auto spans = projectdb::normalSpans(p);
    CHECK(spans.ok() && spans.value().size() == 2 && autoCount(spans.value()) == 2);
    if (!spans.ok() || spans.value().size() != 2)
        return;
    const projectdb::NormalSpan& phase = spans.value()[0];
    const projectdb::NormalSpan& top = spans.value()[1];
    CHECK(phase.id > 0 && top.id > 0 && phase.id != top.id);
    CHECK(phase.automatic && phase.automatic->wire == Wire::Phase && phase.automatic->stage == Stage::Before);
    CHECK(top.automatic && top.automatic->wire == Wire::TopLeft);
    CHECK(phase.note == "Automatisk · Stolptabell före · faslinan ST0102–ST0107");
    CHECK(top.note == "Automatisk · Stolptabell före · topplinorna ST0101–ST0108");
    CHECK(phase.docKeys.size() == 1 && phase.docKeys[0] == "ns_calc_phase_before");
    CHECK(top.docKeys.size() == 1 && top.docKeys[0] == "ns_calc_top_left_before");
    CHECK(phase.poles.size() == 6 && top.poles.size() == 8);
    CHECK(phase.automatic && phase.automatic->firstRow == 2 && phase.automatic->lastRow == 7);
    CHECK(phase.automatic && phase.automatic->issue.empty());
    CHECK(phase.limits.longest == 1.5 && phase.limits.shortest == 0.5 && phase.limits.adjacent == 2.5);
    const auto phaseCalc = projectdb::calculateSpan(phase);
    const auto topCalc = projectdb::calculateSpan(top);
    CHECK(phaseCalc.result && closeTo(phaseCalc.result->an, 315.2) && closeTo(phaseCalc.result->anExact, 315.1557));
    CHECK(topCalc.result && closeTo(topCalc.result->an, 313.8));
    checkAutoMatches(p);

    // Linberäkningarna: automatiska först, kopplade till sin rad, med förvalen som indata.
    auto calcs = projectdb::lineCalcs(p);
    CHECK(calcs.ok() && calcs.value().size() == 2);
    if (calcs.ok() && calcs.value().size() == 2) {
        const projectdb::LineCalc& lc = calcs.value()[0];
        CHECK(lc.automatic && lc.automatic->wire == Wire::Phase);
        CHECK(lc.docSource == "line_calc_phase_before");
        CHECK(lc.docKeys.size() == 1 && lc.docKeys[0] == "line_calc_phase_before");
        CHECK(lc.note == phase.note);
        checkSameInput(lc.input, linecalc::newLineInput());
        auto docs = projectdb::docLineSources(p);
        CHECK(docs.ok());
        if (docs.ok()) {
            bool found = false;
            for (const auto& [key, an] : docs.value().normalSpans)
                if (key == "ns_calc_phase_before") {
                    found = an.has_value() && closeTo(*an, 315.2);
                }
            CHECK(found);
            CHECK(std::find(docs.value().autoSpanKeys.begin(), docs.value().autoSpanKeys.end(),
                            "ns_calc_phase_before") != docs.value().autoSpanKeys.end());
            const projectdb::ResolvedLineInput r = projectdb::resolveLineInput(lc.input, lc.docSource, docs.value());
            CHECK(r.input.normalSpanM && closeTo(*r.input.normalSpanM, 315.2));
            CHECK(hasSource(r, projectdb::LineSourceField::NormalSpan, "ns_calc_phase_before"));
        }
    }

    // ST0104 = 365 och ST0105 = 295 ger aₙ 316,8 och 315,0 och ny modified_utc. Id:na står kvar.
    freeze();
    std::vector<poletable::PoleRow> rows = annexRows();
    rows[4].spanM = "365";
    rows[5].spanM = "295";
    CHECK(projectdb::setPoles(p, Stage::Before, rows).ok());
    auto changed = projectdb::normalSpans(p);
    CHECK(changed.ok() && changed.value().size() == 2);
    if (changed.ok() && changed.value().size() == 2) {
        CHECK(changed.value()[0].id == phase.id && changed.value()[1].id == top.id);
        const auto c0 = projectdb::calculateSpan(changed.value()[0]);
        const auto c1 = projectdb::calculateSpan(changed.value()[1]);
        CHECK(c0.result && closeTo(c0.result->an, 316.8));
        CHECK(c1.result && closeTo(c1.result->an, 315.0));
        CHECK(changed.value()[0].modifiedUtc != dummy && changed.value()[1].modifiedUtc != dummy);
        CHECK(changed.value()[0].createdUtc == phase.createdUtc);
    }
    checkAutoMatches(p);

    // Bara Av, Ah, N, E och LKL ändras: stolparna är desamma och modified_utc rörs inte.
    freeze();
    rows[3].av = "999";
    rows[3].lkl = "ändrad åäö";
    rows[2].north = "1.5";
    CHECK(projectdb::setPoles(p, Stage::Before, rows).ok());
    auto quiet = projectdb::normalSpans(p);
    CHECK(quiet.ok() && quiet.value().size() == 2);
    if (quiet.ok() && quiet.value().size() == 2)
        CHECK(quiet.value()[0].modifiedUtc == dummy && quiet.value()[1].modifiedUtc == dummy);
    auto quietCalcs = projectdb::lineCalcs(p);
    CHECK(quietCalcs.ok() && quietCalcs.value().size() == 2 && quietCalcs.value()[0].modifiedUtc == dummy);
    checkAutoMatches(p);

    // En rad före sträckan ändras: inga ändrade stolpar, men sträckans rader flyttas om en rad
    // sätts in före. Raden flyttar sträckan ett steg, och bara firstRow och lastRow skiljer.
    freeze();
    std::vector<poletable::PoleRow> shifted = rows;
    shifted.insert(shifted.begin(), poleRow("ST0099", "", "", "", "", "", "", ""));
    CHECK(projectdb::setPoles(p, Stage::Before, shifted).ok());
    auto moved = projectdb::normalSpans(p);
    CHECK(moved.ok() && moved.value().size() == 2);
    if (moved.ok() && moved.value().size() == 2) {
        CHECK(moved.value()[0].automatic && moved.value()[0].automatic->firstRow == 3 &&
              moved.value()[0].automatic->lastRow == 8);
        CHECK(moved.value()[0].modifiedUtc == dummy);
    }
    checkAutoMatches(p);
    CHECK(projectdb::setPoles(p, Stage::Before, rows).ok());

    // En ny sträcka ger nya stolpar och en ny anteckning.
    freeze();
    projectdb::AutoSettings s = standardSettings();
    s.ranges[0] = {"ST0103", "ST0106"};
    CHECK(projectdb::setAutoSettings(p, Stage::Before, s).ok());
    auto ranged = projectdb::normalSpans(p);
    CHECK(ranged.ok() && ranged.value().size() == 2);
    if (ranged.ok() && ranged.value().size() == 2) {
        CHECK(ranged.value()[0].id == phase.id);
        CHECK(ranged.value()[0].poles.size() == 4);
        CHECK(ranged.value()[0].note == "Automatisk · Stolptabell före · faslinan ST0103–ST0106");
        CHECK(ranged.value()[0].modifiedUtc != dummy);
        CHECK(ranged.value()[1].modifiedUtc == dummy);
    }
    auto rangedCalc = projectdb::lineCalcs(p);
    CHECK(rangedCalc.ok() && rangedCalc.value().size() == 2 &&
          rangedCalc.value()[0].note == "Automatisk · Stolptabell före · faslinan ST0103–ST0106" &&
          rangedCalc.value()[0].modifiedUtc != dummy && rangedCalc.value()[1].modifiedUtc == dummy);
    checkAutoMatches(p);

    // Ett fel i sträckan ger issue, inga stolpar och felet ur calculateSpan. Rättad kommer stolparna tillbaka.
    s.ranges[0] = {"ST0102", "ST0199"};
    CHECK(projectdb::setAutoSettings(p, Stage::Before, s).ok());
    auto broken = projectdb::normalSpans(p);
    CHECK(broken.ok() && broken.value().size() == 2);
    if (broken.ok() && broken.value().size() == 2) {
        const projectdb::NormalSpan& b = broken.value()[0];
        CHECK(b.id == phase.id && b.poles.empty());
        CHECK(b.automatic && b.automatic->issue == "Stolpnr ST0199 finns inte i Stolptabell före.");
        CHECK(b.automatic && b.automatic->firstRow == -1 && b.automatic->lastRow == -1);
        const auto calc = projectdb::calculateSpan(b);
        CHECK(!calc.result && calc.issues.size() == 1 && calc.issues[0].message == b.automatic->issue);
        CHECK(calc.issues.size() == 1 && calc.issues[0].poleIndex == -1);
    }
    checkAutoMatches(p);
    s.ranges[0] = {"ST0102", "ST0107"};
    CHECK(projectdb::setAutoSettings(p, Stage::Before, s).ok());
    auto fixed = projectdb::normalSpans(p);
    CHECK(fixed.ok() && fixed.value().size() == 2 && fixed.value()[0].poles.size() == 6);
    checkAutoMatches(p);

    // En egen lina på Topplina höger ger en tredje automatisk beräkning med felet "Välj ...".
    CHECK(projectdb::setDocConductor(p, projectdb::kTopRightConductorKey, curlew()).ok());
    auto three = projectdb::normalSpans(p);
    CHECK(three.ok() && autoCount(three.value()) == 3);
    if (three.ok()) {
        const projectdb::NormalSpan* right = findAutoSpan(three.value(), Stage::Before, Wire::TopRight);
        CHECK(right != nullptr && right->poles.empty());
        if (right && right->automatic) {
            CHECK(right->automatic->issue == "Välj var topplina höger börjar och slutar.");
            CHECK(right->docKeys.size() == 1 && right->docKeys[0] == "ns_calc_top_right_before");
            CHECK(right->note == "Automatisk · Stolptabell före · topplina höger, sträckan är inte vald");
        }
        const projectdb::NormalSpan* left = findAutoSpan(three.value(), Stage::Before, Wire::TopLeft);
        CHECK(left != nullptr && left->note == "Automatisk · Stolptabell före · topplina vänster ST0101–ST0108");
    }
    CHECK(countIn(p, "SELECT count(*) FROM auto_calc") == 3);
    checkAutoMatches(p);

    // Samma tar bort den tredje, men raden i auto_calc står kvar med sina id:n.
    CHECK(projectdb::setDocValue(p, {std::string(projectdb::kTopRightConductorKey),
                                     std::string(projectdb::kSameConductor), ""})
              .ok());
    auto back = projectdb::normalSpans(p);
    CHECK(back.ok() && autoCount(back.value()) == 2);
    if (back.ok())
        CHECK(findAutoSpan(back.value(), Stage::Before, Wire::TopLeft) != nullptr &&
              findAutoSpan(back.value(), Stage::Before, Wire::TopLeft)->note ==
                  "Automatisk · Stolptabell före · topplinorna ST0101–ST0108");
    CHECK(countIn(p, "SELECT count(*) FROM auto_calc") == 3);
    checkAutoMatches(p);
    const Reserved right = reservedIds(p, Stage::Before, Wire::TopRight);
    CHECK(right.exists);

    // Läget av: raderna i normal_span och line_calc är borta, raderna i auto_calc finns kvar.
    const Reserved phaseIds = reservedIds(p, Stage::Before, Wire::Phase);
    const Reserved topIds = reservedIds(p, Stage::Before, Wire::TopLeft);
    CHECK(phaseIds.spanId == phase.id && topIds.spanId == top.id);
    s.enabled = false;
    CHECK(projectdb::setAutoSettings(p, Stage::Before, s).ok());
    auto gone = projectdb::normalSpans(p);
    CHECK(gone.ok() && gone.value().empty());
    auto goneCalcs = projectdb::lineCalcs(p);
    CHECK(goneCalcs.ok() && goneCalcs.value().empty());
    CHECK(countIn(p, "SELECT count(*) FROM normal_span") == 0);
    CHECK(countIn(p, "SELECT count(*) FROM normal_span_pole") == 0);
    CHECK(countIn(p, "SELECT count(*) FROM line_calc") == 0);
    CHECK(countIn(p, "SELECT count(*) FROM line_calc_case") == 0);
    CHECK(countIn(p, "SELECT count(*) FROM auto_calc") == 3);
    checkAutoMatches(p);

    // En ny vanlig beräkning medan läget är av får ett id över alla reserverade.
    projectdb::NormalSpan ordinary;
    ordinary.poles = exampleC();
    auto ordinarySpan = projectdb::saveNormalSpan(p, ordinary);
    CHECK(ordinarySpan.ok());
    if (ordinarySpan.ok())
        CHECK(ordinarySpan.value() > std::max({phaseIds.spanId, topIds.spanId, right.spanId}));
    auto ordinaryCalc = projectdb::saveLineCalc(p, manualCalc(t1Input(linecalc::CalcMode::New)));
    CHECK(ordinaryCalc.ok());
    if (ordinaryCalc.ok())
        CHECK(ordinaryCalc.value() > std::max({phaseIds.calcId, topIds.calcId, right.calcId}));

    // Läget på igen: samma id och värden ur tabellen som den är nu, de vanliga efter de automatiska.
    rows[6].spanM = "300";
    CHECK(projectdb::setPoles(p, Stage::Before, rows).ok());
    s.enabled = true;
    CHECK(projectdb::setAutoSettings(p, Stage::Before, s).ok());
    auto again = projectdb::normalSpans(p);
    CHECK(again.ok() && again.value().size() == 3 && autoCount(again.value()) == 2);
    if (again.ok() && again.value().size() == 3) {
        CHECK(again.value()[0].id == phase.id && again.value()[1].id == top.id);
        CHECK(!again.value()[2].automatic && ordinarySpan.ok() && again.value()[2].id == ordinarySpan.value());
        CHECK(again.value()[0].poles.size() == 6 && again.value()[0].poles[4].lengthM == 300.0);
        const auto c0 = projectdb::calculateSpan(again.value()[0]);
        CHECK(c0.result && !closeTo(c0.result->an, 316.8));
    }
    auto againCalcs = projectdb::lineCalcs(p);
    CHECK(againCalcs.ok() && againCalcs.value().size() == 3);
    if (againCalcs.ok() && againCalcs.value().size() == 3) {
        CHECK(againCalcs.value()[0].id == phaseIds.calcId && againCalcs.value()[1].id == topIds.calcId);
        CHECK(!againCalcs.value()[2].automatic);
    }
    checkAutoMatches(p);
}

// En lagrad normalspannsberäkning och en kopplad linberäkning på de tagna raderna vilar medan
// läget är på, och är tillbaka oförändrade när läget är av.
void testAutoDormantLinks()
{
    std::optional<projectdb::ProjectDb> opened;
    if (!openTestProject("Vilande kopplingar", opened))
        return;
    projectdb::ProjectDb& p = *opened;
    CHECK(projectdb::setPoles(p, Stage::Before, annexRows()).ok());
    projectdb::NormalSpan stored;
    stored.note = "Lagrad åäö";
    stored.poles = exampleC();
    auto spanId = projectdb::saveNormalSpan(p, stored, "ns_calc_phase_before");
    auto calcId = projectdb::saveLineCalc(p, linkedCalc("line_calc_phase_before"));
    CHECK(spanId.ok() && calcId.ok());
    if (!spanId.ok() || !calcId.ok())
        return;
    CHECK(projectdb::setDocValue(p, {"ns_calc_top_left_before", "330", "Manuell åäö"}).ok());
    CHECK(projectdb::setDocValue(p, {"tension_phase", "90", ""}).ok());

    // Utan läget visar raderna de lagrade beräkningarna, och aₙ är exempel C:s.
    auto span = projectdb::normalSpan(p, spanId.value());
    CHECK(span.ok() && span.value() && span.value()->docKeys == std::vector<std::string>{"ns_calc_phase_before"});
    CHECK(readCalc(p, calcId.value()).docKeys == std::vector<std::string>{"line_calc_phase_before"});
    const auto exampleCSpan = ancalc::calculateNormalSpan(exampleC(), {});
    auto plainDocs = projectdb::docLineSources(p);
    CHECK(plainDocs.ok() && plainDocs.value().autoSpanKeys.empty());
    if (plainDocs.ok() && exampleCSpan.result) {
        bool found = false;
        for (const auto& [key, an] : plainDocs.value().normalSpans)
            if (key == "ns_calc_phase_before")
                found = an && closeTo(*an, exampleCSpan.result->an);
        CHECK(found);
    }

    // Med läget på har de lagrade beräkningarna inga rader, men kopplingarna finns kvar i tabellerna.
    CHECK(projectdb::setAutoSettings(p, Stage::Before, standardSettings()).ok());
    span = projectdb::normalSpan(p, spanId.value());
    CHECK(span.ok() && span.value() && span.value()->docKeys.empty() && !span.value()->automatic);
    CHECK(readCalc(p, calcId.value()).docKeys.empty());
    CHECK(countIn(p, "SELECT count(*) FROM doc_normal_span WHERE doc_key = 'ns_calc_phase_before'") == 1);
    CHECK(countIn(p, "SELECT count(*) FROM doc_line_calc WHERE doc_key = 'line_calc_phase_before'") == 1);
    auto autoDocs = projectdb::docLineSources(p);
    CHECK(autoDocs.ok());
    if (autoDocs.ok()) {
        int count = 0;
        for (const auto& [key, an] : autoDocs.value().normalSpans)
            if (key == "ns_calc_phase_before") {
                ++count;
                CHECK(an && closeTo(*an, 315.2));
            }
        CHECK(count == 1);
        CHECK(autoDocs.value().autoSpanKeys.size() == 2);
    }

    // Tillåtet: spara den lagrade kopplade beräkningen och sätta ett värde på en tagen rad.
    projectdb::LineCalc lc = readCalc(p, calcId.value());
    lc.note = "Ändrad åäö";
    CHECK(projectdb::saveLineCalc(p, lc).ok());
    CHECK(readCalc(p, calcId.value()).note == "Ändrad åäö");
    CHECK(projectdb::setDocValue(p, {"ns_calc_top_left_before", "335", "Manuell åäö"}).ok());
    checkAutoMatches(p);

    // Med läget av är kopplingarna och värdet tillbaka, oförändrade.
    projectdb::AutoSettings off = standardSettings();
    off.enabled = false;
    CHECK(projectdb::setAutoSettings(p, Stage::Before, off).ok());
    span = projectdb::normalSpan(p, spanId.value());
    CHECK(span.ok() && span.value() && span.value()->docKeys == std::vector<std::string>{"ns_calc_phase_before"});
    CHECK(readCalc(p, calcId.value()).docKeys == std::vector<std::string>{"line_calc_phase_before"});
    auto values = projectdb::docValues(p);
    CHECK(values.ok());
    if (values.ok()) {
        bool found = false;
        for (const auto& v : values.value())
            if (v.key == "ns_calc_top_left_before")
                found = v.value == "335" && v.reference == "Manuell åäö";
        CHECK(found);
    }
    checkAutoMatches(p);
}

// Varje spärr i planen 7.5 ger Invalid med sin text, och revisionen och raderna är oförändrade.
void testAutoGuards()
{
    std::optional<projectdb::ProjectDb> opened;
    if (!openTestProject("Spärrar", opened))
        return;
    projectdb::ProjectDb& p = *opened;
    CHECK(projectdb::setPoles(p, Stage::Before, annexRows()).ok());
    CHECK(projectdb::setAutoSettings(p, Stage::Before, standardSettings()).ok());
    CHECK(projectdb::copyBeforeToAfter(p).ok());
    projectdb::NormalSpan ordinary;
    ordinary.poles = exampleC();
    auto ordinarySpan = projectdb::saveNormalSpan(p, ordinary);
    auto ordinaryCalc = projectdb::saveLineCalc(p, manualCalc(t1Input(linecalc::CalcMode::New)));
    CHECK(ordinarySpan.ok() && ordinaryCalc.ok());
    if (!ordinarySpan.ok() || !ordinaryCalc.ok())
        return;
    const Reserved phase = reservedIds(p, Stage::Before, Wire::Phase);
    const Reserved afterPhase = reservedIds(p, Stage::After, Wire::Phase);
    CHECK(phase.exists && afterPhase.exists);

    const std::string t1 = "Den automatiska beräkningen räknas ur Stolptabell före och kan inte ändras.";
    const std::string t1After = "Den automatiska beräkningen räknas ur Stolptabell efter och kan inte ändras.";
    const std::string t2 =
        "Den automatiska beräkningen kan inte tas bort. Stäng av automatiskt läge i Stolptabell före.";
    const std::string t2After =
        "Den automatiska beräkningen kan inte tas bort. Stäng av automatiskt läge i Stolptabell efter.";
    const std::string t3 = "Raden visar en automatisk beräkning ur Stolptabell före. Stäng av automatiskt "
                           "läge där för att koppla en annan beräkning.";
    const std::string t3After = "Raden visar en automatisk beräkning ur Stolptabell efter. Stäng av "
                                "automatiskt läge där för att koppla en annan beräkning.";

    const auto revision = p.revision();
    const auto spanCount = countIn(p, "SELECT count(*) FROM normal_span");
    const auto calcCount = countIn(p, "SELECT count(*) FROM line_calc");
    const auto checkInvalid = [&](const core::Result<void>& r, const std::string& text) {
        CHECK(!r.ok() && r.error().code == ErrorCode::Invalid);
        if (!r.ok())
            CHECK(r.error().message == text);
        CHECK(p.revision() == revision);
        CHECK(countIn(p, "SELECT count(*) FROM normal_span") == spanCount);
        CHECK(countIn(p, "SELECT count(*) FROM line_calc") == calcCount);
    };
    const auto checkInvalidId = [&](const core::Result<std::int64_t>& r, const std::string& text) {
        CHECK(!r.ok() && r.error().code == ErrorCode::Invalid);
        if (!r.ok())
            CHECK(r.error().message == text);
        CHECK(p.revision() == revision);
    };

    // T1: ändra en automatisk beräkning.
    projectdb::NormalSpan edit = *projectdb::normalSpan(p, phase.spanId).value();
    edit.note = "Ändrad";
    checkInvalidId(projectdb::saveNormalSpan(p, edit), t1);
    checkInvalid(projectdb::linkNormalSpan(p, phase.spanId, "ns_calc_top_right_before"), t1);
    projectdb::LineCalc editCalc = readCalc(p, phase.calcId);
    editCalc.note = "Ändrad";
    checkInvalidId(projectdb::saveLineCalc(p, editCalc), t1);
    checkInvalid(projectdb::linkLineCalc(p, phase.calcId, "line_calc_phase_before"), t1);
    checkInvalid(projectdb::detachLineCalc(p, phase.calcId), t1);
    edit.id = afterPhase.spanId;
    checkInvalidId(projectdb::saveNormalSpan(p, edit), t1After);

    // T2: ta bort en automatisk beräkning.
    checkInvalid(projectdb::removeNormalSpan(p, phase.spanId), t2);
    checkInvalid(projectdb::removeLineCalc(p, phase.calcId), t2);
    checkInvalid(projectdb::removeNormalSpan(p, afterPhase.spanId), t2After);
    checkInvalid(projectdb::removeLineCalc(p, afterPhase.calcId), t2After);

    // T3: koppla om en tagen rad.
    projectdb::NormalSpan fresh;
    fresh.poles = exampleC();
    checkInvalidId(projectdb::saveNormalSpan(p, fresh, "ns_calc_phase_before"), t3);
    checkInvalidId(projectdb::saveNormalSpan(p, fresh, "ns_calc_phase_after"), t3After);
    checkInvalid(projectdb::linkNormalSpan(p, ordinarySpan.value(), "ns_calc_phase_before"), t3);
    checkInvalid(projectdb::unlinkDocRow(p, "ns_calc_phase_before"), t3);
    checkInvalid(projectdb::unlinkDocRow(p, "line_calc_top_left_before"), t3);
    checkInvalidId(projectdb::saveLineCalc(p, linkedCalc("line_calc_phase_before")), t3);
    checkInvalidId(projectdb::saveLineCalc(p, manualCalc(t1Input(linecalc::CalcMode::New)),
                                           "line_calc_phase_before"),
                   t3);
    checkInvalid(projectdb::linkLineCalc(p, ordinaryCalc.value(), "line_calc_phase_after"), t3After);

    // Raden för topplina höger är inte tagen med Samma, och en vanlig beräkning går att koppla dit.
    CHECK(projectdb::linkNormalSpan(p, ordinarySpan.value(), "ns_calc_top_right_before").ok());
    CHECK(projectdb::unlinkDocRow(p, "ns_calc_top_right_before").ok());

    // Spärrarna gäller de reserverade id:na också när läget är av.
    projectdb::AutoSettings off = standardSettings();
    off.enabled = false;
    CHECK(projectdb::setAutoSettings(p, Stage::Before, off).ok());
    CHECK(countIn(p, "SELECT count(*) FROM normal_span WHERE id = " + std::to_string(phase.spanId)) == 0);
    const auto revisionOff = p.revision();
    auto removed = projectdb::removeNormalSpan(p, phase.spanId);
    CHECK(!removed.ok() && removed.error().code == ErrorCode::Invalid && removed.error().message == t2);
    auto removedCalc = projectdb::removeLineCalc(p, phase.calcId);
    CHECK(!removedCalc.ok() && removedCalc.error().code == ErrorCode::Invalid);
    CHECK(p.revision() == revisionOff);
    // Och raden är fri att koppla när läget är av.
    CHECK(projectdb::linkNormalSpan(p, ordinarySpan.value(), "ns_calc_phase_before").ok());
}

// En sträcka till en stolpe som saknas ger issue och inget aₙ, och upplösningen säger var det rättas.
void testAutoIssue()
{
    std::optional<projectdb::ProjectDb> opened;
    if (!openTestProject("Sträckfel", opened))
        return;
    projectdb::ProjectDb& p = *opened;
    CHECK(projectdb::setPoles(p, Stage::Before, annexRows()).ok());
    projectdb::AutoSettings s = standardSettings();
    s.ranges[0] = {"ST0102", "ST0199"};
    CHECK(projectdb::setAutoSettings(p, Stage::Before, s).ok());

    auto spans = projectdb::normalSpans(p);
    CHECK(spans.ok() && !spans.value().empty());
    if (!spans.ok() || spans.value().empty())
        return;
    const projectdb::NormalSpan& phase = spans.value()[0];
    CHECK(phase.automatic && phase.automatic->issue == "Stolpnr ST0199 finns inte i Stolptabell före.");
    CHECK(phase.poles.empty());

    auto docs = projectdb::docLineSources(p);
    CHECK(docs.ok());
    if (!docs.ok())
        return;
    bool found = false;
    for (const auto& [key, an] : docs.value().normalSpans)
        if (key == "ns_calc_phase_before") {
            found = true;
            CHECK(!an.has_value());
        }
    CHECK(found);
    auto calcs = projectdb::lineCalcs(p);
    CHECK(calcs.ok() && !calcs.value().empty());
    if (!calcs.ok() || calcs.value().empty())
        return;
    const projectdb::LineCalc& lc = calcs.value()[0];
    const projectdb::ResolvedLineInput r = projectdb::resolveLineInput(lc.input, lc.docSource, docs.value());
    const linecalc::LineIssue* issue = issueFor(r.issues, linecalc::LineField::NormalSpan);
    CHECK(issue != nullptr);
    if (issue)
        CHECK(issue->message == "Faslina normalspannsberäkning före ombyggnad har fel och ger inget aₙ. "
                                "Rätta sträckan eller stolparna i Stolptabell före.");
    CHECK(!r.input.normalSpanM.has_value());
}

// Skapar projektfilen som v0.4.0 hade den: steg 1 till 4, user_version 4 och uppgifter i alla
// tabeller. Namnet är åäö-v040. Stolptabell före har tre rader med tio decimaler.
bool writeV040Project(const std::string& path)
{
    db::OpenOptions o;
    o.mode = db::OpenMode::CreateNew;
    auto d = db::Database::open(path, o);
    CHECK(d.ok());
    if (!d.ok())
        return false;
    const std::string head =
        "PRAGMA application_id = " + std::to_string(projectdb::kApplicationId) +
        "; PRAGMA user_version = 4; "
        "CREATE TABLE project (id INTEGER PRIMARY KEY CHECK (id = 1), name TEXT NOT NULL, "
        "created_utc TEXT NOT NULL, created_with TEXT NOT NULL); ";
    bool ok = d.value().exec(head).ok();
    ok = ok && d.value().exec(std::string(projectdb::dev::kStep2)).ok();
    ok = ok && d.value().exec(std::string(projectdb::dev::kStep3)).ok();
    ok = ok && d.value().exec(std::string(projectdb::dev::kStep4)).ok();
    const std::string data = R"sql(
INSERT INTO project VALUES (1, 'åäö-v040', '2026-11-26T12:00:00Z', 'Kalkyl 0.4.0');
INSERT INTO doc_value(key, value, reference) VALUES
    ('tension_phase', '90', 'Ref åäö'),
    ('temp_max', '50', '');
INSERT INTO normal_span(id, note, limit_longest, limit_shortest, limit_adjacent, limit_local_min,
                        limit_local_max, created_utc, modified_utc)
VALUES (1, 'Exempel C åäö', 1.6, 0.4, 2.2, 0.9, 1.2, '2026-11-26T12:00:00Z', '2026-11-26T12:00:00Z');
INSERT INTO normal_span_pole(span_id, position, pole, length_m, height_m) VALUES
    (1, 0, 'P0', 250, 10), (1, 1, 'P1', 300, 20), (1, 2, 'P2', 200, 15), (1, 3, 'P3', 400, 30);
INSERT INTO doc_normal_span(doc_key, span_id) VALUES ('ns_calc_phase_before', 1);
INSERT INTO line_calc(id, note, doc_source, created_utc, modified_utc)
VALUES (1, 'Linberäkning åäö', 'line_calc_phase_after', '2026-11-26T12:00:00Z', '2026-11-26T12:00:00Z');
WITH RECURSIVE n(i) AS (SELECT 1 UNION ALL SELECT i + 1 FROM n WHERE i < 15)
INSERT INTO line_calc_case(calc_id, case_id) SELECT 1, i FROM n;
INSERT INTO doc_line_calc(doc_key, calc_id) VALUES ('line_calc_phase_after', 1);
INSERT INTO pole_before(position, pole, av, ah, span_m, coord_n, coord_e, coord_h, lkl) VALUES
    (0, 'ST0378', '363', '362', '348', '6512240.8500000000', '412181.98', '44.19', '255/262'),
    (1, 'Åkerö-ÄÖ', NULL, '-12.5', '0', NULL, NULL, NULL, ''),
    (2, 'ST0381', '44.10', NULL, NULL, '6511247.0800000000', '411804.35', '30.01', '262/262');
)sql";
    ok = ok && d.value().exec(data).ok();
    CHECK(ok);
    return ok;
}

// Kontrollerar att allt som v0.4.0-filen innehöll finns kvar. Med läget på visar de tagna raderna
// automatiska beräkningar, så den lagrade linberäkningen och normalspannet har inga rader då.
void checkV040Data(projectdb::ProjectDb& p, bool autoOn)
{
    CHECK(p.info().name == "åäö-v040");
    CHECK(p.info().createdWith == "Kalkyl 0.4.0");
    auto values = projectdb::docValues(p);
    CHECK(values.ok() && values.value().size() == 2);
    if (values.ok()) {
        for (const auto& v : values.value()) {
            if (v.key == "tension_phase")
                CHECK(v.value == "90" && v.reference == "Ref åäö");
            else if (v.key == "temp_max")
                CHECK(v.value == "50");
            else
                CHECK(false);
        }
    }
    auto span = projectdb::normalSpan(p, 1);
    CHECK(span.ok() && span.value().has_value());
    if (span.ok() && span.value()) {
        CHECK(span.value()->note == "Exempel C åäö" && span.value()->poles.size() == 4);
        CHECK(span.value()->limits.longest == 1.6 && span.value()->limits.localMax == 1.2);
        CHECK(span.value()->docKeys.size() == 1 && span.value()->docKeys[0] == "ns_calc_phase_before");
        CHECK(!span.value()->automatic);
    }
    const projectdb::LineCalc lc = readCalc(p, 1);
    CHECK(lc.note == "Linberäkning åäö" && lc.docSource == "line_calc_phase_after" && !lc.automatic);
    if (autoOn)
        CHECK(lc.docKeys.empty());
    else
        CHECK(lc.docKeys.size() == 1 && lc.docKeys[0] == "line_calc_phase_after");
    CHECK(countIn(p, "SELECT count(*) FROM line_calc_case WHERE calc_id = 1") == 15);
    CHECK(countIn(p, "SELECT count(*) FROM doc_line_calc") == 1);
    CHECK(countIn(p, "SELECT count(*) FROM doc_normal_span") == 1);
    auto before = projectdb::polesBefore(p);
    CHECK(before.ok() && before.value() == tenDecimalRows());
    if (before.ok() && before.value().size() == 3)
        CHECK(before.value()[0].north == "6512240.8500000000" && before.value()[2].av == "44.10");
}

// Ett projekt som v0.4.0 hade det migreras till steg 5: allt gammalt finns kvar, Stolptabell efter
// är tom, läget är av utan automatiska beräkningar, och med läget på finns de automatiska
// beräkningarna kvar med samma id efter publicering och ny öppning.
void testMigrateFromV040()
{
    const std::string path = join(g_share, "Från 0.4.0.kalkyl");
    if (!writeV040Project(path))
        return;
    Reserved phaseIds;
    {
        auto opened = projectdb::ProjectDb::open(path, options());
        CHECK(opened.ok());
        if (!opened.ok()) {
            std::cerr << opened.error().message << " / " << opened.error().detail << std::endl;
            return;
        }
        projectdb::ProjectDb& p = opened.value();
        CHECK(countIn(p, "PRAGMA user_version") == projectdb::kSchemaVersion);
        CHECK(countIn(p, kNineteenTablesSql) == 19);
        CHECK(countIn(p, "SELECT count(*) FROM pole_import") == 0);
        {
            auto none = projectdb::lastPoleImport(p);
            CHECK(none.ok() && !none.value().has_value());
        }
        checkV040Data(p, false);
        auto after = projectdb::poles(p, Stage::After);
        CHECK(after.ok() && after.value().empty());
        for (Stage stage : {Stage::Before, Stage::After}) {
            auto settings = projectdb::autoSettings(p, stage);
            CHECK(settings.ok() && settings.value() == projectdb::AutoSettings{});
        }
        CHECK(countIn(p, "SELECT count(*) FROM auto_calc") == 0);
        CHECK(countIn(p, "SELECT count(*) FROM auto_mode") == 0);
        auto spans = projectdb::normalSpans(p);
        CHECK(spans.ok() && spans.value().size() == 1 && autoCount(spans.value()) == 0);

        // Läget på i efter med bilaga A: de gamla raderna på efterraden vilar.
        CHECK(projectdb::setPoles(p, Stage::After, annexRows()).ok());
        CHECK(projectdb::setAutoSettings(p, Stage::After, standardSettings()).ok());
        checkV040Data(p, true);
        checkAutoMatches(p);
        phaseIds = reservedIds(p, Stage::After, Wire::Phase);
        CHECK(phaseIds.exists && phaseIds.spanId > 1 && phaseIds.calcId > 1);
        CHECK(publishNow(p));
        p.close();
    }
    CHECK(scalar(path, "PRAGMA user_version") == projectdb::kSchemaVersion);
    CHECK(scalar(path, "SELECT count(*) FROM project WHERE name = 'åäö-v040'") == 1);
    CHECK(scalar(path, "SELECT count(*) FROM pole_before") == 3);
    CHECK(scalar(path, "SELECT count(*) FROM pole_after") == 10);
    CHECK(scalar(path, "SELECT count(*) FROM auto_calc") == 2);
    CHECK(scalar(path, "SELECT count(*) FROM normal_span") == 3);
    auto reopened = projectdb::ProjectDb::open(path, options());
    CHECK(reopened.ok());
    if (!reopened.ok())
        return;
    projectdb::ProjectDb& p = reopened.value();
    checkV040Data(p, true);
    checkAutoMatches(p);
    const Reserved again = reservedIds(p, Stage::After, Wire::Phase);
    CHECK(again.exists && again.spanId == phaseIds.spanId && again.calcId == phaseIds.calcId);
    auto spans = projectdb::normalSpans(p);
    CHECK(spans.ok() && autoCount(spans.value()) == 2);
    if (spans.ok() && !spans.value().empty())
        CHECK(spans.value()[0].id == phaseIds.spanId && spans.value()[0].poles.size() == 6);

    // Läget av ger tillbaka de gamla kopplingarna, och läget på igen samma id.
    projectdb::AutoSettings off = standardSettings();
    off.enabled = false;
    CHECK(projectdb::setAutoSettings(p, Stage::After, off).ok());
    checkV040Data(p, false);
    CHECK(projectdb::setAutoSettings(p, Stage::After, standardSettings()).ok());
    const Reserved third = reservedIds(p, Stage::After, Wire::Phase);
    CHECK(third.spanId == phaseIds.spanId && third.calcId == phaseIds.calcId);
    checkAutoMatches(p);
}

// Ett projekt som skapades med steg 5 men före steg 6 (user_version 5 och bara steg 5:s tabeller,
// utan importens rådata) migreras till steg 6: allt finns kvar, också Stolptabell efter och
// inställningen för automatiskt läge, och de fyra pole_import-tabellerna finns och är tomma.
void testMigrateFromStep5()
{
    const std::string path = join(g_share, "Från steg 5.kalkyl");
    if (!writeV040Project(path))
        return;
    {
        auto d = db::Database::open(path, db::OpenOptions{});
        CHECK(d.ok());
        if (!d.ok())
            return;
        bool ok = d.value().exec(std::string(projectdb::dev::kStep5)).ok();
        ok = ok && d.value().exec(R"sql(
PRAGMA user_version = 5;
INSERT INTO pole_after(position, pole, av, ah, span_m, coord_n, coord_e, coord_h, lkl)
VALUES (0, 'ST0378', '363', NULL, '348', NULL, NULL, '44.19', '255/262');
INSERT INTO auto_mode(stage, enabled, phase_from, phase_to) VALUES ('before', 0, 'ST0378', 'ST0381');
)sql").ok();
        CHECK(ok);
        if (!ok)
            return;
    }
    CHECK(scalar(path, "PRAGMA user_version") == 5);
    CHECK(scalar(path, "SELECT count(*) FROM sqlite_master WHERE type = 'table' AND name LIKE 'pole_import%'") == 0);

    auto opened = projectdb::ProjectDb::open(path, options());
    CHECK(opened.ok());
    if (!opened.ok()) {
        std::cerr << opened.error().message << " / " << opened.error().detail << std::endl;
        return;
    }
    projectdb::ProjectDb& p = opened.value();
    CHECK(countIn(p, "PRAGMA user_version") == projectdb::kSchemaVersion);
    CHECK(countIn(p, kNineteenTablesSql) == 19);
    CHECK(countIn(p, "SELECT count(*) FROM pole_import_cell") == 0);
    auto none = projectdb::lastPoleImport(p);
    CHECK(none.ok() && !none.value().has_value());
    checkV040Data(p, false);
    auto after = projectdb::poles(p, Stage::After);
    CHECK(after.ok() && after.value().size() == 1 && after.value()[0].pole == "ST0378" &&
          after.value()[0].spanM == "348");
    auto settings = projectdb::autoSettings(p, Stage::Before);
    CHECK(settings.ok() && !settings.value().enabled && settings.value().ranges[0].fromPole == "ST0378" &&
          settings.value().ranges[0].toPole == "ST0381");
}

// Ett projekt från v0.5.0 (steg 1–6, user_version 6) migreras till steg 7: pole_after får Av Fas2
// och Fas3 med Av:s värde på varje rad (NULL förblir tomt), allt annat finns kvar, och Av per fas
// går att spara och läsa tillbaka. copyBeforeToAfter ger Av i alla tre faserna.
void testMigrateFromStep6()
{
    const std::string path = join(g_share, "Från steg 6.kalkyl");
    if (!writeV040Project(path))
        return;
    {
        auto d = db::Database::open(path, db::OpenOptions{});
        CHECK(d.ok());
        if (!d.ok())
            return;
        bool ok = d.value().exec(std::string(projectdb::dev::kStep5)).ok();
        ok = ok && d.value().exec(std::string(projectdb::dev::kStep6)).ok();
        ok = ok && d.value().exec(R"sql(
PRAGMA user_version = 6;
INSERT INTO pole_after(position, pole, av, ah, span_m, coord_n, coord_e, coord_h, lkl)
VALUES (0, 'ST0378', '363', NULL, '348', NULL, NULL, '44.19', '255/262'),
       (1, 'ST0379', NULL, '12', '320', NULL, NULL, '41.5', '');
)sql").ok();
        CHECK(ok);
        if (!ok)
            return;
    }
    CHECK(scalar(path, "PRAGMA user_version") == 6);

    auto opened = projectdb::ProjectDb::open(path, options());
    CHECK(opened.ok());
    if (!opened.ok()) {
        std::cerr << opened.error().message << " / " << opened.error().detail << std::endl;
        return;
    }
    projectdb::ProjectDb& p = opened.value();
    CHECK(countIn(p, "PRAGMA user_version") == projectdb::kSchemaVersion);
    CHECK(countIn(p, kNineteenTablesSql) == 19);
    checkV040Data(p, false);
    auto after = projectdb::poles(p, Stage::After);
    CHECK(after.ok() && after.value().size() == 2);
    if (after.ok() && after.value().size() == 2) {
        const auto& a = after.value();
        CHECK(a[0].pole == "ST0378" && a[0].av == "363" && a[0].av2 == "363" && a[0].av3 == "363");
        CHECK(a[0].spanM == "348" && a[0].lkl == "255/262");
        CHECK(a[1].pole == "ST0379" && a[1].av.empty() && a[1].av2.empty() && a[1].av3.empty() && a[1].ah == "12");
    }

    // Av per fas sparas och läses tillbaka, och före får inga faser.
    std::vector<poletable::PoleRow> rows = after.ok() ? after.value() : std::vector<poletable::PoleRow>{};
    if (!rows.empty()) {
        rows[0].av2 = "370.5";
        rows[0].av3 = "";
        CHECK(projectdb::setPoles(p, Stage::After, rows).ok());
        auto back = projectdb::poles(p, Stage::After);
        CHECK(back.ok() && back.value() == rows);
    }
    auto bad = projectdb::setPoles(p, Stage::After, {poletable::PoleRow{"X1", "1", "", "", "", "", "", "", "abc", ""}});
    CHECK(!bad.ok() && bad.error().code == ErrorCode::Invalid);

    // Kopieringen från före ger Av i alla tre faserna.
    CHECK(projectdb::setPolesBefore(p, {poletable::PoleRow{"ST0100", "300.25", "", "", "", "", "", ""}}).ok());
    CHECK(projectdb::copyBeforeToAfter(p).ok());
    auto copied = projectdb::poles(p, Stage::After);
    CHECK(copied.ok() && copied.value().size() == 1 && copied.value()[0].av == "300.25" &&
          copied.value()[0].av2 == "300.25" && copied.value()[0].av3 == "300.25");
    auto before = projectdb::polesBefore(p);
    CHECK(before.ok() && before.value().size() == 1 && before.value()[0].av2.empty());
}

// Skapar projektfilen som v0.3.2 hade den: steg 1 till 3, user_version 3 och uppgifter i alla
// tabeller. Namnet är åäö-v030. Linberäkningen är manuell med förvalen för alla fält utom
// anteckningen, och normalspannet är kopplat till en rad.
bool writeV030Project(const std::string& path)
{
    db::OpenOptions o;
    o.mode = db::OpenMode::CreateNew;
    auto d = db::Database::open(path, o);
    CHECK(d.ok());
    if (!d.ok())
        return false;
    const std::string head =
        "PRAGMA application_id = " + std::to_string(projectdb::kApplicationId) +
        "; PRAGMA user_version = 3; "
        "CREATE TABLE project (id INTEGER PRIMARY KEY CHECK (id = 1), name TEXT NOT NULL, "
        "created_utc TEXT NOT NULL, created_with TEXT NOT NULL); ";
    bool ok = d.value().exec(head).ok();
    ok = ok && d.value().exec(std::string(projectdb::dev::kStep2)).ok();
    ok = ok && d.value().exec(std::string(projectdb::dev::kStep3)).ok();
    const std::string data = R"sql(
INSERT INTO project VALUES (1, 'åäö-v030', '2026-11-26T12:00:00Z', 'Kalkyl 0.3.2');
INSERT INTO doc_value(key, value, reference) VALUES
    ('tension_phase', '90', 'Ref åäö'),
    ('temp_max', '50', '');
INSERT INTO normal_span(id, note, limit_longest, limit_shortest, limit_adjacent, limit_local_min,
                        limit_local_max, created_utc, modified_utc)
VALUES (1, 'Exempel C åäö', 1.6, 0.4, 2.2, 0.9, 1.2, '2026-11-26T12:00:00Z', '2026-11-26T12:00:00Z');
INSERT INTO normal_span_pole(span_id, position, pole, length_m, height_m) VALUES
    (1, 0, 'P0', 250, 10), (1, 1, 'P1', 300, 20), (1, 2, 'P2', 200, 15), (1, 3, 'P3', 400, 30);
INSERT INTO doc_normal_span(doc_key, span_id) VALUES ('ns_calc_phase_before', 1);
INSERT INTO line_calc(id, note, created_utc, modified_utc)
VALUES (1, 'Linberäkning åäö', '2026-11-26T12:00:00Z', '2026-11-26T12:00:00Z');
WITH RECURSIVE n(i) AS (SELECT 1 UNION ALL SELECT i + 1 FROM n WHERE i < 15)
INSERT INTO line_calc_case(calc_id, case_id) SELECT 1, i FROM n;
INSERT INTO doc_line_calc(doc_key, calc_id) VALUES ('line_calc_phase_after', 1);
)sql";
    ok = ok && d.value().exec(data).ok();
    CHECK(ok);
    return ok;
}

// Kontrollerar att allt som v0.3.2-filen innehöll finns kvar i det öppnade projektet.
void checkV030Data(projectdb::ProjectDb& p)
{
    CHECK(p.info().name == "åäö-v030");
    CHECK(p.info().createdWith == "Kalkyl 0.3.2");
    auto values = projectdb::docValues(p);
    CHECK(values.ok() && values.value().size() == 2);
    if (values.ok()) {
        for (const auto& v : values.value()) {
            if (v.key == "tension_phase")
                CHECK(v.value == "90" && v.reference == "Ref åäö");
            else if (v.key == "temp_max")
                CHECK(v.value == "50");
            else
                CHECK(false);
        }
    }
    auto spans = projectdb::normalSpans(p);
    CHECK(spans.ok() && spans.value().size() == 1);
    if (spans.ok() && spans.value().size() == 1) {
        const projectdb::NormalSpan& ns = spans.value()[0];
        CHECK(ns.note == "Exempel C åäö");
        CHECK(ns.poles.size() == 4);
        CHECK(ns.limits.longest == 1.6 && ns.limits.shortest == 0.4 && ns.limits.adjacent == 2.2);
        CHECK(ns.limits.localMin == 0.9 && ns.limits.localMax == 1.2);
        CHECK(ns.docKeys.size() == 1 && ns.docKeys[0] == "ns_calc_phase_before");
    }
    auto calcs = projectdb::lineCalcs(p);
    CHECK(calcs.ok() && calcs.value().size() == 1);
    if (calcs.ok() && calcs.value().size() == 1) {
        const projectdb::LineCalc& lc = calcs.value()[0];
        CHECK(lc.id == 1 && lc.note == "Linberäkning åäö");
        CHECK(lc.docSource.empty());
        CHECK(lc.input.mode == linecalc::CalcMode::New);
        CHECK(!lc.input.conductor.has_value());
        CHECK(lc.input.constants.pi == 3.1416);
        CHECK(lc.docKeys.size() == 1 && lc.docKeys[0] == "line_calc_phase_after");
    }
    CHECK(countIn(p, "SELECT count(*) FROM line_calc_case") == 15);
}

// Ett projekt som v0.3.2 hade det migreras till det senaste steget: allt gammalt finns kvar,
// Stolptabell före är tom och fungerar, och tabellen med tio decimaler finns kvar efter
// publicering och ny öppning.
void testMigrateFromV030()
{
    const std::string path = join(g_share, "Från 0.3.0.kalkyl");
    if (!writeV030Project(path))
        return;
    {
        auto opened = projectdb::ProjectDb::open(path, options());
        CHECK(opened.ok());
        if (!opened.ok()) {
            std::cerr << opened.error().message << " / " << opened.error().detail << std::endl;
            return;
        }
        projectdb::ProjectDb& p = opened.value();
        CHECK(countIn(p, "PRAGMA user_version") == projectdb::kSchemaVersion);
        CHECK(countIn(p, kNineteenTablesSql) == 19);
        checkV030Data(p);
        auto none = projectdb::polesBefore(p);
        CHECK(none.ok() && none.value().empty());
        CHECK(projectdb::setPolesBefore(p, tenDecimalRows()).ok());
        auto back = projectdb::polesBefore(p);
        CHECK(back.ok() && back.value() == tenDecimalRows());
        CHECK(publishNow(p));
        p.close();
    }
    CHECK(scalar(path, "PRAGMA user_version") == projectdb::kSchemaVersion);
    CHECK(scalar(path, "SELECT count(*) FROM project WHERE name = 'åäö-v030'") == 1);
    CHECK(scalar(path, "SELECT count(*) FROM pole_before") == 3);
    auto reopened = projectdb::ProjectDb::open(path, options());
    CHECK(reopened.ok());
    if (!reopened.ok())
        return;
    checkV030Data(reopened.value());
    auto again = projectdb::polesBefore(reopened.value());
    CHECK(again.ok() && again.value() == tenDecimalRows());
    if (again.ok() && again.value().size() == 3)
        CHECK(again.value()[0].north == "6512240.8500000000");
}

// De sex nycklarna för linberäkningsraderna och vägen tillbaka till lina och skede.
void testLineCalcKeys()
{
    using projectdb::Stage;
    using projectdb::Wire;
    CHECK(projectdb::lineCalcDocKey(Wire::Phase, Stage::Before) == "line_calc_phase_before");
    CHECK(projectdb::lineCalcDocKey(Wire::Phase, Stage::After) == "line_calc_phase_after");
    CHECK(projectdb::lineCalcDocKey(Wire::TopLeft, Stage::Before) == "line_calc_top_left_before");
    CHECK(projectdb::lineCalcDocKey(Wire::TopLeft, Stage::After) == "line_calc_top_left_after");
    CHECK(projectdb::lineCalcDocKey(Wire::TopRight, Stage::Before) == "line_calc_top_right_before");
    CHECK(projectdb::lineCalcDocKey(Wire::TopRight, Stage::After) == "line_calc_top_right_after");
    for (Wire wire : {Wire::Phase, Wire::TopLeft, Wire::TopRight}) {
        for (Stage stage : {Stage::Before, Stage::After}) {
            const auto row = projectdb::lineCalcRowOf(projectdb::lineCalcDocKey(wire, stage));
            CHECK(row.has_value());
            if (row) {
                CHECK(row->first == wire);
                CHECK(row->second == stage);
            }
        }
    }
    CHECK(!projectdb::lineCalcRowOf("ns_calc_phase_before").has_value());
    CHECK(!projectdb::lineCalcRowOf("temp_max").has_value());
    CHECK(!projectdb::lineCalcRowOf("").has_value());
}

// Sparar och läser tillbaka ett T1 med varje fält satt, lindata utan namn och brottlast, och
// utan lindata, och ersätter lastfallen vid en ny sparning.
void testLineCalcRoundTrip()
{
    std::optional<projectdb::ProjectDb> opened;
    if (!openTestProject("Linberäkning rundtur", opened))
        return;
    projectdb::ProjectDb& p = *opened;

    projectdb::LineCalc lc = manualCalc(variedInput());
    lc.note = "Anteckning åäö";
    auto id = projectdb::saveLineCalc(p, lc);
    CHECK(id.ok());
    if (!id.ok())
        return;
    CHECK(id.value() > 0);
    const projectdb::LineCalc back = readCalc(p, id.value());
    CHECK(back.id == id.value());
    CHECK(back.note == "Anteckning åäö");
    CHECK(back.docSource.empty());
    CHECK(back.docKeys.empty());
    CHECK(!back.createdUtc.empty());
    CHECK(!back.modifiedUtc.empty());
    checkSameInput(back.input, lc.input);
    CHECK(back.input.conductor && back.input.conductor->id == 0);
    CHECK(back.input.conductor && back.input.conductor->origin == conductor::ConductorOrigin::Own);

    // Lindata utan namn och utan brottlast, och sedan utan lindata alls.
    lc.id = id.value();
    lc.input.conductor->name.clear();
    lc.input.conductor->breakingLoadKn.reset();
    CHECK(projectdb::saveLineCalc(p, lc).ok());
    {
        const projectdb::LineCalc again = readCalc(p, id.value());
        CHECK(again.input.conductor.has_value());
        if (again.input.conductor) {
            CHECK(again.input.conductor->name.empty());
            CHECK(!again.input.conductor->breakingLoadKn.has_value());
        }
        checkSameInput(again.input, lc.input);
        CHECK(again.createdUtc == back.createdUtc);
    }
    lc.input.conductor.reset();
    lc.input.normalSpanM.reset();
    lc.input.tensionMpa.reset();
    CHECK(projectdb::saveLineCalc(p, lc).ok());
    checkSameInput(readCalc(p, id.value()).input, lc.input);

    // Nya lastfall ersätter de gamla: fortfarande 15 rader för beräkningen.
    lc.input.cases[0].name = "Ändrad";
    lc.input.cases[0].manual = linecalc::ManualLoad{1, 2};
    lc.input.cases[3].manual.reset();
    CHECK(projectdb::saveLineCalc(p, lc).ok());
    checkSameInput(readCalc(p, id.value()).input, lc.input);
    CHECK(countIn(p, "SELECT count(*) FROM line_calc_case WHERE calc_id = " + std::to_string(id.value())) == 15);
    CHECK(countIn(p, "SELECT count(*) FROM line_calc") == 1);

    auto all = projectdb::lineCalcs(p);
    CHECK(all.ok() && all.value().size() == 1);
    auto missing = projectdb::lineCalc(p, 9999);
    CHECK(missing.ok() && !missing.value().has_value());
}

// Kopplingen till raderna: ett manuellt värde töms vid koppling, en manuell beräkning kan visas på
// flera rader, en kopplad hör till en rad, och en kopplad som förlorar sin rad blir manuell.
void testLineCalcLinking()
{
    std::optional<projectdb::ProjectDb> opened;
    if (!openTestProject("Linberäkning koppling", opened))
        return;
    projectdb::ProjectDb& p = *opened;

    CHECK(projectdb::setDocValue(p, {"line_calc_phase_before", "354", "Excel"}).ok());
    auto m = projectdb::saveLineCalc(p, manualCalc(t1Input(linecalc::CalcMode::Excel)),
                                     "line_calc_phase_before");
    CHECK(m.ok());
    if (!m.ok())
        return;
    {
        auto values = projectdb::docValues(p);
        CHECK(values.ok() && values.value().size() == 1);
        if (values.ok() && values.value().size() == 1) {
            CHECK(values.value()[0].key == "line_calc_phase_before");
            CHECK(values.value()[0].value.empty());
            CHECK(values.value()[0].reference == "Excel");
        }
    }
    CHECK(projectdb::linkLineCalc(p, m.value(), "line_calc_phase_after").ok());
    CHECK(readCalc(p, m.value()).docKeys.size() == 2);

    // Dokumentationens värden som den kopplade beräkningen K hämtar.
    CHECK(projectdb::setDocConductor(p, projectdb::kTopLeftConductorKey, dotterel()).ok());
    CHECK(projectdb::setDocValue(p, {"tension_top_left", "90", ""}).ok());
    CHECK(projectdb::setDocValue(p, {"normal_span_top_left", "314.6", ""}).ok());
    CHECK(projectdb::setDocValue(p, {"temp_min", "-40", ""}).ok());
    CHECK(projectdb::setDocValue(p, {"temp_max", "60", ""}).ok());

    auto k = projectdb::saveLineCalc(p, linkedCalc("line_calc_top_left_before"));
    CHECK(k.ok());
    if (!k.ok())
        return;
    {
        const projectdb::LineCalc kb = readCalc(p, k.value());
        CHECK(kb.docSource == "line_calc_top_left_before");
        CHECK(kb.docKeys.size() == 1 && kb.docKeys[0] == "line_calc_top_left_before");
    }
    auto bad = projectdb::linkLineCalc(p, k.value(), "line_calc_phase_before");
    CHECK(!bad.ok() && bad.error().code == ErrorCode::Invalid);
    if (!bad.ok())
        CHECK(bad.error().message == "En linberäkning med indata ur Dokumentation hör till en rad.");

    // M på K:s rad gör K manuell, med de värden den hade.
    CHECK(projectdb::linkLineCalc(p, m.value(), "line_calc_top_left_before").ok());
    {
        const projectdb::LineCalc kb = readCalc(p, k.value());
        CHECK(kb.docSource.empty());
        CHECK(kb.docKeys.empty());
        CHECK(kb.input.conductor && kb.input.conductor->name == dotterel().name);
        CHECK(kb.input.normalSpanM == std::optional<double>(314.6));
        CHECK(kb.input.tensionMpa == std::optional<double>(90));
        CHECK(kb.input.minTempC == -40);
        CHECK(kb.input.maxTempC == 60);
        const projectdb::LineCalc mb = readCalc(p, m.value());
        CHECK(mb.docKeys.size() == 3);
    }

    // unlinkDocRow på en rad med en kopplad beräkning gör den manuell.
    auto k2 = projectdb::saveLineCalc(p, linkedCalc("line_calc_top_left_after"));
    CHECK(k2.ok());
    if (k2.ok()) {
        CHECK(projectdb::unlinkDocRow(p, "line_calc_top_left_after").ok());
        const projectdb::LineCalc kb = readCalc(p, k2.value());
        CHECK(kb.docSource.empty());
        CHECK(kb.docKeys.empty());
        CHECK(kb.input.conductor && kb.input.conductor->name == dotterel().name);
        CHECK(kb.input.tensionMpa == std::optional<double>(90));
    }
    // På en rad med en manuell beräkning lämnas den orörd, och raden visar den inte längre.
    const projectdb::LineCalc before = readCalc(p, m.value());
    CHECK(projectdb::unlinkDocRow(p, "line_calc_phase_after").ok());
    const projectdb::LineCalc after = readCalc(p, m.value());
    checkSameInput(after.input, before.input);
    CHECK(after.docSource.empty());
    CHECK(after.docKeys.size() == 2);

    // Okända nycklar och primärnyckeln.
    for (const char* key : {"temp_max", "ns_calc_phase_before"}) {
        auto link = projectdb::linkLineCalc(p, m.value(), key);
        CHECK(!link.ok() && link.error().code == ErrorCode::Invalid);
        auto save = projectdb::saveLineCalc(p, manualCalc(t1Input(linecalc::CalcMode::New)), key);
        CHECK(!save.ok() && save.error().code == ErrorCode::Invalid);
        if (!save.ok())
            CHECK(save.error().message == std::string("Raden ") + key + " kan inte visa en linberäkning.");
    }
    CHECK(countIn(p, "SELECT count(*) FROM line_calc") == 3);
    auto direct = p.local().exec("INSERT INTO doc_line_calc(doc_key, calc_id) VALUES('line_calc_phase_before', " +
                                 std::to_string(k.value()) + ")");
    CHECK(!direct.ok());
}

// Upplösningen av Dokumentationens värden (ren funktion, källor byggda för hand) och
// calculateResolved.
void testLineCalcResolve()
{
    using linecalc::CalcMode;
    using linecalc::LineField;
    using projectdb::LineSourceField;
    const std::string before = "line_calc_phase_before";
    const projectdb::DocLineSources full =
        makeDocs({{"phase_conductor", dotterel().name, ""},
                  {"tension_phase", "90", ""},
                  {"normal_span_phase", "314.6", ""},
                  {"temp_min", "-40", ""},
                  {"temp_max", "60", ""}});

    // Alla fem fälten ur Dokumentation, i läget Excel och i läget Ny.
    for (CalcMode mode : {CalcMode::Excel, CalcMode::New}) {
        linecalc::LineInput base = linecalc::newLineInput(mode);
        base.cases[0].tempSource = linecalc::TempSource::Fixed;
        base.cases[0].tempC = 7;
        const auto r = projectdb::resolveLineInput(base, before, full);
        CHECK(r.issues.empty());
        CHECK(r.input.conductor && r.input.conductor->name == dotterel().name);
        CHECK(r.input.normalSpanM == std::optional<double>(314.6));
        CHECK(r.input.tensionMpa == std::optional<double>(90));
        CHECK(r.input.minTempC == -40);
        CHECK(r.input.maxTempC == 60);
        CHECK(r.sources.size() == 5);
        CHECK(hasSource(r, LineSourceField::Conductor, "phase_conductor"));
        CHECK(hasSource(r, LineSourceField::NormalSpan, "normal_span_phase"));
        CHECK(hasSource(r, LineSourceField::Tension, "tension_phase"));
        CHECK(hasSource(r, LineSourceField::MinTemp, "temp_min"));
        CHECK(hasSource(r, LineSourceField::MaxTemp, "temp_max"));
        // Excel skriver över lastfall 1, 2 och 15 som arkets händelser, Ny följer källorna.
        CHECK(r.input.cases[0].tempC == (mode == CalcMode::Excel ? -40 : 7));
        CHECK(r.input.cases[1].tempC == -20);
        CHECK(r.input.cases[14].tempC == 60);
    }

    // En kopplad normalspannsberäkning går före den gröna raden, och med fel ger den Missing.
    {
        projectdb::DocLineSources docs = full;
        docs.normalSpans = {{"ns_calc_phase_before", 331.7}};
        const auto r = projectdb::resolveLineInput(linecalc::newLineInput(), before, docs);
        CHECK(r.input.normalSpanM == std::optional<double>(331.7));
        CHECK(hasSource(r, LineSourceField::NormalSpan, "ns_calc_phase_before"));
        docs.normalSpans = {{"ns_calc_phase_before", std::nullopt}};
        const auto bad = projectdb::resolveLineInput(linecalc::newLineInput(), before, docs);
        CHECK(!bad.input.normalSpanM.has_value());
        CHECK(bad.issues.size() == 1);
        const auto* issue = issueFor(bad.issues, LineField::NormalSpan);
        CHECK(issue != nullptr);
        if (issue) {
            CHECK(issue->kind == linecalc::LineIssueKind::Missing);
            CHECK(issue->caseId == 0);
            CHECK(issue->message == "Faslina normalspannsberäkning före ombyggnad har fel och ger "
                                    "inget aₙ. Rätta den i fliken Normalspann.");
        }
    }
    // Ett manuellt värde på normalspannsraden går före den gröna raden.
    {
        projectdb::DocLineSources docs = full;
        docs.values.push_back({"ns_calc_phase_before", "300.5", ""});
        const auto r = projectdb::resolveLineInput(linecalc::newLineInput(), before, docs);
        CHECK(r.input.normalSpanM == std::optional<double>(300.5));
        CHECK(hasSource(r, LineSourceField::NormalSpan, "ns_calc_phase_before"));
    }
    // Tom linrad, saknad uppspänning och inget aₙ: Missing med texterna i planen.
    {
        const auto r = projectdb::resolveLineInput(linecalc::newLineInput(), before, makeDocs({}));
        CHECK(!r.input.conductor.has_value());
        CHECK(r.sources.empty());
        const auto* c = issueFor(r.issues, LineField::Conductor);
        const auto* n = issueFor(r.issues, LineField::NormalSpan);
        const auto* t = issueFor(r.issues, LineField::Tension);
        const auto* lo = issueFor(r.issues, LineField::MinTemp);
        const auto* hi = issueFor(r.issues, LineField::MaxTemp);
        CHECK(c && c->message == "Fyll i Faslina i fliken Dokumentation.");
        CHECK(n && n->message == "Fyll i Normalspann faslina eller Faslina normalspannsberäkning före "
                                 "ombyggnad i fliken Dokumentation.");
        CHECK(t && t->message == "Fyll i Uppspänning fas i fliken Dokumentation.");
        CHECK(lo && lo->message == "Fyll i Min temp i fliken Dokumentation.");
        CHECK(hi && hi->message == "Fyll i Max temp i fliken Dokumentation.");
        CHECK(r.issues.size() == 5);
    }
    // En lina som inte finns i projektet.
    {
        const auto r = projectdb::resolveLineInput(linecalc::newLineInput(), "line_calc_phase_after",
                                                   makeDocs({{"phase_conductor", "Okänd lina", ""}}));
        const auto* c = issueFor(r.issues, LineField::Conductor);
        CHECK(c && c->message == "Linan Okänd lina finns inte i projektet. Välj linan igen i fliken "
                                 "Dokumentation.");
    }
    // "Samma" på Topplina höger: höger-raden löses upp med vänster-värdena.
    {
        const projectdb::DocLineSources docs =
            makeDocs({{"top_conductor_right", "Samma", ""},
                      {"top_conductor_left", dotterel().name, ""},
                      {"tension_top_left", "88", ""},
                      {"normal_span_top_left", "200", ""},
                      {"temp_min", "-40", ""},
                      {"temp_max", "60", ""}});
        const auto r = projectdb::resolveLineInput(linecalc::newLineInput(), "line_calc_top_right_after", docs);
        CHECK(r.issues.empty());
        CHECK(r.input.conductor && r.input.conductor->name == dotterel().name);
        CHECK(r.input.tensionMpa == std::optional<double>(88));
        CHECK(r.input.normalSpanM == std::optional<double>(200));
        CHECK(hasSource(r, LineSourceField::Tension, "tension_top_left"));
        CHECK(hasSource(r, LineSourceField::NormalSpan, "normal_span_top_left"));
        // Texterna för topplinan med "Samma".
        const auto none = projectdb::resolveLineInput(linecalc::newLineInput(), "line_calc_top_right_after",
                                                      makeDocs({{"top_conductor_right", "Samma", ""}}));
        const auto* c = issueFor(none.issues, LineField::Conductor);
        const auto* n = issueFor(none.issues, LineField::NormalSpan);
        const auto* t = issueFor(none.issues, LineField::Tension);
        CHECK(c && c->message == "Fyll i Topplina vänster i fliken Dokumentation.");
        CHECK(n && n->message == "Fyll i Normalspann topplina eller Topplinor normalspannsberäkning efter "
                                 "ombyggnad i fliken Dokumentation.");
        CHECK(t && t->message == "Fyll i Uppspänning topp i fliken Dokumentation.");
    }
    // Egen topplina höger: texterna för höger.
    {
        const auto r = projectdb::resolveLineInput(
            linecalc::newLineInput(), "line_calc_top_right_before",
            makeDocs({{"top_conductor_right", dotterel().name, ""}}));
        const auto* n = issueFor(r.issues, LineField::NormalSpan);
        const auto* t = issueFor(r.issues, LineField::Tension);
        CHECK(r.input.conductor && r.input.conductor->name == dotterel().name);
        CHECK(n && n->message == "Fyll i Normalspann topplina höger eller Topplina höger normalspannsberäkning "
                                 "före ombyggnad i fliken Dokumentation.");
        CHECK(t && t->message == "Fyll i Uppspänning topp höger i fliken Dokumentation.");
    }
    // Tom docSource ger indata orörd, och en okänd nyckel ger en upplysning.
    {
        const linecalc::LineInput input = variedInput();
        const auto r = projectdb::resolveLineInput(input, "", full);
        CHECK(r.issues.empty());
        CHECK(r.sources.empty());
        checkSameInput(r.input, input);
        const auto unknown = projectdb::resolveLineInput(input, "temp_max", full);
        CHECK(unknown.issues.size() == 1);
        if (unknown.issues.size() == 1) {
            CHECK(unknown.issues[0].field == LineField::None);
            CHECK(unknown.issues[0].message == "Raden temp_max finns inte i Dokumentation.");
        }
        checkSameInput(unknown.input, input);
    }
    // Utan min respektive max: Missing, och inget resultat fast calculateLine kunde räkna.
    {
        projectdb::DocLineSources noMin = full;
        noMin.values.erase(std::remove_if(noMin.values.begin(), noMin.values.end(),
                                          [](const projectdb::DocValue& v) { return v.key == "temp_min"; }),
                           noMin.values.end());
        const auto r = projectdb::resolveLineInput(linecalc::newLineInput(), before, noMin);
        CHECK(r.issues.size() == 1);
        const auto* issue = issueFor(r.issues, LineField::MinTemp);
        CHECK(issue && issue->message == "Fyll i Min temp i fliken Dokumentation.");
        CHECK(r.input.minTempC == -50);
        CHECK(linecalc::calculateLine(r.input).result.has_value());
        CHECK(!projectdb::calculateResolved(r).result.has_value());

        projectdb::DocLineSources noMax = full;
        noMax.values.erase(std::remove_if(noMax.values.begin(), noMax.values.end(),
                                          [](const projectdb::DocValue& v) { return v.key == "temp_max"; }),
                           noMax.values.end());
        const auto r2 = projectdb::resolveLineInput(linecalc::newLineInput(), before, noMax);
        const auto* issue2 = issueFor(r2.issues, LineField::MaxTemp);
        CHECK(issue2 && issue2->message == "Fyll i Max temp i fliken Dokumentation.");
        CHECK(linecalc::calculateLine(r2.input).result.has_value());
        CHECK(!projectdb::calculateResolved(r2).result.has_value());
    }
    // calculateResolved: upplösningens upplysning står först och beräkningens Missing för
    // samma fält tas bort. Utan upplysningar blir det samma resultat som calculateLine.
    {
        projectdb::DocLineSources noTension = full;
        noTension.values.erase(
            std::remove_if(noTension.values.begin(), noTension.values.end(),
                           [](const projectdb::DocValue& v) { return v.key == "tension_phase"; }),
            noTension.values.end());
        const auto r = projectdb::resolveLineInput(linecalc::newLineInput(), before, noTension);
        const auto own = linecalc::calculateLine(r.input);
        CHECK(issueFor(own.issues, LineField::Tension) != nullptr);
        const auto calc = projectdb::calculateResolved(r);
        CHECK(!calc.result.has_value());
        CHECK(!calc.issues.empty());
        if (!calc.issues.empty())
            CHECK(calc.issues[0].message == "Fyll i Uppspänning fas i fliken Dokumentation.");
        CHECK(issueFor(calc.issues, LineField::Tension) != nullptr);
        std::size_t tensionIssues = 0;
        for (const auto& issue : calc.issues)
            if (issue.field == LineField::Tension)
                ++tensionIssues;
        CHECK(tensionIssues == 1);

        const projectdb::DocLineSources t1Docs =
            makeDocs({{"phase_conductor", dotterel().name, ""},
                      {"tension_phase", "90", ""},
                      {"normal_span_phase", "314.6", ""},
                      {"temp_min", "-50", ""},
                      {"temp_max", "50", ""}});
        const auto ok = projectdb::resolveLineInput(linecalc::newLineInput(), before, t1Docs);
        CHECK(ok.issues.empty());
        const auto good = projectdb::calculateResolved(ok);
        const auto direct = linecalc::calculateLine(t1Input(CalcMode::New));
        CHECK(good.result.has_value() && direct.result.has_value());
        if (good.result && direct.result) {
            CHECK(relClose(good.result->stakingMall, direct.result->stakingMall, 1e-12));
            CHECK(relClose(good.result->stakingMall, 353.71610854541211, 1e-5));   // läget New, exakt pi
        }
        CHECK(good.issues.size() == direct.issues.size());
    }
}

// detachLineCalc skriver Dokumentationens värden som beräkningens egna, och en kopplad beräkning
// lagras utan lina, aₙ och σ₀.
void testLineCalcDetach()
{
    std::optional<projectdb::ProjectDb> opened;
    if (!openTestProject("Linberäkning detach", opened))
        return;
    projectdb::ProjectDb& p = *opened;
    CHECK(projectdb::setDocConductor(p, projectdb::kPhaseConductorKey, dotterel()).ok());
    CHECK(projectdb::setDocValue(p, {"tension_phase", "90", ""}).ok());
    CHECK(projectdb::setDocValue(p, {"normal_span_phase", "314.6", ""}).ok());
    CHECK(projectdb::setDocValue(p, {"temp_min", "-40", ""}).ok());
    CHECK(projectdb::setDocValue(p, {"temp_max", "60", ""}).ok());

    // Sparas med lindata, aₙ och σ₀ i indata, men lagras utan dem.
    projectdb::LineCalc lc = linkedCalc("line_calc_phase_before");
    lc.input = t1Input(linecalc::CalcMode::New);
    lc.input.normalSpanM = 100;
    lc.input.tensionMpa = 50;
    auto id = projectdb::saveLineCalc(p, lc);
    CHECK(id.ok());
    if (!id.ok())
        return;
    const std::string sel = "SELECT count(*) FROM line_calc WHERE id = " + std::to_string(id.value());
    CHECK(countIn(p, sel + " AND cond_name IS NULL AND normal_span_m IS NULL AND tension_mpa IS NULL") == 1);
    CHECK(countIn(p, sel + " AND cond_type = '' AND cond_area_mm2 IS NULL AND doc_source IS NOT NULL") == 1);
    {
        const projectdb::LineCalc back = readCalc(p, id.value());
        CHECK(back.docSource == "line_calc_phase_before");
        CHECK(!back.input.conductor.has_value());
        CHECK(!back.input.normalSpanM.has_value());
        CHECK(!back.input.tensionMpa.has_value());
    }

    // En ändring i Dokumentation syns i nästa upplösning.
    auto sources = projectdb::docLineSources(p);
    CHECK(sources.ok());
    if (!sources.ok())
        return;
    CHECK(projectdb::resolveLineInput(readCalc(p, id.value()).input, "line_calc_phase_before",
                                      sources.value()).input.tensionMpa == std::optional<double>(90));
    CHECK(projectdb::setDocValue(p, {"tension_phase", "95", ""}).ok());
    sources = projectdb::docLineSources(p);
    CHECK(sources.ok());
    if (!sources.ok())
        return;
    CHECK(projectdb::resolveLineInput(readCalc(p, id.value()).input, "line_calc_phase_before",
                                      sources.value()).input.tensionMpa == std::optional<double>(95));

    // Detach skriver värdena som egna, och kopplingen till raden står kvar.
    CHECK(projectdb::detachLineCalc(p, id.value()).ok());
    {
        const projectdb::LineCalc back = readCalc(p, id.value());
        CHECK(back.docSource.empty());
        CHECK(back.docKeys.size() == 1 && back.docKeys[0] == "line_calc_phase_before");
        CHECK(back.input.conductor && back.input.conductor->name == dotterel().name);
        CHECK(back.input.normalSpanM == std::optional<double>(314.6));
        CHECK(back.input.tensionMpa == std::optional<double>(95));
        CHECK(back.input.minTempC == -40);
        CHECK(back.input.maxTempC == 60);
    }
    // Ändras Dokumentation efteråt ändras inte beräkningen. En manuell lämnas orörd.
    CHECK(projectdb::setDocValue(p, {"tension_phase", "100", ""}).ok());
    CHECK(readCalc(p, id.value()).input.tensionMpa == std::optional<double>(95));
    CHECK(projectdb::detachLineCalc(p, id.value()).ok());
    CHECK(readCalc(p, id.value()).input.tensionMpa == std::optional<double>(95));
    auto missing = projectdb::detachLineCalc(p, 9999);
    CHECK(!missing.ok() && missing.error().code == ErrorCode::NotFound);
}

// Ta bort en linberäkning med två rader: lastfall och kopplingar följer med, och ett id som
// saknas är inget fel.
void testLineCalcRemove()
{
    std::optional<projectdb::ProjectDb> opened;
    if (!openTestProject("Linberäkning ta bort", opened))
        return;
    projectdb::ProjectDb& p = *opened;
    auto id = projectdb::saveLineCalc(p, manualCalc(t1Input(linecalc::CalcMode::Excel)),
                                      "line_calc_phase_before");
    CHECK(id.ok());
    if (!id.ok())
        return;
    CHECK(projectdb::linkLineCalc(p, id.value(), "line_calc_phase_after").ok());
    CHECK(projectdb::setDocValue(p, {"temp_max", "50", ""}).ok());
    CHECK(countIn(p, "SELECT count(*) FROM doc_line_calc") == 2);
    CHECK(countIn(p, "SELECT count(*) FROM line_calc_case") == 15);

    CHECK(projectdb::removeLineCalc(p, id.value()).ok());
    CHECK(countIn(p, "SELECT count(*) FROM line_calc") == 0);
    CHECK(countIn(p, "SELECT count(*) FROM line_calc_case") == 0);
    CHECK(countIn(p, "SELECT count(*) FROM doc_line_calc") == 0);
    CHECK(countIn(p, "SELECT count(*) FROM doc_value") == 1);
    CHECK(projectdb::removeLineCalc(p, 9999).ok());
}

// ---- Rådata från importen av en stolptabell ----

using poletable::ImportCell;
using poletable::ImportCellKind;
using poletable::ImportColumn;
using poletable::ImportRecord;
using poletable::ImportRow;

// Texten i första kolumnen på första raden i en SQL-fråga mot arbetskopian, "" utan rad.
std::string textIn(projectdb::ProjectDb& p, const std::string& sql)
{
    auto stmt = p.local().prepare(sql);
    CHECK(stmt.ok());
    if (!stmt.ok())
        return {};
    auto row = stmt.value().step();
    CHECK(row.ok());
    return row.ok() && row.value() ? stmt.value().text(0) : std::string{};
}

// En importpost till tenDecimalRows(): tre rader (Excelrad 19, 20 och 22), alla fyra cellslag, en
// text med radbrytning, ett tal med 17 siffror och kolumner med och utan fält.
ImportRecord sampleRecord(const std::vector<poletable::PoleRow>& rows)
{
    const std::string nl(1, char{10});
    ImportRecord rec;
    rec.fileName = "Stolptabell åäö.xlsm";
    rec.sheetName = "Stolptabell";
    rec.title = "Stolptabell i Banken Ver. 6 GRUNDLÄGGNING & JORDNING";
    rec.version = "Ver. 6";
    rec.headerFirstRow = 11;
    rec.headerLastRow = 15;
    rec.firstDataRow = 19;
    rec.fromPole = rows.front().pole;
    rec.toPole = rows.back().pole;
    rec.columns = {{1, "A", "Internt Id", -1},
                   {2, "B", "Objekt ID", -1},
                   {9, "I", "Koordinater › N › SWEREF 99 TM", poletable::kNorth},
                   {18, "R", "Profil › Stpl nr › Slutgiltigt", poletable::kPole},
                   {23, "W", "Profil › Av › Max temp › m", poletable::kAv},
                   {25, "Y", "", -1},
                   {60, "BH", "Stag › E › cm", -1}};
    rec.rows.resize(3);
    rec.rows[0] = {19, rows[0].pole,
                   {{1, ImportCellKind::Number, "214021"},
                    {2, ImportCellKind::Text, "L1:0001"},
                    {9, ImportCellKind::Number, "6512240.85"},
                    {18, ImportCellKind::Text, rows[0].pole},
                    {23, ImportCellKind::Number, "422.10000000000002"}}};
    rec.rows[1] = {20, rows[1].pole,
                   {{2, ImportCellKind::Text, "L1:0002"},
                    {18, ImportCellKind::Text, rows[1].pole},
                    {60, ImportCellKind::Text, "256" + nl + "256"}}};
    rec.rows[2] = {22, rows[2].pole,
                   {{18, ImportCellKind::Text, rows[2].pole},
                    {23, ImportCellKind::Bool, "1"},
                    {25, ImportCellKind::Error, "#N/A"}}};
    return rec;
}

// Posten som lastPoleImport ska ge: samma post med tidpunkten som projektet satte.
ImportRecord withStamp(ImportRecord rec, const std::string& stamp)
{
    rec.importedUtc = stamp;
    return rec;
}

// Importen skriver raderna och rådata, och lastPoleImport ger samma post tillbaka (också efter
// publicering och ny öppning). SQL-frågan i planen ger rätt celler per stolpe.
void testPoleImportRoundTrip()
{
    const std::string path = join(g_share, "Import rådata.kalkyl");
    const std::vector<poletable::PoleRow> rows = tenDecimalRows();
    const ImportRecord rec = sampleRecord(rows);
    std::string stamp;
    {
        auto created = projectdb::ProjectDb::create(path, "Import rådata", options());
        CHECK(created.ok());
        if (!created.ok())
            return;
        projectdb::ProjectDb& p = created.value();
        auto none = projectdb::lastPoleImport(p);
        CHECK(none.ok() && !none.value().has_value());

        CHECK(projectdb::importPolesBefore(p, rows, rec).ok());
        auto got = projectdb::lastPoleImport(p);
        CHECK(got.ok() && got.value().has_value());
        if (!got.ok() || !got.value())
            return;
        stamp = got.value()->importedUtc;
        CHECK(!stamp.empty());
        CHECK(*got.value() == withStamp(rec, stamp));
        auto before = projectdb::polesBefore(p);
        CHECK(before.ok() && before.value() == rows);

        // Slagen och fältnamnen som de lagras.
        CHECK(countIn(p, "SELECT count(*) FROM pole_import") == 1);
        CHECK(countIn(p, "SELECT count(*) FROM pole_import_column") == 7);
        CHECK(countIn(p, "SELECT count(*) FROM pole_import_row") == 3);
        CHECK(countIn(p, "SELECT count(*) FROM pole_import_cell") == 11);
        CHECK(countIn(p, "SELECT count(*) FROM pole_import_cell WHERE kind = 'number'") == 3);
        CHECK(countIn(p, "SELECT count(*) FROM pole_import_cell WHERE kind = 'text'") == 6);
        CHECK(countIn(p, "SELECT count(*) FROM pole_import_cell WHERE kind = 'bool' AND text = '1'") == 1);
        CHECK(countIn(p, "SELECT count(*) FROM pole_import_cell WHERE kind = 'error' AND text = '#N/A'") == 1);
        CHECK(textIn(p, "SELECT field FROM pole_import_column WHERE col = 9") == "coord_n");
        CHECK(textIn(p, "SELECT field FROM pole_import_column WHERE col = 18") == "pole");
        CHECK(countIn(p, "SELECT count(*) FROM pole_import_column WHERE field IS NULL") == 4);
        CHECK(textIn(p, "SELECT text FROM pole_import_cell WHERE excel_row = 19 AND col = 23") ==
              "422.10000000000002");
        CHECK(textIn(p, "SELECT file_name FROM pole_import") == "Stolptabell åäö.xlsm");
        CHECK(textIn(p, "SELECT position FROM pole_import_row WHERE excel_row = 22") == "2");

        // Exemplet i planen 6.3: alla celler på en stolpe, i kolumnordning.
        auto stmt = p.local().prepare(R"sql(
SELECT c.letter, c.header, x.text
FROM pole_import_row r
JOIN pole_import_cell x ON x.excel_row = r.excel_row
JOIN pole_import_column c ON c.col = x.col
WHERE r.pole = 'Åkerö-ÄÖ'
ORDER BY x.col
)sql");
        CHECK(stmt.ok());
        if (stmt.ok()) {
            std::vector<std::string> letters;
            std::vector<std::string> texts;
            while (true) {
                auto step = stmt.value().step();
                CHECK(step.ok());
                if (!step.ok() || !step.value())
                    break;
                letters.push_back(stmt.value().text(0));
                texts.push_back(stmt.value().text(2));
            }
            CHECK((letters == std::vector<std::string>{"B", "R", "BH"}));
            CHECK(texts.size() == 3 && texts[0] == "L1:0002" && texts[1] == "Åkerö-ÄÖ" &&
                  texts[2] == "256" + std::string(1, char{10}) + "256");
        }
        CHECK(publishNow(p));
        p.close();
    }
    auto reopened = projectdb::ProjectDb::open(path, options());
    CHECK(reopened.ok());
    if (!reopened.ok())
        return;
    auto got = projectdb::lastPoleImport(reopened.value());
    CHECK(got.ok() && got.value().has_value() && *got.value() == withStamp(rec, stamp));
}

// En andra import ersätter allt i de fyra tabellerna, och inga celler från den första finns kvar.
void testPoleImportReplaced()
{
    auto created = projectdb::ProjectDb::create(join(g_share, "Import ersatt.kalkyl"), "Import ersatt", options());
    CHECK(created.ok());
    if (!created.ok())
        return;
    projectdb::ProjectDb& p = created.value();
    const std::vector<poletable::PoleRow> first = tenDecimalRows();
    CHECK(projectdb::importPolesBefore(p, first, sampleRecord(first)).ok());

    const std::vector<poletable::PoleRow> second = {annexRows()[2], annexRows()[3]};
    ImportRecord rec;
    rec.fileName = "Andra.xlsx";
    rec.sheetName = "Blad";
    rec.headerFirstRow = 1;
    rec.headerLastRow = 1;
    rec.firstDataRow = 5;
    rec.fromPole = "ST0102";
    rec.toPole = "ST0103";
    rec.columns = {{2, "B", "Stolpnr", poletable::kPole}, {3, "C", "N", poletable::kNorth}};
    rec.rows = {{5, "ST0102", {{2, ImportCellKind::Text, "ST0102"}, {3, ImportCellKind::Number, "6511380"}}},
                {6, "ST0103", {{2, ImportCellKind::Text, "ST0103"}}}};
    CHECK(projectdb::importPolesBefore(p, second, rec).ok());

    CHECK(countIn(p, "SELECT count(*) FROM pole_import") == 1);
    CHECK(countIn(p, "SELECT count(*) FROM pole_import_column") == 2);
    CHECK(countIn(p, "SELECT count(*) FROM pole_import_row") == 2);
    CHECK(countIn(p, "SELECT count(*) FROM pole_import_cell") == 3);
    CHECK(countIn(p, "SELECT count(*) FROM pole_import_row WHERE excel_row IN (19, 20, 22)") == 0);
    CHECK(countIn(p, "SELECT count(*) FROM pole_import_cell WHERE col NOT IN (2, 3)") == 0);
    CHECK(textIn(p, "SELECT file_name FROM pole_import") == "Andra.xlsx");
    auto got = projectdb::lastPoleImport(p);
    CHECK(got.ok() && got.value().has_value());
    if (got.ok() && got.value())
        CHECK(*got.value() == withStamp(rec, got.value()->importedUtc));
    auto before = projectdb::polesBefore(p);
    CHECK(before.ok() && before.value() == second);
}

// Handändringar rör inte rådata: setPoles (båda skedena), copyBeforeToAfter, setAutoSettings och
// en tömd Stolptabell före lämnar posten lika.
void testPoleImportUntouched()
{
    auto created = projectdb::ProjectDb::create(join(g_share, "Import orörd.kalkyl"), "Import orörd", options());
    CHECK(created.ok());
    if (!created.ok())
        return;
    projectdb::ProjectDb& p = created.value();
    const std::vector<poletable::PoleRow> rows = tenDecimalRows();
    CHECK(projectdb::importPolesBefore(p, rows, sampleRecord(rows)).ok());
    auto saved = projectdb::lastPoleImport(p);
    CHECK(saved.ok() && saved.value().has_value());
    if (!saved.ok() || !saved.value())
        return;
    const ImportRecord expected = *saved.value();

    const auto unchanged = [&]() {
        auto now = projectdb::lastPoleImport(p);
        CHECK(now.ok() && now.value().has_value() && *now.value() == expected);
    };
    std::vector<poletable::PoleRow> changed = annexRows();
    CHECK(projectdb::setPoles(p, Stage::Before, changed).ok());
    unchanged();
    changed[0].north = "1";
    CHECK(projectdb::setPoles(p, Stage::Before, changed).ok());
    unchanged();
    CHECK(projectdb::setPoles(p, Stage::After, annexRows()).ok());
    unchanged();
    CHECK(projectdb::setAutoSettings(p, Stage::Before, standardSettings()).ok());
    unchanged();
    CHECK(projectdb::copyBeforeToAfter(p).ok());
    unchanged();
    CHECK(projectdb::setPoles(p, Stage::Before, {}).ok());
    unchanged();
}

// Poster som inte hänger ihop ger Invalid, och inget ändras: raderna, rådata och revisionen.
void testPoleImportInvalid()
{
    auto created = projectdb::ProjectDb::create(join(g_share, "Import ogiltig.kalkyl"), "Import ogiltig", options());
    CHECK(created.ok());
    if (!created.ok())
        return;
    projectdb::ProjectDb& p = created.value();
    const std::vector<poletable::PoleRow> rows = tenDecimalRows();
    const ImportRecord rec = sampleRecord(rows);
    CHECK(projectdb::importPolesBefore(p, rows, rec).ok());
    auto saved = projectdb::lastPoleImport(p);
    CHECK(saved.ok() && saved.value().has_value());
    if (!saved.ok() || !saved.value())
        return;
    const ImportRecord expected = *saved.value();
    const std::uint64_t revision = p.revision();
    const std::string incomplete = "Importen av stolptabellen är ofullständig och sparades inte.";

    // Ett fel i posten: Invalid med den fasta texten och en detalj.
    const auto expectIncomplete = [&](const std::vector<poletable::PoleRow>& r, const ImportRecord& bad) {
        auto result = projectdb::importPolesBefore(p, r, bad);
        CHECK(!result.ok());
        if (result.ok())
            return;
        CHECK(result.error().code == ErrorCode::Invalid);
        CHECK(result.error().message == incomplete);
        CHECK(!result.error().detail.empty());
    };

    ImportRecord fewer = rec;
    fewer.rows.pop_back();
    expectIncomplete(rows, fewer);

    ImportRecord wrongPole = rec;
    wrongPole.rows[1].pole = "Annan";
    expectIncomplete(rows, wrongPole);

    std::vector<poletable::PoleRow> withEmpty = rows;
    withEmpty[1] = poletable::PoleRow{};
    ImportRecord emptyRec = rec;
    emptyRec.rows[1].pole = "";
    expectIncomplete(withEmpty, emptyRec);

    ImportRecord sameField = rec;
    sameField.columns[0].field = poletable::kPole;
    expectIncomplete(rows, sameField);

    ImportRecord sameColumn = rec;
    sameColumn.columns[1].column = sameColumn.columns[0].column;
    expectIncomplete(rows, sameColumn);

    ImportRecord unknownColumn = rec;
    unknownColumn.rows[0].cells.push_back({99, ImportCellKind::Text, "x"});
    expectIncomplete(rows, unknownColumn);

    ImportRecord sameRow = rec;
    sameRow.rows[1].excelRow = sameRow.rows[0].excelRow;
    expectIncomplete(rows, sameRow);

    // Ett ogiltigt tal ger texten som setPoles ger.
    std::vector<poletable::PoleRow> badNumber = rows;
    badNumber[0].north = "6512240,85";
    auto invalid = projectdb::importPolesBefore(p, badNumber, rec);
    CHECK(!invalid.ok());
    if (!invalid.ok()) {
        CHECK(invalid.error().code == ErrorCode::Invalid);
        CHECK(invalid.error().message == "Stolptabell före har ett ogiltigt tal på rad 1 i kolumnen Koordinat N.");
    }

    auto now = projectdb::lastPoleImport(p);
    CHECK(now.ok() && now.value().has_value() && *now.value() == expected);
    auto before = projectdb::polesBefore(p);
    CHECK(before.ok() && before.value() == rows);
    CHECK(p.revision() == revision);
}

// Ett skrivskyddat projekt ger AccessDenied för alla skrivningar och förblir oförändrat.
void testReadOnlyWrites()
{
    const std::string path = join(g_share, "Skrivskyddat.kalkyl");
    {
        auto created = projectdb::ProjectDb::create(path, "Skrivskyddat", options());
        CHECK(created.ok());
        if (!created.ok())
            return;
        projectdb::NormalSpan ns;
        ns.poles = exampleC();
        CHECK(projectdb::saveNormalSpan(created.value(), ns).ok());
        CHECK(projectdb::saveLineCalc(created.value(), manualCalc(t1Input(linecalc::CalcMode::New))).ok());
        CHECK(publishNow(created.value()));
    }
    auto opened = projectdb::ProjectDb::open(path, options(projectdb::Access::ReadOnly));
    CHECK(opened.ok());
    if (!opened.ok())
        return;
    projectdb::ProjectDb& p = opened.value();
    projectdb::NormalSpan ns;
    ns.poles = exampleC();
    CHECK(projectdb::setDocValue(p, {"temp_max", "1", ""}).error().code == ErrorCode::AccessDenied);
    CHECK(projectdb::setDocConductor(p, projectdb::kPhaseConductorKey, curlew()).error().code ==
          ErrorCode::AccessDenied);
    CHECK(projectdb::refreshProjectConductor(p, curlew()).error().code == ErrorCode::AccessDenied);
    CHECK(projectdb::saveNormalSpan(p, ns).error().code == ErrorCode::AccessDenied);
    CHECK(projectdb::linkNormalSpan(p, 1, "ns_calc_phase_before").error().code == ErrorCode::AccessDenied);
    CHECK(projectdb::unlinkDocRow(p, "ns_calc_phase_before").error().code == ErrorCode::AccessDenied);
    CHECK(projectdb::removeNormalSpan(p, 1).error().code == ErrorCode::AccessDenied);
    CHECK(projectdb::saveLineCalc(p, manualCalc(t1Input(linecalc::CalcMode::New))).error().code ==
          ErrorCode::AccessDenied);
    CHECK(projectdb::linkLineCalc(p, 1, "line_calc_phase_before").error().code == ErrorCode::AccessDenied);
    CHECK(projectdb::detachLineCalc(p, 1).error().code == ErrorCode::AccessDenied);
    CHECK(projectdb::removeLineCalc(p, 1).error().code == ErrorCode::AccessDenied);
    CHECK(projectdb::setPolesBefore(p, tenDecimalRows()).error().code == ErrorCode::AccessDenied);
    CHECK(projectdb::setPoles(p, projectdb::Stage::After, tenDecimalRows()).error().code ==
          ErrorCode::AccessDenied);
    CHECK(projectdb::setAutoSettings(p, projectdb::Stage::Before, {}).error().code == ErrorCode::AccessDenied);
    CHECK(projectdb::copyBeforeToAfter(p).error().code == ErrorCode::AccessDenied);
    CHECK(projectdb::importPolesBefore(p, tenDecimalRows(), sampleRecord(tenDecimalRows())).error().code ==
          ErrorCode::AccessDenied);
    CHECK(projectdb::setSpanHeight(p, lineheight::spanKey(1, 2), lineheight::Part::Outer, "12.3").error().code ==
          ErrorCode::AccessDenied);
    CHECK(projectdb::setTopAv(p, 1, "265.9").error().code == ErrorCode::AccessDenied);
    CHECK(!p.isModified());
    CHECK(p.revision() == 0);
    auto spans = projectdb::normalSpans(p);
    CHECK(spans.ok() && spans.value().size() == 1);
    auto calcs = projectdb::lineCalcs(p);
    CHECK(calcs.ok() && calcs.value().size() == 1);
    auto poles = projectdb::polesBefore(p);
    CHECK(poles.ok() && poles.value().empty());
}

// De lagrade automatiska beräkningarna går att läsa i ett skrivskyddat projekt, och de tre
// skrivningarna för automatiskt läge ger AccessDenied utan att något ändras.
void testReadOnlyAutoCalcs()
{
    const std::string path = join(g_share, "Skrivskyddat automatiskt.kalkyl");
    {
        auto created = projectdb::ProjectDb::create(path, "Skrivskyddat automatiskt", options());
        CHECK(created.ok());
        if (!created.ok())
            return;
        projectdb::ProjectDb& w = created.value();
        CHECK(projectdb::setPoles(w, projectdb::Stage::Before, annexRows()).ok());
        CHECK(projectdb::setAutoSettings(w, projectdb::Stage::Before, standardSettings()).ok());
        CHECK(publishNow(w));
    }
    auto opened = projectdb::ProjectDb::open(path, options(projectdb::Access::ReadOnly));
    CHECK(opened.ok());
    if (!opened.ok())
        return;
    projectdb::ProjectDb& p = opened.value();
    auto spans = projectdb::normalSpans(p);
    CHECK(spans.ok() && spans.value().size() == 2);
    if (spans.ok() && spans.value().size() == 2) {
        CHECK(spans.value()[0].automatic.has_value() && spans.value()[1].automatic.has_value());
        CHECK(spans.value()[0].poles.size() == 6);
    }
    auto calcs = projectdb::lineCalcs(p);
    CHECK(calcs.ok() && calcs.value().size() == 2);
    auto settings = projectdb::autoSettings(p, projectdb::Stage::Before);
    CHECK(settings.ok() && settings.value() == standardSettings());
    CHECK(projectdb::setAutoSettings(p, projectdb::Stage::Before, {}).error().code == ErrorCode::AccessDenied);
    CHECK(projectdb::setPoles(p, projectdb::Stage::After, annexRows()).error().code == ErrorCode::AccessDenied);
    CHECK(projectdb::copyBeforeToAfter(p).error().code == ErrorCode::AccessDenied);
    CHECK(p.revision() == 0);
    auto again = projectdb::normalSpans(p);
    CHECK(again.ok() && again.value().size() == 2);
}

// Ogiltiga gränser och tal ger Invalid, och ett id som saknas ger NotFound.
void testInvalid()
{
    auto created = projectdb::ProjectDb::create(join(g_share, "Ogiltigt.kalkyl"), "Ogiltigt", options());
    CHECK(created.ok());
    if (!created.ok())
        return;
    projectdb::ProjectDb& p = created.value();
    const double nan = std::numeric_limits<double>::quiet_NaN();

    projectdb::NormalSpan ns;
    ns.poles = exampleC();
    ns.limits.longest = 0;
    CHECK(projectdb::saveNormalSpan(p, ns).error().code == ErrorCode::Invalid);
    ns.limits.longest = nan;
    CHECK(projectdb::saveNormalSpan(p, ns).error().code == ErrorCode::Invalid);
    ns.limits = {};
    ns.limits.localMax = -1;
    CHECK(projectdb::saveNormalSpan(p, ns).error().code == ErrorCode::Invalid);
    ns.limits = {};
    ns.poles[1].lengthM = nan;
    CHECK(projectdb::saveNormalSpan(p, ns).error().code == ErrorCode::Invalid);
    ns.poles = exampleC();
    CHECK(countIn(p, "SELECT count(*) FROM normal_span") == 0);
    CHECK(!p.isModified());

    ns.id = 9999;
    CHECK(projectdb::saveNormalSpan(p, ns).error().code == ErrorCode::NotFound);
    CHECK(projectdb::linkNormalSpan(p, 9999, "ns_calc_phase_before").error().code == ErrorCode::NotFound);
    CHECK(!p.isModified());

    // Linberäkningar: tal som inte är ändliga, olika nycklar och id som saknas.
    const std::string invalidNumber = "Ogiltigt tal.";
    projectdb::LineCalc lc = manualCalc(t1Input(linecalc::CalcMode::New));
    lc.input.cases[4].windMagnitudePercent = nan;
    auto bad = projectdb::saveLineCalc(p, lc);
    CHECK(!bad.ok() && bad.error().code == ErrorCode::Invalid);
    if (!bad.ok())
        CHECK(bad.error().message == invalidNumber);
    lc = manualCalc(t1Input(linecalc::CalcMode::New));
    lc.input.conductor->weightNPerM = nan;
    bad = projectdb::saveLineCalc(p, lc);
    CHECK(!bad.ok() && bad.error().code == ErrorCode::Invalid);
    lc = manualCalc(t1Input(linecalc::CalcMode::New));
    lc.input.constants.gravity = nan;
    bad = projectdb::saveLineCalc(p, lc);
    CHECK(!bad.ok() && bad.error().code == ErrorCode::Invalid);
    lc = manualCalc(t1Input(linecalc::CalcMode::New));
    lc.input.cases[2].manual = linecalc::ManualLoad{nan, 1};
    bad = projectdb::saveLineCalc(p, lc);
    CHECK(!bad.ok() && bad.error().code == ErrorCode::Invalid);
    lc = manualCalc(t1Input(linecalc::CalcMode::New));
    lc.docSource = "line_calc_phase_before";
    bad = projectdb::saveLineCalc(p, lc, "line_calc_phase_after");
    CHECK(!bad.ok() && bad.error().code == ErrorCode::Invalid);
    if (!bad.ok())
        CHECK(bad.error().message == "En linberäkning med indata ur Dokumentation hör till en rad.");
    lc = manualCalc(t1Input(linecalc::CalcMode::New));
    lc.id = 9999;
    bad = projectdb::saveLineCalc(p, lc);
    CHECK(!bad.ok() && bad.error().code == ErrorCode::NotFound);
    if (!bad.ok())
        CHECK(bad.error().message == "Linberäkningen finns inte längre i projektet.");
    CHECK(projectdb::linkLineCalc(p, 9999, "line_calc_phase_before").error().code == ErrorCode::NotFound);
    CHECK(countIn(p, "SELECT count(*) FROM line_calc") == 0);
    CHECK(!p.isModified());

    // En okänd text i en lagrad kolumn ger Invalid vid läsning.
    auto id = projectdb::saveLineCalc(p, manualCalc(t1Input(linecalc::CalcMode::New)));
    CHECK(id.ok());
    if (!id.ok())
        return;
    const std::string where = " WHERE id = " + std::to_string(id.value());
    CHECK(p.local().exec("UPDATE line_calc SET mode = 'okänt'" + where).ok());
    auto read = projectdb::lineCalc(p, id.value());
    CHECK(!read.ok() && read.error().code == ErrorCode::Invalid);
    if (!read.ok())
        CHECK(read.error().message == "Linberäkningen " + std::to_string(id.value()) +
                                          " i projektfilen är skadad.");
    CHECK(p.local().exec("UPDATE line_calc SET mode = 'new'" + where).ok());
    CHECK(projectdb::lineCalc(p, id.value()).ok());
    CHECK(p.local().exec("UPDATE line_calc_case SET temp_source = 'okänt' WHERE case_id = 3").ok());
    CHECK(!projectdb::lineCalc(p, id.value()).ok());
    CHECK(p.local().exec("UPDATE line_calc_case SET temp_source = 'fixed' WHERE case_id = 3").ok());
    CHECK(p.local().exec("UPDATE line_calc_case SET pressure_source = 'okänt' WHERE case_id = 4").ok());
    CHECK(!projectdb::lineCalc(p, id.value()).ok());
    CHECK(p.local().exec("UPDATE line_calc_case SET pressure_source = 'fixed' WHERE case_id = 4").ok());
    CHECK(p.local().exec("DELETE FROM line_calc_case WHERE case_id = 15").ok());
    CHECK(!projectdb::lineCalc(p, id.value()).ok());
}

// Ett tal som inte är kanonisk decimaltext ger Invalid med rad och kolumn, och ingenting ändras.
void testPoleTableInvalid()
{
    std::optional<projectdb::ProjectDb> opened;
    if (!openTestProject("Ogiltig stolptabell", opened))
        return;
    projectdb::ProjectDb& p = *opened;
    CHECK(projectdb::setPolesBefore(p, tenDecimalRows()).ok());
    const auto revision = p.revision();

    // Decimalkomma är inte kanonisk text, och bokstäver är inget tal. Raden räknas från 1.
    std::vector<poletable::PoleRow> rows = tenDecimalRows();
    rows[1].av = "6512240,85";
    auto bad = projectdb::setPolesBefore(p, rows);
    CHECK(!bad.ok() && bad.error().code == ErrorCode::Invalid);
    if (!bad.ok())
        CHECK(bad.error().message == "Stolptabell före har ett ogiltigt tal på rad 2 i kolumnen Av.");

    rows = tenDecimalRows();
    rows[2].north = "abc";
    bad = projectdb::setPolesBefore(p, rows);
    CHECK(!bad.ok() && bad.error().code == ErrorCode::Invalid);
    if (!bad.ok())
        CHECK(bad.error().message == "Stolptabell före har ett ogiltigt tal på rad 3 i kolumnen Koordinat N.");

    // En ogiltig rad som står sist bland tomma rader avvisas också, och inget har ändrats.
    rows = tenDecimalRows();
    rows.push_back(poleRow("", "", "", "", "", "", "1e5", ""));
    bad = projectdb::setPolesBefore(p, rows);
    CHECK(!bad.ok() && bad.error().code == ErrorCode::Invalid);
    CHECK(p.revision() == revision);
    auto back = projectdb::polesBefore(p);
    CHECK(back.ok() && back.value() == tenDecimalRows());

    // Samma fel för Stolptabell efter, med den tabellens namn, och före är orört.
    rows = tenDecimalRows();
    rows[1].ah = "x";
    bad = projectdb::setPoles(p, projectdb::Stage::After, rows);
    CHECK(!bad.ok() && bad.error().code == ErrorCode::Invalid);
    if (!bad.ok())
        CHECK(bad.error().message == "Stolptabell efter har ett ogiltigt tal på rad 2 i kolumnen Ah.");
    CHECK(p.revision() == revision);
    auto after = projectdb::poles(p, projectdb::Stage::After);
    CHECK(after.ok() && after.value().empty());
}

// Revisionen räknas upp av varje skrivning, och en skrivning efter bilden lämnar projektet ändrat.
void testRevision()
{
    auto created = projectdb::ProjectDb::create(join(g_share, "Revision.kalkyl"), "Revision", options());
    CHECK(created.ok());
    if (!created.ok())
        return;
    projectdb::ProjectDb& p = created.value();
    CHECK(p.revision() == 0);
    CHECK(projectdb::setDocValue(p, {"temp_max", "40", ""}).ok());
    CHECK(p.revision() == 1);
    auto snap = p.snapshot();
    CHECK(snap.ok());
    if (!snap.ok())
        return;
    CHECK(snap.value().revision == 1);
    CHECK(projectdb::setDocValue(p, {"temp_max", "41", ""}).ok());
    CHECK(p.revision() == 2);
    auto stamp = projectdb::ProjectDb::publish(snap.value(), std::stop_token{});
    CHECK(stamp.ok());
    if (!stamp.ok())
        return;
    p.published(snap.value(), stamp.value());
    CHECK(p.isModified());
    CHECK(publishNow(p));
    CHECK(!p.isModified());
    CHECK(projectdb::setPolesBefore(p, tenDecimalRows()).ok());
    CHECK(p.revision() == 3);
    CHECK(p.isModified());

    // De tre skrivningarna för automatiskt läge räknar upp revisionen ett steg var, och synkningen
    // i dem ger ingen extra uppräkning.
    CHECK(projectdb::setAutoSettings(p, projectdb::Stage::Before, standardSettings()).ok());
    CHECK(p.revision() == 4);
    CHECK(projectdb::setPoles(p, projectdb::Stage::After, annexRows()).ok());
    CHECK(p.revision() == 5);
    CHECK(projectdb::copyBeforeToAfter(p).ok());
    CHECK(p.revision() == 6);

    // Linhöjderna är skrivningar med en uppräkning var. En vägrad skrivning räknar inte upp.
    CHECK(projectdb::setSpanHeight(p, lineheight::spanKey(1, 2), lineheight::Part::Outer, "12.3").ok());
    CHECK(p.revision() == 7);
    CHECK(projectdb::setTopAv(p, 2, "265.9").ok());
    CHECK(p.revision() == 8);
    CHECK(!projectdb::setSpanHeight(p, lineheight::spanKey(1, 3), lineheight::Part::Outer, "1").ok());
    CHECK(p.revision() == 8);

    // Importen av en stolptabell är en skrivning, med synkningen i samma transaktion.
    CHECK(projectdb::importPolesBefore(p, tenDecimalRows(), sampleRecord(tenDecimalRows())).ok());
    CHECK(p.revision() == 9);
}

// ---- Radernas id och linhöjderna ----

// Tolv stolpar ST0374–ST0385 med Av, Av Fas2, Av Fas3 och H, utan id.
std::vector<poletable::PoleRow> twelveRows()
{
    std::vector<poletable::PoleRow> rows;
    for (int i = 0; i < 12; ++i) {
        poletable::PoleRow row = poleRow(("ST0" + std::to_string(374 + i)).c_str(), "300", "", "", "", "", "140", "");
        row.av2 = "300";
        row.av3 = "300";
        rows.push_back(row);
    }
    return rows;
}

// Linhöjderna som de ligger i projektet.
lineheight::StoredHeights storedOf(projectdb::ProjectDb& p)
{
    auto s = projectdb::lineHeights(p);
    CHECK(s.ok());
    return s.ok() ? s.value() : lineheight::StoredHeights{};
}

// Sant när spannet finns med exakt de tre värdena.
bool hasSpan(const lineheight::StoredHeights& s, std::int64_t a, std::int64_t b, const std::string& outer,
             const std::string& middle, const std::string& top)
{
    const auto it = s.spans.find(lineheight::spanKey(a, b));
    return it != s.spans.end() && it->second == lineheight::SpanHeights{outer, middle, top};
}

// Raderna i Stolptabell efter med sina id, eller en tom lista.
std::vector<poletable::PoleRow> afterRows(projectdb::ProjectDb& p)
{
    auto rows = projectdb::poles(p, Stage::After);
    CHECK(rows.ok());
    return rows.ok() ? rows.value() : std::vector<poletable::PoleRow>{};
}

// Radernas id i Stolptabell efter: rader utan id får nästa lediga i tabellens ordning, givna id
// står kvar efter omöppning, dubbletter får nya, före har alltid id 0, indexet finns och
// kopieringen från före ger position + 1 och tömmer linhöjderna.
void testRowIds()
{
    std::optional<projectdb::ProjectDb> opened;
    if (!openTestProject("Radid", opened))
        return;
    projectdb::ProjectDb& p = *opened;

    std::vector<poletable::PoleRow> rows = {poleRow("A", "1", "", "", "", "", "", ""), poleRow("B", "2", "", "", "", "", "", ""),
                                            poleRow("C", "3", "", "", "", "", "", ""), poleRow("D", "4", "", "", "", "", "", "")};
    rows[0].id = 5;
    rows[2].id = 9;
    CHECK(projectdb::setPoles(p, Stage::After, rows).ok());
    std::vector<poletable::PoleRow> back = afterRows(p);
    CHECK(back.size() == 4);
    if (back.size() == 4) {
        CHECK(back[0].id == 5 && back[1].id == 10 && back[2].id == 9 && back[3].id == 11);
        CHECK(back[1].pole == "B" && back[3].pole == "D");
    }

    // Ett dubblerat id får ett nytt, det första står kvar.
    rows[1].id = 5;
    rows[3].id = 9;
    CHECK(projectdb::setPoles(p, Stage::After, rows).ok());
    back = afterRows(p);
    if (back.size() == 4) {
        CHECK(back[0].id == 5 && back[2].id == 9);
        CHECK(back[1].id == 10 && back[3].id == 11);
    }

    // Före har alltid id 0, också när raderna skickas med id.
    std::vector<poletable::PoleRow> beforeRows = rows;
    beforeRows[0].id = 7;
    CHECK(projectdb::setPolesBefore(p, beforeRows).ok());
    auto before = projectdb::polesBefore(p);
    CHECK(before.ok() && before.value().size() == 4);
    if (before.ok())
        for (const poletable::PoleRow& row : before.value())
            CHECK(row.id == 0);
    CHECK(countIn(p, "SELECT count(*) FROM sqlite_master WHERE type = 'index' AND name = 'pole_after_row_id'") == 1);

    // Givna id står kvar efter publicering och omöppning.
    const std::vector<poletable::PoleRow> expected = afterRows(p);
    CHECK(publishNow(p));
    const std::string path = p.path();
    p.close();
    auto reopened = projectdb::ProjectDb::open(path, options());
    CHECK(reopened.ok());
    if (!reopened.ok())
        return;
    CHECK(afterRows(reopened.value()) == expected);

    // Kopieringen från före ger id = position + 1 och tömmer linhöjderna.
    CHECK(projectdb::setSpanHeight(reopened.value(), lineheight::spanKey(5, 10), lineheight::Part::Outer, "12.3").ok());
    CHECK(projectdb::setTopAv(reopened.value(), 9, "265.9").ok());
    CHECK(countIn(reopened.value(), "SELECT count(*) FROM line_height_span") == 1);
    CHECK(countIn(reopened.value(), "SELECT count(*) FROM line_height_pole") == 1);
    CHECK(projectdb::copyBeforeToAfter(reopened.value()).ok());
    const std::vector<poletable::PoleRow> copied = afterRows(reopened.value());
    CHECK(copied.size() == 4);
    for (std::size_t i = 0; i < copied.size(); ++i)
        CHECK(copied[i].id == static_cast<std::int64_t>(i) + 1);
    CHECK(countIn(reopened.value(), "SELECT count(*) FROM line_height_span") == 0);
    CHECK(countIn(reopened.value(), "SELECT count(*) FROM line_height_pole") == 0);
    CHECK(storedOf(reopened.value()) == lineheight::StoredHeights{});
}

// Linhöjderna: tur och retur för de tre delarna och topplinans Av, borttagning med tom text,
// Invalid och NotFound utan ändring, och att ett spann går förbi en helt tom rad.
void testLineHeights()
{
    std::optional<projectdb::ProjectDb> opened;
    if (!openTestProject("Linhöjder", opened))
        return;
    projectdb::ProjectDb& p = *opened;
    CHECK(storedOf(p) == lineheight::StoredHeights{});

    // Rad 3 (id 3) är helt tom, så spannen går förbi den: {2, 4} i stället för {2, 3} och {3, 4}.
    std::vector<poletable::PoleRow> rows = twelveRows();
    rows[2] = poletable::PoleRow{};
    CHECK(projectdb::setPoles(p, Stage::After, rows).ok());
    CHECK(afterRows(p).size() == 12 && afterRows(p)[2].id == 3);
    CHECK(storedOf(p) == lineheight::StoredHeights{});

    using lineheight::Part;
    using lineheight::spanKey;
    CHECK(projectdb::setSpanHeight(p, spanKey(5, 6), Part::Outer, "12.33").ok());
    CHECK(projectdb::setSpanHeight(p, spanKey(5, 6), Part::Middle, "12.33").ok());
    CHECK(projectdb::setSpanHeight(p, spanKey(5, 6), Part::Top, "22.63").ok());
    CHECK(projectdb::setSpanHeight(p, spanKey(6, 7), Part::Outer, "21.2").ok());
    CHECK(projectdb::setSpanHeight(p, spanKey(7, 6), Part::Middle, "21.2").ok());
    CHECK(projectdb::setSpanHeight(p, spanKey(6, 7), Part::Top, "31.17").ok());
    CHECK(projectdb::setSpanHeight(p, spanKey(2, 4), Part::Outer, "0").ok());
    CHECK(projectdb::setTopAv(p, 6, "265.9332").ok());
    CHECK(projectdb::setTopAv(p, 7, "-3.5").ok());

    const auto check = [&](projectdb::ProjectDb& q) {
        const lineheight::StoredHeights s = storedOf(q);
        CHECK(s.spans.size() == 3);
        CHECK(hasSpan(s, 5, 6, "12.33", "12.33", "22.63"));
        CHECK(hasSpan(s, 6, 7, "21.2", "21.2", "31.17"));
        CHECK(hasSpan(s, 2, 4, "0", "", ""));
        CHECK(s.topAv.size() == 2 && s.topAv.at(6) == "265.9332" && s.topAv.at(7) == "-3.5");
    };
    check(p);
    CHECK(publishNow(p));
    const std::string path = p.path();
    p.close();
    auto reopened = projectdb::ProjectDb::open(path, options());
    CHECK(reopened.ok());
    if (!reopened.ok())
        return;
    projectdb::ProjectDb& q = reopened.value();
    check(q);

    // Tom text tar bort värdet, och ett spann utan värden försvinner ur tabellen.
    CHECK(projectdb::setSpanHeight(q, spanKey(6, 7), Part::Top, "").ok());
    CHECK(hasSpan(storedOf(q), 6, 7, "21.2", "21.2", ""));
    CHECK(projectdb::setSpanHeight(q, spanKey(6, 7), Part::Outer, "").ok());
    CHECK(projectdb::setSpanHeight(q, spanKey(6, 7), Part::Middle, "").ok());
    CHECK(storedOf(q).spans.count(spanKey(6, 7)) == 0);
    CHECK(countIn(q, "SELECT count(*) FROM line_height_span WHERE row_a = 6 AND row_b = 7") == 0);
    CHECK(projectdb::setTopAv(q, 7, "").ok());
    CHECK(storedOf(q).topAv.count(7) == 0);
    CHECK(countIn(q, "SELECT count(*) FROM line_height_pole") == 1);

    // Invalid och NotFound ändrar ingenting och räknar inte upp revisionen.
    const lineheight::StoredHeights fixed = storedOf(q);
    const auto revision = q.revision();
    const auto invalid = [&](const core::Result<void>& r) { CHECK(!r.ok() && r.error().code == ErrorCode::Invalid); };
    const auto missing = [&](const core::Result<void>& r) { CHECK(!r.ok() && r.error().code == ErrorCode::NotFound); };
    invalid(projectdb::setSpanHeight(q, lineheight::SpanKey{0, 1}, Part::Outer, "1"));
    invalid(projectdb::setSpanHeight(q, lineheight::SpanKey{3, 3}, Part::Outer, "1"));
    invalid(projectdb::setSpanHeight(q, lineheight::SpanKey{7, 6}, Part::Outer, "1"));
    invalid(projectdb::setSpanHeight(q, spanKey(5, 6), Part::Outer, "12,3"));
    invalid(projectdb::setSpanHeight(q, spanKey(5, 6), Part::Outer, "-1"));
    invalid(projectdb::setSpanHeight(q, spanKey(5, 6), Part::Outer, "abc"));
    invalid(projectdb::setTopAv(q, 0, "1"));
    invalid(projectdb::setTopAv(q, -4, "1"));
    invalid(projectdb::setTopAv(q, 6, "1,5"));
    missing(projectdb::setSpanHeight(q, spanKey(5, 7), Part::Outer, "1"));
    missing(projectdb::setSpanHeight(q, spanKey(2, 3), Part::Outer, "1"));
    missing(projectdb::setSpanHeight(q, spanKey(3, 4), Part::Outer, "1"));
    missing(projectdb::setSpanHeight(q, spanKey(11, 13), Part::Outer, "1"));
    missing(projectdb::setTopAv(q, 99, "1"));
    missing(projectdb::setTopAv(q, 3, "1"));
    CHECK(storedOf(q) == fixed);
    CHECK(q.revision() == revision);
}

// Linhöjderna följer raderna i Stolptabell efter (planen 7.6 och 7.7): nytt Stolpnr, dubbletter och
// tomma Stolpnr ändrar inget, en infogad rad med data, en raderad rad och en tömd rad tar bort det som
// hängde på dem, och före, importen och sträckorna rör inget. Kopieringen från före tar bort allt.
void testLineHeightsFollowAfter()
{
    std::optional<projectdb::ProjectDb> opened;
    if (!openTestProject("Linhöjder följer efter", opened))
        return;
    projectdb::ProjectDb& p = *opened;
    using lineheight::Part;
    using lineheight::spanKey;

    CHECK(projectdb::setPoles(p, Stage::After, twelveRows()).ok());
    std::vector<poletable::PoleRow> rows = afterRows(p);
    CHECK(rows.size() == 12 && rows[5].id == 6);
    CHECK(projectdb::setSpanHeight(p, spanKey(5, 6), Part::Outer, "12.33").ok());
    CHECK(projectdb::setSpanHeight(p, spanKey(6, 7), Part::Outer, "21.2").ok());
    CHECK(projectdb::setTopAv(p, 6, "265.9332").ok());
    const lineheight::StoredHeights base = storedOf(p);
    CHECK(base.spans.size() == 2 && base.topAv.size() == 1);

    // 1. Nytt Stolpnr: allt står kvar.
    rows[5].pole = "ST0379A";
    CHECK(projectdb::setPoles(p, Stage::After, rows).ok());
    CHECK(storedOf(p) == base);

    // 2. En dubblett och en rad utan Stolpnr: allt står kvar, och spannen går att skriva i.
    rows[6].pole = "ST0379";
    rows[7].pole = "";
    CHECK(projectdb::setPoles(p, Stage::After, rows).ok());
    CHECK(storedOf(p) == base);
    CHECK(projectdb::setSpanHeight(p, spanKey(6, 7), Part::Middle, "5").ok());
    CHECK(projectdb::setSpanHeight(p, spanKey(7, 8), Part::Middle, "6").ok());
    CHECK(projectdb::setSpanHeight(p, spanKey(6, 7), Part::Middle, "").ok());
    CHECK(projectdb::setSpanHeight(p, spanKey(7, 8), Part::Middle, "").ok());
    CHECK(storedOf(p) == base);
    rows[6].pole = "ST0380";
    rows[7].pole = "ST0381";
    CHECK(projectdb::setPoles(p, Stage::After, rows).ok());
    CHECK(storedOf(p) == base);

    // 3. En tom rad mellan id 5 och 6 får id 13 och rör inget. Får raden data tas {5, 6} bort.
    rows.insert(rows.begin() + 5, poletable::PoleRow{});
    CHECK(projectdb::setPoles(p, Stage::After, rows).ok());
    rows = afterRows(p);
    CHECK(rows.size() == 13 && rows[5].id == 13 && rows[4].id == 5 && rows[6].id == 6);
    CHECK(storedOf(p) == base);
    rows[5].pole = "ST0378A";
    rows[5].av = "300";
    CHECK(projectdb::setPoles(p, Stage::After, rows).ok());
    {
        const lineheight::StoredHeights s = storedOf(p);
        CHECK(s.spans.count(spanKey(5, 6)) == 0);
        CHECK(hasSpan(s, 6, 7, "21.2", "", "") && s.topAv.count(6) == 1);
    }
    CHECK(projectdb::setSpanHeight(p, spanKey(5, 13), Part::Outer, "1").ok());
    CHECK(projectdb::setSpanHeight(p, spanKey(6, 13), Part::Outer, "2").ok());

    // 4. Raden med id 6 tas bort: dess spann och topplinans Av försvinner, och {7, 13} är nytt och tomt.
    rows = afterRows(p);
    rows.erase(rows.begin() + 6);
    CHECK(projectdb::setPoles(p, Stage::After, rows).ok());
    {
        const lineheight::StoredHeights s = storedOf(p);
        CHECK(s.spans.count(spanKey(6, 7)) == 0 && s.spans.count(spanKey(6, 13)) == 0);
        CHECK(s.topAv.count(6) == 0);
        CHECK(hasSpan(s, 5, 13, "1", "", ""));
        CHECK(s.spans.count(spanKey(7, 13)) == 0);
    }
    CHECK(projectdb::setSpanHeight(p, spanKey(7, 13), Part::Outer, "3").ok());
    CHECK(projectdb::setTopAv(p, 7, "1").ok());

    // 5. Alla fält i raden med id 7 töms (id kvar): dess spann och topplinans Av tas bort.
    rows = afterRows(p);
    CHECK(rows[6].id == 7);
    rows[6] = poletable::PoleRow{};
    rows[6].id = 7;
    CHECK(projectdb::setPoles(p, Stage::After, rows).ok());
    const lineheight::StoredHeights remaining = storedOf(p);
    CHECK(remaining.spans.size() == 1 && hasSpan(remaining, 5, 13, "1", "", ""));
    CHECK(remaining.topAv.empty());

    // 6. Före, importen och sträckorna lämnar linhöjderna orörda.
    CHECK(projectdb::setPolesBefore(p, tenDecimalRows()).ok());
    CHECK(storedOf(p) == remaining);
    CHECK(projectdb::importPolesBefore(p, tenDecimalRows(), sampleRecord(tenDecimalRows())).ok());
    CHECK(storedOf(p) == remaining);
    CHECK(projectdb::setAutoSettings(p, Stage::After, standardSettings()).ok());
    CHECK(storedOf(p) == remaining);

    // 7. Kopieringen från före tar bort allt.
    CHECK(projectdb::copyBeforeToAfter(p).ok());
    CHECK(storedOf(p) == lineheight::StoredHeights{});
}

// Ett projekt från v0.6.0 (steg 1–7, user_version 7) migreras till steg 8: raderna i efter får
// row_id = position + 1 (också den tomma), indexet och de två tomma tabellerna finns, allt annat är
// orört, ett spann förbi den tomma raden går att spara och en ny rad sist får id 4.
void testMigrateFromStep7()
{
    const std::string path = join(g_share, "Från steg 7.kalkyl");
    if (!writeV040Project(path))
        return;
    {
        auto d = db::Database::open(path, db::OpenOptions{});
        CHECK(d.ok());
        if (!d.ok())
            return;
        bool ok = d.value().exec(std::string(projectdb::dev::kStep5)).ok();
        ok = ok && d.value().exec(std::string(projectdb::dev::kStep6)).ok();
        ok = ok && d.value().exec(std::string(projectdb::dev::kStep7)).ok();
        ok = ok && d.value().exec(R"sql(
PRAGMA user_version = 7;
INSERT INTO pole_after(position, pole, av, ah, span_m, coord_n, coord_e, coord_h, lkl, av2, av3)
VALUES (0, 'ST0378', '363', NULL, '348', NULL, NULL, '44.19', '255/262', '362', '360'),
       (1, '', NULL, NULL, NULL, NULL, NULL, NULL, '', NULL, NULL),
       (2, 'ST0379', NULL, '12', '320', NULL, NULL, '41.5', '', NULL, '5');
)sql").ok();
        CHECK(ok);
        if (!ok)
            return;
    }
    CHECK(scalar(path, "PRAGMA user_version") == 7);

    auto opened = projectdb::ProjectDb::open(path, options());
    CHECK(opened.ok());
    if (!opened.ok()) {
        std::cerr << opened.error().message << " / " << opened.error().detail << std::endl;
        return;
    }
    projectdb::ProjectDb& p = opened.value();
    CHECK(countIn(p, "PRAGMA user_version") == projectdb::kSchemaVersion);
    CHECK(projectdb::kSchemaVersion == 8);
    CHECK(countIn(p, kNineteenTablesSql) == 19);
    CHECK(countIn(p, "SELECT count(*) FROM pole_after WHERE row_id = position + 1") == 3);
    CHECK(countIn(p, "SELECT count(*) FROM sqlite_master WHERE type = 'index' AND name = 'pole_after_row_id'") == 1);
    CHECK(countIn(p, "SELECT count(*) FROM line_height_span") == 0);
    CHECK(countIn(p, "SELECT count(*) FROM line_height_pole") == 0);
    CHECK(storedOf(p) == lineheight::StoredHeights{});
    checkV040Data(p, false);
    std::vector<poletable::PoleRow> after = afterRows(p);
    CHECK(after.size() == 3);
    if (after.size() == 3) {
        CHECK(after[0].id == 1 && after[1].id == 2 && after[2].id == 3);
        CHECK(after[0].pole == "ST0378" && after[0].av == "363" && after[0].av2 == "362" && after[0].av3 == "360");
        CHECK(poletable::isEmptyRow(after[1]));
        CHECK(after[2].pole == "ST0379" && after[2].ah == "12" && after[2].av3 == "5");
    }

    // Ett spann förbi den tomma raden, och en ny rad sist får id 4.
    CHECK(projectdb::setSpanHeight(p, lineheight::spanKey(1, 3), lineheight::Part::Outer, "12.5").ok());
    CHECK(publishNow(p));
    const std::string reopenPath = p.path();
    p.close();
    auto reopened = projectdb::ProjectDb::open(reopenPath, options());
    CHECK(reopened.ok());
    if (!reopened.ok())
        return;
    CHECK(hasSpan(storedOf(reopened.value()), 1, 3, "12.5", "", ""));
    after = afterRows(reopened.value());
    after.push_back(poleRow("ST0380", "300", "", "", "", "", "", ""));
    CHECK(projectdb::setPoles(reopened.value(), Stage::After, after).ok());
    const std::vector<poletable::PoleRow> grown = afterRows(reopened.value());
    CHECK(grown.size() == 4 && grown[3].id == 4 && grown[3].pole == "ST0380");
    CHECK(hasSpan(storedOf(reopened.value()), 1, 3, "12.5", "", ""));
}

// Provprojektet för röktestet: ligger kvar i KALKYL_PROV_DIR och är stängt och olåst. Det har
// Faslina CURLEW och exempel B, kopplad till både före- och efterraden.
void testProvProject()
{
    const std::string dir = platform::absolutePath(KALKYL_PROV_DIR);
    CHECK(platform::ensureDirectory(dir).ok());
    const std::string path = join(dir, "Prov åäö.kalkyl");
    CHECK(platform::removeFile(path).ok());
    CHECK(platform::removeFile(path + ".lock").ok());
    {
        auto created = projectdb::ProjectDb::create(path, "Prov åäö", options());
        CHECK(created.ok());
        if (!created.ok())
            return;
        projectdb::ProjectDb& p = created.value();
        CHECK(projectdb::setDocConductor(p, projectdb::kPhaseConductorKey, curlew()).ok());
        projectdb::NormalSpan ns;
        ns.note = "Exempel B";
        ns.poles = exampleB();
        auto id = projectdb::saveNormalSpan(p, ns, "ns_calc_phase_before");
        CHECK(id.ok());
        if (id.ok())
            CHECK(projectdb::linkNormalSpan(p, id.value(), "ns_calc_phase_after").ok());
        // Värdena som en kopplad linberäkning hämtar, och två linberäkningar: en kopplad på
        // före-raden och en manuell T1 i läget Excel på efter-raden.
        CHECK(projectdb::setDocValue(p, {"tension_phase", "90", ""}).ok());
        CHECK(projectdb::setDocValue(p, {"normal_span_phase", "314.6", ""}).ok());
        CHECK(projectdb::setDocValue(p, {"temp_min", "-40", ""}).ok());
        CHECK(projectdb::setDocValue(p, {"temp_max", "60", ""}).ok());
        CHECK(projectdb::saveLineCalc(p, linkedCalc("line_calc_phase_before")).ok());
        CHECK(projectdb::saveLineCalc(p, manualCalc(t1Input(linecalc::CalcMode::Excel)),
                                      "line_calc_phase_after")
                  .ok());
        // Stolptabell före med bilaga A, och automatiskt läge med faslinan ST0102–ST0107 och
        // topplinorna ST0101–ST0108. De lagrade beräkningarna ovan vilar på de tagna raderna.
        CHECK(projectdb::setPolesBefore(p, annexRows()).ok());
        CHECK(projectdb::setAutoSettings(p, Stage::Before, standardSettings()).ok());
        // Stolptabell efter: en kopia där ST0104 har spannlängden 365 och ST0105 295, med ST0110
        // (spannlängd 260) tillagd. Sträckorna och läget följer med kopian.
        CHECK(projectdb::copyBeforeToAfter(p).ok());
        std::vector<poletable::PoleRow> after = annexRows();
        after[4].spanM = "365";
        after[5].spanM = "295";
        after.push_back(poleRow("ST0110", "270", "270", "260", "6508990.00", "411570.00", "39.0", ""));
        CHECK(projectdb::setPoles(p, Stage::After, after).ok());
        checkAutoMatches(p);
        // Linhöjder för spannen runt ST0104 och ST0105 (radernas id 5 och 6) och topplinans Av för ST0104.
        CHECK(projectdb::setSpanHeight(p, lineheight::spanKey(4, 5), lineheight::Part::Outer, "12.33").ok());
        CHECK(projectdb::setSpanHeight(p, lineheight::spanKey(5, 6), lineheight::Part::Outer, "21.2").ok());
        CHECK(projectdb::setSpanHeight(p, lineheight::spanKey(4, 5), lineheight::Part::Middle, "12.33").ok());
        CHECK(projectdb::setSpanHeight(p, lineheight::spanKey(5, 6), lineheight::Part::Middle, "21.2").ok());
        CHECK(projectdb::setSpanHeight(p, lineheight::spanKey(4, 5), lineheight::Part::Top, "22.63").ok());
        CHECK(projectdb::setSpanHeight(p, lineheight::spanKey(5, 6), lineheight::Part::Top, "31.17").ok());
        CHECK(projectdb::setTopAv(p, 5, "265.9332").ok());
        CHECK(publishNow(p));
        p.close();
    }
    CHECK(exists(path));
    CHECK(!exists(path + ".lock"));
}

} // namespace

int main()
{
    const std::string base = platform::pathToUtf8(
        fs::temp_directory_path() /
        platform::pathFromUtf8("kalkyl-test-projectdb åäö " + std::to_string(_getpid())));
    std::error_code ec;
    fs::remove_all(platform::pathFromUtf8(base), ec);
    g_share = join(base, "Projekt åäö");
    g_work = join(base, "arbete åäö");
    CHECK(platform::ensureDirectory(g_share).ok());
    CHECK(platform::ensureDirectory(g_work).ok());

    testWithExtension();
    testCreateAndOpen();
    testLocking();
    testOwnLockWait();
    testSnapshotPublish();
    testConflict();
    testBadFiles();
    testMissingAndCancelled();
    testCleanWorkDir();
    testSchemaTables();
    testMigrateFromV010();
    testMigrateFromV020();
    testMigrateFromV030();
    testMissingTablesRejected();
    testDocValues();
    testConductorCopies();
    testNormalSpanRoundTrip();
    testNormalSpanLinking();
    testNormalSpanRemove();
    testLineCalcKeys();
    testLineCalcRoundTrip();
    testLineCalcLinking();
    testLineCalcResolve();
    testLineCalcDetach();
    testLineCalcRemove();
    testPoleTableRoundTrip();
    testPolesAfterRoundTrip();
    testAutoSettingsRoundTrip();
    testCopyBeforeToAfter();
    testAutoCalcsSync();
    testAutoDormantLinks();
    testAutoGuards();
    testAutoIssue();
    testMigrateFromV040();
    testMigrateFromStep5();
    testMigrateFromStep6();
    testPoleImportRoundTrip();
    testPoleImportReplaced();
    testPoleImportUntouched();
    testPoleImportInvalid();
    testMigrateFromStep7();
    testRowIds();
    testLineHeights();
    testLineHeightsFollowAfter();
    testReadOnlyWrites();
    testReadOnlyAutoCalcs();
    testInvalid();
    testPoleTableInvalid();
    testRevision();
    testProvProject();

    fs::remove_all(platform::pathFromUtf8(base), ec);
    if (g_failures == 0)
        std::cout << "projectdb: alla kontroller lyckades" << std::endl;
    else
        std::cerr << "projectdb: " << g_failures << " kontroller misslyckades" << std::endl;
    return g_failures == 0 ? 0 : 1;
}
