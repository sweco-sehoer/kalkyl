#include "calc/normal_span.h"

#include <algorithm>
#include <cmath>
#include <cstddef>

#include "calc/table_text.h"

namespace kalkyl::calc {

namespace {

// Antalet föregående spann som normalspannet för kontroll 4 och 5 räknas på.
constexpr std::size_t kLocalWindow = 10;

// Sant när texten bara består av blanktecken.
bool blankText(const std::string& s)
{
    return std::all_of(s.begin(), s.end(), [](char c) {
        return c == ' ' || c == char{9} || c == char{10} || c == char{13};
    });
}

// Sant när raden är helt tom: inget Stpnr, ingen spannlängd och ingen H.
bool emptyRow(const SpanPole& p)
{
    return blankText(p.pole) && !p.lengthM && !p.heightM;
}

// Sant när en gräns är ett ändligt tal större än 0.
bool validLimit(double v)
{
    return std::isfinite(v) && v > 0;
}

// Lägger till ett fel eller en varning i listan.
void addIssue(std::vector<SpanIssue>& issues, IssueKind kind, int poleIndex, SpanColumn column,
              std::string message)
{
    issues.push_back({kind, poleIndex, column, std::move(message)});
}

// Normalspannet sqrt(sum(a^3) / sum(a)) för spannen first till last (exklusive last).
double localNormalSpan(const std::vector<double>& spans, std::size_t first, std::size_t last)
{
    double sum = 0;
    double sum3 = 0;
    for (std::size_t i = first; i < last; ++i) {
        sum += spans[i];
        sum3 += spans[i] * spans[i] * spans[i];
    }
    return std::sqrt(sum3 / sum);
}

} // namespace

// Avrundar till 0,1 med halva bort från noll.
double roundTenth(double value)
{
    return std::round(value * 10.0) / 10.0;
}

// Kontroll 1 och 2: kvoten med två decimaler.
std::string ratioText(double ratio)
{
    return formatDecimal(ratio, 2);
}

// Kontroll 3: "1 : q" med q avrundat till 0,1.
std::string adjacentText(double adjacent)
{
    return "1 : " + formatDecimal(roundTenth(adjacent));
}

// Kontroll 4 och 5: an,10 avrundat till heltal och kvoten mot an med två decimaler.
std::string localText(double local10, double an)
{
    return formatDecimal(std::round(local10)) + " / " + formatDecimal(std::round(local10 / an * 100.0) / 100.0);
}

// Räknar normalspannet och kontrollerna 1-5. Först valideras gränser och indata och alla
// fel samlas; finns inget fel räknas summorna, an, an,dH, medelspannet och kontrollerna.
SpanCalculation calculateNormalSpan(const std::vector<SpanPole>& poles, const SpanLimits& limits)
{
    SpanCalculation out;
    auto& issues = out.issues;

    if (!validLimit(limits.longest) || !validLimit(limits.shortest) || !validLimit(limits.adjacent) ||
        !validLimit(limits.localMin) || !validLimit(limits.localMax))
        addIssue(issues, IssueKind::Error, -1, SpanColumn::None, "Gränserna måste vara tal större än 0.");

    // Rader som inte är helt tomma, som index i indatavektorn.
    std::vector<int> active;
    for (std::size_t i = 0; i < poles.size(); ++i)
        if (!emptyRow(poles[i]))
            active.push_back(static_cast<int>(i));

    int missingHeight = 0;
    for (std::size_t k = 0; k < active.size(); ++k) {
        const int idx = active[k];
        const SpanPole& p = poles[static_cast<std::size_t>(idx)];
        if (p.lengthM && !std::isfinite(*p.lengthM))
            addIssue(issues, IssueKind::Error, idx, SpanColumn::Length, "Ogiltigt tal.");
        else if (k > 0 && !p.lengthM)
            addIssue(issues, IssueKind::Error, idx, SpanColumn::Length, "Spannlängd saknas.");
        else if (k > 0 && !(*p.lengthM > 0))
            addIssue(issues, IssueKind::Error, idx, SpanColumn::Length, "Spannlängden måste vara större än 0.");
        if (p.heightM && !std::isfinite(*p.heightM))
            addIssue(issues, IssueKind::Error, idx, SpanColumn::Height, "Ogiltigt tal.");
        else if (!p.heightM)
            ++missingHeight;
    }

    const int poleCount = static_cast<int>(active.size());
    if (poleCount < 2)
        addIssue(issues, IssueKind::Error, -1, SpanColumn::None, "Minst två stolpar (ett spann) behövs.");

    const bool hasError = std::any_of(issues.begin(), issues.end(),
                                      [](const SpanIssue& i) { return i.kind == IssueKind::Error; });
    if (!hasError && missingHeight > 0 && missingHeight < poleCount)
        addIssue(issues, IssueKind::Warning, -1, SpanColumn::None,
                 "H saknas på " + std::to_string(missingHeight) + " av " + std::to_string(poleCount) +
                     " stolpar. aₙ,ΔH räknas bara när alla stolpar har H.");
    if (hasError)
        return out;

    // Spannlängderna: stolpe 1 och framåt (spannet före stolpen). Första stolpens ingår inte.
    const std::size_t n = active.size() - 1;
    std::vector<double> a(n);
    double sumA = 0;
    double sumA3 = 0;
    for (std::size_t s = 0; s < n; ++s) {
        a[s] = *poles[static_cast<std::size_t>(active[s + 1])].lengthM;
        sumA += a[s];
        sumA3 += a[s] * a[s] * a[s];
    }

    SpanResult r;
    r.anExact = std::sqrt(sumA3 / sumA);
    r.an = roundTenth(r.anExact);
    r.totalM = sumA;
    r.spanCount = static_cast<int>(n);
    r.poleCount = poleCount;
    r.meanM = sumA / static_cast<double>(n);
    r.fromPole = poles[static_cast<std::size_t>(active.front())].pole;
    r.toPole = poles[static_cast<std::size_t>(active.back())].pole;

    // Lutande spannlängd och an,dH: bara när alla stolpar har H.
    const bool allHeights = missingHeight == 0;
    double sumL = 0;
    double sumA4OverL = 0;
    r.spans.reserve(n);
    for (std::size_t s = 0; s < n; ++s) {
        const SpanPole& prev = poles[static_cast<std::size_t>(active[s])];
        const SpanPole& cur = poles[static_cast<std::size_t>(active[s + 1])];
        SpanCheck c;
        c.poleIndex = active[s + 1];
        c.lengthM = a[s];
        c.ratio = a[s] / r.an;
        c.tooLong = c.ratio > limits.longest;
        c.tooShort = c.ratio < limits.shortest;
        if (prev.heightM && cur.heightM) {
            const double dh = *cur.heightM - *prev.heightM;
            const double l = std::sqrt(a[s] * a[s] + dh * dh);
            c.inclinedM = l;
            sumL += l;
            sumA4OverL += a[s] * a[s] * a[s] * a[s] / l;
        }
        if (s >= 1) {
            const double q = std::max(a[s - 1], a[s]) / std::min(a[s - 1], a[s]);
            c.adjacent = q;
            c.adjacentExceeded = q > limits.adjacent;
        }
        if (s >= kLocalWindow) {
            const double local = localNormalSpan(a, s - kLocalWindow, s);
            c.local10 = local;
            c.localTooLow = local / r.an < limits.localMin;
            c.localTooHigh = local / r.an > limits.localMax;
        }
        r.tooLongCount += c.tooLong ? 1 : 0;
        r.tooShortCount += c.tooShort ? 1 : 0;
        r.adjacentCount += c.adjacentExceeded ? 1 : 0;
        r.localLowCount += c.localTooLow ? 1 : 0;
        r.localHighCount += c.localTooHigh ? 1 : 0;
        r.spans.push_back(c);
    }
    if (allHeights && sumL > 0)
        r.anDeltaH = roundTenth(std::sqrt(sumA4OverL / sumL));

    out.result = std::move(r);
    return out;
}

} // namespace kalkyl::calc
