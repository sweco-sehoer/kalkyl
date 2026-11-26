#include "normal_span_dialog.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <functional>
#include <limits>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "tableText/table_text.h"
#include "projectdb/project_data.h"

namespace {

namespace ancalc = kalkyl::ancalc;
namespace tabletext = kalkyl::tabletext;
namespace projectdb = kalkyl::projectdb;

// Tabellens kolumner: indata (Stpnr, Spannlängd, H) och kontrollerna 1-5.
constexpr int kColPole = 0;
constexpr int kColLength = 1;
constexpr int kColHeight = 2;
constexpr int kInputColumns = 3;
constexpr int kColumnCount = 8;

// Ett färgmärke per cell. Märket översätts till en färg i baseColor.
constexpr int kMarkNone = 0;
constexpr int kMarkRed = 1;
constexpr int kMarkGreen = 2;
constexpr int kMarkError = 3;
constexpr int kMarkFirst = 4;

// Färgerna i tabellen: fel, första spannet, röd och grön kontroll samt markeringen.
const gfx::Color kErrorColor{220, 60, 60, 80};
const gfx::Color kFirstColor{128, 128, 128, 48};
const gfx::Color kRedColor{200, 0, 0, 90};
const gfx::Color kGreenColor{0, 150, 0, 64};
const gfx::Color kSelectColor{60, 130, 210, 90};

// Texten på antal avvikelser som visas efter varje kontroll är röd när något avviker.
const gfx::Color kCountRed{210, 50, 50};

// Radbrytningen som sträng, eftersom källan inte får innehålla escape-sekvenser.
std::string newline()
{
    return std::string(1, char{10});
}

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

// Tabellkolumnen som ett fel gäller, eller -1 när felet gäller hela beräkningen.
int columnOf(ancalc::SpanColumn column)
{
    switch (column) {
    case ancalc::SpanColumn::Pole: return kColPole;
    case ancalc::SpanColumn::Length: return kColLength;
    case ancalc::SpanColumn::Height: return kColHeight;
    case ancalc::SpanColumn::None: break;
    }
    return -1;
}

// Talet med decimalkomma och hårda mellanslag som tusentalsavgränsare, till resultatpanelen.
std::string groupedText(double value, int decimals)
{
    return tabletext::formatDecimalText(tabletext::formatDecimal(value, decimals, '.'), ',', true);
}

// Antalet avvikelser som text, eller ett streck när ingen beräkning finns.
std::string countText(int count)
{
    return std::to_string(count) + " avvikelser";
}

// Postar ett anrop till UI-tråden som hoppas över om fönstret hunnit försvinna.
void postIfAlive(const std::shared_ptr<bool>& alive, std::function<void()> fn)
{
    std::weak_ptr<bool> weak = alive;
    glaze::app().post([weak, fn = std::move(fn)] {
        if (weak.lock())
            fn();
    });
}

// Sant när urklippet är ett block: flera rader eller flera fält.
// En enda cell från Excel (med avslutande radbrytning) räknas inte.
bool isBlock(const std::string& text)
{
    const auto rows = tabletext::parseTsv(text);
    return rows.size() > 1 || (!rows.empty() && rows.front().size() > 1);
}

} // namespace

// Anger projektet som formuläret läser och sparar i.
// Objektet ägs av main och måste leva längre än fönstret.
void NormalSpanDialog::setProject(kalkyl::projectdb::ProjectDb& project)
{
    project_ = &project;
}

// En ny beräkning, med raden i Dokumentation som ska visa den efter OK (tom = lös).
void NormalSpanDialog::setNew(std::string linkDocKey)
{
    linkDocKey_ = std::move(linkDocKey);
    existingId_ = 0;
    copyOfId_ = 0;
}

// En befintlig beräkning, som läses ur projektet när fönstret laddas.
void NormalSpanDialog::setExisting(std::int64_t id)
{
    existingId_ = id;
    copyOfId_ = 0;
    linkDocKey_.clear();
}

// En ny, lös beräkning som börjar som en kopia av beräkningen id.
void NormalSpanDialog::setCopyOf(std::int64_t id)
{
    copyOfId_ = id;
    existingId_ = 0;
    linkDocKey_.clear();
}

// Radtexterna i Dokumentation per nyckel, till upplysningen om var beräkningen visas.
void NormalSpanDialog::setDocRowLabels(std::vector<std::pair<std::string, std::string>> labels)
{
    docLabels_ = std::move(labels);
}

// Skrivskyddat läge: allt visas men inget kan ändras. Gäller från och med onLoad.
void NormalSpanDialog::setReadOnly(bool readOnly)
{
    readOnly_ = readOnly;
}

// Id för den sparade beräkningen efter run(), tomt om inget sparades.
std::optional<std::int64_t> NormalSpanDialog::savedId() const
{
    return savedId_;
}

// Startlogik, körs när fönstret laddas (aldrig i konstruktorn).
// Läser beräkningen, räknar första gången och markerar första cellen.
void NormalSpanDialog::onLoad(glaze::Widget*, glaze::EventArgs&)
{
    if (!project_)
        return;
    if (!loadCalculation()) {
        postIfAlive(alive_, [this] { closeWithResult(glaze::DialogResult::Cancel); });
        return;
    }
    if (readOnly_)
        applyReadOnly();
    if (automatic_)
        setTitle("Normalspannsberäkning (automatisk)");
    recalc();
    if (copyOfId_ != 0) {
        std::string note = noteTxt.text();
        if (note.empty()) {
            if (calc_.result)
                note = calc_.result->fromPole + " – " + calc_.result->toPole;
            else
                note = "beräkning";
        }
        noteTxt.setText("Kopia av " + note);
    }
    setSelection({0, 0}, {0, 0}, true);
}

