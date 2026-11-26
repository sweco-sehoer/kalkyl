# Stolptabellen i en GlazeUI DataGrid: raderna, Excel-markeringen, papperskorgen, färgerna och
# snabbmenyn. Delas av Stolptabell före och efter. Includes skrivs som "poleGrid/pole_grid.h".
add_library(kalkyl_polegrid STATIC pole_grid.cpp)
target_include_directories(kalkyl_polegrid PUBLIC ${PROJECT_SOURCE_DIR}/libs)
target_link_libraries(kalkyl_polegrid PUBLIC glaze kalkyl_sheetgrid kalkyl_poletable kalkyl_automode
                      PRIVATE kalkyl_tabletext kalkyl_warnings)
