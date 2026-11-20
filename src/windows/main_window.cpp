#include "main_window.h"

#include "appearance/appearance.h"
#include "version/version.h"

#include <string>

// Anger systemdatabasen och projektet som fönstret visar.
// Objekten ägs av main och måste leva längre än fönstret.
void MainWindow::setContext(kalkyl::systemdb::SystemDb& sys, kalkyl::projectdb::ProjectDb& project)
{
    sys_ = &sys;
    project_ = &project;
}

// Svarar hur fönstret stängdes, så att main vet om väljaren ska visas igen.
// Krysset ger Exit, eftersom outcome_ då aldrig ändrats.
MainWindow::Outcome MainWindow::outcome() const
{
    return outcome_;
}

// Startlogik, körs när fönstret laddas (aldrig i konstruktorn).
// Sätter utseendet, titeln med projektnamn, statusraden och kopplar menyernas handlers.
void MainWindow::onLoad(glaze::Widget*, glaze::EventArgs&)
{
    kalkyl::appearance::apply(*this, *sys_);
    kalkyl::appearance::syncButton(themeBtn, themeMode());

    const bool readOnly = project_->access() == kalkyl::projectdb::Access::ReadOnly;
    setTitle(project_->info().name + " – Kalkyl " + KALKYL_VERSION +
             (readOnly ? " (skrivskyddat)" : ""));
    statusBar.setPanelText(0, project_->path());

    if (readOnly) {
        const auto& lock = project_->lockedBy();
        statusBar.addPanel(lock ? "Skrivskyddat – öppnat av " + lock->user + " på " + lock->computer
                                : std::string("Skrivskyddat"));
    }
    if (!sys_->isPersistent())
        statusBar.addPanel("Inställningar sparas inte");

    menuBar.setHandler({"Arkiv", "Stäng projekt"}, [this] { closeProjectClicked(); });
    menuBar.setHandler({"Arkiv", "Avsluta"}, [this] { exitClicked(); });
    menuBar.setHandler({"Hjälp", "Om Kalkyl"}, [this] { aboutClicked(); });
}

// Namnlistens lägesknapp: System -> Mörkt -> Ljust -> System.
// Logiken delas med väljaren via kalkyl::appearance.
void MainWindow::themeBtnClicked(glaze::Widget*, glaze::ClickArgs&)
{
    kalkyl::appearance::cycle(*this, themeBtn, *sys_);
}

// Arkiv > Stäng projekt: main stänger projektet och visar väljaren igen.
// close() verkställs uppskjutet av GlazeUI.
void MainWindow::closeProjectClicked()
{
    outcome_ = Outcome::CloseProject;
    close();
}

// Arkiv > Avsluta: stänger fönstret och därmed programmet.
// Samma resultat som krysset.
void MainWindow::exitClicked()
{
    outcome_ = Outcome::Exit;
    close();
}

// Hjälp > Om Kalkyl: visar programmets namn och version.
// Rutan är GlazeUI:s egen meddelanderuta.
void MainWindow::aboutClicked()
{
    glaze::messageBox(*this,
                      std::string("Kalkyl ") + KALKYL_VERSION + "\nLuftledningsberäkningar",
                      "Om Kalkyl");
}
