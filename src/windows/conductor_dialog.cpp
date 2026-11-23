#include "conductor_dialog.h"

#include <algorithm>
#include <charconv>
#include <cmath>
#include <cstddef>
#include <cstdio>
#include <limits>
#include <optional>
#include <string>
#include <system_error>
#include <utility>

namespace {

using kalkyl::systemdb::Conductor;
using kalkyl::systemdb::ConductorField;
using kalkyl::systemdb::ConductorOrigin;

// Kolumnernas index i tabellen: namn och typ, talen D–M, brottlasten och Källa.
constexpr int kColName = 0;
constexpr int kColType = 1;
constexpr int kColCreepStrain = 9;
constexpr int kColAlpha = 8;
constexpr int kColCreepTemp = 10;
constexpr int kColBreaking = 12;
constexpr int kColSource = 13;
constexpr int kColumnCount = 14;

// Rubriken per fält, till felmeddelanden. Indexen är kolumnerna 0–12.
constexpr const char* kColumnNames[] = {"Namn", "Typ", "Area", "Diameter", "Eik", "Ep",
                                        "σ max Eik", "k / X", "α", "εc", "Krypning",
                                        "Egentyngd", "Brottlast"};

// Rött för fel och bärnsten för varning, med samma alfa som mockupens radfärger.
const gfx::Color kErrorColor{220, 60, 60, 80};
const gfx::Color kWarnColor{220, 160, 40, 80};

// Översätter ett fält ur valideringen till tabellens kolumn.
int columnOf(ConductorField field)
{
    switch (field) {
    case ConductorField::Name: return 0;
    case ConductorField::Type: return 1;
    case ConductorField::Area: return 2;
    case ConductorField::Diameter: return 3;
    case ConductorField::EInitial: return 4;
    case ConductorField::EPermanent: return 5;
    case ConductorField::EInitialLimit: return 6;
    case ConductorField::EivConstant: return 7;
    case ConductorField::Alpha: return 8;
    case ConductorField::CreepStrain: return 9;
    case ConductorField::CreepTemp: return 10;
    case ConductorField::Weight: return 11;
    case ConductorField::BreakingLoad: return 12;
    }
    return 0;
}

// Talet som cellens kanoniska text: punkt som decimaltecken, kortaste form som läses
// tillbaka exakt, aldrig med exponent. Tabellen visar sedan decimalkomma.
std::string numText(double v)
{
    char buf[512];
    const auto r = std::to_chars(buf, buf + sizeof buf, v, std::chars_format::fixed);
    if (r.ec != std::errc())
        return "0";
    return std::string(buf, r.ptr);
}

// En brottlast som saknas är en tom cell.
std::string optText(const std::optional<double>& v)
{
    return v ? numText(*v) : std::string();
}

// Talet i en cell, eller NaN när det saknas eller inte går att tolka.
double numberAt(const glaze::DataGrid& grid, int row, int col)
{
    const auto v = grid.numberValue(row, col);
    return v ? *v : std::numeric_limits<double>::quiet_NaN();
}

// Talet med två decimaler och decimalkomma, till tooltips.
std::string commaText(double v)
{
    char buf[64];
    std::snprintf(buf, sizeof buf, "%.2f", v);
    std::string s = buf;
    const auto dot = s.find('.');
    if (dot != std::string::npos)
        s[dot] = ',';
    return s;
}

// Linan med id ur registret som det lästes, eller nullptr för en ny rad.
const Conductor* findLoaded(const std::vector<Conductor>& loaded, std::int64_t id)
{
    if (id == 0)
        return nullptr;
    for (const auto& c : loaded)
        if (c.id == id)
            return &c;
    return nullptr;
}

} // namespace

// Anger systemdatabasen som fönstret läser och sparar i.
// Objektet ägs av main och måste leva längre än fönstret.
void ConductorDialog::setSystemDb(kalkyl::systemdb::SystemDb& sys)
{
    sys_ = &sys;
}

// Startlogik, körs när fönstret laddas (aldrig i konstruktorn).
// Fyller tabellen ur registret och varnar i statusraden om inget sparas.
void ConductorDialog::onLoad(glaze::Widget*, glaze::EventArgs&)
{
    if (!sys_)
        return;
    if (!sys_->isPersistent())
        statusBar.addPanel("Ändringarna sparas inte: " + sys_->fallbackReason());
    fillGrid();
    updateStatus();
    updateButtons();
}

