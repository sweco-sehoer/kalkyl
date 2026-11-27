#include <iostream>
#include <string>
#include <vector>

#include "lineHeight/line_height.h"
#include "poleView/pole_view.h"

using namespace kalkyl::lineheight;
using kalkyl::poletable::PoleRow;
namespace pt = kalkyl::poletable;

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

// En rad med Stolpnr, id och Av i alla tre faserna, H 140.
PoleRow row(const std::string& pole, std::int64_t id)
{
    PoleRow r;
    r.pole = pole;
    r.av = "300";
    r.av2 = "300";
    r.av3 = "300";
    r.height = "140";
    r.id = id;
    return r;
}

// Provtabellen i efter: tolv rader ST0374-ST0385 med id 1-12.
std::vector<PoleRow> twelve()
{
    std::vector<PoleRow> rows;
    for (int i = 0; i < 12; ++i)
        rows.push_back(row("ST0" + std::to_string(374 + i), i + 1));
    return rows;
}

// Stolptabell före för provtabellen: samma rader utan id och utan Av Fas2 och Fas3.
std::vector<PoleRow> beforeRows()
{
    std::vector<PoleRow> rows = twelve();
    for (PoleRow& r : rows) {
        r.av2.clear();
        r.av3.clear();
        r.id = 0;
    }
    return rows;
}

// Som twelve() men med ST0379 (rad 5, id 6) ändrad: Av 273,29, Av Fas2 273,29 och Av Fas3 270,1.
std::vector<PoleRow> asChanged()
{
    std::vector<PoleRow> rows = twelve();
    rows[5].av = "273.29";
    rows[5].av2 = "273.29";
    rows[5].av3 = "270.1";
    return rows;
}

// Användarens sparade värden: spannen {5, 6} och {6, 7} och topplinans Av för id 6.
StoredHeights userStored()
{
    StoredHeights s;
    s.spans[spanKey(5, 6)] = {"12.33", "12.33", "22.63"};
    s.spans[spanKey(6, 7)] = {"21.2", "21.2", "31.17"};
    s.topAv[6] = "265.9332";
    return s;
}

// Blocket med radens id, eller nullptr.
const PoleBlock* blockWithId(const std::vector<PoleBlock>& blocks, std::int64_t id)
{
    for (const PoleBlock& b : blocks)
        if (b.id == id)
            return &b;
    return nullptr;
}

// Blockens namn i ordning.
std::vector<std::string> labelsOf(const std::vector<PoleBlock>& blocks)
{
    std::vector<std::string> out;
    for (const PoleBlock& b : blocks)
        out.push_back(b.label);
    return out;
}

// Ett spann för medelprovet.
SpanRef spanOf(bool exists, bool usable, const std::string& outer)
{
    SpanRef s;
    s.exists = exists;
    s.usable = usable;
    s.heights.outer = outer;
    return s;
}

// Nycklarna: ordnade och giltiga.
void testKeys()
{
    CHECK(spanKey(7, 6) == SpanKey({6, 7}));
    CHECK(spanKey(6, 7) == SpanKey({6, 7}));
    CHECK(isValidKey({1, 2}));
    CHECK(!isValidKey({0, 2}));
    CHECK(!isValidKey({-1, 2}));
    CHECK(!isValidKey({3, 3}));
    CHECK(!isValidKey({4, 3}));
}

// Spannen i efter: grannrader som inte är helt tomma och har id.
void testAfterSpans()
{
    const std::vector<SpanKey> all = afterSpans(twelve());
    CHECK(all.size() == 11);
    CHECK(all.front() == SpanKey({1, 2}));
    CHECK(all.back() == SpanKey({11, 12}));

    // En helt tom rad hoppas över: spannet går förbi den.
    std::vector<PoleRow> gap = twelve();
    gap[5] = PoleRow{};
    gap[5].id = 6;
    const std::vector<SpanKey> skipped = afterSpans(gap);
    CHECK(skipped.size() == 10);
    CHECK(skipped[4] == SpanKey({5, 7}));

    // En rad med id 0 ger inget spann mot sina grannar.
    std::vector<PoleRow> noId = twelve();
    noId[5].id = 0;
    CHECK(afterSpans(noId).size() == 9);

    // Dubbletter och tomma Stolpnr ger spann som alla andra.
    std::vector<PoleRow> dup = twelve();
    dup[6].pole = "ST0379";
    dup[7].pole.clear();
    CHECK(afterSpans(dup).size() == 11);
}

