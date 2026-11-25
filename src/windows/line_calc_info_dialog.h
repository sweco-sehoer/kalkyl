#pragma once

#include <glaze/glaze.h>

// PM:et om linberäkningen och felen i Excel-arket: lägena Excel och Ny och varje avsteg med
// skäl. Modal dialog som bara visar texten (res/linberakning_excel.md, inbyggd i programmet).
// Öppnas med i-knappen vid läget Ny i formuläret Linberäkning. Regionen GlazeBuilder ägs av editorn.
// Klassen ligger utanför namespace: GlazeBuilder läser bara okvalificerade fönsterklasser.
class LineCalcInfoDialog : public glaze::AppWindow {
#pragma region GlazeBuilder
    // GENERERAD av GlazeBuilder — redigera i GlazeBuilder, inte för hand.
public:
    LineCalcInfoDialog();

private:
    glaze::Button closeBtn;
    glaze::MarkdownView docView;
    void initializeComponent();
#pragma endregion

private:
    // Startlogik: lägger in PM:ets text i vyn. Texten byggs in i programmet av CMake.
    void onLoad(glaze::Widget* sender, glaze::EventArgs& e);
};