// Läser registret och fyller tabellen med en rad per lina.
// Ändringarna nollställs, så fönstret börjar om från databasens läge.
void ConductorDialog::fillGrid()
{
    loaded_.clear();
    rowIds_.clear();
    removedIds_.clear();
    dirty_ = false;
    grid.setRows({});

    auto result = sys_->conductors();
    if (!result) {
        glaze::messageBox(*this, result.error().message, "Kunde inte läsa linregistret",
                          glaze::MessageBoxButtons::OK, glaze::MessageBoxType::Error);
        return;
    }
    loaded_ = std::move(result.value());

    std::vector<glaze::DataGrid::Row> rows;
    rows.reserve(loaded_.size());
    for (const auto& c : loaded_) {
        glaze::DataGrid::Row row;
        row.cells = {c.name,
                     c.type,
                     numText(c.areaMm2),
                     numText(c.diameterMm),
                     numText(c.eInitialMpa),
                     numText(c.ePermanentMpa),
                     numText(c.eInitialLimitMpa),
                     numText(c.eivConstant),
                     numText(c.alphaE6PerC),
                     numText(c.creepStrainPermille),
                     numText(c.creepTempC),
                     numText(c.weightNPerM),
                     optText(c.breakingLoadKn),
                     ""};
        rows.push_back(std::move(row));
        rowIds_.push_back(c.id);
    }
    grid.setRows(std::move(rows));
    markNames();
}

// Bygger en lina av radens celler. Ett tal som saknas eller är otolkbart blir NaN,
// så att valideringen fångar det. Id och ursprung kommer ur rowIds_ och loaded_.
kalkyl::systemdb::Conductor ConductorDialog::rowConductor(int row) const
{
    Conductor c;
    c.id = rowIds_[static_cast<std::size_t>(row)];
    const Conductor* old = findLoaded(loaded_, c.id);
    c.origin = old ? old->origin : ConductorOrigin::Own;
    c.name = grid.cell(row, kColName);
    c.type = grid.cell(row, kColType);
    c.areaMm2 = numberAt(grid, row, 2);
    c.diameterMm = numberAt(grid, row, 3);
    c.eInitialMpa = numberAt(grid, row, 4);
    c.ePermanentMpa = numberAt(grid, row, 5);
    c.eInitialLimitMpa = numberAt(grid, row, 6);
    c.eivConstant = numberAt(grid, row, 7);
    c.alphaE6PerC = numberAt(grid, row, 8);
    c.creepStrainPermille = numberAt(grid, row, 9);
    c.creepTempC = numberAt(grid, row, 10);
    c.weightNPerM = numberAt(grid, row, 11);
    if (!grid.cell(row, kColBreaking).empty())
        c.breakingLoadKn = numberAt(grid, row, kColBreaking);
    return c;
}

// Markerar en rad: nollar färger och tooltips, färgar celler med valideringsfel röda,
// färgar en avvikande krypningstemperatur bärnsten och sätter Källa.
void ConductorDialog::markRow(int row)
{
    for (int col = 0; col < kColSource; ++col) {
        grid.setCellColor(row, col, std::nullopt, std::nullopt);
        grid.setCellToolTip(row, col, "");
    }

    const Conductor c = rowConductor(row);
    const auto issues = kalkyl::systemdb::validateConductor(c);
    bool creepInvalid = false;
    for (const auto& issue : issues) {
        const int col = columnOf(issue.field);
        if (grid.cellBackground(row, col))
            continue;   // första felet per cell gäller
        grid.setCellColor(row, col, kErrorColor, std::nullopt);
        grid.setCellToolTip(row, col, issue.message);
        creepInvalid = creepInvalid || issue.field == ConductorField::CreepStrain ||
                       issue.field == ConductorField::Alpha ||
                       issue.field == ConductorField::CreepTemp;
    }

    if (!creepInvalid && kalkyl::systemdb::creepTempDeviates(c)) {
        const auto derived = kalkyl::systemdb::derivedCreepTempC(c.creepStrainPermille, c.alphaE6PerC);
        grid.setCellColor(row, kColCreepTemp, kWarnColor, std::nullopt);
        grid.setCellToolTip(row, kColCreepTemp,
                            "Avviker från εc/α = " + (derived ? commaText(*derived) : std::string("?")) + " °C");
    }

    const Conductor* old = findLoaded(loaded_, c.id);
    const bool own = !old || old->origin == ConductorOrigin::Own;
    const bool modified = old && (old->origin == ConductorOrigin::SeedModified ||
                                  !kalkyl::systemdb::sameConductorValues(c, *old));
    grid.setCell(row, kColSource, own ? "Egen" : (modified ? "Ändrad" : "Startdata"));
}

// Markerar alla rader och färgar sedan de namn som finns på mer än en rad.
// Körs efter varje ändring av ett namn, ett tillägg och en borttagning.
void ConductorDialog::markNames()
{
    const int count = grid.rowCount();
    for (int i = 0; i < count; ++i)
        markRow(i);
    for (int i = 0; i < count; ++i) {
        for (int j = 0; j < count; ++j) {
            if (i != j && kalkyl::systemdb::sameConductorName(grid.cell(i, kColName), grid.cell(j, kColName))) {
                grid.setCellColor(i, kColName, kErrorColor, std::nullopt);
                grid.setCellToolTip(i, kColName, "Namnet finns på en annan rad");
                break;
            }
        }
    }
}

