#include <array>
#include <cmath>
#include <iostream>
#include <limits>
#include <optional>
#include <string>

#include "lineCalc/load_cases.h"

using namespace kalkyl::linecalc;

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

// Två tal är lika inom toleransen tol.
bool near(double a, double b, double tol)
{
    return std::fabs(a - b) <= tol;
}

using TS = TempSource;
using PS = PressureSource;

// Förväntad rad i en mall, skriven för hand ur planen 2.5 och oberoende av load_cases.cpp.
struct Exp {
    TempSource tempSource;
    double tempC;
    const char* name;
    const char* standardCase;
    const char* clearanceCase;
    int windFrom;               // 0 = tom
    double magnitude;
    PressureSource pressureSource;
    double pressure;
    double refHeight;
    double gradient;
    double ice;
};
using ExpTable = std::array<Exp, kLoadCaseCount>;

// En tom rad i en korsningsmall.
constexpr Exp kEmpty{TS::Fixed, 0, "", "", "", 0, 0, PS::Fixed, 0, 0, 0, 0};

// Standardmallen (planen 2.5.1) för givna min, max, Normal vind och Hög vind.
ExpTable standardTable(double mn, double mx, double normal, double high)
{
    return {{
        {TS::Min, mn, "Vindstilla", "", "H", 0, 0, PS::Fixed, 0, 0, 0, 0},
        {TS::MinPlus20, mn + 20, "Normal Vind", "", "D", 0, 100, PS::Normal, normal, 25, 6, 0},
        {TS::Fixed, 15, "30% Normal Vind", "", "E", 0, 30, PS::Normal, normal, 25, 6, 0},
        {TS::Fixed, 0, "Islast och Normal Vind", "", "A", 0, 100, PS::Normal, normal, 25, 6, 18},
        {TS::Fixed, 0, "Islast och Vindstilla", "2a", "", 4, 0, PS::Fixed, 0, 0, 0, 18},
        {TS::Fixed, 0, "Hög Vind", "1a", "", 0, 100, PS::High, high, 10, 6, 0},
        {TS::Fixed, 0, "Vindstilla", "4", "", 0, 0, PS::Fixed, 0, 0, 0, 0},
        {TS::Fixed, 0, "Islast och Normal Vind", "3", "B", 0, 100, PS::Normal, normal, 25, 6, 18},
        {TS::Fixed, 0, "Islast och Vindstilla", "2a", "", 8, 0, PS::Fixed, 0, 0, 0, 18},
        {TS::Fixed, 0, "Hög Vind", "1a", "", 0, 100, PS::High, high, 10, 6, 0},
        {TS::Fixed, 0, "Vindstilla", "", "", 0, 0, PS::Fixed, 0, 0, 0, 0},
        {TS::Fixed, 15, "Vindstilla", "", "G", 0, 0, PS::Fixed, 0, 0, 0, 0},
        {TS::Fixed, 15, "Normal Vind", "", "C", 0, 100, PS::Normal, normal, 25, 6, 0},
        {TS::Fixed, 15, "30% Normal Vind", "", "F", 0, 30, PS::Normal, normal, 25, 6, 0},
        {TS::Max, mx, "Vindstilla", "", "", 0, 0, PS::Fixed, 0, 0, 0, 0},
    }};
}

// Nedre linan (planen 2.5.2) för givna min och max. Vindtrycket är talet 500.
ExpTable lowerTable(double mn, double mx)
{
    return {{
        {TS::Fixed, 35, "I  (+35 vindstilla)", "", "", 0, 0, PS::Fixed, 0, 0, 0, 0},
        {TS::Fixed, 0, "II-IV  (vindstilla)", "4", "", 0, 0, PS::Fixed, 0, 0, 0, 0},
        {TS::Max, mx, "V  (maxtemp och vindstilla)", "", "", 0, 0, PS::Fixed, 0, 0, 0, 0},
        {TS::Min, mn, "VI  (mintemp och vindstilla)", "", "H", 0, 0, PS::Fixed, 0, 0, 0, 0},
        {TS::Fixed, 0, "Islast och Vindstilla", "2a", "", 6, 0, PS::Fixed, 0, 0, 0, 18},
        {TS::Fixed, 0, "Islast och Normal Vind", "", "A", 0, 100, PS::Normal, 500, 25, 6, 18},
        kEmpty,
        {TS::Fixed, 35, "I  (+35 vindstilla)", "", "", 0, 0, PS::Fixed, 0, 0, 0, 0},
        {TS::Fixed, 0, "II-IV  (vindstilla)", "", "", 0, 0, PS::Fixed, 0, 0, 0, 0},
        {TS::Max, mx, "V  (maxtemp och vindstilla)", "", "", 0, 0, PS::Fixed, 0, 0, 0, 0},
        {TS::Min, mn, "VI  (mintemp och vindstilla)", "", "", 0, 0, PS::Fixed, 0, 0, 0, 0},
        kEmpty, kEmpty, kEmpty, kEmpty,
    }};
}

