#include "pole_import_dialog.h"

#include <algorithm>
#include <cstddef>
#include <set>
#include <utility>

#include "platform/files.h"

namespace {

namespace pi = kalkyl::poleimport;
namespace pt = kalkyl::poletable;
namespace xl = kalkyl::xlsx;

// Radbrytning i texter. Skrivs som tecken så att ingen escape-sekvens behövs i källan.
const std::string kNl(1, char{10});

// Färgerna för säkerheten och de röda cellerna, halvgenomskinliga så att de fungerar i ljust och mörkt läge.
const gfx::Color kSureColor{70, 170, 90, 64};
const gfx::Color kAmberColor{220, 160, 40, 64};
const gfx::Color kErrorColor{220, 60, 60, 80};
const gfx::Color kChosenColor{60, 130, 210, 56};
// Texten i infoLbl och issuesLbl när något är fel.
const gfx::Color kErrorText{200, 0, 0};

// Posterna i rullistan Kolumn i filen som inte är kolumner, och strecket för "inget värde".
constexpr const char* kPickColumn = "Välj kolumn…";
constexpr const char* kNotFetched = "– (hämtas inte)";
constexpr const char* kNoValue = "–";

// Kolumnerna i rutnäten: Kolumn i filen och Första värdet i Kolumner, och första fältkolumnen i förhandsvisningen.
constexpr int kChoiceColumn = 1;
constexpr int kConfidenceColumn = 2;
constexpr int kFirstValueColumn = 3;
constexpr int kPreviewFieldOffset = 1;

// Kolumnen med numret column i igenkänningen, eller nullptr.
const pi::SourceColumn* findColumn(const pi::Recognition& rec, int column)
{
    for (const auto& c : rec.columns)
        if (c.column == column)
            return &c;
    return nullptr;
}

// Rubriken som visas för en kolumn: den sammansatta rubriken, eller "(ingen rubrik)".
std::string labelOf(const pi::Recognition& rec, int column)
{
    const pi::SourceColumn* c = findColumn(rec, column);
    return (c && !c->label.empty()) ? c->label : std::string("(ingen rubrik)");
}

// "1 stolpe" eller "{n} stolpar".
std::string polesText(std::size_t n)
{
    return n == 1 ? std::string("1 stolpe") : std::to_string(n) + " stolpar";
}

// Cellfärgen för en säkerhet: grön, bärnsten (Trolig och Saknas), röd (Välj) eller blå (Ditt val).
gfx::Color confidenceColor(pi::Confidence confidence)
{
    switch (confidence) {
    case pi::Confidence::Sure: return kSureColor;
    case pi::Confidence::Likely:
    case pi::Confidence::Missing: return kAmberColor;
    case pi::Confidence::Ambiguous: return kErrorColor;
    case pi::Confidence::Chosen: return kChosenColor;
    }
    return kAmberColor;
}

// Cellens text som i Excel visar den: ett tal med komma, radbrytningar som blanksteg, "–" när den är tom.
std::string firstValueText(const xl::Cell* cell)
{
    if (!cell)
        return kNoValue;
    std::string text = cell->text;
    if (cell->kind == xl::CellKind::Number) {
        if (const auto number = xl::numberText(text, cell->decimals))
            text = *number;
        std::replace(text.begin(), text.end(), '.', ',');
    }
    for (char& ch : text)
        if (ch == char{10} || ch == char{13} || ch == char{9})
            ch = ' ';
    const auto begin = text.find_first_not_of(' ');
    if (begin == std::string::npos)
        return kNoValue;
    return text.substr(begin, text.find_last_not_of(' ') - begin + 1);
}

} // namespace

// Anger systemdatabasen som de inlärda rubrikerna och importmappen läses från och sparas i.
void PoleImportDialog::setSystemDb(kalkyl::systemdb::SystemDb& sys)
{
    sys_ = &sys;
}

// Anger filen som läses när dialogen öppnas.
void PoleImportDialog::setFile(const std::string& path)
{
    path_ = path;
}

// Anger Stolptabell före nu och dess sträckor, till frågan om ersättning och till sträckorna som tappar en stolpe.
void PoleImportDialog::setCurrentTable(std::vector<kalkyl::poletable::PoleRow> rows,
                                       std::vector<kalkyl::automode::NamedRange> ranges)
{
    currentRows_ = std::move(rows);
    ranges_ = std::move(ranges);
}

