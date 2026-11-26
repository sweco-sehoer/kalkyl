#include "about_dialog.h"

#include <string>

#include "version/version.h"

// Startlogik, körs när fönstret laddas (aldrig i konstruktorn). Versionen kommer ur
// project(VERSION) i rotens CMakeLists.txt via version.h.
void AboutDialog::onLoad(glaze::Widget*, glaze::EventArgs&)
{
    versionLbl.setText(std::string("Version ") + KALKYL_VERSION);
}
