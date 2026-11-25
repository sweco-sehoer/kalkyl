#include <algorithm>
#include <cmath>
#include <iostream>
#include <iterator>
#include <limits>
#include <optional>
#include <string>
#include <vector>

#include "calc/normal_span.h"
#include "calc/table_text.h"

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

// Spannlängderna ST0253 till ST0532 (280 spann) ur arkets kolumn D, rad 8-287.
constexpr double kSpansA[] = {
    338, 175, 188, 355, 434, 346, 212, 453, 301, 384, 396, 338, 389, 287, 410, 343, 260, 262, 300, 255,
    466, 414, 453, 227, 414, 346, 342, 425, 280, 389, 399, 312, 235, 466, 334, 277, 409, 377, 318, 360,
    279, 201, 292, 317, 335, 348, 295, 320, 419, 268, 213, 359, 280, 259, 245, 304, 394, 272, 389, 268,
    367, 227, 185, 408, 207, 297, 319, 314, 497, 317, 262, 272, 156, 270, 309, 452, 290, 230, 191, 301,
    435, 226, 292, 337, 362, 225, 310, 237, 257, 353, 401, 368, 276, 277, 248, 317, 354, 461, 341, 243,
    337, 381, 200, 372, 309, 380, 341, 278, 402, 418, 406, 393, 341, 306, 409, 273, 191, 388, 372, 224,
    288, 285, 165, 258, 384, 348, 375, 332.247, 358.154, 322, 287, 314, 180, 281, 393, 392, 337, 258, 238,
    209, 152, 340, 460, 393, 442, 297, 339, 251, 391, 286, 190, 440, 339, 352, 281, 238, 382, 352, 398,
    301, 353, 266, 441, 460, 332, 275, 284, 320, 211, 250, 371, 244, 437, 267, 262, 201, 349, 441, 459,
    391, 330, 310, 300, 400, 241, 353, 316, 427, 257, 333, 274, 241, 300, 428, 371, 350, 208, 284, 150,
    291, 259, 420, 261, 201, 399, 300, 149, 290, 308, 253, 280, 350, 234, 392, 429, 379, 237, 210, 301,
    273, 210, 283, 343, 256, 366, 314, 309, 403, 393, 448, 227, 254, 207, 230, 309, 377, 268, 397, 292,
    433, 299, 299, 186, 342, 236, 403, 298, 308, 303, 400, 336, 340, 393, 367, 272, 320, 249, 412, 329,
    419, 348, 397, 331, 257, 244, 282, 352, 407, 373, 391, 297, 278, 206, 344, 262, 448, 279, 325, 284, 256};

// Kontroll 4 i exempel A: de 64 spannen (bilaga B), angivna som stolpens Stpnr.
const std::vector<std::string> kLocalLowA = {
    "ST0300", "ST0301", "ST0303", "ST0304", "ST0307", "ST0308", "ST0309", "ST0310", "ST0311", "ST0312",
    "ST0313", "ST0315", "ST0316", "ST0320", "ST0321", "ST0332", "ST0333", "ST0339", "ST0340", "ST0341",
    "ST0342", "ST0343", "ST0344", "ST0347", "ST0348", "ST0349", "ST0350", "ST0376", "ST0377", "ST0378",
    "ST0379", "ST0381", "ST0382", "ST0392", "ST0393", "ST0394", "ST0395", "ST0427", "ST0428", "ST0429",
    "ST0430", "ST0451", "ST0452", "ST0453", "ST0454", "ST0457", "ST0458", "ST0459", "ST0460", "ST0461",
    "ST0462", "ST0463", "ST0464", "ST0465", "ST0466", "ST0467", "ST0468", "ST0475", "ST0477", "ST0478",
    "ST0479", "ST0480", "ST0481", "ST0503"};

// Stpnr med fyra siffror, t.ex. "ST0252".
std::string stpnr(int n)
{
    std::string digits = std::to_string(n);
    while (digits.size() < 4)
        digits.insert(digits.begin(), '0');
    return "ST" + digits;
}

