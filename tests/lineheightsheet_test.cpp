// Linhöjdsbladet (libs/lineHeightSheet) i ett dolt fönster med syntetiska provdata: modellen utan
// fönster (beginEdit, aktuell cell som följer sin rad), och sedan en bild av bladet som provas på
// färgerna i planen och skrivs till KALKYL_PROV_DIR/linhojder.png. Utan skärm svarar testet 77.

#include <glaze/glaze.h>

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <string>
#include <vector>

#include "lineHeight/line_height.h"
#include "lineHeightSheet/line_height_sheet.h"
#include "poleTable/pole_table.h"

#ifndef KALKYL_PROV_DIR
#define KALKYL_PROV_DIR "."
#endif

namespace lh = kalkyl::lineheight;
namespace pt = kalkyl::poletable;
using kalkyl::lineheightsheet::LineHeightSheet;

namespace {

int failures = 0;

// Skriver ett provresultat och räknar felen.
void check(bool ok, const char* what) {
    std::printf("%s %s\n", ok ? "OK " : "FEL", what);
    if (!ok) ++failures;
}

// Tolv rader ST0374-ST0385 med id 1-12, Av, Av Fas2 och Av Fas3 300 och H 140.
std::vector<pt::PoleRow> twelve() {
    std::vector<pt::PoleRow> rows;
    for (int i = 0; i < 12; ++i) {
        pt::PoleRow r;
        r.pole = "ST0" + std::to_string(374 + i);
        r.av = "300";
        r.av2 = "300";
        r.av3 = "300";
        r.height = "140";
        r.id = i + 1;
        rows.push_back(r);
    }
    return rows;
}

// Som twelve() men med ST0379 (rad 5, id 6) ändrad i alla tre faserna.
std::vector<pt::PoleRow> asChanged() {
    std::vector<pt::PoleRow> rows = twelve();
    rows[5].av = "273.29";
    rows[5].av2 = "273.29";
    rows[5].av3 = "270.1";
    return rows;
}

// Användarens exempel: värdena på spannen ST0378-ST0379 ({5, 6}) och ST0379-ST0380 ({6, 7}) och
// topplinans Av för ST0379.
lh::StoredHeights exampleHeights() {
    lh::StoredHeights s;
    s.spans[lh::spanKey(5, 6)] = lh::SpanHeights{"12.33", "12.33", "22.63"};
    s.spans[lh::spanKey(6, 7)] = lh::SpanHeights{"21.2", "21.2", "31.17"};
    s.topAv[6] = "265.9332";
    return s;
}

// Ett block för hand med raden id, namnet och grannarna backId och aheadId. Spannen går att skriva i.
lh::PoleBlock manualBlock(std::int64_t id, const std::string& name, std::int64_t backId, std::int64_t aheadId) {
    lh::PoleBlock b;
    b.row = static_cast<int>(id) - 1;
    b.id = id;
    b.pole = name;
    b.label = name;
    auto span = [&](lh::SpanRef& ref, std::int64_t other, const std::string& label) {
        ref.exists = true;
        ref.usable = true;
        ref.neighbourRow = static_cast<int>(other) - 1;
        ref.label = label;
        ref.key = lh::spanKey(other, id);
    };
    span(b.back, backId, "X-" + name);
    span(b.ahead, aheadId, name + "-Y");
    return b;
}

// Testfönstret: bladet fyller hela ytan.
class TestWindow : public glaze::AppWindow {
public:
    TestWindow() : glaze::AppWindow("Linhöjder", 1240, 700) {
        sheet = add<LineHeightSheet>();
        sheet->name = "sheet";
        sheet->setBorder(true);
        sheet->dock = glaze::Dock::Fill;
    }
    LineHeightSheet* sheet = nullptr;

