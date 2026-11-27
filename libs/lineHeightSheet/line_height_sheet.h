#pragma once
#include <glaze/glaze.h>

#include <cstdint>
#include <string>

#include "lineHeight/line_height.h"

namespace kalkyl::lineheightsheet {

// Linhöjdsbladet i fliken Linhöjder: blocken ur lineheight::Sheet ritade som Excel-förlagan, med
// färger, sammanslagna celler och ramar, i en Panel som rullar. Ett eget textfält redigerar den
// aktuella inmatningscellen. Bladet sparar inget självt: värden läser lastCommit() i onCommit.
class LineHeightSheet : public glaze::Panel {
public:
    LineHeightSheet();
    ~LineHeightSheet() override;

    // Byter bladet. Den aktuella cellen följer sin rad (id) och inmatning (lineheight::findInput),
    // och en öppen redigering står kvar med det som skrivits. Finns cellen inte kvar stängs
    // redigeringen utan att något fyras.
    void setSheet(lineheight::Sheet sheet);
    const lineheight::Sheet& sheet() const;

    // Skrivskyddat: textfältet går att markera och kopiera i men inte ändra, och onCommit fyras aldrig.
    void setReadOnly(bool on);
    bool isReadOnly() const;

    // Börjar redigera cellen (index i sheet().cells, en inmatning) och rullar fram den. Svarar
    // false för en cell som inte går att redigera.
    bool beginEdit(int cell);

    // Den aktuella cellen, eller -1.
    int currentCell() const;

    // Den senaste sparade inmatningen: vad cellen skriver, blockets rad-id och texten som den
    // skrevs. Värden läser den i onCommit.
    struct Commit {
        lineheight::InputRef input;
        std::int64_t blockRow = 0;
        std::string text;
    };
    const Commit& lastCommit() const;

    // Användaren är klar med en cell vars text ändrades: Enter, Tab, pilarna, ett klick på en annan
    // cell, fokus bort (postat) eller commitEdit.
    glaze::Handler<glaze::ChangeArgs> onCommit;

    // Sparar en öppen redigering nu och stänger den (AppWindow::commitEdits når den).
    void commitEdit() override;

    // Tooltipen för cellen under punkten.
    glaze::TrText toolTipAt(const glaze::Ctx& c, float x, float y) const override;

    // Textfältet är bladets eget barn och får inte lossas utifrån.
    bool canReleaseChild(const glaze::Widget& child) const override;

protected:
    void draw(const glaze::Ctx& c) override;
    gfx::Sizef contentExtent(const glaze::Ctx& c) const override;
    void mouseDown(const glaze::Ctx& c, float mx, float my) override;
    bool keyDown(gfx::Key key, std::uint32_t mods) override;
    bool focusable() const override;

private:
    class Editor;   // glaze::TextInput som tar Enter, Tab, Escape, Upp och Ner (som PropertyGrids TextEditor)

    // Resten är implementationens: cellernas och kolumnernas mått, ritningen, träffen, öppna och
    // stänga redigeringen, förflyttningen och rullningen till cellen.
    int cellAtPoint(const glaze::Ctx& c, float mx, float my) const;
    bool canMeasure() const;
    void placeEditor(const glaze::Ctx& c);
    void scrollToCell(int cell);
    bool openAfterSave(int cell);
    void openEdit(int cell);
    bool endEdit(bool save, bool posted);
    bool finish(lineheight::Move move);
    void cancelEdit();
    void setCurrent(int cell);

    lineheight::Sheet sheet_;
    bool readOnly_ = false;
    Commit lastCommit_;
    Editor* editor_ = nullptr;        // ägt barn (add<> i konstruktorn), dolt när det inte används
    bool editing_ = false;            // textfältet är öppet på current_
    int current_ = -1;                // aktuell inmatningscell
    std::int64_t currentRow_ = 0;     // aktuella cellens block (rad-id) och inmatning, så att den
    lineheight::InputRef currentInput_;   // följer med när bladet byts
};

} // namespace kalkyl::lineheightsheet
