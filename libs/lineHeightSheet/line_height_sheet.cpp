#include "lineHeightSheet/line_height_sheet.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <functional>
#include <string>
#include <utility>

namespace kalkyl::lineheightsheet {

namespace lh = kalkyl::lineheight;

// Registreringen med det okvalificerade klassnamnet, inne i namnrymden. Typen heter som medlemsraden
// i .glazeui skriver den, så att glaze-render och GlazeBuilder hittar widgeten i registret.
GLAZE_REGISTER_WIDGET("lineheightsheet", LineHeightSheet)

namespace {

// Luften runt bladet, textfältets insteg mot cellens ram och glyfens största sida, i logiska px.
constexpr float kMargin = 8.0f;
constexpr float kEditorInset = 1.0f;
constexpr float kMarkSize = 16.0f;

// Bladets mått i logiska px: kolumnernas vänsterkanter (och bladets bredd sist), radhöjden och
// cellens luft i sidled. Kolumnbredden är förlagans teckenantal gånger siffrans bredd plus luften.
struct Metrics {
    std::array<float, lh::kSheetColumns + 1> colX{};
    float rowH = 0.0f;
    float pad = 0.0f;
};

// Räknar måtten ur UI-fonten och stilen i Ctx:t.
Metrics measureSheet(const glaze::Ctx& c) {
    Metrics m;
    m.rowH = c.style->gridRowH;
    m.pad = c.style->gridPadX;
    const float digit = glaze::Draw::measure(c, std::string("0")).w;
    float x = 0.0f;
    for (int i = 0; i < lh::kSheetColumns; ++i) {
        m.colX[static_cast<std::size_t>(i)] = x;
        x += lh::kColumnChars[static_cast<std::size_t>(i)] * digit + 2.0f * m.pad;
    }
    m.colX[lh::kSheetColumns] = x;
    return m;
}

// Modellens färg som GlazeUI-färg.
gfx::Color toColor(const lh::Rgb& rgb) { return gfx::Color{rgb.r, rgb.g, rgb.b, 255}; }

constexpr gfx::Color kBlack{0, 0, 0, 255};

// Glyfen för ett beräknat (Σ) eller länkat (Sync) värde, som i Dokumentation, i den storlek
// som ryms i raden.
glaze::ImageSource markIcon(lh::Mark mark) {
    return glaze::stockIcon(mark == lh::Mark::Sum ? glaze::StockIcon::Sum : glaze::StockIcon::Sync);
}

// Ritar en cell: fyllning, glyfen i vänsterkanten, text (fet eller vanlig, kortad med "…" när den
// inte ryms) och ram. Glyfen ritas med det ljusa temat, eftersom bladet har Excel-färgerna också i
// mörkt läge.
void drawSheetCell(const glaze::Ctx& c, const glaze::Ctx& bold, const glaze::Ctx& light, const lh::SheetCell& cell,
                   const gfx::Rectf& r, float pad) {
    const lh::CellLook look = lh::cellLook(cell.style);
    if (look.filled) glaze::Draw::rect(c, r, 0.0f, toColor(look.fill));
    if (look.mark != lh::Mark::None) {
        const float side = std::clamp(r.h - 4.0f, 0.0f, kMarkSize);
        const gfx::Rectf icon{r.x + pad, r.y + (r.h - side) / 2.0f, side, side};
        glaze::Draw::glyph(light, icon, markIcon(look.mark));
    }
    if (!cell.text.empty()) {
        const bool left = look.align == lh::Align::Left;
        const float inset = left ? pad : 2.0f;
        const gfx::Rectf textRect{r.x + inset, r.y, std::max(0.0f, r.w - inset - 2.0f), r.h};
        glaze::Draw::textElided(look.bold ? bold : c, textRect, cell.text, toColor(look.text),
                                left ? glaze::Align::Left : glaze::Align::Center);
    }
    if (look.frame != lh::Frame::None) {
        const float width = look.frame == lh::Frame::Thick ? 2.0f : 1.0f;
        glaze::Draw::frame(c, r, 0.0f, glaze::Pen{kBlack, width});
    }
}

} // namespace

// Textfältet som redigerar den aktuella cellen. En vanlig TextInput som tar Enter, Tab, Escape, Upp
// och Ner före fönstret (som egenskapslistans fält) och som sparar vid fokusförlust, postat.
class LineHeightSheet::Editor final : public glaze::TextInput {
public:
    explicit Editor(LineHeightSheet& owner) : owner_(owner) {}

