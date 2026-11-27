# Linhöjdsbladet, fliken Linhöjders egen GlazeUI-widget. Ett OBJECT-bibliotek, så att
# registreringen (GLAZE_REGISTER_WIDGET) följer med in i kalkyl.exe, och med GLAZE_TOOLS=ON
# också i glaze-render och GlazeBuilder, som då ritar bladet i stället för en ruta med klassnamnet.
add_library(kalkyl_lineheightsheet OBJECT line_height_sheet.cpp)
target_include_directories(kalkyl_lineheightsheet PUBLIC ${PROJECT_SOURCE_DIR}/libs)
target_link_libraries(kalkyl_lineheightsheet PUBLIC glaze kalkyl_lineheight PRIVATE kalkyl_warnings)
foreach(tool glaze-render GlazeBuilder)
    if(TARGET ${tool})
        target_link_libraries(${tool} PRIVATE kalkyl_lineheightsheet)
    endif()
endforeach()
