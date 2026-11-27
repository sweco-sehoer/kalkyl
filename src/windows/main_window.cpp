#include "main_window.h"

#include "about_dialog.h"
#include "conductor_dialog.h"
#include "line_calc_dialog.h"
#include "normal_span_dialog.h"
#include "pole_import_dialog.h"

#include "appearance/appearance.h"
#include "lineCalc/line_calc.h"
#include "lineCalc/load_cases.h"
#include "tableText/table_text.h"
#include "platform/files.h"
#include "platform/system.h"
#include "version/version.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <ctime>
#include <functional>
#include <memory>
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
// Tooltipen på en linberäkningsrad utan kopplad beräkning.
constexpr const char* kLcRowHint = "Högerklicka för att lägga till eller koppla en linberäkning";
// Kolumnen Mall och Högsta utnyttjandegrad i listan Linberäkning, där Fel och över gränsen är röda.
constexpr int kLcMallColumn = 7;
constexpr int kLcUtilColumn = 8;

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
// hur värdet matas in, nyckeln i projektet (tom för beräkningsraderna, vars nyckel kommer ur
// ns eller lc), förvalet för Värde, lina och skede för normalspannsraderna (ns) och för
// linberäkningsraderna (lc). Raderna 0-3 visas alltid.
struct ByggPmRowDef {
    const char* label;
    const char* sameLabel;
    const gfx::Color* color;
    Side side;
    Unit unit;
    const char* key;
    const char* value;
    std::optional<Ns> ns;
    std::optional<Ns> lc;
};

const ByggPmRowDef kByggPmRows[] = {
    {"Faslina", nullptr, &kBlue, Side::Both, Unit::Choice, "phase_conductor", "", std::nullopt, std::nullopt},
    {"Konfiguration", nullptr, &kBlue, Side::Both, Unit::Choice, "configuration", "Singel", std::nullopt, std::nullopt},
    {"Topplina vänster", nullptr, &kBlue, Side::Both, Unit::Choice, "top_conductor_left", "", std::nullopt, std::nullopt},
    {"Topplina höger", nullptr, &kBlue, Side::Both, Unit::Choice, "top_conductor_right", "Samma", std::nullopt, std::nullopt},
    {"Uppspänning fas", nullptr, &kGreen, Side::Both, Unit::Mpa, "tension_phase", "", std::nullopt, std::nullopt},
    {"Uppspänning topp vänster", "Uppspänning topp", &kGreen, Side::Left, Unit::Mpa, "tension_top_left", "", std::nullopt, std::nullopt},
    {"Uppspänning topp höger", nullptr, &kGreen, Side::Right, Unit::Mpa, "tension_top_right", "", std::nullopt, std::nullopt},
    {"Normalspann faslina", nullptr, &kGreen, Side::Both, Unit::Meter, "normal_span_phase", "", std::nullopt, std::nullopt},
    {"Normalspann topplina vänster", "Normalspann topplina", &kGreen, Side::Left, Unit::Meter, "normal_span_top_left", "", std::nullopt, std::nullopt},
    {"Normalspann topplina höger", nullptr, &kGreen, Side::Right, Unit::Meter, "normal_span_top_right", "", std::nullopt, std::nullopt},
    {"Max temp", nullptr, &kAmber, Side::Both, Unit::Celsius, "temp_max", "", std::nullopt, std::nullopt},
    {"Min temp", nullptr, &kAmber, Side::Both, Unit::Celsius, "temp_min", "", std::nullopt, std::nullopt},
    // Beräkningarna: normalspannsberäkning och linberäkning i par. Först alla före ombyggnad
    // (lila), sedan alla efter ombyggnad (rosa). I varje del kommer faslinan först och sedan
    // topplinorna; med egen topplina höger kommer vänster-paret före höger-paret.
    {"Faslina normalspannsberäkning före ombyggnad", nullptr, &kPurple, Side::Both, Unit::Meter, nullptr, "", Ns{Wire::Phase, Stage::Before}, std::nullopt},
    {"Faslina linberäkning före ombyggnad", nullptr, &kPurple, Side::Both, Unit::Mall, nullptr, "", std::nullopt, Ns{Wire::Phase, Stage::Before}},
    {"Topplina vänster normalspannsberäkning före ombyggnad", "Topplinor normalspannsberäkning före ombyggnad", &kPurple, Side::Left, Unit::Meter, nullptr, "", Ns{Wire::TopLeft, Stage::Before}, std::nullopt},
    {"Topplina vänster linberäkning före ombyggnad", "Topplinor linberäkning före ombyggnad", &kPurple, Side::Left, Unit::Mall, nullptr, "", std::nullopt, Ns{Wire::TopLeft, Stage::Before}},
    {"Topplina höger normalspannsberäkning före ombyggnad", nullptr, &kPurple, Side::Right, Unit::Meter, nullptr, "", Ns{Wire::TopRight, Stage::Before}, std::nullopt},
    {"Topplina höger linberäkning före ombyggnad", nullptr, &kPurple, Side::Right, Unit::Mall, nullptr, "", std::nullopt, Ns{Wire::TopRight, Stage::Before}},
    {"Faslina normalspannsberäkning efter ombyggnad", nullptr, &kRose, Side::Both, Unit::Meter, nullptr, "", Ns{Wire::Phase, Stage::After}, std::nullopt},
    {"Faslina linberäkning efter ombyggnad", nullptr, &kRose, Side::Both, Unit::Mall, nullptr, "", std::nullopt, Ns{Wire::Phase, Stage::After}},
    {"Topplina vänster normalspannsberäkning efter ombyggnad", "Topplinor normalspannsberäkning efter ombyggnad", &kRose, Side::Left, Unit::Meter, nullptr, "", Ns{Wire::TopLeft, Stage::After}, std::nullopt},
    {"Topplina vänster linberäkning efter ombyggnad", "Topplinor linberäkning efter ombyggnad", &kRose, Side::Left, Unit::Mall, nullptr, "", std::nullopt, Ns{Wire::TopLeft, Stage::After}},
    {"Topplina höger normalspannsberäkning efter ombyggnad", nullptr, &kRose, Side::Right, Unit::Meter, nullptr, "", Ns{Wire::TopRight, Stage::After}, std::nullopt},
    {"Topplina höger linberäkning efter ombyggnad", nullptr, &kRose, Side::Right, Unit::Mall, nullptr, "", std::nullopt, Ns{Wire::TopRight, Stage::After}},
};

constexpr std::size_t kRowCount = std::size(kByggPmRows);

// Dokumentationens förval i ett nytt projekt: nyckel, värde och om värdet är en lina ur
// linregistret. Topplina höger står kvar på förvalet Samma, så topplinan har en rad per uppgift.
struct NewProjectDefault {
    const char* key;
    const char* value;
    bool conductor;
};

constexpr NewProjectDefault kNewProjectDefaults[] = {
    {"phase_conductor", "593 mm² FeAl CURLEW", true},
    {"configuration", "Triplex", false},
    {"top_conductor_left", "241 mm² FeAl ATLE", true},
    {"tension_phase", "60", false},
    {"tension_top_left", "90", false},
    {"temp_max", "50", false},
    {"temp_min", "-50", false},
};

// Platsen för Topplina höger i kByggPmRows. Dess värde avgör läget.
constexpr std::size_t kRightTopDef = 3;

