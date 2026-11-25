#include "conductor/conductor.h"

#include <cmath>

namespace kalkyl::conductor {

namespace {

// Tecknet i ASCII-gemener. Allt utanför A-Z lämnas orört, som SQLite NOCASE.
char asciiLower(char c)
{
    return (c >= 'A' && c <= 'Z') ? static_cast<char>(c - 'A' + 'a') : c;
}

// Sant om texten är tom efter att blanktecken i början och slutet tagits bort.
bool blank(std::string_view s)
{
    for (char c : s)
        if (c != ' ' && c != '\t' && c != '\r' && c != '\n')
            return false;
    return true;
}

// Lägger till ett fel om talet inte är ändligt och större än noll.
void requirePositive(std::vector<ConductorIssue>& issues, ConductorField field, double value,
                     const char* label)
{
    if (!std::isfinite(value) || !(value > 0))
        issues.push_back({field, std::string(label) + " måste vara ett tal större än 0."});
}

// Lägger till ett fel om talet inte är ändligt och noll eller större.
void requireNonNegative(std::vector<ConductorIssue>& issues, ConductorField field, double value,
                        const char* label)
{
    if (!std::isfinite(value) || value < 0)
        issues.push_back({field, std::string(label) + " måste vara ett tal som är 0 eller större."});
}

// Två tal som lagras lika: exakt jämförelse, eftersom värdena kommer ur databasen oförändrade.
bool sameNumber(double a, double b)
{
    return a == b;
}

// Sant när ett siffertecken, '0'-'9'.
bool isDigit(char c)
{
    return c >= '0' && c <= '9';
}

} // namespace

// Kontrollerar linan mot reglerna i planen 2.9 och svarar med ett fel per fält som bryter mot dem.
// Namnets unikhet hör till databasen och kontrolleras inte här. Typen får vara vad som helst.
std::vector<ConductorIssue> validateConductor(const Conductor& c)
{
    std::vector<ConductorIssue> issues;
    if (blank(c.name))
        issues.push_back({ConductorField::Name, "Namnet får inte vara tomt."});
    requirePositive(issues, ConductorField::Area, c.areaMm2, "Area");
    requirePositive(issues, ConductorField::Diameter, c.diameterMm, "Diametern");
    requirePositive(issues, ConductorField::EInitial, c.eInitialMpa, "Eik");
    requirePositive(issues, ConductorField::EPermanent, c.ePermanentMpa, "Ep");
    requirePositive(issues, ConductorField::EInitialLimit, c.eInitialLimitMpa,
                    "Högsta spänning för Eik");
    requireNonNegative(issues, ConductorField::EivConstant, c.eivConstant, "Konstanten för Eiv");
    if (!std::isfinite(c.alphaE6PerC))
        issues.push_back({ConductorField::Alpha, "α måste vara ett ändligt tal."});
    requireNonNegative(issues, ConductorField::CreepStrain, c.creepStrainPermille, "εc");
    requireNonNegative(issues, ConductorField::CreepTemp, c.creepTempC, "Krypningen");
    requirePositive(issues, ConductorField::Weight, c.weightNPerM, "Egentyngden");
    if (c.breakingLoadKn && (!std::isfinite(*c.breakingLoadKn) || !(*c.breakingLoadKn > 0)))
        issues.push_back({ConductorField::BreakingLoad, "Brottlasten måste vara tom eller större än 0."});
    return issues;
}

// Krypningstemperaturen ur L = K · 1000 / J. Utan krypning blir den 0 oavsett α,
// och med α = 0 men krypning går den inte att räkna ut.
std::optional<double> derivedCreepTempC(double creepStrainPermille, double alphaE6PerC)
{
    if (creepStrainPermille == 0)
        return 0.0;
    if (alphaE6PerC == 0)
        return std::nullopt;
    return creepStrainPermille * 1000.0 / alphaE6PerC;
}

// Sant när det lagrade värdet på L skiljer sig mer än toleransen från det framräknade.
// Kan L inte räknas ut är det inte en avvikelse.
bool creepTempDeviates(const Conductor& c)
{
    const auto derived = derivedCreepTempC(c.creepStrainPermille, c.alphaE6PerC);
    return derived && std::fabs(c.creepTempC - *derived) > kCreepTempTolerance;
}

// Jämför namn som SQLite NOCASE gör: A-Z och a-z är lika, alla andra byte jämförs exakt.
// Å och å är alltså olika namn.
bool sameConductorName(std::string_view a, std::string_view b)
{
    if (a.size() != b.size())
        return false;
    for (std::size_t i = 0; i < a.size(); ++i)
        if (asciiLower(a[i]) != asciiLower(b[i]))
            return false;
    return true;
}

// Sant när typ och alla värden är lika. Namn, id och ursprung hör inte till uppgifterna.
// Används för att se om projektets kopia skiljer sig från linregistret.
bool sameConductorData(const Conductor& a, const Conductor& b)
{
    if (a.type != b.type)
        return false;
    if (a.breakingLoadKn.has_value() != b.breakingLoadKn.has_value())
        return false;
    if (a.breakingLoadKn && !sameNumber(*a.breakingLoadKn, *b.breakingLoadKn))
        return false;
    return sameNumber(a.areaMm2, b.areaMm2) && sameNumber(a.diameterMm, b.diameterMm) &&
           sameNumber(a.eInitialMpa, b.eInitialMpa) && sameNumber(a.ePermanentMpa, b.ePermanentMpa) &&
           sameNumber(a.eInitialLimitMpa, b.eInitialLimitMpa) &&
           sameNumber(a.eivConstant, b.eivConstant) && sameNumber(a.alphaE6PerC, b.alphaE6PerC) &&
           sameNumber(a.creepStrainPermille, b.creepStrainPermille) &&
           sameNumber(a.creepTempC, b.creepTempC) && sameNumber(a.weightNPerM, b.weightNPerM);
}

// Sant när namn, typ och alla värden är lika. id och ursprung hör inte till värdena.
// Används för att se om en rad ändrats och behöver sparas.
bool sameConductorValues(const Conductor& a, const Conductor& b)
{
    return a.name == b.name && sameConductorData(a, b);
}

// Naturlig jämförelse: siffror jämförs som tal (utan inledande nollor), övriga tecken ett
// i taget med ASCII-versaler vikta. Svarar <0, 0 eller >0.
int compareNames(std::string_view a, std::string_view b)
{
    std::size_t i = 0, j = 0;
    while (i < a.size() && j < b.size()) {
        if (isDigit(a[i]) && isDigit(b[j])) {
            while (i < a.size() && a[i] == '0') ++i;   // inledande nollor räknas inte
            while (j < b.size() && b[j] == '0') ++j;
            std::size_t ei = i, ej = j;
            while (ei < a.size() && isDigit(a[ei])) ++ei;
            while (ej < b.size() && isDigit(b[ej])) ++ej;
            if (ei - i != ej - j)
                return (ei - i < ej - j) ? -1 : 1;     // fler siffror = större tal
            if (int c = a.compare(i, ei - i, b.substr(j, ej - j)); c != 0)
                return c;
            i = ei;
            j = ej;
            continue;
        }
        const unsigned char ca = static_cast<unsigned char>(asciiLower(a[i]));
        const unsigned char cb = static_cast<unsigned char>(asciiLower(b[j]));
        if (ca != cb)
            return ca < cb ? -1 : 1;
        ++i;
        ++j;
    }
    if (i < a.size()) return 1;
    if (j < b.size()) return -1;
    return 0;
}

} // namespace kalkyl::conductor
