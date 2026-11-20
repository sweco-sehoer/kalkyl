#pragma once

#include <glaze/glaze.h>
#include <glaze_threads/threads.h>

#include <optional>
#include <string>
#include <vector>

#include "core/result.h"
#include "projectdb/project_db.h"
#include "systemdb/system_db.h"

// Projektväljaren: de senaste projekten, Öppna, Öppna annat projekt och Nytt projekt.
// Fönstret stängs när ett projekt har öppnats, och main hämtar det med takeProject().
// Öppning, skapande och filkontroll går i bakgrunden så att fönstret aldrig hänger.
// Klassen ligger utanför namespace: GlazeBuilder läser bara okvalificerade klassnamn.
class ProjectPicker : public glaze::AppWindow {
#pragma region GlazeBuilder
    // GENERERAD av GlazeBuilder — redigera i GlazeBuilder, inte för hand.
public:
    ProjectPicker();

private:
    glaze::ToolButton themeBtn;
    glaze::Label titleLbl;
    glaze::Button newBtn;
    glaze::Button browseBtn;
    glaze::Button openBtn;
    glaze::Button removeBtn;
    glaze::ListView recentList;
    glaze::Label emptyLbl;
    glaze::Timer showTimer;
    void initializeComponent();
#pragma endregion

public:
    // Anger systemdatabasen som väljaren läser och skriver.
    // Anropas före run(); databasen ägs av main och lever längre än fönstret.
    void setSystemDb(kalkyl::systemdb::SystemDb& sys);

    // Anger en projektfil som ska öppnas direkt vid start. Anropas före run(),
    // och main anropar sedan hide() så att väljaren inte syns om öppningen går fort.
    void setAutoOpen(const std::string& path);

    // Tar det öppnade projektet efter run(). Tomt värde betyder att väljaren
    // stängdes utan att något projekt öppnades.
    std::optional<kalkyl::projectdb::ProjectDb> takeProject();

private:
    // Startlogik: utseende, titel och varning om systemdatabasen.
    // Fyller sedan listan och startar en eventuell direktöppning.
    void onLoad(glaze::Widget* sender, glaze::EventArgs& e);

    // Lägesknappen i namnlisten: System, Mörkt, Ljust.
    // Läget sparas i systemdatabasen.
    void themeBtnClicked(glaze::Widget* sender, glaze::ClickArgs& e);

    // Nytt projekt: väljer filnamn och plats med filväljaren.
    // Projektet skapas sedan i bakgrunden.
    void newBtnClicked(glaze::Widget* sender, glaze::ClickArgs& e);

    // Öppna annat projekt: väljer en projektfil med filväljaren.
    // Projektet öppnas sedan i bakgrunden med skrivrätt.
    void browseBtnClicked(glaze::Widget* sender, glaze::ClickArgs& e);

    // Öppna: öppnar det valda projektet i listan.
    // Knappen är aktiv bara när en rad är vald.
    void openBtnClicked(glaze::Widget* sender, glaze::ClickArgs& e);

    // Ta bort ur listan: tar bara bort raden i systemdatabasen.
    // Projektfilen rörs aldrig.
    void removeBtnClicked(glaze::Widget* sender, glaze::ClickArgs& e);

    // Valet i listan ändrades.
    // Knapparnas läge uppdateras.
    void recentListChanged(glaze::Widget* sender, glaze::ChangeArgs& e);

    // Dubbelklick eller Enter på en rad.
    // Öppnar projektet om fönstret inte är upptaget.
    void recentListActivated(glaze::Widget* sender, glaze::ActivateArgs& e);

    // Visar den dolda väljaren om öppningen drar ut på tiden.
    // Timern är en engångstimer som startas vid direktöppning.
    void showTimerTick(glaze::Timer* sender, glaze::TimerArgs& e);

    // Läser senaste projekten ur systemdatabasen och fyller listan.
    // Startar sedan filkontrollen i bakgrunden.
    void fillRecent();

    // Kontrollerar i bakgrunden vilka filer som finns, saknas eller inte går att nå.
    // Svaret sätts i listans statuskolumn.
    void startProbe();

    // Öppnar ett projekt i bakgrunden med det angivna åtkomstläget.
    // Statusraden visar förloppet medan filen kopieras in.
    void openProject(const std::string& path, kalkyl::projectdb::Access access);

    // Skapar ett projekt i bakgrunden.
    // Projektets namn är filens stam.
    void createProject(const std::string& path);

    // Tar emot resultatet av öppning eller skapande, på UI-tråden.
    // Lyckat: projektet sparas och fönstret stängs. Annars frågar eller visar felet.
    void finishOpen(const std::string& path,
                    kalkyl::core::Result<kalkyl::projectdb::ProjectDb>& r, bool creating);

    // Låser eller låser upp fönstret medan ett jobb pågår.
    // Texten visas i statusraden, och väntepekaren visas.
    void setBusy(bool busy, const std::string& text);

    // Aktiverar Öppna och Ta bort bara när en rad är vald.
    // Båda är inaktiva medan fönstret är upptaget.
    void updateButtons();

    // Sökvägen för den valda raden.
    // Tom sträng om ingen rad är vald.
    std::string selectedPath() const;

    kalkyl::systemdb::SystemDb* sys_ = nullptr;
    std::vector<kalkyl::systemdb::RecentProject> recent_;
    std::optional<std::string> autoOpen_;
    std::optional<kalkyl::projectdb::ProjectDb> project_;
    bool busy_ = false;
    std::string statusIdle_;
    glaze::FileDialog openDlg_{glaze::FileDialogMode::Open};
    glaze::FileDialog newDlg_{glaze::FileDialogMode::Save};

    // Workers SIST: de rivs först och väntar ut sina jobb innan fälten ovan rivs.
    // probeWorker_ kontrollerar filerna, openWorker_ öppnar och skapar projekt.
    glaze::threads::Worker probeWorker_;
    glaze::threads::Worker openWorker_;
};
