#include "tableText/table_text.h"

#include <charconv>
#include <cmath>
#include <cstddef>
#include <system_error>

namespace kalkyl::tabletext {

namespace {

constexpr char kTab = char{9};
constexpr char kLf = char{10};
constexpr char kCr = char{13};
constexpr char kQuote = char{34};

// Sant för ASCII-blanktecken som tas bort i början och slutet av en inmatning.
bool isAsciiSpace(char c)
{
    return c == ' ' || c == kTab || c == kLf || c == kCr;
}

// Hårt mellanslag (U+00A0), smalt hårt mellanslag (U+202F) och minustecken (U+2212) i UTF-8,
// byggda med char-värden så att källan inte behöver escape-sekvenser.
const std::string kNbsp = {static_cast<char>(0xC2), static_cast<char>(0xA0)};
const std::string kNarrowNbsp = {static_cast<char>(0xE2), static_cast<char>(0x80), static_cast<char>(0xAF)};
const std::string kMinus = {static_cast<char>(0xE2), static_cast<char>(0x88), static_cast<char>(0x92)};

// Sant om texten börjar med byteföljden på position i.
bool startsWithAt(std::string_view s, std::size_t i, std::string_view seq)
{
    return s.size() - i >= seq.size() && s.compare(i, seq.size(), seq) == 0;
}

// Talets text utan blanktecken (ASCII, U+00A0 och U+202F), med U+2212 som ASCII-minus.
// Gemensam rensning för parseDecimal och normalizeDecimal; tolkningen sker sedan i dem.
std::string stripBlanks(std::string_view text)
{
    std::string clean;
    for (std::size_t i = 0; i < text.size();) {
        if (startsWithAt(text, i, kNbsp)) {
            i += kNbsp.size();
        } else if (startsWithAt(text, i, kNarrowNbsp)) {
            i += kNarrowNbsp.size();
        } else if (startsWithAt(text, i, kMinus)) {
            clean.push_back('-');
            i += kMinus.size();
        } else if (isAsciiSpace(text[i])) {
            ++i;
        } else {
            clean.push_back(text[i]);
            ++i;
        }
    }
    return clean;
}

// Sant när texten, trimmad i ändarna, har en tabb eller radbrytning mitt i. Ett sådant fält är
// aldrig ett tal: "256", radbrytning, "256" ur en Excelcell med två rader är två värden.
bool hasInnerBreak(std::string_view text)
{
    const std::string inner = trimmed(text);
    return inner.find_first_of(std::string{char{9}, char{10}, char{13}}) != std::string::npos;
}

} // namespace

// Tolkar ett tal ur inmatad eller inklistrad text. Först rensas blanktecken (också hårt och smalt
// mellanslag), tecknet läses (också U+2212) och komma görs till punkt. Hela texten måste vara ett tal.
std::optional<double> parseDecimal(std::string_view text)
{
    if (hasInnerBreak(text))
        return std::nullopt;
    const std::string clean = stripBlanks(text);
    if (clean.empty())
        return std::nullopt;

    bool negative = false;
    std::size_t start = 0;
    if (clean[0] == '-' || clean[0] == '+') {
        negative = clean[0] == '-';
        start = 1;
    }
    std::string body = clean.substr(start);
    if (body.empty() || body[0] == '-' || body[0] == '+')
        return std::nullopt;

    int commas = 0;
    int dots = 0;
    for (char& c : body) {
        if (c == ',') {
            ++commas;
            c = '.';
        } else if (c == '.') {
            ++dots;
        }
    }
    if (commas + dots > 1)
        return std::nullopt;

    double value = 0;
    const char* begin = body.data();
    const char* end = body.data() + body.size();
    const auto result = std::from_chars(begin, end, value);
    if (result.ec != std::errc() || result.ptr != end || !std::isfinite(value))
        return std::nullopt;
    return negative ? -value : value;
}

// Talet i fast form. Utan decimals används kortaste form som läses tillbaka exakt. -0 blir 0.
std::string formatDecimal(double value, int decimals, char separator)
{
    if (!std::isfinite(value))
        return std::string();
    char buffer[400];
    std::to_chars_result r{};
    if (decimals < 0)
        r = std::to_chars(buffer, buffer + sizeof buffer, value, std::chars_format::fixed);
    else
        r = std::to_chars(buffer, buffer + sizeof buffer, value, std::chars_format::fixed, decimals);
    if (r.ec != std::errc())
        return std::string();
    std::string out(buffer, r.ptr);
    if (!out.empty() && out[0] == '-') {
        bool allZero = true;
        for (std::size_t i = 1; i < out.size(); ++i)
            if (out[i] != '0' && out[i] != '.')
                allZero = false;
        if (allZero)
            out.erase(0, 1);
    }
    for (char& c : out)
        if (c == '.')
            c = separator;
    return out;
}

// Gör om inmatad text till kanonisk decimaltext. Blanktecken (också U+00A0 och U+202F) tas bort,
// U+2212 blir minustecken, och resten måste vara siffror med högst ett decimaltecken.
// Inledande nollor i heltalsdelen stryks och decimalerna behålls exakt som de skrevs.
std::optional<std::string> normalizeDecimal(std::string_view text)
{
    if (hasInnerBreak(text))
        return std::nullopt;
    const std::string clean = stripBlanks(text);
    if (clean.empty())
        return std::string();

    // Tecknet först.
    bool negative = false;
    std::size_t start = 0;
    if (clean[0] == '-' || clean[0] == '+') {
        negative = clean[0] == '-';
        start = 1;
    }

    // Siffror och högst ett decimaltecken.
    std::string intPart;
    std::string decPart;
    bool seenSeparator = false;
    for (std::size_t i = start; i < clean.size(); ++i) {
        const char c = clean[i];
        if (c >= '0' && c <= '9') {
            (seenSeparator ? decPart : intPart).push_back(c);
        } else if ((c == ',' || c == '.') && !seenSeparator) {
            seenSeparator = true;
        } else {
            return std::nullopt;
        }
    }
    if (intPart.empty() && decPart.empty())
        return std::nullopt;

    // Inledande nollor stryks men minst en nolla blir kvar.
    const std::size_t firstNonZero = intPart.find_first_not_of('0');
    intPart = firstNonZero == std::string::npos ? std::string("0") : intPart.substr(firstNonZero);

    // Minustecknet stryks när alla siffror är noll.
    const bool allZero = intPart == "0" && decPart.find_first_not_of('0') == std::string::npos;
    std::string out;
    if (negative && !allZero)
        out.push_back('-');
    out += intPart;
    if (!decPart.empty()) {
        out.push_back('.');
        out += decPart;
    }
    return out;
}

// Skriver kanonisk decimaltext med separator som decimaltecken. Med group delas heltalsdelen
// i tretal med hårt mellanslag (U+00A0). Decimalerna grupperas aldrig.
std::string formatDecimalText(std::string_view canonical, char separator, bool group)
{
    const std::size_t dot = canonical.find('.');
    std::string_view intPart = dot == std::string_view::npos ? canonical : canonical.substr(0, dot);

    std::string out;
    if (!intPart.empty() && intPart[0] == '-') {
        out.push_back('-');
        intPart.remove_prefix(1);
    }
    for (std::size_t i = 0; i < intPart.size(); ++i) {
        if (group && i > 0 && (intPart.size() - i) % 3 == 0)
            out += kNbsp;
        out.push_back(intPart[i]);
    }
    if (dot != std::string_view::npos) {
        out.push_back(separator);
        out += canonical.substr(dot + 1);
    }
    return out;
}

// Texten utan ASCII-blanktecken i början och slutet, samma blanktecken som tolkningen hoppar över.
std::string trimmed(std::string_view text)
{
    std::size_t b = 0;
    std::size_t e = text.size();
    while (b < e && isAsciiSpace(text[b]))
        ++b;
    while (e > b && isAsciiSpace(text[e - 1]))
        --e;
    return std::string(text.substr(b, e - b));
}

// Delar tabbseparerad text i rader och fält. Ett fält som börjar med citattecken läses
// som i Excel: tabb och radbrytning ingår och två citattecken blir ett.
std::vector<std::vector<std::string>> parseTsv(std::string_view text)
{
    std::vector<std::vector<std::string>> rows;
    std::vector<std::string> row;
    std::string field;
    bool inQuotes = false;
    bool fieldStart = true;
    bool rowStart = true;   // inget har lästs på den aktuella raden

    for (std::size_t i = 0; i < text.size(); ++i) {
        const char c = text[i];
        if (inQuotes) {
            if (c == kQuote) {
                if (i + 1 < text.size() && text[i + 1] == kQuote) {
                    field.push_back(kQuote);
                    ++i;
                } else {
                    inQuotes = false;
                }
            } else {
                field.push_back(c);
            }
            continue;
        }
        if (c == kQuote && fieldStart) {
            inQuotes = true;
            fieldStart = false;
            rowStart = false;
        } else if (c == kTab) {
            row.push_back(std::move(field));
            field.clear();
            fieldStart = true;
            rowStart = false;
        } else if (c == kLf || (c == kCr && i + 1 < text.size() && text[i + 1] == kLf)) {
            if (c == kCr)
                ++i;
            row.push_back(std::move(field));
            field.clear();
            rows.push_back(std::move(row));
            row.clear();
            fieldStart = true;
            rowStart = true;
        } else {
            field.push_back(c);
            fieldStart = false;
            rowStart = false;
        }
    }
    if (!rowStart) {
        row.push_back(std::move(field));
        rows.push_back(std::move(row));
    }
    return rows;
}

// Skriver rader och fält som tabbseparerad text med LF efter varje rad. Fält med tabb,
// radbrytning eller citattecken skrivs inom citattecken med dubblerade citattecken.
std::string formatTsv(const std::vector<std::vector<std::string>>& rows)
{
    std::string out;
    for (const auto& row : rows) {
        for (std::size_t i = 0; i < row.size(); ++i) {
            if (i > 0)
                out.push_back(kTab);
            const std::string& f = row[i];
            const bool quote = f.find_first_of(std::string{kTab, kLf, kCr, kQuote}) != std::string::npos;
            if (!quote) {
                out += f;
                continue;
            }
            out.push_back(kQuote);
            for (char c : f) {
                if (c == kQuote)
                    out.push_back(kQuote);
                out.push_back(c);
            }
            out.push_back(kQuote);
        }
        out.push_back(kLf);
    }
    return out;
}

// Lägger ut blocket från ankarcellen åt höger och nedåt. Fält utanför kolumnerna
// ignoreras, men raderna räknas med i rowsNeeded.
PastePlan planPaste(int anchorRow, int anchorColumn, int columnCount,
                    const std::vector<std::vector<std::string>>& block)
{
    PastePlan plan;
    plan.rowsNeeded = anchorRow + static_cast<int>(block.size());
    for (std::size_t r = 0; r < block.size(); ++r) {
        for (std::size_t c = 0; c < block[r].size(); ++c) {
            const int column = anchorColumn + static_cast<int>(c);
            if (column >= columnCount)
                break;
            plan.cells.push_back({anchorRow + static_cast<int>(r), column, block[r][c]});
        }
    }
    return plan;
}

} // namespace kalkyl::tabletext
