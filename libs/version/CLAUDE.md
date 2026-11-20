# libs/version

Programmets version som header och som versionsresurs för kalkyl.exe. Biblioteket heter `kalkyl_version` (INTERFACE).

| Fil | Innehåll |
|---|---|
| `CMakeLists.txt` | Genererar `version/version.h` och `version.rc` i byggkatalogen och exporterar headern |
| `version.h.in` | Mall för `version.h`: `KALKYL_VERSION` m.fl., ifyllda från `project(VERSION)` |
| `version.rc.in` | Mall för `version.rc`: utgivare (`KALKYL_PUBLISHER`), produkt och version. `src/CMakeLists.txt` kompilerar den med windres |
