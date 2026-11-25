// Headless-test för db: Database, Statement och migrate.
// Allt går mot en lokal temporär mapp med åäö i namnet. Felkoden är antalet fel.
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <string>

#include <windows.h>

#include "db/database.h"
#include "db/migrate.h"
#include "platform/files.h"
#include "platform/text.h"

using namespace kalkyl::db;
using kalkyl::core::Error;
using kalkyl::core::ErrorCode;
using kalkyl::core::Result;

namespace {

int g_failures = 0;

#define CHECK(cond)                                                                      \
    do {                                                                                 \
        if (!(cond)) {                                                                   \
            ++g_failures;                                                                \
            std::fprintf(stderr, "FEL %s:%d: %s\n", __FILE__, __LINE__, #cond);         \
        }                                                                                \
    } while (0)

// Skriver en fil med givet innehåll. Sökvägen är UTF-8.
void writeFile(const std::string& path, const std::string& content)
{
    std::ofstream out(kalkyl::platform::pathFromUtf8(path), std::ios::binary);
    out << content;
}

// Sant om filen finns just nu.
bool exists(const std::string& path)
{
    return kalkyl::platform::stamp(path).ok();
}

// Öppnar en ny fil för test och stannar testet om det misslyckas.
Database openNew(const std::string& path, bool exclusive = false)
{
    OpenOptions o;
    o.mode = OpenMode::CreateNew;
    o.exclusive = exclusive;
    auto r = Database::open(path, o);
    CHECK(r.ok());
    if (!r.ok()) {
        std::fprintf(stderr, "  %s\n  %s\n", r.error().message.c_str(), r.error().detail.c_str());
        return Database();
    }
    return std::move(r).value();
}

// Skapa och öppna i en mapp med åäö, och felkoderna för saknad och befintlig fil.
void testOpen(const std::string& dir)
{
    const std::string path = dir + "/test åäö.db";
    {
        Database db = openNew(path);
        CHECK(db.path() == path);
        CHECK(db.exec("CREATE TABLE t(a)").ok());
    }
    CHECK(exists(path));

    OpenOptions existing;
    CHECK(Database::open(path, existing).ok());

    OpenOptions createNew;
    createNew.mode = OpenMode::CreateNew;
    auto dup = Database::open(path, createNew);
    CHECK(!dup.ok() && dup.error().code == ErrorCode::AlreadyExists);

    auto missing = Database::open(dir + "/saknas.db", existing);
    CHECK(!missing.ok() && missing.error().code == ErrorCode::NotFound);
    auto noDir = Database::open(dir + "/ingen mapp/x.db", createNew);
    CHECK(!noDir.ok() && noDir.error().code == ErrorCode::NotFound);

    OpenOptions orCreate;
    orCreate.mode = OpenMode::OpenOrCreate;
    CHECK(Database::open(dir + "/eller.db", orCreate).ok());
    CHECK(exists(dir + "/eller.db"));

    // Skrivskyddad öppning: läsning går, skrivning ger AccessDenied.
    OpenOptions ro;
    ro.readOnly = true;
    auto readOnly = Database::open(path, ro);
    CHECK(readOnly.ok());
    if (readOnly.ok()) {
        Database& d = readOnly.value();
        CHECK(d.exec("SELECT count(*) FROM t").ok());
        auto w = d.exec("INSERT INTO t VALUES (1)");
        CHECK(!w.ok() && w.error().code == ErrorCode::AccessDenied);
    }

    // En databas i minnet har ingen sökväg.
    auto mem = Database::openMemory();
    CHECK(mem.ok() && mem.value().path().empty());

    // En flyttad databas lämnar källan tom, och en tom databas går att stänga.
    Database a = openNew(dir + "/flytt.db");
    Database b = std::move(a);
    CHECK(a.path().empty() && !b.path().empty());
    a.close();
    b = Database();
    CHECK(b.path().empty());
}

// Bindning, stegning, NULL och återanvändning av en sats.
void testStatements(const std::string& dir)
{
    Database db = openNew(dir + "/satser.db");
    CHECK(db.exec("CREATE TABLE t(id INTEGER PRIMARY KEY, name TEXT, x REAL, n INTEGER)").ok());

    auto ins = db.prepare("INSERT INTO t(name, x, n) VALUES (?1, ?2, ?3)");
    CHECK(ins.ok());
    if (!ins.ok())
        return;
    Statement& s = ins.value();
    CHECK(s.bind(1, std::string_view("Åsa åäö")).bind(2, 2.5).bind(3, std::int64_t{42}).run().ok());
    s.reset();
    CHECK(s.bind(1, std::string_view("")).bind(2, 0.0).bindNull(3).run().ok());
    s.reset();
    CHECK(s.bindNull(1).bind(2, -1.0).bind(3, std::int64_t{-7}).run().ok());

    auto sel = db.prepare("SELECT name, x, n FROM t ORDER BY id");
    CHECK(sel.ok());
    if (!sel.ok())
        return;
    Statement& q = sel.value();
    auto row = q.step();
    CHECK(row.ok() && row.value());
    CHECK(q.text(0) == "Åsa åäö" && q.real(1) == 2.5 && q.integer(2) == 42);
    CHECK(!q.isNull(0) && !q.isNull(2));
    row = q.step();
    CHECK(row.ok() && row.value());
    CHECK(q.text(0).empty() && !q.isNull(0));   // en tom text är inte NULL
    CHECK(q.isNull(2));
    row = q.step();
    CHECK(row.ok() && row.value());
    CHECK(q.isNull(0) && q.text(0).empty());    // NULL ger en tom text
    CHECK(q.integer(2) == -7);
    row = q.step();
    CHECK(row.ok() && !row.value());

    // reset gör att satsen kan köras om.
    q.reset();
    row = q.step();
    CHECK(row.ok() && row.value() && q.text(0) == "Åsa åäö");

    // Ett SQL-fel ger Sql, och en dubblett i primärnyckel likaså.
    auto bad = db.prepare("SELEKT 1");
    CHECK(!bad.ok() && bad.error().code == ErrorCode::Sql);
    CHECK(!db.exec("INSERT INTO t(id) VALUES (1)").ok());

    // pragmaInt läser heltal.
    auto fk = db.pragmaInt("foreign_keys");
    CHECK(fk.ok() && fk.value() == 1);
}

// En transaktion rullas tillbaka vid fel och sparas annars.
void testTransaction(const std::string& dir)
{
    Database db = openNew(dir + "/transaktion.db");
    CHECK(db.exec("CREATE TABLE t(a INTEGER)").ok());
    auto count = [&]() -> std::int64_t {
        auto st = db.prepare("SELECT count(*) FROM t");
        if (!st.ok() || !st.value().step().ok())
            return -1;
        return st.value().integer(0);
    };

    auto failed = db.transaction([&]() -> Result<void> {
        if (auto r = db.exec("INSERT INTO t VALUES (1)"); !r)
            return r;
        Error e;
        e.code = ErrorCode::Io;
        e.message = "eget fel";
        return e;
    });
    CHECK(!failed.ok() && failed.error().code == ErrorCode::Io && failed.error().message == "eget fel");
    CHECK(count() == 0);

    auto good = db.transaction([&]() -> Result<void> { return db.exec("INSERT INTO t VALUES (1), (2)"); });
    CHECK(good.ok());
    CHECK(count() == 2);

    // Efter en återrullning går nästa transaktion att starta.
    CHECK(db.transaction([&]() -> Result<void> { return db.exec("INSERT INTO t VALUES (3)"); }).ok());
    CHECK(count() == 3);
}

// Migreringarna: initiering, uppgradering, fel id, för ny version och atomicitet.
void testMigrate(const std::string& dir)
{
    const std::uint32_t id = 0x4B4C4B54;
    Schema v1;
    v1.applicationId = id;
    v1.steps = {"CREATE TABLE a(x INTEGER)"};
    v1.allowInitialize = true;

    Database db = openNew(dir + "/migrering.db");
    auto first = migrate(db, v1);
    CHECK(first.ok() && first.value());
    CHECK(db.pragmaInt("application_id").value() == static_cast<std::int64_t>(id));
    CHECK(db.pragmaInt("user_version").value() == 1);
    auto second = migrate(db, v1);
    CHECK(second.ok() && !second.value());

    // Ett steg till uppgraderar, och bara det nya steget körs.
    Schema v2 = v1;
    v2.allowInitialize = false;
    v2.steps = {"CREATE TABLE a(x INTEGER)", "CREATE TABLE b(y INTEGER)"};
    auto up = migrate(db, v2);
    CHECK(up.ok() && up.value());
    CHECK(db.pragmaInt("user_version").value() == 2);
    CHECK(db.exec("INSERT INTO b VALUES (1)").ok());

    // Schemat är nyare än programmet: TooNew, och ingenting ändras.
    auto tooNew = migrate(db, v1);
    CHECK(!tooNew.ok() && tooNew.error().code == ErrorCode::TooNew);
    CHECK(db.pragmaInt("user_version").value() == 2);

    // Fel application_id.
    Schema other = v1;
    other.applicationId = id + 1;
    auto wrong = migrate(db, other);
    CHECK(!wrong.ok() && wrong.error().code == ErrorCode::WrongApplication);

    // En tom fil utan tillåten initiering är inte Kalkyls.
    Database empty = openNew(dir + "/tom.db");
    Schema noInit = v1;
    noInit.allowInitialize = false;
    auto refused = migrate(empty, noInit);
    CHECK(!refused.ok() && refused.error().code == ErrorCode::WrongApplication);
    CHECK(empty.pragmaInt("user_version").value() == 0);

    // En vanlig SQLite-fil med tabeller men utan application_id är inte Kalkyls, ens med initiering.
    Database plain = openNew(dir + "/vanlig.db");
    CHECK(plain.exec("CREATE TABLE z(q)").ok());
    auto plainRes = migrate(plain, v1);
    CHECK(!plainRes.ok() && plainRes.error().code == ErrorCode::WrongApplication);

    // Ett steg som misslyckas rullar tillbaka alla: ingen halv migrering.
    Schema broken = v1;
    broken.steps = {"CREATE TABLE ok1(x)", "CREATE TABLE ok2(x)", "SELEKT FEL"};
    Database atomic = openNew(dir + "/atomisk.db");
    auto fail = migrate(atomic, broken);
    CHECK(!fail.ok() && fail.error().code == ErrorCode::Sql);
    CHECK(atomic.pragmaInt("user_version").value() == 0);
    CHECK(atomic.pragmaInt("application_id").value() == 0);
    CHECK(!atomic.exec("INSERT INTO ok1 VALUES (1)").ok());   // tabellen finns inte

    // En textfil är ingen databas.
    const std::string text = dir + "/text.db";
    writeFile(text, std::string(2000, 'x'));
    OpenOptions existing;
    auto opened = Database::open(text, existing);
    if (opened.ok()) {
        auto m = migrate(opened.value(), v1);
        CHECK(!m.ok() && m.error().code == ErrorCode::NotADatabase);
    } else {
        CHECK(opened.error().code == ErrorCode::NotADatabase);
    }
    // Samma sak skrivskyddat, där ingen pragma rör filen vid öppning.
    OpenOptions ro;
    ro.readOnly = true;
    auto openedRo = Database::open(text, ro);
    if (openedRo.ok()) {
        auto m = migrate(openedRo.value(), v1);
        CHECK(!m.ok() && m.error().code == ErrorCode::NotADatabase);
    } else {
        CHECK(openedRo.error().code == ErrorCode::NotADatabase);
    }
}

// Ögonblicksbilden med VACUUM INTO och kontrollen av en hel fil.
void testSnapshot(const std::string& dir)
{
    const std::uint32_t id = 0x4B4C4B54;
    Schema schema;
    schema.applicationId = id;
    schema.steps = {"CREATE TABLE a(x TEXT)"};
    schema.allowInitialize = true;

    Database db = openNew(dir + "/original.db");
    CHECK(migrate(db, schema).ok());
    CHECK(db.exec("INSERT INTO a VALUES ('åäö')").ok());
    CHECK(db.quickCheck().ok());

    const std::string snap = dir + "/bild åäö.db";
    CHECK(db.vacuumInto(snap).ok());
    CHECK(exists(snap));
    auto again = db.vacuumInto(snap);
    CHECK(!again.ok() && again.error().code == ErrorCode::AlreadyExists);

    OpenOptions existing;
    auto copy = Database::open(snap, existing);
    CHECK(copy.ok());
    if (copy.ok()) {
        Database& c = copy.value();
        CHECK(c.pragmaInt("user_version").value() == 1);
        CHECK(c.pragmaInt("application_id").value() == static_cast<std::int64_t>(id));
        CHECK(c.quickCheck().ok());
        auto st = c.prepare("SELECT x FROM a");
        CHECK(st.ok() && st.value().step().ok() && st.value().text(0) == "åäö");
    }

    // En trunkerad fil upptäcks av migrate eller quickCheck, och ger aldrig ett lyckat svar.
    const std::string cut = dir + "/trasig.db";
    std::error_code ec;
    std::filesystem::copy_file(kalkyl::platform::pathFromUtf8(snap), kalkyl::platform::pathFromUtf8(cut), ec);
    std::filesystem::resize_file(kalkyl::platform::pathFromUtf8(cut), 50, ec);
    auto broken = Database::open(cut, existing);
    if (broken.ok()) {
        auto m = migrate(broken.value(), schema);
        auto q = broken.value().quickCheck();
        CHECK(!m.ok() || !q.ok());
    }
}

// Exklusiv låsning: en annan anslutning får vänta och sedan Busy.
void testExclusive(const std::string& dir)
{
    const std::string path = dir + "/exklusiv.db";
    Database a = openNew(path, true);
    CHECK(a.exec("CREATE TABLE t(a)").ok());
    CHECK(a.exec("INSERT INTO t VALUES (1)").ok());

    OpenOptions b;
    b.busyTimeoutMs = 100;
    auto other = Database::open(path, b);
    bool busy = !other.ok() && other.error().code == ErrorCode::Busy;
    if (other.ok())
        busy = !other.value().exec("SELECT count(*) FROM t").ok();
    CHECK(busy);
}

} // namespace

int main()
{
    const std::string root = kalkyl::platform::pathToUtf8(std::filesystem::temp_directory_path()) + "/kalkyl-db-" +
                             std::to_string(GetCurrentProcessId());
    const std::string dir = root + "/Prov åäö";
    if (!kalkyl::platform::ensureDirectory(dir).ok()) {
        std::fprintf(stderr, "FEL: kunde inte skapa testmappen\n");
        return 1;
    }
    testOpen(dir);
    testStatements(dir);
    testTransaction(dir);
    testMigrate(dir);
    testSnapshot(dir);
    testExclusive(dir);

    std::error_code ec;
    std::filesystem::remove_all(kalkyl::platform::pathFromUtf8(root), ec);
    if (g_failures == 0)
        std::printf("db: alla tester gick igenom\n");
    return g_failures;
}
