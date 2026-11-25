#pragma once
#include <array>
#include <optional>
#include <vector>

// Facit för linberäkningen: sparade värden ur arket Linberäkning i Linberäkning.xlsm,
// med full precision. Ett testfall är en post i kFacitCases längst ned. Huvudet är
// fristående och känner inte till linberäkningens egna typer, så att testet bygger
// indata i samma ordning som användaren i Excel (planen 6.3).
namespace kalkyl::linecalc::facit {

// Linans namn och de tio värdena i arkets J9-J18. Brottlasten är tom när arket visar "-".
struct FacitConductor {
    const char* name = "";
    double areaMm2 = 0;                       // J9
    double diameterMm = 0;                    // J10
    double eInitialMpa = 0;                   // J11
    double ePermanentMpa = 0;                 // J12
    double eInitialLimitMpa = 0;              // J13
    double eivConstant = 0;                   // J14
    double alphaE6PerC = 0;                   // J15
    double creepTempC = 0;                    // J16
    double weightNPerM = 0;                   // J17
    std::optional<double> breakingLoadKn;     // J18
};

// Konstanter som ändras för hand efter knappen Återställ (O10, O13, O14, O15 och O17),
// med arkets förval. Korsningsmallen Övre linan använder kortslutningstemperaturen.
struct FacitConstants {
    double pi = 3.1416;               // O10
    double resonanceFactor = 0.5;     // O13, Gc
    double dragFactor = 1;            // O14, Cc
    double minIceNPerM = 20;          // O15
    int transpositionCase = 5;        // O17
    double shortCircuitTempC = 100;   // svaret på frågan vid Övre linan
};

// Mallen som klickas efter standardmallen och indata. None betyder att bara
// standardmallen används.
enum class FacitTemplate { None, CrossingLower, CrossingUpper };

// Last inskriven för hand i ett lastfall (arkets U och V, kolumn B = 1).
struct FacitManual {
    int caseId = 0;            // 1-15
    double iceNPerM = 0;       // U
    double windNPerM = 0;      // V
};

// Facit för ett lastfall: arkets kolumner M, R, T, U-Z, AA och AC-AE. M, R och T är tomma
// vid manuell last. AA är tom när arket visar "-". AE är arkets avrundade värde.
struct FacitRow {
    std::optional<double> heightM;           // M
    std::optional<double> pressureNPerM2;    // R
    std::optional<double> iceEquivalentMm;   // T
    double iceNPerM = 0;                     // U
    double windNPerM = 0;                    // V
    double sagM = 0;                         // W
    double stressMpa = 0;                    // X
    double forceN = 0;                       // Y
    double mall = 0;                         // Z
    std::optional<double> utilizationPercent;   // AA
    double inclinedSagM = 0;                 // AC
    double swingAngleDeg = 0;                // AD
    double inclinedMall = 0;                 // AE
};

// Ett testfall: indata i den ordning användaren skriver dem, ändringar efter
// standardmallen och facit för lastfall 1-15 (rad 0 är lastfall 1).
struct FacitCase {
    const char* name = "";                   // "T1" och så vidare
    FacitConductor conductor;
    double normalSpanM = 0;                  // J3
    double tensionMpa = 0;                   // J4
    double refTempC = 0;                     // J5
    double minTempC = 0;                     // J6
    double maxTempC = 0;                     // J7
    double lineHeightM = 25;                 // O3
    double normalWindNPerM2 = 500;           // O4
    double highWindNPerM2 = 800;             // O5
    FacitConstants constants;
    FacitTemplate crossing = FacitTemplate::None;
    std::vector<FacitManual> manual;
    std::array<FacitRow, 15> rows;
};

// Alla testfall med facit. Ett nytt testfall är en ny post sist i listan. Facit för
// T2-T15 läggs in när användaren lämnat kopiorna (planen 6.4).
inline const FacitCase kFacitCases[] = {
    // T1: 124 mm2 OPGW AFL (Dotterel), planens bilaga A. Läget Ny ger samma resultat.
    {
        .name = "T1",
        .conductor = {.name = "124 mm² OPGW AFL 1x48 SMF 14230/20-01 (Dotterel)",
                      .areaMm2 = 124,
                      .diameterMm = 14.9,
                      .eInitialMpa = 89000,
                      .ePermanentMpa = 95000,
                      .eInitialLimitMpa = 180,
                      .eivConstant = 40,
                      .alphaE6PerC = 17.1,
                      .creepTempC = 23.391812865497073,
                      .weightNPerM = 5.34,
                      .breakingLoadKn = 77.7},
        .normalSpanM = 314.6,
        .tensionMpa = 90,
        .refTempC = 0,
        .minTempC = -50,
        .maxTempC = 50,
        .lineHeightM = 25,
        .normalWindNPerM2 = 500,
        .highWindNPerM2 = 800,
        .rows = {{
            // M, R, T, U, V, W, X, Y, Z, AA, AC, AD, AE
            {0, 0, 0, 0, 0, 4.2496941021932928, 125.36876604134662, 15545.726989126981,
             171.75137591618949, 20.007370642377069, 4.2496941021932928, 0, 172},
            {25, 500, 0, 0, 3.7250000000000001, 4.40760567141868, 120.8771712723737,
             14988.769237774339, 178.1333715693701, 19.290565299580873, 5.3740222421589543,
             34.898217991235953, 217},
            {25, 500, 0, 0, 1.1174999999999999, 6.346392939596508, 83.949876838075923,
             10409.784727921415, 256.489453892207, 13.397406342241203, 6.4838702103883383,
             11.819667798760729, 262},
            {25, 500, 20.264505893041264, 16.743530380610878, 12.725, 8.8261113418727888,
             249.63464951586255, 30954.696539966957, 356.70726657096884, 39.838734285671762,
             10.186536852909702, 29.951391228421414, 412},
            {0, 0, 20.264505893041264, 20.147407460770967, 0, 10.186536852909702,
             249.63464951586255, 30954.696539966957, 411.68886000647854, 39.838734285671762,
             10.186536852909702, 0, 412},
            {25, 890, 0, 0, 6.6304999999999996, 4.3645223271432467, 122.07038152417618,
             15136.727308997846, 176.39215832426675, 19.480987527667757, 6.9582807749031597,
             51.152984533267528, 281},
            {0, 0, 0, 0, 0, 5.9197656182130949, 90.000000001010662, 11160.000000125323,
             239.24731182527037, 14.362934363095652, 5.9197656182130949, 0, 239},
            {25, 500, 20.264505893041264, 16.743530380610878, 12.725, 9.2549072702898822,
             238.06864262048742, 29520.51168494044, 374.0370528854441, 37.99293653145488,
             10.681425865523444, 29.951391228421414, 432},
            {0, 0, 20.264505893041264, 20.147407460770967, 0, 10.681425865523444,
             238.06864262048742, 29520.51168494044, 431.68979814420163, 37.99293653145488,
             10.681425865523444, 0, 432},
            {25, 890, 0, 0, 6.6304999999999996, 4.9753734856005751, 107.0831983140759,
             13278.316590945411, 201.07970627998841, 17.089210541757286, 7.9321499760725471,
             51.152984533267528, 321},
            {0, 0, 0, 0, 0, 7.0994529510360538, 75.045064643665356, 9305.5880158145046,
             286.92437226561435, 11.976303752657021, 7.0994529510360538, 0, 287},
            {0, 0, 0, 0, 0, 7.6122928145944426, 69.989281629275595, 8678.6709220301746,
             307.65079399685499, 11.169460646113482, 7.6122928145944426, 0, 308},
            {25, 500, 0, 0, 3.7250000000000001, 6.4924670891806118, 82.061086845247502,
             10175.574768810689, 262.39304026184919, 13.095977823437179, 7.9160127163806191,
             34.898217991235953, 320},
            {25, 500, 0, 0, 1.1174999999999999, 7.4815477321236594, 71.212391435739804,
             8830.3365380317355, 302.3667318341117, 11.364654489101332, 7.6436150313439564,
             11.819667798760729, 309},
            {0, 0, 0, 0, 0, 8.7521002514106119, 60.874406181452422, 7548.4263665000999,
             353.71610854541211, 9.7148344485200759, 8.7521002514106119, 0, 354},
        }},
    },
};

} // namespace kalkyl::linecalc::facit
