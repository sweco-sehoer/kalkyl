#include "poleGrid/pole_grid.h"

#include <algorithm>
#include <functional>
#include <utility>

#include "tableText/table_text.h"

namespace kalkyl::polegrid {

namespace {

namespace pt = kalkyl::poletable;

// Radbrytning i texter. Skrivs som tecken så att ingen escape-sekvens behövs i källan.
const std::string kNl(1, char{10});

// Färgerna i tabellerna. Radens ton har fast alfa (se tone), cellens färger har eget alfa.
const gfx::Color kAmber{220, 160, 40, 255};            // Som i före, och alla rader i före
const gfx::Color kRose{210, 90, 140, 255};             // Ändrad rad
const gfx::Color kGreen{70, 170, 90, 255};             // Ny rad
const gfx::Color kChangedCell{210, 90, 140, 130};      // Ändrad cell
const gfx::Color kErrorColor{220, 60, 60, 80};        // Fel
const gfx::Color kHiddenRow{128, 128, 128, 48};        // Grå rad: bakgrund
const gfx::Color kHiddenText{128, 128, 128, 255};      // Grå rad: text

// Radens ton: samma färg med växling mellan två alfavärden, så att raderna är lätta att följa.
gfx::Color tone(gfx::Color color, int row)
{
    color.a = (row % 2 == 0) ? 64 : 52;
    return color;
}

// Postar ett anrop till UI-tråden som hoppas över om fönstret hunnit försvinna. Tabellens ägare
// är en lokal i main-slingan, så en postad lambda får inte nå ett rivet fönster.
void postIfAlive(const std::shared_ptr<bool>& alive, std::function<void()> fn)
{
    std::weak_ptr<bool> weak = alive;
    glaze::app().post([weak, fn = std::move(fn)] {
        if (weak.lock())
            fn();
    });
}

// Kör en funktion när scopet lämnas. Används för att posta en väntande ombyggnad av vyn sist i
// tabellens händelser, oavsett var händelsen returnerar.
template <class F>
struct OnExit {
    F fn;
    ~OnExit() { fn(); }
};
template <class F>
OnExit(F) -> OnExit<F>;

// En rad med tomma celler i alla kolumner, också radera-kolumnen: den tomma raden sist, där en
// ny stolpe skrivs.
glaze::DataGrid::Row blankRow(int cells)
{
    glaze::DataGrid::Row blank;
    blank.cells.assign(static_cast<std::size_t>(cells), std::string());
    return blank;
}

// Lägger till en tooltipdel efter de förra, skilda med radbrytning.
void appendTip(std::string& tip, const std::string& part)
{
    if (part.empty())
        return;
    if (!tip.empty())
        tip += kNl;
    tip += part;
}

// Värdet i Stolptabell före för ett fält i efter, som det visas i tabellen (Av för Av Fas2 och
// Fas3), eller "tom" när cellen är tom eller raden i före saknas. Delas av tooltipen på en ändrad
// cell och statusraden efter Kopiera från Stolptabell före.
std::string beforeText(const std::vector<pt::PoleRow>& before, int beforeRow, int field)
{
    if (beforeRow < 0 || static_cast<std::size_t>(beforeRow) >= before.size())
        return "tom";
    const std::string text = pt::displayText(before[static_cast<std::size_t>(beforeRow)], pt::beforeField(field));
    return text.empty() ? std::string("tom") : text;
}

// Vyns rad v, eller null utanför vyn.
const poleview::ViewRow* viewRowAt(const poleview::PoleView& view, int v)
{
    if (v < 0 || v >= static_cast<int>(view.rows.size()))
        return nullptr;
    return &view.rows[static_cast<std::size_t>(v)];
}

// Tabellens första rad för vyns rad v, klampad till vyn. Den tomma raden ger antalet rader.
int firstModelRow(const poleview::PoleView& view, int v)
{
    if (view.rows.empty())
        return 0;
    const int last = static_cast<int>(view.rows.size()) - 1;
    return view.rows[static_cast<std::size_t>(std::clamp(v, 0, last))].first;
}

// Kolumnen under en punkt i tabellen. GlazeUI v0.11.1 har bara den skyddade Table::columnAt, och en
// pekare till medlem som bildas här, i en härledd klass, får anropas på vilken DataGrid som helst.
// Klassen skapas aldrig. windowX är fönstrets fysiska px (ContextMenuArgs::x), som columnAt väntar
// sig. -1 utanför kolumnerna.
struct GridProbe : glaze::DataGrid {
    static int columnUnder(glaze::DataGrid& grid, float windowX)
    {
        int (glaze::Table::*hit)(const glaze::Ctx&, float) const = &GridProbe::columnAt;
        return (grid.*hit)(grid.behaviorCtx(), windowX);
    }
};

} // namespace

// Förvalet: värden bryr sig inte om att filtret bytts.
void PoleGridHost::filterChanged(poleview::Filter) {}

// Spannlängden ur koordinaterna med två decimaler, via kanonisk text så att visning och urklipp
// formateras som de inskrivna talen.
std::string computedSpanText(double meters, bool group)
{
    return tabletext::formatDecimalText(tabletext::formatDecimal(meters, 2, '.'), ',', group);
}

// Cellens utseende. Radens ton ur jämförelsen, cellens färg och tooltip ur jämförelsen och
// felen, och flaggan i Stolpplats när en sträcka börjar eller slutar på raden. Ett fel vinner
// över jämförelsens färg, och tooltipens delar kommer i ordningen fel, jämförelse, not.
CellLook cellLook(const PoleLooks& looks, int row, int column, int dataRows, std::string_view pole)
{
    CellLook look;
    const bool isData = row >= 0 && row < dataRows;
    const automode::RowCompare* cmp = (isData && static_cast<std::size_t>(row) < looks.compare.size())
                                          ? &looks.compare[static_cast<std::size_t>(row)]
                                          : nullptr;
    gfx::Color base = kAmber;
    if (cmp && cmp->state == automode::RowState::Changed)
        base = kRose;
    else if (cmp && cmp->state == automode::RowState::New)
        base = kGreen;
    look.background = tone(base, row);
    if (!isData || column < 0 || column >= pt::kFieldCount)
        return look;

    std::string tip;
    std::optional<gfx::Color> cellColor;
    bool hasIssue = false;
    for (const auto& issue : looks.marks.issues) {
        if (issue.row == row && issue.column == column) {
            hasIssue = true;
            appendTip(tip, issue.message);
        }
    }

    if (cmp) {
        if (cmp->state == automode::RowState::Changed && cmp->changed[static_cast<std::size_t>(column)]) {
            cellColor = kChangedCell;
            appendTip(tip, "I Stolptabell före: " + beforeText(looks.before, cmp->beforeRow, column));
        } else if (cmp->state == automode::RowState::New && column == pt::kPole) {
            appendTip(tip, pole.empty() ? "Raden saknar Stolpnr och kan inte jämföras med Stolptabell före."
                                        : "Finns inte i Stolptabell före.");
        } else if (cmp->state == automode::RowState::Ambiguous && column == pt::kPole) {
            cellColor = kErrorColor;
            appendTip(tip, "Stolpnr " + std::string(pole) +
                               " finns flera gånger i Stolptabell före och kan inte jämföras.");
        }
    }
    if (hasIssue)
        cellColor = kErrorColor;

    // En tom Spannlängd föreg. med värde ur koordinaterna: Σ-bild och förklaringen.
    if (column == pt::kSpan && static_cast<std::size_t>(row) < looks.coordinateSpans.size() &&
        looks.coordinateSpans[static_cast<std::size_t>(row)]) {
        look.computed = true;
        appendTip(tip, "Räknad ur koordinaterna: vågräta avståndet till föregående stolpe. "
                       "Skriv en spannlängd för att ersätta den, och töm cellen för att räkna igen.");
    }

    if (column == pt::kPole && static_cast<std::size_t>(row) < looks.marks.rowNotes.size() &&
        !looks.marks.rowNotes[static_cast<std::size_t>(row)].empty()) {
        look.flag = true;
        appendTip(tip, looks.marks.rowNotes[static_cast<std::size_t>(row)]);
    }
    if (cellColor)
        look.background = cellColor;
    look.toolTip = std::move(tip);
    return look;
}

// Håller räkningen av tabellens egna händelser (inEvent_). Så länge en händelse pågår får vyn
// inte byggas om, eftersom SheetGrid läser tabellen efter händelsen.
struct PoleGrid::EventScope {
    explicit EventScope(PoleGrid& grid) : grid_(grid) { ++grid_.inEvent_; }
    ~EventScope() { --grid_.inEvent_; }
    EventScope(const EventScope&) = delete;
    EventScope& operator=(const EventScope&) = delete;

private:
    PoleGrid& grid_;
};

// Adaptern till SheetGrid: texten en cell kopieras som, cellens färg utan markering, inklistringen
// över grå rader och snabbmenyns egna poster. Raderna som SheetGrid talar om är vyns rader, och
// adaptern översätter dem till modellens. Ändringar och statustexter går vidare till PoleGrid.
class PoleGrid::Adapter : public sheetgrid::SheetHost {
public:
    explicit Adapter(PoleGrid& grid) : grid_(grid) {}