// Det som står kvar när efter ändras.
void testKeptHeights()
{
    const StoredHeights stored = userStored();

    // Nya Stolpnr, dubbletter och tomma Stolpnr ändrar inget.
    std::vector<PoleRow> renamed = twelve();
    renamed[5].pole = "ST0379A";
    renamed[6].pole = "ST0379A";
    renamed[7].pole.clear();
    CHECK(keptHeights(stored, renamed) == stored);

    // Raden med id 6 borttagen eller tömd tar sina värden med sig.
    std::vector<PoleRow> removed = twelve();
    removed.erase(removed.begin() + 5);
    const StoredHeights keptRemoved = keptHeights(stored, removed);
    CHECK(keptRemoved.spans.empty());
    CHECK(keptRemoved.topAv.empty());

    std::vector<PoleRow> emptied = twelve();
    emptied[5] = PoleRow{};
    emptied[5].id = 6;
    CHECK(keptHeights(stored, emptied).spans.empty());
    CHECK(keptHeights(stored, emptied).topAv.empty());

    // En tom rad mellan id 5 och 6 låter {5, 6} stå kvar, men inte när den får ett Stolpnr.
    std::vector<PoleRow> inserted = twelve();
    PoleRow blank;
    blank.id = 13;
    inserted.insert(inserted.begin() + 5, blank);
    const StoredHeights keptBlank = keptHeights(stored, inserted);
    CHECK(keptBlank.spans.count(spanKey(5, 6)) == 1);
    CHECK(keptBlank.spans.count(spanKey(6, 7)) == 1);
    inserted[5].pole = "NY";
    const StoredHeights keptNamed = keptHeights(stored, inserted);
    CHECK(keptNamed.spans.count(spanKey(5, 6)) == 0);
    CHECK(keptNamed.spans.count(spanKey(6, 7)) == 1);

    // En tom stored ger en tom, och en rad som inte finns i stored ändrar inget.
    CHECK(keptHeights(StoredHeights{}, twelve()) == StoredHeights{});
    StoredHeights lonely;
    lonely.topAv[99] = "1";
    lonely.spans[spanKey(3, 9)] = {"1", "", ""};
    CHECK(keptHeights(lonely, twelve()) == StoredHeights{});
}

// Inmatning: komma eller punkt, inga negativa linhöjder men negativt Av.
void testAccept()
{
    CHECK(acceptHeight("12,33") == std::optional<std::string>("12.33"));
    CHECK(acceptHeight(" 7 ") == std::optional<std::string>("7"));
    CHECK(acceptHeight("") == std::optional<std::string>(""));
    CHECK(!acceptHeight("-1"));
    CHECK(!acceptHeight("abc"));
    CHECK(!acceptHeight("1e5"));
    CHECK(acceptAv("-3,5") == std::optional<std::string>("-3.5"));
    CHECK(acceptAv("265,9332") == std::optional<std::string>("265.9332"));
    CHECK(!acceptAv("x"));
}

// Den exakta halva summan och avrundningen.
void testHalfSumAndRounding()
{
    CHECK(halfSum("12.33", "21.2") == "16.765");
    CHECK(halfSum("22.63", "31.17") == "26.9");
    CHECK(halfSum("0.1", "0.2") == "0.15");
    CHECK(halfSum("100", "0.001") == "50.0005");
    CHECK(halfSum("0", "0") == "0");
    CHECK(halfSum("", "1").empty());
    CHECK(halfSum("1", "").empty());
    CHECK(halfSum("99999999999999999999", "99999999999999999999") == "99999999999999999999");
    CHECK(halfSum("1", "2") == "1.5");

    CHECK(roundedText("16.765", 2) == "16,77");
    CHECK(roundedText("16.775", 2) == "16,78");
    CHECK(roundedText("26.9", 2) == "26,90");
    CHECK(roundedText("9.995", 2) == "10,00");
    CHECK(roundedText("0.004", 2) == "0,00");
    CHECK(roundedText("12", 2) == "12,00");
    CHECK(roundedText("", 2).empty());
    CHECK(roundedText("16.765", 0) == "17");
    CHECK(roundedText("-3.5", 0) == "-4");
    CHECK(roundedText("-0.001", 2) == "0,00");
}

// Medelvärdena, linhöjden för linberäkning och Av för ytterfaserna.
void testMeans()
{
    const SpanRef a = spanOf(true, true, "12.33");
    const SpanRef b = spanOf(true, true, "21.2");
    const SpanRef none = spanOf(false, false, "");
    const SpanRef emptySpan = spanOf(true, true, "");
    const SpanRef locked = spanOf(true, false, "5");

    CHECK(spanMean(a, b, Part::Outer) == "16.765");
    CHECK(spanMean(a, emptySpan, Part::Outer).empty());
    CHECK(spanMean(emptySpan, b, Part::Outer).empty());
    CHECK(spanMean(none, b, Part::Outer) == "21.2");
    CHECK(spanMean(a, none, Part::Outer) == "12.33");
    CHECK(spanMean(none, emptySpan, Part::Outer).empty());
    CHECK(spanMean(none, none, Part::Outer).empty());
    // Ett låst spann räknas som tomt.
    CHECK(spanMean(a, locked, Part::Outer).empty());
    CHECK(spanMean(none, locked, Part::Outer).empty());
    CHECK(spanMean(a, a, Part::Middle).empty());

    CHECK(lineCalcHeight("16.765", "16.775") == "16.77");
    CHECK(lineCalcHeight("", "16.775").empty());
    CHECK(lineCalcHeight("16.765", "").empty());
}

