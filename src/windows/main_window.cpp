#include "main_window.h"

#include "conductor_dialog.h"
#include "normal_span_dialog.h"

#include "appearance/appearance.h"
#include "calc/table_text.h"
#include "platform/files.h"
#include "version/version.h"

#include <algorithm>
#include <cstdio>
#include <ctime>
#include <optional>
#include <string>
#include <string_view>

namespace {

using kalkyl::projectdb::Stage;
using kalkyl::projectdb::Wire;

// Raderna i Dokumentation som väljer lina: Faslina, Topplina vänster och Topplina höger.
// Indexet är platsen i kConductorRows och kConductorKeys. Rad 1 (Konfiguration) har en fast
// rullista. Raderna 0-3 visas alltid och står först, så radens plats i tabellen är också
// dess plats i kByggPmRows.
constexpr int kConductorRows[] = {0, 2, 3};
constexpr std::string_view kConductorKeys[] = {kalkyl::projectdb::kPhaseConductorKey,
                                               kalkyl::projectdb::kTopLeftConductorKey,
                                               kalkyl::projectdb::kTopRightConductorKey};
// Platsen för Topplina höger i kConductorRows. Den har valet "Samma" överst i rullistan:
// samma lina som Topplina vänster.
constexpr std::size_t kRightTopIndex = 2;
constexpr const char* kSameText = "Samma";
// Kolumnen Värde, där linans namn och värdena står. Referens står i kolumnen efter.
constexpr int kValueColumn = 1;
// Tooltipen på en normalspannsrad utan kopplad beräkning.
constexpr const char* kNsRowHint = "Högerklicka för att lägga till eller koppla en normalspannsberäkning";

// Radbrytning i texter. Skrivs som tecken så att ingen escape-sekvens behövs i källan.
const std::string kNl(1, char{10});

// Gruppernas bakgrundsfärger i Dokumentation, halvgenomskinliga så att de fungerar i ljust och mörkt läge.
const gfx::Color kBlue{60, 130, 210, 56};
const gfx::Color kGreen{70, 170, 90, 56};
const gfx::Color kAmber{220, 160, 40, 56};
const gfx::Color kPurple{150, 100, 210, 56};
const gfx::Color kRose{210, 90, 140, 56};

// Var en rad i Dokumentation visas. Both: alltid. Left: alltid, med sameLabel när topplinorna är samma.
// Right: bara när Topplina höger är en egen lina.
enum class Side { Both, Left, Right };

// Hur Värde matas in. Choice: rullista (linorna och konfigurationen, satta separat). Övriga är
// talfält där användaren bara skriver talet, och enheten visas efter talet (MPa, m, °C, Mall).
// Antalet decimaler följer det användaren skrivit.
enum class Unit { Choice, Mpa, Meter, Celsius, Mall };

// Lina och skede för de sex normalspannsraderna.
using Ns = std::pair<Wire, Stage>;

// En rad i Dokumentation: texten (och texten när topplinorna är samma), färgen, när den visas,
// hur värdet matas in, nyckeln i projektet (tom för normalspannsraderna, vars nyckel kommer ur
// ns), förvalet för Värde och lina och skede för normalspannsraderna. Raderna 0-3 visas alltid.
struct ByggPmRowDef {
    const char* label;
    const char* sameLabel;
    const gfx::Color* color;
    Side side;
    Unit unit;
    const char* key;
    const char* value;
    std::optional<Ns> ns;
};

const ByggPmRowDef kByggPmRows[] = {
    {"Faslina", nullptr, &kBlue, Side::Both, Unit::Choice, "phase_conductor", "", std::nullopt},
    {"Konfiguration", nullptr, &kBlue, Side::Both, Unit::Choice, "configuration", "Singel", std::nullopt},
    {"Topplina vänster", nullptr, &kBlue, Side::Both, Unit::Choice, "top_conductor_left", "", std::nullopt},
    {"Topplina höger", nullptr, &kBlue, Side::Both, Unit::Choice, "top_conductor_right", "Samma", std::nullopt},
    {"Uppspänning fas", nullptr, &kGreen, Side::Both, Unit::Mpa, "tension_phase", "", std::nullopt},
    {"Uppspänning topp vänster", "Uppspänning topp", &kGreen, Side::Left, Unit::Mpa, "tension_top_left", "", std::nullopt},
    {"Uppspänning topp höger", nullptr, &kGreen, Side::Right, Unit::Mpa, "tension_top_right", "", std::nullopt},
    {"Normalspann faslina", nullptr, &kGreen, Side::Both, Unit::Meter, "normal_span_phase", "", std::nullopt},
    {"Normalspann topplina vänster", "Normalspann topplina", &kGreen, Side::Left, Unit::Meter, "normal_span_top_left", "", std::nullopt},
    {"Normalspann topplina höger", nullptr, &kGreen, Side::Right, Unit::Meter, "normal_span_top_right", "", std::nullopt},
    {"Max temp", nullptr, &kAmber, Side::Both, Unit::Celsius, "temp_max", "", std::nullopt},
    {"Min temp", nullptr, &kAmber, Side::Both, Unit::Celsius, "temp_min", "", std::nullopt},
    // Beräkningarna: normalspannsberäkning och linberäkning i par. Före ombyggnation är lila och
    // efter ombyggnation rosa. Med egen topplina höger kommer vänster-paret före höger-paret.
    {"Normalspannsberäkning faslina före ombyggnation", nullptr, &kPurple, Side::Both, Unit::Meter, nullptr, "", Ns{Wire::Phase, Stage::Before}},
    {"Linberäkning faslina före ombyggnation", nullptr, &kPurple, Side::Both, Unit::Mall, "line_calc_phase_before", "", std::nullopt},
    {"Normalspannsberäkning faslina efter ombyggnation", nullptr, &kRose, Side::Both, Unit::Meter, nullptr, "", Ns{Wire::Phase, Stage::After}},
    {"Linberäkning faslina efter ombyggnation", nullptr, &kRose, Side::Both, Unit::Mall, "line_calc_phase_after", "", std::nullopt},
    {"Normalspannsberäkning topplina vänster före ombyggnation", "Normalspannsberäkning topplina före ombyggnation", &kPurple, Side::Left, Unit::Meter, nullptr, "", Ns{Wire::TopLeft, Stage::Before}},
    {"Linberäkning topplina vänster före ombyggnation", "Linberäkning topplina före ombyggnation", &kPurple, Side::Left, Unit::Mall, "line_calc_top_left_before", "", std::nullopt},
    {"Normalspannsberäkning topplina höger före ombyggnation", nullptr, &kPurple, Side::Right, Unit::Meter, nullptr, "", Ns{Wire::TopRight, Stage::Before}},
    {"Linberäkning topplina höger före ombyggnation", nullptr, &kPurple, Side::Right, Unit::Mall, "line_calc_top_right_before", "", std::nullopt},
    {"Normalspannsberäkning topplina vänster efter ombyggnation", "Normalspannsberäkning topplina efter ombyggnation", &kRose, Side::Left, Unit::Meter, nullptr, "", Ns{Wire::TopLeft, Stage::After}},
    {"Linberäkning topplina vänster efter ombyggnation", "Linberäkning topplina efter ombyggnation", &kRose, Side::Left, Unit::Mall, "line_calc_top_left_after", "", std::nullopt},
    {"Normalspannsberäkning topplina höger efter ombyggnation", nullptr, &kRose, Side::Right, Unit::Meter, nullptr, "", Ns{Wire::TopRight, Stage::After}},
    {"Linberäkning topplina höger efter ombyggnation", nullptr, &kRose, Side::Right, Unit::Mall, "line_calc_top_right_after", "", std::nullopt},
};

constexpr std::size_t kRowCount = std::size(kByggPmRows);

// Platsen för Topplina höger i kByggPmRows. Dess värde avgör läget.
constexpr std::size_t kRightTopDef = 3;

// Radens nyckel i projektet: den fasta nyckeln, eller normalspannsnyckeln ur lina och skede.
std::string rowKey(const ByggPmRowDef& d)
{
    if (d.ns)
        return kalkyl::projectdb::normalSpanDocKey(d.ns->first, d.ns->second);
    return d.key;
}

// Platsen i kByggPmRows för nyckeln key, eller kRowCount när ingen rad har den.
std::size_t defIndexForKey(const std::string& key)
{
    for (std::size_t i = 0; i < kRowCount; ++i)
        if (rowKey(kByggPmRows[i]) == key)
            return i;
    return kRowCount;
}

// Talfältet för en enhet: enheten som suffix efter talet, tomt tillåtet och decimalerna i den
// kortaste formen (100 visas som 100, 100,123 som 100,123). dash ger "-" för ett tomt värde.
// readOnly låser cellen men ritar den som ett talfält.
glaze::DataGrid::EditorSpec numberEditor(Unit unit, bool dash, bool readOnly)
{
    glaze::DataGrid::EditorSpec spec;
    spec.editor = glaze::DataGrid::Editor::Number;
    spec.allowEmpty = true;
    spec.readOnly = readOnly;
    if (dash)
        spec.emptyText = "-";
    switch (unit) {
    case Unit::Mpa:     spec.suffix = " MPa"; break;
    case Unit::Meter:   spec.suffix = " m"; break;
    case Unit::Celsius: spec.suffix = " °C"; break;
    case Unit::Mall:    spec.suffix = " Mall"; break;
    default:            break;
    }
    return spec;
}

// Kopian i listan med samma namn (utan hänsyn till ASCII-versaler), eller nullptr.
const kalkyl::conductor::Conductor* findConductor(const std::vector<kalkyl::conductor::Conductor>& list,
                                                  const std::string& name)
{
    for (const auto& c : list)
        if (kalkyl::conductor::sameConductorName(c.name, name))
            return &c;
    return nullptr;
}

// Klockslaget nu som hh:mm, för texterna i sparpanelen.
std::string clockText()
{
    const std::time_t now = std::time(nullptr);
    const std::tm* local = std::localtime(&now);
    char buffer[16] = {};
    if (local)
        std::snprintf(buffer, sizeof buffer, "%02d:%02d", local->tm_hour, local->tm_min);
    return buffer;
}

// Texten för en stolpe i en rubrik: "–" när den är tom.
std::string poleText(const std::string& pole)
{
    return pole.empty() ? std::string("–") : pole;
}

// Första och sista stolpens Stpnr, ur resultatet när det finns och annars ur stolparna.
std::pair<std::string, std::string> polesOf(const kalkyl::projectdb::NormalSpan& ns,
                                            const kalkyl::calc::SpanCalculation& calc)
{
    if (calc.result)
        return {calc.result->fromPole, calc.result->toPole};
    std::string from, to;
    for (const auto& p : ns.poles) {
        if (p.pole.empty())
            continue;
        if (from.empty())
            from = p.pole;
        to = p.pole;
    }
    return {from, to};
}

// "ST0252 – ST0532", med "–" för en tom stolpe.
std::string rangeText(const kalkyl::projectdb::NormalSpan& ns, const kalkyl::calc::SpanCalculation& calc)
{
    const auto poles = polesOf(ns, calc);
    return poleText(poles.first) + " – " + poleText(poles.second);
}

// Det första felet i beräkningen (med rad när den finns), eller tomt när den saknar fel.
std::string firstError(const kalkyl::calc::SpanCalculation& calc)
{
    for (const auto& issue : calc.issues) {
        if (issue.kind != kalkyl::calc::IssueKind::Error)
            continue;
        if (issue.poleIndex >= 0)
            return "rad " + std::to_string(issue.poleIndex + 1) + ", " + issue.message;
        return issue.message;
    }
    return {};
}

// Summan av antalen avvikelser i kontrollerna 1-5.
int deviationCount(const kalkyl::calc::SpanResult& r)
{
    return r.tooLongCount + r.tooShortCount + r.adjacentCount + r.localLowCount + r.localHighCount;
}

} // namespace

