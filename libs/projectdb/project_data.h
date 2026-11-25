#pragma once
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "calc/normal_span.h"
#include "conductor/conductor.h"
#include "core/result.h"
#include "projectdb/project_db.h"

namespace kalkyl::projectdb {

// Dokumentationens nycklar för linraderna (planen 3.5). top_conductor_right kan vara "Samma".
inline constexpr std::string_view kPhaseConductorKey    = "phase_conductor";
inline constexpr std::string_view kTopLeftConductorKey  = "top_conductor_left";
inline constexpr std::string_view kTopRightConductorKey = "top_conductor_right";
inline constexpr std::string_view kSameConductor        = "Samma";

// En uppgift i fliken Dokumentation (planen 3.5). value är kanonisk text: tal med punkt,
// val som text, linans namn för linraderna. key är radens stabila nyckel.
struct DocValue { std::string key; std::string value; std::string reference; };

// Lina och skede för normalspannsraderna i Dokumentation: faslina, topplina vänster och
// höger, före (lila) och efter (rosa) ombyggnation.
enum class Wire { Phase, TopLeft, TopRight };
enum class Stage { Before, After };

// Nyckeln för normalspannsraden, t.ex. "ns_calc_top_left_after".
std::string normalSpanDocKey(Wire wire, Stage stage);

// En normalspannsberäkning: indata och gränser, utan lina. Resultatet räknas med
// calc::calculateNormalSpan och lagras aldrig.
struct NormalSpan {
    std::int64_t id = 0;                            // 0 = ny, inte sparad
    std::string note;                              // anteckning
    calc::SpanLimits limits;
    std::vector<calc::SpanPole> poles;              // i tabellens ordning
    std::vector<std::string> docKeys;               // raderna som visar den; bara vid läsning
    std::string createdUtc;                         // sätts vid sparning
    std::string modifiedUtc;
};

// Alla uppgifter i Dokumentation, i nyckelordning.
core::Result<std::vector<DocValue>> docValues(ProjectDb& p);
// Lägger in eller ersätter. Tomt värde och tom referens tar bort raden. På en linrad får
// värdet bara vara tomt, "Samma" (höger) eller namnet på en lina som finns i projektet.
// Fel: AccessDenied (skrivskyddat), Invalid (tom nyckel, okänt linnamn), Sql.
core::Result<void> setDocValue(ProjectDb& p, const DocValue& v);

// Projektets linor, efter namn.
core::Result<std::vector<conductor::Conductor>> projectConductors(ProjectDb& p);
// Sätter linan på en linrad. Finns en lina med samma namn (NOCASE) i projektet återanvänds
// den oförändrad; annars kopieras chosen in. Radens referens står kvar. Linor som inget
// använder tas bort. Fel: AccessDenied, Invalid (inte en linnyckel, ogiltig lina), Sql.
core::Result<void> setDocConductor(ProjectDb& p, std::string_view key,
                                   const conductor::Conductor& chosen);
// Uppdatera från linregistret: kopian med samma namn (NOCASE) får fresh:s typ och värden.
// Namn och id behålls. Fel: AccessDenied, NotFound (ingen sådan lina), Invalid, Sql.
core::Result<void> refreshProjectConductor(ProjectDb& p, const conductor::Conductor& fresh);

// Alla beräkningar med stolpar och rader, efter id.
core::Result<std::vector<NormalSpan>> normalSpans(ProjectDb& p);
// En beräkning. Tomt svar om id saknas.
core::Result<std::optional<NormalSpan>> normalSpan(ProjectDb& p, std::int64_t id);
// Ny (id 0) eller ersätter beräkningen och alla dess stolpar, i EN transaktion, och svarar
// med id. Helt tomma stolpar sparas inte; positionerna blir 0, 1, 2 ... Är linkDocKey satt
// kopplas beräkningen till den raden (som linkNormalSpan) i samma transaktion.
// Fel: AccessDenied, NotFound (id saknas), Invalid (tal inte ändligt, gräns <= 0, okänd
// radnyckel), Sql.
core::Result<std::int64_t> saveNormalSpan(ProjectDb& p, const NormalSpan& ns,
                                          std::string_view linkDocKey = {});
// Raden docKey visar beräkningen id. En annan beräkning på raden kopplas bort, och radens
// manuella värde i doc_value töms (referensen står kvar). Fel: AccessDenied, NotFound,
// Invalid (inte en normalspannsnyckel), Sql.
core::Result<void> linkNormalSpan(ProjectDb& p, std::int64_t id, std::string_view docKey);
// Raden docKey visar ingen beräkning längre. Beräkningen finns kvar. Fel: AccessDenied, Sql.
core::Result<void> unlinkDocRow(ProjectDb& p, std::string_view docKey);
// Tar bort beräkningen, dess stolpar och alla dess kopplingar. Saknat id är inget fel.
// Fel: AccessDenied, Sql.
core::Result<void> removeNormalSpan(ProjectDb& p, std::int64_t id);

} // namespace kalkyl::projectdb
