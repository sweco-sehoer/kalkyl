#include "projectdb/project_data.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <functional>
#include <initializer_list>
#include <map>
#include <set>
#include <type_traits>
#include <utility>
#include <variant>

#include "db/database.h"
#include "lineCalc/load_cases.h"
#include "platform/system.h"
#include "tableText/table_text.h"

namespace kalkyl::projectdb {

namespace {

using conductor::Conductor;
using core::Error;
using core::ErrorCode;
using core::Result;

// Ett värde att binda till en SQL-parameter: NULL, heltal, decimaltal eller text.
using Arg = std::variant<std::nullptr_t, std::int64_t, double, std::string_view>;

// Kolumnerna i project_conductor i den ordning readConductor läser dem.
constexpr std::string_view kConductorColumns =
    "id, name, type, area_mm2, diameter_mm, e_initial_mpa, e_permanent_mpa, e_initial_limit_mpa, "
    "eiv_constant, alpha_e6_per_c, creep_strain_permille, creep_temp_c, weight_n_per_m, "
    "breaking_load_kn";

// Skapar ett fel med en kod och en svensk text.
Error makeError(ErrorCode code, std::string message, std::string detail = {})
{
    Error e;
    e.code = code;
    e.message = std::move(message);
    e.detail = std::move(detail);
    return e;
}

// Binder argumenten i ordning till parametrarna ?1, ?2 och så vidare.
void bindAll(db::Statement& stmt, std::initializer_list<Arg> args)
{
    int index = 1;
    for (const Arg& a : args) {
        std::visit(
            [&](const auto& v) {
                if constexpr (std::is_same_v<std::decay_t<decltype(v)>, std::nullptr_t>)
                    stmt.bindNull(index);
                else
                    stmt.bind(index, v);
            },
            a);
        ++index;
    }
}

// Förbereder en sats och binder argumenten.
Result<db::Statement> prepared(db::Database& d, std::string_view sql, std::initializer_list<Arg> args)
{
    auto stmt = d.prepare(sql);
    if (!stmt)
        return stmt.error();
    bindAll(stmt.value(), args);
    return stmt;
}

// Kör en sats som inte ger några rader (INSERT, UPDATE, DELETE).
Result<void> run(db::Database& d, std::string_view sql, std::initializer_list<Arg> args)
{
    auto stmt = prepared(d, sql, args);
    if (!stmt)
        return stmt.error();
    return stmt.value().run();
}

// Heltalet i första kolumnen på första raden, eller tomt när ingen rad finns.
Result<std::optional<std::int64_t>> firstInt(db::Database& d, std::string_view sql,
                                             std::initializer_list<Arg> args)
{
    auto stmt = prepared(d, sql, args);
    if (!stmt)
        return stmt.error();
    auto row = stmt.value().step();
    if (!row)
        return row.error();
    if (!row.value())
        return std::optional<std::int64_t>{};
    return std::optional<std::int64_t>{stmt.value().integer(0)};
}

// Texten i första kolumnen på första raden, eller tomt när ingen rad finns.
Result<std::optional<std::string>> firstText(db::Database& d, std::string_view sql,
                                             std::initializer_list<Arg> args)
{
    auto stmt = prepared(d, sql, args);
    if (!stmt)
        return stmt.error();
    auto row = stmt.value().step();
    if (!row)
        return row.error();
    if (!row.value())
        return std::optional<std::string>{};
    return std::optional<std::string>{stmt.value().text(0)};
}

// Felet när projektet är öppnat skrivskyddat.
Result<void> requireWritable(const ProjectDb& p)
{
    if (p.access() == Access::ReadOnly)
        return makeError(ErrorCode::AccessDenied,
                         "Projektet är öppnat skrivskyddat och kan inte ändras.");
    return {};
}

// Kör en skrivning i en transaktion mot arbetskopian. Skrivskyddat ger AccessDenied innan
// något görs, och efter en lyckad transaktion räknas revisionen upp.
Result<void> write(ProjectDb& p, const std::function<Result<void>(db::Database&)>& body)
{
    if (auto writable = requireWritable(p); !writable)
        return writable;
    auto done = p.local().transaction([&]() { return body(p.local()); });
    if (!done)
        return done;
    p.markModified();
    return {};
}

// Sant för de tre nycklarna som håller en linas namn.
bool isConductorKey(std::string_view key)
{
    return key == kPhaseConductorKey || key == kTopLeftConductorKey || key == kTopRightConductorKey;
}

// Sant för de sex nycklarna för normalspannsraderna.
bool isNormalSpanKey(std::string_view key)
{
    for (Wire wire : {Wire::Phase, Wire::TopLeft, Wire::TopRight})
        for (Stage stage : {Stage::Before, Stage::After})
            if (key == normalSpanDocKey(wire, stage))
                return true;
    return false;
}

// Sant när texten bara består av mellanslag och tabbar, eller är tom.
bool isBlank(const std::string& text)
{
    return text.find_first_not_of(" \t") == std::string::npos;
}

// Felet när en beräkning saknas.
Error spanNotFound()
{
    return makeError(ErrorCode::NotFound, "Normalspannsberäkningen finns inte längre i projektet.");
}

// Felet när en lina saknas i projektet.
Error conductorNotFound(const std::string& name)
{
    return makeError(ErrorCode::NotFound, "Linan " + name + " finns inte i projektet.");
}

// Kontrollerar linans värden med den gemensamma valideringen.
Result<void> checkConductor(const Conductor& c)
{
    const auto issues = conductor::validateConductor(c);
    if (!issues.empty())
        return makeError(ErrorCode::Invalid,
                         "Linan " + c.name + " har ogiltiga värden: " + issues.front().message);
    return {};
}

// Linans valfria brytlast som argument: ett tal eller NULL.
Arg breakingLoadArg(const Conductor& c)
{
    if (c.breakingLoadKn)
        return Arg{*c.breakingLoadKn};
    return Arg{nullptr};
}

// Läser en lina ur en rad med kolumnerna i kConductorColumns.
Conductor readConductor(const db::Statement& s)
{
    Conductor c;
    c.id = s.integer(0);
    c.origin = conductor::ConductorOrigin::Own;
    c.name = s.text(1);
    c.type = s.text(2);
    c.areaMm2 = s.real(3);
    c.diameterMm = s.real(4);
    c.eInitialMpa = s.real(5);
    c.ePermanentMpa = s.real(6);
    c.eInitialLimitMpa = s.real(7);
    c.eivConstant = s.real(8);
    c.alphaE6PerC = s.real(9);
    c.creepStrainPermille = s.real(10);
    c.creepTempC = s.real(11);
    c.weightNPerM = s.real(12);
    if (!s.isNull(13))
        c.breakingLoadKn = s.real(13);
    return c;
}

// Se till att linan finns i projektet och svarar med kopians namn. Finns en lina med samma
// namn (NOCASE) återanvänds den oförändrad, annars kopieras chosen in.
Result<std::string> ensureConductor(db::Database& d, const Conductor& c)
{
    if (auto valid = checkConductor(c); !valid)
        return valid.error();
    auto existing = firstText(d, "SELECT name FROM project_conductor WHERE name = ?1",
                              {std::string_view(c.name)});
    if (!existing)
        return existing.error();
    if (existing.value())
        return *existing.value();
    const std::string now = platform::nowUtcIso();
    auto inserted = run(d, R"sql(
INSERT INTO project_conductor(name, type, area_mm2, diameter_mm, e_initial_mpa, e_permanent_mpa,
                              e_initial_limit_mpa, eiv_constant, alpha_e6_per_c,
                              creep_strain_permille, creep_temp_c, weight_n_per_m,
                              breaking_load_kn, copied_utc)
VALUES(?1, ?2, ?3, ?4, ?5, ?6, ?7, ?8, ?9, ?10, ?11, ?12, ?13, ?14)
)sql",
                        {std::string_view(c.name), std::string_view(c.type), c.areaMm2, c.diameterMm,
                         c.eInitialMpa, c.ePermanentMpa, c.eInitialLimitMpa, c.eivConstant,
                         c.alphaE6PerC, c.creepStrainPermille, c.creepTempC, c.weightNPerM,
                         breakingLoadArg(c), std::string_view(now)});
    if (!inserted)
        return inserted.error();
    return c.name;
}

// Tar bort linor som ingen linrad i Dokumentation använder.
Result<void> pruneConductors(db::Database& d)
{
    return run(d, R"sql(
DELETE FROM project_conductor
 WHERE name NOT IN (SELECT value FROM doc_value
                     WHERE key IN ('phase_conductor', 'top_conductor_left', 'top_conductor_right'))
)sql",
               {});
}

// Lägger in eller ersätter en rad i doc_value.
Result<void> upsertDocValue(db::Database& d, const std::string& key, const std::string& value,
                            const std::string& reference)
{
    return run(d, R"sql(
INSERT INTO doc_value(key, value, reference) VALUES(?1, ?2, ?3)
ON CONFLICT(key) DO UPDATE SET value = excluded.value, reference = excluded.reference
)sql",
               {std::string_view(key), std::string_view(value), std::string_view(reference)});
}

// Kopplar raden docKey till beräkningen id och tömmer radens manuella värde. En rad som då
// blir helt tom tas bort. Fel: NotFound när beräkningen saknas, Invalid för en okänd nyckel.
Result<void> linkInside(db::Database& d, std::int64_t id, std::string_view docKey)
{
    if (!isNormalSpanKey(docKey))
        return makeError(ErrorCode::Invalid,
                         "Raden " + std::string(docKey) + " kan inte visa en normalspannsberäkning.");
    auto exists = firstInt(d, "SELECT id FROM normal_span WHERE id = ?1", {id});
    if (!exists)
        return exists.error();
    if (!exists.value())
        return spanNotFound();
    if (auto r = run(d, R"sql(
INSERT INTO doc_normal_span(doc_key, span_id) VALUES(?1, ?2)
ON CONFLICT(doc_key) DO UPDATE SET span_id = excluded.span_id
)sql",
                     {docKey, id});
        !r)
        return r;
    if (auto r = run(d, "UPDATE doc_value SET value = '' WHERE key = ?1", {docKey}); !r)
        return r;
    return run(d, "DELETE FROM doc_value WHERE key = ?1 AND value = '' AND reference = ''", {docKey});
}

// Läser beräkningen id med stolpar och rader. Tomt när id saknas.
Result<std::optional<NormalSpan>> readSpan(db::Database& d, std::int64_t id)
{
    NormalSpan ns;
    {
        auto stmt = prepared(d, R"sql(
SELECT id, note, limit_longest, limit_shortest, limit_adjacent, limit_local_min,
       limit_local_max, created_utc, modified_utc
  FROM normal_span WHERE id = ?1
)sql",
                             {id});
        if (!stmt)
            return stmt.error();
        auto row = stmt.value().step();
        if (!row)
            return row.error();
        if (!row.value())
            return std::optional<NormalSpan>{};
        const db::Statement& s = stmt.value();
        ns.id = s.integer(0);
        ns.note = s.text(1);
        ns.limits.longest = s.real(2);
        ns.limits.shortest = s.real(3);
        ns.limits.adjacent = s.real(4);
        ns.limits.localMin = s.real(5);
        ns.limits.localMax = s.real(6);
        ns.createdUtc = s.text(7);
        ns.modifiedUtc = s.text(8);
    }
    {
        auto stmt = prepared(d, R"sql(
SELECT pole, length_m, height_m FROM normal_span_pole WHERE span_id = ?1 ORDER BY position
)sql",
                             {id});
        if (!stmt)
            return stmt.error();
        while (true) {
            auto row = stmt.value().step();
            if (!row)
                return row.error();
            if (!row.value())
                break;
            ancalc::SpanPole pole;
            pole.pole = stmt.value().text(0);
            if (!stmt.value().isNull(1))
                pole.lengthM = stmt.value().real(1);
            if (!stmt.value().isNull(2))
                pole.heightM = stmt.value().real(2);
            ns.poles.push_back(std::move(pole));
        }
    }
    {
        auto stmt = prepared(d, "SELECT doc_key FROM doc_normal_span WHERE span_id = ?1 ORDER BY doc_key",
                             {id});
        if (!stmt)
            return stmt.error();
        while (true) {
            auto row = stmt.value().step();
            if (!row)
                return row.error();
            if (!row.value())
                break;
            ns.docKeys.push_back(stmt.value().text(0));
        }
    }
    return std::optional<NormalSpan>{std::move(ns)};
}

// Läser alla rader i doc_value i nyckelordning.
Result<std::vector<DocValue>> readDocValues(db::Database& d)
{
    auto stmt = prepared(d, "SELECT key, value, reference FROM doc_value ORDER BY key", {});
    if (!stmt)
        return stmt.error();
    std::vector<DocValue> values;
    while (true) {
        auto row = stmt.value().step();
        if (!row)
            return row.error();
        if (!row.value())
            break;
        values.push_back({stmt.value().text(0), stmt.value().text(1), stmt.value().text(2)});
    }
    return values;
}

// Läser alla linor i project_conductor, sorterade på namn.
Result<std::vector<Conductor>> readConductors(db::Database& d)
{
    const std::string sql =
        "SELECT " + std::string(kConductorColumns) + " FROM project_conductor ORDER BY name";
    auto stmt = prepared(d, sql, {});
    if (!stmt)
        return stmt.error();
    std::vector<Conductor> list;
    while (true) {
        auto row = stmt.value().step();
        if (!row)
            return row.error();
        if (!row.value())
            break;
        list.push_back(readConductor(stmt.value()));
    }
    return list;
}

