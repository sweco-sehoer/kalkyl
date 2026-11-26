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

// Den tomma raden sist i tabellen, där en ny stolpe skrivs: tomma celler i alla nio kolumner.
glaze::DataGrid::Row blankRow()
{
    glaze::DataGrid::Row blank;
    blank.cells.assign(kDeleteColumn + 1, std::string());
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

} // namespace

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
    if (!isData || column < 0 || column >= pt::kColumnCount)
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
            std::string shown = "tom";
            if (cmp->beforeRow >= 0 && static_cast<std::size_t>(cmp->beforeRow) < looks.before.size()) {
                const std::string text = pt::displayText(looks.before[static_cast<std::size_t>(cmp->beforeRow)], column);
                if (!text.empty())
                    shown = text;
            }
            appendTip(tip, "I Stolptabell före: " + shown);
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

// Adaptern till SheetGrid: texten en cell kopieras som, cellens färg utan markering och
// snabbmenyns egna poster. Ändringar och statustexter går vidare till PoleGrid och värden.
class PoleGrid::Adapter : public sheetgrid::SheetHost {
public:
    explicit Adapter(PoleGrid& grid) : grid_(grid) {}

    // Cellens text i urklippet, med decimalkomma och utan gruppering, också en spannlängd ur
    // koordinaterna. Den tomma raden sist ger tom text.
    std::string copyText(int row, int column) const override
    {
        if (row < 0 || row >= static_cast<int>(grid_.rows_.size()))
            return std::string();
        return grid_.cellText(row, column, false);
    }

    // Cellens färg utan markering: radens ton och jämförelsens färg ur cellLook.
    std::optional<gfx::Color> baseColor(int row, int column) const override
    {
        const int count = static_cast<int>(grid_.rows_.size());
        const std::string_view pole =
            (row >= 0 && row < count) ? std::string_view(grid_.rows_[static_cast<std::size_t>(row)].pole)
                                      : std::string_view();
        return cellLook(grid_.looks_, row, column, count, pole).background;
    }

    // Inklistrade och raderade celler skrivs i modellen.
    void applyEdits(const std::vector<sheetgrid::CellEdit>& edits) override { grid_.applyEdits(edits); }

    // Snabbmenyns egna poster: Infoga rad ovanför och Radera rader…, och sedan värdens poster.
    void addMenuItems(glaze::Menu& menu, const sheetgrid::CellRect& selection) override
    {
        PoleGrid* grid = &grid_;
        const int count = static_cast<int>(grid_.rows_.size());
        const bool writable = !grid_.readOnly_;
        const int row0 = selection.row0;
        const int row1 = std::min(selection.row1, count - 1);

        glaze::MenuItem& insertItem = menu.addItem("Infoga rad ovanför");
        insertItem.enabled = writable && row0 >= 0 && row0 < count;
        insertItem.onClick = [grid, row0] { grid->insertRowAbove(row0); };

        glaze::MenuItem& removeItem = menu.addItem("Radera rader…");
        removeItem.enabled = writable && row0 <= row1 && row0 < count;
        removeItem.onClick = [grid, row0, row1] {
            postIfAlive(grid->alive_, [grid, row0, row1] { grid->removeRowsAsked(row0, row1); });
        };

        grid_.host_.addMenuItems(menu, (row0 >= 0 && row0 < count) ? row0 : -1);
    }

    // En kort text i statusraden.
    void showStatus(const std::string& text) override { grid_.host_.showStatus(text); }

private:
    PoleGrid& grid_;
};

// Skapar Excel-markeringen: kolumn 0-7 tar emot inklistring, och nya rader får läggas till under
// tabellen. Tabellen får sina rader först med setRows.
PoleGrid::PoleGrid(glaze::AppWindow& owner, glaze::DataGrid& grid, PoleGridHost& host, std::string tableName)
    : owner_(owner), grid_(grid), host_(host), tableName_(std::move(tableName))
{
    sheetgrid::SheetShape shape;
    shape.rowCount = grid_.rowCount();
    shape.columnCount = pt::kColumnCount;
    shape.firstColumn = 0;
    shape.isInput = [](int, int column) { return column >= 0 && column < pt::kColumnCount; };
    shape.accept = [](int, int column, const std::string& field) { return pt::acceptField(column, field); };
    shape.columnName = [](int column) { return pt::columnName(column); };
    shape.growRows = true;
    shape.headerTakesRow = false;
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
    for (int c = 0; c < pt::kColumnCount; ++c) {
        if (readOnly_)
            grid_.setColumnEditor(c, {.editor = glaze::DataGrid::Editor::Text, .readOnly = true});
        else
            grid_.setColumnEditor(c, {glaze::DataGrid::Editor::Text});
    }
    grid_.setColumnEditor(kDeleteColumn, {readOnly_ ? glaze::DataGrid::Editor::None : glaze::DataGrid::Editor::Button});
    sheet_->setReadOnly(readOnly_);
    fill();
}