// Övre linan (planen 2.5.3) för givna min, max och kortslutningstemperatur.
ExpTable upperTable(double mn, double mx, double tks)
{
    const double rise = mn + (tks - mx);
    return {{
        {TS::Fixed, 0, "Islast och Normal vind", "", "A", 0, 100, PS::Normal, 500, 25, 6, 18},
        {TS::Max, mx, "I   (maxtemp och vindstilla)", "", "", 0, 0, PS::Fixed, 0, 0, 0, 0},
        {TS::Fixed, 0, "II  (islast och vindstilla)", "", "", 1, 0, PS::Fixed, 0, 0, 0, 18},
        {TS::Fixed, 0, "IV (vindstilla)", "", "", 0, 0, PS::Fixed, 0, 0, 0, 0},
        {TS::ShortCircuit, tks, "V  (kortslutningstemp.)", "", "", 0, 0, PS::Fixed, 0, 0, 0, 0},
        {TS::ShortCircuitRise, rise, "VI (kortslutningstemp.)", "", "", 0, 0, PS::Fixed, 0, 0, 0, 0},
        kEmpty,
        {TS::Max, mx, "I   (maxtemp och vindstilla)", "", "", 0, 0, PS::Fixed, 0, 0, 0, 0},
        {TS::Fixed, 0, "II  (islast och vindstilla)", "", "", 13, 0, PS::Fixed, 0, 0, 0, 18},
        {TS::Fixed, 0, "IV (vindstilla)", "", "", 0, 0, PS::Fixed, 0, 0, 0, 0},
        {TS::ShortCircuit, tks, "V  (kortslutningstemp.)", "", "", 0, 0, PS::Fixed, 0, 0, 0, 0},
        {TS::ShortCircuitRise, rise, "VI (kortslutningstemp.)", "", "", 0, 0, PS::Fixed, 0, 0, 0, 0},
        {TS::Fixed, 0, "Islast och Normal vind", "3", "B", 0, 100, PS::Normal, 500, 25, 6, 18},
        kEmpty, kEmpty,
    }};
}

// Jämför alla fält i de 15 lastfallen med tabellen och skriver vilket lastfall som skiljer.
// Manuell last ska vara tom på alla rader.
void expectCases(const LineInput& in, const ExpTable& exp, const std::string& what)
{
    for (int i = 0; i < kLoadCaseCount; ++i) {
        const LoadCase& c = in.cases[static_cast<std::size_t>(i)];
        const Exp& e = exp[static_cast<std::size_t>(i)];
        const bool ok = c.tempSource == e.tempSource && near(c.tempC, e.tempC, 1e-12)
            && c.name == e.name && c.standardCase == e.standardCase
            && c.clearanceCase == e.clearanceCase
            && c.windFromCase == (e.windFrom > 0 ? std::optional<int>(e.windFrom) : std::nullopt)
            && near(c.windMagnitudePercent, e.magnitude, 1e-12)
            && c.pressureSource == e.pressureSource
            && near(c.charPressureNPerM2, e.pressure, 1e-12)
            && near(c.refHeightM, e.refHeight, 1e-12)
            && near(c.pressureGradientNPerM3, e.gradient, 1e-12)
            && near(c.iceThicknessMm, e.ice, 1e-12) && !c.manual.has_value();
        if (!ok) {
            ++g_failures;
            std::cerr << what << ": lastfall " << (i + 1) << " skiljer sig från mallen" << std::endl;
        }
    }
}

// En lina med några värden, så att bevarandet av lindatat kan kontrolleras.
kalkyl::conductor::Conductor sampleConductor()
{
    kalkyl::conductor::Conductor c;
    c.name = "Provlina";
    c.areaMm2 = 124;
    c.diameterMm = 14.2;
    c.eInitialMpa = 89000;
    c.ePermanentMpa = 100000;
    return c;
}

