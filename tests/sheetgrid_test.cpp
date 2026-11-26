#include <algorithm>
#include <iostream>
#include <set>
#include <string>
#include <vector>

#include "sheetGrid/sheet_grid.h"
#include "tableText/table_text.h"

using namespace kalkyl;
using sheetgrid::CellEdit;
using sheetgrid::CellPos;
using sheetgrid::CellRect;
using sheetgrid::SheetPaste;
using sheetgrid::SheetShape;

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
#define CHECK(cond) report(static_cast<bool>(cond), #cond, __FILE__, __LINE__)

// Radbrytningen och tabben som tecken, eftersom källan inte får innehålla escape-sekvenser.
const std::string kLf(1, char{10});
const std::string kTab(1, char{9});

// Kolumnerna som tar emot inklistring i lastfallstabellen. Kolumn 1 och 2 är text, resten tal.
bool isInputColumn(int column)
{
    static const std::set<int> columns{1, 2, 4, 5, 6, 8, 9, 10, 11, 13, 15, 16};
    return columns.count(column) != 0;
}

// En form som lastfallstabellen: 16 rader, 26 kolumner, ID som radrubrik och tomrad 7.
SheetShape caseShape()
{
    SheetShape shape;
    shape.rowCount = 16;
    shape.columnCount = 26;
    shape.firstColumn = 1;
    shape.skipRow = [](int row) { return row == 7; };
    shape.isInput = [](int, int column) { return isInputColumn(column); };
    shape.accept = [](int, int column, const std::string& field) -> std::optional<std::string> {
        if (column == 1 || column == 2)
            return field;
        const auto value = tabletext::parseDecimal(field);
        if (!value)
            return std::nullopt;
        return tabletext::formatDecimal(*value, -1, '.');
    };
    shape.columnName = [](int column) { return "K" + std::to_string(column); };
    return shape;
}

// Sant när det finns en ändring i cellen.
const CellEdit* findEdit(const SheetPaste& paste, int row, int column)
{
    for (const auto& e : paste.edits)
        if (e.row == row && e.column == column)
            return &e;
    return nullptr;
}

// Ett block som arkets G23:S38: 16 rader med 13 fält, där tomraden (rad 7) har text.
void testBlockLikeSheet()
{
    const SheetShape shape = caseShape();
    std::vector<std::vector<std::string>> block;
    for (int r = 0; r < 16; ++r) {
        std::vector<std::string> row;
        for (int c = 0; c < 13; ++c)
            row.push_back(r == 7 ? "9" : std::to_string(r * 100 + c) + ",5");
        block.push_back(std::move(row));
    }
    const SheetPaste paste = sheetgrid::planSheetPaste({0, 1}, block, shape);

    CHECK(!paste.rejected);
    CHECK(!paste.skippedHeader);
    for (const auto& e : paste.edits) {
        CHECK(isInputColumn(e.column));
        CHECK(e.row != 7);
        CHECK(e.column >= 1 && e.column <= 13);
    }
    CHECK(findEdit(paste, 15, 13) != nullptr);
    CHECK(findEdit(paste, 15, 13) && findEdit(paste, 15, 13)->text == "1512.5");
    CHECK(findEdit(paste, 0, 1) && findEdit(paste, 0, 1)->text == "0,5");
    CHECK(findEdit(paste, 6, 4) && findEdit(paste, 6, 4)->text == "603.5");
    CHECK(findEdit(paste, 8, 4) && findEdit(paste, 8, 4)->text == "803.5");
    CHECK(findEdit(paste, 0, 3) == nullptr);
    CHECK(findEdit(paste, 0, 7) == nullptr);
    CHECK(findEdit(paste, 0, 12) == nullptr);
    // 10 indatakolumner i 1..13 gånger 15 rader utom tomraden.
    CHECK(paste.edits.size() == 10u * 15u);
    CHECK(paste.area.row0 == 0 && paste.area.row1 == 15);
    CHECK(paste.area.column0 == 1 && paste.area.column1 == 13);
}