// Anger systemdatabasen och projektet som fönstret visar.
// Objekten ägs av main och måste leva längre än fönstret.
void MainWindow::setContext(kalkyl::systemdb::SystemDb& sys, kalkyl::projectdb::ProjectDb& project)
{
    sys_ = &sys;
    project_ = &project;
}

// Svarar hur fönstret stängdes, så att main vet om väljaren ska visas igen.
// Krysset ger Exit, eftersom outcome_ då aldrig ändrats.
MainWindow::Outcome MainWindow::outcome() const
{
    return outcome_;
}

// Sant när projektet är öppnat skrivskyddat. Då är alla ändringar spärrade.
bool MainWindow::readOnly() const
{
    return project_->access() == kalkyl::projectdb::Access::ReadOnly;
}

// Startlogik, körs när fönstret laddas (aldrig i konstruktorn).
// Sätter utseendet, statusraden och menyerna, läser projektets uppgifter och startar den
// automatiska sparningen.
void MainWindow::onLoad(glaze::Widget*, glaze::EventArgs&)
{
    kalkyl::appearance::apply(*this, *sys_);
    kalkyl::appearance::syncButton(themeBtn, themeMode());

    statusBar.setPanelText(0, project_->path());
    statusBar.addPanel("");
    savePanel_ = statusBar.panelCount() - 1;
    if (readOnly()) {
        const auto& lock = project_->lockedBy();
        statusBar.addPanel(lock ? "Skrivskyddat – öppnat av " + lock->user + " på " + lock->computer
                                : std::string("Skrivskyddat"));
    }
    if (!sys_->isPersistent())
        statusBar.addPanel("Inställningar sparas inte");

    menuBar.setHandler({"Arkiv", "Spara"}, [this] { saveClicked(); });
    menuBar.setHandler({"Arkiv", "Stäng projekt"}, [this] { closeProjectClicked(); });
    menuBar.setHandler({"Arkiv", "Avsluta"}, [this] { exitClicked(); });
    menuBar.setHandler({"Inställningar", "Lindata…"}, [this] { lindataClicked(); });
    menuBar.setHandler({"Hjälp", "Om Kalkyl"}, [this] { aboutClicked(); });
    if (readOnly())
        menuBar.setItemEnabled({"Arkiv", "Spara"}, false);

    // Valen i Automatisk sparning, i samma ordning som kAutosaveChoices.
    static constexpr const char* kAutosaveLabels[] = {"Av", "1 minut", "5 minuter", "10 minuter",
                                                      "15 minuter", "30 minuter"};
    static_assert(std::size(kAutosaveLabels) == std::size(kalkyl::systemdb::kAutosaveChoices));
    autosaveMinutes_ = sys_->autosaveMinutes();
    for (std::size_t i = 0; i < std::size(kAutosaveLabels); ++i) {
        const int minutes = kalkyl::systemdb::kAutosaveChoices[i];
        menuBar.setHandler({"Inställningar", "Automatisk sparning", kAutosaveLabels[i]},
                           [this, minutes] { autosaveChosen(minutes); });
        menuBar.setItemChecked({"Inställningar", "Automatisk sparning", kAutosaveLabels[i]},
                               minutes == autosaveMinutes_);
    }

    loadProjectData();
    rebuildByggPm();
    refreshNormalSpans();
    shadeStolpRows();
    updateTitle();
    restartAutosave();
}

