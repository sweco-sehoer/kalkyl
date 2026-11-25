#pragma once
#include "lineCalc/line_calc.h"

namespace kalkyl::linecalc {

// En ny beräkning i läget mode (beslut 6): ingen lina och inget lindata, inget normalspann
// och ingen uppspänning, T0 = 0, min -50 °C, max +50 °C, kortslutningstemperaturen 100 °C,
// konstanterna med arkets förval och lastfallen enligt planen 2.5.1 med min och max ovan.
LineInput newLineInput(CalcMode mode = CalcMode::New);

// Knappen "Återställ indata enl. SS-EN 50341-2-18" (reset_Swedish_standard): linhöjd,
// vindtryck, J6 = -40, J7 = 60, konstanterna utom O15 och alla 15 lastfall enligt planen
// 2.5.1, utan manuell last. Läget, lindatat, an, sigma0, T0 och O15 behålls.
void resetToStandard(LineInput& input);

// Knappen "Nedre linan" (blad1_Korsningslastfall_Nedre): O17 = 5 och lastfallen enligt
// planen 2.5.2, utan manuell last. Övriga indata och konstanter behålls.
void loadCrossingLower(LineInput& input);

// Knappen "Övre linan" (blad1_Korsningslastfall_Ovre): O17 = 3, kortslutningstemperaturen
// och lastfallen enligt planen 2.5.3, utan manuell last. Fönstret frågar efter
// temperaturen (förval 100 °C) och anropar inte funktionen vid Avbryt.
void loadCrossingUpper(LineInput& input, double shortCircuitTempC);

// Det indata som ändrats i applyInputChange: J6, J7, kortslutningstemperaturen, O4 eller O5.
// Fönstret anropar funktionen efter att ha skrivit det nya värdet i input.
enum class InputChange { MinTemp, MaxTemp, ShortCircuitTemp, NormalWind, HighWind };

// Arkets följdändringar (planen 2.4). Läget Excel skriver talen som arkets händelser:
// J6 till lastfall 1 och 2, J7 till lastfall 15 och O4/O5 till hela O-kolumnen.
// Läget New skriver det nya värdet i talen på de lastfall vars källa följer det.
void applyInputChange(LineInput& input, InputChange change);

} // namespace kalkyl::linecalc