// ---- Linberäkningarna ----

// Texten som läget, temperaturkällan och vindtryckskällan lagras som i databasen.
constexpr std::pair<linecalc::TempSource, std::string_view> kTempSources[] = {
    {linecalc::TempSource::Fixed, "fixed"},
    {linecalc::TempSource::Min, "min"},
    {linecalc::TempSource::MinPlus20, "min_plus_20"},
    {linecalc::TempSource::Max, "max"},
    {linecalc::TempSource::ShortCircuit, "short_circuit"},
    {linecalc::TempSource::ShortCircuitRise, "short_circuit_rise"},
};
constexpr std::pair<linecalc::PressureSource, std::string_view> kPressureSources[] = {
    {linecalc::PressureSource::Fixed, "fixed"},
    {linecalc::PressureSource::Normal, "normal"},
    {linecalc::PressureSource::High, "high"},
};

// Lagringstexten för ett läge.
std::string_view modeText(linecalc::CalcMode mode)
{
    return mode == linecalc::CalcMode::Excel ? "excel" : "new";
}

// Läget ur lagringstexten, tomt för en okänd text.
std::optional<linecalc::CalcMode> parseMode(const std::string& text)
{
    if (text == "excel")
        return linecalc::CalcMode::Excel;
    if (text == "new")
        return linecalc::CalcMode::New;
    return std::nullopt;
}

// Lagringstexten för en temperaturkälla.
std::string_view tempSourceText(linecalc::TempSource source)
{
    for (const auto& [value, text] : kTempSources)
        if (value == source)
            return text;
    return "fixed";
}

// Lagringstexten för en vindtryckskälla.
std::string_view pressureSourceText(linecalc::PressureSource source)
{
    for (const auto& [value, text] : kPressureSources)
        if (value == source)
            return text;
    return "fixed";
}

// Temperaturkällan ur lagringstexten, tomt för en okänd text.
std::optional<linecalc::TempSource> parseTempSource(const std::string& text)
{
    for (const auto& [value, name] : kTempSources)
        if (name == text)
            return value;
    return std::nullopt;
}

// Vindtryckskällan ur lagringstexten, tomt för en okänd text.
std::optional<linecalc::PressureSource> parsePressureSource(const std::string& text)
{
    for (const auto& [value, name] : kPressureSources)
        if (name == text)
            return value;
    return std::nullopt;
}

// Sant för de sex nycklarna för linberäkningsraderna.
bool isLineCalcKey(std::string_view key)
{
    return lineCalcRowOf(key).has_value();
}

// Felet när en linberäkning saknas.
Error lineCalcNotFound()
{
    return makeError(ErrorCode::NotFound, "Linberäkningen finns inte längre i projektet.");
}

// Felet när en rad i projektfilen med en linberäkning inte går att tolka.
Error lineCalcCorrupt(std::int64_t id)
{
    return makeError(ErrorCode::Invalid,
                     "Linberäkningen " + std::to_string(id) + " i projektfilen är skadad.");
}

// Felet när en nyckel inte är en linberäkningsrad.
Error notLineCalcKey(std::string_view key)
{
    return makeError(ErrorCode::Invalid,
                     "Raden " + std::string(key) + " kan inte visa en linberäkning.");
}

// Felet när en kopplad beräkning ska höra till en annan rad än sin egen.
Error bondedToOneRow()
{
    return makeError(ErrorCode::Invalid,
                     "En linberäkning med indata ur Dokumentation hör till en rad.");
}

// Sant när ett eventuellt tal är ändligt.
bool finiteOpt(const std::optional<double>& v)
{
    return !v || std::isfinite(*v);
}

// Sant när varje tal i indata är ändligt: lindata, konstanter, lastfall och manuell last.
bool allFinite(const linecalc::LineInput& in)
{
    if (in.conductor) {
        const Conductor& c = *in.conductor;
        const double values[] = {c.areaMm2, c.diameterMm, c.eInitialMpa, c.ePermanentMpa,
                                 c.eInitialLimitMpa, c.eivConstant, c.alphaE6PerC,
                                 c.creepStrainPermille, c.creepTempC, c.weightNPerM};
        for (double v : values)
            if (!std::isfinite(v))
                return false;
        if (!finiteOpt(c.breakingLoadKn))
            return false;
    }
    const linecalc::LineConstants& k = in.constants;
    const double plain[] = {in.refTempC, in.minTempC, in.maxTempC, in.shortCircuitTempC,
                            in.lineHeightM, in.normalWindNPerM2, in.highWindNPerM2,
                            k.iceDensityKgM3, k.pi, k.gravity, k.gustFactor, k.resonanceFactor,
                            k.dragFactor, k.minIceNPerM, k.maxUtilizationPercent};
    for (double v : plain)
        if (!std::isfinite(v))
            return false;
    if (!finiteOpt(in.normalSpanM) || !finiteOpt(in.tensionMpa))
        return false;
    for (const linecalc::LoadCase& lc : in.cases) {
        const double values[] = {lc.tempC, lc.windMagnitudePercent, lc.charPressureNPerM2,
                                 lc.refHeightM, lc.pressureGradientNPerM3, lc.iceThicknessMm};
        for (double v : values)
            if (!std::isfinite(v))
                return false;
        if (lc.manual && (!std::isfinite(lc.manual->iceNPerM) || !std::isfinite(lc.manual->windNPerM)))
            return false;
    }
    return true;
}

// Ett tal eller NULL som SQL-argument.
Arg optionalArg(const std::optional<double>& v)
{
    return v ? Arg{*v} : Arg{nullptr};
}

// Kör en sats med argumenten i en vektor, i ordning till ?1, ?2 och så vidare.
Result<void> runWith(db::Database& d, std::string_view sql, const std::vector<Arg>& args)
{
    auto stmt = d.prepare(sql);
    if (!stmt)
        return stmt.error();
    int index = 1;
    for (const Arg& a : args) {
        std::visit(
            [&](const auto& v) {
                if constexpr (std::is_same_v<std::decay_t<decltype(v)>, std::nullptr_t>)
                    stmt.value().bindNull(index);
                else
                    stmt.value().bind(index, v);
            },
            a);
        ++index;
    }
    return stmt.value().run();
}

// Skriver beräkningen (ny eller befintlig) och dess 15 lastfall och svarar med id. En kopplad
// beräkning (docSource satt) lagrar ingen lina, inget an och inget sigma0. Skapad-tiden på en
// befintlig beräkning står kvar. Kopplingen till en rad görs av anroparen.
Result<std::int64_t> storeLineCalc(db::Database& d, const LineCalc& lc, std::string_view docSource,
                                   const std::string& now)
{
    const linecalc::LineInput& in = lc.input;
    const linecalc::LineConstants& k = in.constants;
    const bool linked = !docSource.empty();
    const Conductor* c = !linked && in.conductor ? &*in.conductor : nullptr;
    const auto condReal = [&](double Conductor::*field) -> Arg {
        return c ? Arg{c->*field} : Arg{nullptr};
    };
    const std::vector<Arg> args = {
        lc.id == 0 ? Arg{nullptr} : Arg{lc.id},
        Arg{std::string_view(lc.note)},
        Arg{modeText(in.mode)},
        linked ? Arg{docSource} : Arg{nullptr},
        c ? Arg{std::string_view(c->name)} : Arg{nullptr},
        Arg{c ? std::string_view(c->type) : std::string_view()},
        condReal(&Conductor::areaMm2),
        condReal(&Conductor::diameterMm),
        condReal(&Conductor::eInitialMpa),
        condReal(&Conductor::ePermanentMpa),
        condReal(&Conductor::eInitialLimitMpa),
        condReal(&Conductor::eivConstant),
        condReal(&Conductor::alphaE6PerC),
        condReal(&Conductor::creepStrainPermille),
        condReal(&Conductor::creepTempC),
        condReal(&Conductor::weightNPerM),
        c ? optionalArg(c->breakingLoadKn) : Arg{nullptr},
        linked ? Arg{nullptr} : optionalArg(in.normalSpanM),
        linked ? Arg{nullptr} : optionalArg(in.tensionMpa),
        Arg{in.refTempC},
        Arg{in.minTempC},
        Arg{in.maxTempC},
        Arg{in.shortCircuitTempC},
        Arg{in.lineHeightM},
        Arg{in.normalWindNPerM2},
        Arg{in.highWindNPerM2},
        Arg{k.iceDensityKgM3},
        Arg{k.pi},
        Arg{k.gravity},
        Arg{k.gustFactor},
        Arg{k.resonanceFactor},
        Arg{k.dragFactor},
        Arg{k.minIceNPerM},
        Arg{k.maxUtilizationPercent},
        Arg{static_cast<std::int64_t>(k.transpositionCase)},
        Arg{std::string_view(now)},
    };
    if (auto r = runWith(d, R"sql(
INSERT INTO line_calc(id, note, mode, doc_source, cond_name, cond_type, cond_area_mm2,
                      cond_diameter_mm, cond_e_initial_mpa, cond_e_permanent_mpa,
                      cond_e_initial_limit_mpa, cond_eiv_constant, cond_alpha_e6_per_c,
                      cond_creep_strain_permille, cond_creep_temp_c, cond_weight_n_per_m,
                      cond_breaking_load_kn, normal_span_m, tension_mpa, ref_temp_c, min_temp_c,
                      max_temp_c, short_circuit_temp_c, line_height_m, normal_wind_n_per_m2,
                      high_wind_n_per_m2, ice_density_kg_m3, pi, gravity, gust_factor,
                      resonance_factor, drag_factor, min_ice_n_per_m, max_utilization_percent,
                      transposition_case, created_utc, modified_utc)
VALUES(?1, ?2, ?3, ?4, ?5, ?6, ?7, ?8, ?9, ?10, ?11, ?12, ?13, ?14, ?15, ?16, ?17, ?18, ?19, ?20,
       ?21, ?22, ?23, ?24, ?25, ?26, ?27, ?28, ?29, ?30, ?31, ?32, ?33, ?34, ?35, ?36, ?36)
ON CONFLICT(id) DO UPDATE SET
    note = excluded.note, mode = excluded.mode, doc_source = excluded.doc_source,
    cond_name = excluded.cond_name, cond_type = excluded.cond_type,
    cond_area_mm2 = excluded.cond_area_mm2, cond_diameter_mm = excluded.cond_diameter_mm,
    cond_e_initial_mpa = excluded.cond_e_initial_mpa,
    cond_e_permanent_mpa = excluded.cond_e_permanent_mpa,
    cond_e_initial_limit_mpa = excluded.cond_e_initial_limit_mpa,
    cond_eiv_constant = excluded.cond_eiv_constant,
    cond_alpha_e6_per_c = excluded.cond_alpha_e6_per_c,
    cond_creep_strain_permille = excluded.cond_creep_strain_permille,
    cond_creep_temp_c = excluded.cond_creep_temp_c,
    cond_weight_n_per_m = excluded.cond_weight_n_per_m,
    cond_breaking_load_kn = excluded.cond_breaking_load_kn,
    normal_span_m = excluded.normal_span_m, tension_mpa = excluded.tension_mpa,
    ref_temp_c = excluded.ref_temp_c, min_temp_c = excluded.min_temp_c,
    max_temp_c = excluded.max_temp_c, short_circuit_temp_c = excluded.short_circuit_temp_c,
    line_height_m = excluded.line_height_m, normal_wind_n_per_m2 = excluded.normal_wind_n_per_m2,
    high_wind_n_per_m2 = excluded.high_wind_n_per_m2, ice_density_kg_m3 = excluded.ice_density_kg_m3,
    pi = excluded.pi, gravity = excluded.gravity, gust_factor = excluded.gust_factor,
    resonance_factor = excluded.resonance_factor, drag_factor = excluded.drag_factor,
    min_ice_n_per_m = excluded.min_ice_n_per_m,
    max_utilization_percent = excluded.max_utilization_percent,
    transposition_case = excluded.transposition_case, modified_utc = excluded.modified_utc
)sql",
                       args);
        !r)
        return r.error();

    std::int64_t id = lc.id;
    if (id == 0) {
        auto rowId = firstInt(d, "SELECT last_insert_rowid()", {});
        if (!rowId)
            return rowId.error();
        id = rowId.value().value_or(0);
    }
    if (auto r = run(d, "DELETE FROM line_calc_case WHERE calc_id = ?1", {id}); !r)
        return r.error();
    std::int64_t caseId = 1;
    for (const linecalc::LoadCase& c1 : in.cases) {
        const std::vector<Arg> caseArgs = {
            Arg{id},
            Arg{caseId},
            Arg{tempSourceText(c1.tempSource)},
            Arg{c1.tempC},
            Arg{std::string_view(c1.name)},
            Arg{std::string_view(c1.standardCase)},
            Arg{std::string_view(c1.clearanceCase)},
            c1.windFromCase ? Arg{static_cast<std::int64_t>(*c1.windFromCase)} : Arg{nullptr},
            Arg{c1.windMagnitudePercent},
            Arg{pressureSourceText(c1.pressureSource)},
            Arg{c1.charPressureNPerM2},
            Arg{c1.refHeightM},
            Arg{c1.pressureGradientNPerM3},
            Arg{c1.iceThicknessMm},
            c1.manual ? Arg{c1.manual->iceNPerM} : Arg{nullptr},
            c1.manual ? Arg{c1.manual->windNPerM} : Arg{nullptr},
        };
        if (auto r = runWith(d, R"sql(
INSERT INTO line_calc_case(calc_id, case_id, temp_source, temp_c, name, standard_case,
                           clearance_case, wind_from_case, wind_magnitude_percent,
                           pressure_source, char_pressure_n_per_m2, ref_height_m,
                           pressure_gradient_n_per_m3, ice_thickness_mm, manual_ice_n_per_m,
                           manual_wind_n_per_m)
VALUES(?1, ?2, ?3, ?4, ?5, ?6, ?7, ?8, ?9, ?10, ?11, ?12, ?13, ?14, ?15, ?16)
)sql",
                           caseArgs);
            !r)
            return r.error();
        ++caseId;
    }
    return id;
}

