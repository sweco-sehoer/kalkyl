# Beräkningar: normalspannet och tabelltext (tal, urklipp, inklistring). Beror bara på standardbiblioteket.
add_library(kalkyl_calc STATIC normal_span.cpp table_text.cpp)
target_include_directories(kalkyl_calc PUBLIC ${PROJECT_SOURCE_DIR}/libs)
target_link_libraries(kalkyl_calc PRIVATE kalkyl_warnings)

if(KALKYL_TESTS)
    add_executable(kalkyl_calc_test calc_test.cpp)
    target_link_libraries(kalkyl_calc_test PRIVATE kalkyl_calc kalkyl_warnings)
    add_test(NAME calc COMMAND kalkyl_calc_test)
endif()
