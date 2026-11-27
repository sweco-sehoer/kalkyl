# Fliken Linhöjder som rena funktioner: spannen på radernas id, blocken, uträkningarna, bladets
# celler och texterna. Includes skrivs som "lineHeight/line_height.h".
add_library(kalkyl_lineheight STATIC line_height.cpp)
target_include_directories(kalkyl_lineheight PUBLIC ${PROJECT_SOURCE_DIR}/libs)
target_link_libraries(kalkyl_lineheight PUBLIC kalkyl_poletable
                      PRIVATE kalkyl_poleview kalkyl_tabletext kalkyl_warnings)
