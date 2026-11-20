# Stödbibliotek, en katalog per bibliotek. Ordningen följer beroendena:
# core ← platform ← db ← systemdb/projectdb ← appearance.
# Ett nytt bibliotek läggs till här med en egen add_subdirectory-rad.
add_subdirectory(version)
add_subdirectory(core)
add_subdirectory(platform)
add_subdirectory(db)
add_subdirectory(systemdb)
add_subdirectory(projectdb)
add_subdirectory(appearance)