    // Cellens text i urklippet, med decimalkomma och utan gruppering, också en spannlängd ur
    // koordinaterna. Den tomma raden sist och grå rader ger tom text.
    std::string copyText(int row, int column) const override
    {
        return grid_.cellText(poleview::modelRow(grid_.view_, row), grid_.fieldOf(column), false);
    }

    // Cellens färg utan markering: radens ton och jämförelsens färg ur cellLook, och grå för en grå rad.
    std::optional<gfx::Color> baseColor(int row, int column) const override
    {
        // Skydd: SheetGrid målar aldrig grå rader, som får sina färger i applyLooks.
        if (poleview::isHiddenRow(grid_.view_, row))
            return kHiddenRow;
        const int count = static_cast<int>(grid_.rows_.size());
        const int m = poleview::modelRow(grid_.view_, row);
        const std::string_view pole =
            (m >= 0 && m < count) ? std::string_view(grid_.rows_[static_cast<std::size_t>(m)].pole)
                                  : std::string_view();
        return cellLook(grid_.looks_, m, grid_.fieldOf(column), count, pole).background;
    }

    // Inklistrade och raderade celler skrivs i modellen.
    void applyEdits(const std::vector<sheetgrid::CellEdit>& edits) override { grid_.applyEdits(edits); }

    // Inklistringen över grå rader skrivs i modellens rader i följd.
    bool pasteBlock(const std::vector<std::vector<std::string>>& block, sheetgrid::CellPos anchor) override
    {
        return grid_.pasteIntoModel(block, anchor);
    }

    // Snabbmenyns egna poster: Infoga rad ovanför, Radera rader… och, i Stolptabell efter, Kopiera
    // från Stolptabell före för den högerklickade cellen. Sist värdens egna poster.
    void addMenuItems(glaze::Menu& menu, const sheetgrid::CellRect& selection) override
    {
        PoleGrid* grid = &grid_;
        const bool writable = !grid_.readOnly_;
        const poleview::ViewRow* first = viewRowAt(grid_.view_, selection.row0);
        const int m0 = (first && first->kind == poleview::ViewKind::Pole) ? first->first : -1;
        const std::vector<int> selected = grid_.selectedModelRows(selection);

        glaze::MenuItem& insertItem = menu.addItem("Infoga rad ovanför");
        insertItem.enabled = writable && m0 >= 0;
        insertItem.onClick = [grid, m0] { grid->insertRowAbove(m0); };

        glaze::MenuItem& removeItem = menu.addItem("Radera rader…");
        removeItem.enabled = writable && !selected.empty();
        removeItem.onClick = [grid, selected] {
            postIfAlive(grid->alive_, [grid, selected] { grid->removeRowsAsked(selected); });
        };

        if (grid_.compares_) {
            const poleview::ViewRow* cell = viewRowAt(grid_.view_, grid_.menuCell_.row);
            const int m = (cell && cell->kind == poleview::ViewKind::Pole) ? cell->first : -1;
            const int field = grid_.fieldOf(grid_.menuCell_.column);
            std::optional<pt::Edit> edit;
            if (m >= 0)
                edit = automode::restoreFromBefore(grid_.looks_.before,
                                                   automode::compareAfter(grid_.looks_.before, grid_.rows_), m, field);
            glaze::MenuItem& restoreItem = menu.addItem("Kopiera från Stolptabell före");
            restoreItem.enabled = writable && edit.has_value();
            restoreItem.onClick = [grid, m, field] { grid->restoreCell(m, field); };
        }

        grid_.host_.addMenuItems(menu, m0);
    }

