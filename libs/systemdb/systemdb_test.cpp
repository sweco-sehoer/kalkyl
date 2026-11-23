// Headless-test för systemdatabasen. Allt sker i en egen temporär mapp med åäö i namnet,
// aldrig i användarens riktiga data. Felkoden är antalet misslyckade kontroller.
#include <process.h>
#include <stdlib.h>

#include <chrono>
#include <cmath>
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

    // Reserven i minnet har också startdatan.
    if (mem.ok()) {
        auto list = mem.value().conductors();
        CHECK(list.ok() && list.value().size() == 129);
    }
}

// Jämför två tal med toleransen 1e-9.
bool near(double a, double b)
{
    return std::fabs(a - b) < 1e-9;
}

// Hittar en lina på namn (exakt) i listan, eller nullptr.
const systemdb::Conductor* findByName(const std::vector<systemdb::Conductor>& list,
                                      const std::string& name)
{
    for (const auto& c : list)
        if (c.name == name)
            return &c;
    return nullptr;
}

// Läser hela registret. Misslyckas det räknas ett fel och listan blir tom.
std::vector<systemdb::Conductor> readAll(systemdb::SystemDb& sys)
{
    auto all = sys.conductors();
    CHECK(all.ok());
    return all.ok() ? all.value() : std::vector<systemdb::Conductor>{};
}

// Startdatan: antal, ordning, utvalda rader, de särskilda fallen och reglerna för hela registret.
void testConductorSeed(const std::string& dir)
{
    auto opened = systemdb::SystemDb::open(join(dir, "seed.db"));
    CHECK(opened.ok());
    if (!opened.ok())
        return;
    systemdb::SystemDb& sys = opened.value();
    const auto all = readAll(sys);
    CHECK(all.size() == 129);
    if (all.size() != 129)
        return;
    CHECK(all.front().name == "910 mm² FeAl FALCON");
    CHECK(all.back().name == "455 mm² Skywrap Condor 1x64HM4");
    for (std::size_t i = 1; i < all.size(); ++i)
        CHECK(all[i].id > all[i - 1].id);

    const auto* curlew = findByName(all, "593 mm² FeAl CURLEW");
    CHECK(curlew != nullptr);
    if (curlew) {
        CHECK(curlew->type == "FeAl CURLEW");
        CHECK(near(curlew->areaMm2, 593));
        CHECK(near(curlew->diameterMm, 31.7));
        CHECK(near(curlew->eInitialMpa, 52000));
        CHECK(near(curlew->ePermanentMpa, 72000));
        CHECK(near(curlew->eInitialLimitMpa, 120));
        CHECK(near(curlew->eivConstant, 90));
        CHECK(near(curlew->alphaE6PerC, 19));
        CHECK(near(curlew->creepStrainPermille, 0.4));
        CHECK(near(curlew->creepTempC, 21));
        CHECK(near(curlew->weightNPerM, 19.6));
        CHECK(curlew->breakingLoadKn && near(*curlew->breakingLoadKn, 165));
        CHECK(curlew->origin == systemdb::ConductorOrigin::Seed);
        auto one = sys.conductor(curlew->id);
        CHECK(one.ok() && one.value() && one.value()->name == curlew->name);
    }
    auto missing = sys.conductor(99999);
    CHECK(missing.ok() && !missing.value());

    const auto* adss = findByName(all, "209 mm² ADSS, SD-nDJ6/31");
    CHECK(adss && near(adss->alphaE6PerC, -0.039));
    const auto* fe60 = findByName(all, "185 mm² Fe60");
    CHECK(fe60 && !fe60->breakingLoadKn);
    const auto* bluejay = findByName(all, "Bluejay");
    CHECK(bluejay && bluejay->type.empty());
    const auto* limit = findByName(all, "184 mm² ADSS, SD-124DJ8");
    CHECK(limit && near(limit->eInitialLimitMpa, 99999));
    CHECK(findByName(all, "16 mm² Cu (Ø 5,1 mm)") != nullptr);
    CHECK(findByName(all, "16 mm² Cu (Ø 4,5 mm)") != nullptr);
    CHECK(findByName(all, "16 mm² Cu") == nullptr);

    int noBreaking = 0;
    int noType = 0;
    for (std::size_t i = 0; i < all.size(); ++i) {
        const auto& c = all[i];
        if (!c.breakingLoadKn)
            ++noBreaking;
        if (c.type.empty())
            ++noType;
        CHECK(c.origin == systemdb::ConductorOrigin::Seed);
        CHECK(systemdb::validateConductor(c).empty());
        CHECK(!systemdb::creepTempDeviates(c));
        for (std::size_t j = i + 1; j < all.size(); ++j)
            CHECK(!systemdb::sameConductorName(c.name, all[j].name));
    }
    CHECK(noBreaking == 7);
    CHECK(noType == 20);
}

