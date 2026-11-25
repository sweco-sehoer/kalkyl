#pragma once

#include <glaze/glaze.h>

#include <array>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "conductor/conductor.h"
#include "lineCalc/line_calc.h"
#include "projectdb/project_data.h"
#include "projectdb/project_db.h"
#include "sheetGrid/sheet_grid.h"
#include "systemdb/system_db.h"

// Linberäkningen: formuläret som arket Linberäkning, med blocken Indata, Lindata / Konstanter
// och Lastfall med Resultat och Data till SAG (3D). Modal dialog som sparar själv i projektet
// när användaren trycker OK. Ändringarna buffras, och Avbryt, krysset och Escape frågar om
// osparade ändringar. Regionen GlazeBuilder ägs av editorn. Allt efter #pragma endregion är eget.
// Klassen ligger utanför namespace: GlazeBuilder läser bara okvalificerade fönsterklasser.
class LineCalcDialog : public glaze::AppWindow {
#pragma region GlazeBuilder
    // GENERERAD av GlazeBuilder — redigera i GlazeBuilder, inte för hand.
public:
    LineCalcDialog();

private:
    glaze::Button cancelBtn;
    glaze::Button okBtn;
    glaze::Label noteLbl;
    glaze::TextInput noteTxt;
    glaze::Label docLbl;
    glaze::Panel sheetPanel;
    glaze::Label conductorLbl;
    glaze::Dropdown conductorDd;
    glaze::Label conductorInfo;
    glaze::DataGrid inputGrid;
    glaze::DataGrid constGrid;
    glaze::GroupBox crossingGrp;
    glaze::Button upperBtn;
    glaze::Button lowerBtn;
    glaze::Button resetBtn;
    glaze::GroupBox modeGrp;
    glaze::Radio modeNewRb;
    glaze::Button modeInfoBtn;
    glaze::Radio modeExcelRb;
    glaze::Checkbox diffChk;
    glaze::Checkbox docInputChk;
    glaze::Label infoLbl;
    glaze::DataGrid caseGrid;
    void initializeComponent();
#pragma endregion

public:
    // Projektet som formuläret läser och sparar i. Anropas före run(owner); ägs av main.
    void setProject(kalkyl::projectdb::ProjectDb& project);
    // Linregistret för rullistan Lina. Anropas före run(owner); ägs av main.
    void setSystemDb(kalkyl::systemdb::SystemDb& sys);
    // En ny beräkning. linkDocKey = raden i Dokumentation som visar den efter OK, tom = lös.
    // fromDoc = indata ur Dokumentation (kopplad, kräver linkDocKey). En manuell beräkning med
    // linkDocKey börjar med Dokumentationens värden.
    void setNew(std::string linkDocKey, bool fromDoc);
    // En befintlig beräkning, som läses ur projektet i onLoad.
    void setExisting(std::int64_t id);
    // En ny, lös och manuell beräkning med en kopia av beräkningen id (Kopiera...).
    void setCopyOf(std::int64_t id);
    // Radtexterna i Dokumentation per nyckel, för upplysningen om var beräkningen visas.
    void setDocRowLabels(std::vector<std::pair<std::string, std::string>> labels);
    // Skrivskyddat: allt visas, inget kan ändras, OK döljs och Avbryt heter Stäng.
    void setReadOnly(bool readOnly);
    // Efter run(): id för den sparade beräkningen, tomt om inget sparades.
    std::optional<std::int64_t> savedId() const;

private:
    // Tabellernas nummer i värdarna och i cellinformationen.
    static constexpr int kInputGrid = 0;
    static constexpr int kConstGrid = 1;
    static constexpr int kCaseGrid = 2;

    // Adaptern mellan en tabell och SheetGrid: svarar på vad en cell kopieras som, vilken
    // färg den har och tar emot inklistrade ändringar. Hör till en tabell via numret.
    class GridHost : public kalkyl::sheetgrid::SheetHost {
    public:
        GridHost(LineCalcDialog& dialog, int grid) : dialog_(dialog), grid_(grid) {}
        std::string copyText(int row, int column) const override;
        std::optional<gfx::Color> baseColor(int row, int column) const override;
        void applyEdits(const std::vector<kalkyl::sheetgrid::CellEdit>& edits) override;
        void addMenuItems(glaze::Menu& menu, const kalkyl::sheetgrid::CellRect& selection) override;
        void showStatus(const std::string& text) override;

