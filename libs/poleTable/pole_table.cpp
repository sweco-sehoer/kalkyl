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

// Fält som aldrig nås (fält utanför 0-9) pekar hit.
const std::string kNoField;

} // namespace

// Av Fas2 och Fas3 har Av som motsvarighet i före, de andra fälten sig själva.
int beforeField(int field)
{
    return (field == kAv2 || field == kAv3) ? kAv : field;
}

// Före: fälten i ordning. Efter: Av per fas före LKL.
const std::vector<int>& beforeColumns()
{
    static const std::vector<int> columns = {kPole, kNorth, kEast, kHeight, kSpan, kAh, kAv, kLkl};
    return columns;
}

const std::vector<int>& afterColumns()
{
    static const std::vector<int> columns = {kPole, kNorth, kEast, kHeight, kSpan, kAh, kAv, kAv2, kAv3, kLkl};
    return columns;
}

// Sant för talfälten: alla utom Stolpnr och LKL.
bool isNumberColumn(int column)
{
    return (column > kPole && column < kLkl) || column == kAv2 || column == kAv3;
}

// Fältets namn i meddelanden. Tom text utanför 0-9.
std::string columnName(int column)
{
    switch (column) {
    case kPole: return "Stolpnr";
    case kNorth: return "Koordinat N";
    case kEast: return "Koordinat E";
    case kHeight: return "Koordinat H";
    case kSpan: return "Avstånd föregående";
    case kAh: return "Ah";
    case kAv: return "Av";
    case kLkl: return "LKL";
    case kAv2: return "Av Fas2";
    case kAv3: return "Av Fas3";
    default: return std::string();
    }
}

// I efter har Av en fas, och de andra fälten heter som i columnName.
std::string afterColumnName(int column)
{
    return column == kAv ? std::string("Av Fas1") : columnName(column);
}

// LKL: delarna mellan radbrytningar och snedstreck, trimmade och utan tomma, med "/" emellan.
std::string normalizeLkl(std::string_view text)
{
    std::string out;
    std::string part;
    const auto flush = [&out, &part] {
        const std::string t = tabletext::trimmed(part);
        if (!t.empty()) {
            if (!out.empty())
                out += '/';
            out += t;
        }
        part.clear();
    };
    for (const char ch : text) {
        if (ch == char{10} || ch == char{13} || ch == '/')
            flush();
        else
            part += ch;
    }
    flush();
    return out;
}

// Fältet 0-9, för läsning. Utanför 0-9 ges ett tomt fält.
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
    case kAv2: return row.av2;
    case kAv3: return row.av3;
    default: return kNoField;
    }
}

// Fältet 0-9, för skrivning. Anroparen ska ha kontrollerat fältet; utanför 0-9 ges Stolpnr.
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
    case kAv2: return row.av2;
    case kAv3: return row.av3;
    default: return row.pole;
    }
}

// Texten som den lagras: text trimmas och tal blir kanonisk decimaltext.
std::optional<std::string> acceptField(int column, std::string_view text)
{
    if (column < kPole || column >= kFieldCount)
        return std::nullopt;
    if (isNumberColumn(column))
        return tabletext::normalizeDecimal(text);
    if (column == kLkl)
        return normalizeLkl(text);
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

// Sant när alla tio fält är tomma.
bool isEmptyRow(const PoleRow& row)
{
    for (int c = 0; c < kFieldCount; ++c)
        if (!field(row, c).empty())
            return false;
    return true;
}

// Första talfältet vars text inte är tom eller kanonisk decimaltext.
std::optional<int> firstInvalidColumn(const PoleRow& row)
{
    for (int c = kPole + 1; c < kFieldCount; ++c) {
        if (!isNumberColumn(c))
            continue;
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
        if (edit.row < 0 || edit.column < kPole || edit.column >= kFieldCount)
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
