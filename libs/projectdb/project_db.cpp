#include "projectdb/project_db.h"

#include <process.h>

#include <atomic>
#include <filesystem>
#include <system_error>
#include <thread>
#include <utility>
#include <vector>

#include "db/migrate.h"
#include "platform/system.h"
#include "projectdb/dev_migration.h"
#include "platform/text.h"
#include "version/version.h"

namespace kalkyl::projectdb {

namespace {

using core::Error;
using core::ErrorCode;
using core::Result;

// Radbrytning i felmeddelandena.
const std::string nl = "\x0a";

// Schema version 1 (v0.1.0): en rad med projektets uppgifter. Steget ändras aldrig.
// Tabellen är inte STRICT, så att äldre SQLite-verktyg också kan läsa filen.
constexpr std::string_view kSchemaV1 = R"sql(
CREATE TABLE project (
    id           INTEGER PRIMARY KEY CHECK (id = 1),
    name         TEXT NOT NULL,
    created_utc  TEXT NOT NULL,
    created_with TEXT NOT NULL
);
)sql";

// Tabellerna som schemat ska ha efter alla steg. Listan hålls lika med stegen.
constexpr std::string_view kExpectedTablesSql =
    "('project', 'project_conductor', 'doc_value', 'normal_span', 'normal_span_pole', 'doc_normal_span', "
    "'line_calc', 'line_calc_case', 'doc_line_calc', 'pole_before', 'pole_after', 'auto_mode', 'auto_calc', "
    "'pole_import', 'pole_import_column', 'pole_import_row', 'pole_import_cell')";
constexpr int kExpectedTableCount = 17;

// Schemat för projektfilen: steg 1 och utvecklingsmigreringens steg (dev_migration.h).
// Bara create får initiera en tom fil.
db::Schema schema(bool allowInitialize)
{
    db::Schema s;
    s.applicationId = kApplicationId;
    s.steps = {kSchemaV1, dev::kStep2, dev::kStep3, dev::kStep4, dev::kStep5, dev::kStep6, dev::kStep7};
    s.allowInitialize = allowInitialize;
    return s;
}

// Fogar ihop en mapp och ett filnamn med std::filesystem, så att UTF-8 och avgränsare blir rätt.
std::string joinPath(const std::string& dir, const std::string& name)
{
    return platform::pathToUtf8(platform::pathFromUtf8(dir) / platform::pathFromUtf8(name));
}

// Ett namn som är unikt i den här processen: <pid>-<nr>. Används för arbetskopior,
// ögonblicksbilder och temporära filer.
std::string uniqueStem()
{
    static std::atomic<unsigned> counter{0};
    return std::to_string(_getpid()) + "-" + std::to_string(++counter);
}

// Programnamnet och versionen som skrivs i låsfilen och i projektraden.
std::string appText()
{
    return std::string("Kalkyl ") + KALKYL_VERSION;
}

// Skapar ett fel med en kod, en svensk text och en teknisk detalj.
Error makeError(ErrorCode code, std::string message, std::string detail = {})
{
    Error e;
    e.code = code;
    e.message = std::move(message);
    e.detail = std::move(detail);
    return e;
}

// Felet när en stop-begäran avbrutit arbetet.
// Fönstren visar inget meddelande för den koden.
Error cancelledError()
{
    return makeError(ErrorCode::Cancelled, "Avbrutet.");
}

// Felet när projektfilen inte finns.
// Texten innehåller sökvägen så att användaren ser vilken fil det gäller.
Error notFoundError(const std::string& path)
{
    return makeError(ErrorCode::NotFound, "Projektfilen finns inte:" + nl + path);
}

// Felet när mappen på nätverket inte svarar.
// Texten nämner mappen och ber användaren kontrollera anslutningen.
Error unreachableError(const std::string& folder)
{
    return makeError(ErrorCode::Unreachable, "Det går inte att nå mappen:" + nl + folder + nl + nl +
                                                 "Kontrollera nätverksanslutningen.");
}

// Felet när målfilen redan finns vid skapande.
// Ett befintligt projekt skrivs aldrig över.
Error alreadyExistsError(const std::string& path)
{
    return makeError(ErrorCode::AlreadyExists, "Det finns redan en fil med namnet:" + nl + path);
}

// Felet när någon annan håller låset: vem, på vilken dator och sedan när.
// Gick låsfilen inte att läsa blir texten allmän i stället för att visa tomma fält.
Error lockedError(const platform::LockInfo& info)
{
    if (info.user.empty() && info.computer.empty())
        return makeError(ErrorCode::Locked, "Projektet är låst av någon annan.");
    std::string since = platform::localTimeText(info.openedUtc);
    if (since.empty())
        since = info.openedUtc;
    return makeError(ErrorCode::Locked, "Projektet är öppnat av " + info.user + " på " +
                                            info.computer + " sedan " + since + ".");
}

// Byter meddelandet mot planens text för de koder som har en, och behåller den ursprungliga
// texten som detalj. Övriga koder behåller sitt meddelande.
Error relabel(Error e, const std::string& path)
{
    std::string message;
    switch (e.code) {
    case ErrorCode::NotFound:
        message = notFoundError(path).message;
        break;
    case ErrorCode::Unreachable:
        message = unreachableError(platform::parentDir(path)).message;
        break;
    case ErrorCode::AlreadyExists:
        message = alreadyExistsError(path).message;
        break;
    case ErrorCode::WrongApplication:
        message = "Filen är inte ett Kalkylprojekt:" + nl + path;
        break;
    case ErrorCode::NotADatabase:
        message = "Filen är inte ett Kalkylprojekt eller är skadad:" + nl + path;
        break;
    case ErrorCode::TooNew:
        message = "Projektet är skapat med en nyare version av Kalkyl. Uppdatera programmet.";
        break;
    case ErrorCode::Conflict:
        message = "Projektfilen har ändrats av någon annan sedan den öppnades.";
        break;
    default:
        return e;
    }
    if (e.detail.empty())
        e.detail = e.message;
    e.message = std::move(message);
    return e;
}

// Samma dator och samma process: låset är vårt eget, till exempel under ett släpp i bakgrunden.
bool isOwnLock(const platform::LockInfo& held, const platform::LockInfo& mine)
{
    return held.computer == mine.computer && held.pid == mine.pid;
}

// Tar låsfilen. Finns den redan prövas först om den är vår egen (väntan upp till 2 s på att
// ett släpp blir klart) och sedan om den är övergiven (då tas den bort och försöket görs om).
// Lever den hos någon annan, eller får den inte tas bort av oss, blir svaret Locked med vem.
// Saknas skrivrätt i mappen blir svaret AccessDenied, och väljaren erbjuder skrivskyddat läge.
Result<platform::LockFile> acquireLock(const std::string& lockPath, const std::string& folder,
                                       std::stop_token stop)
{
    using clock = std::chrono::steady_clock;
    const platform::LockInfo mine = platform::LockFile::currentProcess(appText());
    const auto deadline = clock::now() + std::chrono::seconds(2);
    int staleRemovals = 0;
    while (true) {
        if (stop.stop_requested())
            return cancelledError();
        auto created = platform::LockFile::create(lockPath, mine);
        if (created)
            return std::move(created).value();
        const Error createError = created.error();
        if (createError.code == ErrorCode::AccessDenied)
            return makeError(ErrorCode::AccessDenied,
                             "Du saknar skrivrättigheter i mappen:" + nl + folder,
                             createError.message + " " + createError.detail);
        if (createError.code != ErrorCode::AlreadyExists)
            return createError;

        auto held = platform::LockFile::read(lockPath);
        if (held && isOwnLock(held.value(), mine) && clock::now() < deadline) {
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
            continue;
        }
        auto stale = platform::LockFile::removeStale(lockPath);
        if (stale && ++staleRemovals <= 5)
            continue;
        if (!stale && stale.error().code == ErrorCode::Locked)
            return lockedError(held ? held.value() : platform::LockInfo{});
        if (!stale && stale.error().code == ErrorCode::AccessDenied) {
            Error e = lockedError(held ? held.value() : platform::LockInfo{});
            e.message += nl + nl + "Låsfilen kan inte tas bort av dig:" + nl + lockPath;
            e.detail = stale.error().detail;
            return e;
        }
        if (!stale)
            return stale.error();
        return makeError(ErrorCode::Io, "Det gick inte att ta låset på projektet:" + nl + lockPath);
    }
}

// Kontrollerar efter migreringen att alla tabeller i schemat finns. En skadad fil eller en fil
// med rätt user_version men utan tabellerna vägras här i stället för vid första läsningen.
Result<void> checkTables(db::Database& local, const std::string& path)
{
    const std::string sql =
        "SELECT count(*) FROM sqlite_master WHERE type = 'table' AND name IN " +
        std::string(kExpectedTablesSql);
    auto stmt = local.prepare(sql);
    if (!stmt)
        return stmt.error();
    auto row = stmt.value().step();
    if (!row)
        return row.error();
    if (!row.value() || stmt.value().integer(0) != kExpectedTableCount)
        return makeError(ErrorCode::NotADatabase,
                         "Projektfilen saknar delar som den här versionen av Kalkyl behöver. "
                         "Filen är skadad:" + nl + path,
                         "saknar tabeller i schemat");
    return {};
}

// Läser projektraden ur arbetskopian. Saknas tabellen eller raden är filen inte ett komplett
// Kalkylprojekt.
Result<ProjectInfo> readInfo(db::Database& local, const std::string& path)
{
    auto stmt = local.prepare("SELECT name, created_utc, created_with FROM project WHERE id = 1");
    if (!stmt)
        return relabel(makeError(ErrorCode::NotADatabase, "", stmt.error().message), path);
    auto row = stmt.value().step();
    if (!row)
        return relabel(row.error(), path);
    if (!row.value())
        return relabel(makeError(ErrorCode::NotADatabase, "", "saknar projektuppgifter"), path);
    ProjectInfo info;
    info.name = stmt.value().text(0);
    info.createdUtc = stmt.value().text(1);
    info.createdWith = stmt.value().text(2);
    return info;
}

} // namespace

