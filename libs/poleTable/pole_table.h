#pragma once
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace kalkyl::poletable {

// Fälten i stolptabellen. 0-7 är Stolptabell före i tabellens ordning, samma ordning som i
// stolptabellerna som klistras in och importeras (kColumnCount fält). Stolptabell efter har
// dessutom Av per fas: kAv är Av Fas1, och kAv2 och kAv3 är Fas2 och Fas3 (kFieldCount fält).
// Fält 1-6, 8 och 9 är tal, 0 och 7 är text. Radera-kolumnen hör till fönstret och räknas inte här.
inline constexpr int kPole = 0;       // Stolpnr
inline constexpr int kNorth = 1;      // Koordinat N
inline constexpr int kEast = 2;       // Koordinat E
inline constexpr int kHeight = 3;     // Koordinat H
inline constexpr int kSpan = 4;       // Avstånd föregående (spannlängden)
inline constexpr int kAh = 5;         // Ah
inline constexpr int kAv = 6;         // Av (Av Fas1 i Stolptabell efter)
inline constexpr int kLkl = 7;        // LKL
inline constexpr int kColumnCount = 8;
inline constexpr int kAv2 = 8;        // Av Fas2, bara i Stolptabell efter
inline constexpr int kAv3 = 9;        // Av Fas3, bara i Stolptabell efter
inline constexpr int kFieldCount = 10;

// En rad i stolptabellen. Talen är kanonisk decimaltext (tabletext::normalizeDecimal): punkt
// som decimaltecken, inga tusentalsavgränsare och exakt de decimaler användaren skrev.
// Tom text = tom cell. Textfälten är trimmade. av2 och av3 används bara i Stolptabell efter.
// id är radens id i Stolptabell efter (> 0, unikt i tabellen, 0 = inget id än) och inget fält:
// det syns aldrig i tabellen, urklippet eller jämförelsen med före, och i Stolptabell före är det alltid 0.
struct PoleRow {
    std::string pole;
    std::string av;
    std::string ah;
    std::string spanM;
    std::string north;
    std::string east;
    std::string height;
    std::string lkl;
    std::string av2;
    std::string av3;
    std::int64_t id = 0;
    bool operator==(const PoleRow&) const = default;
};

// Ger varje rad med id <= 0, eller med ett id som redan stått tidigare i listan, nästa lediga id
// (högsta id + 1) i tabellens ordning. Andra rader och alla fält är orörda.
void assignRowIds(std::vector<PoleRow>& rows);

// Fältet i Stolptabell före som ett fält i efter jämförs med och återställs från: Av Fas2 och
// Fas3 jämförs med Av, de andra med sig själva.
int beforeField(int field);

// Kolumnernas fält i tabellens ordning. Före: de åtta fälten 0-7. Efter: Stolpnr, Koordinat N, E
// och H, Avstånd föregående, Ah, Av Fas1, Av Fas2, Av Fas3 och LKL.
const std::vector<int>& beforeColumns();
const std::vector<int>& afterColumns();

// Sant för talfälten 1-6, 8 och 9.
bool isNumberColumn(int column);

// Fältets namn i meddelanden, t.ex. "Koordinat N". Av heter "Av", och Fas2 och Fas3 "Av Fas2"
// och "Av Fas3". Tom text utanför 0-9.
std::string columnName(int column);

// Fältets namn i Stolptabell efter: som columnName, men Av heter "Av Fas1", så att det skiljs från
// Fas2 och Fas3.
std::string afterColumnName(int column);

// Fältet 0-9.
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

// Sant när alla tio fält är tomma. Radens id räknas inte.
bool isEmptyRow(const PoleRow& row);

// Första talfältet vars text varken är tom eller kanonisk decimaltext, annars tomt svar.
std::optional<int> firstInvalidColumn(const PoleRow& row);

// En ändrad cell: rad, fält 0-9 och texten som acceptField gav.
struct Edit { int row = 0; int column = 0; std::string text; };

// Skriver ändringarna. En rad bortom slutet ger nya tomma rader (id 0) fram till och med den.
// Radens id i en befintlig rad står kvar.
// Ändringar med negativ rad eller fält utanför 0-9 ignoreras.
void applyEdits(std::vector<PoleRow>& rows, const std::vector<Edit>& edits);

// Tar bort helt tomma rader sist i listan.
void trimTrailingEmptyRows(std::vector<PoleRow>& rows);

// Raderna som sparas: alla rader i ordning, också tomma rader mitt i. Bara helt tomma rader
// sist tas bort. Radernas id följer med.
std::vector<PoleRow> rowsToSave(const std::vector<PoleRow>& rows);

} // namespace kalkyl::poletable