// Ger alla lastfall manuell last, så att en mall måste ta bort den.
void setManualOnAll(LineInput& in)
{
    for (LoadCase& c : in.cases)
        c.manual = ManualLoad{1.5, 2.5};
}

// En ny beräkning (beslut 6): tomma lina, aₙ och σ₀, T0 = 0, min -50, max 50, förvalen
// i konstanterna och standardmallen med talen -50, -30 och 50 i lastfall 1, 2 och 15.
void testNewLineInput()
{
    const LineInput in = newLineInput();
    CHECK(in.mode == CalcMode::New);
    CHECK(!in.conductor.has_value());
    CHECK(!in.normalSpanM.has_value());
    CHECK(!in.tensionMpa.has_value());
    CHECK(in.refTempC == 0);
    CHECK(in.minTempC == -50);
    CHECK(in.maxTempC == 50);
    CHECK(in.shortCircuitTempC == 100);
    CHECK(in.lineHeightM == 25);
    CHECK(in.normalWindNPerM2 == 500);
    CHECK(in.highWindNPerM2 == 800);
    CHECK(near(in.constants.iceDensityKgM3, 917.4, 1e-12));
    CHECK(near(in.constants.pi, 3.1416, 1e-12));
    CHECK(near(in.constants.gravity, 9.81, 1e-12));
    CHECK(in.constants.gustFactor == 1);
    CHECK(in.constants.resonanceFactor == 0.5);
    CHECK(in.constants.dragFactor == 1);
    CHECK(in.constants.minIceNPerM == 20);
    CHECK(in.constants.maxUtilizationPercent == 55);
    CHECK(in.constants.transpositionCase == 5);
    expectCases(in, standardTable(-50, 50, 500, 800), "newLineInput");
    CHECK(in.cases[0].tempC == -50);
    CHECK(in.cases[1].tempC == -30);
    CHECK(in.cases[14].tempC == 50);

    CHECK(newLineInput(CalcMode::Excel).mode == CalcMode::Excel);
    CHECK(newLineInput(CalcMode::New).mode == CalcMode::New);
    expectCases(newLineInput(CalcMode::Excel), standardTable(-50, 50, 500, 800), "newLineInput Excel");
}

// Återställ behåller läget, lindatat, aₙ, σ₀, T0 och O15 och sätter allt annat som makrot,
// också efter ändrade värden och manuell last.
void testResetToStandard()
{
    for (const CalcMode mode : {CalcMode::New, CalcMode::Excel}) {
        LineInput in = newLineInput(mode);
        in.conductor = sampleConductor();
        in.normalSpanM = 314.6;
        in.tensionMpa = 90;
        in.refTempC = 7;
        in.constants.minIceNPerM = 33;
        in.shortCircuitTempC = 120;

        in.lineHeightM = 40;
        in.normalWindNPerM2 = 600;
        in.highWindNPerM2 = 900;
        in.minTempC = -10;
        in.maxTempC = 80;
        in.constants.iceDensityKgM3 = 900;
        in.constants.pi = 3.14159;
        in.constants.gravity = 9.8;
        in.constants.gustFactor = 2;
        in.constants.resonanceFactor = 0.7;
        in.constants.dragFactor = 1.2;
        in.constants.maxUtilizationPercent = 70;
        in.constants.transpositionCase = 3;
        in.cases[3].name = "Ändrad";
        in.cases[3].tempC = 99;
        in.cases[3].tempSource = TempSource::Fixed;
        in.cases[5].pressureSource = PressureSource::Fixed;
        in.cases[5].charPressureNPerM2 = 1234;
        setManualOnAll(in);

        resetToStandard(in);

        CHECK(in.mode == mode);
        CHECK(in.conductor.has_value() && in.conductor->name == "Provlina");
        CHECK(in.conductor.has_value() && in.conductor->areaMm2 == 124);
        CHECK(in.normalSpanM == std::optional<double>(314.6));
        CHECK(in.tensionMpa == std::optional<double>(90));
        CHECK(in.refTempC == 7);
        CHECK(in.constants.minIceNPerM == 33);
        CHECK(in.shortCircuitTempC == 120);

        CHECK(in.lineHeightM == 25);
        CHECK(in.normalWindNPerM2 == 500);
        CHECK(in.highWindNPerM2 == 800);
        CHECK(in.minTempC == -40);
        CHECK(in.maxTempC == 60);
        CHECK(near(in.constants.iceDensityKgM3, 917.4, 1e-12));
        CHECK(near(in.constants.pi, 3.1416, 1e-12));
        CHECK(near(in.constants.gravity, 9.81, 1e-12));
        CHECK(in.constants.gustFactor == 1);
        CHECK(in.constants.resonanceFactor == 0.5);
        CHECK(in.constants.dragFactor == 1);
        CHECK(in.constants.maxUtilizationPercent == 55);
        CHECK(in.constants.transpositionCase == 5);
        expectCases(in, standardTable(-40, 60, 500, 800), "resetToStandard");
        CHECK(in.cases[0].tempC == -40);
        CHECK(in.cases[1].tempC == -20);
        CHECK(in.cases[14].tempC == 60);
    }
}

