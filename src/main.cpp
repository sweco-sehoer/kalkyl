#include <glaze/glaze.h>

#include <chrono>
#include <future>
#include <memory>
#include <optional>
#include <string>
#include <thread>
#include <vector>

#include "platform/association.h"
#include "platform/files.h"
#include "platform/system.h"
#include "projectdb/project_db.h"
#include "systemdb/system_db.h"
#include "windows/main_window.h"
#include "windows/project_picker.h"

namespace {

// Filtypen som Kalkyl äger: projektfilerna med ändelsen .kalkyl.
// Används av både --registrera och den automatiska registreringen i Release.
const kalkyl::platform::FileType kProjectType{".kalkyl", "Kalkyl.Projekt", "Kalkylprojekt"};

// Det programargumenten betyder: en flagga för filassociationen
// eller en projektfil som ska öppnas direkt.
struct Arguments {
    bool registerAssociation = false;
    bool unregisterAssociation = false;
    std::optional<std::string> projectFile;
};

// Tolkar argumenten (UTF-8, [0] är programmet). Första argument som inte
// börjar med "--" är projektfilen och görs om till en absolut sökväg.
Arguments parseArguments(const std::vector<std::string>& args)
{
    Arguments result;
    for (std::size_t i = 1; i < args.size(); ++i) {
        const std::string& arg = args[i];
        if (arg == "--registrera")
            result.registerAssociation = true;
        else if (arg == "--avregistrera")
            result.unregisterAssociation = true;
        else if (arg.rfind("--", 0) != 0 && !result.projectFile)
            result.projectFile = kalkyl::platform::absolutePath(arg);
    }
    return result;
}

// Registrerar eller avregistrerar filassociationen och svarar med exit-kod 0 eller 1.
// Inget fönster visas; ett fel loggas.
int runAssociationCommand(bool doRegister)
{
    const auto result = doRegister
        ? kalkyl::platform::registerAssociation(kProjectType, kalkyl::platform::executablePath())
        : kalkyl::platform::unregisterAssociation(kProjectType, kalkyl::platform::executablePath());
    if (!result)
        gfx::log("Filassociationen misslyckades: %s", result.error().detail.c_str());
    return result ? 0 : 1;
}

// Öppnar systemdatabasen. Går det inte används en reserv i minnet, där allt
// fungerar men inget sparas. Misslyckas även den blir svaret tomt.
std::optional<kalkyl::systemdb::SystemDb> openSystemDb()
{
    auto opened = kalkyl::systemdb::SystemDb::open(kalkyl::systemdb::defaultPath());
    if (opened)
        return std::move(opened).value();

    gfx::log("Systemdatabasen kunde inte öppnas: %s", opened.error().detail.c_str());
    auto fallback = kalkyl::systemdb::SystemDb::openInMemory(opened.error().message);
    if (fallback)
        return std::move(fallback).value();
    return std::nullopt;
}

// Stänger projektet före programslut och väntar högst timeout på att låset släppts.
// Stängningen går i en frikopplad tråd, så att en share som inte svarar aldrig hindrar
// programmet från att avsluta. Hinner den inte klart tar Windows bort låsfilen när
// processen dör (FILE_FLAG_DELETE_ON_CLOSE).
void closeBeforeExit(kalkyl::projectdb::ProjectDb project, std::chrono::milliseconds timeout)
{
    auto done = std::make_shared<std::promise<void>>();
    std::future<void> finished = done->get_future();
    std::thread([p = std::move(project), done]() mutable {
        p.close(kalkyl::projectdb::ProjectDb::CloseMode::Wait);
        done->set_value();
    }).detach();
    finished.wait_for(timeout);
}

} // namespace

// Programmets ingång. Läser argumenten, öppnar systemdatabasen och städar
// arbetskopiorna. Sedan växlar programmet mellan projektväljaren och huvudfönstret
// tills användaren avslutar.
int main()
{
    const Arguments args = parseArguments(kalkyl::platform::commandLineArgs());
    if (args.registerAssociation)
        return runAssociationCommand(true);
    if (args.unregisterAssociation)
        return runAssociationCommand(false);

    // Ikonen i assets.bin måste vara inläst före det första fönstret.
    glaze::loadAssets();

    std::optional<kalkyl::systemdb::SystemDb> sysHolder = openSystemDb();
    if (!sysHolder) {
        glaze::messageBox("Systemdatabasen kunde inte öppnas, inte ens i minnet.", "Kalkyl",
                          glaze::MessageBoxButtons::OK, glaze::MessageBoxType::Error);
        return 1;
    }
    kalkyl::systemdb::SystemDb& sys = *sysHolder;

    // Städar arbetskopior som en tidigare körning lämnat kvar.
    const std::string workDir = kalkyl::projectdb::defaultWorkDir();
    const auto made = kalkyl::platform::ensureDirectory(workDir);
    if (!made)
        gfx::log("Arbetsmappen kunde inte skapas: %s", made.error().detail.c_str());
    kalkyl::projectdb::cleanWorkDir(workDir);

#ifdef NDEBUG
    // Release registrerar filtypen själv om den saknas eller pekar på en exe som inte finns.
    // Debug gör det aldrig, så att utvecklingsbyggen inte tar över associationen.
    const std::string exe = kalkyl::platform::executablePath();
    const auto state = kalkyl::platform::associationState(kProjectType, exe);
    if (state == kalkyl::platform::AssociationState::Missing ||
        state == kalkyl::platform::AssociationState::Dead) {
        const auto registered = kalkyl::platform::registerAssociation(kProjectType, exe);
        if (!registered)
            gfx::log("Filassociationen kunde inte registreras: %s",
                     registered.error().detail.c_str());
    }
#endif

    std::optional<std::string> pending = args.projectFile;
    while (true) {
        // Väljaren körs tills den stängs. En dubbelklickad fil öppnas direkt
        // av dold väljare, som visas först om öppningen drar ut på tiden eller misslyckas.
        std::optional<kalkyl::projectdb::ProjectDb> project;
        {
            ProjectPicker picker;
            picker.setSystemDb(sys);
            if (pending) {
                picker.setAutoOpen(*pending);
                picker.hide();
                pending.reset();
            }
            picker.run();
            project = picker.takeProject();
        }
        if (!project)
            return 0;

        // Projektet läggs först i listan över senaste projekt.
        const auto touched = sys.touchRecentProject(project->path(), project->info().name);
        if (!touched)
            gfx::log("Senaste projekt kunde inte uppdateras: %s", touched.error().detail.c_str());
        const auto folder = sys.setLastFolder(kalkyl::platform::parentDir(project->path()));
        if (!folder)
            gfx::log("Senaste mapp kunde inte sparas: %s", folder.error().detail.c_str());

        MainWindow win;
        win.setContext(sys, *project);
        win.run();
        if (win.outcome() == MainWindow::Outcome::Exit) {
            closeBeforeExit(std::move(*project), std::chrono::seconds(2));
            return 0;
        }
        project->close(kalkyl::projectdb::ProjectDb::CloseMode::Background);
    }
}