// Redigering: ursprung, sparning utan ändring, egna linor, fel och namnbyten mellan rader.
void testConductorEdit(const std::string& dir)
{
    const std::string path = join(dir, "edit.db");
    std::int64_t curlewId = 0;
    {
        auto opened = systemdb::SystemDb::open(path);
        CHECK(opened.ok());
        if (!opened.ok())
            return;
        systemdb::SystemDb& sys = opened.value();
        auto all = readAll(sys);
        const auto* curlew = findByName(all, "593 mm² FeAl CURLEW");
        const auto* morkulla = findByName(all, "593 mm² FeAl MORKULLA");
        CHECK(curlew && morkulla);
        if (!curlew || !morkulla)
            return;
        curlewId = curlew->id;

        // En ändrad startdatalina blir SeedModified, och en sparning utan ändring rör inte flaggan.
        systemdb::Conductor changed = *curlew;
        changed.diameterMm = 32;
        auto saved = sys.saveConductor(changed);
        CHECK(saved.ok() && saved.value() == curlewId);
        CHECK(sys.saveConductor(*morkulla).ok());
        auto one = sys.conductor(curlewId);
        CHECK(one.ok() && one.value() && one.value()->origin == systemdb::ConductorOrigin::SeedModified);
        CHECK(one.ok() && one.value() && near(one.value()->diameterMm, 32));
        auto mork = sys.conductor(morkulla->id);
        CHECK(mork.ok() && mork.value() && mork.value()->origin == systemdb::ConductorOrigin::Seed);

        // En egen lina får ett nytt id, hamnar sist och trimmas.
        systemdb::Conductor own = *morkulla;
        own.id = 0;
        own.name = "  Testlina åäö ";
        auto added = sys.saveConductor(own);
        CHECK(added.ok() && added.value() > 129);
        auto after = readAll(sys);
        CHECK(after.size() == 130);
        if (after.size() == 130) {
            CHECK(after.back().name == "Testlina åäö");
            CHECK(after.back().origin == systemdb::ConductorOrigin::Own);
            CHECK(added.ok() && after.back().id == added.value());
        }

        // Namnkrockar: samma namn med andra versaler, och ett namnbyte till ett befintligt namn.
        systemdb::Conductor clash = own;
        clash.name = "593 mm² feal curlew";
        auto clashAdd = sys.saveConductor(clash);
        CHECK(!clashAdd.ok() && clashAdd.error().code == core::ErrorCode::AlreadyExists);
        systemdb::Conductor rename = *morkulla;
        rename.name = "593 mm² FeAl CURLEW";
        auto clashRename = sys.saveConductor(rename);
        CHECK(!clashRename.ok() && clashRename.error().code == core::ErrorCode::AlreadyExists);

        // Ogiltiga värden och en rad som saknas.
        systemdb::Conductor bad = own;
        bad.name = "Ogiltig";
        bad.diameterMm = 0;
        auto badSave = sys.saveConductor(bad);
        CHECK(!badSave.ok() && badSave.error().code == core::ErrorCode::Invalid);
        bad.diameterMm = 10;
        bad.name = "   ";
        auto emptyName = sys.saveConductor(bad);
        CHECK(!emptyName.ok() && emptyName.error().code == core::ErrorCode::Invalid);
        systemdb::Conductor ghost = own;
        ghost.id = 99999;
        ghost.name = "Spöke";
        auto ghostSave = sys.saveConductor(ghost);
        CHECK(!ghostSave.ok() && ghostSave.error().code == core::ErrorCode::NotFound);
    }

    // Ändringen finns kvar efter ny öppning.
    auto again = systemdb::SystemDb::open(path);
    CHECK(again.ok());
    if (!again.ok())
        return;
    systemdb::SystemDb& sys = again.value();
    auto reread = sys.conductor(curlewId);
    CHECK(reread.ok() && reread.value() && near(reread.value()->diameterMm, 32));

    // Två linor byter namn med varandra i en transaktion. Båda blir SeedModified.
    auto all = readAll(sys);
    const auto* a = findByName(all, "593 mm² FeAl CURLEW");
    const auto* b = findByName(all, "593 mm² FeAl MORKULLA");
    CHECK(a && b);
    if (!a || !b)
        return;
    systemdb::Conductor ca = *a;
    systemdb::Conductor cb = *b;
    std::swap(ca.name, cb.name);
    CHECK(sys.saveConductors({ca, cb}, {}).ok());
    auto swappedA = sys.conductor(ca.id);
    auto swappedB = sys.conductor(cb.id);
    CHECK(swappedA.ok() && swappedA.value() && swappedA.value()->name == "593 mm² FeAl MORKULLA");
    CHECK(swappedB.ok() && swappedB.value() && swappedB.value()->name == "593 mm² FeAl CURLEW");
    CHECK(swappedA.ok() && swappedA.value() &&
          swappedA.value()->origin == systemdb::ConductorOrigin::SeedModified);
    CHECK(swappedB.ok() && swappedB.value() &&
          swappedB.value()->origin == systemdb::ConductorOrigin::SeedModified);

    // En giltig och en ogiltig uppdatering: Invalid, och den giltiga sparas inte.
    all = readAll(sys);
    const auto* falcon = findByName(all, "910 mm² FeAl FALCON");
    const auto* orre = findByName(all, "910 mm² FeAl ORRE");
    CHECK(falcon && orre);
    if (!falcon || !orre)
        return;
    systemdb::Conductor goodFalcon = *falcon;
    goodFalcon.diameterMm = 40;
    systemdb::Conductor badOrre = *orre;
    badOrre.diameterMm = 0;
    auto invalid = sys.saveConductors({goodFalcon, badOrre}, {});
    CHECK(!invalid.ok() && invalid.error().code == core::ErrorCode::Invalid);
    auto falconAfter = sys.conductor(falcon->id);
    CHECK(falconAfter.ok() && falconAfter.value() && near(falconAfter.value()->diameterMm, 39.3));

    // En namnkrock efter en lyckad skrivning rullar tillbaka hela sparningen.
    systemdb::Conductor clashOrre = *orre;
    clashOrre.name = "910 mm² FeAl FALCON";
    auto clashed = sys.saveConductors({goodFalcon, clashOrre}, {});
    CHECK(!clashed.ok() && clashed.error().code == core::ErrorCode::AlreadyExists);
    falconAfter = sys.conductor(falcon->id);
    CHECK(falconAfter.ok() && falconAfter.value() && near(falconAfter.value()->diameterMm, 39.3));
    auto orreAfter = sys.conductor(orre->id);
    CHECK(orreAfter.ok() && orreAfter.value() && orreAfter.value()->name == "910 mm² FeAl ORRE");
}

