#pragma once
#include <string>
#include <vector>

namespace kalkyl::poletable {

// Cellens slag i en importerad fil.
enum class ImportCellKind { Text, Number, Bool, Error };

// En cell på en importerad rad: kolumnen (1 = A), slaget och värdet som det står i filen.
struct ImportCell {
    int column = 0;
    ImportCellKind kind = ImportCellKind::Text;
    std::string text;
    bool operator==(const ImportCell&) const = default;
};

// En importerad rad: Excels radnummer, Stolpnr som det blev i Stolptabell före och alla celler med
// värde i kolumnordning.
struct ImportRow {
    int excelRow = 0;
    std::string pole;
    std::vector<ImportCell> cells;
    bool operator==(const ImportRow&) const = default;
};

// En kolumn i filen: numret, bokstäverna, rubriken (rubrikraderna uppifrån med " › " emellan,
// sammanslagna celler ifyllda) och fältet i Stolptabell före (kPole … kLkl), -1 för inget.
struct ImportColumn {
    int column = 0;
    std::string letter;
    std::string header;
    int field = -1;
    bool operator==(const ImportColumn&) const = default;
};

// Rådata för en import till Stolptabell före. rows har en post per rad i tabellen, samma ordning.
struct ImportRecord {
    std::string fileName;      // bara namnet, "Stolptabell.xlsm"
    std::string sheetName;
    std::string title;         // "Stolptabell i Banken Ver. 6 …", "" utan titel
    std::string version;       // "Ver. 6", "" utan version
    std::string importedUtc;   // sätts av projectdb vid skrivningen
    int headerFirstRow = 0;
    int headerLastRow = 0;
    int firstDataRow = 0;
    std::string fromPole;
    std::string toPole;
    std::vector<ImportColumn> columns;
    std::vector<ImportRow> rows;
    bool operator==(const ImportRecord&) const = default;
};

} // namespace kalkyl::poletable