// En stolpe med Stpnr, spannlängd och valfri H.
calc::SpanPole pole(std::string name, std::optional<double> length, std::optional<double> height = std::nullopt)
{
    return {std::move(name), length, height};
}

// Exempel A som stolpar: ST0252 med spannet 274 (ingår inte) och sedan de 280 spannen.
std::vector<calc::SpanPole> exampleA()
{
    std::vector<calc::SpanPole> poles;
    poles.push_back(pole(stpnr(252), 274.0));
    for (std::size_t i = 0; i < std::size(kSpansA); ++i)
        poles.push_back(pole(stpnr(253 + static_cast<int>(i)), kSpansA[i]));
    return poles;
}

// Exempel B: P0 med 999 (ingår inte) och P1-P11 med ett långt spann vid P6.
std::vector<calc::SpanPole> exampleB()
{
    std::vector<calc::SpanPole> poles;
    poles.push_back(pole("P0", 999.0));
    const double lengths[] = {100, 100, 100, 100, 100, 400, 100, 100, 100, 100, 100};
    for (int i = 0; i < 11; ++i)
        poles.push_back(pole("P" + std::to_string(i + 1), lengths[i]));
    return poles;
}

// Exempel C: fyra stolpar med höjder. withHeightP2 = false tar bort H på P2.
std::vector<calc::SpanPole> exampleC(bool withHeightP2 = true)
{
    return {pole("P0", 250.0, 10.0), pole("P1", 300.0, 20.0),
            pole("P2", 200.0, withHeightP2 ? std::optional<double>(15.0) : std::nullopt),
            pole("P3", 400.0, 30.0)};
}

// Stpnr för alla spann i listan som uppfyller villkoret, i ordning.
template <class Pred>
std::vector<std::string> polesWhere(const std::vector<calc::SpanPole>& poles, const calc::SpanResult& r, Pred pred)
{
    std::vector<std::string> out;
    for (const auto& s : r.spans)
        if (pred(s))
            out.push_back(poles[static_cast<std::size_t>(s.poleIndex)].pole);
    return out;
}

// Spannet som slutar vid stolpen med Stpnr name.
const calc::SpanCheck* spanAt(const std::vector<calc::SpanPole>& poles, const calc::SpanResult& r,
                              const std::string& name)
{
    for (const auto& s : r.spans)
        if (poles[static_cast<std::size_t>(s.poleIndex)].pole == name)
            return &s;
    return nullptr;
}

