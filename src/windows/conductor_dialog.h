#pragma once

#include <glaze/glaze.h>

#include <cstdint>
#include <vector>

#include "systemdb/system_db.h"

// Lindata: linregistret i en modal dialog. Tabellen buffrar alla ändringar, och inget skrivs
// till systemdatabasen förrän OK. Avbryt, krysset och Escape frågar om osparade ändringar.
// Regionen GlazeBuilder ägs av editorn. Allt efter #pragma endregion är eget.
// Klassen ligger utanför namespace: GlazeBuilder läser bara okvalificerade fönsterklasser.
class ConductorDialog : public glaze::AppWindow {
#pragma region GlazeBuilder
    // GENERERAD av GlazeBuilder — redigera i GlazeBuilder, inte för hand.
public:
    ConductorDialog();

private:
    glaze::Label infoLbl;
    glaze::Button cancelBtn;
    glaze::Button okBtn;
    glaze::Button addBtn;
    glaze::Button removeBtn;
    glaze::Button restoreBtn;
    glaze::DataGrid grid;
    void initializeComponent();
#pragma endregion

public:
    // Anger systemdatabasen som registret läses från och sparas i.
    // Anropas före run(owner); databasen ägs av main och lever längre än fönstret.
    void setSystemDb(kalkyl::systemdb::SystemDb& sys);

private:
    // Startlogik: fyller tabellen ur registret och sätter statusraden och knapparnas läge.
    // Körs en gång när fönstret laddas, aldrig i konstruktorn.
    void onLoad(glaze::Widget* sender, glaze::EventArgs& e);

    // Krysset, Avbryt och Escape: frågar om osparade ändringar.
    // Ja sparar, Nej kastar ändringarna och Avbryt lämnar fönstret öppet.
    void onClosing(glaze::Widget* sender, glaze::CloseArgs& e);

    // OK: sparar ändringarna i en transaktion och stänger.
    // Vid ett fel visas det och fönstret står kvar.
    void okBtnClicked(glaze::Widget* sender, glaze::ClickArgs& e);

    // Lägg till: en ny rad sist med den aktuella radens värden.
    // Namncellen börjar redigeras direkt.
    void addBtnClicked(glaze::Widget* sender, glaze::ClickArgs& e);

    // Ta bort: tar bort den aktuella raden efter en Ja/Nej-fråga.
    // Borttagningen sparas först när användaren trycker OK.
    void removeBtnClicked(glaze::Widget* sender, glaze::ClickArgs& e);

    // Återställ startdata: SWECO:s värden tillbaka, direkt i databasen.
    // Tabellen läses sedan om och osparade ändringar går förlorade.
    void restoreBtnClicked(glaze::Widget* sender, glaze::ClickArgs& e);

    // En cell sparades: räknar om krypningstemperaturen, markerar fel och uppdaterar statusen.
    // Namnändringar kontrollerar också dubbletter.
    void gridCommitted(glaze::Widget* sender, glaze::CellChangeArgs& e);

    // Den aktuella raden ändrades.
    // Knapparnas läge uppdateras.
    void gridSelectionChanged(glaze::Widget* sender, glaze::ChangeArgs& e);

    // Läser registret och fyller tabellen.
    // Nollställer också ändringarna: dirty_, borttagna och radernas id.
    void fillGrid();

    // Bygger en lina av radens celler. Ett tal som saknas eller inte går att tolka blir NaN.
    // Id och ursprung hämtas ur rowIds_ och loaded_.
    kalkyl::systemdb::Conductor rowConductor(int row) const;

    // Markerar felen på en rad: röda celler, bärnsten för avvikande krypning, tooltips och Källa.
    // Giltiga celler får sina färger nollade.
    void markRow(int row);

    // Färgar namn som förekommer på mer än en rad.
    // Körs efter varje namnändring, tillägg och borttagning.
    void markNames();

    // Validerar och sparar ändringarna. Svarar false när fönstret ska stå kvar,
    // efter att felet visats. Sparar inget om något är ogiltigt.
    bool saveChanges();

    // Skriver antalet linor, och orsaken om systemdatabasen inte är beständig, i statusraden.
    void updateStatus();

    // Aktiverar Ta bort bara när en rad är aktuell.
    void updateButtons();

    kalkyl::systemdb::SystemDb* sys_ = nullptr;
    std::vector<kalkyl::systemdb::Conductor> loaded_;   // som registret lästes
    std::vector<std::int64_t> rowIds_;                  // id per rad (appens index), 0 = ny
    std::vector<std::int64_t> removedIds_;              // id på rader som tagits bort
    bool dirty_ = false;                                // något är ändrat sedan inläsningen
    bool saved_ = false;                                // sparat: onClosing frågar inte
};
