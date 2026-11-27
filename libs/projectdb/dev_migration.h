#pragma once
#include <string_view>

// Utvecklingsmigreringen för projektdatabasen: stegen efter v0.1.0 (steg 1 i project_db.cpp).
// Varje commit som ändrar schemat får ett nytt steg sist; ett committat steg ändras aldrig.
// Vid 1.0.0 slås alla steg ihop till ett nytt steg 1, och den här filen töms.

namespace kalkyl::projectdb::dev {

// Steg 2 (user_version 1 -> 2): linkopiorna, Dokumentationens värden, normalspannsberäkningarna
// och kopplingarna. IF NOT EXISTS gör att steget tål en fil som redan har tabellerna.
// Tabellerna är inte STRICT, så att äldre SQLite-verktyg också kan läsa filen.
inline constexpr std::string_view kStep2 = R"sql(
-- Linorna som projektet använder: kopior av linregistrets linor (Lindata B-N), unika på namn.
-- Inga CHECK på värdena: reglerna ligger i conductor::validateConductor.
CREATE TABLE IF NOT EXISTS project_conductor (
    id                    INTEGER PRIMARY KEY AUTOINCREMENT,
    name                  TEXT NOT NULL COLLATE NOCASE UNIQUE,
    type                  TEXT NOT NULL DEFAULT '',
    area_mm2              REAL NOT NULL,
    diameter_mm           REAL NOT NULL,
    e_initial_mpa         REAL NOT NULL,
    e_permanent_mpa       REAL NOT NULL,
    e_initial_limit_mpa   REAL NOT NULL,
    eiv_constant          REAL NOT NULL,
    alpha_e6_per_c        REAL NOT NULL,
    creep_strain_permille REAL NOT NULL,
    creep_temp_c          REAL NOT NULL,
    weight_n_per_m        REAL NOT NULL,
    breaking_load_kn      REAL,
    copied_utc            TEXT NOT NULL
);
-- Fliken Dokumentation: en rad per uppgift med värde eller referens.
-- value är kanonisk text: tal med punkt, val som text, linans namn för linraderna.
CREATE TABLE IF NOT EXISTS doc_value (
    key       TEXT PRIMARY KEY NOT NULL,
    value     TEXT NOT NULL DEFAULT '',
    reference TEXT NOT NULL DEFAULT ''
);
-- Normalspannsberäkningarna. De beror bara på stolparna, inte på någon lina.
-- Gränserna är kontrollerna 1-5 (Excel R16, R19, R22, R26, R30).
CREATE TABLE IF NOT EXISTS normal_span (
    id              INTEGER PRIMARY KEY AUTOINCREMENT,
    note           TEXT NOT NULL DEFAULT '',
    limit_longest   REAL NOT NULL DEFAULT 1.5,
    limit_shortest  REAL NOT NULL DEFAULT 0.5,
    limit_adjacent  REAL NOT NULL DEFAULT 2.5,
    limit_local_min REAL NOT NULL DEFAULT 0.95,
    limit_local_max REAL NOT NULL DEFAULT 1.15,
    created_utc     TEXT NOT NULL,
    modified_utc    TEXT NOT NULL
);
-- Stolparna i tabellens ordning. length_m är spannet före stolpen; NULL = tom cell.
CREATE TABLE IF NOT EXISTS normal_span_pole (
    span_id  INTEGER NOT NULL REFERENCES normal_span(id) ON DELETE CASCADE,
    position INTEGER NOT NULL,
    pole     TEXT NOT NULL DEFAULT '',
    length_m REAL,
    height_m REAL,
    PRIMARY KEY (span_id, position)
);
-- Vilken beräkning en rad i Dokumentation visar: högst en per rad, en beräkning på flera rader.
CREATE TABLE IF NOT EXISTS doc_normal_span (
    doc_key TEXT PRIMARY KEY NOT NULL,
    span_id INTEGER NOT NULL REFERENCES normal_span(id) ON DELETE CASCADE
);
CREATE INDEX IF NOT EXISTS doc_normal_span_span ON doc_normal_span(span_id);
)sql";

