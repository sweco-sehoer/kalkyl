#pragma once
// Provbladet "Stolptabell i Banken Ver. 6" som testerna bygger i minnet (planen 2026-11-26_Import_stolptabell,
// bilaga A). Rubrikerna är mallens, utan data ur ref/.
#include <algorithm>
#include <string>
#include <vector>

#include "poleTable/pole_table.h"
#include "tableText/table_text.h"
#include "xlsx/xlsx.h"

namespace kalkyl::testfixture {

// Radbrytningen i cellerna.
inline const std::string kLf(1, char{10});

// Banken Ver. 6 har rubrikerna i rad 11-15, kolumnnummer i rad 16 och första dataraden i rad 19.
inline constexpr int kBankenFirstDataRow = 19;

// Bladets celler under uppbyggnad: en text- eller talcell läggs till per anrop, och finish sorterar
// dem i rad- och kolumnordning och räknar ut lastRow och lastColumn.
struct SheetBuilder {
    xlsx::Sheet sheet;

    // En textcell. Tom text ger ingen cell.
    void text(int row, int column, const std::string& value)
    {
        if (!value.empty())
            sheet.cells.push_back({row, column, xlsx::CellKind::Text, value, -1});
    }

    // En talcell med talet som det står i filen (punkt som decimaltecken).
    void number(int row, int column, const std::string& value, int decimals = -1)
    {
        sheet.cells.push_back({row, column, xlsx::CellKind::Number, value, decimals});
    }

    // En sammanslagning av cellerna mellan två hörn (båda med).
    void merge(int firstRow, int firstColumn, int lastRow, int lastColumn)
    {
        sheet.merges.push_back({firstRow, firstColumn, lastRow, lastColumn});
    }

