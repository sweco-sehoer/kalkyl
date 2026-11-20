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

if(KALKYL_TESTS)
    add_executable(kalkyl_platform_test platform_test.cpp)
    target_link_libraries(kalkyl_platform_test PRIVATE kalkyl_platform kalkyl_warnings)
    add_test(NAME platform COMMAND kalkyl_platform_test)

    add_executable(kalkyl_association_test association_test.cpp)
    target_link_libraries(kalkyl_association_test PRIVATE kalkyl_platform kalkyl_warnings)
    add_test(NAME association COMMAND kalkyl_association_test)
endif()
