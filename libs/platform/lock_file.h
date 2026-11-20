#pragma once
#include <string>

#include "core/result.h"

namespace kalkyl::platform {

// Vem som håller ett lås: läses ur låsfilens nyckel=värde-rader.
struct LockInfo { std::string user; std::string computer; std::string pid;
                  std::string openedUtc; std::string app; };

// Låsfilen: skapas med CREATE_NEW, FILE_SHARE_READ och FILE_FLAG_DELETE_ON_CLOSE, dold, och hålls öppen.
// Andra kan läsa den men inte ta bort den medan handtaget lever. När handtaget stängs, också
// vid krasch eller processlut, tar Windows (eller servern) bort filen.
class LockFile {
public:
    static core::Result<LockFile> create(const std::string& path, const LockInfo& info);  // AlreadyExists om filen finns
    // ok = var övergiven och är borttagen; Locked = lever; AccessDenied = får inte tas bort av oss
    static core::Result<void>     removeStale(const std::string& path);
    static core::Result<LockInfo> read(const std::string& path);         // öppnar med FILE_SHARE_READ|WRITE|DELETE
    static LockInfo               currentProcess(std::string app);       // användare, dator, pid, nu
    LockFile(LockFile&&) noexcept;
    LockFile& operator=(LockFile&&) noexcept;
    ~LockFile();                     // = release()
    void release();                  // stänger handtaget och tar bort filen, blockerande
    void releaseInBackground();      // samma sak i en frikopplad std::thread som inte rör glaze
private:
    LockFile() = default;
    void*       handle_ = nullptr;   // HANDLE
    std::string path_;
};

} // namespace kalkyl::platform
