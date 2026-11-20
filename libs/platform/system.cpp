#include "platform/system.h"

#include <windows.h>
#include <initguid.h>
#include <knownfolders.h>
#include <shellapi.h>
#include <shlobj.h>

#include <cstdio>

#include "platform/text.h"

namespace kalkyl::platform {

namespace {

// Läser en miljövariabel som UTF-8. Saknad variabel ger en tom sträng.
std::string envVar(const wchar_t* name)
{
    const DWORD n = GetEnvironmentVariableW(name, nullptr, 0);
    if (n == 0)
        return {};
    std::wstring buf(n, L'\0');
    const DWORD got = GetEnvironmentVariableW(name, buf.data(), n);
    buf.resize(got);
    return toUtf8(buf);
}

// Hämtar en känd mapp (Dokument m.fl.) som UTF-8. Fel ger en tom sträng.
std::string knownFolder(const KNOWNFOLDERID& id)
{
    PWSTR raw = nullptr;
    if (FAILED(SHGetKnownFolderPath(id, 0, nullptr, &raw))) {
        CoTaskMemFree(raw);
        return {};
    }
    std::string out = toUtf8(raw);
    CoTaskMemFree(raw);
    return out;
}

} // namespace

// Argumenten ur kommandoraden som UTF-8. MinGW:s argv är ANSI och tappar tecken
// utanför teckentabellen, så raden tolkas om med CommandLineToArgvW.
std::vector<std::string> commandLineArgs()
{
    std::vector<std::string> out;
    int argc = 0;
    LPWSTR* argv = CommandLineToArgvW(GetCommandLineW(), &argc);
    if (!argv)
        return out;
    for (int i = 0; i < argc; ++i)
        out.push_back(toUtf8(argv[i]));
    LocalFree(argv);
    return out;
}

// Exe-filens sökväg. Bufferten dubblas tills sökvägen ryms, eftersom
// GetModuleFileNameW trunkerar tyst när den är för liten.
std::string executablePath()
{
    std::wstring buf(MAX_PATH, L'\0');
    for (;;) {
        const DWORD n = GetModuleFileNameW(nullptr, buf.data(), static_cast<DWORD>(buf.size()));
        if (n == 0)
            return {};
        if (n < buf.size()) {
            buf.resize(n);
            return toUtf8(buf);
        }
        buf.resize(buf.size() * 2);
    }
}

// Programmets datamapp. KALKYL_DATA_DIR har företräde (tester), annars
// %LOCALAPPDATA%\Kalkyl. Mappen skapas inte här.
std::string kalkylDataDir()
{
    std::string dir = envVar(L"KALKYL_DATA_DIR");
    if (!dir.empty())
        return dir;
    std::string local = envVar(L"LOCALAPPDATA");
    if (local.empty())
        local = knownFolder(FOLDERID_LocalAppData);
    return local.empty() ? std::string() : local + '\\' + "Kalkyl";
}

// Användarens Dokument-mapp.
std::string documentsDir()
{
    return knownFolder(FOLDERID_Documents);
}

// Inloggat användarnamn. Bufferten får UNLEN + 1 tecken.
std::string userName()
{
    wchar_t buf[257] = {};
    DWORD n = 257;
    if (!GetUserNameW(buf, &n))
        return {};
    return toUtf8(buf);
}

// Datorns DNS-värdnamn. Första anropet frågar efter storleken.
std::string computerName()
{
    DWORD n = 0;
    GetComputerNameExW(ComputerNameDnsHostname, nullptr, &n);
    if (n == 0)
        return {};
    std::wstring buf(n, L'\0');
    if (!GetComputerNameExW(ComputerNameDnsHostname, buf.data(), &n))
        return {};
    buf.resize(n);
    return toUtf8(buf);
}

// Nuvarande tid i UTC med millisekunder, i formen 2026-11-20T13:45:07.123Z.
std::string nowUtcIso()
{
    SYSTEMTIME st;
    GetSystemTime(&st);
    char buf[40];
    std::snprintf(buf, sizeof buf, "%04d-%02d-%02dT%02d:%02d:%02d.%03dZ", st.wYear, st.wMonth,
                  st.wDay, st.wHour, st.wMinute, st.wSecond, st.wMilliseconds);
    return buf;
}

// Gör om en UTC-tid i nowUtcIso-formen till lokal tid "ÅÅÅÅ-MM-DD HH:MM".
// Fel format eller ogiltig tid ger en tom sträng.
std::string localTimeText(std::string_view utcIso)
{
    const std::string s(utcIso);
    SYSTEMTIME utc = {};
    int y, mo, d, h, mi, sec;
    if (std::sscanf(s.c_str(), "%4d-%2d-%2dT%2d:%2d:%2d", &y, &mo, &d, &h, &mi, &sec) != 6)
        return {};
    utc.wYear = static_cast<WORD>(y);
    utc.wMonth = static_cast<WORD>(mo);
    utc.wDay = static_cast<WORD>(d);
    utc.wHour = static_cast<WORD>(h);
    utc.wMinute = static_cast<WORD>(mi);
    utc.wSecond = static_cast<WORD>(sec);
    SYSTEMTIME local;
    if (!SystemTimeToTzSpecificLocalTime(nullptr, &utc, &local))
        return {};
    char buf[32];
    std::snprintf(buf, sizeof buf, "%04d-%02d-%02d %02d:%02d", local.wYear, local.wMonth,
                  local.wDay, local.wHour, local.wMinute);
    return buf;
}

} // namespace kalkyl::platform
