#include "lineCalc/line_calc.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <numbers>

#include "tableText/table_text.h"

namespace kalkyl::linecalc {

namespace {

constexpr double kNaN = std::numeric_limits<double>::quiet_NaN();

// Excels egenheter som flaggor, en per avvikelse i planen 4.2. rulesFor(CalcMode::New)
// ger alla false. När läget Excel tas bort försvinner strukturen och grenarna den styr.
struct ModeRules {
    bool excelWindRows = false;          // D2: L läses som arkets rad
    bool manualRefZero = false;          // D3: L till manuell last ger R = 0
    bool lastEivModulus = false;         // D1: E4 från sista Ei-lastfallet med Eiv
    bool emptyTranspositionEiv = false;  // D4: Eiv med temperatur 0 och last 0
    bool eivWhenEpEqualsEi = false;      // D5: Eiv också i Ep när Ep == Ei
    bool sheetPi = false;                // D10: arkets pi (O10) i stället för exakt pi
};

// Reglerna för ett läge. Läget New har inga av Excels egenheter.
ModeRules rulesFor(CalcMode mode)
{
    ModeRules r;
    if (mode == CalcMode::Excel) {
        r.excelWindRows = true;
        r.manualRefZero = true;
        r.lastEivModulus = true;
        r.emptyTranspositionEiv = true;
        r.eivWhenEpEqualsEi = true;
        r.sheetPi = true;
    }
    return r;
}

// VBA:s Sgn: 1, 0 eller -1.
double sgn(double v)
{
    return v > 0 ? 1.0 : (v < 0 ? -1.0 : 0.0);
}

// Talet med en decimal, för texterna till användaren.
std::string num(double v)
{
    return tabletext::formatDecimal(v, 1);
}

// Lastfallens ID som text, skilda med komma och mellanslag.
std::string joinIds(const std::vector<int>& ids)
{
    std::string s;
    for (std::size_t i = 0; i < ids.size(); ++i) {
        if (i > 0)
            s += ", ";
        s += std::to_string(ids[i]);
    }
    return s;
}

// En upplysning med slag, lastfall, fält och text.
LineIssue makeIssue(LineIssueKind kind, int caseId, LineField field, std::string message)
{
    LineIssue issue;
    issue.kind = kind;
    issue.caseId = caseId;
    issue.field = field;
    issue.message = std::move(message);
    return issue;
}

// Sant om upplysningen hindrar ett resultat.
bool blocks(const LineIssue& issue)
{
    return issue.kind != LineIssueKind::Warning;
}

// Lastfallets höjd i räkningen: linhöjden, men href om linhöjden är lägre (planen 3.2, punkt 1).
double heightFor(const LineInput& in, const LoadCase& lc)
{
    return in.lineHeightM < lc.refHeightM ? lc.refHeightM : in.lineHeightM;
}

// Radens lastfall i läget Excel när L läses som arkets rad (planen 4.2, D2): rad 22 + L i
// Ei och 23 + L i Ep. Svarar 0 för den tomma raden 30.
int excelRowToId(LineStage stage, int windFrom)
{
    const int row = (stage == LineStage::Ei ? 22 : 23) + windFrom;
    if (row >= 23 && row <= 29)
        return row - 22;
    if (row >= 31 && row <= 38)
        return row - 23;
    return 0;
}

// Vindtrycket R för ett lastfall ur dess egna indata, före faktorerna (planen 3.2, punkt 2).
double rawPressure(const LineInput& in, int id)
{
    const LoadCase& lc = in.cases[static_cast<std::size_t>(id - 1)];
    const double h = heightFor(in, lc);
    return caseCharPressureNPerM2(in, id) + lc.pressureGradientNPerM3 * (h - lc.refHeightM);
}

// Sant om alla tal som behövs för reducerade trycket q i lastfallet är ändliga.
bool pressureInputsFinite(const LineInput& in, int id)
{
    const LoadCase& lc = in.cases[static_cast<std::size_t>(id - 1)];
    const LineConstants& k = in.constants;
    return std::isfinite(in.lineHeightM) && std::isfinite(k.gustFactor) &&
           std::isfinite(k.resonanceFactor) && std::isfinite(k.dragFactor) &&
           std::isfinite(lc.refHeightM) && std::isfinite(lc.pressureGradientNPerM3) &&
           std::isfinite(caseCharPressureNPerM2(in, id));
}

// Kontrollerar ett tal som ska vara ändligt. Svarar true när det är det, annars läggs
// upplysningen "Ogiltigt tal." till.
bool requireFinite(std::vector<LineIssue>& issues, double v, int caseId, LineField field)
{
    if (std::isfinite(v))
        return true;
    issues.push_back(makeIssue(LineIssueKind::Error, caseId, field, "Ogiltigt tal."));
    return false;
}

// Kontrollerar ett tal som ska vara ändligt och 0 eller större.
void requireNonNegative(std::vector<LineIssue>& issues, double v, int caseId, LineField field)
{
    if (!requireFinite(issues, v, caseId, field))
        return;
    if (v < 0)
        issues.push_back(makeIssue(LineIssueKind::Error, caseId, field,
                                   "Värdet måste vara 0 eller större."));
}

// Kontrollerar ett tal som ska vara ändligt och större än 0.
void requirePositive(std::vector<LineIssue>& issues, double v, int caseId, LineField field)
{
    if (!requireFinite(issues, v, caseId, field))
        return;
    if (!(v > 0))
        issues.push_back(makeIssue(LineIssueKind::Error, caseId, field,
                                   "Värdet måste vara större än 0."));
}

// Lastfallets egna kontroller: temperatur, vindindata, manuell last och L (planen 5.5).
// Källornas globala fält kontrolleras för sig, så de rapporteras inte en gång per lastfall.
void validateCase(const LineInput& in, int id, std::vector<LineIssue>& issues)
{
    const LoadCase& lc = in.cases[static_cast<std::size_t>(id - 1)];
    const bool tempSourcesOk = std::isfinite(in.minTempC) && std::isfinite(in.maxTempC) &&
                               std::isfinite(in.shortCircuitTempC);
    const double temp = caseTempC(in, id);
    if (!std::isfinite(temp) && tempSourcesOk)
        issues.push_back(makeIssue(LineIssueKind::Error, id, LineField::CaseTemp, "Ogiltigt tal."));

    // Ett manuellt lastfall behöver sitt vindindata bara när ett annat lastfall pekar hit med L.
    if (lc.manual) {
        requireNonNegative(issues, lc.manual->iceNPerM, id, LineField::CaseManualIce);
        requireNonNegative(issues, lc.manual->windNPerM, id, LineField::CaseManualWind);
        bool target = false;
        for (const LoadCase& other : in.cases)
            target = target || (other.windFromCase && *other.windFromCase == id);
        if (!target)
            return;
    }

    requireNonNegative(issues, lc.windMagnitudePercent, id, LineField::CaseMagnitude);
    const double pressure = caseCharPressureNPerM2(in, id);
    const bool windGlobalsOk = std::isfinite(in.normalWindNPerM2) && std::isfinite(in.highWindNPerM2) &&
                               in.normalWindNPerM2 >= 0 && in.highWindNPerM2 >= 0;
    if (!std::isfinite(pressure)) {
        if (std::isfinite(in.normalWindNPerM2) && std::isfinite(in.highWindNPerM2))
            issues.push_back(makeIssue(LineIssueKind::Error, id, LineField::CasePressure,
                                       "Ogiltigt tal."));
    } else if (pressure < 0 && windGlobalsOk) {
        issues.push_back(makeIssue(LineIssueKind::Error, id, LineField::CasePressure,
                                   "Värdet måste vara 0 eller större."));
    }
    requireNonNegative(issues, lc.refHeightM, id, LineField::CaseRefHeight);
    requireFinite(issues, lc.pressureGradientNPerM3, id, LineField::CaseGradient);
    requireNonNegative(issues, lc.iceThicknessMm, id, LineField::CaseIce);
    if (lc.windFromCase && (*lc.windFromCase < 1 || *lc.windFromCase > kLoadCaseCount))
        issues.push_back(makeIssue(LineIssueKind::Error, id, LineField::CaseWindFrom,
                                   "Lastfall-ID för vindparametrarna måste vara 1–15."));
}

// Missing och valideringens fel enligt planen 5.5, alla samlade innan något räknas.
// Svarar true när inget Missing eller Error hittades. Varningen om brottlasten ingår.
bool validate(const LineInput& in, std::vector<LineIssue>& issues)
{
    const LineConstants& k = in.constants;
    if (!in.conductor) {
        issues.push_back(makeIssue(LineIssueKind::Missing, 0, LineField::Conductor,
                                   "Lina eller lindata saknas."));
    } else {
        for (const auto& ci : conductor::validateConductor(*in.conductor)) {
            if (ci.field == conductor::ConductorField::Name)
                continue;
            issues.push_back(makeIssue(LineIssueKind::Error, 0, LineField::Conductor,
                                       "Lindata: " + ci.message));
        }
        if (!in.conductor->breakingLoadKn)
            issues.push_back(makeIssue(LineIssueKind::Warning, 0, LineField::Conductor,
                                       "Linan saknar brottlast. Utnyttjandegraden visas som -."));
    }
    if (!in.normalSpanM)
        issues.push_back(makeIssue(LineIssueKind::Missing, 0, LineField::NormalSpan,
                                   "Normalspannet saknas."));
    else if (requireFinite(issues, *in.normalSpanM, 0, LineField::NormalSpan) && !(*in.normalSpanM > 0))
        issues.push_back(makeIssue(LineIssueKind::Error, 0, LineField::NormalSpan,
                                   "Normalspannet måste vara större än 0."));
    if (!in.tensionMpa)
        issues.push_back(makeIssue(LineIssueKind::Missing, 0, LineField::Tension,
                                   "Uppspänningen saknas."));
    else if (requireFinite(issues, *in.tensionMpa, 0, LineField::Tension) && !(*in.tensionMpa > 0))
        issues.push_back(makeIssue(LineIssueKind::Error, 0, LineField::Tension,
                                   "Uppspänningen måste vara större än 0."));

    requireFinite(issues, in.refTempC, 0, LineField::RefTemp);
    requireFinite(issues, in.minTempC, 0, LineField::MinTemp);
    requireFinite(issues, in.maxTempC, 0, LineField::MaxTemp);
    requireFinite(issues, in.shortCircuitTempC, 0, LineField::ShortCircuitTemp);
    requireNonNegative(issues, in.lineHeightM, 0, LineField::LineHeight);
    requireNonNegative(issues, in.normalWindNPerM2, 0, LineField::NormalWind);
    requireNonNegative(issues, in.highWindNPerM2, 0, LineField::HighWind);
    requireNonNegative(issues, k.iceDensityKgM3, 0, LineField::IceDensity);
    if (in.mode == CalcMode::Excel)   // D10: läget New använder inte O10
        requirePositive(issues, k.pi, 0, LineField::Pi);
    requirePositive(issues, k.gravity, 0, LineField::Gravity);
    requireNonNegative(issues, k.gustFactor, 0, LineField::GustFactor);
    requireNonNegative(issues, k.resonanceFactor, 0, LineField::ResonanceFactor);
    requireNonNegative(issues, k.dragFactor, 0, LineField::DragFactor);
    requireNonNegative(issues, k.minIceNPerM, 0, LineField::MinIce);
    requirePositive(issues, k.maxUtilizationPercent, 0, LineField::MaxUtilization);
    if (k.transpositionCase < 1 || k.transpositionCase > kEiCaseCount)
        issues.push_back(makeIssue(LineIssueKind::Error, 0, LineField::TranspositionCase,
                                   "Lastfallet för transponering måste vara ett av lastfallen 1–7."));

    for (int id = 1; id <= kLoadCaseCount; ++id)
        validateCase(in, id, issues);

    // L måste finnas där lastfallet har is utan vind. Kontrolleras när talen är ändliga.
    for (int id = 1; id <= kLoadCaseCount; ++id) {
        const LoadCase& lc = in.cases[static_cast<std::size_t>(id - 1)];
        if (lc.manual || lc.windFromCase || !pressureInputsFinite(in, id))
            continue;
        const double q = rawPressure(in, id) * k.gustFactor * k.resonanceFactor * k.dragFactor;
        if (q == 0 && lc.iceThicknessMm != 0 && std::isfinite(lc.iceThicknessMm))
            issues.push_back(makeIssue(LineIssueKind::Error, id, LineField::CaseWindFrom,
                                       "Lastfallet har is utan vind och behöver ett lastfall-ID för "
                                       "vindparametrarna."));
    }

    return std::none_of(issues.begin(), issues.end(), blocks);
}

// Rotstatus från Eiv.
enum class EivStatus { Ok, NoConvergence, BadModulus, NoRoot };

// Resultatet av Eiv: spänningen och E-modulen E4, eller varför det misslyckades.
struct EivOutcome {
    EivStatus status = EivStatus::Ok;
    double stressMpa = 0;
    double modulusMpa = 0;
    double failStressMpa = 0;   // spänningen när E4 blev 0 eller negativ
};

// Stadiets referens: spänning och temperatur vid referenstillståndet och stadiets E-modul.
struct StageRef {
    double sigma = 0;
    double tempC = 0;
    double modulus = 0;
};

// Räknar en linberäkning. Lindata och indata läses in i konstruktorn (planen 3.7, punkt 1)
// och run() utför lasterna, Ei-stadiet, transponeringen och Ep-stadiet.
class Calculator {
public:
    Calculator(const LineInput& in, std::vector<LineIssue>& issues)
        : in_(in), issues_(issues), rules_(rulesFor(in.mode))
    {
        const conductor::Conductor& c = *in.conductor;
        area_ = c.areaMm2;
        dia_ = c.diameterMm;
        ei_ = c.eInitialMpa;
        ep_ = c.ePermanentMpa;
        sigmap_ = c.eInitialLimitMpa;
        eivK_ = c.eivConstant;
        alpha_ = c.alphaE6PerC * std::pow(10.0, -6);
        tp_ = c.creepTempC;
        qe_ = c.weightNPerM;
        fu_ = c.breakingLoadKn ? *c.breakingLoadKn * 1000 : 0.0;
        an_ = *in.normalSpanM;
        sigma0_ = *in.tensionMpa;
        t0_ = in.refTempC;
        pi_ = rules_.sheetPi ? in.constants.pi : std::numbers::pi;   // D10
        grav_ = in.constants.gravity;
        isdens_ = (in.constants.iceDensityKgM3 / 1000) * in.constants.gravity;
    }

