// Headless-test för hela vägen från en Excelfil till projektet: en Banken-tabell byggs i minnet
// som riktig xlsx, läses, känns igen, dras ut och importeras med importPolesBefore. Allt sker i en
// egen temporär mapp med åäö i namnet. Första filen lämnas kvar som `Stolptabell prov.xlsx` i
// KALKYL_PROV_DIR åt röktestet och handtestet. Felkoden är antalet misslyckade kontroller.
#include <process.h>

#include <cmath>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

#include "banken_fixture.h"
#include "conductor/conductor.h"
#include "platform/files.h"
#include "platform/text.h"
#include "poleImport/pole_import.h"
#include "projectdb/project_data.h"
#include "projectdb/project_db.h"
#include "xlsx/xlsx.h"
#include "xlsx_maker.h"

using namespace kalkyl;
using projectdb::Stage;
using projectdb::Wire;
namespace fs = std::filesystem;

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
#define CHECK(cond) report(static_cast<bool>(cond), #cond, __FILE__, __LINE__)

// Fogar ihop mapp och namn till en UTF-8-sökväg.
std::string join(const std::string& dir, const std::string& name)
{
    return platform::pathToUtf8(platform::pathFromUtf8(dir) / platform::pathFromUtf8(name));
}

// Skriver en binärfil.
void writeBytes(const std::string& path, const std::string& bytes)
{
    std::ofstream out(platform::pathFromUtf8(path), std::ios::binary);
    out << bytes;
}

// Texten i första kolumnen på första raden i en SQL-fråga mot arbetskopian, "" utan rad.
std::string query(projectdb::ProjectDb& p, const std::string& sql)
{
    auto stmt = p.local().prepare(sql);
    CHECK(stmt.ok());
    if (!stmt.ok())
        return {};
    auto row = stmt.value().step();
    CHECK(row.ok());
    return row.ok() && row.value() ? stmt.value().text(0) : std::string{};
}

// En rad i stolptabellen med de åtta fälten i tabellens ordning.
poletable::PoleRow poleRow(const char* pole, const char* av, const char* ah, const char* span,
                           const char* north, const char* east, const char* height, const char* lkl)
{
    poletable::PoleRow row;
    row.pole = pole;
    row.av = av;
    row.ah = ah;
    row.spanM = span;
    row.north = north;
    row.east = east;
    row.height = height;
    row.lkl = lkl;
    return row;
}

// Raderna i filen: ST0098 och ST0099 före bilaga A (ST0100-ST0109) och ST0110 efter, 13 stolpar.
std::vector<poletable::PoleRow> fileRows()
{
    std::vector<poletable::PoleRow> rows;
    rows.push_back(poleRow("ST0098", "290", "290", "", "6512600.00", "412100.00", "38.0", ""));
    rows.push_back(poleRow("ST0099", "295", "295", "310", "6512300.00", "412050.00", "39.5", ""));
    for (const poletable::PoleRow& r : testfixture::annexRows())
        rows.push_back(r);
    rows.push_back(poleRow("ST0110", "270", "270", "260", "6508990.00", "411570.00", "39.0", ""));
    return rows;
}

// En giltig lina att sätta i Dokumentation.
conductor::Conductor testConductor()
{
    conductor::Conductor c;
    c.name = "593 mm² FeAl CURLEW";
    c.type = "FeAl CURLEW";
    c.areaMm2 = 593;
    c.diameterMm = 31.7;
    c.eInitialMpa = 52000;
    c.ePermanentMpa = 72000;
    c.eInitialLimitMpa = 120;
    c.eivConstant = 90;
    c.alphaE6PerC = 19;
    c.creepStrainPermille = 0.4;
    c.creepTempC = 21;
    c.weightNPerM = 19.6;
    c.breakingLoadKn = 165;
    return c;
}

// Läget på med faslinan ST0102-ST0107 och topplinorna ST0101-ST0108.
projectdb::AutoSettings standardSettings()
{
    projectdb::AutoSettings s;
    s.enabled = true;
    s.ranges[0] = {"ST0102", "ST0107"};
    s.ranges[1] = {"ST0101", "ST0108"};
    return s;
}

