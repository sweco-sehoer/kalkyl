#pragma once
// Hjälpfunktioner som bara recognize.cpp och extract.cpp delar. Ingen del av det publika gränssnittet.
#include <algorithm>
#include <span>
#include <string>
#include <string_view>

#include "poleImport/pole_import.h"
#include "tableText/table_text.h"
#include "xlsx/xlsx.h"

namespace kalkyl::poleimport::detail {

// Cellen på raden och kolumnen, eller nullptr när den är tom (xlsx::Sheet::find).
inline const xlsx::Cell* cellAt(const xlsx::Sheet& sheet, int row, int column)
{
    return sheet.find(row, column);
}

// Första dataraden: raden efter rubrikområdet och kolumnnummerraden.
inline int dataStartRow(const HeaderArea& header)
{
    return std::max(header.lastRow, header.numberRow) + 1;
}

// Alla celler på en rad i kolumnordning. Tomt när raden saknar celler.
inline std::span<const xlsx::Cell> rowCells(const xlsx::Sheet& sheet, int row)
{
    const auto cmp = [](const xlsx::Cell& c, int r) { return c.row < r; };
    const auto first = std::lower_bound(sheet.cells.begin(), sheet.cells.end(), row, cmp);
    auto last = first;
    while (last != sheet.cells.end() && last->row == row)
        ++last;
    return {sheet.cells.data() + (first - sheet.cells.begin()), static_cast<std::size_t>(last - first)};
}

// Sant när texten bara är ett streck ("-", "–", "—" eller "/"), som Banken skriver för "ingen uppgift".
// Texten ska vara trimmad.
inline bool isDashOnly(std::string_view text)
{
    return text == "-" || text == "/" || text == "\xE2\x80\x93" || text == "\xE2\x80\x94";
}

// Texten med blanktecken (också hårt mellanslag och radbrytningar) ihop till ett blanksteg och utan
// blanksteg i början och slutet.
inline std::string collapseSpaces(std::string_view text)
{
    std::string out;
    bool pendingSpace = false;
    for (std::size_t i = 0; i < text.size(); ++i) {
        const unsigned char c = static_cast<unsigned char>(text[i]);
        std::size_t skip = 0;
        if (c == ' ' || c == '\t' || c == '\r' || c == '\n')
            skip = 1;
        else if (c == 0xC2 && i + 1 < text.size() && static_cast<unsigned char>(text[i + 1]) == 0xA0)
            skip = 2;
        else if (c == 0xE2 && i + 2 < text.size() && static_cast<unsigned char>(text[i + 1]) == 0x80 &&
                 static_cast<unsigned char>(text[i + 2]) == 0xAF)
            skip = 3;
        if (skip > 0) {
            pendingSpace = !out.empty();
            i += skip - 1;
            continue;
        }
        if (pendingSpace)
            out += ' ';
        pendingSpace = false;
        out += text[i];
    }
    return out;
}

// Vad en cell är i dataprovet: tom (tom text eller bara ett streck), tal (ett tal eller text som är
// ett tal) eller annat (text, sant/falskt och fel).
enum class ValueKind { Empty, Number, Other };
inline ValueKind valueKind(const xlsx::Cell& cell)
{
    switch (cell.kind) {
    case xlsx::CellKind::Number:
        return ValueKind::Number;
    case xlsx::CellKind::Text: {
        const std::string t = tabletext::trimmed(cell.text);
        if (t.empty() || isDashOnly(t))
            return ValueKind::Empty;
        return tabletext::normalizeDecimal(t) ? ValueKind::Number : ValueKind::Other;
    }
    default:
        return ValueKind::Other;
    }
}

} // namespace kalkyl::poleimport::detail
