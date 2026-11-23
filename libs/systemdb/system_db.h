#pragma once
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include "core/result.h"
#include "db/database.h"
#include "systemdb/conductor.h"

namespace kalkyl::systemdb {

// Utseendeläget. Speglar glaze::ThemeMode utan att bero på glaze.
enum class ThemeMode { System, Light, Dark };

// En rad i listan över senaste projekt.
struct RecentProject { std::string path; std::string name; std::string lastOpenedUtc; };

inline constexpr int           kMaxRecent     = 10;
inline constexpr std::uint32_t kApplicationId = 0x4B4C4B53;   // "KLKS"

// Systemdatabasens förvalda plats: system.db i platform::kalkylDataDir().
std::string defaultPath();

// Systemdatabasen: alltid lokal och synkron på UI-tråden.
// Lagrar utseendeläge, senaste mapp och de senaste projekten. Inställningar som inte går
// att läsa ger förval i stället för fel, och ändringar returnerar Result.
class SystemDb {
public:
    // Skapar mappen och filen och migrerar. En skadad fil eller fel id flyttas undan
    // (system-trasig-<tid>.db) och ersätts. TooNew och åtkomstfel returneras.
    static core::Result<SystemDb> open(const std::string& utf8Path);
    // Reserv i minnet: allt fungerar men inget sparas. reason visas i statusraden.
    static core::Result<SystemDb> openInMemory(std::string reason);
    SystemDb(SystemDb&&) noexcept; SystemDb& operator=(SystemDb&&) noexcept; ~SystemDb();

    bool isPersistent() const;
    const std::string& fallbackReason() const;   // "" när databasen är en fil

    ThemeMode themeMode();                        // System vid fel eller okänt värde
    core::Result<void> setThemeMode(ThemeMode mode);
    std::string lastFolder();                     // "" om inget är sparat
    core::Result<void> setLastFolder(const std::string& folder);

    core::Result<std::vector<RecentProject>> recentProjects();   // senast först, högst kMaxRecent
    core::Result<void> touchRecentProject(const std::string& path, const std::string& name);
    core::Result<void> removeRecentProject(const std::string& path);   // via pathKey

    // Linregistret i registrets ordning (sort_order, id).
    core::Result<std::vector<Conductor>> conductors();
    // En lina. Tomt svar om id saknas.
    core::Result<std::optional<Conductor>> conductor(std::int64_t id);
    // Lägger till (id 0) eller uppdaterar och svarar med id. Namn och typ trimmas.
    // Fel: Invalid, AlreadyExists (namnet finns, NOCASE), NotFound (id saknas), Sql.
    core::Result<std::int64_t> saveConductor(const Conductor& c);
    // Tar bort en lina. En startdatalina skrivs i conductor_seed_removed. Saknat id är inget fel.
    core::Result<void> removeConductor(std::int64_t id);
    // Fönstrets sparning i EN transaktion: borttagningar, sedan uppdateringar och nya linor.
    // Två linor får byta namn med varandra. Felen som saveConductor; vid fel sparas inget.
    core::Result<void> saveConductors(const std::vector<Conductor>& upserts,
                                      const std::vector<std::int64_t>& removals);
    // Startdatan tillbaka enligt planen 2.7. Egna linor behålls, id på startdatarader behålls.
    // Fel: AlreadyExists (en egen lina har ett namn ur startdatan), Sql.
    core::Result<void> restoreSeedConductors();
private:
    SystemDb(db::Database database, std::string fallbackReason);
    std::string setting(const char* key);
    core::Result<void> setSetting(const char* key, const std::string& value);

    db::Database db_;
    std::string  fallbackReason_;
};

} // namespace kalkyl::systemdb
