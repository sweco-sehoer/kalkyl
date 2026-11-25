#pragma once
#include <array>
#include <optional>
#include <string>
#include <vector>

#include "conductor/conductor.h"

namespace kalkyl::linecalc {

// Antalet lastfall i arket Linberäkning: ID 1-7 i Ei-stadiet (rad 23-29) och
// ID 8-15 i Ep-stadiet (rad 31-38). LineInput::cases[id - 1] är lastfall id.
inline constexpr int kLoadCaseCount = 15;
inline constexpr int kEiCaseCount = 7;

// Hur beräkningen räknar (planen 4.2). Excel räknar som arket med dess fel, New rättar
// felen. Läget sparas per beräkning. Excel ska kunna tas bort utan att New ändras.
enum class CalcMode { Excel, New };

// Stadiet för ett lastfall (kolumn I): Ei före transponeringen och Ep efter.
// Stadiet följer av ID och kan inte ändras.
enum class LineStage { Ei, Ep };

// Var lastfallets temperatur kommer ifrån i läget New (planen 2.5). Fixed är talet i
// LoadCase::tempC. De andra följer indata: Min = J6, MinPlus20 = J6 + 20, Max = J7,
// ShortCircuit = kortslutningstemperaturen och ShortCircuitRise = J6 + (Tks - J7).
enum class TempSource { Fixed, Min, MinPlus20, Max, ShortCircuit, ShortCircuitRise };

// Var det karakteristiska vindtrycket (kolumn O) kommer ifrån i läget New: talet i
// LoadCase::charPressureNPerM2, Normal vind (O4) eller Hög vind (O5).
enum class PressureSource { Fixed, Normal, High };

// Is- och vindlast inskrivna för hand (arkets kolumn B = 1 med U och V).
// Lastfallet räknas då inte ur vind och is.
struct ManualLoad {
    double iceNPerM = 0;    // U, I
    double windNPerM = 0;   // V, Qwc
};

// Ett lastfall: en rad i arket, kolumn G-S. Resultaten står i LoadCaseResult.
// tempC och charPressureNPerM2 är cellernas tal, som läget Excel räknar med.
// Läget New räknar med källorna och använder talen bara när källan är Fixed.
struct LoadCase {
    TempSource tempSource = TempSource::Fixed;
    double tempC = 0;                            // G
    std::string name;                            // H, tom = lastfallet används inte
    std::string standardCase;                    // J, SS-EN 50341 Standard load case
    std::string clearanceCase;                   // K, Load case for clearances
    std::optional<int> windFromCase;             // L, lastfall-ID 1-15
    double windMagnitudePercent = 0;             // N
    PressureSource pressureSource = PressureSource::Fixed;
    double charPressureNPerM2 = 0;               // O
    double refHeightM = 0;                       // P, href
    double pressureGradientNPerM3 = 0;           // Q, dqh
    double iceThicknessMm = 0;                   // S
    std::optional<ManualLoad> manual;            // B = 1
};

// Konstanterna i arket (O9-O17) med arkets förval. pi används bara i läget Excel, där VBA
// använder arkets pi. Läget New räknar med exakt pi och läser inte pi (D10).
struct LineConstants {
    double iceDensityKgM3 = 917.4;       // O9
    double pi = 3.1416;                  // O10
    double gravity = 9.81;               // O11
    double gustFactor = 1;               // O12, Gq
    double resonanceFactor = 0.5;        // O13, Gc
    double dragFactor = 1;               // O14, Cc
    double minIceNPerM = 20;             // O15
    double maxUtilizationPercent = 55;   // O16
    int transpositionCase = 5;           // O17, lastfall-ID 1-7
};

// Indata till en linberäkning: läget, arkets J2-J18, O3-O17 och de 15 lastfallen.
// Tomt lindata, normalspann eller uppspänning ger ingen beräkning (planen 5.5).
// En ny beräkning skapas med newLineInput() i load_cases.h.
struct LineInput {
    CalcMode mode = CalcMode::New;
    std::optional<conductor::Conductor> conductor;   // J2, J9-J18; tomt = inget lindata
    std::optional<double> normalSpanM;               // J3, an
    std::optional<double> tensionMpa;                // J4, sigma0
    double refTempC = 0;                             // J5, T0
    double minTempC = -50;                           // J6
    double maxTempC = 50;                            // J7
    double shortCircuitTempC = 100;                  // korsningsmallen Övre linan
    double lineHeightM = 25;                         // O3
    double normalWindNPerM2 = 500;                   // O4
    double highWindNPerM2 = 800;                     // O5
    LineConstants constants;
    std::array<LoadCase, kLoadCaseCount> cases;
};

// Resultatet för ett lastfall: arkets beräknade kolumner M, R, T, U-AA och AC-AE.
// Talen är oavrundade. Visningen avrundar enligt arkets format (planen 2.1).
struct LoadCaseResult {
    int id = 0;                                  // 1-15
    LineStage stage = LineStage::Ei;
    double tempC = 0;                            // G som räkningen använde
    double charPressureNPerM2 = 0;               // O som räkningen använde
    std::optional<double> heightM;               // M; tom vid manuell last
    std::optional<double> pressureNPerM2;        // R; tom vid manuell last
    std::optional<double> iceEquivalentMm;       // T; tom vid manuell last
    double iceNPerM = 0;                         // U, I
    double windNPerM = 0;                        // V, Qwc
    double resultantNPerM = 0;                   // Q i planen 3.3 (inte i arket)
    double modulusMpa = 0;                       // E i lösningen: Ei, Eiv eller Ep
    double sagM = 0;                             // W, b
    double stressMpa = 0;                        // X, sigma
    double forceN = 0;                           // Y, F
    double mall = 0;                             // Z
    std::optional<double> utilizationPercent;    // AA; tom = "-"
    bool overUtilization = false;                // AA röd: över O16
    double inclinedSagM = 0;                     // AC
    double swingAngleDeg = 0;                    // AD
    double inclinedMall = 0;                     // AE, oavrundad
};

// Resultatet för alla lastfall och transponeringens mellanvärden (planen 3.5).
// cases[id - 1] är lastfall id. stakingMall är Z i lastfall 15 (beslut 7).
struct LineResult {
    CalcMode mode = CalcMode::New;
    std::array<LoadCaseResult, kLoadCaseCount> cases;
    double k1 = 0;                               // K1, MPa²
    double unloadedTranspositionStressMpa = 0;   // x0 som L räknades med
    int transpositionEivCase = 0;                // lastfallet som gav E4; 0 = Ei eller D4
    double transpositionModulusMpa = 0;          // E4
    double transpositionStressMpa = 0;           // sigma_tr = y * Q_tr * an / A
    double epReferenceStressMpa = 0;             // sigma_p0
    double epReferenceTempC = 0;                 // T_tr
    double stakingMall = 0;                      // Z i lastfall 15
};

// Fältet som en upplysning gäller. Fönstret översätter fältet och lastfallet till
// en cell. Result gäller själva räkningen i ett lastfall.
enum class LineField {
    None, Conductor, NormalSpan, Tension, RefTemp, MinTemp, MaxTemp, ShortCircuitTemp,
    LineHeight, NormalWind, HighWind, IceDensity, Pi, Gravity, GustFactor, ResonanceFactor,
    DragFactor, MinIce, MaxUtilization, TranspositionCase, CaseTemp, CaseWindFrom,
    CaseMagnitude, CasePressure, CaseRefHeight, CaseGradient, CaseIce, CaseManualIce,
    CaseManualWind, Result
};

// Slaget av upplysning. Missing: indata saknas, inget resultat, visas neutralt.
// Error: fel, inget resultat. Warning: resultatet finns men något bör ses över.
enum class LineIssueKind { Missing, Error, Warning };

// En upplysning. caseId 0 gäller hela beräkningen, annars lastfall 1-15.
// message är svensk text till användaren (planen 5.5).
struct LineIssue {
    LineIssueKind kind = LineIssueKind::Error;
    int caseId = 0;
    LineField field = LineField::None;
    std::string message;
};

// Svaret från calculateLine: resultatet när varken Missing eller Error finns, och alla
// upplysningar i ordningen hela beräkningen, sedan lastfall 1-15.
struct LineCalculation {
    std::optional<LineResult> result;
    std::vector<LineIssue> issues;
};

// Stadiet för lastfall id (1-15): Ei för 1-7, Ep för 8-15.
// id utanför 1-15 är ett programfel och ger Ep.
LineStage stageOf(int id);

// Lastfallets temperatur och karakteristiska vindtryck i input.mode: talet i läget
// Excel, källan i läget New (planen 2.5 och 4.2, D7-D9).
double caseTempC(const LineInput& input, int id);
double caseCharPressureNPerM2(const LineInput& input, int id);

// ACOS i Modul3 med arkets pi: -atan(y / sqrt(1 - y²)) + pi / 2. Vid y = 1 eller -1
// ger läget Excel pi / 2 och läget New 0 eller pi (planen 4.2, D6). |y| > 1 ger NaN.
double vbaAcos(double y, double pi, CalcMode mode);

// En rot till x^3 + c*x^2 - d = 0, räknad som VBA:s TREDJEGRAD (planen 3.3 och 5.6).
// pi och mode gäller ACOS-steget. Den icke-negativa rot som ligger närmast sigmaRef väljs
// i den trigonometriska grenen. Svarar NaN om svaret inte är ändligt.
double solveStateCubic(double c, double d, double pi, double sigmaRef, CalcMode mode);

// Linberäkningen enligt arket Linberäkning (planen avsnitt 3) i input.mode, med
// avvikelserna i 4.2. Missing och Error ger inget resultat, Warning ger resultat.
// Funktionen är ren och kastar inte.
LineCalculation calculateLine(const LineInput& input);

} // namespace kalkyl::linecalc