    // Hela räkningen. Svarar false när ett fel hindrar resultatet (felet ligger i issues).
    bool run(LineResult& out);

private:
    const LineInput& in_;
    std::vector<LineIssue>& issues_;
    ModeRules rules_;
    LineResult res_;

    double area_ = 0, dia_ = 0, ei_ = 0, ep_ = 0, sigmap_ = 0, eivK_ = 0, alpha_ = 0, tp_ = 0;
    double qe_ = 0, fu_ = 0, an_ = 0, sigma0_ = 0, t0_ = 0, pi_ = 0, grav_ = 0, isdens_ = 0;
    double k1_ = 0;

    std::array<double, kLoadCaseCount> temp_{};      // G
    std::array<double, kLoadCaseCount> rawR_{};      // R ur lastfallets egna indata
    std::array<double, kLoadCaseCount> ice_{};       // U
    std::array<double, kLoadCaseCount> wind_{};      // V
    std::array<double, kLoadCaseCount> resultant_{}; // Q
    std::array<std::optional<double>, kLoadCaseCount> eivModulus_;  // E4 när Eiv räknades
    int lastEivCase_ = 0;           // sista Ei-lastfallet med Eiv (D1)
    double lastEivModulus_ = 0;     // E4 från det lastfallet
    std::vector<int> epEivCases_;   // Ep-lastfall där Eiv räknades (D5)