// Arkets data: alla storheter i tabellen för exempel A i planen 2.7.
void testExampleA()
{
    const auto poles = exampleA();
    CHECK(poles.size() == 281);
    const auto calc = calc::calculateNormalSpan(poles, calc::SpanLimits{});
    CHECK(calc.result.has_value());
    CHECK(calc.issues.empty());
    if (!calc.result)
        return;
    const calc::SpanResult& r = *calc.result;
    CHECK(near(r.anExact, 343.1371310, 1e-6));
    CHECK(near(r.an, 343.1, 1e-9));
    CHECK(!r.anDeltaH.has_value());
    CHECK(near(r.meanM, 318.4300, 1e-3));
    CHECK(near(r.totalM, 89160.401, 1e-6));
    CHECK(r.poleCount == 281);
    CHECK(r.spanCount == 280);
    CHECK(r.spans.size() == 280);
    CHECK(r.fromPole == "ST0252");
    CHECK(r.toPole == "ST0532");

    CHECK(r.tooLongCount == 0);
    CHECK(r.tooShortCount == 5);
    CHECK(r.adjacentCount == 0);
    CHECK(r.localLowCount == 64);
    CHECK(r.localHighCount == 1);

    const std::vector<std::string> shortOnes = {"ST0325", "ST0375", "ST0393", "ST0451", "ST0459"};
    CHECK(polesWhere(poles, r, [](const calc::SpanCheck& s) { return s.tooShort; }) == shortOnes);
    CHECK(polesWhere(poles, r, [](const calc::SpanCheck& s) { return s.localTooLow; }) == kLocalLowA);
    const std::vector<std::string> highOnes = {"ST0283"};
    CHECK(polesWhere(poles, r, [](const calc::SpanCheck& s) { return s.localTooHigh; }) == highOnes);
    const auto low = polesWhere(poles, r, [](const calc::SpanCheck& s) { return s.localTooLow; });
    CHECK(!low.empty() && low.front() == "ST0300" && low.back() == "ST0503");

    const auto* s325 = spanAt(poles, r, "ST0325");
    CHECK(s325 && near(s325->ratio, 0.4547, 1e-4));
    const auto* s283 = spanAt(poles, r, "ST0283");
    CHECK(s283 && s283->local10 && near(*s283->local10, 395.250, 1e-3));
    CHECK(s283 && s283->local10 && near(*s283->local10 / r.an, 1.152, 1e-3));
    CHECK(s283 && s283->local10 && calc::localText(*s283->local10, r.an) == "395 / 1,15");

    const auto* first = spanAt(poles, r, "ST0263");
    CHECK(first && first->local10 && near(*first->local10, 356.496, 1e-3));
    CHECK(first && first->local10 && calc::localText(*first->local10, r.an) == "356 / 1,04");
    const auto* last = spanAt(poles, r, "ST0532");
    CHECK(last && last->local10 && near(*last->local10, 332.253, 1e-3));
    CHECK(last && last->local10 && calc::localText(*last->local10, r.an) == "332 / 0,97");
    const auto* before = spanAt(poles, r, "ST0262");
    CHECK(before && !before->local10.has_value());

    const auto* s253 = spanAt(poles, r, "ST0253");
    CHECK(s253 && near(s253->ratio, 0.98514, 1e-5));
    CHECK(s253 && calc::ratioText(s253->ratio) == "0,99");
    CHECK(s253 && !s253->adjacent.has_value());

    double maxUp = 0;
    double maxDown = 0;
    for (std::size_t i = 1; i < r.spans.size(); ++i) {
        const double a0 = r.spans[i - 1].lengthM;
        const double a1 = r.spans[i].lengthM;
        if (a1 > a0)
            maxUp = std::max(maxUp, a1 / a0);
        else
            maxDown = std::max(maxDown, a0 / a1);
    }
    CHECK(near(maxUp, 2.316, 1e-3));
    CHECK(near(maxDown, 2.013, 1e-3));

    calc::SpanLimits tighter;
    tighter.shortest = 0.45;
    const auto other = calc::calculateNormalSpan(poles, tighter);
    CHECK(other.result.has_value());
    if (other.result) {
        const std::vector<std::string> three = {"ST0393", "ST0451", "ST0459"};
        CHECK(other.result->tooShortCount == 3);
        CHECK(polesWhere(poles, *other.result, [](const calc::SpanCheck& s) { return s.tooShort; }) == three);
    }
}