// Arbetskopiornas lokala mapp, arbete i Kalkyls datamapp.
// KALKYL_DATA_DIR flyttar den, till exempel i tester.
std::string defaultWorkDir()
{
    return joinPath(platform::kalkylDataDir(), "arbete");
}

// Lägger till .kalkyl när sökvägen inte har någon filändelse alls.
std::string withExtension(std::string path)
{
    if (platform::pathFromUtf8(path).extension().empty())
        path += kExtension;
    return path;
}

// Går igenom arbetsmappen och försöker ta bort varje fil. En fil som en annan Kalkyl-process
// har öppen går inte att ta bort och lämnas kvar. Alla fel ignoreras.
void cleanWorkDir(const std::string& workDir)
{
    std::error_code ec;
    const auto dir = platform::pathFromUtf8(workDir);
    std::vector<std::string> files;
    for (std::filesystem::directory_iterator it(dir, ec), end; !ec && it != end; it.increment(ec)) {
        std::error_code typeEc;
        if (it->is_regular_file(typeEc))
            files.push_back(platform::pathToUtf8(it->path()));
    }
    for (const std::string& file : files)
        (void)platform::removeFile(file);
}

// Flytten tar över allt. Källans arbetskopia och lås nollas, så att källan
// inte tar bort arbetskopian eller släpper låset när den förstörs.
ProjectDb::ProjectDb(ProjectDb&& other) noexcept
    : path_(std::move(other.path_)),
      localPath_(std::move(other.localPath_)),
      workDir_(std::move(other.workDir_)),
      info_(std::move(other.info_)),
      access_(other.access_),
      lockedBy_(std::move(other.lockedBy_)),
      lock_(std::move(other.lock_)),
      base_(other.base_),
      revision_(other.revision_),
      savedRevision_(other.savedRevision_),
      created_(other.created_),
      local_(std::move(other.local_))
{
    other.localPath_.clear();
    other.lock_.reset();
}

