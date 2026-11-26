#include <algorithm>
#include <map>
#include <set>

#include "poleImport/internal.h"
#include "poleImport/pole_import.h"

namespace kalkyl::poleimport {
namespace {

// Cellens värde som text i ett textfält (Stolpnr och LKL): text som den är, tal i kortaste form
// (1234 blir "1234"), sant/falskt som 1 eller 0 och fel som tom text.
std::string textValue(const xlsx::Cell& cell)
{
    switch (cell.kind) {
    case xlsx::CellKind::Number:
        return xlsx::numberText(cell.text, -1).value_or(cell.text);
    case xlsx::CellKind::Error:
        return "";
    default:
        return cell.text;
    }
}

// Skriver ett talfält ur en cell (planen 5.7 punkt 4): text trimmas och ett streck ger tomt, ett tal
// skrivs med cellens decimaler. Det som inte är ett tal lämnar fältet tomt och texten i invalid.
void setNumberField(int field, const xlsx::Cell& cell, SourceRow& row)
{
    std::string text;
    std::string& invalid = row.invalid[static_cast<std::size_t>(field)];
    switch (cell.kind) {
    case xlsx::CellKind::Text:
        text = tabletext::trimmed(cell.text);
        if (text.empty() || detail::isDashOnly(text))
            return;
        break;
    case xlsx::CellKind::Number: {
        const auto shown = xlsx::numberText(cell.text, cell.decimals);
        if (!shown) {
            invalid = cell.text;
            return;
        }
        text = *shown;
        break;
    }
    default:
        invalid = cell.text;
        return;
    }
    const auto accepted = poletable::acceptField(field, text);
    if (!accepted) {
        invalid = text;
        return;
    }
    poletable::field(row.pole, field) = *accepted;
}

// Rader som text: "rad 3", "rad 3, 5" och så vidare, högst fem nummer och " …" när det finns fler.
std::string rowList(const std::vector<int>& rows)
{
    std::string out = "rad ";
    const std::size_t shown = std::min<std::size_t>(rows.size(), 5);
    for (std::size_t i = 0; i < shown; ++i) {
        if (i > 0)
            out += ", ";
        out += std::to_string(rows[i]);
    }
    out += rows.size() > shown ? " …" : ".";
    return out;
}

} // namespace

// Stolparna i bladet med de valda kolumnerna (planen 5.7): dataraderna till bladets slut, rader utan
// Stolpnr och upprepade rubriker hoppas över.
Extraction extract(const xlsx::Sheet& sheet, const HeaderArea& header, const Mapping& mapping)
{
    using namespace poletable;
    Extraction ex;
    const int poleColumn = mapping[kPole];
    if (poleColumn <= 0)
        return ex;

    // Stolpnr-kolumnens rubrikdelar, för att känna igen en upprepad rubrikrad.
    std::set<std::string> headerTexts;
    for (const SourceColumn& sc : sourceColumns(sheet, header)) {
        if (sc.column != poleColumn)
            continue;
        for (const std::string& part : sc.parts) {
            headerTexts.insert(part);
            const std::string key = normalizeHeader(part);
            if (!key.empty())
                headerTexts.insert(key);
        }
    }

    const int start = detail::dataStartRow(header);
    auto it = std::lower_bound(sheet.cells.begin(), sheet.cells.end(), start,
                               [](const xlsx::Cell& c, int r) { return c.row < r; });
    while (it != sheet.cells.end()) {
        const int row = it->row;
        while (it != sheet.cells.end() && it->row == row)
            ++it;

        SourceRow src;
        src.excelRow = row;
        const xlsx::Cell* poleCell = detail::cellAt(sheet, row, poleColumn);
        const std::string pole = poleCell ? poletable::acceptField(kPole, textValue(*poleCell)).value_or("") : "";
        if (pole.empty()) {
            // Utan Stolpnr: listas när någon annan vald kolumn har ett värde, annars tyst.
            for (int f = 0; f < kColumnCount; ++f) {
                const int col = mapping[static_cast<std::size_t>(f)];
                if (f == kPole || col <= 0)
                    continue;
                const xlsx::Cell* c = detail::cellAt(sheet, row, col);
                if (c && detail::valueKind(*c) != detail::ValueKind::Empty) {
                    ex.skippedRows.push_back(row);
                    break;
                }
            }
            continue;
        }
        if (headerTexts.count(pole) || headerTexts.count(normalizeHeader(pole)))
            continue;

        src.pole.pole = pole;
        for (int f = kNorth; f < kColumnCount; ++f) {
            const int col = mapping[static_cast<std::size_t>(f)];
            if (col <= 0)
                continue;
            const xlsx::Cell* c = detail::cellAt(sheet, row, col);
            if (!c)
                continue;
            if (f == kLkl)
                src.pole.lkl = poletable::acceptField(kLkl, textValue(*c)).value_or("");
            else
                setNumberField(f, *c, src);
        }
        ex.rows.push_back(std::move(src));
    }
    ex.firstDataRow = ex.rows.empty() ? 0 : ex.rows.front().excelRow;
    return ex;
}

// Raderna first..last som rader i Stolptabell före. Index utanför listan klampas.
std::vector<poletable::PoleRow> poleRows(const Extraction& ex, int first, int last)
{
    std::vector<poletable::PoleRow> out;
    first = std::max(first, 0);
    last = std::min(last, static_cast<int>(ex.rows.size()) - 1);
    for (int i = first; i <= last; ++i)
        out.push_back(ex.rows[static_cast<std::size_t>(i)].pole);
    return out;
}

// Rådataposten (planen 5.8): alla celler på de importerade raderna, kolumnerna med rubrik eller data
// och fälten som kolumnerna gav.
poletable::ImportRecord importRecord(const xlsx::Sheet& sheet, const Recognition& rec, const Mapping& mapping,
                                     const Extraction& ex, int first, int last, std::string fileName)
{
    poletable::ImportRecord record;
    record.fileName = std::move(fileName);
    record.sheetName = sheet.name;
    record.title = rec.title;
    record.version = rec.version;
    record.headerFirstRow = rec.header.firstRow;
    record.headerLastRow = rec.header.lastRow;

    first = std::max(first, 0);
    last = std::min(last, static_cast<int>(ex.rows.size()) - 1);
    std::set<int> usedColumns;
    for (int i = first; i <= last; ++i) {
        const SourceRow& src = ex.rows[static_cast<std::size_t>(i)];
        poletable::ImportRow row;
        row.excelRow = src.excelRow;
        row.pole = src.pole.pole;
        for (const xlsx::Cell& c : detail::rowCells(sheet, src.excelRow)) {
            poletable::ImportCell cell;
            cell.column = c.column;
            cell.text = c.text;
            switch (c.kind) {
            case xlsx::CellKind::Text:
                cell.kind = poletable::ImportCellKind::Text;
                break;
            case xlsx::CellKind::Number:
                cell.kind = poletable::ImportCellKind::Number;
                break;
            case xlsx::CellKind::Bool:
                cell.kind = poletable::ImportCellKind::Bool;
                break;
            case xlsx::CellKind::Error:
                cell.kind = poletable::ImportCellKind::Error;
                break;
            }
            usedColumns.insert(c.column);
            row.cells.push_back(std::move(cell));
        }
        record.rows.push_back(std::move(row));
    }
    if (!record.rows.empty()) {
        record.firstDataRow = record.rows.front().excelRow;
        record.fromPole = record.rows.front().pole;
        record.toPole = record.rows.back().pole;
    }

    // Kolumnerna: de med rubrik eller en cell på raderna, och fältet som mapping pekar på.
    const auto fieldOf = [&](int column) {
        for (int f = 0; f < poletable::kColumnCount; ++f)
            if (mapping[static_cast<std::size_t>(f)] == column)
                return f;
        return -1;
    };
    std::set<int> seen;
    for (const SourceColumn& sc : rec.columns) {
        if (sc.label.empty() && !usedColumns.count(sc.column))
            continue;
        seen.insert(sc.column);
        record.columns.push_back({sc.column, sc.letter, sc.label, fieldOf(sc.column)});
    }
    for (const int column : usedColumns) {
        if (!seen.count(column))
            record.columns.push_back({column, xlsx::columnLetters(column), "", fieldOf(column)});
    }
    std::sort(record.columns.begin(), record.columns.end(),
              [](const poletable::ImportColumn& a, const poletable::ImportColumn& b) { return a.column < b.column; });
    return record;
}

// Problemen med importen i visningsordning (planen 9.3): först de som stoppar, sedan de övriga.
std::vector<Issue> importIssues(const Recognition& rec, const Mapping& mapping, const Extraction& ex, int first,
                                int last)
{
    using namespace poletable;
    std::vector<Issue> issues;
    const auto block = [&](std::string text) { issues.push_back({true, std::move(text)}); };
    const auto note = [&](std::string text) { issues.push_back({false, std::move(text)}); };

    // Stoppar: Stolpnr, tvetydiga fält, en kolumn för två fält, inga stolpar och Till före Från.
    if (mapping[kPole] <= 0)
        block("Välj kolumnen för Stolpnr.");
    for (int f = 0; f < kColumnCount; ++f) {
        if (f == kPole || mapping[static_cast<std::size_t>(f)] != -1)
            continue;
        const auto& cands = rec.fields[static_cast<std::size_t>(f)].candidates;
        if (cands.size() >= 2)
            block("Välj kolumn för " + columnName(f) + ": " + xlsx::columnLetters(cands[0].column) + " och " +
                  xlsx::columnLetters(cands[1].column) + " passar båda.");
        else
            block("Välj kolumn för " + columnName(f) + ".");
    }
    std::map<int, std::vector<int>> fieldsPerColumn;
    for (int f = 0; f < kColumnCount; ++f)
        if (mapping[static_cast<std::size_t>(f)] > 0)
            fieldsPerColumn[mapping[static_cast<std::size_t>(f)]].push_back(f);
    for (const auto& [column, fields] : fieldsPerColumn) {
        if (fields.size() < 2)
            continue;
        std::string names;
        for (std::size_t i = 0; i < fields.size(); ++i) {
            if (i > 0)
                names += i + 1 == fields.size() ? " och " : ", ";
            names += columnName(fields[i]);
        }
        block("Kolumn " + xlsx::columnLetters(column) + " är vald för både " + names + ". Välj olika kolumner.");
    }
    if (mapping[kPole] > 0 && ex.rows.empty()) {
        std::string label = "ingen rubrik";
        for (const SourceColumn& sc : rec.columns)
            if (sc.column == mapping[kPole] && !sc.label.empty())
                label = sc.label;
        block("Inga stolpar under rubrikerna i kolumn " + xlsx::columnLetters(mapping[kPole]) + " (" + label + ").");
    }
    if (!ex.rows.empty() && first > last)
        block("Till står före Från i filen. Byt Från och Till.");

    // Stoppar inte: ogiltiga tal, rader utan Stolpnr, dubbletter och Avstånd föregående som räknas.
    const int lo = std::max(first, 0);
    const int hi = std::min(last, static_cast<int>(ex.rows.size()) - 1);
    int invalid = 0;
    for (int i = lo; i <= hi; ++i)
        for (const std::string& s : ex.rows[static_cast<std::size_t>(i)].invalid)
            if (!s.empty())
                ++invalid;
    if (invalid == 1)
        note("1 ogiltigt tal lämnas tomt (rött i förhandsvisningen).");
    else if (invalid > 1)
        note(std::to_string(invalid) + " ogiltiga tal lämnas tomma (röda i förhandsvisningen).");

    std::vector<int> skipped;
    const bool validRange = !ex.rows.empty() && lo <= hi;
    for (const int row : ex.skippedRows) {
        if (validRange && (row < ex.rows[static_cast<std::size_t>(lo)].excelRow ||
                           row > ex.rows[static_cast<std::size_t>(hi)].excelRow))
            continue;
        skipped.push_back(row);
    }
    if (skipped.size() == 1)
        note("1 rad utan Stolpnr hoppas över: " + rowList(skipped));
    else if (skipped.size() > 1)
        note(std::to_string(skipped.size()) + " rader utan Stolpnr hoppas över: " + rowList(skipped));

    if (validRange) {
        const std::vector<std::string> dups = automode::duplicatePoles(poleRows(ex, lo, hi));
        for (std::size_t i = 0; i < dups.size() && i < 3; ++i)
            note("Stolpnr " + dups[i] + " finns flera gånger. Dubbletterna markeras i tabellen efter importen.");
        if (dups.size() > 3)
            note("… och " + std::to_string(dups.size() - 3) + " till.");
    }
    if (mapping[kSpan] == 0)
        note("Avstånd föregående hämtas inte och räknas ur koordinaterna.");
    return issues;
}

// Sträckor som har start och slut i nuvarande tabell men tappar en av dem i filen (planen 5.9).
std::vector<std::string> lostRanges(const std::vector<automode::NamedRange>& ranges,
                                    const std::vector<poletable::PoleRow>& current,
                                    const std::vector<poletable::PoleRow>& imported)
{
    std::vector<std::string> out;
    for (const automode::NamedRange& nr : ranges) {
        if (tabletext::trimmed(nr.range.fromPole).empty() || tabletext::trimmed(nr.range.toPole).empty())
            continue;
        if (!automode::findRange(current, nr.range, nr.word, "Stolptabell före").issue.empty())
            continue;
        const automode::RangeRows inFile = automode::findRange(imported, nr.range, nr.word, "filen");
        if (!inFile.issue.empty())
            out.push_back(automode::capitalized(nr.word) + ": " + inFile.issue);
    }
    return out;
}

} // namespace kalkyl::poleimport
