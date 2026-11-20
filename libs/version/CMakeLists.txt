# Programmets version som header, genererad från project(VERSION) i rotens CMakeLists.txt.
# Den hamnar i byggkatalogen och checkas aldrig in. Includes skrivs som "version/version.h".
configure_file(version.h.in ${CMAKE_BINARY_DIR}/generated/version/version.h @ONLY)

# Versionsresursen för kalkyl.exe med samma version och KALKYL_PUBLISHER ur rotens
# CMakeLists.txt. Den kompileras med windres i src/CMakeLists.txt och checkas aldrig in.
configure_file(version.rc.in ${CMAKE_BINARY_DIR}/generated/version/version.rc @ONLY)

add_library(kalkyl_version INTERFACE)
target_include_directories(kalkyl_version INTERFACE ${CMAKE_BINARY_DIR}/generated)