// Flyttilldelning: det egna projektet stängs först (låset släpps i bakgrunden),
// därefter tas den andras över på samma sätt som i flyttkonstruktorn.
ProjectDb& ProjectDb::operator=(ProjectDb&& other) noexcept
{
    if (this == &other)
        return *this;
    close(CloseMode::Background);
    path_ = std::move(other.path_);
    localPath_ = std::move(other.localPath_);
    workDir_ = std::move(other.workDir_);
    info_ = std::move(other.info_);
    access_ = other.access_;
    lockedBy_ = std::move(other.lockedBy_);
    lock_ = std::move(other.lock_);
    base_ = other.base_;
    revision_ = other.revision_;
    savedRevision_ = other.savedRevision_;
    created_ = other.created_;
    local_ = std::move(other.local_);
    other.localPath_.clear();
    other.lock_.reset();
    return *this;
}

// Ett projekt som går ur räckvidd stängs. Låset släpps i bakgrunden,
// så att en seg share inte håller upp den som förstör objektet.
ProjectDb::~ProjectDb()
{
    close(CloseMode::Background);
}

// Öppnar ett befintligt projekt. Först kontrolleras filen och låset tas, därefter kopieras
// filen in i ett svep och arbetskopian migreras och kontrolleras. Varje fel efter att låset
// tagits städar bort arbetskopian och släpper låset igen (fail).
Result<ProjectDb> ProjectDb::open(const std::string& path, const OpenOptions& options)
{
    if (options.stop.stop_requested())
        return cancelledError();
    if (options.workDir.empty())
        return makeError(ErrorCode::Io, "Arbetsmappen saknas.", "OpenOptions::workDir är tom");

    const std::string file = platform::absolutePath(path);
    switch (platform::probe(file, options.probeTimeout)) {
    case platform::PathState::Exists:
        break;
    case platform::PathState::Missing:
        return notFoundError(file);
    case platform::PathState::Unreachable:
        return unreachableError(platform::parentDir(file));
    case platform::PathState::NoAccess:
        return makeError(ErrorCode::AccessDenied,
                         "Du saknar behörighet till projektfilen:" + nl + file);
    }

    ProjectDb p;
    p.path_ = file;
    p.workDir_ = options.workDir;
    p.access_ = options.access;
    auto fail = [&p](Error e) -> Error {
        p.close(CloseMode::Wait);
        return e;
    };

    const std::string lockPath = file + ".lock";
    if (options.access == Access::ReadWrite) {
        auto lock = acquireLock(lockPath, platform::parentDir(file), options.stop);
        if (!lock)
            return lock.error();
        p.lock_ = std::move(lock).value();
    } else if (auto held = platform::LockFile::read(lockPath); held) {
        p.lockedBy_ = held.value();
    }

    auto stamp = platform::stamp(file);
    if (!stamp)
        return fail(relabel(stamp.error(), file));
    p.base_ = stamp.value();

    if (auto dir = platform::ensureDirectory(options.workDir); !dir)
        return fail(dir.error());
    p.localPath_ = joinPath(options.workDir, uniqueStem() + ".kalkyl");
    if (auto copied = platform::copyFile(file, p.localPath_, options.progress, options.stop); !copied)
        return fail(relabel(copied.error(), file));
    // Kopian ärver originalets attribut. Ett skrivskyddat original får inte göra arbetskopian skrivskyddad.
    platform::clearReadOnly(p.localPath_);

    db::OpenOptions dbOptions;
    dbOptions.exclusive = true;
    auto local = db::Database::open(p.localPath_, dbOptions);
    if (!local)
        return fail(relabel(local.error(), file));
    p.local_ = std::move(local).value();

    if (options.stop.stop_requested())
        return fail(cancelledError());
    if (auto migrated = db::migrate(p.local_, schema(false)); !migrated)
        return fail(relabel(migrated.error(), file));
    if (auto tables = checkTables(p.local_, file); !tables)
        return fail(tables.error());
    if (auto checked = p.local_.quickCheck(); !checked)
        return fail(relabel(checked.error(), file));

    auto info = readInfo(p.local_, file);
    if (!info)
        return fail(info.error());
    p.info_ = std::move(info).value();
    return p;
}