    private:
        LineCalcDialog& dialog_;
        int grid_;
    };

    // Märket en cell har och tooltipen som hör till det. Märkena står i prioritetsordning:
    // det lägsta numret gäller när flera märken passar.
    struct CellInfo {
        int mark = 0;
        std::string tip;       // tooltipen som hör till märket
        std::string baseTip;   // tooltipen utan märke: fullt värde och källa
    };

    // En post i rullistan Lina: namnet och linans data från registret, projektet och
    // beräkningen själv. Posten finns i minst en av dem.
    struct ConductorChoice {
        std::string name;
        std::optional<kalkyl::conductor::Conductor> registry;
        std::optional<kalkyl::conductor::Conductor> project;
        std::optional<kalkyl::conductor::Conductor> own;
    };

    // Startlogik: läser beräkningen, bygger tabellerna och räknar första gången.
    void onLoad(glaze::Widget* sender, glaze::EventArgs& e);

    // Krysset, Avbryt och Escape: frågar om osparade ändringar.
    // Ja sparar, Nej kastar ändringarna och Avbryt lämnar fönstret öppet.
    void onClosing(glaze::Widget* sender, glaze::CloseArgs& e);

    // OK: validerar, sparar beräkningen i projektet och stänger.
    void okBtnClicked(glaze::Widget* sender, glaze::ClickArgs& e);

    // Övre linan: frågar efter kortslutningstemperaturen och läser in korsningslastfallen.
    void upperBtnClicked(glaze::Widget* sender, glaze::ClickArgs& e);

    // Nedre linan: läser in korsningslastfallen för nedre linan.
    void lowerBtnClicked(glaze::Widget* sender, glaze::ClickArgs& e);

    // Återställ: sätter standardmallen enligt SS-EN 50341-2-18.
    void resetBtnClicked(glaze::Widget* sender, glaze::ClickArgs& e);

    // Anteckningen ändrades: markerar formuläret som ändrat.
    void noteChanged(glaze::Widget* sender, glaze::ChangeArgs& e);

    // En lina valdes i rullistan: fyller lindatat.
    void conductorChanged(glaze::Widget* sender, glaze::ChangeArgs& e);

    // Läget byttes med radioknapparna: räknar om i det nya läget.
    void modeChanged(glaze::Widget* sender, glaze::ChangeArgs& e);

    // i-knappen vid läget Ny: visar PM:et om felen i Excel-arket och hur läget Ny rättar dem.
    void modeInfoBtnClicked(glaze::Widget* sender, glaze::ClickArgs& e);

    // Skillnadsrutan: visar eller döljer skillnaden mot det andra läget.
    void diffChanged(glaze::Widget* sender, glaze::ChangeArgs& e);

    // Rutan Hämta ur Dokumentation: byter mellan kopplad och manuell beräkning.
    void docInputChanged(glaze::Widget* sender, glaze::ChangeArgs& e);

    // En cell sparades i Indata, Lindata / Konstanter respektive Lastfall.
    void inputGridCommitted(glaze::Widget* sender, glaze::CellChangeArgs& e);
    void constGridCommitted(glaze::Widget* sender, glaze::CellChangeArgs& e);
    void caseGridCommitted(glaze::Widget* sender, glaze::CellChangeArgs& e);

    // Tangenter, mus och snabbmeny i tabellerna går direkt till deras SheetGrid.
    void inputGridKeyDown(glaze::Widget* sender, glaze::KeyArgs& e);
    void inputGridMouseUp(glaze::Widget* sender, glaze::MouseArgs& e);
    void inputGridContextMenu(glaze::Widget* sender, glaze::ContextMenuArgs& e);
    void constGridKeyDown(glaze::Widget* sender, glaze::KeyArgs& e);
    void constGridMouseUp(glaze::Widget* sender, glaze::MouseArgs& e);
    void constGridContextMenu(glaze::Widget* sender, glaze::ContextMenuArgs& e);
    void caseGridKeyDown(glaze::Widget* sender, glaze::KeyArgs& e);
    void caseGridMouseUp(glaze::Widget* sender, glaze::MouseArgs& e);
    void caseGridContextMenu(glaze::Widget* sender, glaze::ContextMenuArgs& e);

