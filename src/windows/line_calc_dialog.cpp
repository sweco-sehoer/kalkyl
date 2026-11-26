#include "line_calc_dialog.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <functional>
#include <limits>
#include <memory>
#include <numbers>
#include <string>
#include <utility>
#include <vector>

#include "lineCalc/load_cases.h"
#include "line_calc_info_dialog.h"
#include "tableText/table_text.h"

namespace {

namespace linecalc = kalkyl::linecalc;
namespace conductor = kalkyl::conductor;
namespace tabletext = kalkyl::tabletext;
namespace projectdb = kalkyl::projectdb;
namespace sheetgrid = kalkyl::sheetgrid;

using Lindata = std::array<std::optional<double>, 10>;

// Tabellernas nummer och mått. Raderna i lastfallstabellen är 16: rad 7 är tomraden (arkets rad 30).
constexpr int kGridInput = 0;
constexpr int kGridConst = 1;
constexpr int kGridCase = 2;
constexpr int kGridRows[3] = {5, 10, 16};
constexpr int kGridCols[3] = {4, 4, 26};
constexpr int kBlankRow = 7;

// Märkena i prioritetsordning (6.5): det lägsta numret gäller. kMarkTip är en tooltip utan färg.
constexpr int kMarkError = 1;
constexpr int kMarkMissing = 2;
constexpr int kMarkOver = 3;
constexpr int kMarkDiff = 4;
constexpr int kMarkManual = 5;
constexpr int kMarkDoc = 6;
constexpr int kMarkChanged = 7;
constexpr int kMarkLabel = 8;
constexpr int kMarkEven = 9;
constexpr int kMarkTip = 100;

// Färgen för ett märke. Halvgenomskinliga, så att de fungerar i ljust och mörkt läge.
std::optional<gfx::Color> colorOfMark(int mark)
{
    switch (mark) {
    case kMarkError: return gfx::Color{220, 60, 60, 80};
    case kMarkMissing: return gfx::Color{220, 160, 40, 72};
    case kMarkOver: return gfx::Color{200, 0, 0, 90};
    case kMarkDiff: return gfx::Color{40, 170, 170, 72};
    case kMarkManual: return gfx::Color{150, 100, 210, 64};
    case kMarkDoc: return gfx::Color{70, 170, 90, 56};
    case kMarkChanged: return gfx::Color{200, 120, 40, 56};
    case kMarkLabel: return gfx::Color{128, 128, 128, 40};
    case kMarkEven: return gfx::Color{245, 160, 90, 64};
    default: break;
    }
    return std::nullopt;
}

// Textfärgen för ett lastfall utan benämning.
const gfx::Color kDimText{128, 128, 128, 255};

// Radbrytningen som sträng, eftersom källan inte får innehålla escape-sekvenser.
std::string newline()
{
    return std::string(1, char{10});
}

// Texten utan blanksteg i början och slutet. Tabb och radbrytning räknas som blanksteg.
std::string trim(const std::string& s)
{
    const auto isSpace = [](char c) {
        return c == ' ' || c == char{9} || c == char{10} || c == char{13};
    };
    std::size_t b = 0;
    std::size_t e = s.size();
    while (b < e && isSpace(s[b]))
        ++b;
    while (e > b && isSpace(s[e - 1]))
        --e;
    return s.substr(b, e - b);
}

// Postar ett anrop till UI-tråden som hoppas över om fönstret hunnit försvinna.
void postIfAlive(const std::shared_ptr<bool>& alive, std::function<void()> fn)
{
    std::weak_ptr<bool> weak = alive;
    glaze::app().post([weak, fn = std::move(fn)] {
        if (weak.lock())
            fn();
    });
}

// Talet som text med decimaler (-1 = kortaste form) och skiljetecken.
std::string fmt(double value, int decimals, char separator)
{
    return tabletext::formatDecimal(value, decimals, separator);
}

// De värden en cell i Indata eller Lindata / Konstanter kan hålla. Lindatats tio tal ligger
// sist och hör ihop med lindata_ i dialogen.
enum class Slot {
    None, NormalSpan, Tension, RefTemp, MinTemp, MaxTemp, ShortCircuit, LineHeight, NormalWind,
    HighWind, IceDensity, Pi, Gravity, Gust, Resonance, Drag, MinIce, MaxUtil, Transposition,
    Area, Diameter, EInitial, EPermanent, ELimit, Eiv, Alpha, CreepTemp, Weight, Breaking
};

// En rad i Indata eller Lindata / Konstanter: etikett och värde, två gånger.
struct SlotRow {
    const char* label0;
    Slot slot1;
    const char* label2;
    Slot slot3;
};

const SlotRow kInputRows[5] = {
    {"Normalspann, aₙ (m)", Slot::NormalSpan, "Linhöjd, snitt (m)", Slot::LineHeight},
    {"Uppspänning, σ₀ (MPa)", Slot::Tension, "Karakteristiskt vindtryck Normal Vind (N/m²)", Slot::NormalWind},
    {"Referenstemperatur, T₀ (°C)", Slot::RefTemp, "Karakteristiskt vindtryck Hög Vind (N/m²)", Slot::HighWind},
    {"Minimumtemperatur (°C)", Slot::MinTemp, "Kortslutningstemperatur, Tks (°C)", Slot::ShortCircuit},
    {"Maximumtemperatur (°C)", Slot::MaxTemp, "", Slot::None}};

const SlotRow kConstRows[10] = {
    {"Tvärsnittsarea, A (mm²)", Slot::Area, "Isdensitet, ρ (kg/m³)", Slot::IceDensity},
    {"Diameter, d (mm)", Slot::Diameter, "Pi", Slot::Pi},
    {"Initiell E-modul, Eᵢₖ (MPa)", Slot::EInitial, "Tyngdaccelerationen (m/s²)", Slot::Gravity},
    {"Permanent E-modul, Eₚ (MPa)", Slot::EPermanent, "Gust response factor för linan, Gq", Slot::Gust},
    {"Högsta spänningen då Eᵢₖ är giltig, σₚ (MPa)", Slot::ELimit, "Structural resonance factor för linan, Gc", Slot::Resonance},
    {"Konstant för beräkning av Eᵢᵥ, k", Slot::Eiv, "Drag factor för linan, Cc", Slot::Drag},
    {"Utvidgningskoefficient, α (10⁻⁶/°C)", Slot::Alpha, "Minimum islast vid vindstilla, I (N/m)", Slot::MinIce},
    {"Permanent förlängning pga krypning, Tₚ (°C)", Slot::CreepTemp, "Maximal tillåten utnyttjandegrad av brottlast (%)", Slot::MaxUtil},
    {"Egentyngd, qₑ (N/m)", Slot::Weight, "Lastfall för transponering till Ep-stadiet (nr)", Slot::Transposition},
    {"Brottlast, fᵤ (kN)", Slot::Breaking, "", Slot::None}};

// Slotten i rad row och kolumn col (1 eller 3) i en av de två etikettabellerna.
Slot slotAt(int grid, int row, int col)
{
    if (grid > kGridConst || row < 0 || row >= kGridRows[grid])
        return Slot::None;
    const SlotRow& r = grid == kGridInput ? kInputRows[row] : kConstRows[row];
    if (col == 1)
        return r.slot1;
    if (col == 3)
        return r.slot3;
    return Slot::None;
}

// Etiketten som hör till en värdecell (kolumn 1 eller 3), eller etiketten i kolumn 0 och 2.
std::string labelAt(int grid, int row, int col)
{
    if (grid > kGridConst || row < 0 || row >= kGridRows[grid])
        return {};
    const SlotRow& r = grid == kGridInput ? kInputRows[row] : kConstRows[row];
    return (col == 0 || col == 1) ? r.label0 : r.label2;
}

// Sant för lindatats tio tal.
bool isLindataSlot(Slot s)
{
    return static_cast<int>(s) >= static_cast<int>(Slot::Area);
}

// Index i lindata_ för en lindataslot.
std::size_t lindataIndex(Slot s)
{
    return static_cast<std::size_t>(static_cast<int>(s) - static_cast<int>(Slot::Area));
}

// Sant när slotten får vara tom (och då betyder "saknas"). Övriga tomma tal räknas som 0.
bool isOptionalSlot(Slot s)
{
    return s == Slot::NormalSpan || s == Slot::Tension || isLindataSlot(s);
}

// Fältet i upplysningarna som slotten hör till.
linecalc::LineField fieldOfSlot(Slot s)
{
    using F = linecalc::LineField;
    switch (s) {
    case Slot::NormalSpan: return F::NormalSpan;
    case Slot::Tension: return F::Tension;
    case Slot::RefTemp: return F::RefTemp;
    case Slot::MinTemp: return F::MinTemp;
    case Slot::MaxTemp: return F::MaxTemp;
    case Slot::ShortCircuit: return F::ShortCircuitTemp;
    case Slot::LineHeight: return F::LineHeight;
    case Slot::NormalWind: return F::NormalWind;
    case Slot::HighWind: return F::HighWind;
    case Slot::IceDensity: return F::IceDensity;
    case Slot::Pi: return F::Pi;
    case Slot::Gravity: return F::Gravity;
    case Slot::Gust: return F::GustFactor;
    case Slot::Resonance: return F::ResonanceFactor;
    case Slot::Drag: return F::DragFactor;
    case Slot::MinIce: return F::MinIce;
    case Slot::MaxUtil: return F::MaxUtilization;
    case Slot::Transposition: return F::TranspositionCase;
    default: break;
    }
    return isLindataSlot(s) ? F::Conductor : F::None;
}

// Antalet decimaler cellen visas med (-1 = kortaste form). Krypningstemperaturen visas som i arket.
int slotDecimals(Slot s)
{
    return s == Slot::CreepTemp ? 1 : -1;
}

// Slottens värde i indata, eller i lindatat för lindataslotarna.
std::optional<double> getSlot(const linecalc::LineInput& in, const Lindata& lin, Slot s)
{
    const linecalc::LineConstants& k = in.constants;
    switch (s) {
    case Slot::NormalSpan: return in.normalSpanM;
    case Slot::Tension: return in.tensionMpa;
    case Slot::RefTemp: return in.refTempC;
    case Slot::MinTemp: return in.minTempC;
    case Slot::MaxTemp: return in.maxTempC;
    case Slot::ShortCircuit: return in.shortCircuitTempC;
    case Slot::LineHeight: return in.lineHeightM;
    case Slot::NormalWind: return in.normalWindNPerM2;
    case Slot::HighWind: return in.highWindNPerM2;
    case Slot::IceDensity: return k.iceDensityKgM3;
    case Slot::Pi: return k.pi;
    case Slot::Gravity: return k.gravity;
    case Slot::Gust: return k.gustFactor;
    case Slot::Resonance: return k.resonanceFactor;
    case Slot::Drag: return k.dragFactor;
    case Slot::MinIce: return k.minIceNPerM;
    case Slot::MaxUtil: return k.maxUtilizationPercent;
    case Slot::Transposition: return static_cast<double>(k.transpositionCase);
    case Slot::None: break;
    default: return lin[lindataIndex(s)];
    }
    return std::nullopt;
}

// Skriver slottens värde. Ett tomt värde i en slot som inte får vara tom blir 0.
void setSlot(linecalc::LineInput& in, Lindata& lin, Slot s, std::optional<double> v)
{
    linecalc::LineConstants& k = in.constants;
    const double d = v.value_or(0);
    switch (s) {
    case Slot::NormalSpan: in.normalSpanM = v; break;
    case Slot::Tension: in.tensionMpa = v; break;
    case Slot::RefTemp: in.refTempC = d; break;
    case Slot::MinTemp: in.minTempC = d; break;
    case Slot::MaxTemp: in.maxTempC = d; break;
    case Slot::ShortCircuit: in.shortCircuitTempC = d; break;
    case Slot::LineHeight: in.lineHeightM = d; break;
    case Slot::NormalWind: in.normalWindNPerM2 = d; break;
    case Slot::HighWind: in.highWindNPerM2 = d; break;
    case Slot::IceDensity: k.iceDensityKgM3 = d; break;
    case Slot::Pi: k.pi = d; break;
    case Slot::Gravity: k.gravity = d; break;
    case Slot::Gust: k.gustFactor = d; break;
    case Slot::Resonance: k.resonanceFactor = d; break;
    case Slot::Drag: k.dragFactor = d; break;
    case Slot::MinIce: k.minIceNPerM = d; break;
    case Slot::MaxUtil: k.maxUtilizationPercent = d; break;
    case Slot::Transposition: k.transpositionCase = static_cast<int>(std::lround(d)); break;
    case Slot::None: break;
    default: lin[lindataIndex(s)] = v; break;
    }
}

// Cellen i en tabell. grid -1 betyder att upplysningen inte hör till någon cell.
struct CellRef {
    int grid = -1;
    int row = 0;
    int col = 0;
    std::string tip;   // egen text i stället för upplysningens
};

// Raden i lastfallstabellen för lastfall id (1-15).
int rowOfCaseId(int id)
{
    return id <= linecalc::kEiCaseCount ? id - 1 : id;
}

// Kolumnen i lastfallstabellen för ett lastfallsfält, eller -1.
int caseColumnOf(linecalc::LineField f)
{
    using F = linecalc::LineField;
    switch (f) {
    case F::CaseTemp: return 1;
    case F::CaseWindFrom: return 6;
    case F::CaseMagnitude: return 8;
    case F::CasePressure: return 9;
    case F::CaseRefHeight: return 10;
    case F::CaseGradient: return 11;
    case F::CaseIce: return 13;
    case F::CaseManualIce: return 15;
    case F::CaseManualWind: return 16;
    case F::Result: return 18;
    default: break;
    }
    return -1;
}

// Raden i Lindata / Konstanter för ett fält i linans validering, eller -1 utan cell.
int rowOfConductorField(conductor::ConductorField f)
{
    using C = conductor::ConductorField;
    switch (f) {
    case C::Area: return 0;
    case C::Diameter: return 1;
    case C::EInitial: return 2;
    case C::EPermanent: return 3;
    case C::EInitialLimit: return 4;
    case C::EivConstant: return 5;
    case C::Alpha: return 6;
    case C::CreepTemp: return 7;
    case C::Weight: return 8;
    case C::BreakingLoad: return 9;
    default: break;
    }
    return -1;
}

// Cellerna en upplysning gäller (6.5). Lindata med fel pekar ut cellerna validateConductor
// anger, och saknat lindata pekar ut de tomma cellerna.
std::vector<CellRef> cellsOfIssue(const linecalc::LineIssue& issue, const Lindata& lin,
                                  const std::optional<conductor::Conductor>& cond)
{
    std::vector<CellRef> out;
    if (issue.field == linecalc::LineField::Conductor) {
        if (issue.kind == linecalc::LineIssueKind::Missing) {
            for (int i = 0; i < 9; ++i)
                if (!lin[static_cast<std::size_t>(i)])
                    out.push_back({kGridConst, i, 1, {}});
        } else if (cond) {
            for (const auto& ci : conductor::validateConductor(*cond)) {
                const int row = rowOfConductorField(ci.field);
                if (row >= 0)
                    out.push_back({kGridConst, row, 1, ci.message});
            }
        }
        return out;
    }
    if (issue.field == linecalc::LineField::None)
        return out;
    const int caseCol = caseColumnOf(issue.field);
    if (caseCol >= 0) {
        if (issue.caseId >= 1 && issue.caseId <= linecalc::kLoadCaseCount)
            out.push_back({kGridCase, rowOfCaseId(issue.caseId), caseCol, {}});
        return out;
    }
    for (int g = kGridInput; g <= kGridConst; ++g)
        for (int r = 0; r < kGridRows[g]; ++r)
            for (int c = 1; c <= 3; c += 2)
                if (slotAt(g, r, c) != Slot::None && fieldOfSlot(slotAt(g, r, c)) == issue.field)
                    out.push_back({g, r, c, {}});
    return out;
}

// Slag av cell i lastfallstabellen: N tal, I heltal, T text och 0 för kolumner som inte är indata.
constexpr const char* kCaseKind = "0NT0TTI0NNNN0N0NN000000000";

// Kolumnernas namn i meddelanden (rubrikens rader med mellanslag).
const char* const kCaseNames[26] = {
    "ID", "Temp T (°C)", "Lastfall benämning", "Stadium", "SS-EN 50341 Standard load cases",
    "SS-EN 50341 Load cases for calculation of clearances",
    "Vid is och vindstilla använd vindparametrar från lastfall-ID", "Höjd h (m)",
    "Magnitud vindlast (%)", "Karakteristiska vindtrycket qh (N/m²)", "Referenshöjd href (m)",
    "dqh (N/m³)", "Vindtryck qh (N/m²)", "Istjocklek vid normal vind (mm)",
    "Motsvarande istjocklek vid vindstilla (mm)", "Islast I (N/m)", "Vindlast Qwc (N/m)",
    "Nedhäng vertikal projektion b (m)", "Spänning σ (MPa)", "Linkraft F (N)", "Mall (nr)",
    "Utnyttjandegrad av brottlast (%)", "", "Nedhäng b (m)", "Utsvängningsvinkel α (°)", "Mall (nr)"};

// Sant för kolumnerna som är tomma och låsta i ett lastfall med manuell last (J, K, N-Q och S).
bool isManualLockedColumn(int col)
{
    return col == 4 || col == 5 || col == 8 || col == 9 || col == 10 || col == 11 || col == 13;
}

// Sant för kolumnerna som visas tomma i ett lastfall med manuell last (också M, R och T).
bool isManualBlankColumn(int col)
{
    return isManualLockedColumn(col) || col == 7 || col == 12 || col == 14;
}

// Värdet i en cell i lastfallstabellen: ett tal med format, en text eller inget alls.
struct CellVal {
    bool numeric = false;
    bool present = false;
    double value = 0;
    int decimals = -1;
    std::string text;
};

CellVal numVal(double v, int decimals)
{
    CellVal c;
    c.numeric = true;
    c.present = true;
    c.value = v;
    c.decimals = decimals;
    return c;
}

CellVal textVal(std::string t)
{
    CellVal c;
    c.text = std::move(t);
    return c;
}

CellVal absentVal()
{
    CellVal c;
    c.numeric = true;
    return c;
}

// Texten en cellvärde visas med. Tal utan värde och tomma texter blir tom text.
std::string valText(const CellVal& v, char separator)
{
    if (v.numeric)
        return v.present ? fmt(v.value, v.decimals, separator) : std::string();
    return v.text;
}

// Texten en cell kopieras som: talet med fullt antal decimaler, annars cellens text.
std::string valCopyText(const CellVal& v)
{
    if (v.numeric)
        return v.present ? fmt(v.value, -1, ',') : std::string();
    return v.text;
}

// Värdet i lastfall id och kolumn col ur indata och resultat (planen 6.4). Ett lastfall med
// manuell last visar kolumnerna J, K och M-T tomma, och utan resultat är resultatkolumnerna tomma.
CellVal caseCellVal(const linecalc::LineInput& in, const linecalc::LineCalculation& calc, int id, int col)
{
    const linecalc::LoadCase& c = in.cases[static_cast<std::size_t>(id - 1)];
    const linecalc::LoadCaseResult* r = calc.result ? &calc.result->cases[static_cast<std::size_t>(id - 1)] : nullptr;
    const bool manual = c.manual.has_value();
    switch (col) {
    case 0: return textVal(std::to_string(id));
    case 1: return numVal(linecalc::caseTempC(in, id), -1);
    case 2: return textVal(c.name);
    case 3: return textVal(id <= linecalc::kEiCaseCount ? "Ei" : "Ep");
    case 4: return textVal(manual ? std::string() : c.standardCase);
    case 5: return textVal(manual ? std::string() : c.clearanceCase);
    case 6: return c.windFromCase ? numVal(*c.windFromCase, 0) : absentVal();
    case 7: return (!manual && r && r->heightM) ? numVal(*r->heightM, -1) : absentVal();
    case 8: return manual ? absentVal() : numVal(c.windMagnitudePercent, -1);
    case 9: return manual ? absentVal() : numVal(linecalc::caseCharPressureNPerM2(in, id), -1);
    case 10: return manual ? absentVal() : numVal(c.refHeightM, -1);
    case 11: return manual ? absentVal() : numVal(c.pressureGradientNPerM3, -1);
    case 12: return (!manual && r && r->pressureNPerM2) ? numVal(*r->pressureNPerM2, 0) : absentVal();
    case 13: return manual ? absentVal() : numVal(c.iceThicknessMm, 1);
    case 14: return (!manual && r && r->iceEquivalentMm) ? numVal(*r->iceEquivalentMm, 1) : absentVal();
    case 15:
        if (manual)
            return numVal(c.manual->iceNPerM, 2);
        return r ? numVal(r->iceNPerM, 2) : absentVal();
    case 16:
        if (manual)
            return numVal(c.manual->windNPerM, 2);
        return r ? numVal(r->windNPerM, 2) : absentVal();
    case 17: return r ? numVal(r->sagM, 2) : absentVal();
    case 18: return r ? numVal(r->stressMpa, 2) : absentVal();
    case 19: return r ? numVal(r->forceN, 0) : absentVal();
    case 20: return r ? numVal(r->mall, 0) : absentVal();
    case 21:
        if (!r)
            return absentVal();
        return r->utilizationPercent ? numVal(*r->utilizationPercent, 0) : textVal("-");
    case 23: return r ? numVal(r->inclinedSagM, 2) : absentVal();
    case 24: return r ? numVal(r->swingAngleDeg, 2) : absentVal();
    case 25: return r ? numVal(std::nearbyint(r->inclinedMall), 0) : absentVal();
    default: break;
    }
    return absentVal();
}

// Kolumnerna som jämförs mellan lägena (6.12).
bool isDiffColumn(int col)
{
    return col == 1 || col == 9 || col == 7 || col == 12 || col == 14 || (col >= 15 && col <= 21) ||
           (col >= 23 && col <= 25);
}

// Redigerarens form för en cell: tal, heltal, text eller skrivskyddad text. readOnly låser den.
glaze::DataGrid::EditorSpec specFor(int grid, int row, int col, bool readOnly)
{
    using E = glaze::DataGrid::Editor;
    glaze::DataGrid::EditorSpec s;
    s.editor = E::None;
    if (grid != kGridCase) {
        const Slot slot = slotAt(grid, row, col);
        if (slot != Slot::None) {
            s.editor = E::Number;
            s.allowEmpty = true;
            if (slot == Slot::Breaking)
                s.emptyText = "-";
            if (slot == Slot::Transposition)
                s.decimals = 0;
        }
    } else if (col >= 0 && col < 26) {
        switch (kCaseKind[col]) {
        case 'N':
            s.editor = E::Number;
            s.allowEmpty = true;
            break;
        case 'I':
            s.editor = E::Number;
            s.allowEmpty = true;
            s.decimals = 0;
            break;
        case 'T': s.editor = E::Text; break;
        default: break;
        }
    }
    s.readOnly = readOnly && s.editor != E::None;
    return s;
}

// Källan till ett lastfalls temperatur i läget Ny som tooltip, eller tom text för ett fast tal.
std::string tempSourceTip(linecalc::TempSource s)
{
    switch (s) {
    case linecalc::TempSource::Min: return "Följer minimumtemperaturen (J6)";
    case linecalc::TempSource::MinPlus20: return "Följer minimumtemperaturen + 20";
    case linecalc::TempSource::Max: return "Följer maximumtemperaturen (J7)";
    case linecalc::TempSource::ShortCircuit: return "Följer kortslutningstemperaturen";
    case linecalc::TempSource::ShortCircuitRise:
        return "Följer minimumtemperaturen + (kortslutningstemperaturen − maximumtemperaturen)";
    case linecalc::TempSource::Fixed: break;
    }
    return {};
}

// Källan till ett lastfalls vindtryck i läget Ny som tooltip, eller tom text för ett fast tal.
std::string pressureSourceTip(linecalc::PressureSource s)
{
    switch (s) {
    case linecalc::PressureSource::Normal: return "Följer Normal vind (O4)";
    case linecalc::PressureSource::High: return "Följer Hög vind (O5)";
    case linecalc::PressureSource::Fixed: break;
    }
    return {};
}

// Namnet på ett läge i texterna.
const char* modeName(linecalc::CalcMode m)
{
    return m == linecalc::CalcMode::Excel ? "Excel" : "Ny";
}

// Sant när två tal är lika på ett tiotusendels värde när, till jämförelsen med linregistret.
bool sameNumber(const std::optional<double>& a, const std::optional<double>& b)
{
    if (a.has_value() != b.has_value())
        return false;
    if (!a)
        return true;
    return std::fabs(*a - *b) <= 1e-9 * std::max(1.0, std::fabs(*b));
}

// Lindatats tio tal ur en lina, i ordningen i Lindata / Konstanter.
Lindata lindataOf(const conductor::Conductor& c)
{
    return {c.areaMm2, c.diameterMm, c.eInitialMpa, c.ePermanentMpa, c.eInitialLimitMpa,
            c.eivConstant, c.alphaE6PerC, c.creepTempC, c.weightNPerM, c.breakingLoadKn};
}

} // namespace

