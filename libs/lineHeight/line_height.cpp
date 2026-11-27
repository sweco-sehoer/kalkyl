#include "lineHeight/line_height.h"

#include <algorithm>
#include <cstddef>
#include <set>
#include <string>

#include "poleView/pole_view.h"
#include "tableText/table_text.h"

namespace kalkyl::lineheight {

namespace {

namespace pt = kalkyl::poletable;

// En kanonisk decimaltext delad i tecken, heltalssiffror och decimaler.
struct Decimal {
    bool negative = false;
    std::string whole;
    std::string frac;
};

// Delar en kanonisk decimaltext ("-12.33") i delarna. Tom heltalsdel blir "0".
Decimal parseDecimalText(std::string_view text) {
    Decimal d;
    if (!text.empty() && text.front() == '-') {
        d.negative = true;
        text.remove_prefix(1);
    }
    const std::size_t dot = text.find('.');
    d.whole = std::string(text.substr(0, dot));
    if (dot != std::string_view::npos)
        d.frac = std::string(text.substr(dot + 1));
    if (d.whole.empty())
        d.whole = "0";
    return d;
}

// Lägger ihop två siffersträngar av samma längd. En ny första siffra läggs till vid minnessiffra.
std::string addDigits(const std::string& x, const std::string& y) {
    std::string out(x.size(), '0');
    int carry = 0;
    for (std::size_t i = x.size(); i-- > 0;) {
        const int sum = (x[i] - '0') + (y[i] - '0') + carry;
        out[i] = static_cast<char>('0' + sum % 10);
        carry = sum / 10;
    }
    if (carry > 0)
        out.insert(out.begin(), static_cast<char>('0' + carry));
    return out;
}

// Multiplicerar en siffersträng med 5.
std::string timesFive(const std::string& x) {
    std::string out(x.size(), '0');
    int carry = 0;
    for (std::size_t i = x.size(); i-- > 0;) {
        const int product = (x[i] - '0') * 5 + carry;
        out[i] = static_cast<char>('0' + product % 10);
        carry = product / 10;
    }
    if (carry > 0)
        out.insert(0, std::to_string(carry));
    return out;
}

// Ökar en siffersträng med ett.
std::string incrementDigits(std::string x) {
    for (std::size_t i = x.size(); i-- > 0;) {
        if (x[i] == '9') {
            x[i] = '0';
        } else {
            ++x[i];
            return x;
        }
    }
    x.insert(x.begin(), '1');
    return x;
}

// Tar bort inledande nollor men låter en nolla stå kvar.
std::string stripLeadingZeros(const std::string& digits) {
    const std::size_t first = digits.find_first_not_of('0');
    return first == std::string::npos ? std::string("0") : digits.substr(first);
}

// Texten med första bokstaven versal när namnet är "rad {n}", så att den kan börja en mening.
// Ett Stolpnr visas som det står.
std::string sentenceName(const std::string& label) {
    if (label.rfind("rad ", 0) == 0)
        return "R" + label.substr(1);
    return label;
}

// Talet som kanonisk text visat med decimalkomma, utan gruppering.
std::string commaText(std::string_view canonical) {
    return tabletext::formatDecimalText(canonical, ',', false);
}

// Delens namn i tooltipen.
const char* partName(Part part) {
    switch (part) {
    case Part::Outer:
        return "ytterfaserna";
    case Part::Middle:
        return "mittfasen";
    case Part::Top:
        return "topplinan";
    }
    return "";
}

// Värdet i spannet för delen när spannet går att skriva i, annars tom text.
std::string spanValue(const SpanRef& span, Part part) {
    return span.exists && span.usable ? heightOf(span.heights, part) : std::string();
}

// Närmaste rad ovanför respektive nedanför varje rad som inte är helt tom, eller -1.
struct Neighbours {
    std::vector<int> back;
    std::vector<int> ahead;
};
Neighbours neighboursOf(const std::vector<pt::PoleRow>& rows) {
    const std::size_t n = rows.size();
    Neighbours nb;
    nb.back.assign(n, -1);
    nb.ahead.assign(n, -1);
    int last = -1;
    for (std::size_t i = 0; i < n; ++i) {
        nb.back[i] = last;
        if (!pt::isEmptyRow(rows[i]))
            last = static_cast<int>(i);
    }
    last = -1;
    for (std::size_t i = n; i-- > 0;) {
        nb.ahead[i] = last;
        if (!pt::isEmptyRow(rows[i]))
            last = static_cast<int>(i);
    }
    return nb;
}

// Bygger spannet från raden m till grannen n (back: grannen ligger ovanför, annars nedanför).
SpanRef makeSpan(const std::vector<pt::PoleRow>& after, const StoredHeights& stored, int m, int n, bool back,
                 const std::string& label) {
    SpanRef span;
    if (n < 0) {
        span.issue = sentenceName(label) + (back ? " är den första stolpen i Stolptabell efter och har inget spann bakåt."
                                                  : " är den sista stolpen i Stolptabell efter och har inget spann framåt.");
        return span;
    }
    span.exists = true;
    span.neighbourRow = n;
    const std::string other = poleview::rowLabel(after, n);
    span.label = back ? other + "-" + label : label + "-" + other;
    const std::int64_t idM = after[static_cast<std::size_t>(m)].id;
    const std::int64_t idN = after[static_cast<std::size_t>(n)].id;
    span.key = spanKey(idM, idN);
    span.usable = idM > 0 && idN > 0 && idM != idN;
    if (span.usable) {
        const auto it = stored.spans.find(span.key);
        if (it != stored.spans.end())
            span.heights = it->second;
    } else {
        span.issue = "Spannet " + span.label +
                     " hör till en rad som inte är sparad i projektet än, så linhöjden kan inte skrivas.";
    }
    return span;
}

// Namnet på spannets granne ur etiketten och blockets namn.
std::string neighbourName(const SpanRef& span, const std::string& label, bool back) {
    if (back && span.label.size() > label.size() + 1)
        return span.label.substr(0, span.label.size() - label.size() - 1);
    if (!back && span.label.size() > label.size() + 1)
        return span.label.substr(label.size() + 1);
    return span.label;
}

} // namespace

// Ordnar radernas id så att a är det lägre.
SpanKey spanKey(std::int64_t x, std::int64_t y) {
    return x <= y ? SpanKey{x, y} : SpanKey{y, x};
}

// En nyckel är giltig när 0 < a < b.
bool isValidKey(const SpanKey& key) {
    return key.a > 0 && key.a < key.b;
}

// Delens linhöjd i spannet.
const std::string& heightOf(const SpanHeights& heights, Part part) {
    switch (part) {
    case Part::Outer:
        return heights.outer;
    case Part::Middle:
        return heights.middle;
    case Part::Top:
        return heights.top;
    }
    return heights.outer;
}

// Delens linhöjd i spannet, skrivbar.
std::string& heightOf(SpanHeights& heights, Part part) {
    return const_cast<std::string&>(heightOf(static_cast<const SpanHeights&>(heights), part));
}

// Ett spann för varje par av grannrader som inte är helt tomma och har id. Grannarna kommer ur
// neighboursOf, samma som blocken, så att bladet och städningen i projektet aldrig skiljer sig.
std::vector<SpanKey> afterSpans(const std::vector<pt::PoleRow>& after) {
    std::vector<SpanKey> out;
    const Neighbours nb = neighboursOf(after);
    for (std::size_t i = 0; i < after.size(); ++i) {
        const int next = nb.ahead[i];
        if (pt::isEmptyRow(after[i]) || next < 0)
            continue;
        const SpanKey key = spanKey(after[i].id, after[static_cast<std::size_t>(next)].id);
        if (isValidKey(key))
            out.push_back(key);
    }
    return out;
}

// Behåller spannen som finns i afterSpans och topplinans Av för rader som finns och inte är tomma.
StoredHeights keptHeights(const StoredHeights& stored, const std::vector<pt::PoleRow>& after) {
    StoredHeights kept;
    for (const SpanKey& key : afterSpans(after)) {
        const auto it = stored.spans.find(key);
        if (it != stored.spans.end())
            kept.spans.emplace(key, it->second);
    }
    for (const pt::PoleRow& row : after) {
        if (row.id <= 0 || pt::isEmptyRow(row))
            continue;
        const auto it = stored.topAv.find(row.id);
        if (it != stored.topAv.end())
            kept.topAv.emplace(row.id, it->second);
    }
    return kept;
}

// Kanonisk text, men inga negativa linhöjder.
std::optional<std::string> acceptHeight(std::string_view text) {
    const std::optional<std::string> value = tabletext::normalizeDecimal(text);
    if (!value || (!value->empty() && value->front() == '-'))
        return std::nullopt;
    return value;
}

// Kanonisk text, också negativ.
std::optional<std::string> acceptAv(std::string_view text) {
    return tabletext::normalizeDecimal(text);
}

// Siffrorna jämkas till lika många decimaler, adderas och multipliceras med 5, och decimalpunkten
// flyttas en plats mer. Allt på strängar, så inget går via double.
std::string halfSum(std::string_view a, std::string_view b) {
    if (a.empty() || b.empty())
        return std::string();
    const Decimal x = parseDecimalText(a);
    const Decimal y = parseDecimalText(b);
    const std::size_t scale = std::max(x.frac.size(), y.frac.size());
    std::string dx = x.whole + x.frac + std::string(scale - x.frac.size(), '0');
    std::string dy = y.whole + y.frac + std::string(scale - y.frac.size(), '0');
    const std::size_t width = std::max(dx.size(), dy.size());
    dx.insert(0, width - dx.size(), '0');
    dy.insert(0, width - dy.size(), '0');
    std::string digits = timesFive(addDigits(dx, dy));
    const std::size_t decimals = scale + 1;
    if (digits.size() < decimals + 1)
        digits.insert(0, decimals + 1 - digits.size(), '0');
    const std::string whole = stripLeadingZeros(digits.substr(0, digits.size() - decimals));
    std::string frac = digits.substr(digits.size() - decimals);
    while (!frac.empty() && frac.back() == '0')
        frac.pop_back();
    return frac.empty() ? whole : whole + "." + frac;
}

// Siffrorna kapas vid decimals, ökas med ett när nästa siffra är 5 eller mer, och skrivs om med komma.
std::string roundedText(std::string_view canonical, int decimals) {
    if (canonical.empty())
        return std::string();
    decimals = std::max(decimals, 0);
    const Decimal d = parseDecimalText(canonical);
    const std::size_t want = static_cast<std::size_t>(decimals);
    std::string kept;
    if (d.frac.size() > want) {
        kept = d.whole + d.frac.substr(0, want);
        if (d.frac[want] >= '5')
            kept = incrementDigits(kept);
    } else {
        kept = d.whole + d.frac + std::string(want - d.frac.size(), '0');
    }
    if (kept.size() < want + 1)
        kept.insert(0, want + 1 - kept.size(), '0');
    std::string whole = stripLeadingZeros(kept.substr(0, kept.size() - want));
    const std::string frac = kept.substr(kept.size() - want);
    const bool zero = whole == "0" && frac.find_first_not_of('0') == std::string::npos;
    std::string out = (d.negative && !zero ? "-" : "") + whole;
    if (want > 0)
        out += "," + frac;
    return out;
}

// Båda spannen med värden ger halva summan, ett enda spann ger dess värde, annars tomt.
std::string spanMean(const SpanRef& back, const SpanRef& ahead, Part part) {
    const std::string x = spanValue(back, part);
    const std::string y = spanValue(ahead, part);
    if (back.exists && ahead.exists)
        return (x.empty() || y.empty()) ? std::string() : halfSum(x, y);
    if (back.exists)
        return x;
    if (ahead.exists)
        return y;
    return std::string();
}

// Halva summan av de två medlen.
std::string lineCalcHeight(std::string_view outerMean, std::string_view middleMean) {
    return halfSum(outerMean, middleMean);
}

// Det största av Av Fas1 och Av Fas3 som tal. Ett tomt fält ger det andra, lika ger Fas1.
int outerAvField(const pt::PoleRow& row) {
    const bool has1 = !row.av.empty();
    const bool has3 = !row.av3.empty();
    if (!has1 && !has3)
        return -1;
    if (!has3)
        return pt::kAv;
    if (!has1)
        return pt::kAv3;
    const std::optional<double> v1 = pt::numberValue(row, pt::kAv);
    const std::optional<double> v3 = pt::numberValue(row, pt::kAv3);
    if (v1 && v3 && *v3 > *v1)
        return pt::kAv3;
    return pt::kAv;
}

// Märkningen ger området, och varje rad i området som inte är tom får ett block med sina grannar.
std::vector<PoleBlock> buildBlocks(const std::vector<pt::PoleRow>& before, const std::vector<pt::PoleRow>& after,
                                   const StoredHeights& stored) {
    std::vector<PoleBlock> blocks;
    const std::vector<bool> marks = poleview::filterMarks(before, after);
    const poleview::MarkedRange area = poleview::changedArea(static_cast<int>(after.size()), marks, kBlockMargin);
    if (area.first < 0)
        return blocks;
    const Neighbours nb = neighboursOf(after);
    for (int m = area.first; m <= area.last; ++m) {
        const pt::PoleRow& row = after[static_cast<std::size_t>(m)];
        if (pt::isEmptyRow(row))
            continue;
        PoleBlock block;
        block.row = m;
        block.id = row.id;
        block.pole = tabletext::trimmed(row.pole);
        block.label = poleview::rowLabel(after, m);
        block.back = makeSpan(after, stored, m, nb.back[static_cast<std::size_t>(m)], true, block.label);
        block.ahead = makeSpan(after, stored, m, nb.ahead[static_cast<std::size_t>(m)], false, block.label);
        block.outerMean = spanMean(block.back, block.ahead, Part::Outer);
        block.middleMean = spanMean(block.back, block.ahead, Part::Middle);
        block.topMean = spanMean(block.back, block.ahead, Part::Top);
        block.lineCalc = lineCalcHeight(block.outerMean, block.middleMean);
        block.av1 = pt::displayText(row, pt::kAv);
        block.av2 = pt::displayText(row, pt::kAv2);
        block.av3 = pt::displayText(row, pt::kAv3);
        const int outer = outerAvField(row);
        block.outerAv = outer < 0 ? std::string() : pt::displayText(row, outer);
        if (row.id > 0) {
            const auto it = stored.topAv.find(row.id);
            if (it != stored.topAv.end())
                block.topAv = it->second;
        }
        blocks.push_back(std::move(block));
    }
    return blocks;
}

namespace {

// Bygger cellerna i ett block. Hjälpfunktionerna lägger celler i rad- och kolumnordning och samlar
// inmatningscellerna i läsordning.
class SheetBuilder {
public:
    explicit SheetBuilder(Sheet& sheet) : sheet_(sheet) {}