// Allt som importen inte får röra, som en text: Stolptabell efter, automatiskt läge för båda
// skedena, Dokumentation, de manuella normalspannsberäkningarna och projektets linor.
std::string snapshotOf(projectdb::ProjectDb& p)
{
    std::ostringstream o;
    auto after = projectdb::poles(p, Stage::After);
    CHECK(after.ok());
    if (after.ok())
        for (const poletable::PoleRow& row : after.value()) {
            for (int c = 0; c < poletable::kColumnCount; ++c)
                o << poletable::field(row, c) << '|';
            o << '\n';
        }
    for (Stage stage : {Stage::Before, Stage::After}) {
        auto settings = projectdb::autoSettings(p, stage);
        CHECK(settings.ok());
        if (!settings.ok())
            continue;
        o << "auto " << settings.value().enabled;
        for (const automode::PoleRange& r : settings.value().ranges)
            o << ' ' << r.fromPole << '-' << r.toPole;
        o << '\n';
    }
    auto docs = projectdb::docValues(p);
    CHECK(docs.ok());
    if (docs.ok())
        for (const projectdb::DocValue& v : docs.value())
            o << "doc " << v.key << '=' << v.value << '/' << v.reference << '\n';
    auto spans = projectdb::normalSpans(p);
    CHECK(spans.ok());
    if (spans.ok())
        for (const projectdb::NormalSpan& ns : spans.value()) {
            if (ns.automatic)
                continue;
            o << "span " << ns.id << ' ' << ns.note << ' ' << ns.createdUtc << ' ' << ns.modifiedUtc;
            for (const ancalc::SpanPole& pole : ns.poles)
                o << ' ' << pole.pole << ':' << pole.lengthM.value_or(-1) << ':' << pole.heightM.value_or(-1);
            o << '\n';
        }
    auto conductors = projectdb::projectConductors(p);
    CHECK(conductors.ok());
    if (conductors.ok())
        for (const conductor::Conductor& c : conductors.value())
            o << "lina " << c.id << ' ' << c.name << ' ' << c.areaMm2 << ' ' << c.diameterMm << '\n';
    return o.str();
}

// Förberedelsen: bilaga A i före och efter, några värden i Dokumentation, en manuell
// normalspannsberäkning och automatiskt läge på i före med faslinan ST0102-ST0107 och
// topplinorna ST0101-ST0108.
bool prepareProject(projectdb::ProjectDb& p)
{
    bool ok = true;
    ok = ok && projectdb::setPolesBefore(p, testfixture::annexRows()).ok();
    ok = ok && projectdb::copyBeforeToAfter(p).ok();
    ok = ok && projectdb::setDocConductor(p, projectdb::kPhaseConductorKey, testConductor()).ok();
    ok = ok && projectdb::setDocValue(p, {"temp_max", "60", "Prov"}).ok();
    ok = ok && projectdb::setDocValue(p, {"temp_min", "-40", ""}).ok();
    projectdb::NormalSpan ns;
    ns.note = "Manuell";
    ns.poles = {{"M1", std::nullopt, 40.0}, {"M2", 300.0, 41.0}, {"M3", 320.0, 42.0}};
    ok = ok && projectdb::saveNormalSpan(p, ns).ok();
    ok = ok && projectdb::setAutoSettings(p, Stage::Before, standardSettings()).ok();
    CHECK(ok);
    return ok;
}

// Resultatet av att läsa, känna igen och dra ut alla stolpar ur första bladet som bestSheet väljer.
struct Imported {
    bool looksLikePoleTable = false;
    std::vector<poletable::PoleRow> rows;
    poletable::ImportRecord record;
};

// Hela vägen i fönstret: bestSheet, recognize, extract, poleRows och importRecord för alla rader.
Imported readAll(const xlsx::Workbook& book, const std::string& fileName)
{
    Imported out;
    CHECK(!book.sheets.empty());
    if (book.sheets.empty())
        return out;
    const int index = poleimport::bestSheet(book);
    const xlsx::Sheet& sheet = book.sheets[static_cast<std::size_t>(index)];
    const poleimport::Recognition rec = poleimport::recognize(sheet);
    out.looksLikePoleTable = rec.looksLikePoleTable;
    const poleimport::Mapping mapping = poleimport::mappingOf(rec);
    const poleimport::Extraction ex = poleimport::extract(sheet, rec.header, mapping);
    if (ex.rows.empty())
        return out;
    const int last = static_cast<int>(ex.rows.size()) - 1;
    out.rows = poleimport::poleRows(ex, 0, last);
    out.record = poleimport::importRecord(sheet, rec, mapping, ex, 0, last, fileName);
    return out;
}

// Kolumnens bokstäver för fältet i en rådatapost, "" när ingen kolumn gav fältet.
std::string letterOf(const poletable::ImportRecord& record, int field)
{
    for (const poletable::ImportColumn& c : record.columns)
        if (c.field == field)
            return c.letter;
    return {};
}