// En rubrikrad med text i en talkolumn hoppas över, och raderna efter den flyttas inte upp.
void testHeaderRow()
{
    const SheetShape shape = caseShape();
    const std::vector<std::vector<std::string>> block{{"Rubrik", "Temp", "Last"}, {"1", "2", "3"}};
    // Ankaret i kolumn 4: fälten landar i 4, 5 och 6, alla talkolumner.
    const SheetPaste paste = sheetgrid::planSheetPaste({2, 4}, block, shape);

    CHECK(!paste.rejected);
    CHECK(paste.skippedHeader);
    CHECK(paste.edits.size() == 3u);
    CHECK(findEdit(paste, 2, 4) == nullptr);
    CHECK(findEdit(paste, 3, 4) && findEdit(paste, 3, 4)->text == "1");
    CHECK(findEdit(paste, 3, 5) && findEdit(paste, 3, 5)->text == "2");
    CHECK(findEdit(paste, 3, 6) && findEdit(paste, 3, 6)->text == "3");
    CHECK(paste.area.row0 == 2 && paste.area.row1 == 3);
}

// Text i en talkolumn efter rubrikraden avbryter hela inklistringen utan ändringar.
void testRejected()
{
    const SheetShape shape = caseShape();
    const std::vector<std::vector<std::string>> block{{"1", "2"}, {"3", "4"}, {"5", "abc"}, {"7", "8"}};
    const SheetPaste paste = sheetgrid::planSheetPaste({0, 4}, block, shape);

    const std::string expected = std::string("Inklistringen avbröts: ") + char{34} + "abc" + char{34} +
                                 " på rad 3 i urklippet passar inte i kolumnen K5. Inget ändrades.";
    CHECK(paste.rejected && *paste.rejected == expected);
    CHECK(paste.edits.empty());

    // Med en rubrikrad först räknas den med i radnumret.
    const std::vector<std::vector<std::string>> withHeader{{"x", "y"}, {"3", "4"}, {"5", "abc"}};
    const SheetPaste second = sheetgrid::planSheetPaste({0, 4}, withHeader, shape);
    CHECK(second.rejected && second.rejected->find("på rad 3 i urklippet") != std::string::npos);
    CHECK(second.edits.empty());
}

// Fält utanför tabellen ignoreras, och området klampas till tabellen.
void testOutside()
{
    const SheetShape shape = caseShape();
    const std::vector<std::vector<std::string>> block{{"1", "2", "3"}, {"4", "5", "6"}, {"7", "8", "9"}};

    // Till vänster om firstColumn: kolumn 0 ignoreras, kolumn 1 och 2 tas emot.
    const SheetPaste left = sheetgrid::planSheetPaste({0, 0}, block, shape);
    CHECK(left.edits.size() == 6u);
    CHECK(findEdit(left, 0, 0) == nullptr);
    CHECK(findEdit(left, 0, 1) && findEdit(left, 0, 1)->text == "2");

    // Till höger om sista kolumnen: bara kolumn 25 finns, och den är inte indata.
    const SheetPaste right = sheetgrid::planSheetPaste({0, 24}, block, shape);
    CHECK(right.edits.empty());
    CHECK(right.area.column1 == 25);

    // Under sista raden: bara rad 15 finns.
    const SheetPaste below = sheetgrid::planSheetPaste({14, 4}, block, shape);
    CHECK(below.edits.size() == 6u);
    CHECK(below.area.row1 == 15);
    CHECK(findEdit(below, 14, 4) && findEdit(below, 15, 4));
    CHECK(findEdit(below, 16, 4) == nullptr);
    CHECK(below.rowsNeeded == 16);
    CHECK(left.rowsNeeded == 16);
}

