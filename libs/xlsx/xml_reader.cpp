#include "xlsx/xml_reader.h"

#include <cctype>

namespace kalkyl::xlsx {

namespace {

// Lägger kodpunkten till som UTF-8. Ogiltiga kodpunkter (0, surrogat, över U+10FFFF) ger false.
bool appendUtf8(std::string& out, unsigned long cp)
{
    if (cp == 0 || cp > 0x10FFFF || (cp >= 0xD800 && cp <= 0xDFFF))
        return false;
    if (cp < 0x80) {
        out += static_cast<char>(cp);
    } else if (cp < 0x800) {
        out += static_cast<char>(0xC0 | (cp >> 6));
        out += static_cast<char>(0x80 | (cp & 0x3F));
    } else if (cp < 0x10000) {
        out += static_cast<char>(0xE0 | (cp >> 12));
        out += static_cast<char>(0x80 | ((cp >> 6) & 0x3F));
        out += static_cast<char>(0x80 | (cp & 0x3F));
    } else {
        out += static_cast<char>(0xF0 | (cp >> 18));
        out += static_cast<char>(0x80 | ((cp >> 12) & 0x3F));
        out += static_cast<char>(0x80 | ((cp >> 6) & 0x3F));
        out += static_cast<char>(0x80 | (cp & 0x3F));
    }
    return true;
}

// Värdet av en hexadecimal siffra, eller -1.
int hexValue(char ch)
{
    if (ch >= '0' && ch <= '9')
        return ch - '0';
    if (ch >= 'a' && ch <= 'f')
        return ch - 'a' + 10;
    if (ch >= 'A' && ch <= 'F')
        return ch - 'A' + 10;
    return -1;
}

// Avkodar XML-entiteterna i raw till out: de fem namngivna och talentiteterna (&#228; och &#xE4;).
// En okänd eller trasig entitet ger false.
bool decodeEntities(std::string_view raw, std::string& out)
{
    out.clear();
    out.reserve(raw.size());
    size_t i = 0;
    while (i < raw.size()) {
        const char ch = raw[i];
        if (ch != '&') {
            out += ch;
            ++i;
            continue;
        }
        const size_t semi = raw.find(';', i);
        if (semi == std::string_view::npos || semi - i > 12)
            return false;
        const std::string_view ent = raw.substr(i + 1, semi - i - 1);
        if (ent == "amp")
            out += '&';
        else if (ent == "lt")
            out += '<';
        else if (ent == "gt")
            out += '>';
        else if (ent == "quot")
            out += '"';
        else if (ent == "apos")
            out += '\'';
        else if (ent.size() >= 2 && ent[0] == '#') {
            unsigned long cp = 0;
            const bool hex = ent[1] == 'x' || ent[1] == 'X';
            const size_t start = hex ? 2 : 1;
            if (start >= ent.size())
                return false;
            for (size_t k = start; k < ent.size(); ++k) {
                const int v = hex ? hexValue(ent[k]) : (ent[k] >= '0' && ent[k] <= '9' ? ent[k] - '0' : -1);
                if (v < 0)
                    return false;
                cp = cp * (hex ? 16 : 10) + static_cast<unsigned long>(v);
                if (cp > 0x10FFFF)
                    return false;
            }
            if (!appendUtf8(out, cp))
                return false;
        } else {
            return false;
        }
        i = semi + 1;
    }
    return true;
}

// Sant för XML:s blanktecken: mellanslag, tabb, radbrytning och vagnretur. Används mellan
// attribut och i taggarnas slut.
bool isSpace(char ch) { return ch == ' ' || ch == char{9} || ch == char{10} || ch == char{13}; }

// Namnet utan namnrymdsprefix: "x:c" ger "c".
std::string localName(std::string_view qualified)
{
    const size_t colon = qualified.rfind(':');
    return std::string(colon == std::string_view::npos ? qualified : qualified.substr(colon + 1));
}

} // namespace

// Startar läsningen. En UTF-8 BOM i början hoppas över.
XmlReader::XmlReader(std::string_view xml) : xml_(xml)
{
    if (xml_.size() >= 3 && xml_[0] == static_cast<char>(0xEF) && xml_[1] == static_cast<char>(0xBB) &&
        xml_[2] == static_cast<char>(0xBF))
        pos_ = 3;
}

// Sätter felet och stänger läsaren. Alla senare anrop av next ger Error.
XmlReader::Kind XmlReader::fail(const std::string& what)
{
    failed_ = true;
    error_ = what + " (byte " + std::to_string(pos_) + ")";
    return Kind::Error;
}

// Går till nästa händelse. Instruktioner och kommentarer hoppas över, CDATA blir Text, en DOCTYPE
// vägras, och ett element som inte stängs eller stängs fel ger Error.
XmlReader::Kind XmlReader::next()
{
    if (failed_)
        return Kind::Error;
    if (done_)
        return Kind::Done;
    if (pendingEnd_) {
        pendingEnd_ = false;
        name_ = localName(open_.back());
        open_.pop_back();
        --depth_;
        return Kind::End;
    }
    for (;;) {
        if (pos_ >= xml_.size()) {
            if (!open_.empty())
                return fail("elementet " + open_.back() + " är inte stängt");
            done_ = true;
            return Kind::Done;
        }
        if (xml_[pos_] != '<') {
            const size_t end = xml_.find('<', pos_);
            const std::string_view raw = xml_.substr(pos_, end == std::string_view::npos ? end : end - pos_);
            if (!decodeEntities(raw, text_))
                return fail("ogiltig entitet i texten");
            pos_ = end == std::string_view::npos ? xml_.size() : end;
            return Kind::Text;
        }
        const std::string_view rest = xml_.substr(pos_);
        if (rest.substr(0, 2) == "<?") {
            const size_t end = xml_.find("?>", pos_ + 2);
            if (end == std::string_view::npos)
                return fail("instruktionen är inte avslutad");
            pos_ = end + 2;
            continue;
        }
        if (rest.substr(0, 4) == "<!--") {
            const size_t end = xml_.find("-->", pos_ + 4);
            if (end == std::string_view::npos)
                return fail("kommentaren är inte avslutad");
            pos_ = end + 3;
            continue;
        }
        if (rest.substr(0, 9) == "<![CDATA[") {
            const size_t end = xml_.find("]]>", pos_ + 9);
            if (end == std::string_view::npos)
                return fail("CDATA är inte avslutad");
            text_.assign(xml_.substr(pos_ + 9, end - pos_ - 9));
            pos_ = end + 3;
            return Kind::Text;
        }
        if (rest.substr(0, 2) == "<!")
            return fail("DOCTYPE och deklarationer tillåts inte");
        if (rest.substr(0, 2) == "</") {
            const size_t end = xml_.find('>', pos_ + 2);
            if (end == std::string_view::npos)
                return fail("sluttaggen är inte avslutad");
            std::string_view qualified = xml_.substr(pos_ + 2, end - pos_ - 2);
            while (!qualified.empty() && isSpace(qualified.back()))
                qualified.remove_suffix(1);
            if (open_.empty() || open_.back() != qualified)
                return fail("sluttaggen " + std::string(qualified) + " passar inte");
            name_ = localName(qualified);
            open_.pop_back();
            --depth_;
            pos_ = end + 1;
            return Kind::End;
        }
        return readTag() ? Kind::Start : Kind::Error;
    }
}

// Läser en starttagg: namnet och attributen. Ett tomt element (<c/>) ger en End direkt efter.
bool XmlReader::readTag()
{
    size_t p = pos_ + 1;
    const size_t nameStart = p;
    while (p < xml_.size() && !isSpace(xml_[p]) && xml_[p] != '>' && xml_[p] != '/')
        ++p;
    if (p == nameStart || p >= xml_.size()) {
        fail("starttaggen är trasig");
        return false;
    }
    const std::string qualified(xml_.substr(nameStart, p - nameStart));
    pos_ = p;
    bool selfClosing = false;
    attrs_.clear();
    if (!readAttributes(selfClosing))
        return false;
    name_ = localName(qualified);
    open_.push_back(qualified);
    ++depth_;
    pendingEnd_ = selfClosing;
    return true;
}

// Läser attributen i en starttagg fram till > eller />. Värdena avkodas, och xmlns-attribut hoppas över.
bool XmlReader::readAttributes(bool& selfClosing)
{
    for (;;) {
        while (pos_ < xml_.size() && isSpace(xml_[pos_]))
            ++pos_;
        if (pos_ >= xml_.size()) {
            fail("starttaggen är inte avslutad");
            return false;
        }
        if (xml_[pos_] == '>') {
            ++pos_;
            return true;
        }
        if (xml_[pos_] == '/') {
            if (pos_ + 1 >= xml_.size() || xml_[pos_ + 1] != '>') {
                fail("starttaggen är trasig");
                return false;
            }
            pos_ += 2;
            selfClosing = true;
            return true;
        }
        const size_t nameStart = pos_;
        while (pos_ < xml_.size() && !isSpace(xml_[pos_]) && xml_[pos_] != '=' && xml_[pos_] != '>' &&
               xml_[pos_] != '/')
            ++pos_;
        const std::string_view attrName = xml_.substr(nameStart, pos_ - nameStart);
        while (pos_ < xml_.size() && isSpace(xml_[pos_]))
            ++pos_;
        if (attrName.empty() || pos_ >= xml_.size() || xml_[pos_] != '=') {
            fail("attributet saknar värde");
            return false;
        }
        ++pos_;
        while (pos_ < xml_.size() && isSpace(xml_[pos_]))
            ++pos_;
        if (pos_ >= xml_.size() || (xml_[pos_] != '"' && xml_[pos_] != '\'')) {
            fail("attributvärdet saknar citattecken");
            return false;
        }
        const char quote = xml_[pos_++];
        const size_t end = xml_.find(quote, pos_);
        if (end == std::string_view::npos) {
            fail("attributvärdet är inte avslutat");
            return false;
        }
        XmlAttribute attr;
        attr.name = localName(attrName);
        const bool isNamespace = attrName == "xmlns" || attrName.substr(0, 6) == "xmlns:";
        if (!decodeEntities(xml_.substr(pos_, end - pos_), attr.value)) {
            fail("ogiltig entitet i attributet");
            return false;
        }
        pos_ = end + 1;
        if (!isNamespace)
            attrs_.push_back(std::move(attr));
    }
}

// Värdet för det första attributet med det lokala namnet, eller tom text.
std::string XmlReader::attribute(std::string_view localName) const
{
    for (const XmlAttribute& a : attrs_)
        if (a.name == localName)
            return a.value;
    return {};
}

// Sant när ett attribut med det lokala namnet finns.
bool XmlReader::hasAttribute(std::string_view localName) const
{
    for (const XmlAttribute& a : attrs_)
        if (a.name == localName)
            return true;
    return false;
}

// Läser förbi elementet vars Start nyss gavs, med allt i det, och förbrukar dess End.
bool XmlReader::skipElement()
{
    const int target = depth_ - 1;
    for (;;) {
        const Kind k = next();
        if (k == Kind::Error || k == Kind::Done)
            return false;
        if (k == Kind::End && depth_ == target)
            return true;
    }
}

// Tar _xHHHH_ i texten till tecknet (ett surrogatpar slås ihop) och gör sedan CR LF och ensamt CR
// till LF. Ett _x som inte är en giltig kod lämnas som det är.
std::string decodeOoxmlText(std::string_view text)
{
    auto codeAt = [&](size_t i, unsigned long& code) {
        if (i + 7 > text.size() || text[i] != '_' || text[i + 1] != 'x' || text[i + 6] != '_')
            return false;
        code = 0;
        for (size_t k = 2; k < 6; ++k) {
            const int v = hexValue(text[i + k]);
            if (v < 0)
                return false;
            code = code * 16 + static_cast<unsigned long>(v);
        }
        return true;
    };
    std::string decoded;
    decoded.reserve(text.size());
    for (size_t i = 0; i < text.size();) {
        unsigned long code = 0;
        if (text[i] == '_' && codeAt(i, code)) {
            if (code >= 0xD800 && code <= 0xDBFF) {
                unsigned long low = 0;
                if (codeAt(i + 7, low) && low >= 0xDC00 && low <= 0xDFFF) {
                    appendUtf8(decoded, 0x10000 + ((code - 0xD800) << 10) + (low - 0xDC00));
                    i += 14;
                    continue;
                }
            } else if (code == 0) {
                // _x0000_ blir inget tecken: det går inte att visa.
                i += 7;
                continue;
            } else if (code < 0xDC00 || code > 0xDFFF) {
                appendUtf8(decoded, code);
                i += 7;
                continue;
            }
        }
        decoded += text[i++];
    }
    std::string out;
    out.reserve(decoded.size());
    for (size_t i = 0; i < decoded.size(); ++i) {
        if (decoded[i] == char{13}) {
            out += char{10};
            if (i + 1 < decoded.size() && decoded[i + 1] == char{10})
                ++i;
        } else {
            out += decoded[i];
        }
    }
    return out;
}

} // namespace kalkyl::xlsx
