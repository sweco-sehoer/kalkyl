#include <cmath>
#include <cstdint>
#include <iostream>
#include <string>
#include <vector>

#include "poleTable/pole_table.h"
#include "tableText/table_text.h"

using namespace kalkyl;
using namespace kalkyl::poletable;

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

// Hårt mellanslag (U+00A0) byggt med char-värden.
std::string nbsp()
{
    return {static_cast<char>(0xC2), static_cast<char>(0xA0)};
}

// En rad med alla åtta fält ifyllda.
PoleRow fullRow()
{
    return {"ST0378", "363", "362", "348", "6512240.85", "412181.98", "44.19", "255/262"};
}

// En rad med bara Stolpnr ifylld.
PoleRow poleOnly(const std::string& name)
{
    PoleRow r;
    r.pole = name;
    return r;
}

// Textfält trimmas, tal normaliseras och ogiltiga fält ger tomt svar.
void testAcceptField()
{
    CHECK(acceptField(kPole, "  ST0378 \t") == std::optional<std::string>("ST0378"));
    CHECK(acceptField(kLkl, "255/262") == std::optional<std::string>("255/262"));
    CHECK(acceptField(kPole, "   ") == std::optional<std::string>(""));
    CHECK(acceptField(kNorth, "6512240,8500000000") == std::optional<std::string>("6512240.8500000000"));
    CHECK(acceptField(kAv, " 44,10 ") == std::optional<std::string>("44.10"));
    CHECK(acceptField(kHeight, "") == std::optional<std::string>(""));
    CHECK(acceptField(kEast, "6 512 240,85") == std::optional<std::string>("6512240.85"));
    CHECK(!acceptField(kNorth, "abc"));
    CHECK(!acceptField(kNorth, "6,51E+06"));
    CHECK(!acceptField(kAh, "1.234,5"));
    CHECK(!acceptField(8, "x"));
    CHECK(!acceptField(-1, "x"));

    // LKL blir alltid "227/262", också med radbrytningar och tomma rader från Excel.
    const std::string lf(1, char{10});
    const std::string crlf = std::string(1, char{13}) + lf;
    CHECK(acceptField(kLkl, "227" + lf + lf + "262") == std::optional<std::string>("227/262"));
    CHECK(acceptField(kLkl, "227" + lf + "262") == std::optional<std::string>("227/262"));
    CHECK(acceptField(kLkl, "227" + crlf + crlf + "262") == std::optional<std::string>("227/262"));
    CHECK(acceptField(kLkl, " 227 / 262 ") == std::optional<std::string>("227/262"));
    CHECK(acceptField(kLkl, "227" + lf) == std::optional<std::string>("227"));
    CHECK(acceptField(kLkl, lf + " ") == std::optional<std::string>(""));
    CHECK(normalizeLkl("227//262") == "227/262");
}

// Visning med decimalkomma, urklipp utan gruppering och talvärdet för beräkningar.
void testDisplayAndNumber()
{
    const PoleRow row = fullRow();
    CHECK(displayText(row, kPole) == "ST0378");
    CHECK(displayText(row, kLkl) == "255/262");
    CHECK(displayText(row, kAv) == "363");
    CHECK(displayText(row, kHeight) == "44,19");
    // Heltalsdelen är grupperad eller inte beroende på kGroupThousands.
    const std::string north = displayText(row, kNorth);
    CHECK(north == "6512240,85" || north == "6" + nbsp() + "512" + nbsp() + "240,85");
    CHECK(north == tabletext::formatDecimalText("6512240.85", ',', true) ||
          north == tabletext::formatDecimalText("6512240.85", ',', false));

    CHECK(copyText(row, kNorth) == "6512240,85");
    CHECK(copyText(row, kEast) == "412181,98");
    CHECK(copyText(row, kPole) == "ST0378");

    PoleRow decimals;
    decimals.north = "6512240.8500000000";
    decimals.height = "42.00";
    CHECK(copyText(decimals, kNorth) == "6512240,8500000000");
    CHECK(displayText(decimals, kHeight) == "42,00");
    CHECK(displayText(decimals, kAv).empty());

    const auto n = numberValue(row, kNorth);
    CHECK(n && std::fabs(*n - 6512240.85) < 1e-6);
    const auto h = numberValue(row, kHeight);
    CHECK(h && std::fabs(*h - 44.19) < 1e-12);
    CHECK(!numberValue(PoleRow{}, kNorth));
    CHECK(!numberValue(row, kPole));
    CHECK(!numberValue(row, kLkl));
    CHECK(!numberValue(row, 8));
}

