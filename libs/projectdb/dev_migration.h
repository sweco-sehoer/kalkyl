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

} // namespace kalkyl::projectdb::dev
