#include "platform/text.h"

#include <windows.h>

namespace kalkyl::platform {

// UTF-8 till UTF-16 med MultiByteToWideChar. Ogiltig UTF-8 ger en tom sträng
// (MB_ERR_INVALID_CHARS), så att felaktiga sökvägar inte tolkas tyst.
std::wstring toWide(std::string_view utf8)
{
    if (utf8.empty())
        return {};
    const int n = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, utf8.data(),
                                      static_cast<int>(utf8.size()), nullptr, 0);
    if (n <= 0)
        return {};
    std::wstring out(static_cast<size_t>(n), L'\0');
    MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, utf8.data(),
                        static_cast<int>(utf8.size()), out.data(), n);
    return out;
}

// UTF-16 till UTF-8 med WideCharToMultiByte. Ogiltig UTF-16 ger en tom sträng.
std::string toUtf8(std::wstring_view wide)
{
    if (wide.empty())
        return {};
    const int n = WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, wide.data(),
                                      static_cast<int>(wide.size()), nullptr, 0, nullptr, nullptr);
    if (n <= 0)
        return {};
    std::string out(static_cast<size_t>(n), '\0');
    WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, wide.data(),
                        static_cast<int>(wide.size()), out.data(), n, nullptr, nullptr);
    return out;
}

// En sökväg ur UTF-8 via std::u8string, så att MinGW inte tolkar den som ANSI.
std::filesystem::path pathFromUtf8(std::string_view utf8)
{
    const std::u8string u8(reinterpret_cast<const char8_t*>(utf8.data()), utf8.size());
    return std::filesystem::path(u8);
}

// En sökväg till UTF-8 via std::u8string, med samma skäl som pathFromUtf8.
std::string pathToUtf8(const std::filesystem::path& p)
{
    const std::u8string u8 = p.u8string();
    return std::string(reinterpret_cast<const char*>(u8.data()), u8.size());
}

} // namespace kalkyl::platform
