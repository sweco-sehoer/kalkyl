#pragma once
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "anCalc/normal_span.h"
#include "conductor/conductor.h"
#include "core/result.h"
#include "lineCalc/line_calc.h"
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
// höger, före (lila) och efter (rosa) ombyggnad.
enum class Wire { Phase, TopLeft, TopRight };
enum class Stage { Before, After };

// Nyckeln för normalspannsraden, t.ex. "ns_calc_top_left_after".
std::string normalSpanDocKey(Wire wire, Stage stage);

// En normalspannsberäkning: indata och gränser, utan lina. Resultatet räknas med
// ancalc::calculateNormalSpan och lagras aldrig.
struct NormalSpan {
    std::int64_t id = 0;                            // 0 = ny, inte sparad
    std::string note;                              // anteckning
    ancalc::SpanLimits limits;
    std::vector<ancalc::SpanPole> poles;              // i tabellens ordning
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
// Raden docKey visar ingen beräkning längre, varken normalspann eller linberäkning.
// Beräkningen finns kvar. En kopplad linberäkning blir manuell (detach). Fel: AccessDenied, Sql.
core::Result<void> unlinkDocRow(ProjectDb& p, std::string_view docKey);
// Tar bort beräkningen, dess stolpar och alla dess kopplingar. Saknat id är inget fel.
// Fel: AccessDenied, Sql.
core::Result<void> removeNormalSpan(ProjectDb& p, std::int64_t id);

// Nyckeln för linberäkningsraden i Dokumentation, t.ex. "line_calc_top_left_after".
// Raderna finns redan i v0.2.0 med dessa nycklar och manuella värden (planen fas 2, 8.1).
std::string lineCalcDocKey(Wire wire, Stage stage);

// Lina och skede för en linberäkningsnyckel. Tomt svar för en annan nyckel.
std::optional<std::pair<Wire, Stage>> lineCalcRowOf(std::string_view docKey);

// En linberäkning i projektet. input är indata som de lagras. En kopplad beräkning (docSource
// satt) lagrar ingen lina, inget an och inget sigma0: de och min/max är Dokumentationens och
// sätts in av resolveLineInput. Resultatet räknas med calculateResolved och lagras aldrig.
struct LineCalc {
    std::int64_t id = 0;                     // 0 = ny, inte sparad
    std::string note;                        // anteckning
    std::string docSource;                   // linberäkningsradens nyckel; tom = manuell
    linecalc::LineInput input;
    std::vector<std::string> docKeys;        // raderna som visar den; bara vid läsning
    std::string createdUtc;                  // sätts vid sparning
    std::string modifiedUtc;
};

// Det i Dokumentation som en kopplad linberäkning hämtar: uppgifterna, projektets linor och
// aₙ för varje normalspannsrad med en kopplad beräkning. Läses en gång per omräkning.
struct DocLineSources {
    std::vector<DocValue> values;                  // som docValues
    std::vector<conductor::Conductor> conductors;  // som projectConductors
    // aₙ (avrundat till 0,1 m) per normalspannsnyckel med kopplad beräkning; tomt = fel
    std::vector<std::pair<std::string, std::optional<double>>> normalSpans;
};
core::Result<DocLineSources> docLineSources(ProjectDb& p);

// Fältet i LineInput som ett hämtat värde gäller, och nyckeln i Dokumentation det kom ur.
enum class LineSourceField { Conductor, NormalSpan, Tension, MinTemp, MaxTemp };
struct LineSourceRef { LineSourceField field; std::string docKey; };

// Indata efter Dokumentation (planen fas 2, 4.2). Med tom docSource kommer input tillbaka
// orörd. Annars sätts linan (projektets kopia), an, sigma0, min och max, med
// linecalc::applyInputChange för min och max. Det som saknas ger en Missing-upplysning.
struct ResolvedLineInput {
    linecalc::LineInput input;
    std::vector<linecalc::LineIssue> issues;   // Missing, texterna i planen fas 2, 4.4
    std::vector<LineSourceRef> sources;        // de fält som kom ur Dokumentation
};
ResolvedLineInput resolveLineInput(const linecalc::LineInput& input, std::string_view docSource,
                                   const DocLineSources& docs);

// Räknar en upplöst beräkning: linecalc::calculateLine på input, med upplösningens upplysningar
// först. Finns en Missing ur upplösningen blir det inget resultat, också när calculateLine kunde
// räkna, och calculateLine:s Missing för samma fält tas bort.
linecalc::LineCalculation calculateResolved(const ResolvedLineInput& resolved);

// Alla linberäkningar med lastfall och rader, efter id.
core::Result<std::vector<LineCalc>> lineCalcs(ProjectDb& p);
// En linberäkning. Tomt svar om id saknas.
core::Result<std::optional<LineCalc>> lineCalc(ProjectDb& p, std::int64_t id);
// Ny (id 0) eller ersätter beräkningen och dess 15 lastfall, i EN transaktion, och svarar med id.
// linkDocKey kopplar den till raden (som linkLineCalc). En kopplad beräkning kopplas till
// docSource och bort från alla andra rader. Fel: AccessDenied, NotFound (id saknas), Invalid
// (tal inte ändligt, okänd nyckel, docSource och linkDocKey olika), Sql.
core::Result<std::int64_t> saveLineCalc(ProjectDb& p, const LineCalc& lc,
                                        std::string_view linkDocKey = {});
// Raden docKey visar linberäkningen id, och radens manuella värde töms (referensen står kvar).
// En kopplad beräkning som visades på raden blir manuell (detach). Fel: AccessDenied, NotFound,
// Invalid (inte en linberäkningsnyckel, id är kopplad till en annan rad), Sql.
core::Result<void> linkLineCalc(ProjectDb& p, std::int64_t id, std::string_view docKey);
// Gör en kopplad beräkning manuell: värdena ur Dokumentation skrivs in som egna, och kopplingen
// till raden står kvar. En manuell beräkning lämnas orörd. Fel: AccessDenied, NotFound, Sql.
core::Result<void> detachLineCalc(ProjectDb& p, std::int64_t id);
// Tar bort beräkningen, dess lastfall och alla dess kopplingar. Saknat id är inget fel.
// Fel: AccessDenied, Sql.
core::Result<void> removeLineCalc(ProjectDb& p, std::int64_t id);

} // namespace kalkyl::projectdb