// Adaptern till SheetGrid: texten en cell kopieras som.
std::string LineCalcDialog::GridHost::copyText(int row, int column) const
{
    return dialog_.copyTextOf(grid_, row, column);
}

// Adaptern till SheetGrid: cellens färg utan markering.
std::optional<gfx::Color> LineCalcDialog::GridHost::baseColor(int row, int column) const
{
    return dialog_.colorOfCell(grid_, row, column);
}

// Adaptern till SheetGrid: inklistrade och raderade celler skrivs i modellen.
void LineCalcDialog::GridHost::applyEdits(const std::vector<sheetgrid::CellEdit>& edits)
{
    dialog_.applyEditsTo(grid_, edits);
}

// Adaptern till SheetGrid: lastfallstabellens snabbmeny får posten som tar bort manuell last.
void LineCalcDialog::GridHost::addMenuItems(glaze::Menu& menu, const sheetgrid::CellRect& selection)
{
    if (grid_ != kGridCase)
        return;
    bool anyManual = false;
    for (int r = selection.row0; r <= selection.row1; ++r) {
        const int id = caseIdOfRow(r);
        if (id > 0 && dialog_.lc_.input.cases[static_cast<std::size_t>(id - 1)].manual)
            anyManual = true;
    }
    glaze::MenuItem& item = menu.addItem("Räkna last ur vind och is");
    item.enabled = anyManual && !dialog_.readOnly_;
    const int row0 = selection.row0;
    const int row1 = selection.row1;
    LineCalcDialog* dialog = &dialog_;
    item.onClick = [dialog, row0, row1] { dialog->clearManualLoad(row0, row1); };
}

