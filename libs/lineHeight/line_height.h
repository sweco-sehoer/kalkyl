#pragma once
#include <array>
#include <cstdint>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "poleTable/pole_table.h"

namespace kalkyl::lineheight {

// Linhöjdens del: ytterfaserna, mittfasen eller topplinan. Varje spann har en linhöjd per del.
enum class Part { Outer, Middle, Top };

// Antalet rader före och efter de ändrade raderna som får block (±1, planen B14 och H1). Skild från
// filtrets poleview::kMargin (2): blocken är ±1 och ytterblockens spann når ±2.
inline constexpr int kBlockMargin = 1;

// Ett spann mellan två grannrader i Stolptabell efter: radernas id (poletable::PoleRow::id) med
// a < b. Stolpnr ingår aldrig. Lagras så i projektet.
struct SpanKey {
    std::int64_t a = 0;
    std::int64_t b = 0;
    auto operator<=>(const SpanKey&) const = default;
};

// Nyckeln för spannet mellan raderna med id x och y, ordnade.
SpanKey spanKey(std::int64_t x, std::int64_t y);

// Sant när 0 < a < b.
bool isValidKey(const SpanKey& key);

// Ett spanns tre linhöjder som kanonisk decimaltext. Tom text betyder att värdet inte är ifyllt.
struct SpanHeights {
    std::string outer;
    std::string middle;
    std::string top;
    bool operator==(const SpanHeights&) const = default;
};

// Delens linhöjd i spannet.
const std::string& heightOf(const SpanHeights& heights, Part part);
std::string& heightOf(SpanHeights& heights, Part part);

// Det som är sparat i projektet: linhöjderna per spann och topplinans Av per rad (radens id till
// kanonisk decimaltext). projectdb håller det lika med raderna i Stolptabell efter (keptHeights).
struct StoredHeights {
    std::map<SpanKey, SpanHeights> spans;
    std::map<std::int64_t, std::string> topAv;
    bool operator==(const StoredHeights&) const = default;
};

// Spannen i Stolptabell efter: nyckeln för varje par av grannrader som inte är helt tomma
// (planen 6.2), i tabellens ordning. Ett par där en rad saknar id (0) ger inget spann.
std::vector<SpanKey> afterSpans(const std::vector<poletable::PoleRow>& after);

// Det av stored som hör till raderna i after (planen 7.6): spannen i afterSpans och topplinans Av
// för rader som finns och inte är helt tomma. projectdb tar bort resten.
StoredHeights keptHeights(const StoredHeights& stored, const std::vector<poletable::PoleRow>& after);

// En linhöjd ur inmatad text som kanonisk decimaltext (komma eller punkt). "" för tom text. Tomt
// svar när texten inte är ett tal eller talet är negativt.
std::optional<std::string> acceptHeight(std::string_view text);

// Topplinans Av ur inmatad text: som acceptHeight, men negativa tal är tillåtna.
std::optional<std::string> acceptAv(std::string_view text);

// Exakt (a + b) / 2 av två icke-negativa kanoniska decimaltexter, räknat på siffrorna utan väg
// via double och utan avslutande nollor. Tom text när någon av dem är tom.
std::string halfSum(std::string_view a, std::string_view b);

// Kanonisk decimaltext avrundad till decimals decimaler, halva bort från noll, med decimalkomma
// och utan gruppering: "16.765" ger "16,77". Tom text ger tom text.
std::string roundedText(std::string_view canonical, int decimals);

// Antalet decimaler i medelvärdena och i linhöjden för linberäkning.
inline constexpr int kMeanDecimals = 2;

// Ett spann sett från ett block: om grannen finns, om spannet går att skriva i, grannens rad
// (platsen i after), etiketten i tabellens ordning med radernas namn ("ST0378-ST0379"), nyckeln,
// de sparade värdena och varför spannet är låst (tooltip).
struct SpanRef {
    bool exists = false;
    bool usable = false;
    int neighbourRow = -1;
    std::string label;
    SpanKey key;
    SpanHeights heights;
    std::string issue;
    bool operator==(const SpanRef&) const = default;
};

// Ett block i fliken: raden (platsen i after och id:t), Stolpnr som det står (trimmat, kan vara
// tomt), radens namn i texterna (poleview::rowLabel), dess två spann, medelvärdena och linhöjden
// för linberäkning som exakt kanonisk text (tom när de inte går att räkna), Av per fas som i
// stolptabellen och topplinans Av som den sparats.
struct PoleBlock {
    int row = -1;
    std::int64_t id = 0;
    std::string pole;
    std::string label;
    SpanRef back;
    SpanRef ahead;
    std::string outerMean;
    std::string middleMean;
    std::string topMean;
    std::string lineCalc;
    std::string av1;
    std::string av2;
    std::string av3;
    std::string outerAv;
    std::string topAv;
    bool operator==(const PoleBlock&) const = default;
};

// Medelvärdet av intilliggande spann för en del (planen 6.3): båda spannen finns och har värden
// ger (bak + fram) / 2, bara ett spann ger dess värde, annars tom text.
std::string spanMean(const SpanRef& back, const SpanRef& ahead, Part part);

// Linhöjd faslina för linberäkning: (ytterfasernas medel + mittfasens medel) / 2, tom när något
// av dem saknas.
std::string lineCalcHeight(std::string_view outerMean, std::string_view middleMean);

// Fältet för ytterfasernas Av: kAv eller kAv3, det som är störst som tal (kAv vid lika, det andra
// när ett är tomt). -1 när båda är tomma.
int outerAvField(const poletable::PoleRow& row);

// Blocken för fliken (planen 6.1 och 6.2): raderna som inte är helt tomma i det ändrade området
// ±kBlockMargin, oavsett Stolpnr, med spannen till grannarna, de sparade värdena, medelvärdena och
// Av. Tom lista när inget är ändrat.
std::vector<PoleBlock> buildBlocks(const std::vector<poletable::PoleRow>& before,
                                   const std::vector<poletable::PoleRow>& after, const StoredHeights& stored);

// Bladets mått i celler: kolumnerna P–X i förlagan, 19 rader per block (en tom rad mellan delarna)
// och en tom rad mellan blocken.
inline constexpr int kSheetColumns = 9;
inline constexpr int kBlockRows = 19;
inline constexpr int kBlockStride = 20;

// Kolumnbredderna i förlagan i tecken (Excels mått). Bladet ritar dem gånger siffrans bredd.
inline constexpr std::array<float, kSheetColumns> kColumnChars = {15.11f, 15.44f, 13.66f, 18.11f, 25.0f,
                                                                  17.0f, 13.0f, 12.11f, 17.0f};

// Cellens stil, som ger färg, fet stil, justering och ram (cellLook). "meter" och "Av" har en stil
// per del (ytterfaser, mittfas, topplina) och får delens rubrikfärg.
enum class CellStyle {
    PoleHeader, OuterHeader, MiddleHeader, LineCalcHeader, TopHeader, MergeHeader,
    OuterMeter, MiddleMeter, TopMeter, MeanHeader, Label, Input, Locked, Computed,
    OuterAvLabel, MiddleAvLabel, TopAvLabel, AvValue, Linked
};

// Vad en inmatningscell skriver: en del av ett spann, eller topplinans Av för raden med id row.
// Kind None för alla andra celler, också låsta.
struct InputRef {
    enum class Kind { None, Span, TopAv };
    Kind kind = Kind::None;
    SpanKey span;
    Part part = Part::Outer;
    std::int64_t row = 0;
    bool operator==(const InputRef&) const = default;
};

// En cell i bladet: blocket, raden och kolumnen i bladet (0-baserade, kolumn 0 är P), hur många
// rader och kolumner den täcker, stilen, texten, redigeringstexten (decimalkomma utan gruppering,
// bara för inmatning), tooltipen och vad den skriver.
struct SheetCell {
    int block = 0;
    int row = 0;
    int column = 0;
    int rows = 1;
    int columns = 1;
    CellStyle style = CellStyle::Label;
    std::string text;
    std::string editText;
    std::string toolTip;
    InputRef input;
    bool operator==(const SheetCell&) const = default;
};

// Bladet: cellerna block för block i rad- och kolumnordning, antalet rader, första cellen och
// radens id per block, och inmatningscellerna i läsordning (Tab).
struct Sheet {
    std::vector<SheetCell> cells;
    int rows = 0;
    std::vector<int> blockStart;
    std::vector<std::int64_t> blockRows;
    std::vector<int> inputs;
    bool operator==(const Sheet&) const = default;
};

// Bladet för blocken enligt blockmallen (planen 9.2), med texterna i planen 12.
Sheet buildSheet(const std::vector<PoleBlock>& blocks);

// Cellen som täcker raden och kolumnen i bladet, också inne i en sammanslagen cell, eller -1.
int cellAt(const Sheet& sheet, int row, int column);

// Förflyttningen mellan inmatningscellerna: Next och Previous i läsordning (Tab och Shift+Tab),
// Down och Up till närmaste inmatningscell i samma kolumn nedanför och ovanför, också i nästa
// block. Svarar med cellen, eller from när det inte finns någon åt det hållet.
enum class Move { Next, Previous, Down, Up };
int moveInput(const Sheet& sheet, int from, Move move);

// Inmatningscellen i blocket för raden med id blockRow som skriver input, eller -1. Två block med
// samma Stolpnr skiljs på id:t.
int findInput(const Sheet& sheet, std::int64_t blockRow, const InputRef& input);

// Färgerna och stilen för en cellstil (planen 9.3).
struct Rgb {
    std::uint8_t r = 0;
    std::uint8_t g = 0;
    std::uint8_t b = 0;
    bool operator==(const Rgb&) const = default;
};
enum class Align { Left, Center };
enum class Frame { None, Thin, Thick };
// Glyfen i cellens vänsterkant, som i Dokumentation: Sum för ett beräknat värde och Sync för ett
// länkat (hämtat ur Stolptabell efter eller ur ett spann i blocket).
enum class Mark { None, Sum, Sync };
struct CellLook {
    bool filled = false;
    Rgb fill;
    Rgb text;
    bool bold = false;
    Align align = Align::Center;
    Frame frame = Frame::Thin;
    Mark mark = Mark::None;
    bool operator==(const CellLook&) const = default;
};
CellLook cellLook(CellStyle style);

// Blockets bakgrund: stolptabellernas gula ton (220, 160, 40) med alfa 64 för jämna och 52 för
// udda block, lagd på vitt. Grannblocken skiljs åt som raderna i Stolptabell före och efter.
Rgb blockFill(int block);

// Texten ovanför bladet (planen 12.2): vilka rader som visas, med blockens namn, eller varför
// inga visas.
std::string areaText(const std::vector<poletable::PoleRow>& before, const std::vector<poletable::PoleRow>& after,
                     const std::vector<PoleBlock>& blocks);

} // namespace kalkyl::lineheight