// Det största av Av Fas1 och Fas3 som tal.
void testOuterAv()
{
    PoleRow r;
    r.av = "273.29";
    r.av3 = "270.1";
    CHECK(outerAvField(r) == pt::kAv);
    r.av = "300";
    r.av3 = "1000";
    CHECK(outerAvField(r) == pt::kAv3);
    r.av = "";
    r.av3 = "5";
    CHECK(outerAvField(r) == pt::kAv3);
    r.av = "5";
    r.av3 = "";
    CHECK(outerAvField(r) == pt::kAv);
    r.av = "7";
    r.av3 = "7.0";
    CHECK(outerAvField(r) == pt::kAv);
    r.av = "";
    r.av3 = "";
    CHECK(outerAvField(r) == -1);
}

// Blocken: användarens exempel och alla lägen i planen 13.1.
void testBuildBlocks()
{
    const std::vector<PoleRow> before = beforeRows();
    const StoredHeights stored = userStored();

    // Användarens exempel.
    {
        const std::vector<PoleBlock> blocks = buildBlocks(before, asChanged(), stored);
        CHECK(labelsOf(blocks) == std::vector<std::string>({"ST0378", "ST0379", "ST0380"}));
        CHECK(blocks.size() == 3);
        const PoleBlock& first = blocks[0];
        CHECK(first.back.label == "ST0377-ST0378");
        CHECK(first.back.key == SpanKey({4, 5}));
        CHECK(first.ahead.label == "ST0378-ST0379");
        CHECK(first.ahead.key == SpanKey({5, 6}));
        CHECK(first.outerMean.empty());
        const PoleBlock& b = blocks[1];
        CHECK(b.id == 6 && b.row == 5 && b.pole == "ST0379");
        CHECK(b.back.heights.outer == "12.33" && b.ahead.heights.outer == "21.2");
        CHECK(b.back.label == "ST0378-ST0379" && b.ahead.label == "ST0379-ST0380");
        CHECK(b.back.usable && b.ahead.usable);
        CHECK(b.outerMean == "16.765" && b.middleMean == "16.765");
        CHECK(b.lineCalc == "16.765");
        CHECK(b.topMean == "26.9");
        CHECK(roundedText(b.outerMean, kMeanDecimals) == "16,77");
        CHECK(roundedText(b.topMean, kMeanDecimals) == "26,90");
        CHECK(b.outerAv == "273,29" && b.av2 == "273,29");
        CHECK(b.topAv == "265.9332");
    }

    // Antalet block följer området med kBlockMargin.
    {
        const std::vector<PoleRow> after = asChanged();
        const kalkyl::poleview::MarkedRange area = kalkyl::poleview::changedArea(
            12, kalkyl::poleview::filterMarks(before, after), kBlockMargin);
        CHECK(static_cast<int>(buildBlocks(before, after, stored).size()) == area.last - area.first + 1);
    }

    // Ändring på rad 0: första blocket har ingen bakre granne.
    {
        std::vector<PoleRow> after = twelve();
        after[0].av = "1";
        StoredHeights s;
        s.spans[spanKey(1, 2)] = {"10", "", ""};
        const std::vector<PoleBlock> blocks = buildBlocks(before, after, s);
        CHECK(labelsOf(blocks) == std::vector<std::string>({"ST0374", "ST0375"}));
        CHECK(!blocks[0].back.exists && !blocks[0].back.usable);
        CHECK(blocks[0].back.issue == "ST0374 är den första stolpen i Stolptabell efter och har inget spann bakåt.");
        CHECK(blocks[0].outerMean == "10");
    }

    // Ändringar på rad 1 och 9: ett enda område med allt emellan.
    {
        std::vector<PoleRow> after = twelve();
        after[1].av = "1";
        after[9].av = "1";
        const std::vector<PoleBlock> blocks = buildBlocks(before, after, StoredHeights{});
        CHECK(blocks.size() == 11);
        CHECK(blocks.front().label == "ST0374" && blocks.back().label == "ST0384");
    }

    // Sista raden ändrad: sista blocket har ingen främre granne.
    {
        std::vector<PoleRow> after = twelve();
        after[11].av = "1";
        const std::vector<PoleBlock> blocks = buildBlocks(before, after, StoredHeights{});
        CHECK(!blocks.back().ahead.exists);
        CHECK(blocks.back().ahead.issue ==
              "ST0385 är den sista stolpen i Stolptabell efter och har inget spann framåt.");
    }

    // Nytt Stolpnr: blocket får det nya namnet och behåller värdena.
    {
        std::vector<PoleRow> after = asChanged();
        after[5].pole = "ST0379A";
        const std::vector<PoleBlock> blocks = buildBlocks(before, after, stored);
        const PoleBlock* b = blockWithId(blocks, 6);
        CHECK(b != nullptr);
        if (b) {
            CHECK(b->label == "ST0379A");
            CHECK(b->back.heights.outer == "12.33" && b->ahead.heights.outer == "21.2");
            CHECK(b->topAv == "265.9332");
        }
    }

    // Dubblettnamn: två block heter ST0379 och allt går att skriva i.
    {
        std::vector<PoleRow> after = asChanged();
        after[6].pole = "ST0379";
        const std::vector<PoleBlock> blocks = buildBlocks(before, after, stored);
        const PoleBlock* b6 = blockWithId(blocks, 6);
        const PoleBlock* b7 = blockWithId(blocks, 7);
        CHECK(b6 != nullptr && b7 != nullptr);
        if (b6 && b7) {
            CHECK(b6->label == "ST0379" && b7->label == "ST0379");
            CHECK(b6->back.usable && b6->ahead.usable && b7->back.usable && b7->ahead.usable);
            CHECK(b7->ahead.key == SpanKey({7, 8}));
            CHECK(b6->ahead.key == SpanKey({6, 7}) && b7->back.key == SpanKey({6, 7}));
            const Sheet sheet = buildSheet(blocks);
            InputRef top;
            top.kind = InputRef::Kind::TopAv;
            top.row = 6;
            CHECK(findInput(sheet, 6, top) >= 0);
            top.row = 7;
            CHECK(findInput(sheet, 7, top) >= 0);
        }
    }

    // En rad med data men utan Stolpnr heter "rad {n}".
    {
        std::vector<PoleRow> after = asChanged();
        after[5].pole.clear();
        const std::vector<PoleBlock> blocks = buildBlocks(before, after, stored);
        const PoleBlock* b = blockWithId(blocks, 6);
        const PoleBlock* next = blockWithId(blocks, 7);
        CHECK(b != nullptr && next != nullptr);
        if (b && next) {
            CHECK(b->label == "rad 6" && b->pole.empty());
            CHECK(b->back.usable && b->ahead.usable);
            CHECK(b->back.label == "ST0378-rad 6");
            CHECK(next->back.label == "rad 6-ST0380");
            CHECK(next->back.usable);
        }
    }

    // En helt tom rad infogad efter ST0378 får inget block, och spannet går förbi den.
    {
        std::vector<PoleRow> after = asChanged();
        PoleRow blank;
        blank.id = 13;
        after.insert(after.begin() + 5, blank);
        const std::vector<PoleBlock> blocks = buildBlocks(before, after, stored);
        CHECK(blockWithId(blocks, 13) == nullptr);
        const PoleBlock* b5 = blockWithId(blocks, 5);
        CHECK(b5 != nullptr);
        if (b5) {
            CHECK(b5->ahead.label == "ST0378-ST0379");
            CHECK(b5->ahead.key == SpanKey({5, 6}));
            CHECK(b5->ahead.heights.outer == "12.33");
        }
    }

    // En infogad rad med data: nya, tomma spann och {5, 6} används inte.
    {
        std::vector<PoleRow> after = asChanged();
        PoleRow fresh = row("NY1", 13);
        after.insert(after.begin() + 5, fresh);
        const std::vector<PoleBlock> blocks = buildBlocks(before, after, stored);
        const PoleBlock* b5 = blockWithId(blocks, 5);
        const PoleBlock* b6 = blockWithId(blocks, 6);
        CHECK(b5 != nullptr && b6 != nullptr);
        if (b5 && b6) {
            CHECK(b5->ahead.key == SpanKey({5, 13}) && b5->ahead.heights == SpanHeights{});
            CHECK(b6->back.key == SpanKey({6, 13}) && b6->back.heights == SpanHeights{});
            CHECK(b5->ahead.usable && b6->back.usable);
        }
    }

    // ST0379 borttagen: de fyra blocken runt platsen och ett nytt, tomt spann över luckan.
    {
        std::vector<PoleRow> after = twelve();
        after.erase(after.begin() + 5);
        const std::vector<PoleBlock> blocks = buildBlocks(before, after, stored);
        CHECK(labelsOf(blocks) == std::vector<std::string>({"ST0377", "ST0378", "ST0380", "ST0381"}));
        const PoleBlock* b5 = blockWithId(blocks, 5);
        CHECK(b5 != nullptr);
        if (b5) {
            CHECK(b5->ahead.label == "ST0378-ST0380");
            CHECK(b5->ahead.key == SpanKey({5, 7}));
            CHECK(b5->ahead.heights == SpanHeights{});
        }
    }

    // Flyttad rad: den tömda raden får inget block, och den nya raden sist har ett tomt spann.
    {
        std::vector<PoleRow> after = twelve();
        after[5] = PoleRow{};
        after[5].id = 6;
        PoleRow moved = row("ST0379", 13);
        moved.av = "273.29";
        after.push_back(moved);
        const std::vector<PoleBlock> blocks = buildBlocks(before, after, stored);
        CHECK(blockWithId(blocks, 6) == nullptr);
        const PoleBlock* b5 = blockWithId(blocks, 5);
        const PoleBlock* b13 = blockWithId(blocks, 13);
        CHECK(b5 != nullptr && b13 != nullptr);
        if (b5 && b13) {
            CHECK(b5->ahead.key == SpanKey({5, 7}));
            CHECK(b13->back.key == SpanKey({12, 13}) && b13->back.heights == SpanHeights{});
        }
    }

    // Rader med id 0: spannen och topplinans Av är låsta med texterna.
    {
        std::vector<PoleRow> after = asChanged();
        after[5].id = 0;
        const std::vector<PoleBlock> blocks = buildBlocks(before, after, stored);
        const PoleBlock* b = blockWithId(blocks, 0);
        CHECK(b != nullptr);
        if (b) {
            CHECK(b->back.exists && !b->back.usable);
            CHECK(b->back.issue ==
                  "Spannet ST0378-ST0379 hör till en rad som inte är sparad i projektet än, så linhöjden kan inte "
                  "skrivas.");
            const Sheet sheet = buildSheet({*b});
            const int cell = cellAt(sheet, 13, 7);
            CHECK(cell >= 0);
            if (cell >= 0) {
                CHECK(sheet.cells[static_cast<std::size_t>(cell)].style == CellStyle::Locked);
                CHECK(sheet.cells[static_cast<std::size_t>(cell)].toolTip ==
                      "ST0379 är inte sparad i projektet än, så topplinans Av kan inte skrivas.");
            }
        }
    }

    // Inga ändringar ger inga block, och en tom före ger block för alla rader.
    CHECK(buildBlocks(before, twelve(), stored).empty());
    CHECK(buildBlocks({}, twelve(), stored).size() == 12);
}

