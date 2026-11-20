#include "appearance/appearance.h"

namespace kalkyl::appearance {

// Översättning systemdatabasens läge till GlazeUI:s läge.
// Okända värden ger System.
glaze::ThemeMode toGlaze(systemdb::ThemeMode mode)
{
    switch (mode) {
    case systemdb::ThemeMode::Light: return glaze::ThemeMode::Light;
    case systemdb::ThemeMode::Dark:  return glaze::ThemeMode::Dark;
    case systemdb::ThemeMode::System: break;
    }
    return glaze::ThemeMode::System;
}

// Översättning GlazeUI:s läge till systemdatabasens läge.
// Okända värden ger System.
systemdb::ThemeMode fromGlaze(glaze::ThemeMode mode)
{
    switch (mode) {
    case glaze::ThemeMode::Light: return systemdb::ThemeMode::Light;
    case glaze::ThemeMode::Dark:  return systemdb::ThemeMode::Dark;
    case glaze::ThemeMode::System: break;
    }
    return systemdb::ThemeMode::System;
}

// Sätter utseendet: setUI återställer tema och stil, därför sätts temat efteråt.
// Windows är Slates förvalstema och följer OS:ets ljusa och mörka läge.
void apply(glaze::AppWindow& window, systemdb::SystemDb& sys)
{
    window.setUI(glaze::UI::Slate);
    window.setTheme(glaze::defaultTheme(glaze::UI::Slate));
    window.setThemeMode(toGlaze(sys.themeMode()));
}

// Lägesknappens klick: nästa läge i raden System, Mörkt, Ljust.
// Läget sätts först i GlazeUI, så att fönstret ritas om direkt, och sparas sedan.
void cycle(glaze::AppWindow& window, glaze::ToolButton& button, systemdb::SystemDb& sys)
{
    glaze::ThemeMode next = glaze::ThemeMode::System;
    switch (window.themeMode()) {
    case glaze::ThemeMode::System: next = glaze::ThemeMode::Dark;   break;
    case glaze::ThemeMode::Dark:   next = glaze::ThemeMode::Light;  break;
    case glaze::ThemeMode::Light:  next = glaze::ThemeMode::System; break;
    }
    window.setThemeMode(next);
    syncButton(button, next);

    const auto saved = sys.setThemeMode(fromGlaze(next));
    if (!saved)
        gfx::log("Utseendet kunde inte sparas: %s", saved.error().detail.c_str());
}

// Visar läget på knappen med GlazeUI:s lägesglyfer.
// Verktygstipset säger vilket läge som gäller.
void syncButton(glaze::ToolButton& button, glaze::ThemeMode mode)
{
    switch (mode) {
    case glaze::ThemeMode::System:
        button.setImg(glaze::stockIcon(glaze::StockIcon::ThemeSystem));
        button.setToolTip("Utseende: System");
        break;
    case glaze::ThemeMode::Dark:
        button.setImg(glaze::stockIcon(glaze::StockIcon::ThemeDark));
        button.setToolTip("Utseende: Mörkt");
        break;
    case glaze::ThemeMode::Light:
        button.setImg(glaze::stockIcon(glaze::StockIcon::ThemeLight));
        button.setToolTip("Utseende: Ljust");
        break;
    }
}

} // namespace kalkyl::appearance