// Läser beräkningen id med lastfall och rader. Tomt när id saknas. En okänd text eller färre
// än 15 lastfall ger Invalid. En kopplad beräkning får lina, an och sigma0 tomma.
Result<std::optional<LineCalc>> readLineCalc(db::Database& d, std::int64_t id)
{
    LineCalc lc;
    linecalc::LineInput& in = lc.input;
    {
        auto stmt = prepared(d, R"sql(
SELECT id, note, mode, doc_source, cond_name, cond_type, cond_area_mm2, cond_diameter_mm,
       cond_e_initial_mpa, cond_e_permanent_mpa, cond_e_initial_limit_mpa, cond_eiv_constant,
       cond_alpha_e6_per_c, cond_creep_strain_permille, cond_creep_temp_c, cond_weight_n_per_m,
       cond_breaking_load_kn, normal_span_m, tension_mpa, ref_temp_c, min_temp_c, max_temp_c,
       short_circuit_temp_c, line_height_m, normal_wind_n_per_m2, high_wind_n_per_m2,
       ice_density_kg_m3, pi, gravity, gust_factor, resonance_factor, drag_factor,
       min_ice_n_per_m, max_utilization_percent, transposition_case, created_utc, modified_utc
  FROM line_calc WHERE id = ?1
)sql",
                             {id});
        if (!stmt)
            return stmt.error();
        auto row = stmt.value().step();
        if (!row)
            return row.error();
        if (!row.value())
            return std::optional<LineCalc>{};
        const db::Statement& s = stmt.value();
        lc.id = s.integer(0);
        lc.note = s.text(1);
        const auto mode = parseMode(s.text(2));
        if (!mode)
            return lineCalcCorrupt(id);
        in.mode = *mode;
        lc.docSource = s.text(3);
        if (lc.docSource.empty() && !s.isNull(4)) {
            Conductor c;
            c.origin = conductor::ConductorOrigin::Own;
            c.name = s.text(4);
            c.type = s.text(5);
            c.areaMm2 = s.real(6);
            c.diameterMm = s.real(7);
            c.eInitialMpa = s.real(8);
            c.ePermanentMpa = s.real(9);
            c.eInitialLimitMpa = s.real(10);
            c.eivConstant = s.real(11);
            c.alphaE6PerC = s.real(12);
            c.creepStrainPermille = s.real(13);
            c.creepTempC = s.real(14);
            c.weightNPerM = s.real(15);
            if (!s.isNull(16))
                c.breakingLoadKn = s.real(16);
            in.conductor = std::move(c);
        }
        if (lc.docSource.empty()) {
            if (!s.isNull(17))
                in.normalSpanM = s.real(17);
            if (!s.isNull(18))
                in.tensionMpa = s.real(18);
        }
        in.refTempC = s.real(19);
        in.minTempC = s.real(20);
        in.maxTempC = s.real(21);
        in.shortCircuitTempC = s.real(22);
        in.lineHeightM = s.real(23);
        in.normalWindNPerM2 = s.real(24);
        in.highWindNPerM2 = s.real(25);
        linecalc::LineConstants& k = in.constants;
        k.iceDensityKgM3 = s.real(26);
        k.pi = s.real(27);
        k.gravity = s.real(28);
        k.gustFactor = s.real(29);
        k.resonanceFactor = s.real(30);
        k.dragFactor = s.real(31);
        k.minIceNPerM = s.real(32);
        k.maxUtilizationPercent = s.real(33);
        k.transpositionCase = static_cast<int>(s.integer(34));
        lc.createdUtc = s.text(35);
        lc.modifiedUtc = s.text(36);
    }
    {
        auto stmt = prepared(d, R"sql(
SELECT case_id, temp_source, temp_c, name, standard_case, clearance_case, wind_from_case,
       wind_magnitude_percent, pressure_source, char_pressure_n_per_m2, ref_height_m,
       pressure_gradient_n_per_m3, ice_thickness_mm, manual_ice_n_per_m, manual_wind_n_per_m
  FROM line_calc_case WHERE calc_id = ?1 ORDER BY case_id
)sql",
                             {id});
        if (!stmt)
            return stmt.error();
        std::array<bool, linecalc::kLoadCaseCount> seen{};
        while (true) {
            auto row = stmt.value().step();
            if (!row)
                return row.error();
            if (!row.value())
                break;
            const db::Statement& s = stmt.value();
            const std::int64_t caseId = s.integer(0);
            if (caseId < 1 || caseId > linecalc::kLoadCaseCount)
                return lineCalcCorrupt(id);
            const auto tempSource = parseTempSource(s.text(1));
            const auto pressureSource = parsePressureSource(s.text(8));
            if (!tempSource || !pressureSource || s.isNull(13) != s.isNull(14))
                return lineCalcCorrupt(id);
            const std::size_t index = static_cast<std::size_t>(caseId - 1);
            linecalc::LoadCase& c = in.cases[index];
            c = linecalc::LoadCase{};
            c.tempSource = *tempSource;
            c.tempC = s.real(2);
            c.name = s.text(3);
            c.standardCase = s.text(4);
            c.clearanceCase = s.text(5);
            if (!s.isNull(6))
                c.windFromCase = static_cast<int>(s.integer(6));
            c.windMagnitudePercent = s.real(7);
            c.pressureSource = *pressureSource;
            c.charPressureNPerM2 = s.real(9);
            c.refHeightM = s.real(10);
            c.pressureGradientNPerM3 = s.real(11);
            c.iceThicknessMm = s.real(12);
            if (!s.isNull(13))
                c.manual = linecalc::ManualLoad{s.real(13), s.real(14)};
            seen[index] = true;
        }
        for (bool present : seen)
            if (!present)
                return lineCalcCorrupt(id);
    }
    {
        auto stmt = prepared(d, "SELECT doc_key FROM doc_line_calc WHERE calc_id = ?1 ORDER BY doc_key",
                             {id});
        if (!stmt)
            return stmt.error();
        while (true) {
            auto row = stmt.value().step();
            if (!row)
                return row.error();
            if (!row.value())
                break;
            lc.docKeys.push_back(stmt.value().text(0));
        }
    }
    return std::optional<LineCalc>{std::move(lc)};
}

// Lägger aₙ för normalspannsraderna i sources.normalSpans: de lagrade kopplingarna (links) utom på
// tagna rader, och de automatiska beräkningarna. Definieras efter det automatiska läget.
Result<void> addSpanSources(db::Database& d, const std::vector<std::pair<std::string, std::int64_t>>& links,
                            DocLineSources& sources);

// Läser det i Dokumentation som en kopplad linberäkning hämtar: uppgifterna, projektets linor
// och aₙ för varje normalspannsrad med en kopplad eller automatisk beräkning (tomt aₙ när
// beräkningen har fel).
Result<DocLineSources> readLineSources(db::Database& d)
{
    DocLineSources sources;
    auto values = readDocValues(d);
    if (!values)
        return values.error();
    sources.values = std::move(values.value());
    auto conductors = readConductors(d);
    if (!conductors)
        return conductors.error();
    sources.conductors = std::move(conductors.value());

    std::vector<std::pair<std::string, std::int64_t>> links;
    {
        auto stmt = prepared(d, "SELECT doc_key, span_id FROM doc_normal_span ORDER BY doc_key", {});
        if (!stmt)
            return stmt.error();
        while (true) {
            auto row = stmt.value().step();
            if (!row)
                return row.error();
            if (!row.value())
                break;
            links.emplace_back(stmt.value().text(0), stmt.value().integer(1));
        }
    }
    if (auto r = addSpanSources(d, links, sources); !r)
        return r.error();
    return sources;
}

// Gör en kopplad beräkning manuell: Dokumentationens värden i det här ögonblicket skrivs in som
// beräkningens egna, och doc_source blir NULL. Kopplingen till raden står kvar. En manuell
// beräkning lämnas orörd. Fel: NotFound när beräkningen saknas.
Result<void> detachInside(db::Database& d, std::int64_t id)
{
    auto read = readLineCalc(d, id);
    if (!read)
        return read.error();
    if (!read.value())
        return lineCalcNotFound();
    LineCalc lc = std::move(*read.value());
    if (lc.docSource.empty())
        return {};
    auto sources = readLineSources(d);
    if (!sources)
        return sources.error();
    lc.input = resolveLineInput(lc.input, lc.docSource, sources.value()).input;
    auto stored = storeLineCalc(d, lc, {}, platform::nowUtcIso());
    if (!stored)
        return stored.error();
    return {};
}

// Kopplar raden docKey till beräkningen id och tömmer radens manuella värde. En kopplad
// beräkning som visades på raden blir manuell först. Fel: Invalid för en okänd nyckel eller en
// beräkning som hör till en annan rad, NotFound när beräkningen saknas.
Result<void> linkLineInside(db::Database& d, std::int64_t id, std::string_view docKey)
{
    if (!isLineCalcKey(docKey))
        return notLineCalcKey(docKey);
    auto own = firstText(d, "SELECT COALESCE(doc_source, '') FROM line_calc WHERE id = ?1", {id});
    if (!own)
        return own.error();
    if (!own.value())
        return lineCalcNotFound();
    if (!own.value()->empty() && *own.value() != docKey)
        return bondedToOneRow();

    auto current = firstInt(d, "SELECT calc_id FROM doc_line_calc WHERE doc_key = ?1", {docKey});
    if (!current)
        return current.error();
    if (current.value() && *current.value() != id) {
        auto source = firstText(d, "SELECT COALESCE(doc_source, '') FROM line_calc WHERE id = ?1",
                                {*current.value()});
        if (!source)
            return source.error();
        if (source.value() && *source.value() == docKey)
            if (auto r = detachInside(d, *current.value()); !r)
                return r;
    }
    if (auto r = run(d, R"sql(
INSERT INTO doc_line_calc(doc_key, calc_id) VALUES(?1, ?2)
ON CONFLICT(doc_key) DO UPDATE SET calc_id = excluded.calc_id
)sql",
                     {docKey, id});
        !r)
        return r;
    if (auto r = run(d, "UPDATE doc_value SET value = '' WHERE key = ?1", {docKey}); !r)
        return r;
    return run(d, "DELETE FROM doc_value WHERE key = ?1 AND value = '' AND reference = ''", {docKey});
}

// Radens namn i Dokumentation för en lina: ord för texterna och nycklarna i upplösningen.
struct WireNames {
    std::string word;            // "faslina", "topplina", "topplina vänster" eller "topplina höger"
    std::string rowWord;         // beräkningsradernas början: "Faslina", "Topplinor", "Topplina vänster" ...
    std::string conductorLabel;  // linradens text
    std::string tensionLabel;    // uppspänningsradens text
    std::string conductorKey;
    std::string tensionKey;
    std::string spanKey;         // den gröna normalspannsraden
};

// Namnen för linan wire. same är sant när topplina höger är "Samma", och då heter
// topplinans rader utan vänster.
WireNames wireNames(Wire wire, bool same)
{
    if (wire == Wire::Phase)
        return {"faslina", "Faslina", "Faslina", "Uppspänning fas", std::string(kPhaseConductorKey),
                "tension_phase", "normal_span_phase"};
    if (wire == Wire::TopLeft)
        return {same ? "topplina" : "topplina vänster", same ? "Topplinor" : "Topplina vänster",
                "Topplina vänster", same ? "Uppspänning topp" : "Uppspänning topp vänster",
                std::string(kTopLeftConductorKey), "tension_top_left", "normal_span_top_left"};
    return {"topplina höger", "Topplina höger", "Topplina höger", "Uppspänning topp höger",
            std::string(kTopRightConductorKey), "tension_top_right", "normal_span_top_right"};
}

// En Missing-upplysning för hela beräkningen.
linecalc::LineIssue missingIssue(linecalc::LineField field, std::string message)
{
    linecalc::LineIssue issue;
    issue.kind = linecalc::LineIssueKind::Missing;
    issue.caseId = 0;
    issue.field = field;
    issue.message = std::move(message);
    return issue;
}

// Värdet i doc_value för nyckeln, eller tomt när raden saknas eller är tom.
std::string docText(const DocLineSources& docs, std::string_view key)
{
    for (const DocValue& v : docs.values)
        if (v.key == key)
            return v.value;
    return {};
}

