# Utseendet (Slate, temat Windows, lägesknappen) som båda toppfönstren delar.
# Includes skrivs som "appearance/appearance.h".
add_library(kalkyl_appearance STATIC appearance.cpp)
target_include_directories(kalkyl_appearance PUBLIC ${PROJECT_SOURCE_DIR}/libs)
target_link_libraries(kalkyl_appearance PUBLIC glaze kalkyl_systemdb PRIVATE kalkyl_warnings)
