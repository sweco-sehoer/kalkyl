#include <algorithm>
#include <cstdlib>
#include <initializer_list>
#include <map>
#include <set>
#include <unordered_map>

#include "poleImport/internal.h"
#include "poleImport/pole_import.h"

namespace kalkyl::poleimport {
namespace {

using WordSet = std::set<std::string>;

// Skiljetecknet mellan rubrikens delar i label (U+203A) och minustecknet (U+2212) i skälen.
const std::string kPartSeparator = " \xE2\x80\xBA ";
const std::string kMinus = "\xE2\x88\x92";

// Förkortningarna i planen 5.4, ord för ord efter normaliseringen. Ett ord kan ge flera ord.
struct Abbreviation {
    std::string_view word;
    std::string_view full;
};
constexpr Abbreviation kAbbreviations[] = {
    {"avst", "avstand"},
    {"avstand", "avstand"},
    {"foreg", "foregaende"},
    {"forega", "foregaende"},
    {"foregaende", "foregaende"},
    {"stpl", "stolpe"},
    {"stolp", "stolpe"},
    {"stolpe", "stolpe"},
    {"stolpar", "stolpe"},
    {"stolpens", "stolpe"},
    {"nr", "nummer"},
    {"no", "nummer"},
    {"num", "nummer"},
    {"numr", "nummer"},
    {"nummer", "nummer"},
    {"stolpnr", "stolpe nummer"},
    {"stplnr", "stolpe nummer"},
    {"stpnr", "stolpe nummer"},
    {"stolpnummer", "stolpe nummer"},
    {"stolpplats", "stolpe plats"},
    {"ordningsnr", "ordning nummer"},
    {"temp", "temperatur"},
    {"temperatur", "temperatur"},
    {"koord", "koordinat"},
    {"koordinat", "koordinat"},
    {"koordinater", "koordinat"},
    {"berakn", "berakna"},
    {"beraknat", "berakna"},
    {"beraknad", "berakna"},
    {"beraknade", "berakna"},
    {"spannlangd", "spann langd"},
    {"spannl", "spann langd"},
    {"lkl", "langdklass"},
    {"langdklass", "langdklass"},
    {"slutgiltig", "slutgiltig"},
    {"slutgiltigt", "slutgiltig"},
    {"slutlig", "slutgiltig"},
    {"slutligt", "slutgiltig"},
    {"byggnad", "byggnad"},
    {"byggnads", "byggnad"},
    {"norr", "n"},
    {"nord", "n"},
    {"northing", "n"},
    {"ost", "e"},
    {"east", "e"},
    {"easting", "e"},
};

// Ordet som förkortningen står för, eller tom text när ordet inte är en förkortning.
std::string_view expandWord(std::string_view word)
{
    static const std::unordered_map<std::string_view, std::string_view> map = [] {
        std::unordered_map<std::string_view, std::string_view> m;
        for (const Abbreviation& a : kAbbreviations)
            m.emplace(a.word, a.full);
        return m;
    }();
    const auto it = map.find(word);
    return it == map.end() ? std::string_view{} : it->second;
}

// Nästa Unicode-tecken i UTF-8-texten. Ogiltiga byte ger U+FFFD.
char32_t nextCodepoint(std::string_view s, std::size_t& i)
{
    const unsigned char c = static_cast<unsigned char>(s[i]);
    if (c < 0x80) {
        ++i;
        return c;
    }
    const std::size_t n = c >= 0xF0 ? 4 : c >= 0xE0 ? 3 : c >= 0xC0 ? 2 : 1;
    if (n == 1 || i + n > s.size()) {
        ++i;
        return 0xFFFD;
    }
    char32_t cp = c & (0xFFu >> (n + 1));
    for (std::size_t k = 1; k < n; ++k)
        cp = (cp << 6) | (static_cast<unsigned char>(s[i + k]) & 0x3Fu);
    i += n;
    return cp;
}

// Tecknet i normaliserad form: gemen ASCII-bokstav eller siffra, eller 0 för ett skiljetecken.
// å, ä, ö och några vanliga accenttecken viks till a, o, e och u.
char foldCodepoint(char32_t cp)
{
    if (cp >= 'A' && cp <= 'Z')
        return static_cast<char>(cp - 'A' + 'a');
    if ((cp >= 'a' && cp <= 'z') || (cp >= '0' && cp <= '9'))
        return static_cast<char>(cp);
    switch (cp) {
    case 0xC5: case 0xE5: case 0xC4: case 0xE4: case 0xC0: case 0xE0: case 0xC1: case 0xE1:
        return 'a';
    case 0xD6: case 0xF6: case 0xD8: case 0xF8: case 0xD3: case 0xF3:
        return 'o';
    case 0xC9: case 0xE9: case 0xC8: case 0xE8: case 0xCB: case 0xEB:
        return 'e';
    case 0xDC: case 0xFC:
        return 'u';
    default:
        return 0;
    }
}

// Orden i en rubrikdel efter normaliseringen: små bokstäver, bokstäver och siffror skilda åt och
// förkortningarna utskrivna.
std::vector<std::string> normalizedWords(std::string_view text)
{
    std::vector<std::string> raw;
    std::string cur;
    int cls = 0; // 0 skiljetecken, 1 bokstav, 2 siffra
    for (std::size_t i = 0; i < text.size();) {
        const char ch = foldCodepoint(nextCodepoint(text, i));
        const int c = ch == 0 ? 0 : (ch >= '0' && ch <= '9' ? 2 : 1);
        if (c != cls && !cur.empty()) {
            raw.push_back(cur);
            cur.clear();
        }
        if (c != 0)
            cur += ch;
        cls = c;
    }
    if (!cur.empty())
        raw.push_back(cur);

    std::vector<std::string> out;
    for (const std::string& w : raw) {
        const std::string_view full = expandWord(w);
        if (full.empty()) {
            out.push_back(w);
            continue;
        }
        std::size_t start = 0;
        while (start <= full.size()) {
            const std::size_t space = full.find(' ', start);
            out.emplace_back(full.substr(start, space == std::string_view::npos ? space : space - start));
            if (space == std::string_view::npos)
                break;
            start = space + 1;
        }
    }
    return out;
}

// Orden i varje del av en kolumns rubrik och alla delars ord tillsammans.
struct PartWords {
    std::vector<WordSet> parts;
    WordSet all;
};

PartWords wordsOf(const std::vector<std::string>& parts)
{
    PartWords pw;
    for (const std::string& part : parts) {
        const std::vector<std::string> words = normalizedWords(part);
        WordSet set(words.begin(), words.end());
        pw.all.insert(set.begin(), set.end());
        pw.parts.push_back(std::move(set));
    }
    return pw;
}

// En grundregel: "en del = {…}" (PartIs, någon dels ordmängd är exakt den) eller "alla {…}"
// (HasAll, kolumnens ord innehåller alla), med poängen och texten i skälet.
enum class Match { PartIs, HasAll };
struct BaseRule {
    Match match;
    WordSet words;
    int score;
    std::string text;
};

// Ett plus eller minus: gäller när något av alternativen (alla orden i det) finns bland kolumnens ord.
struct AdjustRule {
    std::vector<WordSet> anyOf;
    int delta;
    std::string text;
};

// Reglerna för ett fält: grundreglerna (den bästa som stämmer ger grundpoängen) och plus och
// minus, som läggs till för varje alternativ som finns bland kolumnens ord.
struct FieldRules {
    std::vector<BaseRule> bases;
    std::vector<AdjustRule> adjusts;
};

// En ordmängd ur en lista med ord, kort så att poängtabellen nedan går att läsa:
// W({"avstand", "foregaende"}).
WordSet W(std::initializer_list<const char*> words)
{
    return WordSet(words.begin(), words.end());
}

// Poängtabellen i planen 5.5, ett FieldRules per fält i poletable-ordning. Texterna i skälen skrivs
// med å, ä och ö.
const std::array<FieldRules, poletable::kColumnCount>& fieldRules()
{
    static const std::array<FieldRules, poletable::kColumnCount> rules = [] {
        using namespace poletable;
        std::array<FieldRules, kColumnCount> r;
        auto base = [&r](int f, Match m, WordSet w, int score, const char* text) {
            r[static_cast<std::size_t>(f)].bases.push_back({m, std::move(w), score, text});
        };
        auto adj = [&r](int f, std::vector<WordSet> any, int delta, const char* text) {
            r[static_cast<std::size_t>(f)].adjusts.push_back({std::move(any), delta, text});
        };

        base(kPole, Match::HasAll, W({"stolpe", "nummer"}), 60, "stolpnummer");
        base(kPole, Match::PartIs, W({"stolpe", "plats"}), 50, "stolpplats");
        adj(kPole, {W({"slutgiltig"})}, 30, "slutgiltigt");
        adj(kPole, {W({"profil"})}, 5, "profil");
        adj(kPole, {W({"byggnad"})}, -40, "byggnads");
        adj(kPole, {W({"ordning"})}, -40, "ordning");
        adj(kPole, {W({"omvand"})}, -40, "omvänd");

        for (const int f : {kNorth, kEast}) {
            const char* letter = f == kNorth ? "n" : "e";
            const char* other = f == kNorth ? "x" : "y";
            base(f, Match::PartIs, W({"koordinat", letter}), 60, f == kNorth ? "koordinat N" : "koordinat E");
            base(f, Match::PartIs, W({letter}), 50, f == kNorth ? "N" : "E");
            base(f, Match::PartIs, W({other}), 30, f == kNorth ? "X" : "Y");
            adj(f, {W({"koordinat"})}, 30, "koordinat");
            adj(f, {W({"sweref"}), W({"rt", "90"})}, 15, "SWEREF");
            adj(f, {W({"berakna"})}, -40, "beräknad");
        }

        base(kHeight, Match::PartIs, W({"koordinat", "h"}), 60, "koordinat H");
        base(kHeight, Match::PartIs, W({"h"}), 50, "H");
        base(kHeight, Match::PartIs, W({"hojd"}), 40, "höjd");
        base(kHeight, Match::PartIs, W({"z"}), 30, "Z");
        adj(kHeight, {W({"koordinat"})}, 30, "koordinat");
        adj(kHeight, {W({"rh"})}, 15, "RH");
        adj(kHeight, {W({"berakna"})}, -40, "beräknad");
        adj(kHeight, {W({"till"})}, -30, "till");

        base(kSpan, Match::HasAll, W({"avstand", "foregaende"}), 60, "avstånd föregående");
        base(kSpan, Match::HasAll, W({"spann", "langd"}), 50, "spannlängd");
        base(kSpan, Match::PartIs, W({"spann"}), 35, "spann");
        adj(kSpan, {W({"stolpe"})}, 10, "stolpe");
        adj(kSpan, {W({"profil"})}, 10, "profil");
        adj(kSpan, {W({"berakna"})}, -40, "beräknad");
        adj(kSpan, {W({"koordinat"})}, -20, "koordinat");
        adj(kSpan, {W({"total"})}, -40, "total");
        adj(kSpan, {W({"station"})}, -30, "station");

        base(kAh, Match::PartIs, W({"ah"}), 60, "Ah");
        adj(kAh, {W({"profil"})}, 10, "profil");
        adj(kAh, {W({"m"})}, 5, "m");
        adj(kAh, {W({"berakna"})}, -40, "beräknad");
        adj(kAh, {W({"koordinat"})}, -20, "koordinat");

        base(kAv, Match::PartIs, W({"av"}), 60, "Av");
        adj(kAv, {W({"max"})}, 20, "max");
        adj(kAv, {W({"profil"})}, 10, "profil");
        adj(kAv, {W({"m"})}, 5, "m");
        adj(kAv, {W({"min"})}, -50, "min");
        adj(kAv, {W({"berakna"})}, -40, "beräknad");

        base(kLkl, Match::HasAll, W({"langdklass"}), 70, "längdklass");
        base(kLkl, Match::PartIs, W({"langd", "klass"}), 70, "längd klass");
        adj(kLkl, {W({"stolpe"})}, 10, "stolpe");
        return r;
    }();
    return rules;
}

// Poäng med tecken för skälen: "+10" eller "−40" med riktigt minustecken.
std::string signedPoints(int delta)
{
    return (delta < 0 ? kMinus : std::string("+")) + std::to_string(delta < 0 ? -delta : delta);
}

// Regelpoängen för ett fält och en kolumn som redan är uppdelad i ord (planen 5.5): den högsta
// grundregeln som passar, plus och minus för orden som finns, summan inom 0-100 och skälet.
Candidate scoreWords(int field, int column, const PartWords& pw)
{
    Candidate cand;
    cand.column = column;
    if (field < 0 || field >= poletable::kColumnCount)
        return cand;
    const FieldRules& fr = fieldRules()[static_cast<std::size_t>(field)];
    const BaseRule* best = nullptr;
    for (const BaseRule& b : fr.bases) {
        bool matches = false;
        if (b.match == Match::PartIs) {
            matches = std::any_of(pw.parts.begin(), pw.parts.end(), [&](const WordSet& p) { return p == b.words; });
        } else {
            matches = std::includes(pw.all.begin(), pw.all.end(), b.words.begin(), b.words.end());
        }
        if (matches && (!best || b.score > best->score))
            best = &b;
    }
    if (!best)
        return cand;
    int score = best->score;
    std::string reason = best->text + " " + std::to_string(best->score);
    for (const AdjustRule& a : fr.adjusts) {
        const bool hit = std::any_of(a.anyOf.begin(), a.anyOf.end(), [&](const WordSet& alt) {
            return std::includes(pw.all.begin(), pw.all.end(), alt.begin(), alt.end());
        });
        if (!hit)
            continue;
        score += a.delta;
        reason += ", " + a.text + " " + signedPoints(a.delta);
    }
    cand.score = std::clamp(score, 0, 100);
    cand.reason = reason;
    return cand;
}

// Kolumnens värden på dataraderna: hur många som har ett värde och hur många av dem som är tal.
struct ColumnStats {
    int values = 0;
    int numbers = 0;
};

// Dataprovet: statistiken per kolumn för raderna från firstRow till bladets slut. Indexet är kolumnnumret.
std::vector<ColumnStats> columnStats(const xlsx::Sheet& sheet, int firstRow)
{
    std::vector<ColumnStats> stats(static_cast<std::size_t>(sheet.lastColumn) + 2);
    auto it = std::lower_bound(sheet.cells.begin(), sheet.cells.end(), firstRow,
                               [](const xlsx::Cell& c, int r) { return c.row < r; });
    for (; it != sheet.cells.end(); ++it) {
        const detail::ValueKind kind = detail::valueKind(*it);
        if (kind == detail::ValueKind::Empty || it->column < 0 || it->column >= static_cast<int>(stats.size()))
            continue;
        ColumnStats& s = stats[static_cast<std::size_t>(it->column)];
        ++s.values;
        if (kind == detail::ValueKind::Number)
            ++s.numbers;
    }
    return stats;
}

// Ett heltal ur en cell: ett tal utan decimaler eller text med bara siffror. Tomt svar för annat.
bool cellInteger(const xlsx::Cell& cell, long& value)
{
    if (cell.kind == xlsx::CellKind::Number) {
        char* end = nullptr;
        const double d = std::strtod(cell.text.c_str(), &end);
        if (end == cell.text.c_str() || *end != 0 || !(d >= 0 && d <= 1000000) ||
            d != static_cast<double>(static_cast<long>(d)))
            return false;
        value = static_cast<long>(d);
        return true;
    }
    if (cell.kind == xlsx::CellKind::Text) {
        const std::string t = tabletext::trimmed(cell.text);
        if (t.empty() || t.size() > 6 || !std::all_of(t.begin(), t.end(), [](char c) { return c >= '0' && c <= '9'; }))
            return false;
        value = std::atol(t.c_str());
        return true;
    }
    return false;
}

// Sant när raden har minst en cell.
bool rowHasCells(const xlsx::Sheet& sheet, int row)
{
    return !detail::rowCells(sheet, row).empty();
}

// Sant när raden har minst en textcell med innehåll.
bool rowHasText(const xlsx::Sheet& sheet, int row)
{
    for (const xlsx::Cell& c : detail::rowCells(sheet, row))
        if (c.kind == xlsx::CellKind::Text && !tabletext::trimmed(c.text).empty())
            return true;
    return false;
}

// Sant när raden ser ut som data: minst 3 talceller, eller minst hälften av cellerna är tal.
bool rowLooksLikeData(const xlsx::Sheet& sheet, int row)
{
    int numbers = 0;
    int total = 0;
    for (const xlsx::Cell& c : detail::rowCells(sheet, row)) {
        ++total;
        if (detail::valueKind(c) == detail::ValueKind::Number)
            ++numbers;
    }
    return total > 0 && (numbers >= 3 || numbers * 2 >= total);
}

// Kolumnnummerraden (planen 5.2 punkt 1): första raden bland 1-100 där minst 8 celler har ett heltal
// som är kolumnens nummer plus samma k, och de är minst 80 % av radens talceller. 0 när ingen finns.
int findNumberRow(const xlsx::Sheet& sheet)
{
    const int limit = std::min(sheet.lastRow, 100);
    for (int row = 1; row <= limit; ++row) {
        std::map<long, int> counts;
        int numeric = 0;
        for (const xlsx::Cell& c : detail::rowCells(sheet, row)) {
            long v = 0;
            if (!cellInteger(c, v))
                continue;
            ++numeric;
            ++counts[v - c.column];
        }
        int best = 0;
        for (const auto& kv : counts)
            best = std::max(best, kv.second);
        if (best >= 8 && best * 5 >= numeric * 4)
            return row;
    }
    return 0;
}

// Rubrikområdet över kolumnnummerraden: raden ovanför (förbi högst två tomma rader) och uppåt över
// rader med text, högst 8 rader. Tomt område när inga rubrikrader finns ovanför.
HeaderArea areaAboveNumberRow(const xlsx::Sheet& sheet, int numberRow)
{
    int last = numberRow - 1;
    int skipped = 0;
    while (last >= 1 && !rowHasCells(sheet, last) && skipped < 2) {
        --last;
        ++skipped;
    }
    if (last < 1 || !rowHasCells(sheet, last))
        return {};
    int first = last;
    while (first > 1 && last - (first - 1) + 1 <= 8 && rowHasText(sheet, first - 1))
        --first;
    return {first, last, numberRow};
}

// Rubrikområdet utan kolumnnummerrad (planen 5.2 punkt 3 och 4): ankarraden med flest celler som ensamma
// ger minst 50 poäng, och området runt den. Utan ankare den första raden som inte är tom.
HeaderArea areaFromAnchor(const xlsx::Sheet& sheet)
{
    int anchor = 0;
    int anchorHits = 0;
    const int limit = std::min(sheet.lastRow, 100);
    for (int row = 1; row <= limit; ++row) {
        int hits = 0;
        for (const xlsx::Cell& c : detail::rowCells(sheet, row)) {
            if (c.kind != xlsx::CellKind::Text)
                continue;
            const std::string text = detail::collapseSpaces(c.text);
            if (text.empty())
                continue;
            const PartWords pw = wordsOf({text});
            for (int f = 0; f < poletable::kColumnCount; ++f) {
                if (scoreWords(f, c.column, pw).score >= 50) {
                    ++hits;
                    break;
                }
            }
        }
        if (hits > anchorHits) {
            anchor = row;
            anchorHits = hits;
        }
    }
    if (anchorHits >= 2) {
        int first = anchor;
        while (first > 1 && anchor - (first - 1) < 3 && rowHasCells(sheet, first - 1))
            --first;
        int last = anchor;
        while (last < sheet.lastRow && last - anchor < 3 && rowHasCells(sheet, last + 1) &&
               !rowLooksLikeData(sheet, last + 1))
            ++last;
        return {first, last, 0};
    }
    for (int row = 1; row <= sheet.lastRow; ++row)
        if (rowHasCells(sheet, row))
            return {row, row, 0};
    return {};
}

// Titeln och versionen ur cellerna ovanför rubrikområdet (planen 5.6 punkt 5): första texten med
// "stolptabell" och "ver" följt av ett tal.
void findTitle(const xlsx::Sheet& sheet, const HeaderArea& header, Recognition& rec)
{
    if (header.firstRow <= 1)
        return;
    for (const xlsx::Cell& c : sheet.cells) {
        if (c.row >= header.firstRow)
            break;
        if (c.kind != xlsx::CellKind::Text)
            continue;
        const std::vector<std::string> words = normalizedWords(c.text);
        if (std::find(words.begin(), words.end(), "stolptabell") == words.end())
            continue;
        for (std::size_t i = 0; i + 1 < words.size(); ++i) {
            const std::string& next = words[i + 1];
            if (words[i] != "ver" || next.empty() || !std::all_of(next.begin(), next.end(), [](char ch) { return ch >= '0' && ch <= '9'; }))
                continue;
            rec.title = detail::collapseSpaces(c.text);
            rec.version = "Ver. " + next;
            rec.knownVersion = std::any_of(std::begin(kKnownVersions), std::end(kKnownVersions),
                                           [&](std::string_view v) { return v == next; });
            return;
        }
    }
}

// Ett par (fält, kolumn) som kan väljas, och om poängen kommer från inlärningen.
struct Scored {
    Candidate cand;
    bool learned = false;
};

} // namespace

// Rubriktexten normaliserad: orden skilda med ett blanksteg.
std::string normalizeHeader(std::string_view text)
{
    std::string out;
    for (const std::string& w : normalizedWords(text)) {
        if (!out.empty())
            out += ' ';
        out += w;
    }
    return out;
}

// Rubrikområdet: kolumnnummerraden om den finns, annars ankarraden (planen 5.2).
HeaderArea findHeaderArea(const xlsx::Sheet& sheet)
{
    const int numberRow = findNumberRow(sheet);
    if (numberRow > 0) {
        const HeaderArea area = areaAboveNumberRow(sheet, numberRow);
        if (area.firstRow > 0)
            return area;
    }
    return areaFromAnchor(sheet);
}

// Kolumnerna med sammansatta rubriker (planen 5.3): rubrikraderna uppifrån, sammanslagna celler
// ifyllda, delarna rensade och upprepningar borttagna.
std::vector<SourceColumn> sourceColumns(const xlsx::Sheet& sheet, const HeaderArea& header)
{
    std::vector<xlsx::Merge> merges;
    int maxColumn = sheet.lastColumn;
    if (header.firstRow > 0) {
        for (const xlsx::Merge& m : sheet.merges) {
            if (m.firstRow >= header.firstRow && m.firstRow <= header.lastRow) {
                merges.push_back(m);
                maxColumn = std::max(maxColumn, m.lastColumn);
            }
        }
    }
    // Kolumnerna som har en cell på dataraderna.
    std::set<int> dataColumns;
    for (const xlsx::Cell& c : sheet.cells)
        if (c.row >= detail::dataStartRow(header))
            dataColumns.insert(c.column);

    std::vector<SourceColumn> out;
    for (int col = 1; col <= maxColumn; ++col) {
        SourceColumn sc;
        sc.column = col;
        sc.letter = xlsx::columnLetters(col);
        std::string previousKey;
        if (header.firstRow > 0) {
            for (int row = header.firstRow; row <= header.lastRow; ++row) {
                const xlsx::Cell* cell = detail::cellAt(sheet, row, col);
                std::string text = cell ? cell->text : std::string();
                if (!cell) {
                    for (const xlsx::Merge& m : merges) {
                        if (row >= m.firstRow && row <= m.lastRow && col >= m.firstColumn && col <= m.lastColumn) {
                            const xlsx::Cell* top = detail::cellAt(sheet, m.firstRow, m.firstColumn);
                            if (top)
                                text = top->text;
                            break;
                        }
                    }
                }
                text = detail::collapseSpaces(text);
                if (text.empty())
                    continue;
                const std::string key = normalizeHeader(text);
                if (!sc.parts.empty() && (text == sc.parts.back() || (!key.empty() && key == previousKey)))
                    continue;
                sc.parts.push_back(text);
                previousKey = key;
            }
        }
        if (sc.parts.empty() && !dataColumns.count(col))
            continue;
        for (std::size_t i = 0; i < sc.parts.size(); ++i) {
            if (i > 0) {
                sc.label += kPartSeparator;
                sc.key += " | ";
            }
            sc.label += sc.parts[i];
            sc.key += normalizeHeader(sc.parts[i]);
        }
        out.push_back(std::move(sc));
    }
    return out;
}

// Regelpoängen för ett fält och en kolumn (planen 5.5).
Candidate scoreField(int field, const SourceColumn& column)
{
    return scoreWords(field, column.column, wordsOf(column.parts));
}

// Hela igenkänningen (planen 5.2-5.6): området, kolumnerna, poängen med dataprov och inlärning, och valet.
Recognition recognize(const xlsx::Sheet& sheet, const Learned& learned)
{
    using namespace poletable;
    Recognition rec;
    rec.header = findHeaderArea(sheet);
    rec.columns = sourceColumns(sheet, rec.header);
    findTitle(sheet, rec.header, rec);

    // Poängen per fält och kolumn, med dataprovet och inlärningen.
    const std::vector<ColumnStats> stats = columnStats(sheet, detail::dataStartRow(rec.header));
    std::array<std::vector<Scored>, kColumnCount> all;
    for (const SourceColumn& col : rec.columns) {
        const PartWords pw = wordsOf(col.parts);
        const ColumnStats st = col.column < static_cast<int>(stats.size()) ? stats[static_cast<std::size_t>(col.column)]
                                                                            : ColumnStats{};
        for (int f = 0; f < kColumnCount; ++f) {
            Scored s;
            s.cand = scoreWords(f, col.column, pw);
            const auto& list = learned[static_cast<std::size_t>(f)];
            if (!col.key.empty() && std::find(list.begin(), list.end(), col.key) != list.end()) {
                s.learned = true;
                s.cand.score = kLearnedScore;
                s.cand.reason = "ditt tidigare val";
            } else if (s.cand.score > 0) {
                if (isNumberColumn(f) && st.values > 0 && st.numbers * 2 < st.values) {
                    s.cand.score = std::max(0, s.cand.score - 40);
                    s.cand.reason += ", mest text i kolumnen " + signedPoints(-40);
                } else if (f == kPole && st.values == 0) {
                    s.cand.score = std::max(0, s.cand.score - 30);
                    s.cand.reason += ", inga värden " + signedPoints(-30);
                }
            }
            if (s.cand.score > 0)
                all[static_cast<std::size_t>(f)].push_back(std::move(s));
        }
    }
    for (auto& list : all) {
        std::sort(list.begin(), list.end(), [](const Scored& a, const Scored& b) {
            return a.cand.score != b.cand.score ? a.cand.score > b.cand.score : a.cand.column < b.cand.column;
        });
    }

    // Valet: giriga par efter poäng, varje fält och varje kolumn högst en gång.
    struct Pair {
        int field;
        int column;
        int score;
        bool learned;
    };
    std::vector<Pair> pairs;
    for (int f = 0; f < kColumnCount; ++f)
        for (const Scored& s : all[static_cast<std::size_t>(f)])
            if (s.cand.score >= kMinScore)
                pairs.push_back({f, s.cand.column, s.cand.score, s.learned});
    std::sort(pairs.begin(), pairs.end(), [](const Pair& a, const Pair& b) {
        if (a.score != b.score)
            return a.score > b.score;
        return a.field != b.field ? a.field < b.field : a.column < b.column;
    });
    std::array<const Pair*, kColumnCount> assigned{};
    std::set<int> taken;
    for (const Pair& p : pairs) {
        if (assigned[static_cast<std::size_t>(p.field)] || taken.count(p.column))
            continue;
        assigned[static_cast<std::size_t>(p.field)] = &p;
        taken.insert(p.column);
    }

    // Säkerheten per fält mot näst bästa kolumn som ingen annan har tagit (planen 5.6 punkt 2).
    for (int f = 0; f < kColumnCount; ++f) {
        FieldMatch& fm = rec.fields[static_cast<std::size_t>(f)];
        const auto& list = all[static_cast<std::size_t>(f)];
        for (std::size_t i = 0; i < list.size() && i < 5; ++i)
            fm.candidates.push_back(list[i].cand);
        const Pair* p = assigned[static_cast<std::size_t>(f)];
        if (!p) {
            fm.confidence = Confidence::Missing;
            fm.column = f == kPole ? -1 : 0;
            continue;
        }
        int next = 0;
        bool otherLearned = false;
        for (const Scored& s : list) {
            if (s.cand.column == p->column || taken.count(s.cand.column))
                continue;
            next = std::max(next, s.cand.score);
            otherLearned = otherLearned || s.learned;
        }
        const int margin = p->score - next;
        fm.column = p->column;
        if (p->learned && !otherLearned) {
            fm.confidence = Confidence::Chosen;
        } else if (p->score >= kSureScore && margin >= kSureMargin) {
            fm.confidence = Confidence::Sure;
        } else if (margin >= kLikelyMargin) {
            fm.confidence = Confidence::Likely;
        } else {
            fm.confidence = Confidence::Ambiguous;
            fm.column = -1;
        }
    }
    const Confidence pc = rec.fields[kPole].confidence;
    rec.looksLikePoleTable = pc == Confidence::Sure || pc == Confidence::Likely || pc == Confidence::Chosen;
    return rec;
}

// Bladet som mest liknar en stolptabell (planen 5.1): ett synligt blad som heter "Stolptabell", annars
// det synliga bladet med flest säkra eller troliga fält, och bara dolda blad när inget är synligt.
int bestSheet(const xlsx::Workbook& book, const Learned& learned)
{
    int best = 0;
    int bestCount = -1;
    bool bestVisible = false;
    for (std::size_t i = 0; i < book.sheets.size(); ++i) {
        const xlsx::Sheet& sheet = book.sheets[i];
        if (!sheet.hidden && normalizeHeader(sheet.name) == "stolptabell")
            return static_cast<int>(i);
        const Recognition rec = recognize(sheet, learned);
        int count = 0;
        for (const FieldMatch& fm : rec.fields)
            if (fm.confidence == Confidence::Sure || fm.confidence == Confidence::Likely ||
                fm.confidence == Confidence::Chosen)
                ++count;
        const bool visible = !sheet.hidden;
        if ((visible && !bestVisible) || (visible == bestVisible && count > bestCount)) {
            best = static_cast<int>(i);
            bestCount = count;
            bestVisible = visible;
        }
    }
    return best;
}

// Kolumnen per fält ur valet.
Mapping mappingOf(const Recognition& rec)
{
    Mapping m{};
    for (int f = 0; f < poletable::kColumnCount; ++f)
        m[static_cast<std::size_t>(f)] = rec.fields[static_cast<std::size_t>(f)].column;
    return m;
}

// Säkerheten som text i dialogen.
std::string confidenceText(Confidence confidence)
{
    switch (confidence) {
    case Confidence::Sure:
        return "Säker";
    case Confidence::Likely:
        return "Trolig";
    case Confidence::Ambiguous:
        return "Välj";
    case Confidence::Missing:
        return "Saknas";
    case Confidence::Chosen:
        return "Ditt val";
    }
    return "";
}

} // namespace kalkyl::poleimport
