#pragma once
#include <string>
#include <string_view>
#include <vector>

namespace kalkyl::platform {

// Programmets argument som UTF-8 (GetCommandLineW + CommandLineToArgvW). [0] är programmet.
std::vector<std::string> commandLineArgs();
// Sökvägen till den körande exe-filen (GetModuleFileNameW med växande buffert).
std::string executablePath();
// KALKYL_DATA_DIR om den är satt, annars %LOCALAPPDATA%\Kalkyl. Mappen skapas inte.
std::string kalkylDataDir();
// Användarens Dokument-mapp (FOLDERID_Documents).
std::string documentsDir();
// Inloggad användare (GetUserNameW).
std::string userName();
// Datorns namn (GetComputerNameExW med ComputerNameDnsHostname).
std::string computerName();
// Nu i UTC som "2026-11-20T13:45:07.123Z".
std::string nowUtcIso();
// En UTC-tid från nowUtcIso som lokal tid "2026-11-20 14:45". Tom sträng vid fel.
std::string localTimeText(std::string_view utcIso);

} // namespace kalkyl::platform
