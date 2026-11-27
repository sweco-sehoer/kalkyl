# Stödbibliotek. Ordningen följer beroendena:
# conductor och tableText ← anCalc, lineCalc och poleTable; poleTable + anCalc ← autoMode ← projectdb;
# core ← platform ← db ← systemdb/projectdb ← appearance; platform ← xlsx ← poleImport (med poleTable och autoMode).
# autoMode ← poleView ← poleGrid. GUI-biblioteken (appearance, sheetGrid, poleGrid) står sist.
# Ett nytt bibliotek läggs till här med en egen add_subdirectory-rad.
add_subdirectory(version)
add_subdirectory(core)
add_subdirectory(conductor)
add_subdirectory(tableText)
add_subdirectory(poleTable)
add_subdirectory(anCalc)
add_subdirectory(autoMode)
add_subdirectory(poleView)
add_subdirectory(lineCalc)
add_subdirectory(platform)
add_subdirectory(xlsx)
add_subdirectory(poleImport)
add_subdirectory(db)
add_subdirectory(systemdb)
add_subdirectory(projectdb)
add_subdirectory(appearance)
add_subdirectory(sheetGrid)
add_subdirectory(poleGrid)
