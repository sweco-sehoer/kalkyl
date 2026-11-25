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

// Sant om texten börjar med byteföljden på position i.
bool startsWithAt(std::string_view s, std::size_t i, std::string_view seq)
{
    return s.size() - i >= seq.size() && s.compare(i, seq.size(), seq) == 0;
}

} // namespace

// Tolkar ett tal ur inmatad eller inklistrad text. Först rensas blanktecken (också hårt och smalt
// mellanslag), tecknet läses (också U+2212) och komma görs till punkt. Hela texten måste vara ett tal.
std::optional<double> parseDecimal(std::string_view text)
{
    constexpr std::string_view kNbsp = "\xC2\xA0";
    constexpr std::string_view kNarrowNbsp = "\xE2\x80\xAF";
    constexpr std::string_view kMinus = "\xE2\x88\x92";

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