// Borttagning och återställning: borttagna startdatalinor, egna linor och id som behålls.
void testConductorRemoveRestore(const std::string& dir)
{
    auto opened = systemdb::SystemDb::open(join(dir, "restore.db"));
    CHECK(opened.ok());
    if (!opened.ok())
        return;
    systemdb::SystemDb& sys = opened.value();
    auto all = readAll(sys);
    const auto* bluejay = findByName(all, "Bluejay");
    const auto* curlew = findByName(all, "593 mm² FeAl CURLEW");
    CHECK(bluejay && curlew);
    if (!bluejay || !curlew)
        return;
    const std::int64_t curlewId = curlew->id;
    systemdb::Conductor own = *curlew;
    systemdb::Conductor changed = *curlew;

    // Bluejay försvinner, och en egen lina går att ta bort. Ett id som saknas är inget fel.
    CHECK(sys.removeConductor(bluejay->id).ok());
    CHECK(findByName(readAll(sys), "Bluejay") == nullptr);
    own.id = 0;
    own.name = "Egen lina";
    auto ownAdd = sys.saveConductor(own);
    CHECK(ownAdd.ok());
    if (ownAdd.ok()) {
        CHECK(sys.removeConductor(ownAdd.value()).ok());
        CHECK(findByName(readAll(sys), "Egen lina") == nullptr);
    }
    CHECK(sys.removeConductor(99999).ok());

    // Med CURLEW ändrad, Bluejay borta och en egen lina ger återställningen startdatan tillbaka.
    changed.diameterMm = 32;
    CHECK(sys.saveConductor(changed).ok());
    CHECK(sys.saveConductor(own).ok());
    CHECK(sys.restoreSeedConductors().ok());
    all = readAll(sys);
    CHECK(all.size() == 130);
    const auto* restored = findByName(all, "593 mm² FeAl CURLEW");
    CHECK(restored != nullptr);
    if (restored) {
        CHECK(near(restored->diameterMm, 31.7));
        CHECK(restored->origin == systemdb::ConductorOrigin::Seed);
        CHECK(restored->id == curlewId);
    }
    CHECK(findByName(all, "Bluejay") != nullptr);
    CHECK(findByName(all, "Egen lina") != nullptr);

    // En egen lina med ett startdatanamn stoppar återställningen, och inget ändras.
    all = readAll(sys);
    const auto* blue = findByName(all, "Bluejay");
    const auto* curlew2 = findByName(all, "593 mm² FeAl CURLEW");
    CHECK(blue && curlew2);
    if (!blue || !curlew2)
        return;
    systemdb::Conductor changed2 = *curlew2;
    changed2.diameterMm = 32;
    CHECK(sys.saveConductor(changed2).ok());
    CHECK(sys.removeConductor(blue->id).ok());
    systemdb::Conductor lower = changed2;
    lower.id = 0;
    lower.name = "bluejay";
    CHECK(sys.saveConductor(lower).ok());
    auto blocked = sys.restoreSeedConductors();
    CHECK(!blocked.ok() && blocked.error().code == core::ErrorCode::AlreadyExists);
    all = readAll(sys);
    CHECK(findByName(all, "Bluejay") == nullptr);
    CHECK(findByName(all, "bluejay") != nullptr);
    const auto* stillChanged = findByName(all, "593 mm² FeAl CURLEW");
    CHECK(stillChanged && near(stillChanged->diameterMm, 32));
}

