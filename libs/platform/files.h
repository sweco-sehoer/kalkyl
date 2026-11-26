#pragma once
#include <chrono>
#include <cstdint>
#include <functional>
#include <stop_token>
#include <string>
#include <string_view>
#include <vector>

#include "core/result.h"

namespace kalkyl::platform {

// Vad en sökväg svarar vid en förfrågan.
enum class PathState { Exists, Missing, Unreachable, NoAccess };

// Storlek och senaste skrivtid (FILETIME i 100 ns), för att upptäcka ändringar.
struct FileStamp {
    std::uint64_t size = 0;
    std::int64_t  lastWrite = 0;
    friend bool operator==(const FileStamp&, const FileStamp&) = default;
};

// Förloppsrapport från en kopiering: byte klara och byte totalt.
using Progress = std::function<void(std::uint64_t done, std::uint64_t total)>;

// Strängbehandling utan I/O, säker på UI-tråden även för nätverkssökvägar.
std::string absolutePath(std::string_view path);   // GetFullPathNameW, avgränsaren '\'
std::string pathKey(std::string_view path);        // absolutePath + CharLowerBuffW: jämförelsenyckel
std::string parentDir(std::string_view path);
std::string fileName(std::string_view path);
std::string fileStem(std::string_view path);

// Frågar efter sökvägen i en frikopplad tråd och väntar högst timeout.
// En hängande server ger Unreachable när tiden gått ut. Tråden får hänga kvar utan att störa.
PathState probe(const std::string& path, std::chrono::milliseconds timeout);
// Alla sökvägar parallellt med EN gemensam tidsgräns. Svaret har samma ordning som indata.
std::vector<PathState> probeMany(const std::vector<std::string>& paths,
                                 std::chrono::milliseconds timeout);

core::Result<FileStamp> stamp(const std::string& path);
core::Result<void> ensureDirectory(const std::string& path);   // hela kedjan
// En fil som inte finns räknas som lyckat. En skrivskyddad fil får skrivskyddet borttaget och tas sedan bort.
core::Result<void> removeFile(const std::string& path);
// CopyFileExW med COPY_FILE_FAIL_IF_EXISTS. Förlopp per block. stop ger PROGRESS_CANCEL och Cancelled.
core::Result<void> copyFile(const std::string& from, const std::string& to,
                            const Progress& progress, std::stop_token stop);
core::Result<void> flushFile(const std::string& path);         // öppna + FlushFileBuffers + stäng
// Läser hela filen i minnet med delad åtkomst (GENERIC_READ och FILE_SHARE_READ, WRITE och DELETE),
// så att en fil som är öppen i Excel går att läsa. En fil som är större än maxBytes ger Invalid.
// Övriga fel som win32Error (NotFound, AccessDenied, Busy, Unreachable, Io).
core::Result<std::string> readFileShared(const std::string& path, std::uint64_t maxBytes);
// MoveFileExW(MOVEFILE_WRITE_THROUGH). AlreadyExists om målet finns.
core::Result<void> moveNew(const std::string& from, const std::string& to);
// MoveFileExW(REPLACE_EXISTING | WRITE_THROUGH). Vid delningsfel 3 försök × 500 ms, sedan Busy.
core::Result<void> moveReplace(const std::string& from, const std::string& to);
void setHidden(const std::string& path, bool hidden = true);   // best effort, false visar filen igen
// Tar bort attributet Skrivskyddad, t.ex. på en arbetskopia som CopyFileExW gett originalets attribut.
void clearReadOnly(const std::string& path);                   // best effort

// Win32-fel som Error: FILE/PATH_NOT_FOUND → NotFound, ACCESS_DENIED/WRITE_PROTECT → AccessDenied,
// FILE_EXISTS/ALREADY_EXISTS → AlreadyExists, SHARING/LOCK_VIOLATION → Busy,
// REQUEST_ABORTED → Cancelled, nätverksfelen (53, 54, 55, 59, 64, 67, 121, 1222, 1231,
// 1232, 1236, 2250) → Unreachable, övrigt → Io.
core::Error win32Error(unsigned long code, std::string message);

} // namespace kalkyl::platform