// Krysset, Avbryt och Escape: frågar om osparade ändringar innan fönstret stängs.
// Ja sparar (och stannar om det misslyckas), Nej kastar ändringarna, Avbryt stannar.
void NormalSpanDialog::onClosing(glaze::Widget*, glaze::CloseArgs& e)
{
    if (saved_ || readOnly_ || e.reason != glaze::CloseReason::UserClosing)
        return;
    commitEdits();
    if (!dirty_)
        return;

    const auto answer = glaze::messageBox(
        *this, "Du har ändrat normalspannsberäkningen. Vill du spara ändringarna?",
        "Normalspannsberäkning", glaze::MessageBoxButtons::YesNoCancel,
        glaze::MessageBoxType::Warning);
    if (answer == glaze::DialogResult::Yes) {
        if (!saveChanges())
            e.cancel = true;
    } else if (answer != glaze::DialogResult::No) {
        e.cancel = true;
    }
}

// OK: sparar beräkningen i projektet och stänger. Vid fel visas det och fönstret står kvar.
void NormalSpanDialog::okBtnClicked(glaze::Widget*, glaze::ClickArgs&)
{
    if (readOnly_)
        return;
    if (saveChanges())
        closeWithResult(glaze::DialogResult::OK);
}

// Töm: frågar och tömmer sedan hela tabellen så att bara en tom rad återstår.
void NormalSpanDialog::clearBtnClicked(glaze::Widget*, glaze::ClickArgs&)
{
    if (readOnly_)
        return;
    if (glaze::messageBox(*this, "Töm hela tabellen?", "Normalspannsberäkning",
                          glaze::MessageBoxButtons::YesNo,
                          glaze::MessageBoxType::Confirmation) != glaze::DialogResult::Yes)
        return;
    poleGrid.setRows({});
    ensureTrailingRow();
    dirty_ = true;
    anchor_ = {0, 0};
    extent_ = {0, 0};
    recalc();
    setSelection({0, 0}, {0, 0}, true);
}

// Anteckningen ändrades av användaren.
void NormalSpanDialog::noteChanged(glaze::Widget*, glaze::ChangeArgs&)
{
    dirty_ = true;
}

// En gräns ändrades: kontrollerna räknas om med de nya gränserna.
void NormalSpanDialog::limitCommitted(glaze::Widget*, glaze::ChangeArgs&)
{
    dirty_ = true;
    recalc();
}

// En cell sparades av användaren. Skrevs den i sista raden läggs en ny tom rad till,
// och sedan räknas allt om.
void NormalSpanDialog::poleGridCommitted(glaze::Widget*, glaze::CellChangeArgs&)
{
    dirty_ = true;
    ensureTrailingRow();
    recalc();
}

