#include "project_picker.h"

#include "appearance/appearance.h"
#include "platform/files.h"
#include "platform/system.h"
#include "version/version.h"

#include <chrono>
#include <utility>

using kalkyl::core::ErrorCode;
using kalkyl::projectdb::Access;
using kalkyl::projectdb::ProjectDb;

namespace {

// Texten i statuskolumnen för resultatet av filkontrollen.
// En fil som finns ger tom text, så att listan är tyst när allt står rätt till.
std::string statusText(kalkyl::platform::PathState state)
{
    switch (state) {
    case kalkyl::platform::PathState::Exists:      return "";
    case kalkyl::platform::PathState::Missing:     return "Saknas";
    case kalkyl::platform::PathState::Unreachable: return "Ej nåbar";
    case kalkyl::platform::PathState::NoAccess:    return "Ingen behörighet";
    }
    return "";
}

// Felmeddelandet följt av en tom rad och en fråga.
// Används i de Ja/Nej-rutor som följer på ett fel.
std::string withQuestion(const std::string& message, const std::string& question)
{
    return message + std::string(2, '\n') + question;
}

// Texten på en rad, för statusraden: varje radbrytning blir ett blanksteg.
// Felmeddelanden har radbrytning före sökvägen.
std::string singleLine(std::string text)
{
    for (char& c : text)
        if (c == '\n' || c == '\r')
            c = ' ';
    return text;
}

} // namespace

// Anger systemdatabasen. Pekaren används tills fönstret rivs.
// Databasen ägs av main och lever längre än fönstret.
void ProjectPicker::setSystemDb(kalkyl::systemdb::SystemDb& sys)
{
    sys_ = &sys;
}

// Anger projektfilen som onLoad öppnar direkt.
// Används när programmet startas med en projektfil som argument.
void ProjectPicker::setAutoOpen(const std::string& path)
{
    autoOpen_ = path;
}

// Lämnar över det öppnade projektet till main. Efteråt är projektet tomt här.
// Ett tomt svar betyder att väljaren stängdes utan val.
std::optional<ProjectDb> ProjectPicker::takeProject()
{
    std::optional<ProjectDb> result = std::move(project_);
    project_.reset();
    return result;
}

// Startlogik, körs när fönstret laddas men innan det syns (aldrig i konstruktorn).
// Sätter utseendet, titeln och statusraden, fyller listan och startar ev. direktöppning.
void ProjectPicker::onLoad(glaze::Widget*, glaze::EventArgs&)
{
    kalkyl::appearance::apply(*this, *sys_);
    kalkyl::appearance::syncButton(themeBtn, themeMode());
    setTitle(std::string("Kalkyl ") + KALKYL_VERSION);

    if (!sys_->isPersistent())
        statusIdle_ = "Inställningar sparas inte: " + singleLine(sys_->fallbackReason());
    statusBar.setPanelText(0, statusIdle_);

    fillRecent();
    updateButtons();

    if (autoOpen_) {
        openProject(*autoOpen_, Access::ReadWrite);
        showTimer.start();
    }
}

// Lägesknappen i namnlisten: System, Mörkt, Ljust.
// Logiken delas med huvudfönstret via kalkyl::appearance.
void ProjectPicker::themeBtnClicked(glaze::Widget*, glaze::ClickArgs&)
{
    kalkyl::appearance::cycle(*this, themeBtn, *sys_);
}

// Nytt projekt: filväljaren i sparläge föreslår namn och mapp.
// Frågan om ersättning är avstängd, eftersom create ger AlreadyExists för en befintlig fil.
void ProjectPicker::newBtnClicked(glaze::Widget*, glaze::ClickArgs&)
{
    std::string folder = sys_->lastFolder();
    if (folder.empty())
        folder = kalkyl::platform::documentsDir();

    newDlg_.setTitle("Nytt projekt");
    newDlg_.setFilters({{"Kalkylprojekt", "*.kalkyl"}});
    newDlg_.setOverwritePrompt(false);
    newDlg_.setFolder(folder);
    newDlg_.setFileName("Nytt projekt");
    if (newDlg_.run(*this) == glaze::DialogResult::OK)
        createProject(kalkyl::projectdb::withExtension(newDlg_.path()));
}

