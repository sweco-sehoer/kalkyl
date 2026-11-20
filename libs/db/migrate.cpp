#include "db/migrate.h"

#include <string>

namespace kalkyl::db {

namespace {

// Ett fel med en given kod och en svensk text. Detaljen är den tekniska förklaringen.
core::Error fail(core::ErrorCode code, std::string message, std::string detail)
{
    core::Error e;
    e.code = code;
    e.message = std::move(message);
    e.detail = std::move(detail);
    return e;
}

// Antal objekt i sqlite_master. Noll betyder en tom fil utan tabeller.
core::Result<std::int64_t> countObjects(Database& db)
{
    auto stmt = db.prepare("SELECT count(*) FROM sqlite_master");
    if (!stmt)
        return stmt.error();
    auto row = stmt.value().step();
    if (!row)
        return row.error();
    return row.value() ? stmt.value().integer(0) : std::int64_t{0};
}

} // namespace

// Kontrollerar application_id och user_version och kör de steg som saknas. Stegen, id och
// version skrivs i en enda transaktion, så att en avbruten migrering inte lämnar halva ändringar.
core::Result<bool> migrate(Database& db, const Schema& schema)
{
    auto idRes = db.pragmaInt("application_id");
    if (!idRes)
        return idRes.error();
    auto verRes = db.pragmaInt("user_version");
    if (!verRes)
        return verRes.error();
    // application_id är ett 32-bitars tal som SQLite kan svara med tecken.
    const auto id = static_cast<std::uint32_t>(idRes.value());
    const std::int64_t version = verRes.value();
    const std::int64_t stepCount = static_cast<std::int64_t>(schema.steps.size());

    std::int64_t current = version;
    if (id == 0 && version == 0) {
        auto objects = countObjects(db);
        if (!objects)
            return objects.error();
        if (objects.value() != 0 || !schema.allowInitialize)
            return fail(core::ErrorCode::WrongApplication, "Filen är inte ett Kalkylprojekt:" "\n" + db.path(),
                        objects.value() != 0 ? "SQLite-fil utan application_id" : "tom fil, initiering inte tillåten");
        current = 0;   // en ny, tom fil: alla steg ska köras
    } else if (id != schema.applicationId) {
        return fail(core::ErrorCode::WrongApplication, "Filen är inte ett Kalkylprojekt:" "\n" + db.path(),
                    "application_id " + std::to_string(id));
    }
    if (version > stepCount)
        return fail(core::ErrorCode::TooNew,
                    "Databasen är skapad med en nyare version av Kalkyl. Uppdatera programmet.",
                    "user_version " + std::to_string(version) + " > " + std::to_string(stepCount));
    if (current == stepCount && id == schema.applicationId)
        return false;

    auto done = db.transaction([&]() -> core::Result<void> {
        // Versionen läses igen inne i transaktionen. En annan anslutning kan ha migrerat
        // samma fil mellan läsningen ovan och BEGIN IMMEDIATE, t.ex. vid två samtidiga starter.
        auto again = db.pragmaInt("user_version");
        if (!again)
            return again.error();
        if (again.value() > current)
            current = again.value();
        if (current >= stepCount)
            return {};
        for (std::int64_t i = current; i < stepCount; ++i)
            if (auto r = db.exec(schema.steps[static_cast<size_t>(i)]); !r)
                return r;
        // Pragmas tar inga parametrar, så talen skrivs in i SQL-texten.
        if (auto r = db.exec("PRAGMA application_id = " + std::to_string(schema.applicationId)); !r)
            return r;
        return db.exec("PRAGMA user_version = " + std::to_string(stepCount));
    });
    if (!done)
        return done.error();
    return true;
}

} // namespace kalkyl::db
