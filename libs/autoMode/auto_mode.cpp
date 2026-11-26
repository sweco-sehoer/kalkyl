#include "autoMode/auto_mode.h"

#include <cmath>
#include <cstddef>
#include <optional>
#include <unordered_map>
#include <utility>

#include "tableText/table_text.h"

namespace kalkyl::automode {

// Versal först när texten börjar med en ASCII-bokstav. Andra tecken (å, ä, ö) lämnas, eftersom
// linans ord alltid börjar med en ASCII-bokstav.
std::string capitalized(std::string_view text) {
    std::string out(text);
    if (!out.empty() && out[0] >= 'a' && out[0] <= 'z')
        out[0] = static_cast<char>(out[0] - 'a' + 'A');
    return out;
}

// Summan av antalen avvikelser i kontrollerna 1-5.
int deviationCount(const ancalc::SpanResult& r) {
    return r.tooLongCount + r.tooShortCount + r.adjacentCount + r.localLowCount + r.localHighCount;
}

namespace {

// Antalet rader med Stolpplats lika med pole (exakt efter trimning) och den första av dem.
// Tom Stolpplats matchar aldrig.
struct PoleHits {
    int count = 0;
    int first = -1;
    int last = -1;
};
PoleHits findPole(const std::vector<poletable::PoleRow>& rows, const std::string& pole) {
    PoleHits hits;
    if (pole.empty())
        return hits;
    for (std::size_t i = 0; i < rows.size(); ++i) {
        if (tabletext::trimmed(rows[i].pole) != pole)
            continue;
        if (hits.count == 0)
            hits.first = static_cast<int>(i);
        hits.last = static_cast<int>(i);
        ++hits.count;
    }
    return hits;
}

// Talet med en decimal och decimalkomma, som i texterna i planen.
std::string tenth(double value) {
    return tabletext::formatDecimal(value, 1, ',');
}

} // namespace

// Söker start och slut i tabellen och prövar felen i planens ordning (7.1). Utan fel ger den
// första och sista raden, båda medräknade.
RangeRows findRange(const std::vector<poletable::PoleRow>& rows, const PoleRange& range,
                    std::string_view word, std::string_view table) {
    RangeRows out;
    const std::string from = tabletext::trimmed(range.fromPole);
    const std::string to = tabletext::trimmed(range.toPole);
    if (from.empty() || to.empty()) {
        out.issue = "Välj var " + std::string(word) + " börjar och slutar.";
        return out;
    }
    const PoleHits fromHits = findPole(rows, from);
    const PoleHits toHits = findPole(rows, to);
    if (fromHits.count == 0) {
        out.issue = "Stolpnr " + from + " finns inte i " + std::string(table) + ".";
    } else if (toHits.count == 0) {
        out.issue = "Stolpnr " + to + " finns inte i " + std::string(table) + ".";
    } else if (fromHits.count > 1) {
        out.issue = "Stolpnr " + from + " finns flera gånger i " + std::string(table) + ".";
    } else if (toHits.count > 1) {
        out.issue = "Stolpnr " + to + " finns flera gånger i " + std::string(table) + ".";
    } else if (from == to) {
        out.issue = capitalized(word) + " börjar och slutar på samma stolpe.";
    } else if (toHits.first < fromHits.first) {
        out.issue = "Slutet " + to + " står före starten " + from + " i " + std::string(table) +
                    ". Byt start och slut.";
    } else {
        out.first = fromHits.first;
        out.last = toHits.first;
    }
    return out;
}

// Spannlängderna ur koordinaterna. Helt tomma rader hoppas över, så att föregående stolpe är
// den närmaste raden ovanför med innehåll, som i normalspannsberäkningen.
std::vector<std::optional<double>> coordinateSpans(const std::vector<poletable::PoleRow>& rows) {
    std::vector<std::optional<double>> spans(rows.size());
    const poletable::PoleRow* previous = nullptr;
    for (std::size_t i = 0; i < rows.size(); ++i) {
        const poletable::PoleRow& row = rows[i];
        if (poletable::isEmptyRow(row))
            continue;
        if (previous && !poletable::numberValue(row, poletable::kSpan)) {
            const auto n0 = poletable::numberValue(*previous, poletable::kNorth);
            const auto e0 = poletable::numberValue(*previous, poletable::kEast);
            const auto n1 = poletable::numberValue(row, poletable::kNorth);
            const auto e1 = poletable::numberValue(row, poletable::kEast);
            if (n0 && e0 && n1 && e1) {
                const double d = std::hypot(*n1 - *n0, *e1 - *e0);
                if (d > 0)
                    spans[i] = d;  // samma punkt två gånger ger inget spann
            }
        }
        previous = &row;
    }
    return spans;
}

// Raderna first..last som normalspannets stolpar. Ogiltiga gränser ger en tom lista.
std::vector<ancalc::SpanPole> spanPoles(const std::vector<poletable::PoleRow>& rows, int first, int last) {
    std::vector<ancalc::SpanPole> poles;
    if (first < 0 || last < first || last >= static_cast<int>(rows.size()))
        return poles;
    const std::vector<std::optional<double>> computed = coordinateSpans(rows);
    poles.reserve(static_cast<std::size_t>(last - first + 1));
    for (int i = first; i <= last; ++i) {
        const poletable::PoleRow& row = rows[static_cast<std::size_t>(i)];
        ancalc::SpanPole p;
        p.pole = row.pole;
        p.lengthM = poletable::numberValue(row, poletable::kSpan);
        if (!p.lengthM)
            p.lengthM = computed[static_cast<std::size_t>(i)];  // tom spannlängd: ur koordinaterna
        p.heightM = poletable::numberValue(row, poletable::kHeight);
        poles.push_back(std::move(p));
    }
    return poles;
}

// Sträckans stolpar och normalspannet ur dem. Har sträckan fel byggs svaret för hand med
// bara sträckans fel, och inga stolpar returneras.
RangeSpan rangeSpan(const std::vector<poletable::PoleRow>& rows, const PoleRange& range,
                    std::string_view word, std::string_view table, const ancalc::SpanLimits& limits) {
    RangeSpan span;
    span.rows = findRange(rows, range, word, table);
    if (!span.rows.issue.empty()) {
        span.calc = {std::nullopt,
                     {{ancalc::IssueKind::Error, -1, ancalc::SpanColumn::None, span.rows.issue}}};
        return span;
    }
    span.poles = spanPoles(rows, span.rows.first, span.rows.last);
    span.calc = ancalc::calculateNormalSpan(span.poles, limits);
    return span;
}

// Panelens text: antal spann, aₙ, aₙ,ΔH och avvikelser, eller det första felet med stolpens
// Stolpplats (eller radnumret när Stolpplats saknas).
RangeStatus rangeStatus(const RangeSpan& span, const std::vector<poletable::PoleRow>& rows) {
    RangeStatus status;
    if (span.calc.result) {
        const ancalc::SpanResult& r = *span.calc.result;
        status.ok = true;
        status.text = std::to_string(r.spanCount) + " spann · aₙ " + tenth(r.an) + " m";
        if (r.anDeltaH)
            status.text += " · aₙ,ΔH " + tenth(*r.anDeltaH) + " m";
        const int deviations = deviationCount(r);
        if (deviations == 1)
            status.text += " · 1 avvikelse";
        else if (deviations > 1)
            status.text += " · " + std::to_string(deviations) + " avvikelser";
        return status;
    }
    for (const ancalc::SpanIssue& issue : span.calc.issues) {
        if (issue.kind != ancalc::IssueKind::Error)
            continue;
        if (issue.poleIndex < 0) {
            status.text = issue.message;
            return status;
        }
        const int row = span.rows.first + issue.poleIndex;
        std::string pole;
        if (row >= 0 && row < static_cast<int>(rows.size()))
            pole = tabletext::trimmed(rows[static_cast<std::size_t>(row)].pole);
        const std::string label = pole.empty() ? "Rad " + std::to_string(row + 1) : pole;
        status.text = label + ": " + issue.message;
        return status;
    }
    return status;
}

// Stolpplatserna som finns på mer än en rad, i första förekomstens ordning.
std::vector<std::string> duplicatePoles(const std::vector<poletable::PoleRow>& rows) {
    std::unordered_map<std::string, int> counts;
    std::vector<std::string> order;
    for (const poletable::PoleRow& row : rows) {
        const std::string pole = tabletext::trimmed(row.pole);
        if (pole.empty())
            continue;
        if (++counts[pole] == 1)
            order.push_back(pole);
    }
    std::vector<std::string> out;
    for (const std::string& pole : order)
        if (counts[pole] > 1)
            out.push_back(pole);
    return out;
}

namespace {

// Sant när två fält i samma kolumn är lika: tal som tal (tomt bara mot tomt), text exakt.
bool sameField(const poletable::PoleRow& a, const poletable::PoleRow& b, int column) {
    const std::string& x = poletable::field(a, column);
    const std::string& y = poletable::field(b, column);
    if (!poletable::isNumberColumn(column))
        return x == y;
    if (x.empty() || y.empty())
        return x.empty() && y.empty();
    const std::optional<double> nx = poletable::numberValue(a, column);
    const std::optional<double> ny = poletable::numberValue(b, column);
    if (nx && ny)
        return *nx == *ny;
    return x == y;
}

} // namespace

// Matchar varje rad i efter mot före på Stolpplats och jämför fälten. En Stolpplats som finns
// flera gånger i före är osäker, och en rad utan träff eller utan Stolpplats är ny.
std::vector<RowCompare> compareAfter(const std::vector<poletable::PoleRow>& before,
                                     const std::vector<poletable::PoleRow>& after) {
    std::unordered_map<std::string, std::vector<int>> index;
    for (std::size_t i = 0; i < before.size(); ++i) {
        const std::string pole = tabletext::trimmed(before[i].pole);
        if (!pole.empty())
            index[pole].push_back(static_cast<int>(i));
    }
    std::vector<RowCompare> out(after.size());
    for (std::size_t i = 0; i < after.size(); ++i) {
        const poletable::PoleRow& row = after[i];
        RowCompare& cmp = out[i];
        if (poletable::isEmptyRow(row)) {
            cmp.state = RowState::Empty;
            continue;
        }
        const std::string pole = tabletext::trimmed(row.pole);
        const auto it = pole.empty() ? index.end() : index.find(pole);
        if (it == index.end()) {
            cmp.state = RowState::New;
        } else if (it->second.size() > 1) {
            cmp.state = RowState::Ambiguous;
        } else {
            cmp.beforeRow = it->second.front();
            const poletable::PoleRow& match = before[static_cast<std::size_t>(cmp.beforeRow)];
            cmp.state = RowState::Same;
            for (int c = 0; c < poletable::kColumnCount; ++c) {
                if (!sameField(row, match, c)) {
                    cmp.changed[static_cast<std::size_t>(c)] = true;
                    cmp.state = RowState::Changed;
                }
            }
        }
    }
    return out;
}

// Markeringarna: start- och slutnoter per rad, normalspannets fel i giltiga sträckor och
// dubbletter av Stolpplats. En sträcka med fel ger ingen markering.
TableMarks tableMarks(const std::vector<poletable::PoleRow>& rows, const std::vector<NamedRange>& ranges,
                      std::string_view table) {
    TableMarks marks;
    marks.rowNotes.assign(rows.size(), std::string());

    // Dubbletterna först, i tabellens ordning.
    const std::vector<std::string> duplicates = duplicatePoles(rows);
    if (!duplicates.empty()) {
        for (std::size_t i = 0; i < rows.size(); ++i) {
            const std::string pole = tabletext::trimmed(rows[i].pole);
            for (const std::string& d : duplicates) {
                if (d == pole) {
                    marks.issues.push_back({static_cast<int>(i), poletable::kPole,
                                            "Stolpnr " + pole + " finns flera gånger i " +
                                                std::string(table) + "."});
                    break;
                }
            }
        }
    }

    for (const NamedRange& named : ranges) {
        const RangeRows where = findRange(rows, named.range, named.word, table);
        if (!where.issue.empty())
            continue;
        const std::string word = capitalized(named.word);
        auto addNote = [&](int row, const std::string& text) {
            std::string& note = marks.rowNotes[static_cast<std::size_t>(row)];
            if (!note.empty())
                note += char{10};
            note += text;
        };
        addNote(where.first, word + " börjar här.");
        addNote(where.last, word + " slutar här.");

        const ancalc::SpanCalculation calc =
            ancalc::calculateNormalSpan(spanPoles(rows, where.first, where.last), ancalc::SpanLimits{});
        for (const ancalc::SpanIssue& issue : calc.issues) {
            if (issue.kind != ancalc::IssueKind::Error || issue.poleIndex < 0)
                continue;
            int column = poletable::kPole;
            if (issue.column == ancalc::SpanColumn::Length)
                column = poletable::kSpan;
            else if (issue.column == ancalc::SpanColumn::Height)
                column = poletable::kHeight;
            marks.issues.push_back({where.first + issue.poleIndex, column, word + ": " + issue.message});
        }
    }
    return marks;
}

} // namespace kalkyl::automode
