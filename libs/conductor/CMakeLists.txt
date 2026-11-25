# Linor: typen Conductor och valideringen. Beror bara på standardbiblioteket.
add_library(kalkyl_conductor STATIC conductor.cpp)
target_include_directories(kalkyl_conductor PUBLIC ${PROJECT_SOURCE_DIR}/libs)
target_link_libraries(kalkyl_conductor PRIVATE kalkyl_warnings)