// Talet i doc_value för nyckeln, eller tomt när raden saknas, är tom eller inte är ett tal.
std::optional<double> docNumber(const DocLineSources& docs, std::string_view key)
{
    const std::string text = docText(docs, key);
    if (text.empty())
        return std::nullopt;
    return tabletext::parseDecimal(text);
}

// ---- Automatiskt läge ----

constexpr std::array<Wire, 3> kWires = {Wire::Phase, Wire::TopLeft, Wire::TopRight};
constexpr std::array<Stage, 2> kStages = {Stage::Before, Stage::After};

// Lagringstexten för ett skede i auto_mode och auto_calc.
std::string_view stageText(Stage stage)
{
    return stage == Stage::Before ? "before" : "after";
}

// Skedet ur lagringstexten.
Stage stageFromText(const std::string& text)
{
    return text == "after" ? Stage::After : Stage::Before;
}

// Lagringstexten för en lina i auto_calc.
std::string_view wireText(Wire wire)
{
    return wire == Wire::Phase ? "phase" : wire == Wire::TopLeft ? "top_left" : "top_right";
}

// Linan ur lagringstexten.
Wire wireFromText(const std::string& text)
{
    return text == "phase" ? Wire::Phase : text == "top_left" ? Wire::TopLeft : Wire::TopRight;
}

// Skedets index i listor om två och linans index i listor om tre.
std::size_t stageIndex(Stage stage)
{
    return stage == Stage::Before ? 0 : 1;
}
std::size_t wireIndex(Wire wire)
{
    return wire == Wire::Phase ? 0 : wire == Wire::TopLeft ? 1 : 2;
}

// Tabellen som håller stolptabellen för skedet.
std::string_view poleTableSql(Stage stage)
{
    return stage == Stage::Before ? "pole_before" : "pole_after";
}

// Läser stolptabellen för skedet i tabellens ordning. NULL i en talkolumn blir tom text.
Result<std::vector<poletable::PoleRow>> readPoles(db::Database& d, Stage stage)
{
    const std::string sql = "SELECT pole, av, ah, span_m, coord_n, coord_e, coord_h, lkl FROM " +
                            std::string(poleTableSql(stage)) + " ORDER BY position";
    auto stmt = prepared(d, sql, {});
    if (!stmt)
        return stmt.error();
    std::vector<poletable::PoleRow> rows;
    while (true) {
        auto step = stmt.value().step();
        if (!step)
            return step.error();
        if (!step.value())
            break;
        // Fälten med namn, så att tabellens kolumnordning (poletable::k…) inte spelar någon roll.
        const db::Statement& s = stmt.value();
        poletable::PoleRow row;
        row.pole = s.text(0);
        row.av = s.text(1);
        row.ah = s.text(2);
        row.spanM = s.text(3);
        row.north = s.text(4);
        row.east = s.text(5);
        row.height = s.text(6);
        row.lkl = s.text(7);
        rows.push_back(std::move(row));
    }
    return rows;
}

// Läser automatiskt läge för skedet. Utan rad i auto_mode: av och inga sträckor.
Result<AutoSettings> readAutoSettings(db::Database& d, Stage stage)
{
    auto stmt = prepared(d, R"sql(
SELECT enabled, phase_from, phase_to, top_left_from, top_left_to, top_right_from, top_right_to
  FROM auto_mode WHERE stage = ?1
)sql",
                         {stageText(stage)});
    if (!stmt)
        return stmt.error();
    auto row = stmt.value().step();
    if (!row)
        return row.error();
    AutoSettings settings;
    if (!row.value())
        return settings;
    settings.enabled = stmt.value().integer(0) != 0;
    for (std::size_t i = 0; i < settings.ranges.size(); ++i) {
        const int column = static_cast<int>(1 + 2 * i);
        settings.ranges[i].fromPole = stmt.value().text(column);
        settings.ranges[i].toPole = stmt.value().text(column + 1);
    }
    return settings;
}

// En rad i auto_calc, och om raderna i normal_span och line_calc med de reserverade id:na finns.
struct AutoRow {
    Stage stage = Stage::Before;
    Wire wire = Wire::Phase;
    std::int64_t spanId = 0;
    std::int64_t calcId = 0;
    int firstRow = -1;
    int lastRow = -1;
    std::string issue;
    bool spanPresent = false;
    bool calcPresent = false;
};

// Läser alla rader i auto_calc, före först och i varje skede Phase, TopLeft och TopRight.
Result<std::vector<AutoRow>> readAutoRows(db::Database& d)
{
    auto stmt = prepared(d, R"sql(
SELECT a.stage, a.wire, a.span_id, a.calc_id, a.first_row, a.last_row, a.issue,
       (SELECT count(*) FROM normal_span WHERE id = a.span_id),
       (SELECT count(*) FROM line_calc WHERE id = a.calc_id)
  FROM auto_calc a
 ORDER BY CASE a.stage WHEN 'before' THEN 0 ELSE 1 END,
          CASE a.wire WHEN 'phase' THEN 0 WHEN 'top_left' THEN 1 ELSE 2 END
)sql",
                         {});
    if (!stmt)
        return stmt.error();
    std::vector<AutoRow> rows;
    while (true) {
        auto step = stmt.value().step();
        if (!step)
            return step.error();
        if (!step.value())
            break;
        const db::Statement& s = stmt.value();
        AutoRow row;
        row.stage = stageFromText(s.text(0));
        row.wire = wireFromText(s.text(1));
        row.spanId = s.integer(2);
        row.calcId = s.integer(3);
        row.firstRow = static_cast<int>(s.integer(4));
        row.lastRow = static_cast<int>(s.integer(5));
        row.issue = s.text(6);
        row.spanPresent = s.integer(7) != 0;
        row.calcPresent = s.integer(8) != 0;
        rows.push_back(std::move(row));
    }
    return rows;
}

// Det som läsningarna behöver för att märka en beräkning som automatisk: raderna i auto_calc och
// inställningen för varje skede.
struct AutoContext {
    std::vector<AutoRow> rows;
    std::array<AutoSettings, 2> settings;
};

// Läser auto_calc och inställningarna för båda skedena.
Result<AutoContext> readAutoContext(db::Database& d)
{
    AutoContext ctx;
    auto rows = readAutoRows(d);
    if (!rows)
        return rows.error();
    ctx.rows = std::move(rows.value());
    for (Stage stage : kStages) {
        auto settings = readAutoSettings(d, stage);
        if (!settings)
            return settings.error();
        ctx.settings[stageIndex(stage)] = std::move(settings.value());
    }
    return ctx;
}

// Märkningen av en automatisk beräkning: skede, lina, sträcka och sträckans läsning.
AutoOrigin originOf(const AutoRow& row, const AutoContext& ctx)
{
    AutoOrigin origin;
    origin.stage = row.stage;
    origin.wire = row.wire;
    origin.range = ctx.settings[stageIndex(row.stage)].ranges[wireIndex(row.wire)];
    origin.firstRow = row.firstRow;
    origin.lastRow = row.lastRow;
    origin.issue = row.issue;
    return origin;
}

// Raden i auto_calc vars normalspann har id, när normalspannet finns.
const AutoRow* autoRowOfSpan(const AutoContext& ctx, std::int64_t id)
{
    for (const AutoRow& row : ctx.rows)
        if (row.spanPresent && row.spanId == id)
            return &row;
    return nullptr;
}

// Raden i auto_calc vars linberäkning har id, när linberäkningen finns.
const AutoRow* autoRowOfCalc(const AutoContext& ctx, std::int64_t id)
{
    for (const AutoRow& row : ctx.rows)
        if (row.calcPresent && row.calcId == id)
            return &row;
    return nullptr;
}

// Märker en läst normalspannsberäkning: en automatisk får sin märkning och bara sin egen rad, en
// annan förlorar de rader som visar en automatisk beräkning.
void decorateSpan(NormalSpan& ns, const AutoContext& ctx)
{
    if (const AutoRow* row = autoRowOfSpan(ctx, ns.id)) {
        ns.automatic = originOf(*row, ctx);
        ns.docKeys = {normalSpanDocKey(row->wire, row->stage)};
        return;
    }
    std::erase_if(ns.docKeys, [&](const std::string& key) {
        for (const AutoRow& row : ctx.rows)
            if (row.spanPresent && key == normalSpanDocKey(row.wire, row.stage))
                return true;
        return false;
    });
}

// Märker en läst linberäkning på samma sätt som decorateSpan.
void decorateCalc(LineCalc& lc, const AutoContext& ctx)
{
    if (const AutoRow* row = autoRowOfCalc(ctx, lc.id)) {
        lc.automatic = originOf(*row, ctx);
        lc.docKeys = {lineCalcDocKey(row->wire, row->stage)};
        return;
    }
    std::erase_if(lc.docKeys, [&](const std::string& key) {
        for (const AutoRow& row : ctx.rows)
            if (row.calcPresent && key == lineCalcDocKey(row.wire, row.stage))
                return true;
        return false;
    });
}

// Den automatiska beräkningens rad i auto_calc som raden docKey (normalspanns- eller
// linberäkningsnyckel) visar, eller nullptr när raden inte är tagen. Delas av isTakenKey och guardRow.
const AutoRow* takenBy(const std::vector<AutoRow>& rows, std::string_view docKey)
{
    for (const AutoRow& row : rows) {
        if (row.spanPresent && docKey == normalSpanDocKey(row.wire, row.stage))
            return &row;
        if (row.calcPresent && docKey == lineCalcDocKey(row.wire, row.stage))
            return &row;
    }
    return nullptr;
}

// Sant när raden docKey (normalspanns- eller linberäkningsnyckel) visar en automatisk beräkning.
bool isTakenKey(const AutoContext& ctx, std::string_view docKey)
{
    return takenBy(ctx.rows, docKey) != nullptr;
}

// Spärrarna i planen 7.5. Raderna med de reserverade id:na och de tagna raderna får bara
// synkningen skriva, också när läget är av och raderna saknas.
enum class Guard { Change, Remove };

// Felet T1 (ändra) eller T2 (ta bort) för en automatisk beräkning i skedet.
Error autoGuardError(Stage stage, Guard guard)
{
    const std::string table(poleTableName(stage));
    if (guard == Guard::Remove)
        return makeError(ErrorCode::Invalid, "Den automatiska beräkningen kan inte tas bort. Stäng av automatiskt läge i " +
                                                 table + ".");
    return makeError(ErrorCode::Invalid,
                     "Den automatiska beräkningen räknas ur " + table + " och kan inte ändras.");
}

// Spärrar ett normalspannsid som är reserverat i auto_calc. id 0 (ny beräkning) släpps igenom.
Result<void> guardSpanId(db::Database& d, std::int64_t id, Guard guard)
{
    if (id == 0)
        return {};
    auto stage = firstText(d, "SELECT stage FROM auto_calc WHERE span_id = ?1", {id});
    if (!stage)
        return stage.error();
    if (stage.value())
        return autoGuardError(stageFromText(*stage.value()), guard);
    return {};
}

// Spärrar ett linberäkningsid som är reserverat i auto_calc. id 0 (ny beräkning) släpps igenom.
Result<void> guardCalcId(db::Database& d, std::int64_t id, Guard guard)
{
    if (id == 0)
        return {};
    auto stage = firstText(d, "SELECT stage FROM auto_calc WHERE calc_id = ?1", {id});
    if (!stage)
        return stage.error();
    if (stage.value())
        return autoGuardError(stageFromText(*stage.value()), guard);
    return {};
}

// Spärrar T3: raden docKey visar en automatisk beräkning och får inte kopplas om.
Result<void> guardRow(db::Database& d, std::string_view docKey)
{
    auto ctx = readAutoRows(d);
    if (!ctx)
        return ctx.error();
    if (const AutoRow* row = takenBy(ctx.value(), docKey))
        return makeError(ErrorCode::Invalid,
                         "Raden visar en automatisk beräkning ur " + std::string(poleTableName(row->stage)) +
                             ". Stäng av automatiskt läge där för att koppla en annan beräkning.");
    return {};
}

// Anteckningen på de automatiska beräkningarna (planen 7.7).
std::string autoNote(Stage stage, const std::string& word, const automode::PoleRange& range)
{
    std::string note = "Automatisk · " + std::string(poleTableName(stage)) + " · " + word;
    if (range.fromPole.empty() || range.toPole.empty())
        return note + ", sträckan är inte vald";
    return note + " " + range.fromPole + "–" + range.toPole;
}

// Sant för en stolpe utan Stpnr, spannlängd och H, som inte sparas.
bool isEmptyPole(const ancalc::SpanPole& pole)
{
    return isBlank(pole.pole) && !pole.lengthM && !pole.heightM;
}

// Sant när två stolplistor är lika i Stpnr, spannlängd och H.
bool samePoles(const std::vector<ancalc::SpanPole>& a, const std::vector<ancalc::SpanPole>& b)
{
    if (a.size() != b.size())
        return false;
    for (std::size_t i = 0; i < a.size(); ++i)
        if (a[i].pole != b[i].pole || a[i].lengthM != b[i].lengthM || a[i].heightM != b[i].heightM)
            return false;
    return true;
}

// Sant när två uppsättningar gränser är lika.
bool sameLimits(const ancalc::SpanLimits& a, const ancalc::SpanLimits& b)
{
    return a.longest == b.longest && a.shortest == b.shortest && a.adjacent == b.adjacent &&
           a.localMin == b.localMin && a.localMax == b.localMax;
}