// Tar emot raderna ur projektet. De blir också det senast sparade, och tabellen byggs om.
void PoleGrid::setRows(std::vector<poletable::PoleRow> rows)
{
    rows_ = std::move(rows);
    saved_ = pt::rowsToSave(rows_);
    fill();
}

// Raderna som de står i modellen, utan den tomma raden sist.
const std::vector<poletable::PoleRow>& PoleGrid::rows() const
{
    return rows_;
}

// Nya färger och märken. Tooltips och flaggor sätts om och alla celler målas om, men raderna
// byggs inte om.
void PoleGrid::setLooks(PoleLooks looks)
{
    looks_ = std::move(looks);
    applyLooks();
}

// Gör raden aktuell, rullar den i sikte och markerar dess Stolpplats.
void PoleGrid::showRow(int row)
{
    if (row < 0 || row >= static_cast<int>(rows_.size()))
        return;
    sheet_->select({row, 0}, {row, 0});
}

// Bygger om tabellen: en rad per stolpe ur rows_ och den tomma raden sist. Raderna får knapp och
// färger, och den aktuella cellen sätts tillbaka klampad, eftersom setRows nollar den.
void PoleGrid::fill()
{
    const int currentRow = grid_.currentRow();
    const int currentColumn = grid_.currentColumn();

    looks_.coordinateSpans = automode::coordinateSpans(rows_);
    std::vector<glaze::DataGrid::Row> rows;
    rows.reserve(rows_.size() + 1);
    for (int r = 0; r < static_cast<int>(rows_.size()); ++r) {
        glaze::DataGrid::Row row;
        row.cells.resize(kDeleteColumn + 1);
        for (int c = 0; c < pt::kColumnCount; ++c)
            row.cells[static_cast<std::size_t>(c)] = cellText(r, c, true);
        rows.push_back(std::move(row));
    }
    rows.push_back(blankRow());
    grid_.setRows(std::move(rows));

    const int total = grid_.rowCount();
    for (int r = 0; r < total; ++r)
        decorateRow(r);
    applyLooks();
    if (currentRow >= 0)
        grid_.setCurrentCell(std::clamp(currentRow, 0, total - 1), std::clamp(currentColumn, 0, kDeleteColumn));
    sheet_->rowsChanged();
}

// En datarad i ett skrivbart projekt får en knapp med papperskorg i radera-kolumnen. Den tomma
// raden sist och skrivskyddat läge får en låst cell utan bild.
void PoleGrid::decorateRow(int row)
{
    if (row < static_cast<int>(rows_.size()) && !readOnly_) {
        grid_.clearCellEditor(row, kDeleteColumn);  // den förra tomma raden var låst
        grid_.setCellImage(row, kDeleteColumn, glaze::stockIcon(glaze::StockIcon::Trash));
    } else {
        grid_.setCellEditor(row, kDeleteColumn, {glaze::DataGrid::Editor::None});
    }
}

// Räknar spannlängderna ur koordinaterna på nytt och skriver dem i de tomma cellerna. Sätter
// tooltips, Stolpplatsens flagga och Σ-bilden ur cellLook, radera-kolumnens ton och papperskorgens
// tooltip, och målar sist om alla celler.
void PoleGrid::applyLooks()
{
    looks_.coordinateSpans = automode::coordinateSpans(rows_);
    const int dataRows = static_cast<int>(rows_.size());
    const int total = grid_.rowCount();
    for (int r = 0; r < total; ++r) {
        const std::string_view pole =
            r < dataRows ? std::string_view(rows_[static_cast<std::size_t>(r)].pole) : std::string_view();
        if (r < dataRows && rows_[static_cast<std::size_t>(r)].spanM.empty())
            grid_.setCell(r, pt::kSpan, cellText(r, pt::kSpan, true));  // tomt eller ur koordinaterna
        for (int c = 0; c < pt::kColumnCount; ++c) {
            const CellLook look = cellLook(looks_, r, c, dataRows, pole);
            grid_.setCellToolTip(r, c, look.toolTip);
            if (c == pt::kPole)
                grid_.setCellImage(r, c, look.flag ? glaze::stockIcon(glaze::StockIcon::Flag) : glaze::ImageSource{});
            if (c == pt::kSpan)
                grid_.setCellImage(r, c, look.computed ? glaze::stockIcon(glaze::StockIcon::Sum) : glaze::ImageSource{});
        }
        const CellLook del = cellLook(looks_, r, kDeleteColumn, dataRows, pole);
        grid_.setCellColor(r, kDeleteColumn, del.background, std::nullopt);
        grid_.setCellToolTip(r, kDeleteColumn, (r < dataRows && !readOnly_) ? "Radera raden" : "");
    }
    sheet_->paintAll();
}