// Bladet för ett block: rad, kolumn, storlek, stil och text enligt planen 9.2.
void testBuildSheet()
{
    const std::vector<PoleRow> before = beforeRows();
    const std::vector<PoleBlock> blocks = buildBlocks(before, asChanged(), userStored());
    const Sheet sheet = buildSheet(blocks);
    CHECK(sheet.rows == 3 * kBlockStride - 1);
    CHECK(sheet.blockStart.size() == 3 && sheet.blockRows.size() == 3);
    CHECK(sheet.blockRows[1] == 6);
    CHECK(sheet.blockStart[0] == 0);
    CHECK(buildSheet({}).rows == 0);
    CHECK(buildSheet({blocks[0]}).rows == kBlockRows);
    CHECK(buildSheet({blocks[0], blocks[1]}).rows == 39);

    // Cell för cell i blocket ST0379 (block 1, rad 20).
    const int base = kBlockStride;
    auto at = [&](int r, int c) -> const SheetCell& {
        const int i = cellAt(sheet, base + r, c);
        static const SheetCell none;
        return i >= 0 ? sheet.cells[static_cast<std::size_t>(i)] : none;
    };
    CHECK(at(0, 0).text == "ST0379" && at(0, 0).style == CellStyle::PoleHeader && at(0, 0).columns == 9);
    CHECK(at(1, 0).text == "Linhöjd ST0379 ytterfaser" && at(1, 0).style == CellStyle::OuterHeader &&
          at(1, 0).columns == 2);
    CHECK(at(1, 3).text == "meter" && at(1, 3).style == CellStyle::OuterMeter);
    CHECK(at(5, 3).style == CellStyle::MiddleMeter && at(12, 3).style == CellStyle::TopMeter);
    CHECK(at(6, 6).style == CellStyle::MiddleAvLabel && at(13, 6).style == CellStyle::TopAvLabel);
    CHECK(at(1, 4).text == "medelvärdet av intilliggande spann" && at(1, 4).style == CellStyle::MeanHeader &&
          at(1, 4).columns == 2);
    CHECK(at(2, 0).text == "ST0378-ST0379" && at(2, 0).style == CellStyle::Label);
    CHECK(at(2, 3).text == "12,33" && at(2, 3).editText == "12,33" && at(2, 3).style == CellStyle::Input);
    CHECK(at(2, 3).input.kind == InputRef::Kind::Span && at(2, 3).input.part == Part::Outer &&
          at(2, 3).input.span == SpanKey({5, 6}));
    CHECK(at(2, 4).text == "16,77" && at(2, 4).style == CellStyle::Computed && at(2, 4).rows == 2);
    CHECK(cellAt(sheet, base + 3, 4) == cellAt(sheet, base + 2, 4));
    CHECK(at(2, 6).text == "Av" && at(2, 6).style == CellStyle::OuterAvLabel);
    CHECK(at(2, 7).text == "273,29" && at(2, 7).style == CellStyle::AvValue);
    CHECK(at(3, 0).text == "ST0379-ST0380");
    CHECK(at(3, 3).text == "21,2" && at(3, 3).input.span == SpanKey({6, 7}));
    CHECK(at(5, 0).text == "Linhöjd ST0379 mittfas" && at(5, 0).style == CellStyle::MiddleHeader);
    CHECK(at(6, 3).input.part == Part::Middle && at(6, 4).text == "16,77");
    CHECK(at(6, 7).text == "273,29");
    CHECK(at(7, 3).text == "21,2");
    CHECK(at(9, 0).text == "Linhöjd faslina för linberäkning" && at(9, 0).style == CellStyle::LineCalcHeader &&
          at(9, 0).columns == 4);
    CHECK(at(10, 0).text == "ST0379" && at(10, 1).text == "16,77" && at(10, 1).columns == 3 &&
          at(10, 1).style == CellStyle::Computed);
    CHECK(at(12, 0).text == "Linhöjd ST0379 Topplina" && at(12, 0).style == CellStyle::TopHeader);
    CHECK(at(13, 3).text == "22,63" && at(13, 3).input.part == Part::Top);
    CHECK(at(13, 4).text == "26,90");
    CHECK(at(13, 7).text == "265,9332" && at(13, 7).style == CellStyle::Input &&
          at(13, 7).input.kind == InputRef::Kind::TopAv && at(13, 7).input.row == 6);
    CHECK(at(14, 3).text == "31,17");
    CHECK(at(16, 0).text == "Linhöjd faslina för beräkning av sammanslagning" &&
          at(16, 0).style == CellStyle::MergeHeader);
    CHECK(at(17, 0).text == "ST0378-ST0379" && at(17, 1).text == "12,33" && at(17, 1).style == CellStyle::Linked);
    CHECK(at(18, 0).text == "ST0379-ST0380" && at(18, 1).text == "21,2");
    // Raderna 4, 8, 11 och 15 har inga celler: en tom rad mellan delarna.
    for (int r : {4, 8, 11, 15})
        CHECK(cellAt(sheet, base + r, 0) == -1);
    // Raden mellan blocken har inga celler.
    CHECK(cellAt(sheet, base - 1, 0) == -1);

    // Blockens bakgrund växlar mellan stolptabellernas två gula toner.
    CHECK(blockFill(0) == Rgb({246, 231, 201}));
    CHECK(blockFill(1) == Rgb({248, 236, 211}));
    CHECK(blockFill(2) == blockFill(0));

    // Läsordningen: ytter bak, ytter fram, mitt bak, mitt fram, topp bak, topplinans Av och topp fram.
    CHECK(sheet.inputs.size() == 21);
    const std::vector<std::pair<int, int>> order = {{2, 3}, {3, 3}, {6, 3}, {7, 3}, {13, 3}, {13, 7}, {14, 3}};
    for (std::size_t k = 0; k < order.size(); ++k) {
        const SheetCell& c = sheet.cells[static_cast<std::size_t>(sheet.inputs[7 + k])];
        CHECK(c.block == 1 && c.row == base + order[k].first && c.column == order[k].second);
    }

    // Den saknade bakre cellen är låst, med skälet som tooltip.
    std::vector<PoleRow> after = twelve();
    after[0].av = "1";
    const std::vector<PoleBlock> edge = buildBlocks(before, after, StoredHeights{});
    const Sheet edgeSheet = buildSheet(edge);
    const int locked = cellAt(edgeSheet, 2, 3);
    CHECK(locked >= 0);
    if (locked >= 0) {
        const SheetCell& c = edgeSheet.cells[static_cast<std::size_t>(locked)];
        CHECK(c.style == CellStyle::Locked && c.input.kind == InputRef::Kind::None && c.text.empty());
        CHECK(c.toolTip == "ST0374 är den första stolpen i Stolptabell efter och har inget spann bakåt.");
    }
    CHECK(edgeSheet.inputs.size() == 4 + 7);

    // Tooltips för medelvärdet och Av.
    CHECK(at(2, 4).toolTip == "Medelvärdet av intilliggande spann: (12,33 + 21,2) / 2 = 16,765.");
    CHECK(at(2, 7).toolTip == "Det största av Av Fas1 (273,29) och Av Fas3 (270,1) i Stolptabell efter.");
    CHECK(at(6, 7).toolTip == "Av Fas2 i Stolptabell efter.");
    CHECK(at(10, 1).toolTip ==
          "Medelvärdet av ytterfasernas och mittfasens medelvärden: (16,765 + 16,765) / 2 = 16,765.");
    CHECK(at(13, 7).toolTip == "Topplinans Av för ST0379, i meter. Skrivs här och sparas i projektet.");
    CHECK(at(2, 3).toolTip ==
          "Linhöjd för ytterfaserna i spannet ST0378-ST0379, i meter. Spannet visas också i blocket för ST0378 "
          "och sparas en gång.");
    CHECK(at(3, 3).toolTip ==
          "Linhöjd för ytterfaserna i spannet ST0379-ST0380, i meter. Spannet visas också i blocket för ST0380 "
          "och sparas en gång.");
    CHECK(at(17, 1).toolTip == "Mittfasens linhöjd i spannet ST0378-ST0379.");
}

