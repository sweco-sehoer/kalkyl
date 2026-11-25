// Headless-test för projektdatabasen. Allt sker i en egen temporär mapp med åäö i namnet och
// en egen arbetsmapp, aldrig i användarens riktiga data. Sist lämnas ett provprojekt kvar i
// KALKYL_PROV_DIR åt röktestet. Felkoden är antalet misslyckade kontroller.
#include <process.h>

#include <cmath>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <limits>
#include <stop_token>
#include <string>
#include <vector>

#include "calc/normal_span.h"
#include "conductor/conductor.h"
#include "db/database.h"
#include "platform/files.h"
#include "platform/lock_file.h"
#include "platform/system.h"
#include "platform/text.h"
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
std::vector<calc::SpanPole> exampleC()
{
    return {{"P0", 250.0, 10.0}, {"P1", 300.0, 20.0}, {"P2", 200.0, 15.0}, {"P3", 400.0, 30.0}};
}

// Exempel B ur planen: P0 med 999 och därefter 100 (5 st), 400 och 100 (5 st), utan H.
std::vector<calc::SpanPole> exampleB()
{
    std::vector<calc::SpanPole> poles;
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

// Ett nytt projekt har alla sex tabellerna i schemat.
void testSchemaTables()
{
    auto created = projectdb::ProjectDb::create(join(g_share, "Tabeller.kalkyl"), "Tabeller", options());
    CHECK(created.ok());
    if (!created.ok())
        return;
    CHECK(countIn(created.value(),
                  "SELECT count(*) FROM sqlite_master WHERE type = 'table' AND name IN ('project', "
                  "'project_conductor', 'doc_value', 'normal_span', 'normal_span_pole', "
                  "'doc_normal_span')") == 6);
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
        CHECK(countIn(p,
                      "SELECT count(*) FROM sqlite_master WHERE type = 'table' AND name IN ('project', "
                      "'project_conductor', 'doc_value', 'normal_span', 'normal_span_pole', "
                      "'doc_normal_span')") == 6);
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
    ns.poles.insert(ns.poles.begin() + 2, calc::SpanPole{});
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
        const std::vector<calc::SpanPole> want = exampleC();
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
    CHECK(!p.isModified());
    CHECK(p.revision() == 0);
    auto spans = projectdb::normalSpans(p);
    CHECK(spans.ok() && spans.value().size() == 1);
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
    testMissingTablesRejected();
    testDocValues();
    testConductorCopies();
    testNormalSpanRoundTrip();
    testNormalSpanLinking();
    testNormalSpanRemove();
    testReadOnlyWrites();
    testInvalid();
    testRevision();
    testProvProject();

    fs::remove_all(platform::pathFromUtf8(base), ec);
    if (g_failures == 0)
        std::cout << "projectdb: alla kontroller lyckades" << std::endl;
    else
        std::cerr << "projectdb: " << g_failures << " kontroller misslyckades" << std::endl;
    return g_failures == 0 ? 0 : 1;
}