// Öppna annat projekt: filväljaren i öppningsläge, filtrerad på .kalkyl.
// Startmappen är den senast använda, annars Dokument.
void ProjectPicker::browseBtnClicked(glaze::Widget*, glaze::ClickArgs&)
{
    std::string folder = sys_->lastFolder();
    if (folder.empty())
        folder = kalkyl::platform::documentsDir();

    openDlg_.setFilters({{"Kalkylprojekt", "*.kalkyl"}});
    openDlg_.setFolder(folder);
    if (openDlg_.run(*this) == glaze::DialogResult::OK)
        openProject(openDlg_.path(), Access::ReadWrite);
}

// Öppna: öppnar den valda raden med skrivrätt.
// Är projektet låst frågar finishOpen om skrivskyddat läge.
void ProjectPicker::openBtnClicked(glaze::Widget*, glaze::ClickArgs&)
{
    const std::string path = selectedPath();
    if (!path.empty())
        openProject(path, Access::ReadWrite);
}

// Ta bort ur listan: tar bara bort raden i systemdatabasen.
// Projektfilen rörs aldrig.
void ProjectPicker::removeBtnClicked(glaze::Widget*, glaze::ClickArgs&)
{
    const std::string path = selectedPath();
    if (path.empty())
        return;
    const auto removed = sys_->removeRecentProject(path);
    if (!removed)
        gfx::log("Raden kunde inte tas bort: %s", removed.error().detail.c_str());
    fillRecent();
    updateButtons();
}

// Valet ändrades: Öppna och Ta bort aktiveras eller inaktiveras.
// Knapparna är aktiva bara när en rad är vald.
void ProjectPicker::recentListChanged(glaze::Widget*, glaze::ChangeArgs&)
{
    updateButtons();
}

// Dubbelklick eller Enter på en rad öppnar projektet.
// Ett upptaget fönster ignorerar aktiveringen.
void ProjectPicker::recentListActivated(glaze::Widget*, glaze::ActivateArgs&)
{
    if (busy_)
        return;
    const std::string path = selectedPath();
    if (!path.empty())
        openProject(path, Access::ReadWrite);
}

// Tidsgränsen för direktöppning gick ut.
// Väljaren visas då med "Öppnar…" i statusraden.
void ProjectPicker::showTimerTick(glaze::Timer*, glaze::TimerArgs&)
{
    if (busy_ && isHidden())
        show();
}

// Läser senaste projekten ur systemdatabasen och fyller listan.
// Statuskolumnen står på "Kontrollerar…" tills bakgrundskontrollen svarat.
void ProjectPicker::fillRecent()
{
    recent_.clear();
    auto recent = sys_->recentProjects();
    if (recent)
        recent_ = std::move(recent).value();
    else
        gfx::log("Senaste projekt kunde inte läsas: %s", recent.error().detail.c_str());

    std::vector<glaze::ListView::Row> rows;
    for (const auto& item : recent_) {
        glaze::ListView::Row row;
        row.cells = {item.name, kalkyl::platform::parentDir(item.path),
                     kalkyl::platform::localTimeText(item.lastOpenedUtc), "Kontrollerar…"};
        rows.push_back(std::move(row));
    }
    recentList.setRows(std::move(rows));

    if (recent_.empty()) {
        recentList.hide();
        emptyLbl.show();
    } else {
        emptyLbl.hide();
        recentList.show();
    }
    startProbe();
}