// En cell sparades. Ett ogiltigt tal i en talkolumn ger ingen ruta, cellen får bara tillbaka sitt
// förra värde. Ett giltigt värde skrivs om i visningsform och tabellen sparas. I den tomma raden
// sist läggs en ny stolpe till, och en ny tom rad kommer efter den.
void PoleGrid::committed(glaze::CellChangeArgs& e)
{
    const int count = static_cast<int>(rows_.size());
    if (readOnly_ || e.row < 0 || e.row > count || e.column < 0 || e.column >= pt::kColumnCount)
        return;
    const int r = e.row;
    const int c = e.column;
    const auto accepted = pt::acceptField(c, grid_.cell(r, c));

    // Cellens visningstext ur modellen, eller tom text i den tomma raden.
    const auto shown = [this, r, c, count]() {
        return r < count ? cellText(r, c, true) : std::string();
    };
    if (!accepted) {
        grid_.setCell(r, c, shown());
        return;
    }

    // En spannlängd ur koordinaterna som bekräftas oförändrad (F2 och Enter) förblir uträknad.
    std::string value = *accepted;
    if (c == pt::kSpan && r < count && rows_[static_cast<std::size_t>(r)].spanM.empty() &&
        static_cast<std::size_t>(r) < looks_.coordinateSpans.size() && looks_.coordinateSpans[static_cast<std::size_t>(r)] &&
        tabletext::parseDecimal(value) ==
            tabletext::parseDecimal(computedSpanText(*looks_.coordinateSpans[static_cast<std::size_t>(r)], false)))
        value.clear();

    if (r == count) {
        // Den tomma raden sist: tom text lägger inte till något.
        if (accepted->empty()) {
            grid_.setCell(r, c, std::string());
            return;
        }
        rows_.emplace_back();
        pt::field(rows_.back(), c) = *accepted;
        grid_.setCell(r, c, pt::displayText(rows_.back(), c));
        grid_.addRow(blankRow().cells);
        decorateRow(r);
        decorateRow(r + 1);
        applyLooks();
        sheet_->rowsChanged();
        const bool wrote = dirty();
        if (save() && wrote)
            host_.polesChanged();
        return;
    }

    std::string& field = pt::field(rows_[static_cast<std::size_t>(r)], c);
    const std::string previous = field;
    const bool changed = previous != value;
    field = value;
    if (changed && (c == pt::kSpan || c == pt::kNorth || c == pt::kEast))
        looks_.coordinateSpans = automode::coordinateSpans(rows_);  // cellens text före setLooks
    grid_.setCell(r, c, shown());
    if (!changed)
        return;
    const bool renamed = c == pt::kPole && !previous.empty() && !value.empty();
    const bool wrote = dirty();
    if (save() && wrote) {
        if (renamed)
            host_.poleRenamed(previous, value);
        host_.polesChanged();
    }
}

// Cellens text ur modellen. En tom Spannlängd föreg. visar värdet ur koordinaterna när det
// finns (looks_.coordinateSpans, som fill, applyLooks och committed håller aktuell).
std::string PoleGrid::cellText(int row, int column, bool group) const
{
    if (row < 0 || row >= static_cast<int>(rows_.size()))
        return std::string();
    const pt::PoleRow& pole = rows_[static_cast<std::size_t>(row)];
    if (column == pt::kSpan && pole.spanM.empty() && static_cast<std::size_t>(row) < looks_.coordinateSpans.size() &&
        looks_.coordinateSpans[static_cast<std::size_t>(row)])
        return computedSpanText(*looks_.coordinateSpans[static_cast<std::size_t>(row)], group);
    return group ? pt::displayText(pole, column) : pt::copyText(pole, column);
}

// Radera-knappen på en rad. Frågan postas, så att tabellen inte ändras mitt i sin egen händelse.
void PoleGrid::cellClicked(glaze::CellClickArgs& e)
{
    if (e.column != kDeleteColumn || readOnly_ || e.row < 0 || e.row >= static_cast<int>(rows_.size()))
        return;
    const int row = e.row;
    postIfAlive(alive_, [this, row] { removeRowsAsked(row, row); });
}

