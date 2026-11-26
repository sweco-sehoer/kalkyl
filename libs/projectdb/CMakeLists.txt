# Projektdatabasen: en .kalkyl-fil per projekt, med lokal arbetskopia, lås och säker publicering.
add_library(kalkyl_projectdb STATIC project_db.cpp project_data.cpp)
target_include_directories(kalkyl_projectdb PUBLIC ${PROJECT_SOURCE_DIR}/libs)
target_link_libraries(kalkyl_projectdb PUBLIC kalkyl_db kalkyl_platform kalkyl_ancalc kalkyl_conductor
                                              kalkyl_linecalc kalkyl_poletable kalkyl_automode
                      PRIVATE kalkyl_tabletext kalkyl_version kalkyl_warnings)