// En form som stolptabellen: tre rader och tre kolumner där alla celler tar emot tal och
// inklistringen får växa förbi sista raden.
SheetShape growShape()
{
    SheetShape shape;
    shape.rowCount = 3;
    shape.columnCount = 3;
    shape.isInput = [](int, int) { return true; };
    shape.accept = [](int, int, const std::string& field) -> std::optional<std::string> {
        const auto value = tabletext::parseDecimal(field);
        if (!value)
            return std::nullopt;
        return tabletext::formatDecimal(*value, -1, '.');
    };
    shape.columnName = [](int column) { return "K" + std::to_string(column); };
    shape.growRows = true;
    shape.headerTakesRow = false;
    return shape;
}

// Med growRows blir fält under tabellen ändringar i nya rader, och radantalet som behövs räknas ut.
void testGrowRows()
{
    const SheetShape shape = growShape();
    const std::vector<std::vector<std::string>> block{{"1"}, {"2"}, {"3"}, {"4"}, {"5"}};
    const SheetPaste paste = sheetgrid::planSheetPaste({2, 0}, block, shape);

    CHECK(!paste.rejected);
    CHECK(paste.edits.size() == 5u);
    for (int r = 2; r <= 6; ++r)
        CHECK(findEdit(paste, r, 0) && findEdit(paste, r, 0)->text == std::to_string(r - 1));
    CHECK(paste.rowsNeeded == 7);
    CHECK(paste.area.row0 == 2 && paste.area.row1 == 6);

    // Ett block som ryms ändrar inte radantalet.
    const SheetPaste fits = sheetgrid::planSheetPaste({0, 0}, {{"1"}, {"2"}}, shape);
    CHECK(fits.rowsNeeded == 3);

    // Ett ogiltigt tal under tabellen avbryter fortfarande allt.
    const std::vector<std::vector<std::string>> bad{{"1"}, {"2"}, {"3"}, {"4"}, {"abc"}};
    const SheetPaste rejected = sheetgrid::planSheetPaste({2, 0}, bad, shape);
    CHECK(rejected.rejected && rejected.rejected->find("på rad 5 i urklippet") != std::string::npos);
    CHECK(rejected.edits.empty());

    // Utan growRows ignoreras fälten under tabellen och radantalet är oförändrat.
    SheetShape fixed = shape;
    fixed.growRows = false;
    const SheetPaste cut = sheetgrid::planSheetPaste({2, 0}, block, fixed);
    CHECK(cut.edits.size() == 1u);
    CHECK(cut.rowsNeeded == 3);
    CHECK(cut.area.row1 == 2);
}

// Utan rubrikrad som tar en tabellrad börjar datat på ankarraden, och området stämmer.
void testHeaderNoRow()
{
    SheetShape shape = growShape();
    const std::vector<std::vector<std::string>> block{{"Rubrik", "Temp"}, {"1", "2"}, {"3", "4"}};

    const SheetPaste paste = sheetgrid::planSheetPaste({1, 0}, block, shape);
    CHECK(!paste.rejected);
    CHECK(paste.skippedHeader);
    CHECK(paste.edits.size() == 4u);
    CHECK(findEdit(paste, 1, 0) && findEdit(paste, 1, 0)->text == "1");
    CHECK(findEdit(paste, 1, 1) && findEdit(paste, 1, 1)->text == "2");
    CHECK(findEdit(paste, 2, 0) && findEdit(paste, 2, 0)->text == "3");
    CHECK(findEdit(paste, 2, 1) && findEdit(paste, 2, 1)->text == "4");
    CHECK(paste.area.row0 == 1 && paste.area.row1 == 2);
    CHECK(paste.rowsNeeded == 3);

    // Datat växer förbi sista raden från ankarraden.
    const SheetPaste tail = sheetgrid::planSheetPaste({2, 0}, block, shape);
    CHECK(findEdit(tail, 2, 0) && findEdit(tail, 3, 1));
    CHECK(tail.area.row1 == 3 && tail.rowsNeeded == 4);

    // En rubrikrad med ett ogiltigt tal längre ned räknas med i radnumret.
    const std::vector<std::vector<std::string>> bad{{"Rubrik", "Temp"}, {"1", "x"}};
    const SheetPaste rejected = sheetgrid::planSheetPaste({0, 0}, bad, shape);
    CHECK(rejected.rejected && rejected.rejected->find("på rad 2 i urklippet") != std::string::npos);

    // Med headerTakesRow tar rubrikraden en tabellrad, som i arket Linberäkning.
    shape.headerTakesRow = true;
    const SheetPaste taken = sheetgrid::planSheetPaste({0, 0}, block, shape);
    CHECK(taken.skippedHeader);
    CHECK(findEdit(taken, 0, 0) == nullptr);
    CHECK(findEdit(taken, 1, 0) && findEdit(taken, 1, 0)->text == "1");
    CHECK(findEdit(taken, 2, 1) && findEdit(taken, 2, 1)->text == "4");
    CHECK(taken.area.row0 == 0 && taken.area.row1 == 2);
}

