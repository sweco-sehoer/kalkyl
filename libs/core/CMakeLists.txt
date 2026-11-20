# Felmodellen Result<T> som header, gemensam för alla bibliotek. Includes skrivs som "core/result.h".
add_library(kalkyl_core INTERFACE)
target_include_directories(kalkyl_core INTERFACE ${PROJECT_SOURCE_DIR}/libs)
