#pragma once
#include <array>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "anCalc/normal_span.h"
#include "poleTable/pole_table.h"

namespace kalkyl::automode {

// En sträcka i en stolptabell: Stolpplats för första och sista stolpen, trimmade.
// Tom text = stolpen är inte vald.
struct PoleRange {
    std::string fromPole;
    std::string toPole;
    bool operator==(const PoleRange&) const = default;
};

// Var sträckan ligger i tabellen: första och sista raden, 0-baserade och båda med. issue är
// felet när sträckan inte går att använda (planen 7.1), och då är first och last -1.
struct RangeRows {
    int first = -1;
    int last = -1;
    std::string issue;
};

// Söker sträckan bland raderna. word är linans ord i texterna ("faslinan", "topplinorna",
// "topplina vänster", "topplina höger") och table tabellens namn ("Stolptabell före").
// Stolpplats jämförs exakt efter trimning. Felen prövas i ordningen i planen 7.1.
RangeRows findRange(const std::vector<poletable::PoleRow>& rows, const PoleRange& range,
                    std::string_view word, std::string_view table);

// Spannlängd föreg. räknad ur koordinaterna, en post per rad. En rad med tom spannlängd får det
// vågräta avståndet √(ΔN² + ΔE²) till närmast föregående rad som inte är helt tom, när båda har
// Koordinat N och E, avrundat till två decimaler. En ifylld spannlängd, en helt tom rad,
// tabellens första stolpe (som inte har någon föregående) och avståndet 0 (samma punkt två
// gånger) ger inget värde.
std::vector<std::optional<double>> coordinateSpans(const std::vector<poletable::PoleRow>& rows);

// Raderna first..last som normalspannets stolpar, i tabellens ordning: Stolpplats som Stpnr,
// Spannlängd föreg. som spannlängd och Koordinat H som H (poletable::numberValue). En tom
// spannlängd tas ur koordinaterna (coordinateSpans) när det går, och andra tomma tal blir tomma.
// first < 0, last utanför tabellen eller first > last ger en tom lista.
std::vector<ancalc::SpanPole> spanPoles(const std::vector<poletable::PoleRow>& rows, int first, int last);

// Normalspannet för en sträcka: var den ligger, stolparna och beräkningen. Har sträckan fel är
// poles tom, och calc har inget resultat och bara sträckans fel (Error, poleIndex -1).
struct RangeSpan {
    RangeRows rows;
    std::vector<ancalc::SpanPole> poles;
    ancalc::SpanCalculation calc;
};
RangeSpan rangeSpan(const std::vector<poletable::PoleRow>& rows, const PoleRange& range,
                    std::string_view word, std::string_view table,
                    const ancalc::SpanLimits& limits = {});

// Panelens text för en sträcka (planen 7.2): "5 spann · aₙ 315,2 m" med aₙ,ΔH och avvikelser
// när de finns, eller det första felet med stolpens Stolpplats ("ST0105: Spannlängd saknas.").
struct RangeStatus {
    bool ok = false;
    std::string text;
};
RangeStatus rangeStatus(const RangeSpan& span, const std::vector<poletable::PoleRow>& rows);

// Texten med första bokstaven som versal när den är en ASCII-bokstav, till början av en mening:
// linans ord "faslinan" blir "Faslinan". Delas av autoMode, poleImport och huvudfönstret.
std::string capitalized(std::string_view text);

// Antalet spann med avvikelse i kontrollerna 1-5 (summan av de fem antalen), för panelens text
// och kolumnen Avvikelser i fliken Normalspann.
int deviationCount(const ancalc::SpanResult& r);

// Stolpplatserna som finns på mer än en rad, i första förekomstens ordning. Tom Stolpplats räknas inte.
std::vector<std::string> duplicatePoles(const std::vector<poletable::PoleRow>& rows);

// Hur en rad i Stolptabell efter förhåller sig till Stolptabell före, matchad på Stolpplats.
enum class RowState {
    Empty,       // helt tom rad
    Same,        // Stolpplats finns på en rad i före, och alla fält är lika
    Changed,     // Stolpplats finns på en rad i före, men minst ett fält skiljer sig
    New,         // Stolpplats finns inte i före, eller raden saknar Stolpplats
    Ambiguous,   // Stolpplats finns på flera rader i före
};

// Jämförelsen för en rad: läget, raden i före (-1 utan en enda träff) och fälten som skiljer sig.
struct RowCompare {
    RowState state = RowState::Empty;
    int beforeRow = -1;
    std::array<bool, poletable::kFieldCount> changed{};
    bool operator==(const RowCompare&) const = default;
};

// En jämförelse per rad i after, för alla tio fälten. Av Fas2 och Fas3 jämförs med Av i före
// (poletable::beforeField). Talfälten jämförs som tal (44,10 och 44,1 är lika, och tomt är bara
// lika med tomt), textfälten exakt.
std::vector<RowCompare> compareAfter(const std::vector<poletable::PoleRow>& before,
                                     const std::vector<poletable::PoleRow>& after);

// En sträcka med linans ord, till markeringarna i tabellen. word börjar med en ASCII-bokstav,
// som tableMarks gör versal i början av en mening ("Faslinan börjar här.").
struct NamedRange {
    std::string word;
    PoleRange range;
};

// Ett fel i en cell: raden, fältet 0-9 och texten.
struct CellIssue {
    int row = -1;
    int column = -1;
    std::string message;
    bool operator==(const CellIssue&) const = default;
};

// Markeringarna i en stolptabell: per rad en text om de sträckor som börjar eller slutar där
// (tom = ingen), och felen per cell, alltså dubbletter av Stolpplats och normalspannets fel i en sträcka.
struct TableMarks {
    std::vector<std::string> rowNotes;
    std::vector<CellIssue> issues;
};

// Markeringarna för raderna och sträckorna (planen 7.3). Utan sträckor blir det bara dubbletterna.
// En sträcka med fel ger inga markeringar, eftersom felet visas i panelen.
TableMarks tableMarks(const std::vector<poletable::PoleRow>& rows, const std::vector<NamedRange>& ranges,
                      std::string_view table);

// Ändringen som skriver tillbaka värdet ur Stolptabell före i cellen row, column i Stolptabell efter:
// bara när raden är Changed och fältet skiljer sig (compare[row].changed), med före-cellens text som
// den står (Av för Av Fas2 och Fas3). Tomt svar för Stolpnr (kPole), för ett fält utanför 0-9, för en
// rad utanför compare och när cellen inte skiljer sig. compare är compareAfter(before, efter).
std::optional<poletable::Edit> restoreFromBefore(const std::vector<poletable::PoleRow>& before,
                                                 const std::vector<RowCompare>& compare, int row, int column);

} // namespace kalkyl::automode