// field når alla åtta fält i rätt ordning och columnName ger alla åtta namnen.
void testFieldAndNames()
{
    // Ordningen är den i de stolptabeller som klistras in: Stolpnr, N, E, H, Avstånd
    // föregående, Ah, Av och LKL.
    PoleRow row = fullRow();
    CHECK(field(row, 0) == "ST0378");
    CHECK(field(row, 1) == "6512240.85");
    CHECK(field(row, 2) == "412181.98");
    CHECK(field(row, 3) == "44.19");
    CHECK(field(row, 4) == "348");
    CHECK(field(row, 5) == "362");
    CHECK(field(row, 6) == "363");
    CHECK(field(row, 7) == "255/262");
    const PoleRow& constRow = row;
    CHECK(field(constRow, 8).empty());
    CHECK(field(constRow, -1).empty());

    field(row, kSpan) = "375";
    CHECK(row.spanM == "375");
    field(row, kLkl) = "262";
    CHECK(row.lkl == "262");

    CHECK(columnName(0) == "Stolpnr");
    CHECK(columnName(1) == "Koordinat N");
    CHECK(columnName(2) == "Koordinat E");
    CHECK(columnName(3) == "Koordinat H");
    CHECK(columnName(4) == "Avstånd föregående");
    CHECK(columnName(5) == "Ah");
    CHECK(columnName(6) == "Av");
    CHECK(columnName(7) == "LKL");
    CHECK(columnName(kAv2) == "Av Fas2");
    CHECK(columnName(kAv3) == "Av Fas3");
    CHECK(afterColumnName(kAv) == "Av Fas1" && afterColumnName(kAv2) == "Av Fas2" && afterColumnName(kLkl) == "LKL");
    CHECK(columnName(kFieldCount).empty());
    CHECK(columnName(-1).empty());

    CHECK(!isNumberColumn(0));
    CHECK(isNumberColumn(1) && isNumberColumn(6));
    CHECK(!isNumberColumn(7));
    CHECK(isNumberColumn(kAv2) && isNumberColumn(kAv3));
    CHECK(!isNumberColumn(kFieldCount));
    CHECK(kColumnCount == 8);
    CHECK(kFieldCount == 10);
}

// Av per fas i Stolptabell efter: fälten, motsvarigheten i före och kolumnordningarna.
void testPhaseFields()
{
    PoleRow row;
    field(row, kAv2) = "300";
    field(row, kAv3) = "301";
    CHECK(row.av2 == "300" && row.av3 == "301" && row.av.empty());
    CHECK(!isEmptyRow(row));
    CHECK(acceptField(kAv2, "1,50") == std::optional<std::string>("1.50"));
    CHECK(!acceptField(kAv3, "abc"));
    row.av3 = "1,5";
    CHECK(firstInvalidColumn(row) == std::optional<int>(kAv3));
    CHECK(displayText(PoleRow{"", "", "", "", "", "", "", "", "1234.5", ""}, kAv2) != "");

    CHECK(beforeField(kAv2) == kAv && beforeField(kAv3) == kAv && beforeField(kAv) == kAv);
    CHECK(beforeField(kLkl) == kLkl && beforeField(kPole) == kPole);

    CHECK(beforeColumns() == std::vector<int>({kPole, kNorth, kEast, kHeight, kSpan, kAh, kAv, kLkl}));
    CHECK(afterColumns() == std::vector<int>({kPole, kNorth, kEast, kHeight, kSpan, kAh, kAv, kAv2, kAv3, kLkl}));

    std::vector<PoleRow> rows(1);
    applyEdits(rows, {{0, kAv2, "5"}, {0, kAv3, "6"}});
    CHECK(rows[0].av2 == "5" && rows[0].av3 == "6");
}

// applyEdits skapar tomma rader fram till en rad bortom slutet och ignorerar ogiltiga ändringar.
void testApplyEdits()
{
    std::vector<PoleRow> rows = {poleOnly("A"), poleOnly("B")};
    applyEdits(rows, {{5, kNorth, "6512240.85"}});
    CHECK(rows.size() == 6);
    CHECK(rows.size() == 6 && rows[5].north == "6512240.85");
    CHECK(rows.size() == 6 && isEmptyRow(rows[2]) && isEmptyRow(rows[3]) && isEmptyRow(rows[4]));
    CHECK(rows.size() == 6 && rows[0].pole == "A" && rows[1].pole == "B");

    const std::vector<PoleRow> before = rows;
    applyEdits(rows, {{-1, kPole, "x"}, {0, kFieldCount, "x"}, {0, -1, "x"}, {9, kFieldCount, "x"}});
    CHECK(rows == before);

    applyEdits(rows, {{0, kPole, "C"}, {0, kLkl, "262"}});
    CHECK(rows[0].pole == "C" && rows[0].lkl == "262");

    std::vector<PoleRow> none;
    applyEdits(none, {{0, kPole, "A"}});
    CHECK(none.size() == 1 && none[0].pole == "A");
}