    // En ny cell börjar: texten, tom ångra-historik och hela texten markerad.
    void begin(const std::string& text, bool readOnly) {
        setReadOnly(readOnly);
        setText(text);
        resetHistory();
        selectAll();
    }

protected:
    // Tangenterna som sparar och flyttar eller avbryter. Med Ctrl, Alt eller Cmd är tangenten någon
    // annans genväg (Ctrl+Tab går vidare till flikarna) och går orörd till fältet.
    bool keyDown(gfx::Key k, std::uint32_t mods) override {
        const bool shortcut = (mods & (gfx::kModCtrl | gfx::kModCmd | gfx::kModAlt)) != 0;
        const bool shift = (mods & gfx::kModShift) != 0;
        if (!shortcut) {
            if (k == gfx::Key::Tab) {
                owner_.finish(shift ? lh::Move::Previous : lh::Move::Next);
                return true;
            }
            if (k == gfx::Key::Return) {
                owner_.finish(shift ? lh::Move::Up : lh::Move::Down);
                return true;
            }
            if (k == gfx::Key::Down && !shift) {
                owner_.finish(lh::Move::Down);
                return true;
            }
            if (k == gfx::Key::Up && !shift) {
                owner_.finish(lh::Move::Up);
                return true;
            }
            if (k == gfx::Key::Escape) {
                owner_.cancelEdit();
                return true;
            }
        }
        return glaze::TextInput::keyDown(k, mods);
    }

