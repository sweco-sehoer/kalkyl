# Linberäkningen och lastfallsmallarna. Länkar kalkyl_conductor PUBLIC eftersom LineInput
# innehåller en Conductor, och kalkyl_tabletext PRIVATE för talen i upplysningstexterna.
add_library(kalkyl_linecalc STATIC line_calc.cpp load_cases.cpp)
target_include_directories(kalkyl_linecalc PUBLIC ${PROJECT_SOURCE_DIR}/libs)
target_link_libraries(kalkyl_linecalc PUBLIC kalkyl_conductor PRIVATE kalkyl_tabletext kalkyl_warnings)