// Radens nyckel i projektet: den fasta nyckeln, eller normalspanns- eller linberäkningsnyckeln
// ur lina och skede.
std::string rowKey(const ByggPmRowDef& d)
{
    if (d.ns)
        return kalkyl::projectdb::normalSpanDocKey(d.ns->first, d.ns->second);
    if (d.lc)
        return kalkyl::projectdb::lineCalcDocKey(d.lc->first, d.lc->second);
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

// Första och sista stolpens Stpnr, ur resultatet när det finns. Saknas resultatet för en
// automatisk beräkning är det sträckans Stolpplatser, annars stolparnas.
std::pair<std::string, std::string> polesOf(const kalkyl::projectdb::NormalSpan& ns,
                                            const kalkyl::ancalc::SpanCalculation& calc)
{
    if (calc.result)
        return {calc.result->fromPole, calc.result->toPole};
    if (ns.automatic)
        return {ns.automatic->range.fromPole, ns.automatic->range.toPole};
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
std::string rangeText(const kalkyl::projectdb::NormalSpan& ns, const kalkyl::ancalc::SpanCalculation& calc)
{
    const auto poles = polesOf(ns, calc);
    return poleText(poles.first) + " – " + poleText(poles.second);
}

// Det första felet i beräkningen (med rad när den finns), eller tomt när den saknar fel.
std::string firstError(const kalkyl::ancalc::SpanCalculation& calc)
{
    for (const auto& issue : calc.issues) {
        if (issue.kind != kalkyl::ancalc::IssueKind::Error)
            continue;
        if (issue.poleIndex >= 0)
            return "rad " + std::to_string(issue.poleIndex + 1) + ", " + issue.message;
        return issue.message;
    }
    return {};
}

// Det första felet i en automatisk beräkning: "ST0105: Spannlängd saknas.", eller "rad 3: …"
// när stolpen saknar Stpnr. Ett fel utan stolpe (sträckans eget) visas som det är.
std::string autoErrorText(const kalkyl::projectdb::NormalSpan& ns, const kalkyl::ancalc::SpanCalculation& calc)
{
    for (const auto& issue : calc.issues) {
        if (issue.kind != kalkyl::ancalc::IssueKind::Error)
            continue;
        if (issue.poleIndex < 0)
            return issue.message;
        const auto index = static_cast<std::size_t>(issue.poleIndex);
        if (index < ns.poles.size() && !ns.poles[index].pole.empty())
            return ns.poles[index].pole + ": " + issue.message;
        return "rad " + std::to_string(issue.poleIndex + 1) + ": " + issue.message;
    }
    return {};
}

// Radens ton: färgen med alfa 64 på jämna och 52 på udda rader, så att raderna är lätta att följa.
// Används av Dokumentation och listorna i flikarna Normalspann och Linberäkning.
gfx::Color rowTone(gfx::Color color, std::size_t index)
{
    color.a = (index % 2 == 0) ? 64 : 52;
    return color;
}

// Tonen på en rad i flikarna Normalspann och Linberäkning: gul för en vanlig beräkning, och för en
// automatisk skedets färg, lila för före och rosa för efter.
gfx::Color calcTone(const std::optional<kalkyl::projectdb::AutoOrigin>& automatic, std::size_t index)
{
    if (!automatic)
        return rowTone(kAmber, index);
    return rowTone(automatic->stage == kalkyl::projectdb::Stage::Before ? kPurple : kRose, index);
}

// Upplysningens text, med lastfallet först när den gäller ett lastfall ("Lastfall 4: ...").
std::string issueText(const kalkyl::linecalc::LineIssue& issue)
{
    if (issue.caseId > 0)
        return "Lastfall " + std::to_string(issue.caseId) + ": " + issue.message;
    return issue.message;
}

// Första upplysningen av slaget kind, med " (och n till)" när fler av samma slag finns.
// Tomt svar när ingen finns.
std::string firstIssueOf(const kalkyl::linecalc::LineCalculation& calc, kalkyl::linecalc::LineIssueKind kind)
{
    std::string first;
    int count = 0;
    for (const auto& issue : calc.issues) {
        if (issue.kind != kind)
            continue;
        if (count == 0)
            first = issueText(issue);
        ++count;
    }
    if (count > 1)
        first += " (och " + std::to_string(count - 1) + " till)";
    return first;
}

// Sant när beräkningen saknar resultat på grund av ett fel (inte bara saknade indata).
bool hasLineError(const kalkyl::linecalc::LineCalculation& calc)
{
    return !calc.result && !firstIssueOf(calc, kalkyl::linecalc::LineIssueKind::Error).empty();
}

// Stakningsmallen som heltal, eller "–" utan resultat.
std::string mallText(const kalkyl::linecalc::LineCalculation& calc)
{
    if (!calc.result)
        return "–";
    return kalkyl::tabletext::formatDecimal(std::round(calc.result->stakingMall), 0, '.');
}

// Talet i kortaste form med decimalkomma och riktigt minustecken, t.ex. "−50".
std::string shortNumber(double v)
{
    std::string text = kalkyl::tabletext::formatDecimal(v, -1, ',');
    if (!text.empty() && text[0] == '-')
        text.replace(0, 1, "−");
    return text;
}

// Lastfallen vars utnyttjandegrad är över gränsen, t.ex. "4, 5". Tomt när inget är över.
std::string overCases(const kalkyl::linecalc::LineResult& r)
{
    std::string text;
    for (const auto& c : r.cases) {
        if (!c.overUtilization)
            continue;
        if (!text.empty())
            text += ", ";
        text += std::to_string(c.id);
    }
    return text;
}

// Högsta utnyttjandegraden som heltal och lastfallen som visar samma värde, t.ex. "40 (4, 5)".
// "–" utan resultat eller utan brottlast.
std::string utilizationText(const kalkyl::linecalc::LineCalculation& calc)
{
    if (!calc.result)
        return "–";
    double best = 0;
    bool any = false;
    for (const auto& c : calc.result->cases) {
        if (c.utilizationPercent && (!any || *c.utilizationPercent > best)) {
            best = *c.utilizationPercent;
            any = true;
        }
    }
    if (!any)
        return "–";
    const double shown = std::round(best);
    std::string ids;
    for (const auto& c : calc.result->cases) {
        if (c.utilizationPercent && std::round(*c.utilizationPercent) == shown)
            ids += (ids.empty() ? "" : ", ") + std::to_string(c.id);
    }
    return kalkyl::tabletext::formatDecimal(shown, 0, '.') + " (" + ids + ")";
}

// Linans namn i listor och tooltips: "(utan namn)" för lindata utan lina, "–" utan lindata.
std::string conductorText(const kalkyl::linecalc::LineInput& input)
{
    if (!input.conductor)
        return "–";
    return input.conductor->name.empty() ? std::string("(utan namn)") : input.conductor->name;
}

// aₙ i en decimal, eller "–" när det saknas.
std::string spanText(const kalkyl::linecalc::LineInput& input)
{
    return input.normalSpanM ? kalkyl::tabletext::formatDecimal(*input.normalSpanM, 1, ',') : std::string("–");
}

// σ₀ i kortaste form, eller "–" när det saknas.
std::string tensionText(const kalkyl::linecalc::LineInput& input)
{
    return input.tensionMpa ? shortNumber(*input.tensionMpa) : std::string("–");
}

// Filtret i vyn ur systemdatabasens val. Rullistans post 0 är Förändrade ±2 och post 1 är Alla.
kalkyl::poleview::Filter viewFilterOf(kalkyl::systemdb::AfterFilter filter)
{
    return filter == kalkyl::systemdb::AfterFilter::All ? kalkyl::poleview::Filter::All
                                                        : kalkyl::poleview::Filter::Changed;
}

// Systemdatabasens val ur filtret i vyn.
kalkyl::systemdb::AfterFilter sysFilterOf(kalkyl::poleview::Filter filter)
{
    return filter == kalkyl::poleview::Filter::All ? kalkyl::systemdb::AfterFilter::All
                                                   : kalkyl::systemdb::AfterFilter::Changed;
}

// Filtrets post i rullistan efterFilterDrop: 0 är Förändrade ±2 och 1 är Alla.
int dropIndexOf(kalkyl::poleview::Filter filter)
{
    return filter == kalkyl::poleview::Filter::All ? 1 : 0;
}

// Filtret för en post i rullistan efterFilterDrop.
kalkyl::poleview::Filter filterOfDropIndex(int index)
{
    return index == 1 ? kalkyl::poleview::Filter::All : kalkyl::poleview::Filter::Changed;
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
    statusBar.addPanel("");
    gridStatusPanel_ = statusBar.panelCount() - 1;
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

    // Ett nytt projekt får förvalen och sparas direkt, så att filen har dem från början.
    const bool defaults = project_->created() && !readOnly();
    if (defaults)
        applyNewProjectDefaults();

    loadProjectData();
    rebuildByggPm();

    // Stolptabellerna, med PoleGrid och en värd för varje, och pekarna till panelerna.
    beforeHost_ = std::make_unique<PoleHost>(*this, Stage::Before);
    afterHost_ = std::make_unique<PoleHost>(*this, Stage::After);
    beforeGrid_ = std::make_unique<kalkyl::polegrid::PoleGrid>(
        *this, stolpGrid, *beforeHost_, std::string(kalkyl::projectdb::poleTableName(Stage::Before)));
    afterGrid_ = std::make_unique<kalkyl::polegrid::PoleGrid>(
        *this, efterGrid, *afterHost_, std::string(kalkyl::projectdb::poleTableName(Stage::After)),
        kalkyl::poletable::afterColumns());
    beforePanel_.check = &stolpAutoChk;
    beforePanel_.names = {&stolpFasLbl, &stolpToppVLbl, &stolpToppHLbl};
    beforePanel_.drops = {&stolpFasFran, &stolpFasTill, &stolpToppVFran, &stolpToppVTill, &stolpToppHFran, &stolpToppHTill};
    beforePanel_.status = {&stolpFasStatus, &stolpToppVStatus, &stolpToppHStatus};
    afterPanel_.check = &efterAutoChk;
    afterPanel_.names = {&efterFasLbl, &efterToppVLbl, &efterToppHLbl};
    afterPanel_.drops = {&efterFasFran, &efterFasTill, &efterToppVFran, &efterToppVTill, &efterToppHFran, &efterToppHTill};
    afterPanel_.status = {&efterFasStatus, &efterToppVStatus, &efterToppHStatus};

    // Stolptabell efter jämförs med före, och filtret är det sparade valet. Rullistan och
    // knappen släcks inte skrivskyddat, eftersom de bara ändrar visningen.
    afterGrid_->setComparesWithBefore(true);
    applyAfterFilter(viewFilterOf(sys_->afterFilter()), false, false);

    // Skrivskyddat: rutorna, rullistorna och knapparna släcks, och tabellerna låses.
    if (readOnly()) {
        beforeGrid_->setReadOnly(true);
        afterGrid_->setReadOnly(true);
        for (AutoPanel* panel : {&beforePanel_, &afterPanel_}) {
            panel->check->disable();
            for (glaze::Dropdown* drop : panel->drops)
                drop->disable();
        }
        stolpImportBtn.disable();
        efterKopieraBtn.disable();
        efterSomForeBtn.disable();
    }

    loadPoleTables();
    refreshAutomatic();
    updateTitle();
    restartAutosave();
    if (defaults && project_->isModified())
        startSave(SaveKind::Auto);
}

// Skriver förvalen i ett nytt projekt. Linorna kopieras ur linregistret med setDocConductor,
// övriga värden skrivs med setDocValue. Saknas en lina i registret hoppas den över.
void MainWindow::applyNewProjectDefaults()
{
    const auto registry = sys_->conductors();
    if (!registry)
        gfx::log("Linregistret kunde inte läsas för förvalen: %s", registry.error().detail.c_str());
    for (const auto& d : kNewProjectDefaults) {
        kalkyl::core::Result<void> saved;
        if (d.conductor) {
            const auto* chosen = registry ? findConductor(registry.value(), d.value) : nullptr;
            if (!chosen) {
                gfx::log("Förvalet %s finns inte i linregistret.", d.value);
                continue;
            }
            saved = kalkyl::projectdb::setDocConductor(*project_, d.key, *chosen);
        } else {
            saved = kalkyl::projectdb::setDocValue(*project_, {d.key, d.value, ""});
        }
        if (!saved)
            gfx::log("Förvalet %s kunde inte sparas: %s", d.key, saved.error().detail.c_str());
    }
}

// Läser båda stolptabellerna och inställningarna för automatiskt läge ur projektet. Ett fel
// loggas och ger en meddelanderuta, och tabellen blir tom med automatiskt läge av.
void MainWindow::loadPoleTables()
{
    for (const Stage stage : {Stage::Before, Stage::After}) {
        const std::string table(kalkyl::projectdb::poleTableName(stage));
        auto rows = kalkyl::projectdb::poles(*project_, stage);
        auto settings = kalkyl::projectdb::autoSettings(*project_, stage);
        if (!rows || !settings) {
            const auto& error = !rows ? rows.error() : settings.error();
            gfx::log("%s kunde inte läsas: %s", table.c_str(), error.detail.c_str());
            glaze::messageBox(*this, table + " kunde inte läsas." + kNl + error.message, "Kalkyl",
                              glaze::MessageBoxButtons::OK, glaze::MessageBoxType::Error);
            poleGridOf(stage).setRows({});
            settingsOf(stage) = {};
            continue;
        }
        poleGridOf(stage).setRows(std::move(rows.value()));
        settingsOf(stage) = settings.value();
    }
}

// Sparar hela tabellen för skedet i projektets arbetskopia. Vid fel loggas det och en ruta
// visas. Svarar om det lyckades.
bool MainWindow::savePoles(Stage stage, const std::vector<kalkyl::poletable::PoleRow>& rows)
{
    const auto saved = kalkyl::projectdb::setPoles(*project_, stage, rows);
    if (!saved) {
        const std::string table(kalkyl::projectdb::poleTableName(stage));
        gfx::log("%s kunde inte sparas: %s", table.c_str(), saved.error().detail.c_str());
        glaze::messageBox(*this, table + " kunde inte sparas." + kNl + saved.error().message, "Kalkyl",
                          glaze::MessageBoxButtons::OK, glaze::MessageBoxType::Error);
        return false;
    }
    updateTitle();
    return true;
}

// Tabellens rader som de står i projektet. Vid fel loggas det och listan är tom.
std::vector<kalkyl::poletable::PoleRow> MainWindow::reloadPoles(Stage stage)
{
    auto rows = kalkyl::projectdb::poles(*project_, stage);
    if (!rows) {
        gfx::log("%s kunde inte läsas: %s", std::string(kalkyl::projectdb::poleTableName(stage)).c_str(),
                 rows.error().detail.c_str());
        return {};
    }
    return std::move(rows.value());
}

// Stolptabellernas händelser går vidare till PoleGrid, som äger redigeringen, Excel-markeringen,
// papperskorgen och snabbmenyn. Stolptabell före först: en cell sparades.
void MainWindow::stolpGridCommitted(glaze::Widget*, glaze::CellChangeArgs& e)
{
    if (beforeGrid_)
        beforeGrid_->committed(e);
}

// Stolptabell före: ett klick i en cell, till exempel på papperskorgen, går till PoleGrid.
// Före onLoad finns ingen PoleGrid, och då händer ingenting.
void MainWindow::stolpGridCellClicked(glaze::Widget*, glaze::CellClickArgs& e)
{
    if (beforeGrid_)
        beforeGrid_->cellClicked(e);
}

// Stolptabell före: tangenterna för Excel-markeringen och urklippet (Shift, Ctrl+C, X, V, A och
// Delete) går till PoleGrid.
void MainWindow::stolpGridKeyDown(glaze::Widget*, glaze::KeyArgs& e)
{
    if (beforeGrid_)
        beforeGrid_->keyDown(e);
}

// Stolptabell före: Shift+klick utökar markeringen i PoleGrid.
// Musknappen släpps efter klicket, så att tabellen redan har flyttat den aktuella cellen.
void MainWindow::stolpGridMouseUp(glaze::Widget*, glaze::MouseArgs& e)
{
    if (beforeGrid_)
        beforeGrid_->mouseUp(e);
}

// Stolptabell före: snabbmenyn med urklipp, Infoga rad ovanför, Radera rader… och undermenyn
// Sträcka byggs av PoleGrid och värden.
void MainWindow::stolpGridContextMenu(glaze::Widget*, glaze::ContextMenuArgs& e)
{
    if (beforeGrid_)
        beforeGrid_->contextMenu(e);
}

// Stolptabell efter: samma händelser som i före, till sin egen PoleGrid. En cell sparades.
// Före onLoad finns ingen PoleGrid, och då händer ingenting.
void MainWindow::efterGridCommitted(glaze::Widget*, glaze::CellChangeArgs& e)
{
    if (afterGrid_)
        afterGrid_->committed(e);
}

// Stolptabell efter: ett klick i en cell, till exempel på papperskorgen, går till PoleGrid.
// Före onLoad finns ingen PoleGrid, och då händer ingenting.
void MainWindow::efterGridCellClicked(glaze::Widget*, glaze::CellClickArgs& e)
{
    if (afterGrid_)
        afterGrid_->cellClicked(e);
}

// Stolptabell efter: tangenterna för Excel-markeringen och urklippet (Shift, Ctrl+C, X, V, A och
// Delete) går till PoleGrid.
void MainWindow::efterGridKeyDown(glaze::Widget*, glaze::KeyArgs& e)
{
    if (afterGrid_)
        afterGrid_->keyDown(e);
}

// Stolptabell efter: Shift+klick utökar markeringen i PoleGrid.
// Musknappen släpps efter klicket, så att tabellen redan har flyttat den aktuella cellen.
void MainWindow::efterGridMouseUp(glaze::Widget*, glaze::MouseArgs& e)
{
    if (afterGrid_)
        afterGrid_->mouseUp(e);
}

// Stolptabell efter: snabbmenyn med urklipp, Infoga rad ovanför, Radera rader… och undermenyn
// Sträcka byggs av PoleGrid och värden.
void MainWindow::efterGridContextMenu(glaze::Widget*, glaze::ContextMenuArgs& e)
{
    if (afterGrid_)
        afterGrid_->contextMenu(e);
}

// Värden till PoleGrid: sparar tabellen i projektet. Ett fel har visats av savePoles, och
// tabellen läses då om av PoleGrid.
bool MainWindow::PoleHost::savePoles(const std::vector<kalkyl::poletable::PoleRow>& rows)
{
    return window_.savePoles(stage_, rows);
}

// Värden till PoleGrid: raderna som de står i projektet.
std::vector<kalkyl::poletable::PoleRow> MainWindow::PoleHost::reloadPoles()
{
    return window_.reloadPoles(stage_);
}

// Värden till PoleGrid: tabellen är sparad med en ändring, så beräkningarna räknas om.
void MainWindow::PoleHost::polesChanged()
{
    window_.refreshAutomatic();
}

// Värden till PoleGrid: en Stolpplats döptes om, så sträckorna följer med.
void MainWindow::PoleHost::poleRenamed(const std::string& from, const std::string& to)
{
    window_.renameInRanges(stage_, from, to);
}

// Värden till PoleGrid: undermenyn Sträcka sist i snabbmenyn.
void MainWindow::PoleHost::addMenuItems(glaze::Menu& menu, int row)
{
    window_.addRangeMenuItems(stage_, menu, row);
}

// Värden till PoleGrid: noten om sträckor i frågan Radera.
std::string MainWindow::PoleHost::removeNote(int first, int last) const
{
    return window_.rangeNote(stage_, first, last);
}

// Värden till PoleGrid: en kort text i statusraden.
void MainWindow::PoleHost::showStatus(const std::string& text)
{
    window_.setPoleStatus(text);
}

// Värden till PoleGrid: filtret byttes i tabellen (Visa alla rader, eller en dold stolpe som
// visas). Valet sparas och rullistan följer.
void MainWindow::PoleHost::filterChanged(kalkyl::poleview::Filter filter)
{
    window_.applyAfterFilter(filter, true, false);
}

// Tabellen för skedet.
kalkyl::polegrid::PoleGrid& MainWindow::poleGridOf(Stage stage) const
{
    return stage == Stage::Before ? *beforeGrid_ : *afterGrid_;
}

// Inställningen för skedet.
kalkyl::projectdb::AutoSettings& MainWindow::settingsOf(Stage stage)
{
    return stage == Stage::Before ? autoBefore_ : autoAfter_;
}

const kalkyl::projectdb::AutoSettings& MainWindow::settingsOf(Stage stage) const
{
    return stage == Stage::Before ? autoBefore_ : autoAfter_;
}

// Panelen Automatiskt läge för skedet.
MainWindow::AutoPanel& MainWindow::panelOf(Stage stage)
{
    return stage == Stage::Before ? beforePanel_ : afterPanel_;
}

// De linor som är automatiska: faslinan och topplina vänster (Topplinor när topplinorna är
// samma), och topplina höger när den är en egen lina.
std::vector<Wire> MainWindow::activeWires() const
{
    std::vector<Wire> wires = {Wire::Phase, Wire::TopLeft};
    if (byggPmSplit_)
        wires.push_back(Wire::TopRight);
    return wires;
}

// Skedets sträckor med linornas ord för de aktiva linorna. Ordet börjar med en liten bokstav och
// får versal först i en mening (tableMarks).
std::vector<kalkyl::automode::NamedRange> MainWindow::namedRanges(Stage stage) const
{
    std::vector<kalkyl::automode::NamedRange> named;
    for (const Wire wire : activeWires())
        named.push_back({kalkyl::projectdb::autoLineWord(wire, !byggPmSplit_),
                         settingsOf(stage).ranges[static_cast<std::size_t>(wire)]});
    return named;
}

// Läser om de automatiska beräkningarna ur projektet och uppdaterar allt som visar dem:
// Dokumentation och flikarna Normalspann och Linberäkning, panelerna och tabellernas färger.
void MainWindow::refreshAutomatic()
{
    refreshNormalSpans();
    refreshLineCalcs();
    updateAutoPanels();
    updatePoleLooks();
}

// Fyller panelen Automatiskt läge för båda skedena.
void MainWindow::updateAutoPanels()
{
    fillAutoPanel(Stage::Before);
    fillAutoPanel(Stage::After);
}

// Fyller panelen för skedet. Rullistorna byggs bara om när tabellens Stolpplatser ändrats,
// eftersom en omfyllning stänger en öppen lista. Statustexten visas också när läget är av.
void MainWindow::fillAutoPanel(Stage stage)
{
    AutoPanel& panel = panelOf(stage);
    const auto& settings = settingsOf(stage);
    const auto& rows = poleGridOf(stage).rows();
    const std::string table(kalkyl::projectdb::poleTableName(stage));
    panel.check->setChecked(settings.enabled);

    // Stolpplatserna i tabellens ordning, en gång var.
    std::vector<std::string> poles;
    for (const auto& row : rows) {
        const std::string pole = kalkyl::tabletext::trimmed(row.pole);
        if (!pole.empty() && std::find(poles.begin(), poles.end(), pole) == poles.end())
            poles.push_back(pole);
    }

    const bool split = byggPmSplit_;
    for (std::size_t w = 0; w < 3; ++w) {
        const bool shown = w < 2 || split;
        const Wire wire = static_cast<Wire>(w);
        panel.names[w]->setText(kalkyl::projectdb::autoLineLabel(wire, !split));
        if (shown) {
            panel.names[w]->show();
            panel.status[w]->show();
        } else {
            panel.names[w]->hide();
            panel.status[w]->hide();
        }

        // En rullista per ände: "–" och Stolpplatserna, med en sparad Stolpplats som saknas sist.
        for (std::size_t end = 0; end < 2; ++end) {
            const std::size_t i = w * 2 + end;
            glaze::Dropdown& drop = *panel.drops[i];
            if (shown)
                drop.show();
            else
                drop.hide();
            const std::string& saved = end == 0 ? settings.ranges[w].fromPole : settings.ranges[w].toPole;

            std::vector<std::string> itemPoles = {std::string()};
            std::vector<std::string> itemTexts = {"–"};
            int selected = 0;
            for (const auto& pole : poles) {
                if (pole == saved)
                    selected = static_cast<int>(itemPoles.size());
                itemPoles.push_back(pole);
                itemTexts.push_back(pole);
            }
            if (!saved.empty() && selected == 0) {
                selected = static_cast<int>(itemPoles.size());
                itemPoles.push_back(saved);
                itemTexts.push_back(saved + " (finns inte)");
            }
            if (itemTexts != panel.itemTexts[i]) {
                drop.setItems(itemTexts);
                panel.itemPoles[i] = std::move(itemPoles);
                panel.itemTexts[i] = std::move(itemTexts);
            }
            drop.setSelectedIndex(selected);
        }

        // Statustexten ur samma beräkning som de automatiska beräkningarna bygger på.
        if (shown) {
            const auto span = kalkyl::automode::rangeSpan(rows, settings.ranges[w],
                                                          kalkyl::projectdb::autoLineWord(wire, !split), table);
            const auto status = kalkyl::automode::rangeStatus(span, rows);
            panel.status[w]->setText(status.text);
            if (status.ok)
                panel.status[w]->setRole(glaze::Label::Role::Dim);
            else
                panel.status[w]->setColor(gfx::Color{200, 0, 0});
        }
    }
}

// Räknar om markeringarna i båda tabellerna och jämförelsen mellan efter och före. Markeringarna
// av sträckorna visas bara när automatiskt läge är på för skedet.
void MainWindow::updatePoleLooks()
{
    const auto& before = beforeGrid_->rows();
    const auto& after = afterGrid_->rows();

    kalkyl::polegrid::PoleLooks beforeLooks;
    beforeLooks.marks = kalkyl::automode::tableMarks(
        before, autoBefore_.enabled ? namedRanges(Stage::Before) : std::vector<kalkyl::automode::NamedRange>{},
        kalkyl::projectdb::poleTableName(Stage::Before));
    beforeGrid_->setLooks(std::move(beforeLooks));

    kalkyl::polegrid::PoleLooks afterLooks;
    afterLooks.compare = kalkyl::automode::compareAfter(before, after);
    afterLooks.before = before;
    afterLooks.marks = kalkyl::automode::tableMarks(
        after, autoAfter_.enabled ? namedRanges(Stage::After) : std::vector<kalkyl::automode::NamedRange>{},
        kalkyl::projectdb::poleTableName(Stage::After));
    afterGrid_->setLooks(std::move(afterLooks));
    updateAfterFilterInfo();
}

// Infotexten bredvid Visa förändrade ur poleview::summarize, och knappen är tänd när minst en
// rad är förändrad eller har en borttagen stolpe bredvid sig.
void MainWindow::updateAfterFilterInfo()
{
    const auto summary = kalkyl::poleview::summarize(beforeGrid_->rows(), afterGrid_->rows(), afterFilter_);
    efterFilterInfo.setText(kalkyl::poleview::summaryText(summary, afterFilter_));
    if (summary.marked > 0)
        efterVisaBtn.enable();
    else
        efterVisaBtn.disable();
}

// Sätter filtret i Stolptabell efter: rullistan, PoleGrid och, med save, systemdatabasen. Ett fel
// vid sparningen loggas och visas i statusraden, och valet gäller ändå under sessionen. center
// rullar tabellen till de förändrade stolparna.
void MainWindow::applyAfterFilter(kalkyl::poleview::Filter filter, bool save, bool center)
{
    commitEdits();
    afterFilter_ = filter;
    efterFilterDrop.setSelectedIndex(dropIndexOf(filter));
    afterGrid_->setFilter(filter);
    if (save) {
        const auto saved = sys_->setAfterFilter(sysFilterOf(filter));
        if (!saved) {
            gfx::log("Filtret kunde inte sparas: %s", saved.error().detail.c_str());
            setPoleStatus("Filtret kunde inte sparas: " + saved.error().message);
        }
    }
    if (center)
        afterGrid_->showChanged();
    updateAfterFilterInfo();
}

// Sparar inställningen för skedet. Vid fel loggas det och en ruta visas, och inställningen läses
// om så att panelen visar det som gäller. Annars räknas allt om.
void MainWindow::saveAutoSettings(Stage stage, const kalkyl::projectdb::AutoSettings& settings)
{
    const auto saved = kalkyl::projectdb::setAutoSettings(*project_, stage, settings);
    if (!saved) {
        gfx::log("Inställningen kunde inte sparas: %s", saved.error().detail.c_str());
        glaze::messageBox(*this, "Inställningen för automatiskt läge kunde inte sparas." + kNl + saved.error().message,
                          "Kalkyl", glaze::MessageBoxButtons::OK, glaze::MessageBoxType::Error);
        if (auto current = kalkyl::projectdb::autoSettings(*project_, stage))
            settingsOf(stage) = current.value();
        updateAutoPanels();
        return;
    }
    settingsOf(stage) = settings;
    updateTitle();
    refreshAutomatic();
}

// Rutan Automatiskt läge i Stolptabell före.
void MainWindow::stolpAutoChkChanged(glaze::Widget*, glaze::ChangeArgs&)
{
    auto settings = autoBefore_;
    settings.enabled = stolpAutoChk.isChecked();
    saveAutoSettings(Stage::Before, settings);
    if (autoBefore_.enabled == settings.enabled)
        setPoleStatus(settings.enabled
                          ? "Automatiskt läge är på för Stolptabell före. Raderna före ombyggnad i Dokumentation visar de automatiska beräkningarna."
                          : "Automatiskt läge är av för Stolptabell före. Raderna i Dokumentation visar det de visade innan.");
}

// Rutan Automatiskt läge i Stolptabell efter.
void MainWindow::efterAutoChkChanged(glaze::Widget*, glaze::ChangeArgs&)
{
    auto settings = autoAfter_;
    settings.enabled = efterAutoChk.isChecked();
    saveAutoSettings(Stage::After, settings);
    if (autoAfter_.enabled == settings.enabled)
        setPoleStatus(settings.enabled
                          ? "Automatiskt läge är på för Stolptabell efter. Raderna efter ombyggnad i Dokumentation visar de automatiska beräkningarna."
                          : "Automatiskt läge är av för Stolptabell efter. Raderna i Dokumentation visar det de visade innan.");
}

// En rullista Från eller Till ändrades. Avsändaren hittas bland panelernas rullistor, och postens
// Stolpplats skrivs i sträckan. En post "(finns inte)" har den sparade texten, så den ändrar inget.
void MainWindow::rangeChanged(glaze::Widget* sender, glaze::ChangeArgs&)
{
    for (const Stage stage : {Stage::Before, Stage::After}) {
        AutoPanel& panel = panelOf(stage);
        for (std::size_t i = 0; i < panel.drops.size(); ++i) {
            if (sender != panel.drops[i])
                continue;
            const int chosen = panel.drops[i]->selectedIndex();
            if (chosen < 0 || static_cast<std::size_t>(chosen) >= panel.itemPoles[i].size())
                return;
            auto settings = settingsOf(stage);
            auto& range = settings.ranges[i / 2];
            (i % 2 == 0 ? range.fromPole : range.toPole) = panel.itemPoles[i][static_cast<std::size_t>(chosen)];
            if (settings != settingsOf(stage))
                saveAutoSettings(stage, settings);
            return;
        }
    }
}

// Undermenyn Sträcka i tabellens snabbmeny: en post för start och en för slut per aktiv lina.
// Posterna är aktiva när projektet är skrivbart och raden har en Stolpplats.
void MainWindow::addRangeMenuItems(Stage stage, glaze::Menu& menu, int row)
{
    const auto& rows = poleGridOf(stage).rows();
    std::string pole;
    if (row >= 0 && static_cast<std::size_t>(row) < rows.size())
        pole = kalkyl::tabletext::trimmed(rows[static_cast<std::size_t>(row)].pole);
    const bool enabled = !readOnly() && !pole.empty();

    glaze::MenuItem& submenu = menu.addSubmenu("Sträcka");
    for (const Wire wire : activeWires()) {
        const std::string word = kalkyl::automode::capitalized(kalkyl::projectdb::autoLineWord(wire, !byggPmSplit_));
        for (const bool start : {true, false}) {
            glaze::MenuItem entry;
            entry.text = word + (start ? " börjar här" : " slutar här");
            entry.enabled = enabled;
            entry.onClick = [this, stage, wire, start, pole] {
                auto settings = settingsOf(stage);
                auto& range = settings.ranges[static_cast<std::size_t>(wire)];
                (start ? range.fromPole : range.toPole) = pole;
                saveAutoSettings(stage, settings);
            };
            submenu.submenu.push_back(std::move(entry));
        }
    }
}

// En Stolpplats döptes om från from till to: varje sträcka i skedet som börjar eller slutar på
// den gamla får den nya, och inställningen sparas.
void MainWindow::renameInRanges(Stage stage, const std::string& from, const std::string& to)
{
    const std::string oldName = kalkyl::tabletext::trimmed(from);
    const std::string newName = kalkyl::tabletext::trimmed(to);
    auto settings = settingsOf(stage);
    for (auto& range : settings.ranges) {
        if (range.fromPole == oldName)
            range.fromPole = newName;
        if (range.toPole == oldName)
            range.toPole = newName;
    }
    if (settings != settingsOf(stage))
        saveAutoSettings(stage, settings);
}

// Noten i frågan Radera: en rad för varje sträcka (aktiv lina) som börjar eller slutar på en
// Stolpplats bland raderna first till last.
std::string MainWindow::rangeNote(Stage stage, int first, int last) const
{
    const auto& rows = poleGridOf(stage).rows();
    std::string note;
    for (int r = std::max(first, 0); r <= last && static_cast<std::size_t>(r) < rows.size(); ++r) {
        const std::string pole = kalkyl::tabletext::trimmed(rows[static_cast<std::size_t>(r)].pole);
        if (pole.empty())
            continue;
        for (const Wire wire : activeWires()) {
            const auto& range = settingsOf(stage).ranges[static_cast<std::size_t>(wire)];
            const std::string word = kalkyl::projectdb::autoLineWord(wire, !byggPmSplit_);
            for (const bool start : {true, false}) {
                if ((start ? range.fromPole : range.toPole) != pole)
                    continue;
                if (!note.empty())
                    note += kNl;
                note += pole + " är " + (start ? "start" : "slut") + " för " + word + " och behöver väljas om efteråt.";
            }
        }
    }
    return note;
}

// Kopiera från Stolptabell före…: är före tom visas en ruta. Har efter rader frågas först, med
// antalen ändrade och nya rader. Vid Ja eller en tom efter ersätts efter, och inställningen
// och tabellen läses om.
void MainWindow::copyBeforeToAfterAsked()
{
    commitEdits();
    const auto& before = beforeGrid_->rows();
    const auto& after = afterGrid_->rows();
    const auto beforeCount = kalkyl::poletable::rowsToSave(before).size();
    if (beforeCount == 0) {
        glaze::messageBox(*this, "Stolptabell före är tom. Det finns inget att kopiera.", "Kopiera från Stolptabell före",
                          glaze::MessageBoxButtons::OK, glaze::MessageBoxType::Information);
        return;
    }

    if (!kalkyl::poletable::rowsToSave(after).empty()) {
        std::size_t changed = 0;
        std::size_t added = 0;
        for (const auto& c : kalkyl::automode::compareAfter(before, after)) {
            if (c.state == kalkyl::automode::RowState::Changed)
                ++changed;
            else if (c.state == kalkyl::automode::RowState::New)
                ++added;
        }
        const std::string text =
            "Ersätta Stolptabell efter med en kopia av Stolptabell före?" + kNl + kNl + "Stolptabell efter har " +
            std::to_string(after.size()) + " rader, varav " + std::to_string(changed) + " ändrade och " +
            std::to_string(added) + " nya jämfört med före. De ersätts av före-tabellens " +
            std::to_string(before.size()) + " rader, och start och slut och automatiskt läge blir som i före.";
        if (glaze::messageBox(*this, text, "Kopiera från Stolptabell före", glaze::MessageBoxButtons::YesNo,
                              glaze::MessageBoxType::Warning) != glaze::DialogResult::Yes)
            return;
    }

    const auto copied = kalkyl::projectdb::copyBeforeToAfter(*project_);
    if (!copied) {
        gfx::log("Stolptabell efter kunde inte kopieras: %s", copied.error().detail.c_str());
        glaze::messageBox(*this, "Stolptabell efter kunde inte kopieras." + kNl + copied.error().message, "Kalkyl",
                          glaze::MessageBoxButtons::OK, glaze::MessageBoxType::Error);
        return;
    }
    updateTitle();
    afterGrid_->setRows(reloadPoles(Stage::After));
    if (auto settings = kalkyl::projectdb::autoSettings(*project_, Stage::After))
        autoAfter_ = settings.value();
    refreshAutomatic();
    setPoleStatus("Kopierade " + std::to_string(before.size()) + " rader från Stolptabell före.");
}

// Importera stolptabell…: filväljaren öppnar i senaste importmappen, annars projektfilens mapp och
// annars Dokument. Sedan visas PoleImportDialog med filen, och vid OK byts bara raderna i Stolptabell
// före (projectdb::importPolesBefore). Läget, sträckorna, efter och Dokumentation rörs inte, men
// refreshAutomatic visar de omräknade automatiska beräkningarna. Ett fel visas i en ruta.
void MainWindow::importPolesAsked()
{
    commitEdits();
    std::string folder = sys_->lastImportFolder();
    if (folder.empty())
        folder = kalkyl::platform::parentDir(project_->path());
    if (folder.empty())
        folder = kalkyl::platform::documentsDir();

    importFileDlg_.setTitle("Importera stolptabell");
    importFileDlg_.setFilters({{"Excel-filer", "*.xlsx;*.xlsm"}, {glaze::tr(glaze::str::FileAllFiles), "*.*"}});
    importFileDlg_.setFolder(folder);
    if (importFileDlg_.run(*this) != glaze::DialogResult::OK)
        return;
    const std::string path = importFileDlg_.path();

    // Mappen sparas av dialogen först när importen har lyckats.
    PoleImportDialog dlg;
    dlg.setSystemDb(*sys_);
    dlg.setFile(path);
    dlg.setCurrentTable(beforeGrid_->rows(), namedRanges(Stage::Before));
    if (dlg.run(*this) != glaze::DialogResult::OK)
        return;

    const auto imported = kalkyl::projectdb::importPolesBefore(*project_, dlg.importedRows(), dlg.importRecord());
    if (!imported) {
        gfx::log("Stolptabellen kunde inte importeras: %s", imported.error().detail.c_str());
        glaze::messageBox(*this, "Stolptabellen kunde inte importeras." + kNl + imported.error().message, "Kalkyl",
                          glaze::MessageBoxButtons::OK, glaze::MessageBoxType::Error);
        return;
    }
    updateTitle();
    beforeGrid_->setRows(reloadPoles(Stage::Before));
    refreshAutomatic();

    const std::size_t count = dlg.importedRows().size();
    std::string status = "Importerade " + (count == 1 ? std::string("1 stolpe") : std::to_string(count) + " stolpar") +
                         " från " + dlg.importRecord().fileName + " till Stolptabell före.";
    if (dlg.invalidCount() == 1)
        status += " 1 ogiltigt tal lämnades tomt.";
    else if (dlg.invalidCount() > 1)
        status += " " + std::to_string(dlg.invalidCount()) + " ogiltiga tal lämnades tomma.";
    setPoleStatus(status);
}

// Väljer fliken för skedet och gör raden aktuell i tabellen när row inte är negativ.
void MainWindow::showPoleTab(Stage stage, int row)
{
    flikar.setSelectedIndex(flikar.pageIndex(stage == Stage::Before ? stolpSida : efterSida));
    if (row >= 0)
        poleGridOf(stage).showRow(row);
}

// Importera stolptabell…
void MainWindow::stolpImportBtnClicked(glaze::Widget*, glaze::ClickArgs&)
{
    importPolesAsked();
}

// Kopiera från Stolptabell före…
void MainWindow::efterKopieraBtnClicked(glaze::Widget*, glaze::ClickArgs&)
{
    copyBeforeToAfterAsked();
}

// Sträckorna som i före: start och slut hämtas ur före, men raderna och läget i efter rörs inte.
void MainWindow::efterSomForeBtnClicked(glaze::Widget*, glaze::ClickArgs&)
{
    auto settings = autoAfter_;
    settings.ranges = autoBefore_.ranges;
    saveAutoSettings(Stage::After, settings);
    if (autoAfter_ == settings)
        setPoleStatus("Start och slut är nu som i Stolptabell före.");
}

// Filtret i Stolptabell efter byttes i rullistan. Är valet nytt sparas det, och Förändrade ±2
// rullar till de förändrade stolparna.
void MainWindow::efterFilterDropChanged(glaze::Widget*, glaze::ChangeArgs&)
{
    const auto filter = filterOfDropIndex(efterFilterDrop.selectedIndex());
    if (filter == afterFilter_)
        return;
    applyAfterFilter(filter, true, filter == kalkyl::poleview::Filter::Changed);
}

// Visa förändrade: rullar Stolptabell efter till de förändrade stolparna. Finns inga visas en text.
void MainWindow::efterVisaBtnClicked(glaze::Widget*, glaze::ClickArgs&)
{
    commitEdits();
    if (!afterGrid_->showChanged())
        setPoleStatus("Stolptabell efter har inga förändringar mot Stolptabell före.");
}

// Skriver en kort text i statusradens panel för tabellens meddelanden.
void MainWindow::setPoleStatus(const std::string& text)
{
    if (gridStatusPanel_ >= 0)
        statusBar.setPanelText(gridStatusPanel_, text);
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
        const gfx::Color tone = rowTone(*color, static_cast<std::size_t>(inGroup));
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
    showLineCalcRows();
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
            // De automatiska beräkningarna beror på läget, och listorna i flikarna visar radernas
            // korta namn, som följer läget. Allt läses därför om.
            glaze::app().post([this] {
                rebuildByggPm();
                refreshAutomatic();
            });
        }
    }
    if (!conductorRow)
        ok = saveDocValue(def, e.column);

    if (!ok)
        byggPmGrid.setCell(e.row, e.column, byggPmValues_[def][static_cast<std::size_t>(e.column - 1)]);
    updateTitle();
    fillConductorChoices();
    // En ändrad uppgift kan ändra linberäkningarna: raderna och listan räknas om direkt.
    if (ok)
        refreshLineCalcs();
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
    refreshLineCalcs();
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
    if (i == kRowCount || (!kByggPmRows[i].ns && !kByggPmRows[i].lc))
        return key;
    const Ns ns = kByggPmRows[i].ns ? *kByggPmRows[i].ns : *kByggPmRows[i].lc;
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

// De kopplade radernas korta namn för en linberäkning, i radordning och skilda med komma.
// Utan rader blir svaret "–".
std::string MainWindow::shortRowsText(const kalkyl::projectdb::LineCalc& lc) const
{
    std::string text;
    for (const auto& d : kByggPmRows) {
        if (!d.lc)
            continue;
        const std::string key = rowKey(d);
        if (std::find(lc.docKeys.begin(), lc.docKeys.end(), key) == lc.docKeys.end())
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

// Nyckel och radtext för de sex linberäkningsraderna, i radordning, till formuläret.
std::vector<std::pair<std::string, std::string>> MainWindow::lineCalcRowLabels() const
{
    std::vector<std::pair<std::string, std::string>> labels;
    for (const auto& d : kByggPmRows) {
        if (!d.lc)
            continue;
        const std::string key = rowKey(d);
        labels.emplace_back(key, fullDocLabel(key));
    }
    return labels;
}

// Platsen i lineCalcs_ för beräkningen som linberäkningsraden key visar, eller -1.
int MainWindow::linkedLineCalcIndex(const std::string& key) const
{
    for (std::size_t i = 0; i < lineCalcs_.size(); ++i) {
        const auto& keys = lineCalcs_[i].docKeys;
        if (std::find(keys.begin(), keys.end(), key) != keys.end())
            return static_cast<int>(i);
    }
    return -1;
}

// Texten i undermenyn Koppla befintlig: lina, aₙ, σ₀, mall och raderna beräkningen visas på.
// Exempel: "CURLEW · aₙ 314,6 m · σ₀ 90 MPa · Mall 354 · Fas före".
std::string MainWindow::lineCalcSummary(std::size_t index) const
{
    const auto& lc = lineCalcs_[index];
    const auto& input = lcResolved_[index].input;
    std::string text = conductorText(input) + " · aₙ " + spanText(input) + " m · σ₀ " + tensionText(input) +
                       " MPa · Mall " + mallText(lcCalcs_[index]) + " · ";
    text += lc.docKeys.empty() ? std::string("ej kopplad") : shortRowsText(lc);
    return text;
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
            nsCalcs_.push_back(kalkyl::projectdb::calculateSpan(ns));
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
            const auto& ns = normalSpans_[static_cast<std::size_t>(index)];
            const auto& calc = nsCalcs_[static_cast<std::size_t>(index)];
            byggPmGrid.setCellEditor(row, kValueColumn, numberEditor(Unit::Meter, true, true));
            if (ns.automatic) {
                // Den automatiska beräkningen: Sync-bild, och sträckan eller felet i tooltipen.
                const std::string table(kalkyl::projectdb::poleTableName(ns.automatic->stage));
                if (calc.result) {
                    byggPmGrid.setCell(row, kValueColumn, kalkyl::tabletext::formatDecimal(calc.result->an, -1, '.'));
                    byggPmGrid.setCellImage(row, kValueColumn, glaze::stockIcon(glaze::StockIcon::Sync));
                    byggPmGrid.setCellToolTip(row, kValueColumn,
                                              "Automatisk ur " + table + ": " + ns.automatic->range.fromPole + " – " +
                                                  ns.automatic->range.toPole + ", " +
                                                  std::to_string(calc.result->spanCount) + " spann. Den ändras i " +
                                                  table + ".");
                } else {
                    byggPmGrid.setCell(row, kValueColumn, "");
                    byggPmGrid.setCellImage(row, kValueColumn, {});
                    byggPmGrid.setCellToolTip(row, kValueColumn, "Automatisk ur " + table + ": " + autoErrorText(ns, calc));
                }
            } else if (calc.result) {
                byggPmGrid.setCell(row, kValueColumn, kalkyl::tabletext::formatDecimal(calc.result->an, -1, '.'));
                byggPmGrid.setCellImage(row, kValueColumn, glaze::stockIcon(glaze::StockIcon::Sum));
                byggPmGrid.setCellToolTip(row, kValueColumn,
                                          "Beräknad " + rangeText(ns, calc) + ", " +
                                              std::to_string(calc.result->spanCount) +
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
    if (conductorIndex < 0 && !d.ns && !d.lc)
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

    // Linberäkningsraden: Lägg till (kopplad, utan formulär), Lägg till manuell…, Koppla befintlig och Manuellt värde, eller
    // Ändra…, Koppla bort och Ta bort… när en beräkning visas. Skrivskyddat finns bara Visa….
    if (d.lc) {
        const std::string lcKey = rowKey(d);
        const int lcIndex = linkedLineCalcIndex(lcKey);
        if (lcIndex >= 0) {
            const auto& linked = lineCalcs_[static_cast<std::size_t>(lcIndex)];
            const std::int64_t id = linked.id;
            const bool wasDocSource = !linked.docSource.empty();
            if (linked.automatic) {
                // Den automatiska beräkningen går bara att visa, och tabellen den kommer ur att gå till.
                const Stage stage = linked.automatic->stage;
                const int firstRow = linked.automatic->firstRow;
                byggPmMenu_.addItem("Visa…").onClick = [this, id] { openLineCalcDialog(LcOpen::Existing, id, ""); };
                byggPmMenu_.addItem("Gå till " + std::string(kalkyl::projectdb::poleTableName(stage))).onClick =
                    [this, stage, firstRow] { showPoleTab(stage, firstRow); };
            } else if (readOnly()) {
                byggPmMenu_.addItem("Visa…").onClick = [this, id] {
                    openLineCalcDialog(LcOpen::Existing, id, "");
                };
            } else {
                byggPmMenu_.addItem("Ändra…").onClick = [this, id] {
                    openLineCalcDialog(LcOpen::Existing, id, "");
                };
                byggPmMenu_.addItem("Koppla bort").onClick = [this, lcKey, wasDocSource] {
                    auto done = kalkyl::projectdb::unlinkDocRow(*project_, lcKey);
                    if (!done) {
                        glaze::messageBox(*this, done.error().message, "Kalkyl", glaze::MessageBoxButtons::OK,
                                          glaze::MessageBoxType::Error);
                        return;
                    }
                    reloadAfterChange();
                    if (wasDocSource)
                        statusBar.setPanelText(savePanel_,
                                               "Linberäkningen är bortkopplad och har nu egna indata. "
                                               "Den finns kvar i fliken Linberäkning.");
                };
                byggPmMenu_.addSeparator();
                byggPmMenu_.addItem("Ta bort…").onClick = [this, id] { removeLineCalcAsked(id); };
            }
        } else {
            if (readOnly())
                return;
            byggPmMenu_.addItem("Lägg till").onClick = [this, lcKey] { addLinkedLineCalc(lcKey); };
            byggPmMenu_.addItem("Lägg till manuell…").onClick = [this, lcKey] {
                openLineCalcDialog(LcOpen::NewManualForRow, 0, lcKey);
            };

            // Undermenyn har en post per manuell beräkning. En kopplad beräkning hör till sin rad.
            glaze::MenuItem& link = byggPmMenu_.addSubmenu("Koppla befintlig");
            bool anyManual = false;
            for (std::size_t i = 0; i < lineCalcs_.size(); ++i) {
                if (!lineCalcs_[i].docSource.empty())
                    continue;
                anyManual = true;
                glaze::MenuItem entry;
                entry.text = lineCalcSummary(i);
                const std::int64_t id = lineCalcs_[i].id;
                entry.onClick = [this, id, lcKey] {
                    auto done = kalkyl::projectdb::linkLineCalc(*project_, id, lcKey);
                    if (!done)
                        glaze::messageBox(*this, done.error().message, "Kalkyl", glaze::MessageBoxButtons::OK,
                                          glaze::MessageBoxType::Error);
                    reloadAfterChange();
                };
                link.submenu.push_back(std::move(entry));
            }
            link.enabled = anyManual;

            byggPmMenu_.addSeparator();
            byggPmMenu_.addItem("Manuellt värde").onClick = [this, row] {
                byggPmGrid.setCurrentCell(row, kValueColumn);
                byggPmGrid.beginEdit();
            };
        }
        byggPmMenu_.popupAt(byggPmGrid, e.x, e.y);
        return;
    }

    // Normalspannsraden.
    const std::string key = rowKey(d);
    const int index = linkedSpanIndex(key);
    if (index >= 0) {
        const auto& shown = normalSpans_[static_cast<std::size_t>(index)];
        const std::int64_t id = shown.id;
        if (shown.automatic) {
            // Den automatiska beräkningen går bara att visa, och tabellen den kommer ur att gå till.
            const Stage stage = shown.automatic->stage;
            const int firstRow = shown.automatic->firstRow;
            byggPmMenu_.addItem("Visa…").onClick = [this, id] { openNormalSpanDialog(NsOpen::Existing, id, ""); };
            byggPmMenu_.addItem("Gå till " + std::string(kalkyl::projectdb::poleTableName(stage))).onClick =
                [this, stage, firstRow] { showPoleTab(stage, firstRow); };
        } else if (readOnly()) {
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

        // Undermenyn har en post per beräkning, också de som redan visas på andra rader. De
        // automatiska beräkningarna går inte att koppla om och visas inte.
        glaze::MenuItem& link = byggPmMenu_.addSubmenu("Koppla befintlig");
        link.enabled = std::any_of(normalSpans_.begin(), normalSpans_.end(),
                                   [](const auto& ns) { return !ns.automatic; });
        for (std::size_t i = 0; i < normalSpans_.size(); ++i) {
            const auto& ns = normalSpans_[i];
            if (ns.automatic)
                continue;
            const auto& calc = nsCalcs_[i];
            std::string text = rangeText(ns, calc) + " · ";
            text += calc.result ? kalkyl::tabletext::formatDecimal(calc.result->an, 1) + " m" : std::string("fel");
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
    if (ns.automatic)
        return;

    std::string text = "Ta bort normalspannsberäkningen " + rangeText(ns, calc) + " (";
    if (calc.result)
        text += std::to_string(calc.result->spanCount) + " spann, aₙ " +
                kalkyl::tabletext::formatDecimal(calc.result->an, 1) + " m";
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

// Bygger listan i fliken Normalspann: en rad per beräkning med de tio kolumnerna, gul växlande
// radton (lila eller rosa med Sync-bild för en automatisk beräkning) och sorteringsnycklar för
// talkolumnerna. Den tidigare valda beräkningen väljs igen.
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
        if (ns.automatic)
            row.img = glaze::stockIcon(glaze::StockIcon::Sync);
        row.cells = {ns.automatic ? std::string("Automatisk") : std::string("Manuell"),
                     poleText(poles.first),
                     poleText(poles.second),
                     calc.result ? std::to_string(calc.result->spanCount) : std::string("–"),
                     calc.result ? kalkyl::tabletext::formatDecimal(calc.result->totalM, 1) : std::string("–"),
                     calc.result ? kalkyl::tabletext::formatDecimal(calc.result->an, 1) : std::string("–"),
                     calc.result && calc.result->anDeltaH ? kalkyl::tabletext::formatDecimal(*calc.result->anDeltaH, 1)
                                                          : std::string("–"),
                     calc.result ? std::to_string(kalkyl::automode::deviationCount(*calc.result)) : std::string("Fel"),
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
            nsLista.setSortKey(row, 3, calc.result->spanCount);
            nsLista.setSortKey(row, 4, calc.result->totalM);
            nsLista.setSortKey(row, 5, calc.result->an);
            nsLista.setSortKey(row, 6, calc.result->anDeltaH.value_or(0));
            nsLista.setSortKey(row, 7, kalkyl::automode::deviationCount(*calc.result));
        }
        const gfx::Color tone = calcTone(normalSpans_[i].automatic, i);
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
    const bool automatic = has && normalSpans_[static_cast<std::size_t>(selected)].automatic;
    set(nsNyBtn, write);
    set(nsAndraBtn, has);
    set(nsKopieraBtn, has && write);
    set(nsTaBortBtn, has && write && !automatic);
    nsAndraBtn.setText(write && !automatic ? "Ändra…" : "Visa…");

    std::string tip;
    if (has) {
        const auto& ns = normalSpans_[static_cast<std::size_t>(selected)];
        const auto& calc = nsCalcs_[static_cast<std::size_t>(selected)];
        if (ns.automatic)
            tip = ns.note + kNl;
        if (calc.result) {
            const auto& r = *calc.result;
            tip += "Avvikelser, 1: " + std::to_string(r.tooLongCount) + " · 2: " + std::to_string(r.tooShortCount) +
                   " · 3: " + std::to_string(r.adjacentCount) + " · 4: " + std::to_string(r.localLowCount) +
                   " · 5: " + std::to_string(r.localHighCount);
        } else {
            tip += "Beräkningen har fel: " + (ns.automatic ? autoErrorText(ns, calc) : firstError(calc));
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
        if (ns.automatic)
            tip += kNl + "Automatisk: den ändras i " + std::string(kalkyl::projectdb::poleTableName(ns.automatic->stage)) +
                   " och kan inte tas bort.";
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
    if (const auto& origin = normalSpans_[static_cast<std::size_t>(row)].automatic) {
        // Den automatiska beräkningen: Visa…, Kopiera… och en väg till tabellen den kommer ur.
        const Stage stage = origin->stage;
        const int firstRow = origin->firstRow;
        nsMenu_.addItem("Visa…").onClick = [this, id] { openNormalSpanDialog(NsOpen::Existing, id, ""); };
        glaze::MenuItem& copy = nsMenu_.addItem("Kopiera…");
        copy.enabled = !readOnly();
        copy.onClick = [this, id] { openNormalSpanDialog(NsOpen::Copy, id, ""); };
        nsMenu_.addSeparator();
        nsMenu_.addItem("Gå till " + std::string(kalkyl::projectdb::poleTableName(stage))).onClick =
            [this, stage, firstRow] { showPoleTab(stage, firstRow); };
    } else if (readOnly()) {
        nsMenu_.addItem("Visa…").onClick = [this, id] { openNormalSpanDialog(NsOpen::Existing, id, ""); };
    } else {
        nsMenu_.addItem("Ändra…").onClick = [this, id] { openNormalSpanDialog(NsOpen::Existing, id, ""); };
        nsMenu_.addItem("Kopiera…").onClick = [this, id] { openNormalSpanDialog(NsOpen::Copy, id, ""); };
        nsMenu_.addSeparator();
        nsMenu_.addItem("Ta bort…").onClick = [this, id] { removeNormalSpanAsked(id); };
    }
    nsMenu_.popupAt(nsLista, e.x, e.y);
}

// Läser om alla linberäkningar och Dokumentationens uppgifter, löser upp varje beräkning mot
// dem och räknar den. Kopplade beräkningar får alltså alltid Dokumentationens aktuella värden.
// Raderna i Dokumentation och listan i fliken Linberäkning uppdateras efteråt.
void MainWindow::refreshLineCalcs()
{
    lineCalcs_.clear();
    lcResolved_.clear();
    lcCalcs_.clear();

    auto calcs = kalkyl::projectdb::lineCalcs(*project_);
    if (!calcs) {
        gfx::log("Linberäkningarna kunde inte läsas: %s", calcs.error().detail.c_str());
        glaze::messageBox(*this, "Linberäkningarna kunde inte läsas." + kNl + calcs.error().message, "Kalkyl",
                          glaze::MessageBoxButtons::OK, glaze::MessageBoxType::Error);
    } else {
        kalkyl::projectdb::DocLineSources sources;
        auto docs = kalkyl::projectdb::docLineSources(*project_);
        if (docs) {
            sources = std::move(docs.value());
        } else {
            gfx::log("Dokumentationens uppgifter kunde inte läsas: %s", docs.error().detail.c_str());
            glaze::messageBox(*this, "Dokumentationens uppgifter kunde inte läsas." + kNl + docs.error().message,
                              "Kalkyl", glaze::MessageBoxButtons::OK, glaze::MessageBoxType::Error);
        }
        lineCalcs_ = std::move(calcs.value());
        for (const auto& lc : lineCalcs_) {
            lcResolved_.push_back(kalkyl::projectdb::resolveLineInput(lc.input, lc.docSource, sources));
            lcCalcs_.push_back(kalkyl::projectdb::calculateResolved(lcResolved_.back()));
        }
    }
    showLineCalcRows();
    fillLineCalcList();
}

// Värde-cellen på linberäkningsraderna. Kopplad beräkning med resultat: stakningsmallen i en låst
// cell med Σ-bild (varningsbild när utnyttjandegraden är för hög). Utan resultat: tom cell och
// orsaken som tooltip. Ingen koppling: det manuella värdet, eller "-".
void MainWindow::showLineCalcRows()
{
    for (std::size_t r = 0; r < byggPmShown_.size(); ++r) {
        const std::size_t def = byggPmShown_[r];
        const ByggPmRowDef& d = kByggPmRows[def];
        if (!d.lc)
            continue;
        const int row = static_cast<int>(r);
        const int index = linkedLineCalcIndex(rowKey(d));

        if (index < 0) {
            byggPmGrid.setCellEditor(row, kValueColumn, numberEditor(Unit::Mall, true, readOnly()));
            byggPmGrid.setCell(row, kValueColumn, byggPmValues_[def][0]);
            byggPmGrid.setCellImage(row, kValueColumn, {});
            byggPmGrid.setCellToolTip(row, kValueColumn, readOnly() ? "" : kLcRowHint);
            continue;
        }

        const std::size_t i = static_cast<std::size_t>(index);
        const auto& lc = lineCalcs_[i];
        const auto& input = lcResolved_[i].input;
        const auto& calc = lcCalcs_[i];
        byggPmGrid.setCellEditor(row, kValueColumn, numberEditor(Unit::Mall, true, true));
        if (calc.result) {
            const std::string over = overCases(*calc.result);
            std::string tip;
            if (lc.automatic) {
                const std::string table(kalkyl::projectdb::poleTableName(lc.automatic->stage));
                tip = "Stakningsmall (lastfall 15, maxtemperatur) ur den automatiska linberäkningen: " +
                      conductorText(input) + ", aₙ " + spanText(input) + " m ur " + table + ", σ₀ " +
                      tensionText(input) + " MPa. Den ändras i " + table + " och Dokumentation.";
            } else {
                tip = std::string("Stakningsmall (lastfall 15, maxtemperatur) ur en ") +
                      (lc.docSource.empty() ? "manuell linberäkning: " : "linberäkning med indata ur Dokumentation: ") +
                      conductorText(input) + ", aₙ " + spanText(input) + " m, σ₀ " + tensionText(input) +
                      " MPa. Högerklicka för att ändra.";
            }
            if (!over.empty())
                tip += " Utnyttjandegraden är över " + shortNumber(input.constants.maxUtilizationPercent) +
                       " % i lastfall " + over + ".";
            byggPmGrid.setCell(row, kValueColumn, mallText(calc));
            const auto okIcon = lc.automatic ? glaze::StockIcon::Sync : glaze::StockIcon::Sum;
            byggPmGrid.setCellImage(row, kValueColumn,
                                    glaze::stockIcon(over.empty() ? okIcon : glaze::StockIcon::Warning));
            byggPmGrid.setCellToolTip(row, kValueColumn, tip);
        } else {
            const std::string error = firstIssueOf(calc, kalkyl::linecalc::LineIssueKind::Error);
            byggPmGrid.setCell(row, kValueColumn, "");
            byggPmGrid.setCellImage(row, kValueColumn, {});
            byggPmGrid.setCellToolTip(
                row, kValueColumn,
                !error.empty() ? "Linberäkningen har fel: " + error
                               : "Linberäkningen saknar indata: " +
                                     firstIssueOf(calc, kalkyl::linecalc::LineIssueKind::Missing));
        }
    }
}

// Bygger listan i fliken Linberäkning: en rad per beräkning med de elva kolumnerna, gul växlande
// radton och sorteringsnycklar för talkolumnerna. Kolumnerna visar de upplösta värdena, alltså
// dem som räkningen använde. Den tidigare valda beräkningen väljs igen.
void MainWindow::fillLineCalcList()
{
    std::optional<std::int64_t> previous;
    const int selected = lcLista.selectedIndex();
    if (selected >= 0 && static_cast<std::size_t>(selected) < lcListIds_.size())
        previous = lcListIds_[static_cast<std::size_t>(selected)];

    std::vector<glaze::ListView::Row> rows;
    lcListIds_.clear();
    for (std::size_t i = 0; i < lineCalcs_.size(); ++i) {
        const auto& lc = lineCalcs_[i];
        const auto& input = lcResolved_[i].input;
        const auto& calc = lcCalcs_[i];
        glaze::ListView::Row row;
        if (lc.automatic)
            row.img = glaze::stockIcon(glaze::StockIcon::Sync);
        row.cells = {conductorText(input),
                     lc.automatic ? std::string("Automatisk")
                                  : lc.docSource.empty() ? std::string("Manuell") : std::string("Dokumentation"),
                     input.mode == kalkyl::linecalc::CalcMode::New ? std::string("Ny") : std::string("Excel"),
                     spanText(input),
                     tensionText(input),
                     shortNumber(input.refTempC),
                     shortNumber(input.minTempC) + " / " + shortNumber(input.maxTempC),
                     hasLineError(calc) ? std::string("Fel") : mallText(calc),
                     utilizationText(calc),
                     shortRowsText(lc),
                     lc.note};
        rows.push_back(std::move(row));
        lcListIds_.push_back(lc.id);
    }
    lcLista.setRows(std::move(rows));

    const gfx::Color red{200, 0, 0, 255};
    for (std::size_t i = 0; i < lineCalcs_.size(); ++i) {
        const int row = static_cast<int>(i);
        const auto& input = lcResolved_[i].input;
        const auto& calc = lcCalcs_[i];
        if (input.normalSpanM)
            lcLista.setSortKey(row, 3, *input.normalSpanM);
        if (input.tensionMpa)
            lcLista.setSortKey(row, 4, *input.tensionMpa);
        lcLista.setSortKey(row, 5, input.refTempC);
        lcLista.setSortKey(row, 6, input.minTempC);
        bool over = false;
        if (calc.result) {
            lcLista.setSortKey(row, kLcMallColumn, std::round(calc.result->stakingMall));
            double best = 0;
            for (const auto& c : calc.result->cases) {
                if (c.utilizationPercent)
                    best = std::max(best, *c.utilizationPercent);
                over = over || c.overUtilization;
            }
            lcLista.setSortKey(row, kLcUtilColumn, best);
        }

        const gfx::Color tone = calcTone(lineCalcs_[i].automatic, i);
        for (int col = 0; col < lcLista.columnCount(); ++col) {
            std::optional<gfx::Color> text;
            if ((col == kLcUtilColumn && over) || (col == kLcMallColumn && hasLineError(calc)))
                text = red;
            lcLista.setCellColor(row, col, tone, text);
        }
    }

    if (previous) {
        const auto it = std::find(lcListIds_.begin(), lcListIds_.end(), *previous);
        if (it != lcListIds_.end())
            lcLista.setSelectedIndex(static_cast<int>(it - lcListIds_.begin()));
    }
    updateLcButtons();
}

// Aktiverar knapparna efter valet och skrivskyddat läge. Ändra heter Visa… skrivskyddat. Listan
// får en tooltip om den valda beräkningen: första upplysningen eller mallen, och raderna den visas på.
void MainWindow::updateLcButtons()
{
    const int selected = lcLista.selectedIndex();
    const bool has = selected >= 0 && static_cast<std::size_t>(selected) < lineCalcs_.size();
    const bool write = !readOnly();

    const auto set = [](glaze::Button& b, bool on) {
        if (on)
            b.enable();
        else
            b.disable();
    };
    const bool automatic = has && lineCalcs_[static_cast<std::size_t>(selected)].automatic;
    set(lcNyBtn, write);
    set(lcAndraBtn, has);
    set(lcKopieraBtn, has && write);
    set(lcTaBortBtn, has && write && !automatic);
    lcAndraBtn.setText(write && !automatic ? "Ändra…" : "Visa…");

    std::string tip;
    if (has) {
        const std::size_t i = static_cast<std::size_t>(selected);
        const auto& calc = lcCalcs_[i];
        const std::string error = firstIssueOf(calc, kalkyl::linecalc::LineIssueKind::Error);
        const std::string missing = firstIssueOf(calc, kalkyl::linecalc::LineIssueKind::Missing);
        const std::string warning = firstIssueOf(calc, kalkyl::linecalc::LineIssueKind::Warning);
        if (automatic)
            tip = lineCalcs_[i].note + kNl;
        if (!error.empty()) {
            tip += "Linberäkningen har fel: " + error;
        } else if (!missing.empty()) {
            tip += "Linberäkningen saknar indata: " + missing;
        } else if (calc.result) {
            tip += "Stakningsmall " + mallText(calc) + " · Högsta utnyttjandegrad " + utilizationText(calc);
            if (!warning.empty())
                tip += kNl + warning;
        }
        tip += kNl;
        std::string rows;
        for (const auto& d : kByggPmRows) {
            if (!d.lc)
                continue;
            const std::string key = rowKey(d);
            const auto& keys = lineCalcs_[i].docKeys;
            if (std::find(keys.begin(), keys.end(), key) == keys.end())
                continue;
            rows += kNl + fullDocLabel(key);
        }
        tip += rows.empty() ? std::string("Visas inte i Dokumentation.") : "Visas i Dokumentation på" + rows;
        if (automatic)
            tip += kNl + "Automatisk: den ändras i " +
                   std::string(kalkyl::projectdb::poleTableName(lineCalcs_[i].automatic->stage)) +
                   " och kan inte tas bort.";
    }
    lcLista.setToolTip(tip);
}

// Öppnar formuläret modalt. rowKey kopplar en ny beräkning till en rad i Dokumentation när
// användaren trycker OK. Läser om allt efteråt och väljer en ny sparad beräkning i listan.
void MainWindow::openLineCalcDialog(LcOpen mode, std::int64_t id, const std::string& rowKey)
{
    commitEdits();
    LineCalcDialog dlg;
    dlg.setProject(*project_);
    dlg.setSystemDb(*sys_);
    switch (mode) {
    case LcOpen::New:             dlg.setNew("", false); break;
    case LcOpen::NewManualForRow: dlg.setNew(rowKey, false); break;
    case LcOpen::Existing:        dlg.setExisting(id); break;
    case LcOpen::Copy:            dlg.setCopyOf(id); break;
    }
    dlg.setDocRowLabels(lineCalcRowLabels());
    dlg.setReadOnly(readOnly());
    dlg.run(*this);
    const auto saved = dlg.savedId();

    reloadAfterChange();
    if (saved) {
        const auto it = std::find(lcListIds_.begin(), lcListIds_.end(), *saved);
        if (it != lcListIds_.end()) {
            const int row = static_cast<int>(it - lcListIds_.begin());
            lcLista.setSelectedIndex(row);
            lcLista.ensureVisible(row);
            updateLcButtons();
        }
    }
}

// Skapar en kopplad linberäkning för raden rowKey: förvalen för en ny beräkning, och linan, aₙ,
// σ₀ och temperaturerna ur Dokumentation. Raden visar mallen direkt, eller "-" med orsaken när
// något saknas i Dokumentation. Beräkningen väljs i fliken Linberäkning.
void MainWindow::addLinkedLineCalc(const std::string& rowKey)
{
    commitEdits();
    kalkyl::projectdb::LineCalc lc;
    lc.input = kalkyl::linecalc::newLineInput();
    lc.docSource = rowKey;
    const auto saved = kalkyl::projectdb::saveLineCalc(*project_, lc, rowKey);
    if (!saved) {
        gfx::log("Linberäkningen kunde inte sparas: %s", saved.error().detail.c_str());
        glaze::messageBox(*this, saved.error().message, "Kalkyl", glaze::MessageBoxButtons::OK,
                          glaze::MessageBoxType::Error);
        return;
    }
    reloadAfterChange();
    const auto it = std::find(lcListIds_.begin(), lcListIds_.end(), saved.value());
    if (it != lcListIds_.end()) {
        lcLista.setSelectedIndex(static_cast<int>(it - lcListIds_.begin()));
        updateLcButtons();
    }
}

// Frågar Ja/Nej och tar bort beräkningen. Frågan säger vilka rader i Dokumentation som visar den
// och att de visar - efteråt.
void MainWindow::removeLineCalcAsked(std::int64_t id)
{
    std::size_t index = lineCalcs_.size();
    for (std::size_t i = 0; i < lineCalcs_.size(); ++i)
        if (lineCalcs_[i].id == id)
            index = i;
    if (index == lineCalcs_.size())
        return;
    const auto& lc = lineCalcs_[index];
    const auto& calc = lcCalcs_[index];
    if (lc.automatic)
        return;

    std::string text = "Ta bort linberäkningen " + conductorText(lcResolved_[index].input) + " (" +
                       (lc.docSource.empty() ? "Manuell" : "Indata ur Dokumentation") + ", ";
    if (calc.result)
        text += "Mall " + mallText(calc);
    else
        text += hasLineError(calc) ? "beräkningen har fel" : "beräkningen saknar indata";
    text += ") ur projektet?" + kNl + kNl;

    std::vector<std::string> rows;
    for (const auto& d : kByggPmRows) {
        if (!d.lc)
            continue;
        const std::string key = rowKey(d);
        if (std::find(lc.docKeys.begin(), lc.docKeys.end(), key) != lc.docKeys.end())
            rows.push_back(fullDocLabel(key));
    }
    if (rows.empty()) {
        text += "Den visas inte i Dokumentation.";
    } else if (rows.size() == 1) {
        text += "Den visas på raden " + rows[0] + ", som visar - efteråt.";
    } else {
        text += "Den visas på " + std::to_string(rows.size()) + " rader i Dokumentation, som visar - efteråt:";
        for (const auto& r : rows)
            text += kNl + r;
    }

    if (glaze::messageBox(*this, text, "Ta bort beräkning", glaze::MessageBoxButtons::YesNo,
                          glaze::MessageBoxType::Warning) != glaze::DialogResult::Yes)
        return;
    auto done = kalkyl::projectdb::removeLineCalc(*project_, id);
    if (!done)
        glaze::messageBox(*this, done.error().message, "Kalkyl", glaze::MessageBoxButtons::OK,
                          glaze::MessageBoxType::Error);
    reloadAfterChange();
}

// Ny beräkning…: en lös, manuell beräkning. Den kopplas till en rad i Dokumentation med högerklick där.
void MainWindow::lcNyBtnClicked(glaze::Widget*, glaze::ClickArgs&)
{
    openLineCalcDialog(LcOpen::New, 0, "");
}

// Ändra… (Visa…): den valda beräkningen.
void MainWindow::lcAndraBtnClicked(glaze::Widget*, glaze::ClickArgs&)
{
    const int selected = lcLista.selectedIndex();
    if (selected >= 0 && static_cast<std::size_t>(selected) < lcListIds_.size())
        openLineCalcDialog(LcOpen::Existing, lcListIds_[static_cast<std::size_t>(selected)], "");
}

// Kopiera…: en ny, lös och manuell beräkning med samma värden som den valda.
void MainWindow::lcKopieraBtnClicked(glaze::Widget*, glaze::ClickArgs&)
{
    const int selected = lcLista.selectedIndex();
    if (selected >= 0 && static_cast<std::size_t>(selected) < lcListIds_.size())
        openLineCalcDialog(LcOpen::Copy, lcListIds_[static_cast<std::size_t>(selected)], "");
}

// Ta bort: frågan och borttagningen görs av removeLineCalcAsked.
void MainWindow::lcTaBortBtnClicked(glaze::Widget*, glaze::ClickArgs&)
{
    const int selected = lcLista.selectedIndex();
    if (selected >= 0 && static_cast<std::size_t>(selected) < lcListIds_.size())
        removeLineCalcAsked(lcListIds_[static_cast<std::size_t>(selected)]);
}

// Dubbelklick och Enter i listan öppnar den valda beräkningen.
void MainWindow::lcListaActivated(glaze::Widget*, glaze::ActivateArgs&)
{
    const int current = lcLista.currentRow();
    if (current >= 0 && static_cast<std::size_t>(current) < lcListIds_.size())
        openLineCalcDialog(LcOpen::Existing, lcListIds_[static_cast<std::size_t>(current)], "");
}

// Valet i listan ändrades: knapparna och tooltipen följer det.
void MainWindow::lcListaChanged(glaze::Widget*, glaze::ChangeArgs&)
{
    updateLcButtons();
}

// Högerklick i listan. Högertrycket har redan valt raden, så den ges av contextRow().
// Skrivskyddat finns bara Visa…. Det finns ingen kopplingspost här.
void MainWindow::lcListaContextMenu(glaze::Widget*, glaze::ContextMenuArgs& e)
{
    const int row = lcLista.contextRow();
    if (row < 0 || static_cast<std::size_t>(row) >= lcListIds_.size())
        return;
    const std::int64_t id = lcListIds_[static_cast<std::size_t>(row)];
    lcMenu_.items.clear();
    if (const auto& origin = lineCalcs_[static_cast<std::size_t>(row)].automatic) {
        // Den automatiska beräkningen: Visa…, Kopiera… och en väg till tabellen den kommer ur.
        const Stage stage = origin->stage;
        const int firstRow = origin->firstRow;
        lcMenu_.addItem("Visa…").onClick = [this, id] { openLineCalcDialog(LcOpen::Existing, id, ""); };
        glaze::MenuItem& copy = lcMenu_.addItem("Kopiera…");
        copy.enabled = !readOnly();
        copy.onClick = [this, id] { openLineCalcDialog(LcOpen::Copy, id, ""); };
        lcMenu_.addSeparator();
        lcMenu_.addItem("Gå till " + std::string(kalkyl::projectdb::poleTableName(stage))).onClick =
            [this, stage, firstRow] { showPoleTab(stage, firstRow); };
    } else if (readOnly()) {
        lcMenu_.addItem("Visa…").onClick = [this, id] { openLineCalcDialog(LcOpen::Existing, id, ""); };
    } else {
        lcMenu_.addItem("Ändra…").onClick = [this, id] { openLineCalcDialog(LcOpen::Existing, id, ""); };
        lcMenu_.addItem("Kopiera…").onClick = [this, id] { openLineCalcDialog(LcOpen::Copy, id, ""); };
        lcMenu_.addSeparator();
        lcMenu_.addItem("Ta bort…").onClick = [this, id] { removeLineCalcAsked(id); };
    }
    lcMenu_.popupAt(lcLista, e.x, e.y);
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

// Hjälp > Om Kalkyl: öppnar AboutDialog med ikonen, versionen, underlaget och copyright.
void MainWindow::aboutClicked()
{
    AboutDialog dlg;
    dlg.run(*this);
}