// Skriver stolparna för normalspannet spanId med positionerna 0, 1, 2 ...
Result<void> writeSpanPoles(db::Database& d, std::int64_t spanId, const std::vector<ancalc::SpanPole>& list)
{
    std::int64_t position = 0;
    for (const ancalc::SpanPole& pole : list) {
        const Arg length = pole.lengthM ? Arg{*pole.lengthM} : Arg{nullptr};
        const Arg height = pole.heightM ? Arg{*pole.heightM} : Arg{nullptr};
        if (auto r = run(d, R"sql(
INSERT INTO normal_span_pole(span_id, position, pole, length_m, height_m) VALUES(?1, ?2, ?3, ?4, ?5)
)sql",
                         {spanId, position, std::string_view(pole.pole), length, height});
            !r)
            return r;
        ++position;
    }
    return {};
}

// Skapar den automatiska normalspannsraden med förvalda gränser. Med id angivet skapas den med
// just det id:t (läget slås på igen), annars ger AUTOINCREMENT ett nytt. Svarar med id.
Result<std::int64_t> insertAutoSpan(db::Database& d, std::optional<std::int64_t> id, const std::string& note,
                                    const std::string& now)
{
    const ancalc::SpanLimits limits;
    if (auto r = run(d, R"sql(
INSERT INTO normal_span(id, note, limit_longest, limit_shortest, limit_adjacent, limit_local_min,
                        limit_local_max, created_utc, modified_utc)
VALUES(?1, ?2, ?3, ?4, ?5, ?6, ?7, ?8, ?8)
)sql",
                     {id ? Arg{*id} : Arg{nullptr}, std::string_view(note), limits.longest, limits.shortest,
                      limits.adjacent, limits.localMin, limits.localMax, std::string_view(now)});
        !r)
        return r.error();
    if (id)
        return *id;
    auto rowId = firstInt(d, "SELECT last_insert_rowid()", {});
    if (!rowId)
        return rowId.error();
    return rowId.value().value_or(0);
}

// Gör den automatiska beräkningen för skedet och linan stämma med stolptabellen och sträckan:
// skapar saknade rader (med de reserverade id:na när de finns), och uppdaterar stolpar,
// anteckning och sträckans läsning, men bara det som skiljer sig.
Result<void> syncOneAuto(db::Database& d, const std::string& now, Stage stage, Wire wire, bool same,
                         const automode::PoleRange& range, const std::vector<poletable::PoleRow>& rows,
                         const AutoRow* existing)
{
    const std::string word = autoLineWord(wire, same);
    const automode::RangeSpan span = automode::rangeSpan(rows, range, word, poleTableName(stage));
    std::vector<ancalc::SpanPole> wanted;
    for (const ancalc::SpanPole& pole : span.poles)
        if (!isEmptyPole(pole))
            wanted.push_back(pole);
    const std::string note = autoNote(stage, word, range);

    std::int64_t spanId = existing ? existing->spanId : 0;
    std::int64_t calcId = existing ? existing->calcId : 0;
    const bool spanFresh = !existing || !existing->spanPresent;
    const bool calcFresh = !existing || !existing->calcPresent;

    if (spanFresh) {
        auto id = insertAutoSpan(d, existing ? std::optional<std::int64_t>{spanId} : std::nullopt, note, now);
        if (!id)
            return id.error();
        spanId = id.value();
        if (auto r = writeSpanPoles(d, spanId, wanted); !r)
            return r;
    } else {
        auto stored = readSpan(d, spanId);
        if (!stored)
            return stored.error();
        if (stored.value()) {
            const NormalSpan& ns = *stored.value();
            const ancalc::SpanLimits defaults;
            const bool polesDiffer = !samePoles(ns.poles, wanted);
            if (polesDiffer || ns.note != note || !sameLimits(ns.limits, defaults)) {
                if (auto r = run(d, R"sql(
UPDATE normal_span
   SET note = ?2, limit_longest = ?3, limit_shortest = ?4, limit_adjacent = ?5,
       limit_local_min = ?6, limit_local_max = ?7, modified_utc = ?8
 WHERE id = ?1
)sql",
                                 {spanId, std::string_view(note), defaults.longest, defaults.shortest,
                                  defaults.adjacent, defaults.localMin, defaults.localMax,
                                  std::string_view(now)});
                    !r)
                    return r;
            }
            if (polesDiffer) {
                if (auto r = run(d, "DELETE FROM normal_span_pole WHERE span_id = ?1", {spanId}); !r)
                    return r;
                if (auto r = writeSpanPoles(d, spanId, wanted); !r)
                    return r;
            }
        }
    }

    const std::string docSource = lineCalcDocKey(wire, stage);
    if (calcFresh) {
        LineCalc lc;
        lc.id = existing ? calcId : 0;
        lc.note = note;
        lc.docSource = docSource;
        lc.input = linecalc::newLineInput();
        auto stored = storeLineCalc(d, lc, docSource, now);
        if (!stored)
            return stored.error();
        calcId = stored.value();
    } else {
        auto oldNote = firstText(d, "SELECT note FROM line_calc WHERE id = ?1", {calcId});
        if (!oldNote)
            return oldNote.error();
        if (oldNote.value() && *oldNote.value() != note)
            if (auto r = run(d, "UPDATE line_calc SET note = ?2, modified_utc = ?3 WHERE id = ?1",
                             {calcId, std::string_view(note), std::string_view(now)});
                !r)
                return r;
    }

    if (!existing)
        return run(d, R"sql(
INSERT INTO auto_calc(stage, wire, span_id, calc_id, first_row, last_row, issue)
VALUES(?1, ?2, ?3, ?4, ?5, ?6, ?7)
)sql",
                   {stageText(stage), wireText(wire), spanId, calcId,
                    static_cast<std::int64_t>(span.rows.first), static_cast<std::int64_t>(span.rows.last),
                    std::string_view(span.rows.issue)});
    if (existing->firstRow != span.rows.first || existing->lastRow != span.rows.last ||
        existing->issue != span.rows.issue)
        return run(d, "UPDATE auto_calc SET first_row = ?3, last_row = ?4, issue = ?5 WHERE stage = ?1 AND wire = ?2",
                   {stageText(stage), wireText(wire), static_cast<std::int64_t>(span.rows.first),
                    static_cast<std::int64_t>(span.rows.last), std::string_view(span.rows.issue)});
    return {};
}

// Håller de automatiska beräkningarna aktuella (planen 6.4). För varje skede och lina tas raderna
// i normal_span och line_calc bort när linan inte är aktiv, och annars skapas eller uppdateras de
// ur stolptabellen och sträckan. Körs sist i varje skrivning som ändrar det som den läser:
// stolptabellerna, auto_mode och doc_value top_conductor_right.
Result<void> syncAutoCalcs(db::Database& d, const std::string& now)
{
    auto right = firstText(d, "SELECT value FROM doc_value WHERE key = 'top_conductor_right'", {});
    if (!right)
        return right.error();
    const bool same = !right.value() || right.value()->empty() || *right.value() == kSameConductor;
    auto existingRows = readAutoRows(d);
    if (!existingRows)
        return existingRows.error();

    for (Stage stage : kStages) {
        auto settings = readAutoSettings(d, stage);
        if (!settings)
            return settings.error();
        std::vector<poletable::PoleRow> rows;
        if (settings.value().enabled) {
            auto read = readPoles(d, stage);
            if (!read)
                return read.error();
            rows = std::move(read.value());
        }
        for (Wire wire : kWires) {
            const AutoRow* existing = nullptr;
            for (const AutoRow& row : existingRows.value())
                if (row.stage == stage && row.wire == wire)
                    existing = &row;
            const bool active = settings.value().enabled && (wire != Wire::TopRight || !same);
            if (!active) {
                if (existing) {
                    if (auto r = run(d, "DELETE FROM normal_span WHERE id = ?1", {existing->spanId}); !r)
                        return r;
                    if (auto r = run(d, "DELETE FROM line_calc WHERE id = ?1", {existing->calcId}); !r)
                        return r;
                }
                continue;
            }
            if (auto r = syncOneAuto(d, now, stage, wire, same, settings.value().ranges[wireIndex(wire)], rows,
                                     existing);
                !r)
                return r;
        }
    }
    return {};
}

// Lägger aₙ för normalspannsraderna i sources.normalSpans, sorterade på nyckel. En tagen rad
// visar sin automatiska beräkning: en lagrad koppling på raden räknas inte (den vilar), och
// nyckeln läggs i autoSpanKeys.
Result<void> addSpanSources(db::Database& d, const std::vector<std::pair<std::string, std::int64_t>>& links,
                            DocLineSources& sources)
{
    auto ctx = readAutoContext(d);
    if (!ctx)
        return ctx.error();
    for (const auto& [key, spanId] : links) {
        if (isTakenKey(ctx.value(), key))
            continue;
        auto span = readSpan(d, spanId);
        if (!span)
            return span.error();
        std::optional<double> an;
        if (span.value()) {
            const auto calc = calculateSpan(*span.value());
            if (calc.result)
                an = calc.result->an;
        }
        sources.normalSpans.emplace_back(key, an);
    }
    for (const AutoRow& row : ctx.value().rows) {
        if (!row.spanPresent)
            continue;
        auto span = readSpan(d, row.spanId);
        if (!span)
            return span.error();
        if (!span.value())
            continue;
        decorateSpan(*span.value(), ctx.value());
        std::optional<double> an;
        const auto calc = calculateSpan(*span.value());
        if (calc.result)
            an = calc.result->an;
        const std::string key = normalSpanDocKey(row.wire, row.stage);
        sources.normalSpans.emplace_back(key, an);
        sources.autoSpanKeys.push_back(key);
    }
    std::sort(sources.normalSpans.begin(), sources.normalSpans.end(),
              [](const auto& a, const auto& b) { return a.first < b.first; });
    return {};
}

} // namespace

// Nyckeln för normalspannsraden: ns_calc_<lina>_<skede>.
std::string normalSpanDocKey(Wire wire, Stage stage)
{
    std::string key = "ns_calc_";
    key += wire == Wire::Phase ? "phase" : wire == Wire::TopLeft ? "top_left" : "top_right";
    key += stage == Stage::Before ? "_before" : "_after";
    return key;
}

// Normalspannet för en beräkning. En automatisk beräkning vars sträcka inte går att använda
// ger sträckans fel och inget resultat, annars räknas stolparna.
ancalc::SpanCalculation calculateSpan(const NormalSpan& ns)
{
    if (ns.automatic && !ns.automatic->issue.empty()) {
        ancalc::SpanCalculation calc;
        calc.issues.push_back({ancalc::IssueKind::Error, -1, ancalc::SpanColumn::None, ns.automatic->issue});
        return calc;
    }
    return ancalc::calculateNormalSpan(ns.poles, ns.limits);
}

// Tabellens namn i de automatiska lägets texter.
std::string_view poleTableName(Stage stage)
{
    return stage == Stage::Before ? "Stolptabell före" : "Stolptabell efter";
}

// Linans ord i texterna. Topplina vänster heter topplinorna när topplina höger är Samma.
std::string autoLineWord(Wire wire, bool same)
{
    if (wire == Wire::Phase)
        return "faslinan";
    if (wire == Wire::TopLeft)
        return same ? "topplinorna" : "topplina vänster";
    return "topplina höger";
}

// Linans rubrik i panelen, med samma regel för Samma som autoLineWord.
std::string autoLineLabel(Wire wire, bool same)
{
    if (wire == Wire::Phase)
        return "Faslina";
    if (wire == Wire::TopLeft)
        return same ? "Topplinor" : "Topplina vänster";
    return "Topplina höger";
}

// Läser alla rader i doc_value i nyckelordning.
Result<std::vector<DocValue>> docValues(ProjectDb& p)
{
    return readDocValues(p.local());
}

// Lägger in, ersätter eller tar bort en uppgift. På en linrad kontrolleras värdet mot
// projektets linor, och raden får kopians namn. Rensar oanvända linor efter en linrad.
Result<void> setDocValue(ProjectDb& p, const DocValue& v)
{
    return write(p, [&](db::Database& d) -> Result<void> {
        if (v.key.empty())
            return makeError(ErrorCode::Invalid, "Uppgiften saknar nyckel.");
        const bool lineKey = isConductorKey(v.key);
        std::string value = v.value;
        const bool same = v.key == kTopRightConductorKey && value == kSameConductor;
        if (lineKey && !value.empty() && !same) {
            auto name = firstText(d, "SELECT name FROM project_conductor WHERE name = ?1",
                                  {std::string_view(value)});
            if (!name)
                return name.error();
            if (!name.value())
                return makeError(ErrorCode::Invalid, "Linan " + value + " finns inte i projektet.");
            value = *name.value();
        }
        if (value.empty() && v.reference.empty()) {
            if (auto r = run(d, "DELETE FROM doc_value WHERE key = ?1", {std::string_view(v.key)}); !r)
                return r;
        } else if (auto r = upsertDocValue(d, v.key, value, v.reference); !r) {
            return r;
        }
        if (lineKey)
            if (auto r = pruneConductors(d); !r)
                return r;
        // Samma eller en egen topplina höger ändrar vilka linor som är automatiska.
        if (v.key == kTopRightConductorKey)
            return syncAutoCalcs(d, platform::nowUtcIso());
        return Result<void>{};
    });
}