// Tangenter i tabellen. Under redigering tas bara Ctrl+V med ett block. Annars gäller
// Ctrl+C, X, V, A, Delete och markering med Shift. Övriga tangenter lämnas åt tabellen,
// och markeringen synkas efteråt med den aktuella cellen.
void NormalSpanDialog::poleGridKeyDown(glaze::Widget*, glaze::KeyArgs& e)
{
    const bool ctrl = (e.mods & gfx::kModCtrl) != 0 && (e.mods & gfx::kModAlt) == 0;
    const bool shift = (e.mods & gfx::kModShift) != 0;

    if (poleGrid.isEditing()) {
        if (ctrl && !shift && e.key == gfx::Key::V && !readOnly_ && isBlock(gfx::clipboardText())) {
            e.handled = true;
            const CellPos cell{poleGrid.currentRow(), poleGrid.currentColumn()};
            poleGrid.cancelEdit();
            pasteAt(cell);
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
            if (!readOnly_) {
                const CellRect rect = selectionRect();
                pasteAt({rect.row0, rect.column0});
            }
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
        bool handled = true;
        if (ctrl) {
            if (e.key == gfx::Key::Home)
                next = {0, 0};
            else if (e.key == gfx::Key::End)
                next = {lastDataRow(), kColumnCount - 1};
            else
                handled = false;
        } else {
            switch (e.key) {
            case gfx::Key::Left: --next.column; break;
            case gfx::Key::Right: ++next.column; break;
            case gfx::Key::Up: --next.row; break;
            case gfx::Key::Down: ++next.row; break;
            case gfx::Key::Home: next.column = 0; break;
            case gfx::Key::End: next.column = kColumnCount - 1; break;
            case gfx::Key::PageUp: next.row -= 20; break;
            case gfx::Key::PageDown: next.row += 20; break;
            default: handled = false; break;
            }
        }
        if (handled) {
            setSelection(anchor_, next, true);
            e.handled = true;
            return;
        }
    }

    postIfAlive(alive_, [this] { syncSelection(false); });
}

// Musen släpptes i tabellen. Trycket har redan gjort cellen aktuell, så markeringen
// läses efteråt. Med Shift utökas markeringen från ankaret.
void NormalSpanDialog::poleGridMouseUp(glaze::Widget*, glaze::MouseArgs& e)
{
    if (e.button != gfx::MouseButton::Left)
        return;
    const bool shift = (e.mods & gfx::kModShift) != 0;
    postIfAlive(alive_, [this, shift] { syncSelection(shift); });
}

// Högerklick i tabellen. Raden under pekaren blir markerad om den ligger utanför
// markeringen, och sedan visas snabbmenyn.
void NormalSpanDialog::poleGridContextMenu(glaze::Widget*, glaze::ContextMenuArgs& e)
{
    int row = -1;
    if (const auto p = poleGrid.fromWindow(gfx::Pointf{e.x, e.y}))
        row = poleGrid.rowAt(p->x, p->y);
    if (row >= 0) {
        const CellRect rect = selectionRect();
        if (row < rect.row0 || row > rect.row1) {
            const int column = std::max(0, poleGrid.currentColumn());
            setSelection({row, column}, {row, column}, true);
        }
    }
    showGridMenu(e.x, e.y);
}

// Läser beräkningen som ska öppnas (befintlig eller kopia) och fyller formuläret med den.
// En ny beräkning får förvalsgränserna och en tom rad. Svarar false när beräkningen
// inte finns eller inte gick att läsa, efter att felet visats.
bool NormalSpanDialog::loadCalculation()
{
    std::vector<glaze::DataGrid::Row> rows;
    std::vector<std::string> docKeys;
    const std::int64_t id = existingId_ != 0 ? existingId_ : copyOfId_;
    automatic_.reset();

    if (id != 0) {
        auto loaded = projectdb::normalSpan(*project_, id);
        if (!loaded) {
            glaze::messageBox(*this, loaded.error().message, "Normalspannsberäkning",
                              glaze::MessageBoxButtons::OK, glaze::MessageBoxType::Error);
            return false;
        }
        if (!loaded.value()) {
            glaze::messageBox(*this, "Normalspannsberäkningen finns inte längre i projektet.",
                              "Normalspannsberäkning", glaze::MessageBoxButtons::OK,
                              glaze::MessageBoxType::Warning);
            return false;
        }
        const projectdb::NormalSpan& ns = *loaded.value();
        // En automatisk beräkning visas bara, den ändras i stolptabellen.
        if (existingId_ != 0 && ns.automatic) {
            automatic_ = ns.automatic;
            readOnly_ = true;
        }
        noteTxt.setText(ns.note);
        lim1Spin.setValue(ns.limits.longest);
        lim2Spin.setValue(ns.limits.shortest);
        lim3Spin.setValue(ns.limits.adjacent);
        lim4Spin.setValue(ns.limits.localMin);
        lim5Spin.setValue(ns.limits.localMax);
        for (const auto& p : ns.poles) {
            glaze::DataGrid::Row row;
            row.cells = {p.pole,
                         p.lengthM ? tabletext::formatDecimal(*p.lengthM, -1, '.') : std::string(),
                         p.heightM ? tabletext::formatDecimal(*p.heightM, -1, '.') : std::string()};
            rows.push_back(std::move(row));
        }
        if (existingId_ != 0)
            docKeys = ns.docKeys;
    }

    glaze::DataGrid::Row empty;
    empty.cells = {"", "", ""};
    rows.push_back(std::move(empty));
    poleGrid.setRows(std::move(rows));
    updateDocLabel(docKeys);
    return true;
}

// Skriver upplysningen om var beräkningen visas i Dokumentation, efter hur den öppnats.
// En automatisk beräkning får i stället en text om stolptabellen och sträckan.
void NormalSpanDialog::updateDocLabel(const std::vector<std::string>& docKeys)
{
    const auto labelOf = [this](const std::string& key) {
        for (const auto& entry : docLabels_)
            if (entry.first == key)
                return entry.second;
        return key;
    };

    if (automatic_) {
        std::string text = "Automatisk beräkning ur " + std::string(projectdb::poleTableName(automatic_->stage)) +
                           ": " + automatic_->range.fromPole + " – " + automatic_->range.toPole +
                           ". Den följer tabellen och ändras där, och den kan inte tas bort.";
        if (!automatic_->issue.empty())
            text += " Sträckan går inte att använda: " + automatic_->issue;
        std::string joined;
        for (const auto& key : docKeys) {
            if (!joined.empty())
                joined += ", ";
            joined += labelOf(key);
        }
        if (!joined.empty())
            text += " Visas i Dokumentation på: " + joined;
        docLbl.setWrap(true);   // texten är lång: bryts mot bredden
        docLbl.setText(text);
        return;
    }
    if (existingId_ != 0) {
        if (docKeys.empty()) {
            docLbl.setText("Visas inte i Dokumentation. Koppla den med högerklick på en normalspannsrad i fliken Dokumentation.");
            return;
        }
        std::string joined;
        for (const auto& key : docKeys) {
            if (!joined.empty())
                joined += ", ";
            joined += labelOf(key);
        }
        docLbl.setText("Visas i Dokumentation på: " + joined);
    } else if (copyOfId_ == 0 && !linkDocKey_.empty()) {
        docLbl.setText("Visas på raden " + labelOf(linkDocKey_) + " när du trycker OK.");
    } else {
        docLbl.setText("Blir en lös beräkning i fliken Normalspann.");
    }
}

// Gör alla redigerare skrivskyddade och döljer OK och Töm. Avbryt heter Stäng.
void NormalSpanDialog::applyReadOnly()
{
    noteTxt.disable();
    lim1Spin.disable();
    lim2Spin.disable();
    lim3Spin.disable();
    lim4Spin.disable();
    lim5Spin.disable();
    poleGrid.setColumnEditor(kColPole, {.editor = glaze::DataGrid::Editor::Text, .readOnly = true});
    poleGrid.setColumnEditor(kColLength, {.editor = glaze::DataGrid::Editor::Number, .allowEmpty = true, .readOnly = true});
    poleGrid.setColumnEditor(kColHeight, {.editor = glaze::DataGrid::Editor::Number, .allowEmpty = true, .readOnly = true});
    okBtn.hide();
    clearBtn.hide();
    cancelBtn.setText("Stäng");
}

// Läser tabellen till stolpar, utan den sista tomma raden. En cell med ogiltigt tal räknas
// som tom, och ett fel för den läggs i gridIssues.
std::vector<ancalc::SpanPole> NormalSpanDialog::readPoles(std::vector<ancalc::SpanIssue>& gridIssues) const
{
    std::vector<ancalc::SpanPole> poles;
    const int count = std::max(0, poleGrid.rowCount() - 1);
    poles.reserve(static_cast<std::size_t>(count));
    for (int r = 0; r < count; ++r) {
        ancalc::SpanPole pole;
        pole.pole = trim(poleGrid.cell(r, kColPole));
        for (int c = kColLength; c <= kColHeight; ++c) {
            if (poleGrid.hasError(r, c)) {
                gridIssues.push_back({ancalc::IssueKind::Error, r,
                                      c == kColLength ? ancalc::SpanColumn::Length : ancalc::SpanColumn::Height,
                                      "Ogiltigt tal."});
                continue;
            }
            (c == kColLength ? pole.lengthM : pole.heightM) = poleGrid.numberValue(r, c);
        }
        poles.push_back(std::move(pole));
    }
    return poles;
}

// Läser gränserna ur de fem talfälten.
ancalc::SpanLimits NormalSpanDialog::readLimits() const
{
    ancalc::SpanLimits limits;
    limits.longest = lim1Spin.value();
    limits.shortest = lim2Spin.value();
    limits.adjacent = lim3Spin.value();
    limits.localMin = lim4Spin.value();
    limits.localMax = lim5Spin.value();
    return limits;
}

// Räknar om allt: beräkningen, kontrollkolumnerna, färgmärkena, tooltips, resultatpanelen
// och statusraden. Körs efter varje ändring av tabellen eller gränserna.
void NormalSpanDialog::recalc()
{
    const int rows = poleGrid.rowCount();
    std::vector<ancalc::SpanIssue> gridIssues;
    const std::vector<ancalc::SpanPole> poles = readPoles(gridIssues);
    calc_ = ancalc::calculateNormalSpan(poles, readLimits());
    // En automatisk beräkning vars sträcka har fel visar sträckans fel, inte "för få stolpar".
    if (automatic_ && !automatic_->issue.empty())
        calc_ = {std::nullopt, {{ancalc::IssueKind::Error, -1, ancalc::SpanColumn::None, automatic_->issue}}};

    issues_ = std::move(gridIssues);
    issues_.insert(issues_.end(), calc_.issues.begin(), calc_.issues.end());
    std::stable_sort(issues_.begin(), issues_.end(), [](const ancalc::SpanIssue& a, const ancalc::SpanIssue& b) {
        return a.poleIndex < b.poleIndex;
    });

    // Färgmärken och fel per rad.
    info_.assign(static_cast<std::size_t>(rows), RowInfo{});
    firstRow_ = -1;
    for (int r = 0; r + 1 < rows; ++r) {
        if (!isEmptyRow(r)) {
            firstRow_ = r;
            break;
        }
    }
    if (firstRow_ >= 0)
        info_[static_cast<std::size_t>(firstRow_)].mark[kColLength] = kMarkFirst;
    for (const auto& issue : issues_) {
        const int column = columnOf(issue.column);
        if (issue.kind != ancalc::IssueKind::Error || issue.poleIndex < 0 || issue.poleIndex >= rows || column < 0)
            continue;
        RowInfo& info = info_[static_cast<std::size_t>(issue.poleIndex)];
        if (info.error[static_cast<std::size_t>(column)].empty())
            info.error[static_cast<std::size_t>(column)] = issue.message;
        info.mark[static_cast<std::size_t>(column)] = kMarkError;
    }

    // Kontrollerna: märken per spann. Punkt 3 markerar också raden före.
    std::vector<std::array<std::string, 5>> texts(static_cast<std::size_t>(rows));
    if (calc_.result) {
        const auto& spans = calc_.result->spans;
        const double an = calc_.result->an;
        for (std::size_t k = 0; k < spans.size(); ++k) {
            const ancalc::SpanCheck& s = spans[k];
            if (s.poleIndex < 0 || s.poleIndex >= rows)
                continue;
            const std::size_t r = static_cast<std::size_t>(s.poleIndex);
            RowInfo& info = info_[r];
            info.span = static_cast<int>(k);
            info.mark[3] = s.tooLong ? kMarkRed : kMarkNone;
            info.mark[4] = s.tooShort ? kMarkRed : kMarkNone;
            info.mark[5] = s.adjacent ? kMarkGreen : kMarkNone;
            info.mark[6] = s.local10 ? (s.localTooLow ? kMarkRed : kMarkGreen) : kMarkNone;
            info.mark[7] = s.local10 ? (s.localTooHigh ? kMarkRed : kMarkGreen) : kMarkNone;
            texts[r][0] = ancalc::ratioText(s.ratio);
            texts[r][1] = ancalc::ratioText(s.ratio);
            texts[r][2] = s.adjacent ? ancalc::adjacentText(*s.adjacent) : std::string("-");
            texts[r][3] = s.local10 ? ancalc::localText(*s.local10, an) : std::string("-");
            texts[r][4] = texts[r][3];
        }
        for (std::size_t k = 0; k < spans.size(); ++k) {
            if (!spans[k].adjacentExceeded || spans[k].poleIndex < 0 || spans[k].poleIndex >= rows)
                continue;
            info_[static_cast<std::size_t>(spans[k].poleIndex)].mark[5] = kMarkRed;
            if (k > 0 && spans[k - 1].poleIndex >= 0 && spans[k - 1].poleIndex < rows)
                info_[static_cast<std::size_t>(spans[k - 1].poleIndex)].mark[5] = kMarkRed;
        }
    }

    // Kontrollkolumnernas text och indatacellernas tooltips.
    for (int r = 0; r < rows; ++r) {
        const RowInfo& info = info_[static_cast<std::size_t>(r)];
        for (int c = kInputColumns; c < kColumnCount; ++c) {
            const std::string& text = texts[static_cast<std::size_t>(r)][static_cast<std::size_t>(c - kInputColumns)];
            if (poleGrid.cell(r, c) != text)
                poleGrid.setCell(r, c, text);
        }
        for (int c = 0; c < kInputColumns; ++c) {
            std::string tip = info.error[static_cast<std::size_t>(c)];
            if (tip.empty() && c == kColLength && r == firstRow_)
                tip = "Spannet före första stolpen ingår inte i summorna.";
            if (tip.empty() && c == kColLength && info.span >= 0 && calc_.result) {
                const auto& inclined = calc_.result->spans[static_cast<std::size_t>(info.span)].inclinedM;
                if (inclined)
                    tip = "Lutande längd " + tabletext::formatDecimal(*inclined, 1) + " m";
            }
            poleGrid.setCellToolTip(r, c, tip);
        }
    }

    paintAll();
    updateResults();
    updateStatus();
}

// Skriver resultatpanelen och antalen avvikelser. Utan resultat visas ett streck överallt.
void NormalSpanDialog::updateResults()
{
    glaze::Label* counts[] = {&lim1Count, &lim2Count, &lim3Count, &lim4Count, &lim5Count};
    const std::string dash = "–";
    if (!calc_.result) {
        anValue.setText(dash);
        anDhValue.setText(dash);
        meanValue.setText(dash);
        totalValue.setText(dash);
        countValue.setText(dash);
        sectionValue.setText(dash);
        for (auto* label : counts) {
            label->setText(dash);
            label->setRole(glaze::Label::Role::Text);
        }
        return;
    }

    const ancalc::SpanResult& res = *calc_.result;
    anValue.setText(tabletext::formatDecimal(res.an, 1) + " m");
    anDhValue.setText(res.anDeltaH ? tabletext::formatDecimal(*res.anDeltaH, 1) + " m" : dash);
    meanValue.setText(groupedText(res.meanM, 1) + " m");
    totalValue.setText(groupedText(res.totalM, 1) + " m");
    countValue.setText(std::to_string(res.spanCount));
    sectionValue.setText(res.fromPole.empty() && res.toPole.empty() ? dash : res.fromPole + " – " + res.toPole);

    const int values[] = {res.tooLongCount, res.tooShortCount, res.adjacentCount, res.localLowCount,
                          res.localHighCount};
    for (int i = 0; i < 5; ++i) {
        counts[i]->setText(countText(values[i]));
        if (values[i] > 0)
            counts[i]->setColor(kCountRed);
        else
            counts[i]->setRole(glaze::Label::Role::Text);
    }
}

// Skriver statusraden: det första felet, annars den första varningen, annars sträckan.
void NormalSpanDialog::updateStatus()
{
    const auto describe = [](const ancalc::SpanIssue& issue) {
        if (issue.poleIndex >= 0)
            return "Rad " + std::to_string(issue.poleIndex + 1) + ": " + issue.message;
        return issue.message;
    };
    for (const auto& issue : issues_) {
        if (issue.kind == ancalc::IssueKind::Error) {
            setStatus(describe(issue));
            return;
        }
    }
    for (const auto& issue : issues_) {
        if (issue.kind == ancalc::IssueKind::Warning) {
            setStatus(describe(issue));
            return;
        }
    }
    if (calc_.result) {
        setStatus(std::to_string(calc_.result->spanCount) + " spann · " + calc_.result->fromPole + " – " +
                  calc_.result->toPole);
    } else {
        setStatus("");
    }
}

// Skriver en egen text i statusraden.
void NormalSpanDialog::setStatus(const std::string& text)
{
    statusBar.setPanelText(0, text);
}

// Färgen en cell har utan markering. Märkena sätts i recalc ur resultatet och felen.
std::optional<gfx::Color> NormalSpanDialog::baseColor(int row, int column) const
{
    if (row < 0 || static_cast<std::size_t>(row) >= info_.size() || column < 0 || column >= kColumnCount)
        return std::nullopt;
    switch (info_[static_cast<std::size_t>(row)].mark[static_cast<std::size_t>(column)]) {
    case kMarkRed: return kRedColor;
    case kMarkGreen: return kGreenColor;
    case kMarkError: return kErrorColor;
    case kMarkFirst: return kFirstColor;
    default: break;
    }
    return std::nullopt;
}

// Målar om en cell: markeringens färg om den ligger i en utökad markering, annars grundfärgen.
void NormalSpanDialog::paintCell(int row, int column)
{
    if (row < 0 || row >= poleGrid.rowCount() || column < 0 || column >= kColumnCount)
        return;
    const std::optional<gfx::Color> color =
        inSelection(row, column) ? std::optional<gfx::Color>(kSelectColor) : baseColor(row, column);
    poleGrid.setCellColor(row, column, color, std::nullopt);
}

// Målar om alla celler i rektangeln, klampad till tabellen.
void NormalSpanDialog::paintRect(const CellRect& rect)
{
    const int lastRow = std::min(rect.row1, poleGrid.rowCount() - 1);
    for (int r = std::max(0, rect.row0); r <= lastRow; ++r)
        for (int c = std::max(0, rect.column0); c <= std::min(rect.column1, kColumnCount - 1); ++c)
            paintCell(r, c);
}

// Målar om hela tabellen.
void NormalSpanDialog::paintAll()
{
    paintRect({0, 0, poleGrid.rowCount() - 1, kColumnCount - 1});
}

// Rektangeln mellan ankaret och änden, båda inräknade.
NormalSpanDialog::CellRect NormalSpanDialog::selectionRect() const
{
    return {std::min(anchor_.row, extent_.row), std::min(anchor_.column, extent_.column),
            std::max(anchor_.row, extent_.row), std::max(anchor_.column, extent_.column)};
}

// Sant när cellen ligger i en markering som är större än den aktuella cellen.
bool NormalSpanDialog::inSelection(int row, int column) const
{
    if (anchor_.row == extent_.row && anchor_.column == extent_.column)
        return false;
    const CellRect rect = selectionRect();
    return row >= rect.row0 && row <= rect.row1 && column >= rect.column0 && column <= rect.column1;
}

// Klampar en cell till tabellens rader och kolumner.
NormalSpanDialog::CellPos NormalSpanDialog::clampPos(CellPos pos) const
{
    pos.row = std::clamp(pos.row, 0, std::max(0, poleGrid.rowCount() - 1));
    pos.column = std::clamp(pos.column, 0, kColumnCount - 1);
    return pos;
}

// Sätter markeringen och målar om cellerna i den gamla och den nya rektangeln.
// moveCurrent gör änden till tabellens aktuella cell och rullar den i sikte.
void NormalSpanDialog::setSelection(CellPos anchor, CellPos extent, bool moveCurrent)
{
    const CellRect old = selectionRect();
    anchor_ = clampPos(anchor);
    extent_ = clampPos(extent);
    if (moveCurrent) {
        poleGrid.setCurrentCell(extent_.row, extent_.column);
        poleGrid.ensureVisible(extent_.row);
    }
    paintRect(old);
    paintRect(selectionRect());
}

// Läser tabellens aktuella cell. Med extend utökas markeringen från ankaret,
// annars blir markeringen den aktuella cellen.
void NormalSpanDialog::syncSelection(bool extend)
{
    const int row = poleGrid.currentRow();
    const int column = poleGrid.currentColumn();
    if (row < 0 || column < 0)
        return;
    if (extend)
        setSelection(anchor_, {row, column}, false);
    else
        setSelection({row, column}, {row, column}, false);
}

// Markerar alla celler. Den aktuella cellen blir överst till vänster.
void NormalSpanDialog::selectAll()
{
    setSelection({lastDataRow(), kColumnCount - 1}, {0, 0}, true);
}

// Kopierar markeringen som tabbseparerad text. Tal skrivs med decimalkomma, och den
// sista tomma raden tas aldrig med.
void NormalSpanDialog::copySelection()
{
    const std::string text = selectionText();
    if (!text.empty())
        gfx::setClipboardText(text);
}

// Markeringen som tabbseparerad text, eller tom text när den bara täcker den tomma raden.
// Tal skrivs med decimalkomma. Statusraden berättar vad som kopierades.
std::string NormalSpanDialog::selectionText()
{
    const CellRect rect = selectionRect();
    const int lastRow = std::min(rect.row1, poleGrid.rowCount() - 2);
    if (lastRow < rect.row0) {
        setStatus("Inget att kopiera.");
        return {};
    }
    std::vector<std::vector<std::string>> rows;
    for (int r = rect.row0; r <= lastRow; ++r) {
        std::vector<std::string> fields;
        for (int c = rect.column0; c <= rect.column1; ++c) {
            if (c == kColLength || c == kColHeight) {
                const auto value = poleGrid.numberValue(r, c);
                fields.push_back(value ? tabletext::formatDecimal(*value, -1, ',') : poleGrid.cell(r, c));
            } else {
                fields.push_back(poleGrid.cell(r, c));
            }
        }
        rows.push_back(std::move(fields));
    }
    setStatus("Kopierade " + std::to_string(rows.size()) + " × " +
              std::to_string(rect.column1 - rect.column0 + 1) + " celler");
    return tabletext::formatTsv(rows);
}

// Klistrar in urklippet från cellen och åt höger och nedåt.
void NormalSpanDialog::pasteAt(CellPos cell)
{
    pasteText(cell, gfx::clipboardText());
}

// Klistrar in texten från cellen och åt höger och nedåt. En rubrikrad hoppas över, och
// text i en talkolumn avbryter hela inklistringen innan något ändrats.
void NormalSpanDialog::pasteText(CellPos cell, const std::string& clip)
{
    auto block = tabletext::parseTsv(clip);
    if (block.empty()) {
        setStatus("Urklippet är tomt.");
        return;
    }

    // Rubrikraden: ett fält som inte är ett tal där det landar i Spannlängd eller H.
    int skipped = 0;
    if (block.size() > 1) {
        const auto& first = block.front();
        for (std::size_t j = 0; j < first.size(); ++j) {
            const int column = cell.column + static_cast<int>(j);
            const std::string text = trim(first[j]);
            if ((column == kColLength || column == kColHeight) && !text.empty() &&
                !tabletext::parseDecimal(text)) {
                block.erase(block.begin());
                skipped = 1;
                break;
            }
        }
    }

    // Kontroll före ändring: varje icke-tomt fält i en talkolumn måste vara ett tal.
    for (std::size_t i = 0; i < block.size(); ++i) {
        for (std::size_t j = 0; j < block[i].size(); ++j) {
            const int column = cell.column + static_cast<int>(j);
            const std::string text = trim(block[i][j]);
            if ((column == kColLength || column == kColHeight) && !text.empty() &&
                !tabletext::parseDecimal(text)) {
                glaze::messageBox(
                    *this,
                    "Inklistringen avbröts: \"" + text + "\" på rad " +
                        std::to_string(static_cast<int>(i) + 1 + skipped) +
                        " i urklippet är inte ett tal (kolumnen " +
                        (column == kColLength ? "Spannlängd" : "H") + "). Inget ändrades.",
                    "Klistra in", glaze::MessageBoxButtons::OK, glaze::MessageBoxType::Warning);
                return;
            }
        }
    }

    const tabletext::PastePlan plan = tabletext::planPaste(cell.row, cell.column, kInputColumns, block);
    if (plan.cells.empty()) {
        setStatus("Inget att klistra in här. Markera en cell i Stpnr, Spannlängd eller H.");
        return;
    }
    while (poleGrid.rowCount() < plan.rowsNeeded + 1)
        poleGrid.addRow(std::vector<std::string>(kInputColumns));
    for (const auto& pc : plan.cells) {
        std::string text = trim(pc.text);
        if (pc.column != kColPole) {
            const auto value = tabletext::parseDecimal(text);
            text = value ? tabletext::formatDecimal(*value, -1, '.') : std::string();
        }
        poleGrid.setCell(pc.row, pc.column, text);
    }

    // Markeringen blir det som klistrades in.
    std::size_t width = 0;
    for (const auto& row : block)
        width = std::max(width, row.size());
    const CellPos end{cell.row + static_cast<int>(block.size()) - 1,
                      std::min(cell.column + static_cast<int>(width) - 1, kInputColumns - 1)};
    dirty_ = true;
    recalc();
    setSelection(cell, end, true);
    setStatus("Klistrade in " + std::to_string(block.size()) + " rader");
}

// Tömmer indatacellerna i markeringen och tar bort tomma rader sist.
// Kontrollkolumnerna är inte indata och rörs inte.
void NormalSpanDialog::clearSelectionCells()
{
    const CellRect rect = selectionRect();
    if (rect.column0 >= kInputColumns)
        return;
    const int lastRow = std::min(rect.row1, poleGrid.rowCount() - 1);
    for (int r = rect.row0; r <= lastRow; ++r)
        for (int c = rect.column0; c <= std::min(rect.column1, kInputColumns - 1); ++c)
            poleGrid.setCell(r, c, "");
    trimTrailingRows();
    dirty_ = true;
    recalc();
    setSelection(anchor_, extent_, false);
}

// Infogar en tom rad ovanför markeringen. Tabellen byggs om med setRows.
void NormalSpanDialog::insertRowAbove()
{
    const int at = std::min(selectionRect().row0, poleGrid.rowCount() - 1);
    std::vector<glaze::DataGrid::Row> rows;
    for (int r = 0; r < poleGrid.rowCount(); ++r) {
        if (r == at) {
            glaze::DataGrid::Row blank;
            blank.cells = {"", "", ""};
            rows.push_back(std::move(blank));
        }
        glaze::DataGrid::Row row;
        row.cells = {poleGrid.cell(r, kColPole), poleGrid.cell(r, kColLength), poleGrid.cell(r, kColHeight)};
        rows.push_back(std::move(row));
    }
    poleGrid.setRows(std::move(rows));
    ensureTrailingRow();
    dirty_ = true;
    recalc();
    setSelection({at, 0}, {at, 0}, true);
}

// Tar bort raderna som markeringen täcker, utom den sista tomma raden.
// Tabellen byggs om med setRows och får alltid en tom rad sist.
void NormalSpanDialog::deleteSelectedRows()
{
    const CellRect rect = selectionRect();
    const int last = std::min(rect.row1, poleGrid.rowCount() - 2);
    if (last < rect.row0)
        return;
    std::vector<glaze::DataGrid::Row> rows;
    for (int r = 0; r < poleGrid.rowCount(); ++r) {
        if (r >= rect.row0 && r <= last)
            continue;
        glaze::DataGrid::Row row;
        row.cells = {poleGrid.cell(r, kColPole), poleGrid.cell(r, kColLength), poleGrid.cell(r, kColHeight)};
        rows.push_back(std::move(row));
    }
    poleGrid.setRows(std::move(rows));
    ensureTrailingRow();
    dirty_ = true;
    recalc();
    setSelection({rect.row0, 0}, {rect.row0, 0}, true);
}

// Tar bort tomma rader sist tills exakt en finns. Minst en rad finns alltid kvar.
void NormalSpanDialog::trimTrailingRows()
{
    while (poleGrid.rowCount() >= 2 && isEmptyRow(poleGrid.rowCount() - 1) &&
           isEmptyRow(poleGrid.rowCount() - 2))
        poleGrid.removeRow(poleGrid.rowCount() - 1);
    ensureTrailingRow();
    anchor_ = clampPos(anchor_);
    extent_ = clampPos(extent_);
}

// Lägger till en tom rad sist om tabellen saknar rader eller sista raden har innehåll.
void NormalSpanDialog::ensureTrailingRow()
{
    if (poleGrid.rowCount() == 0 || !isEmptyRow(poleGrid.rowCount() - 1))
        poleGrid.addRow(std::vector<std::string>(kInputColumns));
}

// Sant när raden saknar text, tal och fel i indatakolumnerna.
bool NormalSpanDialog::isEmptyRow(int row) const
{
    if (!trim(poleGrid.cell(row, kColPole)).empty())
        return false;
    for (int c = kColLength; c <= kColHeight; ++c)
        if (!poleGrid.cell(row, c).empty() || poleGrid.hasError(row, c))
            return false;
    return true;
}

// Sista raden med data, alltså raden före den tomma sist. Tom tabell ger 0.
int NormalSpanDialog::lastDataRow() const
{
    return std::max(0, poleGrid.rowCount() - 2);
}

// Visar snabbmenyn för markeringen. Skrivskyddat läge har bara Kopiera och Markera alla.
void NormalSpanDialog::showGridMenu(float x, float y)
{
    gridMenu_.items.clear();
    const bool edit = !readOnly_;
    const auto add = [this](const char* text, bool enabled, gfx::Key key, std::uint32_t mods,
                            std::function<void()> action) {
        glaze::MenuItem& item = gridMenu_.addItem(text);
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
    add("Klistra in", edit, gfx::Key::V, gfx::kModCtrl, [this] {
        const CellRect rect = selectionRect();
        pasteAt({rect.row0, rect.column0});
    });
    add("Radera innehåll", edit, gfx::Key::Delete, 0, [this] { clearSelectionCells(); });
    gridMenu_.addSeparator();
    add("Markera alla", true, gfx::Key::A, gfx::kModCtrl, [this] { selectAll(); });
    gridMenu_.addSeparator();
    add("Infoga rad ovanför", edit, gfx::Key::Unknown, 0, [this] { insertRowAbove(); });
    add("Ta bort rader", edit, gfx::Key::Unknown, 0, [this] { deleteSelectedRows(); });
    gridMenu_.popupAt(poleGrid, x, y);
}

// Validerar och sparar beräkningen i projektet. Svarar false när fönstret ska stå kvar,
// efter att det första felet visats och dess cell gjorts aktuell.
bool NormalSpanDialog::saveChanges()
{
    commitEdits();
    recalc();

    std::vector<const ancalc::SpanIssue*> errors;
    for (const auto& issue : issues_)
        if (issue.kind == ancalc::IssueKind::Error)
            errors.push_back(&issue);
    if (!errors.empty()) {
        const ancalc::SpanIssue& first = *errors.front();
        std::string text = "Beräkningen kan inte sparas: ";
        if (first.poleIndex >= 0)
            text += "rad " + std::to_string(first.poleIndex + 1) + ": ";
        text += first.message;
        if (errors.size() > 1)
            text += newline() + "(och " + std::to_string(errors.size() - 1) + " fel till)";
        glaze::messageBox(*this, text, "Normalspannsberäkning", glaze::MessageBoxButtons::OK,
                          glaze::MessageBoxType::Warning);
        const int column = columnOf(first.column);
        if (first.poleIndex >= 0 && column >= 0)
            setSelection({first.poleIndex, column}, {first.poleIndex, column}, true);
        return false;
    }

    projectdb::NormalSpan ns;
    ns.id = existingId_;
    ns.note = noteTxt.text();
    ns.limits = readLimits();
    std::vector<ancalc::SpanIssue> ignored;
    for (auto& pole : readPoles(ignored)) {
        if (pole.pole.empty() && !pole.lengthM && !pole.heightM)
            continue;
        ns.poles.push_back(std::move(pole));
    }

    const auto saved = projectdb::saveNormalSpan(*project_, ns, linkDocKey_);
    if (!saved) {
        gfx::log("Normalspannsberäkningen kunde inte sparas: %s", saved.error().detail.c_str());
        glaze::messageBox(*this, saved.error().message, "Normalspannsberäkning",
                          glaze::MessageBoxButtons::OK, glaze::MessageBoxType::Error);
        return false;
    }
    saved_ = true;
    savedId_ = saved.value();
    return true;
}