    // Läser beräkningen som ska öppnas (ny, befintlig eller kopia) in i lc_.
    // Svarar false när den inte finns, efter att felet visats.
    bool loadCalculation();

    // Bygger posterna i rullistan Lina ur registret, projektet och beräkningen.
    void buildConductorChoices();

    // Väljer posten i rullistan som hör till lindatats namn.
    void selectConductorItem();

    // Fyller lindatats celler ur en lina (eller tömmer dem) och sätter namn, typ och krypning.
    void loadConductorCells(const std::optional<kalkyl::conductor::Conductor>& c);

    // Läser lindatats celler till en lina. Saknas något av de nio talen svarar funktionen
    // tomt och missing får antalet. Utan namn och tal alls finns inget lindata.
    std::optional<kalkyl::conductor::Conductor> readConductor(int& missing) const;

    // Linan i registret eller projektet med lindatats namn, till jämförelsen "ändrat lindata".
    const kalkyl::conductor::Conductor* referenceConductor(std::string& origin) const;

    // Skriver texten om var beräkningen visas och om vad som hämtas ur Dokumentation.
    void updateDocLabel();

    // Radtexten i Dokumentation för nyckeln, eller nyckeln själv när texten saknas.
    std::string docRowLabel(const std::string& key) const;

    // Bygger tabellernas rader, cellernas redigerare och etiketterna, och skapar SheetGrid.
    void buildTables();

    // Räknar om allt: upplösning, beräkning, celler, märken, lås, färger och statusraden.
    // clearErrors tar bort ogiltiga celltexter (när modellen byts ut i bulk).
    void recalc(bool clearErrors = false);

    // Skriver en celltext om den ändrats och cellen inte visar ett ogiltigt tal.
    void putCell(int grid, int row, int column, const std::string& text, bool clearErrors);

    // Märker cellen. Det lägsta märket gäller, och tooltipen följer det märke som gäller.
    void markCell(int grid, int row, int column, int mark, const std::string& tip);

    // Cellernas text, redigerbarhet, märken och tooltips för de tre tabellerna.
    void fillInputCells(bool clearErrors);
    void fillConstCells(bool clearErrors);
    void fillCaseCells(bool clearErrors);

    // Märken ur upplysningarna, skillnaden mot det andra läget, hämtade och ändrade värden.
    void markIssues();
    void markDifferences();
    void markDocFields();
    void markConductorDiffs();

    // Låser eller låser upp en cell i en tabell: redigeraren får readOnly.
    void applyLock(int grid, int row, int column, bool locked);

    // Målar om alla tre tabellerna ur märkena.
    void paintAllSheets();

    // Skriver statusraden och dess tooltip.
    void updateStatus();

    // Skriver en egen text i statusraden.
    void setStatus(const std::string& text);

    // Skriver en cells ändring i modellen. Svarar sant när modellen ändrades.
    // changes samlar min, max, Tks och vindtrycken som kräver följdändringar.
    bool writeCell(int grid, int row, int column, const std::string& text, std::array<bool, 5>& changes);

    // Tar emot ändrade celler (inklistring, Delete, Ctrl+X): skriver dem, följdändrar och räknar om.
    void applyEditsTo(int grid, const std::vector<kalkyl::sheetgrid::CellEdit>& edits);

    // En cell sparades av användaren i en tabell: läser den och skriver modellen.
    void cellCommitted(int grid, glaze::DataGrid& table, int row, int column);

    // Kör följdändringarna i arket (applyInputChange) för det som ändrats.
    void applyChanges(const std::array<bool, 5>& changes);

    // Tar bort den manuella lasten i lastfallsraderna row0 till row1.
    void clearManualLoad(int row0, int row1);

