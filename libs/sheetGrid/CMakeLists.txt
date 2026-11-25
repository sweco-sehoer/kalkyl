# Excel-beteendet i en GlazeUI DataGrid: markering, urklipp och inklistring.
# Includes skrivs som "sheetGrid/sheet_grid.h".
add_library(kalkyl_sheetgrid STATIC sheet_grid.cpp)
target_include_directories(kalkyl_sheetgrid PUBLIC ${PROJECT_SOURCE_DIR}/libs)
target_link_libraries(kalkyl_sheetgrid PUBLIC glaze PRIVATE kalkyl_tabletext kalkyl_warnings)
