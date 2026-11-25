#include "sheetGrid/sheet_grid.h"

#include <algorithm>
#include <cstddef>
#include <utility>

#include "tableText/table_text.h"

namespace kalkyl::sheetgrid {

namespace {

// Markeringens färg över cellerna i en utökad markering.
const gfx::Color kSelectColor{60, 130, 210, 90};

// Texten utan blanksteg i början och slutet. Tabb och radbrytning räknas som blanksteg.
std::string trim(const std::string& s)
{
    const auto isSpace = [](char c) {
        return c == ' ' || c == char{9} || c == char{10} || c == char{13};
    };
    std::size_t b = 0;
    std::size_t e = s.size();
    while (b < e && isSpace(s[b]))
        ++b;
    while (e > b && isSpace(s[e - 1]))
        --e;
    return s.substr(b, e - b);
}

// Postar ett anrop till UI-tråden som hoppas över om objektet hunnit försvinna.
void postIfAlive(const std::shared_ptr<bool>& alive, std::function<void()> fn)
{
    std::weak_ptr<bool> weak = alive;
    glaze::app().post([weak, fn = std::move(fn)] {
        if (weak.lock())
            fn();
    });
}

// Sant när raden är en tomrad enligt formen.
bool isSkipRow(const SheetShape& shape, int row)
{
    return shape.skipRow && shape.skipRow(row);
}

// Sant när cellen tar emot inklistring och Delete.
bool isInputCell(const SheetShape& shape, int row, int column)
{
    return shape.isInput && shape.isInput(row, column);
}

// Sant när cellen ligger i en markering som är större än den aktuella cellen.
bool inExtendedSelection(CellPos anchor, CellPos extent, int row, int column)
{
    if (anchor.row == extent.row && anchor.column == extent.column)
        return false;
    const CellRect rect = rectOf(anchor, extent);
    return row >= rect.row0 && row <= rect.row1 && column >= rect.column0 && column <= rect.column1;
}

// Det de fria hjälpfunktionerna behöver av SheetGrid: tabellen, formen, värden och markeringen.
struct State {
    glaze::DataGrid& grid;
    const SheetShape& shape;
    SheetHost& host;
    CellPos& anchor;
    CellPos& extent;
};

// Målar om en cell: markeringens färg om den ligger i en utökad markering, annars grundfärgen.
// Tomrader och kolumnerna före firstColumn (radrubriken) målas aldrig.
void paintCellOf(const State& s, int row, int column)
{
    if (row < 0 || row >= std::min(s.shape.rowCount, s.grid.rowCount()))
        return;
    if (column < s.shape.firstColumn || column >= s.shape.columnCount || isSkipRow(s.shape, row))
        return;
    const std::optional<gfx::Color> color = inExtendedSelection(s.anchor, s.extent, row, column)
                                                ? std::optional<gfx::Color>(kSelectColor)
                                                : s.host.baseColor(row, column);
    s.grid.setCellColor(row, column, color, std::nullopt);
}

// Målar om alla celler i rektangeln, klampad till tabellen.
void paintRectOf(const State& s, const CellRect& rect)
{
    for (int r = std::max(0, rect.row0); r <= rect.row1; ++r)
        for (int c = std::max(s.shape.firstColumn, rect.column0); c <= rect.column1; ++c)
            paintCellOf(s, r, c);
}

// Sätter markeringen och målar om cellerna i den gamla och den nya rektangeln.
// moveCurrent gör änden till tabellens aktuella cell och rullar den i sikte.
void setSelectionOf(const State& s, CellPos anchor, CellPos extent, bool moveCurrent, int step)
{
    const CellRect old = rectOf(s.anchor, s.extent);
    s.anchor = clampPos(anchor, s.shape, step);
    s.extent = clampPos(extent, s.shape, step);
    if (moveCurrent && s.shape.rowCount > 0) {
        s.grid.setCurrentCell(s.extent.row, s.extent.column);
        s.grid.ensureVisible(s.extent.row);
    }
    paintRectOf(s, old);
    paintRectOf(s, rectOf(s.anchor, s.extent));
}

} // namespace

// Planerar en inklistring: var fälten landar, om första raden är en rubrikrad, kontrollen före
// ändring och blockets område. Ett fält med bara blanksteg räknas som tomt och tömmer cellen.
SheetPaste planSheetPaste(CellPos anchor, const std::vector<std::vector<std::string>>& block,
                          const SheetShape& shape)
{
    SheetPaste paste;
    const int blockRows = static_cast<int>(block.size());
    int width = 0;
    for (const auto& row : block)
        width = std::max(width, static_cast<int>(row.size()));

    // Området: från ankaret till blockets sista rad och kolumn, klampat till tabellen.
    const int lastRow = std::max(0, shape.rowCount - 1);
    const int lastColumn = std::max(0, shape.columnCount - 1);
    paste.area.row0 = std::clamp(anchor.row, 0, lastRow);
    paste.area.column0 = std::clamp(anchor.column, 0, lastColumn);
    paste.area.row1 = std::clamp(anchor.row + std::max(0, blockRows - 1), 0, lastRow);
    paste.area.column1 = std::clamp(anchor.column + std::max(0, width - 1), 0, lastColumn);

    // Cellen som fältet landar i, eller false när fältet ignoreras (utanför, tomrad, ingen indata).
    const auto lands = [&](int r, int c, int& row, int& column) {
        row = anchor.row + r;
        column = anchor.column + c;
        if (row < 0 || row >= shape.rowCount || column < shape.firstColumn || column >= shape.columnCount)
            return false;
        if (isSkipRow(shape, row))
            return false;
        return isInputCell(shape, row, column);
    };
    // Fältet som cellen tar emot, eller tomt svar när det inte passar.
    const auto acceptField = [&](int row, int column, const std::string& field) {
        if (!shape.accept)
            return std::optional<std::string>(field);
        return shape.accept(row, column, field);
    };

    // Rubrikraden: flera rader, och första raden har ett icke-tomt fält som cellen avvisar.
    int firstRow = 0;
    if (blockRows > 1) {
        const auto& first = block.front();
        for (std::size_t j = 0; j < first.size(); ++j) {
            int row = 0;
            int column = 0;
            const std::string field = trim(first[j]);
            if (field.empty() || !lands(0, static_cast<int>(j), row, column))
                continue;
            if (!acceptField(row, column, field)) {
                firstRow = 1;
                paste.skippedHeader = true;
                break;
            }
        }
    }

    // Kontroll före ändring, och sedan en ändring per fält.
    std::vector<CellEdit> edits;
    for (int r = firstRow; r < blockRows; ++r) {
        const auto& fields = block[static_cast<std::size_t>(r)];
        for (std::size_t j = 0; j < fields.size(); ++j) {
            int row = 0;
            int column = 0;
            if (!lands(r, static_cast<int>(j), row, column))
                continue;
            const std::string field = trim(fields[j]);
            if (field.empty()) {
                edits.push_back({row, column, std::string()});
                continue;
            }
            const std::optional<std::string> text = acceptField(row, column, field);
            if (!text) {
                const std::string name = shape.columnName ? shape.columnName(column) : std::to_string(column);
                paste.rejected = std::string("Inklistringen avbröts: ") + char{34} + field + char{34} +
                                 " på rad " + std::to_string(r + 1) + " i urklippet passar inte i kolumnen " +
                                 name + ". Inget ändrades.";
                return paste;
            }
            edits.push_back({row, column, *text});
        }
    }
    paste.edits = std::move(edits);
    return paste;
}

// Rektangeln med de lägre och de högre radnumren och kolumnnumren för sig.
CellRect rectOf(CellPos a, CellPos b)
{
    return {std::min(a.row, b.row), std::min(a.column, b.column), std::max(a.row, b.row),
            std::max(a.column, b.column)};
}

// Klampar till raderna och kolumnerna från firstColumn. En tomrad flyttas ett steg i riktningen
// step, och åt andra hållet när det inte finns någon rad kvar.
CellPos clampPos(CellPos pos, const SheetShape& shape, int step)
{
    pos.row = std::clamp(pos.row, 0, std::max(0, shape.rowCount - 1));
    pos.column = std::clamp(pos.column, std::min(shape.firstColumn, std::max(0, shape.columnCount - 1)),
                            std::max(0, shape.columnCount - 1));
    if (!isSkipRow(shape, pos.row))
        return pos;
    const int dir = step < 0 ? -1 : 1;
    for (const int d : {dir, -dir}) {
        const int next = pos.row + d;
        if (next >= 0 && next < shape.rowCount && !isSkipRow(shape, next)) {
            pos.row = next;
            return pos;
        }
    }
    return pos;
}

// Sant för flera rader eller flera fält. En enda cell, också med avslutande radbrytning, är inget block.
bool isBlock(const std::string& clip)
{
    const auto rows = tabletext::parseTsv(clip);
    return rows.size() > 1 || (!rows.empty() && rows.front().size() > 1);
}

// Förvalet: inga egna poster i snabbmenyn.
void SheetHost::addMenuItems(glaze::Menu&, const CellRect&) {}

// Markeringen börjar i första cellen efter radrubriken.
SheetGrid::SheetGrid(glaze::DataGrid& grid, SheetShape shape, SheetHost& host)
    : grid_(grid), shape_(std::move(shape)), host_(host)
{
    anchor_ = clampPos({0, shape_.firstColumn}, shape_, 1);
    extent_ = anchor_;
}

// Skrivskyddat läge: bara kopiering, Ctrl+A och markering.
void SheetGrid::setReadOnly(bool readOnly)
{
    readOnly_ = readOnly;
}

// Tangenter i tabellen. Under redigering tas bara Ctrl+V med ett block. Annars gäller Ctrl+C, X,
// V, A, Delete och markering med Shift. Övriga tangenter lämnas åt tabellen, och markeringen
// synkas efteråt med den aktuella cellen.
void SheetGrid::keyDown(glaze::KeyArgs& e)
{
    const bool ctrl = (e.mods & gfx::kModCtrl) != 0 && (e.mods & gfx::kModAlt) == 0;
    const bool shift = (e.mods & gfx::kModShift) != 0;
    const State s{grid_, shape_, host_, anchor_, extent_};

    if (grid_.isEditing()) {
        if (ctrl && !shift && e.key == gfx::Key::V && !readOnly_ && isBlock(gfx::clipboardText())) {
            e.handled = true;
            const CellPos cell{grid_.currentRow(), grid_.currentColumn()};
            grid_.cancelEdit();
            setSelectionOf(s, cell, cell, false, 1);
            pasteAtSelection();
            return;
        }
        postIfAlive(alive_, [this] { syncSelection(false); });
        return;
    }

    if (ctrl && !shift) {
        if (e.key == gfx::Key::C) {
            copySelection();
            e.handled = true;
            return;
        }
        if (e.key == gfx::Key::X) {
            copySelection();
            if (!readOnly_)
                clearSelectionCells();
            e.handled = true;
            return;
        }
        if (e.key == gfx::Key::V) {
            if (!readOnly_)
                pasteAtSelection();
            e.handled = true;
            return;
        }
        if (e.key == gfx::Key::A) {
            selectAll();
            e.handled = true;
            return;
        }
    }

    if (!ctrl && !shift && e.key == gfx::Key::Delete) {
        if (!readOnly_)
            clearSelectionCells();
        e.handled = true;
        return;
    }

    if (shift) {
        CellPos next = extent_;
        int step = 1;
        bool handled = true;
        if (ctrl) {
            if (e.key == gfx::Key::Home) {
                next = {0, shape_.firstColumn};
            } else if (e.key == gfx::Key::End) {
                next = {shape_.rowCount - 1, shape_.columnCount - 1};
                step = -1;
            } else {
                handled = false;
            }
        } else {
            switch (e.key) {
            case gfx::Key::Left: --next.column; break;
            case gfx::Key::Right: ++next.column; break;
            case gfx::Key::Up: --next.row; step = -1; break;
            case gfx::Key::Down: ++next.row; break;
            case gfx::Key::Home: next.column = shape_.firstColumn; break;
            case gfx::Key::End: next.column = shape_.columnCount - 1; break;
            case gfx::Key::PageUp: next.row -= 20; step = -1; break;
            case gfx::Key::PageDown: next.row += 20; break;
            default: handled = false; break;
            }
        }
        if (handled) {
            setSelectionOf(s, anchor_, next, true, step);
            e.handled = true;
            return;
        }
    }

    postIfAlive(alive_, [this] { syncSelection(false); });
}

// Musen släpptes i tabellen. Trycket har redan gjort cellen aktuell, så markeringen läses
// efteråt. Med Shift utökas markeringen från ankaret.
void SheetGrid::mouseUp(glaze::MouseArgs& e)
{
    if (e.button != gfx::MouseButton::Left)
        return;
    const bool shift = (e.mods & gfx::kModShift) != 0;
    postIfAlive(alive_, [this, shift] { syncSelection(shift); });
}

// Högerklick i tabellen. Raden under pekaren markeras om den ligger utanför markeringen, och
// sedan visas snabbmenyn. Skrivskyddat läge har bara Kopiera och Markera alla.
void SheetGrid::contextMenu(glaze::ContextMenuArgs& e)
{
    const State s{grid_, shape_, host_, anchor_, extent_};
    int row = -1;
    if (const auto p = grid_.fromWindow(gfx::Pointf{e.x, e.y}))
        row = grid_.rowAt(p->x, p->y);
    if (row >= 0 && !isSkipRow(shape_, row)) {
        const CellRect rect = rectOf(anchor_, extent_);
        if (row < rect.row0 || row > rect.row1) {
            const int column = std::max(shape_.firstColumn, grid_.currentColumn());
            setSelectionOf(s, {row, column}, {row, column}, true, 1);
        }
    }

    menu_.items.clear();
    const bool edit = !readOnly_;
    const auto add = [this](const char* text, bool enabled, gfx::Key key, std::uint32_t mods,
                            std::function<void()> action) {
        glaze::MenuItem& item = menu_.addItem(text);
        item.enabled = enabled;
        if (key != gfx::Key::Unknown)
            item.shortcut = {key, mods};
        item.onClick = [action] { action(); };
    };
    add("Klipp ut", edit, gfx::Key::X, gfx::kModCtrl, [this] {
        copySelection();
        clearSelectionCells();
    });
    add("Kopiera", true, gfx::Key::C, gfx::kModCtrl, [this] { copySelection(); });
    add("Klistra in", edit, gfx::Key::V, gfx::kModCtrl, [this] { pasteAtSelection(); });
    add("Radera innehåll", edit, gfx::Key::Delete, 0, [this] { clearSelectionCells(); });
    menu_.addSeparator();
    add("Markera alla", true, gfx::Key::A, gfx::kModCtrl, [this] { selectAll(); });

    // Fönstrets egna poster kommer efter en separator, som tas bort när inga poster tillkommer.
    menu_.addSeparator();
    const std::size_t before = menu_.items.size();
    host_.addMenuItems(menu_, rectOf(anchor_, extent_));
    if (menu_.items.size() == before)
        menu_.items.pop_back();
    menu_.popupAt(grid_, e.x, e.y);
}

// Målar om alla celler i tabellen.
void SheetGrid::paintAll()
{
    const State s{grid_, shape_, host_, anchor_, extent_};
    paintRectOf(s, {0, shape_.firstColumn, shape_.rowCount - 1, shape_.columnCount - 1});
}

// Målar om en cell.
void SheetGrid::paintCell(int row, int column)
{
    const State s{grid_, shape_, host_, anchor_, extent_};
    paintCellOf(s, row, column);
}

// Rektangeln mellan ankaret och änden, båda inräknade.
CellRect SheetGrid::selection() const
{
    return rectOf(anchor_, extent_);
}

// Sätter markeringen och gör änden till tabellens aktuella cell.
void SheetGrid::select(CellPos anchor, CellPos extent)
{
    const State s{grid_, shape_, host_, anchor_, extent_};
    setSelectionOf(s, anchor, extent, true, 1);
}

// Kopierar markeringen som tabbseparerad text. Tal skrivs som fönstret skriver dem, och en
// tomrad blir en tom rad. Statusraden berättar vad som kopierades.
void SheetGrid::copySelection()
{
    const CellRect rect = rectOf(anchor_, extent_);
    std::vector<std::vector<std::string>> rows;
    for (int r = rect.row0; r <= rect.row1; ++r) {
        std::vector<std::string> fields;
        for (int c = rect.column0; c <= rect.column1; ++c)
            fields.push_back(isSkipRow(shape_, r) ? std::string() : host_.copyText(r, c));
        rows.push_back(std::move(fields));
    }
    gfx::setClipboardText(tabletext::formatTsv(rows));
    host_.showStatus("Kopierade " + std::to_string(rows.size()) + " × " +
                     std::to_string(rect.column1 - rect.column0 + 1) + " celler");
}

// Klistrar in urklippet vid markeringens övre vänstra cell. En inklistring som avbryts visas i
// en ruta och ändrar inget. Annars skrivs ändringarna som en och markeringen blir blocket.
void SheetGrid::pasteAtSelection()
{
    const auto block = tabletext::parseTsv(gfx::clipboardText());
    if (block.empty()) {
        host_.showStatus("Urklippet är tomt.");
        return;
    }
    const CellRect rect = rectOf(anchor_, extent_);
    const SheetPaste paste = planSheetPaste({rect.row0, rect.column0}, block, shape_);
    if (paste.rejected) {
        if (glaze::AppWindow* owner = grid_.appWindow())
            glaze::messageBox(*owner, *paste.rejected, "Klistra in", glaze::MessageBoxButtons::OK,
                              glaze::MessageBoxType::Warning);
        else
            glaze::messageBox(*paste.rejected, "Klistra in", glaze::MessageBoxButtons::OK,
                              glaze::MessageBoxType::Warning);
        return;
    }
    if (paste.edits.empty()) {
        host_.showStatus("Inget att klistra in här. Markera en inmatningscell.");
        return;
    }
    host_.applyEdits(paste.edits);
    select({paste.area.row0, paste.area.column0}, {paste.area.row1, paste.area.column1});
    host_.showStatus("Klistrade in " + std::to_string(block.size() - (paste.skippedHeader ? 1u : 0u)) +
                     " rader");
}

// Läser tabellens aktuella cell. Med extend utökas markeringen från ankaret, annars blir
// markeringen den aktuella cellen.
void SheetGrid::syncSelection(bool extend)
{
    const int row = grid_.currentRow();
    const int column = grid_.currentColumn();
    if (row < 0 || column < 0)
        return;
    const State s{grid_, shape_, host_, anchor_, extent_};
    if (extend)
        setSelectionOf(s, anchor_, {row, column}, false, 1);
    else
        setSelectionOf(s, {row, column}, {row, column}, false, 1);
}

// Tömmer indatacellerna i markeringen med en ändring till fönstret. Tomrader och celler som
// inte är indata rörs inte.
void SheetGrid::clearSelectionCells()
{
    const CellRect rect = rectOf(anchor_, extent_);
    std::vector<CellEdit> edits;
    for (int r = rect.row0; r <= rect.row1; ++r) {
        if (isSkipRow(shape_, r))
            continue;
        for (int c = rect.column0; c <= rect.column1; ++c)
            if (isInputCell(shape_, r, c))
                edits.push_back({r, c, std::string()});
    }
    if (edits.empty())
        return;
    host_.applyEdits(edits);
    const State s{grid_, shape_, host_, anchor_, extent_};
    setSelectionOf(s, anchor_, extent_, false, 1);
}

// Markerar alla celler. Den aktuella cellen blir överst till vänster.
void SheetGrid::selectAll()
{
    const State s{grid_, shape_, host_, anchor_, extent_};
    setSelectionOf(s, {shape_.rowCount - 1, shape_.columnCount - 1}, {0, shape_.firstColumn}, true, 1);
}

} // namespace kalkyl::sheetgrid