    // En kort text i statusraden.
    void showStatus(const std::string& text) override { grid_.host_.showStatus(text); }

private:
    PoleGrid& grid_;
};

// Formen för SheetGrid utan antalet rader och de grå raderna: datakolumnerna tar emot inklistring
// (med fältets regler), och nya rader får läggas till under tabellen.
sheetgrid::SheetShape PoleGrid::baseShape() const
{
    sheetgrid::SheetShape shape;
    const int columns = static_cast<int>(columns_.size());
    shape.columnCount = columns;
    shape.firstColumn = 0;
    shape.isInput = [columns](int, int column) { return column >= 0 && column < columns; };
    shape.accept = [this](int, int column, const std::string& text) { return pt::acceptField(fieldOf(column), text); };
    shape.columnName = [this](int column) { return fieldName(fieldOf(column)); };
    shape.growRows = true;
    shape.headerTakesRow = false;
    return shape;
}

// Fältet för tabellens kolumn, -1 för radera-kolumnen och utanför.
int PoleGrid::fieldOf(int column) const
{
    if (column < 0 || column >= static_cast<int>(columns_.size()))
        return -1;
    return columns_[static_cast<std::size_t>(column)];
}

// Tabellens kolumn för fältet, -1 när tabellen saknar det.
int PoleGrid::columnOf(int field) const
{
    const auto it = std::find(columns_.begin(), columns_.end(), field);
    return it == columns_.end() ? -1 : static_cast<int>(it - columns_.begin());
}

// Radera-kolumnen står efter datakolumnerna.
int PoleGrid::deleteColumn() const
{
    return static_cast<int>(columns_.size());
}

// Fältets namn. Har tabellen Av per fas heter Av "Av Fas1", så att det skiljs från Fas2 och Fas3.
std::string PoleGrid::fieldName(int field) const
{
    return columnOf(pt::kAv2) >= 0 ? pt::afterColumnName(field) : pt::columnName(field);
}

// Skapar Excel-markeringen med formen ur baseShape och de grå raderna ur vyn. Tabellen får sina
// rader först med setRows.
PoleGrid::PoleGrid(glaze::AppWindow& owner, glaze::DataGrid& grid, PoleGridHost& host, std::string tableName,
                   std::vector<int> columns)
    : owner_(owner), grid_(grid), host_(host), tableName_(std::move(tableName)), columns_(std::move(columns))
{
    sheetgrid::SheetShape shape = baseShape();
    shape.rowCount = grid_.rowCount();
    shape.hiddenRow = [this](int row) { return poleview::isHiddenRow(view_, row); };
    adapter_ = std::make_unique<Adapter>(*this);
    sheet_ = std::make_unique<sheetgrid::SheetGrid>(grid_, std::move(shape), *adapter_);
}

// Spärren alive_ släpps först, så att postade anrop hoppas över.
PoleGrid::~PoleGrid() = default;

// Skrivskyddat: kolumnerna blir låsta textceller utan knapp, och markeringen får bara kopiera.
// Tabellen byggs om, så att papperskorgarna kommer eller försvinner.
void PoleGrid::setReadOnly(bool readOnly)
{
    readOnly_ = readOnly;
    for (int c = 0; c < deleteColumn(); ++c) {
        if (readOnly_)
            grid_.setColumnEditor(c, {.editor = glaze::DataGrid::Editor::Text, .readOnly = true});
        else
            grid_.setColumnEditor(c, {glaze::DataGrid::Editor::Text});
    }
    grid_.setColumnEditor(deleteColumn(), {readOnly_ ? glaze::DataGrid::Editor::None : glaze::DataGrid::Editor::Button});
    sheet_->setReadOnly(readOnly_);
    fill();
}

// Tar emot raderna ur projektet. De blir också det senast sparade, och tabellen byggs om med den
// vy som filtret ger.
void PoleGrid::setRows(std::vector<poletable::PoleRow> rows)
{
    rows_ = std::move(rows);
    saved_ = pt::rowsToSave(rows_);
    view_ = desiredView();
    viewPending_ = false;
    fill();
}

// Raderna som de står i modellen, utan den tomma raden sist.
const std::vector<poletable::PoleRow>& PoleGrid::rows() const
{
    return rows_;
}

// Nya färger och märken. Tooltips och flaggor sätts om och alla celler målas om. Ändrade rader kan
// ändra vyn, och den byggs då om.
void PoleGrid::setLooks(PoleLooks looks)
{
    looks_ = std::move(looks);
    applyLooks();
    refreshView();
}

// Stolptabell efter jämförs med före. Utan jämförelse är filtret alltid All.
void PoleGrid::setComparesWithBefore(bool on)
{
    compares_ = on;
    if (!compares_)
        filter_ = poleview::Filter::All;
    refreshView();
}

// Slår på radid: save() ger nya rader id med poletable::assignRowIds före varje sparning.
void PoleGrid::setRowIds(bool on)
{
    rowIds_ = on;
}

// Byter filter. En öppen cell sparas först, och vyn byggs om direkt. Utan jämförelse gäller All.
void PoleGrid::setFilter(poleview::Filter filter)
{
    if (!compares_)
        filter = poleview::Filter::All;
    if (filter == filter_)
        return;
    owner_.commitEdits();
    filter_ = filter;
    viewPending_ = true;
    rebuildView();
}

// Det aktuella filtret.
poleview::Filter PoleGrid::filter() const
{
    return filter_;
}

// Gör tabellens rad aktuell och synlig. Är raden dold av filtret blir filtret All, värden får veta
// det, och statusraden förklarar varför filtret bytte.
void PoleGrid::showRow(int row)
{
    if (row < 0 || row >= static_cast<int>(rows_.size()))
        return;
    if (viewPending_)
        rebuildView();
    if (poleview::isHiddenRow(view_, poleview::viewRowOf(view_, row))) {
        setFilter(poleview::Filter::All);
        host_.filterChanged(poleview::Filter::All);
        host_.showStatus("Filtret är nu Alla, så att " + poleview::rowLabel(rows_, row) + " syns.");
    }
    const int v = poleview::viewRowOf(view_, row);
    sheet_->select({v, 0}, {v, 0});
}

// Visa förändrade: första förändrade raden blir aktuell, och området från den första till den sista
// förändrade raden rullas så att dess mitt hamnar mitt i vyn. Ryms det inte står första raden överst.
bool PoleGrid::showChanged()
{
    if (!compares_)
        return false;
    const poleview::MarkedRange range = poleview::markedRange(poleview::filterMarks(looks_.before, rows_));
    if (range.first < 0)
        return false;
    if (viewPending_ || desiredView() != view_) {
        viewPending_ = true;
        rebuildView();
    }
    const int vf = poleview::viewRowOf(view_, range.first);
    const int vl = poleview::viewRowOf(view_, range.last);
    sheet_->select({vf, 0}, {vf, 0});
    grid_.focus();

    // Vyns höjd mäts genom att rulla längst ned: rullningen där är innehållet minus synfältet.
    const float h = owner_.style().gridRowH;
    const int n = grid_.rowCount();
    grid_.setScroll(grid_.scrollX(), 0.0f);
    grid_.ensureVisible(n - 1);
    const float s = grid_.scrollY();
    const float viewport = s > 0.0f ? static_cast<float>(n) * h - s : static_cast<float>(n) * h;
    grid_.setScroll(grid_.scrollX(), poleview::scrollTop(vf, vl, n, h, viewport));
    return true;
}

// Vyn som filtret och jämförelsen ger. Jämförelsen räknas alltid om ur rows_, så att vyn aldrig
// ligger efter. Utan jämförelse eller med All är vyn tabellens rader i ordning.
poleview::PoleView PoleGrid::desiredView() const
{
    const int count = static_cast<int>(rows_.size());
    if (!compares_ || filter_ == poleview::Filter::All)
        return poleview::identityView(count);
    return poleview::buildView(count, poleview::filterMarks(looks_.before, rows_), poleview::Filter::Changed);
}

// Ser till att vyn blir den önskade. Under en händelse eller en öppen redigering postas ombyggnaden,
// annars görs den direkt. Är vyn redan rätt görs ingenting.
void PoleGrid::refreshView()
{
    if (desiredView() == view_) {
        viewPending_ = false;
        return;
    }
    viewPending_ = true;
    if (inEvent_ > 0 || grid_.isEditing())
        postRebuild();
    else
        rebuildView();
}

// Postar en ombyggnad av vyn, högst en åt gången. Den utförs bara om den fortfarande väntar.
void PoleGrid::postRebuild()
{
    if (rebuildPosted_)
        return;
    rebuildPosted_ = true;
    postIfAlive(alive_, [this] {
        rebuildPosted_ = false;
        if (viewPending_)
            rebuildView();
    });
}

// Bygger om tabellen till den önskade vyn. Den aktuella cellen följer sin stolpe (en dold stolpe ger
// nästa synliga rad), markeringen följer med och den aktuella raden står kvar på samma höjd.
// Under en öppen redigering görs ingenting, och ombyggnaden står kvar som väntande.
void PoleGrid::rebuildView()
{
    if (grid_.isEditing())
        return;
    poleview::PoleView next = desiredView();
    viewPending_ = false;
    if (next == view_)
        return;

    const int curV = grid_.currentRow();
    const sheetgrid::CellRect sel = sheet_->selection();
    const float h = owner_.style().gridRowH;
    const float scroll = grid_.scrollY();

    // Ankaret: den aktuella raden, annars raden överst i synfältet. offset är dess höjd i synfältet.
    int anchorV = curV;
    if (anchorV < 0)
        anchorV = h > 0.0f ? static_cast<int>(scroll / h) : 0;
    anchorV = std::clamp(anchorV, 0, std::max(0, static_cast<int>(view_.rows.size()) - 1));
    const int anchorM = firstModelRow(view_, anchorV);
    const float offset = static_cast<float>(anchorV) * h - scroll;
    const int m0 = firstModelRow(view_, sel.row0);
    const int m1 = firstModelRow(view_, sel.row1);

    view_ = std::move(next);
    fill();

    if (curV >= 0) {
        const int n0 = poleview::visibleViewRow(view_, m0, +1);
        const int n1 = std::max(n0, poleview::visibleViewRow(view_, m1, -1));
        if (sel.row0 == sel.row1 && sel.column0 == sel.column1)
            sheet_->select({n0, sel.column0}, {n0, sel.column0});
        else
            sheet_->select({n0, sel.column0}, {n1, sel.column1});
    }
    const float top = static_cast<float>(poleview::visibleViewRow(view_, anchorM, +1)) * h - offset;
    grid_.setScroll(grid_.scrollX(), std::max(0.0f, top));
}

// Modellens rader för stolparna i rektangelns vyrader, stigande. Grå rader och den tomma raden
// räknas inte.
std::vector<int> PoleGrid::selectedModelRows(const sheetgrid::CellRect& rect) const
{
    std::vector<int> rows;
    for (int v = std::max(0, rect.row0); v <= rect.row1; ++v) {
        const poleview::ViewRow* vr = viewRowAt(view_, v);
        if (!vr)
            break;
        if (vr->kind == poleview::ViewKind::Pole)
            rows.push_back(vr->first);
    }
    return rows;
}

// Sant när vyn har en grå rad.
bool PoleGrid::hasHiddenRows() const
{
    return std::any_of(view_.rows.begin(), view_.rows.end(),
                       [](const poleview::ViewRow& r) { return r.kind == poleview::ViewKind::Hidden; });
}

// Bygger om tabellen ur vyn: en tabellrad per vyrad. En stolpe får sina celler, en grå rad texten i
// kolumn 0, och den tomma raden sist är tom. Den aktuella cellen sätts tillbaka klampad, eftersom
// setRows nollar den.
void PoleGrid::fill()
{
    const int currentRow = grid_.currentRow();
    const int currentColumn = grid_.currentColumn();

    // Vyn ska alltid ha tabellens radantal; en avvikelse rättas med samma form.
    if (poleview::rowCount(view_) != static_cast<int>(rows_.size()) || view_.rows.empty())
        view_ = poleview::resizeView(view_, static_cast<int>(rows_.size()));

    looks_.coordinateSpans = automode::coordinateSpans(rows_);
    std::vector<glaze::DataGrid::Row> rows;
    rows.reserve(view_.rows.size());
    for (const poleview::ViewRow& vr : view_.rows) {
        if (vr.kind == poleview::ViewKind::Blank) {
            rows.push_back(blankRow(deleteColumn() + 1));
            continue;
        }
        glaze::DataGrid::Row row;
        row.cells.resize(static_cast<std::size_t>(deleteColumn()) + 1);
        if (vr.kind == poleview::ViewKind::Hidden) {
            row.cells[0] = poleview::hiddenText(rows_, vr);
        } else {
            for (int c = 0; c < deleteColumn(); ++c)
                row.cells[static_cast<std::size_t>(c)] = cellText(vr.first, fieldOf(c), true);
        }
        rows.push_back(std::move(row));
    }
    grid_.setRows(std::move(rows));

    const int total = grid_.rowCount();
    for (int v = 0; v < total; ++v)
        decorateRow(v);
    applyLooks();
    if (currentRow >= 0)
        grid_.setCurrentCell(std::clamp(currentRow, 0, total - 1), std::clamp(currentColumn, 0, deleteColumn()));
    sheet_->rowsChanged();
}

// En stolpe i ett skrivbart projekt får en knapp med papperskorg i radera-kolumnen. Den tomma raden
// sist och skrivskyddat läge får en låst cell utan bild, och en grå rad låsta celler i alla kolumner.
void PoleGrid::decorateRow(int viewRow)
{
    const poleview::ViewRow* vr = viewRowAt(view_, viewRow);
    if (!vr)
        return;
    const int del = deleteColumn();
    if (vr->kind == poleview::ViewKind::Hidden) {
        for (int c = 0; c <= del; ++c)
            grid_.setCellEditor(viewRow, c, {glaze::DataGrid::Editor::None});
        return;
    }
    if (vr->kind == poleview::ViewKind::Pole && !readOnly_) {
        grid_.clearCellEditor(viewRow, del);  // den förra tomma raden var låst
        grid_.setCellImage(viewRow, del, glaze::stockIcon(glaze::StockIcon::Trash));
    } else {
        grid_.setCellEditor(viewRow, del, {glaze::DataGrid::Editor::None});
    }
}

// Räknar spannlängderna ur koordinaterna på nytt och skriver dem i de tomma cellerna. Sätter
// tooltips, Stolpplatsens flagga och Σ-bilden ur cellLook, radera-kolumnens ton och papperskorgens
// tooltip, och grå färger och tooltips på de grå raderna. Målar sist om alla celler.
void PoleGrid::applyLooks()
{
    looks_.coordinateSpans = automode::coordinateSpans(rows_);
    const int dataRows = static_cast<int>(rows_.size());
    const int total = grid_.rowCount();
    const int delColumn = deleteColumn();
    const int poleColumn = columnOf(pt::kPole);
    const int spanColumn = columnOf(pt::kSpan);
    for (int v = 0; v < total; ++v) {
        const poleview::ViewRow* vr = viewRowAt(view_, v);
        if (!vr)
            break;
        if (vr->kind == poleview::ViewKind::Hidden) {
            const std::string tip = poleview::hiddenToolTip(*vr);
            for (int c = 0; c < delColumn; ++c)
                grid_.setCellToolTip(v, c, tip);
            grid_.setCellImage(v, poleColumn, glaze::ImageSource{});
            grid_.setCellImage(v, spanColumn, glaze::ImageSource{});
            for (int c = 0; c <= delColumn; ++c)
                grid_.setCellColor(v, c, kHiddenRow, kHiddenText);
            grid_.setCellToolTip(v, delColumn, "");
            continue;
        }
        const int m = vr->first;   // tabellens rad; den tomma raden ger antalet rader
        const std::string_view pole =
            m < dataRows ? std::string_view(rows_[static_cast<std::size_t>(m)].pole) : std::string_view();
        if (m < dataRows && rows_[static_cast<std::size_t>(m)].spanM.empty())
            grid_.setCell(v, spanColumn, cellText(m, pt::kSpan, true));  // tomt eller ur koordinaterna
        for (int c = 0; c < delColumn; ++c) {
            const int f = fieldOf(c);
            const CellLook look = cellLook(looks_, m, f, dataRows, pole);
            grid_.setCellToolTip(v, c, look.toolTip);
            if (f == pt::kPole)
                grid_.setCellImage(v, c, look.flag ? glaze::stockIcon(glaze::StockIcon::Flag) : glaze::ImageSource{});
            if (f == pt::kSpan)
                grid_.setCellImage(v, c, look.computed ? glaze::stockIcon(glaze::StockIcon::Sum) : glaze::ImageSource{});
        }
        const CellLook del = cellLook(looks_, m, -1, dataRows, pole);
        grid_.setCellColor(v, delColumn, del.background, std::nullopt);
        grid_.setCellToolTip(v, delColumn, (m < dataRows && !readOnly_) ? "Radera raden" : "");
    }
    sheet_->paintAll();
}

// En cell sparades. Ett ogiltigt tal i en talkolumn ger ingen ruta, cellen får bara tillbaka sitt
// förra värde. Ett giltigt värde skrivs om i visningsform och tabellen sparas. I den tomma raden
// sist läggs en ny stolpe till, och en ny tom rad kommer efter den. En grå rad ändras aldrig.
// Ändringen kan ändra vyn, och den byggs då om postat.
void PoleGrid::committed(glaze::CellChangeArgs& e)
{
    EventScope scope(*this);
    const OnExit tail{[this] {
        if (viewPending_)
            postRebuild();
    }};
    const int count = static_cast<int>(rows_.size());
    const poleview::ViewRow* vr = viewRowAt(view_, e.row);
    if (readOnly_ || !vr || vr->kind == poleview::ViewKind::Hidden || fieldOf(e.column) < 0)
        return;
    const int r = e.row;                                              // tabellens rad (vyns)
    const int m = vr->kind == poleview::ViewKind::Blank ? count : vr->first;   // modellens rad
    const int gc = e.column;                                          // tabellens kolumn
    const int c = fieldOf(gc);                                        // fältet
    const auto accepted = pt::acceptField(c, grid_.cell(r, gc));

    // Cellens visningstext ur modellen, eller tom text i den tomma raden.
    const auto shown = [this, m, c, count]() {
        return m < count ? cellText(m, c, true) : std::string();
    };
    if (!accepted) {
        grid_.setCell(r, gc, shown());
        return;
    }

    // En spannlängd ur koordinaterna som bekräftas oförändrad (F2 och Enter) förblir uträknad.
    std::string value = *accepted;
    if (c == pt::kSpan && m < count && rows_[static_cast<std::size_t>(m)].spanM.empty() &&
        static_cast<std::size_t>(m) < looks_.coordinateSpans.size() && looks_.coordinateSpans[static_cast<std::size_t>(m)] &&
        tabletext::parseDecimal(value) ==
            tabletext::parseDecimal(computedSpanText(*looks_.coordinateSpans[static_cast<std::size_t>(m)], false)))
        value.clear();

    if (m == count) {
        // Den tomma raden sist: tom text lägger inte till något.
        if (accepted->empty()) {
            grid_.setCell(r, gc, std::string());
            return;
        }
        rows_.emplace_back();
        pt::field(rows_.back(), c) = *accepted;
        grid_.setCell(r, gc, pt::displayText(rows_.back(), c));
        grid_.addRow(blankRow(deleteColumn() + 1).cells);
        view_ = poleview::resizeView(view_, static_cast<int>(rows_.size()));
        decorateRow(r);
        decorateRow(r + 1);
        applyLooks();
        sheet_->rowsChanged();
        const bool wrote = dirty();
        if (save() && wrote)
            host_.polesChanged();
        refreshView();
        return;
    }

    std::string& field = pt::field(rows_[static_cast<std::size_t>(m)], c);
    const std::string previous = field;
    const bool changed = previous != value;
    field = value;
    if (changed && (c == pt::kSpan || c == pt::kNorth || c == pt::kEast))
        looks_.coordinateSpans = automode::coordinateSpans(rows_);  // cellens text före setLooks
    grid_.setCell(r, gc, shown());
    if (!changed)
        return;
    const bool renamed = c == pt::kPole && !previous.empty() && !value.empty();
    const bool wrote = dirty();
    if (save() && wrote) {
        if (renamed)
            host_.poleRenamed(previous, value);
        host_.polesChanged();
    }
    refreshView();
}

// Fältets text ur modellen. En tom Spannlängd föreg. visar värdet ur koordinaterna när det
// finns (looks_.coordinateSpans, som fill, applyLooks och committed håller aktuell).
std::string PoleGrid::cellText(int row, int field, bool group) const
{
    if (row < 0 || row >= static_cast<int>(rows_.size()) || field < 0)
        return std::string();
    const pt::PoleRow& pole = rows_[static_cast<std::size_t>(row)];
    if (field == pt::kSpan && pole.spanM.empty() && static_cast<std::size_t>(row) < looks_.coordinateSpans.size() &&
        looks_.coordinateSpans[static_cast<std::size_t>(row)])
        return computedSpanText(*looks_.coordinateSpans[static_cast<std::size_t>(row)], group);
    return group ? pt::displayText(pole, field) : pt::copyText(pole, field);
}

// Radera-knappen på en stolpe. Frågan postas, så att tabellen inte ändras mitt i sin egen händelse.
// Grå rader och den tomma raden har ingen knapp.
void PoleGrid::cellClicked(glaze::CellClickArgs& e)
{
    EventScope scope(*this);
    const poleview::ViewRow* vr = viewRowAt(view_, e.row);
    if (e.column != deleteColumn() || readOnly_ || !vr || vr->kind != poleview::ViewKind::Pole)
        return;
    const int row = vr->first;
    postIfAlive(alive_, [this, row] { removeRowsAsked({row}); });
}

// Tangenterna i tabellen går till Excel-markeringen. En ombyggnad som väntade på att en
// redigering skulle sluta postas efteråt.
void PoleGrid::keyDown(glaze::KeyArgs& e)
{
    EventScope scope(*this);
    const OnExit tail{[this] {
        if (viewPending_)
            postRebuild();
    }};
    sheet_->keyDown(e);
}

// Musen i tabellen går till Excel-markeringen, som synkar markeringen efter klicket.
void PoleGrid::mouseUp(glaze::MouseArgs& e)
{
    EventScope scope(*this);
    const OnExit tail{[this] {
        if (viewPending_)
            postRebuild();
    }};
    sheet_->mouseUp(e);
}

// Raden och kolumnen under pekaren vid ett högerklick, var för sig -1 utanför tabellens celler.
// Raden kommer ur rowAt och kolumnen ur GridProbe, som tar fönstrets fysiska px.
sheetgrid::CellPos PoleGrid::cellUnderPointer(const glaze::ContextMenuArgs& e)
{
    int row = -1;
    if (const auto p = grid_.fromWindow(gfx::Pointf{e.x, e.y}))
        row = grid_.rowAt(p->x, p->y);
    return {row, GridProbe::columnUnder(grid_, e.x)};
}

// Högerklicket i tabellen. Pekar det på en grå rad visas en meny med bara Visa alla rader. Annars
// noteras cellen under pekaren för Kopiera från Stolptabell före, och en cell utanför markeringen
// markeras, innan Excel-markeringens snabbmeny visas. Utan jämförelse går högerklicket direkt till Excel-markeringen.
void PoleGrid::contextMenu(glaze::ContextMenuArgs& e)
{
    EventScope scope(*this);
    menuCell_ = {-1, -1};
    if (!compares_) {
        sheet_->contextMenu(e);
        return;
    }
    const sheetgrid::CellPos cell = cellUnderPointer(e);
    if (poleview::isHiddenRow(view_, cell.row)) {
        hiddenMenu_.items.clear();
        glaze::MenuItem& showAll = hiddenMenu_.addItem("Visa alla rader");
        showAll.onClick = [this] {
            setFilter(poleview::Filter::All);
            host_.filterChanged(poleview::Filter::All);
            host_.showStatus("Filtret är nu Alla.");
        };
        hiddenMenu_.popupAt(grid_, e.x, e.y);
        return;
    }
    if (cell.row >= 0 && cell.column >= 0)
        menuCell_ = cell;
    if (menuCell_.row >= 0 && menuCell_.column < deleteColumn()) {
        const sheetgrid::CellRect sel = sheet_->selection();
        const bool inside = menuCell_.row >= sel.row0 && menuCell_.row <= sel.row1 &&
                            menuCell_.column >= sel.column0 && menuCell_.column <= sel.column1;
        if (!inside)
            sheet_->select(menuCell_, menuCell_);
    }
    sheet_->contextMenu(e);
}

// Skriver ändringarna i rows_ (Delete, Ctrl+X och inklistring utan grå rader): vyns rader blir
// modellens rader, nya rader läggs till och helt tomma rader sist tas bort. Vyn behåller sin form
// tills den byggs om, så att SheetGrids markering stämmer. Sparas som EN ändring.
void PoleGrid::applyEdits(const std::vector<sheetgrid::CellEdit>& edits)
{
    if (readOnly_)
        return;
    EventScope scope(*this);
    std::vector<pt::Edit> converted;
    converted.reserve(edits.size());
    for (const auto& edit : edits) {
        const int m = poleview::modelRow(view_, edit.row);
        const int f = fieldOf(edit.column);
        if (m >= 0 && f >= 0)
            converted.push_back({m, f, edit.text});
    }
    pt::applyEdits(rows_, converted);
    pt::trimTrailingEmptyRows(rows_);
    view_ = poleview::resizeView(view_, static_cast<int>(rows_.size()));
    fill();
    const bool wrote = dirty();
    if (save() && wrote)
        host_.polesChanged();
    refreshView();
}

// Klistrar in ett block när vyn har grå rader. Blocket skrivs i modellens rader i följd från ankarets
// rad, också i dolda rader, som om alla rader syntes. Vyn byggs om direkt, så de rader som blev
// förändrade syns, och markeringen blir det inklistrade området. Svarar false utan grå rader.
bool PoleGrid::pasteIntoModel(const std::vector<std::vector<std::string>>& block, sheetgrid::CellPos anchor)
{
    if (!hasHiddenRows())
        return false;
    EventScope scope(*this);
    if (readOnly_)
        return true;
    const int m = poleview::modelRow(view_, anchor.row);
    if (m < 0)
        return true;

    // Planeringen mot modellen: tabellens form med den tomma raden som en rad och utan grå rader.
    sheetgrid::SheetShape shape = baseShape();
    shape.rowCount = static_cast<int>(rows_.size()) + 1;
    const sheetgrid::SheetPaste paste = sheetgrid::planSheetPaste({m, anchor.column}, block, shape);
    if (paste.rejected) {
        sheetgrid::showPasteRejected(grid_, *paste.rejected);
        return true;
    }
    if (paste.edits.empty()) {
        host_.showStatus(sheetgrid::nothingToPasteText());
        return true;
    }

    std::vector<pt::Edit> converted;
    converted.reserve(paste.edits.size());
    for (const auto& edit : paste.edits) {
        const int f = fieldOf(edit.column);
        if (f >= 0)
            converted.push_back({edit.row, f, edit.text});
    }
    pt::applyEdits(rows_, converted);
    pt::trimTrailingEmptyRows(rows_);
    view_ = desiredView();
    viewPending_ = false;
    fill();

    const int n0 = poleview::visibleViewRow(view_, paste.area.row0, +1);
    const int n1 = std::max(n0, poleview::visibleViewRow(view_, paste.area.row1, -1));
    sheet_->select({n0, paste.area.column0}, {n1, paste.area.column1});

    const bool wrote = dirty();
    if (save() && wrote)
        host_.polesChanged();

    host_.showStatus(sheetgrid::pasteStatus(paste, block.size()));
    return true;
}

// Sant när raderna skiljer sig från det senast sparade.
bool PoleGrid::dirty() const
{
    return pt::rowsToSave(rows_) != saved_;
}

// Sparar hela tabellen via värden när den skiljer sig från det senast sparade. Vid fel postas en
// omläsning (så att tabellen inte byggs om mitt i sin egen händelse). Med radid får nya rader id
// först, så att raderna i tabellen och i projektet har samma id. Svarar om det lyckades.
bool PoleGrid::save()
{
    if (rowIds_)
        pt::assignRowIds(rows_);
    const auto toSave = pt::rowsToSave(rows_);
    if (toSave == saved_)
        return true;
    if (!host_.savePoles(rows_)) {
        postIfAlive(alive_, [this] { setRows(host_.reloadPoles()); });
        return false;
    }
    saved_ = toSave;
    return true;
}

// Infogar en tom rad ovanför modellens rad, bygger om vyn och tabellen, markerar den nya raden och
// sparar. Tomma rader mitt i tabellen sparas. Den tomma raden är förändrad och syns.
void PoleGrid::insertRowAbove(int modelRow)
{
    if (readOnly_ || modelRow < 0 || modelRow >= static_cast<int>(rows_.size()))
        return;
    owner_.commitEdits();
    rows_.insert(rows_.begin() + modelRow, pt::PoleRow{});
    view_ = desiredView();
    viewPending_ = false;
    fill();
    const int v = poleview::visibleViewRow(view_, modelRow, +1);
    sheet_->select({v, 0}, {v, 0});
    const bool wrote = dirty();
    if (save() && wrote)
        host_.polesChanged();
}

// Frågar "Radera stolpnr ST0381?" med Ja förvalt, och en notis från värden om raderna är start eller
// slut för en sträcka. Är raderna inte en följd (en grå rad däremellan) säger frågan att dolda rader
// inte raderas. Vid Ja tas raderna bort, markeringen hamnar på raden som tog deras plats och
// tabellen sparas.
void PoleGrid::removeRowsAsked(std::vector<int> modelRows)
{
    if (readOnly_)
        return;
    owner_.commitEdits();
    const int count = static_cast<int>(rows_.size());
    std::sort(modelRows.begin(), modelRows.end());
    modelRows.erase(std::unique(modelRows.begin(), modelRows.end()), modelRows.end());
    modelRows.erase(std::remove_if(modelRows.begin(), modelRows.end(), [count](int r) { return r < 0 || r >= count; }),
                    modelRows.end());
    if (modelRows.empty())
        return;

    const int first = modelRows.front();
    const int last = modelRows.back();
    const int n = static_cast<int>(modelRows.size());
    const std::string& firstPole = rows_[static_cast<std::size_t>(first)].pole;
    const std::string& lastPole = rows_[static_cast<std::size_t>(last)].pole;
    std::string text;
    if (last - first + 1 != n) {
        // Dolda rader däremellan: frågan räknar bara de synliga. Med ett enda synligt område händer det
        // bara kort, när en ny stolpe står efter den sista grå raden tills vyn byggs om.
        text = !firstPole.empty() && !lastPole.empty()
                   ? "Radera " + std::to_string(n) + " synliga rader mellan stolpnr " + firstPole + " och " + lastPole +
                         "? Dolda rader däremellan raderas inte."
                   : "Radera " + std::to_string(n) + " synliga rader mellan rad " + std::to_string(first + 1) +
                         " och rad " + std::to_string(last + 1) + "? Dolda rader däremellan raderas inte.";
    } else if (n == 1) {
        text = firstPole.empty() ? "Radera rad " + std::to_string(first + 1) + "?"
                                 : "Radera stolpnr " + firstPole + "?";
    } else if (!firstPole.empty() && !lastPole.empty()) {
        text = "Radera stolpnr " + firstPole + " – " + lastPole + " (" + std::to_string(n) + " rader)?";
    } else {
        text = "Radera rad " + std::to_string(first + 1) + " – " + std::to_string(last + 1) + " (" +
               std::to_string(n) + " rader)?";
    }

    // Notiserna: en per sammanhängande följd, skilda med radbrytning.
    std::string note;
    for (std::size_t i = 0; i < modelRows.size();) {
        std::size_t j = i;
        while (j + 1 < modelRows.size() && modelRows[j + 1] == modelRows[j] + 1)
            ++j;
        const std::string part = host_.removeNote(modelRows[i], modelRows[j]);
        if (!part.empty()) {
            if (!note.empty())
                note += kNl;
            note += part;
        }
        i = j + 1;
    }
    if (!note.empty())
        text += kNl + kNl + note;
    if (glaze::messageBox(owner_, text, "Radera", glaze::MessageBoxButtons::YesNo,
                          glaze::MessageBoxType::Warning) != glaze::DialogResult::Yes)
        return;

    for (auto it = modelRows.rbegin(); it != modelRows.rend(); ++it)
        rows_.erase(rows_.begin() + *it);
    view_ = desiredView();
    viewPending_ = false;
    fill();
    const int target = poleview::visibleViewRow(view_, std::min(first, static_cast<int>(rows_.size())), +1);
    sheet_->select({target, 0}, {target, 0});
    const bool wrote = dirty();
    if (save() && wrote)
        host_.polesChanged();
    host_.showStatus(n == 1 ? std::string("Raderade 1 rad.") : "Raderade " + std::to_string(n) + " rader.");
}

// Skriver tillbaka värdet ur Stolptabell före i en cell (modellens rad och fältet) i en Changed-rad,
// som en vanlig ändring: sparas direkt, och färgerna, beräkningarna och filtret räknas om.
// Statusraden säger vilken cell som fick vilket värde.
void PoleGrid::restoreCell(int modelRow, int field)
{
    if (readOnly_)
        return;
    owner_.commitEdits();
    if (modelRow < 0 || modelRow >= static_cast<int>(rows_.size()))
        return;
    const std::vector<automode::RowCompare> compare = automode::compareAfter(looks_.before, rows_);
    const std::optional<pt::Edit> edit = automode::restoreFromBefore(looks_.before, compare, modelRow, field);
    if (!edit)
        return;
    pt::applyEdits(rows_, {*edit});
    fill();
    const bool wrote = dirty();
    if (save() && wrote)
        host_.polesChanged();

    const int beforeRow = compare[static_cast<std::size_t>(modelRow)].beforeRow;
    host_.showStatus(fieldName(field) + " för " + poleview::rowLabel(rows_, modelRow) +
                     " är nu som i Stolptabell före: " + beforeText(looks_.before, beforeRow, field) + ".");
    refreshView();
}

} // namespace kalkyl::polegrid
