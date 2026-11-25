# Tabelltext: tal med decimalkomma, tabbseparerat urklipp och inklistring. Beror bara på standardbiblioteket.
add_library(kalkyl_tabletext STATIC table_text.cpp)
target_include_directories(kalkyl_tabletext PUBLIC ${PROJECT_SOURCE_DIR}/libs)
target_link_libraries(kalkyl_tabletext PRIVATE kalkyl_warnings)
