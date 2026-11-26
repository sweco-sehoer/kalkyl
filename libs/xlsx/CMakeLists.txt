# miniz som statiskt bibliotek ur källorna som rotens CMakeLists.txt hämtar. Tredjepartskod: inga
# varningar (-w), headrarna som SYSTEM, utan stdio, tid och zlib-namn. miniz_export.h skrivs här.
file(CONFIGURE OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/miniz/miniz_export.h" CONTENT [=[
// Genererad av libs/xlsx/CMakeLists.txt. miniz byggs statiskt i kalkyl_miniz, utan export.
#pragma once
#define MINIZ_EXPORT
#define MINIZ_NO_EXPORT
#define MINIZ_DEPRECATED
#define MINIZ_DEPRECATED_EXPORT
#define MINIZ_DEPRECATED_NO_EXPORT
]=])
add_library(kalkyl_miniz STATIC
    ${miniz_SOURCE_DIR}/miniz.c ${miniz_SOURCE_DIR}/miniz_zip.c
    ${miniz_SOURCE_DIR}/miniz_tinfl.c ${miniz_SOURCE_DIR}/miniz_tdef.c)
target_include_directories(kalkyl_miniz SYSTEM PUBLIC ${miniz_SOURCE_DIR} ${CMAKE_CURRENT_BINARY_DIR}/miniz)
target_compile_definitions(kalkyl_miniz PUBLIC MINIZ_NO_STDIO MINIZ_NO_TIME MINIZ_NO_ZLIB_COMPATIBLE_NAMES)
target_compile_options(kalkyl_miniz PRIVATE -w)

# Excelfiler: celler, delade strängar, talformat och sammanslagningar ur .xlsx och .xlsm.
add_library(kalkyl_xlsx STATIC xlsx.cpp xml_reader.cpp)
target_include_directories(kalkyl_xlsx PUBLIC ${PROJECT_SOURCE_DIR}/libs)
target_link_libraries(kalkyl_xlsx PUBLIC kalkyl_core PRIVATE kalkyl_platform kalkyl_miniz kalkyl_warnings)