    void computeLoads();
    double solveRoot(double c, double d, double sigmaRef) const;
    double stateC(const StageRef& ref, double tempC, double modulus) const;
    EivOutcome runEiv(double stress, const StageRef& ref, double tempC, double resultant) const;
    bool fail(int caseId, const std::string& message);
    bool failEiv(int caseId, const EivOutcome& outcome);
    bool solveStage(int first, int last, const StageRef& ref, bool eivEnabled);
    bool transpose(StageRef& epRef);
    void fillResult(int id, double stress, double modulus);
    void finish(LineResult& out);
};

// Lägger till ett fel i ett lastfall (eller hela beräkningen) och svarar false.
bool Calculator::fail(int caseId, const std::string& message)
{
    issues_.push_back(makeIssue(LineIssueKind::Error, caseId, LineField::Result, message));
    return false;
}

// Översätter ett misslyckat Eiv till felet i planen 5.5.
bool Calculator::failEiv(int caseId, const EivOutcome& outcome)
{
    switch (outcome.status) {
    case EivStatus::NoConvergence:
        return fail(caseId, "Eiv konvergerar inte.");
    case EivStatus::BadModulus:
        return fail(caseId, "Eiv blir 0 eller negativ vid σ = " + num(outcome.failStressMpa) + " MPa.");
    default:
        return fail(caseId, "Tillståndsekvationen saknar lösning.");
    }
}

// Laster per lastfall (planen 3.2): vindtryck R i ett första varv för alla lastfall och
// sedan vindlast, islast, motsvarande istjocklek och höjd i ett andra varv. Resultatet
// blir som efter Excels andra omräkning, eftersom L får peka framåt.
void Calculator::computeLoads()
{
    const LineConstants& k = in_.constants;
    for (int id = 1; id <= kLoadCaseCount; ++id) {
        const auto i = static_cast<std::size_t>(id - 1);
        temp_[i] = caseTempC(in_, id);
        rawR_[i] = rawPressure(in_, id);
    }

    for (int id = 1; id <= kLoadCaseCount; ++id) {
        const auto i = static_cast<std::size_t>(id - 1);
        const LoadCase& lc = in_.cases[i];
        LoadCaseResult& r = res_.cases[i];
        r.id = id;
        r.stage = stageOf(id);
        r.tempC = temp_[i];
        r.charPressureNPerM2 = caseCharPressureNPerM2(in_, id);

        if (lc.manual) {
            ice_[i] = lc.manual->iceNPerM;
            wind_[i] = lc.manual->windNPerM;
            r.iceNPerM = ice_[i];
            r.windNPerM = wind_[i];
            continue;
        }

        const double h = heightFor(in_, lc);
        const double rr = rawR_[i];
        const double q = rr * k.gustFactor * k.resonanceFactor * k.dragFactor;
        const double dIce = dia_ + lc.iceThicknessMm * 2;
        const double windLoad = q * (lc.windMagnitudePercent / 100) * dIce / 1000;
        const double iceWind = isdens_ * (dIce * dIce - dia_ * dia_) * pi_ / 4 / 1000;
        const bool calm = q == 0 && lc.iceThicknessMm != 0;

        // Vindtrycket i basen för isen vid vindstilla: R i lastfallet L, annars lastfallets eget.
        double base = rr;
        if (calm) {
            double refR = 0;
            const int windFrom = lc.windFromCase.value_or(1);
            int target = windFrom;
            if (rules_.excelWindRows) {
                // D2: Excel läser arkets rad 22 + L (Ei) eller 23 + L (Ep)
                target = excelRowToId(stageOf(id), windFrom);
                if (stageOf(windFrom) != stageOf(id)) {
                    std::string msg = "Lastfall-ID " + std::to_string(windFrom) +
                                      " pekar till det andra stadiet. Excel läser då ";
                    msg += target == 0 ? "en tom rad, och vindtrycket blir 0."
                                       : "lastfall " + std::to_string(target) + ".";
                    issues_.push_back(makeIssue(LineIssueKind::Warning, id, LineField::CaseWindFrom, msg));
                }
            }
            if (target != 0) {
                if (rules_.manualRefZero && in_.cases[static_cast<std::size_t>(target - 1)].manual) {
                    // D3: Excel har tömt R i ett lastfall med manuell last
                    issues_.push_back(makeIssue(
                        LineIssueKind::Warning, id, LineField::CaseWindFrom,
                        "Lastfall " + std::to_string(target) +
                            " har manuell last. Excel läser då inget vindtryck, och vindtrycket blir 0."));
                } else {
                    refR = rawR_[static_cast<std::size_t>(target - 1)];
                }
            }
            base = refR + lc.pressureGradientNPerM3 * (h - lc.refHeightM);
        }
        const double halfWind = base * k.resonanceFactor * 1 * dIce / 1000;
        double calmIce = std::sqrt(halfWind * halfWind + (qe_ + iceWind) * (qe_ + iceWind)) - qe_;

        double u = iceWind;
        if (calm) {
            if (calmIce < k.minIceNPerM)
                calmIce = k.minIceNPerM;
            u = calmIce;
        } else if (lc.iceThicknessMm == 0) {
            u = 0;
        }

        // Motsvarande istjocklek vid vindstilla (kolumn T), också på rader med vind och is.
        double tIce = 0;
        if (lc.iceThicknessMm != 0)
            tIce = ((std::sqrt((calmIce / grav_) * 4 / (1 * pi_ * (isdens_ / grav_) * 1000) +
                               (dia_ / 1000) * (dia_ / 1000)) * 1000) - dia_) / 2;

        ice_[i] = u;
        wind_[i] = windLoad;
        r.heightM = lc.windMagnitudePercent == 0 ? 0.0 : in_.lineHeightM;
        r.pressureNPerM2 = rr;
        r.iceEquivalentMm = tIce;
        r.iceNPerM = u;
        r.windNPerM = windLoad;
    }
}

// Roten till tillståndsekvationen i stadiet med referensen ref (planen 3.3).
double Calculator::solveRoot(double c, double d, double sigmaRef) const
{
    return solveStateCubic(c, d, pi_, sigmaRef, in_.mode);
}

// Koefficienten c i tillståndsekvationen för temperaturen tempC och E-modulen modulus.
double Calculator::stateC(const StageRef& ref, double tempC, double modulus) const
{
    return k1_ / (ref.sigma * ref.sigma) * modulus - ref.sigma + alpha_ * (tempC - ref.tempC) * modulus;
}

// Eiv enligt planen 3.4 med starten stress. Högst 100 varv. tempC och resultant är
// temperaturen och lasten som varje varv räknas med (planen 4.2, D4 för andra värden).
EivOutcome Calculator::runEiv(double stress, const StageRef& ref, double tempC, double resultant) const
{
    EivOutcome out;
    double x = stress;
    double e3 = ei_;
    double e4 = ei_;
    const double ratio = resultant / qe_;
    for (int n = 0;; ++n) {
        e4 = ei_ - eivK_ * (x - sigmap_);
        if (!(e4 > 0)) {
            out.status = EivStatus::BadModulus;
            out.failStressMpa = x;
            return out;
        }
        if (std::fabs(e4 - e3) < 10)
            break;
        if (n >= 100) {
            out.status = EivStatus::NoConvergence;
            return out;
        }
        e3 = e4;
        x = solveRoot(stateC(ref, tempC, e4), k1_ * e4 * (ratio * ratio), ref.sigma);
        if (!std::isfinite(x)) {
            out.status = EivStatus::NoRoot;
            return out;
        }
    }
    x = solveRoot(stateC(ref, tempC, e4), k1_ * e4 * (ratio * ratio), ref.sigma);
    if (!std::isfinite(x)) {
        out.status = EivStatus::NoRoot;
        return out;
    }
    out.stressMpa = x;
    out.modulusMpa = e4;
    return out;
}

// Räknar lastfallen first-last i ett stadium (planen 3.3): resulterande last, rot och Eiv när
// spänningen går över σp och eivEnabled är sant. Stannar vid första felet.
bool Calculator::solveStage(int first, int last, const StageRef& ref, bool eivEnabled)
{
    for (int id = first; id <= last; ++id) {
        const auto i = static_cast<std::size_t>(id - 1);
        const double v = wind_[i];
        const double q = std::sqrt(v * v + (qe_ + ice_[i]) * (qe_ + ice_[i]));
        resultant_[i] = q;
        const double ratio = q / qe_;
        double e = ref.modulus;
        double x = solveRoot(stateC(ref, temp_[i], e), k1_ * e * (ratio * ratio), ref.sigma);
        if (!std::isfinite(x) || !(x > 0))
            return fail(id, "Tillståndsekvationen saknar lösning.");
        if (eivEnabled && x > sigmap_) {
            const EivOutcome eiv = runEiv(x, ref, temp_[i], q);
            if (eiv.status != EivStatus::Ok)
                return failEiv(id, eiv);
            x = eiv.stressMpa;
            e = eiv.modulusMpa;
            eivModulus_[i] = e;
            if (stageOf(id) == LineStage::Ei) {
                lastEivCase_ = id;
                lastEivModulus_ = e;
            } else {
                epEivCases_.push_back(id);
            }
            if (!std::isfinite(x) || !(x > 0))
                return fail(id, "Tillståndsekvationen saknar lösning.");
        }
        fillResult(id, x, e);
    }
    return true;
}

// Transponeringen till Ep-stadiet (planen 3.5). E4 väljs enligt D1 och x0 > σp enligt D4.
// Fyller i mellanvärdena i res_ och svarar med Ep-stadiets referens i epRef.
bool Calculator::transpose(StageRef& epRef)
{
    const int tr = in_.constants.transpositionCase;
    const auto ti = static_cast<std::size_t>(tr - 1);
    const double tTr = temp_[ti];
    const double qTr = resultant_[ti];
    const StageRef ref0{sigma0_, t0_, ei_};

    double x0 = solveRoot(stateC(ref0, tTr, ei_), k1_ * ei_, sigma0_);
    if (!std::isfinite(x0) || !(x0 > 0))
        return fail(tr, "Tillståndsekvationen saknar lösning.");

    double e4 = ei_;
    int eivCase = 0;
    if (rules_.lastEivModulus) {
        // D1: Excel tar E4 från det sista Ei-lastfallet med Eiv
        if (lastEivCase_ != 0) {
            e4 = lastEivModulus_;
            eivCase = lastEivCase_;
        }
    } else if (eivModulus_[ti]) {
        e4 = *eivModulus_[ti];
        eivCase = tr;
    }

    if (x0 > sigmap_) {
        EivOutcome eiv;
        if (rules_.emptyTranspositionEiv) {
            // D4: Excel räknar Eiv med temperatur 0 och last 0
            eiv = runEiv(x0, ref0, 0.0, 0.0);
            issues_.push_back(makeIssue(
                LineIssueKind::Warning, tr, LineField::Result,
                "Spänningen utan tilläggslast vid transponeringstemperaturen blir " + num(x0) +
                    " MPa, över σp. Excel räknar då Eiv med tomma värden (temperatur 0 och last 0), "
                    "och resultatet beror på avrundningen."));
        } else {
            eiv = runEiv(x0, ref0, tTr, qe_);
            issues_.push_back(makeIssue(
                LineIssueKind::Warning, tr, LineField::Result,
                "Spänningen utan tilläggslast vid transponeringstemperaturen blir " + num(x0) +
                    " MPa, över σp. Eiv räknas för det tillståndet."));
        }
        if (eiv.status != EivStatus::Ok)
            return failEiv(tr, eiv);
        x0 = eiv.stressMpa;
        e4 = eiv.modulusMpa;
        eivCase = 0;
        if (!std::isfinite(x0) || !(x0 > 0))
            return fail(tr, "Tillståndsekvationen saknar lösning.");
    } else if (rules_.lastEivModulus && eivCase != 0 && eivCase != tr) {
        issues_.push_back(makeIssue(
            LineIssueKind::Warning, tr, LineField::Result,
            "Transponeringen använder Eiv från lastfall " + std::to_string(eivCase) +
                ", som Excel, inte från lastfall " + std::to_string(tr) + "."));
    }

    const double relLength = k1_ / x0 / x0 - x0 / e4;
    const double y = solveRoot(relLength * e4 * area_ / qTr / an_, e4 * area_ / 24 / qTr / an_, sigma0_);
    if (!std::isfinite(y) || !(y > 0))
        return fail(tr, "Tillståndsekvationen saknar lösning.");
    const double k2 = (ep_ - e4) * qTr * an_ / ep_ / e4 / area_;
    const double sigmaP0 =
        solveRoot(ep_ * (relLength + alpha_ * tp_ + k2 * y), k1_ * ep_, sigma0_);
    if (!std::isfinite(sigmaP0) || !(sigmaP0 > 0))
        return fail(tr, "Tillståndsekvationen saknar lösning.");

    res_.unloadedTranspositionStressMpa = x0;
    res_.transpositionEivCase = eivCase;
    res_.transpositionModulusMpa = e4;
    res_.transpositionStressMpa = y * qTr * an_ / area_;
    res_.epReferenceStressMpa = sigmaP0;
    res_.epReferenceTempC = tTr;
    epRef = StageRef{sigmaP0, tTr, ep_};
    return true;
}

// Resultatkolumnerna för ett lastfall (planen 3.6) ur spänningen och E-modulen.
void Calculator::fillResult(int id, double stress, double modulus)
{
    const auto i = static_cast<std::size_t>(id - 1);
    LoadCaseResult& r = res_.cases[i];
    r.resultantNPerM = resultant_[i];
    r.modulusMpa = modulus;
    r.sagM = (qe_ + ice_[i]) * an_ * an_ / (8 * stress * area_);
    r.stressMpa = stress;
    r.forceN = stress * area_;
    r.mall = (2000 / an_) * (2000 / an_) * r.sagM;
    if (fu_ != 0) {
        const double percent = r.forceN / fu_ * 100;
        r.utilizationPercent = percent;
        r.overUtilization = percent > in_.constants.maxUtilizationPercent;
    }
    r.inclinedSagM = resultant_[i] * an_ * an_ / (8 * stress * area_);
    r.swingAngleDeg = std::atan(wind_[i] / (ice_[i] + qe_)) * (180 / pi_);
    r.inclinedMall = (2000 / an_) * (2000 / an_) * r.inclinedSagM;
}

// Sista delen: mellanvärdena, stakningsmallen och varningarna om utnyttjandegraden och Eiv i Ep.
void Calculator::finish(LineResult& out)
{
    res_.mode = in_.mode;
    res_.k1 = k1_;
    res_.stakingMall = res_.cases[kLoadCaseCount - 1].mall;

    if (!epEivCases_.empty())
        issues_.push_back(makeIssue(LineIssueKind::Warning, 0, LineField::Result,
                                    "Ep = Ei, och Excel räknar då Eiv också i Ep-stadiet (lastfall " +
                                        joinIds(epEivCases_) + ")."));
    std::vector<int> over;
    for (const auto& c : res_.cases)
        if (c.overUtilization)
            over.push_back(c.id);
    if (!over.empty())
        issues_.push_back(makeIssue(LineIssueKind::Warning, 0, LineField::Result,
                                    "Utnyttjandegraden är över " + num(in_.constants.maxUtilizationPercent) +
                                        " % i lastfall " + joinIds(over) + "."));
    out = res_;
}

// Hela räkningen i ordningen i planen 3.7 och 5.6.
bool Calculator::run(LineResult& out)
{
    computeLoads();
    k1_ = qe_ * qe_ * (an_ * an_) / (area_ * area_) / 24;

    if (!solveStage(1, kEiCaseCount, StageRef{sigma0_, t0_, ei_}, true))
        return false;
    StageRef epRef;
    if (!transpose(epRef))
        return false;
    // D5: Excel räknar Eiv också i Ep-stadiet när Ep = Ei
    const bool epEiv = rules_.eivWhenEpEqualsEi && ep_ == ei_;
    if (!solveStage(kEiCaseCount + 1, kLoadCaseCount, epRef, epEiv))
        return false;
    finish(out);
    return true;
}

} // namespace

// Stadiet för lastfall id: Ei för 1-7, Ep för allt annat.
LineStage stageOf(int id)
{
    return id >= 1 && id <= kEiCaseCount ? LineStage::Ei : LineStage::Ep;
}

// Lastfallets temperatur i input.mode: talet i läget Excel (D7-D9), annars källan.
// Ett ID utanför 1-15 är ett programfel och ger 0.
double caseTempC(const LineInput& input, int id)
{
    if (id < 1 || id > kLoadCaseCount)
        return 0;
    const LoadCase& lc = input.cases[static_cast<std::size_t>(id - 1)];
    if (input.mode == CalcMode::Excel)
        return lc.tempC;
    switch (lc.tempSource) {
    case TempSource::Min:
        return input.minTempC;
    case TempSource::MinPlus20:
        return input.minTempC + 20;
    case TempSource::Max:
        return input.maxTempC;
    case TempSource::ShortCircuit:
        return input.shortCircuitTempC;
    case TempSource::ShortCircuitRise:
        return input.minTempC + (input.shortCircuitTempC - input.maxTempC);
    case TempSource::Fixed:
        break;
    }
    return lc.tempC;
}

// Lastfallets karakteristiska vindtryck i input.mode: talet i läget Excel (D7-D9), annars källan.
// Ett ID utanför 1-15 är ett programfel och ger 0.
double caseCharPressureNPerM2(const LineInput& input, int id)
{
    if (id < 1 || id > kLoadCaseCount)
        return 0;
    const LoadCase& lc = input.cases[static_cast<std::size_t>(id - 1)];
    if (input.mode == CalcMode::Excel)
        return lc.charPressureNPerM2;
    switch (lc.pressureSource) {
    case PressureSource::Normal:
        return input.normalWindNPerM2;
    case PressureSource::High:
        return input.highWindNPerM2;
    case PressureSource::Fixed:
        break;
    }
    return lc.charPressureNPerM2;
}

// VBA:s ACOS med arkets pi. Vid y = 1 och y = -1 skiljer lägena (planen 4.2, D6).
double vbaAcos(double y, double pi, CalcMode mode)
{
    if (y == 1 || y == -1) {
        if (mode == CalcMode::Excel)
            return pi / 2;   // D6: Excel ger pi / 2
        return y == 1 ? 0.0 : pi;
    }
    if (!(std::fabs(y) < 1))
        return kNaN;
    return -std::atan(y / std::sqrt(1 - y * y)) + pi / 2;
}

// Roten till x^3 + c*x^2 - d = 0 som VBA:s TREDJEGRAD: Cardano när z >= 0, annars den
// trigonometriska lösningen med rotvalet närmast sigmaRef. Samma uttryck i samma ordning.
double solveStateCubic(double c, double d, double pi, double sigmaRef, CalcMode mode)
{
    const double p = -c * c / 3;
    const double q = 2 * std::pow(c, 3) / 27 - d;
    const double z = q * q / 4 + std::pow(p, 3) / 27;
    double x = kNaN;
    if (z >= 0) {
        const double sz = std::sqrt(z);
        const double u = std::pow(std::fabs(sz - q / 2), 1.0 / 3.0) * sgn(sz - q / 2);
        const double v = std::pow(std::fabs(-sz - q / 2), 1.0 / 3.0) * sgn(-sz - q / 2);
        x = u + v - c / 3;
    } else {
        const double inner = -p * std::pow(p, 2) / 27;
        const double r = 2 * std::fabs(std::pow(std::sqrt(inner), 1.0 / 3.0)) * sgn(inner);
        const double f = vbaAcos(-q / 2 / std::sqrt(inner), pi, mode);
        double x1 = r * std::cos(f / 3) - c / 3;
        double x2 = r * std::cos(f / 3 + 2.0 / 3.0 * pi) - c / 3;
        double x3 = r * std::cos(f / 3 + 4.0 / 3.0 * pi) - c / 3;
        if (x1 < 0)
            x1 = -1e25;
        if (x2 < 0)
            x2 = -1e25;
        if (x3 < 0)
            x3 = -1e25;
        const double a1 = std::fabs(sigmaRef - x1);
        const double a2 = std::fabs(sigmaRef - x2);
        const double a3 = std::fabs(sigmaRef - x3);
        double d8 = 0;
        if (a1 > a2 && a3 > a2)
            d8 = a2;
        if (a2 > a1 && a3 > a1)
            d8 = a1;
        if (a1 > a3 && a2 > a3)
            d8 = a3;
        if (a1 - d8 < 0.000005)
            x = x1;
        if (a3 - d8 < 0.000005)
            x = x3;
        if (a2 - d8 < 0.000005)
            x = x2;
        if (std::isfinite(x) && x < 0)
            x = kNaN;
    }
    return std::isfinite(x) ? x : kNaN;
}

// Linberäkningen: validerar, räknar och samlar upplysningarna. Upplysningarna sorteras med
// hela beräkningen först och sedan lastfall 1-15, i den ordning de uppstod.
LineCalculation calculateLine(const LineInput& input)
{
    LineCalculation calc;
    if (validate(input, calc.issues)) {
        LineResult result;
        Calculator calculator(input, calc.issues);
        if (calculator.run(result))
            calc.result = result;
    }
    std::stable_sort(calc.issues.begin(), calc.issues.end(),
                     [](const LineIssue& a, const LineIssue& b) { return a.caseId < b.caseId; });
    return calc;
}

} // namespace kalkyl::linecalc
