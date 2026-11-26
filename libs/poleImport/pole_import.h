#pragma once
#include <array>
#include <string>
#include <string_view>
#include <vector>

#include "autoMode/auto_mode.h"
#include "poleTable/import_record.h"
#include "poleTable/pole_table.h"
#include "xlsx/xlsx.h"

namespace kalkyl::poleimport {

// Gränserna för valet (planen 5.6): lägsta poäng för en kandidat, säker och trolig.
inline constexpr int kMinScore = 40;
inline constexpr int kSureScore = 70;
inline constexpr int kSureMargin = 20;
inline constexpr int kLikelyMargin = 10;
// Poängen för en rubrik som användaren har valt förut.
inline constexpr int kLearnedScore = 100;
// Versionerna av "Stolptabell i Banken" som är provade.
inline constexpr std::string_view kKnownVersions[] = {"6"};

// Excelkolumn per fält i Stolptabell före (poletable::kPole … kLkl): kolumnens nummer (1 = A),
// 0 = hämtas inte, -1 = inte valt än (ett tvetydigt fält, eller Stolpnr som saknas).
using Mapping = std::array<int, poletable::kColumnCount>;
// Inlärda rubriker per fält: normaliserade rubriker (SourceColumn::key) som användaren valt förut.
using Learned = std::array<std::vector<std::string>, poletable::kColumnCount>;

// Rubrikområdet, 1-baserade rader. firstRow 0 = inget hittades. numberRow 0 = ingen kolumnnummerrad.
struct HeaderArea {
    int firstRow = 0;
    int lastRow = 0;
    int numberRow = 0;
    bool operator==(const HeaderArea&) const = default;
};

// En kolumn i bladet med sin sammansatta rubrik: rubrikraderna uppifrån, sammanslagna celler
// ifyllda, varje del trimmad med radbrytningar och blanktecken ihop till ett blanksteg, och tomma
// delar och upprepningar borttagna. label är delarna med " › " emellan ("" utan rubrik), key är
// normalizeHeader av varje del med " | " emellan.
struct SourceColumn {
    int column = 0;
    std::string letter;
    std::vector<std::string> parts;
    std::string label;
    std::string key;
};

// Hur säkert ett fält har sin kolumn: Säker, Trolig, Välj (tvetydig), Saknas och Ditt val.
enum class Confidence { Sure, Likely, Ambiguous, Missing, Chosen };

// En kolumn som passar ett fält: poängen och skälet ("avstånd föregående 60, stolpe +10, profil +10").
struct Candidate {
    int column = 0;
    int score = 0;
    std::string reason;
};

// Valet för ett fält: kolumnen (som i Mapping), säkerheten och de bästa kandidaterna (högst fem med
// poäng över 0, bäst först, också under kMinScore, så att tooltipen kan visa näst bäst).
struct FieldMatch {
    int column = 0;
    Confidence confidence = Confidence::Missing;
    std::vector<Candidate> candidates;
};

// Igenkänningen av ett blad.
struct Recognition {
    HeaderArea header;
    std::string title;                      // "Stolptabell i Banken Ver. 6 GRUNDLÄGGNING & JORDNING", "" utan
    std::string version;                    // "Ver. 6", "" utan
    bool knownVersion = false;              // versionen finns i kKnownVersions
    std::vector<SourceColumn> columns;      // kolumnerna med rubrik eller data, i kolumnordning
    std::array<FieldMatch, poletable::kColumnCount> fields;
    bool looksLikePoleTable = false;        // Stolpnr är Säker, Trolig eller Ditt val
};

// Rubriktext i normaliserad form (planen 5.4): gemener, å/ä/ö vikta, skiljetecken bort och
// förkortningar utskrivna. "Avst. föreg." ger "avstand foregaende".
std::string normalizeHeader(std::string_view text);

// Rubrikområdet (planen 5.2): kolumnnummerraden, annars raden med flest träffar på fältens ord.
HeaderArea findHeaderArea(const xlsx::Sheet& sheet);

// Kolumnerna med sammansatta rubriker ur rubrikområdet och sammanslagningarna (planen 5.3).
std::vector<SourceColumn> sourceColumns(const xlsx::Sheet& sheet, const HeaderArea& header);

// Regelpoängen 0-100 för ett fält och en kolumn, utan dataprov och inlärning (planen 5.5).
Candidate scoreField(int field, const SourceColumn& column);

// Hela igenkänningen: rubrikområdet, titeln, kolumnerna, poängen med dataprov och inlärning, och valet.
Recognition recognize(const xlsx::Sheet& sheet, const Learned& learned = {});

// Bladet som ser mest ut som en stolptabell: ett synligt blad som heter "Stolptabell" (normaliserat),
// annars det med flest fält som är säkra eller troliga. 0 för en bok utan blad.
int bestSheet(const xlsx::Workbook& book, const Learned& learned = {});

// Kolumnerna ur valet: fields[f].column per fält.
Mapping mappingOf(const Recognition& rec);

// "Säker", "Trolig", "Välj", "Saknas" och "Ditt val".
std::string confidenceText(Confidence confidence);

// En stolpe ur filen: Excels radnummer, raden som acceptField gav (ett ogiltigt tal är tomt) och
// den ogiltiga texten per fält ("" = giltig eller tom).
struct SourceRow {
    int excelRow = 0;
    poletable::PoleRow pole;
    std::array<std::string, poletable::kColumnCount> invalid;
};

// Stolparna i bladet med de valda kolumnerna (planen 5.7).
struct Extraction {
    int firstDataRow = 0;                // första raden med Stolpnr, 0 = inga stolpar
    std::vector<SourceRow> rows;         // raderna med Stolpnr, i filens ordning
    std::vector<int> skippedRows;        // rader utan Stolpnr men med andra värden
};
Extraction extract(const xlsx::Sheet& sheet, const HeaderArea& header, const Mapping& mapping);

// Raderna first..last (index i ex.rows, båda med) som rader i Stolptabell före.
std::vector<poletable::PoleRow> poleRows(const Extraction& ex, int first, int last);

// Importens rådata för raderna first..last: alla celler med värde på raderna, kolumnerna med
// rubrik eller data på raderna, fälten ur mapping (bara kolumner > 0), titel, version,
// rubrikområdet, bladets namn och fileName (bara namnet). importedUtc lämnas tom.
poletable::ImportRecord importRecord(const xlsx::Sheet& sheet, const Recognition& rec, const Mapping& mapping,
                                     const Extraction& ex, int first, int last, std::string fileName);

// Ett problem med importen. blocking = Importera går inte att trycka.
struct Issue {
    bool blocking = false;
    std::string text;
};
// Problemen i den ordning dialogen visar dem (planen 9.3): först de som stoppar, sedan de övriga.
std::vector<Issue> importIssues(const Recognition& rec, const Mapping& mapping, const Extraction& ex,
                                int first, int last);

// Sträckor som har en giltig start och ett giltigt slut i current men inte i imported, en rad per
// sträcka: "{Word}: " och felet ur automode::findRange med tabellen "filen", till exempel
// "Faslinan: Stolpnr ST0107 finns inte i filen.". Sträckor utan start eller slut räknas inte.
std::vector<std::string> lostRanges(const std::vector<automode::NamedRange>& ranges,
                                    const std::vector<poletable::PoleRow>& current,
                                    const std::vector<poletable::PoleRow>& imported);

} // namespace kalkyl::poleimport