// Stolptabellen får samma gula ton och samma växling som temperaturraderna i Dokumentation:
// varannan rad en aning mörkare, så att raderna är lätta att följa.
void MainWindow::shadeStolpRows()
{
    for (int r = 0; r < stolpGrid.rowCount(); ++r) {
        gfx::Color tone = kAmber;
        tone.a = (r % 2 == 0) ? 64 : 52;
        for (int col = 0; col < stolpGrid.columnCount(); ++col)
            stolpGrid.setCellColor(r, col, tone, std::nullopt);
    }
}

// Läser projektets uppgifter: Dokumentationens värden och referenser och linkopiorna. Det som
// saknas får förvalet. Ett fel ger en meddelanderuta, och fönstret fortsätter med förvalen.
void MainWindow::loadProjectData()
{
    byggPmValues_.assign(kRowCount, {});
    for (std::size_t i = 0; i < kRowCount; ++i)
        byggPmValues_[i] = {kByggPmRows[i].value, ""};

    auto docs = kalkyl::projectdb::docValues(*project_);
    if (!docs) {
        gfx::log("Projektets uppgifter kunde inte läsas: %s", docs.error().detail.c_str());
        glaze::messageBox(*this, "Projektets uppgifter kunde inte läsas." + kNl + docs.error().message,
                          "Kalkyl", glaze::MessageBoxButtons::OK, glaze::MessageBoxType::Error);
    } else {
        for (const auto& v : docs.value()) {
            const std::size_t i = defIndexForKey(v.key);
            if (i == kRowCount)
                continue;
            // Ett tomt värde med en referens lämnar förvalet (Singel, Samma) orört.
            if (!v.value.empty())
                byggPmValues_[i][0] = v.value;
            byggPmValues_[i][1] = v.reference;
        }
    }

    projectConductors_.clear();
    auto copies = kalkyl::projectdb::projectConductors(*project_);
    if (copies)
        projectConductors_ = std::move(copies.value());
    else
        gfx::log("Projektets linor kunde inte läsas: %s", copies.error().detail.c_str());
}

// Inställningar > Lindata…: visar linregistret modalt över huvudfönstret.
// Rullistorna och varningarna läses om efteråt. Projektets kopior ändras aldrig av Lindata.
void MainWindow::lindataClicked()
{
    commitEdits();
    ConductorDialog dlg;
    dlg.setSystemDb(*sys_);
    dlg.run(*this);
    fillConductorChoices();
}

// Läser linregistret och ger varje linrad en rullista med registrets linor och de av projektets
// linor som saknas i registret. Tooltip och bild per rad visar om projektets kopia avviker från
// registret eller om linan bara finns i projektet.
void MainWindow::fillConductorChoices()
{
    conductors_.clear();
    auto result = sys_->conductors();
    if (result)
        conductors_ = std::move(result.value());
    else
        gfx::log("Linregistret kunde inte läsas: %s", result.error().detail.c_str());

    std::vector<std::string> names;
    names.reserve(conductors_.size() + projectConductors_.size());
    for (const auto& c : conductors_)
        names.push_back(c.name);
    for (const auto& c : projectConductors_)
        if (!findConductor(conductors_, c.name))
            names.push_back(c.name);
    // Rullistan står efter namn, fallande: största arean först, som kolumnen Namn i Lindata.
    std::stable_sort(names.begin(), names.end(), [](const std::string& a, const std::string& b) {
        return kalkyl::conductor::compareNames(a, b) > 0;
    });

    // Topplina höger har "Samma" först, före linorna.
    std::vector<std::string> rightNames;
    rightNames.reserve(names.size() + 1);
    rightNames.push_back(kSameText);
    rightNames.insert(rightNames.end(), names.begin(), names.end());

    for (std::size_t i = 0; i < std::size(kConductorRows); ++i) {
        const int row = kConductorRows[i];
        const bool right = (i == kRightTopIndex);
        const std::string text = byggPmValues_[static_cast<std::size_t>(row)][0];

        glaze::DataGrid::EditorSpec spec;
        if (names.empty() && !right) {
            spec.editor = glaze::DataGrid::Editor::None;
        } else {
            spec.editor = glaze::DataGrid::Editor::Choice;
            spec.choices = right ? rightNames : names;
        }
        spec.readOnly = readOnly();
        byggPmGrid.setCellEditor(row, kValueColumn, spec);
        byggPmGrid.setCell(row, kValueColumn, text);

        std::string tip;
        glaze::ImageSource image;
        if (right && text == kSameText) {
            tip = "Samma lina som Topplina vänster";
        } else if (text.empty()) {
            if (names.empty())
                tip = "Linregistret är tomt";
        } else {
            const auto* registry = findConductor(conductors_, text);
            const auto* copy = findConductor(projectConductors_, text);
            if (!registry) {
                tip = "Linan finns inte i ditt linregister. Projektets kopia används.";
            } else if (copy && !kalkyl::conductor::sameConductorData(*copy, *registry)) {
                tip = "Projektets kopia skiljer sig från ditt linregister. Högerklicka för att uppdatera.";
                image = glaze::stockIcon(glaze::StockIcon::Warning);
            }
        }
        byggPmGrid.setCellToolTip(row, kValueColumn, tip);
        byggPmGrid.setCellImage(row, kValueColumn, image);
    }
}

// Bygger raderna i Dokumentation efter läget: är Topplina höger "Samma" visas inga höger-rader
// och vänster-raderna heter som en topplina; annars visas vänster och höger var för sig.
// Värdena läses ur byggPmValues_, som är sanningen om vad projektet innehåller.
void MainWindow::rebuildByggPm()
{
    byggPmSplit_ = byggPmValues_[kRightTopDef][0] != kSameText;

    // Raderna som syns i det här läget, med rätt text.
    std::vector<glaze::DataGrid::Row> rows;
    byggPmShown_.clear();
    for (std::size_t i = 0; i < kRowCount; ++i) {
        const ByggPmRowDef& d = kByggPmRows[i];
        if (d.side == Side::Right && !byggPmSplit_)
            continue;
        const char* label = (d.side == Side::Left && !byggPmSplit_) ? d.sameLabel : d.label;
        glaze::DataGrid::Row row;
        row.cells = {label, byggPmValues_[i][0], byggPmValues_[i][1]};
        rows.push_back(std::move(row));
        byggPmShown_.push_back(i);
    }
    byggPmGrid.setRows(std::move(rows));

    // setRows nollar cellernas färger och redigerare, så de sätts om. Inom varje färggrupp
    // växlar raderna mellan starkare och svagare ton, så att en rad är lätt att följa.
    const gfx::Color* previous = nullptr;
    int inGroup = 0;
    for (std::size_t r = 0; r < byggPmShown_.size(); ++r) {
        const gfx::Color* color = kByggPmRows[byggPmShown_[r]].color;
        inGroup = (color == previous) ? inGroup + 1 : 0;
        previous = color;
        gfx::Color tone = *color;
        tone.a = (inGroup % 2 == 0) ? 64 : 52;
        for (int col = 0; col < 3; ++col)
            byggPmGrid.setCellColor(static_cast<int>(r), col, tone, std::nullopt);
    }

    glaze::DataGrid::EditorSpec configuration;
    configuration.editor = glaze::DataGrid::Editor::Choice;
    configuration.choices = {"Singel", "Duplex", "Triplex"};
    configuration.readOnly = readOnly();
    byggPmGrid.setCellEditor(1, kValueColumn, configuration);

    for (std::size_t r = 0; r < byggPmShown_.size(); ++r) {
        const int row = static_cast<int>(r);
        const Unit unit = kByggPmRows[byggPmShown_[r]].unit;
        if (unit != Unit::Choice)
            byggPmGrid.setCellEditor(row, kValueColumn, numberEditor(unit, false, readOnly()));
        if (readOnly())
            byggPmGrid.setCellEditor(row, kValueColumn + 1, {glaze::DataGrid::Editor::None});
        // Referensen som tooltip, så att en lång text går att läsa när musen står över cellen.
        byggPmGrid.setCellToolTip(row, kValueColumn + 1, byggPmGrid.cell(row, kValueColumn + 1));
    }
    fillConductorChoices();
    showNormalSpanRows();
}