    // Matar fönstret med en tangent respektive skrivna tecken, samma väg som OS:ets händelser.
    void key(gfx::Key k, std::uint32_t mods = 0) {
        gfx::Event e;
        e.type = gfx::EventType::KeyDown;
        e.key = k;
        e.mods = mods;
        handleEvent(e);
    }
    void type(const char* utf8) {
        gfx::Event e;
        e.type = gfx::EventType::TextInput;
        std::strncpy(e.text, utf8, sizeof(e.text) - 1);
        handleEvent(e);
    }
};

// Antalet bildpunkter med exakt färgen r, g, b i en RGBA-bild.
int countColor(const std::vector<std::uint8_t>& px, int r, int g, int b) {
    int n = 0;
    for (std::size_t i = 0; i + 3 < px.size(); i += 4)
        if (px[i] == r && px[i + 1] == g && px[i + 2] == b) ++n;
    return n;
}

// Först index i inputs, eller -1.
int firstInput(const lh::Sheet& s) { return s.inputs.empty() ? -1 : s.inputs.front(); }

// Första cellen med stilen, eller -1.
int firstWithStyle(const lh::Sheet& s, lh::CellStyle style) {
    for (std::size_t i = 0; i < s.cells.size(); ++i)
        if (s.cells[i].style == style) return static_cast<int>(i);
    return -1;
}

// Modellen utan fönster: bladet, beginEdit och den aktuella cellen som följer sin rad i ett nytt blad.
void testWithoutWindow(TestWindow& w, const lh::Sheet& sheet) {
    check(w.sheet->sheet() == sheet, "sheet() är bladet som lades in");
    check(w.sheet->currentCell() == -1, "ingen aktuell cell från början");

    const int computed = firstWithStyle(sheet, lh::CellStyle::Computed);
    check(computed >= 0 && !w.sheet->beginEdit(computed), "beginEdit på en beräkningscell svarar false");
    check(!w.sheet->beginEdit(-1) && !w.sheet->beginEdit(100000), "beginEdit utanför bladet svarar false");

    const int input = firstInput(sheet);
    check(input >= 0 && w.sheet->beginEdit(input), "beginEdit på en inmatning svarar true");
    check(w.sheet->currentCell() == input, "currentCell är den öppnade cellen");
    w.sheet->commitEdit();
    check(w.sheet->currentCell() == input, "den aktuella cellen står kvar efter commitEdit");
    check(w.sheet->lastCommit().text.empty(), "ingen sparning av en oförändrad text");

    // Två block heter ST0379 (id 6 och 7). Den aktuella cellen i det andra följer med när ett block
    // tillkommer före dem.
    lh::Sheet twins = lh::buildSheet({manualBlock(6, "ST0379", 5, 7), manualBlock(7, "ST0379", 6, 8)});
    w.sheet->setSheet(twins);
    const lh::InputRef ref{lh::InputRef::Kind::Span, lh::spanKey(6, 7), lh::Part::Outer, 0};
    const int inSecond = lh::findInput(twins, 7, ref);
    check(inSecond >= 0, "provbladet har ytterfasens spann {6, 7} i blocket med id 7");
    check(w.sheet->beginEdit(inSecond), "beginEdit i det andra ST0379-blocket");
    lh::Sheet grown = lh::buildSheet(
        {manualBlock(5, "ST0378", 4, 6), manualBlock(6, "ST0379", 5, 7), manualBlock(7, "ST0379", 6, 8)});
    w.sheet->setSheet(grown);
    const int expected = lh::findInput(grown, 7, ref);
    check(expected >= 0 && w.sheet->currentCell() == expected, "aktuell cell följer blocket med id 7");
    check(expected >= 0 && grown.cells[static_cast<std::size_t>(expected)].block == 2,
          "cellen ligger nu i det tredje blocket");

    // Försvinner cellens rad stängs redigeringen och ingen cell är aktuell.
    w.sheet->setSheet(lh::buildSheet({manualBlock(5, "ST0378", 4, 6)}));
    check(w.sheet->currentCell() == -1, "ingen aktuell cell när blocket är borta");

    // Skrivskyddat: cellen går att öppna men inget sparas.
    w.sheet->setSheet(sheet);
    w.sheet->setReadOnly(true);
    check(w.sheet->isReadOnly(), "setReadOnly");
    check(w.sheet->beginEdit(input), "en skrivskyddad inmatning går att öppna");
    w.sheet->commitEdit();
    check(w.sheet->lastCommit().text.empty(), "skrivskyddat sparar inget");
    w.sheet->setReadOnly(false);
    w.sheet->setSheet(sheet);
}

// Redigeringen i ett öppet fönster: skrivna tecken sparas med Enter och flyttar till cellen nedanför,
// Escape kastar texten, Tab sparar och flyttar till nästa inmatning, och en oförändrad text fyrar inget.
void testTyping(TestWindow& w, const lh::Sheet& sheet) {
    int commits = 0;
    w.sheet->onCommit = [&commits] { ++commits; };
    w.sheet->setSheet(sheet);
    const int first = firstInput(sheet);

    check(first >= 0 && w.sheet->beginEdit(first), "beginEdit i det öppna fönstret");
    w.type("7,5");
    w.key(gfx::Key::Return);
    const LineHeightSheet::Commit c = w.sheet->lastCommit();
    check(commits == 1, "Enter fyrar onCommit en gång");
    check(c.text == "7,5" && c.input.kind == lh::InputRef::Kind::Span && c.input.part == lh::Part::Outer &&
              c.blockRow == 5,
          "lastCommit bär texten, ytterfasens spann och blockets rad-id");
    check(w.sheet->currentCell() == lh::moveInput(sheet, first, lh::Move::Down), "Enter öppnar cellen nedanför");

    const int below = w.sheet->currentCell();
    w.type("9");
    w.key(gfx::Key::Escape);
    check(commits == 1 && w.sheet->currentCell() == below, "Escape kastar texten och behåller cellen");

    check(w.sheet->beginEdit(first), "beginEdit igen efter Escape");
    w.type("1,5");
    w.key(gfx::Key::Tab);
    check(commits == 2 && w.sheet->lastCommit().text == "1,5", "Tab sparar texten");
    check(w.sheet->currentCell() == lh::moveInput(sheet, first, lh::Move::Next), "Tab öppnar nästa inmatning");

    w.key(gfx::Key::Return);
    check(commits == 2, "en oförändrad text fyrar ingen händelse");
    w.sheet->commitEdit();
    check(commits == 2, "commitEdit på en oförändrad cell fyrar ingen händelse");

    // Skrivskyddat: texten går att skriva men sparas aldrig.
    w.sheet->setReadOnly(true);
    check(w.sheet->beginEdit(first), "en skrivskyddad cell öppnas");
    w.type("5");
    w.key(gfx::Key::Return);
    check(commits == 2, "skrivskyddat fyrar aldrig onCommit");
    w.sheet->setReadOnly(false);
    w.sheet->onCommit = nullptr;
}

} // namespace