// Tangenterna i tabellen går till Excel-markeringen.
void PoleGrid::keyDown(glaze::KeyArgs& e)
{
    sheet_->keyDown(e);
}

// Musen i tabellen går till Excel-markeringen, som synkar markeringen efter klicket.
void PoleGrid::mouseUp(glaze::MouseArgs& e)
{
    sheet_->mouseUp(e);
}

// Högerklicket i tabellen visar Excel-markeringens snabbmeny.
void PoleGrid::contextMenu(glaze::ContextMenuArgs& e)
{
    sheet_->contextMenu(e);
}

// Skriver ändringarna i rows_: nya rader läggs till, helt tomma rader sist tas bort.
// Tabellen byggs om och sparas som EN ändring.
void PoleGrid::applyEdits(const std::vector<sheetgrid::CellEdit>& edits)
{
    if (readOnly_)
        return;
    std::vector<pt::Edit> converted;
    converted.reserve(edits.size());
    for (const auto& edit : edits)
        converted.push_back({edit.row, edit.column, edit.text});
    pt::applyEdits(rows_, converted);
    pt::trimTrailingEmptyRows(rows_);
    fill();
    const bool wrote = dirty();
    if (save() && wrote)
        host_.polesChanged();
}

// Sant när raderna skiljer sig från det senast sparade.
bool PoleGrid::dirty() const
{
    return pt::rowsToSave(rows_) != saved_;
}

// Sparar hela tabellen via värden när den skiljer sig från det senast sparade. Vid fel postas en
// omläsning (så att tabellen inte byggs om mitt i sin egen händelse). Svarar om det lyckades.
bool PoleGrid::save()
{
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

// Infogar en tom rad vid row, bygger om tabellen, markerar den nya raden och sparar. Tomma rader
// mitt i tabellen sparas.
void PoleGrid::insertRowAbove(int row)
{
    if (readOnly_ || row < 0 || row >= static_cast<int>(rows_.size()))
        return;
    owner_.commitEdits();
    rows_.insert(rows_.begin() + row, pt::PoleRow{});
    fill();
    sheet_->select({row, 0}, {row, 0});
    const bool wrote = dirty();
    if (save() && wrote)
        host_.polesChanged();
}

// Frågar "Radera stolpplats ST0381?" med Ja förvalt, och en notis från värden om raderna är
// start eller slut för en sträcka. Vid Ja tas raderna first till last bort, markeringen hamnar
// på raden som tog deras plats och tabellen sparas.
void PoleGrid::removeRowsAsked(int first, int last)
{
    if (readOnly_)
        return;
    owner_.commitEdits();
    const int count = static_cast<int>(rows_.size());
    first = std::max(first, 0);
    last = std::min(last, count - 1);
    if (first > last)
        return;

    const std::string& firstPole = rows_[static_cast<std::size_t>(first)].pole;
    const std::string& lastPole = rows_[static_cast<std::size_t>(last)].pole;
    const int n = last - first + 1;
    std::string text;
    if (n == 1) {
        text = firstPole.empty() ? "Radera rad " + std::to_string(first + 1) + "?"
                                 : "Radera stolpnr " + firstPole + "?";
    } else if (!firstPole.empty() && !lastPole.empty()) {
        text = "Radera stolpnr " + firstPole + " – " + lastPole + " (" + std::to_string(n) + " rader)?";
    } else {
        text = "Radera rad " + std::to_string(first + 1) + " – " + std::to_string(last + 1) + " (" +
               std::to_string(n) + " rader)?";
    }
    const std::string note = host_.removeNote(first, last);
    if (!note.empty())
        text += kNl + kNl + note;
    if (glaze::messageBox(owner_, text, "Radera", glaze::MessageBoxButtons::YesNo,
                          glaze::MessageBoxType::Warning) != glaze::DialogResult::Yes)
        return;

    rows_.erase(rows_.begin() + first, rows_.begin() + last + 1);
    fill();
    const int target = std::min(first, static_cast<int>(rows_.size()));
    sheet_->select({target, 0}, {target, 0});
    const bool wrote = dirty();
    if (save() && wrote)
        host_.polesChanged();
    host_.showStatus(n == 1 ? std::string("Raderade 1 rad.") : "Raderade " + std::to_string(n) + " rader.");
}

} // namespace kalkyl::polegrid
