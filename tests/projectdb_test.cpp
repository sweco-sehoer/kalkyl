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

// SQL-frågan som räknar de tio tabellerna i schemat.
const char* const kTenTablesSql =
    "SELECT count(*) FROM sqlite_master WHERE type = 'table' AND name IN ('project', "
    "'project_conductor', 'doc_value', 'normal_span', 'normal_span_pole', 'doc_normal_span', "
    "'line_calc', 'line_calc_case', 'doc_line_calc', 'pole_before')";

// Ett nytt projekt har alla tio tabellerna i schemat.
void testSchemaTables()
{
    auto created = projectdb::ProjectDb::create(join(g_share, "Tabeller.kalkyl"), "Tabeller", options());
    CHECK(created.ok());
    if (!created.ok())
        return;
    CHECK(countIn(created.value(), kTenTablesSql) == 10);
    CHECK(countIn(created.value(), "PRAGMA user_version") == projectdb::kSchemaVersion);
    CHECK(projectdb::kSchemaVersion == 4);
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
        CHECK(countIn(p, kTenTablesSql) == 10);
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
        CHECK(countIn(p, kTenTablesSql) == 10);
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

// Ett projekt som v0.3.2 hade det migreras till steg 4: allt gammalt finns kvar, Stolptabell före
// är tom och fungerar, och tabellen med tio decimaler finns kvar efter publicering och ny öppning.
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
        CHECK(countIn(p, "PRAGMA user_version") == 4);
        CHECK(countIn(p, kTenTablesSql) == 10);
        checkV030Data(p);
        auto none = projectdb::polesBefore(p);
        CHECK(none.ok() && none.value().empty());
        CHECK(projectdb::setPolesBefore(p, tenDecimalRows()).ok());
        auto back = projectdb::polesBefore(p);
        CHECK(back.ok() && back.value() == tenDecimalRows());
        CHECK(publishNow(p));
        p.close();
    }
    CHECK(scalar(path, "PRAGMA user_version") == 4);
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
    CHECK(!p.isModified());
    CHECK(p.revision() == 0);
    auto spans = projectdb::normalSpans(p);
    CHECK(spans.ok() && spans.value().size() == 1);
    auto calcs = projectdb::lineCalcs(p);
    CHECK(calcs.ok() && calcs.value().size() == 1);
    auto poles = projectdb::polesBefore(p);
    CHECK(poles.ok() && poles.value().empty());
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
        CHECK(bad.error().message == "Stolptabellen har ett ogiltigt tal på rad 2 i kolumnen Av.");

    rows = tenDecimalRows();
    rows[2].north = "abc";
    bad = projectdb::setPolesBefore(p, rows);
    CHECK(!bad.ok() && bad.error().code == ErrorCode::Invalid);
    if (!bad.ok())
        CHECK(bad.error().message == "Stolptabellen har ett ogiltigt tal på rad 3 i kolumnen Koordinat N.");

    // En ogiltig rad som står sist bland tomma rader avvisas också, och inget har ändrats.
    rows = tenDecimalRows();
    rows.push_back(poleRow("", "", "", "", "", "", "1e5", ""));
    bad = projectdb::setPolesBefore(p, rows);
    CHECK(!bad.ok() && bad.error().code == ErrorCode::Invalid);
    CHECK(p.revision() == revision);
    auto back = projectdb::polesBefore(p);
    CHECK(back.ok() && back.value() == tenDecimalRows());
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
        // Stolptabell före med mockens sex rader (planen, bilaga A).
        CHECK(projectdb::setPolesBefore(
                  p, {poleRow("ST0378", "363", "362", "348", "6512240.85", "412181.98", "44.19", "255/262"),
                      poleRow("ST0379", "267", "320", "375", "6511904.92", "412013.87", "28.05", ""),
                      poleRow("ST0380", "344", "345", "264", "6511653.69", "411933.25", "22.27", "262/262"),
                      poleRow("ST0381", "344", "374", "427", "6511247.08", "411804.35", "30.01", "262/262"),
                      poleRow("ST0382", "378", "305", "322", "6510940.33", "411706.06", "48.54", ""),
                      poleRow("ST0383", "181", "300", "287", "6510691.90", "411562.70", "42.00", "241/241")})
                  .ok());
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
    testReadOnlyWrites();
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