// Adaptern till SheetGrid: en kort text i statusraden.
void LineCalcDialog::GridHost::showStatus(const std::string& text)
{
    dialog_.setStatus(text);
}

// Anger projektet som formuläret läser och sparar i.
// Objektet ägs av main och måste leva längre än fönstret.
void LineCalcDialog::setProject(kalkyl::projectdb::ProjectDb& project)
{
    project_ = &project;
}

// Anger systemdatabasen, som ger linregistret till rullistan Lina.
void LineCalcDialog::setSystemDb(kalkyl::systemdb::SystemDb& sys)
{
    sys_ = &sys;
}

// En ny beräkning, med raden i Dokumentation som ska visa den efter OK (tom = lös).
// fromDoc gör den kopplad: indata hämtas då ur Dokumentation.
void LineCalcDialog::setNew(std::string linkDocKey, bool fromDoc)
{
    linkDocKey_ = std::move(linkDocKey);
    newFromDoc_ = fromDoc && !linkDocKey_.empty();
    existingId_ = 0;
    copyOfId_ = 0;
}

// En befintlig beräkning, som läses ur projektet när fönstret laddas.
void LineCalcDialog::setExisting(std::int64_t id)
{
    existingId_ = id;
    copyOfId_ = 0;
    newFromDoc_ = false;
    linkDocKey_.clear();
}

// En ny, lös och manuell beräkning som börjar som en kopia av beräkningen id.
void LineCalcDialog::setCopyOf(std::int64_t id)
{
    copyOfId_ = id;
    existingId_ = 0;
    newFromDoc_ = false;
    linkDocKey_.clear();
}

// Radtexterna i Dokumentation per nyckel, till upplysningen om var beräkningen visas.
void LineCalcDialog::setDocRowLabels(std::vector<std::pair<std::string, std::string>> labels)
{
    docLabels_ = std::move(labels);
}

// Skrivskyddat läge: allt visas men inget kan ändras. Gäller från och med onLoad.
void LineCalcDialog::setReadOnly(bool readOnly)
{
    readOnly_ = readOnly;
}

// Id för den sparade beräkningen efter run(), tomt om inget sparades.
std::optional<std::int64_t> LineCalcDialog::savedId() const
{
    return savedId_;
}

// Tabellen med numret: Indata, Lindata / Konstanter eller Lastfall.
glaze::DataGrid& LineCalcDialog::tableOf(int grid)
{
    if (grid == kGridInput)
        return inputGrid;
    return grid == kGridConst ? constGrid : caseGrid;
}

const glaze::DataGrid& LineCalcDialog::tableOf(int grid) const
{
    if (grid == kGridInput)
        return inputGrid;
    return grid == kGridConst ? constGrid : caseGrid;
}

// Numret på lastfallet för en rad i lastfallstabellen, 0 för tomraden.
int LineCalcDialog::caseIdOfRow(int row)
{
    if (row < 0 || row == kBlankRow || row > linecalc::kLoadCaseCount)
        return 0;
    return row < kBlankRow ? row + 1 : row;
}

// Raden i lastfallstabellen för lastfall 1-15.
int LineCalcDialog::rowOfCase(int id)
{
    return rowOfCaseId(id);
}

// Startlogik, körs när fönstret laddas (aldrig i konstruktorn). Läser beräkningen, bygger
// lindatats rullista och tabellerna, räknar första gången och markerar första cellen.
void LineCalcDialog::onLoad(glaze::Widget*, glaze::EventArgs&)
{
    if (!project_)
        return;
    if (!loadCalculation()) {
        postIfAlive(alive_, [this] { closeWithResult(glaze::DialogResult::Cancel); });
        return;
    }
    loadConductorCells(lc_.input.conductor);
    buildConductorChoices();
    selectConductorItem();

    modeNewRb.setChecked(lc_.input.mode == linecalc::CalcMode::New);
    modeExcelRb.setChecked(lc_.input.mode == linecalc::CalcMode::Excel);
    diffChk.setText(lc_.input.mode == linecalc::CalcMode::Excel ? "Markera skillnaden mot läget Ny"
                                                                 : "Markera skillnaden mot läget Excel");

    // Rutan Hämta ur Dokumentation gäller raden beräkningen är kopplad till eller visas på.
    docRowKey_.clear();
    if (!lc_.docSource.empty())
        docRowKey_ = lc_.docSource;
    else if (existingId_ != 0 && existingDocKeys_.size() == 1)
        docRowKey_ = existingDocKeys_.front();
    else if (existingId_ == 0 && copyOfId_ == 0)
        docRowKey_ = linkDocKey_;
    if (docRowKey_.empty() || automatic_)
        docInputChk.hide();
    else
        docInputChk.setChecked(!lc_.docSource.empty());
    if (automatic_)
        setTitle("Linberäkning (automatisk)");

    buildTables();
    if (readOnly_)
        applyReadOnly();
    updateDocLabel();
    recalc(true);
    inputGrid.setCurrentCell(0, 1);
    sheets_[kGridInput]->select({0, 1}, {0, 1});
}

// Krysset, Avbryt och Escape: frågar om osparade ändringar innan fönstret stängs.
// Ja sparar (och stannar om det misslyckas), Nej kastar ändringarna, Avbryt stannar.
void LineCalcDialog::onClosing(glaze::Widget*, glaze::CloseArgs& e)
{
    if (saved_ || readOnly_ || e.reason != glaze::CloseReason::UserClosing)
        return;
    commitEdits();
    if (!dirty_)
        return;

    const auto answer = glaze::messageBox(
        *this, "Du har ändrat linberäkningen. Vill du spara ändringarna?", "Linberäkning",
        glaze::MessageBoxButtons::YesNoCancel, glaze::MessageBoxType::Warning);
    if (answer == glaze::DialogResult::Yes) {
        if (!saveChanges())
            e.cancel = true;
    } else if (answer != glaze::DialogResult::No) {
        e.cancel = true;
    }
}

