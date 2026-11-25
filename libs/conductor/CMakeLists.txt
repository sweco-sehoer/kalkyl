# Linor: typen Conductor och valideringen. Beror bara på standardbiblioteket.
add_library(kalkyl_conductor STATIC conductor.cpp)
target_include_directories(kalkyl_conductor PUBLIC ${PROJECT_SOURCE_DIR}/libs)
target_link_libraries(kalkyl_conductor PRIVATE kalkyl_warnings)

if(KALKYL_TESTS)
    add_executable(kalkyl_conductor_test conductor_test.cpp)
    target_link_libraries(kalkyl_conductor_test PRIVATE kalkyl_conductor kalkyl_warnings)
    add_test(NAME conductor COMMAND kalkyl_conductor_test)
endif()
