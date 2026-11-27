#pragma once
#include <string>
#include <vector>

#include "poleTable/pole_table.h"

namespace kalkyl::poleview {

// Filtret i Stolptabell efter. Changed är Förändrade ±2: de förändrade raderna och kMargin rader
// runt dem syns, resten står ihop i grå rader. All visar alla rader.
enum class Filter { Changed, All };

// Antalet rader ovanför och nedanför en förändrad rad som också syns (±2).
inline constexpr int kMargin = 2;

// Vad en rad i vyn står för: en rad i tabellen, en grå rad för dolda rader eller den tomma raden sist.
enum class ViewKind { Pole, Hidden, Blank };

// En rad i vyn. Pole: first = last = raden i tabellen. Hidden: de dolda raderna first..last, minst
// två. Blank: first = last = antalet rader i tabellen.
struct ViewRow {
    ViewKind kind = ViewKind::Pole;
    int first = 0;
    int last = 0;
    bool operator==(const ViewRow&) const = default;
};

// Vyn: raderna i tabellens ordning och alltid en Blank sist.
struct PoleView {
    std::vector<ViewRow> rows;
    bool operator==(const PoleView&) const = default;
};

// Raderna i after närmast platsen där en stolpe i before saknas i after (planen 5.2): för varje
// rad i before med Stolpnr som inte finns i after, närmaste rad ovanför och nedanför i before vars
// Stolpnr finns på exakt en rad i after, och den raden i after. Sorterade, utan dubbletter.
std::vector<int> removedAnchors(const std::vector<poletable::PoleRow>& before,
                                const std::vector<poletable::PoleRow>& after);

// Antalet rader i before med Stolpnr (trimmad, inte tom) som inte finns på någon rad i after.
int removedCount(const std::vector<poletable::PoleRow>& before, const std::vector<poletable::PoleRow>& after);

// En post per rad i after: sant när raden inte är Same i automode::compareAfter (Changed, New,
// Empty och Ambiguous) eller står i removedAnchors.
std::vector<bool> filterMarks(const std::vector<poletable::PoleRow>& before,
                              const std::vector<poletable::PoleRow>& after);

// Första och sista sanna posten i marks, eller {-1, -1}.
struct MarkedRange {
    int first = -1;
    int last = -1;
    bool operator==(const MarkedRange&) const = default;
};
MarkedRange markedRange(const std::vector<bool>& marks);

// Vyn med en Pole per rad och Blank sist.
PoleView identityView(int count);

// Vyn för count rader (planen 5.2). Changed visar ett enda område, från första sanna posten i marks
// minus kMargin till sista plus kMargin, och raderna före och efter blir var sin Hidden när de är
// minst två. All, eller ingen sann post i marks, ger identityView. Poster bortom count ignoreras och
// saknade poster är falska.
PoleView buildView(int count, const std::vector<bool>& marks, Filter filter);

// Samma vy för en tabell med count rader: rader bortom count tas bort, en Hidden klipps (en enda
// rad kvar blir Pole), nya rader blir Pole före Blank. Formen bevaras, så att markeringen stämmer.
PoleView resizeView(const PoleView& view, int count);

// Antalet rader i tabellen: Blank.first (0 för en tom vy).
int rowCount(const PoleView& view);

// Tabellens rad för vyns rad v. Pole ger raden, Hidden och v < 0 ger -1. Blank och raderna under
// den ger rowCount + (v - blankindex), alltså nya rader vid en inklistring.
int modelRow(const PoleView& view, int v);

// Vyns rad för tabellens rad m: Pole-raden, Hidden-raden som döljer den, eller Blank för m >= rowCount
// och m < 0 ger -1.
int viewRowOf(const PoleView& view, int m);

// Vyns rad för m som inte är Hidden: Pole-raden, annars raden efter den grå (step +1) eller före den
// (step -1, och efter när den grå står först). -1 för m < 0.
int visibleViewRow(const PoleView& view, int m, int step);

// Sant när vyns rad v är en Hidden-rad. Falskt utanför vyn.
bool isHiddenRow(const PoleView& view, int v);

// Radens namn i texterna: trimmad Stolpnr, eller "rad {m + 1}" när den är tom.
std::string rowLabel(const std::vector<poletable::PoleRow>& rows, int m);

// Den grå radens text och tooltip (planen 7.1).
std::string hiddenText(const std::vector<poletable::PoleRow>& rows, const ViewRow& hidden);
std::string hiddenToolTip(const ViewRow& hidden);

// Var vyn ska rullas, i logiska px, för att visa vyns rader first..last (planen K9): mitten av
// området mitt i synfältet när det ryms, annars first överst. Klampat till 0..(viewRows * rowHeight
// - viewportHeight). rowHeight <= 0 eller viewportHeight <= 0 ger 0.
float scrollTop(int first, int last, int viewRows, float rowHeight, float viewportHeight);

// Siffrorna i infotexten. rows: raderna i after. changed: raderna som inte är Same. marked: sanna
// poster i filterMarks. removed: removedCount. shown: Pole-raderna i buildView med filtret.
struct Summary {
    int rows = 0;
    int changed = 0;
    int marked = 0;
    int removed = 0;
    int shown = 0;
    bool operator==(const Summary&) const = default;
};
Summary summarize(const std::vector<poletable::PoleRow>& before, const std::vector<poletable::PoleRow>& after,
                  Filter filter);

// Infotexten bredvid Visa förändrade (planen 7.2).
std::string summaryText(const Summary& summary, Filter filter);

} // namespace kalkyl::poleview