// En cell i Dokumentation sparades. Linval går via chooseConductor, "Samma" och alla andra celler
// sparas som text i projektet. Vid fel visas en ruta och cellen får tillbaka sitt värde.
// Byter Topplina höger läge byggs raderna om, efter den här händelsen.
void MainWindow::byggPmGridCommitted(glaze::Widget*, glaze::CellChangeArgs& e)
{
    if (e.row < 0 || static_cast<std::size_t>(e.row) >= byggPmShown_.size())
        return;
    if (e.column != kValueColumn && e.column != kValueColumn + 1)
        return;
    const std::size_t def = byggPmShown_[static_cast<std::size_t>(e.row)];

    if (e.column == kValueColumn + 1)   // Referens: tooltipen följer texten, tom text ger ingen
        byggPmGrid.setCellToolTip(e.row, e.column, byggPmGrid.cell(e.row, e.column));

    bool ok = true;
    bool conductorRow = false;
    for (std::size_t i = 0; i < std::size(kConductorRows); ++i) {
        if (e.row != kConductorRows[i] || e.column != kValueColumn)
            continue;
        conductorRow = true;
        const std::string text = byggPmGrid.cell(e.row, kValueColumn);
        if (i == kRightTopIndex && text == kSameText) {
            ok = saveDocValue(def, kValueColumn);
            // "Samma" kan ha rensat bort högerlinan ur projektet: läs om kopiorna.
            if (auto copies = kalkyl::projectdb::projectConductors(*project_); ok && copies)
                projectConductors_ = std::move(copies.value());
        } else {
            ok = chooseConductor(i, text);
        }
        if (ok && i == kRightTopIndex && (text != kSameText) != byggPmSplit_) {
            // Läget byts i pump-svansen, så att tabellen inte byggs om mitt i sin egen händelse.
            // Listan i fliken Normalspann visar radernas korta namn, som följer läget.
            glaze::app().post([this] {
                rebuildByggPm();
                fillNormalSpanList();
                updateNsButtons();
            });
        }
    }
    if (!conductorRow)
        ok = saveDocValue(def, e.column);

    if (!ok)
        byggPmGrid.setCell(e.row, e.column, byggPmValues_[def][static_cast<std::size_t>(e.column - 1)]);
    updateTitle();
    fillConductorChoices();
}

// Sparar den ändrade cellen (kolumn 1 eller 2) för raden def i projektet. Bara den ändrade
// kolumnen byts, så ett manuellt värde under en kopplad beräkning inte skrivs över av aₙ.
// Vid fel visas en meddelanderuta och false svaras.
bool MainWindow::saveDocValue(std::size_t def, int column)
{
    int row = -1;
    for (std::size_t r = 0; r < byggPmShown_.size(); ++r)
        if (byggPmShown_[r] == def)
            row = static_cast<int>(r);
    if (row < 0)
        return false;

    auto next = byggPmValues_[def];
    next[static_cast<std::size_t>(column - 1)] = byggPmGrid.cell(row, column);
    const kalkyl::projectdb::DocValue value{rowKey(kByggPmRows[def]), next[0], next[1]};
    auto saved = kalkyl::projectdb::setDocValue(*project_, value);
    if (!saved) {
        gfx::log("Cellen kunde inte sparas: %s", saved.error().detail.c_str());
        glaze::messageBox(*this, saved.error().message, "Kalkyl", glaze::MessageBoxButtons::OK,
                          glaze::MessageBoxType::Error);
        return false;
    }
    byggPmValues_[def] = next;
    return true;
}

// Väljer linan name på linraden conductorRow. Finns namnet bland projektets kopior används den
// kopian, annars kopieras registrets lina in. Därefter läses projektets uppgifter om, så att
// kopior och värden är som i projektet.
bool MainWindow::chooseConductor(std::size_t conductorRow, const std::string& name)
{
    const auto* chosen = findConductor(projectConductors_, name);
    if (!chosen)
        chosen = findConductor(conductors_, name);
    if (!chosen) {
        glaze::messageBox(*this, "Linan " + name + " finns inte i linregistret.", "Kalkyl",
                          glaze::MessageBoxButtons::OK, glaze::MessageBoxType::Error);
        return false;
    }
    const std::string key(kConductorKeys[conductorRow]);
    auto saved = kalkyl::projectdb::setDocConductor(*project_, key, *chosen);
    if (!saved) {
        gfx::log("Linan kunde inte sparas: %s", saved.error().detail.c_str());
        glaze::messageBox(*this, saved.error().message, "Kalkyl", glaze::MessageBoxButtons::OK,
                          glaze::MessageBoxType::Error);
        return false;
    }
    loadProjectData();
    return true;
}

// Läser om projektets uppgifter och beräkningar efter en ändring och uppdaterar rullistorna,
// raderna med beräkningar och titeln.
void MainWindow::reloadAfterChange()
{
    loadProjectData();
    refreshNormalSpans();
    fillConductorChoices();
    updateTitle();
}

// Radens hela text i Dokumentation, med läget (Samma eller egna topplinor) beaktat.
std::string MainWindow::fullDocLabel(const std::string& key) const
{
    const std::size_t i = defIndexForKey(key);
    if (i == kRowCount)
        return key;
    const ByggPmRowDef& d = kByggPmRows[i];
    const char* label = (d.side == Side::Left && !byggPmSplit_ && d.sameLabel) ? d.sameLabel : d.label;
    return label;
}

// Radens korta namn: Fas före, Topp efter, Topp v. före, Topp h. efter. En dold rad (topplina
// höger när topplinorna är samma) får "(dold)".
std::string MainWindow::shortDocLabel(const std::string& key) const
{
    const std::size_t i = defIndexForKey(key);
    if (i == kRowCount || !kByggPmRows[i].ns)
        return key;
    const Ns ns = *kByggPmRows[i].ns;
    const std::string stage = ns.second == Stage::Before ? "före" : "efter";
    switch (ns.first) {
    case Wire::Phase:    return "Fas " + stage;
    case Wire::TopLeft:  return (byggPmSplit_ ? "Topp v. " : "Topp ") + stage;
    case Wire::TopRight: return "Topp h. " + stage + (byggPmSplit_ ? "" : " (dold)");
    }
    return key;
}

// De kopplade radernas korta namn, i radordning och skilda med komma.
std::string MainWindow::shortRowsText(const kalkyl::projectdb::NormalSpan& ns) const
{
    std::string text;
    for (const auto& d : kByggPmRows) {
        if (!d.ns)
            continue;
        const std::string key = rowKey(d);
        if (std::find(ns.docKeys.begin(), ns.docKeys.end(), key) == ns.docKeys.end())
            continue;
        if (!text.empty())
            text += ", ";
        text += shortDocLabel(key);
    }
    return text.empty() ? std::string("–") : text;
}

