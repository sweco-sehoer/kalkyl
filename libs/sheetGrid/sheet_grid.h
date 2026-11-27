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
    // Inklistringen får gå förbi sista raden: fält under tabellen blir ändringar i nya rader,
    // som fönstret lägger till i applyEdits. SheetGrid läser då radantalet ur tabellen före
    // varje åtgärd, och rowCount är bara startvärdet.
    bool growRows = false;
    // En överhoppad rubrikrad tar en rad i tabellen (true, som arket Linberäkning) eller ingen,
    // så att datat börjar på ankarraden (false, som normalspannet och stolptabellen).
    bool headerTakesRow = true;
    // Rader som står för dolda rader (filtrets grå rader). Som skipRow markeras, målas och tar de
    // emot inget, och kopieringen hoppar över dem helt. En värd som vill klistra in i raderna bakom
    // dem tar inklistringen själv (SheetHost::pasteBlock). Tom = inga.
    std::function<bool(int row)> hiddenRow;
};

// Ett planerat inklistrat block: ändringarna, om en rubrikrad hoppades över, och blockets
// storlek i tabellen. rejected är meddelandet när inklistringen avbryts; då är edits tom.
struct SheetPaste {
    std::vector<CellEdit> edits;
    bool skippedHeader = false;
    CellRect area;
    std::optional<std::string> rejected;
    // Radantalet tabellen behöver för ändringarna: max(rowCount, sista ändrade raden + 1).
    int rowsNeeded = 0;
};

// Var ett inklistrat block hamnar (planen fas 2, 5.2). Ren funktion utan fönster.
SheetPaste planSheetPaste(CellPos anchor, const std::vector<std::vector<std::string>>& block,
                          const SheetShape& shape);

// Statustexten efter en inklistring av ett block med blockRows rader: "Klistrade in N rader", och
// "(rubrikraden hoppades över)" när en rubrikrad hoppades över. Delas av SheetGrid och värdar som
// klistrar in själva (SheetHost::pasteBlock).
std::string pasteStatus(const SheetPaste& paste, std::size_t blockRows);

// Statustexten när inget fält i blocket landade i en inmatningscell.
std::string nothingToPasteText();

// Rutan "Klistra in" med texten när en inklistring avbröts, över tabellens fönster när det finns.
void showPasteRejected(glaze::DataGrid& grid, const std::string& text);

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
    // Inklistringen av ett block vid ankaret, markeringens övre vänstra cell. Svarar true när värden
    // har tagit hand om den själv (planeringen, ändringarna, markeringen och statusraden), och då gör
    // SheetGrid inget mer. Förvalet svarar false, och SheetGrid planerar och klistrar in själv.
    virtual bool pasteBlock(const std::vector<std::vector<std::string>>& block, CellPos anchor);
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
    // Fönstret har lagt till eller tagit bort rader: radantalet läses om, markeringen klampas
    // och alla celler målas om.
    void rowsChanged();
    // Markeringen och den aktuella cellen.
    CellRect selection() const;
    void select(CellPos anchor, CellPos extent);
    // Kopierar markeringen till urklippet och klistrar in urklippet vid markeringen.
    void copySelection();
    void pasteAtSelection();

private:
    // Med growRows: läser radantalet när det ändrats (readRowCount). Körs först i varje åtgärd.
    void syncRowCount();
    // Läser radantalet ur tabellen och klampar markeringen till det.
    void readRowCount();
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