// Förflyttning mellan inmatningscellerna och sökning av celler.
void testNavigation()
{
    const std::vector<PoleRow> before = beforeRows();
    const std::vector<PoleBlock> blocks = buildBlocks(before, asChanged(), userStored());
    const Sheet sheet = buildSheet(blocks);

    CHECK(cellAt(sheet, 2, 4) != -1 && cellAt(sheet, 2, 4) == cellAt(sheet, 3, 4));
    CHECK(cellAt(sheet, 0, 5) == cellAt(sheet, 0, 0) && cellAt(sheet, 0, 0) != -1);
    CHECK(cellAt(sheet, 4, 0) == -1);
    CHECK(cellAt(sheet, kBlockRows, 0) == -1);
    CHECK(cellAt(sheet, 5000, 0) == -1);
    CHECK(cellAt(sheet, -1, 0) == -1);

    const int topBack = cellAt(sheet, 13, 3);
    const int topAv = cellAt(sheet, 13, 7);
    const int topAhead = cellAt(sheet, 14, 3);
    CHECK(moveInput(sheet, topBack, Move::Next) == topAv);
    CHECK(moveInput(sheet, topAv, Move::Next) == topAhead);
    CHECK(moveInput(sheet, topAv, Move::Previous) == topBack);
    CHECK(moveInput(sheet, topBack, Move::Down) == topAhead);
    CHECK(moveInput(sheet, topAhead, Move::Down) == cellAt(sheet, kBlockStride + 2, 3));
    CHECK(moveInput(sheet, topAv, Move::Down) == cellAt(sheet, kBlockStride + 13, 7));
    CHECK(moveInput(sheet, cellAt(sheet, kBlockStride + 2, 3), Move::Up) == topAhead);
    CHECK(moveInput(sheet, topAhead, Move::Up) == topBack);
    // Next från sista topp fram i block 0 går till nästa blocks första inmatning.
    CHECK(moveInput(sheet, topAhead, Move::Next) == cellAt(sheet, kBlockStride + 2, 3));

    // Kanterna svarar med from.
    const int first = sheet.inputs.front();
    const int last = sheet.inputs.back();
    CHECK(moveInput(sheet, first, Move::Previous) == first);
    CHECK(moveInput(sheet, first, Move::Up) == first);
    CHECK(moveInput(sheet, last, Move::Next) == last);
    CHECK(moveInput(sheet, last, Move::Down) == last);

    // findInput skiljer block med samma Stolpnr på radens id.
    std::vector<PoleRow> after = asChanged();
    after[6].pole = "ST0379";
    const std::vector<PoleBlock> dupBlocks = buildBlocks(before, after, userStored());
    const Sheet dupSheet = buildSheet(dupBlocks);
    InputRef span;
    span.kind = InputRef::Kind::Span;
    span.span = spanKey(6, 7);
    span.part = Part::Outer;
    const int in6 = findInput(dupSheet, 6, span);
    const int in7 = findInput(dupSheet, 7, span);
    CHECK(in6 >= 0 && in7 >= 0 && in6 != in7);
    CHECK(dupSheet.blockRows[static_cast<std::size_t>(dupSheet.cells[static_cast<std::size_t>(in6)].block)] == 6);
    CHECK(dupSheet.blockRows[static_cast<std::size_t>(dupSheet.cells[static_cast<std::size_t>(in7)].block)] == 7);
    CHECK(findInput(dupSheet, 99, span) == -1);
}

