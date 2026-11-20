#include "platform/lock_file.h"

#include <windows.h>

#include <thread>
#include <utility>

#include "platform/files.h"
#include "platform/system.h"
#include "platform/text.h"

namespace kalkyl::platform {

namespace {

// Låsfilens innehåll: en rad per nyckel i formen nyckel=värde. Nya nycklar kan läggas till
// utan att äldre läsare går sönder, eftersom okända nycklar hoppas över.
std::string serialize(const LockInfo& info)
{
    return "user=" + info.user + "\n" + "computer=" + info.computer + "\n" + "pid=" + info.pid + "\n" +
           "opened=" + info.openedUtc + "\n" + "app=" + info.app + "\n";
}

// Läser nyckel=värde-raderna till en LockInfo. Okända nycklar och rader utan = hoppas över.
LockInfo parse(const std::string& text)
{
    LockInfo info;
    size_t start = 0;
    while (start < text.size()) {
        size_t end = text.find('\n', start);
        if (end == std::string::npos)
            end = text.size();
        std::string line = text.substr(start, end - start);
        start = end + 1;
        if (!line.empty() && line.back() == '\r')
            line.pop_back();
        const size_t eq = line.find('=');
        if (eq == std::string::npos)
            continue;
        const std::string key = line.substr(0, eq);
        const std::string value = line.substr(eq + 1);
        if (key == "user")
            info.user = value;
        else if (key == "computer")
            info.computer = value;
        else if (key == "pid")
            info.pid = value;
        else if (key == "opened")
            info.openedUtc = value;
        else if (key == "app")
            info.app = value;
    }
    return info;
}

// Ett fel för låsfilen med svensk text och sökvägen.
core::Error lockError(DWORD code, const std::string& what, const std::string& path)
{
    return win32Error(code, what + ":" + "\n" + path);
}

} // namespace

// Skapar låsfilen med CREATE_NEW så att bara en kan lyckas. FILE_SHARE_READ ger andra
// rätt att läsa men inte ta bort den. FILE_FLAG_DELETE_ON_CLOSE gör att filen försvinner
// när handtaget stängs, också när processen dör eller avslutas innan släppet hunnit klart.
// Handtaget hålls öppet tills låset släpps.
core::Result<LockFile> LockFile::create(const std::string& path, const LockInfo& info)
{
    HANDLE h = CreateFileW(toWide(path).c_str(), GENERIC_WRITE | DELETE, FILE_SHARE_READ, nullptr,
                           CREATE_NEW, FILE_ATTRIBUTE_HIDDEN | FILE_FLAG_DELETE_ON_CLOSE, nullptr);
    if (h == INVALID_HANDLE_VALUE)
        return lockError(GetLastError(), "Det går inte att skapa låsfilen", path);
    const std::string body = serialize(info);
    DWORD written = 0;
    const BOOL ok = WriteFile(h, body.data(), static_cast<DWORD>(body.size()), &written, nullptr) &&
                    FlushFileBuffers(h);
    if (!ok) {
        const DWORD code = GetLastError();
        CloseHandle(h);
        DeleteFileW(toWide(path).c_str());
        return lockError(code, "Det går inte att skriva låsfilen", path);
    }
    LockFile lock;
    lock.handle_ = h;
    lock.path_ = path;
    return lock;
}

// Försöker ta bort en låsfil. Går det har ingen hållit den, och låset var övergivet.
// Ett delningsfel betyder att ägaren lever, vilket ger Locked. Nekad åtkomst ger AccessDenied:
// filen kan vara kvarlämnad men får inte tas bort av den här användaren.
core::Result<void> LockFile::removeStale(const std::string& path)
{
    if (DeleteFileW(toWide(path).c_str()))
        return {};
    const DWORD code = GetLastError();
    if (code == ERROR_FILE_NOT_FOUND || code == ERROR_PATH_NOT_FOUND)
        return {};
    if (code == ERROR_SHARING_VIOLATION || code == ERROR_LOCK_VIOLATION) {
        core::Error e = lockError(code, "Låsfilen hålls av någon annan", path);
        e.code = core::ErrorCode::Locked;
        return e;
    }
    if (code == ERROR_ACCESS_DENIED)
        return lockError(code, "Du har inte rätt att ta bort låsfilen", path);
    return lockError(code, "Det går inte att ta bort låsfilen", path);
}

// Läser vem som håller låset. Öppnar med full delning, så att det går att läsa
// medan ägaren håller filen öppen.
core::Result<LockInfo> LockFile::read(const std::string& path)
{
    HANDLE h = CreateFileW(toWide(path).c_str(), GENERIC_READ,
                           FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr, OPEN_EXISTING,
                           FILE_ATTRIBUTE_NORMAL, nullptr);
    if (h == INVALID_HANDLE_VALUE)
        return lockError(GetLastError(), "Det går inte att läsa låsfilen", path);
    std::string body;
    char buf[1024];
    DWORD got = 0;
    while (body.size() < 65536 && ReadFile(h, buf, sizeof buf, &got, nullptr) && got > 0)
        body.append(buf, got);
    CloseHandle(h);
    return parse(body);
}

// Låsinformation för den här processen: användare, dator, pid och nuvarande UTC-tid.
LockInfo LockFile::currentProcess(std::string app)
{
    LockInfo info;
    info.user = userName();
    info.computer = computerName();
    info.pid = std::to_string(GetCurrentProcessId());
    info.openedUtc = nowUtcIso();
    info.app = std::move(app);
    return info;
}

// Flytten tar över handtaget och sökvägen, och lämnar källan tom (släpper ingenting).
LockFile::LockFile(LockFile&& other) noexcept
    : handle_(std::exchange(other.handle_, nullptr)), path_(std::move(other.path_))
{
    other.path_.clear();
}

// Flytttilldelning: det egna låset släpps först, därefter tas den andras över.
LockFile& LockFile::operator=(LockFile&& other) noexcept
{
    if (this != &other) {
        release();
        handle_ = std::exchange(other.handle_, nullptr);
        path_ = std::move(other.path_);
        other.path_.clear();
    }
    return *this;
}

// Ett lås som går ur räckvidd släpps.
LockFile::~LockFile()
{
    release();
}

// Stänger handtaget, vilket tar bort låsfilen (FILE_FLAG_DELETE_ON_CLOSE). DeleteFileW efteråt
// är en reserv om servern inte tagit bort den, med några försök ifall någon läser filen just då.
// Ett tomt lås gör ingenting.
void LockFile::release()
{
    if (!handle_)
        return;
    CloseHandle(static_cast<HANDLE>(handle_));
    handle_ = nullptr;
    const std::wstring wide = toWide(path_);
    for (int attempt = 0; attempt < 5; ++attempt) {
        if (DeleteFileW(wide.c_str()) || GetLastError() == ERROR_FILE_NOT_FOUND)
            break;
        Sleep(50);
    }
    path_.clear();
}

// Samma sak som release men i en frikopplad tråd, så att ett långsamt nätverk inte
// håller upp anroparen. Tråden rör inget i glaze. Dör processen först stänger OS:et handtaget.
void LockFile::releaseInBackground()
{
    if (!handle_)
        return;
    std::thread([lock = std::move(*this)]() mutable { lock.release(); }).detach();
}

} // namespace kalkyl::platform