// Nyckel och radtext för de sex normalspannsraderna, i radordning.
std::vector<std::pair<std::string, std::string>> MainWindow::docRowLabels() const
{
    std::vector<std::pair<std::string, std::string>> labels;
    for (const auto& d : kByggPmRows) {
        if (!d.ns)
            continue;
        const std::string key = rowKey(d);
        labels.emplace_back(key, fullDocLabel(key));
    }
    return labels;
}

// Platsen i normalSpans_ för beräkningen som raden key visar, eller -1.
int MainWindow::linkedSpanIndex(const std::string& key) const
{
    for (std::size_t i = 0; i < normalSpans_.size(); ++i) {
        const auto& keys = normalSpans_[i].docKeys;
        if (std::find(keys.begin(), keys.end(), key) != keys.end())
            return static_cast<int>(i);
    }
    return -1;
}

// Läser om alla beräkningar ur projektet och räknar dem. Raderna i Dokumentation och listan
// i fliken Normalspann uppdateras efteråt.
void MainWindow::refreshNormalSpans()
{
    normalSpans_.clear();
    nsCalcs_.clear();
    auto spans = kalkyl::projectdb::normalSpans(*project_);
    if (!spans) {
        gfx::log("Normalspannsberäkningarna kunde inte läsas: %s", spans.error().detail.c_str());
        glaze::messageBox(*this, "Normalspannsberäkningarna kunde inte läsas." + kNl + spans.error().message,
                          "Kalkyl", glaze::MessageBoxButtons::OK, glaze::MessageBoxType::Error);
    } else {
        normalSpans_ = std::move(spans.value());
        for (const auto& ns : normalSpans_)
            nsCalcs_.push_back(kalkyl::calc::calculateNormalSpan(ns.poles, ns.limits));
    }
    showNormalSpanRows();
    fillNormalSpanList();
}

// Värde-cellen på normalspannsraderna. Kopplad beräkning: aₙ i en låst cell med Σ-bild. Kopplad
// men med fel: tom cell och felet som tooltip. Ingen koppling: det manuella värdet, eller "-".
void MainWindow::showNormalSpanRows()
{
    for (std::size_t r = 0; r < byggPmShown_.size(); ++r) {
        const std::size_t def = byggPmShown_[r];
        const ByggPmRowDef& d = kByggPmRows[def];
        if (!d.ns)
            continue;
        const int row = static_cast<int>(r);
        const int index = linkedSpanIndex(rowKey(d));

        if (index >= 0) {
            const auto& calc = nsCalcs_[static_cast<std::size_t>(index)];
            byggPmGrid.setCellEditor(row, kValueColumn, numberEditor(Unit::Meter, true, true));
            if (calc.result) {
                byggPmGrid.setCell(row, kValueColumn, kalkyl::calc::formatDecimal(calc.result->an, -1, '.'));
                byggPmGrid.setCellImage(row, kValueColumn, glaze::stockIcon(glaze::StockIcon::Sum));
                byggPmGrid.setCellToolTip(row, kValueColumn,
                                          "Beräknad " + rangeText(normalSpans_[static_cast<std::size_t>(index)], calc) +
                                              ", " + std::to_string(calc.result->spanCount) +
                                              " spann. Högerklicka för att ändra.");
            } else {
                byggPmGrid.setCell(row, kValueColumn, "");
                byggPmGrid.setCellImage(row, kValueColumn, {});
                byggPmGrid.setCellToolTip(row, kValueColumn, "Beräkningen har fel: " + firstError(calc));
            }
        } else {
            byggPmGrid.setCellEditor(row, kValueColumn, numberEditor(Unit::Meter, true, readOnly()));
            byggPmGrid.setCell(row, kValueColumn, byggPmValues_[def][0]);
            byggPmGrid.setCellImage(row, kValueColumn, {});
            byggPmGrid.setCellToolTip(row, kValueColumn, readOnly() ? "" : kNsRowHint);
        }
    }
}

// Högerklick i Dokumentation. Linrader med en lina får "Uppdatera från linregistret".
// Normalspannsrader får Lägg till, Koppla befintlig och Manuellt värde, eller Ändra, Koppla bort
// och Ta bort när en beräkning är kopplad. Skrivskyddat finns bara Visa. Övriga rader har ingen meny.
void MainWindow::byggPmGridContextMenu(glaze::Widget*, glaze::ContextMenuArgs& e)
{
    const auto p = byggPmGrid.fromWindow({e.x, e.y});
    if (!p)
        return;
    const int row = byggPmGrid.rowAt(p->x, p->y);
    if (row < 0 || static_cast<std::size_t>(row) >= byggPmShown_.size())
        return;
    const std::size_t def = byggPmShown_[static_cast<std::size_t>(row)];
    const ByggPmRowDef& d = kByggPmRows[def];

    int conductorIndex = -1;
    for (std::size_t i = 0; i < std::size(kConductorRows); ++i)
        if (kConductorRows[i] == row)
            conductorIndex = static_cast<int>(i);
    if (conductorIndex < 0 && !d.ns)
        return;

    commitEdits();
    byggPmGrid.setCurrentCell(row, kValueColumn);
    byggPmMenu_.items.clear();

    if (conductorIndex >= 0) {
        // Linraden: bara när en egen lina är vald och projektet inte är skrivskyddat.
        const std::string name = byggPmValues_[def][0];
        if (readOnly() || name.empty() || name == kSameText)
            return;
        const auto* registry = findConductor(conductors_, name);
        const auto* copy = findConductor(projectConductors_, name);
        const bool differs = registry && copy && !kalkyl::conductor::sameConductorData(*copy, *registry);
        glaze::MenuItem& item = byggPmMenu_.addItem("Uppdatera från linregistret");
        item.enabled = differs;
        if (registry) {
            const kalkyl::conductor::Conductor fresh = *registry;
            item.onClick = [this, fresh] {
                auto done = kalkyl::projectdb::refreshProjectConductor(*project_, fresh);
                if (!done) {
                    glaze::messageBox(*this, done.error().message, "Kalkyl", glaze::MessageBoxButtons::OK,
                                      glaze::MessageBoxType::Error);
                    return;
                }
                reloadAfterChange();
                statusBar.setPanelText(savePanel_, "Linan " + fresh.name + " uppdaterad från linregistret.");
            };
        }
        byggPmMenu_.popupAt(byggPmGrid, e.x, e.y);
        return;
    }

    // Normalspannsraden.
    const std::string key = rowKey(d);
    const int index = linkedSpanIndex(key);
    if (index >= 0) {
        const std::int64_t id = normalSpans_[static_cast<std::size_t>(index)].id;
        if (readOnly()) {
            byggPmMenu_.addItem("Visa…").onClick = [this, id] {
                openNormalSpanDialog(NsOpen::Existing, id, "");
            };
        } else {
            byggPmMenu_.addItem("Ändra…").onClick = [this, id] {
                openNormalSpanDialog(NsOpen::Existing, id, "");
            };
            byggPmMenu_.addItem("Koppla bort").onClick = [this, key] {
                auto done = kalkyl::projectdb::unlinkDocRow(*project_, key);
                if (!done)
                    glaze::messageBox(*this, done.error().message, "Kalkyl", glaze::MessageBoxButtons::OK,
                                      glaze::MessageBoxType::Error);
                reloadAfterChange();
            };
            byggPmMenu_.addSeparator();
            byggPmMenu_.addItem("Ta bort…").onClick = [this, id] { removeNormalSpanAsked(id); };
        }
    } else {
        if (readOnly())
            return;
        byggPmMenu_.addItem("Lägg till…").onClick = [this, key] {
            openNormalSpanDialog(NsOpen::New, 0, key);
        };

        // Undermenyn har en post per beräkning, också de som redan visas på andra rader.
        glaze::MenuItem& link = byggPmMenu_.addSubmenu("Koppla befintlig");
        link.enabled = !normalSpans_.empty();
        for (std::size_t i = 0; i < normalSpans_.size(); ++i) {
            const auto& ns = normalSpans_[i];
            const auto& calc = nsCalcs_[i];
            std::string text = rangeText(ns, calc) + " · ";
            text += calc.result ? kalkyl::calc::formatDecimal(calc.result->an, 1) + " m" : std::string("fel");
            text += " · " + (ns.docKeys.empty() ? std::string("ej kopplad") : shortRowsText(ns));
            glaze::MenuItem entry;
            entry.text = text;
            const std::int64_t id = ns.id;
            entry.onClick = [this, id, key] {
                auto done = kalkyl::projectdb::linkNormalSpan(*project_, id, key);
                if (!done)
                    glaze::messageBox(*this, done.error().message, "Kalkyl", glaze::MessageBoxButtons::OK,
                                      glaze::MessageBoxType::Error);
                reloadAfterChange();
            };
            link.submenu.push_back(std::move(entry));
        }

        byggPmMenu_.addSeparator();
        byggPmMenu_.addItem("Manuellt värde").onClick = [this, row] {
            byggPmGrid.setCurrentCell(row, kValueColumn);
            byggPmGrid.beginEdit();
        };
    }
    byggPmMenu_.popupAt(byggPmGrid, e.x, e.y);
}