// Skapar ett nytt projekt. Mappen och målfilen kontrolleras med en gemensam tidsgräns, låset
// tas, projektet initieras i en lokal arbetskopia och publiceras med moveNew, som vägrar
// ersätta en fil som dykt upp under tiden. Fel efter att låset tagits städar som i open.
Result<ProjectDb> ProjectDb::create(const std::string& path, const std::string& name,
                                    const OpenOptions& options)
{
    if (options.stop.stop_requested())
        return cancelledError();
    if (options.workDir.empty())
        return makeError(ErrorCode::Io, "Arbetsmappen saknas.", "OpenOptions::workDir är tom");

    const std::string file = platform::absolutePath(path);
    const std::string folder = platform::parentDir(file);
    const auto states = platform::probeMany({folder, file}, options.probeTimeout);
    switch (states[0]) {
    case platform::PathState::Exists:
        break;
    case platform::PathState::Missing:
        return makeError(ErrorCode::NotFound, "Mappen finns inte:" + nl + folder);
    case platform::PathState::Unreachable:
        return unreachableError(folder);
    case platform::PathState::NoAccess:
        return makeError(ErrorCode::AccessDenied, "Du saknar behörighet till mappen:" + nl + folder);
    }
    if (states[1] == platform::PathState::Unreachable)
        return unreachableError(folder);
    if (states[1] != platform::PathState::Missing)
        return alreadyExistsError(file);

    ProjectDb p;
    p.path_ = file;
    p.workDir_ = options.workDir;
    p.access_ = Access::ReadWrite;
    auto fail = [&p](Error e) -> Error {
        p.close(CloseMode::Wait);
        return e;
    };

    auto lock = acquireLock(file + ".lock", folder, options.stop);
    if (!lock)
        return lock.error();
    p.lock_ = std::move(lock).value();

    if (auto dir = platform::ensureDirectory(options.workDir); !dir)
        return fail(dir.error());
    p.localPath_ = joinPath(options.workDir, uniqueStem() + ".kalkyl");
    db::OpenOptions dbOptions;
    dbOptions.mode = db::OpenMode::CreateNew;
    dbOptions.exclusive = true;
    auto local = db::Database::open(p.localPath_, dbOptions);
    if (!local)
        return fail(relabel(local.error(), file));
    p.local_ = std::move(local).value();

    if (auto migrated = db::migrate(p.local_, schema(true)); !migrated)
        return fail(relabel(migrated.error(), file));
    p.info_ = {name, platform::nowUtcIso(), appText()};
    auto inserted = p.local_.transaction([&]() -> Result<void> {
        auto stmt = p.local_.prepare("INSERT INTO project(id, name, created_utc, created_with) "
                                     "VALUES(1, ?1, ?2, ?3)");
        if (!stmt)
            return stmt.error();
        stmt.value().bind(1, p.info_.name).bind(2, p.info_.createdUtc).bind(3, p.info_.createdWith);
        return stmt.value().run();
    });
    if (!inserted)
        return fail(inserted.error());

    auto snap = p.snapshot();
    if (!snap)
        return fail(snap.error());
    snap.value().replace = false;
    auto published = publish(snap.value(), options.stop);
    if (!published) {
        (void)platform::removeFile(snap.value().localFile);
        return fail(published.error());
    }
    p.published(snap.value(), published.value());
    p.created_ = true;
    return p;
}