// Alla linor i projektet, sorterade på namn.
Result<std::vector<Conductor>> projectConductors(ProjectDb& p)
{
    return readConductors(p.local());
}

// Sätter linan på en linrad. Radens referens står kvar, och oanvända linor tas bort.
Result<void> setDocConductor(ProjectDb& p, std::string_view key, const Conductor& chosen)
{
    return write(p, [&](db::Database& d) -> Result<void> {
        if (!isConductorKey(key))
            return makeError(ErrorCode::Invalid, "Raden " + std::string(key) + " är ingen linrad.");
        auto name = ensureConductor(d, chosen);
        if (!name)
            return name.error();
        auto reference = firstText(d, "SELECT reference FROM doc_value WHERE key = ?1", {key});
        if (!reference)
            return reference.error();
        if (auto r = upsertDocValue(d, std::string(key), name.value(), reference.value().value_or("")); !r)
            return r;
        if (auto r = pruneConductors(d); !r)
            return r;
        if (key == kTopRightConductorKey)
            return syncAutoCalcs(d, platform::nowUtcIso());
        return Result<void>{};
    });
}

// Ersätter kopians typ och värden med fresh:s. Namn och id behålls.
Result<void> refreshProjectConductor(ProjectDb& p, const Conductor& fresh)
{
    return write(p, [&](db::Database& d) -> Result<void> {
        if (auto valid = checkConductor(fresh); !valid)
            return valid;
        auto id = firstInt(d, "SELECT id FROM project_conductor WHERE name = ?1",
                           {std::string_view(fresh.name)});
        if (!id)
            return id.error();
        if (!id.value())
            return conductorNotFound(fresh.name);
        const std::string now = platform::nowUtcIso();
        return run(d, R"sql(
UPDATE project_conductor
   SET type = ?2, area_mm2 = ?3, diameter_mm = ?4, e_initial_mpa = ?5, e_permanent_mpa = ?6,
       e_initial_limit_mpa = ?7, eiv_constant = ?8, alpha_e6_per_c = ?9,
       creep_strain_permille = ?10, creep_temp_c = ?11, weight_n_per_m = ?12,
       breaking_load_kn = ?13, copied_utc = ?14
 WHERE id = ?1
)sql",
                   {*id.value(), std::string_view(fresh.type), fresh.areaMm2, fresh.diameterMm,
                    fresh.eInitialMpa, fresh.ePermanentMpa, fresh.eInitialLimitMpa,
                    fresh.eivConstant, fresh.alphaE6PerC, fresh.creepStrainPermille,
                    fresh.creepTempC, fresh.weightNPerM, breakingLoadArg(fresh),
                    std::string_view(now)});
    });
}

// Läser alla beräkningar: de automatiska först, sedan de övriga efter id.
Result<std::vector<NormalSpan>> normalSpans(ProjectDb& p)
{
    auto ctx = readAutoContext(p.local());
    if (!ctx)
        return ctx.error();
    std::vector<std::int64_t> ids;
    for (const AutoRow& row : ctx.value().rows)
        if (row.spanPresent)
            ids.push_back(row.spanId);
    {
        auto stmt = prepared(p.local(), "SELECT id FROM normal_span ORDER BY id", {});
        if (!stmt)
            return stmt.error();
        while (true) {
            auto row = stmt.value().step();
            if (!row)
                return row.error();
            if (!row.value())
                break;
            const std::int64_t id = stmt.value().integer(0);
            if (!autoRowOfSpan(ctx.value(), id))
                ids.push_back(id);
        }
    }
    std::vector<NormalSpan> spans;
    for (std::int64_t id : ids) {
        auto span = readSpan(p.local(), id);
        if (!span)
            return span.error();
        if (!span.value())
            continue;
        decorateSpan(*span.value(), ctx.value());
        spans.push_back(std::move(*span.value()));
    }
    return spans;
}

// Läser en beräkning med märkning och rader som i normalSpans. Tomt svar när id saknas.
Result<std::optional<NormalSpan>> normalSpan(ProjectDb& p, std::int64_t id)
{
    auto ctx = readAutoContext(p.local());
    if (!ctx)
        return ctx.error();
    auto span = readSpan(p.local(), id);
    if (!span)
        return span.error();
    if (span.value())
        decorateSpan(*span.value(), ctx.value());
    return span;
}

// Sparar en ny eller befintlig beräkning med alla stolpar i en transaktion. Gränser och tal
// valideras först, och raden linkDocKey (om satt) kopplas.
Result<std::int64_t> saveNormalSpan(ProjectDb& p, const NormalSpan& ns, std::string_view linkDocKey)
{
    std::int64_t savedId = 0;
    auto done = write(p, [&](db::Database& d) -> Result<void> {
        if (auto g = guardSpanId(d, ns.id, Guard::Change); !g)
            return g;
        if (!linkDocKey.empty())
            if (auto g = guardRow(d, linkDocKey); !g)
                return g;
        const double limits[] = {ns.limits.longest, ns.limits.shortest, ns.limits.adjacent,
                                 ns.limits.localMin, ns.limits.localMax};
        for (double limit : limits)
            if (!std::isfinite(limit) || limit <= 0)
                return makeError(ErrorCode::Invalid, "Gränserna måste vara tal större än 0.");
        for (const ancalc::SpanPole& pole : ns.poles) {
            if ((pole.lengthM && !std::isfinite(*pole.lengthM)) ||
                (pole.heightM && !std::isfinite(*pole.heightM)))
                return makeError(ErrorCode::Invalid, "Ogiltigt tal.");
        }
        if (!linkDocKey.empty() && !isNormalSpanKey(linkDocKey))
            return makeError(ErrorCode::Invalid,
                             "Raden " + std::string(linkDocKey) +
                                 " kan inte visa en normalspannsberäkning.");

        if (ns.id != 0) {
            auto exists = firstInt(d, "SELECT id FROM normal_span WHERE id = ?1", {ns.id});
            if (!exists)
                return exists.error();
            if (!exists.value())
                return spanNotFound();
        }

        const std::string now = platform::nowUtcIso();

        if (ns.id == 0) {
            if (auto r = run(d, R"sql(
INSERT INTO normal_span(note, limit_longest, limit_shortest, limit_adjacent,
                        limit_local_min, limit_local_max, created_utc, modified_utc)
VALUES(?1, ?2, ?3, ?4, ?5, ?6, ?7, ?7)
)sql",
                             {std::string_view(ns.note), ns.limits.longest,
                              ns.limits.shortest, ns.limits.adjacent, ns.limits.localMin,
                              ns.limits.localMax, std::string_view(now)});
                !r)
                return r;
            auto id = firstInt(d, "SELECT last_insert_rowid()", {});
            if (!id)
                return id.error();
            savedId = id.value().value_or(0);
        } else {
            savedId = ns.id;
            if (auto r = run(d, R"sql(
UPDATE normal_span
   SET note = ?2, limit_longest = ?3, limit_shortest = ?4, limit_adjacent = ?5,
       limit_local_min = ?6, limit_local_max = ?7, modified_utc = ?8
 WHERE id = ?1
)sql",
                             {savedId, std::string_view(ns.note), ns.limits.longest,
                              ns.limits.shortest, ns.limits.adjacent, ns.limits.localMin,
                              ns.limits.localMax, std::string_view(now)});
                !r)
                return r;
            if (auto r = run(d, "DELETE FROM normal_span_pole WHERE span_id = ?1", {savedId}); !r)
                return r;
        }

        // Helt tomma stolpar sparas inte; de övriga får positionerna 0, 1, 2 ...
        std::vector<ancalc::SpanPole> kept;
        kept.reserve(ns.poles.size());
        for (const ancalc::SpanPole& pole : ns.poles)
            if (!isEmptyPole(pole))
                kept.push_back(pole);
        if (auto r = writeSpanPoles(d, savedId, kept); !r)
            return r;

        if (!linkDocKey.empty())
            return linkInside(d, savedId, linkDocKey);
        return Result<void>{};
    });
    if (!done)
        return done.error();
    return savedId;
}

// Kopplar raden docKey till beräkningen id. Radens manuella värde töms.
Result<void> linkNormalSpan(ProjectDb& p, std::int64_t id, std::string_view docKey)
{
    return write(p, [&](db::Database& d) -> Result<void> {
        if (auto g = guardSpanId(d, id, Guard::Change); !g)
            return g;
        if (auto g = guardRow(d, docKey); !g)
            return g;
        return linkInside(d, id, docKey);
    });
}

// Tar bort kopplingen på raden docKey, för normalspann och linberäkning. Beräkningarna finns
// kvar, och en kopplad linberäkning blir manuell med sina senaste värden som egna.
Result<void> unlinkDocRow(ProjectDb& p, std::string_view docKey)
{
    return write(p, [&](db::Database& d) -> Result<void> {
        if (auto g = guardRow(d, docKey); !g)
            return g;
        auto current = firstInt(d, "SELECT calc_id FROM doc_line_calc WHERE doc_key = ?1", {docKey});
        if (!current)
            return current.error();
        if (current.value()) {
            auto source = firstText(d, "SELECT COALESCE(doc_source, '') FROM line_calc WHERE id = ?1",
                                    {*current.value()});
            if (!source)
                return source.error();
            if (source.value() && !source.value()->empty())
                if (auto r = detachInside(d, *current.value()); !r)
                    return r;
        }
        if (auto r = run(d, "DELETE FROM doc_line_calc WHERE doc_key = ?1", {docKey}); !r)
            return r;
        return run(d, "DELETE FROM doc_normal_span WHERE doc_key = ?1", {docKey});
    });
}

// Tar bort beräkningen. Stolpar och kopplingar följer med (ON DELETE CASCADE).
Result<void> removeNormalSpan(ProjectDb& p, std::int64_t id)
{
    return write(p, [&](db::Database& d) -> Result<void> {
        if (auto g = guardSpanId(d, id, Guard::Remove); !g)
            return g;
        return run(d, "DELETE FROM normal_span WHERE id = ?1", {id});
    });
}

// Nyckeln för linberäkningsraden: line_calc_<lina>_<skede>.
std::string lineCalcDocKey(Wire wire, Stage stage)
{
    std::string key = "line_calc_";
    key += wire == Wire::Phase ? "phase" : wire == Wire::TopLeft ? "top_left" : "top_right";
    key += stage == Stage::Before ? "_before" : "_after";
    return key;
}

// Lina och skede för en linberäkningsnyckel, genom att jämföra med de sex nycklarna.
std::optional<std::pair<Wire, Stage>> lineCalcRowOf(std::string_view docKey)
{
    for (Wire wire : {Wire::Phase, Wire::TopLeft, Wire::TopRight})
        for (Stage stage : {Stage::Before, Stage::After})
            if (docKey == lineCalcDocKey(wire, stage))
                return std::pair<Wire, Stage>{wire, stage};
    return std::nullopt;
}

// Läser Dokumentationens värden, projektets linor och aₙ för de kopplade normalspannsraderna.
Result<DocLineSources> docLineSources(ProjectDb& p)
{
    return readLineSources(p.local());
}

