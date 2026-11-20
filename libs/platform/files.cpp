#include "platform/files.h"

#include <windows.h>

#include <condition_variable>
#include <filesystem>
#include <memory>
#include <mutex>
#include <optional>
#include <thread>

#include "platform/text.h"

namespace kalkyl::platform {

namespace {

// Gemensamt tillstånd för probeMany. Delas med de frikopplade trådarna (shared_ptr),
// så att en tråd som hänger kvar efter tidsgränsen aldrig rör något som är borta.
struct ProbeState {
    std::mutex mutex;
    std::condition_variable cv;
    std::vector<std::optional<PathState>> results;
    size_t pending = 0;
};

// Ett Win32-felkod till ett PathState. Okända fel räknas som att sökvägen inte går att nå.
PathState stateFromError(DWORD code)
{
    if (code == ERROR_FILE_NOT_FOUND || code == ERROR_PATH_NOT_FOUND || code == ERROR_INVALID_NAME ||
        code == ERROR_BAD_PATHNAME)
        return PathState::Missing;
    if (code == ERROR_ACCESS_DENIED)
        return PathState::NoAccess;
    return PathState::Unreachable;
}

// Systemets text för en Win32-felkod, utan radbrytning i slutet. Tom om texten saknas.
std::string systemMessage(DWORD code)
{
    wchar_t* raw = nullptr;
    const DWORD n = FormatMessageW(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM |
                                       FORMAT_MESSAGE_IGNORE_INSERTS,
                                   nullptr, code, 0, reinterpret_cast<LPWSTR>(&raw), 0, nullptr);
    if (n == 0 || !raw)
        return {};
    std::wstring text(raw, n);
    LocalFree(raw);
    while (!text.empty() && (text.back() == L'\r' || text.back() == L'\n' || text.back() == L' '))
        text.pop_back();
    return toUtf8(text);
}

// Ett fel för en sökväg: koden väljer användartexten, och sökvägen läggs till.
// Alla filfunktioner här använder den, så att texterna blir lika överallt.
core::Error pathError(DWORD code, const std::string& path)
{
    const std::string nl = "\n";
    const core::Error classified = win32Error(code, {});
    std::string message;
    if (classified.code == core::ErrorCode::NotFound)
        message = "Filen eller mappen finns inte:" + nl + path;
    else if (classified.code == core::ErrorCode::Unreachable)
        message = "Det går inte att nå:" + nl + path + nl + nl + "Kontrollera nätverksanslutningen.";
    else if (classified.code == core::ErrorCode::AccessDenied)
        message = "Åtkomst nekas:" + nl + path;
    else if (classified.code == core::ErrorCode::AlreadyExists)
        message = "Det finns redan en fil med namnet:" + nl + path;
    else if (classified.code == core::ErrorCode::Busy)
        message = "Filen används av någon annan:" + nl + path;
    else if (classified.code == core::ErrorCode::Cancelled)
        message = "Avbrutet.";
    else
        message = "Det gick inte att komma åt:" + nl + path;
    return win32Error(code, std::move(message));
}

// Förlopp och avbrott för CopyFileExW. Rutinen får en pekare hit som lpData.
struct CopyContext {
    const Progress* progress;
    std::stop_token stop;
};

// CopyFileExW:s förloppsrutin. Rapporterar förloppet och avbryter när stop begärts.
// Avbrottet raderar målfilen, så ingen halv kopia blir kvar.
DWORD CALLBACK copyRoutine(LARGE_INTEGER total, LARGE_INTEGER transferred, LARGE_INTEGER, LARGE_INTEGER,
                           DWORD, DWORD, HANDLE, HANDLE, LPVOID data)
{
    auto* ctx = static_cast<CopyContext*>(data);
    if (ctx->progress && *ctx->progress)
        (*ctx->progress)(static_cast<std::uint64_t>(transferred.QuadPart),
                         static_cast<std::uint64_t>(total.QuadPart));
    return ctx->stop.stop_requested() ? PROGRESS_CANCEL : PROGRESS_CONTINUE;
}

} // namespace

// Ordnar sökvägen till en absolut med avgränsaren för Windows. Ingen I/O mot nätverk görs.
// En tom sträng in ger en tom sträng ut.
std::string absolutePath(std::string_view path)
{
    if (path.empty())
        return {};
    const std::wstring wide = toWide(path);
    if (wide.empty())
        return {};
    const DWORD n = GetFullPathNameW(wide.c_str(), 0, nullptr, nullptr);
    if (n == 0)
        return std::string(path);
    std::wstring out(n, L'\0');
    const DWORD got = GetFullPathNameW(wide.c_str(), n, out.data(), nullptr);
    out.resize(got);
    return toUtf8(out);
}

// Jämförelsenyckeln: absolut sökväg i gemener. CharLowerBuffW hanterar åäö och ÅÄÖ,
// och Windows jämför sökvägar utan hänsyn till skiftläge.
std::string pathKey(std::string_view path)
{
    std::wstring wide = toWide(absolutePath(path));
    if (!wide.empty())
        CharLowerBuffW(wide.data(), static_cast<DWORD>(wide.size()));
    return toUtf8(wide);
}

// Mappen som innehåller sökvägen. Roten på en enhet behåller sitt avslutande snedstreck.
// Ren strängbehandling.
std::string parentDir(std::string_view path)
{
    const size_t pos = path.find_last_of(R"(\/)");
    if (pos == std::string_view::npos)
        return {};
    if (pos == 2 && path[1] == ':')
        return std::string(path.substr(0, 3));
    return std::string(path.substr(0, pos));
}

// Filnamnet efter sista avgränsaren. Ren strängbehandling.
std::string fileName(std::string_view path)
{
    const size_t pos = path.find_last_of(R"(\/)");
    return std::string(pos == std::string_view::npos ? path : path.substr(pos + 1));
}

// Filnamnet utan sista ändelsen. En punkt först i namnet räknas inte som ändelse.
std::string fileStem(std::string_view path)
{
    const std::string name = fileName(path);
    const size_t dot = name.find_last_of('.');
    return (dot == std::string::npos || dot == 0) ? name : name.substr(0, dot);
}

// Frågar efter alla sökvägar parallellt i frikopplade trådar och väntar på dem
// till en gemensam tidsgräns. De som inte svarat i tid blir Unreachable.
std::vector<PathState> probeMany(const std::vector<std::string>& paths, std::chrono::milliseconds timeout)
{
    auto state = std::make_shared<ProbeState>();
    state->results.resize(paths.size());
    state->pending = paths.size();
    for (size_t i = 0; i < paths.size(); ++i) {
        std::thread([state, i, wide = toWide(paths[i])] {
            PathState result = PathState::Exists;
            WIN32_FILE_ATTRIBUTE_DATA data;
            if (wide.empty())
                result = PathState::Missing;
            else if (!GetFileAttributesExW(wide.c_str(), GetFileExInfoStandard, &data))
                result = stateFromError(GetLastError());
            std::lock_guard lock(state->mutex);
            state->results[i] = result;
            --state->pending;
            state->cv.notify_all();
        }).detach();
    }
    std::unique_lock lock(state->mutex);
    state->cv.wait_for(lock, timeout, [&] { return state->pending == 0; });
    std::vector<PathState> out;
    out.reserve(paths.size());
    for (const auto& r : state->results)
        out.push_back(r.value_or(PathState::Unreachable));
    return out;
}

// En enda sökväg: probeMany med ett element.
PathState probe(const std::string& path, std::chrono::milliseconds timeout)
{
    return probeMany({path}, timeout).front();
}

// Storlek och skrivtid för en fil. Används för att upptäcka att originalet ändrats.
core::Result<FileStamp> stamp(const std::string& path)
{
    WIN32_FILE_ATTRIBUTE_DATA data;
    if (!GetFileAttributesExW(toWide(path).c_str(), GetFileExInfoStandard, &data))
        return pathError(GetLastError(), path);
    FileStamp s;
    s.size = (static_cast<std::uint64_t>(data.nFileSizeHigh) << 32) | data.nFileSizeLow;
    s.lastWrite = (static_cast<std::int64_t>(data.ftLastWriteTime.dwHighDateTime) << 32) |
                  data.ftLastWriteTime.dwLowDateTime;
    return s;
}

// Skapar mappen och alla mappar ovanför. En mapp som redan finns är ett lyckat resultat.
// MinGW:s std::filesystem svarar med errno-koder (generic_category), så de översätts
// till motsvarande Win32-kod innan felet klassas. Övriga errno-koder blir Io.
core::Result<void> ensureDirectory(const std::string& path)
{
    std::error_code ec;
    std::filesystem::create_directories(pathFromUtf8(path), ec);
    if (!ec)
        return {};
    DWORD code = ERROR_GEN_FAILURE;
    if (ec.category() == std::system_category())
        code = static_cast<DWORD>(ec.value());
    else if (ec == std::errc::permission_denied)
        code = ERROR_ACCESS_DENIED;
    else if (ec == std::errc::no_such_file_or_directory)
        code = ERROR_PATH_NOT_FOUND;
    return pathError(code, path);
}

// Tar bort en fil. En fil som redan saknas räknas som lyckat. Nekas borttagningen
// för att filen är skrivskyddad tas skrivskyddet bort och ett nytt försök görs.
core::Result<void> removeFile(const std::string& path)
{
    const std::wstring wide = toWide(path);
    if (DeleteFileW(wide.c_str()))
        return {};
    DWORD code = GetLastError();
    if (code == ERROR_ACCESS_DENIED) {
        const DWORD attrs = GetFileAttributesW(wide.c_str());
        if (attrs != INVALID_FILE_ATTRIBUTES && (attrs & FILE_ATTRIBUTE_READONLY) &&
            SetFileAttributesW(wide.c_str(), attrs & ~FILE_ATTRIBUTE_READONLY)) {
            if (DeleteFileW(wide.c_str()))
                return {};
            code = GetLastError();
        }
    }
    if (code == ERROR_FILE_NOT_FOUND || code == ERROR_PATH_NOT_FOUND)
        return {};
    return pathError(code, path);
}

// Kopierar med CopyFileExW i ett svep. Förloppet rapporteras per block,
// och en begärd stop avbryter kopieringen mellan blocken.
core::Result<void> copyFile(const std::string& from, const std::string& to, const Progress& progress,
                            std::stop_token stop)
{
    CopyContext ctx{&progress, std::move(stop)};
    BOOL cancel = FALSE;
    if (CopyFileExW(toWide(from).c_str(), toWide(to).c_str(), copyRoutine, &ctx, &cancel,
                    COPY_FILE_FAIL_IF_EXISTS))
        return {};
    const DWORD code = GetLastError();
    // Finns målet redan gäller felet det, annars källan (saknas, nås inte).
    return pathError(code, code == ERROR_FILE_EXISTS || code == ERROR_ALREADY_EXISTS ? to : from);
}

// Tvingar ut filens data till disken eller servern. Öppnar för skrivning
// eftersom FlushFileBuffers kräver det.
core::Result<void> flushFile(const std::string& path)
{
    HANDLE h = CreateFileW(toWide(path).c_str(), GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE,
                           nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (h == INVALID_HANDLE_VALUE)
        return pathError(GetLastError(), path);
    const BOOL ok = FlushFileBuffers(h);
    const DWORD code = GetLastError();
    CloseHandle(h);
    if (!ok)
        return pathError(code, path);
    return {};
}

// Byter namn utan att ersätta. Finns målet blir felet AlreadyExists.
core::Result<void> moveNew(const std::string& from, const std::string& to)
{
    if (MoveFileExW(toWide(from).c_str(), toWide(to).c_str(), MOVEFILE_WRITE_THROUGH))
        return {};
    return pathError(GetLastError(), to);
}

// Byter namn och ersätter målet atomärt. Ett delningsfel (någon läser filen just då)
// ger 3 försök med 500 ms mellanrum, därefter felet Busy.
core::Result<void> moveReplace(const std::string& from, const std::string& to)
{
    constexpr int kAttempts = 3;
    const std::wstring wFrom = toWide(from);
    const std::wstring wTo = toWide(to);
    DWORD code = 0;
    for (int attempt = 1; attempt <= kAttempts; ++attempt) {
        if (MoveFileExW(wFrom.c_str(), wTo.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH))
            return {};
        code = GetLastError();
        if ((code != ERROR_SHARING_VIOLATION && code != ERROR_LOCK_VIOLATION) || attempt == kAttempts)
            break;
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
    }
    return pathError(code, to);
}

// Döljer en fil, eller visar den igen med hidden = false. Misslyckas det är det inget fel,
// filen behåller bara sitt läge. Övriga attribut lämnas orörda.
void setHidden(const std::string& path, bool hidden)
{
    const std::wstring wide = toWide(path);
    const DWORD attrs = GetFileAttributesW(wide.c_str());
    if (attrs == INVALID_FILE_ATTRIBUTES)
        return;
    const DWORD wanted = hidden ? (attrs | FILE_ATTRIBUTE_HIDDEN) : (attrs & ~FILE_ATTRIBUTE_HIDDEN);
    if (wanted != attrs)
        SetFileAttributesW(wide.c_str(), wanted);
}

// Tar bort attributet Skrivskyddad. CopyFileExW kopierar attributen, så en arbetskopia av
// ett skrivskyddat original blir annars skrivskyddad för SQLite och går inte att ta bort.
void clearReadOnly(const std::string& path)
{
    const std::wstring wide = toWide(path);
    const DWORD attrs = GetFileAttributesW(wide.c_str());
    if (attrs != INVALID_FILE_ATTRIBUTES && (attrs & FILE_ATTRIBUTE_READONLY))
        SetFileAttributesW(wide.c_str(), attrs & ~FILE_ATTRIBUTE_READONLY);
}

// Win32-felkoden som Error. Koden avgör ErrorCode enligt tabellen i headern,
// och systemets egen text läggs i detail.
core::Error win32Error(unsigned long code, std::string message)
{
    core::Error e;
    e.nativeCode = static_cast<int>(code);
    e.message = std::move(message);
    e.detail = systemMessage(static_cast<DWORD>(code)) + " (Win32 " + std::to_string(code) + ")";
    switch (code) {
    case ERROR_FILE_NOT_FOUND:
    case ERROR_PATH_NOT_FOUND:
        e.code = core::ErrorCode::NotFound;
        break;
    case ERROR_ACCESS_DENIED:
    case ERROR_WRITE_PROTECT:
        e.code = core::ErrorCode::AccessDenied;
        break;
    case ERROR_FILE_EXISTS:
    case ERROR_ALREADY_EXISTS:
        e.code = core::ErrorCode::AlreadyExists;
        break;
    case ERROR_SHARING_VIOLATION:
    case ERROR_LOCK_VIOLATION:
        e.code = core::ErrorCode::Busy;
        break;
    case ERROR_REQUEST_ABORTED:
        e.code = core::ErrorCode::Cancelled;
        break;
    case 53: case 54: case 55: case 59: case 64: case 67: case 121:
    case 1222: case 1231: case 1232: case 1236: case 2250:
        e.code = core::ErrorCode::Unreachable;
        break;
    default:
        e.code = core::ErrorCode::Io;
        break;
    }
    return e;
}

} // namespace kalkyl::platform
