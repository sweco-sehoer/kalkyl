#pragma once
#include <optional>
#include <string>
#include <utility>
#include <variant>

namespace kalkyl::core {

// Felets slag. Fönstren väljer text och åtgärd efter koden.
enum class ErrorCode {
    NotFound,          // filen eller mappen finns inte
    Unreachable,       // nätverkssökvägen svarar inte, eller tidsgränsen gick ut
    AccessDenied,      // ingen behörighet, eller skrivskyddat läge
    AlreadyExists,     // målet finns redan
    Locked,            // projektet är öppnat av någon annan (låsfilen lever)
    Conflict,          // originalet har ändrats sedan det öppnades
    NotADatabase,      // inte SQLite, trunkerad eller skadad, eller saknar projektuppgifter
    WrongApplication,  // SQLite-fil men inte Kalkyls (application_id)
    TooNew,            // schemat är nyare än programmet
    Busy,              // SQLITE_BUSY efter busy_timeout, eller filen används vid namnbyte
    Cancelled,         // avbrutet via stop_token
    Io,                // övriga fil- och nätverksfel
    Sql,               // övriga SQLite-fel (tyder på programfel)
};

// Ett fel: koden, en svensk text till användaren och en teknisk detalj till loggen.
// Fönstren väljer åtgärd efter koden och visar message. detail och nativeCode är till loggen.
struct Error {
    ErrorCode   code = ErrorCode::Io;
    std::string message;     // till användaren, med sökväg
    std::string detail;      // sqlite3_errmsg, Win32-felets text o.d.
    int         nativeCode = 0;   // SQLite- eller Win32-kod, 0 = ingen
};

// Antingen ett värde eller ett fel. [[nodiscard]]: ett fel får inte tappas tyst.
// Ett värde läses med value() först när ok() har kontrollerats.
template <class T>
class [[nodiscard]] Result {
public:
    Result(T value) : v_(std::in_place_index<0>, std::move(value)) {}
    Result(Error error) : v_(std::in_place_index<1>, std::move(error)) {}
    bool ok() const { return v_.index() == 0; }
    explicit operator bool() const { return ok(); }
    T&       value() &       { return std::get<0>(v_); }
    const T& value() const&  { return std::get<0>(v_); }
    T&&      value() &&      { return std::get<0>(std::move(v_)); }
    const Error& error() const { return std::get<1>(v_); }
private:
    std::variant<T, Error> v_;
};

// Lyckat eller fel, utan värde.
// Den förvalda konstruktorn betyder lyckat.
template <>
class [[nodiscard]] Result<void> {
public:
    Result() = default;
    Result(Error error) : e_(std::move(error)) {}
    bool ok() const { return !e_.has_value(); }
    explicit operator bool() const { return ok(); }
    const Error& error() const { return *e_; }
private:
    std::optional<Error> e_;
};

} // namespace kalkyl::core