// Nedre och övre linan: lastfallen fält för fält, O17 och Tks, och oförändrade indata.
// Källorna räknas om mot indata som ändrats före knappen.
void testCrossingTemplates()
{
    LineInput in = newLineInput();
    loadCrossingLower(in);
    expectCases(in, lowerTable(-50, 50), "loadCrossingLower");
    CHECK(in.constants.transpositionCase == 5);
    CHECK(in.minTempC == -50 && in.maxTempC == 50);
    CHECK(in.lineHeightM == 25 && in.normalWindNPerM2 == 500 && in.highWindNPerM2 == 800);
    CHECK(in.shortCircuitTempC == 100);

    LineInput up = newLineInput();
    loadCrossingUpper(up, 100);
    expectCases(up, upperTable(-50, 50, 100), "loadCrossingUpper");
    CHECK(up.constants.transpositionCase == 3);
    CHECK(up.shortCircuitTempC == 100);
    CHECK(up.cases[5].tempC == 0);
    CHECK(up.minTempC == -50 && up.maxTempC == 50);
    CHECK(up.lineHeightM == 25 && up.normalWindNPerM2 == 500 && up.highWindNPerM2 == 800);
    CHECK(near(up.constants.pi, 3.1416, 1e-12) && up.constants.gustFactor == 1);

    // Andra värden: temperaturerna följer min, max och Tks, och O-talet är 500 i båda mallarna
    LineInput other = newLineInput();
    other.minTempC = -40;
    other.maxTempC = 60;
    other.normalWindNPerM2 = 600;
    other.constants.minIceNPerM = 33;
    loadCrossingUpper(other, 120);
    expectCases(other, upperTable(-40, 60, 120), "loadCrossingUpper -40/60/120");
    CHECK(other.cases[4].tempC == 120);
    CHECK(other.cases[5].tempC == 20);
    CHECK(other.shortCircuitTempC == 120);
    CHECK(other.constants.minIceNPerM == 33);
    loadCrossingLower(other);
    expectCases(other, lowerTable(-40, 60), "loadCrossingLower -40/60");
    CHECK(other.constants.transpositionCase == 5);
    CHECK(other.shortCircuitTempC == 120);
    CHECK(other.cases[5].charPressureNPerM2 == 500);
}

// Ingen mall lämnar manuell last kvar, i något läge.
void testNoManualLeft()
{
    for (const CalcMode mode : {CalcMode::New, CalcMode::Excel}) {
        LineInput a = newLineInput(mode);
        setManualOnAll(a);
        resetToStandard(a);
        for (const LoadCase& c : a.cases)
            CHECK(!c.manual.has_value());

        LineInput b = newLineInput(mode);
        setManualOnAll(b);
        loadCrossingLower(b);
        for (const LoadCase& c : b.cases)
            CHECK(!c.manual.has_value());

        LineInput u = newLineInput(mode);
        setManualOnAll(u);
        loadCrossingUpper(u, 100);
        for (const LoadCase& c : u.cases)
            CHECK(!c.manual.has_value());
    }
}

