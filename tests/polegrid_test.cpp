// Test av PoleGrids rena del, cellLook: radens ton, jämförelsen mot Stolptabell före, felen och
// noterna. Själva tabellen kräver ett fönster och provas med glaze-render och handtest.
#include <iostream>
#include <string>
#include <vector>

#include "poleGrid/pole_grid.h"

using namespace kalkyl;
namespace pt = kalkyl::poletable;
using automode::RowCompare;
using automode::RowState;
using polegrid::cellLook;
using polegrid::CellLook;
using polegrid::PoleLooks;

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

// Radbrytningen som tecken, eftersom källan inte får innehålla escape-sekvenser.
const std::string kLf(1, char{10});

// Sant när cellens bakgrund finns och är exakt färgen.
bool hasColor(const CellLook& look, gfx::Color color)
{
    return look.background && *look.background == color;
}

// Före-raden med spannlängden 350 som jämförelserna pekar på.
PoleLooks changedLooks()
{
    PoleLooks looks;
    pt::PoleRow before;
    before.pole = "ST0104";
    before.spanM = "350";
    looks.before = {before};
    RowCompare cmp;
    cmp.state = RowState::Changed;
    cmp.beforeRow = 0;
    cmp.changed[pt::kSpan] = true;
    looks.compare = {cmp, RowCompare{RowState::Changed, 0, {}}};
    return looks;
}

// Före har bara den gula tonen med alfa 64 på jämna och 52 på udda rader, också på den tomma
// raden sist, och inga tooltips eller flaggor.
void testBefore()
{
    const PoleLooks looks;
    for (int c = 0; c < pt::kColumnCount; ++c) {
        CHECK(hasColor(cellLook(looks, 0, c, 3), gfx::Color{220, 160, 40, 64}));
        CHECK(hasColor(cellLook(looks, 1, c, 3), gfx::Color{220, 160, 40, 52}));
        CHECK(cellLook(looks, 0, c, 3).toolTip.empty());
        CHECK(!cellLook(looks, 0, c, 3).flag);
    }
    CHECK(hasColor(cellLook(looks, 3, 0, 3), gfx::Color{220, 160, 40, 52}));   // den tomma raden
    CHECK(hasColor(cellLook(looks, 4, 2, 4), gfx::Color{220, 160, 40, 64}));
    CHECK(cellLook(looks, 3, 0, 3).toolTip.empty());
}

// En ändrad rad är rosa. Den ändrade cellen har alfa 130 och visar värdet i före, och de andra
// cellerna har bara radens ton. Är cellen tom i före står "tom".
void testChanged()
{
    PoleLooks looks = changedLooks();
    CHECK(hasColor(cellLook(looks, 0, pt::kPole, 2), gfx::Color{210, 90, 140, 64}));
    CHECK(hasColor(cellLook(looks, 1, pt::kPole, 2), gfx::Color{210, 90, 140, 52}));
    const CellLook cell = cellLook(looks, 0, pt::kSpan, 2);
    CHECK(hasColor(cell, gfx::Color{210, 90, 140, 130}));
    CHECK(cell.toolTip == "I Stolptabell före: 350");
    CHECK(!cell.flag);
    CHECK(cellLook(looks, 0, pt::kHeight, 2).toolTip.empty());
    CHECK(hasColor(cellLook(looks, 0, pt::kHeight, 2), gfx::Color{210, 90, 140, 64}));

    looks.before[0].spanM.clear();
    CHECK(cellLook(looks, 0, pt::kSpan, 2).toolTip == "I Stolptabell före: tom");
}

