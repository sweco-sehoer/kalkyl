// Headless-test för libs/xlsx: zip och XML ur minnet (byggda av xlsx_maker.h), delade och inline-
// strängar, tal och talformat, sammanslagningar, relationer, felen med exakt text och openWorkbook
// mot en fil i en mapp med åäö. Felkoden är antalet fel. Inga filer ur ref/ används.
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <optional>
#include <string>
#include <vector>

#include "platform/files.h"
#include "platform/text.h"
#include "xlsx/xlsx.h"
#include "xlsx_maker.h"

// Gränsen för en uppackad del, ur xlsx.cpp, så att felet "för stor del" går att prova utan 256 MB.
namespace kalkyl::xlsx::detail {
void setPartLimitForTest(std::uint64_t limit);
}

using namespace kalkyl;
using namespace kalkyl::xlsx;
using testxlsx::Part;
using testxlsx::SheetPart;

namespace {

int g_failures = 0;

#define CHECK(cond)                                                                      \
    do {                                                                                 \
        if (!(cond)) {                                                                   \
            ++g_failures;                                                                \
            std::fprintf(stderr, "FEL %s:%d: %s\n", __FILE__, __LINE__, #cond);         \
        }                                                                                \
    } while (0)

// Radbrytningarna som tecken, eftersom hooken inte tillåter bakvända snedstreck i källan.
const std::string kLf(1, char{10});
const std::string kCr(1, char{13});

// Bladets rot med sheetData runt raderna och valfritt innehåll efter sheetData.
std::string sheetXml(const std::string& rows, const std::string& after = {})
{
    return R"xml(<?xml version="1.0" encoding="UTF-8" standalone="yes"?>)xml"
           R"xml(<worksheet xmlns="http://schemas.openxmlformats.org/spreadsheetml/2006/main"><sheetData>)xml" +
           rows + "</sheetData>" + after + "</worksheet>";
}

// En arbetsbok med ett blad "Blad1" och de extra delarna.
std::string bookOf(const std::string& xml, const std::vector<Part>& extra = {})
{
    return testxlsx::workbook({SheetPart{"Blad1", xml, ""}}, extra);
}

// De delade strängarna som en sst-del.
Part sharedPart(const std::string& items)
{
    return {"xl/sharedStrings.xml",
            R"xml(<?xml version="1.0" encoding="UTF-8" standalone="yes"?>)xml"
            R"xml(<sst xmlns="http://schemas.openxmlformats.org/spreadsheetml/2006/main">)xml" +
                items + "</sst>"};
}

// Läser boken och ger första bladet. Ett fel räknas som testfel och ger ett tomt blad.
Sheet firstSheet(const std::string& bytes)
{
    auto r = readWorkbook(bytes, "prov.xlsx");
    if (!r.ok() || r.value().sheets.empty()) {
        ++g_failures;
        std::fprintf(stderr, "FEL: boken gick inte att läsa: %s\n", r.ok() ? "inga blad" : r.error().message.c_str());
        return {};
    }
    return r.value().sheets.front();
}

// Cellen på raden och kolumnen som {slag, text}, eller en tom post när cellen saknas.
const Cell* at(const Sheet& s, int row, int column) { return s.find(row, column); }

// Sant när cellen finns och har slaget och texten.
bool is(const Sheet& s, int row, int column, CellKind kind, const std::string& text)
{
    const Cell* c = s.find(row, column);
    return c && c->kind == kind && c->text == text;
}

// Ett Invalid-fel med exakt text.
bool failsWith(const std::string& bytes, const std::string& message)
{
    auto r = readWorkbook(bytes, "a.xlsx");
    return !r.ok() && r.error().code == core::ErrorCode::Invalid && r.error().message == message;
}

// Tur och retur: fromSheets och readWorkbook ger samma blad, med delade och med inline-strängar.
void testRoundTrip()
{
    Sheet a;
    a.name = "Stolptabell";
    a.cells = {
        {1, 1, CellKind::Text, "   Stolptabell åäö", -1},
        {1, 3, CellKind::Text, "rad" + kLf + "två & <tre>", -1},
        {2, 1, CellKind::Number, "7047024.75", -1},
        {2, 2, CellKind::Number, "422.10000000000002", -1},
        {2, 5, CellKind::Number, "6512000", 2},
        {3, 2, CellKind::Number, "12", 0},
        {4, 1, CellKind::Bool, "1", -1},
        {4, 2, CellKind::Bool, "0", -1},
        {5, 4, CellKind::Error, "#N/A", -1},
        {70, 89, CellKind::Text, "sist", -1},
    };
    a.merges = {{11, 9, 11, 15}, {12, 23, 12, 24}};
    a.lastRow = 70;
    a.lastColumn = 89;
    Sheet b;
    b.name = "Dolt & gömt";
    b.hidden = true;
    b.cells = {{2, 2, CellKind::Text, "x", -1}};
    b.lastRow = 2;
    b.lastColumn = 2;
    for (bool shared : {true, false}) {
        auto r = readWorkbook(testxlsx::fromSheets({a, b}, shared), "prov.xlsx");
        CHECK(r.ok());
        if (!r.ok())
            continue;
        const auto& sheets = r.value().sheets;
        CHECK(sheets.size() == 2);
        if (sheets.size() != 2)
            continue;
        for (size_t i = 0; i < 2; ++i) {
            const Sheet& want = i == 0 ? a : b;
            CHECK(sheets[i].name == want.name);
            CHECK(sheets[i].hidden == want.hidden);
            CHECK(sheets[i].cells == want.cells);
            CHECK(sheets[i].merges == want.merges);
            CHECK(sheets[i].lastRow == want.lastRow);
            CHECK(sheets[i].lastColumn == want.lastColumn);
        }
        CHECK(sheets[0].find(2, 2) && sheets[0].find(2, 2)->text == "422.10000000000002");
        CHECK(sheets[0].find(2, 3) == nullptr);
        CHECK(sheets[0].find(1, 3)->text == "rad" + kLf + "två & <tre>");
    }
}

// Delade strängar: rik text ihopfogad, fonetiska körningar borta, tom text ger ingen cell och
// blanktecken behålls med xml:space="preserve".
void testSharedStrings()
{
    const std::string items =
        R"xml(<si><r><t xml:space="preserve">Hej </t></r><r><rPr><b/></rPr><t>då</t></r>)xml"
        R"xml(<rPh sb="0" eb="1"><t>ruby</t></rPh><phoneticPr fontId="1"/></si>)xml"
        R"xml(<si><t/></si>)xml"
        R"xml(<si><t xml:space="preserve">  x  </t><phoneticPr fontId="1"/></si>)xml"
        R"xml(<si><t>plain</t></si>)xml";
    const std::string rows =
        R"xml(<row r="1"><c r="A1" t="s"><v>0</v></c><c r="B1" t="s"><v>1</v></c></row>)xml"
        R"xml(<row r="2"><c r="A2" t="s"><v>2</v></c><c r="B2" t="s"><v>3</v></c></row>)xml";
    const Sheet s = firstSheet(bookOf(sheetXml(rows), {sharedPart(items)}));
    CHECK(is(s, 1, 1, CellKind::Text, "Hej då"));
    CHECK(at(s, 1, 2) == nullptr);
    CHECK(is(s, 2, 1, CellKind::Text, "  x  "));
    CHECK(is(s, 2, 2, CellKind::Text, "plain"));
    CHECK(s.cells.size() == 3);
}

// Inline-strängar: is/t och is/r/t.
void testInlineStrings()
{
    const std::string rows =
        R"xml(<row r="1"><c r="A1" t="inlineStr"><is><t>ab</t></is></c>)xml"
        R"xml(<c r="B1" t="inlineStr"><is><r><t>c</t></r><r><t>d</t></r></is></c>)xml"
        R"xml(<c r="C1" t="inlineStr"><is><t/></is></c></row>)xml";
    const Sheet s = firstSheet(bookOf(sheetXml(rows)));
    CHECK(is(s, 1, 1, CellKind::Text, "ab"));
    CHECK(is(s, 1, 2, CellKind::Text, "cd"));
    CHECK(at(s, 1, 3) == nullptr);
}

// Talen som de står, talformatens decimaler ur styles.xml och numberText och formatDecimals.
void testNumbers()
{
    const std::string styles =
        R"xml(<?xml version="1.0" encoding="UTF-8" standalone="yes"?>)xml"
        R"xml(<styleSheet xmlns="http://schemas.openxmlformats.org/spreadsheetml/2006/main">)xml"
        R"xml(<numFmts count="3"><numFmt numFmtId="164" formatCode="0.000"/>)xml"
        R"xml(<numFmt numFmtId="165" formatCode="#,##0.00"/><numFmt numFmtId="166" formatCode="0%"/></numFmts>)xml"
        R"xml(<cellStyleXfs count="1"><xf numFmtId="2"/></cellStyleXfs>)xml"
        R"xml(<cellXfs count="7"><xf numFmtId="0"/><xf numFmtId="2"/><xf numFmtId="164"/><xf numFmtId="165"/>)xml"
        R"xml(<xf numFmtId="166"/><xf numFmtId="1"/><xf numFmtId="14"/></cellXfs></styleSheet>)xml";
    const std::string rows =
        R"xml(<row r="1"><c r="A1"><v>7047024.75</v></c><c r="B1"><v>422.10000000000002</v></c>)xml"
        R"xml(<c r="C1" s="1"><v>6512000</v></c><c r="D1" s="2"><v>1.5</v></c><c r="E1" s="3"><v>2</v></c>)xml"
        R"xml(<c r="F1" s="4"><v>0.5</v></c><c r="G1" s="5"><v>7</v></c><c r="H1" s="6"><v>46000</v></c>)xml"
        R"xml(<c r="I1" s="9"><v>3</v></c><c r="J1" t="n"><v>-0</v></c></row>)xml";
    const Sheet s = firstSheet(bookOf(sheetXml(rows), {Part{"xl/styles.xml", styles}}));
    CHECK(is(s, 1, 1, CellKind::Number, "7047024.75") && at(s, 1, 1)->decimals == -1);
    CHECK(is(s, 1, 2, CellKind::Number, "422.10000000000002"));
    CHECK(is(s, 1, 3, CellKind::Number, "6512000") && at(s, 1, 3)->decimals == 2);
    CHECK(at(s, 1, 4) && at(s, 1, 4)->decimals == 3);
    CHECK(at(s, 1, 5) && at(s, 1, 5)->decimals == 2);
    CHECK(at(s, 1, 6) && at(s, 1, 6)->decimals == -1);
    CHECK(at(s, 1, 7) && at(s, 1, 7)->decimals == 0);
    CHECK(at(s, 1, 8) && at(s, 1, 8)->decimals == -1);
    CHECK(at(s, 1, 9) && at(s, 1, 9)->decimals == -1);
    CHECK(is(s, 1, 10, CellKind::Number, "-0"));

    CHECK(numberText("422.10000000000002", -1) == "422.1");
    CHECK(numberText("0.30000000000000004", -1) == "0.3");
    CHECK(numberText("1E-3", -1) == "0.001");
    CHECK(numberText("1.5E+20", -1) == "150000000000000000000");
    CHECK(numberText("-0", -1) == "0");
    CHECK(numberText("6512000", 2) == "6512000.00");
    CHECK(numberText("7047024.75", -1) == "7047024.75");
    CHECK(numberText("1234", -1) == "1234");
    CHECK(numberText("0.285", 2) == "0.29");
    CHECK(numberText("-1.005", 2) == "-1.01");
    CHECK(numberText("9.999", 2) == "10.00");
    CHECK(numberText("0.5", 0) == "1");
    CHECK(numberText("0.004", 2) == "0.00");
    CHECK(numberText("-0.004", 2) == "0.00");
    CHECK(numberText("-12.5", -1) == "-12.5");
    CHECK(numberText("123456789.123456789", -1) == "123456789.123457");
    CHECK(numberText("", -1) == std::nullopt);
    CHECK(numberText("abc", -1) == std::nullopt);
    CHECK(numberText("nan", -1) == std::nullopt);
    CHECK(numberText("1e999", -1) == std::nullopt);
    CHECK(numberText("12 m", -1) == std::nullopt);

    const std::string bs(1, char{92});
    CHECK(formatDecimals("General") == -1);
    CHECK(formatDecimals("0%") == -1);
    CHECK(formatDecimals("0.00E+00") == -1);
    CHECK(formatDecimals("yyyy-mm-dd") == -1);
    CHECK(formatDecimals("[Red]0.0") == 1);
    CHECK(formatDecimals("#,##0") == 0);
    CHECK(formatDecimals("0.00") == 2);
    CHECK(formatDecimals("#,##0.000") == 3);
    CHECK(formatDecimals("#,##0.00_);[Red](#,##0.00)") == 2);
    CHECK(formatDecimals(std::string("0.00 ") + char{34} + "kr" + char{34}) == 2);
    CHECK(formatDecimals(std::string("0.0 ") + char{34} + "dag" + char{34}) == 1);
    CHECK(formatDecimals("0.0" + bs + "m") == 1);
    CHECK(formatDecimals("@") == -1);
    CHECK(formatDecimals("") == -1);
}

// Cellslagen Bool, Error, formler med sparat värde och ISO-datum som text.
void testOtherKinds()
{
    const std::string rows =
        R"xml(<row r="1"><c r="A1" t="b"><v>1</v></c><c r="B1" t="b"><v>0</v></c><c r="C1" t="e"><v>#N/A</v></c></row>)xml"
        R"xml(<row r="2"><c r="A2" t="str"><f>A1&amp;B1</f><v>summa</v></c><c r="B2"><f>1+2</f><v>3</v></c>)xml"
        R"xml(<c r="C2" t="d"><v>2026-11-26</v></c><c r="D2"><f>A1</f></c></row>)xml";
    const Sheet s = firstSheet(bookOf(sheetXml(rows)));
    CHECK(is(s, 1, 1, CellKind::Bool, "1"));
    CHECK(is(s, 1, 2, CellKind::Bool, "0"));
    CHECK(is(s, 1, 3, CellKind::Error, "#N/A"));
    CHECK(is(s, 2, 1, CellKind::Text, "summa"));
    CHECK(is(s, 2, 2, CellKind::Number, "3"));
    CHECK(is(s, 2, 3, CellKind::Text, "2026-11-26"));
    CHECK(at(s, 2, 4) == nullptr);
}

// Radbrytningar och _xHHHH_ i texten.
void testLineBreaks()
{
    const std::string utf8Emoji = {static_cast<char>(0xF0), static_cast<char>(0x9F), static_cast<char>(0x98),
                                   static_cast<char>(0x80)};
    auto inlineCell = [](int col, const std::string& text) {
        return "<c r=\"" + std::string(1, static_cast<char>('A' + col)) + "1\" t=\"inlineStr\"><is><t>" + text +
               "</t></is></c>";
    };
    const std::string rows = "<row r=\"1\">" + inlineCell(0, "a" + kCr + kLf + "b") + inlineCell(1, "a" + kCr + "b") +
                             inlineCell(2, "a_x000D_" + kLf + "b") + inlineCell(3, "_x005F_x000D_") +
                             inlineCell(4, "_xD83D__xDE00_") + inlineCell(5, "x_x00E5_y") + inlineCell(6, "_xZZZZ_") +
                             "</row>";
    const Sheet s = firstSheet(bookOf(sheetXml(rows)));
    CHECK(is(s, 1, 1, CellKind::Text, "a" + kLf + "b"));
    CHECK(is(s, 1, 2, CellKind::Text, "a" + kLf + "b"));
    CHECK(is(s, 1, 3, CellKind::Text, "a" + kLf + "b"));
    CHECK(is(s, 1, 4, CellKind::Text, "_x000D_"));
    CHECK(is(s, 1, 5, CellKind::Text, utf8Emoji));
    CHECK(is(s, 1, 6, CellKind::Text, "xåy"));
    CHECK(is(s, 1, 7, CellKind::Text, "_xZZZZ_"));
}

// Entiteter, CDATA, och en DOCTYPE som vägras.
void testEntities()
{
    const std::string rows =
        R"xml(<row r="1"><c r="A1" t="inlineStr"><is><t>&amp;&lt;&gt;&quot;&apos; &#228;&#xE4;</t></is></c>)xml"
        R"xml(<c r="B1" t="inlineStr"><is><t><![CDATA[a<b&c]]></t></is></c></row>)xml";
    const Sheet s = firstSheet(bookOf(sheetXml(rows)));
    CHECK(is(s, 1, 1, CellKind::Text, std::string("&<>") + char{34} + "' ää"));
    CHECK(is(s, 1, 2, CellKind::Text, "a<b&c"));

    const std::string damaged = "a.xlsx är skadad och kan inte läsas som en Excelfil.";
    const std::string doctype = R"xml(<!DOCTYPE x [<!ENTITY e "y">]>)xml" + sheetXml(rows);
    CHECK(failsWith(bookOf(doctype), damaged));
    CHECK(failsWith(bookOf(sheetXml(R"xml(<row r="1"><c r="A1" t="inlineStr"><is><t>&foo;</t></is></c></row>)xml")),
                    damaged));
    CHECK(failsWith(bookOf(R"xml(<worksheet><sheetData><row></sheetData></worksheet>)xml"), damaged));
}

// Celler och rader utan r, och namnrymdsprefix.
void testMissingRefsAndPrefix()
{
    const std::string rows =
        R"xml(<row><c t="inlineStr"><is><t>a</t></is></c><c t="inlineStr"><is><t>b</t></is></c></row>)xml"
        R"xml(<row><c><v>1</v></c></row>)xml"
        R"xml(<row r="5"><c><v>1</v></c><c r="D5"><v>2</v></c><c><v>3</v></c></row>)xml";
    const Sheet s = firstSheet(bookOf(sheetXml(rows)));
    CHECK(is(s, 1, 1, CellKind::Text, "a"));
    CHECK(is(s, 1, 2, CellKind::Text, "b"));
    CHECK(is(s, 2, 1, CellKind::Number, "1"));
    CHECK(is(s, 5, 1, CellKind::Number, "1"));
    CHECK(is(s, 5, 4, CellKind::Number, "2"));
    CHECK(is(s, 5, 5, CellKind::Number, "3"));
    CHECK(s.lastRow == 5 && s.lastColumn == 5);

    const std::string prefixed =
        R"xml(<?xml version="1.0"?><x:worksheet xmlns:x="http://schemas.openxmlformats.org/spreadsheetml/2006/main">)xml"
        R"xml(<x:sheetData><x:row r="2"><x:c r="B2"><x:v>7</x:v></x:c><x:c r="C2" t="inlineStr"><x:is><x:t>p</x:t></x:is></x:c>)xml"
        R"xml(</x:row></x:sheetData><x:mergeCells count="1"><x:mergeCell ref="B2:C3"/></x:mergeCells></x:worksheet>)xml";
    const Sheet p = firstSheet(bookOf(prefixed));
    CHECK(is(p, 2, 2, CellKind::Number, "7"));
    CHECK(is(p, 2, 3, CellKind::Text, "p"));
    CHECK(p.merges.size() == 1);
}

// columnLetters och parseCellRef.
void testReferences()
{
    CHECK(columnLetters(1) == "A");
    CHECK(columnLetters(26) == "Z");
    CHECK(columnLetters(27) == "AA");
    CHECK(columnLetters(52) == "AZ");
    CHECK(columnLetters(53) == "BA");
    CHECK(columnLetters(89) == "CK");
    CHECK(columnLetters(16384) == "XFD");
    CHECK(columnLetters(0).empty());
    CHECK(columnLetters(16385).empty());
    using Ref = std::optional<std::pair<int, int>>;
    CHECK(parseCellRef("A1") == Ref(std::pair{1, 1}));
    CHECK(parseCellRef("AB12") == Ref(std::pair{12, 28}));
    CHECK(parseCellRef("$AB$12") == Ref(std::pair{12, 28}));
    CHECK(parseCellRef("CK16") == Ref(std::pair{16, 89}));
    CHECK(parseCellRef("XFD1048576") == Ref(std::pair{1048576, 16384}));
    CHECK(!parseCellRef("A0"));
    CHECK(!parseCellRef("1A"));
    CHECK(!parseCellRef(""));
    CHECK(!parseCellRef("XFE1"));
    CHECK(!parseCellRef("A1048577"));
    CHECK(!parseCellRef("A"));
    CHECK(!parseCellRef("A1B"));
}

// Sammanslagningar, och en sammanslagning med bara en cell hoppas över.
void testMerges()
{
    const std::string after = R"xml(<mergeCells count="4"><mergeCell ref="W12:X12"/><mergeCell ref="I12:I14"/>)xml"
                              R"xml(<mergeCell ref="A1:A1"/><mergeCell ref="C3"/></mergeCells>)xml";
    const Sheet s = firstSheet(bookOf(sheetXml(R"xml(<row r="12"><c r="W12" t="inlineStr"><is><t>Av</t></is></c></row>)xml",
                                               after)));
    CHECK(s.merges.size() == 2);
    const Merge first{12, 23, 12, 24};
    const Merge second{12, 9, 14, 9};
    CHECK(s.merges.size() == 2 && s.merges[0] == first);
    CHECK(s.merges.size() == 2 && s.merges[1] == second);
}

// Relationerna: Target med snedstreck först och med "../", ett diagramblad, ett dolt blad, och en bok
// utan sharedStrings.xml och styles.xml. Utan _rels/.rels används xl/workbook.xml.
void testRelationships()
{
    const std::string book =
        R"xml(<?xml version="1.0"?><workbook xmlns="http://schemas.openxmlformats.org/spreadsheetml/2006/main" )xml"
        R"xml(xmlns:r="http://schemas.openxmlformats.org/officeDocument/2006/relationships"><sheets>)xml"
        R"xml(<sheet name="Blad1" sheetId="1" r:id="rId1"/><sheet name="Diagram" sheetId="2" r:id="rId2"/>)xml"
        R"xml(<sheet name="Blad3" sheetId="3" state="veryHidden" r:id="rId3"/></sheets></workbook>)xml";
    const std::string rels =
        R"xml(<?xml version="1.0"?><Relationships xmlns="http://schemas.openxmlformats.org/package/2006/relationships">)xml"
        R"xml(<Relationship Id="rId1" Type="http://schemas.openxmlformats.org/officeDocument/2006/relationships/worksheet" )xml"
        R"xml(Target="/xl/worksheets/a.xml"/>)xml"
        R"xml(<Relationship Id="rId2" Type="http://schemas.openxmlformats.org/officeDocument/2006/relationships/chartsheet" )xml"
        R"xml(Target="chartsheets/sheet1.xml"/>)xml"
        R"xml(<Relationship Id="rId3" Type="http://schemas.openxmlformats.org/officeDocument/2006/relationships/worksheet" )xml"
        R"xml(Target="../xl/worksheets/c.xml"/></Relationships>)xml";
    const std::string a = sheetXml(R"xml(<row r="1"><c r="A1"><v>1</v></c></row>)xml");
    const std::string c = sheetXml(R"xml(<row r="1"><c r="A1"><v>3</v></c></row>)xml");
    auto r = readWorkbook(testxlsx::zip({{"xl/workbook.xml", book},
                                         {"xl/_rels/workbook.xml.rels", rels},
                                         {"xl/worksheets/a.xml", a},
                                         {"xl/worksheets/c.xml", c}}),
                          "prov.xlsx");
    CHECK(r.ok());
    if (r.ok()) {
        const auto& sheets = r.value().sheets;
        CHECK(sheets.size() == 2);
        if (sheets.size() == 2) {
            CHECK(sheets[0].name == "Blad1" && !sheets[0].hidden);
            CHECK(is(sheets[0], 1, 1, CellKind::Number, "1"));
            CHECK(sheets[1].name == "Blad3" && sheets[1].hidden);
            CHECK(is(sheets[1], 1, 1, CellKind::Number, "3"));
        }
    }
    // Delnamn jämförs utan skillnad på stora och små bokstäver.
    auto upper = readWorkbook(testxlsx::zip({{"XL/Workbook.xml", book},
                                             {"XL/_rels/Workbook.xml.rels", rels},
                                             {"xl/Worksheets/A.xml", a},
                                             {"xl/worksheets/C.xml", c}}),
                              "prov.xlsx");
    CHECK(upper.ok() && upper.value().sheets.size() == 2);
}

// Felen med exakt text (planen 9.1).
void testErrors()
{
    CHECK(failsWith("det här är bara text", "a.xlsx är inte en Excelfil (.xlsx eller .xlsm)."));
    CHECK(failsWith("", "a.xlsx är inte en Excelfil (.xlsx eller .xlsm)."));
    const std::string ole = {static_cast<char>(0xD0), static_cast<char>(0xCF), static_cast<char>(0x11),
                             static_cast<char>(0xE0), static_cast<char>(0xA1), static_cast<char>(0xB1),
                             static_cast<char>(0x1A), static_cast<char>(0xE1), 'x',  'x'};
    CHECK(failsWith(ole, "a.xlsx är en äldre Excelfil (.xls) eller lösenordsskyddad. Spara den som .xlsx i Excel utan "
                         "lösenord och importera den."));

    const std::string good = bookOf(sheetXml(R"xml(<row r="1"><c r="A1"><v>1</v></c></row>)xml"));
    CHECK(readWorkbook(good, "a.xlsx").ok());
    CHECK(failsWith(good.substr(0, good.size() / 2), "a.xlsx är skadad och kan inte läsas som en Excelfil."));
    CHECK(failsWith(testxlsx::zip({{"ord/dokument.xml", "hej"}}), "a.xlsx är inte en Excelfil (.xlsx eller .xlsm)."));
    CHECK(failsWith(testxlsx::workbook({}), "a.xlsx har inga kalkylblad."));
    // Bara diagramblad räknas också som inga kalkylblad.
    CHECK(failsWith(bookOf(R"xml(<worksheet><sheetData></sheetData></worksheet>)xml",
                           {Part{"xl/_rels/workbook.xml.rels",
                                 R"xml(<Relationships xmlns="http://schemas.openxmlformats.org/package/2006/relationships">)xml"
                                 R"xml(<Relationship Id="rId1" Type="http://schemas.openxmlformats.org/officeDocument/2006/relationships/chartsheet" )xml"
                                 R"xml(Target="worksheets/sheet1.xml"/></Relationships>)xml"}}),
                    "a.xlsx har inga kalkylblad."));

    // En del som är för stor: gränsen sänks tillfälligt.
    detail::setPartLimitForTest(10);
    CHECK(failsWith(good, "a.xlsx har en del som är för stor uppackad (mer än 256 MB)."));
    detail::setPartLimitForTest(kMaxPartBytes);
    CHECK(readWorkbook(good, "a.xlsx").ok());

    // Ett delat index utanför listan är en skadad fil.
    const std::string badIndex =
        bookOf(sheetXml(R"xml(<row r="1"><c r="A1" t="s"><v>5</v></c></row>)xml"),
               {sharedPart("<si><t>a</t></si>")});
    CHECK(failsWith(badIndex, "a.xlsx är skadad och kan inte läsas som en Excelfil."));
}

// openWorkbook mot en fil i en mapp med åäö, och felen för en fil som saknas eller inte är Excel.
void testOpenWorkbook(const std::string& dir)
{
    Sheet s;
    s.name = "Stolptabell";
    s.cells = {{1, 1, CellKind::Text, "Stolpnr", -1}, {2, 1, CellKind::Text, "ST9002", -1}};
    s.lastRow = 2;
    s.lastColumn = 1;
    const std::string path = dir + "/Stolptabell åäö.xlsx";
    {
        std::ofstream out(platform::pathFromUtf8(path), std::ios::binary);
        out << testxlsx::fromSheets({s});
    }
    auto ok = openWorkbook(path);
    CHECK(ok.ok());
    if (ok.ok()) {
        CHECK(ok.value().sheets.size() == 1);
        CHECK(ok.value().sheets.front().cells == s.cells);
    }

    auto missing = openWorkbook(dir + "/finns inte.xlsx");
    CHECK(!missing.ok() && missing.error().code == core::ErrorCode::NotFound &&
          missing.error().message == "Filen finns inte.xlsx finns inte.");

    const std::string text = dir + "/anteckning.txt";
    {
        std::ofstream out(platform::pathFromUtf8(text), std::ios::binary);
        out << "bara text";
    }
    auto notExcel = openWorkbook(text);
    CHECK(!notExcel.ok() && notExcel.error().code == core::ErrorCode::Invalid &&
          notExcel.error().message == "anteckning.txt är inte en Excelfil (.xlsx eller .xlsm).");
}

} // namespace

int main()
{
    const std::string root = platform::pathToUtf8(std::filesystem::temp_directory_path()) + "/kalkyl-xlsx-test";
    const std::string dir = root + "/Prov åäö";
    if (!platform::ensureDirectory(dir).ok()) {
        std::fprintf(stderr, "FEL: kunde inte skapa testmappen\n");
        return 1;
    }
    testRoundTrip();
    testSharedStrings();
    testInlineStrings();
    testNumbers();
    testOtherKinds();
    testLineBreaks();
    testEntities();
    testMissingRefsAndPrefix();
    testReferences();
    testMerges();
    testRelationships();
    testErrors();
    testOpenWorkbook(dir);

    std::error_code ec;
    std::filesystem::remove_all(platform::pathFromUtf8(root), ec);
    if (g_failures == 0)
        std::printf("xlsx: alla tester gick igenom\n");
    return g_failures;
}