// Tomma rader mitt i behålls, tomma rader sist tas bort.
void testTrimAndSave()
{
    CHECK(isEmptyRow(PoleRow{}));
    CHECK(!isEmptyRow(poleOnly("A")));
    PoleRow onlyLkl;
    onlyLkl.lkl = "262";
    CHECK(!isEmptyRow(onlyLkl));

    const std::vector<PoleRow> rows = {poleOnly("A"), PoleRow{}, poleOnly("B"), PoleRow{}, PoleRow{}};
    const auto saved = rowsToSave(rows);
    CHECK(saved.size() == 3);
    CHECK(saved.size() == 3 && saved[0].pole == "A" && isEmptyRow(saved[1]) && saved[2].pole == "B");
    CHECK(rows.size() == 5);

    std::vector<PoleRow> trimmed = rows;
    trimTrailingEmptyRows(trimmed);
    CHECK(trimmed == saved);

    std::vector<PoleRow> allEmpty = {PoleRow{}, PoleRow{}};
    trimTrailingEmptyRows(allEmpty);
    CHECK(allEmpty.empty());
    CHECK(rowsToSave({}).empty());
}

// firstInvalidColumn pekar ut första talkolumnen som inte är kanonisk decimaltext.
void testFirstInvalidColumn()
{
    PoleRow row = fullRow();
    CHECK(!firstInvalidColumn(row));
    CHECK(!firstInvalidColumn(PoleRow{}));
    row.north = "6512240,85";
    CHECK(firstInvalidColumn(row) == std::optional<int>(kNorth));
    row.north = "6512240.85";
    row.ah = "abc";
    CHECK(firstInvalidColumn(row) == std::optional<int>(kAh));
    row.ah = "362";
    row.height = "1e5";
    CHECK(firstInvalidColumn(row) == std::optional<int>(kHeight));
    row.height = "007.50";
    CHECK(firstInvalidColumn(row) == std::optional<int>(kHeight));
    // Text i textkolumnerna kontrolleras inte.
    row.height = "44.19";
    row.pole = "abc,1.2";
    CHECK(!firstInvalidColumn(row));
}

// Radens id: assignRowIds ger nästa lediga id i tabellens ordning, id:t är inget fält och syns
// därför aldrig i urklipp, visning eller tömningen av raden, och det följer raden genom ändringar.
void testRowIds()
{
    const auto withId = [](std::int64_t id) {
        PoleRow r;
        r.id = id;
        return r;
    };
    std::vector<PoleRow> rows = {withId(0), withId(3), withId(0), withId(3), withId(-1)};
    assignRowIds(rows);
    CHECK(rows[0].id == 4 && rows[1].id == 3 && rows[2].id == 5 && rows[3].id == 6 && rows[4].id == 7);

    std::vector<PoleRow> valid = {withId(2), withId(7), withId(4)};
    const std::vector<PoleRow> sameValid = valid;
    assignRowIds(valid);
    CHECK(valid == sameValid);
    std::vector<PoleRow> none;
    assignRowIds(none);
    CHECK(none.empty());

    // Ett id är inget fält: en rad med bara id är tom, och fälten 0-9 visar det aldrig.
    PoleRow idOnly = withId(12);
    CHECK(isEmptyRow(idOnly));
    for (int c = 0; c < kFieldCount; ++c) {
        CHECK(field(idOnly, c).empty());
        CHECK(displayText(idOnly, c).empty() && copyText(idOnly, c).empty());
    }
    CHECK(!acceptField(kFieldCount, "12"));
    CHECK(!firstInvalidColumn(idOnly));

    // id står med i jämförelsen, så två rader med samma fält men olika id är olika.
    PoleRow a = poleOnly("A");
    PoleRow b = poleOnly("A");
    b.id = 3;
    CHECK(!(a == b));

    // applyEdits behåller id:t i en befintlig rad och ger nya rader id 0.
    std::vector<PoleRow> edited = {poleOnly("A")};
    edited[0].id = 7;
    applyEdits(edited, {{0, kNorth, "1.5"}, {2, kPole, "C"}});
    CHECK(edited.size() == 3 && edited[0].id == 7 && edited[0].north == "1.5");
    CHECK(edited[1].id == 0 && edited[2].id == 0);

    // trimTrailingEmptyRows och rowsToSave behåller id:na på raderna som står kvar.
    std::vector<PoleRow> trailing = {poleOnly("A"), withId(9), poleOnly("B"), withId(10)};
    trailing[0].id = 1;
    trailing[2].id = 3;
    const std::vector<PoleRow> saved = rowsToSave(trailing);
    CHECK(saved.size() == 3 && saved[0].id == 1 && saved[1].id == 9 && saved[2].id == 3);
    trimTrailingEmptyRows(trailing);
    CHECK(trailing == saved);
}

} // namespace

int main()
{
    testRowIds();
    testAcceptField();
    testDisplayAndNumber();
    testFieldAndNames();
    testPhaseFields();
    testApplyEdits();
    testTrimAndSave();
    testFirstInvalidColumn();
    if (g_failures == 0)
        std::cout << "poletable: alla kontroller lyckades" << std::endl;
    else
        std::cerr << "poletable: " << g_failures << " kontroller misslyckades" << std::endl;
    return g_failures == 0 ? 0 : 1;
}
