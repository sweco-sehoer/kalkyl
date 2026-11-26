#pragma once
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "core/result.h"

namespace kalkyl::xlsx {

// Cellens slag. En tom cell (inget värde eller en tom text) finns inte i Sheet::cells.
enum class CellKind { Text, Number, Bool, Error };

// En cell med värde. row och column är 1-baserade (A = 1). text är värdet som det står i filen:
// strängen (delad eller inline, rik text ihopfogad, radbrytningar som LF), talet som i <v> med
// punkt ("7047024.75", "422.10000000000002"), Bool som "1" eller "0" och Error som "#N/A". En
// formel ger sitt sparade resultat. decimals är decimalerna i cellens talformat ("0.00" ger 2),
// -1 för Allmänt eller ett format som inte är ett enkelt talformat.
struct Cell {
    int row = 0;
    int column = 0;
    CellKind kind = CellKind::Text;
    std::string text;
    int decimals = -1;
    bool operator==(const Cell&) const = default;
};

// En sammanslagning, till exempel W12:X12. Bara den övre vänstra cellen har värdet.
struct Merge {
    int firstRow = 0;
    int firstColumn = 0;
    int lastRow = 0;
    int lastColumn = 0;
    bool operator==(const Merge&) const = default;
};

// Ett kalkylblad: cellerna med värde i rad- och kolumnordning och sammanslagningarna.
struct Sheet {
    std::string name;
    bool hidden = false;            // state "hidden" eller "veryHidden" i workbook.xml
    std::vector<Cell> cells;
    std::vector<Merge> merges;
    int lastRow = 0;                // största rad med en cell, 0 = tomt blad
    int lastColumn = 0;             // största kolumn med en cell
    // Cellen på raden och kolumnen, eller nullptr när den är tom. Binärsökning i cells.
    const Cell* find(int row, int column) const;
};

// Arbetsboken: kalkylbladen i bokens ordning. Diagramblad och makroblad tas inte med.
struct Workbook {
    std::vector<Sheet> sheets;
};

// Läser en .xlsx eller .xlsm ur minnet. fileName står i felens text. Makron och allt annat än
// relationerna, bladen, de delade strängarna, talformaten och sammanslagningarna läses aldrig.
// Fel (Invalid, texterna i planen 9.1): inte en zip, en äldre .xls eller lösenordsskyddad fil,
// skadad zip eller XML, inga kalkylblad, en del som är för stor uppackad.
core::Result<Workbook> readWorkbook(std::string_view bytes, std::string_view fileName);

// Läser filen med delad läsning (platform::readFileShared, högst kMaxFileBytes), så att den kan
// vara öppen i Excel, och sedan readWorkbook. Felen har filens namn i texten (planen 9.1).
core::Result<Workbook> openWorkbook(const std::string& utf8Path);

// Gränserna: filen på disk och varje uppackad del.
inline constexpr std::uint64_t kMaxFileBytes = 64ull * 1024 * 1024;
inline constexpr std::uint64_t kMaxPartBytes = 256ull * 1024 * 1024;

// Kolumnens bokstäver: 1 = "A", 27 = "AA", 89 = "CK". Tom text utanför 1-16384.
std::string columnLetters(int column);

// En cellreferens som "AB12" eller "$AB$12" som {rad, kolumn}. Tomt svar när den inte är giltig
// (rad 1-1048576, kolumn 1-16384).
std::optional<std::pair<int, int>> parseCellRef(std::string_view ref);

// Decimalerna i ett talformat: "0.00" ger 2 och "#,##0" ger 0. Allmänt, procent, exponent, datum,
// tid, text och andra format ger -1.
int formatDecimals(std::string_view formatCode);

// Talet som Excel visar det: med decimals decimaler, eller med decimals -1 som formatet Allmänt
// (högst 15 värdesiffror, utan nollor sist). Punkt som decimaltecken och ingen exponent.
// Tomt svar när text inte är ett tal.
std::optional<std::string> numberText(std::string_view text, int decimals);

} // namespace kalkyl::xlsx
