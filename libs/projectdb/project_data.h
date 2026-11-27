#pragma once
#include <array>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "anCalc/normal_span.h"
#include "autoMode/auto_mode.h"
#include "conductor/conductor.h"
#include "core/result.h"
#include "lineCalc/line_calc.h"
#include "lineHeight/line_height.h"
#include "poleTable/import_record.h"
#include "poleTable/pole_table.h"
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

// Var en automatisk beräkning kommer ifrån (planen Automatiskt läge, 4.1). Satt bara på de
// automatiska, som projectdb skapar, håller aktuella och tar bort (syncAutoCalcs).
struct AutoOrigin {
    Stage stage = Stage::Before;
    Wire wire = Wire::Phase;
    automode::PoleRange range;
    int firstRow = -1;      // sträckans första rad i stolptabellen, -1 när sträckan har fel
    int lastRow = -1;       // sträckans sista rad, -1 när sträckan har fel
    std::string issue;      // sträckans fel (automode::findRange), tomt när den gick att läsa
};

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
    std::optional<AutoOrigin> automatic;            // satt för en automatisk beräkning
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

// Räknar normalspannet för en beräkning: sträckans fel för en automatisk beräkning vars sträcka
// inte går att använda, annars ancalc::calculateNormalSpan på stolparna och gränserna.
ancalc::SpanCalculation calculateSpan(const NormalSpan& ns);

// Tabellens namn i texterna: "Stolptabell före" eller "Stolptabell efter".
std::string_view poleTableName(Stage stage);

// Linans ord i automatiska lägets texter: "faslinan", "topplinorna" (same: topplina höger är
// Samma), "topplina vänster" eller "topplina höger".
std::string autoLineWord(Wire wire, bool same);

// Linans rubrik i panelen: "Faslina", "Topplinor" (same), "Topplina vänster" eller "Topplina höger".
std::string autoLineLabel(Wire wire, bool same);

// Alla beräkningar med stolpar och rader. Först de automatiska (före och sedan efter, i varje
// skede Phase, TopLeft och TopRight), sedan de övriga efter id. De övrigas docKeys saknar rader
// som visar en automatisk beräkning.
core::Result<std::vector<NormalSpan>> normalSpans(ProjectDb& p);
// En beräkning, med automatic och docKeys som i normalSpans. Tomt svar om id saknas.
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
    std::optional<AutoOrigin> automatic;     // satt för en automatisk beräkning
};

