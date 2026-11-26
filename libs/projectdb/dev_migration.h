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

} // namespace kalkyl::projectdb::dev
