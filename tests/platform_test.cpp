// Headless-test för platform: text, system, files och LockFile.
// Allt går mot en lokal temporär mapp med åäö i namnet. Felkoden är antalet fel.
// windows.h har en funktion LockFile som krockar med klassen platform::LockFile.
#define LockFile WinLockFile
#include <windows.h>
#undef LockFile

#include <chrono>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <optional>
#include <stop_token>
#include <string>
#include <thread>

#include "platform/files.h"
#include "platform/lock_file.h"
#include "platform/system.h"
#include "platform/text.h"

using namespace kalkyl::platform;
using kalkyl::core::ErrorCode;

namespace {

int g_failures = 0;

#define CHECK(cond)                                                                      \
    do {                                                                                 \
        if (!(cond)) {                                                                   \
            ++g_failures;                                                                \
            std::fprintf(stderr, "FEL %s:%d: %s\n", __FILE__, __LINE__, #cond);         \
        }                                                                                \
    } while (0)

// Skriver en fil med givet innehåll. Sökvägen är UTF-8.
void writeFile(const std::string& path, const std::string& content)
{
    std::ofstream out(pathFromUtf8(path), std::ios::binary);
    out << content;
}

// Textfilens storlek i byte, eller -1 om filen saknas.
long long sizeOf(const std::string& path)
{
    auto s = stamp(path);
    return s ? static_cast<long long>(s.value().size) : -1;
}

// Sant om filen finns just nu.
bool exists(const std::string& path)
{
    return probe(path, std::chrono::seconds(2)) == PathState::Exists;
}

// Text och UTF-8: tur och retur med åäö, och sökvägar via std::u8string.
void testText()
{
    const std::string s = "åäö ÅÄÖ é 日本";
    CHECK(toUtf8(toWide(s)) == s);
    CHECK(toWide("åäö").size() == 3);
    CHECK(pathToUtf8(pathFromUtf8(s)) == s);
    CHECK(toWide("").empty());
    // Ogiltig UTF-8 ger en tom sträng.
    CHECK(toWide(std::string("\xff\xfe")).empty());
}

// Strängfunktionerna för sökvägar: ingen I/O, och skiftlägesoberoende nyckel.
void testPathStrings()
{
    const char sep = '\\';
    const std::string dir = std::string("C:") + sep + "Temp" + sep + "Prov åäö";
    const std::string file = dir + sep + "Ny ledning.kalkyl";
    CHECK(parentDir(file) == dir);
    CHECK(fileName(file) == "Ny ledning.kalkyl");
    CHECK(fileStem(file) == "Ny ledning");
    CHECK(fileStem(".dold") == ".dold");
    CHECK(fileStem("utan") == "utan");
    CHECK(parentDir(std::string("C:") + sep + "a.txt") == std::string("C:") + sep);
    CHECK(parentDir("a.txt").empty());

    // Snedstreck normaliseras till avgränsaren, och ÅÄÖ blir åäö i nyckeln.
    const std::string abs = absolutePath("C:/Temp/ÅÄÖ/x.kalkyl");
    CHECK(abs.find('/') == std::string::npos);
    CHECK(abs.rfind("C:", 0) == 0);
    CHECK(pathKey("C:/Temp/ÅÄÖ/X.KALKYL") == pathKey("c:/temp/åäö/x.kalkyl"));
    CHECK(pathKey("C:/Temp/ÅÄÖ/X.KALKYL") != pathKey("c:/temp/aao/x.kalkyl"));
    CHECK(absolutePath("").empty());
    // En relativ sökväg blir absolut.
    CHECK(absolutePath("rel.txt").rfind("C:", 0) == 0 || absolutePath("rel.txt").size() > 7);
}

// System: argument, exe, datamapp, tid och namn.
void testSystem()
{
    CHECK(!commandLineArgs().empty());
    CHECK(!executablePath().empty());
    CHECK(!userName().empty());
    CHECK(!computerName().empty());
    CHECK(!documentsDir().empty());

    const std::string now = nowUtcIso();
    CHECK(now.size() == 24);
    CHECK(now[10] == 'T' && now.back() == 'Z');
    const std::string local = localTimeText(now);
    CHECK(local.size() == 16 && local[10] == ' ');
    CHECK(localTimeText("skräp").empty());

    // KALKYL_DATA_DIR har företräde framför LOCALAPPDATA.
    SetEnvironmentVariableW(L"KALKYL_DATA_DIR", L"C:/Temp/Data åäö");
    CHECK(kalkylDataDir() == "C:/Temp/Data åäö");
    SetEnvironmentVariableW(L"KALKYL_DATA_DIR", nullptr);
    CHECK(kalkylDataDir().find("Kalkyl") != std::string::npos);
}

// Win32-felens översättning till felkoder.
void testWin32Error()
{
    CHECK(win32Error(ERROR_FILE_NOT_FOUND, "x").code == ErrorCode::NotFound);
    CHECK(win32Error(ERROR_PATH_NOT_FOUND, "x").code == ErrorCode::NotFound);
    CHECK(win32Error(ERROR_ACCESS_DENIED, "x").code == ErrorCode::AccessDenied);
    CHECK(win32Error(ERROR_FILE_EXISTS, "x").code == ErrorCode::AlreadyExists);
    CHECK(win32Error(ERROR_ALREADY_EXISTS, "x").code == ErrorCode::AlreadyExists);
    CHECK(win32Error(ERROR_SHARING_VIOLATION, "x").code == ErrorCode::Busy);
    CHECK(win32Error(ERROR_LOCK_VIOLATION, "x").code == ErrorCode::Busy);
    CHECK(win32Error(ERROR_REQUEST_ABORTED, "x").code == ErrorCode::Cancelled);
    CHECK(win32Error(53, "x").code == ErrorCode::Unreachable);
    CHECK(win32Error(1231, "x").code == ErrorCode::Unreachable);
    CHECK(win32Error(2250, "x").code == ErrorCode::Unreachable);
    CHECK(win32Error(ERROR_DISK_FULL, "x").code == ErrorCode::Io);
    CHECK(win32Error(ERROR_FILE_NOT_FOUND, "text").message == "text");
    CHECK(win32Error(ERROR_FILE_NOT_FOUND, "x").nativeCode == ERROR_FILE_NOT_FOUND);
}

// Filfunktionerna mot en lokal mapp: probe, stamp, kopiering, namnbyte och städning.
void testFiles(const std::string& dir)
{
    const auto sec2 = std::chrono::seconds(2);
    const std::string a = dir + "/a.txt";
    writeFile(a, "hej");
    CHECK(probe(a, sec2) == PathState::Exists);
    CHECK(probe(dir + "/finns inte.txt", sec2) == PathState::Missing);
    CHECK(probe(dir + "/finns inte/heller.txt", sec2) == PathState::Missing);
    const auto states = probeMany({a, dir + "/saknas.txt", a}, sec2);
    CHECK(states.size() == 3);
    CHECK(states[0] == PathState::Exists && states[1] == PathState::Missing && states[2] == PathState::Exists);
    CHECK(probeMany({}, sec2).empty());

    auto st = stamp(a);
    CHECK(st.ok() && st.value().size == 3 && st.value().lastWrite != 0);
    CHECK(stamp(a).value() == st.value());
    CHECK(!stamp(dir + "/saknas.txt").ok() && stamp(dir + "/saknas.txt").error().code == ErrorCode::NotFound);

    // ensureDirectory skapar hela kedjan och tål att mappen finns.
    CHECK(ensureDirectory(dir + "/ny/mapp/åäö").ok());
    CHECK(ensureDirectory(dir + "/ny/mapp/åäö").ok());
    CHECK(exists(dir + "/ny/mapp/åäö"));

    // En större fil så att förloppet hinner rapporteras.
    const std::string big = dir + "/stor.bin";
    writeFile(big, std::string(4 * 1024 * 1024, 'x'));
    const std::string copy = dir + "/kopia åäö.bin";
    std::uint64_t lastDone = 0, lastTotal = 0;
    int calls = 0;
    auto copied = copyFile(big, copy, [&](std::uint64_t done, std::uint64_t total) {
        ++calls; lastDone = done; lastTotal = total;
    }, {});
    CHECK(copied.ok());
    CHECK(calls >= 1);
    CHECK(lastTotal == 4u * 1024 * 1024 && lastDone == lastTotal);
    CHECK(sizeOf(copy) == 4 * 1024 * 1024);
    // Ett mål som finns ger AlreadyExists.
    auto again = copyFile(big, copy, {}, {});
    CHECK(!again.ok() && again.error().code == ErrorCode::AlreadyExists);
    // En källa som saknas ger NotFound.
    auto missingSrc = copyFile(dir + "/saknas.bin", dir + "/x.bin", {}, {});
    CHECK(!missingSrc.ok() && missingSrc.error().code == ErrorCode::NotFound);

    // Avbrott vid första förloppsanropet: Cancelled och ingen halv fil kvar.
    const std::string cancelled = dir + "/avbruten.bin";
    std::stop_source source;
    int cancelCalls = 0;
    auto cut = copyFile(big, cancelled, [&](std::uint64_t, std::uint64_t) {
        ++cancelCalls;
        source.request_stop();
    }, source.get_token());
    CHECK(!cut.ok() && cut.error().code == ErrorCode::Cancelled);
    CHECK(cancelCalls == 1);
    CHECK(!exists(cancelled));

    CHECK(flushFile(copy).ok());
    CHECK(!flushFile(dir + "/saknas.bin").ok());

    // moveNew avvisar ett mål som finns och flyttar annars.
    const std::string m1 = dir + "/m1.txt", m2 = dir + "/m2.txt", m3 = dir + "/m3 åäö.txt";
    writeFile(m1, "ett");
    writeFile(m2, "två");
    auto refused = moveNew(m1, m2);
    CHECK(!refused.ok() && refused.error().code == ErrorCode::AlreadyExists);
    CHECK(exists(m1) && sizeOf(m2) == 4);   // "två" är 4 byte i UTF-8
    CHECK(moveNew(m1, m3).ok());
    CHECK(!exists(m1) && exists(m3));

    // moveReplace ersätter ett mål som finns.
    writeFile(m1, "ersättare");
    CHECK(moveReplace(m1, m2).ok());
    CHECK(!exists(m1));
    CHECK(sizeOf(m2) == static_cast<long long>(std::string("ersättare").size()));

    // setHidden döljer filen, och setHidden(false) visar den igen.
    setHidden(m2);
    CHECK((GetFileAttributesW(toWide(m2).c_str()) & FILE_ATTRIBUTE_HIDDEN) != 0);
    setHidden(m2, false);
    CHECK((GetFileAttributesW(toWide(m2).c_str()) & FILE_ATTRIBUTE_HIDDEN) == 0);

    // removeFile tar bort och tål en fil som saknas.
    CHECK(removeFile(m2).ok());
    CHECK(!exists(m2));
    CHECK(removeFile(m2).ok());
    CHECK(removeFile(dir + "/ingen/mapp/fil.txt").ok());

    // clearReadOnly tar bort skrivskyddet, och removeFile tar bort en skrivskyddad fil.
    writeFile(m1, "skyddad");
    SetFileAttributesW(toWide(m1).c_str(), FILE_ATTRIBUTE_READONLY);
    clearReadOnly(m1);
    CHECK((GetFileAttributesW(toWide(m1).c_str()) & FILE_ATTRIBUTE_READONLY) == 0);
    SetFileAttributesW(toWide(m1).c_str(), FILE_ATTRIBUTE_READONLY);
    CHECK(removeFile(m1).ok());
    CHECK(!exists(m1));
}

// Låsfilen: ett enda lås i taget, läsbar för andra, och övergivna lås städas.
void testLockFile(const std::string& dir)
{
    const std::string path = dir + "/Ny ledning.kalkyl.lock";
    LockInfo info = LockFile::currentProcess("Kalkyl 0.2.0");
    CHECK(!info.user.empty() && !info.computer.empty());
    CHECK(info.pid == std::to_string(GetCurrentProcessId()));
    CHECK(info.app == "Kalkyl 0.2.0");
    info.user = "Åsa Öberg";

    auto first = LockFile::create(path, info);
    CHECK(first.ok());
    if (!first.ok())
        return;
    std::optional<LockFile> lock(std::move(first).value());

    // Ett andra lås på samma fil ger AlreadyExists.
    auto second = LockFile::create(path, info);
    CHECK(!second.ok() && second.error().code == ErrorCode::AlreadyExists);

    // Medan handtaget lever ger removeStale Locked, och filen finns kvar.
    auto stale = LockFile::removeStale(path);
    CHECK(!stale.ok() && stale.error().code == ErrorCode::Locked);
    CHECK(exists(path));
    CHECK((GetFileAttributesW(toWide(path).c_str()) & FILE_ATTRIBUTE_HIDDEN) != 0);

    // read ger innehållet, även med åäö och medan låset hålls.
    auto read = LockFile::read(path);
    CHECK(read.ok());
    if (read.ok()) {
        CHECK(read.value().user == "Åsa Öberg");
        CHECK(read.value().computer == info.computer);
        CHECK(read.value().pid == info.pid);
        CHECK(read.value().openedUtc == info.openedUtc);
        CHECK(read.value().app == "Kalkyl 0.2.0");
    }

    // Efter release finns inte filen, och ett nytt lås går att ta.
    lock->release();
    CHECK(!exists(path));
    lock->release();   // dubbel release är ofarlig
    CHECK(LockFile::create(path, info).ok());   // släpps direkt när resultatet rivs
    CHECK(!exists(path));

    // Flytt av ett lås: bara den nya ägaren släpper.
    auto third = LockFile::create(path, info);
    CHECK(third.ok());
    std::optional<LockFile> moved(std::move(third).value());
    LockFile target = std::move(*moved);
    moved.reset();
    CHECK(exists(path));
    target.release();
    CHECK(!exists(path));

    // releaseInBackground tar bort filen i en egen tråd.
    auto fourth = LockFile::create(path, info);
    CHECK(fourth.ok());
    {
        LockFile held = std::move(fourth).value();
        held.releaseInBackground();
    }
    for (int i = 0; i < 40 && exists(path); ++i)
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    CHECK(!exists(path));

    // En övergiven låsfil (ingen håller den) städas utan fel, och en som saknas likaså.
    writeFile(path, "user=Någon\ncomputer=PC\npid=1\n");
    CHECK(LockFile::removeStale(path).ok());
    CHECK(!exists(path));
    CHECK(LockFile::removeStale(path).ok());
    // read av en fil som saknas ger NotFound.
    auto none = LockFile::read(path);
    CHECK(!none.ok() && none.error().code == ErrorCode::NotFound);
}

} // namespace

// readFileShared: läser en fil som en annan handtag har öppen för läsning och skrivning med bara
// FILE_SHARE_READ (som Excel), och ger NotFound för en saknad fil och Invalid för en för stor.
void testReadFileShared(const std::string& dir)
{
    const std::string path = dir + "/delad läsning.xlsx";
    const std::string content = "PK innehåll åäö med radbrytning" + std::string(1, char{10}) + "och mer";
    writeFile(path, content);

    HANDLE excel = CreateFileW(toWide(path).c_str(), GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ, nullptr,
                               OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    CHECK(excel != INVALID_HANDLE_VALUE);
    auto shared = readFileShared(path, 1024);
    CHECK(shared.ok());
    if (shared.ok())
        CHECK(shared.value() == content);
    if (excel != INVALID_HANDLE_VALUE)
        CloseHandle(excel);

    auto missing = readFileShared(dir + "/finns inte.xlsx", 1024);
    CHECK(!missing.ok() && missing.error().code == ErrorCode::NotFound);

    auto tooBig = readFileShared(path, content.size() - 1);
    CHECK(!tooBig.ok() && tooBig.error().code == ErrorCode::Invalid);
    auto exact = readFileShared(path, content.size());
    CHECK(exact.ok());

    writeFile(path, "");
    auto empty = readFileShared(path, 10);
    CHECK(empty.ok() && empty.value().empty());
    CHECK(removeFile(path).ok());
}

int main()
{
    const std::string root = pathToUtf8(std::filesystem::temp_directory_path()) + "/kalkyl-platform-" +
                             std::to_string(GetCurrentProcessId());
    const std::string dir = root + "/Prov åäö";
    if (!ensureDirectory(dir).ok()) {
        std::fprintf(stderr, "FEL: kunde inte skapa testmappen\n");
        return 1;
    }
    testText();
    testPathStrings();
    testSystem();
    testWin32Error();
    testFiles(dir);
    testReadFileShared(dir);
    testLockFile(dir);

    std::error_code ec;
    std::filesystem::remove_all(pathFromUtf8(root), ec);
    if (g_failures == 0)
        std::printf("platform: alla tester gick igenom\n");
    return g_failures;
}
