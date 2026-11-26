cmake_minimum_required(VERSION 3.25)

# Projektets version (major.minor.patch) står bara här.
# Den skrivs in i version.h vid konfigurering, se libs/version/CMakeLists.txt.
project(Kalkyl
        VERSION 0.5.0
        DESCRIPTION "Luftledningsberäkningar"
        LANGUAGES C CXX)

# Utgivaren i exe-filernas versionsinformation och i installationens Uninstall-nyckel.
set(KALKYL_PUBLISHER "SWECO Sverige AB, Erik Holmbom")

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Statisk länkning av alla program (C++-runtime, winpthread, SQLite): kalkyl.exe går
# att köra utan MSYS2 i PATH, och Git:s egna runtime-DLL:er i PATH kan inte laddas av misstag.
add_link_options(-static)

# GlazeUI hämtas på en fast tagg, aldrig main. SYSTEM gör att GlazeUI:s headers
# inte ger varningar i våra mål.
# Lokal utcheckning: -DFETCHCONTENT_SOURCE_DIR_GLAZEUI=<sökväg>.
include(FetchContent)
FetchContent_Declare(GlazeUI
    GIT_REPOSITORY https://github.com/holmbom/glazeui.git
    GIT_TAG        v0.11.1
    GIT_SHALLOW    TRUE
    SYSTEM)
FetchContent_MakeAvailable(GlazeUI)

# miniz (zip) hämtas på en fast tagg, aldrig master. Bara källorna hämtas (SOURCE_SUBDIR pekar på
# en katalog som inte finns): libs/xlsx bygger dem som kalkyl_miniz, eftersom minizs egen
# CMakeLists ändrar CMAKE_C_FLAGS och kräver CMake 3.5. Lokal utcheckning: -DFETCHCONTENT_SOURCE_DIR_MINIZ=<sökväg>.
FetchContent_Declare(miniz
    GIT_REPOSITORY https://github.com/richgel999/miniz.git
    GIT_TAG        3.1.2
    GIT_SHALLOW    TRUE
    SOURCE_SUBDIR  kalkyl-bara-kallor)
FetchContent_MakeAvailable(miniz)

# Headless-testerna (ctest). Alla tester ligger i tests/, aldrig bland källkoden.
option(KALKYL_TESTS "Bygg Kalkyls headless-tester (ctest)" OFF)
if(KALKYL_TESTS)
    enable_testing()
endif()

# Varningar för projektets egna mål. Länkas PRIVATE av varje eget mål.
add_library(kalkyl_warnings INTERFACE)
target_compile_options(kalkyl_warnings INTERFACE -Wall -Wextra -Wno-missing-field-initializers)

# Modulerna först, sedan programmet som länkar dem och sist installationen.
add_subdirectory(libs)
add_subdirectory(src)
add_subdirectory(installer)
if(KALKYL_TESTS)
    add_subdirectory(tests)
endif()