// Raderna som importeras, giltiga efter run(owner) == OK.
const std::vector<kalkyl::poletable::PoleRow>& PoleImportDialog::importedRows() const
{
    return rows_;
}

// Rådata för importen, giltig efter run(owner) == OK.
const kalkyl::poletable::ImportRecord& PoleImportDialog::importRecord() const
{
    return record_;
}

// Antalet ogiltiga tal som lämnades tomma i de importerade raderna.
int PoleImportDialog::invalidCount() const
{
    return invalid_;
}

// Startlogik: filfältet, de inlärda rubrikerna och läsningen av filen. Importera är släckt
// tills igenkänningen är klar.
void PoleImportDialog::onLoad(glaze::Widget*, glaze::EventArgs&)
{
    fileBox.setPath(path_);
    okBtn.disable();
    learned_ = {};
    if (sys_) {
        for (const auto& h : sys_->learnedHeaders())
            if (h.field >= 0 && h.field < pt::kColumnCount)
                learned_[static_cast<std::size_t>(h.field)].push_back(h.key);
    }
    if (!path_.empty())
        startLoad(path_);
}

// Filfältet: en ny sökväg startar en ny läsning. Samma fil som redan lästs, eller som läses just nu, ger ingenting.
void PoleImportDialog::fileBoxCommitted(glaze::Widget*, glaze::ChangeArgs&)
{
    const std::string path = fileBox.path();
    if (path.empty() || path == loadedPath_ || (path == path_ && loadWorker_.busy()))
        return;
    startLoad(path);
}

// Läser filen i bakgrunden. Tidigare innehåll töms så att inget kan ändras under läsningen. Arbetet
// fångar bara värden. Fortsättningen visar felet i rött, eller tar emot boken och igenkänningen.
void PoleImportDialog::startLoad(const std::string& path)
{
    path_ = path;
    loadedPath_.clear();
    book_.reset();
    rec_ = {};
    ex_ = {};
    mapping_ = {};
    chosen_ = {};
    fieldGrid.setRows({});
    previewGrid.setRows({});
    fromDrop.setItems({});
    toDrop.setItems({});
    countLbl.setText("");
    issuesLbl.setText("");
    okBtn.disable();
    infoLbl.setRole(glaze::Label::Role::Text);
    infoLbl.setText("Läser " + kalkyl::platform::fileName(path) + "…");
    setUseWaitCursor(true);

    loadWorker_.cancel();
    const pi::Learned learned = learned_;
    loadWorker_.run(
        [path, learned]() -> LoadOutcome {
            LoadOutcome out;
            auto book = xl::openWorkbook(path);
            if (!book) {
                out.error = book.error();
                return out;
            }
            out.book = std::move(book).value();
            out.sheet = pi::bestSheet(*out.book, learned);
            if (out.sheet >= 0 && out.sheet < static_cast<int>(out.book->sheets.size()))
                out.rec = pi::recognize(out.book->sheets[static_cast<std::size_t>(out.sheet)], learned);
            out.ok = true;
            return out;
        },
        [this, path](glaze::threads::Result<LoadOutcome>& r) {
            setUseWaitCursor(false);
            if (!r.ok()) {
                gfx::log("Importfilen kunde inte läsas: %s", r.error().c_str());
                infoLbl.setColor(kErrorText);
                infoLbl.setText(kalkyl::platform::fileName(path) + " kunde inte läsas.");
                return;
            }
            LoadOutcome& out = *r;
            if (!out.ok) {
                gfx::log("Importfilen kunde inte läsas: %s %s", out.error.message.c_str(), out.error.detail.c_str());
                infoLbl.setColor(kErrorText);
                infoLbl.setText(out.error.message);
                return;
            }
            book_ = std::move(out.book);
            rec_ = std::move(out.rec);
            loadedPath_ = path;
            applySheet(out.sheet);
        });
}

