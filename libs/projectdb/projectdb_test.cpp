// Headless-test för projektdatabasen. Allt sker i en egen temporär mapp med åäö i namnet och
// en egen arbetsmapp, aldrig i användarens riktiga data. Sist lämnas ett provprojekt kvar i
// KALKYL_PROV_DIR åt röktestet. Felkoden är antalet misslyckade kontroller.
#include <process.h>

#include <filesystem>
#include <fstream>
#include <iostream>
#include <stop_token>
#include <string>
#include <vector>

#include "db/database.h"
#include "platform/files.h"
#include "platform/lock_file.h"
#include "platform/system.h"
#include "platform/text.h"
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
    p.published(stamp.value());
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
        p.published(stamp2.value());
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

// Provprojektet för röktestet: ligger kvar i KALKYL_PROV_DIR och är stängt och olåst.
void testProvProject()
{
    const std::string dir = platform::absolutePath(KALKYL_PROV_DIR);
    CHECK(platform::ensureDirectory(dir).ok());
    const std::string path = join(dir, "Prov åäö.kalkyl");
    CHECK(platform::removeFile(path).ok());
    CHECK(platform::removeFile(path + ".lock").ok());
    CHECK(makeProject(path, "Prov åäö"));
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
    testProvProject();

    fs::remove_all(platform::pathFromUtf8(base), ec);
    if (g_failures == 0)
        std::cout << "projectdb: alla kontroller lyckades" << std::endl;
    else
        std::cerr << "projectdb: " << g_failures << " kontroller misslyckades" << std::endl;
    return g_failures == 0 ? 0 : 1;
}
