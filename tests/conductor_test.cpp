#include <cmath>
#include <iostream>
#include <string>

#include "conductor/conductor.h"

using namespace kalkyl;

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

// Två tal är lika inom en liten tolerans.
bool near(double a, double b)
{
    return std::fabs(a - b) < 1e-9;
}

// En giltig lina att utgå från i testerna.
conductor::Conductor sample()
{
    conductor::Conductor c;
    c.name = "Prov";
    c.type = "FeAl";
    c.areaMm2 = 100;
    c.diameterMm = 10;
    c.eInitialMpa = 50000;
    c.ePermanentMpa = 60000;
    c.eInitialLimitMpa = 100;
    c.alphaE6PerC = 19;
    c.creepStrainPermille = 0.4;
    c.creepTempC = 21.05;
    c.weightNPerM = 3;
    return c;
}

// Reglerna: framräknad krypningstemperatur, namnjämförelse, avvikelse och validering.
void testConductorRules()
{
    const auto derived = conductor::derivedCreepTempC(0.4, 19);
    CHECK(derived && std::fabs(*derived - 21.0526) < 1e-4);
    const auto zero = conductor::derivedCreepTempC(0, 0);
    CHECK(zero && near(*zero, 0));
    CHECK(!conductor::derivedCreepTempC(0.4, 0));

    CHECK(conductor::sameConductorName("ABC å", "abc å"));
    CHECK(!conductor::sameConductorName("Å", "å"));

    conductor::Conductor c = sample();
    CHECK(conductor::validateConductor(c).empty());
    CHECK(!conductor::creepTempDeviates(c));
    c.creepTempC = 21.0526 + 0.6;
    CHECK(conductor::creepTempDeviates(c));
    c.creepTempC = 21.05;
    c.alphaE6PerC = -1;
    c.breakingLoadKn = 0;
    const auto issues = conductor::validateConductor(c);
    CHECK(issues.size() == 1 && issues.front().field == conductor::ConductorField::BreakingLoad);
}

// sameConductorData ignorerar namn, id och ursprung men ser typ, tal och brottlast.
void testSameConductorData()
{
    conductor::Conductor a = sample();
    a.breakingLoadKn = 165;
    conductor::Conductor b = a;
    b.name = "Annat namn";
    b.id = 42;
    b.origin = conductor::ConductorOrigin::Seed;
    CHECK(conductor::sameConductorData(a, b));
    CHECK(!conductor::sameConductorValues(a, b));

    conductor::Conductor d = a;
    d.diameterMm = 11;
    CHECK(!conductor::sameConductorData(a, d));
    conductor::Conductor t = a;
    t.type = "Al";
    CHECK(!conductor::sameConductorData(a, t));
    conductor::Conductor e = a;
    e.breakingLoadKn.reset();
    CHECK(!conductor::sameConductorData(a, e));
    CHECK(!conductor::sameConductorData(e, a));
}

// Den naturliga jämförelsen: tal som tal, versaler vikta, tecken för tecken i övrigt.
void testCompareNames()
{
    CHECK(conductor::compareNames("910 mm² FeAl FALCON", "185 mm² Fe60") > 0);
    CHECK(conductor::compareNames("185 mm² Fe60", "910 mm² FeAl FALCON") < 0);
    CHECK(conductor::compareNames("abc", "ABC") == 0);
    CHECK(conductor::compareNames("a2", "a10") < 0);
    CHECK(conductor::compareNames("a10", "a2") > 0);
    CHECK(conductor::compareNames("a007", "a7") == 0);
    CHECK(conductor::compareNames("ab", "abc") < 0);
    CHECK(conductor::compareNames("", "") == 0);
}

} // namespace

int main()
{
    testConductorRules();
    testSameConductorData();
    testCompareNames();
    if (g_failures == 0)
        std::cout << "conductor: alla kontroller lyckades" << std::endl;
    else
        std::cerr << "conductor: " << g_failures << " kontroller misslyckades" << std::endl;
    return g_failures == 0 ? 0 : 1;
}
