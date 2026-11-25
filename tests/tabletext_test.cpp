#include <cmath>
#include <iostream>
#include <string>
#include <vector>

#include "tableText/table_text.h"

using namespace kalkyl;

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

// Två tal är lika inom toleransen tol.
bool near(double a, double b, double tol)
{
    return std::fabs(a - b) <= tol;
}

// Tal, tabbseparerad text och inklistring.
void testTableText()
{
    using tabletext::formatDecimal;
    using tabletext::parseDecimal;

    CHECK(parseDecimal("332,247") && near(*parseDecimal("332,247"), 332.247, 1e-12));
    CHECK(parseDecimal("332.247") && near(*parseDecimal("332.247"), 332.247, 1e-12));
    CHECK(parseDecimal("1 234,5") && near(*parseDecimal("1 234,5"), 1234.5, 1e-12));
    CHECK(parseDecimal("1\xC2\xA0" "234,5") && near(*parseDecimal("1\xC2\xA0" "234,5"), 1234.5, 1e-12));
    CHECK(parseDecimal("1\xE2\x80\xAF" "234,5") && near(*parseDecimal("1\xE2\x80\xAF" "234,5"), 1234.5, 1e-12));
    CHECK(parseDecimal("\xE2\x88\x92" "3") && near(*parseDecimal("\xE2\x88\x92" "3"), -3, 1e-12));
    CHECK(parseDecimal("+4,5") && near(*parseDecimal("+4,5"), 4.5, 1e-12));
    CHECK(parseDecimal("1e3") && near(*parseDecimal("1e3"), 1000, 1e-9));
    CHECK(!parseDecimal("1.234,5"));
    CHECK(!parseDecimal("abc"));
    CHECK(!parseDecimal(""));
    CHECK(!parseDecimal("inf"));
    CHECK(!parseDecimal("1,2,3"));
    CHECK(!parseDecimal("12 abc"));

    CHECK(formatDecimal(332.247, -1, ',') == "332,247");
    CHECK(formatDecimal(343.1, 1) == "343,1");
    CHECK(formatDecimal(0.98514, 2) == "0,99");
    CHECK(formatDecimal(1, -1) == "1");
    CHECK(formatDecimal(-0.0, -1) == "0");
    CHECK(formatDecimal(89160.401, -1, '.') == "89160.401");

    const std::string crlf = {char{13}, char{10}};
    const std::string lf(1, char{10});
    const std::string tab(1, char{9});
    const std::string quote(1, char{34});

    const std::string excel = "a" + tab + "b" + tab + "c" + crlf + "1" + tab + "2" + tab + "3" + crlf +
                              "4" + tab + "5" + tab + "6" + crlf;
    const auto grid = tabletext::parseTsv(excel);
    CHECK(grid.size() == 3);
    bool shape = true;
    for (const auto& row : grid)
        shape = shape && row.size() == 3;
    CHECK(shape);
    CHECK(grid.size() == 3 && grid[1][2] == "3" && grid[2][0] == "4");
    const std::string back = "a" + tab + "b" + tab + "c" + lf + "1" + tab + "2" + tab + "3" + lf +
                             "4" + tab + "5" + tab + "6" + lf;
    CHECK(tabletext::formatTsv(grid) == back);

    const std::string quoted = quote + "x" + tab + "y" + quote + quote + "z" + lf + "w" + quote + tab + "q";
    const auto q = tabletext::parseTsv(quoted);
    CHECK(q.size() == 1 && q[0].size() == 2);
    CHECK(q.size() == 1 && q[0].size() == 2 && q[0][0] == "x" + tab + "y" + quote + "z" + lf + "w");
    CHECK(q.size() == 1 && q[0].size() == 2 && q[0][1] == "q");
    CHECK(tabletext::formatTsv({{"x" + tab + "y" + quote + "z"}}) == quote + "x" + tab + "y" + quote + quote + "z" + quote + lf);

    const auto column = tabletext::parseTsv("a" + lf + "b" + lf + "c");
    CHECK(column.size() == 3 && column[0].size() == 1 && column[2][0] == "c");
    const auto rowOnly = tabletext::parseTsv("a" + tab + "b" + tab + "c");
    CHECK(rowOnly.size() == 1 && rowOnly[0].size() == 3);
    const auto gaps = tabletext::parseTsv("a" + tab + tab + "c" + tab + lf + tab + "z");
    CHECK(gaps.size() == 2 && gaps[0].size() == 4 && gaps[0][1].empty() && gaps[0][2] == "c" && gaps[0][3].empty());
    CHECK(gaps.size() == 2 && gaps[1].size() == 2 && gaps[1][0].empty() && gaps[1][1] == "z");
    CHECK(tabletext::parseTsv("").empty());
    CHECK(tabletext::parseTsv(lf).size() == 1);

    const std::vector<std::vector<std::string>> block23 = {{"1", "2", "3"}, {"4", "5", "6"}};
    const auto p1 = tabletext::planPaste(5, 1, 3, block23);
    CHECK(p1.cells.size() == 4);
    CHECK(p1.rowsNeeded == 7);
    CHECK(p1.cells.size() == 4 && p1.cells[0].row == 5 && p1.cells[0].column == 1 && p1.cells[0].text == "1");
    CHECK(p1.cells.size() == 4 && p1.cells[3].row == 6 && p1.cells[3].column == 2 && p1.cells[3].text == "5");
    const auto p2 = tabletext::planPaste(0, 0, 3, {{"a", "b", "c", "d", "e"}});
    CHECK(p2.cells.size() == 3);
    CHECK(p2.rowsNeeded == 1);
    const auto p3 = tabletext::planPaste(0, 2, 3, {{"a"}, {"b"}, {"c"}});
    CHECK(p3.cells.size() == 3);
    bool onlyCol2 = true;
    for (const auto& c : p3.cells)
        onlyCol2 = onlyCol2 && c.column == 2;
    CHECK(onlyCol2);
    CHECK(p3.rowsNeeded == 3);
}

} // namespace

int main()
{
    testTableText();
    if (g_failures == 0)
        std::cout << "tabletext: alla kontroller lyckades" << std::endl;
    else
        std::cerr << "tabletext: " << g_failures << " kontroller misslyckades" << std::endl;
    return g_failures == 0 ? 0 : 1;
}