// En cell sparades av användaren. Ändrar användaren εc eller α räknas krypningstemperaturen om
// till en decimal. Raden markeras om, och vid en namnändring kontrolleras dubbletter.
void ConductorDialog::gridCommitted(glaze::Widget*, glaze::CellChangeArgs& e)
{
    dirty_ = true;
    if (e.row >= 0 && (e.column == kColCreepStrain || e.column == kColAlpha)) {
        const auto k = grid.numberValue(e.row, kColCreepStrain);
        const auto a = grid.numberValue(e.row, kColAlpha);
        if (k && a) {
            if (const auto derived = kalkyl::systemdb::derivedCreepTempC(*k, *a))
                grid.setCell(e.row, kColCreepTemp, numText(std::round(*derived * 10.0) / 10.0));
        }
    }
    if (e.column == kColName)
        markNames();
    else if (e.row >= 0)
        markRow(e.row);
    updateStatus();
}

// Den aktuella raden ändrades, så knapparnas läge uppdateras.
void ConductorDialog::gridSelectionChanged(glaze::Widget*, glaze::ChangeArgs&)
{
    updateButtons();
}

// Lägg till: kopierar den aktuella radens celler (eller nollor) till en ny rad sist,
// med ett namn som inte finns och källan Egen. Namncellen börjar redigeras.
void ConductorDialog::addBtnClicked(glaze::Widget*, glaze::ClickArgs&)
{
    std::vector<std::string> cells(kColumnCount, "0");
    cells[kColName] = "";
    cells[kColType] = "";
    cells[kColBreaking] = "";
    cells[kColSource] = "Egen";
    const int current = grid.currentRow();
    if (current >= 0) {
        for (int col = 0; col < kColSource; ++col)
            cells[static_cast<std::size_t>(col)] = grid.cell(current, col);
    }

    // Första lediga namnet: "Ny lina", "Ny lina 2", ...
    std::string name = "Ny lina";
    for (int n = 2;; ++n) {
        bool taken = false;
        for (int i = 0; i < grid.rowCount() && !taken; ++i)
            taken = kalkyl::systemdb::sameConductorName(grid.cell(i, kColName), name);
        if (!taken)
            break;
        name = "Ny lina " + std::to_string(n);
    }
    cells[kColName] = name;

    grid.addRow(std::move(cells));
    rowIds_.push_back(0);
    const int row = grid.rowCount() - 1;
    dirty_ = true;
    markNames();
    grid.setCurrentCell(row, kColName);
    grid.ensureVisible(row);
    grid.beginEdit();
    updateStatus();
    updateButtons();
}

// Ta bort: frågar och tar sedan bort den aktuella raden ur tabellen.
// Ett id som finns i databasen noteras, och borttagningen sparas först med OK.
void ConductorDialog::removeBtnClicked(glaze::Widget*, glaze::ClickArgs&)
{
    const int row = grid.currentRow();
    if (row < 0)
        return;
    const std::string question = "Ta bort linan \"" + grid.cell(row, kColName) + "\" ur linregistret?" +
                                 std::string(2, '\n') + "Borttagningen sparas när du trycker OK.";
    if (glaze::messageBox(*this, question, "Ta bort lina", glaze::MessageBoxButtons::YesNo,
                          glaze::MessageBoxType::Confirmation) != glaze::DialogResult::Yes)
        return;

    const std::int64_t id = rowIds_[static_cast<std::size_t>(row)];
    if (id != 0)
        removedIds_.push_back(id);
    grid.removeRow(row);
    rowIds_.erase(rowIds_.begin() + row);
    dirty_ = true;
    markNames();
    updateStatus();
    updateButtons();
}

// Återställ startdata: frågar och återställer sedan direkt i databasen (en transaktion).
// Tabellen läses om, så osparade ändringar i fönstret går förlorade.
void ConductorDialog::restoreBtnClicked(glaze::Widget*, glaze::ClickArgs&)
{
    const std::string question =
        "Startdatans linor får SWECO:s värden igen och borttagna linor kommer tillbaka. "
        "Dina egna linor behålls." +
        std::string(2, '\n') +
        "Återställningen sparas direkt, och ändringar i fönstret som inte är sparade går förlorade. "
        "Vill du fortsätta?";
    if (glaze::messageBox(*this, question, "Återställ startdata", glaze::MessageBoxButtons::YesNo,
                          glaze::MessageBoxType::Warning) != glaze::DialogResult::Yes)
        return;

    const auto result = sys_->restoreSeedConductors();
    if (!result) {
        glaze::messageBox(*this, result.error().message, "Kunde inte återställa startdata",
                          glaze::MessageBoxButtons::OK, glaze::MessageBoxType::Error);
        return;
    }
    fillGrid();
    updateStatus();
    updateButtons();
}