    // Fokus bort sparar. Händelsen postas, eftersom focusLost inte får köra användarkod.
    void focusLost() override {
        glaze::TextInput::focusLost();
        owner_.endEdit(true, true);
    }

private:
    LineHeightSheet& owner_;
};

// Bladet är en Panel som klipper och rullar. Textfältet skapas en gång och göms tills en cell redigeras.
LineHeightSheet::LineHeightSheet() {
    setClip(true);
    editor_ = add<Editor>(*this);
    editor_->hide();
}

LineHeightSheet::~LineHeightSheet() = default;

// Textfältet är ett ägt barn som inte får lossas utifrån.
bool LineHeightSheet::canReleaseChild(const glaze::Widget& child) const {
    return &child != static_cast<const glaze::Widget*>(editor_);
}

const lh::Sheet& LineHeightSheet::sheet() const { return sheet_; }
bool LineHeightSheet::isReadOnly() const { return readOnly_; }
int LineHeightSheet::currentCell() const { return current_; }
const LineHeightSheet::Commit& LineHeightSheet::lastCommit() const { return lastCommit_; }
bool LineHeightSheet::focusable() const { return true; }

// Fönstret är öppet, så att det finns font och renderer att mäta med.
bool LineHeightSheet::canMeasure() const {
    return appWindow() != nullptr && appWindow()->isOpen();
}

// Gör cellen aktuell och minns dess block och inmatning, så att den kan hittas igen i ett nytt blad.
void LineHeightSheet::setCurrent(int cell) {
    current_ = cell;
    if (cell >= 0) {
        const lh::SheetCell& sc = sheet_.cells[static_cast<std::size_t>(cell)];
        currentRow_ = sheet_.blockRows[static_cast<std::size_t>(sc.block)];
        currentInput_ = sc.input;
    } else {
        currentRow_ = 0;
        currentInput_ = lh::InputRef{};
    }
}

// Byter bladet. Den aktuella cellen hittas igen på blockets rad-id och inmatning. Finns den inte kvar
// stängs en öppen redigering utan händelse, annars flyttas textfältet till cellens nya plats.
void LineHeightSheet::setSheet(lh::Sheet sheet) {
    sheet_ = std::move(sheet);
    int cur = -1;
    if (currentInput_.kind != lh::InputRef::Kind::None) cur = lh::findInput(sheet_, currentRow_, currentInput_);
    if (cur < 0) {
        if (editing_) {
            editing_ = false;
            editor_->hide();
        }
        setCurrent(-1);
    } else {
        current_ = cur;
        if (editing_ && canMeasure()) placeEditor(behaviorCtx());
    }
    invalidate();
}

// Skrivskyddat bladet: textfältet går att markera och kopiera i men inte ändra.
void LineHeightSheet::setReadOnly(bool on) {
    readOnly_ = on;
    editor_->setReadOnly(on);
}

// Cellen under en punkt (fysiska px) i bladet, eller -1. Träffen räknas i samma logiska rum som
// ritningen, med rullningen avdragen.
int LineHeightSheet::cellAtPoint(const glaze::Ctx& c, float mx, float my) const {
    if (sheet_.cells.empty()) return -1;
    const Metrics m = measureSheet(c);
    const gfx::Rectf base = resolve(c);
    const float lx = glaze::Draw::localX(c, base, mx);
    const float ly = glaze::Draw::localY(c, base, my);
    const gfx::Rectf in = innerBounds(c);
    if (lx < in.x || ly < in.y || lx >= in.x + in.w || ly >= in.y + in.h) return -1;
    const gfx::Sizef range = scrollRange(c);
    const float sx = std::clamp(scrollX(), 0.0f, range.w);
    const float sy = std::clamp(scrollY(), 0.0f, range.h);
    const float cx = lx - (in.x + kMargin - sx);
    const float cy = ly - (in.y + kMargin - sy);
    if (cx < 0.0f || cy < 0.0f || cx >= m.colX.back()) return -1;
    const auto it = std::upper_bound(m.colX.begin(), m.colX.end(), cx);
    const int column = static_cast<int>(it - m.colX.begin()) - 1;
    const int row = static_cast<int>(cy / m.rowH);
    if (row >= sheet_.rows) return -1;
    return lh::cellAt(sheet_, row, column);
}

// Lägger textfältet över den aktuella cellen, med ett pixels insteg. Platsen är relativ barnytan,
// så fältet följer rullningen. Ritningen ändras bara när något flyttade sig.
void LineHeightSheet::placeEditor(const glaze::Ctx& c) {
    if (current_ < 0 || current_ >= static_cast<int>(sheet_.cells.size())) return;
    const Metrics m = measureSheet(c);
    const lh::SheetCell& sc = sheet_.cells[static_cast<std::size_t>(current_)];
    const std::size_t col = static_cast<std::size_t>(sc.column);
    const std::size_t colEnd = static_cast<std::size_t>(sc.column + sc.columns);
    const float x = kMargin + m.colX[col] + kEditorInset;
    const float y = kMargin + static_cast<float>(sc.row) * m.rowH + kEditorInset;
    const float w = m.colX[colEnd] - m.colX[col] - 2.0f * kEditorInset;
    const float h = static_cast<float>(sc.rows) * m.rowH - 2.0f * kEditorInset;
    if (editor_->x == x && editor_->y == y && editor_->width == w && editor_->height == h) return;
    editor_->x = x;
    editor_->y = y;
    editor_->width = w;
    editor_->height = h;
    invalidate();
}

// Rullar bladet så att cellen syns. Utan öppet fönster finns inga mått och inget görs.
void LineHeightSheet::scrollToCell(int cell) {
    if (cell < 0 || !canMeasure()) return;
    const glaze::Ctx c = behaviorCtx();
    const Metrics m = measureSheet(c);
    const gfx::Rectf in = innerBounds(c);
    const gfx::Sizef range = scrollRange(c);
    const lh::SheetCell& sc = sheet_.cells[static_cast<std::size_t>(cell)];
    const float x0 = m.colX[static_cast<std::size_t>(sc.column)];
    const float x1 = m.colX[static_cast<std::size_t>(sc.column + sc.columns)];
    const float y0 = static_cast<float>(sc.row) * m.rowH;
    const float y1 = y0 + static_cast<float>(sc.rows) * m.rowH;
    float sx = std::clamp(scrollX(), 0.0f, range.w);
    float sy = std::clamp(scrollY(), 0.0f, range.h);
    if (x0 < sx) sx = x0;
    else if (x1 + 2.0f * kMargin > sx + in.w) sx = x1 + 2.0f * kMargin - in.w;
    if (y0 < sy) sy = y0;
    else if (y1 + 2.0f * kMargin > sy + in.h) sy = y1 + 2.0f * kMargin - in.h;
    setScroll(sx, sy);
}

// Öppnar textfältet på cellen: den blir aktuell, texten läggs in med allt markerat, fältet får
// fokus och cellen rullas fram.
void LineHeightSheet::openEdit(int cell) {
    setCurrent(cell);
    editing_ = true;
    editor_->begin(sheet_.cells[static_cast<std::size_t>(cell)].editText, readOnly_);
    if (canMeasure()) placeEditor(behaviorCtx());
    editor_->show();
    if (canMeasure()) editor_->focus();   // fokus kräver ett öppet fönster
    scrollToCell(cell);
    invalidate();
}

// Stänger redigeringen. Med save och en ändrad text sätts lastCommit_ och onCommit fyras, direkt eller
// postat (fokusförlust). Svarar false när en handler har rivit bladet, och då rörs inget mer.
bool LineHeightSheet::endEdit(bool save, bool posted) {
    if (!editing_) return true;
    const std::string text = editor_->text();
    const int cell = current_;
    editing_ = false;
    editor_->hide();
    invalidate();
    if (!save || readOnly_ || cell < 0 || cell >= static_cast<int>(sheet_.cells.size())) return true;
    const lh::SheetCell& sc = sheet_.cells[static_cast<std::size_t>(cell)];
    if (text == sc.editText) return true;
    lastCommit_ = Commit{sc.input, sheet_.blockRows[static_cast<std::size_t>(sc.block)], text};
    if (!posted) return fireEvent(onCommit);
    // Den postade händelsen bär sin egen nyckel och text, så en senare sparning skriver inte över den.
    postEvent([this, commit = lastCommit_] {
        lastCommit_ = commit;
        fireEvent(onCommit);
    });
    return true;
}

// Sparar och öppnar cellen som Tab, Enter eller pilarna pekar på. Målet tas fram före händelsen och
// hittas igen efteråt, eftersom en handler kan ha bytt bladet.
bool LineHeightSheet::finish(lh::Move move) {
    if (!editing_ || current_ < 0) return true;
    const int target = lh::moveInput(sheet_, current_, move);
    const lh::SheetCell& tc = sheet_.cells[static_cast<std::size_t>(target)];
    const std::int64_t targetRow = sheet_.blockRows[static_cast<std::size_t>(tc.block)];
    const lh::InputRef targetInput = tc.input;
    if (!endEdit(true, false)) return false;
    const int t = lh::findInput(sheet_, targetRow, targetInput);
    if (t >= 0) openEdit(t);
    return true;
}

// Escape: texten kastas, fältet stängs och fokus går till bladet med cellen kvar som aktuell.
void LineHeightSheet::cancelEdit() {
    if (!editing_) return;
    editing_ = false;
    editor_->hide();
    if (canMeasure()) focus();
    invalidate();
}

// Sparar en öppen redigering och öppnar cellen. Är det redan den öppna cellen görs inget.
bool LineHeightSheet::openAfterSave(int cell) {
    if (cell < 0 || cell >= static_cast<int>(sheet_.cells.size())) return false;
    const lh::SheetCell& sc = sheet_.cells[static_cast<std::size_t>(cell)];
    if (sc.input.kind == lh::InputRef::Kind::None) return false;
    if (editing_ && current_ == cell) return true;
    const std::int64_t row = sheet_.blockRows[static_cast<std::size_t>(sc.block)];
    const lh::InputRef input = sc.input;
    if (editing_ && !endEdit(true, false)) return false;
    const int t = lh::findInput(sheet_, row, input);
    if (t < 0) return false;
    openEdit(t);
    return true;
}

// Börjar redigera cellen från koden. Svarar false för en cell som inte går att redigera.
bool LineHeightSheet::beginEdit(int cell) { return openAfterSave(cell); }

// Sparar en öppen redigering nu och stänger den (AppWindow::commitEdits).
void LineHeightSheet::commitEdit() { endEdit(true, false); }

// Tooltipen för cellen under punkten, annars widgetens egen.
glaze::TrText LineHeightSheet::toolTipAt(const glaze::Ctx& c, float x, float y) const {
    const int cell = cellAtPoint(c, x, y);
    if (cell >= 0 && !sheet_.cells[static_cast<std::size_t>(cell)].toolTip.empty())
        return glaze::TrText(sheet_.cells[static_cast<std::size_t>(cell)].toolTip);
    return glaze::Widget::toolTipAt(c, x, y);
}

// Bladets storlek i fysiska px: cellernas bredd och höjd plus luften, så att rullningen täcker allt.
gfx::Sizef LineHeightSheet::contentExtent(const glaze::Ctx& c) const {
    const gfx::Sizef base = glaze::Panel::contentExtent(c);
    if (sheet_.rows <= 0) return base;
    const Metrics m = measureSheet(c);
    const float w = (2.0f * kMargin + m.colX.back()) * c.scale;
    const float h = (2.0f * kMargin + static_cast<float>(sheet_.rows) * m.rowH) * c.scale;
    return gfx::Sizef{std::max(base.w, w), std::max(base.h, h)};
}

// Ritar bara de block som syns: blockets gula yta (växlar mellan blocken), cellerna med fyllning, text och ram, blockets kraftiga ram,
// och den aktuella cellen med accentfärgad ram när bladet eller textfältet har fokus.
void LineHeightSheet::draw(const glaze::Ctx& c) {
    glaze::Panel::draw(c);
    if (editing_) placeEditor(c);
    if (sheet_.blockStart.empty()) return;

    const Metrics m = measureSheet(c);
    const gfx::Rectf in = innerBounds(c);
    const gfx::Sizef range = scrollRange(c);
    const float sx = std::clamp(scrollX(), 0.0f, range.w);
    const float sy = std::clamp(scrollY(), 0.0f, range.h);
    const float ox = in.x + kMargin - sx;
    const float oy = in.y + kMargin - sy;
    const float blockH = static_cast<float>(lh::kBlockRows) * m.rowH;
    const float stride = static_cast<float>(lh::kBlockStride) * m.rowH;
    const int blocks = static_cast<int>(sheet_.blockStart.size());
    const int first = std::max(0, static_cast<int>(std::floor((in.y - oy) / stride)));
    const int last = std::min(blocks - 1, static_cast<int>(std::floor((in.y + in.h - oy) / stride)));
    const glaze::Ctx bold = glaze::Draw::withFont(c, gfx::FontFamily::UI, gfx::FontStyle::Bold);
    glaze::Ctx light = c;
    if (const glaze::AppWindow* w = appWindow())
        light.theme = &w->theme().light;  // glyferna på Excel-färgerna
    const bool showCurrent = current_ >= 0 && (focused() || editing_);

    glaze::Draw::clip(c, in, innerRadius(c));
    for (int b = first; b <= last; ++b) {
        const gfx::Rectf blockRect{ox, oy + static_cast<float>(b) * stride, m.colX.back(), blockH};
        glaze::Draw::rect(c, blockRect, 0.0f, toColor(lh::blockFill(b)));
        const int from = sheet_.blockStart[static_cast<std::size_t>(b)];
        const int to = b + 1 < blocks ? sheet_.blockStart[static_cast<std::size_t>(b + 1)]
                                      : static_cast<int>(sheet_.cells.size());
        for (int i = from; i < to; ++i) {
            const lh::SheetCell& cell = sheet_.cells[static_cast<std::size_t>(i)];
            const std::size_t col = static_cast<std::size_t>(cell.column);
            const std::size_t colEnd = static_cast<std::size_t>(cell.column + cell.columns);
            const gfx::Rectf r{ox + m.colX[col], oy + static_cast<float>(cell.row) * m.rowH,
                               m.colX[colEnd] - m.colX[col], static_cast<float>(cell.rows) * m.rowH};
            drawSheetCell(c, bold, light, cell, r, m.pad);
        }
        glaze::Draw::frame(c, blockRect, 0.0f, glaze::Pen{kBlack, 2.0f});
        if (showCurrent && current_ >= from && current_ < to) {
            const lh::SheetCell& cell = sheet_.cells[static_cast<std::size_t>(current_)];
            const std::size_t col = static_cast<std::size_t>(cell.column);
            const std::size_t colEnd = static_cast<std::size_t>(cell.column + cell.columns);
            const gfx::Rectf r{ox + m.colX[col], oy + static_cast<float>(cell.row) * m.rowH,
                               m.colX[colEnd] - m.colX[col], static_cast<float>(cell.rows) * m.rowH};
            glaze::Draw::frame(c, r, 0.0f, glaze::Pen{c.theme->colors.accent, 2.0f});
        }
    }
    glaze::Draw::endClip(c);
}

// Ett tryck sparar en öppen cell och öppnar den som träffades när den är en inmatning. Remsorna
// tar sina egna tryck. Målet tas fram före sparningen och hittas igen efteråt (bladet kan ha bytts).
void LineHeightSheet::mouseDown(const glaze::Ctx& c, float mx, float my) {
    if (inScrollStrip(c, mx, my)) {
        glaze::Panel::mouseDown(c, mx, my);
        return;
    }
    glaze::Panel::mouseDown(c, mx, my);
    const int cell = cellAtPoint(c, mx, my);
    const bool target = cell >= 0 && sheet_.cells[static_cast<std::size_t>(cell)].input.kind !=
                                         lh::InputRef::Kind::None;
    if (!target) {
        endEdit(true, false);
        return;
    }
    openAfterSave(cell);
}

// Enter och F2 öppnar den aktuella cellen, eller den första inmatningen, när bladet har fokus.
bool LineHeightSheet::keyDown(gfx::Key key, std::uint32_t mods) {
    const bool shortcut = (mods & (gfx::kModCtrl | gfx::kModCmd | gfx::kModAlt)) != 0;
    if (shortcut || (key != gfx::Key::Return && key != gfx::Key::F2)) return glaze::Panel::keyDown(key, mods);
    int cell = current_;
    if (cell < 0 && !sheet_.inputs.empty()) cell = sheet_.inputs.front();
    if (cell < 0) return false;
    openAfterSave(cell);
    return true;
}

} // namespace kalkyl::lineheightsheet
