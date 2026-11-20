#pragma once

#include <glaze/glaze.h>

#include "projectdb/project_db.h"
#include "systemdb/system_db.h"

// Huvudfönstret: programikon (stor först med verktygsfält), menyer och lägesknapp i namnlisten,
// arbetsyta och statusrad. Titeln visar projektnamnet och statusraden projektets sökväg.
// Regionen GlazeBuilder ägs av editorn. Allt efter #pragma endregion är eget.
// Klassen ligger utanför namespace: GlazeBuilder läser bara okvalificerade fönsterklasser.
class MainWindow : public glaze::AppWindow {
#pragma region GlazeBuilder
    // GENERERAD av GlazeBuilder — redigera i GlazeBuilder, inte för hand.
public:
    MainWindow();

private:
    glaze::ToolButton themeBtn;
    void initializeComponent();
#pragma endregion

public:
    // Vad main gör när run() har returnerat.
    // Exit avslutar programmet, CloseProject går tillbaka till väljaren.
    enum class Outcome { Exit, CloseProject };

    // Anger systemdatabasen och det öppna projektet. Anropas före run().
    // Objekten ägs av main och måste leva längre än fönstret.
    void setContext(kalkyl::systemdb::SystemDb& sys, kalkyl::projectdb::ProjectDb& project);

    // Hur fönstret stängdes. Krysset och Arkiv > Avsluta ger Exit.
    // Arkiv > Stäng projekt ger CloseProject.
    Outcome outcome() const;

private:
    // Startlogik: utseende, titel med projektnamn och statusraden.
    // Kopplar också menyernas handlers.
    void onLoad(glaze::Widget* sender, glaze::EventArgs& e);

    // Namnlistens lägesknapp: växlar System -> Mörkt -> Ljust -> System.
    // Läget sparas i systemdatabasen.
    void themeBtnClicked(glaze::Widget* sender, glaze::ClickArgs& e);

    // Arkiv > Stäng projekt: stänger fönstret och går tillbaka till väljaren.
    // Själva stängningen av projektet gör main.
    void closeProjectClicked();

    // Arkiv > Avsluta: stänger fönstret och avslutar programmet.
    // Samma resultat som krysset.
    void exitClicked();

    // Hjälp > Om Kalkyl: visar namn och version.
    // Rutan är GlazeUI:s egen meddelanderuta.
    void aboutClicked();

    kalkyl::systemdb::SystemDb*   sys_ = nullptr;
    kalkyl::projectdb::ProjectDb* project_ = nullptr;
    Outcome outcome_ = Outcome::Exit;
};
