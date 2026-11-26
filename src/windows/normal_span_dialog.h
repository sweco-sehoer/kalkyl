#pragma once

#include <glaze/glaze.h>

#include <array>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "anCalc/normal_span.h"
#include "projectdb/project_data.h"
#include "projectdb/project_db.h"

// Normalspannsberäkningen: formuläret med tabellen som i Excel och resultatet.
// Modal dialog som sparar själv i projektet när användaren trycker OK. Ändringarna buffras,
// och Avbryt, krysset och Escape frågar om osparade ändringar.
// Regionen GlazeBuilder ägs av editorn. Allt efter #pragma endregion är eget.
// Klassen ligger utanför namespace: GlazeBuilder läser bara okvalificerade fönsterklasser.
class NormalSpanDialog : public glaze::AppWindow {
#pragma region GlazeBuilder
    // GENERERAD av GlazeBuilder — redigera i GlazeBuilder, inte för hand.
public:
    NormalSpanDialog();

private:
    glaze::Label noteLbl;
    glaze::TextInput noteTxt;
    glaze::Label docLbl;
    glaze::Label infoLbl;
    glaze::Button cancelBtn;
    glaze::Button okBtn;
    glaze::Button clearBtn;
    glaze::Panel resultPanel;
    glaze::GroupBox resultGrp;
    glaze::Label anLbl;
    glaze::Label anValue;
    glaze::Label anDhLbl;
    glaze::Label anDhValue;
    glaze::Label meanLbl;
    glaze::Label meanValue;
    glaze::Label totalLbl;
    glaze::Label totalValue;
    glaze::Label countLbl;
    glaze::Label countValue;
    glaze::Label sectionLbl;
    glaze::Label sectionValue;
    glaze::GroupBox limitGrp;
    glaze::Label lim1Lbl;
    glaze::SpinBox lim1Spin;
    glaze::Label lim1Count;
    glaze::Label lim2Lbl;
    glaze::SpinBox lim2Spin;
    glaze::Label lim2Count;
    glaze::Label lim3Lbl;
    glaze::SpinBox lim3Spin;
    glaze::Label lim3Count;
    glaze::Label lim4Lbl;
    glaze::SpinBox lim4Spin;
    glaze::Label lim4Count;
    glaze::Label lim5Lbl;
    glaze::SpinBox lim5Spin;
    glaze::Label lim5Count;
    glaze::Label methodLbl;
    glaze::DataGrid poleGrid;
    void initializeComponent();
#pragma endregion

public:
    // Projektet som formuläret läser och sparar i. Anropas före run(owner); ägs av main.
    void setProject(kalkyl::projectdb::ProjectDb& project);
    // En ny beräkning. linkDocKey = raden i Dokumentation som ska visa den när användaren
    // trycker OK (Lägg till...), tom = lös.
    void setNew(std::string linkDocKey);
    // En befintlig beräkning, som läses ur projektet i onLoad.
    void setExisting(std::int64_t id);
    // En ny, lös beräkning med en kopia av beräkningen id (Kopiera...).
    void setCopyOf(std::int64_t id);
    // Radtexterna i Dokumentation per nyckel, för upplysningen om var beräkningen visas.
    void setDocRowLabels(std::vector<std::pair<std::string, std::string>> labels);
    // Skrivskyddat: allt visas, inget kan ändras, OK döljs och Avbryt heter Stäng.
    void setReadOnly(bool readOnly);
    // Efter run(): id för den sparade beräkningen, tomt om inget sparades.
    std::optional<std::int64_t> savedId() const;

private:
    // En cell i tabellen. Markeringen är rektangeln mellan ankaret och änden.
    struct CellPos {
        int row = 0;
        int column = 0;
    };

    // Rektangeln som markeringen täcker, båda ändar inräknade.
    struct CellRect {
        int row0 = 0;
        int column0 = 0;
        int row1 = 0;
        int column1 = 0;
    };

    // Vad tabellen visar för en rad: färgmärke per kolumn, felets text per indatakolumn
    // och spannets index i resultatet (-1 när raden inte är ett spann).
    struct RowInfo {
        std::array<int, 8> mark{};
        std::array<std::string, 3> error;
        int span = -1;
    };

    // Startlogik: läser beräkningen och räknar första gången.
    // Körs en gång när fönstret laddas, aldrig i konstruktorn.
    void onLoad(glaze::Widget* sender, glaze::EventArgs& e);

    // Krysset, Avbryt och Escape: frågar om osparade ändringar.
    // Ja sparar, Nej kastar ändringarna och Avbryt lämnar fönstret öppet.
    void onClosing(glaze::Widget* sender, glaze::CloseArgs& e);

    // OK: validerar, sparar beräkningen i projektet och stänger.
    // Vid ett fel visas det och fönstret står kvar.
    void okBtnClicked(glaze::Widget* sender, glaze::ClickArgs& e);

    // Töm: tömmer hela tabellen efter en Ja/Nej-fråga.
    void clearBtnClicked(glaze::Widget* sender, glaze::ClickArgs& e);

    // Anteckningen ändrades: markerar formuläret som ändrat.
    void noteChanged(glaze::Widget* sender, glaze::ChangeArgs& e);

    // En gräns ändrades: markerar formuläret som ändrat och räknar om kontrollerna.
    void limitCommitted(glaze::Widget* sender, glaze::ChangeArgs& e);

    // En cell sparades: lägger till en tom rad sist om det behövs och räknar om.
    void poleGridCommitted(glaze::Widget* sender, glaze::CellChangeArgs& e);