// Bladet som aktuellt: igenkänningens kolumner blir valen, utdraget görs och raden under
// filfältet beskriver rubrikerna och datat. Sedan fylls kolumnerna, Från och Till och förhandsvisningen.
void PoleImportDialog::applySheet(int index)
{
    sheetIndex_ = index;
    const xl::Sheet* s = sheet();
    if (!s)
        return;
    mapping_ = pi::mappingOf(rec_);
    chosen_ = {};
    ex_ = pi::extract(*s, rec_.header, mapping_);

    if (!rec_.looksLikePoleTable) {
        infoLbl.setColor(kErrorText);
        infoLbl.setText("Hittar ingen stolptabell på bladet " + s->name +
                        ". Välj kolumnerna själv eller en annan fil.");
    } else {
        std::string text = rec_.title.empty() ? "Blad " + s->name : rec_.title;
        text += " · rubriker på rad " + std::to_string(rec_.header.firstRow) + "–" + std::to_string(rec_.header.lastRow);
        if (!ex_.rows.empty())
            text += " · data från rad " + std::to_string(ex_.firstDataRow);
        if (!rec_.title.empty() && !rec_.knownVersion)
            text += " · versionen är inte provad, kontrollera kolumnerna";
        infoLbl.setRole(glaze::Label::Role::Text);
        infoLbl.setText(text);
    }

    fillFields();
    fromDrop.setItems({});
    toDrop.setItems({});
    fillRange();
    refreshPreview();
}

// Rullistans text för fältets nuvarande kolumn: Välj kolumn… (−1), hämtas inte (0) eller kolumnens rubrik.
std::string PoleImportDialog::choiceText(int field) const
{
    const int column = mapping_[static_cast<std::size_t>(field)];
    if (column < 0)
        return kPickColumn;
    if (column == 0)
        return kNotFetched;
    return columnTitle(column);
}

// Rubriken för kolumnen som "{L} · {label}", eller "{L} · (ingen rubrik)".
std::string PoleImportDialog::columnTitle(int column) const
{
    return xl::columnLetters(column) + " · " + labelOf(rec_, column);
}

// Fyller Kolumner med en rad per fält. Rullistans poster byggs per fält: Välj kolumn… för Stolpnr och
// för ett tvetydigt fält, hämtas inte för de valfria och sist varje kolumn. choiceColumns_ håller kolumnen per post.
void PoleImportDialog::fillFields()
{
    std::vector<glaze::DataGrid::Row> rows;
    for (int f = 0; f < pt::kColumnCount; ++f) {
        glaze::DataGrid::Row row;
        row.cells = {pt::columnName(f), choiceText(f), pi::confidenceText(confidenceOf(f)), kNoValue};
        rows.push_back(std::move(row));
    }
    fieldGrid.setRows(std::move(rows));

    for (int f = 0; f < pt::kColumnCount; ++f) {
        const bool optional = f != pt::kPole;
        std::vector<std::string> choices;
        std::vector<int>& columns = choiceColumns_[static_cast<std::size_t>(f)];
        columns.clear();
        if (!optional || mapping_[static_cast<std::size_t>(f)] == -1) {
            choices.push_back(kPickColumn);
            columns.push_back(-1);
        }
        if (optional) {
            choices.push_back(kNotFetched);
            columns.push_back(0);
        }
        for (const auto& c : rec_.columns) {
            choices.push_back(columnTitle(c.column));
            columns.push_back(c.column);
        }
        glaze::DataGrid::EditorSpec spec;
        spec.editor = glaze::DataGrid::Editor::Choice;
        spec.choices = std::move(choices);
        fieldGrid.setCellEditor(f, kChoiceColumn, std::move(spec));
        paintConfidence(f);
    }
    fieldGrid.setCellToolTip(pt::kPole, 0, "Stolpnr krävs. De andra fälten kan lämnas tomma.");
}

// Säkerheten för fältet: Ditt val när användaren valt kolumnen (Saknas när hon valt hämtas inte),
// annars igenkänningens.
pi::Confidence PoleImportDialog::confidenceOf(int field) const
{
    const auto f = static_cast<std::size_t>(field);
    if (chosen_[f])
        return mapping_[f] > 0 ? pi::Confidence::Chosen : pi::Confidence::Missing;
    return rec_.fields[f].confidence;
}

// Skriver säkerhetscellen: texten, färgen och tooltipen.
void PoleImportDialog::paintConfidence(int field)
{
    const pi::Confidence confidence = confidenceOf(field);
    fieldGrid.setCell(field, kConfidenceColumn, pi::confidenceText(confidence));
    fieldGrid.setCellColor(field, kConfidenceColumn, confidenceColor(confidence), std::nullopt);
    fieldGrid.setCellToolTip(field, kConfidenceColumn, confidenceTip(field));
}