// OK: sparar ändringarna och stänger. Utan ändringar stängs fönstret direkt.
// Ett valideringsfel eller databasfel visas, och fönstret står kvar.
void ConductorDialog::okBtnClicked(glaze::Widget*, glaze::ClickArgs&)
{
    commitEdits();
    if (dirty_) {
        if (!saveChanges())
            return;
        saved_ = true;
    }
    closeWithResult(glaze::DialogResult::OK);
}

// Krysset, Avbryt och Escape: frågar om osparade ändringar innan fönstret stängs.
// Ja sparar (och stannar om det misslyckas), Nej kastar ändringarna, Avbryt stannar.
void ConductorDialog::onClosing(glaze::Widget*, glaze::CloseArgs& e)
{
    if (saved_ || e.reason != glaze::CloseReason::UserClosing)
        return;
    commitEdits();
    if (!dirty_)
        return;

    const auto answer = glaze::messageBox(*this, "Du har ändrat lindata. Vill du spara ändringarna?",
                                          "Lindata", glaze::MessageBoxButtons::YesNoCancel,
                                          glaze::MessageBoxType::Warning);
    if (answer == glaze::DialogResult::Yes) {
        if (saveChanges())
            saved_ = true;
        else
            e.cancel = true;
    } else if (answer != glaze::DialogResult::No) {
        e.cancel = true;
    }
}

// Validerar alla rader och sparar sedan nya och ändrade linor tillsammans med borttagningarna
// i en transaktion. Svarar false när fönstret ska stå kvar, efter att det första felet visats.
bool ConductorDialog::saveChanges()
{
    struct Problem {
        int row;
        int col;
        std::string text;
    };
    std::vector<Problem> problems;
    const int count = grid.rowCount();
    for (int row = 0; row < count; ++row) {
        const Conductor c = rowConductor(row);
        const std::string prefix = "Linan \"" + c.name + "\": ";
        std::vector<bool> reported(kColBreaking + 1, false);

        // Otolkbara tal i en cell (som cellen själv markerar som Ogiltigt värde).
        for (int col = 2; col <= kColBreaking; ++col) {
            if (!grid.hasError(row, col))
                continue;
            reported[static_cast<std::size_t>(col)] = true;
            problems.push_back({row, col,
                                "Linan \"" + c.name + "\" har ett ogiltigt tal i kolumnen " +
                                    kColumnNames[col] + ". Rätta det eller tryck Esc i cellen."});
        }
        for (const auto& issue : kalkyl::systemdb::validateConductor(c)) {
            const int col = columnOf(issue.field);
            if (!reported[static_cast<std::size_t>(col)])
                problems.push_back({row, col, prefix + issue.message});
        }
        for (int other = 0; other < row; ++other) {
            if (kalkyl::systemdb::sameConductorName(c.name, grid.cell(other, kColName))) {
                problems.push_back({row, kColName, prefix + "Namnet finns på en annan rad."});
                break;
            }
        }
    }

    if (!problems.empty()) {
        std::string text = problems.front().text;
        if (problems.size() > 1)
            text += "\n(och " + std::to_string(problems.size() - 1) + " fel till)";
        glaze::messageBox(*this, text, "Kan inte spara", glaze::MessageBoxButtons::OK,
                          glaze::MessageBoxType::Warning);
        grid.setCurrentCell(problems.front().row, problems.front().col);
        grid.ensureVisible(problems.front().row);
        return false;
    }

    std::vector<Conductor> upserts;
    for (int row = 0; row < count; ++row) {
        const Conductor c = rowConductor(row);
        const Conductor* old = findLoaded(loaded_, c.id);
        if (!old || !kalkyl::systemdb::sameConductorValues(c, *old))
            upserts.push_back(c);
    }
    const auto result = sys_->saveConductors(upserts, removedIds_);
    if (!result) {
        glaze::messageBox(*this, result.error().message, "Kunde inte spara linregistret",
                          glaze::MessageBoxButtons::OK, glaze::MessageBoxType::Error);
        return false;
    }
    return true;
}

// Skriver antalet linor i statusrutan.
// Panelen med orsaken till att inget sparas läggs till i onLoad.
void ConductorDialog::updateStatus()
{
    statusBar.setPanelText(0, std::to_string(grid.rowCount()) + " linor");
}

// Ta bort är aktiv bara när en rad är aktuell.
// Anropas efter varje ändring av raderna och av den aktuella raden.
void ConductorDialog::updateButtons()
{
    if (grid.currentRow() >= 0)
        removeBtn.enable();
    else
        removeBtn.disable();
}
