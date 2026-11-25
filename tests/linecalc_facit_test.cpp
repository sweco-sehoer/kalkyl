#include <algorithm>
#include <array>
#include <cmath>
#include <iomanip>
#include <iostream>
#include <iterator>
#include <limits>
#include <optional>
#include <sstream>
#include <string>
#include <vector>

#include "lineCalc/line_calc.h"
#include "lineCalc/load_cases.h"
#include "linecalc_facit.h"

using namespace kalkyl::linecalc;
namespace fc = kalkyl::linecalc::facit;

namespace {

// Antalet testfall som inte stämde med facit. Programmet svarar 1 om det är över noll.
int g_failures = 0;

// Resultatkolumnerna som jämförs, i arkets ordning. Namnen används i utskriften.
enum Col { kM, kR, kT, kU, kV, kW, kX, kY, kZ, kAA, kAC, kAD, kAE, kColCount };
constexpr const char* kColNames[kColCount] = {"M", "R", "T", "U", "V", "W", "X",
                                              "Y", "Z", "AA", "AC", "AD", "AE"};

// En rad i resultattabellen: ett tal per kolumn, tomt där arket visar en tom cell eller "-".
using Cols = std::array<std::optional<double>, kColCount>;
using CaseCols = std::array<Cols, kLoadCaseCount>;

// Arkets radnummer för lastfall id: rad 23-29 för 1-7 och rad 31-38 för 8-15.
int sheetRow(int id)
{
    return id <= kEiCaseCount ? 22 + id : 23 + id;
}

// Cellens namn i arket, till exempel X26 för kolumn X i lastfall 4.
std::string cellName(int col, int id)
{
    return std::string(kColNames[col]) + std::to_string(sheetRow(id));
}

// Talet med decimalkomma och prec gällande siffror, som i planens exempel.
std::string num(double v, int prec)
{
    std::ostringstream out;
    out << std::setprecision(prec) << v;
    std::string s = out.str();
    std::replace(s.begin(), s.end(), '.', ',');
    return s;
}

// Talet i vetenskaplig form med en decimal, till exempel 3,1e-16.
std::string sci(double v)
{
    std::ostringstream out;
    out << std::scientific << std::setprecision(1) << v;
    std::string s = out.str();
    std::replace(s.begin(), s.end(), '.', ',');
    return s;
}

// Ett valfritt tal för tabellen med full precision, eller ordet tom.
std::string optText(const std::optional<double>& v)
{
    return v ? num(*v, 17) : std::string("tom");
}

// Resultatet från beräkningen som en tabell med arkets kolumner.
CaseCols fromResult(const LineResult& r)
{
    CaseCols out;
    for (int i = 0; i < kLoadCaseCount; ++i) {
        const LoadCaseResult& c = r.cases[static_cast<std::size_t>(i)];
        Cols& row = out[static_cast<std::size_t>(i)];
        row[kM] = c.heightM;
        row[kR] = c.pressureNPerM2;
        row[kT] = c.iceEquivalentMm;
        row[kU] = c.iceNPerM;
        row[kV] = c.windNPerM;
        row[kW] = c.sagM;
        row[kX] = c.stressMpa;
        row[kY] = c.forceN;
        row[kZ] = c.mall;
        row[kAA] = c.utilizationPercent;
        row[kAC] = c.inclinedSagM;
        row[kAD] = c.swingAngleDeg;
        row[kAE] = c.inclinedMall;
    }
    return out;
}

// Facit som en tabell med arkets kolumner.
CaseCols fromFacit(const fc::FacitCase& f)
{
    CaseCols out;
    for (int i = 0; i < kLoadCaseCount; ++i) {
        const fc::FacitRow& c = f.rows[static_cast<std::size_t>(i)];
        Cols& row = out[static_cast<std::size_t>(i)];
        row[kM] = c.heightM;
        row[kR] = c.pressureNPerM2;
        row[kT] = c.iceEquivalentMm;
        row[kU] = c.iceNPerM;
        row[kV] = c.windNPerM;
        row[kW] = c.sagM;
        row[kX] = c.stressMpa;
        row[kY] = c.forceN;
        row[kZ] = c.mall;
        row[kAA] = c.utilizationPercent;
        row[kAC] = c.inclinedSagM;
        row[kAD] = c.swingAngleDeg;
        row[kAE] = c.inclinedMall;
    }
    return out;
}

// En cell som ligger utanför toleransen. rel är skillnaden delad med max(1, facit).
struct Mismatch {
    int id = 0;
    int col = 0;
    std::optional<double> actual;
    std::optional<double> expected;
    double rel = 0;
};

// Jämförelsens resultat: cellerna utanför toleransen, antalet lastfall som skiljer sig
// och den största relativa skillnaden med cellen den finns i.
struct Comparison {
    std::vector<Mismatch> bad;
    int differingCases = 0;
    double maxRel = 0;
    std::string maxCell;
};

// Jämför två tabeller cell för cell. Mot facit gäller planens toleranser (6.5): M exakt,
// AE inom 0,5 och övriga 1e-9 relativt. Mellan lägena jämförs alla kolumner med 1e-9.
// Tom mot tal är alltid ett fel. M och AE ingår inte i största relativa skillnaden mot facit.
Comparison compareCols(const CaseCols& actual, const CaseCols& expected, bool vsFacit)
{
    Comparison cmp;
    std::array<bool, kLoadCaseCount> differs{};
    for (int id = 1; id <= kLoadCaseCount; ++id) {
        for (int col = 0; col < kColCount; ++col) {
            const auto& a = actual[static_cast<std::size_t>(id - 1)][static_cast<std::size_t>(col)];
            const auto& e = expected[static_cast<std::size_t>(id - 1)][static_cast<std::size_t>(col)];
            if (!a && !e)
                continue;
            const bool counts = col != kM && !(col == kAE && vsFacit);
            double rel = std::numeric_limits<double>::infinity();
            bool ok = false;
            if (a && e) {
                const double diff = std::fabs(*a - *e);
                const double scale = std::max(1.0, std::fabs(*e));
                rel = diff / scale;
                double tol = 1e-9 * scale;
                if (col == kM)
                    tol = 0;
                else if (col == kAE && vsFacit)
                    tol = 0.5 + 1e-9;
                ok = diff <= tol;
            }
            if (counts && (rel > cmp.maxRel || std::isnan(rel))) {
                cmp.maxRel = rel;
                cmp.maxCell = cellName(col, id);
            }
            if (!ok) {
                cmp.bad.push_back({id, col, a, e, rel});
                differs[static_cast<std::size_t>(id - 1)] = true;
            }
        }
    }
    cmp.differingCases = static_cast<int>(std::count(differs.begin(), differs.end(), true));
    return cmp;
}

// Skriver en rad per cell utanför toleransen: Kalkyl, facit och skillnad.
// what säger vad som jämförs, till exempel facit.
void printMismatches(const std::string& name, const std::string& what, const Comparison& cmp)
{
    for (const Mismatch& m : cmp.bad) {
        std::cerr << name << " " << cellName(m.col, m.id) << " (lastfall " << m.id << "): Kalkyl = "
                  << optText(m.actual) << ", " << what << " = " << optText(m.expected);
        if (m.actual && m.expected)
            std::cerr << ", skillnad = " << num(*m.actual - *m.expected, 6);
        std::cerr << std::endl;
    }
}

// Skriver upplysningarna ur en beräkning, en per rad, med slag, lastfall och text.
void printIssues(const std::string& name, const LineCalculation& calc)
{
    for (const LineIssue& issue : calc.issues) {
        const char* kind = issue.kind == LineIssueKind::Missing ? "Saknas"
                           : issue.kind == LineIssueKind::Error ? "Fel"
                                                                : "Varning";
        std::cerr << name << " " << kind << " (lastfall " << issue.caseId << "): " << issue.message
                  << std::endl;
    }
}

// Bygger indata i läget mode i samma ordning som användaren i Excel (planen 6.3):
// ny beräkning, lina, standardmallen, J3-J5, J6, J7, O4 och O5 med följdändringar, O3,
// konstanterna, mallen och sist den manuella lasten.
LineInput buildInput(const fc::FacitCase& f, CalcMode mode)
{
    LineInput in = newLineInput(mode);

    kalkyl::conductor::Conductor c;
    c.name = f.conductor.name;
    c.areaMm2 = f.conductor.areaMm2;
    c.diameterMm = f.conductor.diameterMm;
    c.eInitialMpa = f.conductor.eInitialMpa;
    c.ePermanentMpa = f.conductor.ePermanentMpa;
    c.eInitialLimitMpa = f.conductor.eInitialLimitMpa;
    c.eivConstant = f.conductor.eivConstant;
    c.alphaE6PerC = f.conductor.alphaE6PerC;
    c.creepTempC = f.conductor.creepTempC;
    c.weightNPerM = f.conductor.weightNPerM;
    c.breakingLoadKn = f.conductor.breakingLoadKn;
    in.conductor = c;

    resetToStandard(in);
    in.normalSpanM = f.normalSpanM;
    in.tensionMpa = f.tensionMpa;
    in.refTempC = f.refTempC;

    in.minTempC = f.minTempC;
    applyInputChange(in, InputChange::MinTemp);
    in.maxTempC = f.maxTempC;
    applyInputChange(in, InputChange::MaxTemp);
    in.normalWindNPerM2 = f.normalWindNPerM2;
    applyInputChange(in, InputChange::NormalWind);
    in.highWindNPerM2 = f.highWindNPerM2;
    applyInputChange(in, InputChange::HighWind);
    in.lineHeightM = f.lineHeightM;

    in.constants.pi = f.constants.pi;
    in.constants.resonanceFactor = f.constants.resonanceFactor;
    in.constants.dragFactor = f.constants.dragFactor;
    in.constants.minIceNPerM = f.constants.minIceNPerM;
    in.constants.transpositionCase = f.constants.transpositionCase;

    if (f.crossing == fc::FacitTemplate::CrossingLower)
        loadCrossingLower(in);
    else if (f.crossing == fc::FacitTemplate::CrossingUpper)
        loadCrossingUpper(in, f.constants.shortCircuitTempC);

    for (const fc::FacitManual& m : f.manual) {
        ManualLoad load;
        load.iceNPerM = m.iceNPerM;
        load.windNPerM = m.windNPerM;
        in.cases[static_cast<std::size_t>(m.caseId - 1)].manual = load;
    }
    return in;
}

// Räknar ett testfall i läget Excel och jämför med facit, och räknar det sedan i läget Ny
// och jämför med Excel. En rad per läge skrivs ut, och celler utanför toleransen skrivs ut var för sig.
void runCase(const fc::FacitCase& f)
{
    const std::string name = f.name;

    const LineCalculation excel = calculateLine(buildInput(f, CalcMode::Excel));
    if (!excel.result) {
        ++g_failures;
        std::cerr << name << ": inget resultat i läget Excel" << std::endl;
        printIssues(name, excel);
        return;
    }

    const Comparison facitCmp = compareCols(fromResult(*excel.result), fromFacit(f), true);
    std::cout << name << ": " << kLoadCaseCount << " lastfall, största relativa skillnad "
              << sci(facitCmp.maxRel) << " (" << facitCmp.maxCell << ")";
    if (!facitCmp.bad.empty())
        std::cout << ", " << facitCmp.bad.size() << " celler utanför toleransen";
    std::cout << std::endl;
    if (!facitCmp.bad.empty()) {
        ++g_failures;
        printMismatches(name, "facit", facitCmp);
    }

    const double stakingFacit = f.rows[kLoadCaseCount - 1].mall;
    if (!(std::fabs(excel.result->stakingMall - stakingFacit) <= 1e-9 * std::max(1.0, std::fabs(stakingFacit)))) {
        ++g_failures;
        std::cerr << name << " stakingMall = " << num(excel.result->stakingMall, 17) << ", facit Z38 = "
                  << num(stakingFacit, 17) << std::endl;
    }

    const LineCalculation fresh = calculateLine(buildInput(f, CalcMode::New));
    if (!fresh.result) {
        std::cout << name << " New: inget resultat" << std::endl;
        printIssues(name + " New", fresh);
        return;
    }
    const Comparison modeCmp = compareCols(fromResult(*fresh.result), fromResult(*excel.result), false);
    std::cout << name << " New: " << modeCmp.differingCases << " lastfall skiljer sig från Excel, största skillnad "
              << num(modeCmp.maxRel * 100, 2) << " % (" << (modeCmp.maxCell.empty() ? "-" : modeCmp.maxCell)
              << ")" << std::endl;
}

} // namespace

// Kör alla testfall med facit och svarar 1 om något avviker från facit i läget Excel.
int main()
{
    for (const fc::FacitCase& f : fc::kFacitCases)
        runCase(f);
    if (g_failures == 0)
        std::cout << "linecalc_facit: alla " << std::size(fc::kFacitCases) << " testfall stämmer med facit" << std::endl;
    else
        std::cerr << "linecalc_facit: " << g_failures << " fel" << std::endl;
    return g_failures == 0 ? 0 : 1;
}