// Tooltipen för säkerhetscellen enligt planen 9.3: poäng och skäl med näst bästa kolumn för Säker och
// Trolig, de två kandidaterna för Välj, och vad som saknas eller är valt för de andra.
std::string PoleImportDialog::confidenceTip(int field) const
{
    const pi::FieldMatch& match = rec_.fields[static_cast<std::size_t>(field)];
    const std::string quote = "”";
    switch (confidenceOf(field)) {
    case pi::Confidence::Chosen:
        return "Ditt val. Kalkyl minns det för filer med samma rubrik.";
    case pi::Confidence::Missing:
        if (field == pt::kPole)
            return "Stolpnr behövs. Välj kolumnen med stolparnas nummer.";
        return "Ingen kolumn känns igen som " + pt::columnName(field) + ". Fältet lämnas tomt, eller välj en kolumn.";
    case pi::Confidence::Ambiguous: {
        if (match.candidates.size() < 2)
            return "Flera kolumner passar. Välj den rätta.";
        const pi::Candidate& a = match.candidates[0];
        const pi::Candidate& b = match.candidates[1];
        return xl::columnLetters(a.column) + " " + quote + labelOf(rec_, a.column) + quote + " och " +
               xl::columnLetters(b.column) + " " + quote + labelOf(rec_, b.column) + quote + " passar båda (" +
               std::to_string(a.score) + " och " + std::to_string(b.score) + " poäng). Välj den rätta.";
    }
    case pi::Confidence::Sure:
    case pi::Confidence::Likely: {
        std::string text;
        for (const auto& c : match.candidates)
            if (c.column == match.column)
                text = std::to_string(c.score) + " poäng: " + c.reason + ".";
        for (const auto& c : match.candidates) {
            if (c.column == match.column)
                continue;
            text += " Näst bäst: " + xl::columnLetters(c.column) + " " + quote + labelOf(rec_, c.column) + quote + ", " +
                    std::to_string(c.score) + " poäng.";
            break;
        }
        return text;
    }
    }
    return "";
}

// En ny kolumn valdes i rullistan. Välj kolumn… går tillbaka till igenkänningen, hämtas inte ger 0 och en
// kolumn blir Ditt val. Sedan görs utdraget om, Från och Till fylls om när Stolpnr ändrats, och förhandsvisningen räknas om.
void PoleImportDialog::fieldGridCommitted(glaze::Widget*, glaze::CellChangeArgs& e)
{
    if (e.column != kChoiceColumn || e.row < 0 || e.row >= pt::kColumnCount || !sheet())
        return;
    const auto f = static_cast<std::size_t>(e.row);
    const std::string text = fieldGrid.cell(e.row, kChoiceColumn);
    const auto& choices = fieldGrid.cellEditor(e.row, kChoiceColumn).choices;
    const auto it = std::find(choices.begin(), choices.end(), text);
    if (it == choices.end())
        return;
    const int column = choiceColumns_[f][static_cast<std::size_t>(it - choices.begin())];
    if (column < 0) {
        mapping_[f] = pi::mappingOf(rec_)[f];
        chosen_[f] = false;
    } else {
        mapping_[f] = column;
        chosen_[f] = true;
    }
    fieldGrid.setCell(e.row, kChoiceColumn, choiceText(e.row));
    paintConfidence(e.row);
    ex_ = pi::extract(*sheet(), rec_.header, mapping_);
    if (e.row == pt::kPole)
        fillRange();
    refreshPreview();
}

// Fyller Från och Till med en post per stolpe: "{Stolpnr} · rad {r}". Ett val som står kvar på samma post
// behålls, annars väljs första respektive sista stolpen.
void PoleImportDialog::fillRange()
{
    const int n = static_cast<int>(ex_.rows.size());
    std::vector<std::string> items;
    items.reserve(ex_.rows.size());
    for (const auto& row : ex_.rows)
        items.push_back(row.pole.pole + " · rad " + std::to_string(row.excelRow));

    const std::vector<std::string> old = fromDrop.items();
    const auto sameItem = [&](int index) {
        return index >= 0 && index < n && index < static_cast<int>(old.size()) &&
               old[static_cast<std::size_t>(index)] == items[static_cast<std::size_t>(index)];
    };
    const int from = fromDrop.selectedIndex();
    const int to = toDrop.selectedIndex();
    const bool keepFrom = sameItem(from);
    const bool keepTo = sameItem(to);

    fromDrop.setItems(items);
    toDrop.setItems(items);
    fromDrop.setSelectedIndex(keepFrom ? from : (n > 0 ? 0 : -1));
    toDrop.setSelectedIndex(keepTo ? to : n - 1);
}

