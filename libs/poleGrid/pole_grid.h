#pragma once
#include <glaze/glaze.h>

#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "autoMode/auto_mode.h"
#include "poleTable/pole_table.h"
#include "poleView/pole_view.h"
#include "sheetGrid/sheet_grid.h"

namespace kalkyl::polegrid {

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
    // PoleGrid bytte filtret själv: Visa alla rader i den grå radens snabbmeny, eller showRow på en
    // dold rad. Värden sparar valet och visar det i rullistan. Förvalet gör ingenting.
    virtual void filterChanged(poleview::Filter filter);
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

// Cellens utseende ur märkena och jämförelsen (planen 8.4). column är fältet 0-9 (poletable), och
// -1 (radera-kolumnen) ger bara radens ton. dataRows är antalet datarader; raden dataRows är den
// tomma raden sist. pole är radens Stolpplats, som tooltipen för en ny eller osäker rad nämner.
// Ren funktion som testas utan fönster.
CellLook cellLook(const PoleLooks& looks, int row, int column, int dataRows, std::string_view pole = {});

// Stolptabellen i en DataGrid: raderna som kanonisk decimaltext med den tomma raden sist,
// Excel-markeringen (sheetGrid med växande rader), papperskorgen med frågan, snabbmenyn och
// färgerna. Varje ändring sparas direkt via värden. Fönstret kopplar tabellens händelser hit.
class PoleGrid {
public:
    // owner: fönstret, för frågorna och commitEdits. tableName står i meddelandena ("Stolptabell före").
    // columns är tabellens kolumner som fält i ordning (poletable::beforeColumns eller afterColumns),
    // och radera-kolumnen står efter dem.
    PoleGrid(glaze::AppWindow& owner, glaze::DataGrid& grid, PoleGridHost& host, std::string tableName,
             std::vector<int> columns = poletable::beforeColumns());
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
    // Tabellen jämförs med Stolptabell före (Stolptabell efter): filtret Changed går att använda och
    // snabbmenyn får Kopiera från Stolptabell före. Förval av, och då är filtret alltid All.
    void setComparesWithBefore(bool on);
    // Raderna har id (Stolptabell efter): före varje sparning får nya rader ett id med
    // poletable::assignRowIds, så att raderna i tabellen och i projektet har samma id. Förval av.
    void setRowIds(bool on);
    // Filtret. Sparar en öppen cell, bygger om vyn och behåller den aktuella raden på samma höjd (5.5).
    void setFilter(poleview::Filter filter);
    poleview::Filter filter() const;
    // Visa förändrade (5.6): den första förändrade raden blir aktuell och området rullas fram. Svarar
    // false och gör ingenting när ingen rad är förändrad eller tabellen inte jämförs med före.
    bool showChanged();
    // Gör tabellens rad aktuell och synlig och markerar dess Stolpnr. Är raden dold av filtret blir
    // filtret All, värden får filterChanged och statusraden förklarar.
    void showRow(int row);
    // Tabellens händelser.
    void committed(glaze::CellChangeArgs& e);
    void cellClicked(glaze::CellClickArgs& e);
    void keyDown(glaze::KeyArgs& e);
    void mouseUp(glaze::MouseArgs& e);
    void contextMenu(glaze::ContextMenuArgs& e);

private:
    class Adapter;      // sheetgrid::SheetHost för tabellen, definieras i pole_grid.cpp
    struct EventScope;  // räknar inEvent_ upp och ner, definieras i pole_grid.cpp

