#pragma once
#include <filesystem>
#include <string>
#include <string_view>

namespace kalkyl::platform {

// Konvertering mellan UTF-8 och UTF-16 (Win32). Ogiltig indata ger en tom sträng.
std::wstring toWide(std::string_view utf8);
std::string  toUtf8(std::wstring_view wide);

// Sökvägar som UTF-8. En std::filesystem::path ur en std::string tolkas som ANSI på MinGW,
// så all konvertering går via std::u8string här.
std::filesystem::path pathFromUtf8(std::string_view utf8);
std::string           pathToUtf8(const std::filesystem::path& p);

} // namespace kalkyl::platform
