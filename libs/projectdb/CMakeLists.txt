# Projektdatabasen: en .kalkyl-fil per projekt, med lokal arbetskopia, lås och säker publicering.
add_library(kalkyl_projectdb STATIC project_db.cpp)
target_include_directories(kalkyl_projectdb PUBLIC ${PROJECT_SOURCE_DIR}/libs)
target_link_libraries(kalkyl_projectdb PUBLIC kalkyl_db kalkyl_platform
                      PRIVATE kalkyl_version kalkyl_warnings)

if(KALKYL_TESTS)
    add_executable(kalkyl_projectdb_test projectdb_test.cpp)
    target_link_libraries(kalkyl_projectdb_test PRIVATE kalkyl_projectdb kalkyl_warnings)
    # Provprojektet som testet lämnar kvar åt röktestet.
    target_compile_definitions(kalkyl_projectdb_test PRIVATE
        KALKYL_PROV_DIR="${CMAKE_BINARY_DIR}/prov")
    add_test(NAME projectdb COMMAND kalkyl_projectdb_test)
endif()
