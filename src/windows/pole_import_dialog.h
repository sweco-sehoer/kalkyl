#pragma once

#include <glaze/glaze.h>
#include <glaze_threads/threads.h>

#include <array>
#include <optional>
#include <string>
#include <vector>

#include "autoMode/auto_mode.h"
#include "core/result.h"
#include "poleImport/pole_import.h"
#include "poleTable/import_record.h"
#include "poleTable/pole_table.h"
#include "systemdb/system_db.h"
#include "xlsx/xlsx.h"

// Importera stolptabell…: dialogen läser en stolptabell från Banken (.xlsx eller .xlsm) i bakgrunden,
// känner igen kolumnerna och visar dem med säkerhet och en rullista per fält. Användaren väljer
// Från och Till, ser en förhandsvisning och trycker Importera. Dialogen skriver ingenting i
// projektet: huvudfönstret hämtar raderna och rådata efteråt.
// Regionen GlazeBuilder ägs av editorn. Allt efter #pragma endregion är eget.
// Klassen ligger utanför namespace: GlazeBuilder läser bara okvalificerade fönsterklasser.
class PoleImportDialog : public glaze::AppWindow {
#pragma region GlazeBuilder
    // GENERERAD av GlazeBuilder — redigera i GlazeBuilder, inte för hand.
public:
    PoleImportDialog();

private:
    glaze::Label hintLbl;
    glaze::Label fileLbl;
    glaze::FilePicker fileBox;
    glaze::Label infoLbl;
    glaze::Label fieldsRubrik;
    glaze::DataGrid fieldGrid;
    glaze::Label rangeRubrik;
    glaze::Label fromLbl;
    glaze::Dropdown fromDrop;
    glaze::Label toLbl;
    glaze::Dropdown toDrop;
    glaze::Button allBtn;
    glaze::Label countLbl;
    glaze::Label previewRubrik;
    glaze::DataGrid previewGrid;
    glaze::Label issuesLbl;
    glaze::Button cancelBtn;
    glaze::Button okBtn;
    void initializeComponent();
#pragma endregion

public:
    // Systemdatabasen, för de inlärda rubrikerna och importmappen. Anropas före run(owner).
    void setSystemDb(kalkyl::systemdb::SystemDb& sys);

    // Filen som läses när dialogen öppnas. Anropas före run(owner).
    void setFile(const std::string& path);

    // Raderna i Stolptabell före och dess sträckor, till frågan om ersättning. Anropas före run(owner).
    void setCurrentTable(std::vector<kalkyl::poletable::PoleRow> rows,
                         std::vector<kalkyl::automode::NamedRange> ranges);

    // Efter run(owner) == OK: raderna att importera och deras rådata.
    const std::vector<kalkyl::poletable::PoleRow>& importedRows() const;
    const kalkyl::poletable::ImportRecord& importRecord() const;

    // Antalet ogiltiga tal som lämnades tomma i de importerade raderna.
    int invalidCount() const;

private:
    // Resultatet av läsningen i bakgrunden: boken, bladet som bestSheet valde och igenkänningen.
    // Vid fel är ok falskt och error har felet. Bara värden, så att arbetet aldrig rör en widget.
    struct LoadOutcome {
        bool ok = false;
        kalkyl::core::Error error;
        std::optional<kalkyl::xlsx::Workbook> book;
        int sheet = 0;
        kalkyl::poleimport::Recognition rec;
    };

    // Startlogik: sätter filfältet, släcker Importera, läser de inlärda rubrikerna och startar läsningen.
    void onLoad(glaze::Widget* sender, glaze::EventArgs& e);

    // Filfältet: en ny sökväg (via … eller skriven) startar en ny läsning.
    void fileBoxCommitted(glaze::Widget* sender, glaze::ChangeArgs& e);

    // En ny kolumn valdes för ett fält i rullistan: fältet blir Ditt val, och utdraget, Från och Till
    // och förhandsvisningen räknas om.
    void fieldGridCommitted(glaze::Widget* sender, glaze::CellChangeArgs& e);

    // Från eller Till ändrades: förhandsvisningen, problemen och knappens läge räknas om.
    void rangeChanged(glaze::Widget* sender, glaze::ChangeArgs& e);