// Från eller Till ändrades.
void PoleImportDialog::rangeChanged(glaze::Widget*, glaze::ChangeArgs&)
{
    refreshPreview();
}

// Alla stolpar: första till sista stolpen i filen.
void PoleImportDialog::allBtnClicked(glaze::Widget*, glaze::ClickArgs&)
{
    const int n = static_cast<int>(ex_.rows.size());
    if (n > 0) {
        fromDrop.setSelectedIndex(0);
        toDrop.setSelectedIndex(n - 1);
    }
    refreshPreview();
}

// Räknar om allt som hör till sträckan Från–Till: antalet, förhandsvisningen med röda ogiltiga
// celler och dubbletter, Första värdet i Kolumner, problemen under tabellen och Importera.
void PoleImportDialog::refreshPreview()
{
    const xl::Sheet* s = sheet();
    if (!s) {
        previewGrid.setRows({});
        countLbl.setText("");
        issuesLbl.setText("");
        okBtn.disable();
        return;
    }
    const int n = static_cast<int>(ex_.rows.size());
    first_ = std::max(fromDrop.selectedIndex(), 0);
    last_ = n > 0 ? toDrop.selectedIndex() : -1;
    if (n > 0 && last_ < 0)
        last_ = n - 1;
    const bool valid = n > 0 && first_ <= last_ && last_ < n;

    // Antalet stolpar.
    if (n == 0)
        countLbl.setText("Inga stolpar");
    else if (n == 1)
        countLbl.setText("1 stolpe");
    else if (valid && last_ - first_ + 1 == n)
        countLbl.setText("Alla " + std::to_string(n) + " stolpar");
    else
        countLbl.setText(std::to_string(valid ? last_ - first_ + 1 : 0) + " av " + std::to_string(n) + " stolpar");

    // Förhandsvisningen: Excelraden och de åtta fälten, med röda ogiltiga celler och dubbletter.
    std::vector<pt::PoleRow> poles;
    std::vector<glaze::DataGrid::Row> rows;
    std::set<std::string> duplicates;
    if (valid) {
        poles = pi::poleRows(ex_, first_, last_);
        const auto dup = kalkyl::automode::duplicatePoles(poles);
        duplicates.insert(dup.begin(), dup.end());
        for (int i = first_; i <= last_; ++i) {
            const pi::SourceRow& source = ex_.rows[static_cast<std::size_t>(i)];
            glaze::DataGrid::Row row;
            row.cells.push_back(std::to_string(source.excelRow));
            for (int f = 0; f < pt::kColumnCount; ++f)
                row.cells.push_back(pt::displayText(source.pole, f));
            rows.push_back(std::move(row));
        }
    }
    previewGrid.setRows(std::move(rows));
    if (valid) {
        for (int i = first_; i <= last_; ++i) {
            const pi::SourceRow& source = ex_.rows[static_cast<std::size_t>(i)];
            const int r = i - first_;
            for (int f = 0; f < pt::kColumnCount; ++f) {
                const std::string& invalid = source.invalid[static_cast<std::size_t>(f)];
                if (invalid.empty())
                    continue;
                previewGrid.setCellColor(r, f + kPreviewFieldOffset, kErrorColor, std::nullopt);
                previewGrid.setCellToolTip(r, f + kPreviewFieldOffset,
                                           "Ogiltigt tal i " + xl::columnLetters(mapping_[static_cast<std::size_t>(f)]) +
                                               std::to_string(source.excelRow) + ": ”" + invalid +
                                               "”. Cellen lämnas tom.");
            }
            if (duplicates.count(source.pole.pole)) {
                previewGrid.setCellColor(r, pt::kPole + kPreviewFieldOffset, kErrorColor, std::nullopt);
                previewGrid.setCellToolTip(r, pt::kPole + kPreviewFieldOffset,
                                           "Stolpnr " + source.pole.pole + " finns flera gånger i filen.");
            }
        }
    }

    // Första värdet: cellen i filen på första importerade raden, per fält.
    for (int f = 0; f < pt::kColumnCount; ++f) {
        const int column = mapping_[static_cast<std::size_t>(f)];
        const xl::Cell* cell =
            (valid && column > 0) ? s->find(ex_.rows[static_cast<std::size_t>(first_)].excelRow, column) : nullptr;
        fieldGrid.setCell(f, kFirstValueColumn, firstValueText(cell));
    }

    // Problemen under tabellen. Importera är tänd bara när inget stoppar.
    std::vector<pi::Issue> issues = pi::importIssues(rec_, mapping_, ex_, n > 0 ? first_ : 0, n > 0 ? last_ : 0);
    if (valid)
        for (auto& text : pi::lostRanges(ranges_, currentRows_, poles))
            issues.push_back({false, std::move(text)});
    bool blocking = false;
    std::string text;
    for (const auto& issue : issues) {
        blocking = blocking || issue.blocking;
        if (!text.empty())
            text += kNl;
        text += issue.text;
    }
    if (blocking)
        issuesLbl.setColor(kErrorText);
    else
        issuesLbl.setRole(glaze::Label::Role::Dim);
    issuesLbl.setText(text);
    if (valid && !blocking)
        okBtn.enable();
    else
        okBtn.disable();
}

