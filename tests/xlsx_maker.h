// Bygger xlsx-filer i minnet åt testerna: zip med minizs skrivdel, arbetsboken med relationer och
// kalkylblad, och xlsx::Sheet som riktiga blad. Inga filer ur ref/ används.
#pragma once
#include <map>
#include <set>
#include <string>
#include <vector>

#include "miniz.h"
#include "xlsx/xlsx.h"

namespace kalkyl::testxlsx {

// En del i zipfilen: namnet i zipen och innehållet.
struct Part {
    std::string name;
    std::string data;
};

// Ett kalkylblad till workbook(): namnet, bladets XML och state ("hidden", "veryHidden" eller "").
struct SheetPart {
    std::string name;
    std::string xml;
    std::string state;
};

// Gör en zip ur delarna, i den ordning de står. Tom text om skrivningen misslyckas.
inline std::string zip(const std::vector<Part>& parts)
{
    mz_zip_archive archive{};
    if (!mz_zip_writer_init_heap(&archive, 0, 0))
        return {};
    for (const Part& p : parts) {
        if (!mz_zip_writer_add_mem(&archive, p.name.c_str(), p.data.data(), p.data.size(), MZ_DEFAULT_COMPRESSION)) {
            mz_zip_writer_end(&archive);
            return {};
        }
    }
    void* buffer = nullptr;
    size_t size = 0;
    if (!mz_zip_writer_finalize_heap_archive(&archive, &buffer, &size)) {
        mz_zip_writer_end(&archive);
        return {};
    }
    std::string out(static_cast<const char*>(buffer), size);
    mz_free(buffer);
    mz_zip_writer_end(&archive);
    return out;
}

// Gör en arbetsbok: [Content_Types].xml, _rels/.rels, xl/workbook.xml med relationer, ett blad per
// post (xl/worksheets/sheetN.xml) och extra delar som sharedStrings.xml och styles.xml. Extra delar
// som har samma namn som en av de egna ersätter den.
inline std::string workbook(const std::vector<SheetPart>& sheets, const std::vector<Part>& extra = {})
{
    std::vector<Part> parts;
    std::string book = R"(<?xml version="1.0" encoding="UTF-8" standalone="yes"?>)"
                       R"(<workbook xmlns="http://schemas.openxmlformats.org/spreadsheetml/2006/main" )"
                       R"(xmlns:r="http://schemas.openxmlformats.org/officeDocument/2006/relationships"><sheets>)";
    std::string rels = R"(<?xml version="1.0" encoding="UTF-8" standalone="yes"?>)"
                       R"(<Relationships xmlns="http://schemas.openxmlformats.org/package/2006/relationships">)";
    int n = 0;
    for (const SheetPart& s : sheets) {
        ++n;
        const std::string idx = std::to_string(n);
        book += "<sheet name=\"" + s.name + "\" sheetId=\"" + std::to_string(n + 10) + "\"";
        if (!s.state.empty())
            book += " state=\"" + s.state + "\"";
        book += " r:id=\"rId" + idx + "\"/>";
        rels += "<Relationship Id=\"rId" + idx +
                "\" Type=\"http://schemas.openxmlformats.org/officeDocument/2006/relationships/worksheet\" "
                "Target=\"worksheets/sheet" + idx + ".xml\"/>";
        parts.push_back({"xl/worksheets/sheet" + idx + ".xml", s.xml});
    }
    book += "</sheets></workbook>";
    // De delade strängarna och formaten får varsin relation när de finns bland de extra delarna.
    for (const Part& e : extra) {
        const char* type = e.name == "xl/sharedStrings.xml" ? "sharedStrings" : e.name == "xl/styles.xml" ? "styles" : nullptr;
        if (type)
            rels += std::string("<Relationship Id=\"rIdx") + type +
                    "\" Type=\"http://schemas.openxmlformats.org/officeDocument/2006/relationships/" + type +
                    "\" Target=\"" + e.name.substr(3) + "\"/>";
    }
    rels += "</Relationships>";
    std::vector<Part> head = {
        {"[Content_Types].xml",
         R"(<?xml version="1.0" encoding="UTF-8" standalone="yes"?>)"
         R"(<Types xmlns="http://schemas.openxmlformats.org/package/2006/content-types">)"
         R"(<Default Extension="xml" ContentType="application/xml"/></Types>)"},
        {"_rels/.rels",
         R"(<?xml version="1.0" encoding="UTF-8" standalone="yes"?>)"
         R"(<Relationships xmlns="http://schemas.openxmlformats.org/package/2006/relationships">)"
         R"(<Relationship Id="rId1" Type="http://schemas.openxmlformats.org/officeDocument/2006/relationships/officeDocument" )"
         R"(Target="xl/workbook.xml"/></Relationships>)"},
        {"xl/workbook.xml", book},
        {"xl/_rels/workbook.xml.rels", rels},
    };
    std::vector<Part> all;
    for (Part& p : head)
        all.push_back(std::move(p));
    for (Part& p : parts)
        all.push_back(std::move(p));
    for (const Part& e : extra) {
        bool replaced = false;
        for (Part& p : all)
            if (p.name == e.name) {
                p.data = e.data;
                replaced = true;
            }
        if (!replaced)
            all.push_back(e);
    }
    return zip(all);
}

namespace detail {

// Kolumnens bokstäver (1 = A, 27 = AA), skrivna här så att makern inte beror på koden som prövas.
inline std::string letters(int column)
{
    std::string out;
    for (int c = column; c > 0; c = (c - 1) / 26)
        out.insert(out.begin(), static_cast<char>('A' + (c - 1) % 26));
    return out;
}

// Text som XML: &, < och > som entiteter.
inline std::string escape(const std::string& s)
{
    std::string out;
    for (char ch : s) {
        if (ch == '&')
            out += "&amp;";
        else if (ch == '<')
            out += "&lt;";
        else if (ch == '>')
            out += "&gt;";
        else
            out += ch;
    }
    return out;
}

// Ett <t>-element som behåller blanktecken och radbrytningar.
inline std::string textElement(const std::string& s)
{
    return "<t xml:space=\"preserve\">" + escape(s) + "</t>";
}

} // namespace detail

// Skriver xlsx::Sheet som riktiga blad. Text blir delade strängar (shared = true) eller inline-
// strängar, med LF som tecken. Tal, Bool, Error, sammanslagningar och dolda blad skrivs som Excel,
// och decimaler (decimals >= 0) blir egna talformat i styles.xml. Cellerna ska vara sorterade.
inline std::string fromSheets(const std::vector<xlsx::Sheet>& sheets, bool shared = true)
{
    std::vector<std::string> sharedList;
    std::map<std::string, int> sharedIndex;
    std::set<int> decimalSet;
    for (const xlsx::Sheet& sh : sheets)
        for (const xlsx::Cell& c : sh.cells) {
            if (c.kind == xlsx::CellKind::Text && shared && !sharedIndex.count(c.text)) {
                sharedIndex[c.text] = static_cast<int>(sharedList.size());
                sharedList.push_back(c.text);
            }
            if (c.kind == xlsx::CellKind::Number && c.decimals >= 0)
                decimalSet.insert(c.decimals);
        }
    // Stilindex: 0 är Allmänt, därefter ett per decimaltal i ordning.
    std::map<int, int> styleOf;
    std::string numFmts;
    std::string xfs = "<xf numFmtId=\"0\"/>";
    int next = 1;
    for (int d : decimalSet) {
        const int fmtId = 164 + next;
        std::string code = d == 0 ? "0" : "0." + std::string(static_cast<size_t>(d), '0');
        numFmts += "<numFmt numFmtId=\"" + std::to_string(fmtId) + "\" formatCode=\"" + code + "\"/>";
        xfs += "<xf numFmtId=\"" + std::to_string(fmtId) + "\"/>";
        styleOf[d] = next++;
    }
    std::vector<SheetPart> parts;
    for (const xlsx::Sheet& sh : sheets) {
        std::string xml = R"(<?xml version="1.0" encoding="UTF-8" standalone="yes"?>)"
                          R"(<worksheet xmlns="http://schemas.openxmlformats.org/spreadsheetml/2006/main"><sheetData>)";
        int currentRow = 0;
        for (const xlsx::Cell& c : sh.cells) {
            if (c.row != currentRow) {
                if (currentRow != 0)
                    xml += "</row>";
                xml += "<row r=\"" + std::to_string(c.row) + "\">";
                currentRow = c.row;
            }
            const std::string ref = detail::letters(c.column) + std::to_string(c.row);
            switch (c.kind) {
            case xlsx::CellKind::Text:
                if (shared)
                    xml += "<c r=\"" + ref + "\" t=\"s\"><v>" + std::to_string(sharedIndex[c.text]) + "</v></c>";
                else
                    xml += "<c r=\"" + ref + "\" t=\"inlineStr\"><is>" + detail::textElement(c.text) + "</is></c>";
                break;
            case xlsx::CellKind::Number: {
                std::string style;
                if (c.decimals >= 0)
                    style = " s=\"" + std::to_string(styleOf[c.decimals]) + "\"";
                xml += "<c r=\"" + ref + "\"" + style + "><v>" + c.text + "</v></c>";
                break;
            }
            case xlsx::CellKind::Bool:
                xml += "<c r=\"" + ref + "\" t=\"b\"><v>" + c.text + "</v></c>";
                break;
            case xlsx::CellKind::Error:
                xml += "<c r=\"" + ref + "\" t=\"e\"><v>" + detail::escape(c.text) + "</v></c>";
                break;
            }
        }
        if (currentRow != 0)
            xml += "</row>";
        xml += "</sheetData>";
        if (!sh.merges.empty()) {
            xml += "<mergeCells count=\"" + std::to_string(sh.merges.size()) + "\">";
            for (const xlsx::Merge& m : sh.merges)
                xml += "<mergeCell ref=\"" + detail::letters(m.firstColumn) + std::to_string(m.firstRow) + ":" +
                       detail::letters(m.lastColumn) + std::to_string(m.lastRow) + "\"/>";
            xml += "</mergeCells>";
        }
        xml += "</worksheet>";
        parts.push_back({detail::escape(sh.name), xml, sh.hidden ? "hidden" : ""});
    }
    std::vector<Part> extra;
    if (shared) {
        std::string sst = R"(<?xml version="1.0" encoding="UTF-8" standalone="yes"?>)"
                          R"(<sst xmlns="http://schemas.openxmlformats.org/spreadsheetml/2006/main">)";
        for (const std::string& s : sharedList)
            sst += "<si>" + detail::textElement(s) + "</si>";
        sst += "</sst>";
        extra.push_back({"xl/sharedStrings.xml", sst});
    }
    std::string styles = R"(<?xml version="1.0" encoding="UTF-8" standalone="yes"?>)"
                         R"(<styleSheet xmlns="http://schemas.openxmlformats.org/spreadsheetml/2006/main">)";
    if (!numFmts.empty())
        styles += "<numFmts count=\"" + std::to_string(decimalSet.size()) + "\">" + numFmts + "</numFmts>";
    styles += "<cellXfs count=\"" + std::to_string(decimalSet.size() + 1) + "\">" + xfs + "</cellXfs></styleSheet>";
    extra.push_back({"xl/styles.xml", styles});
    return workbook(parts, extra);
}

} // namespace kalkyl::testxlsx