// Öppnar formuläret modalt. En ny beräkning kopplas till raden rowKey när användaren trycker OK.
// Efteråt läses uppgifterna och beräkningarna om, eftersom en koppling tömmer radens manuella
// värde, och en sparad beräkning väljs i listan.
void MainWindow::openNormalSpanDialog(NsOpen mode, std::int64_t id, const std::string& rowKey)
{
    commitEdits();
    NormalSpanDialog dlg;
    dlg.setProject(*project_);
    switch (mode) {
    case NsOpen::New:      dlg.setNew(rowKey); break;
    case NsOpen::Existing: dlg.setExisting(id); break;
    case NsOpen::Copy:     dlg.setCopyOf(id); break;
    }
    dlg.setDocRowLabels(docRowLabels());
    dlg.setReadOnly(readOnly());
    dlg.run(*this);
    const auto saved = dlg.savedId();

    reloadAfterChange();
    if (saved) {
        const auto it = std::find(nsListIds_.begin(), nsListIds_.end(), *saved);
        if (it != nsListIds_.end()) {
            const int row = static_cast<int>(it - nsListIds_.begin());
            nsLista.setSelectedIndex(row);
            nsLista.ensureVisible(row);
            updateNsButtons();
        }
    }
}

// Frågar Ja/Nej och tar bort beräkningen. Frågan säger hur många rader i Dokumentation som visar
// den och att de visar - efteråt.
void MainWindow::removeNormalSpanAsked(std::int64_t id)
{
    std::size_t index = normalSpans_.size();
    for (std::size_t i = 0; i < normalSpans_.size(); ++i)
        if (normalSpans_[i].id == id)
            index = i;
    if (index == normalSpans_.size())
        return;
    const auto& ns = normalSpans_[index];
    const auto& calc = nsCalcs_[index];

    std::string text = "Ta bort normalspannsberäkningen " + rangeText(ns, calc) + " (";
    if (calc.result)
        text += std::to_string(calc.result->spanCount) + " spann, aₙ " +
                kalkyl::calc::formatDecimal(calc.result->an, 1) + " m";
    else
        text += "beräkningen har fel";
    text += ") ur projektet?" + kNl + kNl;

    std::vector<std::string> rows;
    for (const auto& d : kByggPmRows) {
        if (!d.ns)
            continue;
        const std::string key = rowKey(d);
        if (std::find(ns.docKeys.begin(), ns.docKeys.end(), key) != ns.docKeys.end())
            rows.push_back(fullDocLabel(key));
    }
    if (rows.empty()) {
        text += "Den visas inte i Dokumentation.";
    } else if (rows.size() == 1) {
        text += "Den visas på raden " + rows[0] + ", som visar - efteråt.";
    } else {
        text += "Den visas på " + std::to_string(rows.size()) + " rader i Dokumentation, som visar - efteråt.";
        for (const auto& r : rows)
            text += kNl + r;
    }

    if (glaze::messageBox(*this, text, "Ta bort beräkning", glaze::MessageBoxButtons::YesNo,
                          glaze::MessageBoxType::Warning) != glaze::DialogResult::Yes)
        return;
    auto done = kalkyl::projectdb::removeNormalSpan(*project_, id);
    if (!done)
        glaze::messageBox(*this, done.error().message, "Kalkyl", glaze::MessageBoxButtons::OK,
                          glaze::MessageBoxType::Error);
    reloadAfterChange();
}

// Bygger listan i fliken Normalspann: en rad per beräkning med de nio kolumnerna, gul växlande
// radton och sorteringsnycklar för talkolumnerna. Den tidigare valda beräkningen väljs igen.
void MainWindow::fillNormalSpanList()
{
    std::optional<std::int64_t> previous;
    const int selected = nsLista.selectedIndex();
    if (selected >= 0 && static_cast<std::size_t>(selected) < nsListIds_.size())
        previous = nsListIds_[static_cast<std::size_t>(selected)];

    std::vector<glaze::ListView::Row> rows;
    nsListIds_.clear();
    for (std::size_t i = 0; i < normalSpans_.size(); ++i) {
        const auto& ns = normalSpans_[i];
        const auto& calc = nsCalcs_[i];
        const auto poles = polesOf(ns, calc);
        glaze::ListView::Row row;
        row.cells = {poleText(poles.first),
                     poleText(poles.second),
                     calc.result ? std::to_string(calc.result->spanCount) : std::string("–"),
                     calc.result ? kalkyl::calc::formatDecimal(calc.result->totalM, 1) : std::string("–"),
                     calc.result ? kalkyl::calc::formatDecimal(calc.result->an, 1) : std::string("–"),
                     calc.result && calc.result->anDeltaH ? kalkyl::calc::formatDecimal(*calc.result->anDeltaH, 1)
                                                          : std::string("–"),
                     calc.result ? std::to_string(deviationCount(*calc.result)) : std::string("Fel"),
                     shortRowsText(ns),
                     ns.note};
        rows.push_back(std::move(row));
        nsListIds_.push_back(ns.id);
    }
    nsLista.setRows(std::move(rows));

    for (std::size_t i = 0; i < normalSpans_.size(); ++i) {
        const int row = static_cast<int>(i);
        const auto& calc = nsCalcs_[i];
        if (calc.result) {
            nsLista.setSortKey(row, 2, calc.result->spanCount);
            nsLista.setSortKey(row, 3, calc.result->totalM);
            nsLista.setSortKey(row, 4, calc.result->an);
            nsLista.setSortKey(row, 5, calc.result->anDeltaH.value_or(0));
            nsLista.setSortKey(row, 6, deviationCount(*calc.result));
        }
        gfx::Color tone = kAmber;
        tone.a = (i % 2 == 0) ? 64 : 52;
        for (int col = 0; col < nsLista.columnCount(); ++col)
            nsLista.setCellColor(row, col, tone, std::nullopt);
    }

    if (previous) {
        const auto it = std::find(nsListIds_.begin(), nsListIds_.end(), *previous);
        if (it != nsListIds_.end())
            nsLista.setSelectedIndex(static_cast<int>(it - nsListIds_.begin()));
    }
    updateNsButtons();
}

