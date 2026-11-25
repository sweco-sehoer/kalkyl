# Systemdatabasen: utseendeläge, senaste mapp, senaste projekt, automatisk sparning och linregistret.
add_library(kalkyl_systemdb STATIC system_db.cpp)
target_include_directories(kalkyl_systemdb PUBLIC ${PROJECT_SOURCE_DIR}/libs)
target_link_libraries(kalkyl_systemdb PUBLIC kalkyl_db kalkyl_platform kalkyl_conductor PRIVATE kalkyl_warnings)
