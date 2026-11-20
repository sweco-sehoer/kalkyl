# SQLite ur MSYS2, statiskt: cachevariabeln före find_package gör att FindSQLite3
# tar libsqlite3.a i stället för importbiblioteket libsqlite3.dll.a.
find_library(SQLite3_LIBRARY NAMES libsqlite3.a REQUIRED)
find_package(SQLite3 3.40 REQUIRED)
if(NOT TARGET SQLite3::SQLite3)          # CMake < 4.3
    add_library(SQLite3::SQLite3 ALIAS SQLite::SQLite3)
endif()

add_library(kalkyl_db STATIC database.cpp migrate.cpp)
target_include_directories(kalkyl_db PUBLIC ${PROJECT_SOURCE_DIR}/libs)
target_link_libraries(kalkyl_db PUBLIC kalkyl_core PRIVATE kalkyl_platform SQLite3::SQLite3 kalkyl_warnings)

if(KALKYL_TESTS)
    add_executable(kalkyl_db_test db_test.cpp)
    target_link_libraries(kalkyl_db_test PRIVATE kalkyl_db kalkyl_platform kalkyl_warnings)
    add_test(NAME db COMMAND kalkyl_db_test)
endif()
