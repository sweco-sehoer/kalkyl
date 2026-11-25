# Fönstren i tre-filsmodellen: windows/<bas>.cpp + windows/<bas>.glazeui (kompileras som C++).
set(KALKYL_WINDOWS "")
glaze_add_window(KALKYL_WINDOWS
    windows/conductor_dialog
    windows/main_window
    windows/normal_span_dialog
    windows/project_picker)

# Programmet. WIN32 ger en GUI-exe utan konsol.
# Ta bort WIN32 tillfälligt vid felsökning för att se loggen.
add_executable(kalkyl WIN32
    main.cpp
    ${KALKYL_WINDOWS})
target_include_directories(kalkyl PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(kalkyl PRIVATE
    glaze
    glaze_threads_ui
    kalkyl_version
    kalkyl_core
    kalkyl_platform
    kalkyl_conductor
    kalkyl_calc
    kalkyl_systemdb
    kalkyl_projectdb
    kalkyl_appearance
    kalkyl_warnings)

# Programfilens ikon (windres) ur res/kalkyl.ico, framtagen en gång (res/CLAUDE.md).
glaze_app_icon(kalkyl ICO ${PROJECT_SOURCE_DIR}/res/kalkyl.ico)

# Versionsresursen (utgivare, produkt och version) ur libs/version, kompilerad med windres
# som glaze_app_icon gör. Ingen enable_language(RC): då kan målets C++-flaggor
# (-Wall -Wextra) nå windres, som inte känner till dem.
get_filename_component(_bindir "${CMAKE_CXX_COMPILER}" DIRECTORY)
find_program(KALKYL_WINDRES windres HINTS "${_bindir}" REQUIRED)
set(_rc "${CMAKE_BINARY_DIR}/generated/version/version.rc")
set(_rcobj "${CMAKE_CURRENT_BINARY_DIR}/kalkyl_version_rc.o")
add_custom_command(
    OUTPUT "${_rcobj}"
    COMMAND "${KALKYL_WINDRES}" --input-format=rc --output-format=coff -i "${_rc}" -o "${_rcobj}"
    DEPENDS "${_rc}"
    COMMENT "Versionsresursen för kalkyl (windres)"
    VERBATIM)
set_source_files_properties("${_rcobj}" PROPERTIES EXTERNAL_OBJECT TRUE GENERATED TRUE)
target_sources(kalkyl PRIVATE "${_rcobj}")

# assets.bin bär programikonen i fönstret. Den kopieras till exe:ns katalog.
add_custom_command(TARGET kalkyl POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${PROJECT_SOURCE_DIR}/res/assets.bin" "$<TARGET_FILE_DIR:kalkyl>"
    COMMENT "Kopierar assets.bin till körplatsen")