// Steg 3 (user_version 2 -> 3): linberäkningarna med sina 15 lastfall och kopplingen till
// raderna "Linberäkning ..." i Dokumentation. IF NOT EXISTS som i steg 2.
inline constexpr std::string_view kStep3 = R"sql(
-- Linberäkningarna enligt arket Linberäkning. doc_source NULL = manuell, med lindatat som egen
-- kopia (cond_name NULL = inget lindata, '' = lindata utan namn). Annars nyckeln för raden som
-- linan, an, sigma0, min och max hämtas ur; då är cond_*, normal_span_m och tension_mpa NULL.
CREATE TABLE IF NOT EXISTS line_calc (
    id                         INTEGER PRIMARY KEY AUTOINCREMENT,
    note                       TEXT NOT NULL DEFAULT '',
    mode                       TEXT NOT NULL DEFAULT 'new',
    doc_source                 TEXT,
    cond_name                  TEXT,
    cond_type                  TEXT NOT NULL DEFAULT '',
    cond_area_mm2              REAL,
    cond_diameter_mm           REAL,
    cond_e_initial_mpa         REAL,
    cond_e_permanent_mpa       REAL,
    cond_e_initial_limit_mpa   REAL,
    cond_eiv_constant          REAL,
    cond_alpha_e6_per_c        REAL,
    cond_creep_strain_permille REAL,
    cond_creep_temp_c          REAL,
    cond_weight_n_per_m        REAL,
    cond_breaking_load_kn      REAL,
    normal_span_m              REAL,
    tension_mpa                REAL,
    ref_temp_c                 REAL NOT NULL DEFAULT 0,
    min_temp_c                 REAL NOT NULL DEFAULT -50,
    max_temp_c                 REAL NOT NULL DEFAULT 50,
    short_circuit_temp_c       REAL NOT NULL DEFAULT 100,
    line_height_m              REAL NOT NULL DEFAULT 25,
    normal_wind_n_per_m2       REAL NOT NULL DEFAULT 500,
    high_wind_n_per_m2         REAL NOT NULL DEFAULT 800,
    ice_density_kg_m3          REAL NOT NULL DEFAULT 917.4,
    pi                         REAL NOT NULL DEFAULT 3.1416,
    gravity                    REAL NOT NULL DEFAULT 9.81,
    gust_factor                REAL NOT NULL DEFAULT 1,
    resonance_factor           REAL NOT NULL DEFAULT 0.5,
    drag_factor                REAL NOT NULL DEFAULT 1,
    min_ice_n_per_m            REAL NOT NULL DEFAULT 20,
    max_utilization_percent    REAL NOT NULL DEFAULT 55,
    transposition_case         INTEGER NOT NULL DEFAULT 5,
    created_utc                TEXT NOT NULL,
    modified_utc               TEXT NOT NULL
);
-- Lastfallen 1-15 (arkets rad 23-29 och 31-38). Källorna som text. manual_* båda NULL = ingen
-- manuell last, båda satta = I och Qwc inskrivna för hand.
CREATE TABLE IF NOT EXISTS line_calc_case (
    calc_id                    INTEGER NOT NULL REFERENCES line_calc(id) ON DELETE CASCADE,
    case_id                    INTEGER NOT NULL,
    temp_source                TEXT NOT NULL DEFAULT 'fixed',
    temp_c                     REAL NOT NULL DEFAULT 0,
    name                       TEXT NOT NULL DEFAULT '',
    standard_case              TEXT NOT NULL DEFAULT '',
    clearance_case             TEXT NOT NULL DEFAULT '',
    wind_from_case             INTEGER,
    wind_magnitude_percent     REAL NOT NULL DEFAULT 0,
    pressure_source            TEXT NOT NULL DEFAULT 'fixed',
    char_pressure_n_per_m2     REAL NOT NULL DEFAULT 0,
    ref_height_m               REAL NOT NULL DEFAULT 0,
    pressure_gradient_n_per_m3 REAL NOT NULL DEFAULT 0,
    ice_thickness_mm           REAL NOT NULL DEFAULT 0,
    manual_ice_n_per_m         REAL,
    manual_wind_n_per_m        REAL,
    PRIMARY KEY (calc_id, case_id)
);
-- Vilken linberäkning en rad "Linberäkning ..." i Dokumentation visar: högst en per rad.
CREATE TABLE IF NOT EXISTS doc_line_calc (
    doc_key TEXT PRIMARY KEY NOT NULL,
    calc_id INTEGER NOT NULL REFERENCES line_calc(id) ON DELETE CASCADE
);
CREATE INDEX IF NOT EXISTS doc_line_calc_calc ON doc_line_calc(calc_id);
)sql";

// Steg 4 (user_version 3 -> 4): stolptabellen i fliken Stolptabell före. IF NOT EXISTS som i
// steg 2. Talen är TEXT, så att användarens decimaler bevaras exakt.
inline constexpr std::string_view kStep4 = R"sql(
-- Fliken Stolptabell före: en rad per stolpe i tabellens ordning (position 0, 1, 2 ...),
-- också tomma rader mitt i tabellen.
-- Talen är kanonisk decimaltext (punkt, inga tusentalsavgränsare, användarens decimaler kvar).
-- TEXT-affiniteten gör att SQLite aldrig gör om dem till tal. NULL = tom cell.
CREATE TABLE IF NOT EXISTS pole_before (
    position INTEGER PRIMARY KEY,
    pole     TEXT    NOT NULL DEFAULT '',
    av       TEXT,
    ah       TEXT,
    span_m   TEXT,
    coord_n  TEXT,
    coord_e  TEXT,
    coord_h  TEXT,
    lkl      TEXT    NOT NULL DEFAULT ''
);
)sql";

