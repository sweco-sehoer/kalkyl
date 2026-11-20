#pragma once

#include <glaze/glaze.h>

#include "systemdb/system_db.h"

// Programmets utseende: Slate med temat Windows och läget System/Mörkt/Ljust.
// Båda toppfönstren (väljaren och huvudfönstret) använder samma funktioner,
// så att utseendet och lägesknappen beter sig likadant överallt.
namespace kalkyl::appearance {

// Översätter läget mellan systemdatabasens typ och GlazeUI:s typ.
// Systemdatabasen beror inte på glaze, därför finns två enum.
glaze::ThemeMode toGlaze(systemdb::ThemeMode mode);
systemdb::ThemeMode fromGlaze(glaze::ThemeMode mode);

// Sätter Slate, temat Windows och läget ur systemdatabasen.
// Anropas i varje toppfönsters onLoad, alltså innan fönstret syns.
// Utseendet är app-globalt, så det är ofarligt att sätta det flera gånger.
void apply(glaze::AppWindow& window, systemdb::SystemDb& sys);

// Lägesknappens klick: System, Mörkt, Ljust och tillbaka till System.
// Läget sätts app-globalt, knappen uppdateras och läget sparas.
// Ett fel vid sparandet loggas med gfx::log.
void cycle(glaze::AppWindow& window, glaze::ToolButton& button, systemdb::SystemDb& sys);

// Visar läget på knappen: glyf och verktygstips (Utseende: System/Mörkt/Ljust).
// Anropas i onLoad och efter varje klick på knappen.
void syncButton(glaze::ToolButton& button, glaze::ThemeMode mode);

} // namespace kalkyl::appearance