    // Skriver in det valda lindatats namn i modellen och fyller cellerna (rullistan).
    void chooseConductor(int itemIndex);

    // Numret på lastfallet för en rad i lastfallstabellen, 0 för tomraden.
    static int caseIdOfRow(int row);

    // Raden i lastfallstabellen för ett lastfall 1-15.
    static int rowOfCase(int id);

    // Cellens namn i meddelanden: etiketten eller kolumnrubriken.
    std::string cellName(int grid, int row, int column) const;

    // Visar cellens värde som tal till kopiering, tomt när cellen inte har något tal.
    std::string copyTextOf(int grid, int row, int column) const;

    // Cellens grundfärg ur märket.
    std::optional<gfx::Color> colorOfCell(int grid, int row, int column) const;

    // Sant när cellen är en indatacell som går att ändra just nu.
    bool isInputCell(int grid, int row, int column) const;

    // Fältet som en inklistrad text passar som, tomt svar när det inte passar.
    std::optional<std::string> acceptField(int grid, int row, int column, const std::string& field) const;

    // Gör alla redigerare skrivskyddade och döljer OK. Avbryt heter Stäng.
    void applyReadOnly();

    // Validerar och sparar. Svarar false när fönstret ska stå kvar, efter att felet visats.
    bool saveChanges();

    // Tabellen med numret.
    glaze::DataGrid& tableOf(int grid);
    const glaze::DataGrid& tableOf(int grid) const;

    kalkyl::projectdb::ProjectDb* project_ = nullptr;
    kalkyl::systemdb::SystemDb* sys_ = nullptr;
    std::string linkDocKey_;                  // raden som ska visa beräkningen efter OK
    bool newFromDoc_ = false;                 // en ny beräkning med indata ur Dokumentation
    std::int64_t existingId_ = 0;             // beräkningen som ändras, 0 = ingen
    std::int64_t copyOfId_ = 0;               // beräkningen som kopieras, 0 = ingen
    std::vector<std::pair<std::string, std::string>> docLabels_;   // radtext per nyckel
    bool readOnly_ = false;
    bool dirty_ = false;                      // något är ändrat sedan inläsningen
    bool saved_ = false;                      // sparat: onClosing frågar inte
    std::optional<std::int64_t> savedId_;

    kalkyl::projectdb::LineCalc lc_;          // beräkningen som redigeras
    std::vector<std::string> existingDocKeys_;   // raderna som visar en befintlig beräkning
    std::string docRowKey_;                   // raden rutan Hämta ur Dokumentation gäller
    kalkyl::projectdb::DocLineSources docs_;  // Dokumentationens värden
    kalkyl::projectdb::ResolvedLineInput res_;   // senaste upplösningen
    kalkyl::linecalc::LineCalculation calc_;  // senaste beräkningen
    kalkyl::linecalc::LineCalculation other_;   // det andra läget, bara med skillnadsrutan
    kalkyl::linecalc::LineInput otherInput_;  // indata för det andra läget
    std::array<std::optional<double>, 10> lindata_;   // lindatats tal, i ordningen i Lindata-tabellen
    std::string conductorName_;               // lindatats namn
    std::string conductorType_;               // lindatats typ
    double creepStrain_ = 0;                  // lindatats krypning, visas inte
    std::vector<ConductorChoice> choices_;    // posterna i rullistan, från och med index 1
    std::array<std::vector<CellInfo>, 3> info_;   // märken per cell i de tre tabellerna
    std::array<std::vector<char>, 3> locked_;     // cellens läge: 1 = låst, per tabell
    int diffCount_ = 0;                       // antal celler som skiljer sig mot det andra läget
    int lindataMissing_ = 0;                  // antal saknade tal i ett ofullständigt lindata
    std::string invalidText_;                 // första cellen med ogiltigt tal, till statusraden
    std::unique_ptr<GridHost> hosts_[3];
    std::unique_ptr<kalkyl::sheetgrid::SheetGrid> sheets_[3];
    std::shared_ptr<bool> alive_ = std::make_shared<bool>(true);   // spärr för postade anrop
};
