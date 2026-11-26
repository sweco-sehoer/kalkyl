# Stödbibliotek. Ordningen följer beroendena:
# conductor och tableText ← anCalc, lineCalc och poleTable; core ← platform ← db ← systemdb/projectdb ← appearance.
# GUI-biblioteken (appearance, sheetGrid) står sist.
# Ett nytt bibliotek läggs till här med en egen add_subdirectory-rad.
add_subdirectory(version)
add_subdirectory(core)
add_subdirectory(conductor)
add_subdirectory(tableText)
add_subdirectory(poleTable)
add_subdirectory(anCalc)
add_subdirectory(lineCalc)
add_subdirectory(platform)
add_subdirectory(db)
add_subdirectory(systemdb)
add_subdirectory(projectdb)
add_subdirectory(appearance)
add_subdirectory(sheetGrid)
