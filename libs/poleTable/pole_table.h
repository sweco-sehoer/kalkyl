#pragma once
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace kalkyl::poletable {

// Kolumnerna i stolptabellen i tabellens ordning, samma ordning som i stolptabellerna som
// klistras in. Kolumn 1-6 är tal, 0 och 7 är text. Radera-kolumnen hör till fönstret och
// räknas inte här.
inline constexpr int kPole = 0;       // Stolpnr
inline constexpr int kNorth = 1;      // Koordinat N
inline constexpr int kEast = 2;       // Koordinat E
inline constexpr int kHeight = 3;     // Koordinat H
inline constexpr int kSpan = 4;       // Avstånd föregående (spannlängden)
inline constexpr int kAh = 5;         // Ah
inline constexpr int kAv = 6;         // Av
inline constexpr int kLkl = 7;        // LKL
inline constexpr int kColumnCount = 8;

// En rad i stolptabellen. Talen är kanonisk decimaltext (tabletext::normalizeDecimal): punkt
// som decimaltecken, inga tusentalsavgränsare och exakt de decimaler användaren skrev.
// Tom text = tom cell. Textfälten är trimmade.
struct PoleRow {
    std::string pole;
    std::string av;
    std::string ah;
    std::string spanM;
    std::string north;
    std::string east;
    std::string height;
    std::string lkl;
    bool operator==(const PoleRow&) const = default;
};

// Sant för talkolumnerna 1-6.
bool isNumberColumn(int column);

// Kolumnens namn i meddelanden, t.ex. "Koordinat N". Tom text utanför 0-7.
std::string columnName(int column);

// Fältet för kolumnen 0-7.
const std::string& field(const PoleRow& row, int column);
std::string& field(PoleRow& row, int column);

// LKL i formen "227/262": delarna skilda med radbrytningar (också tomma rader) eller snedstreck
// trimmas och skrivs med ett snedstreck emellan. "227", "227/262", "227\n262" och "227\n\n262"
// ger "227", "227/262", "227/262" och "227/262".
std::string normalizeLkl(std::string_view text);

// Texten som den lagras: text trimmas (LKL via normalizeLkl), tal blir kanonisk decimaltext
// ("" för tomt). Tomt svar = passar inte: inte ett tal i en talkolumn, eller kolumnen finns inte.
std::optional<std::string> acceptField(int column, std::string_view text);

// Cellens text i tabellen: tal med decimalkomma, de lagrade decimalerna och heltalsdelen
// grupperad när kGroupThousands är på; text som den är.
std::string displayText(const PoleRow& row, int column);

// Cellens text i urklippet: som displayText men alltid utan gruppering, så att Excel läser talet.
std::string copyText(const PoleRow& row, int column);

// Talet i en talkolumn, för beräkningar. Tomt för tom cell, textkolumn eller ogiltig text.
std::optional<double> numberValue(const PoleRow& row, int column);

// Sant när alla åtta fält är tomma.
bool isEmptyRow(const PoleRow& row);

// Första talkolumnen vars text varken är tom eller kanonisk decimaltext, annars tomt svar.
std::optional<int> firstInvalidColumn(const PoleRow& row);

// En ändrad cell: rad, kolumn 0-7 och texten som acceptField gav.
struct Edit { int row = 0; int column = 0; std::string text; };

// Skriver ändringarna. En rad bortom slutet ger nya tomma rader fram till och med den.
// Ändringar med negativ rad eller kolumn utanför 0-7 ignoreras.
void applyEdits(std::vector<PoleRow>& rows, const std::vector<Edit>& edits);

// Tar bort helt tomma rader sist i listan.
void trimTrailingEmptyRows(std::vector<PoleRow>& rows);

// Raderna som sparas: alla rader i ordning, också tomma rader mitt i. Bara helt tomma rader
// sist tas bort.
std::vector<PoleRow> rowsToSave(const std::vector<PoleRow>& rows);

} // namespace kalkyl::poletable
