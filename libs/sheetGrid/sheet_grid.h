#pragma once
#include <glaze/glaze.h>

#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace kalkyl::sheetgrid {

// En cell i tabellen och en rektangel av celler, båda ändar inräknade.
struct CellPos { int row = 0; int column = 0; };
struct CellRect { int row0 = 0; int column0 = 0; int row1 = 0; int column1 = 0; };

// En ändring som fönstret ska göra: texten i cellen, med tal i kanonisk form med punkt.
// Tom text = töm cellen.
struct CellEdit { int row = 0; int column = 0; std::string text; };

// Tabellens form för markeringen och inklistringen. Fönstret svarar på frågorna.
// skipRow: tomraden, som aldrig markeras och vars fält ignoreras. isInput: cellen tar emot
// inklistring och Delete. accept: fältet som cellens text, tomt svar = passar inte (text i en
// talcell). columnName: kolumnens namn i meddelandet när en inklistring avbryts.
struct SheetShape {
    int rowCount = 0;
    int columnCount = 0;
    int firstColumn = 0;                        // 1 när kolumn 0 är radrubrik
    std::function<bool(int row)> skipRow;
    std::function<bool(int row, int column)> isInput;
    std::function<std::optional<std::string>(int row, int column, const std::string& field)> accept;
    std::function<std::string(int column)> columnName;
};

// Ett planerat inklistrat block: ändringarna, om en rubrikrad hoppades över, och blockets
// storlek i tabellen. rejected är meddelandet när inklistringen avbryts; då är edits tom.
struct SheetPaste {
    std::vector<CellEdit> edits;
    bool skippedHeader = false;
    CellRect area;
    std::optional<std::string> rejected;
};

// Var ett inklistrat block hamnar (planen fas 2, 5.2). Ren funktion utan fönster.
SheetPaste planSheetPaste(CellPos anchor, const std::vector<std::vector<std::string>>& block,
                          const SheetShape& shape);

// Rektangeln mellan två celler, med rad0 <= rad1 och kolumn0 <= kolumn1.
CellRect rectOf(CellPos a, CellPos b);

// Cellen klampad till tabellen. En tomrad flyttas till nästa rad i riktningen step (+1 eller -1),
// och åt andra hållet när det inte finns någon.
CellPos clampPos(CellPos pos, const SheetShape& shape, int step);

// Sant när urklippet är ett block: flera rader eller flera fält. En enda cell från Excel
// (med avslutande radbrytning) räknas inte.
bool isBlock(const std::string& clip);

// Det SheetGrid frågar fönstret om. Fönstret äger innehållet och räknar om.
class SheetHost {
public:
    virtual ~SheetHost() = default;
    // Texten som kopieras för cellen (som den visas, tal med decimalkomma).
    virtual std::string copyText(int row, int column) const = 0;
    // Cellens färg utan markering.
    virtual std::optional<gfx::Color> baseColor(int row, int column) const = 0;
    // Skriver ändringarna som EN ändring (inklistring, Delete, Ctrl+X) och räknar om.
    virtual void applyEdits(const std::vector<CellEdit>& edits) = 0;
    // Egna poster sist i snabbmenyn, efter en separator. Tomt som förval.
    virtual void addMenuItems(glaze::Menu& menu, const CellRect& selection);
    // En kort text i statusraden ("Kopierade 3 × 2 celler").
    virtual void showStatus(const std::string& text) = 0;
};

// Excel-beteendet i en DataGrid (planen fas 2, 5.2): rektangulär markering med Shift och
// Shift+klick, Ctrl+C, X, V, A, Delete och snabbmenyn. Fönstret kopplar tabellens onKeyDown,
// onMouseUp och onContextMenu hit. Tabellen och värden måste leva längre än objektet.
class SheetGrid {
public:
    SheetGrid(glaze::DataGrid& grid, SheetShape shape, SheetHost& host);
    // Skrivskyddat: bara Ctrl+C, Ctrl+A och markeringen fungerar.
    void setReadOnly(bool readOnly);
    void keyDown(glaze::KeyArgs& e);
    void mouseUp(glaze::MouseArgs& e);
    void contextMenu(glaze::ContextMenuArgs& e);
    // Målar om alla celler eller en cell: markeringens färg i en utökad markering, annars
    // värdens baseColor.
    void paintAll();
    void paintCell(int row, int column);
    // Markeringen och den aktuella cellen.
    CellRect selection() const;
    void select(CellPos anchor, CellPos extent);
    // Kopierar markeringen till urklippet och klistrar in urklippet vid markeringen.
    void copySelection();
    void pasteAtSelection();

private:
    void syncSelection(bool extend);
    void clearSelectionCells();
    void selectAll();

    glaze::DataGrid& grid_;
    SheetShape shape_;
    SheetHost& host_;
    bool readOnly_ = false;
    CellPos anchor_;
    CellPos extent_;
    glaze::Menu menu_;
    std::shared_ptr<bool> alive_ = std::make_shared<bool>(true);   // spärr för postade anrop
};

} // namespace kalkyl::sheetgrid
