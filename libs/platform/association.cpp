#include "platform/association.h"

#include <windows.h>
#include <shlobj.h>

#include <initializer_list>
#include <string>
#include <vector>

#include "platform/files.h"
#include "platform/text.h"

namespace kalkyl::platform {

namespace {

// Registrets sökvägsavgränsare (backslash).
constexpr wchar_t kSep = 92;

// Fogar ihop nyckeldelar med avgränsaren, utan avslutande avgränsare.
std::wstring joinKey(std::initializer_list<std::wstring> parts)
{
    std::wstring out;
    for (const std::wstring& p : parts) {
        if (!out.empty()) out += kSep;
        out += p;
    }
    return out;
}

// Hela nyckelvägen under HKCU för en nyckel under klasserna. Allt skrivs här och aldrig under HKLM.
std::wstring classesKey(const std::wstring& subkey)
{
    return joinKey({L"Software", L"Classes", subkey});
}

// Gör ett registerfel till ett Error: AccessDenied för nekad åtkomst, annars Io.
core::Error registryError(LONG code, const std::string& what)
{
    core::Error e;
    e.code = (code == ERROR_ACCESS_DENIED) ? core::ErrorCode::AccessDenied : core::ErrorCode::Io;
    e.message = "Det gick inte att ändra filassociationen i registret: " + what;
    e.detail = "Win32-fel " + std::to_string(code);
    e.nativeCode = static_cast<int>(code);
    return e;
}

// Skriver ett REG_SZ under HKCU\Software\Classes\<subkey>. name == nullptr är standardvärdet.
// Nyckeln skapas om den saknas.
LONG setString(const std::wstring& subkey, const wchar_t* name, const std::wstring& data)
{
    HKEY key = nullptr;
    LONG rc = RegCreateKeyExW(HKEY_CURRENT_USER, classesKey(subkey).c_str(), 0, nullptr,
                              REG_OPTION_NON_VOLATILE, KEY_WRITE, nullptr, &key, nullptr);
    if (rc != ERROR_SUCCESS) return rc;
    const DWORD bytes = static_cast<DWORD>((data.size() + 1) * sizeof(wchar_t));
    rc = RegSetValueExW(key, name, 0, REG_SZ, reinterpret_cast<const BYTE*>(data.c_str()), bytes);
    RegCloseKey(key);
    return rc;
}

// Läser ett REG_SZ under HKCU\Software\Classes\<subkey>. Svarar false om nyckeln
// eller värdet saknas. Miljövariabler i REG_EXPAND_SZ expanderas.
bool getString(const std::wstring& subkey, const wchar_t* name, std::wstring& out)
{
    DWORD bytes = 0;
    const DWORD flags = RRF_RT_REG_SZ | RRF_RT_REG_EXPAND_SZ;
    const std::wstring full = classesKey(subkey);
    if (RegGetValueW(HKEY_CURRENT_USER, full.c_str(), name, flags, nullptr, nullptr, &bytes)
        != ERROR_SUCCESS)
        return false;
    std::vector<wchar_t> buf(bytes / sizeof(wchar_t) + 1, L'\0');
    if (RegGetValueW(HKEY_CURRENT_USER, full.c_str(), name, flags, nullptr, buf.data(), &bytes)
        != ERROR_SUCCESS)
        return false;
    out = buf.data();
    return true;
}

// Jämförelsenyckel för en exe-sökväg: absolut och i gemener, utan I/O.
std::wstring exeKey(const std::wstring& path)
{
    DWORD n = GetFullPathNameW(path.c_str(), 0, nullptr, nullptr);
    if (n == 0) return {};
    std::vector<wchar_t> buf(n + 1, L'\0');
    n = GetFullPathNameW(path.c_str(), static_cast<DWORD>(buf.size()), buf.data(), nullptr);
    if (n == 0 || n >= buf.size()) return {};
    CharLowerBuffW(buf.data(), n);
    return std::wstring(buf.data(), n);
}

// Tar ut exe:n ur ett öppnakommando: det som står inom citationstecken, annars första ordet.
std::wstring exeOfCommand(const std::wstring& command)
{
    if (!command.empty() && command[0] == L'"') {
        const size_t end = command.find(L'"', 1);
        if (end == std::wstring::npos) return command.substr(1);
        return command.substr(1, end - 1);
    }
    return command.substr(0, command.find(L' '));
}

// Sant om filen finns. Ett svar av annat slag än "saknas" räknas som att den finns.
// Frågan har en tidsgräns på 1 s, så att en exe på en server som inte svarar inte
// fördröjer starten. En sådan räknas som att den finns och skrivs alltså inte över.
bool fileExists(const std::wstring& path)
{
    return probe(toUtf8(path), std::chrono::milliseconds(1000)) != PathState::Missing;
}

// Jämför två ProgID utan hänsyn till skiftläge, som Windows gör.
bool sameProgId(const std::wstring& a, const std::wstring& b)
{
    return CompareStringOrdinal(a.c_str(), static_cast<int>(a.size()), b.c_str(),
                                static_cast<int>(b.size()), TRUE) == CSTR_EQUAL;
}

// Tar bort en nyckel under klasserna om den varken har undernycklar eller värden.
void removeIfEmpty(const std::wstring& subkey)
{
    HKEY key = nullptr;
    if (RegOpenKeyExW(HKEY_CURRENT_USER, classesKey(subkey).c_str(), 0, KEY_READ, &key)
        != ERROR_SUCCESS)
        return;
    DWORD subkeys = 0, values = 0;
    const LONG rc = RegQueryInfoKeyW(key, nullptr, nullptr, nullptr, &subkeys, nullptr, nullptr,
                                     &values, nullptr, nullptr, nullptr, nullptr);
    RegCloseKey(key);
    if (rc == ERROR_SUCCESS && subkeys == 0 && values == 0)
        RegDeleteKeyW(HKEY_CURRENT_USER, classesKey(subkey).c_str());
}

// Säger till Utforskaren att associationerna ändrats, så att ikoner och öppna-med uppdateras.
void notifyShell()
{
    SHChangeNotify(SHCNE_ASSOCCHANGED, SHCNF_IDLIST, nullptr, nullptr);
}

} // namespace

// Läser hur ändelsen är kopplad: först ändelsens ProgID, sedan ProgID:ts öppnakommando
// och till sist om kommandots exe finns och är exePath. Läser bara registret och gör
// en fråga med tidsgräns mot exe-filen.
AssociationState associationState(const FileType& type, const std::string& exePath)
{
    const std::wstring ext = toWide(type.extension);
    const std::wstring prog = toWide(type.progId);

    // Ändelsen måste peka ut ett ProgID. Pekar den på ett annat är den upptagen av någon annan.
    std::wstring current;
    if (!getString(ext, nullptr, current) || current.empty()) return AssociationState::Missing;
    if (!sameProgId(current, prog)) return AssociationState::Other;

    // Vårt ProgID måste ha ett öppnakommando, och dess exe måste finnas och vara den här.
    std::wstring command;
    if (!getString(joinKey({prog, L"shell", L"open", L"command"}), nullptr, command)
        || command.empty())
        return AssociationState::Missing;
    const std::wstring registered = exeOfCommand(command);
    if (!fileExists(registered)) return AssociationState::Dead;
    return exeKey(registered) == exeKey(toWide(exePath)) ? AssociationState::Ours
                                                        : AssociationState::Other;
}

// Skriver de fem registerraderna för filtypen med de breda API:erna och meddelar Utforskaren.
// En ogiltig filtyp eller tom exe-sökväg avvisas innan något skrivs. Det första
// registerfelet avbryter och returneras.
core::Result<void> registerAssociation(const FileType& type, const std::string& exePath)
{
    const std::wstring ext = toWide(type.extension);
    const std::wstring prog = toWide(type.progId);
    const std::wstring exe = toWide(exePath);
    if (ext.size() < 2 || ext[0] != L'.' || prog.empty() || exe.empty()) {
        core::Error e;
        e.code = core::ErrorCode::Io;
        e.message = "Ogiltig filtyp eller sökväg för filassociationen.";
        e.detail = "extension=" + type.extension + " progId=" + type.progId;
        return e;
    }

    // Raderna ur planens avsnitt 2.12, i ordning: ändelsen, ProgID, ikon och öppnakommando.
    struct Row { std::wstring subkey; const wchar_t* name; std::wstring data; const char* what; };
    const std::vector<Row> rows = {
        {ext, nullptr, prog, "ändelsen"},
        {joinKey({ext, L"OpenWithProgids"}), prog.c_str(), L"", "OpenWithProgids"},
        {prog, nullptr, toWide(type.description), "beskrivningen"},
        {joinKey({prog, L"DefaultIcon"}), nullptr, L"\"" + exe + L"\",0", "ikonen"},
        {joinKey({prog, L"shell", L"open", L"command"}), nullptr,
         L"\"" + exe + L"\" \"%1\"", "öppnakommandot"},
    };
    for (const Row& r : rows) {
        const LONG rc = setString(r.subkey, r.name, r.data);
        if (rc != ERROR_SUCCESS) return registryError(rc, r.what);
    }
    notifyShell();
    return {};
}

// Tar bort det filtypen registrerat. Med exePath lämnas en registrering som pekar på en
// annan befintlig kopia orörd (Other). Andra programs värden under ändelsen rörs aldrig,
// och tomma nycklar städas bort till sist.
core::Result<void> unregisterAssociation(const FileType& type, const std::string& exePath)
{
    const std::wstring ext = toWide(type.extension);
    const std::wstring prog = toWide(type.progId);
    const std::wstring withProgids = joinKey({ext, L"OpenWithProgids"});

    // Pekar vårt ProgID på en annan kopia av programmet som finns, får den behålla registreringen.
    if (!exePath.empty()) {
        std::wstring command;
        if (getString(joinKey({prog, L"shell", L"open", L"command"}), nullptr, command) &&
            !command.empty()) {
            const std::wstring registered = exeOfCommand(command);
            if (fileExists(registered) && exeKey(registered) != exeKey(toWide(exePath)))
                return {};
        }
    }

    // ProgID-trädet tas bort helt. Saknas det är det redan klart.
    LONG rc = RegDeleteTreeW(HKEY_CURRENT_USER, classesKey(prog).c_str());
    if (rc != ERROR_SUCCESS && rc != ERROR_FILE_NOT_FOUND) return registryError(rc, "ProgID");

    // Vårt värde under OpenWithProgids tas bort, andra program rörs inte.
    rc = RegDeleteKeyValueW(HKEY_CURRENT_USER, classesKey(withProgids).c_str(), prog.c_str());
    if (rc != ERROR_SUCCESS && rc != ERROR_FILE_NOT_FOUND) return registryError(rc, "OpenWithProgids");

    // Standardvärdet tas bara bort om det är vårt ProgID.
    std::wstring current;
    if (getString(ext, nullptr, current) && sameProgId(current, prog)) {
        rc = RegDeleteKeyValueW(HKEY_CURRENT_USER, classesKey(ext).c_str(), nullptr);
        if (rc != ERROR_SUCCESS && rc != ERROR_FILE_NOT_FOUND) return registryError(rc, "ändelsen");
    }

    removeIfEmpty(withProgids);
    removeIfEmpty(ext);
    notifyShell();
    return {};
}

} // namespace kalkyl::platform