// Sant när projektet skapades med create i den här körningen, inte öppnades.
// Huvudfönstret fyller då i Dokumentationens förval.
bool ProjectDb::created() const
{
    return created_;
}

// Originalets absoluta sökväg, på share eller lokalt.
// Det är den som visas för användaren och står i senaste-listan.
const std::string& ProjectDb::path() const
{
    return path_;
}

// Projektets uppgifter ur tabellen project.
// Läses vid öppning och skrivs vid skapande.
const ProjectInfo& ProjectDb::info() const
{
    return info_;
}

// Läs- och skrivläget som projektet öppnades med.
// ReadOnly har inget lås och kan inte sparas.
Access ProjectDb::access() const
{
    return access_;
}

// Vem som höll låset när projektet öppnades skrivskyddat.
// Tomt vid ReadWrite, eller om ingen höll låset.
const std::optional<platform::LockInfo>& ProjectDb::lockedBy() const
{
    return lockedBy_;
}

// Anslutningen till den lokala arbetskopian.
// All läsning och skrivning i projektet går hit, aldrig mot originalet.
db::Database& ProjectDb::local()
{
    return local_;
}

// Tar en konsekvent, kompakt kopia av arbetskopian med VACUUM INTO. Bilden får sin egen fil i
// arbetsmappen och är ett rent värde som kan publiceras i en arbetstråd. Skrivskyddade
// projekt får inte sparas.
Result<Snapshot> ProjectDb::snapshot()
{
    if (access_ == Access::ReadOnly)
        return makeError(ErrorCode::AccessDenied,
                         "Projektet är öppnat skrivskyddat och kan inte sparas.");
    Snapshot s;
    s.localFile = joinPath(workDir_, uniqueStem() + ".snap");
    s.target = path_;
    s.base = base_;
    s.replace = true;
    s.revision = revision_;
    if (auto removed = platform::removeFile(s.localFile); !removed)
        return removed.error();
    if (auto copied = local_.vacuumInto(s.localFile); !copied)
        return copied.error();
    return s;
}

