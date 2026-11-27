# Kalkyls headless-tester (ctest), ett program per testfil. Tas med från rotens
# CMakeLists.txt när KALKYL_TESTS är på. Testerna ligger bara här, aldrig bland källkoden.

# kalkyl_add_test(namn fil bibliotek...): ett testprogram kalkyl_<namn>_test och ctest-testet <namn>.
# Varje test länkar biblioteken det provar och kalkyl_warnings.
function(kalkyl_add_test name file)
    add_executable(kalkyl_${name}_test ${file})
    target_link_libraries(kalkyl_${name}_test PRIVATE ${ARGN} kalkyl_warnings)
    add_test(NAME ${name} COMMAND kalkyl_${name}_test)
endfunction()

# Rena beräkningar och hjälpmedel.
kalkyl_add_test(conductor conductor_test.cpp kalkyl_conductor)
kalkyl_add_test(tabletext tabletext_test.cpp kalkyl_tabletext)
kalkyl_add_test(poletable poletable_test.cpp kalkyl_poletable kalkyl_tabletext)
kalkyl_add_test(ancalc ancalc_test.cpp kalkyl_ancalc)
kalkyl_add_test(automode automode_test.cpp kalkyl_automode kalkyl_poletable kalkyl_ancalc)
kalkyl_add_test(poleview poleview_test.cpp kalkyl_poleview kalkyl_automode kalkyl_poletable)
kalkyl_add_test(lineheight lineheight_test.cpp kalkyl_lineheight kalkyl_poleview kalkyl_automode kalkyl_poletable kalkyl_tabletext)
kalkyl_add_test(linecalc linecalc_test.cpp kalkyl_linecalc)
kalkyl_add_test(load_cases load_cases_test.cpp kalkyl_linecalc)
kalkyl_add_test(linecalc_facit linecalc_facit_test.cpp kalkyl_linecalc)
kalkyl_add_test(linecalc_excel linecalc_excel_test.cpp kalkyl_linecalc)

# Plattform och databaser.
kalkyl_add_test(platform platform_test.cpp kalkyl_platform)
kalkyl_add_test(association association_test.cpp kalkyl_platform)
kalkyl_add_test(db db_test.cpp kalkyl_db kalkyl_platform)
kalkyl_add_test(systemdb systemdb_test.cpp kalkyl_systemdb)
kalkyl_add_test(projectdb projectdb_test.cpp kalkyl_projectdb)

# Provprojektet som projectdb-testet lämnar kvar åt röktestet.
target_compile_definitions(kalkyl_projectdb_test PRIVATE KALKYL_PROV_DIR="${CMAKE_BINARY_DIR}/prov")

# Excelfiler och importen av stolptabellen.
kalkyl_add_test(xlsx xlsx_test.cpp kalkyl_xlsx kalkyl_miniz kalkyl_platform)
kalkyl_add_test(poleimport poleimport_test.cpp kalkyl_poleimport kalkyl_xlsx kalkyl_poletable kalkyl_automode kalkyl_tabletext)
kalkyl_add_test(poleimport_flow poleimport_flow_test.cpp kalkyl_poleimport kalkyl_xlsx kalkyl_miniz kalkyl_projectdb kalkyl_platform)
target_compile_definitions(kalkyl_poleimport_flow_test PRIVATE KALKYL_PROV_DIR="${CMAKE_BINARY_DIR}/prov")

# Excel-tabellen i GlazeUI (rena funktionerna, inget fönster).
kalkyl_add_test(sheetgrid sheetgrid_test.cpp kalkyl_sheetgrid kalkyl_tabletext)

# Stolptabellen i en DataGrid (cellLook, inget fönster).
kalkyl_add_test(polegrid polegrid_test.cpp kalkyl_polegrid kalkyl_automode kalkyl_poletable)

# Linhöjdsbladet i ett dolt fönster (svarar 77 utan skärm).
kalkyl_add_test(lineheightsheet lineheightsheet_test.cpp kalkyl_lineheightsheet kalkyl_lineheight)
target_compile_definitions(kalkyl_lineheightsheet_test PRIVATE KALKYL_PROV_DIR="${CMAKE_BINARY_DIR}/prov")
set_tests_properties(lineheightsheet PROPERTIES TIMEOUT 30 SKIP_RETURN_CODE 77)