// Bygger resultatet för Från–Till: raderna som i Stolptabell före, rådata med filens namn och antalet
// fält som hade ett ogiltigt tal. Utan giltig sträcka blir allt tomt.
void PoleImportDialog::buildResult()
{
    rows_.clear();
    record_ = {};
    invalid_ = 0;
    const xl::Sheet* s = sheet();
    if (!s || ex_.rows.empty() || first_ > last_ || last_ >= static_cast<int>(ex_.rows.size()))
        return;
    rows_ = pi::poleRows(ex_, first_, last_);
    record_ = pi::importRecord(*s, rec_, mapping_, ex_, first_, last_, kalkyl::platform::fileName(loadedPath_));
    for (int i = first_; i <= last_; ++i)
        for (const auto& invalid : ex_.rows[static_cast<std::size_t>(i)].invalid)
            if (!invalid.empty())
                ++invalid_;
}

// Importera: bygger resultatet och frågar när Stolptabell före har rader (med sträckorna som tappar sin
// stolpe). Nej lämnar dialogen öppen. Vid Ja lärs de valda rubrikerna in, importmappen sparas och
// dialogen stänger med OK. Fel vid sparandet loggas bara.
void PoleImportDialog::okBtnClicked(glaze::Widget*, glaze::ClickArgs&)
{
    commitEdits();
    buildResult();
    if (rows_.empty())
        return;

    const std::size_t current = pt::rowsToSave(currentRows_).size();
    if (current > 0) {
        std::string text = "Ersätta raderna i Stolptabell före med " + polesText(rows_.size()) + " från " +
                           record_.fileName + "?" + kNl + kNl + "Stolptabell före har " +
                           (current == 1 ? std::string("1 rad") : std::to_string(current) + " rader") +
                           " nu. Bara raderna byts: start och slut för linorna, automatiskt läge, Stolptabell efter och Dokumentation ändras inte.";
        const auto lost = pi::lostRanges(ranges_, currentRows_, rows_);
        if (!lost.empty()) {
            text += kNl + kNl +
                    "De här sträckorna hittar inte sin stolpe i filen. De står kvar och visar felet tills de väljs om:";
            for (const auto& line : lost)
                text += kNl + line;
        }
        if (glaze::messageBox(*this, text, "Importera stolptabell", glaze::MessageBoxButtons::YesNo,
                              glaze::MessageBoxType::Warning) != glaze::DialogResult::Yes)
            return;
    }

    if (sys_) {
        for (int f = 0; f < pt::kColumnCount; ++f) {
            const auto idx = static_cast<std::size_t>(f);
            if (!chosen_[idx] || mapping_[idx] <= 0)
                continue;
            const pi::SourceColumn* column = findColumn(rec_, mapping_[idx]);
            if (!column || column->key.empty())
                continue;
            if (const auto learned = sys_->learnHeader(f, column->key); !learned)
                gfx::log("Rubriken kunde inte läras in: %s", learned.error().detail.c_str());
        }
        if (const auto saved = sys_->setLastImportFolder(kalkyl::platform::parentDir(loadedPath_)); !saved)
            gfx::log("Importmappen kunde inte sparas: %s", saved.error().detail.c_str());
    }
    closeWithResult(glaze::DialogResult::OK);
}

// Bladet som är aktuellt, eller nullptr utan bok.
const kalkyl::xlsx::Sheet* PoleImportDialog::sheet() const
{
    if (!book_ || sheetIndex_ < 0 || sheetIndex_ >= static_cast<int>(book_->sheets.size()))
        return nullptr;
    return &book_->sheets[static_cast<std::size_t>(sheetIndex_)];
}