// Kontrollerar alla sökvägar i bakgrunden med en gemensam tidsgräns på 2 s.
// Resultatet matchas mot listan på pathKey, eftersom listan kan ha ändrats under tiden.
void ProjectPicker::startProbe()
{
    probeWorker_.cancel();
    std::vector<std::string> paths;
    for (const auto& item : recent_)
        paths.push_back(item.path);
    if (paths.empty())
        return;

    probeWorker_.run(
        [paths](std::stop_token) {
            return kalkyl::platform::probeMany(paths, std::chrono::milliseconds(2000));
        },
        [this, paths](glaze::threads::Result<std::vector<kalkyl::platform::PathState>>& r) {
            if (!r)
                return;
            const auto& states = *r;
            for (std::size_t i = 0; i < paths.size() && i < states.size(); ++i) {
                const std::string key = kalkyl::platform::pathKey(paths[i]);
                for (std::size_t row = 0; row < recent_.size(); ++row)
                    if (kalkyl::platform::pathKey(recent_[row].path) == key)
                        recentList.setCell(static_cast<int>(row), 3, statusText(states[i]));
            }
        });
}

// Öppnar ett projekt i bakgrunden. Arbetet fångar bara värden och postar
// förloppet som statustext till UI-tråden, bara när procenttalet ändras.
void ProjectPicker::openProject(const std::string& path, Access access)
{
    setBusy(true, "Öppnar " + kalkyl::platform::fileName(path) + "…");
    const std::string workDir = kalkyl::projectdb::defaultWorkDir();
    const std::string name = kalkyl::platform::fileName(path);

    openWorker_.cancel();
    openWorker_.run(
        [this, path, access, workDir, name](std::stop_token stop) {
            kalkyl::projectdb::OpenOptions options;
            options.access = access;
            options.workDir = workDir;
            options.stop = stop;
            options.progress = [this, stop, name, last = -1](std::uint64_t done,
                                                             std::uint64_t total) mutable {
                if (total == 0)
                    return;
                const int percent = static_cast<int>(done * 100 / total);
                if (percent == last)
                    return;
                last = percent;
                const std::string text = "Öppnar " + name + "… " + std::to_string(percent) + " %";
                openWorker_.post(stop, [this, text] { statusBar.setPanelText(0, text); });
            };
            return ProjectDb::open(path, options);
        },
        [this, path](glaze::threads::Result<kalkyl::core::Result<ProjectDb>>& r) {
            if (!r) {
                if (r.cancelled())
                    return;
                setBusy(false, "");
                if (isHidden())
                    show();
                glaze::messageBox(*this, "Oväntat fel: " + r.error(), "Kunde inte öppna projektet",
                                  glaze::MessageBoxButtons::OK, glaze::MessageBoxType::Error);
                return;
            }
            finishOpen(path, *r, false);
        });
}

// Skapar ett projekt i bakgrunden. Namnet är filens stam (utan .kalkyl).
// Resultatet tas emot av finishOpen, som vid en öppning.
void ProjectPicker::createProject(const std::string& path)
{
    setBusy(true, "Skapar " + kalkyl::platform::fileName(path) + "…");
    const std::string workDir = kalkyl::projectdb::defaultWorkDir();
    const std::string name = kalkyl::platform::fileStem(path);

    openWorker_.cancel();
    openWorker_.run(
        [path, name, workDir](std::stop_token stop) {
            kalkyl::projectdb::OpenOptions options;
            options.workDir = workDir;
            options.stop = stop;
            return ProjectDb::create(path, name, options);
        },
        [this, path](glaze::threads::Result<kalkyl::core::Result<ProjectDb>>& r) {
            if (!r) {
                if (r.cancelled())
                    return;
                setBusy(false, "");
                if (isHidden())
                    show();
                glaze::messageBox(*this, "Oväntat fel: " + r.error(), "Kunde inte skapa projektet",
                                  glaze::MessageBoxButtons::OK, glaze::MessageBoxType::Error);
                return;
            }
            finishOpen(path, *r, true);
        });
}

