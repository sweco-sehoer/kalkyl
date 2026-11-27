#include "poleView/pole_view.h"

#include <algorithm>
#include <cstddef>
#include <string>
#include <unordered_map>

#include "autoMode/auto_mode.h"
#include "tableText/table_text.h"

namespace kalkyl::poleview {

namespace {

// Radens Stolpnr trimmad. Tom text betyder att raden saknar Stolpnr.
std::string poleOf(const poletable::PoleRow& row) {
    return tabletext::trimmed(row.pole);
}

// Antalet rader i after per Stolpnr, och den sista raden med Stolpnr. Tom Stolpnr räknas inte.
struct PoleIndex {
    int count = 0;
    int row = -1;
};
std::unordered_map<std::string, PoleIndex> indexAfter(const std::vector<poletable::PoleRow>& after) {
    std::unordered_map<std::string, PoleIndex> index;
    for (std::size_t i = 0; i < after.size(); ++i) {
        const std::string pole = poleOf(after[i]);
        if (pole.empty())
            continue;
        PoleIndex& hit = index[pole];
        ++hit.count;
        hit.row = static_cast<int>(i);
    }
    return index;
}

// Antalet rader i vyn som inte är Blank.
int dataRows(const PoleView& view) {
    return view.rows.empty() || view.rows.back().kind != ViewKind::Blank
               ? static_cast<int>(view.rows.size())
               : static_cast<int>(view.rows.size()) - 1;
}

} // namespace

// Två genomgångar över före ger för varje rad närmaste rad ovanför och nedanför vars Stolpnr finns
// på exakt en rad i efter. Varje borttagen rad lägger sina två ankarrader till svaret.
std::vector<int> removedAnchors(const std::vector<poletable::PoleRow>& before,
                                const std::vector<poletable::PoleRow>& after) {
    const std::unordered_map<std::string, PoleIndex> index = indexAfter(after);
    const std::size_t n = before.size();
    // anchor[i]: raden i efter när före-rad i har en enda träff, annars -1. removed[i]: Stolpnr saknas i efter.
    std::vector<int> anchor(n, -1);
    std::vector<bool> removed(n, false);
    for (std::size_t i = 0; i < n; ++i) {
        const std::string pole = poleOf(before[i]);
        if (pole.empty())
            continue;
        const auto it = index.find(pole);
        if (it == index.end())
            removed[i] = true;
        else if (it->second.count == 1)
            anchor[i] = it->second.row;
    }

    std::vector<int> above(n, -1);
    std::vector<int> below(n, -1);
    int last = -1;
    for (std::size_t i = 0; i < n; ++i) {
        above[i] = last;
        if (anchor[i] >= 0)
            last = anchor[i];
    }
    last = -1;
    for (std::size_t i = n; i-- > 0;) {
        below[i] = last;
        if (anchor[i] >= 0)
            last = anchor[i];
    }

    std::vector<int> out;
    for (std::size_t i = 0; i < n; ++i) {
        if (!removed[i])
            continue;
        if (above[i] >= 0)
            out.push_back(above[i]);
        if (below[i] >= 0)
            out.push_back(below[i]);
    }
    std::sort(out.begin(), out.end());
    out.erase(std::unique(out.begin(), out.end()), out.end());
    return out;
}

// Rader i före med en Stolpnr som inte finns någonstans i efter.
int removedCount(const std::vector<poletable::PoleRow>& before, const std::vector<poletable::PoleRow>& after) {
    const std::unordered_map<std::string, PoleIndex> index = indexAfter(after);
    int count = 0;
    for (const poletable::PoleRow& row : before) {
        const std::string pole = poleOf(row);
        if (!pole.empty() && index.find(pole) == index.end())
            ++count;
    }
    return count;
}

// En rad är märkt när den inte är Same mot före, eller när den ligger närmast en borttagen stolpe.
std::vector<bool> filterMarks(const std::vector<poletable::PoleRow>& before,
                              const std::vector<poletable::PoleRow>& after) {
    const std::vector<automode::RowCompare> compare = automode::compareAfter(before, after);
    std::vector<bool> marks(after.size(), false);
    for (std::size_t i = 0; i < compare.size(); ++i)
        marks[i] = compare[i].state != automode::RowState::Same;
    for (int row : removedAnchors(before, after))
        if (row >= 0 && row < static_cast<int>(marks.size()))
            marks[static_cast<std::size_t>(row)] = true;
    return marks;
}

// Första och sista märkta raden.
MarkedRange markedRange(const std::vector<bool>& marks) {
    MarkedRange range;
    for (std::size_t i = 0; i < marks.size(); ++i) {
        if (!marks[i])
            continue;
        if (range.first < 0)
            range.first = static_cast<int>(i);
        range.last = static_cast<int>(i);
    }
    return range;
}

// En Pole per rad och Blank sist.
PoleView identityView(int count) {
    PoleView view;
    count = std::max(count, 0);
    view.rows.reserve(static_cast<std::size_t>(count) + 1);
    for (int i = 0; i < count; ++i)
        view.rows.push_back({ViewKind::Pole, i, i});
    view.rows.push_back({ViewKind::Blank, count, count});
    return view;
}

// Första märkta raden minus margin till sista plus margin, klampat till tabellen. Bara posterna
// för tabellens rader räknas; poster bortom count ignoreras.
MarkedRange changedArea(int count, const std::vector<bool>& marks, int margin) {
    count = std::max(count, 0);
    const std::size_t usable = std::min(static_cast<std::size_t>(count), marks.size());
    const MarkedRange range =
        markedRange(std::vector<bool>(marks.begin(), marks.begin() + static_cast<std::ptrdiff_t>(usable)));
    if (range.first < 0)
        return {-1, -1};
    return {std::max(0, range.first - margin), std::min(count - 1, range.last + margin)};
}

// Reglerna i planen 5.2: ett enda område syns (changedArea med kMargin), med allt emellan. Raderna
// före och efter blir var sin grå rad när de är minst två, och en ensam rad visas som vanligt.
PoleView buildView(int count, const std::vector<bool>& marks, Filter filter) {
    count = std::max(count, 0);
    const MarkedRange area = changedArea(count, marks, kMargin);
    if (filter == Filter::All || area.first < 0)
        return identityView(count);

    // Raderna före och efter området: en grå rad när de är minst två, annars en vanlig rad.
    PoleView view;
    auto addOutside = [&view](int first, int last) {
        if (last - first + 1 >= 2) {
            view.rows.push_back({ViewKind::Hidden, first, last});
            return;
        }
        for (int m = first; m <= last; ++m)
            view.rows.push_back({ViewKind::Pole, m, m});
    };
    addOutside(0, area.first - 1);
    for (int m = area.first; m <= area.last; ++m)
        view.rows.push_back({ViewKind::Pole, m, m});
    addOutside(area.last + 1, count - 1);
    view.rows.push_back({ViewKind::Blank, count, count});
    return view;
}

// Behåller vyns form för en tabell med ett annat antal rader: bortom count tas bort, en grå rad
// klipps och nya rader läggs som Pole före Blank.
PoleView resizeView(const PoleView& view, int count) {
    count = std::max(count, 0);
    PoleView out;
    const int data = dataRows(view);
    for (int i = 0; i < data; ++i) {
        ViewRow row = view.rows[static_cast<std::size_t>(i)];
        if (row.first >= count)
            break;
        if (row.kind == ViewKind::Hidden) {
            row.last = std::min(row.last, count - 1);
            if (row.first == row.last)
                row.kind = ViewKind::Pole;
        }
        out.rows.push_back(row);
    }
    for (int m = out.rows.empty() ? 0 : out.rows.back().last + 1; m < count; ++m)
        out.rows.push_back({ViewKind::Pole, m, m});
    out.rows.push_back({ViewKind::Blank, count, count});
    return out;
}

// Blank.first, eller 0 när vyn inte har någon Blank.
int rowCount(const PoleView& view) {
    if (view.rows.empty() || view.rows.back().kind != ViewKind::Blank)
        return 0;
    return view.rows.back().first;
}

// Pole och Blank ger sin rad, Hidden och v < 0 ger -1, och raderna under Blank ger nya rader.
int modelRow(const PoleView& view, int v) {
    if (v < 0 || view.rows.empty())
        return -1;
    const int size = static_cast<int>(view.rows.size());
    if (v < size) {
        const ViewRow& row = view.rows[static_cast<std::size_t>(v)];
        return row.kind == ViewKind::Hidden ? -1 : row.first;
    }
    return rowCount(view) + (v - (size - 1));
}

// Raden som innehåller m: binärsökning på sista raden i varje vyrad.
int viewRowOf(const PoleView& view, int m) {
    if (m < 0 || view.rows.empty())
        return -1;
    const int blank = static_cast<int>(view.rows.size()) - 1;
    if (m >= rowCount(view))
        return blank;
    int lo = 0;
    int hi = blank;
    while (lo < hi) {
        const int mid = (lo + hi) / 2;
        if (view.rows[static_cast<std::size_t>(mid)].last < m)
            lo = mid + 1;
        else
            hi = mid;
    }
    return lo;
}

// Vyrad för m som inte är grå: den grå radens efterträdare, eller föregångare med step -1.
int visibleViewRow(const PoleView& view, int m, int step) {
    const int v = viewRowOf(view, m);
    if (v < 0 || view.rows[static_cast<std::size_t>(v)].kind != ViewKind::Hidden)
        return v;
    if (step < 0 && v > 0)
        return v - 1;
    return v + 1;
}

// Sant för en grå rad.
bool isHiddenRow(const PoleView& view, int v) {
    return v >= 0 && v < static_cast<int>(view.rows.size()) &&
           view.rows[static_cast<std::size_t>(v)].kind == ViewKind::Hidden;
}

// Stolpnr trimmad, eller "rad N" (1-baserad) när raden saknar Stolpnr eller ligger utanför tabellen.
std::string rowLabel(const std::vector<poletable::PoleRow>& rows, int m) {
    if (m >= 0 && m < static_cast<int>(rows.size())) {
        const std::string pole = poleOf(rows[static_cast<std::size_t>(m)]);
        if (!pole.empty())
            return pole;
    }
    return "rad " + std::to_string(m + 1);
}

// Den grå radens text: första och sista dolda raden och antalet.
std::string hiddenText(const std::vector<poletable::PoleRow>& rows, const ViewRow& hidden) {
    const int n = hidden.last - hidden.first + 1;
    return "Dolda " + rowLabel(rows, hidden.first) + "–" + rowLabel(rows, hidden.last) + " (" +
           std::to_string(n) + " stolpar som i före)";
}

// Den grå radens tooltip.
std::string hiddenToolTip(const ViewRow& hidden) {
    const int n = hidden.last - hidden.first + 1;
    return std::to_string(n) +
           " stolpar som är som i Stolptabell före är dolda av filtret Förändrade ±2. Välj Alla i filtret "
           "ovanför tabellen, eller högerklicka här och välj Visa alla rader.";
}

// Mitten av området mitt i synfältet när det ryms, annars första raden överst, klampat mot ändarna.
float scrollTop(int first, int last, int viewRows, float rowHeight, float viewportHeight) {
    if (rowHeight <= 0.0f || viewportHeight <= 0.0f)
        return 0.0f;
    const float maxScroll = std::max(0.0f, static_cast<float>(viewRows) * rowHeight - viewportHeight);
    const float top = static_cast<float>(first) * rowHeight;
    const float height = static_cast<float>(last - first + 1) * rowHeight;
    const float target = height <= viewportHeight ? top + height / 2.0f - viewportHeight / 2.0f : top;
    return std::clamp(target, 0.0f, maxScroll);
}

// Siffrorna ur jämförelsen, märkningen och vyn med filtret.
Summary summarize(const std::vector<poletable::PoleRow>& before, const std::vector<poletable::PoleRow>& after,
                  Filter filter) {
    Summary s;
    s.rows = static_cast<int>(after.size());
    for (const automode::RowCompare& c : automode::compareAfter(before, after))
        if (c.state != automode::RowState::Same)
            ++s.changed;
    const std::vector<bool> marks = filterMarks(before, after);
    for (bool mark : marks)
        if (mark)
            ++s.marked;
    s.removed = removedCount(before, after);
    const PoleView view = buildView(s.rows, marks, filter);
    for (const ViewRow& row : view.rows)
        if (row.kind == ViewKind::Pole)
            ++s.shown;
    return s;
}

// Texten enligt planen 7.2.
std::string summaryText(const Summary& summary, Filter filter) {
    if (summary.rows == 0 && summary.removed == 0)
        return std::string();
    if (summary.changed == 0 && summary.removed == 0) {
        return filter == Filter::Changed ? "Inga förändringar mot Stolptabell före. Alla rader visas."
                                         : "Inga förändringar mot Stolptabell före.";
    }
    const std::string rows = std::to_string(summary.rows);
    std::string text = "Förändrade: " + std::to_string(summary.changed) + " av " + rows + " rader";
    if (summary.removed > 0)
        text += " · Borttagna: " + std::to_string(summary.removed);
    if (filter == Filter::Changed)
        text += " · Visas: " + std::to_string(summary.shown) + " av " + rows;
    return text;
}

} // namespace kalkyl::poleview
