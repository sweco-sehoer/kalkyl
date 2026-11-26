#include "poleTable/pole_table.h"

#include <algorithm>
#include <cstddef>

#include "tableText/table_text.h"

namespace kalkyl::poletable {

namespace {

// Om talen i tabellen visas med heltalsdelen grupperad i tretal. Urklippet grupperas aldrig.
constexpr bool kGroupThousands = true;

// Cellens text för visning eller urklipp. Textkolumner returneras som de är.
std::string shownText(const PoleRow& row, int column, bool group)
{
    const std::string& value = field(row, column);
    if (!isNumberColumn(column))
        return value;
    return tabletext::formatDecimalText(value, ',', group);
}

// Fält som aldrig nås (kolumn utanför 0-7) pekar hit.
const std::string kNoField;

} // namespace

// Sant för talkolumnerna 1-6.
bool isNumberColumn(int column)
{
    return column >= kAv && column <= kHeight;
}

// Kolumnens namn i meddelanden. Tom text utanför 0-7.
std::string columnName(int column)
{
    switch (column) {
    case kPole: return "Stolpplats";
    case kAv: return "Av";
    case kAh: return "Ah";
    case kSpan: return "Spannlängd föreg.";
    case kNorth: return "Koordinat N";
    case kEast: return "Koordinat E";
    case kHeight: return "Koordinat H";
    case kLkl: return "LKL";
    default: return std::string();
    }
}

// Fältet för kolumnen 0-7, för läsning. Utanför 0-7 ges ett tomt fält.
const std::string& field(const PoleRow& row, int column)
{
    switch (column) {
    case kPole: return row.pole;
    case kAv: return row.av;
    case kAh: return row.ah;
    case kSpan: return row.spanM;
    case kNorth: return row.north;
    case kEast: return row.east;
    case kHeight: return row.height;
    case kLkl: return row.lkl;
    default: return kNoField;
    }
}

// Fältet för kolumnen 0-7, för skrivning. Anroparen ska ha kontrollerat kolumnen;
// utanför 0-7 ges Stolpplats.
std::string& field(PoleRow& row, int column)
{
    switch (column) {
    case kAv: return row.av;
    case kAh: return row.ah;
    case kSpan: return row.spanM;
    case kNorth: return row.north;
    case kEast: return row.east;
    case kHeight: return row.height;
    case kLkl: return row.lkl;
    default: return row.pole;
    }
}

// Texten som den lagras: text trimmas och tal blir kanonisk decimaltext.
std::optional<std::string> acceptField(int column, std::string_view text)
{
    if (column < kPole || column > kLkl)
        return std::nullopt;
    if (isNumberColumn(column))
        return tabletext::normalizeDecimal(text);
    return tabletext::trimmed(text);
}

// Cellens text i tabellen, med gruppering när kGroupThousands är på.
std::string displayText(const PoleRow& row, int column)
{
    return shownText(row, column, kGroupThousands);
}

// Cellens text i urklippet, alltid utan gruppering.
std::string copyText(const PoleRow& row, int column)
{
    return shownText(row, column, false);
}

// Talet för beräkningar. Tomt för tom cell, textkolumn eller ogiltig text.
std::optional<double> numberValue(const PoleRow& row, int column)
{
    if (!isNumberColumn(column))
        return std::nullopt;
    return tabletext::parseDecimal(field(row, column));
}

// Sant när alla åtta fält är tomma.
bool isEmptyRow(const PoleRow& row)
{
    for (int c = 0; c < kColumnCount; ++c)
        if (!field(row, c).empty())
            return false;
    return true;
}

// Första talkolumnen vars text inte är tom eller kanonisk decimaltext.
std::optional<int> firstInvalidColumn(const PoleRow& row)
{
    for (int c = kAv; c <= kHeight; ++c) {
        const std::string& value = field(row, c);
        if (value.empty())
            continue;
        const auto canonical = tabletext::normalizeDecimal(value);
        if (!canonical || *canonical != value)
            return c;
    }
    return std::nullopt;
}

// Skriver ändringarna i ordning. Rader bortom slutet läggs till som tomma rader.
void applyEdits(std::vector<PoleRow>& rows, const std::vector<Edit>& edits)
{
    for (const Edit& edit : edits) {
        if (edit.row < 0 || edit.column < kPole || edit.column > kLkl)
            continue;
        if (static_cast<std::size_t>(edit.row) >= rows.size())
            rows.resize(static_cast<std::size_t>(edit.row) + 1);
        field(rows[static_cast<std::size_t>(edit.row)], edit.column) = edit.text;
    }
}

// Tar bort helt tomma rader sist i listan.
void trimTrailingEmptyRows(std::vector<PoleRow>& rows)
{
    while (!rows.empty() && isEmptyRow(rows.back()))
        rows.pop_back();
}

// Raderna som sparas: tomma rader mitt i behålls, tomma rader sist tas bort.
std::vector<PoleRow> rowsToSave(const std::vector<PoleRow>& rows)
{
    std::vector<PoleRow> out = rows;
    trimTrailingEmptyRows(out);
    return out;
}

} // namespace kalkyl::poletable
