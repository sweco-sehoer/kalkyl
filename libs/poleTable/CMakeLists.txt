# Stolptabellens rader: kolumnerna, talen som decimaltext och ändringarna. Beror på tableText.
add_library(kalkyl_poletable STATIC pole_table.cpp)
target_include_directories(kalkyl_poletable PUBLIC ${PROJECT_SOURCE_DIR}/libs)
target_link_libraries(kalkyl_poletable PRIVATE kalkyl_tabletext kalkyl_warnings)
