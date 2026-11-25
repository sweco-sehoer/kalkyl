#pragma once
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace kalkyl::tabletext {

// Ett tal ur text som användaren skrivit eller klistrat in. Decimalkomma eller decimalpunkt,
// mellanslag (också hårt mellanslag U+00A0 och smalt U+202F) som tusentalsavgränsare, valfritt
// plus- eller minustecken (också U+2212) och exponent. Tomt, otolkbart, både komma och punkt,
// eller ett tal som inte är ändligt ger tomt svar.
std::optional<double> parseDecimal(std::string_view text);

// Talet i fast form utan exponent. decimals -1 = kortaste form som läses tillbaka exakt,
// annars så många decimaler (avrundat). separator ',' för visning och urklipp, '.' för
// DataGrids kanoniska celltext. -0 skrivs som 0.
std::string formatDecimal(double value, int decimals = -1, char separator = ',');

// Tabbseparerad text till rader och fält. Rader skiljs av LF eller CRLF, fält av tabb.
// En avslutande tom rad tas bort. Ett fält inom citattecken (Excel) får dem borttagna, och
// två citattecken i rad blir ett; där får fältet innehålla tabb och radbrytning.
std::vector<std::vector<std::string>> parseTsv(std::string_view text);

// Rader och fält till tabbseparerad text: tabb mellan fält, LF efter varje rad (som Excel).
// Ett fält med tabb, radbrytning eller citattecken skrivs inom citattecken.
std::string formatTsv(const std::vector<std::vector<std::string>>& rows);

// Var ett inklistrat block hamnar: från ankarcellen åt höger och nedåt. Fält som hamnar i
// kolumn >= columnCount ignoreras. rowsNeeded = anchorRow + antalet rader i blocket.
struct PasteCell { int row = 0; int column = 0; std::string text; };
struct PastePlan { std::vector<PasteCell> cells; int rowsNeeded = 0; };
PastePlan planPaste(int anchorRow, int anchorColumn, int columnCount,
                    const std::vector<std::vector<std::string>>& block);

} // namespace kalkyl::tabletext