// Publicerar bilden så att originalet aldrig blir halvskrivet: kontrollera stämpeln, kopiera
// till en dold temporär fil bredvid originalet, töm skrivcachen, kontrollera storleken, visa
// filen igen (namnbytet behåller attributen) och byt namn. Vid fel tas den temporära filen bort
// och originalet är orört. Går stämpeln inte att läsa efter ett lyckat byte används den
// temporära filens stämpel, som namnbytet inte ändrar.
Result<platform::FileStamp> ProjectDb::publish(const Snapshot& s, std::stop_token stop)
{
    if (stop.stop_requested())
        return cancelledError();

    const std::string folder = platform::parentDir(s.target);
    const std::string tmp =
        joinPath(folder, "~" + platform::fileName(s.target) + "." + uniqueStem() + ".tmp");

    if (s.replace) {
        auto current = platform::stamp(s.target);
        if (!current)
            return relabel(current.error(), s.target);
        if (!(current.value() == s.base))
            return relabel(makeError(ErrorCode::Conflict, ""), s.target);
    }

    platform::FileStamp tmpStamp;
    auto sent = [&]() -> Result<void> {
        if (auto copied = platform::copyFile(s.localFile, tmp, platform::Progress{}, stop); !copied)
            return copied;
        platform::setHidden(tmp);
        if (auto flushed = platform::flushFile(tmp); !flushed)
            return flushed;
        auto want = platform::stamp(s.localFile);
        if (!want)
            return want.error();
        platform::setHidden(tmp, false);
        auto have = platform::stamp(tmp);
        if (!have)
            return have.error();
        if (have.value().size != want.value().size)
            return makeError(ErrorCode::Io, "Den temporära filen blev inte komplett:" + nl + tmp);
        tmpStamp = have.value();
        return s.replace ? platform::moveReplace(tmp, s.target) : platform::moveNew(tmp, s.target);
    }();
    if (!sent) {
        (void)platform::removeFile(tmp);
        return relabel(sent.error(), s.target);
    }

    (void)platform::removeFile(s.localFile);
    // Projektfilen är sparad. En stämpel som inte går att läsa nu får inte bli ett fel.
    if (auto after = platform::stamp(s.target); after)
        return after.value();
    return tmpStamp;
}

// Efter en lyckad publicering är det nya originalets stämpel basen för nästa konfliktkontroll,
// och bildens revision är den senast sparade. Skrivningar efter bilden lämnar projektet ändrat.
void ProjectDb::published(const Snapshot& s, const platform::FileStamp& newBase)
{
    base_ = newBase;
    savedRevision_ = s.revision;
}

// Ändringsräknaren: räknas upp av varje lyckad skrivning i arbetskopian.
std::uint64_t ProjectDb::revision() const
{
    return revision_;
}

// Sann när arbetskopian har ändringar som inte finns i den senast publicerade bilden.
bool ProjectDb::isModified() const
{
    return revision_ != savedRevision_;
}

// Räknar upp revisionen. Anropas av dataskrivningarna efter en lyckad transaktion.
void ProjectDb::markModified()
{
    ++revision_;
}

// Stänger arbetskopian, tar bort den och släpper låset. Background släpper låset i en
// frikopplad tråd så att en seg share inte låser UI-tråden. Anropet går att upprepa.
void ProjectDb::close(CloseMode mode)
{
    local_.close();
    if (!localPath_.empty()) {
        (void)platform::removeFile(localPath_);
        localPath_.clear();
    }
    if (lock_) {
        if (mode == CloseMode::Background)
            lock_->releaseInBackground();
        else
            lock_->release();
        lock_.reset();
    }
}

} // namespace kalkyl::projectdb