// En ny rad är grön. Stolpnren har tooltipen "Finns inte i Stolptabell före.", eller den om
// att raden saknar Stolpnr, och de andra cellerna har ingen tooltip.
void testNew()
{
    PoleLooks looks;
    looks.compare = {RowCompare{RowState::New, -1, {}}};
    CHECK(hasColor(cellLook(looks, 0, pt::kPole, 1, "ST0110"), gfx::Color{70, 170, 90, 64}));
    CHECK(hasColor(cellLook(looks, 0, pt::kSpan, 1, "ST0110"), gfx::Color{70, 170, 90, 64}));
    CHECK(cellLook(looks, 0, pt::kPole, 1, "ST0110").toolTip == "Finns inte i Stolptabell före.");
    CHECK(cellLook(looks, 0, pt::kPole, 1, "").toolTip ==
          "Raden saknar Stolpnr och kan inte jämföras med Stolptabell före.");
    CHECK(cellLook(looks, 0, pt::kSpan, 1, "ST0110").toolTip.empty());
}

// En osäker rad har radens gula ton, men Stolpnren får fel-färgen och en förklaring.
void testAmbiguous()
{
    PoleLooks looks;
    looks.compare = {RowCompare{RowState::Ambiguous, -1, {}}};
    const CellLook pole = cellLook(looks, 0, pt::kPole, 1, "ST0104");
    CHECK(hasColor(pole, gfx::Color{220, 60, 60, 80}));
    CHECK(pole.toolTip == "Stolpnr ST0104 finns flera gånger i Stolptabell före och kan inte jämföras.");
    CHECK(hasColor(cellLook(looks, 0, pt::kSpan, 1, "ST0104"), gfx::Color{220, 160, 40, 64}));
}

// Som i före och en tom rad är gula utan tooltip.
void testSameAndEmpty()
{
    PoleLooks looks;
    looks.compare = {RowCompare{RowState::Same, 0, {}}, RowCompare{RowState::Empty, -1, {}}};
    CHECK(hasColor(cellLook(looks, 0, pt::kPole, 2, "ST0100"), gfx::Color{220, 160, 40, 64}));
    CHECK(hasColor(cellLook(looks, 1, pt::kSpan, 2), gfx::Color{220, 160, 40, 52}));
    CHECK(cellLook(looks, 0, pt::kPole, 2, "ST0100").toolTip.empty());
}

// Ett fel i en cell vinner över färgen på en ändrad cell, och tooltipen har felet först och
// jämförelsen efter. Cellens rad behåller sin ton.
void testIssueWins()
{
    PoleLooks looks = changedLooks();
    looks.marks.issues = {{0, pt::kSpan, "Faslinan: Spannlängd saknas."}};
    const CellLook cell = cellLook(looks, 0, pt::kSpan, 2);
    CHECK(hasColor(cell, gfx::Color{220, 60, 60, 80}));
    CHECK(cell.toolTip == "Faslinan: Spannlängd saknas." + kLf + "I Stolptabell före: 350");
    CHECK(hasColor(cellLook(looks, 0, pt::kHeight, 2), gfx::Color{210, 90, 140, 64}));
    CHECK(hasColor(cellLook(looks, 1, pt::kSpan, 2), gfx::Color{210, 90, 140, 52}));   // raden utan fel

    PoleLooks plain;
    plain.marks.issues = {{1, pt::kPole, "Stolpnr ST0104 finns flera gånger i Stolptabell före."}};
    CHECK(hasColor(cellLook(plain, 1, pt::kPole, 3), gfx::Color{220, 60, 60, 80}));
    CHECK(hasColor(cellLook(plain, 1, pt::kAv, 3), gfx::Color{220, 160, 40, 52}));
}

