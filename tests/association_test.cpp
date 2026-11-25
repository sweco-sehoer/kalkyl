// Test för filassociationen. Använder bara testtypen .kalkyltest (ProgID Kalkyl.Test) i HKCU
// och rör aldrig den riktiga .kalkyl-associationen. Testtypen avregistreras före och efter.
#include <windows.h>

#include <cstdio>
#include <cstdlib>
#include <optional>
#include <string>
#include <vector>

#include "platform/association.h"
#include "platform/system.h"
#include "platform/text.h"

namespace {

int g_failures = 0;

// Kontrollerar ett villkor, skriver en rad vid fel och räknar felen.
#define CHECK(cond)                                                                     \
    do {                                                                                \
        if (!(cond)) {                                                                  \
            std::printf("FEL rad %d: %s\n", __LINE__, #cond);                           \
            ++g_failures;                                                               \
        }                                                                               \
    } while (false)

using kalkyl::platform::AssociationState;
using kalkyl::platform::FileType;

// Läser standardvärdet för en nyckel under HKCU\Software\Classes direkt ur registret.
// Nyckelvägen byggs med backslash som tecken 92, så att testet inte beror på platform-koden.
std::optional<std::wstring> classesDefault(const std::wstring& subkey)
{
    const std::wstring key = std::wstring(L"Software") + wchar_t(92) + L"Classes" + wchar_t(92) + subkey;
    DWORD bytes = 0;
    if (RegGetValueW(HKEY_CURRENT_USER, key.c_str(), nullptr, RRF_RT_REG_SZ, nullptr, nullptr, &bytes)
        != ERROR_SUCCESS)
        return std::nullopt;
    std::vector<wchar_t> buf(bytes / sizeof(wchar_t) + 1, L'\0');
    if (RegGetValueW(HKEY_CURRENT_USER, key.c_str(), nullptr, RRF_RT_REG_SZ, nullptr, buf.data(), &bytes)
        != ERROR_SUCCESS)
        return std::nullopt;
    return std::wstring(buf.data());
}

// Sant om en nyckel under HKCU\Software\Classes finns.
bool classesKeyExists(const std::wstring& subkey)
{
    const std::wstring key = std::wstring(L"Software") + wchar_t(92) + L"Classes" + wchar_t(92) + subkey;
    HKEY h = nullptr;
    const LONG rc = RegOpenKeyExW(HKEY_CURRENT_USER, key.c_str(), 0, KEY_READ, &h);
    if (rc == ERROR_SUCCESS) RegCloseKey(h);
    return rc == ERROR_SUCCESS;
}

// En kvarlämnad registrering från ett tidigare avbrutet test städas bort först.
void testCleanSlate(const FileType& type)
{
    CHECK(kalkyl::platform::unregisterAssociation(type).ok());
    CHECK(kalkyl::platform::associationState(type, kalkyl::platform::executablePath())
          == AssociationState::Missing);
}

// Registrering med den körande exe:n ger Ours och rätt värden i registret.
void testRegister(const FileType& type, const std::string& exe)
{
    CHECK(kalkyl::platform::registerAssociation(type, exe).ok());
    CHECK(kalkyl::platform::associationState(type, exe) == AssociationState::Ours);

    const std::wstring wexe = kalkyl::platform::toWide(exe);
    const std::wstring prog = kalkyl::platform::toWide(type.progId);
    const std::wstring sep(1, wchar_t(92));
    CHECK(classesDefault(kalkyl::platform::toWide(type.extension)) == prog);
    CHECK(classesDefault(prog) == kalkyl::platform::toWide(type.description));
    CHECK(classesDefault(prog + sep + L"DefaultIcon") == L"\"" + wexe + L"\",0");
    CHECK(classesDefault(prog + sep + L"shell" + sep + L"open" + sep + L"command")
          == L"\"" + wexe + L"\" \"%1\"");
}

// En annan befintlig exe ger Other, en exe som inte finns ger Dead, och en annan ProgID ger Other.
void testOtherAndDead(const FileType& type, const std::string& exe)
{
    const char* root = std::getenv("SystemRoot");
    const std::string cmd = std::string(root ? root : "C:") + "/System32/cmd.exe";
    CHECK(kalkyl::platform::associationState(type, cmd) == AssociationState::Other);

    // Pekar registreringen på en exe som saknas är den död, vilken exe man än frågar med.
    const std::string gone = exe + ".finns-inte.exe";
    CHECK(kalkyl::platform::registerAssociation(type, gone).ok());
    CHECK(kalkyl::platform::associationState(type, exe) == AssociationState::Dead);
    CHECK(kalkyl::platform::associationState(type, gone) == AssociationState::Dead);

    // Ny registrering skriver över den döda.
    CHECK(kalkyl::platform::registerAssociation(type, exe).ok());
    CHECK(kalkyl::platform::associationState(type, exe) == AssociationState::Ours);

    FileType rival = type;
    rival.progId = "Kalkyl.Test2";
    CHECK(kalkyl::platform::associationState(rival, exe) == AssociationState::Other);
}

// Avregistrering tar bort allt vi skrev, och en ny avregistrering är ett lyckat no-op.
void testUnregister(const FileType& type, const std::string& exe)
{
    CHECK(kalkyl::platform::unregisterAssociation(type).ok());
    CHECK(kalkyl::platform::associationState(type, exe) == AssociationState::Missing);
    CHECK(!classesKeyExists(kalkyl::platform::toWide(type.progId)));
    CHECK(!classesKeyExists(kalkyl::platform::toWide(type.extension)));
    CHECK(kalkyl::platform::unregisterAssociation(type).ok());
}

} // namespace

int main()
{
    const FileType type{".kalkyltest", "Kalkyl.Test", "Kalkyltest"};
    const std::string exe = kalkyl::platform::executablePath();
    CHECK(!exe.empty());

    testCleanSlate(type);
    testRegister(type, exe);
    testOtherAndDead(type, exe);
    testUnregister(type, exe);

    if (g_failures == 0) std::printf("association: alla kontroller gick bra\n");
    return g_failures;
}