// OK: sparar beräkningen i projektet och stänger. Vid fel visas det och fönstret står kvar.
void LineCalcDialog::okBtnClicked(glaze::Widget*, glaze::ClickArgs&)
{
    if (readOnly_)
        return;
    if (saveChanges())
        closeWithResult(glaze::DialogResult::OK);
}

// Övre linan: frågar efter kortslutningstemperaturen och läser in korsningslastfallen.
// Avbryt gör ingenting, och en text som inte är ett tal avvisas.
void LineCalcDialog::upperBtnClicked(glaze::Widget*, glaze::ClickArgs&)
{
    if (readOnly_)
        return;
    const auto answer = glaze::inputBox(
        *this, "Lastfall III (lokal islast) måste beräknas separat. Ange kortslutningstemperaturen för linan (°C).",
        "Övre linan", fmt(lc_.input.shortCircuitTempC, -1, ','));
    if (!answer)
        return;
    const auto value = tabletext::parseDecimal(trim(*answer));
    if (!value) {
        glaze::messageBox(*this, "Kortslutningstemperaturen måste vara ett tal.", "Övre linan",
                          glaze::MessageBoxButtons::OK, glaze::MessageBoxType::Warning);
        return;
    }
    linecalc::loadCrossingUpper(lc_.input, *value);
    dirty_ = true;
    recalc(true);
}

// Nedre linan: läser in korsningslastfallen för nedre linan.
void LineCalcDialog::lowerBtnClicked(glaze::Widget*, glaze::ClickArgs&)
{
    if (readOnly_)
        return;
    linecalc::loadCrossingLower(lc_.input);
    dirty_ = true;
    recalc(true);
}

// Återställ: sätter standardmallen. I en kopplad beräkning behåller de låsta fälten
// Dokumentationens värden, eftersom omräkningen löser upp dem igen.
void LineCalcDialog::resetBtnClicked(glaze::Widget*, glaze::ClickArgs&)
{
    if (readOnly_)
        return;
    linecalc::resetToStandard(lc_.input);
    dirty_ = true;
    recalc(true);
}

// Anteckningen ändrades av användaren.
void LineCalcDialog::noteChanged(glaze::Widget*, glaze::ChangeArgs&)
{
    dirty_ = true;
}

// En lina valdes i rullistan: lindatat fylls med linans värden och allt räknas om.
void LineCalcDialog::conductorChanged(glaze::Widget*, glaze::ChangeArgs&)
{
    if (readOnly_ || !lc_.docSource.empty())
        return;
    chooseConductor(conductorDd.selectedIndex());
    dirty_ = true;
    recalc(true);
}

// Läget byttes. Bara radion som blev vald räknas, och bytet räknar om direkt.
void LineCalcDialog::modeChanged(glaze::Widget* sender, glaze::ChangeArgs&)
{
    auto* radio = static_cast<glaze::Radio*>(sender);
    if (!radio->isChecked())
        return;
    const auto mode = radio == &modeExcelRb ? linecalc::CalcMode::Excel : linecalc::CalcMode::New;
    if (lc_.input.mode == mode)
        return;
    lc_.input.mode = mode;
    diffChk.setText(mode == linecalc::CalcMode::Excel ? "Markera skillnaden mot läget Ny"
                                                       : "Markera skillnaden mot läget Excel");
    dirty_ = true;
    recalc(true);
}

// i-knappen vid läget Ny: öppnar PM:et om felen i Excel-arket som en modal dialog.
// Beräkningen och formuläret påverkas inte.
void LineCalcDialog::modeInfoBtnClicked(glaze::Widget*, glaze::ClickArgs&)
{
    LineCalcInfoDialog dlg;
    dlg.run(*this);
}

// Skillnadsrutan ändrades: bara visningen räknas om, beräkningen ändras inte.
void LineCalcDialog::diffChanged(glaze::Widget*, glaze::ChangeArgs&)
{
    recalc();
}

// Rutan Hämta ur Dokumentation: på kopplar beräkningen till raden, av gör värdena egna.
void LineCalcDialog::docInputChanged(glaze::Widget*, glaze::ChangeArgs&)
{
    if (readOnly_)
        return;
    if (docInputChk.isChecked())
        lc_.docSource = docRowKey_;
    else
        lc_.docSource.clear();
    dirty_ = true;
    updateDocLabel();
    recalc(true);
}

// En cell i Indata sparades av användaren.
void LineCalcDialog::inputGridCommitted(glaze::Widget*, glaze::CellChangeArgs& e)
{
    cellCommitted(kGridInput, inputGrid, e.row, e.column);
}

// En cell i Lindata / Konstanter sparades av användaren.
void LineCalcDialog::constGridCommitted(glaze::Widget*, glaze::CellChangeArgs& e)
{
    cellCommitted(kGridConst, constGrid, e.row, e.column);
}

// En cell i Lastfall sparades av användaren.
void LineCalcDialog::caseGridCommitted(glaze::Widget*, glaze::CellChangeArgs& e)
{
    cellCommitted(kGridCase, caseGrid, e.row, e.column);
}

// Tangenter, mus och snabbmeny i Indata går till tabellens SheetGrid.
void LineCalcDialog::inputGridKeyDown(glaze::Widget*, glaze::KeyArgs& e)
{
    if (sheets_[kGridInput])
        sheets_[kGridInput]->keyDown(e);
}

void LineCalcDialog::inputGridMouseUp(glaze::Widget*, glaze::MouseArgs& e)
{
    if (sheets_[kGridInput])
        sheets_[kGridInput]->mouseUp(e);
}

void LineCalcDialog::inputGridContextMenu(glaze::Widget*, glaze::ContextMenuArgs& e)
{
    if (sheets_[kGridInput])
        sheets_[kGridInput]->contextMenu(e);
}

// Tangenter, mus och snabbmeny i Lindata / Konstanter går till tabellens SheetGrid.
void LineCalcDialog::constGridKeyDown(glaze::Widget*, glaze::KeyArgs& e)
{
    if (sheets_[kGridConst])
        sheets_[kGridConst]->keyDown(e);
}

void LineCalcDialog::constGridMouseUp(glaze::Widget*, glaze::MouseArgs& e)
{
    if (sheets_[kGridConst])
        sheets_[kGridConst]->mouseUp(e);
}

void LineCalcDialog::constGridContextMenu(glaze::Widget*, glaze::ContextMenuArgs& e)
{
    if (sheets_[kGridConst])
        sheets_[kGridConst]->contextMenu(e);
}

// Tangenter, mus och snabbmeny i Lastfall går till tabellens SheetGrid.
void LineCalcDialog::caseGridKeyDown(glaze::Widget*, glaze::KeyArgs& e)
{
    if (sheets_[kGridCase])
        sheets_[kGridCase]->keyDown(e);
}

void LineCalcDialog::caseGridMouseUp(glaze::Widget*, glaze::MouseArgs& e)
{
    if (sheets_[kGridCase])
        sheets_[kGridCase]->mouseUp(e);
}

void LineCalcDialog::caseGridContextMenu(glaze::Widget*, glaze::ContextMenuArgs& e)
{
    if (sheets_[kGridCase])
        sheets_[kGridCase]->contextMenu(e);
}

// Läser beräkningen som ska öppnas (ny, befintlig eller kopia) in i lc_ och Dokumentationens
// värden in i docs_. Svarar false när beräkningen inte finns eller inte gick att läsa,
// efter att felet visats.
bool LineCalcDialog::loadCalculation()
{
    const auto docs = projectdb::docLineSources(*project_);
    if (docs) {
        docs_ = docs.value();
    } else {
        docs_ = {};
        gfx::log("Dokumentationens uppgifter kunde inte läsas: %s", docs.error().detail.c_str());
        glaze::messageBox(*this, "Dokumentationens uppgifter kunde inte läsas." + newline() + docs.error().message,
                          "Linberäkning", glaze::MessageBoxButtons::OK, glaze::MessageBoxType::Warning);
    }

    const std::int64_t id = existingId_ != 0 ? existingId_ : copyOfId_;
    lc_ = {};
    existingDocKeys_.clear();
    automatic_.reset();
    if (id != 0) {
        const auto loaded = projectdb::lineCalc(*project_, id);
        if (!loaded) {
            glaze::messageBox(*this, loaded.error().message, "Linberäkning", glaze::MessageBoxButtons::OK,
                              glaze::MessageBoxType::Error);
            return false;
        }
        if (!loaded.value()) {
            glaze::messageBox(*this, "Linberäkningen finns inte längre i projektet.", "Linberäkning",
                              glaze::MessageBoxButtons::OK, glaze::MessageBoxType::Warning);
            return false;
        }
        lc_ = *loaded.value();
        if (existingId_ != 0) {
            existingDocKeys_ = lc_.docKeys;
            // En automatisk beräkning visas bara, den ändras i stolptabellen.
            automatic_ = lc_.automatic;
            if (automatic_)
                readOnly_ = true;
        } else {
            lc_.automatic.reset();
            // En kopia är lös och manuell, och en kopplad beräkning behåller sina upplösta värden.
            if (!lc_.docSource.empty())
                lc_.input = projectdb::resolveLineInput(lc_.input, lc_.docSource, docs_).input;
            std::string note = trim(lc_.note);
            if (note.empty() && lc_.input.conductor && !lc_.input.conductor->name.empty())
                note = lc_.input.conductor->name;
            lc_.note = note.empty() ? std::string("Kopia") : "Kopia av " + note;
            lc_.id = 0;
            lc_.docSource.clear();
            lc_.docKeys.clear();
        }
    } else {
        lc_.input = linecalc::newLineInput();
        if (newFromDoc_)
            lc_.docSource = linkDocKey_;
        else if (!linkDocKey_.empty())
            lc_.input = projectdb::resolveLineInput(lc_.input, linkDocKey_, docs_).input;
    }
    noteTxt.setText(lc_.note);
    return true;
}

