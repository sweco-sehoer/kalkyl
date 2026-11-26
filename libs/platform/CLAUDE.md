# libs/platform

Win32-nära hjälpmedel. Biblioteket heter `kalkyl_platform` (STATIC) och länkar `shell32`, `ole32`,
`advapi32` och `uuid`. Alla Win32-anrop sker med W-varianterna, och sökvägar är UTF-8 utåt.

| Fil | Innehåll |
|---|---|
| `CMakeLists.txt` | Biblioteket (testerna `platform` och `association` ligger i `tests/`) |
| `text.h`, `text.cpp` | UTF-8 och UTF-16 (`toWide`, `toUtf8`) samt sökvägar via `std::u8string` |
| `system.h`, `system.cpp` | Kommandoradsargument, exe-sökväg, datamapp (`KALKYL_DATA_DIR`), användare, dator och tid |
| `files.h`, `files.cpp` | Sökvägssträngar utan I/O, `probe` med tidsgräns, kopiering med förlopp, atomära namnbyten, `readFileShared` |
| `lock_file.h`, `lock_file.cpp` | `LockFile`: låsfilen som visar att ett projekt är öppet |
| `association.h`, `association.cpp` | Filassociation under `HKCU\Software\Classes` |
- Strängfunktionerna (`absolutePath`, `pathKey`, `parentDir`, `fileName`, `fileStem`) gör ingen I/O och är
  säkra på UI-tråden även för nätverkssökvägar.
- `probe` och `probeMany` frågar i frikopplade trådar och väntar högst tidsgränsen, så att en server som
  inte svarar aldrig hänger anroparen.
- Filfunktionerna returnerar `core::Result` med svensk text och sökväg. `win32Error` översätter Win32-koder
  till `ErrorCode`.
- `LockFile` skapas med `CREATE_NEW`, `FILE_SHARE_READ` och `FILE_FLAG_DELETE_ON_CLOSE` och hålls öppen. Filen
  försvinner när handtaget stängs, också vid krasch. `releaseInBackground` släpper låset i en frikopplad tråd
  som inte rör glaze. `removeStale` skiljer på ett levande lås (`Locked`) och nekad borttagning (`AccessDenied`).
- `removeFile` tar bort skrivskyddet och försöker igen när en skrivskyddad fil nekas. `clearReadOnly` och
  `setHidden(path, false)` ändrar bara det attribut de gäller.
- `readFileShared(path, maxBytes)` läser hela filen i minnet med `GENERIC_READ` och `FILE_SHARE_READ`, `WRITE` och
  `DELETE`, så att en fil som är öppen i Excel går att läsa. En fil över `maxBytes` ger `Invalid`, övriga fel går
  genom `win32Error` (`NotFound`, `AccessDenied`, `Busy`, `Unreachable`, `Io`).
- `unregisterAssociation` med exe-sökväg lämnar en registrering som pekar på en annan befintlig kopia orörd.
- `windows.h` har en global funktion `LockFile` som krockar med klassen vid `using namespace kalkyl::platform`.
  Kvalificera namnet (`kalkyl::platform::LockFile`) eller kringgå krocken som `tests/platform_test.cpp` gör.
