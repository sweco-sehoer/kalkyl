#pragma once
#include <glaze/glaze.h>

#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "autoMode/auto_mode.h"
#include "poleTable/pole_table.h"
#include "sheetGrid/sheet_grid.h"

namespace kalkyl::polegrid {

// Radera-kolumnen: kolumnen efter stolptabellens åtta datakolumner.
inline constexpr int kDeleteColumn = poletable::kColumnCount;

// Det PoleGrid frågar fönstret om. Fönstret äger projektet, sparar och räknar om.
class PoleGridHost {
public:
    virtual ~PoleGridHost() = default;
    // Sparar hela tabellen i projektet. Svarar om det lyckades. Vid fel har värden visat felet,
    // och PoleGrid läser om tabellen med reloadPoles (postat).
    virtual bool savePoles(const std::vector<poletable::PoleRow>& rows) = 0;
    // Raderna som de står i projektet, efter ett fel vid sparningen.
    virtual std::vector<poletable::PoleRow> reloadPoles() = 0;
    // Tabellen är sparad med en ändring: värden räknar om och sätter nya färger (setLooks).
    virtual void polesChanged() = 0;
    // En Stolpplats skrevs om i en cell, från en text till en annan (båda icke-tomma). Körs efter
    // sparningen och före polesChanged, så att sträckorna kan följa med.
    virtual void poleRenamed(const std::string& from, const std::string& to) = 0;
    // Egna poster sist i snabbmenyn för markeringens första rad (-1 = ingen datarad).
    virtual void addMenuItems(glaze::Menu& menu, int row) = 0;
    // En rad till i frågan "Radera …?" för raderna first..last. Tom = ingen.
    virtual std::string removeNote(int first, int last) const = 0;
    // En kort text i statusraden.
    virtual void showStatus(const std::string& text) = 0;
};

// Färgerna och märkena i tabellen. compare och before finns bara i Stolptabell efter.
struct PoleLooks {
    std::vector<automode::RowCompare> compare;    // per rad i efter; tom i före
    std::vector<poletable::PoleRow> before;       // före-raderna, till tooltipen på en ändrad cell
    automode::TableMarks marks;                   // sträckornas början och slut och cellernas fel
    // Spannlängd föreg. ur koordinaterna per rad (automode::coordinateSpans). PoleGrid räknar
    // den själv ur sina rader i applyLooks, så värden behöver inte sätta den.
    std::vector<std::optional<double>> coordinateSpans;
};

// En cells utseende utan markering: bakgrunden, tooltipen, om Stolpplats får flaggan och om
// Spannlängd föreg. är räknad ur koordinaterna (cellen får då Σ-bilden).
struct CellLook {
    std::optional<gfx::Color> background;
    std::string toolTip;
    bool flag = false;
    bool computed = false;
};

// Texten för en spannlängd räknad ur koordinaterna: två decimaler, decimalkomma och, med
// group, tusentalsavgränsare som i tabellen. Utan grupp är den texten som kopieras.
std::string computedSpanText(double meters, bool group);

// Cellens utseende ur märkena och jämförelsen (planen 8.4). dataRows är antalet datarader;
// raden dataRows är den tomma raden sist. pole är radens Stolpplats, som tooltipen för en ny eller
// osäker rad nämner. Ren funktion som testas utan fönster.
CellLook cellLook(const PoleLooks& looks, int row, int column, int dataRows, std::string_view pole = {});

// Stolptabellen i en DataGrid: raderna som kanonisk decimaltext med den tomma raden sist,
// Excel-markeringen (sheetGrid med växande rader), papperskorgen med frågan, snabbmenyn och
// färgerna. Varje ändring sparas direkt via värden. Fönstret kopplar tabellens händelser hit.
class PoleGrid {
public:
    // owner: fönstret, för frågorna och commitEdits. tableName står i meddelandena ("Stolptabell före").
    PoleGrid(glaze::AppWindow& owner, glaze::DataGrid& grid, PoleGridHost& host, std::string tableName);
    ~PoleGrid();
    PoleGrid(const PoleGrid&) = delete;
    PoleGrid& operator=(const PoleGrid&) = delete;
    // Skrivskyddat: låsta celler, inga papperskorgar, skrivskyddad markering. Bygger om tabellen.
    void setReadOnly(bool readOnly);
    // Raderna som de lästes ur projektet. De blir också det senast sparade. Bygger om tabellen.
    void setRows(std::vector<poletable::PoleRow> rows);
    // Raderna utan den tomma raden sist.
    const std::vector<poletable::PoleRow>& rows() const;
    // Nya färger och märken. Målar om och sätter tooltips och flaggor utan att bygga om raderna.
    void setLooks(PoleLooks looks);
    // Gör raden aktuell och synlig och markerar dess Stolpplats.
    void showRow(int row);
    // Tabellens händelser.
    void committed(glaze::CellChangeArgs& e);
    void cellClicked(glaze::CellClickArgs& e);
    void keyDown(glaze::KeyArgs& e);
    void mouseUp(glaze::MouseArgs& e);
    void contextMenu(glaze::ContextMenuArgs& e);

private:
    class Adapter;   // sheetgrid::SheetHost för tabellen, definieras i pole_grid.cpp

    // Bygger om tabellen: en rad per stolpe och den tomma raden sist, färger och märken, och
    // den aktuella cellen sätts tillbaka klampad, eftersom setRows nollar den.
    void fill();
    // Raden får papperskorg i radera-kolumnen (datarad i ett skrivbart projekt) eller en låst cell.
    void decorateRow(int row);
    // Sätter tooltips, flaggor och radera-kolumnens ton ur looks_ och målar om alla celler.
    void applyLooks();
    // Skriver inklistrade och raderade celler i rows_: nya rader läggs till, tomma rader sist tas
    // bort. Tabellen byggs om och sparas som EN ändring.
    void applyEdits(const std::vector<sheetgrid::CellEdit>& edits);
    // Cellens text i tabellen: talet som det skrevs, eller i en tom Spannlängd föreg. värdet
    // ur koordinaterna. group ger tusentalsavgränsare (visning), annars kopieringens text.
    std::string cellText(int row, int column, bool group) const;
    // Sant när rows_ skiljer sig från det senast sparade.
    bool dirty() const;
    // Sparar rows_ via värden när de skiljer sig från det senast sparade. Svarar om det lyckades.
    bool save();
    // Infogar en tom rad vid row, markerar den och sparar.
    void insertRowAbove(int row);
    // Frågar "Radera stolpplats …?" och tar vid Ja bort raderna first till last och sparar.
    void removeRowsAsked(int first, int last);

    glaze::AppWindow& owner_;                          // fönstret, för frågorna och commitEdits
    glaze::DataGrid& grid_;                            // tabellen som styrs
    PoleGridHost& host_;                               // fönstret som äger projektet
    std::string tableName_;                            // tabellens namn i meddelandena
    bool readOnly_ = false;                            // skrivskyddat läge
    std::vector<poletable::PoleRow> rows_;             // raderna, utan den tomma raden sist
    std::vector<poletable::PoleRow> saved_;            // det senast sparade
    PoleLooks looks_;                                  // färger och märken
    std::unique_ptr<Adapter> adapter_;                 // adaptern mot SheetGrid
    std::unique_ptr<sheetgrid::SheetGrid> sheet_;      // Excel-markeringen i grid_
    std::shared_ptr<bool> alive_ = std::make_shared<bool>(true);   // spärr för postade anrop
};

} // namespace kalkyl::polegrid
