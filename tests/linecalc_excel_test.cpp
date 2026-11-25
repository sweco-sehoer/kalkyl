// Testfallen E1-E10 ur tests/linecalc_excel_facit.md: arket Linberäkning räknat av användaren
// med åtta linor ur linregistret, standardmallen och korsningsmallarna Övre och Nedre linan.
// Läget Excel ska ge arkets visade spänning (X), mall (Z) och SAG-mall (AE) i alla 15 lastfall.
// Testet skriver också skillnaden mellan lägena Excel och Ny per fall.
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdio>
#include <vector>

#include "lineCalc/line_calc.h"
#include "lineCalc/load_cases.h"

using namespace kalkyl::linecalc;

namespace {

int g_failures = 0;

// Mallen som fallet räknades med i arket.
enum class Template { Standard, Upper, Lower };

// Ett fall: lindata ur linregistret (A, d, Eik, Ep, σp, k, α, Tp, qe, fu), mallen och arkets
// visade X, Z och AE för lastfall 1-15. Indata är desamma i alla fall (se md-filen).
struct ExcelCase {
    const char* id;
    const char* name;
    std::array<double, 10> lin;
    Template tmpl;
    std::array<double, 15> x;
    std::array<double, 15> z;
    std::array<double, 15> ae;
};

const std::vector<ExcelCase> kCases = {
    {"E1", "910 mm² FeAl FALCON", {910, 39.3, 51000, 71000, 120, 85, 19, 21, 29.9, 251}, Template::Standard,
     {124.74, 111.33, 81.74, 124.58, 124.58, 95.43, 90.00, 113.74, 113.74, 78.16, 71.15, 64.49, 66.85, 64.71, 53.15},
     {132, 148, 201, 260, 273, 172, 183, 285, 299, 210, 231, 255, 246, 254, 309},
     {132, 155, 202, 273, 273, 199, 183, 299, 299, 244, 231, 255, 259, 255, 309}},
    {"E2", "157 mm² Al", {157, 16.3, 45000, 60000, 60, 280, 23, 35, 4.3, 27.4}, Template::Standard,
     {118.35, 113.11, 81.35, 158.37, 158.37, 109.21, 90.00, 168.85, 168.85, 73.63, 42.89, 39.61, 51.93, 40.95, 33.96},
     {116, 121, 168, 437, 510, 125, 152, 410, 479, 186, 319, 346, 264, 334, 403},
     {116, 167, 175, 510, 510, 246, 152, 479, 479, 365, 319, 346, 363, 348, 403}},
    {"E3", "89 mm² Fe140", {89, 12.1, 180000, 180000, 770, 0, 11, 0, 6.85, 122}, Template::Standard,
     {98.58, 102.89, 88.52, 284.35, 299.63, 112.20, 90.00, 284.34, 299.63, 112.20, 90.00, 87.80, 95.35, 88.51, 83.21},
     {390, 374, 435, 438, 503, 343, 428, 438, 503, 343, 428, 438, 404, 435, 462},
     {390, 409, 439, 498, 503, 436, 428, 498, 503, 436, 428, 438, 441, 439, 462}},
    {"E4", "120 mm² Cu", {120, 14.2, 100000, 116000, 200, 166, 17, 0, 10.7, 47.9}, Template::Standard,
     {99.75, 100.02, 87.93, 218.05, 222.95, 102.90, 90.00, 217.71, 222.95, 100.54, 87.69, 85.37, 89.58, 85.76, 80.56},
     {447, 446, 507, 518, 574, 433, 495, 518, 574, 443, 508, 522, 498, 520, 553},
     {447, 470, 510, 570, 574, 503, 495, 571, 574, 515, 508, 522, 524, 522, 553}},
    {"E5", "116 mm² OPGW AFL 11472/11-09 (Dotterel)", {116, 14.9, 91000, 97000, 180, 40, 16.8, 17.857142857142858, 5.34, 75.3}, Template::Standard,
     {121.83, 119.91, 84.67, 258.01, 258.01, 123.77, 90.00, 249.37, 249.37, 111.65, 78.03, 73.08, 85.75, 74.37, 64.04},
     {189, 192, 272, 369, 426, 186, 256, 382, 441, 206, 295, 315, 268, 310, 359},
     {189, 234, 278, 426, 426, 296, 256, 441, 441, 329, 295, 315, 327, 316, 359}},
    {"E6", "124 mm² OPGW AFL 1x48 SMF 14230/20-01 (Dotterel)", {124, 14.9, 89000, 95000, 180, 40, 17.1, 23.391812865497073, 5.34, 77.7}, Template::Standard,
     {125.37, 120.88, 83.95, 249.63, 249.63, 122.07, 90.00, 238.07, 238.07, 107.08, 75.05, 69.99, 82.06, 71.21, 60.87},
     {172, 178, 256, 357, 412, 176, 239, 374, 432, 201, 287, 308, 262, 302, 354},
     {172, 217, 262, 412, 412, 281, 239, 432, 432, 321, 287, 308, 320, 309, 354}},
    {"E7", "161 mm² ADSS, SD-nDJ6/12", {161, 14.3, 12100, 12100, 200, 0, 2.5, 0, 1.6, 33.6}, Template::Standard,
     {91.49, 93.67, 89.82, 138.16, 138.16, 98.02, 90.00, 138.16, 138.16, 98.02, 90.00, 89.55, 92.40, 89.82, 88.51},
     {54, 53, 55, 405, 494, 51, 55, 405, 494, 51, 55, 55, 54, 55, 56},
     {54, 130, 67, 494, 494, 208, 55, 494, 494, 208, 55, 55, 132, 67, 56}},
    {"E8", "455 mm² Skywrap Condor 1x64HM4", {454, 37.3, 52000, 72000, 120, 90, 19, 21, 15.76, 124}, Template::Standard,
     {124.05, 114.40, 82.47, 153.75, 153.75, 106.32, 90.00, 144.11, 144.11, 86.94, 66.94, 61.42, 68.76, 62.15, 51.84},
     {140, 152, 210, 314, 341, 163, 193, 336, 364, 200, 259, 283, 252, 279, 335},
     {140, 176, 214, 341, 341, 237, 193, 364, 364, 290, 259, 283, 293, 284, 335}},
    {"E9", "455 mm² Skywrap Condor 1x64HM4, Övre linan", {454, 37.3, 52000, 72000, 120, 90, 19, 21, 15.76, 124}, Template::Upper,
     {153.75, 66.73, 153.75, 90.00, 52.62, 90.00, 90.00, 51.84, 144.11, 66.94, 43.08, 66.94, 144.11, 66.94, 66.94},
     {314, 260, 341, 193, 330, 193, 193, 335, 364, 259, 403, 259, 336, 259, 259},
     {341, 260, 341, 193, 330, 193, 193, 335, 364, 259, 403, 259, 364, 259, 259}},
    {"E10", "455 mm² Skywrap Condor 1x64HM4, Nedre linan", {454, 37.3, 52000, 72000, 120, 90, 19, 21, 15.76, 124}, Template::Lower,
     {72.58, 90.00, 66.73, 124.05, 153.75, 153.75, 90.00, 55.48, 66.94, 51.84, 95.08, 66.94, 66.94, 66.94, 66.94},
     {239, 193, 260, 140, 341, 314, 193, 313, 259, 335, 183, 259, 259, 259, 259},
     {239, 193, 260, 140, 341, 341, 193, 313, 259, 335, 183, 259, 259, 259, 259}},
};

// Bygger indata i arkets ordning: lina, Återställ, aₙ 314,6, σ₀ 90, T₀ 0, min -50 och max 50
// med följdändringar, vindtrycken 500 och 800, linhöjd 25 och sist korsningsmallen.
LineInput buildInput(const ExcelCase& c, CalcMode mode)
{
    LineInput in = newLineInput(mode);
    kalkyl::conductor::Conductor k;
    k.name = c.name;
    k.areaMm2 = c.lin[0];
    k.diameterMm = c.lin[1];
    k.eInitialMpa = c.lin[2];
    k.ePermanentMpa = c.lin[3];
    k.eInitialLimitMpa = c.lin[4];
    k.eivConstant = c.lin[5];
    k.alphaE6PerC = c.lin[6];
    k.creepTempC = c.lin[7];
    k.weightNPerM = c.lin[8];
    k.breakingLoadKn = c.lin[9];
    in.conductor = k;
    resetToStandard(in);
    in.normalSpanM = 314.6;
    in.tensionMpa = 90;
    in.refTempC = 0;
    in.minTempC = -50;
    applyInputChange(in, InputChange::MinTemp);
    in.maxTempC = 50;
    applyInputChange(in, InputChange::MaxTemp);
    in.normalWindNPerM2 = 500;
    applyInputChange(in, InputChange::NormalWind);
    in.highWindNPerM2 = 800;
    applyInputChange(in, InputChange::HighWind);
    in.lineHeightM = 25;
    if (c.tmpl == Template::Upper)
        loadCrossingUpper(in, 100);
    else if (c.tmpl == Template::Lower)
        loadCrossingLower(in);
    return in;
}

// Sant när Kalkyls tal, avrundat som arket visar det, ger det visade talet (halv enhet i sista siffran).
bool sameShown(double kalkyl, double shown, double half)
{
    return std::fabs(kalkyl - shown) <= half + 1e-9;
}

// Räknar ett fall i båda lägena. Läget Excel jämförs med arket, och varje avvikande lastfall
// skrivs ut. Sist kommer en rad med största skillnaden mellan lägena.
void checkCase(const ExcelCase& c)
{
    const LineCalculation ex = calculateLine(buildInput(c, CalcMode::Excel));
    const LineCalculation ny = calculateLine(buildInput(c, CalcMode::New));
    if (!ex.result || !ny.result) {
        std::printf("%s %s: inget resultat%c", c.id, c.name, char{10});
        ++g_failures;
        return;
    }
    int miss = 0;
    double worstRel = 0;
    double worstMall = 0;
    for (std::size_t i = 0; i < kLoadCaseCount; ++i) {
        const LoadCaseResult& r = ex.result->cases[i];
        if (!sameShown(r.stressMpa, c.x[i], 0.005) || !sameShown(r.mall, c.z[i], 0.5) ||
            !sameShown(r.inclinedMall, c.ae[i], 0.5)) {
            ++miss;
            std::printf("  %s lastfall %zu: X %.4f (arket %.2f), Z %.2f (%.0f), AE %.2f (%.0f)%c", c.id, i + 1,
                        r.stressMpa, c.x[i], r.mall, c.z[i], r.inclinedMall, c.ae[i], char{10});
        }
        const LoadCaseResult& n = ny.result->cases[i];
        worstRel = std::fmax(worstRel, std::fabs(n.stressMpa - r.stressMpa) / r.stressMpa);
        worstMall = std::fmax(worstMall, std::fabs(n.mall - r.mall));
    }
    g_failures += miss;
    std::printf("%-4s %-48s Excel: %s | Ny mot Excel: σ %.5f %%, mall %.3f%c", c.id, c.name,
                miss == 0 ? "stämmer med arket" : "AVVIKER", worstRel * 100, worstMall, char{10});
}

} // namespace

int main()
{
    for (const ExcelCase& c : kCases)
        checkCase(c);
    if (g_failures != 0) {
        std::printf("linecalc_excel: %d avvikelser mot arket%c", g_failures, char{10});
        return 1;
    }
    std::printf("linecalc_excel: alla %zu fall stämmer med arket i läget Excel%c", kCases.size(), char{10});
    return 0;
}
