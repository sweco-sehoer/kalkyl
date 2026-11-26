# Importen av en stolptabell: rubrikerna, valet av kolumner, utdraget och rådata som rena funktioner.
add_library(kalkyl_poleimport STATIC recognize.cpp extract.cpp)
target_include_directories(kalkyl_poleimport PUBLIC ${PROJECT_SOURCE_DIR}/libs)
target_link_libraries(kalkyl_poleimport PUBLIC kalkyl_xlsx kalkyl_poletable kalkyl_automode
                      PRIVATE kalkyl_tabletext kalkyl_warnings)
