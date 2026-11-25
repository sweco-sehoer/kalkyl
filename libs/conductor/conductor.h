#pragma once
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace kalkyl::conductor {

// Var linan kommer ifrån. Härleds vid läsning och ignoreras vid sparning.
enum class ConductorOrigin { Seed, SeedModified, Own };

// En lina i linregistret, med enheterna i SWECO:s blad Lindata (kolumn B–N).
struct Conductor {
    std::int64_t id = 0;                         // 0 = ny, inte sparad
    ConductorOrigin origin = ConductorOrigin::Own;
    std::string name;                            // B
    std::string type;                            // C, får vara tom
    double areaMm2 = 0;                          // D  mm²
    double diameterMm = 0;                       // E  mm
    double eInitialMpa = 0;                      // F  Eik
    double ePermanentMpa = 0;                    // G  Ep
    double eInitialLimitMpa = 0;                 // H  högsta spänning där Eik gäller
    double eivConstant = 0;                      // I  k eller X
    double alphaE6PerC = 0;                      // J  10^-6/°C, får vara negativ
    double creepStrainPermille = 0;              // K  εc, ‰
    double creepTempC = 0;                       // L  °C
    double weightNPerM = 0;                      // M  N/m
    std::optional<double> breakingLoadKn;        // N  kN, tom = saknas
};

// Fältet som ett valideringsfel gäller. Fönstret översätter till kolumn.
enum class ConductorField { Name, Type, Area, Diameter, EInitial, EPermanent, EInitialLimit,
                            EivConstant, Alpha, CreepStrain, CreepTemp, Weight, BreakingLoad };
struct ConductorIssue { ConductorField field; std::string message; };   // message på svenska

inline constexpr double kCreepTempTolerance = 0.5;   // °C, se planen 2.4

// Reglerna i planen 2.9. Tom lista = giltig. Namnets unikhet kontrolleras inte här.
std::vector<ConductorIssue> validateConductor(const Conductor& c);
// L = K · 1000 / J. K = 0 ger 0; J = 0 med K ≠ 0 ger tomt.
std::optional<double> derivedCreepTempC(double creepStrainPermille, double alphaE6PerC);
// true om L avviker mer än kCreepTempTolerance från derivedCreepTempC.
bool creepTempDeviates(const Conductor& c);
// Namnjämförelse som SQLite NOCASE: ASCII utan hänsyn till versaler, allt annat exakt.
bool sameConductorName(std::string_view a, std::string_view b);
// Samma namn (exakt), typ och värden. id och origin ignoreras.
bool sameConductorValues(const Conductor& a, const Conductor& b);
// Samma typ och samma tal (D-N). Namn, id och ursprung ignoreras. Används för att se om
// projektets kopia skiljer sig från linregistret (planen 3.9).
bool sameConductorData(const Conductor& a, const Conductor& b);
// Naturlig jämförelse som tabellernas sortering i GlazeUI: en följd siffror jämförs som ett tal
// ("910" efter "185"), allt annat tecken för tecken med ASCII-versaler vikta. Svarar <0, 0 eller >0.
int compareNames(std::string_view a, std::string_view b);

} // namespace kalkyl::conductor
