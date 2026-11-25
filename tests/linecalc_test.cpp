#include <algorithm>
#include <array>
#include <cmath>
#include <iostream>
#include <limits>
#include <numbers>
#include <optional>
#include <string>
#include <vector>

#include "lineCalc/line_calc.h"

using namespace kalkyl::linecalc;
using kalkyl::conductor::Conductor;

namespace {

int g_failures = 0;

// Räknar ett misslyckande och skriver var det hände.
void report(bool ok, const char* expr, const char* file, int line)
{
    if (ok)
        return;
    ++g_failures;
    std::cerr << file << ":" << line << ": CHECK misslyckades: " << expr << std::endl;
}
#define CHECK(cond) report(static_cast<bool>(cond), #cond, __FILE__, __LINE__)

// Två tal är lika inom den absoluta toleransen tol.
bool near(double a, double b, double tol)
{
    return std::fabs(a - b) <= tol;
}

// Två tal är lika inom den relativa toleransen tol (mot b, men minst 1).
bool relNear(double a, double b, double tol)
{
    return std::fabs(a - b) <= tol * std::max(1.0, std::fabs(b));
}

// Linan i T1 (planens bilaga A): 124 mm² OPGW AFL 1x48 SMF 14230/20-01 (Dotterel).
Conductor t1Conductor()
{
    Conductor c;
    c.name = "124 mm² OPGW AFL 1x48 SMF 14230/20-01 (Dotterel)";
    c.areaMm2 = 124;
    c.diameterMm = 14.9;
    c.eInitialMpa = 89000;
    c.ePermanentMpa = 95000;
    c.eInitialLimitMpa = 180;
    c.eivConstant = 40;
    c.alphaE6PerC = 17.1;
    c.creepTempC = 23.391812865497073;
    c.weightNPerM = 5.34;
    c.breakingLoadKn = 77.7;
    return c;
}

// Ett lastfall i standardmallen (planen 2.5.1): källor och tal som makrot skriver dem.
struct Spec {
    TempSource tempSource;
    double temp;
    const char* name;
    const char* standardCase;
    const char* clearanceCase;
    int windFrom;   // 0 = tom
    double magnitude;
    PressureSource pressureSource;
    double ice;
    double refHeight;
    double gradient;
};

// Standardmallens 15 rader. Vindtrycket i talet är 500 (Normal) eller 800 (Hög).
void applyStandardCases(LineInput& in)
{
    const Spec specs[kLoadCaseCount] = {
        {TempSource::Min, -50, "Vindstilla", "", "H", 0, 0, PressureSource::Fixed, 0, 0, 0},
        {TempSource::MinPlus20, -30, "Normal Vind", "", "D", 0, 100, PressureSource::Normal, 0, 25, 6},
        {TempSource::Fixed, 15, "30% Normal Vind", "", "E", 0, 30, PressureSource::Normal, 0, 25, 6},
        {TempSource::Fixed, 0, "Islast och Normal Vind", "", "A", 0, 100, PressureSource::Normal, 18, 25, 6},
        {TempSource::Fixed, 0, "Islast och Vindstilla", "2a", "", 4, 0, PressureSource::Fixed, 18, 0, 0},
        {TempSource::Fixed, 0, "Hög Vind", "1a", "", 0, 100, PressureSource::High, 0, 10, 6},
        {TempSource::Fixed, 0, "Vindstilla", "4", "", 0, 0, PressureSource::Fixed, 0, 0, 0},
        {TempSource::Fixed, 0, "Islast och Normal Vind", "3", "B", 0, 100, PressureSource::Normal, 18, 25, 6},
        {TempSource::Fixed, 0, "Islast och Vindstilla", "2a", "", 8, 0, PressureSource::Fixed, 18, 0, 0},
        {TempSource::Fixed, 0, "Hög Vind", "1a", "", 0, 100, PressureSource::High, 0, 10, 6},
        {TempSource::Fixed, 0, "Vindstilla", "", "", 0, 0, PressureSource::Fixed, 0, 0, 0},
        {TempSource::Fixed, 15, "Vindstilla", "", "G", 0, 0, PressureSource::Fixed, 0, 0, 0},
        {TempSource::Fixed, 15, "Normal Vind", "", "C", 0, 100, PressureSource::Normal, 0, 25, 6},
        {TempSource::Fixed, 15, "30% Normal Vind", "", "F", 0, 30, PressureSource::Normal, 0, 25, 6},
        {TempSource::Max, 50, "Vindstilla", "", "", 0, 0, PressureSource::Fixed, 0, 0, 0},
    };
    for (int i = 0; i < kLoadCaseCount; ++i) {
        const Spec& s = specs[i];
        LoadCase& lc = in.cases[static_cast<std::size_t>(i)];
        lc = LoadCase{};
        lc.tempSource = s.tempSource;
        lc.tempC = s.temp;
        lc.name = s.name;
        lc.standardCase = s.standardCase;
        lc.clearanceCase = s.clearanceCase;
        if (s.windFrom != 0)
            lc.windFromCase = s.windFrom;
        lc.windMagnitudePercent = s.magnitude;
        lc.pressureSource = s.pressureSource;
        lc.charPressureNPerM2 = s.pressureSource == PressureSource::Normal ? 500
                                : s.pressureSource == PressureSource::High ? 800 : 0;
        lc.refHeightM = s.refHeight;
        lc.pressureGradientNPerM3 = s.gradient;
        lc.iceThicknessMm = s.ice;
    }
}

// T1:s indata i läget mode, byggda utan load_cases.h.
LineInput makeT1(CalcMode mode)
{
    LineInput in;
    in.mode = mode;
    in.conductor = t1Conductor();
    in.normalSpanM = 314.6;
    in.tensionMpa = 90;
    in.refTempC = 0;
    in.minTempC = -50;
    in.maxTempC = 50;
    in.lineHeightM = 25;
    in.normalWindNPerM2 = 500;
    in.highWindNPerM2 = 800;
    applyStandardCases(in);
    return in;
}

// Sant om upplysningen finns med exakt den texten.
bool hasIssue(const LineCalculation& calc, LineIssueKind kind, int caseId, LineField field,
              const std::string& message)
{
    for (const auto& i : calc.issues)
        if (i.kind == kind && i.caseId == caseId && i.field == field && i.message == message)
            return true;
    return false;
}

// Sant om någon upplysning av slaget kind har en text som börjar med prefix.
bool hasIssueStarting(const LineCalculation& calc, LineIssueKind kind, int caseId, const std::string& prefix)
{
    for (const auto& i : calc.issues)
        if (i.kind == kind && i.caseId == caseId && i.message.rfind(prefix, 0) == 0)
            return true;
    return false;
}

// Antalet upplysningar av ett slag.
int countIssues(const LineCalculation& calc, LineIssueKind kind)
{
    int n = 0;
    for (const auto& i : calc.issues)
        if (i.kind == kind)
            ++n;
    return n;
}

// Spänningen X i T1 för lastfall 1-15 (bilaga A).
constexpr double kT1Stress[kLoadCaseCount] = {
    125.36876604134662, 120.8771712723737,  83.949876838075923, 249.63464951586255, 249.63464951586255,
    122.07038152417618, 90.000000001010662, 238.06864262048742, 238.06864262048742, 107.0831983140759,
    75.045064643665356, 69.989281629275595, 82.061086845247502, 71.212391435739804, 60.874406181452422};

// Testet av tredjegradsekvationen: Cardano, den trigonometriska grenen och negativt c.
void testCubic()
{
    const double k1 = 7.647955258454736;
    const double e = 89000;
    const double cCardano = k1 * e / (90.0 * 90.0) - 90;
    const double x = solveStateCubic(cCardano, k1 * e, 3.1416, 90, CalcMode::Excel);
    CHECK(std::fabs(x - 90.000000001010662) <= 1e-15 * 90.000000001010662);

    const double c2 = 291.13235456912173;
    const double d2 = 680668.0180024715;
    const double xt = solveStateCubic(c2, d2, 3.1416, 45, CalcMode::Excel);
    CHECK(relNear(xt, 44.999838055875486, 1e-12));
    CHECK(near(solveStateCubic(c2, d2, std::numbers::pi, 45, CalcMode::Excel), 45, 1e-12));
    CHECK(near(solveStateCubic(c2, d2, 3.1416, 45, CalcMode::New), xt, 1e-9));

    const double cn = -100;
    const double dn = 5e5;
    const double xn = solveStateCubic(cn, dn, 3.1416, 50, CalcMode::New);
    CHECK(xn > 0);
    CHECK(std::fabs(xn * xn * xn + cn * xn * xn - dn) < 1e-9 * dn);
    CHECK(std::isnan(solveStateCubic(std::numeric_limits<double>::quiet_NaN(), 1, 3.1416, 1, CalcMode::New)));
}

// Jämför T1 mot bilaga A i läget mode.
void checkT1(CalcMode mode)
{
    const LineCalculation calc = calculateLine(makeT1(mode));
    CHECK(calc.result.has_value());
    if (!calc.result)
        return;
    const LineResult& r = *calc.result;
    CHECK(r.mode == mode);
    for (int i = 0; i < kLoadCaseCount; ++i) {
        CHECK(relNear(r.cases[static_cast<std::size_t>(i)].stressMpa, kT1Stress[i], 1e-9));
        CHECK(r.cases[static_cast<std::size_t>(i)].id == i + 1);
    }
    const auto& c4 = r.cases[3];
    const auto& c5 = r.cases[4];
    const auto& c8 = r.cases[7];
    const auto& c9 = r.cases[8];
    CHECK(relNear(c4.pressureNPerM2.value_or(-1), 500, 1e-9));
    CHECK(relNear(c5.pressureNPerM2.value_or(-1), 0, 1e-9));
    CHECK(relNear(c8.pressureNPerM2.value_or(-1), 500, 1e-9));
    CHECK(relNear(c9.pressureNPerM2.value_or(-1), 0, 1e-9));
    for (const auto* c : {&c4, &c5, &c8, &c9})
        CHECK(relNear(c->iceEquivalentMm.value_or(-1), 20.264505893041264, 1e-9));
    CHECK(relNear(c4.iceNPerM, 16.743530380610878, 1e-9));
    CHECK(relNear(c8.iceNPerM, 16.743530380610878, 1e-9));
    CHECK(relNear(c5.iceNPerM, 20.147407460770967, 1e-9));
    CHECK(relNear(c9.iceNPerM, 20.147407460770967, 1e-9));
    CHECK(relNear(c4.windNPerM, 12.725, 1e-9));
    CHECK(relNear(c5.windNPerM, 0, 1e-9));
    CHECK(relNear(c4.sagM, 8.8261113418727888, 1e-9));
    CHECK(relNear(c4.forceN, 30954.696539966957, 1e-9));
    CHECK(relNear(c4.mall, 356.70726657096884, 1e-9));
    CHECK(relNear(c4.utilizationPercent.value_or(-1), 39.838734285671762, 1e-9));
    CHECK(relNear(c4.inclinedSagM, 10.186536852909702, 1e-9));
    CHECK(near(c4.inclinedMall, 412, 0.5 + 1e-9));
    CHECK(relNear(r.cases[1].swingAngleDeg, 34.898217991235953, 1e-9));
    CHECK(relNear(r.cases[5].swingAngleDeg, 51.152984533267528, 1e-9));
    CHECK(relNear(r.cases[1].heightM.value_or(-1), 25, 1e-9));
    CHECK(relNear(r.cases[0].heightM.value_or(-1), 0, 1e-9));
    CHECK(relNear(r.stakingMall, 353.71610854541211, 1e-9));
    CHECK(relNear(r.cases[14].mall, 353.71610854541211, 1e-9));
    CHECK(relNear(r.k1, 7.647955258454736, 1e-9));
    CHECK(relNear(r.transpositionModulusMpa, 86214.56857120684, 1e-9));
    CHECK(r.transpositionEivCase == 5);
    CHECK(relNear(r.epReferenceStressMpa, 75.04506464366474, 1e-9));
    CHECK(near(r.epReferenceTempC, 0, 1e-12));
    CHECK(relNear(r.transpositionStressMpa, 249.63464979330303, 1e-8));
    CHECK(r.cases[0].stage == LineStage::Ei);
    CHECK(r.cases[7].stage == LineStage::Ep);
    CHECK(relNear(r.cases[7].modulusMpa, 95000, 1e-12));
    CHECK(!calc.result->cases[0].overUtilization);
    CHECK(countIssues(calc, LineIssueKind::Error) == 0);
}

// T1 i läget New: samma som läget Excel med exakt pi (D10), och nära bilaga A. Med T1
// slår inga andra avvikelser till, så skillnaden mot arket kommer bara av pi.
void checkT1New()
{
    LineInput excelExactPi = makeT1(CalcMode::Excel);
    excelExactPi.constants.pi = std::numbers::pi;
    const LineCalculation e = calculateLine(excelExactPi);
    const LineCalculation n = calculateLine(makeT1(CalcMode::New));
    CHECK(e.result.has_value() && n.result.has_value());
    if (!e.result || !n.result)
        return;
    for (std::size_t i = 0; i < kLoadCaseCount; ++i) {
        const auto& a = e.result->cases[i];
        const auto& b = n.result->cases[i];
        CHECK(relNear(b.stressMpa, a.stressMpa, 1e-12));
        CHECK(relNear(b.iceNPerM, a.iceNPerM, 1e-12));
        CHECK(relNear(b.mall, a.mall, 1e-12));
        CHECK(relNear(b.inclinedMall, a.inclinedMall, 1e-12));
        CHECK(relNear(b.swingAngleDeg, a.swingAngleDeg, 1e-12));
        CHECK(relNear(b.stressMpa, kT1Stress[i], 1e-5));
    }
    CHECK(relNear(n.result->stakingMall, 353.71610854541211, 1e-5));
}

// T1 i läget Excel mot bilaga A och i läget New mot Excel med exakt pi (planen 4.2).
void testT1()
{
    checkT1(CalcMode::Excel);
    checkT1New();
}

// Saknade indata ger Missing och inget resultat, och lindata utan namn räknas.
void testMissing()
{
    LineInput in;
    const LineCalculation calc = calculateLine(in);
    CHECK(!calc.result);
    CHECK(countIssues(calc, LineIssueKind::Missing) == 3);
    CHECK(hasIssue(calc, LineIssueKind::Missing, 0, LineField::Conductor, "Lina eller lindata saknas."));
    CHECK(hasIssue(calc, LineIssueKind::Missing, 0, LineField::NormalSpan, "Normalspannet saknas."));
    CHECK(hasIssue(calc, LineIssueKind::Missing, 0, LineField::Tension, "Uppspänningen saknas."));

    LineInput noName = makeT1(CalcMode::New);
    noName.conductor->name.clear();
    CHECK(calculateLine(noName).result.has_value());
}

// Manuell last: lastfall 4 med I och Qwc ur T1 ger samma spänning, och M, R och T är tomma.
void testManual()
{
    for (CalcMode mode : {CalcMode::Excel, CalcMode::New}) {
        LineInput in = makeT1(mode);
        in.cases[3].manual = ManualLoad{16.743530380610878, 12.725};
        const LineCalculation calc = calculateLine(in);
        CHECK(calc.result.has_value());
        if (!calc.result)
            continue;
        const auto& c4 = calc.result->cases[3];
        CHECK(relNear(c4.stressMpa, 249.63464951586255, 1e-9));
        CHECK(!c4.heightM && !c4.pressureNPerM2 && !c4.iceEquivalentMm);
        CHECK(relNear(c4.iceNPerM, 16.743530380610878, 1e-12));
        CHECK(relNear(c4.windNPerM, 12.725, 1e-12));
    }
}

// Islasten vid vind i T1 (18 mm is) i läget mode. Den är proportionell mot pi, så läget
// New får arkets tal gånger exakt pi / 3,1416 (D10).
double t1IceWind(CalcMode mode)
{
    const double sheet = 16.743530380610878;
    return mode == CalcMode::Excel ? sheet : sheet * std::numbers::pi / 3.1416;
}

// Islasten vid vindstilla i T1 med vindtrycket base i lastfallet som L pekar på (planen 3.2, punkt 7).
double t1CalmIce(double base, CalcMode mode)
{
    return std::sqrt(std::pow(base * 0.5 * 50.9 / 1000, 2) + std::pow(5.34 + t1IceWind(mode), 2)) - 5.34;
}

// L som pekar framåt: lastfall 5 med L = 6 (vindtrycket 890) i båda lägena.
void testForwardRef()
{
    for (CalcMode mode : {CalcMode::Excel, CalcMode::New}) {
        LineInput in = makeT1(mode);
        in.cases[4].windFromCase = 6;
        const LineCalculation calc = calculateLine(in);
        CHECK(calc.result.has_value());
        if (calc.result)
            CHECK(relNear(calc.result->cases[4].iceNPerM, t1CalmIce(890, mode), 1e-9));
    }
}

// Ett T1 med ett ändrat fält, räknat i läget New. Svarar upplysningarna.
template <typename F>
LineCalculation mutated(F mutate, CalcMode mode = CalcMode::New)
{
    LineInput in = makeT1(mode);
    mutate(in);
    return calculateLine(in);
}

// Varje fel i planen 5.5 minst en gång.
void testErrors()
{
    const std::string nonNeg = "Värdet måste vara 0 eller större.";
    const std::string positive = "Värdet måste vara större än 0.";
    const std::string invalid = "Ogiltigt tal.";
    const double nan = std::numeric_limits<double>::quiet_NaN();

    auto c = mutated([](LineInput& in) { in.normalSpanM = 0; });
    CHECK(!c.result);
    CHECK(hasIssue(c, LineIssueKind::Error, 0, LineField::NormalSpan, "Normalspannet måste vara större än 0."));

    c = mutated([](LineInput& in) { in.tensionMpa = 0; });
    CHECK(hasIssue(c, LineIssueKind::Error, 0, LineField::Tension, "Uppspänningen måste vara större än 0."));

    c = mutated([](LineInput& in) { in.constants.transpositionCase = 8; });
    CHECK(hasIssue(c, LineIssueKind::Error, 0, LineField::TranspositionCase,
                   "Lastfallet för transponering måste vara ett av lastfallen 1–7."));

    c = mutated([](LineInput& in) { in.cases[4].windFromCase.reset(); });
    CHECK(hasIssue(c, LineIssueKind::Error, 5, LineField::CaseWindFrom,
                   "Lastfallet har is utan vind och behöver ett lastfall-ID för vindparametrarna."));
    CHECK(!c.result);

    c = mutated([](LineInput& in) { in.cases[4].windFromCase = 16; });
    CHECK(hasIssue(c, LineIssueKind::Error, 5, LineField::CaseWindFrom,
                   "Lastfall-ID för vindparametrarna måste vara 1–15."));

    c = mutated([](LineInput& in) { in.cases[3].iceThicknessMm = -1; });
    CHECK(hasIssue(c, LineIssueKind::Error, 4, LineField::CaseIce, nonNeg));

    c = mutated([](LineInput& in) { in.cases[3].windMagnitudePercent = -1; });
    CHECK(hasIssue(c, LineIssueKind::Error, 4, LineField::CaseMagnitude, nonNeg));

    c = mutated([](LineInput& in) { in.cases[3].refHeightM = -1; });
    CHECK(hasIssue(c, LineIssueKind::Error, 4, LineField::CaseRefHeight, nonNeg));

    c = mutated([](LineInput& in) { in.cases[3].manual = ManualLoad{-1, 2}; });
    CHECK(hasIssue(c, LineIssueKind::Error, 4, LineField::CaseManualIce, nonNeg));
    c = mutated([](LineInput& in) { in.cases[3].manual = ManualLoad{1, -2}; });
    CHECK(hasIssue(c, LineIssueKind::Error, 4, LineField::CaseManualWind, nonNeg));

    c = mutated([nan](LineInput& in) { in.normalSpanM = nan; });
    CHECK(hasIssue(c, LineIssueKind::Error, 0, LineField::NormalSpan, invalid));
    c = mutated([nan](LineInput& in) { in.cases[1].tempC = nan; }, CalcMode::Excel);
    CHECK(hasIssue(c, LineIssueKind::Error, 2, LineField::CaseTemp, invalid));
    c = mutated([nan](LineInput& in) { in.minTempC = nan; });
    CHECK(hasIssue(c, LineIssueKind::Error, 0, LineField::MinTemp, invalid));
    CHECK(countIssues(c, LineIssueKind::Error) == 1);
    c = mutated([nan](LineInput& in) { in.cases[3].pressureGradientNPerM3 = nan; });
    CHECK(hasIssue(c, LineIssueKind::Error, 4, LineField::CaseGradient, invalid));

    c = mutated([](LineInput& in) { in.lineHeightM = -1; });
    CHECK(hasIssue(c, LineIssueKind::Error, 0, LineField::LineHeight, nonNeg));
    c = mutated([](LineInput& in) { in.normalWindNPerM2 = -1; });
    CHECK(hasIssue(c, LineIssueKind::Error, 0, LineField::NormalWind, nonNeg));
    c = mutated([](LineInput& in) { in.constants.gustFactor = -1; });
    CHECK(hasIssue(c, LineIssueKind::Error, 0, LineField::GustFactor, nonNeg));
    c = mutated([](LineInput& in) { in.constants.minIceNPerM = -1; });
    CHECK(hasIssue(c, LineIssueKind::Error, 0, LineField::MinIce, nonNeg));
    c = mutated([](LineInput& in) { in.constants.pi = 0; }, CalcMode::Excel);
    CHECK(hasIssue(c, LineIssueKind::Error, 0, LineField::Pi, positive));
    c = mutated([](LineInput& in) { in.constants.pi = 0; });   // D10: läget New läser inte O10
    CHECK(countIssues(c, LineIssueKind::Error) == 0 && c.result.has_value());
    c = mutated([](LineInput& in) { in.constants.gravity = 0; });
    CHECK(hasIssue(c, LineIssueKind::Error, 0, LineField::Gravity, positive));
    c = mutated([](LineInput& in) { in.constants.maxUtilizationPercent = 0; });
    CHECK(hasIssue(c, LineIssueKind::Error, 0, LineField::MaxUtilization, positive));

    c = mutated([](LineInput& in) { in.conductor->areaMm2 = 0; });
    CHECK(!c.result);
    CHECK(hasIssueStarting(c, LineIssueKind::Error, 0, "Lindata: Area"));

    // Eiv: k så stort att E4 blir negativ i lastfall 4.
    c = mutated([](LineInput& in) { in.conductor->eivConstant = 1e6; });
    CHECK(!c.result);
    CHECK(hasIssueStarting(c, LineIssueKind::Error, 4, "Eiv blir 0 eller negativ vid σ = "));

    // En ekvation utan ändlig lösning: ett normalspann som ger oändligt K1.
    c = mutated([](LineInput& in) { in.normalSpanM = 1e200; });
    CHECK(!c.result);
    CHECK(hasIssue(c, LineIssueKind::Error, 1, LineField::Result, "Tillståndsekvationen saknar lösning."));
}

// Eiv konvergerar inte på 100 varv: söker ett k där iterationen pendlar, och kontrollerar felet.
void testEivNoConvergence()
{
    bool found = false;
    for (double k = 100; k <= 1300 && !found; k += 5) {
        const auto c = mutated([k](LineInput& in) { in.conductor->eivConstant = k; });
        found = hasIssue(c, LineIssueKind::Error, 4, LineField::Result, "Eiv konvergerar inte.");
    }
    CHECK(found);
}

// Brottlast saknas: utnyttjandegraden är tom och varningen finns.
void testNoBreakingLoad()
{
    const auto c = mutated([](LineInput& in) { in.conductor->breakingLoadKn.reset(); });
    CHECK(c.result.has_value());
    CHECK(hasIssue(c, LineIssueKind::Warning, 0, LineField::Conductor,
                   "Linan saknar brottlast. Utnyttjandegraden visas som -."));
    if (c.result)
        for (const auto& lc : c.result->cases)
            CHECK(!lc.utilizationPercent && !lc.overUtilization);
}

// T4:s indata: 157 mm² FeAl PARTRIDGE, aₙ 400, σ₀ 50, min -40 och max 60 ger röd utnyttjandegrad.
void testUtilization()
{
    for (CalcMode mode : {CalcMode::Excel, CalcMode::New}) {
        LineInput in = makeT1(mode);
        Conductor c;
        c.name = "157 mm² FeAl PARTRIDGE";
        c.areaMm2 = 157;
        c.diameterMm = 16.3;
        c.eInitialMpa = 60000;
        c.ePermanentMpa = 76000;
        c.eInitialLimitMpa = 120;
        c.eivConstant = 160;
        c.alphaE6PerC = 19;
        c.creepTempC = 21;
        c.weightNPerM = 5.36;
        c.breakingLoadKn = 50.2;
        in.conductor = c;
        in.normalSpanM = 400;
        in.tensionMpa = 50;
        in.minTempC = -40;
        in.maxTempC = 60;
        in.cases[0].tempC = -40;
        in.cases[1].tempC = -20;
        in.cases[14].tempC = 60;
        const LineCalculation calc = calculateLine(in);
        CHECK(calc.result.has_value());
        if (!calc.result)
            continue;
        const auto& r = *calc.result;
        CHECK(relNear(r.cases[3].stressMpa, 180.9283, 1e-6));
        CHECK(relNear(r.cases[14].stressMpa, 38.0307, mode == CalcMode::Excel ? 1e-5 : 5e-5));
        for (int id : {4, 5, 8, 9})
            CHECK(r.cases[static_cast<std::size_t>(id - 1)].overUtilization);
        CHECK(!r.cases[0].overUtilization);
        CHECK(hasIssue(calc, LineIssueKind::Warning, 0, LineField::Result,
                       "Utnyttjandegraden är över 55,0 % i lastfall 4, 5, 8, 9."));
    }
}

// Temperatur- och vindtryckskällorna i båda lägena (läget Excel ger talen).
void testSources()
{
    LineInput in = makeT1(CalcMode::New);
    in.minTempC = -45;
    in.maxTempC = 55;
    in.shortCircuitTempC = 90;
    in.normalWindNPerM2 = 610;
    in.highWindNPerM2 = 910;
    const TempSource sources[] = {TempSource::Fixed,        TempSource::Min,          TempSource::MinPlus20,
                                  TempSource::Max,          TempSource::ShortCircuit, TempSource::ShortCircuitRise};
    const double expectNew[] = {7, -45, -25, 55, 90, -45 + (90 - 55)};
    for (int i = 0; i < 6; ++i) {
        in.cases[static_cast<std::size_t>(i)].tempSource = sources[i];
        in.cases[static_cast<std::size_t>(i)].tempC = 7;
    }
    for (int i = 0; i < 6; ++i) {
        in.mode = CalcMode::New;
        CHECK(near(caseTempC(in, i + 1), expectNew[i], 1e-12));
        in.mode = CalcMode::Excel;
        CHECK(near(caseTempC(in, i + 1), 7, 1e-12));
    }
    const PressureSource ps[] = {PressureSource::Fixed, PressureSource::Normal, PressureSource::High};
    const double pNew[] = {123, 610, 910};
    for (int i = 0; i < 3; ++i) {
        in.cases[static_cast<std::size_t>(i)].pressureSource = ps[i];
        in.cases[static_cast<std::size_t>(i)].charPressureNPerM2 = 123;
    }
    for (int i = 0; i < 3; ++i) {
        in.mode = CalcMode::New;
        CHECK(near(caseCharPressureNPerM2(in, i + 1), pNew[i], 1e-12));
        in.mode = CalcMode::Excel;
        CHECK(near(caseCharPressureNPerM2(in, i + 1), 123, 1e-12));
    }
    CHECK(near(caseTempC(in, 0), 0, 0) && near(caseTempC(in, 16), 0, 0));
    CHECK(stageOf(1) == LineStage::Ei && stageOf(7) == LineStage::Ei);
    CHECK(stageOf(8) == LineStage::Ep && stageOf(15) == LineStage::Ep);
}

// D1: E4 i transponeringen. T2:s indata, O13 = 0,6, O14 = 1,1 och O17 = 4.
void testModeD1()
{
    auto build = [](CalcMode mode) {
        LineInput in = makeT1(mode);
        in.constants.resonanceFactor = 0.6;
        in.constants.dragFactor = 1.1;
        in.constants.transpositionCase = 4;
        return calculateLine(in);
    };
    const LineCalculation excel = build(CalcMode::Excel);
    const LineCalculation neu = build(CalcMode::New);
    CHECK(excel.result.has_value() && neu.result.has_value());
    if (!excel.result || !neu.result)
        return;
    CHECK(excel.result->transpositionEivCase == 5);
    CHECK(near(excel.result->transpositionModulusMpa, 85875.27, 0.01));
    CHECK(hasIssue(excel, LineIssueKind::Warning, 4, LineField::Result,
                   "Transponeringen använder Eiv från lastfall 5, som Excel, inte från lastfall 4."));
    CHECK(neu.result->transpositionEivCase == 4);
    CHECK(near(std::round(neu.result->transpositionModulusMpa), 85655, 1));
    CHECK(countIssues(neu, LineIssueKind::Warning) == 0);
    CHECK(!near(excel.result->cases[14].stressMpa, neu.result->cases[14].stressMpa, 1e-6));
    CHECK(relNear(excel.result->cases[3].stressMpa, 263.6143, 1e-6));
    CHECK(relNear(excel.result->cases[14].stressMpa, 60.5811, 1e-5));
}

// D2: L som pekar till det andra stadiet.
void testModeD2()
{
    const double fromR6 = t1CalmIce(890, CalcMode::New);
    {
        LineInput in = makeT1(CalcMode::Excel);
        in.cases[8].windFromCase = 6;
        const LineCalculation c = calculateLine(in);
        CHECK(c.result.has_value());
        if (c.result)
            CHECK(relNear(c.result->cases[8].iceNPerM, 20, 1e-12));
        CHECK(hasIssue(c, LineIssueKind::Warning, 9, LineField::CaseWindFrom,
                       "Lastfall-ID 6 pekar till det andra stadiet. Excel läser då lastfall 7."));
    }
    {
        LineInput in = makeT1(CalcMode::New);
        in.cases[8].windFromCase = 6;
        const LineCalculation c = calculateLine(in);
        CHECK(c.result.has_value());
        if (c.result)
            CHECK(relNear(c.result->cases[8].iceNPerM, fromR6, 1e-9));
        CHECK(countIssues(c, LineIssueKind::Warning) == 0);
    }
    {
        LineInput in = makeT1(CalcMode::Excel);
        in.cases[4].windFromCase = 9;
        const LineCalculation c = calculateLine(in);
        CHECK(c.result.has_value());
        if (c.result)
            CHECK(relNear(c.result->cases[4].iceNPerM, 20.147407460770967, 1e-9));
        LineInput inNew = makeT1(CalcMode::New);
        inNew.cases[4].windFromCase = 9;
        const LineCalculation n = calculateLine(inNew);
        CHECK(n.result.has_value());
        if (n.result)
            CHECK(relNear(n.result->cases[4].iceNPerM, 20, 1e-12));
    }
    {
        // Den tomma raden: L = 8 i Ei (rad 30) och L = 7 i Ep.
        LineInput in = makeT1(CalcMode::Excel);
        in.cases[4].windFromCase = 8;
        const LineCalculation c = calculateLine(in);
        CHECK(hasIssue(c, LineIssueKind::Warning, 5, LineField::CaseWindFrom,
                       "Lastfall-ID 8 pekar till det andra stadiet. Excel läser då en tom rad, och "
                       "vindtrycket blir 0."));
    }
}

// D3: L som pekar på ett lastfall med manuell last.
void testModeD3()
{
    LineInput in = makeT1(CalcMode::Excel);
    in.cases[3].manual = ManualLoad{16.743530380610878, 12.725};
    const LineCalculation e = calculateLine(in);
    CHECK(e.result.has_value());
    if (e.result)
        CHECK(relNear(e.result->cases[4].iceNPerM, 20, 1e-12));
    CHECK(hasIssue(e, LineIssueKind::Warning, 5, LineField::CaseWindFrom,
                   "Lastfall 4 har manuell last. Excel läser då inget vindtryck, och vindtrycket blir 0."));

    in.mode = CalcMode::New;
    const LineCalculation n = calculateLine(in);
    CHECK(n.result.has_value());
    if (n.result)
        CHECK(relNear(n.result->cases[4].iceNPerM, t1CalmIce(500, CalcMode::New), 1e-9));
    CHECK(countIssues(n, LineIssueKind::Warning) == 0);
}

// D4: x0 över σp i transponeringen. T1:s lina med σ0 = 170 och T0 = 20.
void testModeD4()
{
    auto build = [](CalcMode mode) {
        LineInput in = makeT1(mode);
        in.tensionMpa = 170;
        in.refTempC = 20;
        return calculateLine(in);
    };
    const LineCalculation n = build(CalcMode::New);
    CHECK(n.result.has_value());
    CHECK(hasIssueStarting(n, LineIssueKind::Warning, 5, "Spänningen utan tilläggslast vid transponeringstemperaturen blir "));
    if (n.result) {
        const LineResult& r = *n.result;
        CHECK(r.unloadedTranspositionStressMpa > 180);
        CHECK(near(r.transpositionModulusMpa, 89000 - 40 * (r.unloadedTranspositionStressMpa - 180), 10));
        CHECK(r.transpositionEivCase == 0);
    }
    const LineCalculation e = build(CalcMode::Excel);
    CHECK(hasIssueStarting(e, LineIssueKind::Warning, 5, "Spänningen utan tilläggslast vid transponeringstemperaturen blir "));
    if (e.result && n.result)
        CHECK(!near(e.result->cases[14].stressMpa, n.result->cases[14].stressMpa, 1e-9) ||
              !near(e.result->transpositionModulusMpa, n.result->transpositionModulusMpa, 1e-9));
    else
        CHECK(hasIssueStarting(e, LineIssueKind::Error, 5, "Eiv") ||
              hasIssue(e, LineIssueKind::Error, 5, LineField::Result, "Tillståndsekvationen saknar lösning."));
}

// D5: Ep = Ei. T5:s lina (106mm²  OPGW 78N94z), aₙ 350, σ0 420, min -40 och max 60.
void testModeD5()
{
    auto build = [](CalcMode mode) {
        LineInput in = makeT1(mode);
        Conductor c;
        c.name = "106mm²  OPGW 78N94z";
        c.areaMm2 = 106;
        c.diameterMm = 17.8;
        c.eInitialMpa = 152000;
        c.ePermanentMpa = 152000;
        c.eInitialLimitMpa = 500;
        c.eivConstant = 75;
        c.alphaE6PerC = 14.6;
        c.creepTempC = 20.5;
        c.weightNPerM = 9.17;
        c.breakingLoadKn = 132;
        in.conductor = c;
        in.normalSpanM = 350;
        in.tensionMpa = 420;
        in.minTempC = -40;
        in.maxTempC = 60;
        in.cases[0].tempC = -40;
        in.cases[1].tempC = -20;
        in.cases[14].tempC = 60;
        return calculateLine(in);
    };
    const LineCalculation e = build(CalcMode::Excel);
    const LineCalculation n = build(CalcMode::New);
    CHECK(e.result.has_value() && n.result.has_value());
    if (!e.result || !n.result)
        return;
    CHECK(e.result->cases[7].modulusMpa < 152000);
    CHECK(hasIssueStarting(e, LineIssueKind::Warning, 0, "Ep = Ei, och Excel räknar då Eiv också i Ep-stadiet (lastfall 8, 9"));
    for (int id = 8; id <= 15; ++id)
        CHECK(relNear(n.result->cases[static_cast<std::size_t>(id - 1)].modulusMpa, 152000, 1e-12));
    CHECK(relNear(e.result->cases[3].stressMpa, 575.3221, 1e-6));
    CHECK(relNear(e.result->cases[14].stressMpa, 277.7226, 1e-6));
}

// D6: ACOS vid y = 1 och y = -1.
void testModeD6()
{
    CHECK(near(vbaAcos(1, 3.1416, CalcMode::Excel), 1.5708, 1e-12));
    CHECK(near(vbaAcos(-1, 3.1416, CalcMode::Excel), 1.5708, 1e-12));
    CHECK(near(vbaAcos(1, 3.1416, CalcMode::New), 0, 1e-12));
    CHECK(near(vbaAcos(-1, 3.1416, CalcMode::New), 3.1416, 1e-12));
    CHECK(vbaAcos(0.5, 3.1416, CalcMode::Excel) == vbaAcos(0.5, 3.1416, CalcMode::New));
    CHECK(near(vbaAcos(0.5, std::numbers::pi, CalcMode::New), std::acos(0.5), 1e-12));
    CHECK(std::isnan(vbaAcos(1.5, 3.1416, CalcMode::New)));
}

} // namespace

// Kör alla tester och svarar 1 om något misslyckades.
int main()
{
    testCubic();
    testT1();
    testMissing();
    testManual();
    testForwardRef();
    testErrors();
    testEivNoConvergence();
    testNoBreakingLoad();
    testUtilization();
    testSources();
    testModeD1();
    testModeD2();
    testModeD3();
    testModeD4();
    testModeD5();
    testModeD6();
    if (g_failures != 0) {
        std::cerr << g_failures << " kontroller misslyckades" << std::endl;
        return 1;
    }
    std::cout << "linecalc: alla kontroller godkända" << std::endl;
    return 0;
}
