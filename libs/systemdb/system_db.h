#pragma once
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include "core/result.h"
#include "db/database.h"
#include "conductor/conductor.h"

namespace kalkyl::systemdb {

// Utseendeläget. Speglar glaze::ThemeMode utan att bero på glaze.
enum class ThemeMode { System, Light, Dark };

// Filtret i Stolptabell efter: Förändrade ±2 (Changed) eller Alla (All). Speglar poleview::Filter
// utan att bero på poleView.
enum class AfterFilter { Changed, All };

// En rad i listan över senaste projekt.
struct RecentProject { std::string path; std::string name; std::string lastOpenedUtc; };

inline constexpr int           kMaxRecent     = 10;
inline constexpr std::uint32_t kApplicationId = 0x4B4C4B53;   // "KLKS"

// Automatisk sparning i minuter: 0 = av. Bara dessa värden är tillåtna (planen 3.7).
inline constexpr int kAutosaveChoices[] = {0, 1, 5, 10, 15, 30};
inline constexpr int kDefaultAutosaveMinutes = 5;

// En rubrik som användaren valt själv för ett fält vid importen av en stolptabell: fältet (0-7,
// poletable-kolumnen) och den normaliserade rubriken (poleimport::SourceColumn::key).
struct LearnedHeader {
    int field = 0;
    std::string key;
    bool operator==(const LearnedHeader&) const = default;
};

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
    // Automatisk sparning i minuter, ett av kAutosaveChoices. Förvalet 5 vid fel, saknat
    // eller okänt värde. Nyckeln autosave_minutes i tabellen setting.
    int autosaveMinutes();
    // Fel: Invalid (inte ett av kAutosaveChoices), Sql.
    core::Result<void> setAutosaveMinutes(int minutes);

    // Mappen där Importera stolptabell… öppnar filväljaren, "" om inget är sparat. Nyckeln last_import_folder.
    std::string lastImportFolder();
    core::Result<void> setLastImportFolder(const std::string& folder);
    // Filtret i Stolptabell efter. Changed vid fel, saknat eller okänt värde. Nyckeln pole_after_filter.
    AfterFilter afterFilter();
    // Sparar filtret som text: changed eller all.
    core::Result<void> setAfterFilter(AfterFilter filter);
    // De inlärda rubrikerna, senast först. Tom lista vid fel eller när inget är sparat.
    // Nyckeln pole_import_learned: en rad per rubrik, fältet och rubriken skilda med en tabb.
    std::vector<LearnedHeader> learnedHeaders();
    // Lägger rubriken först för fältet. Finns den redan flyttas den först, och fler än tio per fält
    // tas bort från slutet. Fel: Invalid (fältet utanför 0-7, eller en tom rubrik eller en rubrik med
    // tabb eller radbrytning), Sql.
    core::Result<void> learnHeader(int field, const std::string& key);

    core::Result<std::vector<RecentProject>> recentProjects();   // senast först, högst kMaxRecent
    core::Result<void> touchRecentProject(const std::string& path, const std::string& name);
    core::Result<void> removeRecentProject(const std::string& path);   // via pathKey

    // Linregistret i registrets ordning (sort_order, id).
    core::Result<std::vector<conductor::Conductor>> conductors();
    // En lina. Tomt svar om id saknas.
    core::Result<std::optional<conductor::Conductor>> conductor(std::int64_t id);
    // Lägger till (id 0) eller uppdaterar och svarar med id. Namn och typ trimmas.
    // Fel: Invalid, AlreadyExists (namnet finns, NOCASE), NotFound (id saknas), Sql.
    core::Result<std::int64_t> saveConductor(const conductor::Conductor& c);
    // Tar bort en lina. En startdatalina skrivs i conductor_seed_removed. Saknat id är inget fel.
    core::Result<void> removeConductor(std::int64_t id);
    // Fönstrets sparning i EN transaktion: borttagningar, sedan uppdateringar och nya linor.
    // Två linor får byta namn med varandra. Felen som saveConductor; vid fel sparas inget.
    core::Result<void> saveConductors(const std::vector<conductor::Conductor>& upserts,
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