// Ett enda värde ger bara en ändring, i ankarcellen, så en markering med flera celler fylls inte.
void testSingleValue()
{
    const SheetShape shape = growShape();
    const SheetPaste paste = sheetgrid::planSheetPaste({1, 1}, {{"7"}}, shape);
    CHECK(paste.edits.size() == 1u);
    CHECK(findEdit(paste, 1, 1) && findEdit(paste, 1, 1)->text == "7");
    CHECK(paste.area.row0 == 1 && paste.area.row1 == 1 && paste.area.column0 == 1 && paste.area.column1 == 1);
}

// Ett tomt fält blir en ändring med tom text, som i Excel.
void testEmptyField()
{
    const SheetShape shape = caseShape();
    const std::vector<std::vector<std::string>> block{{"1", "", "3"}};
    const SheetPaste paste = sheetgrid::planSheetPaste({0, 4}, block, shape);

    CHECK(!paste.rejected);
    CHECK(paste.edits.size() == 3u);
    CHECK(findEdit(paste, 0, 5) && findEdit(paste, 0, 5)->text.empty());
}

// Rektangeln, klampningen kring tomraden och urklippets form.
void testHelpers()
{
    const SheetShape shape = caseShape();

    const CellRect rect = sheetgrid::rectOf({5, 9}, {2, 3});
    CHECK(rect.row0 == 2 && rect.column0 == 3 && rect.row1 == 5 && rect.column1 == 9);

    CHECK(sheetgrid::clampPos({7, 4}, shape, 1).row == 8);
    CHECK(sheetgrid::clampPos({7, 4}, shape, -1).row == 6);
    CHECK(sheetgrid::clampPos({-3, 4}, shape, 1).row == 0);
    CHECK(sheetgrid::clampPos({40, 4}, shape, 1).row == 15);
    CHECK(sheetgrid::clampPos({3, 99}, shape, 1).column == 25);
    CHECK(sheetgrid::clampPos({3, 0}, shape, 1).column == 1);

    // Tomraden som sista rad flyttas åt andra hållet.
    SheetShape tail = shape;
    tail.skipRow = [](int row) { return row == 15; };
    CHECK(sheetgrid::clampPos({15, 4}, tail, 1).row == 14);

    CHECK(!sheetgrid::isBlock("5" + kLf));
    CHECK(!sheetgrid::isBlock("5"));
    CHECK(!sheetgrid::isBlock(""));
    CHECK(sheetgrid::isBlock("a" + kTab + "b"));
    CHECK(sheetgrid::isBlock("a" + kLf + "b" + kLf));
}

} // namespace

int main()
{
    testBlockLikeSheet();
    testHeaderRow();
    testRejected();
    testOutside();
    testEmptyField();
    testHelpers();
    testGrowRows();
    testHeaderNoRow();
    testSingleValue();
    if (g_failures == 0)
        std::cout << "sheetgrid: alla tester gick igenom" << std::endl;
    return g_failures == 0 ? 0 : 1;
}
