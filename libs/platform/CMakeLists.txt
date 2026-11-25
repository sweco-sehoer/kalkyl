# Windows-nära hjälpmedel: text, system, filer, låsfil och filassociation.
# Alla Win32-anrop sker med W-varianterna, och sökvägar är UTF-8 utåt.
add_library(kalkyl_platform STATIC
    text.cpp
    system.cpp
    files.cpp
    lock_file.cpp
    association.cpp)
target_include_directories(kalkyl_platform PUBLIC ${PROJECT_SOURCE_DIR}/libs)
target_compile_definitions(kalkyl_platform PRIVATE UNICODE _UNICODE WIN32_LEAN_AND_MEAN)
target_link_libraries(kalkyl_platform
    PUBLIC kalkyl_core
    PRIVATE shell32 ole32 advapi32 uuid kalkyl_warnings)
