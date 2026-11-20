// Headless-test för systemdatabasen. Allt sker i en egen temporär mapp med åäö i namnet,
// aldrig i användarens riktiga data. Felkoden är antalet misslyckade kontroller.
#include <process.h>
#include <stdlib.h>

#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <thread>
#include <vector>

#include "db/database.h"
#include "platform/files.h"
#include "platform/system.h"
#include "platform/text.h"
#include "systemdb/system_db.h"

using namespace kalkyl;
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

// Fogar ihop mapp och namn till en UTF-8-sökväg.
std::string join(const std::string& dir, const std::string& name)
{
    return platform::pathToUtf8(platform::pathFromUtf8(dir) / platform::pathFromUtf8(name));
}

// Pausar så att två anrop får olika tidsstämplar (millisekunders upplösning).
void tick()
{
    std::this_thread::sleep_for(std::chrono::milliseconds(3));
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

// Filen skapas, och läget finns kvar efter stängning och ny öppning.
void testPersistence(const std::string& dir)
{
    const std::string path = join(dir, "system.db");
    {
        auto sys = systemdb::SystemDb::open(path);
        CHECK(sys.ok());
        if (!sys.ok())
            return;
        CHECK(sys.value().isPersistent());
        CHECK(sys.value().fallbackReason().empty());
        CHECK(fs::exists(platform::pathFromUtf8(path)));
        CHECK(sys.value().themeMode() == systemdb::ThemeMode::System);
        CHECK(sys.value().setThemeMode(systemdb::ThemeMode::Dark).ok());
        CHECK(sys.value().themeMode() == systemdb::ThemeMode::Dark);
        CHECK(sys.value().lastFolder().empty());
        CHECK(sys.value().setLastFolder(join(dir, "Mapp åäö")).ok());
    }
    auto again = systemdb::SystemDb::open(path);
    CHECK(again.ok());
    if (!again.ok())
        return;
    CHECK(again.value().themeMode() == systemdb::ThemeMode::Dark);
    CHECK(again.value().lastFolder() == join(dir, "Mapp åäö"));
    CHECK(again.value().setThemeMode(systemdb::ThemeMode::Light).ok());
    CHECK(again.value().themeMode() == systemdb::ThemeMode::Light);
    CHECK(again.value().setThemeMode(systemdb::ThemeMode::System).ok());
    CHECK(again.value().themeMode() == systemdb::ThemeMode::System);
}

// Senaste-listan: upsert, ordning, trimning till tio, borttagning och dubbletter via pathKey.
void testRecent(const std::string& dir)
{
    auto sys = systemdb::SystemDb::open(join(dir, "recent.db"));
    CHECK(sys.ok());
    if (!sys.ok())
        return;
    systemdb::SystemDb& s = sys.value();

    auto empty = s.recentProjects();
    CHECK(empty.ok() && empty.value().empty());

    CHECK(s.touchRecentProject(join(dir, "Ett.kalkyl"), "Ett").ok());
    tick();
    CHECK(s.touchRecentProject(join(dir, "Två åäö.kalkyl"), "Två").ok());
    tick();
    auto two = s.recentProjects();
    CHECK(two.ok() && two.value().size() == 2);
    if (two.ok() && two.value().size() == 2) {
        CHECK(two.value()[0].name == "Två");
        CHECK(two.value()[0].path == join(dir, "Två åäö.kalkyl"));
        CHECK(two.value()[1].name == "Ett");
        CHECK(!two.value()[0].lastOpenedUtc.empty());
    }

    // Upsert: ett nytt öppnande flyttar raden överst och byter namn.
    CHECK(s.touchRecentProject(join(dir, "Ett.kalkyl"), "Ett igen").ok());
    tick();
    auto upsert = s.recentProjects();
    CHECK(upsert.ok() && upsert.value().size() == 2);
    if (upsert.ok() && upsert.value().size() == 2) {
        CHECK(upsert.value()[0].name == "Ett igen");
        CHECK(upsert.value()[1].name == "Två");
    }

    // Dubbletter: samma fil med andra versaler och åäö ger en rad. Det senaste skrivsättet visas.
    CHECK(s.touchRecentProject(join(dir, "TVÅ ÅÄÖ.KALKYL"), "Två versaler").ok());
    tick();
    auto dup = s.recentProjects();
    CHECK(dup.ok() && dup.value().size() == 2);
    if (dup.ok() && dup.value().size() == 2) {
        CHECK(dup.value()[0].name == "Två versaler");
        CHECK(dup.value()[0].path == join(dir, "TVÅ ÅÄÖ.KALKYL"));
    }

    // Borttagning går via pathKey, så ett annat skrivsätt träffar raden.
    CHECK(s.removeRecentProject(join(dir, "två åäö.kalkyl")).ok());
    auto removed = s.recentProjects();
    CHECK(removed.ok() && removed.value().size() == 1);
    CHECK(s.removeRecentProject(join(dir, "finns inte.kalkyl")).ok());

    // Trimning: tolv projekt ger tio rader, de två äldsta försvinner och den senaste ligger överst.
    for (int i = 1; i <= 12; ++i) {
        CHECK(s.touchRecentProject(join(dir, "P" + std::to_string(i) + ".kalkyl"),
                                   "P" + std::to_string(i)).ok());
        tick();
    }
    auto trimmed = s.recentProjects();
    CHECK(trimmed.ok() && trimmed.value().size() == static_cast<std::size_t>(systemdb::kMaxRecent));
    if (trimmed.ok() && trimmed.value().size() == static_cast<std::size_t>(systemdb::kMaxRecent)) {
        CHECK(trimmed.value().front().name == "P12");
        CHECK(trimmed.value().back().name == "P3");
        for (const auto& r : trimmed.value()) {
            CHECK(r.name != "P1");
            CHECK(r.name != "P2");
            CHECK(r.name != "Ett igen");
        }
    }
}

// En skadad fil och en främmande SQLite-fil flyttas undan och ersätts av en ny.
void testBrokenFile(const std::string& dir)
{
    const std::string textPath = join(dir, "text.db");
    {
        std::ofstream out(platform::pathFromUtf8(textPath), std::ios::binary);
        out << "det här är ingen databas, bara text som är tillräckligt lång för att inte vara tom";
    }
    {
        auto sys = systemdb::SystemDb::open(textPath);
        CHECK(sys.ok());
        if (sys.ok()) {
            CHECK(sys.value().isPersistent());
            CHECK(sys.value().setThemeMode(systemdb::ThemeMode::Dark).ok());
        }
    }
    int moved = 0;
    for (const std::string& name : listNames(dir))
        if (name.rfind("system-trasig-", 0) == 0)
            ++moved;
    CHECK(moved == 1);

    // En SQLite-fil med annat application_id (här 0, med en tabell) avvisas på samma sätt.
    const std::string foreignDir = join(dir, "främmande");
    CHECK(platform::ensureDirectory(foreignDir).ok());
    const std::string foreignPath = join(foreignDir, "system.db");
    {
        db::OpenOptions o;
        o.mode = db::OpenMode::CreateNew;
        auto foreign = db::Database::open(foreignPath, o);
        CHECK(foreign.ok());
        if (foreign.ok())
            CHECK(foreign.value().exec("CREATE TABLE x(a INTEGER)").ok());
    }
    auto sys = systemdb::SystemDb::open(foreignPath);
    CHECK(sys.ok());
    if (sys.ok())
        CHECK(sys.value().themeMode() == systemdb::ThemeMode::System);
    int movedForeign = 0;
    for (const std::string& name : listNames(foreignDir))
        if (name.rfind("system-trasig-", 0) == 0)
            ++movedForeign;
    CHECK(movedForeign == 1);
}

// Reserven i minnet fungerar men är inte beständig, och en mapp som saknas skapas.
void testMemoryAndDirectory(const std::string& dir)
{
    auto mem = systemdb::SystemDb::openInMemory("orsak åäö");
    CHECK(mem.ok());
    if (mem.ok()) {
        CHECK(!mem.value().isPersistent());
        CHECK(mem.value().fallbackReason() == "orsak åäö");
        CHECK(mem.value().setThemeMode(systemdb::ThemeMode::Light).ok());
        CHECK(mem.value().themeMode() == systemdb::ThemeMode::Light);
        CHECK(mem.value().touchRecentProject(join(dir, "A.kalkyl"), "A").ok());
        auto list = mem.value().recentProjects();
        CHECK(list.ok() && list.value().size() == 1);
    }

    const std::string nested = join(join(join(dir, "ny åäö"), "under"), "system.db");
    auto sys = systemdb::SystemDb::open(nested);
    CHECK(sys.ok());
    CHECK(fs::exists(platform::pathFromUtf8(nested)));
}

// defaultPath följer KALKYL_DATA_DIR, så att tester aldrig rör användarens riktiga data.
void testDefaultPath(const std::string& dir)
{
    const std::string data = join(dir, "data åäö");
    _wputenv_s(L"KALKYL_DATA_DIR", platform::toWide(data).c_str());
    CHECK(platform::pathKey(systemdb::defaultPath()) == platform::pathKey(join(data, "system.db")));
}

} // namespace

int main()
{
    const std::string dir = platform::pathToUtf8(
        fs::temp_directory_path() /
        platform::pathFromUtf8("kalkyl-test-systemdb åäö " + std::to_string(_getpid())));
    std::error_code ec;
    fs::remove_all(platform::pathFromUtf8(dir), ec);
    CHECK(platform::ensureDirectory(dir).ok());

    testPersistence(dir);
    testRecent(dir);
    testBrokenFile(dir);
    testMemoryAndDirectory(dir);
    testDefaultPath(dir);

    fs::remove_all(platform::pathFromUtf8(dir), ec);
    if (g_failures == 0)
        std::cout << "systemdb: alla kontroller lyckades" << std::endl;
    else
        std::cerr << "systemdb: " << g_failures << " kontroller misslyckades" << std::endl;
    return g_failures == 0 ? 0 : 1;
}
