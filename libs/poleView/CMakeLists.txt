# Vyn av Stolptabell efter som rena funktioner: filtret Förändrade ±2, det ändrade området (också för fliken Linhöjder),
# de grå raderna, rullningsmålet för Visa förändrade och infotexten. Includes skrivs som "poleView/pole_view.h".
add_library(kalkyl_poleview STATIC pole_view.cpp)
target_include_directories(kalkyl_poleview PUBLIC ${PROJECT_SOURCE_DIR}/libs)
target_link_libraries(kalkyl_poleview PUBLIC kalkyl_poletable
                      PRIVATE kalkyl_automode kalkyl_tabletext kalkyl_warnings)
