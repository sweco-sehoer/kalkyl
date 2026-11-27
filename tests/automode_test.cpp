#include <cmath>
#include <iostream>
#include <string>
#include <vector>

#include "autoMode/auto_mode.h"

using namespace kalkyl;
using namespace kalkyl::automode;
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

// Radbrytningen mellan flera noter på samma rad.
const std::string kNl(1, char{10});

// En rad i bilaga A: Stolpnr, spannlängd och H, med Av och Ah ifyllda. N och E är tomma, så
// att en tömd spannlängd inte räknas ur koordinaterna (det provar testCoordinateSpans).
PoleRow poleRow(const std::string& pole, const std::string& span, const std::string& height)
{
    return {pole, "300", "300", span, "", "", height, ""};
}

// Bilaga A: tio stolpar ST0100-ST0109 med spannlängderna och höjderna ur planen.
std::vector<PoleRow> appendixA()
{
    return {
        poleRow("ST0100", "", "40.0"),    poleRow("ST0101", "300", "42.5"), poleRow("ST0102", "320", "45.0"),
        poleRow("ST0103", "280", "43.0"), poleRow("ST0104", "350", "47.5"), poleRow("ST0105", "310", "50.0"),
        poleRow("ST0106", "290", "48.0"), poleRow("ST0107", "330", "46.5"), poleRow("ST0108", "300", "44.0"),
        poleRow("ST0109", "270", "41.0"),
    };
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

// Bilaga A efter ombyggnad: ST0104 har spannet 365 och ST0105 spannet 295.
std::vector<PoleRow> appendixAfter()
{
    std::vector<PoleRow> rows = asAfter(appendixA());
    rows[4].spanM = "365";
    rows[5].spanM = "295";
    return rows;
}

const char* const kTable = "Stolptabell före";

// Sant när två tal är lika inom tolerans.
bool near(double a, double b, double tol = 1e-3)
{
    return std::fabs(a - b) <= tol;
}

// Alla felen i 7.1 med exakt text och i rätt ordning, och en lyckad sökning.
void testFindRange()
{
    const std::vector<PoleRow> rows = appendixA();

    const RangeRows ok = findRange(rows, {"ST0102", "ST0107"}, "faslinan", kTable);
    CHECK(ok.issue.empty());
    CHECK(ok.first == 2);
    CHECK(ok.last == 7);

    // 1: start eller slut saknas.
    CHECK(findRange(rows, {"", ""}, "faslinan", kTable).issue == "Välj var faslinan börjar och slutar.");
    CHECK(findRange(rows, {"ST0102", ""}, "topplinorna", kTable).issue == "Välj var topplinorna börjar och slutar.");
    CHECK(findRange(rows, {"", "ST0107"}, "topplina höger", kTable).issue ==
          "Välj var topplina höger börjar och slutar.");
    CHECK(findRange(rows, {"  ", "ST0107"}, "faslinan", kTable).issue == "Välj var faslinan börjar och slutar.");
    const RangeRows none = findRange(rows, {"", ""}, "faslinan", kTable);
    CHECK(none.first == -1 && none.last == -1);

    // 2 och 3: stolpen finns inte. Starten prövas före slutet.
    CHECK(findRange(rows, {"ST0199", "ST0107"}, "faslinan", kTable).issue ==
          "Stolpnr ST0199 finns inte i Stolptabell före.");
    CHECK(findRange(rows, {"ST0102", "ST0198"}, "faslinan", kTable).issue ==
          "Stolpnr ST0198 finns inte i Stolptabell före.");
    CHECK(findRange(rows, {"ST0199", "ST0198"}, "faslinan", kTable).issue ==
          "Stolpnr ST0199 finns inte i Stolptabell före.");
    // Stora och små bokstäver skiljs.
    CHECK(findRange(rows, {"st0102", "ST0107"}, "faslinan", kTable).issue ==
          "Stolpnr st0102 finns inte i Stolptabell före.");

    // 4 och 5: stolpen finns flera gånger. "Finns inte" går före "flera gånger".
    std::vector<PoleRow> dup = appendixA();
    dup[8].pole = "ST0103";
    CHECK(findRange(dup, {"ST0103", "ST0107"}, "faslinan", kTable).issue ==
          "Stolpnr ST0103 finns flera gånger i Stolptabell före.");
    CHECK(findRange(dup, {"ST0102", "ST0103"}, "faslinan", kTable).issue ==
          "Stolpnr ST0103 finns flera gånger i Stolptabell före.");
    CHECK(findRange(dup, {"ST0103", "ST0198"}, "faslinan", kTable).issue ==
          "Stolpnr ST0198 finns inte i Stolptabell före.");
    dup[7].pole = "ST0104";
    CHECK(findRange(dup, {"ST0103", "ST0104"}, "faslinan", kTable).issue ==
          "Stolpnr ST0103 finns flera gånger i Stolptabell före.");

    // 6: samma stolpe, med versal i början av meningen.
    CHECK(findRange(rows, {"ST0103", "ST0103"}, "faslinan", kTable).issue ==
          "Faslinan börjar och slutar på samma stolpe.");
    CHECK(findRange(rows, {"ST0103", "ST0103"}, "topplina vänster", kTable).issue ==
          "Topplina vänster börjar och slutar på samma stolpe.");

    // 7: slutet står före starten.
    CHECK(findRange(rows, {"ST0107", "ST0102"}, "faslinan", kTable).issue ==
          "Slutet ST0102 står före starten ST0107 i Stolptabell före. Byt start och slut.");

    // Stolpnr trimmas i både sträckan och tabellen.
    std::vector<PoleRow> padded = appendixA();
    padded[2].pole = " ST0102 ";
    const RangeRows trimmed = findRange(padded, {"  ST0102", "ST0107 "}, "faslinan", kTable);
    CHECK(trimmed.issue.empty() && trimmed.first == 2 && trimmed.last == 7);

    // En tom rad mitt i räknas som en rad.
    std::vector<PoleRow> gap = appendixA();
    gap.insert(gap.begin() + 4, PoleRow{});
    const RangeRows withGap = findRange(gap, {"ST0102", "ST0107"}, "faslinan", kTable);
    CHECK(withGap.issue.empty() && withGap.first == 2 && withGap.last == 8);

    // Tabellens namn står i texten.
    CHECK(findRange(rows, {"ST0199", "ST0107"}, "faslinan", "Stolptabell efter").issue ==
          "Stolpnr ST0199 finns inte i Stolptabell efter.");
}

// Fälten ur raderna, tomma tal som tomma och ogiltiga gränser som en tom lista.
void testSpanPoles()
{
    const std::vector<PoleRow> rows = appendixA();
    const std::vector<ancalc::SpanPole> poles = spanPoles(rows, 2, 4);
    CHECK(poles.size() == 3);
    CHECK(poles[0].pole == "ST0102");
    CHECK(poles[0].lengthM && near(*poles[0].lengthM, 320));
    CHECK(poles[0].heightM && near(*poles[0].heightM, 45.0));
    CHECK(poles[2].pole == "ST0104");
    CHECK(poles[2].lengthM && near(*poles[2].lengthM, 350));
    CHECK(poles[2].heightM && near(*poles[2].heightM, 47.5));

    // Tomt tal blir tomt.
    const std::vector<ancalc::SpanPole> withEmpty = spanPoles(rows, 0, 1);
    CHECK(withEmpty.size() == 2);
    CHECK(!withEmpty[0].lengthM);
    CHECK(withEmpty[1].lengthM && near(*withEmpty[1].lengthM, 300));
    std::vector<PoleRow> noHeight = appendixA();
    noHeight[3].height.clear();
    CHECK(!spanPoles(noHeight, 3, 3)[0].heightM);

    // Index utanför tabellen och first > last ger en tom lista.
    CHECK(spanPoles(rows, -1, 3).empty());
    CHECK(spanPoles(rows, 2, 10).empty());
    CHECK(spanPoles(rows, 5, 4).empty());
    CHECK(spanPoles({}, 0, 0).empty());
    CHECK(spanPoles(rows, 9, 9).size() == 1);
}

// Spannlängd föreg. ur koordinaterna: bara i tomma celler, avståndet till närmast föregående rad
// med innehåll, aldrig för första stolpen och inte när N eller E saknas på någon av raderna.
void testCoordinateSpans()
{
    std::vector<PoleRow> rows = {
        {"A", "", "", "", "1000.00", "2000.00", "", ""},   // första stolpen: inget föregående
        {"B", "", "", "", "1300.00", "2400.00", "", ""},   // 3-4-5: 500 m
        {"C", "", "", "123.4", "1300.00", "2700.00", "", ""},   // ifylld spannlängd gäller
        {},                                                // helt tom rad hoppas över
        {"D", "", "", "", "1300.00", "2950.00", "", ""},   // 250 m till C, förbi den tomma raden
        {"E", "", "", "", "", "3000.00", "", ""},          // N saknas
        {"F", "", "", "", "1300.00", "3100.00", "", ""},   // föregående (E) saknar N
        {"G", "", "", "", "1300.00", "3100.00", "", ""},   // samma koordinater som F: 0 m räknas inte
    };
    const std::vector<std::optional<double>> spans = coordinateSpans(rows);
    CHECK(spans.size() == rows.size());
    CHECK(!spans[0]);
    CHECK(spans[1] && near(*spans[1], 500.0, 1e-9));
    CHECK(!spans[2]);
    CHECK(!spans[3]);
    CHECK(spans[4] && near(*spans[4], 250.0, 1e-9));
    CHECK(!spans[5]);
    CHECK(!spans[6]);
    CHECK(!spans[7]);
    CHECK(coordinateSpans({}).empty());

    // Avrundat till två decimaler: √2 m blir 1,41 m.
    PoleRow a = poleRow("A", "", "");
    a.north = "0";
    a.east = "0";
    PoleRow b = poleRow("B", "", "");
    b.north = "1";
    b.east = "1";
    const auto rounded = coordinateSpans({a, b});
    CHECK(rounded.size() == 2 && rounded[1] && *rounded[1] == 1.41);

    // spanPoles tar den tomma spannlängden ur koordinaterna och behåller den ifyllda.
    const std::vector<ancalc::SpanPole> poles = spanPoles(rows, 0, 4);
    CHECK(poles.size() == 5);
    CHECK(!poles[0].lengthM);
    CHECK(poles[1].lengthM && near(*poles[1].lengthM, 500.0, 1e-9));
    CHECK(poles[2].lengthM && near(*poles[2].lengthM, 123.4, 1e-9));
    CHECK(poles[4].lengthM && near(*poles[4].lengthM, 250.0, 1e-9));

    // En sträcka utan spannlängder räknas helt ur koordinaterna: 500 och 300 m ger aₙ = √(Σa³/Σa).
    std::vector<PoleRow> line = {
        {"P1", "", "", "", "0.00", "0.00", "", ""},
        {"P2", "", "", "", "300.00", "400.00", "", ""},
        {"P3", "", "", "", "300.00", "700.00", "", ""},
    };
    const RangeSpan span = rangeSpan(line, {"P1", "P3"}, "faslinan", kTable);
    CHECK(span.calc.result.has_value());
    if (span.calc.result)
        CHECK(near(span.calc.result->anExact, std::sqrt((125e6 + 27e6) / 800.0), 1e-6));
}

// Normalspannet för sträckorna i bilaga A före och efter ombyggnad, och en sträcka med fel.
void testRangeSpan()
{
    const std::vector<PoleRow> before = appendixA();

    const RangeSpan phase = rangeSpan(before, {"ST0102", "ST0107"}, "faslinan", kTable);
    CHECK(phase.rows.issue.empty());
    CHECK(phase.rows.first == 2 && phase.rows.last == 7);
    CHECK(phase.poles.size() == 6);
    CHECK(phase.calc.result.has_value());
    CHECK(phase.calc.result->spanCount == 5);
    CHECK(near(phase.calc.result->totalM, 1560));
    CHECK(near(phase.calc.result->an, 315.2));
    CHECK(near(phase.calc.result->anExact, 315.1557, 1e-3));
    CHECK(phase.calc.result->anDeltaH.has_value());

    const RangeSpan top = rangeSpan(before, {"ST0101", "ST0108"}, "topplinorna", kTable);
    CHECK(top.calc.result && near(top.calc.result->an, 313.8));
    CHECK(top.calc.result && near(top.calc.result->totalM, 2180));

    const RangeSpan all = rangeSpan(before, {"ST0100", "ST0109"}, "faslinan", kTable);
    CHECK(all.calc.result && near(all.calc.result->an, 308.3));
    CHECK(all.calc.result && near(all.calc.result->totalM, 2750));
    CHECK(all.calc.result && all.calc.result->spanCount == 9);

    const std::vector<PoleRow> after = appendixAfter();
    const RangeSpan afterPhase = rangeSpan(after, {"ST0102", "ST0107"}, "faslinan", "Stolptabell efter");
    CHECK(afterPhase.calc.result && near(afterPhase.calc.result->an, 316.8));
    CHECK(afterPhase.calc.result && near(afterPhase.calc.result->totalM, 1560));
    const RangeSpan afterTop = rangeSpan(after, {"ST0101", "ST0108"}, "topplinorna", "Stolptabell efter");
    CHECK(afterTop.calc.result && near(afterTop.calc.result->an, 315.0));
    CHECK(afterTop.calc.result && near(afterTop.calc.result->totalM, 2180));

    // En sträcka med fel ger inga stolpar och bara sträckans fel.
    const RangeSpan bad = rangeSpan(before, {"ST0102", "ST0199"}, "faslinan", kTable);
    CHECK(!bad.calc.result);
    CHECK(bad.poles.empty());
    CHECK(bad.rows.first == -1 && bad.rows.last == -1);
    CHECK(bad.calc.issues.size() == 1);
    CHECK(bad.calc.issues[0].kind == ancalc::IssueKind::Error);
    CHECK(bad.calc.issues[0].poleIndex == -1);
    CHECK(bad.calc.issues[0].column == ancalc::SpanColumn::None);
    CHECK(bad.calc.issues[0].message == "Stolpnr ST0199 finns inte i Stolptabell före.");
    CHECK(bad.rows.issue == bad.calc.issues[0].message);

    // Gränserna används.
    ancalc::SpanLimits tight;
    tight.longest = 1.0;
    const RangeSpan limited = rangeSpan(before, {"ST0102", "ST0107"}, "faslinan", kTable, tight);
    CHECK(limited.calc.result && limited.calc.result->tooLongCount > 0);
}

// Panelens text: resultatet, avvikelser och felen med Stolpnr eller radnummer.
void testRangeStatus()
{
    const std::vector<PoleRow> rows = appendixA();
    const RangeSpan phase = rangeSpan(rows, {"ST0102", "ST0107"}, "faslinan", kTable);
    const RangeStatus ok = rangeStatus(phase, rows);
    CHECK(ok.ok);
    CHECK(ok.text.rfind("5 spann · aₙ 315,2 m", 0) == 0);
    CHECK(ok.text.find(" · aₙ,ΔH ") != std::string::npos);
    CHECK(ok.text.find("avvikelse") == std::string::npos);

    // Utan H på alla stolpar finns inget aₙ,ΔH.
    std::vector<PoleRow> noHeight = appendixA();
    noHeight[4].height.clear();
    const RangeStatus plain = rangeStatus(rangeSpan(noHeight, {"ST0102", "ST0107"}, "faslinan", kTable), noHeight);
    CHECK(plain.ok);
    CHECK(plain.text == "5 spann · aₙ 315,2 m");

    // Avvikelser: ett mycket långt spann ger minst en.
    std::vector<PoleRow> deviating = appendixA();
    deviating[4].spanM = "1000";
    const RangeSpan dev = rangeSpan(deviating, {"ST0102", "ST0107"}, "faslinan", kTable);
    CHECK(dev.calc.result.has_value());
    if (dev.calc.result) {
        const ancalc::SpanResult& r = *dev.calc.result;
        const int count = r.tooLongCount + r.tooShortCount + r.adjacentCount + r.localLowCount + r.localHighCount;
        CHECK(count >= 1);
        const std::string expected = count == 1 ? " · 1 avvikelse" : " · " + std::to_string(count) + " avvikelser";
        const std::string text = rangeStatus(dev, deviating).text;
        CHECK(text.size() >= expected.size());
        CHECK(text.size() >= expected.size() && text.compare(text.size() - expected.size(), expected.size(), expected) == 0);
    }

    // Fel med Stolpnr.
    std::vector<PoleRow> missing = appendixA();
    missing[5].spanM.clear();
    const RangeStatus err = rangeStatus(rangeSpan(missing, {"ST0102", "ST0107"}, "faslinan", kTable), missing);
    CHECK(!err.ok);
    CHECK(err.text == "ST0105: Spannlängd saknas.");

    // Fel utan Stolpnr: radnumret, 1-baserat i tabellen.
    std::vector<PoleRow> unnamed = appendixA();
    unnamed[5].pole.clear();
    unnamed[5].spanM.clear();
    const RangeSpan unnamedSpan = rangeSpan(unnamed, {"ST0102", "ST0107"}, "faslinan", kTable);
    const RangeStatus unnamedErr = rangeStatus(unnamedSpan, unnamed);
    CHECK(!unnamedErr.ok);
    CHECK(unnamedErr.text == "Rad 6: Spannlängd saknas.");

    // Sträckans eget fel visas som det är.
    const RangeStatus range = rangeStatus(rangeSpan(rows, {"ST0102", ""}, "faslinan", kTable), rows);
    CHECK(!range.ok);
    CHECK(range.text == "Välj var faslinan börjar och slutar.");
}

// Dubbletterna i första förekomstens ordning, och tom Stolpnr räknas inte.
void testDuplicatePoles()
{
    CHECK(duplicatePoles(appendixA()).empty());
    std::vector<PoleRow> rows;
    for (const char* p : {"A", "B", "B", "A", "C", "", "", "A"})
        rows.push_back(poleRow(p, "100", "1"));
    const std::vector<std::string> dups = duplicatePoles(rows);
    CHECK(dups.size() == 2);
    CHECK(dups.size() == 2 && dups[0] == "A" && dups[1] == "B");
    // Trimmade jämförelser, men stora och små bokstäver skiljs.
    CHECK(duplicatePoles({poleRow("X", "", ""), poleRow(" X ", "", "")}).size() == 1);
    CHECK(duplicatePoles({poleRow("x", "", ""), poleRow("X", "", "")}).empty());
}

// Alla fem lägena, jämförelse som tal, exakt text och osäkra rader.
void testCompareAfter()
{
    std::vector<PoleRow> before = appendixA();
    before[1].height = "44.1";
    before[2].lkl = "262/262";
    before[9].lkl = "262/262";

    std::vector<PoleRow> after = asAfter(before);
    after[1].height = "44.10";                              // 44,10 mot 44,1: lika
    after[2].spanM = "999";                                 // ändrad spannlängd
    after[3].lkl = "255/262";                               // ändrad text, tom i före
    after[4].spanM = "350.0";                               // lika
    after[0].spanM = "0";                                   // "" mot "0" skiljer sig
    after[5].pole = "st0105";                               // annan skiftlägeskombination: ny
    after[6].pole.clear();                                  // saknar Stolpnr men har andra fält: ny
    after[7] = PoleRow{};                                   // helt tom
    after[8].pole = "ST0200";                               // finns inte i före: ny
    after.push_back(before[9]);                             // som i före
    after.back().lkl = "262/262 ";                          // text jämförs exakt

    const std::vector<RowCompare> cmp = compareAfter(before, after);
    CHECK(cmp.size() == after.size());

    CHECK(cmp[1].state == RowState::Same);
    CHECK(cmp[1].beforeRow == 1);
    CHECK(cmp[1].changed == std::array<bool, poletable::kFieldCount>{});

    CHECK(cmp[2].state == RowState::Changed);
    CHECK(cmp[2].beforeRow == 2);
    CHECK(cmp[2].changed[poletable::kSpan]);
    CHECK(!cmp[2].changed[poletable::kPole] && !cmp[2].changed[poletable::kHeight] && !cmp[2].changed[poletable::kLkl]);

    CHECK(cmp[3].state == RowState::Changed);
    CHECK(cmp[3].changed[poletable::kLkl]);
    CHECK(!cmp[3].changed[poletable::kSpan]);

    CHECK(cmp[4].state == RowState::Same);

    CHECK(cmp[0].state == RowState::Changed);
    CHECK(cmp[0].changed[poletable::kSpan]);

    CHECK(cmp[5].state == RowState::New);
    CHECK(cmp[5].beforeRow == -1);
    CHECK(cmp[6].state == RowState::New);
    CHECK(cmp[7].state == RowState::Empty);
    CHECK(cmp[8].state == RowState::New);

    // Texten jämförs exakt: ett blanktecken sist skiljer, men bara i texten.
    CHECK(cmp[10].state == RowState::Changed);
    CHECK(cmp[10].changed[poletable::kLkl]);
    CHECK(cmp[10].beforeRow == 9);

    // Före har ST0103 två gånger: raden i efter är osäker.
    std::vector<PoleRow> dupBefore = appendixA();
    dupBefore[8].pole = "ST0103";
    const std::vector<RowCompare> amb = compareAfter(dupBefore, asAfter(appendixA()));
    CHECK(amb[3].state == RowState::Ambiguous);
    CHECK(amb[3].beforeRow == -1);
    CHECK(amb[2].state == RowState::Same);
    CHECK(amb[8].state == RowState::New);

    // Av Fas2 och Fas3 jämförs med Av i före: lika som tal är lika, en tom fas skiljer sig.
    std::vector<PoleRow> phases = asAfter(appendixA());
    phases[1].av2 = "300.0";
    phases[2].av3.clear();
    phases[3].av2 = "301";
    const std::vector<RowCompare> ph = compareAfter(appendixA(), phases);
    CHECK(ph[1].state == RowState::Same);
    CHECK(ph[2].state == RowState::Changed && ph[2].changed[poletable::kAv3] && !ph[2].changed[poletable::kAv]);
    CHECK(ph[3].state == RowState::Changed && ph[3].changed[poletable::kAv2] && !ph[3].changed[poletable::kAv3]);

    // Tom före och tom efter.
    CHECK(compareAfter({}, {}).empty());
    const std::vector<RowCompare> allNew = compareAfter({}, appendixA());
    CHECK(allNew.size() == 10 && allNew[0].state == RowState::New && allNew[9].state == RowState::New);
}

// Noter på start- och slutrader, två noter på samma rad, fel i cellerna och dubbletter.
void testTableMarks()
{
    const std::vector<PoleRow> rows = appendixA();
    const std::vector<NamedRange> ranges = {{"faslinan", {"ST0102", "ST0107"}}, {"topplinorna", {"ST0107", "ST0108"}}};
    const TableMarks marks = tableMarks(rows, ranges, kTable);
    CHECK(marks.rowNotes.size() == rows.size());
    CHECK(marks.issues.empty());
    CHECK(marks.rowNotes[2] == "Faslinan börjar här.");
    CHECK(marks.rowNotes[7] == "Faslinan slutar här." + kNl + "Topplinorna börjar här.");
    CHECK(marks.rowNotes[8] == "Topplinorna slutar här.");
    CHECK(marks.rowNotes[0].empty() && marks.rowNotes[3].empty() && marks.rowNotes[9].empty());

    // Normalspannets fel hamnar i rätt cell med linans ord.
    std::vector<PoleRow> missing = appendixA();
    missing[5].spanM.clear();
    const TableMarks err = tableMarks(missing, {{"faslinan", {"ST0102", "ST0107"}}}, kTable);
    CHECK(err.issues.size() == 1);
    CHECK(err.issues.size() == 1 && err.issues[0] == CellIssue{5, poletable::kSpan, "Faslinan: Spannlängd saknas."});
    CHECK(err.rowNotes[2] == "Faslinan börjar här.");

    // En varning (H saknas) och fel utanför sträckan markeras inte.
    std::vector<PoleRow> warn = appendixA();
    warn[3].height.clear();
    warn[8].spanM.clear();
    CHECK(tableMarks(warn, {{"faslinan", {"ST0102", "ST0107"}}}, kTable).issues.empty());

    // Dubbletter markeras också utan sträckor.
    std::vector<PoleRow> dup = appendixA();
    dup[8].pole = "ST0103";
    const TableMarks dups = tableMarks(dup, {}, kTable);
    CHECK(dups.rowNotes.size() == dup.size());
    CHECK(dups.issues.size() == 2);
    const std::string text = "Stolpnr ST0103 finns flera gånger i Stolptabell före.";
    CHECK(dups.issues.size() == 2 && dups.issues[0] == CellIssue{3, poletable::kPole, text});
    CHECK(dups.issues.size() == 2 && dups.issues[1] == CellIssue{8, poletable::kPole, text});

    // En sträcka med fel ger inga markeringar.
    const TableMarks bad = tableMarks(rows, {{"faslinan", {"ST0199", "ST0107"}}}, kTable);
    CHECK(bad.issues.empty());
    for (const std::string& note : bad.rowNotes)
        CHECK(note.empty());
}

// Återställningen av en cell: bara en Changed-rad och en kolumn som skiljer sig, aldrig Stolpnr.
void testRestoreFromBefore()
{
    using namespace kalkyl::poletable;
    const std::vector<PoleRow> before = appendixA();
    std::vector<PoleRow> after = asAfter(before);
    after[2].spanM = "999";
    after[2].height = "45.00";   // lika som tal
    after[4].av = "301";
    after[5].av3 = "12";         // Av Fas3 återställs ur Av
    after[0].spanM = "0";        // före har tom
    after[8].pole = "ST0200";    // ny
    const std::vector<RowCompare> cmp = compareAfter(before, after);

    const std::optional<Edit> span = restoreFromBefore(before, cmp, 2, kSpan);
    CHECK(span && span->row == 2 && span->column == kSpan && span->text == "320");
    CHECK(!restoreFromBefore(before, cmp, 2, kHeight));
    const std::optional<Edit> av = restoreFromBefore(before, cmp, 4, kAv);
    CHECK(av && av->row == 4 && av->column == kAv && av->text == "300");
    CHECK(!restoreFromBefore(before, cmp, 4, kSpan));
    const std::optional<Edit> empty = restoreFromBefore(before, cmp, 0, kSpan);
    CHECK(empty && empty->row == 0 && empty->column == kSpan && empty->text.empty());
    CHECK(!restoreFromBefore(before, cmp, 8, kSpan));
    CHECK(!restoreFromBefore(before, cmp, 1, kAh));
    const std::optional<Edit> av3 = restoreFromBefore(before, cmp, 5, kAv3);
    CHECK(av3 && av3->row == 5 && av3->column == kAv3 && av3->text == "300");
    CHECK(!restoreFromBefore(before, cmp, 5, kAv2));
    CHECK(!restoreFromBefore(before, cmp, 2, -1));
    CHECK(!restoreFromBefore(before, cmp, 2, kAv2));
    CHECK(!restoreFromBefore(before, cmp, 2, kFieldCount));
    CHECK(!restoreFromBefore(before, cmp, 99, kSpan));
    CHECK(!restoreFromBefore(before, cmp, -1, kSpan));

    // Stolpnr återställs aldrig, inte ens när en handbyggd jämförelse säger att den skiljer sig.
    RowCompare handmade;
    handmade.state = RowState::Changed;
    handmade.beforeRow = 0;
    handmade.changed[kPole] = true;
    CHECK(!restoreFromBefore(before, {handmade}, 0, kPole));

    // En osäker rad (Stolpnr flera gånger i före) kan inte återställas.
    std::vector<PoleRow> dupBefore = appendixA();
    dupBefore[8].pole = "ST0103";
    std::vector<PoleRow> dupAfter = appendixA();
    dupAfter[3].spanM = "999";
    const std::vector<RowCompare> amb = compareAfter(dupBefore, dupAfter);
    CHECK(amb[3].state == RowState::Ambiguous);
    CHECK(!restoreFromBefore(dupBefore, amb, 3, kSpan));
}

} // namespace

int main()
{
    testFindRange();
    testSpanPoles();
    testCoordinateSpans();
    testRangeSpan();
    testRangeStatus();
    testDuplicatePoles();
    testCompareAfter();
    testTableMarks();
    testRestoreFromBefore();
    if (g_failures != 0) {
        std::cerr << g_failures << " kontroller misslyckades" << std::endl;
        return 1;
    }
    std::cout << "automode: alla kontroller gick igenom" << std::endl;
    return 0;
}