// Aktiverar knapparna efter valet och skrivskyddat läge. Ändra heter Visa… skrivskyddat. Listan
// får en tooltip om den valda beräkningen: avvikelser per kontroll och raderna den visas på.
void MainWindow::updateNsButtons()
{
    const int selected = nsLista.selectedIndex();
    const bool has = selected >= 0 && static_cast<std::size_t>(selected) < normalSpans_.size();
    const bool write = !readOnly();

    const auto set = [](glaze::Button& b, bool on) {
        if (on)
            b.enable();
        else
            b.disable();
    };
    set(nsNyBtn, write);
    set(nsAndraBtn, has);
    set(nsKopieraBtn, has && write);
    set(nsTaBortBtn, has && write);
    nsAndraBtn.setText(write ? "Ändra…" : "Visa…");

    std::string tip;
    if (has) {
        const auto& ns = normalSpans_[static_cast<std::size_t>(selected)];
        const auto& calc = nsCalcs_[static_cast<std::size_t>(selected)];
        if (calc.result) {
            const auto& r = *calc.result;
            tip = "Avvikelser, 1: " + std::to_string(r.tooLongCount) + " · 2: " + std::to_string(r.tooShortCount) +
                  " · 3: " + std::to_string(r.adjacentCount) + " · 4: " + std::to_string(r.localLowCount) +
                  " · 5: " + std::to_string(r.localHighCount);
        } else {
            tip = "Beräkningen har fel: " + firstError(calc);
        }
        tip += kNl;
        std::string rows;
        for (const auto& d : kByggPmRows) {
            if (!d.ns)
                continue;
            const std::string key = rowKey(d);
            if (std::find(ns.docKeys.begin(), ns.docKeys.end(), key) == ns.docKeys.end())
                continue;
            rows += kNl + fullDocLabel(key);
        }
        tip += rows.empty() ? std::string("Visas inte i Dokumentation.") : "Visas i Dokumentation på" + rows;
    }
    nsLista.setToolTip(tip);
}

// Ny beräkning…: en lös beräkning. Den kopplas till en rad i Dokumentation med högerklick där.
void MainWindow::nsNyBtnClicked(glaze::Widget*, glaze::ClickArgs&)
{
    openNormalSpanDialog(NsOpen::New, 0, "");
}

// Ändra… (Visa…): den valda beräkningen.
void MainWindow::nsAndraBtnClicked(glaze::Widget*, glaze::ClickArgs&)
{
    const int selected = nsLista.selectedIndex();
    if (selected >= 0 && static_cast<std::size_t>(selected) < nsListIds_.size())
        openNormalSpanDialog(NsOpen::Existing, nsListIds_[static_cast<std::size_t>(selected)], "");
}

// Kopiera…: en ny, lös beräkning med samma stolpar och gränser som den valda.
void MainWindow::nsKopieraBtnClicked(glaze::Widget*, glaze::ClickArgs&)
{
    const int selected = nsLista.selectedIndex();
    if (selected >= 0 && static_cast<std::size_t>(selected) < nsListIds_.size())
        openNormalSpanDialog(NsOpen::Copy, nsListIds_[static_cast<std::size_t>(selected)], "");
}

// Ta bort: frågan och borttagningen görs av removeNormalSpanAsked.
void MainWindow::nsTaBortBtnClicked(glaze::Widget*, glaze::ClickArgs&)
{
    const int selected = nsLista.selectedIndex();
    if (selected >= 0 && static_cast<std::size_t>(selected) < nsListIds_.size())
        removeNormalSpanAsked(nsListIds_[static_cast<std::size_t>(selected)]);
}

// Dubbelklick och Enter i listan öppnar den valda beräkningen.
void MainWindow::nsListaActivated(glaze::Widget*, glaze::ActivateArgs&)
{
    const int current = nsLista.currentRow();
    if (current >= 0 && static_cast<std::size_t>(current) < nsListIds_.size())
        openNormalSpanDialog(NsOpen::Existing, nsListIds_[static_cast<std::size_t>(current)], "");
}

// Valet i listan ändrades: knapparna och tooltipen följer det.
void MainWindow::nsListaChanged(glaze::Widget*, glaze::ChangeArgs&)
{
    updateNsButtons();
}

// Högerklick i listan. Högertrycket har redan valt raden, så den ges av contextRow().
// Skrivskyddat finns bara Visa…. Det finns ingen kopplingspost här.
void MainWindow::nsListaContextMenu(glaze::Widget*, glaze::ContextMenuArgs& e)
{
    const int row = nsLista.contextRow();
    if (row < 0 || static_cast<std::size_t>(row) >= nsListIds_.size())
        return;
    const std::int64_t id = nsListIds_[static_cast<std::size_t>(row)];
    nsMenu_.items.clear();
    if (readOnly()) {
        nsMenu_.addItem("Visa…").onClick = [this, id] { openNormalSpanDialog(NsOpen::Existing, id, ""); };
    } else {
        nsMenu_.addItem("Ändra…").onClick = [this, id] { openNormalSpanDialog(NsOpen::Existing, id, ""); };
        nsMenu_.addItem("Kopiera…").onClick = [this, id] { openNormalSpanDialog(NsOpen::Copy, id, ""); };
        nsMenu_.addSeparator();
        nsMenu_.addItem("Ta bort…").onClick = [this, id] { removeNormalSpanAsked(id); };
    }
    nsMenu_.popupAt(nsLista, e.x, e.y);
}

// Arkiv > Spara (Ctrl+S). Gör inget skrivskyddat. En pågående sparning och ett oförändrat
// projekt ger en rad i sparpanelen. Annars sparas det som redigeras först och sedan publiceras.
void MainWindow::saveClicked()
{
    if (readOnly())
        return;
    if (saving_) {
        statusBar.setPanelText(savePanel_, "Sparning pågår…");
        return;
    }
    commitEdits();
    if (!project_->isModified()) {
        statusBar.setPanelText(savePanel_, "Inget att spara.");
        return;
    }
    startSave(SaveKind::Manual);
}

// Tick från den automatiska sparningen. Hoppar över skrivskyddat, pågående sparning, avstängning
// efter konflikt och ett oförändrat projekt. Rör inte det som redigeras: bara arbetskopian sparas.
void MainWindow::autosaveTimerTick(glaze::Timer*, glaze::TimerArgs&)
{
    if (readOnly() || saving_ || autosaveBlocked_ || !project_->isModified())
        return;
    startSave(SaveKind::Auto);
}

// Inställningar > Automatisk sparning. Valet sparas per användare i systemdatabasen. Ett fel
// visas i statusraden, och valet gäller ändå under sessionen. Timern startas om.
void MainWindow::autosaveChosen(int minutes)
{
    autosaveMinutes_ = minutes;
    auto saved = sys_->setAutosaveMinutes(minutes);
    if (!saved) {
        gfx::log("Inställningen kunde inte sparas: %s", saved.error().detail.c_str());
        statusBar.setPanelText(savePanel_, "Inställningen kunde inte sparas. " + saved.error().message);
    }
    restartAutosave();
}