    // Alla stolpar: Från blir första och Till sista stolpen i filen.
    void allBtnClicked(glaze::Widget* sender, glaze::ClickArgs& e);

    // Importera: bygger resultatet, frågar när Stolptabell före har rader, lär in valda rubriker,
    // sparar importmappen och stänger med OK. Ett Nej i frågan lämnar dialogen öppen.
    void okBtnClicked(glaze::Widget* sender, glaze::ClickArgs& e);

    // Läser filen i bakgrunden (öppna, bestSheet och igenkänning). Under läsningen visas "Läser …",
    // väntepekaren är på och Importera är släckt. Ett nytt anrop avbryter det förra.
    void startLoad(const std::string& path);

    // Gör bladet index till det aktuella: igenkänningen, kolumnerna, utdraget, Från och Till och
    // förhandsvisningen. Bladet är det som bestSheet valde.
    void applySheet(int index);

    // Fyller rutnätet Kolumner med en rad per fält: namn, vald kolumn med rullista, säkerhet och
    // färg. Rullistornas poster sätts per cell efter setRows.
    void fillFields();

    // Fyller Från och Till med stolparna i filen. Giltiga val behålls, annars väljs första och sista.
    void fillRange();

    // Räknar om förhandsvisningen, antalet, Första värdet, problemen och Importera efter Från och Till.
    void refreshPreview();

    // Bygger resultatet för Från till Till: raderna, rådata och antalet ogiltiga tal.
    void buildResult();

    // Säkerheten som visas för fältet: Ditt val när användaren valt kolumnen, annars igenkänningens.
    kalkyl::poleimport::Confidence confidenceOf(int field) const;

    // Skriver säkerhetscellen för fältet: text, färg och tooltip.
    void paintConfidence(int field);

    // Tooltipen för säkerhetscellen: poäng och skäl, näst bästa kolumn, de tvetydiga kandidaterna
    // eller vad som saknas.
    std::string confidenceTip(int field) const;

    // Texten i rullistan för fältets nuvarande kolumn: "Välj kolumn…", "– (hämtas inte)" eller kolumnens rubrik.
    std::string choiceText(int field) const;

    // Bladet som är aktuellt, eller nullptr utan bok.
    const kalkyl::xlsx::Sheet* sheet() const;

    // Rubriken för kolumnen som "{L} · {label}" eller "{L} · (ingen rubrik)".
    std::string columnTitle(int column) const;

    kalkyl::systemdb::SystemDb* sys_ = nullptr;
    std::string path_;                                        // filen som begärdes senast
    std::string loadedPath_;                                  // filen som lästes utan fel, "" annars
    std::vector<kalkyl::poletable::PoleRow> currentRows_;     // Stolptabell före nu
    std::vector<kalkyl::automode::NamedRange> ranges_;        // sträckorna i före
    std::optional<kalkyl::xlsx::Workbook> book_;              // den inlästa boken
    int sheetIndex_ = 0;                                      // bladet som bestSheet valde
    kalkyl::poleimport::Learned learned_;                     // inlärda rubriker per fält
    kalkyl::poleimport::Recognition rec_;                     // igenkänningen av bladet
    kalkyl::poleimport::Mapping mapping_{};                   // vald kolumn per fält
    std::array<bool, kalkyl::poletable::kColumnCount> chosen_{};   // fält som användaren valt själv
    std::array<std::vector<int>, kalkyl::poletable::kColumnCount> choiceColumns_;   // kolumnen per post i varje rullista
    kalkyl::poleimport::Extraction ex_;                       // stolparna med de valda kolumnerna
    int first_ = 0;                                           // första importerade stolpen (index i ex_.rows)
    int last_ = -1;                                           // sista importerade stolpen
    std::vector<kalkyl::poletable::PoleRow> rows_;            // resultatet: raderna att importera
    kalkyl::poletable::ImportRecord record_;                  // resultatet: rådata
    int invalid_ = 0;                                         // ogiltiga tal i resultatet
    glaze::threads::Worker loadWorker_;                       // sist, så att den rivs först
};
