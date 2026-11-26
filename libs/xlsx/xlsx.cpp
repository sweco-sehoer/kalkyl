#include "xlsx/xlsx.h"

#include <algorithm>
#include <charconv>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <map>

#include "miniz.h"
#include "platform/files.h"
#include "xlsx/xml_reader.h"

namespace kalkyl::xlsx {

namespace detail {

// Gränsen för en uppackad del. Är en variabel så att testet kan prova felet utan en 256 MB stor del.
std::uint64_t g_partLimit = kMaxPartBytes;
void setPartLimitForTest(std::uint64_t limit) { g_partLimit = limit; }

} // namespace detail

namespace {

// Ett fel av slaget Invalid. message är texten i planen 9.1 och detail säger vilken del som är fel.
core::Error invalid(std::string message, std::string detail = {})
{
    core::Error e;
    e.code = core::ErrorCode::Invalid;
    e.message = std::move(message);
    e.detail = std::move(detail);
    return e;
}

// Texterna i planen 9.1 för en läsning ur minnet. name är filens namn.
std::string notExcelText(std::string_view name) { return std::string(name) + " är inte en Excelfil (.xlsx eller .xlsm)."; }
std::string oleText(std::string_view name)
{
    return std::string(name) +
           " är en äldre Excelfil (.xls) eller lösenordsskyddad. Spara den som .xlsx i Excel utan lösenord och importera den.";
}
std::string damagedText(std::string_view name)
{
    return std::string(name) + " är skadad och kan inte läsas som en Excelfil.";
}

// Gemener för ASCII, till jämförelser av delnamn och typer.
std::string lower(std::string_view s)
{
    std::string out(s);
    for (char& ch : out)
        if (ch >= 'A' && ch <= 'Z')
            ch = static_cast<char>(ch - 'A' + 'a');
    return out;
}

// Texten utan blanktecken (blanksteg, tabb, CR och LF) i början och slutet.
std::string_view trimBlank(std::string_view s)
{
    auto blank = [](char ch) { return ch == ' ' || ch == char{9} || ch == char{10} || ch == char{13}; };
    while (!s.empty() && blank(s.front()))
        s.remove_prefix(1);
    while (!s.empty() && blank(s.back()))
        s.remove_suffix(1);
    return s;
}

// Sant när s slutar med tail, till exempel en relations Type som slutar på "/worksheet".
// Jämförelsen skiljer på stora och små bokstäver.
bool endsWith(std::string_view s, std::string_view tail)
{
    return s.size() >= tail.size() && s.substr(s.size() - tail.size()) == tail;
}

// Heltalet i texten (bara siffror), eller -1.
long parseInt(std::string_view s)
{
    s = trimBlank(s);
    if (s.empty() || s.size() > 9)
        return -1;
    long v = 0;
    for (char ch : s) {
        if (ch < '0' || ch > '9')
            return -1;
        v = v * 10 + (ch - '0');
    }
    return v;
}

// Löser upp target mot mappen baseDir ("xl/", eller "" för roten). Ett target med snedstreck först
// räknas från roten, och "." och ".." löses upp. Svaret har inget snedstreck först.
std::string resolveTarget(std::string_view baseDir, std::string_view target)
{
    std::string full;
    if (!target.empty() && target.front() == '/')
        full = std::string(target.substr(1));
    else
        full = std::string(baseDir) + std::string(target);
    std::vector<std::string> parts;
    size_t start = 0;
    while (start <= full.size()) {
        size_t slash = full.find('/', start);
        if (slash == std::string::npos)
            slash = full.size();
        const std::string seg = full.substr(start, slash - start);
        if (seg == "..") {
            if (!parts.empty())
                parts.pop_back();
        } else if (!seg.empty() && seg != ".") {
            parts.push_back(seg);
        }
        start = slash + 1;
    }
    std::string out;
    for (size_t i = 0; i < parts.size(); ++i)
        out += (i ? "/" : "") + parts[i];
    return out;
}

// Mappen i en delsökväg med snedstreck sist ("xl/workbook.xml" ger "xl/"), "" för roten.
std::string dirOf(std::string_view path)
{
    const size_t slash = path.rfind('/');
    return slash == std::string_view::npos ? std::string() : std::string(path.substr(0, slash + 1));
}

// Zipfilen i minnet och dess delar. Delnamn jämförs utan skillnad på stora och små bokstäver.
class ZipFile {
public:
    ZipFile() = default;
    ZipFile(const ZipFile&) = delete;
    ZipFile& operator=(const ZipFile&) = delete;
    ~ZipFile()
    {
        if (open_)
            mz_zip_reader_end(&zip_);
    }

