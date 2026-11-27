#include <iostream>
#include <string>
#include <vector>

#include "poleView/pole_view.h"

using namespace kalkyl::poleview;
using kalkyl::poletable::PoleRow;

namespace {

int g_failures = 0;

// Räknar ett misslyckande och skriver var det hände.
void report(bool ok, const char* expr, const char* file, int line)
{
    if (ok)
        return;
    ++g_failures;
    std::cerr << file << ":" << line << ": CHECK misslyckades: " << expr << std::endl;
}
#define CHECK(...) report(static_cast<bool>(__VA_ARGS__), #__VA_ARGS__, __FILE__, __LINE__)

// En rad med Stolpnr, spannlängd och H, Av och Ah ifyllda.
PoleRow poleRow(const std::string& pole, const std::string& span, const std::string& height)
{
    return {pole, "300", "300", span, "", "", height, ""};
}

// Bilaga A: tio stolpar ST0100-ST0109.
std::vector<PoleRow> appendixA()
{
    return {
        poleRow("ST0100", "", "40.0"),    poleRow("ST0101", "300", "42.5"), poleRow("ST0102", "320", "45.0"),
        poleRow("ST0103", "280", "43.0"), poleRow("ST0104", "350", "47.5"), poleRow("ST0105", "310", "50.0"),
        poleRow("ST0106", "290", "48.0"), poleRow("ST0107", "330", "46.5"), poleRow("ST0108", "300", "44.0"),
        poleRow("ST0109", "270", "41.0"),
    };
}

// Provtabellen: 801 rader STO100-STO900 med lika värden.
std::vector<PoleRow> stoRows()
{
    std::vector<PoleRow> rows;
    for (int i = 0; i < 801; ++i)
        rows.push_back(poleRow("STO" + std::to_string(100 + i), "300", "40.0"));
    return rows;
}

// En rad i en förväntad vy: Pole med first < last står för en Pole per rad.
struct Spec {
    ViewKind kind;
    int first;
    int last;
};

// Bygger den förväntade vyn av specarna och en Blank med count rader sist.
PoleView expected(const std::vector<Spec>& specs, int count)
{
    PoleView view;
    for (const Spec& s : specs) {
        if (s.kind == ViewKind::Pole) {
            for (int m = s.first; m <= s.last; ++m)
                view.rows.push_back({ViewKind::Pole, m, m});
        } else {
            view.rows.push_back({s.kind, s.first, s.last});
        }
    }
    view.rows.push_back({ViewKind::Blank, count, count});
    return view;
}

// Märkena för count rader med de givna raderna sanna.
std::vector<bool> marksOf(int count, const std::vector<int>& rows)
{
    std::vector<bool> marks(static_cast<std::size_t>(count), false);
    for (int r : rows)
        marks[static_cast<std::size_t>(r)] = true;
    return marks;
}

// De sanna posterna som radnummer.
std::vector<int> indicesOf(const std::vector<bool>& marks)
{
    std::vector<int> out;
    for (std::size_t i = 0; i < marks.size(); ++i)
        if (marks[i])
            out.push_back(static_cast<int>(i));
    return out;
}

constexpr ViewKind P = ViewKind::Pole;
constexpr ViewKind H = ViewKind::Hidden;

// Reglerna i planen 5.2 och användarens exempel.
void testBuildView()
{
    const int n = 801;
    CHECK(buildView(n, marksOf(n, {350}), Filter::Changed) == expected({{H, 0, 347}, {P, 348, 352}, {H, 353, 800}}, n));
    CHECK(buildView(n, marksOf(n, {350, 360}), Filter::Changed) ==
          expected({{H, 0, 347}, {P, 348, 362}, {H, 363, 800}}, n));
    CHECK(buildView(n, marksOf(n, {350, 356}), Filter::Changed) ==
          expected({{H, 0, 347}, {P, 348, 358}, {H, 359, 800}}, n));
    CHECK(buildView(n, marksOf(n, {350, 357}), Filter::Changed) ==
          expected({{H, 0, 347}, {P, 348, 359}, {H, 360, 800}}, n));
    // Allt emellan syns också när ändringarna ligger långt isär.
    CHECK(buildView(n, marksOf(n, {20, 780}), Filter::Changed) ==
          expected({{H, 0, 17}, {P, 18, 782}, {H, 783, 800}}, n));
    CHECK(buildView(n, marksOf(n, {0}), Filter::Changed) == expected({{P, 0, 2}, {H, 3, 800}}, n));
    CHECK(buildView(n, marksOf(n, {3}), Filter::Changed) == expected({{P, 0, 5}, {H, 6, 800}}, n));
    CHECK(buildView(n, marksOf(n, {800}), Filter::Changed) == expected({{H, 0, 797}, {P, 798, 800}}, n));

    // Inga märkta rader eller filtret All ger identiteten.
    CHECK(buildView(n, marksOf(n, {}), Filter::Changed) == identityView(n));
    CHECK(buildView(n, marksOf(n, {350}), Filter::All) == identityView(n));
    CHECK(identityView(3) == expected({{P, 0, 2}}, 3));

    // Få rader.
    CHECK(buildView(0, {}, Filter::Changed) == expected({}, 0));
    CHECK(buildView(0, {}, Filter::Changed).rows.size() == 1);
    CHECK(buildView(3, marksOf(3, {1}), Filter::Changed) == expected({{P, 0, 2}}, 3));

    // Poster bortom count ignoreras, och saknade poster är falska.
    CHECK(buildView(3, marksOf(10, {8}), Filter::Changed) == identityView(3));
    CHECK(buildView(10, marksOf(2, {1}), Filter::Changed) == expected({{P, 0, 3}, {H, 4, 9}}, 10));
}

// Raderna som de blir i Stolptabell efter vid en kopiering från före: Av i alla tre faserna.
std::vector<PoleRow> asAfter(std::vector<PoleRow> rows)
{
    for (PoleRow& row : rows) {
        row.av2 = row.av;
        row.av3 = row.av;
    }
    return rows;
}

// Vilka rader som är märkta: ändrad, ny, tom och osäker rad, men inte Same.
void testFilterMarks()
{
    const std::vector<PoleRow> before = appendixA();

    CHECK(filterMarks(before, asAfter(before)) == marksOf(10, {}));

    std::vector<PoleRow> changed = asAfter(before);
    changed[1].spanM = "999";
    CHECK(indicesOf(filterMarks(before, changed)) == std::vector<int>({1}));

    // Av Fas2 jämförs med Av i före, och en tom Fas3 skiljer sig från Av.
    std::vector<PoleRow> phases = asAfter(before);
    phases[2].av2 = "1";
    phases[5].av3.clear();
    CHECK(indicesOf(filterMarks(before, phases)) == std::vector<int>({2, 5}));

    // En ny Stolpnr märker raden och raderna närmast den borttagna stolpen.
    std::vector<PoleRow> fresh = asAfter(before);
    fresh[3].pole = "ST0200";
    CHECK(indicesOf(filterMarks(before, fresh)) == std::vector<int>({2, 3, 4}));

    // En tom rad är märkt, och dess borttagna stolpe märker grannarna.
    std::vector<PoleRow> empty = asAfter(before);
    empty[7] = PoleRow{};
    CHECK(indicesOf(filterMarks(before, empty)) == std::vector<int>({6, 7, 8}));

    // En osäker rad: ST0103 finns två gånger i före, och ST0108 saknas i före (ny).
    std::vector<PoleRow> dupBefore = before;
    dupBefore[8].pole = "ST0103";
    CHECK(indicesOf(filterMarks(dupBefore, asAfter(before))) == std::vector<int>({3, 8}));
}

// Ankarraderna vid en borttagen stolpe och antalet borttagna.
void testRemovedAnchors()
{
    const std::vector<PoleRow> before = appendixA();

    std::vector<PoleRow> noPole4 = before;
    noPole4.erase(noPole4.begin() + 4);
    CHECK(removedAnchors(before, noPole4) == std::vector<int>({3, 4}));
    CHECK(removedCount(before, noPole4) == 1);

    std::vector<PoleRow> noFirst = before;
    noFirst.erase(noFirst.begin());
    CHECK(removedAnchors(before, noFirst) == std::vector<int>({0}));

    std::vector<PoleRow> noLast = before;
    noLast.pop_back();
    CHECK(removedAnchors(before, noLast) == std::vector<int>({8}));

    // ST0104 och ST0105 borta: ST0103 och ST0106 är ankarraderna, på rad 3 och 4 i efter.
    std::vector<PoleRow> noTwo = before;
    noTwo.erase(noTwo.begin() + 4, noTwo.begin() + 6);
    CHECK(removedAnchors(before, noTwo) == std::vector<int>({3, 4}));
    CHECK(removedCount(before, noTwo) == 2);

    // Tom efter: ingen ankarrad, alla tio borttagna.
    CHECK(removedAnchors(before, {}).empty());
    CHECK(removedCount(before, {}) == 10);

    // En Stolpnr som finns två gånger i efter hoppas över, och sökningen går vidare.
    std::vector<PoleRow> dup = before;
    dup.erase(dup.begin() + 3);
    dup.push_back(poleRow("ST0102", "320", "45.0"));
    CHECK(removedAnchors(before, dup) == std::vector<int>({1, 3}));

    // En tom Stolpnr i före räknas aldrig som borttagen.
    std::vector<PoleRow> blank = before;
    blank[5].pole.clear();
    CHECK(removedCount(blank, before) == 0);
    CHECK(removedAnchors(blank, before).empty());

    // Ingenting borttaget.
    CHECK(removedAnchors(before, before).empty());
    CHECK(removedCount(before, before) == 0);
}

// Vyn för STO450 ändrad.
PoleView viewOf350()
{
    return buildView(801, marksOf(801, {350}), Filter::Changed);
}

// Index mellan vyn och tabellen.
void testIndex()
{
    const PoleView v = viewOf350();
    CHECK(v.rows.size() == 8);

    CHECK(modelRow(v, 0) == -1);
    CHECK(modelRow(v, 1) == 348);
    CHECK(modelRow(v, 5) == 352);
    CHECK(modelRow(v, 6) == -1);
    CHECK(modelRow(v, 7) == 801);
    CHECK(modelRow(v, 9) == 803);
    CHECK(modelRow(v, -1) == -1);

    CHECK(viewRowOf(v, 0) == 0);
    CHECK(viewRowOf(v, 347) == 0);
    CHECK(viewRowOf(v, 348) == 1);
    CHECK(viewRowOf(v, 400) == 6);
    CHECK(viewRowOf(v, 801) == 7);
    CHECK(viewRowOf(v, 805) == 7);
    CHECK(viewRowOf(v, -1) == -1);

    CHECK(visibleViewRow(v, 100, 1) == 1);
    CHECK(visibleViewRow(v, 100, -1) == 1);
    CHECK(visibleViewRow(v, 400, -1) == 5);
    CHECK(visibleViewRow(v, 400, 1) == 7);
    CHECK(visibleViewRow(v, 350, 1) == 3);
    CHECK(visibleViewRow(v, 350, -1) == 3);
    CHECK(visibleViewRow(v, -1, 1) == -1);

    CHECK(isHiddenRow(v, 0));
    CHECK(isHiddenRow(v, 6));
    CHECK(!isHiddenRow(v, 1));
    CHECK(!isHiddenRow(v, 7));
    CHECK(!isHiddenRow(v, -1));
    CHECK(!isHiddenRow(v, 8));
    CHECK(rowCount(v) == 801);
    CHECK(rowCount(PoleView{}) == 0);
}

// Samma form för en tabell med ett annat antal rader.
void testResizeView()
{
    const PoleView v = viewOf350();
    CHECK(resizeView(v, 803) == expected({{H, 0, 347}, {P, 348, 352}, {H, 353, 800}, {P, 801, 802}}, 803));
    CHECK(resizeView(v, 801) == v);
    CHECK(resizeView(v, 354) == expected({{H, 0, 347}, {P, 348, 353}}, 354));
    CHECK(resizeView(v, 355) == expected({{H, 0, 347}, {P, 348, 352}, {H, 353, 354}}, 355));
    CHECK(resizeView(v, 200) == expected({{H, 0, 199}}, 200));
    CHECK(resizeView(v, 1) == expected({{P, 0, 0}}, 1));
    CHECK(resizeView(v, 0) == expected({}, 0));
    CHECK(resizeView(PoleView{}, 2) == expected({{P, 0, 1}}, 2));
}

// De grå radens text och tooltip, och radens namn.
void testTexts()
{
    const std::vector<PoleRow> rows = stoRows();
    CHECK(rowLabel(rows, 0) == "STO100");
    CHECK(hiddenText(rows, {H, 0, 347}) == "Dolda STO100–STO447 (348 stolpar som i före)");
    CHECK(hiddenText(rows, {H, 353, 800}) == "Dolda STO453–STO900 (448 stolpar som i före)");

    std::vector<PoleRow> withBlank = rows;
    withBlank[4].pole = "  ";
    CHECK(rowLabel(withBlank, 4) == "rad 5");
    CHECK(hiddenText(withBlank, {H, 4, 6}) == "Dolda rad 5–STO106 (3 stolpar som i före)");

    CHECK(hiddenToolTip({H, 0, 347}) ==
          "348 stolpar som är som i Stolptabell före är dolda av filtret Förändrade ±2. Välj Alla i filtret "
          "ovanför tabellen, eller högerklicka här och välj Visa alla rader.");
}

// Rullningsmålet: mitten mitt i synfältet, annars första raden överst, klampat.
void testScrollTop()
{
    CHECK(scrollTop(1, 5, 8, 20.0f, 100.0f) == 20.0f);
    CHECK(scrollTop(3, 4, 20, 20.0f, 200.0f) == 0.0f);
    CHECK(scrollTop(15, 16, 20, 20.0f, 200.0f) == 200.0f);
    CHECK(scrollTop(2, 30, 50, 20.0f, 200.0f) == 40.0f);
    CHECK(scrollTop(0, 0, 5, 20.0f, 200.0f) == 0.0f);
    CHECK(scrollTop(2, 3, 50, 0.0f, 200.0f) == 0.0f);
    CHECK(scrollTop(2, 3, 50, 20.0f, 0.0f) == 0.0f);
    // Området i mitten: rad 20-21 i 100 rader, synfält 200 px.
    CHECK(scrollTop(20, 21, 100, 20.0f, 200.0f) == 20.0f * 20.0f + 20.0f - 100.0f);
}

// Siffrorna och texterna i infotexten.
void testSummary()
{
    const std::vector<PoleRow> before = stoRows();
    std::vector<PoleRow> after = asAfter(before);
    after[300].spanM = "999";
    after[500].spanM = "999";
    after.erase(after.begin() + 100);

    const Summary s = summarize(before, after, Filter::Changed);
    CHECK(s.rows == 800);
    CHECK(s.changed == 2);
    CHECK(s.marked == 4);
    CHECK(s.removed == 1);
    CHECK(s.shown == 405);   // raderna 97-501 i ett enda område
    CHECK(summaryText(s, Filter::Changed) == "Förändrade: 2 av 800 rader · Borttagna: 1 · Visas: 405 av 800");
    CHECK(summaryText(s, Filter::All) == "Förändrade: 2 av 800 rader · Borttagna: 1");
    CHECK(summarize(before, after, Filter::All).shown == 800);

    // Utan borttagna stolpar.
    std::vector<PoleRow> two = asAfter(before);
    two[350].spanM = "999";
    two[360].spanM = "999";
    const Summary t = summarize(before, two, Filter::Changed);
    CHECK(t == Summary({801, 2, 2, 0, 15}));
    CHECK(summaryText(t, Filter::Changed) == "Förändrade: 2 av 801 rader · Visas: 15 av 801");
    CHECK(summaryText(t, Filter::All) == "Förändrade: 2 av 801 rader");

    // Inga förändringar.
    const Summary none = summarize(before, asAfter(before), Filter::Changed);
    CHECK(none.changed == 0 && none.marked == 0 && none.shown == 801);
    CHECK(summaryText(none, Filter::Changed) == "Inga förändringar mot Stolptabell före. Alla rader visas.");
    CHECK(summaryText(none, Filter::All) == "Inga förändringar mot Stolptabell före.");
    CHECK(summaryText(Summary{}, Filter::Changed).empty());
    CHECK(summaryText(Summary{}, Filter::All).empty());

    // Bara borttagna stolpar.
    Summary onlyRemoved;
    onlyRemoved.rows = 0;
    onlyRemoved.removed = 3;
    CHECK(summaryText(onlyRemoved, Filter::Changed) == "Förändrade: 0 av 0 rader · Borttagna: 3 · Visas: 0 av 0");

    CHECK(markedRange(marksOf(10, {})) == MarkedRange({-1, -1}));
    CHECK(markedRange(marksOf(10, {3})) == MarkedRange({3, 3}));
    CHECK(markedRange(marksOf(10, {7, 2, 5})) == MarkedRange({2, 7}));
    CHECK(markedRange({}) == MarkedRange({-1, -1}));
}

} // namespace

int main()
{
    testBuildView();
    testFilterMarks();
    testRemovedAnchors();
    testIndex();
    testResizeView();
    testTexts();
    testScrollTop();
    testSummary();
    if (g_failures != 0) {
        std::cerr << g_failures << " kontroller misslyckades" << std::endl;
        return 1;
    }
    std::cout << "poleview: alla kontroller gick igenom" << std::endl;
    return 0;
}