// Sätter in Dokumentationens värden i indata enligt planen fas 2, 4.2. Ren funktion: allt som
// behövs finns i docs. Det som saknas ger en Missing-upplysning och aldrig ett reservvärde.
ResolvedLineInput resolveLineInput(const linecalc::LineInput& input, std::string_view docSource,
                                   const DocLineSources& docs)
{
    ResolvedLineInput out;
    out.input = input;
    if (docSource.empty())
        return out;
    const auto row = lineCalcRowOf(docSource);
    if (!row) {
        out.issues.push_back(missingIssue(linecalc::LineField::None,
                                          "Raden " + std::string(docSource) +
                                              " finns inte i Dokumentation."));
        return out;
    }

    // Topplina höger "Samma" (eller ingen rad, som är förvalet) löses upp som vänster.
    const std::string right = docText(docs, kTopRightConductorKey);
    const bool same = right.empty() || right == kSameConductor;
    Wire wire = row->first;
    if (wire == Wire::TopRight && same)
        wire = Wire::TopLeft;
    const WireNames names = wireNames(wire, same);
    const Stage stage = row->second;
    const std::string stageText = stage == Stage::Before ? "före" : "efter";
    const std::string spanRow = names.rowWord + " normalspannsberäkning " + stageText + " ombyggnad";
    const std::string fillIn = " i fliken Dokumentation.";

    // Linan: namnet på linraden, och projektets kopia med det namnet.
    out.input.conductor.reset();
    const std::string conductorName = docText(docs, names.conductorKey);
    if (conductorName.empty()) {
        out.issues.push_back(
            missingIssue(linecalc::LineField::Conductor, "Fyll i " + names.conductorLabel + fillIn));
    } else {
        const Conductor* found = nullptr;
        for (const Conductor& c : docs.conductors)
            if (conductor::sameConductorName(c.name, conductorName))
                found = &c;
        if (found) {
            out.input.conductor = *found;
            out.sources.push_back({LineSourceField::Conductor, names.conductorKey});
        } else {
            out.issues.push_back(missingIssue(
                linecalc::LineField::Conductor,
                "Linan " + conductorName +
                    " finns inte i projektet. Välj linan igen i fliken Dokumentation."));
        }
    }

    // aₙ: den kopplade beräkningen, sedan radens manuella värde och sist den gröna raden.
    out.input.normalSpanM.reset();
    const std::string spanKey = normalSpanDocKey(wire, stage);
    const std::pair<std::string, std::optional<double>>* linkedSpan = nullptr;
    for (const auto& entry : docs.normalSpans)
        if (entry.first == spanKey)
            linkedSpan = &entry;
    if (linkedSpan && !linkedSpan->second) {
        const bool automatic = std::find(docs.autoSpanKeys.begin(), docs.autoSpanKeys.end(), spanKey) !=
                               docs.autoSpanKeys.end();
        const std::string fix = automatic ? "Rätta sträckan eller stolparna i " +
                                                std::string(poleTableName(stage)) + "."
                                          : "Rätta den i fliken Normalspann.";
        out.issues.push_back(
            missingIssue(linecalc::LineField::NormalSpan, spanRow + " har fel och ger inget aₙ. " + fix));
    } else if (linkedSpan) {
        out.input.normalSpanM = linkedSpan->second;
        out.sources.push_back({LineSourceField::NormalSpan, spanKey});
    } else if (const auto manual = docNumber(docs, spanKey)) {
        out.input.normalSpanM = manual;
        out.sources.push_back({LineSourceField::NormalSpan, spanKey});
    } else if (const auto green = docNumber(docs, names.spanKey)) {
        out.input.normalSpanM = green;
        out.sources.push_back({LineSourceField::NormalSpan, names.spanKey});
    } else {
        out.issues.push_back(missingIssue(
            linecalc::LineField::NormalSpan,
            "Fyll i Normalspann " + names.word + " eller " + spanRow + fillIn));
    }

    // σ₀ ur uppspänningsraden.
    out.input.tensionMpa.reset();
    if (const auto tension = docNumber(docs, names.tensionKey)) {
        out.input.tensionMpa = tension;
        out.sources.push_back({LineSourceField::Tension, names.tensionKey});
    } else {
        out.issues.push_back(
            missingIssue(linecalc::LineField::Tension, "Fyll i " + names.tensionLabel + fillIn));
    }

    // Min och max: ett nytt värde sätts med arkets följdändringar, ett som saknas lämnar talet.
    if (const auto minTemp = docNumber(docs, "temp_min")) {
        if (*minTemp != out.input.minTempC) {
            out.input.minTempC = *minTemp;
            linecalc::applyInputChange(out.input, linecalc::InputChange::MinTemp);
        }
        out.sources.push_back({LineSourceField::MinTemp, "temp_min"});
    } else {
        out.issues.push_back(
            missingIssue(linecalc::LineField::MinTemp, "Fyll i Min temp" + fillIn));
    }
    if (const auto maxTemp = docNumber(docs, "temp_max")) {
        if (*maxTemp != out.input.maxTempC) {
            out.input.maxTempC = *maxTemp;
            linecalc::applyInputChange(out.input, linecalc::InputChange::MaxTemp);
        }
        out.sources.push_back({LineSourceField::MaxTemp, "temp_max"});
    } else {
        out.issues.push_back(
            missingIssue(linecalc::LineField::MaxTemp, "Fyll i Max temp" + fillIn));
    }
    return out;
}

// Räknar den upplösta beräkningen. Upplösningens upplysningar står först, och beräkningens
// egna Missing för samma fält tas bort. Med en Missing ur upplösningen finns inget resultat.
linecalc::LineCalculation calculateResolved(const ResolvedLineInput& resolved)
{
    linecalc::LineCalculation calc = linecalc::calculateLine(resolved.input);
    if (resolved.issues.empty())
        return calc;
    linecalc::LineCalculation out;
    out.issues = resolved.issues;
    for (const linecalc::LineIssue& issue : calc.issues) {
        bool duplicate = false;
        if (issue.kind == linecalc::LineIssueKind::Missing && issue.field != linecalc::LineField::None)
            for (const linecalc::LineIssue& own : resolved.issues)
                if (own.field == issue.field)
                    duplicate = true;
        if (!duplicate)
            out.issues.push_back(issue);
    }
    return out;
}

// Läser alla linberäkningar: de automatiska först, sedan de övriga efter id.
Result<std::vector<LineCalc>> lineCalcs(ProjectDb& p)
{
    auto ctx = readAutoContext(p.local());
    if (!ctx)
        return ctx.error();
    std::vector<std::int64_t> ids;
    for (const AutoRow& row : ctx.value().rows)
        if (row.calcPresent)
            ids.push_back(row.calcId);
    {
        auto stmt = prepared(p.local(), "SELECT id FROM line_calc ORDER BY id", {});
        if (!stmt)
            return stmt.error();
        while (true) {
            auto row = stmt.value().step();
            if (!row)
                return row.error();
            if (!row.value())
                break;
            const std::int64_t id = stmt.value().integer(0);
            if (!autoRowOfCalc(ctx.value(), id))
                ids.push_back(id);
        }
    }
    std::vector<LineCalc> calcs;
    for (std::int64_t id : ids) {
        auto calc = readLineCalc(p.local(), id);
        if (!calc)
            return calc.error();
        if (!calc.value())
            continue;
        decorateCalc(*calc.value(), ctx.value());
        calcs.push_back(std::move(*calc.value()));
    }
    return calcs;
}

// Läser en linberäkning med märkning och rader som i lineCalcs. Tomt svar när id saknas.
Result<std::optional<LineCalc>> lineCalc(ProjectDb& p, std::int64_t id)
{
    auto ctx = readAutoContext(p.local());
    if (!ctx)
        return ctx.error();
    auto calc = readLineCalc(p.local(), id);
    if (!calc)
        return calc.error();
    if (calc.value())
        decorateCalc(*calc.value(), ctx.value());
    return calc;
}

// Sparar en ny eller befintlig linberäkning med de 15 lastfallen i en transaktion och kopplar
// den till sin rad. En kopplad beräkning kopplas till docSource och bort från alla andra rader.
Result<std::int64_t> saveLineCalc(ProjectDb& p, const LineCalc& lc, std::string_view linkDocKey)
{
    std::int64_t savedId = 0;
    auto done = write(p, [&](db::Database& d) -> Result<void> {
        if (auto g = guardCalcId(d, lc.id, Guard::Change); !g)
            return g;
        if (!linkDocKey.empty())
            if (auto g = guardRow(d, linkDocKey); !g)
                return g;
        if (lc.id == 0 && !lc.docSource.empty())
            if (auto g = guardRow(d, lc.docSource); !g)
                return g;
        if (!allFinite(lc.input))
            return makeError(ErrorCode::Invalid, "Ogiltigt tal.");
        if (!lc.docSource.empty() && !isLineCalcKey(lc.docSource))
            return notLineCalcKey(lc.docSource);
        if (!linkDocKey.empty() && !isLineCalcKey(linkDocKey))
            return notLineCalcKey(linkDocKey);
        if (!lc.docSource.empty() && !linkDocKey.empty() && lc.docSource != linkDocKey)
            return bondedToOneRow();
        if (lc.id != 0) {
            auto exists = firstInt(d, "SELECT id FROM line_calc WHERE id = ?1", {lc.id});
            if (!exists)
                return exists.error();
            if (!exists.value())
                return lineCalcNotFound();
        }

        auto stored = storeLineCalc(d, lc, lc.docSource, platform::nowUtcIso());
        if (!stored)
            return stored.error();
        savedId = stored.value();

        if (!lc.docSource.empty()) {
            if (auto r = run(d, "DELETE FROM doc_line_calc WHERE calc_id = ?1 AND doc_key <> ?2",
                             {savedId, std::string_view(lc.docSource)});
                !r)
                return r;
            return linkLineInside(d, savedId, lc.docSource);
        }
        if (!linkDocKey.empty())
            return linkLineInside(d, savedId, linkDocKey);
        return Result<void>{};
    });
    if (!done)
        return done.error();
    return savedId;
}

// Kopplar raden docKey till linberäkningen id. Radens manuella värde töms.
Result<void> linkLineCalc(ProjectDb& p, std::int64_t id, std::string_view docKey)
{
    return write(p, [&](db::Database& d) -> Result<void> {
        if (auto g = guardCalcId(d, id, Guard::Change); !g)
            return g;
        if (auto g = guardRow(d, docKey); !g)
            return g;
        return linkLineInside(d, id, docKey);
    });
}

// Gör en kopplad linberäkning manuell. Kopplingen till raden står kvar.
Result<void> detachLineCalc(ProjectDb& p, std::int64_t id)
{
    return write(p, [&](db::Database& d) -> Result<void> {
        if (auto g = guardCalcId(d, id, Guard::Change); !g)
            return g;
        return detachInside(d, id);
    });
}

// Tar bort linberäkningen. Lastfall och kopplingar följer med (ON DELETE CASCADE).
Result<void> removeLineCalc(ProjectDb& p, std::int64_t id)
{
    return write(p, [&](db::Database& d) -> Result<void> {
        if (auto g = guardCalcId(d, id, Guard::Remove); !g)
            return g;
        return run(d, "DELETE FROM line_calc WHERE id = ?1", {id});
    });
}

// Läser stolptabellen för skedet i tabellens ordning. NULL i en talkolumn blir tom text.
Result<std::vector<poletable::PoleRow>> poles(ProjectDb& p, Stage stage)
{
    return readPoles(p.local(), stage);
}

namespace {

// Kontrollerar att alla tal i raderna är kanonisk decimaltext. Det första felet ger Invalid med
// tabellens namn, radens nummer (1-baserat) och kolumnens namn.
Result<void> validatePoleRows(Stage stage, const std::vector<poletable::PoleRow>& rows)
{
    for (std::size_t i = 0; i < rows.size(); ++i) {
        if (const auto bad = poletable::firstInvalidColumn(rows[i]))
            return makeError(ErrorCode::Invalid,
                             std::string(poleTableName(stage)) + " har ett ogiltigt tal på rad " +
                                 std::to_string(i + 1) + " i kolumnen " + poletable::columnName(*bad) +
                                 ".");
    }
    return {};
}

// Ersätter raderna i stolptabellen för skedet: tömmer tabellen och skriver poletable::rowsToSave(rows)
// med positionerna 0, 1, 2 ... Tomma tal binds som NULL. Ingen validering och ingen synkning här.
Result<void> writePoleRows(db::Database& d, Stage stage, const std::vector<poletable::PoleRow>& rows)
{
    const std::string table(poleTableSql(stage));
    if (auto r = run(d, "DELETE FROM " + table, {}); !r)
        return r;
    const std::string insert = "INSERT INTO " + table +
                               "(position, pole, av, ah, span_m, coord_n, coord_e, coord_h, lkl) "
                               "VALUES(?1, ?2, ?3, ?4, ?5, ?6, ?7, ?8, ?9)";
    const std::vector<poletable::PoleRow> toSave = poletable::rowsToSave(rows);
    std::int64_t position = 0;
    for (const poletable::PoleRow& row : toSave) {
        // Tom text i en talkolumn blir NULL, text i textkolumnerna sparas som den är.
        const auto number = [&](int column) -> Arg {
            const std::string& text = poletable::field(row, column);
            return text.empty() ? Arg{nullptr} : Arg{std::string_view(text)};
        };
        if (auto r = run(d, insert,
                         {position, std::string_view(row.pole), number(poletable::kAv),
                          number(poletable::kAh), number(poletable::kSpan),
                          number(poletable::kNorth), number(poletable::kEast),
                          number(poletable::kHeight), std::string_view(row.lkl)});
            !r)
            return r;
        ++position;
    }
    return {};
}

} // namespace

// Ersätter hela stolptabellen för skedet i en transaktion. Talen valideras först, och en tom rad
// sist sparas inte. Tomma tal binds som NULL. De automatiska beräkningarna följer med.
Result<void> setPoles(ProjectDb& p, Stage stage, const std::vector<poletable::PoleRow>& rows)
{
    return write(p, [&](db::Database& d) -> Result<void> {
        if (auto v = validatePoleRows(stage, rows); !v)
            return v;
        if (auto w = writePoleRows(d, stage, rows); !w)
            return w;
        return syncAutoCalcs(d, platform::nowUtcIso());
    });
}

// Stolptabell före är stolptabellen för skedet före.
Result<std::vector<poletable::PoleRow>> polesBefore(ProjectDb& p)
{
    return poles(p, Stage::Before);
}

// Ersätter Stolptabell före, som setPoles.
Result<void> setPolesBefore(ProjectDb& p, const std::vector<poletable::PoleRow>& rows)
{
    return setPoles(p, Stage::Before, rows);
}

// Läser automatiskt läge för skedet. Utan rad i auto_mode: av och inga sträckor.
Result<AutoSettings> autoSettings(ProjectDb& p, Stage stage)
{
    return readAutoSettings(p.local(), stage);
}