// Stoppar timern och startar den igen med inställningens intervall, om den automatiska sparningen
// är på, projektet inte är skrivskyddat och ingen konflikt har stängt av den.
void MainWindow::restartAutosave()
{
    autosaveTimer.stop();
    if (autosaveMinutes_ <= 0 || readOnly() || autosaveBlocked_)
        return;
    autosaveTimer.setInterval(autosaveMinutes_ * 60000);
    autosaveTimer.start();
}

// Titeln visar projektnamnet, en stjärna när projektet är ändrat, versionen och skrivskyddat.
// Sparpanelen visar "Ej sparat" när projektet är ändrat och senaste utfallet inte var ett fel,
// och annars senaste utfallet. Under en sparning visar den "Sparar…" och lämnas orörd.
void MainWindow::updateTitle()
{
    const bool modified = project_->isModified();
    setTitle(project_->info().name + (modified ? " *" : "") + " – Kalkyl " + KALKYL_VERSION +
             (readOnly() ? " (skrivskyddat)" : ""));
    if (saving_ || savePanel_ < 0)
        return;
    statusBar.setPanelText(savePanel_, (modified && !saveStatusIsError_) ? std::string("Ej sparat") : saveStatus_);
}

// Publicerar projektet. Bilden tas på UI-tråden och publiceras i en Worker. Utfallet tas emot
// på UI-tråden: lyckat ger ny bas och en rad i sparpanelen, fel ger en ruta (Ctrl+S och stängning)
// eller bara en rad i statusraden (automatiskt). En konflikt stänger av den automatiska sparningen.
// Pågår redan en sparning görs inget: dess utfall gäller, också för en väntande stängning.
void MainWindow::startSave(SaveKind kind)
{
    if (saving_)
        return;
    auto snap = project_->snapshot();
    if (!snap) {
        gfx::log("Bilden kunde inte tas: %s", snap.error().detail.c_str());
        if (kind == SaveKind::Auto) {
            saveStatus_ = "Automatisk sparning misslyckades " + clockText() + ": " + snap.error().message;
            saveStatusIsError_ = true;
            updateTitle();
        } else {
            closeAfterSave_ = false;
            if (kind == SaveKind::Closing) {
                outcome_ = Outcome::Exit;
                restartAutosave();
            }
            glaze::messageBox(*this, "Projektet kunde inte sparas." + kNl + snap.error().message, "Kalkyl",
                              glaze::MessageBoxButtons::OK, glaze::MessageBoxType::Error);
        }
        return;
    }

    saving_ = true;
    statusBar.setPanelText(savePanel_, "Sparar…");
    menuBar.setItemEnabled({"Arkiv", "Spara"}, false);

    const kalkyl::projectdb::Snapshot image = snap.value();
    saveWorker_.run(
        [image](std::stop_token stop) { return kalkyl::projectdb::ProjectDb::publish(image, stop); },
        [this, image, kind](glaze::threads::Result<kalkyl::core::Result<kalkyl::platform::FileStamp>>& r) {
            saving_ = false;
            if (!readOnly())
                menuBar.setItemEnabled({"Arkiv", "Spara"}, true);
            if (r.cancelled())
                return;

            // Lyckat: den nya basen och en rad i sparpanelen. Stängs fönstret efter sparningen
            // bekräftas stängningen bara om inget ändrats under tiden.
            if (r && *r) {
                project_->published(image, (*r).value());
                saveStatus_ = (kind == SaveKind::Auto ? "Sparat automatiskt " : "Sparat ") + clockText();
                saveStatusIsError_ = false;
                updateTitle();
                if (closeAfterSave_) {
                    closeAfterSave_ = false;
                    if (!project_->isModified())
                        closingConfirmed_ = true;
                    close();
                }
                return;
            }

            // Fel: bilden tas bort och felet visas efter hur sparningen startades.
            static_cast<void>(kalkyl::platform::removeFile(image.localFile));
            const bool conflict = r && (*r).error().code == kalkyl::core::ErrorCode::Conflict;
            const std::string message = !r ? "Oväntat fel: " + r.error() : (*r).error().message;
            if (kind == SaveKind::Auto) {
                saveStatus_ = "Automatisk sparning misslyckades " + clockText() + ": " + message;
                saveStatusIsError_ = true;
                if (conflict) {
                    autosaveBlocked_ = true;
                    autosaveTimer.stop();
                    saveStatus_ = "Automatisk sparning avstängd. Projektfilen har ändrats av någon annan. "
                                  "Spara med Ctrl+S för mer information.";
                }
                updateTitle();
                if (closeAfterSave_) {
                    closeAfterSave_ = false;
                    close();
                }
                return;
            }
            closeAfterSave_ = false;
            outcome_ = Outcome::Exit;
            if (kind == SaveKind::Closing)
                restartAutosave();
            saveStatus_ = "Sparningen misslyckades " + clockText();
            saveStatusIsError_ = true;
            updateTitle();
            glaze::messageBox(*this, "Projektet kunde inte sparas." + kNl + message, "Kalkyl",
                              glaze::MessageBoxButtons::OK, glaze::MessageBoxType::Error);
        });
}

// Fönstret stängs. Oförändrat, skrivskyddat eller bekräftat ger ingen fråga. Under en sparning
// väntas den in. Annars frågas Ja (spara och stäng), Nej (kasta ändringarna) eller Avbryt.
// Avbryt gör att ett senare kryss avslutar programmet i stället för att gå till väljaren.
// Timern står still medan frågan visas, så att ingen automatisk sparning startar under den.
void MainWindow::onClosing(glaze::Widget*, glaze::CloseArgs& e)
{
    if (closingConfirmed_) {
        autosaveTimer.stop();
        return;
    }
    commitEdits();
    if (readOnly() || !project_->isModified()) {
        autosaveTimer.stop();
        return;
    }
    if (saving_) {
        e.cancel = true;
        closeAfterSave_ = true;
        statusBar.setPanelText(savePanel_, "Stänger när sparningen är klar…");
        return;
    }

    autosaveTimer.stop();
    const auto answer = glaze::messageBox(
        *this, "Projektet har ändringar som inte är sparade. Vill du spara dem innan projektet stängs?",
        "Spara projektet", glaze::MessageBoxButtons::YesNoCancel, glaze::MessageBoxType::Warning);
    if (answer == glaze::DialogResult::Yes) {
        e.cancel = true;
        closeAfterSave_ = true;
        startSave(SaveKind::Closing);
    } else if (answer != glaze::DialogResult::No) {
        e.cancel = true;
        outcome_ = Outcome::Exit;
        restartAutosave();
    }
}

// Namnlistens lägesknapp: System -> Mörkt -> Ljust -> System.
// Logiken delas med väljaren via kalkyl::appearance.
void MainWindow::themeBtnClicked(glaze::Widget*, glaze::ClickArgs&)
{
    kalkyl::appearance::cycle(*this, themeBtn, *sys_);
}

// Arkiv > Stäng projekt: main stänger projektet och visar väljaren igen.
// close() verkställs uppskjutet av GlazeUI, och onClosing frågar om osparade ändringar.
void MainWindow::closeProjectClicked()
{
    outcome_ = Outcome::CloseProject;
    close();
}

// Arkiv > Avsluta: stänger fönstret och därmed programmet.
// Samma resultat som krysset.
void MainWindow::exitClicked()
{
    outcome_ = Outcome::Exit;
    close();
}

// Hjälp > Om Kalkyl: visar programmets namn och version.
// Rutan är GlazeUI:s egen meddelanderuta.
void MainWindow::aboutClicked()
{
    glaze::messageBox(*this,
                      std::string("Kalkyl ") + KALKYL_VERSION + kNl + "Luftledningsberäkningar",
                      "Om Kalkyl");
}