// Bygger posterna i rullistan Lina: registrets linor, projektets linor och beräkningens egen,
// ordnade fallande som i Dokumentation. Första posten är "(utan namn)".
void LineCalcDialog::buildConductorChoices()
{
    choices_.clear();
    const auto find = [this](const std::string& name) -> ConductorChoice* {
        for (auto& c : choices_)
            if (conductor::sameConductorName(c.name, name))
                return &c;
        return nullptr;
    };
    const auto ensure = [&](const std::string& name) -> ConductorChoice& {
        if (auto* c = find(name))
            return *c;
        choices_.push_back({name, {}, {}, {}});
        return choices_.back();
    };

    if (sys_) {
        const auto registry = sys_->conductors();
        if (registry)
            for (const auto& c : registry.value())
                ensure(c.name).registry = c;
    }
    const auto project = projectdb::projectConductors(*project_);
    if (project)
        for (const auto& c : project.value())
            ensure(c.name).project = c;
    if (lc_.input.conductor && !lc_.input.conductor->name.empty())
        ensure(lc_.input.conductor->name).own = *lc_.input.conductor;

    std::stable_sort(choices_.begin(), choices_.end(), [](const ConductorChoice& a, const ConductorChoice& b) {
        return conductor::compareNames(a.name, b.name) > 0;
    });

    std::vector<std::string> items;
    items.push_back("(utan namn)");
    for (const auto& c : choices_)
        items.push_back(c.name);
    conductorDd.setItems(std::move(items));
}

// Väljer posten i rullistan som hör till lindatats namn. Utan namn men med lindata väljs
// "(utan namn)", och utan båda står rullistan tom.
void LineCalcDialog::selectConductorItem()
{
    int index = -1;
    if (!conductorName_.empty()) {
        for (std::size_t i = 0; i < choices_.size(); ++i)
            if (conductor::sameConductorName(choices_[i].name, conductorName_))
                index = static_cast<int>(i) + 1;
    } else if (std::any_of(lindata_.begin(), lindata_.end(), [](const auto& v) { return v.has_value(); })) {
        index = 0;
    }
    conductorDd.setSelectedIndex(index);
}

// Fyller lindatats celler ur en lina, eller tömmer dem. Namn, typ och krypning följer med
// men visas inte som celler.
void LineCalcDialog::loadConductorCells(const std::optional<conductor::Conductor>& c)
{
    if (c) {
        lindata_ = lindataOf(*c);
        conductorName_ = c->name;
        conductorType_ = c->type;
        creepStrain_ = c->creepStrainPermille;
    } else {
        lindata_ = {};
        conductorName_.clear();
        conductorType_.clear();
        creepStrain_ = 0;
    }
}

// Läser lindatats celler till en lina. Saknas något av de nio talen svarar funktionen tomt
// och missing får antalet. Brottlasten får saknas. Utan namn och tal alls finns inget lindata.
std::optional<conductor::Conductor> LineCalcDialog::readConductor(int& missing) const
{
    missing = 0;
    const bool any = !conductorName_.empty() ||
                     std::any_of(lindata_.begin(), lindata_.end(), [](const auto& v) { return v.has_value(); });
    if (!any)
        return std::nullopt;
    for (std::size_t i = 0; i < 9; ++i)
        if (!lindata_[i])
            ++missing;
    if (missing > 0)
        return std::nullopt;
    conductor::Conductor c;
    c.origin = conductor::ConductorOrigin::Own;
    c.name = conductorName_;
    c.type = conductorType_;
    c.areaMm2 = *lindata_[0];
    c.diameterMm = *lindata_[1];
    c.eInitialMpa = *lindata_[2];
    c.ePermanentMpa = *lindata_[3];
    c.eInitialLimitMpa = *lindata_[4];
    c.eivConstant = *lindata_[5];
    c.alphaE6PerC = *lindata_[6];
    c.creepTempC = *lindata_[7];
    c.weightNPerM = *lindata_[8];
    c.creepStrainPermille = creepStrain_;
    c.breakingLoadKn = lindata_[9];
    return c;
}

// Linan som lindatat jämförs med: registrets, annars projektets kopia. origin får
// "Linregistret" eller "Projektets kopia". Svarar null när linan bara finns i beräkningen.
const conductor::Conductor* LineCalcDialog::referenceConductor(std::string& origin) const
{
    if (conductorName_.empty())
        return nullptr;
    for (const auto& c : choices_) {
        if (!conductor::sameConductorName(c.name, conductorName_))
            continue;
        if (c.registry) {
            origin = "Linregistret";
            return &*c.registry;
        }
        if (c.project) {
            origin = "Projektets kopia";
            return &*c.project;
        }
    }
    return nullptr;
}

// Radtexten i Dokumentation för nyckeln, eller nyckeln själv när texten saknas.
std::string LineCalcDialog::docRowLabel(const std::string& key) const
{
    for (const auto& entry : docLabels_)
        if (entry.first == key)
            return entry.second;
    return key;
}

// Skriver upplysningen om var beräkningen visas i Dokumentation, och om att linan, aₙ, σ₀
// och temperaturerna är Dokumentationens när beräkningen är kopplad. En automatisk
// beräkning får i stället en text om stolptabellen den räknas ur.
void LineCalcDialog::updateDocLabel()
{
    std::string text;
    if (automatic_) {
        text = "Automatisk linberäkning ur " + std::string(projectdb::poleTableName(automatic_->stage)) +
               ". Linan, σ₀ och temperaturerna är Dokumentationens, och aₙ kommer ur den automatiska "
               "normalspannsberäkningen (" + automatic_->range.fromPole + " – " + automatic_->range.toPole +
               "). Den kan inte ändras eller tas bort.";
        docLbl.setWrap(true);   // texten är lång: bryts mot bredden
        docLbl.setText(text);
        return;
    }
    if (existingId_ != 0) {
        if (existingDocKeys_.empty()) {
            text = "Visas inte i Dokumentation. Koppla den med högerklick på en rad Linberäkning … i fliken Dokumentation.";
        } else {
            std::string joined;
            for (const auto& key : existingDocKeys_) {
                if (!joined.empty())
                    joined += ", ";
                joined += docRowLabel(key);
            }
            text = "Visas i Dokumentation på: " + joined;
        }
    } else if (copyOfId_ == 0 && !linkDocKey_.empty()) {
        text = "Visas på raden " + docRowLabel(linkDocKey_) + " när du trycker OK.";
    } else {
        text = "Blir en lös beräkning i fliken Linberäkning.";
    }
    if (!lc_.docSource.empty())
        text += " Linan, aₙ, σ₀ och temperaturerna är Dokumentationens och ändras där.";
    else if (existingId_ == 0 && copyOfId_ == 0 && !linkDocKey_.empty())
        text += " Indata är kopierade ur Dokumentation och följer inte ändringar där. Allt går att ändra.";
    docLbl.setText(text);
}

// Bygger tabellernas rader, cellernas särskilda redigerare och etiketterna, och skapar
// en SheetGrid per tabell. Raderna sätts en gång: setRows nollar cellinställningarna.
void LineCalcDialog::buildTables()
{
    for (int g = 0; g < 3; ++g) {
        const std::size_t n = static_cast<std::size_t>(kGridRows[g] * kGridCols[g]);
        info_[static_cast<std::size_t>(g)].assign(n, CellInfo{});
        locked_[static_cast<std::size_t>(g)].assign(n, 0);
    }

    // Indata och Lindata / Konstanter: etiketterna i kolumn 0 och 2, värdena fylls av recalc.
    for (int g = kGridInput; g <= kGridConst; ++g) {
        std::vector<glaze::DataGrid::Row> rows;
        for (int r = 0; r < kGridRows[g]; ++r) {
            glaze::DataGrid::Row row;
            row.cells = {labelAt(g, r, 0), "", labelAt(g, r, 2), ""};
            rows.push_back(std::move(row));
        }
        tableOf(g).setRows(std::move(rows));
        for (int r = 0; r < kGridRows[g]; ++r)
            for (int c = 1; c <= 3; c += 2)
                tableOf(g).setCellEditor(r, c, specFor(g, r, c, false));
    }
    constGrid.setCellToolTip(7, 0, "Permanent förlängning pga krypning uttryckt i temperaturhöjning, Tₚ (°C)");

    // Lastfall: 16 rader med tomraden som rad 7.
    std::vector<glaze::DataGrid::Row> rows;
    for (int r = 0; r < kGridRows[kGridCase]; ++r) {
        glaze::DataGrid::Row row;
        row.cells.assign(26, std::string());
        if (r == kBlankRow)
            row.kind = glaze::DataGrid::RowKind::Blank;
        rows.push_back(std::move(row));
    }
    caseGrid.setRows(std::move(rows));

    // Varje tabell får en SheetGrid med sin form och sin värd.
    for (int g = 0; g < 3; ++g) {
        sheetgrid::SheetShape shape;
        shape.rowCount = kGridRows[g];
        shape.columnCount = kGridCols[g];
        shape.firstColumn = g == kGridCase ? 1 : 0;
        shape.skipRow = [g](int row) { return g == kGridCase && row == kBlankRow; };
        shape.isInput = [this, g](int row, int column) { return isInputCell(g, row, column); };
        shape.accept = [this, g](int row, int column, const std::string& field) {
            return acceptField(g, row, column, field);
        };
        shape.columnName = [this, g](int column) {
            if (g == kGridCase)
                return std::string(kCaseNames[column]);
            return cellName(g, 0, column);
        };
        hosts_[g] = std::make_unique<GridHost>(*this, g);
        sheets_[static_cast<std::size_t>(g)] =
            std::make_unique<sheetgrid::SheetGrid>(tableOf(g), std::move(shape), *hosts_[g]);
    }
}

// Räknar om allt efter en ändring: lindatat, upplösningen ur Dokumentation, beräkningen,
// cellernas text, märken och lås, färgerna och statusraden. clearErrors tar bort
// ogiltiga celltexter, för när modellen byts ut i bulk.
void LineCalcDialog::recalc(bool clearErrors)
{
    for (int g = 0; g < 3; ++g)
        info_[static_cast<std::size_t>(g)].assign(static_cast<std::size_t>(kGridRows[g] * kGridCols[g]), CellInfo{});

    // Lindatat ur cellerna, för en manuell beräkning.
    lindataMissing_ = 0;
    if (lc_.docSource.empty())
        lc_.input.conductor = readConductor(lindataMissing_);

    // En kopplad beräkning löses upp igen varje gång, så att de låsta fälten behåller
    // Dokumentationens värden. En manuell beräkning har en tom upplösning.
    res_ = projectdb::ResolvedLineInput{lc_.input, {}, {}};
    if (!lc_.docSource.empty()) {
        res_ = projectdb::resolveLineInput(lc_.input, lc_.docSource, docs_);
        lc_.input = res_.input;
        loadConductorCells(lc_.input.conductor);
        selectConductorItem();
    }
    calc_ = projectdb::calculateResolved(res_);

    // Det andra läget, bara när skillnaden visas.
    other_ = {};
    otherInput_ = res_.input;
    if (diffChk.isChecked()) {
        otherInput_.mode = res_.input.mode == linecalc::CalcMode::Excel ? linecalc::CalcMode::New
                                                                        : linecalc::CalcMode::Excel;
        projectdb::ResolvedLineInput otherRes{otherInput_, res_.issues, res_.sources};
        other_ = projectdb::calculateResolved(otherRes);
    }

    if (readOnly_ || !lc_.docSource.empty())
        conductorDd.disable();
    else
        conductorDd.enable();

    fillInputCells(clearErrors);
    fillConstCells(clearErrors);
    fillCaseCells(clearErrors);

    markIssues();
    markDifferences();
    markDocFields();
    markConductorDiffs();
    paintAllSheets();
    updateStatus();
}

