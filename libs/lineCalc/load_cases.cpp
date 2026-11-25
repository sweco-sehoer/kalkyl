#include "lineCalc/load_cases.h"

#include <array>
#include <cmath>
#include <limits>

namespace kalkyl::linecalc {

namespace {

using TS = TempSource;
using PS = PressureSource;

// Talet i O-kolumnen tas ur källan (Normal eller Hög vind) när raden saknar eget tal.
constexpr double kFromSource = std::numeric_limits<double>::quiet_NaN();

// En rad i en mall (planen 2.5): kolumnerna G-S som knapparna skriver dem.
// tempFixed gäller bara källan Fixed, pressureNumber är O-talet (kFromSource = källans värde).
// windFrom 0 betyder att L är tom.
struct CaseSpec {
    TempSource tempSource;
    double tempFixed;
    const char* name;
    const char* standardCase;
    const char* clearanceCase;
    int windFrom;
    double magnitudePercent;
    PressureSource pressureSource;
    double pressureNumber;
    double refHeightM;
    double gradientNPerM3;
    double iceMm;
};

using Template = std::array<CaseSpec, kLoadCaseCount>;

// Standardmallen enligt planen 2.5.1 (reset_Swedish_standard).
constexpr Template kStandard{{
    {TS::Min, 0, "Vindstilla", "", "H", 0, 0, PS::Fixed, 0, 0, 0, 0},
    {TS::MinPlus20, 0, "Normal Vind", "", "D", 0, 100, PS::Normal, kFromSource, 25, 6, 0},
    {TS::Fixed, 15, "30% Normal Vind", "", "E", 0, 30, PS::Normal, kFromSource, 25, 6, 0},
    {TS::Fixed, 0, "Islast och Normal Vind", "", "A", 0, 100, PS::Normal, kFromSource, 25, 6, 18},
    {TS::Fixed, 0, "Islast och Vindstilla", "2a", "", 4, 0, PS::Fixed, 0, 0, 0, 18},
    {TS::Fixed, 0, "Hög Vind", "1a", "", 0, 100, PS::High, kFromSource, 10, 6, 0},
    {TS::Fixed, 0, "Vindstilla", "4", "", 0, 0, PS::Fixed, 0, 0, 0, 0},
    {TS::Fixed, 0, "Islast och Normal Vind", "3", "B", 0, 100, PS::Normal, kFromSource, 25, 6, 18},
    {TS::Fixed, 0, "Islast och Vindstilla", "2a", "", 8, 0, PS::Fixed, 0, 0, 0, 18},
    {TS::Fixed, 0, "Hög Vind", "1a", "", 0, 100, PS::High, kFromSource, 10, 6, 0},
    {TS::Fixed, 0, "Vindstilla", "", "", 0, 0, PS::Fixed, 0, 0, 0, 0},
    {TS::Fixed, 15, "Vindstilla", "", "G", 0, 0, PS::Fixed, 0, 0, 0, 0},
    {TS::Fixed, 15, "Normal Vind", "", "C", 0, 100, PS::Normal, kFromSource, 25, 6, 0},
    {TS::Fixed, 15, "30% Normal Vind", "", "F", 0, 30, PS::Normal, kFromSource, 25, 6, 0},
    {TS::Max, 0, "Vindstilla", "", "", 0, 0, PS::Fixed, 0, 0, 0, 0},
}};

// Korsningslastfall för den nedre linan enligt planen 2.5.2. Talet 500 är det Excel skriver.
constexpr Template kCrossingLower{{
    {TS::Fixed, 35, "I  (+35 vindstilla)", "", "", 0, 0, PS::Fixed, 0, 0, 0, 0},
    {TS::Fixed, 0, "II-IV  (vindstilla)", "4", "", 0, 0, PS::Fixed, 0, 0, 0, 0},
    {TS::Max, 0, "V  (maxtemp och vindstilla)", "", "", 0, 0, PS::Fixed, 0, 0, 0, 0},
    {TS::Min, 0, "VI  (mintemp och vindstilla)", "", "H", 0, 0, PS::Fixed, 0, 0, 0, 0},
    {TS::Fixed, 0, "Islast och Vindstilla", "2a", "", 6, 0, PS::Fixed, 0, 0, 0, 18},
    {TS::Fixed, 0, "Islast och Normal Vind", "", "A", 0, 100, PS::Normal, 500, 25, 6, 18},
    {TS::Fixed, 0, "", "", "", 0, 0, PS::Fixed, 0, 0, 0, 0},
    {TS::Fixed, 35, "I  (+35 vindstilla)", "", "", 0, 0, PS::Fixed, 0, 0, 0, 0},
    {TS::Fixed, 0, "II-IV  (vindstilla)", "", "", 0, 0, PS::Fixed, 0, 0, 0, 0},
    {TS::Max, 0, "V  (maxtemp och vindstilla)", "", "", 0, 0, PS::Fixed, 0, 0, 0, 0},
    {TS::Min, 0, "VI  (mintemp och vindstilla)", "", "", 0, 0, PS::Fixed, 0, 0, 0, 0},
    {TS::Fixed, 0, "", "", "", 0, 0, PS::Fixed, 0, 0, 0, 0},
    {TS::Fixed, 0, "", "", "", 0, 0, PS::Fixed, 0, 0, 0, 0},
    {TS::Fixed, 0, "", "", "", 0, 0, PS::Fixed, 0, 0, 0, 0},
    {TS::Fixed, 0, "", "", "", 0, 0, PS::Fixed, 0, 0, 0, 0},
}};

// Korsningslastfall för den övre linan enligt planen 2.5.3. Talet 500 är det Excel skriver.
constexpr Template kCrossingUpper{{
    {TS::Fixed, 0, "Islast och Normal vind", "", "A", 0, 100, PS::Normal, 500, 25, 6, 18},
    {TS::Max, 0, "I   (maxtemp och vindstilla)", "", "", 0, 0, PS::Fixed, 0, 0, 0, 0},
    {TS::Fixed, 0, "II  (islast och vindstilla)", "", "", 1, 0, PS::Fixed, 0, 0, 0, 18},
    {TS::Fixed, 0, "IV (vindstilla)", "", "", 0, 0, PS::Fixed, 0, 0, 0, 0},
    {TS::ShortCircuit, 0, "V  (kortslutningstemp.)", "", "", 0, 0, PS::Fixed, 0, 0, 0, 0},
    {TS::ShortCircuitRise, 0, "VI (kortslutningstemp.)", "", "", 0, 0, PS::Fixed, 0, 0, 0, 0},
    {TS::Fixed, 0, "", "", "", 0, 0, PS::Fixed, 0, 0, 0, 0},
    {TS::Max, 0, "I   (maxtemp och vindstilla)", "", "", 0, 0, PS::Fixed, 0, 0, 0, 0},
    {TS::Fixed, 0, "II  (islast och vindstilla)", "", "", 13, 0, PS::Fixed, 0, 0, 0, 18},
    {TS::Fixed, 0, "IV (vindstilla)", "", "", 0, 0, PS::Fixed, 0, 0, 0, 0},
    {TS::ShortCircuit, 0, "V  (kortslutningstemp.)", "", "", 0, 0, PS::Fixed, 0, 0, 0, 0},
    {TS::ShortCircuitRise, 0, "VI (kortslutningstemp.)", "", "", 0, 0, PS::Fixed, 0, 0, 0, 0},
    {TS::Fixed, 0, "Islast och Normal vind", "3", "B", 0, 100, PS::Normal, 500, 25, 6, 18},
    {TS::Fixed, 0, "", "", "", 0, 0, PS::Fixed, 0, 0, 0, 0},
    {TS::Fixed, 0, "", "", "", 0, 0, PS::Fixed, 0, 0, 0, 0},
}};

// O-kolumnens mönster när O4 eller O5 ändras i Excel (planen 2.4): Normal vind på lastfall
// 2, 3, 4, 8, 13 och 14, Hög vind på 6 och 10, och 0 på övriga.
constexpr std::array<PressureSource, kLoadCaseCount> kExcelWindPattern{
    PS::Fixed, PS::Normal, PS::Normal, PS::Normal, PS::Fixed, PS::High, PS::Fixed, PS::Normal,
    PS::Fixed, PS::High, PS::Fixed, PS::Fixed, PS::Normal, PS::Normal, PS::Fixed};

// Temperaturen som källan ger i input. Fixed ger fixed.
double resolveTemp(const LineInput& input, TempSource source, double fixed)
{
    switch (source) {
    case TempSource::Fixed: return fixed;
    case TempSource::Min: return input.minTempC;
    case TempSource::MinPlus20: return input.minTempC + 20;
    case TempSource::Max: return input.maxTempC;
    case TempSource::ShortCircuit: return input.shortCircuitTempC;
    case TempSource::ShortCircuitRise:
        return input.minTempC + (input.shortCircuitTempC - input.maxTempC);
    }
    return fixed;
}

// Vindtrycket som källan ger i input. Fixed ger fixed.
double resolvePressure(const LineInput& input, PressureSource source, double fixed)
{
    switch (source) {
    case PressureSource::Fixed: return fixed;
    case PressureSource::Normal: return input.normalWindNPerM2;
    case PressureSource::High: return input.highWindNPerM2;
    }
    return fixed;
}

// Skriver mallens 15 rader i input.cases med källor och tal, och tar bort manuell last.
// Talen tas ur input som den ser ut nu, som makrot gör när knappen trycks.
void applyTemplate(LineInput& input, const Template& tmpl)
{
    for (int i = 0; i < kLoadCaseCount; ++i) {
        const CaseSpec& s = tmpl[static_cast<std::size_t>(i)];
        LoadCase c;
        c.tempSource = s.tempSource;
        c.tempC = resolveTemp(input, s.tempSource, s.tempFixed);
        c.name = s.name;
        c.standardCase = s.standardCase;
        c.clearanceCase = s.clearanceCase;
        if (s.windFrom > 0)
            c.windFromCase = s.windFrom;
        c.windMagnitudePercent = s.magnitudePercent;
        c.pressureSource = s.pressureSource;
        c.charPressureNPerM2 = std::isnan(s.pressureNumber)
            ? resolvePressure(input, s.pressureSource, 0)
            : s.pressureNumber;
        c.refHeightM = s.refHeightM;
        c.pressureGradientNPerM3 = s.gradientNPerM3;
        c.iceThicknessMm = s.iceMm;
        input.cases[static_cast<std::size_t>(i)] = c;
    }
}

// Följer temperaturkällan det indata som ändrats? Min ingår också i Min + 20 och i
// Tks-stigningen, och max och Tks ingår i stigningen.
bool tempFollows(TempSource source, InputChange change)
{
    switch (source) {
    case TempSource::Fixed: return false;
    case TempSource::Min:
    case TempSource::MinPlus20: return change == InputChange::MinTemp;
    case TempSource::Max: return change == InputChange::MaxTemp;
    case TempSource::ShortCircuit: return change == InputChange::ShortCircuitTemp;
    case TempSource::ShortCircuitRise:
        return change == InputChange::MinTemp || change == InputChange::MaxTemp
            || change == InputChange::ShortCircuitTemp;
    }
    return false;
}

// Följer vindtryckskällan det indata som ändrats?
bool pressureFollows(PressureSource source, InputChange change)
{
    switch (source) {
    case PressureSource::Fixed: return false;
    case PressureSource::Normal: return change == InputChange::NormalWind;
    case PressureSource::High: return change == InputChange::HighWind;
    }
    return false;
}

} // namespace

// En ny beräkning: tomma lina, normalspann och uppspänning, och standardmallen med
// min -50 och max +50. Konstanterna får förvalen i LineConstants.
LineInput newLineInput(CalcMode mode)
{
    LineInput input;
    input.mode = mode;
    input.conductor.reset();
    input.normalSpanM.reset();
    input.tensionMpa.reset();
    input.refTempC = 0;
    input.minTempC = -50;
    input.maxTempC = 50;
    input.shortCircuitTempC = 100;
    input.lineHeightM = 25;
    input.normalWindNPerM2 = 500;
    input.highWindNPerM2 = 800;
    input.constants = LineConstants{};
    applyTemplate(input, kStandard);
    return input;
}

// Knappen Återställ: J6 = -40, J7 = 60 och arkets förval för linhöjd, vindtryck och
// konstanter. O15, läget, lindatat, an, sigma0 och T0 rörs inte.
void resetToStandard(LineInput& input)
{
    input.lineHeightM = 25;
    input.normalWindNPerM2 = 500;
    input.highWindNPerM2 = 800;
    input.minTempC = -40;
    input.maxTempC = 60;

    LineConstants& k = input.constants;
    k.iceDensityKgM3 = 917.4;
    k.pi = 3.1416;
    k.gravity = 9.81;
    k.gustFactor = 1;
    k.resonanceFactor = 0.5;
    k.dragFactor = 1;
    k.maxUtilizationPercent = 55;
    k.transpositionCase = 5;

    applyTemplate(input, kStandard);
}

// Knappen Nedre linan: O17 = 5 och mallen i 2.5.2.
void loadCrossingLower(LineInput& input)
{
    input.constants.transpositionCase = 5;
    applyTemplate(input, kCrossingLower);
}

// Knappen Övre linan: O17 = 3, kortslutningstemperaturen och mallen i 2.5.3.
// I läget Excel avrundas temperaturen till heltal som makrots Integer-variabel,
// med avrundning till jämnt tal (std::nearbyint i standardläget).
void loadCrossingUpper(LineInput& input, double shortCircuitTempC)
{
    input.constants.transpositionCase = 3;
    input.shortCircuitTempC = input.mode == CalcMode::Excel ? std::nearbyint(shortCircuitTempC)
                                                            : shortCircuitTempC;
    applyTemplate(input, kCrossingUpper);
}

// Följdändringarna efter att J6, J7, Tks, O4 eller O5 ändrats. Excel skriver talen som
// arkets händelser (D7, D9), New låter talen följa källorna och rör inte Fixed.
void applyInputChange(LineInput& input, InputChange change)
{
    if (input.mode == CalcMode::Excel) {
        // D7, D9: Excel skriver över fasta rader, också efter en korsningsmall
        switch (change) {
        case InputChange::MinTemp:
            input.cases[0].tempC = input.minTempC;
            input.cases[1].tempC = input.minTempC + 20;
            break;
        case InputChange::MaxTemp:
            input.cases[14].tempC = input.maxTempC;
            break;
        case InputChange::ShortCircuitTemp:
            break;
        case InputChange::NormalWind:
        case InputChange::HighWind:
            for (std::size_t i = 0; i < input.cases.size(); ++i)
                input.cases[i].charPressureNPerM2 = resolvePressure(input, kExcelWindPattern[i], 0);
            break;
        }
        return;
    }

    for (LoadCase& c : input.cases) {
        if (tempFollows(c.tempSource, change))
            c.tempC = resolveTemp(input, c.tempSource, c.tempC);
        if (pressureFollows(c.pressureSource, change))
            c.charPressureNPerM2 = resolvePressure(input, c.pressureSource, c.charPressureNPerM2);
    }
}

} // namespace kalkyl::linecalc