    // Tangenter i tabellen: urklipp, Ctrl+A, Delete och markering med Shift.
    // Under redigering tas bara Ctrl+V med ett block.
    void poleGridKeyDown(glaze::Widget* sender, glaze::KeyArgs& e);

    // Musen släpptes i tabellen: markeringen följer den aktuella cellen,
    // och Shift+klick utökar den.
    void poleGridMouseUp(glaze::Widget* sender, glaze::MouseArgs& e);

    // Högerklick i tabellen: snabbmenyn för markeringen.
    void poleGridContextMenu(glaze::Widget* sender, glaze::ContextMenuArgs& e);

    // Läser beräkningen som ska öppnas (befintlig eller kopia) och fyller formuläret.
    // Svarar false när den inte finns, efter att felet visats.
    bool loadCalculation();

    // Skriver upplysningen om var beräkningen visas i Dokumentation.
    void updateDocLabel(const std::vector<std::string>& docKeys);

    // Gör alla redigerare skrivskyddade och döljer OK och Töm.
    void applyReadOnly();

    // Läser tabellen till stolpar, utan den sista tomma raden. En cell med ogiltigt tal
    // räknas som tom och ger ett fel i gridIssues.
    std::vector<kalkyl::ancalc::SpanPole> readPoles(
        std::vector<kalkyl::ancalc::SpanIssue>& gridIssues) const;

    // Läser gränserna ur de fem talfälten.
    kalkyl::ancalc::SpanLimits readLimits() const;

    // Räknar om allt: resultatet, kontrollkolumnerna, färgerna, tooltips, panelen och statusraden.
    void recalc();

    // Skriver resultatpanelen och antalen avvikelser.
    void updateResults();

    // Skriver statusraden: första felet, annars första varningen, annars sträckan.
    void updateStatus();

    // Skriver en egen text i statusraden.
    void setStatus(const std::string& text);

    // Färgen en cell har utan markering, ur resultatet och felen.
    std::optional<gfx::Color> baseColor(int row, int column) const;

    // Målar om en cell, med markeringens färg om den ligger i en utökad markering.
    void paintCell(int row, int column);

    // Målar om alla celler i en rektangel.
    void paintRect(const CellRect& rect);

    // Målar om hela tabellen.
    void paintAll();

    // Markeringens rektangel och om en cell ligger i en utökad markering.
    CellRect selectionRect() const;
    bool inSelection(int row, int column) const;

    // Klampar en cell till tabellen.
    CellPos clampPos(CellPos pos) const;

    // Sätter markeringen och målar om de berörda cellerna. moveCurrent flyttar
    // tabellens aktuella cell till änden.
    void setSelection(CellPos anchor, CellPos extent, bool moveCurrent);

    // Läser tabellens aktuella cell. extend utökar markeringen, annars blir den en cell.
    void syncSelection(bool extend);

    // Markerar alla celler, med den aktuella cellen överst till vänster.
    void selectAll();

    // Kopierar markeringen som tabbseparerad text.
    void copySelection();

    // Markeringen som tabbseparerad text, tom när det inte finns något att kopiera.
    std::string selectionText();

    // Klistrar in urklippet från cellen och åt höger och nedåt.
    void pasteAt(CellPos cell);

    // Klistrar in texten från cellen och åt höger och nedåt.
    void pasteText(CellPos cell, const std::string& clip);

    // Tömmer indatacellerna i markeringen och tar bort tomma rader sist.
    void clearSelectionCells();

    // Infogar en tom rad ovanför markeringen.
    void insertRowAbove();

    // Tar bort raderna som markeringen täcker.
    void deleteSelectedRows();

    // Tar bort tomma rader sist tills exakt en finns.
    void trimTrailingRows();

    // Lägger till en tom rad sist om den sista raden har innehåll.
    void ensureTrailingRow();

    // Sant när en rad saknar text, tal och fel i indatakolumnerna.
    bool isEmptyRow(int row) const;

    // Sista raden med data, eller 0 när tabellen är tom.
    int lastDataRow() const;

    // Visar snabbmenyn för markeringen.
    void showGridMenu(float x, float y);

    // Validerar och sparar. Svarar false när fönstret ska stå kvar, efter att felet visats.
    bool saveChanges();

    kalkyl::projectdb::ProjectDb* project_ = nullptr;
    std::string linkDocKey_;                  // raden som ska visa beräkningen efter OK
    std::int64_t existingId_ = 0;             // beräkningen som ändras, 0 = ingen
    std::int64_t copyOfId_ = 0;               // beräkningen som kopieras, 0 = ingen
    std::vector<std::pair<std::string, std::string>> docLabels_;   // radtext per nyckel
    bool readOnly_ = false;
    std::optional<kalkyl::projectdb::AutoOrigin> automatic_;   // satt för en automatisk beräkning
    bool dirty_ = false;                     // något är ändrat sedan inläsningen
    bool saved_ = false;                      // sparat: onClosing frågar inte
    std::optional<std::int64_t> savedId_;
    kalkyl::ancalc::SpanCalculation calc_;      // senaste beräkningen
    std::vector<kalkyl::ancalc::SpanIssue> issues_;   // fel och varningar, i radordning
    std::vector<RowInfo> info_;               // färger och fel per rad
    int firstRow_ = -1;                       // första raden med innehåll
    CellPos anchor_;                          // markeringens ankare
    CellPos extent_;                          // markeringens ände, tabellens aktuella cell
    glaze::Menu gridMenu_;                    // snabbmenyn i tabellen
    std::shared_ptr<bool> alive_ = std::make_shared<bool>(true);   // spärr för postade anrop
};