// Hela flödet: första filen med 13 stolpar, och sedan en andra utan ST0107.
void testFlow(const std::string& dir)
{
    const std::vector<poletable::PoleRow> rows = fileRows();
    CHECK(rows.size() == 13);

    // Förberedelsen.
    const std::string share = join(dir, "Projekt åäö");
    CHECK(platform::ensureDirectory(share).ok());
    projectdb::OpenOptions options;
    options.workDir = join(dir, "arbete åäö");
    CHECK(platform::ensureDirectory(options.workDir).ok());
    const std::string path = join(share, "Importprov åäö.kalkyl");
    auto created = projectdb::ProjectDb::create(path, "Importprov åäö", options);
    CHECK(created.ok());
    if (!created.ok()) {
        std::cerr << created.error().message << " / " << created.error().detail << std::endl;
        return;
    }
    projectdb::ProjectDb& p = created.value();
    if (!prepareProject(p))
        return;
    const std::string snapshot = snapshotOf(p);
    CHECK(!snapshot.empty());
    auto autoBefore = projectdb::autoSettings(p, Stage::Before);
    CHECK(autoBefore.ok() && autoBefore.value().enabled);

    // Filen: riktig xlsx i minnet, läst med readWorkbook. Den lämnas kvar åt röktestet.
    const std::string bytes = testxlsx::fromSheets({testfixture::bankenV6(rows)});
    const std::string provDir = platform::absolutePath(KALKYL_PROV_DIR);
    CHECK(platform::ensureDirectory(provDir).ok());
    writeBytes(join(provDir, "Stolptabell prov.xlsx"), bytes);
    auto book = xlsx::readWorkbook(bytes, "Stolptabell prov.xlsx");
    CHECK(book.ok());
    if (!book.ok()) {
        std::cerr << book.error().message << " / " << book.error().detail << std::endl;
        return;
    }

    // Importen av första filen.
    const Imported first = readAll(book.value(), "Stolptabell prov.xlsx");
    CHECK(first.looksLikePoleTable);
    CHECK(first.rows == rows);
    CHECK(first.record.rows.size() == 13);
    CHECK(projectdb::importPolesBefore(p, first.rows, first.record).ok());

    // Kontrollerna: raderna, läget, allt annat orört och de automatiska beräkningarna.
    auto before = projectdb::poles(p, Stage::Before);
    CHECK(before.ok() && before.value() == rows);
    auto autoNow = projectdb::autoSettings(p, Stage::Before);
    CHECK(autoNow.ok() && autoNow.value() == autoBefore.value());
    CHECK(snapshotOf(p) == snapshot);
    CHECK(query(p, "SELECT first_row FROM auto_calc WHERE stage = 'before' AND wire = 'phase'") == "4");
    CHECK(query(p, "SELECT last_row FROM auto_calc WHERE stage = 'before' AND wire = 'phase'") == "9");
    CHECK(query(p, "SELECT issue FROM auto_calc WHERE stage = 'before' AND wire = 'phase'").empty());

    auto spans = projectdb::normalSpans(p);
    CHECK(spans.ok());
    if (spans.ok()) {
        bool foundPhase = false;
        bool foundTop = false;
        for (const projectdb::NormalSpan& ns : spans.value()) {
            if (!ns.automatic || ns.automatic->stage != Stage::Before)
                continue;
            const auto calc = projectdb::calculateSpan(ns);
            if (ns.automatic->wire == Wire::Phase) {
                foundPhase = true;
                CHECK(ns.poles.size() == 6);
                CHECK(!ns.poles.empty() && ns.poles.front().pole == "ST0102");
                CHECK(!ns.poles.empty() && ns.poles.back().pole == "ST0107");
                CHECK(calc.result && std::fabs(calc.result->an - 315.2) < 1e-9);
            } else if (ns.automatic->wire == Wire::TopLeft) {
                foundTop = true;
                CHECK(calc.result && std::fabs(calc.result->an - 313.8) < 1e-9);
            }
        }
        CHECK(foundPhase && foundTop);
    }

    // Rådata: 13 rader med Excelrad 19-31 och fälten på R, I, J, K, U, V, W och AG.
    auto data = projectdb::lastPoleImport(p);
    CHECK(data.ok() && data.value().has_value());
    if (data.ok() && data.value()) {
        const poletable::ImportRecord& r = *data.value();
        CHECK(r.rows.size() == 13);
        for (std::size_t i = 0; i < r.rows.size(); ++i) {
            CHECK(r.rows[i].excelRow == 19 + static_cast<int>(i));
            CHECK(r.rows[i].pole == rows[i].pole);
        }
        CHECK(r.fileName == "Stolptabell prov.xlsx");
        CHECK(r.sheetName == "Stolptabell");
        CHECK(r.version == "Ver. 6");
        CHECK(r.headerFirstRow == 11 && r.headerLastRow == 15 && r.firstDataRow == 19);
        CHECK(r.fromPole == "ST0098" && r.toPole == "ST0110");
        CHECK(letterOf(r, poletable::kPole) == "R");
        CHECK(letterOf(r, poletable::kNorth) == "I");
        CHECK(letterOf(r, poletable::kEast) == "J");
        CHECK(letterOf(r, poletable::kHeight) == "K");
        CHECK(letterOf(r, poletable::kSpan) == "U");
        CHECK(letterOf(r, poletable::kAh) == "V");
        CHECK(letterOf(r, poletable::kAv) == "W");
        CHECK(letterOf(r, poletable::kLkl) == "AG");
        CHECK(!r.importedUtc.empty());
    }

    // En andra fil utan ST0107: faslinan tappar sin slutstolpe, och bara raderna byts.
    std::vector<poletable::PoleRow> rows2 = rows;
    for (std::size_t i = 0; i < rows2.size(); ++i)
        if (rows2[i].pole == "ST0107") {
            rows2.erase(rows2.begin() + static_cast<std::ptrdiff_t>(i));
            break;
        }
    CHECK(rows2.size() == 12);
    auto book2 = xlsx::readWorkbook(testxlsx::fromSheets({testfixture::bankenV6(rows2)}), "Stolptabell utan ST0107.xlsx");
    CHECK(book2.ok());
    if (!book2.ok())
        return;
    const Imported second = readAll(book2.value(), "Stolptabell utan ST0107.xlsx");
    CHECK(second.rows == rows2);

    const std::vector<automode::NamedRange> named = {
        {projectdb::autoLineWord(Wire::Phase, true), autoBefore.value().ranges[0]},
        {projectdb::autoLineWord(Wire::TopLeft, true), autoBefore.value().ranges[1]}};
    const std::vector<std::string> lost = poleimport::lostRanges(named, rows, second.rows);
    CHECK(lost.size() == 1);
    if (lost.size() == 1)
        CHECK(lost[0] == "Faslinan: Stolpnr ST0107 finns inte i filen.");

    CHECK(projectdb::importPolesBefore(p, second.rows, second.record).ok());
    auto before2 = projectdb::poles(p, Stage::Before);
    CHECK(before2.ok() && before2.value() == rows2);
    auto autoAfter = projectdb::autoSettings(p, Stage::Before);
    CHECK(autoAfter.ok() && autoAfter.value() == autoBefore.value());
    CHECK(query(p, "SELECT issue FROM auto_calc WHERE stage = 'before' AND wire = 'phase'") ==
          "Stolpnr ST0107 finns inte i Stolptabell före.");
    CHECK(snapshotOf(p) == snapshot);

    auto data2 = projectdb::lastPoleImport(p);
    CHECK(data2.ok() && data2.value().has_value());
    if (data2.ok() && data2.value()) {
        CHECK(data2.value()->rows.size() == 12);
        CHECK(data2.value()->fileName == "Stolptabell utan ST0107.xlsx");
        CHECK(data2.value()->toPole == "ST0110");
    }
    CHECK(query(p, "SELECT count(*) FROM pole_import_row") == "12");
    CHECK(query(p, "SELECT count(*) FROM pole_import_row WHERE pole = 'ST0107'") == "0");
}

} // namespace

int main()
{
    const std::string dir = platform::pathToUtf8(
        fs::temp_directory_path() /
        platform::pathFromUtf8("kalkyl-test-poleimport-flow åäö " + std::to_string(_getpid())));
    std::error_code ec;
    fs::remove_all(platform::pathFromUtf8(dir), ec);
    CHECK(platform::ensureDirectory(dir).ok());

    testFlow(dir);

    fs::remove_all(platform::pathFromUtf8(dir), ec);
    if (g_failures == 0)
        std::cout << "poleimport_flow: alla kontroller lyckades" << std::endl;
    else
        std::cerr << "poleimport_flow: " << g_failures << " kontroller misslyckades" << std::endl;
    return g_failures == 0 ? 0 : 1;
}
