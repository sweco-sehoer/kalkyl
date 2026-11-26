#include <algorithm>
#include <iostream>
#include <string>
#include <vector>

#include "banken_fixture.h"
#include "poleImport/pole_import.h"

using namespace kalkyl;
using namespace kalkyl::poleimport;
using kalkyl::testfixture::kLf;
using kalkyl::testfixture::SheetBuilder;

namespace {

int g_failures = 0;

// Räknar ett misslyckande och skriver var det hände.
void report(bool ok, const char* expr, const char* file, int line)
{
    if (ok)
        return;
    ++g_failures;
    std::cerr << file << ":" << line << ": CHECK misslyckades: " << expr << std::endl;
}
#define CHECK(...) report(static_cast<bool>(__VA_ARGS__), #__VA_ARGS__, __FILE__, __LINE__)

// Minustecknet (U+2212) och skiljetecknet i rubrikerna (U+203A), byggda med byte.
const std::string kMinus = "\xE2\x88\x92";
const std::string kSep = " \xE2\x80\xBA ";

// Kolumnen med numret i igenkänningen, eller nullptr.
const SourceColumn* colOf(const Recognition& rec, int column)
{
    for (const SourceColumn& c : rec.columns)
        if (c.column == column)
            return &c;
    return nullptr;
}

// Fältets bästa kandidat med kolumnen, eller en tom kandidat.
Candidate candidateOf(const Recognition& rec, int field, int column)
{
    for (const Candidate& c : rec.fields[static_cast<std::size_t>(field)].candidates)
        if (c.column == column)
            return c;
    return {};
}

// Banken Ver. 6 med de tio stolparna i bilaga A.
xlsx::Sheet banken()
{
    return testfixture::bankenV6(testfixture::annexRows());
}

// Tar bort alla celler i kolumnen från och med raden.
void dropColumn(xlsx::Sheet& sheet, int column, int fromRow)
{
    std::vector<xlsx::Cell> kept;
    for (const xlsx::Cell& c : sheet.cells)
        if (!(c.column == column && c.row >= fromRow))
            kept.push_back(c);
    sheet.cells = kept;
}

// En platt tabell: rubrikerna i headerRow från kolumn 1 och dataraderna under, tom text ger ingen cell.
xlsx::Sheet flatTable(const std::vector<std::string>& headers, const std::vector<std::vector<std::string>>& data,
                      int headerRow = 1)
{
    SheetBuilder b;
    b.sheet.name = "Blad1";
    for (std::size_t c = 0; c < headers.size(); ++c)
        b.text(headerRow, static_cast<int>(c) + 1, headers[c]);
    int r = headerRow + 1;
    for (const auto& row : data) {
        for (std::size_t c = 0; c < row.size(); ++c)
            b.text(r, static_cast<int>(c) + 1, row[c]);
        ++r;
    }
    return b.finish();
}

// Två datarader som passar rubrikerna i flatTable (Stolpnr, N, E, H, Avstånd, Ah, Av, LKL).
std::vector<std::vector<std::string>> flatData()
{
    return {{"ST0001", "6511700,00", "411950,00", "42,5", "300", "310", "310", "255/262"},
            {"ST0002", "6511380,00", "411900,00", "45,0", "320", "300", "300", ""}};
}

// Rubrikerna i en platt tabell med de omdöpta namnen.
std::vector<std::string> renamedHeaders()
{
    return {"Stolpnummer", "Koordinat N", "Koordinat E", "Koordinat H", "Spannlängd", "Ah", "Av", "LKL"};
}

// Normaliseringen: gemener, å/ä/ö vikta, skiljetecken bort och förkortningar utskrivna.
void testNormalize()
{
    CHECK(normalizeHeader("Avst föreg stolpe") == "avstand foregaende stolpe");
    CHECK(normalizeHeader("Stpl nr") == "stolpe nummer");
    CHECK(normalizeHeader("Avst. föreg.") == "avstand foregaende");
    CHECK(normalizeHeader("Längdklass") == "langdklass");
    CHECK(normalizeHeader("LKL") == "langdklass");
    CHECK(normalizeHeader("RH2000") == "rh 2000");
    CHECK(normalizeHeader("Avst\xC2\xA0" "föreg" + kLf + "stolpe") == "avstand foregaende stolpe");
    CHECK(normalizeHeader("Stolpnummer") == "stolpe nummer");
    CHECK(normalizeHeader("Stolpnr") == "stolpe nummer");
    CHECK(normalizeHeader("  Beräknat från  koordinater ") == "berakna fran koordinat");
    CHECK(normalizeHeader("Max temp") == "max temperatur");
    CHECK(normalizeHeader("Ordningsnr") == "ordning nummer");
    CHECK(normalizeHeader("Spannlängd") == "spann langd");
    CHECK(normalizeHeader("Easting") == "e");
    CHECK(normalizeHeader("Ö-Å (ä)") == "o a a");
    CHECK(normalizeHeader("") == "");
}

// Rubrikområdet i Banken, i en enkel tabell och under en titel.
void testHeaderArea()
{
    const xlsx::Sheet b = banken();
    const HeaderArea h = findHeaderArea(b);
    CHECK(h.firstRow == 11);
    CHECK(h.lastRow == 15);
    CHECK(h.numberRow == 16);

    const xlsx::Sheet simple = flatTable({"Stolpnr", "N", "E", "H", "Avstånd föregående", "Ah", "Av", "LKL"}, flatData());
    CHECK(findHeaderArea(simple) == (HeaderArea{1, 1, 0}));

    // Rubriken i rad 4 under en titel, med tomma rader emellan.
    SheetBuilder under;
    under.text(1, 1, "Provlinje L1");
    under.text(2, 1, "Mall Ver. 11");
    const std::vector<std::string> names = {"Stolpnr", "N", "E", "H", "Avstånd föregående", "Ah", "Av", "LKL"};
    for (std::size_t c = 0; c < names.size(); ++c)
        under.text(4, static_cast<int>(c) + 1, names[c]);
    for (std::size_t c = 0; c < flatData()[0].size(); ++c)
        under.text(5, static_cast<int>(c) + 1, flatData()[0][c]);
    CHECK(findHeaderArea(under.finish()) == (HeaderArea{4, 4, 0}));

    // En tom blad har inget område.
    CHECK(findHeaderArea(xlsx::Sheet{}) == (HeaderArea{}));
}

// De sammansatta rubrikerna: sammanslagna celler ger sin text åt alla kolumner de täcker.
void testSourceColumns()
{
    const xlsx::Sheet b = banken();
    const std::vector<SourceColumn> cols = sourceColumns(b, findHeaderArea(b));
    const auto find = [&](int column) -> const SourceColumn* {
        for (const SourceColumn& c : cols)
            if (c.column == column)
                return &c;
        return nullptr;
    };
    const SourceColumn* w = find(23);
    CHECK(w && w->letter == "W");
    CHECK(w && w->parts == (std::vector<std::string>{"Profil", "Av", "Max temp", "m"}));
    CHECK(w && w->label == "Profil" + kSep + "Av" + kSep + "Max temp" + kSep + "m");
    CHECK(w && w->key == "profil | av | max temperatur | m");
    const SourceColumn* i = find(9);
    CHECK(i && i->parts == (std::vector<std::string>{"Koordinater", "N", "SWEREF 99 TM"}));
    // AG: "Stolpe" i rad 11 och 12 ger en del.
    const SourceColumn* ag = find(33);
    CHECK(ag && ag->parts == (std::vector<std::string>{"Stolpe", "Längdklass"}));
    // Gruppen B11:F11 ger sin text åt C-F.
    const SourceColumn* d = find(4);
    CHECK(d && d->parts == (std::vector<std::string>{"Objekt ID"}));
    // Gruppen BN11:CB11 sträcker sig förbi sista cellen.
    const SourceColumn* cb = find(80);
    CHECK(cb && cb->parts == (std::vector<std::string>{"Linor / Isolation"}));
    // Radbrytningar i rubriker blir blanksteg.
    const SourceColumn* m = find(13);
    CHECK(m && m->parts.back() == "m");
    CHECK(m && m->parts[2] == "Beräknat totalavst. från station");
    // Kolumnnummerraden ingår inte.
    CHECK(w && w->parts.size() == 4);

    // En grupp sammanslagen över tre kolumner ger sin text åt alla tre.
    SheetBuilder sb;
    sb.text(1, 1, "Stolpnr");
    sb.text(1, 2, "Grupp");
    sb.merge(1, 2, 1, 4);
    sb.text(2, 2, "Koordinat N");
    sb.text(2, 3, "Koordinat E");
    sb.text(2, 4, "Koordinat H");
    sb.text(3, 1, "ST0001");
    sb.number(3, 2, "6511700");
    const xlsx::Sheet g = sb.finish();
    const std::vector<SourceColumn> gc = sourceColumns(g, HeaderArea{1, 2, 0});
    CHECK(gc.size() == 4);
    CHECK(gc.size() == 4 && gc[1].parts == (std::vector<std::string>{"Grupp", "Koordinat N"}));
    CHECK(gc.size() == 4 && gc[2].parts == (std::vector<std::string>{"Grupp", "Koordinat E"}));
    CHECK(gc.size() == 4 && gc[3].parts == (std::vector<std::string>{"Grupp", "Koordinat H"}));
}

// Banken Ver. 6: alla åtta fält är Säker på rätt kolumn, och poängen i planen 5.5 stämmer.
void testBanken()
{
    const xlsx::Sheet sheet = banken();
    const Recognition rec = recognize(sheet);
    CHECK(rec.header == (HeaderArea{11, 15, 16}));
    CHECK(rec.title == "Stolptabell i Banken Ver. 6 GRUNDLÄGGNING & JORDNING");
    CHECK(rec.version == "Ver. 6");
    CHECK(rec.knownVersion);
    CHECK(rec.looksLikePoleTable);
    const int expected[] = {18, 9, 10, 11, 21, 22, 23, 33};
    for (int f = 0; f < poletable::kColumnCount; ++f) {
        CHECK(rec.fields[static_cast<std::size_t>(f)].confidence == Confidence::Sure);
        CHECK(rec.fields[static_cast<std::size_t>(f)].column == expected[f]);
    }
    CHECK(mappingOf(rec) == (Mapping{18, 9, 10, 11, 21, 22, 23, 33}));

    CHECK(candidateOf(rec, poletable::kPole, 18).score == 95);
    CHECK(candidateOf(rec, poletable::kPole, 19).score == 25);
    CHECK(candidateOf(rec, poletable::kNorth, 9).score == 95);
    CHECK(candidateOf(rec, poletable::kEast, 10).score == 95);
    CHECK(candidateOf(rec, poletable::kHeight, 11).score == 95);
    CHECK(candidateOf(rec, poletable::kSpan, 21).score == 80);
    CHECK(candidateOf(rec, poletable::kAh, 22).score == 75);
    CHECK(candidateOf(rec, poletable::kAh, 14).score == 5);
    CHECK(candidateOf(rec, poletable::kAv, 23).score == 95);
    CHECK(candidateOf(rec, poletable::kAv, 24).score == 25);
    CHECK(candidateOf(rec, poletable::kLkl, 33).score == 80);
    CHECK(candidateOf(rec, poletable::kSpan, 12).score == 0);

    // Skälen.
    CHECK(candidateOf(rec, poletable::kSpan, 21).reason == "avstånd föregående 60, stolpe +10, profil +10");
    CHECK(candidateOf(rec, poletable::kAv, 23).reason == "Av 60, max +20, profil +10, m +5");
    CHECK(candidateOf(rec, poletable::kPole, 18).reason == "stolpnummer 60, slutgiltigt +30, profil +5");
    CHECK(candidateOf(rec, poletable::kAv, 24).reason ==
          "Av 60, profil +10, m +5, min " + kMinus + "50");

    // Kandidaterna är bäst först, högst fem.
    CHECK(rec.fields[poletable::kAv].candidates.size() == 2);
    CHECK(rec.fields[poletable::kAv].candidates[0].column == 23);
    CHECK(rec.fields[poletable::kAv].candidates[1].column == 24);
    for (const FieldMatch& fm : rec.fields)
        CHECK(fm.candidates.size() <= 5);

    // Dataprovet: E under Stagförankring (BH) har text i kolumnen och får 10 poäng.
    const Candidate bh = candidateOf(rec, poletable::kEast, 60);
    CHECK(bh.score == 10);
    CHECK(bh.reason == "E 50, mest text i kolumnen " + kMinus + "40");

    // Namnen på säkerheten.
    CHECK(confidenceText(Confidence::Sure) == "Säker");
    CHECK(confidenceText(Confidence::Likely) == "Trolig");
    CHECK(confidenceText(Confidence::Ambiguous) == "Välj");
    CHECK(confidenceText(Confidence::Missing) == "Saknas");
    CHECK(confidenceText(Confidence::Chosen) == "Ditt val");
}

// De nära varianterna väljs aldrig: Min temp, Byggnads, beräknad Ah och Avstånd, och E under Stagförankring.
void testNearVariants()
{
    const xlsx::Sheet sheet = banken();
    const Recognition rec = recognize(sheet);
    const auto score = [&](int field, int column) {
        const SourceColumn* c = colOf(rec, column);
        return c ? scoreField(field, *c).score : -1;
    };
    CHECK(score(poletable::kAv, 24) == 25);
    CHECK(score(poletable::kPole, 19) == 25);
    CHECK(score(poletable::kAh, 14) == 5);
    CHECK(score(poletable::kSpan, 12) == 0);
    CHECK(score(poletable::kEast, 60) == 50);
    CHECK(score(poletable::kHeight, 49) == 0);
    for (const int taken : {24, 19, 14, 12, 60}) {
        for (const FieldMatch& fm : rec.fields)
            CHECK(fm.column != taken);
    }
    // Ordningsnr är inte Stolpnr.
    CHECK(score(poletable::kPole, 2) == 0);
}

// Samma rubriker i en annan ordning: Stolpnr i B, LKL i C och N, E och H i X, Y och Z.
void testMovedColumns()
{
    SheetBuilder b;
    b.sheet.name = "Stolptabell";
    b.text(1, 24, "Koordinater");
    b.merge(1, 24, 1, 26);
    const char* names[][2] = {{"2", "Stolpnr"}, {"3", "LKL"},  {"24", "N"},  {"25", "E"},
                              {"26", "H"},      {"27", "Avstånd föregående"}, {"28", "Ah"}, {"29", "Av"}};
    for (const auto& n : names)
        b.text(2, std::stoi(n[0]), n[1]);
    b.text(3, 2, "ST0001");
    b.text(3, 3, "255");
    b.text(3, 24, "6511700,00");
    b.text(3, 25, "411950,00");
    b.text(3, 26, "42,5");
    b.text(3, 27, "300");
    b.text(3, 28, "310");
    b.text(3, 29, "310");
    const Recognition rec = recognize(b.finish());
    CHECK(rec.looksLikePoleTable);
    CHECK(mappingOf(rec) == (Mapping{2, 24, 25, 26, 27, 28, 29, 3}));
    for (const FieldMatch& fm : rec.fields)
        CHECK(fm.confidence == Confidence::Sure || fm.confidence == Confidence::Likely);
}

// Omdöpta rubriker känns igen, och en annan version ger samma val men inte knownVersion.
void testRenamed()
{
    SheetBuilder b;
    b.sheet.name = "Blad1";
    const std::vector<std::string> headers = renamedHeaders();
    for (std::size_t c = 0; c < headers.size(); ++c)
        b.text(1, static_cast<int>(c) + 1, headers[c]);
    b.text(2, 7, "Max temperatur");
    int r = 3;
    for (const auto& row : flatData()) {
        for (std::size_t c = 0; c < row.size(); ++c)
            b.text(r, static_cast<int>(c) + 1, row[c]);
        ++r;
    }
    const Recognition rec = recognize(b.finish());
    CHECK(rec.header == (HeaderArea{1, 2, 0}));
    CHECK(rec.looksLikePoleTable);
    CHECK(mappingOf(rec) == (Mapping{1, 2, 3, 4, 5, 6, 7, 8}));
    for (const FieldMatch& fm : rec.fields)
        CHECK(fm.confidence == Confidence::Sure || fm.confidence == Confidence::Likely);
    CHECK(rec.fields[poletable::kAv].confidence == Confidence::Sure);
    CHECK(rec.title.empty());
    CHECK(!rec.knownVersion);

    // Version 7 under en titel.
    xlsx::Sheet v7 = flatTable(headers, flatData(), 3);
    v7.cells.insert(v7.cells.begin(), {1, 1, xlsx::CellKind::Text, "Stolptabell i Banken Ver. 7", -1});
    const Recognition rec7 = recognize(v7);
    CHECK(rec7.version == "Ver. 7");
    CHECK(rec7.title == "Stolptabell i Banken Ver. 7");
    CHECK(!rec7.knownVersion);
    CHECK(mappingOf(rec7) == (Mapping{1, 2, 3, 4, 5, 6, 7, 8}));
}

// En kolumn som saknas ger Saknas, och utan Stolpnr stoppar importen.
void testMissing()
{
    xlsx::Sheet noLkl = banken();
    dropColumn(noLkl, 33, 13);
    const Recognition rec = recognize(noLkl);
    CHECK(rec.fields[poletable::kLkl].confidence == Confidence::Missing);
    CHECK(rec.fields[poletable::kLkl].column == 0);
    CHECK(rec.looksLikePoleTable);
    const Mapping m = mappingOf(rec);
    const Extraction ex = extract(noLkl, rec.header, m);
    CHECK(ex.rows.size() == 10);
    CHECK(ex.rows.size() == 10 && ex.rows[0].pole.lkl.empty());
    CHECK(importIssues(rec, m, ex, 0, 9).empty());

    xlsx::Sheet noPole = banken();
    dropColumn(noPole, 18, 12);
    const Recognition rec2 = recognize(noPole);
    CHECK(rec2.fields[poletable::kPole].confidence == Confidence::Missing);
    CHECK(rec2.fields[poletable::kPole].column == -1);
    CHECK(!rec2.looksLikePoleTable);
    const Mapping m2 = mappingOf(rec2);
    const Extraction ex2 = extract(noPole, rec2.header, m2);
    CHECK(ex2.rows.empty());
    const std::vector<Issue> issues = importIssues(rec2, m2, ex2, 0, -1);
    CHECK(!issues.empty());
    CHECK(!issues.empty() && issues[0].blocking && issues[0].text == "Välj kolumnen för Stolpnr.");
}

// Två kolumner med samma rubrik ger Välj, båda kandidaterna och ett problem som stoppar.
void testAmbiguous()
{
    xlsx::Sheet sheet = banken();
    // CL (90) har samma rubrik som V: Profil, Ah, m.
    sheet.cells.push_back({11, 90, xlsx::CellKind::Text, "Profil", -1});
    sheet.cells.push_back({12, 90, xlsx::CellKind::Text, "Ah", -1});
    sheet.cells.push_back({15, 90, xlsx::CellKind::Text, "m", -1});
    for (int r = 19; r < 29; ++r)
        sheet.cells.push_back({r, 90, xlsx::CellKind::Text, "310", -1});
    std::sort(sheet.cells.begin(), sheet.cells.end(), [](const xlsx::Cell& a, const xlsx::Cell& b) {
        return a.row != b.row ? a.row < b.row : a.column < b.column;
    });
    sheet.lastColumn = 90;
    const Recognition rec = recognize(sheet);
    const FieldMatch& ah = rec.fields[poletable::kAh];
    CHECK(ah.confidence == Confidence::Ambiguous);
    CHECK(ah.column == -1);
    CHECK(ah.candidates.size() >= 2);
    CHECK(ah.candidates.size() >= 2 && ah.candidates[0].column == 22 && ah.candidates[1].column == 90);
    CHECK(rec.fields[poletable::kAv].confidence == Confidence::Sure);
    const Mapping m = mappingOf(rec);
    CHECK(m[poletable::kAh] == -1);
    const Extraction ex = extract(sheet, rec.header, m);
    const std::vector<Issue> issues = importIssues(rec, m, ex, 0, 9);
    CHECK(issues.size() == 1);
    CHECK(issues.size() == 1 && issues[0].blocking);
    CHECK(issues.size() == 1 && issues[0].text == "Välj kolumn för Ah: V och CL passar båda.");
}

// Inlärning: en okänd rubrik saknas, och med inlärningen blir den Ditt val med 100 poäng.
void testLearned()
{
    const xlsx::Sheet sheet = flatTable({"Stolpbeteckning", "Koordinat N", "Koordinat E"},
                                        {{"ST0001", "6511700,00", "411950,00"}, {"ST0002", "6511380,00", "411900,00"}});
    const Recognition plain = recognize(sheet);
    CHECK(plain.fields[poletable::kPole].confidence == Confidence::Missing);
    CHECK(plain.fields[poletable::kPole].column == -1);
    CHECK(!plain.looksLikePoleTable);

    Learned learned;
    learned[poletable::kPole] = {"stolpbeteckning"};
    const Recognition rec = recognize(sheet, learned);
    CHECK(rec.fields[poletable::kPole].confidence == Confidence::Chosen);
    CHECK(rec.fields[poletable::kPole].column == 1);
    CHECK(rec.looksLikePoleTable);
    CHECK(candidateOf(rec, poletable::kPole, 1).score == kLearnedScore);
    CHECK(candidateOf(rec, poletable::kPole, 1).reason == "ditt tidigare val");
    CHECK(rec.columns.size() == 3 && rec.columns[0].key == "stolpbeteckning");

    // Två kolumner med samma inlärda rubrik ger Välj.
    const xlsx::Sheet twice = flatTable({"Stolpbeteckning", "Stolpbeteckning", "Koordinat N", "Koordinat E"},
                                        {{"ST0001", "ST0001", "6511700,00", "411950,00"}});
    const Recognition rec2 = recognize(twice, learned);
    CHECK(rec2.fields[poletable::kPole].confidence == Confidence::Ambiguous);
    CHECK(rec2.fields[poletable::kPole].column == -1);
}

// Dataprovet: en talkolumn med mest text väljs inte före en med tal.
void testDataTrial()
{
    const xlsx::Sheet sheet = flatTable({"Stolpnr", "N", "N", "E"},
                                        {{"ST0001", "abc", "6511700,00", "411950,00"},
                                         {"ST0002", "def", "6511380,00", "411900,00"},
                                         {"ST0003", "ghi", "6511100,00", "411860,00"}});
    const Recognition rec = recognize(sheet);
    CHECK(rec.fields[poletable::kNorth].column == 3);
    CHECK(candidateOf(rec, poletable::kNorth, 2).score == 10);
    CHECK(candidateOf(rec, poletable::kNorth, 3).score == 50);
    // Stolpnr utan värden får -30.
    const xlsx::Sheet empty = flatTable({"Stolpnr", "N", "E"}, {{"", "6511700,00", "411950,00"}});
    const Recognition rec2 = recognize(empty);
    CHECK(candidateOf(rec2, poletable::kPole, 1).score == 30);
    CHECK(candidateOf(rec2, poletable::kPole, 1).reason == "stolpnummer 60, inga värden " + kMinus + "30");
}

// Utdraget: tal, streck, ogiltig text, Stolpnr som tal, LKL med tom rad, rader utan Stolpnr och upprepade rubriker.
void testExtract()
{
    SheetBuilder b;
    b.sheet.name = "Blad1";
    const std::vector<std::string> headers = {"Stolpnr", "N", "E", "H", "Avstånd föregående", "Ah", "Av", "LKL"};
    for (std::size_t c = 0; c < headers.size(); ++c)
        b.text(1, static_cast<int>(c) + 1, headers[c]);
    // Rad 2: alla sorters värden.
    b.text(2, 1, " ST0001 ");
    b.text(2, 2, "7047024,75");
    b.number(2, 3, "422.10000000000002");
    b.number(2, 4, "6512000", 2);
    b.text(2, 5, "ca 300");
    b.text(2, 6, "-");
    b.text(2, 7, "485");
    b.text(2, 8, "T373" + kLf + kLf + "T373");
    // Rad 3: Stolpnr som tal och LKL som tal.
    b.number(3, 1, "1234");
    b.number(3, 8, "255");
    // Rad 4 är tom. Rad 5 saknar Stolpnr men har en koordinat.
    b.text(5, 2, "100");
    // Rad 6 är en upprepad rubrikrad.
    b.text(6, 1, "Stpl nr");
    b.text(6, 2, "N");
    // Rad 7: en rad.
    b.text(7, 1, "ST0004");
    b.text(7, 5, "–");
    // Rad 8: sant och fel i talfält.
    b.text(8, 1, "ST0005");
    b.sheet.cells.push_back({8, 2, xlsx::CellKind::Bool, "1", -1});
    b.sheet.cells.push_back({8, 3, xlsx::CellKind::Error, "#N/A", -1});
    // Rad 9 har bara blanktecken och streck: tyst överhoppad.
    b.text(9, 1, "   ");
    b.text(9, 2, "-");
    // Rad 10: radbrytning mitt i ett tal.
    b.text(10, 1, "ST0006");
    b.text(10, 2, "256" + kLf + "256");
    const xlsx::Sheet sheet = b.finish();

    // Kolumnerna väljs för hand, eftersom dataprovet med flit skulle välja bort Avstånd föregående.
    const HeaderArea header = findHeaderArea(sheet);
    CHECK(header == (HeaderArea{1, 1, 0}));
    const Mapping m{1, 2, 3, 4, 5, 6, 7, 8};
    const Extraction ex = extract(sheet, header, m);
    CHECK(ex.firstDataRow == 2);
    CHECK(ex.rows.size() == 5);
    CHECK(ex.skippedRows == (std::vector<int>{5}));
    if (ex.rows.size() != 5)
        return;
    // Rad 10: en radbrytning mitt i en talcell är inte ett tal.
    CHECK(ex.rows[4].excelRow == 10);
    CHECK(ex.rows[4].invalid[poletable::kNorth] == "256" + kLf + "256");
    CHECK(ex.rows[4].pole.north.empty());
    CHECK(ex.rows[0].excelRow == 2);
    CHECK(ex.rows[0].pole.pole == "ST0001");
    CHECK(ex.rows[0].pole.north == "7047024.75");
    CHECK(ex.rows[0].pole.east == "422.1");
    CHECK(ex.rows[0].pole.height == "6512000.00");
    CHECK(ex.rows[0].pole.spanM.empty());
    CHECK(ex.rows[0].invalid[poletable::kSpan] == "ca 300");
    CHECK(ex.rows[0].pole.ah.empty());
    CHECK(ex.rows[0].invalid[poletable::kAh].empty());
    CHECK(ex.rows[0].pole.av == "485");
    CHECK(ex.rows[0].pole.lkl == "T373/T373");
    CHECK(ex.rows[1].excelRow == 3);
    CHECK(ex.rows[1].pole.pole == "1234");
    CHECK(ex.rows[1].pole.lkl == "255");
    CHECK(ex.rows[2].excelRow == 7);
    CHECK(ex.rows[2].pole.pole == "ST0004");
    CHECK(ex.rows[2].invalid[poletable::kSpan].empty());
    CHECK(ex.rows[3].excelRow == 8);
    CHECK(ex.rows[3].invalid[poletable::kNorth] == "1");
    CHECK(ex.rows[3].invalid[poletable::kEast] == "#N/A");
    CHECK(ex.rows[3].pole.north.empty() && ex.rows[3].pole.east.empty());

    // poleRows ger raderna first..last.
    const std::vector<poletable::PoleRow> rows = poleRows(ex, 1, 2);
    CHECK(rows.size() == 2);
    CHECK(rows.size() == 2 && rows[0].pole == "1234" && rows[1].pole == "ST0004");
    CHECK(poleRows(ex, 3, 1).empty());
    CHECK(poleRows(ex, -5, 99).size() == 5);

    // Utan kolumn för Stolpnr blir det inga rader.
    Mapping none = m;
    none[poletable::kPole] = 0;
    CHECK(extract(sheet, header, none).rows.empty());
    // Ett fält som inte hämtas blir tomt.
    Mapping noAv = m;
    noAv[poletable::kAv] = 0;
    CHECK(extract(sheet, header, noAv).rows[0].pole.av.empty());
}

// Banken: alla tio stolpar ut till raderna i bilaga A, på Excelraderna 19-28, och en tom rad mitt i hoppas över.
void testExtractBanken()
{
    xlsx::Sheet sheet = banken();
    const Recognition rec = recognize(sheet);
    const Mapping m = mappingOf(rec);
    const Extraction ex = extract(sheet, rec.header, m);
    CHECK(ex.rows.size() == 10);
    CHECK(ex.firstDataRow == 19);
    CHECK(ex.skippedRows.empty());
    CHECK(poleRows(ex, 0, 9) == testfixture::annexRows());
    CHECK(ex.rows.size() == 10 && ex.rows[9].excelRow == 28);
    for (const SourceRow& r : ex.rows)
        for (const std::string& s : r.invalid)
            CHECK(s.empty());

    // Rad 17 och 18 är tomma i Banken, och en tom rad mitt i data ändrar inget.
    std::vector<poletable::PoleRow> rows = testfixture::annexRows();
    rows.insert(rows.begin() + 4, poletable::PoleRow{});
    const xlsx::Sheet gap = testfixture::bankenV6(rows);
    const Extraction exGap = extract(gap, rec.header, m);
    CHECK(exGap.rows.size() == 10);
    CHECK(exGap.rows.size() == 10 && exGap.rows[4].pole.pole == "ST0104");
}

// Rådataposten: raderna 2..4, alla celler, kolumnernas rubriker och fält, titel, version och sträckan.
void testImportRecord()
{
    const xlsx::Sheet sheet = banken();
    const Recognition rec = recognize(sheet);
    const Mapping m = mappingOf(rec);
    const Extraction ex = extract(sheet, rec.header, m);
    const poletable::ImportRecord r = importRecord(sheet, rec, m, ex, 2, 4, "Stolptabell.xlsm");
    CHECK(r.fileName == "Stolptabell.xlsm");
    CHECK(r.sheetName == "Stolptabell");
    CHECK(r.title == "Stolptabell i Banken Ver. 6 GRUNDLÄGGNING & JORDNING");
    CHECK(r.version == "Ver. 6");
    CHECK(r.importedUtc.empty());
    CHECK(r.headerFirstRow == 11 && r.headerLastRow == 15);
    CHECK(r.firstDataRow == 21);
    CHECK(r.fromPole == "ST0102");
    CHECK(r.toPole == "ST0104");
    CHECK(r.rows.size() == 3);
    if (r.rows.size() != 3)
        return;
    CHECK(r.rows[0].excelRow == 21 && r.rows[1].excelRow == 22 && r.rows[2].excelRow == 23);
    CHECK(r.rows[0].pole == "ST0102");
    // Alla celler på raden, också de som inte används: A, B, I, J, K, L, N, R, S, U, V, W, X, AG och BH.
    CHECK(r.rows[0].cells.size() == 15);
    CHECK(r.rows[0].cells.size() == 15 && r.rows[0].cells[0].column == 1 &&
          r.rows[0].cells[0].kind == poletable::ImportCellKind::Number && r.rows[0].cells[0].text == "214023");
    CHECK(r.rows[0].cells.size() == 15 && r.rows[0].cells[1].text == "L1:0003" &&
          r.rows[0].cells[1].kind == poletable::ImportCellKind::Text);
    CHECK(r.rows[0].cells.size() == 15 && r.rows[0].cells[13].column == 33 &&
          r.rows[0].cells[13].text == "262" + kLf + kLf + "262");
    // Radens celler är i kolumnordning.
    for (const poletable::ImportRow& row : r.rows)
        for (std::size_t i = 1; i < row.cells.size(); ++i)
            CHECK(row.cells[i - 1].column < row.cells[i].column);

    const auto colInfo = [&](int column) -> const poletable::ImportColumn* {
        for (const poletable::ImportColumn& c : r.columns)
            if (c.column == column)
                return &c;
        return nullptr;
    };
    const poletable::ImportColumn* pole = colInfo(18);
    CHECK(pole && pole->letter == "R" && pole->field == poletable::kPole &&
          pole->header == "Profil" + kSep + "Stpl nr" + kSep + "Slutgiltigt");
    const poletable::ImportColumn* id = colInfo(1);
    CHECK(id && id->field == -1 && id->header == "Internt Id");
    const poletable::ImportColumn* span = colInfo(21);
    CHECK(span && span->field == poletable::kSpan && span->header == "Profil" + kSep + "Avst föreg stolpe");
    const poletable::ImportColumn* lkl = colInfo(33);
    CHECK(lkl && lkl->field == poletable::kLkl && lkl->header == "Stolpe" + kSep + "Längdklass");
    const poletable::ImportColumn* other = colInfo(24);
    CHECK(other && other->field == -1);
    // En kolumn utan rubrik och utan cell på raderna kommer inte med.
    CHECK(colInfo(85) == nullptr);
    for (std::size_t i = 1; i < r.columns.size(); ++i)
        CHECK(r.columns[i - 1].column < r.columns[i].column);
    // Varje fält har högst en kolumn.
    for (int f = 0; f < poletable::kColumnCount; ++f) {
        int n = 0;
        for (const poletable::ImportColumn& c : r.columns)
            if (c.field == f)
                ++n;
        CHECK(n == 1);
    }
    // Samma anrop ger samma post, och en tom sträcka ger en post utan rader.
    CHECK(importRecord(sheet, rec, m, ex, 2, 4, "Stolptabell.xlsm") == r);
    const poletable::ImportRecord none = importRecord(sheet, rec, m, ex, 3, 2, "x.xlsx");
    CHECK(none.rows.empty() && none.firstDataRow == 0 && none.fromPole.empty());
}

// Problemen i 9.3: varje text exakt, i rätt ordning, och ental och flertal.
void testIssues()
{
    const xlsx::Sheet sheet = banken();
    const Recognition rec = recognize(sheet);
    const Mapping m = mappingOf(rec);
    const Extraction ex = extract(sheet, rec.header, m);
    CHECK(importIssues(rec, m, ex, 0, 9).empty());

    // Avstånd föregående hämtas inte.
    Mapping noSpan = m;
    noSpan[poletable::kSpan] = 0;
    std::vector<Issue> issues = importIssues(rec, noSpan, ex, 0, 9);
    CHECK(issues.size() == 1);
    CHECK(issues.size() == 1 && !issues[0].blocking &&
          issues[0].text == "Avstånd föregående hämtas inte och räknas ur koordinaterna.");

    // Samma kolumn för två fält.
    Mapping twice = m;
    twice[poletable::kAv] = 22;
    issues = importIssues(rec, twice, ex, 0, 9);
    CHECK(issues.size() == 1);
    CHECK(issues.size() == 1 && issues[0].blocking &&
          issues[0].text == "Kolumn V är vald för både Ah och Av. Välj olika kolumner.");
    Mapping three = m;
    three[poletable::kAv] = 22;
    three[poletable::kSpan] = 22;
    issues = importIssues(rec, three, ex, 0, 9);
    CHECK(issues.size() == 1 && issues[0].text == "Kolumn V är vald för både Avstånd föregående, Ah och Av. Välj olika kolumner.");

    // Stolpnr saknas, och inga stolpar under rubrikerna.
    Mapping noPole = m;
    noPole[poletable::kPole] = -1;
    issues = importIssues(rec, noPole, Extraction{}, 0, -1);
    CHECK(issues.size() == 1 && issues[0].blocking && issues[0].text == "Välj kolumnen för Stolpnr.");
    issues = importIssues(rec, m, Extraction{}, 0, -1);
    CHECK(issues.size() == 1 && issues[0].blocking &&
          issues[0].text == "Inga stolpar under rubrikerna i kolumn R (Profil" + kSep + "Stpl nr" + kSep + "Slutgiltigt).");

    // Till före Från.
    issues = importIssues(rec, m, ex, 5, 2);
    CHECK(issues.size() == 1 && issues[0].blocking &&
          issues[0].text == "Till står före Från i filen. Byt Från och Till.");

    // Ordningen: de som stoppar först, sedan ogiltiga tal, överhoppade rader, dubbletter och Avstånd.
    Extraction fake;
    for (int i = 0; i < 6; ++i) {
        SourceRow row;
        row.excelRow = 10 + i * 10;
        row.pole.pole = i < 4 ? "A" : (i == 4 ? "B" : "C");
        fake.rows.push_back(row);
    }
    fake.rows.push_back(fake.rows[4]);
    fake.rows.back().excelRow = 100;
    fake.rows[0].invalid[poletable::kNorth] = "x";
    fake.skippedRows = {15, 25, 35, 45, 55, 65, 70, 200};
    Mapping mixed = noSpan;
    mixed[poletable::kAh] = 22;
    mixed[poletable::kAv] = 22;
    issues = importIssues(rec, mixed, fake, 0, 6);
    std::vector<std::string> texts;
    for (const Issue& i : issues)
        texts.push_back(i.text);
    const std::vector<std::string> expected = {
        "Kolumn V är vald för både Ah och Av. Välj olika kolumner.",
        "1 ogiltigt tal lämnas tomt (rött i förhandsvisningen).",
        "7 rader utan Stolpnr hoppas över: rad 15, 25, 35, 45, 55 …",
        "Stolpnr A finns flera gånger. Dubbletterna markeras i tabellen efter importen.",
        "Stolpnr B finns flera gånger. Dubbletterna markeras i tabellen efter importen.",
        "Avstånd föregående hämtas inte och räknas ur koordinaterna.",
    };
    CHECK(texts == expected);
    CHECK(issues.size() == 6 && issues[0].blocking && !issues[1].blocking && !issues[5].blocking);

    // Flertal av ogiltiga tal, ett överhoppat rad och fler än tre dubbletter.
    fake.rows[1].invalid[poletable::kEast] = "y";
    fake.skippedRows = {15};
    for (int i = 0; i < 6; ++i) {
        SourceRow row;
        row.excelRow = 200 + i;
        row.pole.pole = std::string(1, static_cast<char>('D' + i / 2));
        fake.rows.push_back(row);
    }
    issues = importIssues(rec, m, fake, 0, 12);
    texts.clear();
    for (const Issue& i : issues)
        texts.push_back(i.text);
    CHECK(texts.size() == 6);
    CHECK(texts.size() == 6 && texts[0] == "2 ogiltiga tal lämnas tomma (röda i förhandsvisningen).");
    CHECK(texts.size() == 6 && texts[1] == "1 rad utan Stolpnr hoppas över: rad 15.");
    CHECK(texts.size() == 6 && texts[4] == "Stolpnr D finns flera gånger. Dubbletterna markeras i tabellen efter importen.");
    CHECK(texts.size() == 6 && texts[5] == "… och 2 till.");
}

// Sträckor som tappar sin start eller sitt slut i filen.
void testLostRanges()
{
    const std::vector<poletable::PoleRow> current = testfixture::annexRows();
    std::vector<poletable::PoleRow> imported = current;
    imported.erase(imported.begin() + 7);
    const std::vector<automode::NamedRange> ranges = {{"faslinan", {"ST0102", "ST0107"}},
                                                      {"topplinorna", {"ST0101", "ST0108"}}};
    const std::vector<std::string> lost = lostRanges(ranges, current, imported);
    CHECK(lost == (std::vector<std::string>{"Faslinan: Stolpnr ST0107 finns inte i filen."}));

    // Alla sträckor finns kvar: ingen rad.
    CHECK(lostRanges(ranges, current, current).empty());

    // En sträcka utan start, och en som redan har fel i nuvarande tabell, räknas inte.
    const std::vector<automode::NamedRange> invalid = {{"faslinan", {"", "ST0107"}},
                                                       {"topplinorna", {"ST0999", "ST0108"}},
                                                       {"topplina vänster", {"ST0101", ""}}};
    CHECK(lostRanges(invalid, current, imported).empty());

    // Slutet före starten i filen.
    std::vector<poletable::PoleRow> reversed(current.rbegin(), current.rend());
    const std::vector<std::string> both = lostRanges(ranges, current, reversed);
    CHECK(both.size() == 2);
    CHECK(both.size() == 2 &&
          both[0] == "Faslinan: Slutet ST0107 står före starten ST0102 i filen. Byt start och slut.");
    CHECK(both.size() == 2 &&
          both[1] == "Topplinorna: Slutet ST0108 står före starten ST0101 i filen. Byt start och slut.");
}

// Valet av blad: namnet Stolptabell, annars flest säkra fält, och dolda blad sist.
void testBestSheet()
{
    CHECK(bestSheet(xlsx::Workbook{}) == 0);

    xlsx::Sheet junk = flatTable({"foo", "bar"}, {{"1", "2"}});
    junk.name = "Blad1";
    xlsx::Sheet table = banken();
    xlsx::Workbook byName;
    byName.sheets = {junk, table};
    CHECK(bestSheet(byName) == 1);

    // Utan namnet vinner bladet med flest säkra fält.
    table.name = "Data";
    xlsx::Sheet partial = flatTable({"Stolpnr", "Koordinat N"}, {{"ST0001", "6511700,00"}});
    partial.name = "Delvis";
    xlsx::Workbook byFields;
    byFields.sheets = {junk, partial, table};
    CHECK(bestSheet(byFields) == 2);
    byFields.sheets = {junk, table, partial};
    CHECK(bestSheet(byFields) == 1);

    // Ett dolt blad väljs inte före ett synligt, och inte heller om det heter Stolptabell.
    xlsx::Sheet hidden = banken();
    hidden.hidden = true;
    xlsx::Workbook withHidden;
    withHidden.sheets = {hidden, partial};
    CHECK(bestSheet(withHidden) == 1);
    xlsx::Workbook onlyHidden;
    onlyHidden.sheets = {junk, hidden};
    onlyHidden.sheets[0].hidden = true;
    CHECK(bestSheet(onlyHidden) == 1);
}

} // namespace

int main()
{
    testNormalize();
    testHeaderArea();
    testSourceColumns();
    testBanken();
    testNearVariants();
    testMovedColumns();
    testRenamed();
    testMissing();
    testAmbiguous();
    testLearned();
    testDataTrial();
    testExtract();
    testExtractBanken();
    testImportRecord();
    testIssues();
    testLostRanges();
    testBestSheet();
    if (g_failures == 0)
        std::cout << "poleimport: alla kontroller lyckades" << std::endl;
    else
        std::cerr << "poleimport: " << g_failures << " kontroller misslyckades" << std::endl;
    return g_failures == 0 ? 0 : 1;
}
