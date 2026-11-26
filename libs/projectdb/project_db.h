#pragma once
#include <chrono>
#include <cstdint>
#include <optional>
#include <stop_token>
#include <string>
#include <string_view>

#include "core/result.h"
#include "db/database.h"
#include "platform/files.h"
#include "platform/lock_file.h"

namespace kalkyl::projectdb {

inline constexpr std::string_view kExtension     = ".kalkyl";
inline constexpr std::uint32_t    kApplicationId = 0x4B4C4B50;   // "KLKP"
inline constexpr int              kSchemaVersion = 6;  // antal steg: 1 (v0.1.0) + dev_migration.h

// Projektets uppgifter ur tabellen project.
struct ProjectInfo { std::string name; std::string createdUtc; std::string createdWith; };
enum class Access { ReadWrite, ReadOnly };

// Inställningar för create/open. Båda körs i en arbetstråd, aldrig på UI-tråden.
struct OpenOptions {
    Access access = Access::ReadWrite;
    std::string workDir;                              // arbetskopiornas lokala mapp (krävs)
    std::chrono::milliseconds probeTimeout{3000};
    platform::Progress progress;                      // anropas i arbetstråden; får vara tom
    std::stop_token stop;
};

// Arbetskopiornas förvalda mapp: arbete i platform::kalkylDataDir().
std::string defaultWorkDir();
// Lägger till .kalkyl om filnamnet saknar ändelse.
std::string withExtension(std::string path);
// Tar bort filer i workDir som inte är öppna i någon process. Fel ignoreras.
void cleanWorkDir(const std::string& workDir);

// En ögonblicksbild att publicera. Ett rent värde som är säkert att skicka till en arbetstråd.
struct Snapshot {
    std::string localFile;       // ögonblicksbilden i workDir
    std::string target;          // projektfilen (originalet)
    platform::FileStamp base;    // originalets stämpel när det öppnades eller publicerades senast
    bool replace = true;         // false vid create: målet får inte finnas
    std::uint64_t revision = 0;  // arbetskopians revision när bilden togs
};

// Ett öppet projekt: originalet (på share eller lokalt), den lokala arbetskopian och låset.
// SQLite öppnar aldrig originalet. Allt arbete sker mot arbetskopian, och originalet ändras
// bara genom att en hel temporär fil byter namn med det.
class ProjectDb {
public:
    // Kontroll → lås → lokal init (schema + projektrad) → publicering med moveNew.
    // Fel: Unreachable, NotFound (mappen), AlreadyExists, Locked, Io, Cancelled.
    static core::Result<ProjectDb> create(const std::string& path, const std::string& name,
                                          const OpenOptions& options);
    // Kontroll → lås (vid ReadWrite) → stämpel → kopiering in → migrate (utan init) → quick_check
    // → läs projektraden. ReadOnly tar inget lås men läser vem som håller det (lockedBy).
    // Fel: NotFound, Unreachable, AccessDenied, Locked, NotADatabase, WrongApplication,
    // TooNew, Io, Cancelled.
    static core::Result<ProjectDb> open(const std::string& path, const OpenOptions& options);

    ProjectDb(ProjectDb&&) noexcept;
    ProjectDb& operator=(ProjectDb&&) noexcept;
    ~ProjectDb();                                   // = close(CloseMode::Background)

    const std::string& path() const;                // originalet, absolut
    const ProjectInfo& info() const;
    Access access() const;
    const std::optional<platform::LockInfo>& lockedBy() const;
    bool created() const;                           // skapat med create, inte öppnat
    db::Database& local();                          // arbetskopian, för all läsning och skrivning

    // Ändringsräknaren. Varje skrivning i arbetskopian (project_data.h) räknar upp revision();
    // isModified() är sann när revisionen skiljer sig från den senast publicerade bilden.
    std::uint64_t revision() const;
    bool isModified() const;
    // Anropas av dataskrivningarna efter en lyckad transaktion.
    void markModified();

    // Sparning i tre steg: bilden tas på UI-tråden, publiceras i en arbetstråd och bekräftas på UI-tråden.
    core::Result<Snapshot> snapshot();              // UI-tråden: VACUUM INTO lokalt; AccessDenied om ReadOnly
    static core::Result<platform::FileStamp> publish(const Snapshot& s, std::stop_token stop);  // arbetstråd
    // UI-tråden: ny bas och sparad revision.
    void published(const Snapshot& s, const platform::FileStamp& newBase);

    enum class CloseMode { Wait, Background };
    // Stänger anslutningen, tar bort arbetskopian och släpper låset (Background: frikopplat).
    void close(CloseMode mode = CloseMode::Wait);
private:
    ProjectDb() = default;
    std::string path_, localPath_, workDir_;
    ProjectInfo info_;
    Access access_ = Access::ReadWrite;
    std::optional<platform::LockInfo> lockedBy_;
    std::optional<platform::LockFile> lock_;
    platform::FileStamp base_;
    std::uint64_t revision_ = 0;
    std::uint64_t savedRevision_ = 0;
    bool created_ = false;
    db::Database local_;          // tomt efter en flytt eller close
};

} // namespace kalkyl::projectdb
