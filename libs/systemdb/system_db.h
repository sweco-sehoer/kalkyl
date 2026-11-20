#pragma once
#include <cstdint>
#include <string>
#include <vector>

#include "core/result.h"
#include "db/database.h"

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
private:
    SystemDb(db::Database database, std::string fallbackReason);
    std::string setting(const char* key);
    core::Result<void> setSetting(const char* key, const std::string& value);

    db::Database db_;
    std::string  fallbackReason_;
};

} // namespace kalkyl::systemdb