int main() {
    std::setvbuf(stdout, nullptr, _IONBF, 0);
    // Användarens exempel: tre block ST0378, ST0379 och ST0380.
    const std::vector<pt::PoleRow> before = twelve();
    const std::vector<pt::PoleRow> after = asChanged();
    const std::vector<lh::PoleBlock> blocks = lh::buildBlocks(before, after, exampleHeights());
    check(blocks.size() == 3, "tre block för användarens exempel");
    const lh::Sheet sheet = lh::buildSheet(blocks);
    check(sheet.blockStart.size() == 3 && !sheet.cells.empty(), "bladet har tre block med celler");

    TestWindow w;
    w.sheet->setSheet(sheet);
    testWithoutWindow(w, sheet);

    // Bilden: ett dolt fönster. Utan skärm hoppas resten över.
    w.setOffscreen(true);
    w.show();
    if (!w.isOpen()) {
        std::puts("ingen display - bildproven hoppas över");
        return failures ? 1 : 77;
    }
    w.window().renderFrame();
    std::vector<std::uint8_t> px;
    int pw = 0, ph = 0;
    check(w.window().captureFrame(px, pw, ph) && pw > 0 && ph > 0, "captureFrame");

    struct Wanted {
        int r, g, b;
        const char* name;
    };
    const Wanted wanted[] = {{0xBF, 0xBF, 0xBF, "grå BFBFBF"},       {0xFF, 0xE6, 0x99, "gul FFE699"},
                             {0xF8, 0xCB, 0xAD, "laxrosa F8CBAD"},   {0x00, 0xB0, 0xF0, "blå 00B0F0"},
                             {0x92, 0xD0, 0x50, "grön 92D050"},      {0xC5, 0xE0, 0xB4, "ljusgrön C5E0B4"},
                             {0xFF, 0xFF, 0xFF, "inmatningsvit FFFFFF"}};
    for (const Wanted& c : wanted) {
        const int n = countColor(px, c.r, c.g, c.b);
        std::printf("   %s: %d bildpunkter\n", c.name, n);
        check(n >= 50, c.name);
    }

    std::error_code ec;
    std::filesystem::create_directories(KALKYL_PROV_DIR, ec);
    const std::string path = std::string(KALKYL_PROV_DIR) + "/linhojder.png";
    check(gfx::encodePng(path, px, pw, ph), "linhojder.png skrevs");

    // En bild med textfältet öppet på ytterfasens första spann och ett skrivet värde i fönstret.
    const int input = firstInput(sheet);
    if (input >= 0 && w.sheet->beginEdit(input)) {
        w.window().renderFrame();
        std::vector<std::uint8_t> px2;
        int pw2 = 0, ph2 = 0;
        if (w.window().captureFrame(px2, pw2, ph2))
            gfx::encodePng(std::string(KALKYL_PROV_DIR) + "/linhojder_redigerar.png", px2, pw2, ph2);
    }
    check(w.sheet->currentCell() == input, "aktuell cell i det öppna fönstret");
    testTyping(w, sheet);
    return failures ? 1 : 0;
}