// Färgerna och stilen för alla cellstilar.
void testLooks()
{
    auto look = [](CellStyle s) { return cellLook(s); };
    const CellLook pole = look(CellStyle::PoleHeader);
    CHECK(pole.filled && pole.fill == Rgb({0xBF, 0xBF, 0xBF}) && pole.bold && pole.frame == Frame::Thick &&
          pole.align == Align::Center && pole.text == Rgb({0, 0, 0}));
    CHECK(look(CellStyle::OuterHeader).fill == Rgb({0xFF, 0xE6, 0x99}));
    CHECK(look(CellStyle::MiddleHeader).fill == Rgb({0xF8, 0xCB, 0xAD}));
    CHECK(look(CellStyle::LineCalcHeader).fill == Rgb({0x00, 0xB0, 0xF0}));
    CHECK(look(CellStyle::MergeHeader).fill == Rgb({0x00, 0xB0, 0xF0}));
    CHECK(look(CellStyle::TopHeader).fill == Rgb({0x92, 0xD0, 0x50}));
    for (CellStyle s : {CellStyle::OuterHeader, CellStyle::MiddleHeader, CellStyle::LineCalcHeader,
                        CellStyle::TopHeader, CellStyle::MergeHeader}) {
        CHECK(look(s).bold && look(s).frame == Frame::Thick && look(s).filled);
    }
    // "meter" och "Av" har sin dels rubrikfärg.
    CHECK(look(CellStyle::OuterMeter).fill == Rgb({0xFF, 0xE6, 0x99}));
    CHECK(look(CellStyle::MiddleMeter).fill == Rgb({0xF8, 0xCB, 0xAD}));
    CHECK(look(CellStyle::TopMeter).fill == Rgb({0x92, 0xD0, 0x50}));
    for (CellStyle s : {CellStyle::OuterMeter, CellStyle::MiddleMeter, CellStyle::TopMeter})
        CHECK(look(s).filled && look(s).bold && look(s).frame == Frame::Thick && look(s).align == Align::Center);
    CHECK(look(CellStyle::OuterAvLabel).fill == Rgb({0xFF, 0xE6, 0x99}) && look(CellStyle::OuterAvLabel).bold &&
          look(CellStyle::OuterAvLabel).frame == Frame::Thin);
    CHECK(look(CellStyle::MiddleAvLabel).fill == Rgb({0xF8, 0xCB, 0xAD}));
    CHECK(look(CellStyle::TopAvLabel).fill == Rgb({0x92, 0xD0, 0x50}));
    const CellLook mean = look(CellStyle::MeanHeader);
    CHECK(!mean.filled && mean.bold && mean.frame == Frame::None && mean.align == Align::Left);
    const CellLook label = look(CellStyle::Label);
    CHECK(label.filled && label.fill == Rgb({0xC5, 0xE0, 0xB4}) && !label.bold && label.frame == Frame::Thin &&
          label.text == Rgb({0, 0, 0}));
    // Inmatningen är vit, länkade och beräknade fält gröna med sin glyf.
    const CellLook input = look(CellStyle::Input);
    CHECK(input.fill == Rgb({0xFF, 0xFF, 0xFF}) && input.text == Rgb({0x00, 0x61, 0x00}) && !input.bold &&
          input.mark == Mark::None);
    const CellLook locked = look(CellStyle::Locked);
    CHECK(locked.fill == Rgb({0xE7, 0xE6, 0xE6}) && locked.text == Rgb({0x80, 0x80, 0x80}));
    const CellLook computed = look(CellStyle::Computed);
    CHECK(computed.fill == Rgb({0xC5, 0xE0, 0xB4}) && computed.text == Rgb({0xFA, 0x7D, 0x00}) && computed.bold &&
          computed.frame == Frame::Thin && computed.mark == Mark::Sum);
    const CellLook linked = look(CellStyle::Linked);
    CHECK(linked.fill == computed.fill && linked.text == computed.text && linked.mark == Mark::Sync);
    CHECK(look(CellStyle::AvValue).fill == Rgb({0xC5, 0xE0, 0xB4}) && look(CellStyle::AvValue).text == Rgb({0, 0, 0}) &&
          look(CellStyle::AvValue).mark == Mark::Sync);
    CHECK(label.mark == Mark::None);
}

