# Automatiskt läge som rena funktioner: sträckorna i stolptabellen, normalspannets stolpar och
# jämförelsen mellan Stolptabell före och efter. Includes skrivs som "autoMode/auto_mode.h".
add_library(kalkyl_automode STATIC auto_mode.cpp)
target_include_directories(kalkyl_automode PUBLIC ${PROJECT_SOURCE_DIR}/libs)
target_link_libraries(kalkyl_automode PUBLIC kalkyl_poletable kalkyl_ancalc
                      PRIVATE kalkyl_tabletext kalkyl_warnings)