// Steg 5 (user_version 4 -> 5): Stolptabell efter, automatiskt läge och märkningen av de
// automatiska beräkningarna. IF NOT EXISTS som i steg 2. De automatiska beräkningarna är vanliga
// rader i normal_span och line_calc; auto_calc säger vilka och reserverar deras id.
inline constexpr std::string_view kStep5 = R"sql(
-- Fliken Stolptabell efter: som pole_before, en rad per stolpe i tabellens ordning
-- (position 0, 1, 2 ...), också tomma rader mitt i tabellen. Talen är kanonisk decimaltext.
-- NULL = tom cell.
CREATE TABLE IF NOT EXISTS pole_after (
    position INTEGER PRIMARY KEY,
    pole     TEXT    NOT NULL DEFAULT '',
    av       TEXT,
    ah       TEXT,
    span_m   TEXT,
    coord_n  TEXT,
    coord_e  TEXT,
    coord_h  TEXT,
    lkl      TEXT    NOT NULL DEFAULT ''
);
-- Automatiskt läge per stolptabell: på eller av, och var faslinan och topplinorna börjar och
-- slutar (Stolpplats, '' = inte vald). top_left är Topplinor när topplina höger är Samma.
-- Saknas raden är läget av och inga sträckor valda.
CREATE TABLE IF NOT EXISTS auto_mode (
    stage          TEXT    PRIMARY KEY NOT NULL CHECK (stage IN ('before', 'after')),
    enabled        INTEGER NOT NULL DEFAULT 0 CHECK (enabled IN (0, 1)),
    phase_from     TEXT    NOT NULL DEFAULT '',
    phase_to       TEXT    NOT NULL DEFAULT '',
    top_left_from  TEXT    NOT NULL DEFAULT '',
    top_left_to    TEXT    NOT NULL DEFAULT '',
    top_right_from TEXT    NOT NULL DEFAULT '',
    top_right_to   TEXT    NOT NULL DEFAULT ''
);
-- De automatiska beräkningarna: en rad per skede och lina som någon gång har varit automatisk.
-- span_id och calc_id är id i normal_span och line_calc. De är reserverade för skedet och linan:
-- raderna där finns bara när linan är automatisk och skapas igen med samma id. Ingen främmande
-- nyckel, eftersom raderna tas bort när läget är av. first_row, last_row och issue är sträckan
-- som den senast lästes ur stolptabellen (-1 och felets text när den inte går att använda).
CREATE TABLE IF NOT EXISTS auto_calc (
    stage     TEXT    NOT NULL CHECK (stage IN ('before', 'after')),
    wire      TEXT    NOT NULL CHECK (wire IN ('phase', 'top_left', 'top_right')),
    span_id   INTEGER NOT NULL UNIQUE,
    calc_id   INTEGER NOT NULL UNIQUE,
    first_row INTEGER NOT NULL DEFAULT -1,
    last_row  INTEGER NOT NULL DEFAULT -1,
    issue     TEXT    NOT NULL DEFAULT '',
    PRIMARY KEY (stage, wire)
);
)sql";