// Skriver en celltext om den ändrats. En cell som visar ett ogiltigt tal rörs inte, utom när
// clearErrors är satt, så att användarens text inte försvinner vid en omräkning.
void LineCalcDialog::putCell(int grid, int row, int column, const std::string& text, bool clearErrors)
{
    glaze::DataGrid& table = tableOf(grid);
    const bool bad = table.hasError(row, column);
    if (bad && !clearErrors)
        return;
    if (bad || table.cell(row, column) != text)
        table.setCell(row, column, text);
}

// Märker cellen. Det lägsta märket gäller, och tooltipen följer märket som gäller.
// En tooltip utan färg sätts bara när cellen inte har någon annan.
void LineCalcDialog::markCell(int grid, int row, int column, int mark, const std::string& tip)
{
    if (grid < 0 || grid > 2 || row < 0 || row >= kGridRows[grid] || column < 0 || column >= kGridCols[grid])
        return;
    CellInfo& info = info_[static_cast<std::size_t>(grid)][static_cast<std::size_t>(row * kGridCols[grid] + column)];
    if (mark == kMarkTip) {
        if (info.tip.empty())
            info.tip = tip;
        return;
    }
    if (info.mark == 0 || mark < info.mark) {
        info.mark = mark;
        info.tip = tip;
    }
}

// Skriver cellerna i Indata: värdena, låsen för det som hämtas ur Dokumentation, tooltips
// för avrundade värden och etikettfärgen.
void LineCalcDialog::fillInputCells(bool clearErrors)
{
    const bool linked = !lc_.docSource.empty();
    for (int r = 0; r < kGridRows[kGridInput]; ++r) {
        for (int c = 0; c < 4; ++c) {
            const Slot slot = slotAt(kGridInput, r, c);
            if (c == 0 || c == 2) {
                if (!labelAt(kGridInput, r, c).empty())
                    markCell(kGridInput, r, c, kMarkLabel, {});
                continue;
            }
            if (slot == Slot::None)
                continue;
            const auto value = getSlot(res_.input, lindata_, slot);
            putCell(kGridInput, r, c, value ? fmt(*value, slotDecimals(slot), '.') : std::string(), clearErrors);
            const bool locked = readOnly_ || (linked && (slot == Slot::NormalSpan || slot == Slot::Tension ||
                                                         slot == Slot::MinTemp || slot == Slot::MaxTemp));
            applyLock(kGridInput, r, c, locked);
        }
    }
}

// Skriver cellerna i Lindata / Konstanter: värdena, låsen (lindatat är alltid låst i en
// kopplad beräkning), tooltips för avrundade värden och etikettfärgen.
void LineCalcDialog::fillConstCells(bool clearErrors)
{
    const bool linked = !lc_.docSource.empty();
    for (int r = 0; r < kGridRows[kGridConst]; ++r) {
        for (int c = 0; c < 4; ++c) {
            const Slot slot = slotAt(kGridConst, r, c);
            if (c == 0 || c == 2) {
                if (!labelAt(kGridConst, r, c).empty())
                    markCell(kGridConst, r, c, kMarkLabel, {});
                continue;
            }
            if (slot == Slot::None)
                continue;
            // D10: läget Ny räknar med exakt pi. Cellen visar tecknet π i en låst textcell;
            // låset återställer talfältet när läget byts tillbaka till Excel.
            const bool exactPi = slot == Slot::Pi && res_.input.mode == linecalc::CalcMode::New;
            if (exactPi) {
                applyLock(kGridConst, r, c, true);
                if (constGrid.cell(r, c) != "π") {
                    glaze::DataGrid::EditorSpec text;
                    text.editor = glaze::DataGrid::Editor::Text;
                    text.readOnly = true;
                    constGrid.setCellEditor(r, c, text);
                    constGrid.setCell(r, c, "π");
                }
                info_[kGridConst][static_cast<std::size_t>(r * 4 + c)].baseTip =
                    "Läget Ny räknar med exakt π. Arkets 3,1416 gäller i läget Excel.";
                continue;
            }
            const auto value = getSlot(res_.input, lindata_, slot);
            const int d = slotDecimals(slot);
            applyLock(kGridConst, r, c, readOnly_ || (linked && isLindataSlot(slot)));
            putCell(kGridConst, r, c, value ? fmt(*value, d, '.') : std::string(), clearErrors);
            if (value && d >= 0) {
                const std::string full = fmt(*value, -1, ',');
                if (full != fmt(*value, d, ','))
                    info_[kGridConst][static_cast<std::size_t>(r * 4 + c)].baseTip = "Värde: " + full;
            }
        }
    }
}

// Skriver cellerna i Lastfall ur indata och resultat: texten, låsen i lastfall med manuell
// last, tooltips med fullt värde och källor, jämna lastfall och dämpade rader utan benämning.
void LineCalcDialog::fillCaseCells(bool clearErrors)
{
    const linecalc::LineInput& in = res_.input;
    for (int r = 0; r < kGridRows[kGridCase]; ++r) {
        if (r == kBlankRow)
            continue;
        const int id = caseIdOfRow(r);
        const linecalc::LoadCase& lcase = in.cases[static_cast<std::size_t>(id - 1)];
        const bool manual = lcase.manual.has_value();
        for (int c = 0; c < 26; ++c) {
            const CellVal v = caseCellVal(in, calc_, id, c);
            const char kind = kCaseKind[c];
            const char sep = (kind == 'N' || kind == 'I') ? '.' : ',';
            putCell(kGridCase, r, c, valText(v, sep), clearErrors);
            CellInfo& info = info_[kGridCase][static_cast<std::size_t>(r * 26 + c)];
            if (v.numeric && v.present && v.decimals >= 0) {
                const std::string full = fmt(v.value, -1, ',');
                if (full != fmt(v.value, v.decimals, ','))
                    info.baseTip = "Värde: " + full;
            }
            if (in.mode == linecalc::CalcMode::New) {
                if (c == 1)
                    info.baseTip = tempSourceTip(lcase.tempSource).empty() ? info.baseTip : tempSourceTip(lcase.tempSource);
                if (c == 9 && !manual)
                    info.baseTip = pressureSourceTip(lcase.pressureSource).empty()
                                       ? info.baseTip
                                       : pressureSourceTip(lcase.pressureSource);
            }
            if (manual && isManualBlankColumn(c))
                info.baseTip = "Lastfallet har manuell last. Högerklicka och välj Räkna last ur vind och is för att räkna med vind och is igen.";
            if (manual && (c == 15 || c == 16))
                markCell(kGridCase, r, c, kMarkManual, {});
            if (kind != '0')
                applyLock(kGridCase, r, c, readOnly_ || (manual && isManualLockedColumn(c)));
            if (id % 2 == 0)
                markCell(kGridCase, r, c, kMarkEven, {});
        }
        // Utnyttjandegraden över gränsen är röd.
        if (calc_.result && calc_.result->cases[static_cast<std::size_t>(id - 1)].overUtilization)
            markCell(kGridCase, r, 21, kMarkOver, "Utnyttjandegraden är över den tillåtna.");
        caseGrid.setRowColor(r, lcase.name.empty() ? std::optional<gfx::Color>(kDimText) : std::nullopt);
    }
}

// Märker cellerna som upplysningarna gäller: fel, saknade värden och, utan färg, varningar.
// Dessutom märks celler med ogiltigt tal. Tooltipen är upplysningens text.
void LineCalcDialog::markIssues()
{
    std::string conductorTip;
    for (const auto& issue : calc_.issues) {
        if (issue.kind == linecalc::LineIssueKind::Warning)
            continue;
        const int mark = issue.kind == linecalc::LineIssueKind::Error ? kMarkError : kMarkMissing;
        for (const CellRef& ref : cellsOfIssue(issue, lindata_, res_.input.conductor))
            markCell(ref.grid, ref.row, ref.col, mark, ref.tip.empty() ? issue.message : ref.tip);
        if (issue.field == linecalc::LineField::Conductor) {
            if (!conductorTip.empty())
                conductorTip += newline();
            conductorTip += issue.message;
        }
    }
    for (const auto& issue : calc_.issues) {
        if (issue.kind != linecalc::LineIssueKind::Warning)
            continue;
        for (const CellRef& ref : cellsOfIssue(issue, lindata_, res_.input.conductor))
            markCell(ref.grid, ref.row, ref.col, kMarkTip, issue.message);
    }
    conductorDd.setToolTip(conductorTip);

    // Ogiltiga tal i cellerna: röda, och den första nämns i statusraden.
    invalidText_.clear();
    for (int g = 0; g < 3; ++g) {
        for (int r = 0; r < kGridRows[g]; ++r) {
            for (int c = 0; c < kGridCols[g]; ++c) {
                if (!tableOf(g).hasError(r, c))
                    continue;
                markCell(g, r, c, kMarkError, "Ogiltigt tal.");
                if (invalidText_.empty())
                    invalidText_ = cellName(g, r, c);
            }
        }
    }
}

// Jämför de två lägena cell för cell på den visade texten. En cell som skiljer sig får märket
// Skillnad och en tooltip med båda värdena.
void LineCalcDialog::markDifferences()
{
    diffCount_ = 0;
    if (!diffChk.isChecked() || !calc_.result || !other_.result)
        return;
    const bool excelNow = res_.input.mode == linecalc::CalcMode::Excel;
    for (int id = 1; id <= linecalc::kLoadCaseCount; ++id) {
        for (int c = 1; c < 26; ++c) {
            if (!isDiffColumn(c))
                continue;
            const std::string mine = valText(caseCellVal(res_.input, calc_, id, c), ',');
            const std::string theirs = valText(caseCellVal(otherInput_, other_, id, c), ',');
            if (mine == theirs)
                continue;
            ++diffCount_;
            const std::string& ny = excelNow ? theirs : mine;
            const std::string& excel = excelNow ? mine : theirs;
            markCell(kGridCase, rowOfCaseId(id), c, kMarkDiff, "Ny: " + ny + " · Excel: " + excel);
        }
    }
}

