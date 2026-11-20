# setup.exe för Kalkyl med NSIS (makensis ur MSYS2-paketet mingw-w64-ucrt-x86_64-nsis).
# Målet installer ingår inte i ALL och byggs med: cmake --build --preset installer.
# Bara Release paketeras, eftersom Debug aldrig registrerar filtypen och är långsam.
if(NOT CMAKE_BUILD_TYPE STREQUAL "Release")
    return()
endif()
get_filename_component(_bindir "${CMAKE_CXX_COMPILER}" DIRECTORY)
find_program(KALKYL_MAKENSIS makensis HINTS "${_bindir}")
if(NOT KALKYL_MAKENSIS)
    message(STATUS "makensis saknas, målet installer skapas inte (pacman -S mingw-w64-ucrt-x86_64-nsis)")
    return()
endif()

# Version och utgivare kommer ur rotens CMakeLists.txt, sökvägarna med omvända snedstreck.
# -WX gör varningar till fel, och -INPUTCHARSET UTF8 läser skriptet som UTF-8.
set(_setup "${CMAKE_CURRENT_BINARY_DIR}/Kalkyl-${PROJECT_VERSION}-setup.exe")
add_custom_command(
    OUTPUT "${_setup}"
    COMMAND "${KALKYL_MAKENSIS}" -V2 -WX -INPUTCHARSET UTF8
            "-DVERSION=${PROJECT_VERSION}"
            "-DPUBLISHER=${KALKYL_PUBLISHER}"
            "-DEXE=$<SHELL_PATH:$<TARGET_FILE:kalkyl>>"
            "-DASSETS=$<SHELL_PATH:${PROJECT_SOURCE_DIR}/res/assets.bin>"
            "-DICON=$<SHELL_PATH:${PROJECT_SOURCE_DIR}/res/kalkyl.ico>"
            "-DOUTFILE=$<SHELL_PATH:${_setup}>"
            "$<SHELL_PATH:${CMAKE_CURRENT_SOURCE_DIR}/kalkyl.nsi>"
    DEPENDS kalkyl
            "${CMAKE_CURRENT_SOURCE_DIR}/kalkyl.nsi"
            "${CMAKE_CURRENT_SOURCE_DIR}/licens.txt"
            "${PROJECT_SOURCE_DIR}/res/assets.bin"
            "${PROJECT_SOURCE_DIR}/res/kalkyl.ico"
    COMMENT "Bygger Kalkyl-${PROJECT_VERSION}-setup.exe (NSIS)"
    VERBATIM)
add_custom_target(installer DEPENDS "${_setup}")
