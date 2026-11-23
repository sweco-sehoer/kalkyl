#include "main_window.h"

#include "conductor_dialog.h"

#include "appearance/appearance.h"
#include "version/version.h"

#include <algorithm>
#include <string>

namespace {

// Raderna i Bygg PM som väljer lina: Faslina, Topplina vänster och Topplina höger.
// Indexet är platsen i selectedConductor_. Rad 1 (Konfiguration) har en fast rullista i .glazeui.
constexpr int kConductorRows[] = {0, 2, 3};
// Platsen för Topplina höger i kConductorRows. Den har valet "Samma" överst i rullistan:
// samma lina som Topplina vänster, och då är id tomt.
constexpr std::size_t kRightTopIndex = 2;
constexpr const char* kSameText = "Samma";
// Kolumnen Värde, där linans namn står.
constexpr int kValueColumn = 1;

// Gruppernas bakgrundsfärger i Bygg PM, halvgenomskinliga så att de fungerar i ljust och mörkt läge.
const gfx::Color kBlue{60, 130, 210, 56};
const gfx::Color kGreen{70, 170, 90, 56};
const gfx::Color kAmber{220, 160, 40, 56};
const gfx::Color kPurple{150, 100, 210, 56};
const gfx::Color kRose{210, 90, 140, 56};

// Var en rad i Bygg PM visas. Both: alltid. Left: alltid, med sameLabel när topplinorna är samma.
// Right: bara när Topplina höger är en egen lina.
enum class Side { Both, Left, Right };

// Hur Värde matas in. Choice: rullista (linorna och konfigurationen, satta separat). Övriga är
// talfält där användaren bara skriver talet, och enheten visas efter talet (MPa, m, °C, Mall).
// Antalet decimaler följer det användaren skrivit.
enum class Unit { Choice, Mpa, Meter, Celsius, Mall };

// En rad i Bygg PM: texten (och texten när topplinorna är samma), färgen, när den visas, hur
// värdet matas in och mockupens förval för Värde (tal med punkt) och Referens. Raderna 0–3
// visas alltid och står först, så att kConductorRows gäller i båda lägena.
struct ByggPmRowDef {
    const char* label;
    const char* sameLabel;
    const gfx::Color* color;
    Side side;
    Unit unit;
    const char* value;
    const char* reference;
};

const ByggPmRowDef kByggPmRows[] = {
    {"Faslina", nullptr, &kBlue, Side::Both, Unit::Choice, "593 mm² FeAl CURLEW", ""},
    {"Konfiguration", nullptr, &kBlue, Side::Both, Unit::Choice, "Singel", ""},
    {"Topplina vänster", nullptr, &kBlue, Side::Both, Unit::Choice, "142 mm² FeAl DOTTEREL", ""},
    {"Topplina höger", nullptr, &kBlue, Side::Both, Unit::Choice, "124 mm² OPGW AFL 1x48 SMF 14230/20-01 (Dotterel)", ""},
    {"Uppspänning fas", nullptr, &kGreen, Side::Both, Unit::Mpa, "60", ""},
    {"Uppspänning topp vänster", "Uppspänning topp", &kGreen, Side::Left, Unit::Mpa, "90", ""},
    {"Uppspänning topp höger", nullptr, &kGreen, Side::Right, Unit::Mpa, "90", ""},
    {"Normalspann faslina", nullptr, &kGreen, Side::Both, Unit::Meter, "330", ""},
    {"Normalspann topplina vänster", "Normalspann topplina", &kGreen, Side::Left, Unit::Meter, "341", ""},
    {"Normalspann topplina höger", nullptr, &kGreen, Side::Right, Unit::Meter, "341", ""},
    {"Max temp", nullptr, &kAmber, Side::Both, Unit::Celsius, "50", ""},
    {"Min temp", nullptr, &kAmber, Side::Both, Unit::Celsius, "-50", ""},
    // Beräkningarna: normalspannsberäkning och linberäkning i par. Före ombyggnation är lila och
    // efter ombyggnation rosa. Med egen topplina höger kommer vänster-paret före höger-paret.
    {"Normalspannsberäkning faslina före ombyggnation", nullptr, &kPurple, Side::Both, Unit::Meter,"343.4", "STO252-532"},
    {"Linberäkning faslina före ombyggnation", nullptr, &kPurple, Side::Both, Unit::Mall, "429", "STP252-532"},
    {"Normalspannsberäkning faslina efter ombyggnation", nullptr, &kRose, Side::Both, Unit::Meter,"343.1", "STO252-532"},
    {"Linberäkning faslina efter ombyggnation", nullptr, &kRose, Side::Both, Unit::Mall, "429", "STP252-532 (380 flyttad)"},
    {"Normalspannsberäkning topplina vänster före ombyggnation", "Normalspannsberäkning topplina före ombyggnation", &kPurple, Side::Left, Unit::Meter,"", ""},
    {"Linberäkning topplina vänster före ombyggnation", "Linberäkning topplina före ombyggnation", &kPurple, Side::Left, Unit::Mall, "", ""},
    {"Normalspannsberäkning topplina höger före ombyggnation", nullptr, &kPurple, Side::Right, Unit::Meter,"", ""},
    {"Linberäkning topplina höger före ombyggnation", nullptr, &kPurple, Side::Right, Unit::Mall, "", ""},
    {"Normalspannsberäkning topplina vänster efter ombyggnation", "Normalspannsberäkning topplina efter ombyggnation", &kRose, Side::Left, Unit::Meter,"314.6", "STO371-385"},
    {"Linberäkning topplina vänster efter ombyggnation", "Linberäkning topplina efter ombyggnation", &kRose, Side::Left, Unit::Mall, "352", "STP371-385"},
    {"Normalspannsberäkning topplina höger efter ombyggnation", nullptr, &kRose, Side::Right, Unit::Meter,"", ""},
    {"Linberäkning topplina höger efter ombyggnation", nullptr, &kRose, Side::Right, Unit::Mall, "354", "STP371-385"},
};

// Talfältet för en enhet: enheten som suffix efter talet, tomt tillåtet och
// decimalerna i den kortaste formen (100 visas som 100, 100,123 som 100,123).
glaze::DataGrid::EditorSpec numberEditor(Unit unit)
{
    glaze::DataGrid::EditorSpec spec;
    spec.editor = glaze::DataGrid::Editor::Number;
    spec.allowEmpty = true;
    switch (unit) {
    case Unit::Mpa:     spec.suffix = " MPa"; break;
    case Unit::Meter:   spec.suffix = " m"; break;
    case Unit::Celsius: spec.suffix = " °C"; break;
    case Unit::Mall:    spec.suffix = " Mall"; break;
    default:            break;
    }
    return spec;
}

// Platsen för Topplina höger i kByggPmRows. Dess värde avgör läget.
constexpr std::size_t kRightTopDef = 3;

// Naturlig jämförelse som tabellernas sortering i GlazeUI: en följd siffror jämförs som ett tal
// ("910" efter "185"), allt annat tecken för tecken med ASCII-versaler vikta. Svarar <0, 0 eller >0.
// Används för rullistornas ordning, så att de står som kolumnen Namn i Lindata.
int compareNatural(const std::string& a, const std::string& b)
{
    auto isDigit = [](char c) { return c >= '0' && c <= '9'; };
    auto lower = [](char c) { return (c >= 'A' && c <= 'Z') ? static_cast<char>(c - 'A' + 'a') : c; };
    std::size_t i = 0, j = 0;
    while (i < a.size() && j < b.size()) {
        if (isDigit(a[i]) && isDigit(b[j])) {
            while (i < a.size() && a[i] == '0') ++i;   // inledande nollor räknas inte
            while (j < b.size() && b[j] == '0') ++j;
            std::size_t ei = i, ej = j;
            while (ei < a.size() && isDigit(a[ei])) ++ei;
            while (ej < b.size() && isDigit(b[ej])) ++ej;
            if (ei - i != ej - j)
                return (ei - i < ej - j) ? -1 : 1;     // fler siffror = större tal
            if (int c = a.compare(i, ei - i, b, j, ej - j); c != 0)
                return c;
            i = ei;
            j = ej;
            continue;
        }
        const unsigned char ca = static_cast<unsigned char>(lower(a[i]));
        const unsigned char cb = static_cast<unsigned char>(lower(b[j]));
        if (ca != cb)
            return ca < cb ? -1 : 1;
        ++i;
        ++j;
    }
    if (i < a.size()) return 1;
    if (j < b.size()) return -1;
    return 0;
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

// Startlogik, körs när fönstret laddas (aldrig i konstruktorn).
// Sätter utseendet, titeln med projektnamn, statusraden och kopplar menyernas handlers.
void MainWindow::onLoad(glaze::Widget*, glaze::EventArgs&)
{
    kalkyl::appearance::apply(*this, *sys_);
    kalkyl::appearance::syncButton(themeBtn, themeMode());

    const bool readOnly = project_->access() == kalkyl::projectdb::Access::ReadOnly;
    setTitle(project_->info().name + " – Kalkyl " + KALKYL_VERSION +
             (readOnly ? " (skrivskyddat)" : ""));
    statusBar.setPanelText(0, project_->path());

    if (readOnly) {
        const auto& lock = project_->lockedBy();
        statusBar.addPanel(lock ? "Skrivskyddat – öppnat av " + lock->user + " på " + lock->computer
                                : std::string("Skrivskyddat"));
    }
    if (!sys_->isPersistent())
        statusBar.addPanel("Inställningar sparas inte");

    menuBar.setHandler({"Arkiv", "Stäng projekt"}, [this] { closeProjectClicked(); });
    menuBar.setHandler({"Arkiv", "Avsluta"}, [this] { exitClicked(); });
    menuBar.setHandler({"Inställningar", "Lindata…"}, [this] { lindataClicked(); });
    menuBar.setHandler({"Hjälp", "Om Kalkyl"}, [this] { aboutClicked(); });

    rebuildByggPm();
    shadeStolpRows();
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

// Inställningar > Lindata…: visar linregistret modalt över huvudfönstret.
// Rullistorna läses om alltid, eftersom återställ startdata sparas direkt.
void MainWindow::lindataClicked()
{
    commitEdits();
    ConductorDialog dlg;
    dlg.setSystemDb(*sys_);
    dlg.run(*this);
    fillConductorChoices();
}

// Läser linregistret och ger Faslina och Topplina varsin rullista med linornas namn.
// Ett valt id visar linans nuvarande namn. Utan id matchas cellens text mot registret.
void MainWindow::fillConductorChoices()
{
    conductors_.clear();
    auto result = sys_->conductors();
    if (result)
        conductors_ = std::move(result.value());
    else
        gfx::log("Linregistret kunde inte läsas: %s", result.error().detail.c_str());

    std::vector<std::string> names;
    names.reserve(conductors_.size());
    for (const auto& c : conductors_)
        names.push_back(c.name);
    // Rullistan står efter namn, fallande: största arean först, som kolumnen Namn i Lindata.
    std::stable_sort(names.begin(), names.end(),
                     [](const std::string& a, const std::string& b) { return compareNatural(a, b) > 0; });

    // Topplina höger har "Samma" först, före linorna.
    std::vector<std::string> rightNames;
    rightNames.reserve(names.size() + 1);
    rightNames.push_back(kSameText);
    rightNames.insert(rightNames.end(), names.begin(), names.end());

    for (std::size_t i = 0; i < std::size(kConductorRows); ++i) {
        const int row = kConductorRows[i];
        const bool right = (i == kRightTopIndex);
        if (names.empty() && !right)
            byggPmGrid.setCellEditor(row, kValueColumn, {glaze::DataGrid::Editor::None});
        else
            byggPmGrid.setCellEditor(row, kValueColumn,
                                     {glaze::DataGrid::Editor::Choice, right ? rightNames : names});

        const auto find = [this](std::int64_t id) {
            return std::find_if(conductors_.begin(), conductors_.end(),
                                [id](const kalkyl::systemdb::Conductor& c) { return c.id == id; });
        };
        std::string tip;
        if (right && !selectedConductor_[i] && byggPmGrid.cell(row, kValueColumn) == kSameText) {
            tip = "Samma lina som Topplina vänster";
        } else if (names.empty()) {
            tip = "Linregistret är tomt";
        } else if (selectedConductor_[i]) {
            const auto it = find(*selectedConductor_[i]);
            if (it != conductors_.end()) {
                byggPmGrid.setCell(row, kValueColumn, it->name);
            } else {
                tip = "Linan finns inte längre i linregistret";
                selectedConductor_[i].reset();
            }
        } else {
            const std::string text = byggPmGrid.cell(row, kValueColumn);
            const auto it = std::find_if(conductors_.begin(), conductors_.end(),
                                         [&text](const kalkyl::systemdb::Conductor& c) {
                                             return kalkyl::systemdb::sameConductorName(c.name, text);
                                         });
            if (it != conductors_.end()) {
                selectedConductor_[i] = it->id;
                byggPmGrid.setCell(row, kValueColumn, it->name);
            } else {
                tip = "Finns inte i linregistret – välj en lina i listan";
            }
        }
        byggPmGrid.setCellToolTip(row, kValueColumn, tip);
    }
}

// Bygger raderna i Bygg PM efter läget: är Topplina höger "Samma" visas inga höger-rader och
// vänster-raderna heter som en topplina; annars visas vänster och höger var för sig. Värdena
// som syns sparas först i byggPmValues_, så att inget går förlorat när raderna byts.
void MainWindow::rebuildByggPm()
{
    if (byggPmValues_.empty()) {
        for (const auto& d : kByggPmRows)
            byggPmValues_.push_back({d.value, d.reference});
    } else {
        for (std::size_t r = 0; r < byggPmShown_.size(); ++r)
            byggPmValues_[byggPmShown_[r]] = {byggPmGrid.cell(static_cast<int>(r), kValueColumn),
                                              byggPmGrid.cell(static_cast<int>(r), kValueColumn + 1)};
    }
    byggPmSplit_ = byggPmValues_[kRightTopDef][0] != kSameText;

    // Raderna som syns i det här läget, med rätt text.
    std::vector<glaze::DataGrid::Row> rows;
    byggPmShown_.clear();
    for (std::size_t i = 0; i < std::size(kByggPmRows); ++i) {
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
    byggPmGrid.setCellEditor(1, kValueColumn,
                             {glaze::DataGrid::Editor::Choice, {"Singel", "Duplex", "Triplex"}});
    for (std::size_t r = 0; r < byggPmShown_.size(); ++r) {
        const Unit unit = kByggPmRows[byggPmShown_[r]].unit;
        if (unit != Unit::Choice)
            byggPmGrid.setCellEditor(static_cast<int>(r), kValueColumn, numberEditor(unit));
        // Referensen som tooltip, så att en lång text går att läsa när musen står över cellen.
        byggPmGrid.setCellToolTip(static_cast<int>(r), kValueColumn + 1,
                                  byggPmGrid.cell(static_cast<int>(r), kValueColumn + 1));
    }
    fillConductorChoices();
}

// En cell i Bygg PM sparades. Värdet sparas i byggPmValues_. För linraderna slås det valda namnet
// upp i registret och linans id sparas i minnet. Byter Topplina höger mellan "Samma" och en egen
// lina byggs raderna om, efter den här händelsen. Valet sparas inte i projektet.
void MainWindow::byggPmGridCommitted(glaze::Widget*, glaze::CellChangeArgs& e)
{
    if (e.row >= 0 && static_cast<std::size_t>(e.row) < byggPmShown_.size())
        byggPmValues_[byggPmShown_[e.row]] = {byggPmGrid.cell(e.row, kValueColumn),
                                              byggPmGrid.cell(e.row, kValueColumn + 1)};
    if (e.column == kValueColumn + 1)   // Referens: tooltipen följer texten, tom text ger ingen
        byggPmGrid.setCellToolTip(e.row, e.column, byggPmGrid.cell(e.row, e.column));
    if (e.column != kValueColumn)
        return;
    for (std::size_t i = 0; i < std::size(kConductorRows); ++i) {
        if (e.row != kConductorRows[i])
            continue;
        const std::string text = byggPmGrid.cell(e.row, kValueColumn);
        if (i == kRightTopIndex && text == kSameText) {
            selectedConductor_[i].reset();   // samma som vänster: inget eget id
            byggPmGrid.setCellToolTip(e.row, kValueColumn, "Samma lina som Topplina vänster");
        } else {
            const auto it = std::find_if(conductors_.begin(), conductors_.end(),
                                         [&text](const kalkyl::systemdb::Conductor& c) { return c.name == text; });
            if (it != conductors_.end()) {
                selectedConductor_[i] = it->id;
                byggPmGrid.setCellToolTip(e.row, kValueColumn, "");
            }
        }
        // Läget byts i pump-svansen, så att tabellen inte byggs om mitt i sin egen händelse.
        if (i == kRightTopIndex && (text != kSameText) != byggPmSplit_)
            glaze::app().post([this] { rebuildByggPm(); });
    }
}

// Namnlistens lägesknapp: System -> Mörkt -> Ljust -> System.
// Logiken delas med väljaren via kalkyl::appearance.
void MainWindow::themeBtnClicked(glaze::Widget*, glaze::ClickArgs&)
{
    kalkyl::appearance::cycle(*this, themeBtn, *sys_);
}

// Arkiv > Stäng projekt: main stänger projektet och visar väljaren igen.
// close() verkställs uppskjutet av GlazeUI.
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
                      std::string("Kalkyl ") + KALKYL_VERSION + "\nLuftledningsberäkningar",
                      "Om Kalkyl");
}