// Sparar läget och sträckorna för skedet (Stolpplats trimmas), och synkar de automatiska
// beräkningarna i samma transaktion.
Result<void> setAutoSettings(ProjectDb& p, Stage stage, const AutoSettings& s)
{
    return write(p, [&](db::Database& d) -> Result<void> {
        std::array<std::string, 6> texts;
        for (std::size_t i = 0; i < s.ranges.size(); ++i) {
            texts[2 * i] = tabletext::trimmed(s.ranges[i].fromPole);
            texts[2 * i + 1] = tabletext::trimmed(s.ranges[i].toPole);
        }
        if (auto r = run(d, R"sql(
INSERT INTO auto_mode(stage, enabled, phase_from, phase_to, top_left_from, top_left_to,
                      top_right_from, top_right_to)
VALUES(?1, ?2, ?3, ?4, ?5, ?6, ?7, ?8)
ON CONFLICT(stage) DO UPDATE SET
    enabled = excluded.enabled, phase_from = excluded.phase_from, phase_to = excluded.phase_to,
    top_left_from = excluded.top_left_from, top_left_to = excluded.top_left_to,
    top_right_from = excluded.top_right_from, top_right_to = excluded.top_right_to
)sql",
                         {stageText(stage), std::int64_t{s.enabled ? 1 : 0}, std::string_view(texts[0]),
                          std::string_view(texts[1]), std::string_view(texts[2]), std::string_view(texts[3]),
                          std::string_view(texts[4]), std::string_view(texts[5])});
            !r)
            return r;
        return syncAutoCalcs(d, platform::nowUtcIso());
    });
}

// Gör Stolptabell efter till en kopia av före: raderna, sträckorna och läget. Det som fanns i
// efter försvinner, och de automatiska beräkningarna synkas i samma transaktion.
Result<void> copyBeforeToAfter(ProjectDb& p)
{
    return write(p, [&](db::Database& d) -> Result<void> {
        if (auto r = run(d, "DELETE FROM pole_after", {}); !r)
            return r;
        if (auto r = run(d, R"sql(
INSERT INTO pole_after(position, pole, av, ah, span_m, coord_n, coord_e, coord_h, lkl)
SELECT position, pole, av, ah, span_m, coord_n, coord_e, coord_h, lkl FROM pole_before
)sql",
                         {});
            !r)
            return r;
        if (auto r = run(d, "DELETE FROM auto_mode WHERE stage = 'after'", {}); !r)
            return r;
        if (auto r = run(d, R"sql(
INSERT INTO auto_mode(stage, enabled, phase_from, phase_to, top_left_from, top_left_to,
                      top_right_from, top_right_to)
SELECT 'after', enabled, phase_from, phase_to, top_left_from, top_left_to, top_right_from, top_right_to
  FROM auto_mode WHERE stage = 'before'
)sql",
                         {});
            !r)
            return r;
        return syncAutoCalcs(d, platform::nowUtcIso());
    });
}

namespace {

// Fältens namn i pole_import_column.field, i poletable-ordning (kPole … kLkl).
constexpr std::array<std::string_view, poletable::kColumnCount> kImportFieldNames = {
    "pole", "coord_n", "coord_e", "coord_h", "span_m", "ah", "av", "lkl"};

// Lagringstexten för en cells slag i pole_import_cell.kind.
std::string_view importKindText(poletable::ImportCellKind kind)
{
    switch (kind) {
    case poletable::ImportCellKind::Number: return "number";
    case poletable::ImportCellKind::Bool: return "bool";
    case poletable::ImportCellKind::Error: return "error";
    case poletable::ImportCellKind::Text: break;
    }
    return "text";
}

// Cellens slag ur lagringstexten. Tomt svar för en okänd text.
std::optional<poletable::ImportCellKind> importKindFromText(const std::string& text)
{
    if (text == "text")
        return poletable::ImportCellKind::Text;
    if (text == "number")
        return poletable::ImportCellKind::Number;
    if (text == "bool")
        return poletable::ImportCellKind::Bool;
    if (text == "error")
        return poletable::ImportCellKind::Error;
    return std::nullopt;
}

// Felet när rådataposten inte hänger ihop med raderna.
Error importIncomplete(std::string detail)
{
    return makeError(ErrorCode::Invalid, "Importen av stolptabellen är ofullständig och sparades inte.",
                     std::move(detail));
}

// Kontrollerar att posten hänger ihop med raderna: lika många, samma Stolpnr, inga tomma rader,
// unika kolumner, fält och Excelrader, och celler som pekar på kända kolumner.
Result<void> validateImport(const std::vector<poletable::PoleRow>& rows, const poletable::ImportRecord& record)
{
    if (rows.size() != record.rows.size())
        return importIncomplete("Antalet rader i raderna och i rådata skiljer sig.");
    for (std::size_t i = 0; i < rows.size(); ++i) {
        if (rows[i].pole != record.rows[i].pole)
            return importIncomplete("Stolpnr på rad " + std::to_string(i + 1) + " skiljer sig.");
        if (poletable::isEmptyRow(rows[i]))
            return importIncomplete("Rad " + std::to_string(i + 1) + " är tom.");
    }
    std::set<int> columns;
    std::set<int> fields;
    for (const poletable::ImportColumn& c : record.columns) {
        if (!columns.insert(c.column).second)
            return importIncomplete("Kolumn " + std::to_string(c.column) + " finns flera gånger.");
        if (c.field >= poletable::kColumnCount || c.field < -1)
            return importIncomplete("Okänt fält för kolumn " + std::to_string(c.column) + ".");
        if (c.field >= 0 && !fields.insert(c.field).second)
            return importIncomplete("Fältet " + std::string(kImportFieldNames[c.field]) +
                                    " har flera kolumner.");
    }
    std::set<int> excelRows;
    for (const poletable::ImportRow& r : record.rows) {
        if (!excelRows.insert(r.excelRow).second)
            return importIncomplete("Excelrad " + std::to_string(r.excelRow) + " finns flera gånger.");
        std::set<int> cellColumns;
        for (const poletable::ImportCell& cell : r.cells) {
            if (!columns.contains(cell.column))
                return importIncomplete("En cell på rad " + std::to_string(r.excelRow) +
                                        " pekar på en kolumn som saknas.");
            if (!cellColumns.insert(cell.column).second)
                return importIncomplete("Kolumn " + std::to_string(cell.column) + " finns flera gånger på rad " +
                                        std::to_string(r.excelRow) + ".");
        }
    }
    return {};
}

// Skriver posten: tömmer de fyra tabellerna (celler först) och lägger in raden, kolumnerna, raderna
// och cellerna med en förberedd sats per tabell. Tidpunkten är importedUtc.
Result<void> writeImportRecord(db::Database& d, const poletable::ImportRecord& record,
                               const std::string& importedUtc)
{
    for (const char* table : {"pole_import_cell", "pole_import_row", "pole_import_column", "pole_import"})
        if (auto r = run(d, std::string("DELETE FROM ") + table, {}); !r)
            return r;

    if (auto r = run(d, R"sql(
INSERT INTO pole_import(id, file_name, sheet_name, title, version, imported_utc, header_first_row,
                        header_last_row, first_data_row, from_pole, to_pole)
VALUES(1, ?1, ?2, ?3, ?4, ?5, ?6, ?7, ?8, ?9, ?10)
)sql",
                     {std::string_view(record.fileName), std::string_view(record.sheetName),
                      std::string_view(record.title), std::string_view(record.version),
                      std::string_view(importedUtc), std::int64_t{record.headerFirstRow},
                      std::int64_t{record.headerLastRow}, std::int64_t{record.firstDataRow},
                      std::string_view(record.fromPole), std::string_view(record.toPole)});
        !r)
        return r;

    auto columnStmt = d.prepare("INSERT INTO pole_import_column(col, letter, header, field) VALUES(?1, ?2, ?3, ?4)");
    if (!columnStmt)
        return columnStmt.error();
    for (const poletable::ImportColumn& c : record.columns) {
        db::Statement& s = columnStmt.value();
        s.bind(1, std::int64_t{c.column}).bind(2, c.letter).bind(3, c.header);
        if (c.field >= 0)
            s.bind(4, kImportFieldNames[c.field]);
        else
            s.bindNull(4);
        if (auto r = s.run(); !r)
            return r;
        s.reset();
    }

    auto rowStmt = d.prepare("INSERT INTO pole_import_row(excel_row, position, pole) VALUES(?1, ?2, ?3)");
    if (!rowStmt)
        return rowStmt.error();
    auto cellStmt = d.prepare("INSERT INTO pole_import_cell(excel_row, col, kind, text) VALUES(?1, ?2, ?3, ?4)");
    if (!cellStmt)
        return cellStmt.error();
    std::int64_t position = 0;
    for (const poletable::ImportRow& row : record.rows) {
        db::Statement& rs = rowStmt.value();
        rs.bind(1, std::int64_t{row.excelRow}).bind(2, position).bind(3, row.pole);
        if (auto r = rs.run(); !r)
            return r;
        rs.reset();
        ++position;
        for (const poletable::ImportCell& cell : row.cells) {
            db::Statement& cs = cellStmt.value();
            cs.bind(1, std::int64_t{row.excelRow})
                .bind(2, std::int64_t{cell.column})
                .bind(3, importKindText(cell.kind))
                .bind(4, cell.text);
            if (auto r = cs.run(); !r)
                return r;
            cs.reset();
        }
    }
    return {};
}

} // namespace

// Ersätter Stolptabell före och importens rådata i EN transaktion. Kontrollerna kommer först och
// inget skrivs när de felar. Synkningen av de automatiska beräkningarna körs sist.
Result<void> importPolesBefore(ProjectDb& p, const std::vector<poletable::PoleRow>& rows,
                               const poletable::ImportRecord& record)
{
    return write(p, [&](db::Database& d) -> Result<void> {
        if (auto v = validateImport(rows, record); !v)
            return v;
        if (auto v = validatePoleRows(Stage::Before, rows); !v)
            return v;
        if (auto w = writePoleRows(d, Stage::Before, rows); !w)
            return w;
        if (auto w = writeImportRecord(d, record, platform::nowUtcIso()); !w)
            return w;
        return syncAutoCalcs(d, platform::nowUtcIso());
    });
}

namespace {

// Felet när rådata i projektfilen inte går att tolka.
Error importDamaged()
{
    return makeError(ErrorCode::Invalid, "Importens rådata i projektfilen är skadad.");
}

} // namespace

// Läser den senaste importens rådata: posten, kolumnerna, raderna i platsordning och cellerna i
// kolumnordning. Utan rad i pole_import blir svaret tomt.
Result<std::optional<poletable::ImportRecord>> lastPoleImport(ProjectDb& p)
{
    db::Database& d = p.local();
    poletable::ImportRecord record;
    {
        auto stmt = d.prepare(R"sql(
SELECT file_name, sheet_name, title, version, imported_utc, header_first_row, header_last_row,
       first_data_row, from_pole, to_pole
FROM pole_import WHERE id = 1
)sql");
        if (!stmt)
            return stmt.error();
        auto row = stmt.value().step();
        if (!row)
            return row.error();
        if (!row.value())
            return std::optional<poletable::ImportRecord>{};
        const db::Statement& s = stmt.value();
        record.fileName = s.text(0);
        record.sheetName = s.text(1);
        record.title = s.text(2);
        record.version = s.text(3);
        record.importedUtc = s.text(4);
        record.headerFirstRow = static_cast<int>(s.integer(5));
        record.headerLastRow = static_cast<int>(s.integer(6));
        record.firstDataRow = static_cast<int>(s.integer(7));
        record.fromPole = s.text(8);
        record.toPole = s.text(9);
    }
    {
        auto stmt = d.prepare("SELECT col, letter, header, field FROM pole_import_column ORDER BY col");
        if (!stmt)
            return stmt.error();
        while (true) {
            auto row = stmt.value().step();
            if (!row)
                return row.error();
            if (!row.value())
                break;
            const db::Statement& s = stmt.value();
            poletable::ImportColumn c;
            c.column = static_cast<int>(s.integer(0));
            c.letter = s.text(1);
            c.header = s.text(2);
            if (!s.isNull(3)) {
                const std::string name = s.text(3);
                c.field = -1;
                for (int f = 0; f < poletable::kColumnCount; ++f)
                    if (kImportFieldNames[static_cast<std::size_t>(f)] == name)
                        c.field = f;
                if (c.field < 0)
                    return importDamaged();
            }
            record.columns.push_back(std::move(c));
        }
    }
    std::map<int, std::size_t> rowIndex;
    {
        auto stmt = d.prepare("SELECT excel_row, pole FROM pole_import_row ORDER BY position");
        if (!stmt)
            return stmt.error();
        while (true) {
            auto row = stmt.value().step();
            if (!row)
                return row.error();
            if (!row.value())
                break;
            poletable::ImportRow r;
            r.excelRow = static_cast<int>(stmt.value().integer(0));
            r.pole = stmt.value().text(1);
            rowIndex[r.excelRow] = record.rows.size();
            record.rows.push_back(std::move(r));
        }
    }
    {
        auto stmt = d.prepare("SELECT excel_row, col, kind, text FROM pole_import_cell ORDER BY excel_row, col");
        if (!stmt)
            return stmt.error();
        while (true) {
            auto row = stmt.value().step();
            if (!row)
                return row.error();
            if (!row.value())
                break;
            const db::Statement& s = stmt.value();
            const auto kind = importKindFromText(s.text(2));
            const auto at = rowIndex.find(static_cast<int>(s.integer(0)));
            if (!kind || at == rowIndex.end())
                return importDamaged();
            poletable::ImportCell cell;
            cell.column = static_cast<int>(s.integer(1));
            cell.kind = *kind;
            cell.text = s.text(3);
            record.rows[at->second].cells.push_back(std::move(cell));
        }
    }
    return std::optional<poletable::ImportRecord>{std::move(record)};
}

} // namespace kalkyl::projectdb