    // Börjar ett block: första cellens index och radens id.
    void beginBlock(int block, std::int64_t id) {
        block_ = block;
        sheet_.blockStart.push_back(static_cast<int>(sheet_.cells.size()));
        sheet_.blockRows.push_back(id);
    }

    // Lägger en cell på raden r i blocket och i kolumnen column.
    void add(int r, int column, int rows, int columns, CellStyle style, std::string text, std::string tip = {},
             std::string edit = {}, InputRef input = {}) {
        SheetCell cell;
        cell.block = block_;
        cell.row = block_ * kBlockStride + r;
        cell.column = column;
        cell.rows = rows;
        cell.columns = columns;
        cell.style = style;
        cell.text = std::move(text);
        cell.editText = std::move(edit);
        cell.toolTip = std::move(tip);
        cell.input = input;
        if (style == CellStyle::Input)
            sheet_.inputs.push_back(static_cast<int>(sheet_.cells.size()));
        sheet_.cells.push_back(std::move(cell));
    }

    // En spanncell: inmatning när spannet går att skriva i, annars låst med skälet som tooltip.
    // back säger om spannet är blockets bakre, så att grannens namn tas ur rätt ände av etiketten.
    void addSpan(int r, const SpanRef& span, Part part, const std::string& label, bool back) {
        if (span.exists && span.usable) {
            InputRef ref;
            ref.kind = InputRef::Kind::Span;
            ref.span = span.key;
            ref.part = part;
            const std::string value = commaText(heightOf(span.heights, part));
            const std::string tip = std::string("Linhöjd för ") + partName(part) + " i spannet " + span.label +
                                    ", i meter. Spannet visas också i blocket för " +
                                    neighbourName(span, label, back) + " och sparas en gång.";
            add(r, 3, 1, 1, CellStyle::Input, value, tip, value, ref);
        } else {
            add(r, 3, 1, 1, CellStyle::Locked, std::string(), span.issue);
        }
    }

private:
    Sheet& sheet_;
    int block_ = 0;
};

// Tooltipen för ett medelvärde: halva summan av spannens värden, bara ett spann eller tomt.
std::string meanTip(const PoleBlock& b, Part part, const std::string& mean) {
    if (b.back.exists && b.ahead.exists) {
        const std::string x = spanValue(b.back, part);
        const std::string y = spanValue(b.ahead, part);
        if (!x.empty() && !y.empty())
            return "Medelvärdet av intilliggande spann: (" + commaText(x) + " + " + commaText(y) + ") / 2 = " +
                   commaText(mean) + ".";
        return "Medelvärdet räknas när båda spannen har en linhöjd.";
    }
    if (b.back.exists || b.ahead.exists)
        return sentenceName(b.label) + " har bara ett spann, så medelvärdet är spannets linhöjd.";
    return "Medelvärdet räknas när båda spannen har en linhöjd.";
}

} // namespace

// Blockmallen i planen 9.2, ett block under det andra med en tom rad emellan. Inne i blocket står
// en tom rad mellan delarna.
Sheet buildSheet(const std::vector<PoleBlock>& blocks) {
    Sheet sheet;
    sheet.rows = blocks.empty() ? 0 : static_cast<int>(blocks.size()) * kBlockStride - 1;
    SheetBuilder sb(sheet);
    for (std::size_t i = 0; i < blocks.size(); ++i) {
        const PoleBlock& b = blocks[i];
        sb.beginBlock(static_cast<int>(i), b.id);
        const std::string& p = b.label;
        sb.add(0, 0, 1, 9, CellStyle::PoleHeader, p);

        // Ytterfaserna.
        sb.add(1, 0, 1, 2, CellStyle::OuterHeader, "Linhöjd " + p + " ytterfaser");
        sb.add(1, 3, 1, 1, CellStyle::OuterMeter, "meter");
        sb.add(1, 4, 1, 2, CellStyle::MeanHeader, "medelvärdet av intilliggande spann");
        sb.add(2, 0, 1, 2, CellStyle::Label, b.back.label);
        sb.addSpan(2, b.back, Part::Outer, p, true);
        sb.add(2, 4, 2, 1, CellStyle::Computed, roundedText(b.outerMean, kMeanDecimals),
               meanTip(b, Part::Outer, b.outerMean));
        sb.add(2, 6, 1, 1, CellStyle::OuterAvLabel, "Av");
        {
            const std::string x = b.av1.empty() ? "tom" : b.av1;
            const std::string y = b.av3.empty() ? "tom" : b.av3;
            sb.add(2, 7, 1, 1, CellStyle::AvValue, b.outerAv,
                   "Det största av Av Fas1 (" + x + ") och Av Fas3 (" + y + ") i Stolptabell efter.");
        }
        sb.add(3, 0, 1, 2, CellStyle::Label, b.ahead.label);
        sb.addSpan(3, b.ahead, Part::Outer, p, false);

        // Mittfasen.
        sb.add(5, 0, 1, 2, CellStyle::MiddleHeader, "Linhöjd " + p + " mittfas");
        sb.add(5, 3, 1, 1, CellStyle::MiddleMeter, "meter");
        sb.add(5, 4, 1, 2, CellStyle::MeanHeader, "medelvärdet av intilliggande spann");
        sb.add(6, 0, 1, 2, CellStyle::Label, b.back.label);
        sb.addSpan(6, b.back, Part::Middle, p, true);
        sb.add(6, 4, 2, 1, CellStyle::Computed, roundedText(b.middleMean, kMeanDecimals),
               meanTip(b, Part::Middle, b.middleMean));
        sb.add(6, 6, 1, 1, CellStyle::MiddleAvLabel, "Av");
        sb.add(6, 7, 1, 1, CellStyle::AvValue, b.av2, "Av Fas2 i Stolptabell efter.");
        sb.add(7, 0, 1, 2, CellStyle::Label, b.ahead.label);
        sb.addSpan(7, b.ahead, Part::Middle, p, false);

        // Linhöjd faslina för linberäkning.
        sb.add(9, 0, 1, 4, CellStyle::LineCalcHeader, "Linhöjd faslina för linberäkning");
        {
            std::string tip = "Räknas när ytterfasernas och mittfasens medelvärden finns.";
            if (!b.outerMean.empty() && !b.middleMean.empty())
                tip = "Medelvärdet av ytterfasernas och mittfasens medelvärden: (" + commaText(b.outerMean) + " + " +
                      commaText(b.middleMean) + ") / 2 = " + commaText(b.lineCalc) + ".";
            sb.add(10, 0, 1, 1, CellStyle::Label, p);
            sb.add(10, 1, 1, 3, CellStyle::Computed, roundedText(b.lineCalc, kMeanDecimals), tip);
        }

        // Topplinan med topplinans Av.
        sb.add(12, 0, 1, 2, CellStyle::TopHeader, "Linhöjd " + p + " Topplina");
        sb.add(12, 3, 1, 1, CellStyle::TopMeter, "meter");
        sb.add(12, 4, 1, 2, CellStyle::MeanHeader, "medelvärdet av intilliggande spann");
        sb.add(13, 0, 1, 2, CellStyle::Label, b.back.label);
        sb.addSpan(13, b.back, Part::Top, p, true);
        sb.add(13, 4, 2, 1, CellStyle::Computed, roundedText(b.topMean, kMeanDecimals),
               meanTip(b, Part::Top, b.topMean));
        sb.add(13, 6, 1, 1, CellStyle::TopAvLabel, "Av");
        if (b.id > 0) {
            InputRef ref;
            ref.kind = InputRef::Kind::TopAv;
            ref.row = b.id;
            sb.add(13, 7, 1, 1, CellStyle::Input, tabletext::formatDecimalText(b.topAv, ',', true),
                   "Topplinans Av för " + p + ", i meter. Skrivs här och sparas i projektet.",
                   commaText(b.topAv), ref);
        } else {
            sb.add(13, 7, 1, 1, CellStyle::Locked, std::string(),
                   sentenceName(p) + " är inte sparad i projektet än, så topplinans Av kan inte skrivas.");
        }
        sb.add(14, 0, 1, 2, CellStyle::Label, b.ahead.label);
        sb.addSpan(14, b.ahead, Part::Top, p, false);

        // Linhöjd faslina för beräkning av sammanslagning: mittfasens värden bak och fram.
        sb.add(16, 0, 1, 4, CellStyle::MergeHeader, "Linhöjd faslina för beräkning av sammanslagning");
        const SpanRef* spans[2] = {&b.back, &b.ahead};
        for (int k = 0; k < 2; ++k) {
            const SpanRef& s = *spans[k];
            const std::string value = commaText(spanValue(s, Part::Middle));
            sb.add(17 + k, 0, 1, 1, CellStyle::Label, s.label);
            sb.add(17 + k, 1, 1, 3, CellStyle::Linked, value,
                   s.exists ? "Mittfasens linhöjd i spannet " + s.label + "." : std::string());
        }
    }
    return sheet;
}

// Cellen som täcker raden och kolumnen, sökt bara i blocket som raden hör till.
int cellAt(const Sheet& sheet, int row, int column) {
    if (row < 0 || column < 0 || sheet.blockStart.empty())
        return -1;
    const std::size_t block = static_cast<std::size_t>(row / kBlockStride);
    if (block >= sheet.blockStart.size())
        return -1;
    const int first = sheet.blockStart[block];
    const int end = block + 1 < sheet.blockStart.size() ? sheet.blockStart[block + 1]
                                                        : static_cast<int>(sheet.cells.size());
    for (int i = first; i < end; ++i) {
        const SheetCell& c = sheet.cells[static_cast<std::size_t>(i)];
        if (row >= c.row && row < c.row + c.rows && column >= c.column && column < c.column + c.columns)
            return i;
    }
    return -1;
}

// Next och Previous går i läsordning, Down och Up till närmaste inmatning i samma kolumn.
int moveInput(const Sheet& sheet, int from, Move move) {
    const std::vector<int>& in = sheet.inputs;
    const auto pos = std::find(in.begin(), in.end(), from);
    if (pos == in.end())
        return from;
    if (move == Move::Next)
        return pos + 1 != in.end() ? *(pos + 1) : from;
    if (move == Move::Previous)
        return pos != in.begin() ? *(pos - 1) : from;
    const SheetCell& cur = sheet.cells[static_cast<std::size_t>(from)];
    int best = from;
    int bestRow = 0;
    for (int i : in) {
        const SheetCell& c = sheet.cells[static_cast<std::size_t>(i)];
        if (c.column != cur.column)
            continue;
        const bool wanted = move == Move::Down ? c.row > cur.row : c.row < cur.row;
        if (!wanted)
            continue;
        const bool better = best == from || (move == Move::Down ? c.row < bestRow : c.row > bestRow);
        if (better) {
            best = i;
            bestRow = c.row;
        }
    }
    return best;
}

// Inmatningscellen i blocket för raden som skriver input.
int findInput(const Sheet& sheet, std::int64_t blockRow, const InputRef& input) {
    for (int i : sheet.inputs) {
        const SheetCell& c = sheet.cells[static_cast<std::size_t>(i)];
        const std::size_t block = static_cast<std::size_t>(c.block);
        if (block < sheet.blockRows.size() && sheet.blockRows[block] == blockRow && c.input == input)
            return i;
    }
    return -1;
}

// Färgerna och stilen i planen 9.3.
CellLook cellLook(CellStyle style) {
    CellLook look;
    look.filled = true;
    look.text = {0, 0, 0};
    look.bold = false;
    look.align = Align::Center;
    look.frame = Frame::Thin;
    switch (style) {
    case CellStyle::PoleHeader:
        look.fill = {0xBF, 0xBF, 0xBF};
        look.bold = true;
        look.frame = Frame::Thick;
        break;
    case CellStyle::OuterHeader:
    case CellStyle::OuterMeter:
        look.fill = {0xFF, 0xE6, 0x99};
        look.bold = true;
        look.frame = Frame::Thick;
        break;
    case CellStyle::MiddleHeader:
    case CellStyle::MiddleMeter:
        look.fill = {0xF8, 0xCB, 0xAD};
        look.bold = true;
        look.frame = Frame::Thick;
        break;
    case CellStyle::LineCalcHeader:
    case CellStyle::MergeHeader:
        look.fill = {0x00, 0xB0, 0xF0};
        look.bold = true;
        look.frame = Frame::Thick;
        break;
    case CellStyle::TopHeader:
    case CellStyle::TopMeter:
        look.fill = {0x92, 0xD0, 0x50};
        look.bold = true;
        look.frame = Frame::Thick;
        break;
    case CellStyle::MeanHeader:
        look.filled = false;
        look.bold = true;
        look.align = Align::Left;
        look.frame = Frame::None;
        break;
    case CellStyle::OuterAvLabel:
        look.fill = {0xFF, 0xE6, 0x99};
        look.bold = true;
        break;
    case CellStyle::MiddleAvLabel:
        look.fill = {0xF8, 0xCB, 0xAD};
        look.bold = true;
        break;
    case CellStyle::TopAvLabel:
        look.fill = {0x92, 0xD0, 0x50};
        look.bold = true;
        break;
    case CellStyle::Label:
        look.fill = {0xC5, 0xE0, 0xB4};
        break;
    case CellStyle::AvValue:
        look.fill = {0xC5, 0xE0, 0xB4};
        look.mark = Mark::Sync;
        break;
    case CellStyle::Input:
        look.fill = {0xFF, 0xFF, 0xFF};
        look.text = {0x00, 0x61, 0x00};
        break;
    case CellStyle::Locked:
        look.fill = {0xE7, 0xE6, 0xE6};
        look.text = {0x80, 0x80, 0x80};
        break;
    case CellStyle::Computed:
    case CellStyle::Linked:
        look.fill = {0xC5, 0xE0, 0xB4};
        look.text = {0xFA, 0x7D, 0x00};
        look.bold = true;
        look.mark = style == CellStyle::Computed ? Mark::Sum : Mark::Sync;
        break;
    }
    return look;
}

// Gult med alfa på vitt: vitt + (gult − vitt) · alfa / 255, avrundat.
Rgb blockFill(int block) {
    const int alpha = block % 2 == 0 ? 64 : 52;
    auto mix = [alpha](int c) {
        return static_cast<std::uint8_t>(255 - ((255 - c) * alpha * 2 + 255) / 510);
    };
    return Rgb{mix(220), mix(160), mix(40)};
}

// Texten ovanför bladet enligt planen 12.2.
std::string areaText(const std::vector<pt::PoleRow>& before, const std::vector<pt::PoleRow>& after,
                     const std::vector<PoleBlock>& blocks) {
    if (after.empty()) {
        return "Stolptabell efter är tom. Kopiera Stolptabell före dit och ändra de stolpar som byggs om, så visas "
               "deras linhöjder här.";
    }
    const std::vector<bool> marks = poleview::filterMarks(before, after);
    if (std::find(marks.begin(), marks.end(), true) == marks.end()) {
        return "Inga stolpar i Stolptabell efter är ändrade mot Stolptabell före, så det finns inga linhöjder att "
               "visa.";
    }
    if (blocks.empty())
        return "De ändrade raderna i Stolptabell efter är tomma, så det finns inga linhöjder att visa.";
    if (blocks.size() == 1)
        return "Visar 1 stolpe: " + blocks.front().label + ".";
    std::string margin = std::to_string(kBlockMargin) + " rader";
    if (kBlockMargin == 1)
        margin = "en rad";
    else if (kBlockMargin == 2)
        margin = "två rader";
    return "Visar " + std::to_string(blocks.size()) + " stolpar: " + blocks.front().label + "–" +
           blocks.back().label + ", de ändrade raderna och " + margin + " före och efter dem.";
}

} // namespace kalkyl::lineheight