// Texten ovanför bladet i alla lägen.
void testAreaText()
{
    const std::vector<PoleRow> before = beforeRows();
    const StoredHeights stored = userStored();

    // Stolptabell efter saknar rader.
    CHECK(areaText(before, {}, {}) ==
          "Stolptabell efter är tom. Kopiera Stolptabell före dit och ändra de stolpar som byggs om, så visas "
          "deras linhöjder här.");

    // Ingen rad är märkt.
    CHECK(areaText(before, twelve(), buildBlocks(before, twelve(), stored)) ==
          "Inga stolpar i Stolptabell efter är ändrade mot Stolptabell före, så det finns inga linhöjder att "
          "visa.");

    // Märkta rader men inga block.
    const std::vector<PoleRow> blankAfter(1);
    CHECK(areaText(before, blankAfter, buildBlocks(before, blankAfter, stored)) ==
          "De ändrade raderna i Stolptabell efter är tomma, så det finns inga linhöjder att visa.");

    // Ett block: en enda rad utan Stolpnr.
    std::vector<PoleRow> single(1);
    single[0].av = "1";
    CHECK(areaText({}, single, buildBlocks({}, single, stored)) == "Visar 1 stolpe: rad 1.");

    // Flera block.
    const std::string margin = kBlockMargin == 1 ? "en rad" : (kBlockMargin == 2 ? "två rader" : "fler rader");
    const std::vector<PoleRow> after = asChanged();
    CHECK(areaText(before, after, buildBlocks(before, after, stored)) ==
          "Visar 3 stolpar: ST0378–ST0380, de ändrade raderna och " + margin + " före och efter dem.");
}

} // namespace

int main()
{
    testKeys();
    testAfterSpans();
    testKeptHeights();
    testAccept();
    testHalfSumAndRounding();
    testMeans();
    testOuterAv();
    testBuildBlocks();
    testBuildSheet();
    testNavigation();
    testLooks();
    testAreaText();
    if (g_failures != 0) {
        std::cerr << g_failures << " kontroller misslyckades" << std::endl;
        return 1;
    }
    std::cout << "lineheight: alla kontroller gick igenom" << std::endl;
    return 0;
}