// D7: Excel skriver J6, J7, O4 och O5 som tal på fasta rader, också efter en korsningsmall.
// New låter talen följa källorna och rör inte Fixed.
void testModeD7()
{
    // Nedre linan, sedan J7 = 40
    LineInput ex = newLineInput(CalcMode::Excel);
    loadCrossingLower(ex);
    ex.maxTempC = 40;
    applyInputChange(ex, InputChange::MaxTemp);
    CHECK(ex.cases[14].tempC == 40);
    CHECK(ex.cases[2].tempC == 50);
    CHECK(ex.cases[9].tempC == 50);

    LineInput nw = newLineInput(CalcMode::New);
    loadCrossingLower(nw);
    nw.maxTempC = 40;
    applyInputChange(nw, InputChange::MaxTemp);
    CHECK(nw.cases[2].tempC == 40);
    CHECK(nw.cases[9].tempC == 40);
    CHECK(nw.cases[14].tempC == 0);
    CHECK(nw.cases[3].tempC == -50);

    // Övre linan, sedan O4 = 600
    LineInput exUp = newLineInput(CalcMode::Excel);
    loadCrossingUpper(exUp, 100);
    exUp.normalWindNPerM2 = 600;
    applyInputChange(exUp, InputChange::NormalWind);
    CHECK(exUp.cases[0].charPressureNPerM2 == 0);
    CHECK(exUp.cases[5].charPressureNPerM2 == 800);
    CHECK(exUp.cases[1].charPressureNPerM2 == 600);
    CHECK(exUp.cases[12].charPressureNPerM2 == 600);
    CHECK(exUp.cases[13].charPressureNPerM2 == 600);
    CHECK(exUp.cases[0].pressureSource == PressureSource::Normal);

    LineInput nwUp = newLineInput(CalcMode::New);
    loadCrossingUpper(nwUp, 100);
    nwUp.normalWindNPerM2 = 600;
    applyInputChange(nwUp, InputChange::NormalWind);
    CHECK(nwUp.cases[0].charPressureNPerM2 == 600);
    CHECK(nwUp.cases[12].charPressureNPerM2 == 600);
    CHECK(nwUp.cases[5].charPressureNPerM2 == 0);
    CHECK(nwUp.cases[1].charPressureNPerM2 == 0);

    // Excel: J6 till lastfall 1 och 2, och Hög vind till O-kolumnen på lastfall 6 och 10
    LineInput exStd = newLineInput(CalcMode::Excel);
    exStd.minTempC = -45;
    applyInputChange(exStd, InputChange::MinTemp);
    CHECK(exStd.cases[0].tempC == -45);
    CHECK(exStd.cases[1].tempC == -25);
    exStd.highWindNPerM2 = 950;
    applyInputChange(exStd, InputChange::HighWind);
    CHECK(exStd.cases[5].charPressureNPerM2 == 950);
    CHECK(exStd.cases[9].charPressureNPerM2 == 950);
    CHECK(exStd.cases[1].charPressureNPerM2 == 500);
    CHECK(exStd.cases[14].charPressureNPerM2 == 0);

    // New: Tks och stigningen följer Tks, J6 och J7 men ingen annan rad rörs
    LineInput nwTks = newLineInput(CalcMode::New);
    loadCrossingUpper(nwTks, 100);
    nwTks.shortCircuitTempC = 120;
    applyInputChange(nwTks, InputChange::ShortCircuitTemp);
    CHECK(nwTks.cases[4].tempC == 120);
    CHECK(nwTks.cases[10].tempC == 120);
    CHECK(nwTks.cases[5].tempC == 20);
    CHECK(nwTks.cases[11].tempC == 20);
    CHECK(nwTks.cases[1].tempC == 50);
    nwTks.minTempC = -40;
    applyInputChange(nwTks, InputChange::MinTemp);
    CHECK(nwTks.cases[5].tempC == 30);
    nwTks.maxTempC = 60;
    applyInputChange(nwTks, InputChange::MaxTemp);
    CHECK(nwTks.cases[5].tempC == 20);
    CHECK(nwTks.cases[1].tempC == 60);

    // Excel: Tks ändrar ingenting
    LineInput exTks = newLineInput(CalcMode::Excel);
    loadCrossingUpper(exTks, 100);
    const LineInput before = exTks;
    exTks.shortCircuitTempC = 120;
    applyInputChange(exTks, InputChange::ShortCircuitTemp);
    for (int i = 0; i < kLoadCaseCount; ++i) {
        const auto k = static_cast<std::size_t>(i);
        CHECK(exTks.cases[k].tempC == before.cases[k].tempC);
        CHECK(exTks.cases[k].charPressureNPerM2 == before.cases[k].charPressureNPerM2);
    }
}