// Reglerna utan databas: framräknad krypningstemperatur, namnjämförelse och avvikelse.
void testConductorRules()
{
    const auto derived = systemdb::derivedCreepTempC(0.4, 19);
    CHECK(derived && std::fabs(*derived - 21.0526) < 1e-4);
    const auto zero = systemdb::derivedCreepTempC(0, 0);
    CHECK(zero && near(*zero, 0));
    CHECK(!systemdb::derivedCreepTempC(0.4, 0));

    CHECK(systemdb::sameConductorName("ABC å", "abc å"));
    CHECK(!systemdb::sameConductorName("Å", "å"));

    systemdb::Conductor c;
    c.name = "Prov";
    c.areaMm2 = 100;
    c.diameterMm = 10;
    c.eInitialMpa = 50000;
    c.ePermanentMpa = 60000;
    c.eInitialLimitMpa = 100;
    c.alphaE6PerC = 19;
    c.creepStrainPermille = 0.4;
    c.creepTempC = 21.05;
    c.weightNPerM = 3;
    CHECK(systemdb::validateConductor(c).empty());
    CHECK(!systemdb::creepTempDeviates(c));
    c.creepTempC = 21.0526 + 0.6;
    CHECK(systemdb::creepTempDeviates(c));
    c.creepTempC = 21.05;
    c.alphaE6PerC = -1;
    c.breakingLoadKn = 0;
    const auto issues = systemdb::validateConductor(c);
    CHECK(issues.size() == 1 && issues.front().field == systemdb::ConductorField::BreakingLoad);
}

// En databas med det gamla schemat (utan linregistret) flyttas undan som skadad och ersätts
// av en ny med startdatan. Läget går tillbaka till System.
void testOldSchemaMovedAside(const std::string& dir)
{
    const std::string oldDir = join(dir, "gammal");
    CHECK(platform::ensureDirectory(oldDir).ok());
    const std::string path = join(oldDir, "system.db");
    {
        db::OpenOptions o;
        o.mode = db::OpenMode::CreateNew;
        auto old = db::Database::open(path, o);
        CHECK(old.ok());
        if (!old.ok())
            return;
        CHECK(old.value().exec(
                  "CREATE TABLE setting (key TEXT PRIMARY KEY NOT NULL, value TEXT NOT NULL);"
                  "CREATE TABLE recent_project (path_key TEXT PRIMARY KEY NOT NULL, path TEXT NOT NULL, "
                  "name TEXT NOT NULL, last_opened TEXT NOT NULL);"
                  "INSERT INTO setting(key, value) VALUES('theme_mode', 'dark');"
                  "PRAGMA application_id = " + std::to_string(systemdb::kApplicationId) + ";"
                  "PRAGMA user_version = 1;").ok());
    }
    auto sys = systemdb::SystemDb::open(path);
    CHECK(sys.ok());
    if (sys.ok()) {
        CHECK(sys.value().themeMode() == systemdb::ThemeMode::System);
        auto list = sys.value().conductors();
        CHECK(list.ok() && list.value().size() == 129);
    }
    int moved = 0;
    for (const std::string& name : listNames(oldDir))
        if (name.rfind("system-trasig-", 0) == 0)
            ++moved;
    CHECK(moved == 1);
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
    testConductorSeed(dir);
    testConductorEdit(dir);
    testConductorRemoveRestore(dir);
    testConductorRules();
    testOldSchemaMovedAside(dir);

    fs::remove_all(platform::pathFromUtf8(dir), ec);
    if (g_failures == 0)
        std::cout << "systemdb: alla kontroller lyckades" << std::endl;
    else
        std::cerr << "systemdb: " << g_failures << " kontroller misslyckades" << std::endl;
    return g_failures == 0 ? 0 : 1;
}
