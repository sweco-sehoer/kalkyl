#pragma once
#include <string>
#include <string_view>
#include <vector>

namespace kalkyl::xlsx {

// Ett attribut med lokalt namn (utan namnrymdsprefix: "r:id" ger "id") och värdet med entiteter avkodade.
struct XmlAttribute {
    std::string name;
    std::string value;
};

// En liten pull-läsare för XML. Den ger start- och slutelement, text (entiteter avkodade, CDATA
// ingår) och ett fel för skadad XML. Prefix tas bort från element- och attributnamn, så att "x:c"
// och "c" är samma element. En DOCTYPE vägras, så inga entitetsdefinitioner expanderas.
// Läsaren äger inte texten: xml måste leva lika länge som läsaren.
class XmlReader {
public:
    enum class Kind { Start, End, Text, Done, Error };

    explicit XmlReader(std::string_view xml);

    // Går till nästa händelse och ger dess slag. Ett tomt element (<c/>) ger Start och sedan End.
    // Text kan komma i flera delar (text, CDATA, text): den som läser fogar ihop dem.
    // Efter Done eller Error ger varje anrop samma svar.
    Kind next();

    // Start och End: elementets lokala namn.
    const std::string& name() const { return name_; }
    // Start: elementets attribut i filens ordning.
    const std::vector<XmlAttribute>& attributes() const { return attrs_; }
    // Värdet för attributet med det lokala namnet, eller tom text när det saknas.
    std::string attribute(std::string_view localName) const;
    bool hasAttribute(std::string_view localName) const;
    // Text: den avkodade texten (blanktecken orörda, så att xml:space="preserve" hålls).
    const std::string& text() const { return text_; }
    // Error: vad som är fel och ungefär var (byteposition).
    const std::string& error() const { return error_; }
    // Antalet öppna element efter den senaste händelsen.
    int depth() const { return depth_; }

    // Efter ett Start: läser förbi elementet och allt i det, och förbrukar dess End. Ger false vid
    // skadad XML.
    bool skipElement();

private:
    Kind fail(const std::string& what);
    bool readTag();
    bool readAttributes(bool& selfClosing);

    std::string_view xml_;
    size_t pos_ = 0;
    std::string name_;
    std::vector<XmlAttribute> attrs_;
    std::string text_;
    std::string error_;
    std::vector<std::string> open_;
    bool pendingEnd_ = false;
    bool done_ = false;
    bool failed_ = false;
    int depth_ = 0;
};

// OOXML:s textregler för en sträng som läsaren redan har avkodat: _xHHHH_ blir tecknet (UTF-16,
// surrogatpar ihop, "_x005F_" blir ett understreck), och sedan blir CR LF och ensamt CR till LF.
std::string decodeOoxmlText(std::string_view text);

} // namespace kalkyl::xlsx