// Märker de fält i en kopplad beräkning som hämtas ur Dokumentation, med en tooltip om varifrån.
void LineCalcDialog::markDocFields()
{
    if (lc_.docSource.empty())
        return;
    for (const auto& src : res_.sources) {
        const std::string tip = "Hämtas ur Dokumentation: " + docRowLabel(src.docKey) + ". Ändra det där.";
        switch (src.field) {
        case projectdb::LineSourceField::Conductor:
            for (int i = 0; i < 10; ++i)
                if (lindata_[static_cast<std::size_t>(i)])
                    markCell(kGridConst, i, 1, kMarkDoc, tip);
            break;
        case projectdb::LineSourceField::NormalSpan: markCell(kGridInput, 0, 1, kMarkDoc, tip); break;
        case projectdb::LineSourceField::Tension: markCell(kGridInput, 1, 1, kMarkDoc, tip); break;
        case projectdb::LineSourceField::MinTemp: markCell(kGridInput, 3, 1, kMarkDoc, tip); break;
        case projectdb::LineSourceField::MaxTemp: markCell(kGridInput, 4, 1, kMarkDoc, tip); break;
        }
    }
}

// Märker lindatacellerna som skiljer sig från samma lina i linregistret (eller projektet), och
// skriver texten vid rullistan om linans ursprung. Kopplad beräkning har sina egna märken.
void LineCalcDialog::markConductorDiffs()
{
    std::string info;
    std::string origin;
    const conductor::Conductor* ref = lc_.docSource.empty() ? referenceConductor(origin) : nullptr;
    if (!lc_.docSource.empty()) {
        std::string label = docRowLabel(lc_.docSource);
        for (const auto& src : res_.sources)
            if (src.field == projectdb::LineSourceField::Conductor)
                label = docRowLabel(src.docKey);
        info = "Ur Dokumentation: " + label;
    } else if (ref) {
        const Lindata refData = lindataOf(*ref);
        bool differs = false;
        for (std::size_t i = 0; i < refData.size(); ++i) {
            if (sameNumber(lindata_[i], refData[i]))
                continue;
            differs = true;
            markCell(kGridConst, static_cast<int>(i), 1, kMarkChanged,
                     origin + ": " + (refData[i] ? fmt(*refData[i], -1, ',') : std::string("-")));
        }
        if (differs)
            info = origin == "Linregistret" ? "Ändrat för hand. Linregistret har andra värden."
                                            : "Ändrat för hand. Projektets kopia har andra värden.";
        else if (origin != "Linregistret")
            info = "Projektets kopia. Linan finns inte i ditt linregister.";
    } else if (!conductorName_.empty()) {
        info = "Beräkningens egen kopia. Linan finns inte i linregistret eller i projektet.";
    } else if (std::any_of(lindata_.begin(), lindata_.end(), [](const auto& v) { return v.has_value(); })) {
        info = "Lindata utan lina";
    }
    conductorInfo.setText(info);
}

// Låser eller låser upp en cell: redigeraren får readOnly. Bara en ändring skrivs.
void LineCalcDialog::applyLock(int grid, int row, int column, bool locked)
{
    auto& state = locked_[static_cast<std::size_t>(grid)];
    const std::size_t index = static_cast<std::size_t>(row * kGridCols[grid] + column);
    const char want = locked ? 1 : 0;
    if (state[index] == want)
        return;
    state[index] = want;
    tableOf(grid).setCellEditor(row, column, specFor(grid, row, column, locked));
}

// Skriver tooltips och målar om alla tre tabellerna ur märkena.
void LineCalcDialog::paintAllSheets()
{
    for (int g = 0; g < 3; ++g) {
        glaze::DataGrid& table = tableOf(g);
        for (int r = 0; r < kGridRows[g]; ++r) {
            if (g == kGridCase && r == kBlankRow)
                continue;
            for (int c = 0; c < kGridCols[g]; ++c) {
                if (g != kGridCase && (c == 0 || c == 2))
                    continue;
                const CellInfo& info = info_[static_cast<std::size_t>(g)][static_cast<std::size_t>(r * kGridCols[g] + c)];
                table.setCellToolTip(r, c, info.tip.empty() ? info.baseTip : info.tip);
            }
        }
        if (sheets_[static_cast<std::size_t>(g)])
            sheets_[static_cast<std::size_t>(g)]->paintAll();
    }
}

// Skriver statusraden: det första felet, annars den första saknade uppgiften, annars den
// första varningen, annars stakningsmallen och högsta utnyttjandegraden. Tooltipen listar allt.
void LineCalcDialog::updateStatus()
{
    const auto describe = [](const linecalc::LineIssue& issue) {
        if (issue.caseId > 0)
            return "Lastfall " + std::to_string(issue.caseId) + ": " + issue.message;
        return issue.message;
    };
    const auto& issues = calc_.issues;
    const auto first = [&](linecalc::LineIssueKind kind) -> const linecalc::LineIssue* {
        for (const auto& issue : issues)
            if (issue.kind == kind)
                return &issue;
        return nullptr;
    };
    const std::string more = issues.size() > 1 ? " (och " + std::to_string(issues.size() - 1) + " till)" : std::string();

    std::string text;
    if (const auto* e = first(linecalc::LineIssueKind::Error)) {
        text = describe(*e) + more;
    } else if (!invalidText_.empty()) {
        text = "Ogiltigt tal i " + invalidText_ + ".";
    } else if (const auto* m = first(linecalc::LineIssueKind::Missing)) {
        text = describe(*m) + more;
    } else if (const auto* w = first(linecalc::LineIssueKind::Warning)) {
        text = describe(*w) + more;
    } else if (calc_.result) {
        const linecalc::LineResult& res = *calc_.result;
        text = "Stakningsmall " + fmt(std::nearbyint(res.stakingMall), 0, ',') + " · Högsta utnyttjandegrad ";
        double best = -1;
        for (const auto& c : res.cases)
            if (c.utilizationPercent)
                best = std::max(best, *c.utilizationPercent);
        if (best < 0) {
            text += "-";
        } else {
            std::string ids;
            for (const auto& c : res.cases) {
                if (c.utilizationPercent && std::fabs(*c.utilizationPercent - best) < 0.005) {
                    if (!ids.empty())
                        ids += ", ";
                    ids += std::to_string(c.id);
                }
            }
            text += fmt(std::nearbyint(best), 0, ',') + " % (lastfall " + ids + ")";
        }
        text += std::string(" · Läge ") + modeName(res.mode);
        if (diffChk.isChecked())
            text += " · " + std::to_string(diffCount_) + " celler skiljer sig mot läget " +
                    modeName(otherInput_.mode);
    }
    if (diffChk.isChecked() && calc_.result && !other_.result) {
        const linecalc::LineIssue* why = nullptr;
        for (const auto& issue : other_.issues)
            if (issue.kind != linecalc::LineIssueKind::Warning) {
                why = &issue;
                break;
            }
        text += std::string(" · Läget ") + modeName(otherInput_.mode) + " ger ingen beräkning" +
                (why ? ": " + describe(*why) : std::string()) + ".";
    }
    setStatus(text);

    // Tooltipen: alla upplysningar, och Excel-lägets varningar när Ny är valt.
    std::string tip;
    for (const auto& issue : issues) {
        if (!tip.empty())
            tip += newline();
        tip += describe(issue);
    }
    if (res_.input.mode == linecalc::CalcMode::New && diffChk.isChecked()) {
        std::string warnings;
        for (const auto& issue : other_.issues) {
            if (issue.kind != linecalc::LineIssueKind::Warning)
                continue;
            warnings += newline() + describe(issue);
        }
        if (!warnings.empty())
            tip += (tip.empty() ? std::string() : newline()) + "Läget Excel" + std::string(1, ':') + warnings;
    }
    statusBar.setToolTip(tip);
}

// Skriver en egen text i statusraden.
void LineCalcDialog::setStatus(const std::string& text)
{
    statusBar.setPanelText(0, text);
}

// Skriver en cells värde i modellen ur dess text (kanonisk form eller tom). Svarar sant när
// något skrevs. changes samlar min, max, Tks, Normal vind och Hög vind som kräver följdändringar
// i lastfallen, i den ordningen.
bool LineCalcDialog::writeCell(int grid, int row, int column, const std::string& text, std::array<bool, 5>& changes)
{
    const std::optional<double> value = tabletext::parseDecimal(trim(text));
    if (grid != kGridCase) {
        const Slot slot = slotAt(grid, row, column);
        if (slot == Slot::None)
            return false;
        const std::optional<double> before = getSlot(lc_.input, lindata_, slot);
        setSlot(lc_.input, lindata_, slot, isOptionalSlot(slot) ? value : std::optional<double>(value.value_or(0)));
        const std::optional<double> after = getSlot(lc_.input, lindata_, slot);
        if (sameNumber(before, after))
            return false;
        if (slot == Slot::MinTemp)
            changes[0] = true;
        else if (slot == Slot::MaxTemp)
            changes[1] = true;
        else if (slot == Slot::ShortCircuit)
            changes[2] = true;
        else if (slot == Slot::NormalWind)
            changes[3] = true;
        else if (slot == Slot::HighWind)
            changes[4] = true;
        return true;
    }

    const int id = caseIdOfRow(row);
    if (id == 0)
        return false;
    linecalc::LoadCase& c = lc_.input.cases[static_cast<std::size_t>(id - 1)];
    const auto shown = [&](bool ice) {
        if (!calc_.result)
            return 0.0;
        const auto& r = calc_.result->cases[static_cast<std::size_t>(id - 1)];
        return ice ? r.iceNPerM : r.windNPerM;
    };
    switch (column) {
    case 1:
        c.tempSource = linecalc::TempSource::Fixed;
        c.tempC = value.value_or(0);
        return true;
    case 2: c.name = trim(text); return true;
    case 4: c.standardCase = trim(text); return true;
    case 5: c.clearanceCase = trim(text); return true;
    case 6:
        c.windFromCase = value ? std::optional<int>(static_cast<int>(std::lround(*value))) : std::nullopt;
        return true;
    case 8: c.windMagnitudePercent = value.value_or(0); return true;
    case 9:
        c.pressureSource = linecalc::PressureSource::Fixed;
        c.charPressureNPerM2 = value.value_or(0);
        return true;
    case 10: c.refHeightM = value.value_or(0); return true;
    case 11: c.pressureGradientNPerM3 = value.value_or(0); return true;
    case 13: c.iceThicknessMm = value.value_or(0); return true;
    case 15:
    case 16: {
        if (!value && !c.manual)
            return false;
        linecalc::ManualLoad m = c.manual ? *c.manual : linecalc::ManualLoad{shown(true), shown(false)};
        (column == 15 ? m.iceNPerM : m.windNPerM) = value.value_or(0);
        c.manual = m;
        return true;
    }
    default: break;
    }
    return false;
}

// Tar emot ändrade celler (inklistring, Delete, Ctrl+X): skriver dem i modellen, kör
// följdändringarna i arket en gång och räknar om en gång.
void LineCalcDialog::applyEditsTo(int grid, const std::vector<sheetgrid::CellEdit>& edits)
{
    if (readOnly_)
        return;
    std::array<bool, 5> changes{};
    bool any = false;
    for (const auto& edit : edits)
        any = writeCell(grid, edit.row, edit.column, edit.text, changes) || any;
    if (!any)
        return;
    applyChanges(changes);
    dirty_ = true;
    recalc(true);
}

