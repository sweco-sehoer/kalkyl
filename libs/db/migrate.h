#pragma once
#include <cstdint>
#include <string_view>
#include <vector>

#include "core/result.h"
#include "db/database.h"

namespace kalkyl::db {

// Schemat: steps[i] tar filen från version i till i+1 (SQL, literaler med statisk livstid).
struct Schema {
    std::uint32_t applicationId = 0;
    std::vector<std::string_view> steps;
    bool allowInitialize = false;    // en tom fil (id 0, version 0, inga tabeller) får skapas
};

// Kontrollerar id och version och kör de steg som saknas i EN transaktion, med id och
// user_version. Svarar true om schemat ändrades.
// Fel: WrongApplication, TooNew, NotADatabase, Sql.
core::Result<bool> migrate(Database& db, const Schema& schema);

} // namespace kalkyl::db