    // Öppnar zipen och listar delarna. false när zipen är skadad eller avkortad.
    bool open(std::string_view bytes)
    {
        if (!mz_zip_reader_init_mem(&zip_, bytes.data(), bytes.size(), 0))
            return false;
        open_ = true;
        const mz_uint n = mz_zip_reader_get_num_files(&zip_);
        for (mz_uint i = 0; i < n; ++i) {
            mz_zip_archive_file_stat st;
            if (!mz_zip_reader_file_stat(&zip_, i, &st))
                return false;
            if (!mz_zip_reader_is_file_a_directory(&zip_, i))
                index_.emplace(lower(st.m_filename), i);
        }
        return true;
    }

    bool has(const std::string& name) const { return index_.count(lower(name)) != 0; }

    // Läser delen. Ger false när delen saknas, och sätter tooBig när den är för stor uppackad eller
    // fail när den inte går att packa upp.
    enum class Status { Ok, Missing, TooBig, Damaged };
    Status read(const std::string& name, std::string& out) const
    {
        const auto it = index_.find(lower(name));
        if (it == index_.end())
            return Status::Missing;
        mz_zip_archive_file_stat st;
        if (!mz_zip_reader_file_stat(&zip_, it->second, &st))
            return Status::Damaged;
        if (st.m_uncomp_size > detail::g_partLimit)
            return Status::TooBig;
        size_t size = 0;
        void* heap = mz_zip_reader_extract_to_heap(&zip_, it->second, &size, 0);
        if (!heap)
            return Status::Damaged;
        out.assign(static_cast<const char*>(heap), size);
        mz_free(heap);
        return Status::Ok;
    }

private:
    mutable mz_zip_archive zip_{};
    bool open_ = false;
    std::map<std::string, mz_uint> index_;
};

// En relation ur en .rels-del.
struct Relation {
    std::string id;
    std::string type;     // hela Type-adressen, i gemener
    std::string target;   // som den står
    bool external = false;
};

// Läser Relationship-elementen. false vid skadad XML.
bool parseRelations(std::string_view xml, std::vector<Relation>& out)
{
    XmlReader r(xml);
    for (;;) {
        const XmlReader::Kind k = r.next();
        if (k == XmlReader::Kind::Error)
            return false;
        if (k == XmlReader::Kind::Done)
            return true;
        if (k == XmlReader::Kind::Start && r.name() == "Relationship") {
            Relation rel;
            rel.id = r.attribute("Id");
            rel.type = lower(r.attribute("Type"));
            rel.target = r.attribute("Target");
            rel.external = lower(r.attribute("TargetMode")) == "external";
            out.push_back(std::move(rel));
        }
    }
}

// Läser texten i ett si- eller is-element (anropas direkt efter dess Start): texten i t direkt under
// elementet och under varje r, ihopfogad. Fonetiska körningar hoppas över. false vid skadad XML.
bool readRichText(XmlReader& xml, std::string& out)
{
    const int target = xml.depth() - 1;
    std::string raw;
    for (;;) {
        const XmlReader::Kind k = xml.next();
        if (k == XmlReader::Kind::Error || k == XmlReader::Kind::Done)
            return false;
        if (k == XmlReader::Kind::Start) {
            const std::string& name = xml.name();
            if (name == "rPh" || name == "phoneticPr") {
                if (!xml.skipElement())
                    return false;
            } else if (name == "t") {
                for (;;) {
                    const XmlReader::Kind k2 = xml.next();
                    if (k2 == XmlReader::Kind::Text)
                        raw += xml.text();
                    else if (k2 == XmlReader::Kind::End)
                        break;
                    else
                        return false;
                }
            }
        } else if (k == XmlReader::Kind::End && xml.depth() == target) {
            break;
        }
    }
    out = decodeOoxmlText(raw);
    return true;
}

// De delade strängarna i ordning. false vid skadad XML.
bool parseSharedStrings(std::string_view xml, std::vector<std::string>& out)
{
    XmlReader r(xml);
    for (;;) {
        const XmlReader::Kind k = r.next();
        if (k == XmlReader::Kind::Error)
            return false;
        if (k == XmlReader::Kind::Done)
            return true;
        if (k == XmlReader::Kind::Start && r.name() == "si") {
            std::string text;
            if (!readRichText(r, text))
                return false;
            out.push_back(std::move(text));
        }
    }
}

// Decimalerna för ett inbyggt talformat-id: 1 och 3 ger 0, 2 och 4 ger 2, övriga -1.
int builtinDecimals(int id)
{
    if (id == 1 || id == 3)
        return 0;
    if (id == 2 || id == 4)
        return 2;
    return -1;
}

// Decimalerna per formatindex (cellens s) ur styles.xml. false vid skadad XML.
bool parseStyles(std::string_view xml, std::vector<int>& xfDecimals)
{
    XmlReader r(xml);
    std::map<int, int> custom;
    bool inCellXfs = false;
    for (;;) {
        const XmlReader::Kind k = r.next();
        if (k == XmlReader::Kind::Error)
            return false;
        if (k == XmlReader::Kind::Done)
            return true;
        if (k == XmlReader::Kind::Start) {
            if (r.name() == "numFmt") {
                const long id = parseInt(r.attribute("numFmtId"));
                if (id >= 0)
                    custom[static_cast<int>(id)] = formatDecimals(r.attribute("formatCode"));
            } else if (r.name() == "cellXfs") {
                inCellXfs = true;
            } else if (r.name() == "xf" && inCellXfs) {
                const long id = parseInt(r.attribute("numFmtId"));
                int decimals = -1;
                if (id >= 0) {
                    const auto it = custom.find(static_cast<int>(id));
                    decimals = it != custom.end() ? it->second : builtinDecimals(static_cast<int>(id));
                }
                xfDecimals.push_back(decimals);
            }
        } else if (k == XmlReader::Kind::End && r.name() == "cellXfs") {
            inCellXfs = false;
        }
    }
}

// Läser ett blad: raderna, cellerna och sammanslagningarna. shared och xfDecimals kan vara tomma.
// false vid skadad XML eller ett delat index som saknas.
bool parseSheet(std::string_view xml, const std::vector<std::string>& shared, const std::vector<int>& xfDecimals,
                Sheet& sheet)
{
    XmlReader r(xml);
    int currentRow = 0;
    int nextColumn = 1;
    for (;;) {
        const XmlReader::Kind k = r.next();
        if (k == XmlReader::Kind::Error)
            return false;
        if (k == XmlReader::Kind::Done)
            break;
        if (k != XmlReader::Kind::Start)
            continue;
        if (r.name() == "row") {
            const long row = parseInt(r.attribute("r"));
            currentRow = row > 0 ? static_cast<int>(row) : currentRow + 1;
            nextColumn = 1;
        } else if (r.name() == "mergeCell") {
            const std::string ref = r.attribute("ref");
            const size_t colon = ref.find(':');
            if (colon != std::string::npos) {
                const auto a = parseCellRef(std::string_view(ref).substr(0, colon));
                const auto b = parseCellRef(std::string_view(ref).substr(colon + 1));
                if (a && b && *a != *b)
                    sheet.merges.push_back({std::min(a->first, b->first), std::min(a->second, b->second),
                                            std::max(a->first, b->first), std::max(a->second, b->second)});
            }
        } else if (r.name() == "c") {
            Cell cell;
            cell.row = currentRow;
            cell.column = nextColumn;
            if (const auto ref = parseCellRef(r.attribute("r"))) {
                cell.row = ref->first;
                cell.column = ref->second;
            }
            nextColumn = cell.column + 1;
            const std::string type = r.attribute("t");
            const long style = parseInt(r.attribute("s"));
            // Innehållet i cellen: v, is och f (formeln läses inte).
            std::string value;
            std::string inlineText;
            const int target = r.depth() - 1;
            for (;;) {
                const XmlReader::Kind k2 = r.next();
                if (k2 == XmlReader::Kind::Error || k2 == XmlReader::Kind::Done)
                    return false;
                if (k2 == XmlReader::Kind::End && r.depth() == target)
                    break;
                if (k2 != XmlReader::Kind::Start)
                    continue;
                if (r.name() == "v") {
                    for (;;) {
                        const XmlReader::Kind k3 = r.next();
                        if (k3 == XmlReader::Kind::Text)
                            value += r.text();
                        else if (k3 == XmlReader::Kind::End)
                            break;
                        else
                            return false;
                    }
                } else if (r.name() == "is") {
                    if (!readRichText(r, inlineText))
                        return false;
                } else if (!r.skipElement()) {
                    return false;
                }
            }
            if (type == "s") {
                const long index = parseInt(value);
                if (value.empty() || (trimBlank(value).empty()))
                    continue;
                if (index < 0 || static_cast<size_t>(index) >= shared.size())
                    return false;
                cell.kind = CellKind::Text;
                cell.text = shared[static_cast<size_t>(index)];
            } else if (type == "inlineStr") {
                cell.kind = CellKind::Text;
                cell.text = inlineText;
            } else if (type == "str" || type == "d") {
                cell.kind = CellKind::Text;
                cell.text = decodeOoxmlText(value);
            } else if (type == "b") {
                cell.kind = CellKind::Bool;
                const std::string_view v = trimBlank(value);
                cell.text = (v == "1" || v == "true") ? "1" : (v.empty() ? "" : "0");
            } else if (type == "e") {
                cell.kind = CellKind::Error;
                cell.text = std::string(trimBlank(value));
            } else {
                cell.kind = CellKind::Number;
                cell.text = std::string(trimBlank(value));
                if (style >= 0 && static_cast<size_t>(style) < xfDecimals.size())
                    cell.decimals = xfDecimals[static_cast<size_t>(style)];
            }
            if (cell.text.empty())
                continue;
            sheet.cells.push_back(std::move(cell));
        }
    }
    std::stable_sort(sheet.cells.begin(), sheet.cells.end(), [](const Cell& a, const Cell& b) {
        return a.row != b.row ? a.row < b.row : a.column < b.column;
    });
    for (const Cell& c : sheet.cells) {
        sheet.lastRow = std::max(sheet.lastRow, c.row);
        sheet.lastColumn = std::max(sheet.lastColumn, c.column);
    }
    return true;
}

// Ett blad i workbook.xml: namnet, om det är dolt och relationens id.
struct SheetEntry {
    std::string name;
    bool hidden = false;
    std::string relId;
};

// Läser sheet-elementen i workbook.xml. false vid skadad XML.
bool parseWorkbookXml(std::string_view xml, std::vector<SheetEntry>& out)
{
    XmlReader r(xml);
    for (;;) {
        const XmlReader::Kind k = r.next();
        if (k == XmlReader::Kind::Error)
            return false;
        if (k == XmlReader::Kind::Done)
            return true;
        if (k == XmlReader::Kind::Start && r.name() == "sheet") {
            SheetEntry e;
            e.name = r.attribute("name");
            const std::string state = r.attribute("state");
            e.hidden = state == "hidden" || state == "veryHidden";
            e.relId = r.attribute("id");
            out.push_back(std::move(e));
        }
    }
}

// Avrundar absolutvärdet till 15 värdesiffror och ger siffrorna, och exponenten som antalet
// heltalssiffror: värdet är 0.SIFFROR × 10^intLen. Noll ger siffran "0" och intLen 1.
void significantDigits(double absValue, std::string& digits, int& intLen)
{
    if (absValue == 0) {
        digits = "0";
        intLen = 1;
        return;
    }
    char buf[64];
    std::snprintf(buf, sizeof buf, "%.14e", absValue);
    const std::string s(buf);
    const size_t e = s.find('e');
    digits = s.substr(0, 1) + s.substr(2, e - 2);
    intLen = std::atoi(s.c_str() + e + 1) + 1;
}

} // namespace

// Binärsökning efter cellen i cells, som är sorterad på rad och kolumn.
const Cell* Sheet::find(int row, int column) const
{
    const auto it = std::lower_bound(cells.begin(), cells.end(), std::pair{row, column},
                                     [](const Cell& c, const std::pair<int, int>& key) {
                                         return c.row != key.first ? c.row < key.first : c.column < key.second;
                                     });
    return it != cells.end() && it->row == row && it->column == column ? &*it : nullptr;
}

// Kolumnens bokstäver: 1 = A, 26 = Z, 27 = AA. Utanför 1-16384 ger tom text.
std::string columnLetters(int column)
{
    if (column < 1 || column > 16384)
        return {};
    std::string out;
    for (int c = column; c > 0; c = (c - 1) / 26)
        out.insert(out.begin(), static_cast<char>('A' + (c - 1) % 26));
    return out;
}

// En cellreferens: valfria dollartecken, bokstäver och sedan siffror. Rad och kolumn ska vara giltiga.
std::optional<std::pair<int, int>> parseCellRef(std::string_view ref)
{
    size_t i = 0;
    if (i < ref.size() && ref[i] == '$')
        ++i;
    long column = 0;
    const size_t letterStart = i;
    while (i < ref.size() && ((ref[i] >= 'A' && ref[i] <= 'Z') || (ref[i] >= 'a' && ref[i] <= 'z'))) {
        column = column * 26 + ((ref[i] | 0x20) - 'a' + 1);
        if (column > 16384)
            return std::nullopt;
        ++i;
    }
    if (i == letterStart)
        return std::nullopt;
    if (i < ref.size() && ref[i] == '$')
        ++i;
    const size_t digitStart = i;
    long row = 0;
    while (i < ref.size() && ref[i] >= '0' && ref[i] <= '9') {
        row = row * 10 + (ref[i] - '0');
        if (row > 1048576)
            return std::nullopt;
        ++i;
    }
    if (i == digitStart || i != ref.size() || row < 1)
        return std::nullopt;
    return std::pair<int, int>{static_cast<int>(row), static_cast<int>(column)};
}

// Decimalerna i talformatets första avsnitt. Text i citattecken, tecken efter bakvänt snedstreck,
// hakparenteser och utfyllnad (_ och *) tas bort först. Procent, exponent, datum, tid, text och
// Allmänt ger -1, och annars räknas nollorna direkt efter punkten.
int formatDecimals(std::string_view formatCode)
{
    std::string kept;
    for (size_t i = 0; i < formatCode.size(); ++i) {
        const char ch = formatCode[i];
        if (ch == ';')
            break;
        if (ch == char{34}) {
            const size_t close = formatCode.find(char{34}, i + 1);
            if (close == std::string_view::npos)
                break;
            i = close;
        } else if (ch == char{92} || ch == '_' || ch == '*') {
            ++i;
        } else if (ch == '[') {
            const size_t close = formatCode.find(']', i + 1);
            if (close == std::string_view::npos)
                break;
            i = close;
        } else {
            kept += ch;
        }
    }
    const std::string low = lower(kept);
    if (low.find("general") != std::string::npos)
        return -1;
    for (char ch : low)
        if (ch == '%' || ch == 'e' || ch == '@' || ch == '/' || ch == 'y' || ch == 'm' || ch == 'd' || ch == 'h' ||
            ch == 's')
            return -1;
    if (kept.find_first_of("0#?") == std::string::npos)
        return -1;
    const size_t dot = kept.find('.');
    if (dot == std::string::npos)
        return 0;
    int zeros = 0;
    for (size_t i = dot + 1; i < kept.size() && kept[i] == '0'; ++i)
        ++zeros;
    return zeros;
}

// Talet som Excel visar det. Först avrundas det till 15 värdesiffror (som Excel), sedan skrivs det
// med decimals decimaler (avrundat uppåt vid 5) eller, med -1, i kortaste form. Aldrig exponent.
std::optional<std::string> numberText(std::string_view text, int decimals)
{
    text = trimBlank(text);
    if (text.empty())
        return std::nullopt;
    for (char ch : text)
        if (!((ch >= '0' && ch <= '9') || ch == '.' || ch == 'e' || ch == 'E' || ch == '+' || ch == '-'))
            return std::nullopt;
    if (text.front() == '+')
        text.remove_prefix(1);
    double value = 0;
    const auto res = std::from_chars(text.data(), text.data() + text.size(), value);
    if (res.ec != std::errc() || res.ptr != text.data() + text.size() || !std::isfinite(value))
        return std::nullopt;
    std::string digits;
    int intLen = 0;
    significantDigits(std::fabs(value), digits, intLen);
    // Heltals- och bråkdelens siffror.
    std::string whole;
    std::string frac;
    if (intLen <= 0) {
        whole = "0";
        frac = std::string(static_cast<size_t>(-intLen), '0') + digits;
    } else if (static_cast<size_t>(intLen) >= digits.size()) {
        whole = digits + std::string(static_cast<size_t>(intLen) - digits.size(), '0');
    } else {
        whole = digits.substr(0, static_cast<size_t>(intLen));
        frac = digits.substr(static_cast<size_t>(intLen));
    }
    if (decimals < 0) {
        while (!frac.empty() && frac.back() == '0')
            frac.pop_back();
    } else {
        const size_t want = static_cast<size_t>(decimals);
        if (frac.size() > want) {
            const bool up = frac[want] >= '5';
            frac.resize(want);
            if (up) {
                std::string all = whole + frac;
                size_t i = all.size();
                while (i > 0) {
                    --i;
                    if (all[i] == '9') {
                        all[i] = '0';
                    } else {
                        ++all[i];
                        break;
                    }
                    if (i == 0)
                        all.insert(all.begin(), '1');
                }
                whole = all.substr(0, all.size() - want);
                frac = all.substr(all.size() - want);
            }
        } else {
            frac.append(want - frac.size(), '0');
        }
    }
    std::string out = whole;
    if (!frac.empty())
        out += "." + frac;
    const bool zero = out.find_first_not_of("0.") == std::string::npos;
    if (value < 0 && !zero)
        out.insert(out.begin(), '-');
    return out;
}

// Läser arbetsboken ur minnet: filtypen, zipen, huvuddelen och relationerna, de delade strängarna,
// formaten och sedan varje kalkylblad i bokens ordning. Alla fel är Invalid med texterna i planen.
core::Result<Workbook> readWorkbook(std::string_view bytes, std::string_view fileName)
{
    static constexpr unsigned char kOle[] = {0xD0, 0xCF, 0x11, 0xE0, 0xA1, 0xB1, 0x1A, 0xE1};
    auto startsWith = [&](const unsigned char* sig, size_t n) {
        return bytes.size() >= n && std::equal(sig, sig + n, reinterpret_cast<const unsigned char*>(bytes.data()));
    };
    static constexpr unsigned char kZipLocal[] = {'P', 'K', 3, 4};
    static constexpr unsigned char kZipEmpty[] = {'P', 'K', 5, 6};
    if (startsWith(kOle, sizeof kOle))
        return invalid(oleText(fileName));
    if (!startsWith(kZipLocal, 4) && !startsWith(kZipEmpty, 4))
        return invalid(notExcelText(fileName));

    ZipFile zip;
    if (!zip.open(bytes))
        return invalid(damagedText(fileName), "zipens centrala katalog går inte att läsa");

    // Läser en del med felen översatta till planens texter. Saknas den ger det Missing.
    auto readPart = [&](const std::string& name, std::string& out, ZipFile::Status& status) -> std::optional<core::Error> {
        status = zip.read(name, out);
        if (status == ZipFile::Status::TooBig)
            return invalid(std::string(fileName) + " har en del som är för stor uppackad (mer än 256 MB).", name);
        if (status == ZipFile::Status::Damaged)
            return invalid(damagedText(fileName), "delen " + name + " går inte att packa upp");
        return std::nullopt;
    };
    ZipFile::Status status = ZipFile::Status::Ok;
    std::string part;

    // Huvuddelen ur _rels/.rels, annars xl/workbook.xml.
    std::string mainPath = "xl/workbook.xml";
    if (auto err = readPart("_rels/.rels", part, status))
        return *err;
    if (status == ZipFile::Status::Ok) {
        std::vector<Relation> rels;
        if (!parseRelations(part, rels))
            return invalid(damagedText(fileName), "_rels/.rels är inte giltig XML");
        for (const Relation& rel : rels)
            if (!rel.external && endsWith(rel.type, "/officedocument"))
                mainPath = resolveTarget("", rel.target);
    }
    if (auto err = readPart(mainPath, part, status))
        return *err;
    if (status == ZipFile::Status::Missing)
        return invalid(notExcelText(fileName), mainPath + " saknas");
    std::vector<SheetEntry> entries;
    if (!parseWorkbookXml(part, entries))
        return invalid(damagedText(fileName), mainPath + " är inte giltig XML");

    // Arbetsbokens relationer: bladen, de delade strängarna och formaten.
    const std::string baseDir = dirOf(mainPath);
    const std::string relsPath = baseDir + "_rels/" + mainPath.substr(baseDir.size()) + ".rels";
    std::vector<Relation> bookRels;
    if (auto err = readPart(relsPath, part, status))
        return *err;
    if (status == ZipFile::Status::Ok && !parseRelations(part, bookRels))
        return invalid(damagedText(fileName), relsPath + " är inte giltig XML");

    std::vector<std::string> shared;
    std::vector<int> xfDecimals;
    for (const Relation& rel : bookRels) {
        if (rel.external)
            continue;
        const bool isShared = endsWith(rel.type, "/sharedstrings");
        const bool isStyles = endsWith(rel.type, "/styles");
        if (!isShared && !isStyles)
            continue;
        const std::string path = resolveTarget(baseDir, rel.target);
        if (auto err = readPart(path, part, status))
            return *err;
        if (status != ZipFile::Status::Ok)
            continue;
        if (isShared && !parseSharedStrings(part, shared))
            return invalid(damagedText(fileName), path + " är inte giltig XML");
        if (isStyles && !parseStyles(part, xfDecimals))
            return invalid(damagedText(fileName), path + " är inte giltig XML");
    }

    Workbook book;
    for (const SheetEntry& entry : entries) {
        const Relation* found = nullptr;
        for (const Relation& rel : bookRels)
            if (rel.id == entry.relId && !rel.external)
                found = &rel;
        // Diagramblad, makroblad och dialogblad är inga kalkylblad.
        if (!found || !endsWith(found->type, "/worksheet"))
            continue;
        const std::string path = resolveTarget(baseDir, found->target);
        if (auto err = readPart(path, part, status))
            return *err;
        if (status == ZipFile::Status::Missing)
            return invalid(damagedText(fileName), path + " saknas i zipen");
        Sheet sheet;
        sheet.name = entry.name;
        sheet.hidden = entry.hidden;
        if (!parseSheet(part, shared, xfDecimals, sheet))
            return invalid(damagedText(fileName), path + " är inte giltig XML eller har ett delat index som saknas");
        book.sheets.push_back(std::move(sheet));
    }
    if (book.sheets.empty())
        return invalid(std::string(fileName) + " har inga kalkylblad.");
    return book;
}

// Läser filen med delad åtkomst och sedan arbetsboken. Läsfelen får planens texter med filens namn.
core::Result<Workbook> openWorkbook(const std::string& utf8Path)
{
    const std::string name = platform::fileName(utf8Path);
    auto data = platform::readFileShared(utf8Path, kMaxFileBytes);
    if (!data.ok()) {
        core::Error e = data.error();
        switch (e.code) {
        case core::ErrorCode::NotFound:
            e.message = "Filen " + name + " finns inte.";
            break;
        case core::ErrorCode::Busy:
            e.message = name + " används av ett annat program och kan inte läsas just nu. Stäng filen där, "
                               "eller spara en kopia och importera den.";
            break;
        case core::ErrorCode::AccessDenied:
            e.message = "Du har inte behörighet att läsa " + name + ".";
            break;
        case core::ErrorCode::Unreachable:
            e.message = "Mappen med " + name + " svarar inte. Kontrollera nätverket och försök igen.";
            break;
        case core::ErrorCode::Invalid:
            e.message = name + " är för stor för att importeras (mer än 64 MB).";
            break;
        default:
            e.code = core::ErrorCode::Io;
            e.message = name + " kunde inte läsas.";
            break;
        }
        return e;
    }
    return readWorkbook(data.value(), name);
}

} // namespace kalkyl::xlsx