// En cell sparades av användaren: värdet läses ur tabellen och skrivs i modellen.
// En cell med ogiltigt tal ändrar ingenting.
void LineCalcDialog::cellCommitted(int grid, glaze::DataGrid& table, int row, int column)
{
    if (readOnly_ || table.hasError(row, column))
        return;
    std::string text;
    if (specFor(grid, row, column, false).editor == glaze::DataGrid::Editor::Number) {
        const auto value = table.numberValue(row, column);
        text = value ? fmt(*value, -1, '.') : std::string();
    } else {
        text = table.cell(row, column);
    }
    std::array<bool, 5> changes{};
    writeCell(grid, row, column, text, changes);
    applyChanges(changes);
    dirty_ = true;
    recalc();
}

// Kör arkets följdändringar för det som ändrats: min och max till lastfall 1, 2 och 15
// (läget Excel) eller källorna (läget Ny), Tks och vindtrycken.
void LineCalcDialog::applyChanges(const std::array<bool, 5>& changes)
{
    static constexpr linecalc::InputChange kOrder[5] = {
        linecalc::InputChange::MinTemp, linecalc::InputChange::MaxTemp,
        linecalc::InputChange::ShortCircuitTemp, linecalc::InputChange::NormalWind,
        linecalc::InputChange::HighWind};
    for (std::size_t i = 0; i < 5; ++i)
        if (changes[i])
            linecalc::applyInputChange(lc_.input, kOrder[i]);
}

// Tar bort den manuella lasten i lastfallsraderna row0 till row1, så att lastfallen räknas
// ur vind och is igen.
void LineCalcDialog::clearManualLoad(int row0, int row1)
{
    if (readOnly_)
        return;
    bool any = false;
    for (int r = row0; r <= row1; ++r) {
        const int id = caseIdOfRow(r);
        if (id > 0 && lc_.input.cases[static_cast<std::size_t>(id - 1)].manual) {
            lc_.input.cases[static_cast<std::size_t>(id - 1)].manual.reset();
            any = true;
        }
    }
    if (!any)
        return;
    dirty_ = true;
    recalc();
}

// Väljer posten i rullistan. "(utan namn)" tar bort namnet men behåller lindatat. En lina
// fyller lindatat med projektets kopia, annars registrets lina, annars beräkningens egen.
void LineCalcDialog::chooseConductor(int itemIndex)
{
    if (itemIndex <= 0) {
        conductorName_.clear();
        conductorType_.clear();
        creepStrain_ = 0;
        return;
    }
    if (static_cast<std::size_t>(itemIndex) > choices_.size())
        return;
    const ConductorChoice& choice = choices_[static_cast<std::size_t>(itemIndex - 1)];
    const std::optional<conductor::Conductor>& src =
        choice.project ? choice.project : (choice.registry ? choice.registry : choice.own);
    loadConductorCells(src);
}

// Cellens namn i meddelanden: etiketten i Indata och Lindata / Konstanter, och i Lastfall
// lastfallets nummer och kolumnens namn.
std::string LineCalcDialog::cellName(int grid, int row, int column) const
{
    if (grid == kGridCase) {
        const std::string name = column >= 0 && column < 26 ? kCaseNames[column] : std::string();
        return "lastfall " + std::to_string(caseIdOfRow(row)) + ", " + name;
    }
    return labelAt(grid, row, column);
}

// Texten en cell kopieras som: talet med fullt antal decimaler och decimalkomma, annars
// cellens text. Låsta manuella celler och tomraden ger tom text.
std::string LineCalcDialog::copyTextOf(int grid, int row, int column) const
{
    if (grid == kGridCase) {
        const int id = caseIdOfRow(row);
        if (id == 0 || column < 0 || column >= 26)
            return {};
        return valCopyText(caseCellVal(res_.input, calc_, id, column));
    }
    const Slot slot = slotAt(grid, row, column);
    if (slot == Slot::None)
        return tableOf(grid).cell(row, column);
    if (slot == Slot::Pi && res_.input.mode == linecalc::CalcMode::New)   // D10: talet som räknas
        return fmt(std::numbers::pi, -1, ',');
    const auto value = getSlot(res_.input, lindata_, slot);
    return value ? fmt(*value, -1, ',') : std::string();
}

// Cellens grundfärg ur märket.
std::optional<gfx::Color> LineCalcDialog::colorOfCell(int grid, int row, int column) const
{
    if (grid < 0 || grid > 2 || row < 0 || row >= kGridRows[grid] || column < 0 || column >= kGridCols[grid])
        return std::nullopt;
    const auto& info = info_[static_cast<std::size_t>(grid)];
    const std::size_t index = static_cast<std::size_t>(row * kGridCols[grid] + column);
    if (index >= info.size())
        return std::nullopt;
    return colorOfMark(info[index].mark);
}

// Sant när cellen är en indatacell som går att ändra just nu.
bool LineCalcDialog::isInputCell(int grid, int row, int column) const
{
    if (readOnly_)
        return false;
    if (grid != kGridCase) {
        const Slot slot = slotAt(grid, row, column);
        if (slot == Slot::None)
            return false;
        if (!lc_.docSource.empty() &&
            (slot == Slot::NormalSpan || slot == Slot::Tension || slot == Slot::MinTemp ||
             slot == Slot::MaxTemp || isLindataSlot(slot)))
            return false;
        if (slot == Slot::Pi && lc_.input.mode == linecalc::CalcMode::New)   // D10: exakt pi
            return false;
        return true;
    }
    const int id = caseIdOfRow(row);
    if (id == 0 || column < 0 || column >= 26 || kCaseKind[column] == '0')
        return false;
    const bool manual = lc_.input.cases[static_cast<std::size_t>(id - 1)].manual.has_value();
    return !(manual && isManualLockedColumn(column));
}

// Fältet som en inklistrad text passar som: talet i kanonisk form, texten trimmad, eller
// heltalet 1-15 i L. Ett fält som inte passar ger tomt svar. Ett tomt fält töms cellen.
std::optional<std::string> LineCalcDialog::acceptField(int grid, int row, int column, const std::string& field) const
{
    const std::string text = trim(field);
    char kind = 'N';
    if (grid == kGridCase) {
        if (column < 0 || column >= 26)
            return std::nullopt;
        kind = kCaseKind[column];
    } else if (slotAt(grid, row, column) == Slot::None) {
        return std::nullopt;
    }
    if (kind == 'T')
        return text;
    if (text.empty())
        return std::string();
    const auto value = tabletext::parseDecimal(text);
    if (!value)
        return std::nullopt;
    if (kind == 'I') {
        const double rounded = std::nearbyint(*value);
        if (rounded != *value || rounded < 1 || rounded > linecalc::kLoadCaseCount)
            return std::nullopt;
        return fmt(rounded, 0, '.');
    }
    return fmt(*value, -1, '.');
}

// Gör alla redigerare skrivskyddade och döljer OK. Avbryt heter Stäng. Cellernas lås
// sätts i recalc, som läser readOnly_.
void LineCalcDialog::applyReadOnly()
{
    noteTxt.disable();
    conductorDd.disable();
    modeNewRb.disable();
    modeExcelRb.disable();
    docInputChk.disable();
    upperBtn.disable();
    lowerBtn.disable();
    resetBtn.disable();
    for (auto& sheet : sheets_)
        if (sheet)
            sheet->setReadOnly(true);
    okBtn.hide();
    cancelBtn.setText("Stäng");
}

// Validerar och sparar beräkningen i projektet. Svarar false när fönstret ska stå kvar,
// efter att det första problemet visats och dess cell gjorts aktuell.
bool LineCalcDialog::saveChanges()
{
    commitEdits();
    recalc();

    // Ett ogiltigt tal i en cell sparas aldrig.
    for (int g = 0; g < 3; ++g) {
        for (int r = 0; r < kGridRows[g]; ++r) {
            for (int c = 0; c < kGridCols[g]; ++c) {
                if (!tableOf(g).hasError(r, c))
                    continue;
                glaze::messageBox(*this, "Linberäkningen kan inte sparas: " + cellName(g, r, c) + " innehåller inget tal.",
                                  "Linberäkning", glaze::MessageBoxButtons::OK, glaze::MessageBoxType::Warning);
                tableOf(g).setCurrentCell(r, c);
                tableOf(g).ensureVisible(r);
                return false;
            }
        }
    }

    // Ofullständigt lindata sparas inte.
    if (lc_.docSource.empty() && lindataMissing_ > 0) {
        const auto answer = glaze::messageBox(
            *this,
            "Lindatat är ofullständigt (" + std::to_string(lindataMissing_) +
                " värden saknas) och sparas inte. Vill du spara beräkningen ändå?",
            "Linberäkning", glaze::MessageBoxButtons::YesNo, glaze::MessageBoxType::Warning);
        if (answer != glaze::DialogResult::Yes)
            return false;
    }

    // Fel i beräkningen: frågan, och felets cell blir aktuell vid Nej.
    const linecalc::LineIssue* firstError = nullptr;
    int errorCount = 0;
    for (const auto& issue : calc_.issues) {
        if (issue.kind != linecalc::LineIssueKind::Error)
            continue;
        ++errorCount;
        if (!firstError)
            firstError = &issue;
    }
    if (firstError) {
        std::string text = "Linberäkningen har fel: " + firstError->message;
        if (errorCount > 1)
            text += " (och " + std::to_string(errorCount - 1) + " fel till)";
        text += ". Vill du spara den ändå?";
        const auto answer = glaze::messageBox(*this, text, "Linberäkning", glaze::MessageBoxButtons::YesNo,
                                              glaze::MessageBoxType::Warning);
        if (answer != glaze::DialogResult::Yes) {
            const auto cells = cellsOfIssue(*firstError, lindata_, res_.input.conductor);
            if (!cells.empty()) {
                tableOf(cells.front().grid).setCurrentCell(cells.front().row, cells.front().col);
                tableOf(cells.front().grid).ensureVisible(cells.front().row);
            }
            return false;
        }
    }

    projectdb::LineCalc out = lc_;
    out.id = existingId_;
    out.note = trim(noteTxt.text());
    const auto saved = projectdb::saveLineCalc(*project_, out, linkDocKey_);
    if (!saved) {
        gfx::log("Linberäkningen kunde inte sparas: %s", saved.error().detail.c_str());
        glaze::messageBox(*this, saved.error().message, "Linberäkning", glaze::MessageBoxButtons::OK,
                          glaze::MessageBoxType::Error);
        return false;
    }
    saved_ = true;
    savedId_ = saved.value();
    return true;
}
