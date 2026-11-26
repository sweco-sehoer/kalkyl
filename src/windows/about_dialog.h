#pragma once

#include <glaze/glaze.h>

// Om Kalkyl: programikonen i stort format, namn, version, underlaget för beräkningarna,
// utvecklaren och copyright. Modal dialog som bara visar text. Öppnas med Hjälp > Om Kalkyl.
// Regionen GlazeBuilder ägs av editorn.
// Klassen ligger utanför namespace: GlazeBuilder läser bara okvalificerade fönsterklasser.
class AboutDialog : public glaze::AppWindow {
#pragma region GlazeBuilder
    // GENERERAD av GlazeBuilder — redigera i GlazeBuilder, inte för hand.
public:
    AboutDialog();

private:
    glaze::Picture iconPic;
    glaze::Label nameLbl;
    glaze::Label versionLbl;
    glaze::Label descLbl;
    glaze::Label basisLbl;
    glaze::Label devLbl;
    glaze::Label copyLbl;
    glaze::Button closeBtn;
    void initializeComponent();
#pragma endregion

private:
    // Startlogik: skriver programmets version, som bara finns i bygget.
    void onLoad(glaze::Widget* sender, glaze::EventArgs& e);
};
