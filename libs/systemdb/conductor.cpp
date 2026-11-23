#include "systemdb/conductor.h"

#include <cmath>

namespace kalkyl::systemdb {

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

// Sant när namn, typ och alla värden är lika. id och ursprung hör inte till värdena.
// Används för att se om en rad ändrats och behöver sparas.
bool sameConductorValues(const Conductor& a, const Conductor& b)
{
    if (a.name != b.name || a.type != b.type)
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

} // namespace kalkyl::systemdb