    // Sorterar cellerna och räknar ut bladets storlek.
    xlsx::Sheet finish()
    {
        std::sort(sheet.cells.begin(), sheet.cells.end(), [](const xlsx::Cell& a, const xlsx::Cell& b) {
            return a.row != b.row ? a.row < b.row : a.column < b.column;
        });
        sheet.lastRow = 0;
        sheet.lastColumn = 0;
        for (const xlsx::Cell& c : sheet.cells) {
            sheet.lastRow = std::max(sheet.lastRow, c.row);
            sheet.lastColumn = std::max(sheet.lastColumn, c.column);
        }
        return sheet;
    }
};

// Kanonisk decimaltext som Banken skriver den: med decimalkomma. Tom text ger tom text.
inline std::string commaText(const std::string& canonical)
{
    std::string s = canonical;
    std::replace(s.begin(), s.end(), '.', ',');
    return s;
}

// Talet i kanonisk text plus delta, som text med decimalkomma. Tom text ger tom text.
inline std::string commaPlus(const std::string& canonical, double delta)
{
    const auto v = tabletext::parseDecimal(canonical);
    if (!v)
        return "";
    return tabletext::formatDecimal(*v + delta, -1, ',');
}

// LKL med delarna på egna rader och en tom rad emellan ("255/262" blir "255", tom rad, "262").
inline std::string lklText(const std::string& lkl)
{
    std::string out;
    for (const char ch : lkl) {
        if (ch == '/')
            out += kLf + kLf;
        else
            out += ch;
    }
    return out;
}

// Rubrikerna i bilaga A: celler, kolumnnummerraden och sammanslagningarna. Kolumnerna är numrerade
// som i Excel (A = 1, I = 9, R = 18, W = 23, AG = 33, BH = 60, BP = 68).
inline void addBankenHeaders(SheetBuilder& b)
{
    b.text(1, 2, "   Stolptabell i Banken Ver. 6 GRUNDLÄGGNING & JORDNING");
    b.text(2, 2, "Stolptabell med data hämtad från IFS (Linjär tillgång). Mall Ver. 11.0 (svk)");
    b.text(9, 2, "Provlinje L1");

    // Rad 11: grupperna.
    b.text(11, 2, "Objekt ID");
    b.text(11, 9, "Koordinater");
    b.text(11, 18, "Profil");
    b.text(11, 29, "Stolpe");
    b.text(11, 42, "Fundament");
    b.text(11, 54, "Stag / Stagförankring");
    b.text(11, 66, "Linor / Isolation");

    // Rad 12.
    b.text(12, 2, "Ordningsnr");
    b.text(12, 9, "N");
    b.text(12, 10, "E");
    b.text(12, 11, "H");
    b.text(12, 12, "Beräknat från koordinater");
    b.text(12, 18, "Stpl nr");
    b.text(12, 20, "Sektion");
    b.text(12, 21, "Avst föreg stolpe");
    b.text(12, 22, "Ah");
    b.text(12, 23, "Av");
    b.text(12, 29, "Stolpe");
    b.text(12, 49, "Höjd till underdel ö.k" + kLf + "Höjd till årtalsspik");
    b.text(12, 58, "Stagförankring");
    b.text(12, 66, "Faslinor");

    // Rad 13.
    b.text(13, 12, "Avst. föreg.");
    b.text(13, 13, "Beräknat" + kLf + "totalavst." + kLf + "från" + kLf + "station");
    b.text(13, 14, "Ah");
    b.text(13, 18, "Slutgiltigt");
    b.text(13, 19, "Byggnads");
    b.text(13, 23, "Max temp");
    b.text(13, 24, "Min temp");
    b.text(13, 33, "Längdklass");
    b.text(13, 59, "Mått från typritning");
    b.text(13, 68, "Max temperatur");

    // Rad 14.
    b.text(14, 1, "Internt Id");
    b.text(14, 60, "E");

    // Rad 15: enheter och referenssystem.
    b.text(15, 9, "SWEREF 99 TM");
    b.text(15, 10, "SWEREF 99 TM");
    b.text(15, 11, "RH 2000");
    b.text(15, 12, "m");
    b.text(15, 13, "m");
    b.text(15, 14, "m");
    b.text(15, 20, "km+m");
    b.text(15, 22, "m");
    b.text(15, 23, "m");
    b.text(15, 24, "m");
    b.text(15, 49, "m");
    b.text(15, 60, "cm");
    b.text(15, 68, "°C");

    // Rad 16: kolumnens nummer, B (2) till BP (68).
    for (int c = 2; c <= 68; ++c)
        b.number(16, c, std::to_string(c));

    // Sammanslagningarna, rad 11.
    b.merge(11, 2, 11, 6);
    b.merge(11, 9, 11, 15);
    b.merge(11, 18, 11, 26);
    b.merge(11, 29, 11, 39);
    b.merge(11, 42, 11, 51);
    b.merge(11, 54, 11, 63);
    b.merge(11, 66, 11, 80);
    // Rad 12.
    b.merge(12, 2, 14, 2);
    b.merge(12, 9, 14, 9);
    b.merge(12, 10, 14, 10);
    b.merge(12, 11, 14, 11);
    b.merge(12, 12, 12, 15);
    b.merge(12, 18, 12, 19);
    b.merge(12, 20, 14, 20);
    b.merge(12, 21, 14, 21);
    b.merge(12, 22, 14, 22);
    b.merge(12, 23, 12, 24);
    b.merge(12, 29, 12, 36);
    b.merge(12, 49, 14, 49);
    b.merge(12, 58, 12, 62);
    b.merge(12, 66, 12, 70);
    // Rad 13.
    b.merge(13, 12, 14, 12);
    b.merge(13, 13, 14, 13);
    b.merge(13, 14, 14, 14);
    b.merge(13, 18, 14, 18);
    b.merge(13, 19, 14, 19);
    b.merge(13, 23, 14, 23);
    b.merge(13, 24, 14, 24);
    b.merge(13, 33, 14, 33);
    b.merge(13, 59, 13, 61);
    b.merge(13, 68, 14, 68);
}

// Bladet "Stolptabell" med rubrikerna och en datarad per PoleRow från rad 19, talen som text med
// decimalkomma. Fällorna har egna värden: L är spannet + 1, N är "999", S "A", X är Av - 100 och
// BH "256", radbrytning, "256".
inline xlsx::Sheet bankenV6(const std::vector<poletable::PoleRow>& rows)
{
    SheetBuilder b;
    b.sheet.name = "Stolptabell";
    addBankenHeaders(b);
    int r = kBankenFirstDataRow;
    int id = 214021;
    int seq = 1;
    for (const poletable::PoleRow& p : rows) {
        b.number(r, 1, std::to_string(id++));
        std::string objekt = std::to_string(seq++);
        b.text(r, 2, "L1:" + std::string(4 - std::min<size_t>(4, objekt.size()), '0') + objekt);
        b.text(r, 9, commaText(p.north));
        b.text(r, 10, commaText(p.east));
        b.text(r, 11, commaText(p.height));
        b.text(r, 12, commaPlus(p.spanM, 1));
        b.text(r, 14, "999");
        b.text(r, 18, p.pole);
        b.text(r, 19, "A");
        b.text(r, 21, commaText(p.spanM));
        b.text(r, 22, commaText(p.ah));
        b.text(r, 23, commaText(p.av));
        b.text(r, 24, commaPlus(p.av, -100));
        b.text(r, 33, lklText(p.lkl));
        b.text(r, 60, "256" + kLf + "256");
        ++r;
    }
    return b.finish();
}

// Bilaga A i planen för automatiskt läge: ST0100-ST0109 med Av, Ah, spannlängd, koordinater, H och
// LKL. Raderna har kanonisk decimaltext som Stolptabell före.
inline std::vector<poletable::PoleRow> annexRows()
{
    // Stolpnr, Av, Ah, spannlängd, N, E, H, LKL.
    return {
        {"ST0100", "300", "300", "", "6512000.00", "412000.00", "40.0", "255/262"},
        {"ST0101", "310", "310", "300", "6511700.00", "411950.00", "42.5", ""},
        {"ST0102", "300", "300", "320", "6511380.00", "411900.00", "45.0", "262/262"},
        {"ST0103", "315", "315", "280", "6511100.00", "411860.00", "43.0", ""},
        {"ST0104", "330", "330", "350", "6510750.00", "411810.00", "47.5", "262/262"},
        {"ST0105", "300", "300", "310", "6510440.00", "411770.00", "50.0", ""},
        {"ST0106", "310", "310", "290", "6510150.00", "411730.00", "48.0", "241/241"},
        {"ST0107", "315", "315", "330", "6509820.00", "411690.00", "46.5", ""},
        {"ST0108", "285", "285", "300", "6509520.00", "411650.00", "44.0", ""},
        {"ST0109", "270", "270", "270", "6509250.00", "411610.00", "41.0", "255/262"},
    };
}

} // namespace kalkyl::testfixture