// D8: korsningsmallarna skriver talet 500 i O med källan Normal. Excel räknar med talet,
// New med källan O4.
void testModeD8()
{
    for (const CalcMode mode : {CalcMode::Excel, CalcMode::New}) {
        LineInput lo = newLineInput(mode);
        lo.normalWindNPerM2 = 600;
        loadCrossingLower(lo);
        CHECK(lo.cases[5].charPressureNPerM2 == 500);
        CHECK(lo.cases[5].pressureSource == PressureSource::Normal);

        LineInput up = newLineInput(mode);
        up.normalWindNPerM2 = 600;
        loadCrossingUpper(up, 100);
        CHECK(up.cases[0].charPressureNPerM2 == 500);
        CHECK(up.cases[12].charPressureNPerM2 == 500);
        CHECK(up.cases[0].pressureSource == PressureSource::Normal);
        CHECK(up.cases[12].pressureSource == PressureSource::Normal);
    }

    LineInput ex = newLineInput(CalcMode::Excel);
    ex.normalWindNPerM2 = 600;
    loadCrossingLower(ex);
    CHECK(caseCharPressureNPerM2(ex, 6) == 500);

    LineInput nw = newLineInput(CalcMode::New);
    nw.normalWindNPerM2 = 600;
    loadCrossingLower(nw);
    CHECK(caseCharPressureNPerM2(nw, 6) == 600);
}

// D9: en temperatur som ändrats för hand skrivs över av Excel men inte av New.
// Källan Fixed ändras inte, och lastfall 2 följer J6.
void testModeD9()
{
    for (const CalcMode mode : {CalcMode::Excel, CalcMode::New}) {
        LineInput in = newLineInput(mode);
        in.cases[0].tempSource = TempSource::Fixed;
        in.cases[0].tempC = -10;
        in.minTempC = -45;
        applyInputChange(in, InputChange::MinTemp);
        if (mode == CalcMode::Excel) {
            CHECK(in.cases[0].tempC == -45);
            CHECK(in.cases[1].tempC == -25);
        } else {
            CHECK(in.cases[0].tempC == -10);
            CHECK(in.cases[1].tempC == -25);
        }
        CHECK(in.cases[0].tempSource == TempSource::Fixed);
    }

    // O-kolumnen: en lastfallsrad med eget tal (Fixed) skrivs över av Excel men inte av New
    for (const CalcMode mode : {CalcMode::Excel, CalcMode::New}) {
        LineInput in = newLineInput(mode);
        in.cases[1].pressureSource = PressureSource::Fixed;
        in.cases[1].charPressureNPerM2 = 420;
        in.normalWindNPerM2 = 600;
        applyInputChange(in, InputChange::NormalWind);
        if (mode == CalcMode::Excel)
            CHECK(in.cases[1].charPressureNPerM2 == 600);
        else
            CHECK(in.cases[1].charPressureNPerM2 == 420);
        CHECK(in.cases[2].charPressureNPerM2 == 600);
    }
}

// D11: Övre linan avrundar kortslutningstemperaturen till heltal i läget Excel (makrots Integer,
// avrundning till jämnt), men inte i läget New.
void testModeD11()
{
    LineInput ex = newLineInput(CalcMode::Excel);
    loadCrossingUpper(ex, 100.5);
    CHECK(ex.shortCircuitTempC == 100);
    loadCrossingUpper(ex, 101.5);
    CHECK(ex.shortCircuitTempC == 102);
    loadCrossingUpper(ex, 99.6);
    CHECK(ex.shortCircuitTempC == 100);

    LineInput nw = newLineInput(CalcMode::New);
    loadCrossingUpper(nw, 100.5);
    CHECK(nw.shortCircuitTempC == 100.5);
}

} // namespace

int main()
{
    testNewLineInput();
    testResetToStandard();
    testCrossingTemplates();
    testNoManualLeft();
    testModeD7();
    testModeD8();
    testModeD9();
    testModeD11();

    if (g_failures != 0) {
        std::cerr << g_failures << " misslyckade kontroller" << std::endl;
        return 1;
    }
    std::cout << "load_cases: alla kontroller godkända" << std::endl;
    return 0;
}
