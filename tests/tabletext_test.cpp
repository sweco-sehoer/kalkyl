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

// Kanonisk decimaltext: normalizeDecimal och formatDecimalText, och trimningen av textfält.
void testCanonicalDecimal()
{
    using tabletext::formatDecimalText;
    using tabletext::trimmed;
    using tabletext::normalizeDecimal;
    using Text = std::optional<std::string>;

    // Specialtecken byggda med char-värden.
    const std::string nbsp = {static_cast<char>(0xC2), static_cast<char>(0xA0)};
    const std::string narrow = {static_cast<char>(0xE2), static_cast<char>(0x80), static_cast<char>(0xAF)};
    const std::string minus = {static_cast<char>(0xE2), static_cast<char>(0x88), static_cast<char>(0x92)};

    CHECK(normalizeDecimal("6512240,8500000000") == Text("6512240.8500000000"));
    CHECK(normalizeDecimal("6512240.85") == Text("6512240.85"));
    CHECK(normalizeDecimal("6 512 240,85") == Text("6512240.85"));
    CHECK(normalizeDecimal("6" + nbsp + "512" + nbsp + "240,85") == Text("6512240.85"));
    CHECK(normalizeDecimal("6" + narrow + "512" + narrow + "240,85") == Text("6512240.85"));
    CHECK(normalizeDecimal("44") == Text("44"));
    CHECK(normalizeDecimal("44,10") == Text("44.10"));
    CHECK(normalizeDecimal("44.10") == Text("44.10"));
    CHECK(normalizeDecimal("-12,5") == Text("-12.5"));
    CHECK(normalizeDecimal(minus + "12,5") == Text("-12.5"));
    CHECK(normalizeDecimal("+3") == Text("3"));
    CHECK(normalizeDecimal("-0,00") == Text("0.00"));
    CHECK(normalizeDecimal("-0") == Text("0"));
    CHECK(normalizeDecimal(",5") == Text("0.5"));
    CHECK(normalizeDecimal("5,") == Text("5"));
    CHECK(normalizeDecimal("007,50") == Text("7.50"));
    CHECK(normalizeDecimal("000") == Text("0"));
    CHECK(normalizeDecimal("0,0000000001") == Text("0.0000000001"));
    CHECK(normalizeDecimal("") == Text(""));
    CHECK(normalizeDecimal("   ") == Text(""));
    CHECK(normalizeDecimal(nbsp + narrow) == Text(""));
    CHECK(normalizeDecimal(" 12 " + std::string(1, char{9}) + " ") == Text("12"));
    // Tabb eller radbrytning mitt i (en Excelcell med två rader) är aldrig ett tal; i ändarna trimmas de.
    const std::string lfIn(1, char{10});
    CHECK(!normalizeDecimal("256" + lfIn + "256"));
    CHECK(!normalizeDecimal("256" + lfIn + lfIn + "256"));
    CHECK(!normalizeDecimal("12" + std::string(1, char{9}) + "5"));
    CHECK(!normalizeDecimal("1" + std::string{char{13}, char{10}} + "2"));
    CHECK(normalizeDecimal(lfIn + "256" + lfIn) == Text("256"));
    CHECK(!tabletext::parseDecimal("256" + lfIn + "256"));
    CHECK(tabletext::parseDecimal(lfIn + "2,5" + lfIn) == std::optional<double>(2.5));
    CHECK(!normalizeDecimal("abc"));
    CHECK(!normalizeDecimal("1,2,3"));
    CHECK(!normalizeDecimal("1.234,5"));
    CHECK(!normalizeDecimal("1,234.5"));
    CHECK(!normalizeDecimal("1e5"));
    CHECK(!normalizeDecimal("6,51E+06"));
    CHECK(!normalizeDecimal("-"));
    CHECK(!normalizeDecimal(","));
    CHECK(!normalizeDecimal("+-3"));
    CHECK(!normalizeDecimal("--3"));
    CHECK(!normalizeDecimal("3-"));
    CHECK(!normalizeDecimal("12 abc"));

    CHECK(formatDecimalText("6512240.8500000000") == "6512240,8500000000");
    CHECK(formatDecimalText("6512240.8500000000", '.') == "6512240.8500000000");
    CHECK(formatDecimalText("44") == "44");
    CHECK(formatDecimalText("-12.5") == "-12,5");
    CHECK(formatDecimalText("") == "");
    CHECK(formatDecimalText("6512240.8500000000", ',', true) == "6" + nbsp + "512" + nbsp + "240,8500000000");
    CHECK(formatDecimalText("-1234.5", ',', true) == "-1" + nbsp + "234,5");
    CHECK(formatDecimalText("999", ',', true) == "999");
    CHECK(formatDecimalText("0.123456", ',', true) == "0,123456");
    CHECK(formatDecimalText("-999.5", ',', true) == "-999,5");
    CHECK(formatDecimalText("1000", ',', true) == "1" + nbsp + "000");
    CHECK(formatDecimalText("123456", ',', true) == "123" + nbsp + "456");

    CHECK(trimmed("  ST 0381 " + std::string(1, char{9})) == "ST 0381");
    CHECK(trimmed(" ") == "");
    CHECK(trimmed("") == "");

    // Det kanoniska värdet läses som samma tal som originalet.
    const std::vector<std::string> valid = {"6512240,85", "6 512 240,85", "-12,5", "+3", "44.10", ",5", "007,50"};
    for (const std::string& x : valid) {
        const auto canonical = normalizeDecimal(x);
        const auto a = tabletext::parseDecimal(x);
        const auto b = canonical ? tabletext::parseDecimal(*canonical) : std::nullopt;
        CHECK(canonical && a && b && *a == *b);
    }
}

} // namespace

int main()
{
    testTableText();
    testCanonicalDecimal();
    if (g_failures == 0)
        std::cout << "tabletext: alla kontroller lyckades" << std::endl;
    else
        std::cerr << "tabletext: " << g_failures << " kontroller misslyckades" << std::endl;
    return g_failures == 0 ? 0 : 1;
}