// Exempel B: kontrollerna och rättelsen av punkt 3 i båda riktningarna.
void testExampleB()
{
    const auto poles = exampleB();
    const auto calc = calc::calculateNormalSpan(poles, calc::SpanLimits{});
    CHECK(calc.result.has_value());
    CHECK(calc.issues.empty());
    if (!calc.result)
        return;
    const calc::SpanResult& r = *calc.result;
    CHECK(r.spanCount == 11);
    CHECK(r.poleCount == 12);
    CHECK(near(r.totalM, 1400, 1e-9));
    CHECK(near(r.anExact, 229.9068134, 1e-6));
    CHECK(near(r.an, 229.9, 1e-9));
    CHECK(near(r.meanM, 127.273, 1e-3));
    CHECK(r.tooLongCount == 1);
    CHECK(r.tooShortCount == 10);
    CHECK(r.adjacentCount == 2);
    CHECK(r.localLowCount == 0);
    CHECK(r.localHighCount == 0);

    const std::vector<std::string> longOnes = {"P6"};
    CHECK(polesWhere(poles, r, [](const calc::SpanCheck& s) { return s.tooLong; }) == longOnes);
    const auto* p6 = spanAt(poles, r, "P6");
    CHECK(p6 && near(p6->ratio, 1.7399, 1e-4));
    std::size_t shortCount = 0;
    for (const auto& s : r.spans)
        shortCount += s.tooShort ? 1 : 0;
    CHECK(shortCount == 10);
    CHECK(!spanAt(poles, r, "P6")->tooShort);

    const auto* p1 = spanAt(poles, r, "P1");
    CHECK(p1 && !p1->adjacent.has_value() && !p1->adjacentExceeded);
    for (const char* name : {"P2", "P3", "P4", "P5", "P8", "P9", "P10", "P11"}) {
        const auto* s = spanAt(poles, r, name);
        CHECK(s && s->adjacent && calc::adjacentText(*s->adjacent) == "1 : 1" && !s->adjacentExceeded);
    }
    const auto* p5 = spanAt(poles, r, "P5");
    CHECK(p5 && !p5->adjacentExceeded);
    for (const char* name : {"P6", "P7"}) {
        const auto* s = spanAt(poles, r, name);
        CHECK(s && s->adjacent && calc::adjacentText(*s->adjacent) == "1 : 4" && s->adjacentExceeded);
    }

    for (const auto& s : r.spans) {
        const std::string& name = poles[static_cast<std::size_t>(s.poleIndex)].pole;
        if (name == "P11")
            continue;
        CHECK(!s.local10.has_value());
    }
    const auto* p11 = spanAt(poles, r, "P11");
    CHECK(p11 && p11->local10 && near(*p11->local10, 236.968, 1e-3));
    CHECK(p11 && p11->local10 && calc::localText(*p11->local10, r.an) == "237 / 1,03");
    CHECK(p11 && !p11->localTooLow && !p11->localTooHigh);
}

// Exempel C: normalspannet med ΔH, med och utan H på P2, och medelspannet.
void testExampleC()
{
    {
        const auto poles = exampleC();
        const auto calc = calc::calculateNormalSpan(poles, calc::SpanLimits{});
        CHECK(calc.result.has_value());
        CHECK(calc.issues.empty());
        if (calc.result) {
            const calc::SpanResult& r = *calc.result;
            CHECK(near(r.anExact, 331.6624790, 1e-6));
            CHECK(near(r.an, 331.7, 1e-9));
            CHECK(r.anDeltaH && near(*r.anDeltaH, 331.5, 1e-9));
            CHECK(near(r.meanM, 300.0, 1e-9));
            CHECK(near(r.totalM, 900, 1e-9));
            CHECK(r.spans.size() == 3);
            if (r.spans.size() == 3) {
                CHECK(r.spans[0].inclinedM && near(*r.spans[0].inclinedM, 300.166620, 1e-6));
                CHECK(r.spans[1].inclinedM && near(*r.spans[1].inclinedM, 200.062490, 1e-6));
                CHECK(r.spans[2].inclinedM && near(*r.spans[2].inclinedM, 400.281151, 1e-6));
            }
        }
    }
    {
        const auto poles = exampleC(false);
        const auto calc = calc::calculateNormalSpan(poles, calc::SpanLimits{});
        CHECK(calc.result.has_value());
        if (calc.result) {
            CHECK(near(calc.result->an, 331.7, 1e-9));
            CHECK(!calc.result->anDeltaH.has_value());
        }
        CHECK(calc.issues.size() == 1);
        if (calc.issues.size() == 1) {
            CHECK(calc.issues[0].kind == calc::IssueKind::Warning);
            CHECK(calc.issues[0].poleIndex == -1);
            CHECK(calc.issues[0].column == calc::SpanColumn::None);
            CHECK(calc.issues[0].message ==
                  "H saknas på 1 av 4 stolpar. aₙ,ΔH räknas bara när alla stolpar har H.");
        }
    }
}

