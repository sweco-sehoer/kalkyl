#pragma once

#include <glaze/glaze.h>

#include <array>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

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
    glaze::TabControl flikar;
    glaze::TabPage byggPmSida;
    glaze::TabPage stolpSida;
    glaze::Panel byggPmPanel;
    glaze::Label byggPmRubrik;
    glaze::Label byggPmBla;
    glaze::Label byggPmGron;
    glaze::Label byggPmGul;
    glaze::Label byggPmFore;
    glaze::Label byggPmLila;
    glaze::Label byggPmEfter;
    glaze::Label byggPmRosa;
    glaze::DataGrid byggPmGrid;
    glaze::Panel stolpPanel;
    glaze::Label stolpRubrik;
    glaze::DataGrid stolpGrid;
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

    // Inställningar > Lindata…: öppnar linregistret som en modal dialog.
    // Rullistorna i Bygg PM läses om efteråt, eftersom återställningen sparas direkt.
    void lindataClicked();

    // En cell i Bygg PM sparades. Gäller Faslina och Topplina: den valda linans id
    // sparas i minnet och en tooltip om att linan saknas töms. Valet sparas inte i projektet.
    void byggPmGridCommitted(glaze::Widget* sender, glaze::CellChangeArgs& e);

    // Läser linregistret och ger Faslina och Topplina rullistor med linornas namn.
    // Ett valt id följer med sitt nya namn, och en borttagen lina står kvar som text med tooltip.
    void fillConductorChoices();

    // Ger stolptabellens rader samma gula, växlande ton som temperaturraderna i Dokumentation
    // (varannan rad en aning mörkare), så att raderna är lätta att följa.
    void shadeStolpRows();

    // Bygger raderna i Bygg PM efter läget. Är Topplina höger "Samma" döljs höger-raderna och
    // vänster-raderna heter som en topplina. Värdena behålls när raderna byts.
    void rebuildByggPm();

    kalkyl::systemdb::SystemDb*   sys_ = nullptr;
    kalkyl::projectdb::ProjectDb* project_ = nullptr;
    Outcome outcome_ = Outcome::Exit;
    std::vector<kalkyl::systemdb::Conductor> conductors_;        // registret som det lästes senast
    std::array<std::optional<std::int64_t>, 3> selectedConductor_;   // vald lina: fas, topp vänster, topp höger (tom = Samma)
    std::vector<std::array<std::string, 2>> byggPmValues_;           // Värde och Referens per rad i kByggPmRows
    std::vector<std::size_t> byggPmShown_;                           // synlig rad -> plats i kByggPmRows
    bool byggPmSplit_ = false;                                       // topplina höger är en egen lina
};