// Det i Dokumentation som en kopplad linberäkning hämtar: uppgifterna, projektets linor och
// aₙ för varje normalspannsrad med en kopplad eller automatisk beräkning. Läses en gång per omräkning.
struct DocLineSources {
    std::vector<DocValue> values;                  // som docValues
    std::vector<conductor::Conductor> conductors;  // som projectConductors
    // aₙ (avrundat till 0,1 m) per normalspannsnyckel med kopplad eller automatisk beräkning;
    // tomt = fel. En tagen rad (automatisk beräkning) räknas bara med sin automatiska beräkning.
    std::vector<std::pair<std::string, std::optional<double>>> normalSpans;
    // Normalspannsnycklarna vars aₙ kommer ur en automatisk beräkning (för upplysningens text)
    std::vector<std::string> autoSpanKeys;
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

// Alla linberäkningar med lastfall och rader. Först de automatiska (före och sedan efter, i varje
// skede Phase, TopLeft och TopRight, docKeys = den egna raden), sedan de övriga efter id. De
// övrigas docKeys saknar rader som visar en automatisk beräkning.
core::Result<std::vector<LineCalc>> lineCalcs(ProjectDb& p);
// En linberäkning, med automatic och docKeys som i lineCalcs. Tomt svar om id saknas.
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

// Stolptabellen för skedet i tabellens ordning. En tom tabell ger en tom lista. Stolptabell efter
// ger radernas id (PoleRow::id), före ger alltid id 0.
core::Result<std::vector<poletable::PoleRow>> poles(ProjectDb& p, Stage stage);
// Ersätter hela stolptabellen för skedet i EN transaktion med poletable::rowsToSave(rows): tomma
// rader mitt i sparas, tomma rader sist inte. Positionerna blir 0, 1, 2 ... Talen ska vara
// kanonisk decimaltext (poletable::acceptField). I efter får rader utan id ett
// (poletable::assignRowIds), och linhöjderna som inte längre hör till raderna tas bort. De
// automatiska beräkningarna följer med i samma transaktion. Fel: AccessDenied, Invalid ("Stolptabell före har ett ogiltigt tal på rad <n> i
// kolumnen <namn>." respektive "Stolptabell efter …"), Sql.
core::Result<void> setPoles(ProjectDb& p, Stage stage, const std::vector<poletable::PoleRow>& rows);

// Stolptabell före: poles(p, Stage::Before) respektive setPoles(p, Stage::Before, rows).
core::Result<std::vector<poletable::PoleRow>> polesBefore(ProjectDb& p);
core::Result<void> setPolesBefore(ProjectDb& p, const std::vector<poletable::PoleRow>& rows);

// Automatiskt läge för en stolptabell: på eller av, och en sträcka per lina i Wire-ordningen
// (Phase, TopLeft, TopRight). TopLeft är Topplinor när topplina höger är Samma. TopRight används
// bara när topplina höger är en egen lina, men sparas ändå.
struct AutoSettings {
    bool enabled = false;
    std::array<automode::PoleRange, 3> ranges;
    bool operator==(const AutoSettings&) const = default;
};
// Inställningen för skedet. Utan rad i auto_mode: av och inga sträckor.
core::Result<AutoSettings> autoSettings(ProjectDb& p, Stage stage);
// Sparar inställningen (Stolpplats trimmas), och de automatiska beräkningarna följer med i samma
// transaktion. Fel: AccessDenied, Sql.
core::Result<void> setAutoSettings(ProjectDb& p, Stage stage, const AutoSettings& s);
// Stolptabell efter blir en kopia av Stolptabell före: alla rader, sträckorna och läget, i EN
// transaktion med de automatiska beräkningarna. Det som fanns i efter försvinner. Raderna i efter
// får nya id (position + 1), och alla linhöjder tas bort.
// Fel: AccessDenied, Sql.
core::Result<void> copyBeforeToAfter(ProjectDb& p);

// Stolptabell före ur en importerad fil: raderna som setPoles(p, Stage::Before, rows) och importens
// rådata (pole_import, pole_import_column, pole_import_row och pole_import_cell), som ersätter den
// förra importens. Allt sker i EN transaktion, med synkningen av de automatiska beräkningarna sist.
// record.rows har en post per rad i rows med samma Stolpnr, och rows har inga tomma rader.
// importedUtc sätts här. Inget annat ändras: Stolptabell efter, automatiskt läge med sträckorna,
// Dokumentation, linkopiorna och beräkningarna står kvar, utom det synkningen skriver.
// Fel: AccessDenied, Invalid (ogiltigt tal som i setPoles, eller "Importen av stolptabellen är
// ofullständig och sparades inte."), Sql.
core::Result<void> importPolesBefore(ProjectDb& p, const std::vector<poletable::PoleRow>& rows,
                                     const poletable::ImportRecord& record);

// Den senaste importens rådata, eller tomt svar när ingen import har gjorts. Kolumnerna i
// kolumnordning, raderna i platsordning och cellerna i kolumnordning. Fel: Sql.
core::Result<std::optional<poletable::ImportRecord>> lastPoleImport(ProjectDb& p);

// Linhöjderna i fliken Linhöjder: spannen (radernas id) och topplinans Av per rad som användaren
// har skrivit. Allt hör till rader som finns i Stolptabell efter (planen 7.6). Tomt svar när inget
// är sparat.
core::Result<lineheight::StoredHeights> lineHeights(ProjectDb& p);

// Skriver en del av ett spann som kanonisk decimaltext >= 0. Tom text tar bort värdet, och ett
// spann utan värden tas bort. Fel: AccessDenied, Invalid för ogiltig nyckel eller ogiltigt tal,
// och NotFound för ett spann som inte är två grannrader i Stolptabell efter.
core::Result<void> setSpanHeight(ProjectDb& p, const lineheight::SpanKey& span, lineheight::Part part,
                                 const std::string& value);

// Skriver topplinans Av för raden med id row i Stolptabell efter som kanonisk decimaltext. Tom
// text tar bort värdet. Fel: AccessDenied, Invalid för id <= 0 eller ogiltigt tal, och NotFound
// för en rad som inte finns eller är helt tom.
core::Result<void> setTopAv(ProjectDb& p, std::int64_t row, const std::string& value);

} // namespace kalkyl::projectdb