// Kantfallen i planen 2.7, med felens texter, rader och kolumner.
void testEdgeCases()
{
    const std::string minTwo = "Minst två stolpar (ett spann) behövs.";
    {
        const auto calc = calc::calculateNormalSpan({}, calc::SpanLimits{});
        CHECK(!calc.result.has_value());
        CHECK(calc.issues.size() == 1);
        if (!calc.issues.empty()) {
            CHECK(calc.issues[0].kind == calc::IssueKind::Error);
            CHECK(calc.issues[0].poleIndex == -1);
            CHECK(calc.issues[0].column == calc::SpanColumn::None);
            CHECK(calc.issues[0].message == minTwo);
        }
    }
    {
        const auto calc = calc::calculateNormalSpan({pole("P0", 100.0)}, calc::SpanLimits{});
        CHECK(!calc.result.has_value());
        CHECK(!calc.issues.empty() && calc.issues[0].message == minTwo);
    }
    {
        auto poles = exampleC();
        poles[2].lengthM.reset();
        const auto calc = calc::calculateNormalSpan(poles, calc::SpanLimits{});
        CHECK(!calc.result.has_value());
        CHECK(calc.issues.size() == 1);
        if (!calc.issues.empty()) {
            CHECK(calc.issues[0].kind == calc::IssueKind::Error);
            CHECK(calc.issues[0].poleIndex == 2);
            CHECK(calc.issues[0].column == calc::SpanColumn::Length);
            CHECK(calc.issues[0].message == "Spannlängd saknas.");
        }
    }
    {
        auto poles = exampleC();
        poles[2].lengthM = 0.0;
        const auto calc = calc::calculateNormalSpan(poles, calc::SpanLimits{});
        CHECK(!calc.result.has_value());
        CHECK(!calc.issues.empty() && calc.issues[0].poleIndex == 2 &&
              calc.issues[0].column == calc::SpanColumn::Length &&
              calc.issues[0].message == "Spannlängden måste vara större än 0.");
    }
    {
        auto poles = exampleC();
        poles[1].lengthM = std::numeric_limits<double>::quiet_NaN();
        poles[3].heightM = std::numeric_limits<double>::infinity();
        const auto calc = calc::calculateNormalSpan(poles, calc::SpanLimits{});
        CHECK(!calc.result.has_value());
        CHECK(calc.issues.size() == 2);
        if (calc.issues.size() == 2) {
            CHECK(calc.issues[0].poleIndex == 1 && calc.issues[0].column == calc::SpanColumn::Length &&
                  calc.issues[0].message == "Ogiltigt tal.");
            CHECK(calc.issues[1].poleIndex == 3 && calc.issues[1].column == calc::SpanColumn::Height &&
                  calc.issues[1].message == "Ogiltigt tal.");
        }
    }
    {
        // En helt tom rad mellan P1 och P2 ger samma resultat som utan raden.
        auto poles = exampleC();
        poles.insert(poles.begin() + 2, pole("", std::nullopt));
        const auto calc = calc::calculateNormalSpan(poles, calc::SpanLimits{});
        CHECK(calc.result.has_value());
        CHECK(calc.issues.empty());
        if (calc.result) {
            const calc::SpanResult& r = *calc.result;
            CHECK(near(r.meanM, 300.0, 1e-9));
            CHECK(r.poleCount == 4);
            CHECK(near(r.an, 331.7, 1e-9));
            CHECK(r.anDeltaH && near(*r.anDeltaH, 331.5, 1e-9));
            CHECK(r.spans.size() == 3);
            if (r.spans.size() == 3) {
                CHECK(r.spans[0].poleIndex == 1);
                CHECK(r.spans[1].poleIndex == 3);
                CHECK(r.spans[2].poleIndex == 4);
            }
        }
    }
    for (const double bad : {0.0, std::numeric_limits<double>::quiet_NaN()}) {
        calc::SpanLimits limits;
        limits.adjacent = bad;
        const auto calc = calc::calculateNormalSpan(exampleC(), limits);
        CHECK(!calc.result.has_value());
        CHECK(!calc.issues.empty());
        if (!calc.issues.empty()) {
            CHECK(calc.issues[0].poleIndex == -1 && calc.issues[0].column == calc::SpanColumn::None);
            CHECK(calc.issues[0].message == "Gränserna måste vara tal större än 0.");
        }
    }
    CHECK(near(calc::roundTenth(0.25), 0.3, 1e-12));
    CHECK(near(calc::roundTenth(-0.25), -0.3, 1e-12));
}

