# Normalspannsberäkningen. Använder kalkyl_tabletext för talen i kontrolltexterna.
add_library(kalkyl_ancalc STATIC normal_span.cpp)
target_include_directories(kalkyl_ancalc PUBLIC ${PROJECT_SOURCE_DIR}/libs)
target_link_libraries(kalkyl_ancalc PRIVATE kalkyl_tabletext kalkyl_warnings)
