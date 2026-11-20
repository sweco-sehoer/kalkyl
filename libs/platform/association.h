#pragma once
#include <string>

#include "core/result.h"

namespace kalkyl::platform {

// En filtyp som programmet vill äga: ändelse med punkt (".kalkyl"), ProgID ("Kalkyl.Projekt")
// och den beskrivning Utforskaren visar ("Kalkylprojekt").
struct FileType { std::string extension; std::string progId; std::string description; };

// Hur ändelsen är kopplad just nu. Other = en annan befintlig exe, Dead = exe:n finns inte.
enum class AssociationState { Missing, Ours, Other, Dead };

// Läser associationen under HKCU\Software\Classes och jämför exe:n med exePath.
// Missing: ändelsen eller ProgID:ts öppnakommando saknas. Ours: kommandot pekar på exePath.
// Other: en annan ProgID eller en annan exe som finns. Dead: kommandots exe finns inte längre.
AssociationState   associationState(const FileType& type, const std::string& exePath);

// Skriver ändelsen, ProgID, ikon och öppnakommando under HKCU\Software\Classes
// och meddelar Utforskaren (SHCNE_ASSOCCHANGED). Skriver över en tidigare registrering.
core::Result<void> registerAssociation(const FileType& type, const std::string& exePath);

// Tar bort ProgID:t och dess värde under OpenWithProgids. Ändelsens standardvärde tas bara
// bort om det är vårt ProgID. Tomma nycklar städas bort. Saknad registrering räknas som lyckat.
// Med exePath lämnas registreringen orörd om ProgID:ts öppnakommando pekar på en annan
// kopia av programmet som finns. Utan exePath tas den bort oavsett.
core::Result<void> unregisterAssociation(const FileType& type, const std::string& exePath = {});

} // namespace kalkyl::platform