// En not på raden ger Stolpnren flaggan och noten sist i tooltipen, efter fel och jämförelse.
void testNote()
{
    PoleLooks looks;
    looks.marks.rowNotes = {"Faslinan börjar här.", ""};
    const CellLook cell = cellLook(looks, 0, pt::kPole, 2);
    CHECK(cell.flag);
    CHECK(cell.toolTip == "Faslinan börjar här.");
    CHECK(!cellLook(looks, 0, pt::kSpan, 2).flag);
    CHECK(cellLook(looks, 0, pt::kSpan, 2).toolTip.empty());
    CHECK(!cellLook(looks, 1, pt::kPole, 2).flag);

    looks.marks.rowNotes[0] = "Faslinan börjar här." + kLf + "Topplinorna slutar här.";
    looks.marks.issues = {{0, pt::kPole, "Stolpnr ST0104 finns flera gånger i Stolptabell före."}};
    const CellLook both = cellLook(looks, 0, pt::kPole, 2);
    CHECK(both.flag);
    CHECK(both.toolTip == "Stolpnr ST0104 finns flera gånger i Stolptabell före." + kLf +
                              "Faslinan börjar här." + kLf + "Topplinorna slutar här.");
}

// Radera-kolumnen har radens ton och ingen tooltip eller flagga, också när raden är ändrad eller
// ny och när cellen bredvid har ett fel.
void testDeleteColumn()
{
    PoleLooks looks = changedLooks();
    looks.marks.issues = {{0, pt::kSpan, "fel"}};
    looks.marks.rowNotes = {"Faslinan börjar här."};
    const int kDelete = -1;   // radera-kolumnen har inget fält
    const CellLook del = cellLook(looks, 0, kDelete, 2);
    CHECK(hasColor(del, gfx::Color{210, 90, 140, 64}));
    CHECK(del.toolTip.empty());
    CHECK(!del.flag);
    CHECK(hasColor(cellLook(looks, 1, kDelete, 2), gfx::Color{210, 90, 140, 52}));
    CHECK(hasColor(cellLook(PoleLooks{}, 0, kDelete, 2), gfx::Color{220, 160, 40, 64}));

    PoleLooks added;
    added.compare = {RowCompare{RowState::New, -1, {}}};
    CHECK(hasColor(cellLook(added, 0, kDelete, 1), gfx::Color{70, 170, 90, 64}));
}

// Index utanför listorna ger bara radens grundton, så att en tabell som hunnit ändras före nästa
// setLooks inte kraschar.
void testOutOfRange()
{
    PoleLooks looks;
    looks.compare = {RowCompare{RowState::Changed, 5, {}}};
    looks.marks.rowNotes = {""};
    CHECK(hasColor(cellLook(looks, 4, pt::kPole, 6), gfx::Color{220, 160, 40, 64}));
    looks.compare[0].changed[pt::kSpan] = true;
    CHECK(cellLook(looks, 0, pt::kSpan, 6).toolTip == "I Stolptabell före: tom");   // före-raden finns inte
    CHECK(hasColor(cellLook(looks, -1, 0, 6), gfx::Color{220, 160, 40, 52}));
}

// En spannlängd ur koordinaterna: cellen Spannlängd föreg. får computed och förklaringen i
// tooltipen, andra celler och rader utan värde får det inte. Texten har två decimaler.
void testComputedSpan()
{
    PoleLooks looks;
    looks.coordinateSpans = {std::nullopt, 1234.5678};
    const CellLook span = cellLook(looks, 1, pt::kSpan, 2);
    CHECK(span.computed);
    CHECK(span.toolTip.rfind("Räknad ur koordinaterna", 0) == 0);
    CHECK(!cellLook(looks, 0, pt::kSpan, 2).computed);
    CHECK(!cellLook(looks, 1, pt::kHeight, 2).computed);
    CHECK(!cellLook(looks, 2, pt::kSpan, 2).computed);   // den tomma raden sist
    CHECK(polegrid::computedSpanText(1234.5678, false) == "1234,57");
    CHECK(polegrid::computedSpanText(312.4, false) == "312,40");
}

} // namespace

int main()
{
    testComputedSpan();
    testBefore();
    testChanged();
    testNew();
    testAmbiguous();
    testSameAndEmpty();
    testIssueWins();
    testNote();
    testDeleteColumn();
    testOutOfRange();
    if (g_failures == 0)
        std::cout << "polegrid: alla tester gick igenom" << std::endl;
    return g_failures == 0 ? 0 : 1;
}
