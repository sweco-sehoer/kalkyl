#pragma once
#include <optional>
#include <string>
#include <vector>

namespace kalkyl::calc {

// En stolpe i normalspannsberäkningen: en rad i formulärets tabell (Excel C-E).
// lengthM är det horisontella spannet FÖRE stolpen; på första stolpen ingår det inte i summorna.
struct SpanPole {
    std::string pole;                 // Stpnr, får vara tom
    std::optional<double> lengthM;    // spannlängd, m
    std::optional<double> heightM;    // H, plushöjd för mark eller infästning, m
};

// Gränserna för kontrollerna 1-5 (Excel R16, R19, R22, R26, R30), med Excels förval.
struct SpanLimits {
    double longest = 1.5;     // 1: a <= longest * an
    double shortest = 0.5;    // 2: a >= shortest * an
    double adjacent = 2.5;    // 3: större / mindre av två intilliggande spann <= adjacent
    double localMin = 0.95;   // 4: an,10 >= localMin * an
    double localMax = 1.15;   // 5: an,10 <= localMax * an
};

// Kontrollerna för ett spann (Excel I-M). poleIndex är stolpen spannet slutar vid,
// som index i indatavektorn (också när tomma rader hoppats över).
struct SpanCheck {
    int poleIndex = -1;
    double lengthM = 0;
    double ratio = 0;                  // 1-2: a / an, med an avrundat till 0,1 m
    bool tooLong = false;              // 1
    bool tooShort = false;             // 2
    std::optional<double> adjacent;    // 3: >= 1; tomt för första spannet
    bool adjacentExceeded = false;     // 3: paret (föregående, detta) är över gränsen
    std::optional<double> local10;     // 4-5: an,10 för de 10 föregående spannen
    bool localTooLow = false;          // 4
    bool localTooHigh = false;         // 5
    std::optional<double> inclinedM;   // l = sqrt(a^2 + dH^2) när båda stolparna har H
};

// Resultatet. an och anDeltaH är avrundade till 0,1 m som i Excel; anExact är oavrundat.
struct SpanResult {
    double anExact = 0;
    double an = 0;                        // Excel R10
    std::optional<double> anDeltaH;       // Excel T10; tomt = "-"
    double meanM = 0;                     // Excel R13: summan / antal spann
    double totalM = 0;                    // Excel T13
    int spanCount = 0;
    int poleCount = 0;                    // rader som inte är helt tomma
    std::string fromPole;                 // första stolpens Stpnr
    std::string toPole;                   // sista stolpens Stpnr
    std::vector<SpanCheck> spans;         // ett per spann, i ordning
    int tooLongCount = 0;                 // antal spann med kontroll 1
    int tooShortCount = 0;                // 2
    int adjacentCount = 0;                // 3: antal par över gränsen
    int localLowCount = 0;                // 4
    int localHighCount = 0;               // 5
};

enum class IssueKind { Error, Warning };
enum class SpanColumn { None, Pole, Length, Height };

// Ett fel eller en varning. poleIndex -1 och column None gäller hela beräkningen.
struct SpanIssue {
    IssueKind kind = IssueKind::Error;
    int poleIndex = -1;
    SpanColumn column = SpanColumn::None;
    std::string message;                  // svensk text till användaren
};

// Svaret: resultatet när inget fel finns, och alla fel och varningar i radordning.
struct SpanCalculation {
    std::optional<SpanResult> result;
    std::vector<SpanIssue> issues;
};

// Normalspannet och kontrollerna enligt planen avsnitt 2. Helt tomma stolpar hoppas över.
// Fel (inget resultat): färre än två stolpar, spannlängd saknas, spannlängd <= 0 eller inte
// ändlig, H inte ändlig, en gräns <= 0 eller inte ändlig. Varning: H saknas på några stolpar.
SpanCalculation calculateNormalSpan(const std::vector<SpanPole>& poles, const SpanLimits& limits);

// Avrundning till 0,1 som Excels Round(x, 1), fast halva bort från noll (planen 2.5).
double roundTenth(double value);

// Texterna i kontrollkolumnerna, som i Excel: "0,99", "1 : 1,9", "356 / 1,04".
// Kontroll 1 och 2 har två decimaler (planen 2.4).
std::string ratioText(double ratio);
std::string adjacentText(double adjacent);
std::string localText(double local10, double an);

} // namespace kalkyl::calc
