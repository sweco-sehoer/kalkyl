#include "projectdb/project_data.h"

#include <array>
#include <cmath>
#include <cstddef>
#include <functional>
#include <initializer_list>
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

// Läser det i Dokumentation som en kopplad linberäkning hämtar: uppgifterna, projektets linor
// och aₙ för varje normalspannsrad med en kopplad beräkning (tomt aₙ när beräkningen har fel).
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
    for (const auto& [key, spanId] : links) {
        auto span = readSpan(d, spanId);
        if (!span)
            return span.error();
        std::optional<double> an;
        if (span.value()) {
            const auto calc = ancalc::calculateNormalSpan(span.value()->poles, span.value()->limits);
            if (calc.result)
                an = calc.result->an;
        }
        sources.normalSpans.emplace_back(key, an);
    }
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

} // namespace

// Nyckeln för normalspannsraden: ns_calc_<lina>_<skede>.
std::string normalSpanDocKey(Wire wire, Stage stage)
{
    std::string key = "ns_calc_";
    key += wire == Wire::Phase ? "phase" : wire == Wire::TopLeft ? "top_left" : "top_right";
    key += stage == Stage::Before ? "_before" : "_after";
    return key;
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
        return lineKey ? pruneConductors(d) : Result<void>{};
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
        return pruneConductors(d);
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

// Läser alla beräkningar, efter id.
Result<std::vector<NormalSpan>> normalSpans(ProjectDb& p)
{
    std::vector<std::int64_t> ids;
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
            ids.push_back(stmt.value().integer(0));
        }
    }
    std::vector<NormalSpan> spans;
    for (std::int64_t id : ids) {
        auto span = readSpan(p.local(), id);
        if (!span)
            return span.error();
        if (span.value())
            spans.push_back(std::move(*span.value()));
    }
    return spans;
}

// Läser en beräkning. Tomt svar när id saknas.
Result<std::optional<NormalSpan>> normalSpan(ProjectDb& p, std::int64_t id)
{
    return readSpan(p.local(), id);
}

// Sparar en ny eller befintlig beräkning med alla stolpar i en transaktion. Gränser och tal
// valideras först, och raden linkDocKey (om satt) kopplas.
Result<std::int64_t> saveNormalSpan(ProjectDb& p, const NormalSpan& ns, std::string_view linkDocKey)
{
    std::int64_t savedId = 0;
    auto done = write(p, [&](db::Database& d) -> Result<void> {
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

        std::int64_t position = 0;
        for (const ancalc::SpanPole& pole : ns.poles) {
            if (isBlank(pole.pole) && !pole.lengthM && !pole.heightM)
                continue;
            const Arg length = pole.lengthM ? Arg{*pole.lengthM} : Arg{nullptr};
            const Arg height = pole.heightM ? Arg{*pole.heightM} : Arg{nullptr};
            if (auto r = run(d, R"sql(
INSERT INTO normal_span_pole(span_id, position, pole, length_m, height_m) VALUES(?1, ?2, ?3, ?4, ?5)
)sql",
                             {savedId, position, std::string_view(pole.pole), length, height});
                !r)
                return r;
            ++position;
        }

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
    return write(p, [&](db::Database& d) { return linkInside(d, id, docKey); });
}

// Tar bort kopplingen på raden docKey, för normalspann och linberäkning. Beräkningarna finns
// kvar, och en kopplad linberäkning blir manuell med sina senaste värden som egna.
Result<void> unlinkDocRow(ProjectDb& p, std::string_view docKey)
{
    return write(p, [&](db::Database& d) -> Result<void> {
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
    return write(p, [&](db::Database& d) {
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
        out.issues.push_back(missingIssue(linecalc::LineField::NormalSpan,
                                          spanRow + " har fel och ger inget aₙ. Rätta den i fliken Normalspann."));
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

// Läser alla linberäkningar, efter id.
Result<std::vector<LineCalc>> lineCalcs(ProjectDb& p)
{
    std::vector<std::int64_t> ids;
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
            ids.push_back(stmt.value().integer(0));
        }
    }
    std::vector<LineCalc> calcs;
    for (std::int64_t id : ids) {
        auto calc = readLineCalc(p.local(), id);
        if (!calc)
            return calc.error();
        if (calc.value())
            calcs.push_back(std::move(*calc.value()));
    }
    return calcs;
}

// Läser en linberäkning. Tomt svar när id saknas.
Result<std::optional<LineCalc>> lineCalc(ProjectDb& p, std::int64_t id)
{
    return readLineCalc(p.local(), id);
}

// Sparar en ny eller befintlig linberäkning med de 15 lastfallen i en transaktion och kopplar
// den till sin rad. En kopplad beräkning kopplas till docSource och bort från alla andra rader.
Result<std::int64_t> saveLineCalc(ProjectDb& p, const LineCalc& lc, std::string_view linkDocKey)
{
    std::int64_t savedId = 0;
    auto done = write(p, [&](db::Database& d) -> Result<void> {
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
    return write(p, [&](db::Database& d) { return linkLineInside(d, id, docKey); });
}

// Gör en kopplad linberäkning manuell. Kopplingen till raden står kvar.
Result<void> detachLineCalc(ProjectDb& p, std::int64_t id)
{
    return write(p, [&](db::Database& d) { return detachInside(d, id); });
}

// Tar bort linberäkningen. Lastfall och kopplingar följer med (ON DELETE CASCADE).
Result<void> removeLineCalc(ProjectDb& p, std::int64_t id)
{
    return write(p, [&](db::Database& d) {
        return run(d, "DELETE FROM line_calc WHERE id = ?1", {id});
    });
}

} // namespace kalkyl::projectdb