// Tal, tabbseparerad text och inklistring.
void testTableText()
{
    using calc::formatDecimal;
    using calc::parseDecimal;

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
    const auto grid = calc::parseTsv(excel);
    CHECK(grid.size() == 3);
    bool shape = true;
    for (const auto& row : grid)
        shape = shape && row.size() == 3;
    CHECK(shape);
    CHECK(grid.size() == 3 && grid[1][2] == "3" && grid[2][0] == "4");
    const std::string back = "a" + tab + "b" + tab + "c" + lf + "1" + tab + "2" + tab + "3" + lf +
                             "4" + tab + "5" + tab + "6" + lf;
    CHECK(calc::formatTsv(grid) == back);

    const std::string quoted = quote + "x" + tab + "y" + quote + quote + "z" + lf + "w" + quote + tab + "q";
    const auto q = calc::parseTsv(quoted);
    CHECK(q.size() == 1 && q[0].size() == 2);
    CHECK(q.size() == 1 && q[0].size() == 2 && q[0][0] == "x" + tab + "y" + quote + "z" + lf + "w");
    CHECK(q.size() == 1 && q[0].size() == 2 && q[0][1] == "q");
    CHECK(calc::formatTsv({{"x" + tab + "y" + quote + "z"}}) == quote + "x" + tab + "y" + quote + quote + "z" + quote + lf);

    const auto column = calc::parseTsv("a" + lf + "b" + lf + "c");
    CHECK(column.size() == 3 && column[0].size() == 1 && column[2][0] == "c");
    const auto rowOnly = calc::parseTsv("a" + tab + "b" + tab + "c");
    CHECK(rowOnly.size() == 1 && rowOnly[0].size() == 3);
    const auto gaps = calc::parseTsv("a" + tab + tab + "c" + tab + lf + tab + "z");
    CHECK(gaps.size() == 2 && gaps[0].size() == 4 && gaps[0][1].empty() && gaps[0][2] == "c" && gaps[0][3].empty());
    CHECK(gaps.size() == 2 && gaps[1].size() == 2 && gaps[1][0].empty() && gaps[1][1] == "z");
    CHECK(calc::parseTsv("").empty());
    CHECK(calc::parseTsv(lf).size() == 1);

    const std::vector<std::vector<std::string>> block23 = {{"1", "2", "3"}, {"4", "5", "6"}};
    const auto p1 = calc::planPaste(5, 1, 3, block23);
    CHECK(p1.cells.size() == 4);
    CHECK(p1.rowsNeeded == 7);
    CHECK(p1.cells.size() == 4 && p1.cells[0].row == 5 && p1.cells[0].column == 1 && p1.cells[0].text == "1");
    CHECK(p1.cells.size() == 4 && p1.cells[3].row == 6 && p1.cells[3].column == 2 && p1.cells[3].text == "5");
    const auto p2 = calc::planPaste(0, 0, 3, {{"a", "b", "c", "d", "e"}});
    CHECK(p2.cells.size() == 3);
    CHECK(p2.rowsNeeded == 1);
    const auto p3 = calc::planPaste(0, 2, 3, {{"a"}, {"b"}, {"c"}});
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
    testExampleA();
    testExampleB();
    testExampleC();
    testEdgeCases();
    testTableText();
    if (g_failures == 0)
        std::cout << "calc: alla kontroller lyckades" << std::endl;
    else
        std::cerr << "calc: " << g_failures << " kontroller misslyckades" << std::endl;
    return g_failures == 0 ? 0 : 1;
}