// Tar emot resultatet på UI-tråden. Lyckat: projektet sparas och fönstret stängs.
// Annars visas väljaren om den var dold, och felet ger fråga eller meddelande efter felkoden.
void ProjectPicker::finishOpen(const std::string& path,
                               kalkyl::core::Result<ProjectDb>& r, bool creating)
{
    if (r.ok()) {
        project_ = std::move(r).value();
        showTimer.stop();
        close();
        return;
    }

    const kalkyl::core::Error error = r.error();
    showTimer.stop();
    if (isHidden())
        show();
    setBusy(false, "");
    if (error.code == ErrorCode::Cancelled)
        return;
    gfx::log("Öppning misslyckades (%d): %s", static_cast<int>(error.code), error.detail.c_str());

    // Låst av någon annan, eller ingen skrivrätt i mappen: projektet kan ändå läsas.
    if ((error.code == ErrorCode::Locked || error.code == ErrorCode::AccessDenied) && !creating) {
        const auto answer = glaze::messageBox(
            *this, withQuestion(error.message, "Vill du öppna projektet skrivskyddat?"),
            error.code == ErrorCode::Locked ? "Projektet används" : "Skrivrätt saknas",
            glaze::MessageBoxButtons::YesNo, glaze::MessageBoxType::Warning);
        if (answer == glaze::DialogResult::Yes)
            openProject(path, Access::ReadOnly);
        return;
    }

    if (error.code == ErrorCode::NotFound && !creating) {
        const std::string key = kalkyl::platform::pathKey(path);
        bool inList = false;
        for (const auto& item : recent_)
            inList = inList || kalkyl::platform::pathKey(item.path) == key;
        if (inList) {
            const auto answer = glaze::messageBox(
                *this, withQuestion(error.message, "Vill du ta bort projektet ur listan?"),
                "Projektet finns inte", glaze::MessageBoxButtons::YesNo,
                glaze::MessageBoxType::Warning);
            if (answer == glaze::DialogResult::Yes) {
                const auto removed = sys_->removeRecentProject(path);
                if (!removed)
                    gfx::log("Raden kunde inte tas bort: %s", removed.error().detail.c_str());
                fillRecent();
                updateButtons();
            }
            return;
        }
    }

    glaze::messageBox(*this, error.message,
                      creating ? "Kunde inte skapa projektet" : "Kunde inte öppna projektet",
                      glaze::MessageBoxButtons::OK, glaze::MessageBoxType::Error);
}

// Låser eller låser upp fönstret medan ett jobb pågår: lista och knappar,
// väntepekare och statusrad. Lägesknappen förblir aktiv. När jobbet är klart
// återställs statusradens vanliga text (varningen om systemdatabasen).
void ProjectPicker::setBusy(bool busy, const std::string& text)
{
    busy_ = busy;
    setUseWaitCursor(busy);
    if (busy) {
        recentList.disable();
        newBtn.disable();
        browseBtn.disable();
    } else {
        recentList.enable();
        newBtn.enable();
        browseBtn.enable();
    }
    statusBar.setPanelText(0, busy ? text : statusIdle_);
    updateButtons();
}

// Öppna och Ta bort är aktiva bara när en rad är vald.
// Båda är inaktiva medan fönstret är upptaget.
void ProjectPicker::updateButtons()
{
    const bool enabled = !busy_ && !selectedPath().empty();
    if (enabled) {
        openBtn.enable();
        removeBtn.enable();
    } else {
        openBtn.disable();
        removeBtn.disable();
    }
}

// Sökvägen för den valda raden. Radindexet är appens, alltså samma som i recent_.
// Tom sträng om ingen rad är vald.
std::string ProjectPicker::selectedPath() const
{
    const int index = recentList.selectedIndex();
    if (index < 0 || index >= static_cast<int>(recent_.size()))
        return "";
    return recent_[static_cast<std::size_t>(index)].path;
}
