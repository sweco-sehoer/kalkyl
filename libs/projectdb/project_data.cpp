#include "projectdb/project_data.h"

#include <cmath>
#include <cstddef>
#include <functional>
#include <initializer_list>
#include <type_traits>
#include <utility>
#include <variant>

#include "db/database.h"
#include "platform/system.h"

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
            calc::SpanPole pole;
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
    auto stmt = prepared(p.local(), "SELECT key, value, reference FROM doc_value ORDER BY key", {});
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
    const std::string sql =
        "SELECT " + std::string(kConductorColumns) + " FROM project_conductor ORDER BY name";
    auto stmt = prepared(p.local(), sql, {});
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
        for (const calc::SpanPole& pole : ns.poles) {
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
        for (const calc::SpanPole& pole : ns.poles) {
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

// Tar bort kopplingen på raden docKey. Beräkningen finns kvar.
Result<void> unlinkDocRow(ProjectDb& p, std::string_view docKey)
{
    return write(p, [&](db::Database& d) {
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

} // namespace kalkyl::projectdb