    // Bygger om tabellen ur vyn (view_): en tabellrad per vyrad (stolpe, grå rad eller den tomma
    // raden sist), färger och märken, och den aktuella cellen sätts tillbaka klampad, eftersom
    // setRows nollar den. Filtret räknas aldrig om här.
    void fill();
    // Vyns rad får papperskorg i radera-kolumnen (stolpe i ett skrivbart projekt), en låst cell
    // (den tomma raden, skrivskyddat) eller låsta celler i alla kolumner (grå rad).
    void decorateRow(int viewRow);
    // Sätter tooltips, flaggor och radera-kolumnens ton ur looks_ och målar om alla celler.
    void applyLooks();
    // Skriver ändringar i rows_ (Delete, Ctrl+X och inklistring utan grå rader): vyns rader blir
    // modellens rader, nya rader läggs till och tomma rader sist tas bort. Sparas som EN ändring.
    void applyEdits(const std::vector<sheetgrid::CellEdit>& edits);
    // Inklistringen när vyn har grå rader (planen 5.8): blocket skrivs i modellens rader i följd,
    // också i dolda rader. Svarar false när vyn saknar grå rader, och SheetGrid klistrar då in själv.
    bool pasteIntoModel(const std::vector<std::vector<std::string>>& block, sheetgrid::CellPos anchor);
    // Formen för SheetGrid och planeringen av inklistringen, utan rowCount och hiddenRow.
    sheetgrid::SheetShape baseShape() const;
    // Sant när vyn har en grå rad.
    bool hasHiddenRows() const;
    // Vyn som filtret och jämförelsen ger just nu (planen 5.5). Identitet utan jämförelse eller med All.
    poleview::PoleView desiredView() const;
    // Bygger om vyn när den inte längre är den önskade: direkt, eller postat när en händelse pågår
    // eller en cell redigeras.
    void refreshView();
    // Postar en ombyggnad av vyn, högst en åt gången.
    void postRebuild();
    // Bygger om tabellen till den önskade vyn och behåller den aktuella cellen, markeringen och
    // höjden. Väntar när en cell redigeras.
    void rebuildView();
    // Modellens rader för Pole-raderna i rektangelns rader, stigande.
    std::vector<int> selectedModelRows(const sheetgrid::CellRect& rect) const;
    // Fältets text i modellens rad: talet som det skrevs, eller i en tom Spannlängd föreg. värdet
    // ur koordinaterna. group ger tusentalsavgränsare (visning), annars kopieringens text.
    std::string cellText(int row, int field, bool group) const;
    // Sant när rows_ skiljer sig från det senast sparade.
    bool dirty() const;
    // Sparar rows_ via värden när de skiljer sig från det senast sparade. Svarar om det lyckades.
    bool save();
    // Infogar en tom rad ovanför modellens rad, markerar den och sparar.
    void insertRowAbove(int modelRow);
    // Frågar vilka rader som ska raderas och tar vid Ja bort modellens rader och sparar.
    void removeRowsAsked(std::vector<int> modelRows);
    // Skriver tillbaka värdet ur Stolptabell före i en cell (modellens rad och fältet, planen 5.7)
    // och sparar.
    void restoreCell(int modelRow, int field);
    // Vyns rad och kolumn under pekaren, var för sig -1 utanför tabellens celler.
    sheetgrid::CellPos cellUnderPointer(const glaze::ContextMenuArgs& e);
    // Fältet för tabellens kolumn, eller -1 för radera-kolumnen och utanför.
    int fieldOf(int column) const;
    // Tabellens kolumn för fältet, eller -1 när tabellen inte har fältet.
    int columnOf(int field) const;
    // Radera-kolumnen: kolumnen efter datakolumnerna.
    int deleteColumn() const;
    // Fältets namn i meddelandena. I en tabell med Av per fas heter Av "Av Fas1".
    std::string fieldName(int field) const;

    glaze::AppWindow& owner_;                          // fönstret, för frågorna och commitEdits
    glaze::DataGrid& grid_;                            // tabellen som styrs
    PoleGridHost& host_;                               // fönstret som äger projektet
    std::string tableName_;                            // tabellens namn i meddelandena
    std::vector<int> columns_;                         // tabellens datakolumner som fält
    bool readOnly_ = false;                            // skrivskyddat läge
    std::vector<poletable::PoleRow> rows_;             // raderna, utan den tomma raden sist
    std::vector<poletable::PoleRow> saved_;            // det senast sparade
    PoleLooks looks_;                                  // färger och märken
    std::unique_ptr<Adapter> adapter_;                 // adaptern mot SheetGrid
    std::unique_ptr<sheetgrid::SheetGrid> sheet_;      // Excel-markeringen i grid_
    std::shared_ptr<bool> alive_ = std::make_shared<bool>(true);   // spärr för postade anrop
    bool compares_ = false;                            // jämförs med Stolptabell före
    bool rowIds_ = false;                              // nya rader får id före sparningen
    poleview::Filter filter_ = poleview::Filter::All;  // filtret
    poleview::PoleView view_ = poleview::identityView(0);   // tabellens rader: stolpar, grå rader, tom rad
    bool viewPending_ = false;                         // vyn ska byggas om när det går
    bool rebuildPosted_ = false;                       // en ombyggnad är postad
    int inEvent_ = 0;                                  // djupet i tabellens egna händelser
    glaze::Menu hiddenMenu_;                           // den grå radens snabbmeny
    sheetgrid::CellPos menuCell_{-1, -1};              // cellen som högerklickades (vyns rad och kolumn)
};

} // namespace kalkyl::polegrid