// Steg 6 (user_version 5 -> 6): importens rådata, de fyra tabellerna pole_import*. Ett eget steg,
// eftersom projekt med user_version 5 och bara tabellerna i steg 5 redan finns. IF NOT EXISTS som
// i steg 2.
inline constexpr std::string_view kStep6 = R"sql(
-- Den senaste importen av en stolptabell till Stolptabell före: en rad (id = 1) med filens namn
-- (inte sökvägen), bladet, titeln och versionen ("Ver. 6"), tidpunkten (UTC), rubrikområdet,
-- första dataraden och sträckan som importerades. Saknas raden har ingen import gjorts.
CREATE TABLE IF NOT EXISTS pole_import (
    id               INTEGER PRIMARY KEY CHECK (id = 1),
    file_name        TEXT    NOT NULL,
    sheet_name       TEXT    NOT NULL,
    title            TEXT    NOT NULL DEFAULT '',
    version          TEXT    NOT NULL DEFAULT '',
    imported_utc     TEXT    NOT NULL,
    header_first_row INTEGER NOT NULL,
    header_last_row  INTEGER NOT NULL,
    first_data_row   INTEGER NOT NULL,
    from_pole        TEXT    NOT NULL DEFAULT '',
    to_pole          TEXT    NOT NULL DEFAULT ''
);
-- Kolumnerna i den importerade filen: numret (1 = A), bokstäverna, rubriken (rubrikraderna
-- uppifrån med " › " emellan, sammanslagna celler ifyllda) och fältet i Stolptabell före som
-- kolumnen gav, med pole_before-kolumnens namn. NULL för kolumner som inte gav något fält.
CREATE TABLE IF NOT EXISTS pole_import_column (
    col    INTEGER PRIMARY KEY,
    letter TEXT    NOT NULL,
    header TEXT    NOT NULL DEFAULT '',
    field  TEXT    UNIQUE CHECK (field IN ('pole', 'coord_n', 'coord_e', 'coord_h', 'span_m', 'ah', 'av', 'lkl'))
);
-- De importerade raderna: Excels radnummer, platsen i Stolptabell före vid importen (0, 1, 2 ...)
-- och Stolpnr som det blev där.
CREATE TABLE IF NOT EXISTS pole_import_row (
    excel_row INTEGER PRIMARY KEY,
    position  INTEGER NOT NULL UNIQUE,
    pole      TEXT    NOT NULL
);
-- Cellerna på de importerade raderna, alla kolumner som har ett värde. text är värdet som det står
-- i filen: texten med radbrytningar, talet med punkt och alla decimaler, sant/falskt som 1/0 och
-- felet som #N/A. kind är cellens slag.
CREATE TABLE IF NOT EXISTS pole_import_cell (
    excel_row INTEGER NOT NULL REFERENCES pole_import_row(excel_row) ON DELETE CASCADE,
    col       INTEGER NOT NULL REFERENCES pole_import_column(col) ON DELETE CASCADE,
    kind      TEXT    NOT NULL CHECK (kind IN ('text', 'number', 'bool', 'error')),
    text      TEXT    NOT NULL,
    PRIMARY KEY (excel_row, col)
) WITHOUT ROWID;
)sql";

// Steg 7 (user_version 6 -> 7): Av per fas i Stolptabell efter. av är Av Fas1, och av2 och av3
// är Fas2 och Fas3. De befintliga raderna får Av i alla tre faserna, som vid en kopiering från
// Stolptabell före.
inline constexpr std::string_view kStep7 = R"sql(
ALTER TABLE pole_after ADD COLUMN av2 TEXT;
ALTER TABLE pole_after ADD COLUMN av3 TEXT;
UPDATE pole_after SET av2 = av, av3 = av;
)sql";

// Steg 8 (user_version 7 -> 8): radernas id i Stolptabell efter och linhöjderna i fliken
// Linhöjder. row_id är ett tal > 0, unikt i tabellen, som följer raden när rader infogas, raderas
// och ändras. De befintliga raderna får position + 1. ADD COLUMN får ingen CHECK, eftersom SQLite
// prövar den mot de befintliga raderna innan UPDATE har gett dem id.
inline constexpr std::string_view kStep8 = R"sql(
-- Radernas id i Stolptabell efter: ett tal > 0, unikt i tabellen, som följer raden när rader
-- infogas, raderas och ändras, och som linhöjderna hänger på. Det är inget fält i tabellen utan
-- skrivs av Kalkyl (poletable::assignRowIds). De befintliga raderna får position + 1.
ALTER TABLE pole_after ADD COLUMN row_id INTEGER NOT NULL DEFAULT 0;
UPDATE pole_after SET row_id = position + 1;
CREATE UNIQUE INDEX IF NOT EXISTS pole_after_row_id ON pole_after(row_id);
-- Linhöjderna i fliken Linhöjder: en rad per spann mellan två grannrader i Stolptabell efter.
-- row_a och row_b är radernas row_id med row_a < row_b. Värdena är kanonisk decimaltext och NULL
-- när de inte är ifyllda. En rad utan värden tas bort. Ett spann vars rader inte längre är
-- grannar tas bort i samma transaktion som ändringen i efter (pruneLineHeights).
CREATE TABLE IF NOT EXISTS line_height_span (
    row_a    INTEGER NOT NULL CHECK (row_a > 0),
    row_b    INTEGER NOT NULL,
    outer_m  TEXT,
    middle_m TEXT,
    top_m    TEXT,
    PRIMARY KEY (row_a, row_b),
    CHECK (row_a < row_b)
) WITHOUT ROWID;
-- Topplinans Av per rad i Stolptabell efter (row_id), kanonisk decimaltext skriven av användaren.
CREATE TABLE IF NOT EXISTS line_height_pole (
    row_id INTEGER PRIMARY KEY CHECK (row_id > 0),
    top_av TEXT NOT NULL
);
)sql";

} // namespace kalkyl::projectdb::dev
