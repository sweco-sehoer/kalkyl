#pragma once

#include <glaze/glaze.h>
#include <glaze_threads/threads.h>

#include <array>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "anCalc/normal_span.h"
#include "conductor/conductor.h"
#include "lineCalc/line_calc.h"
#include "poleTable/pole_table.h"
#include "projectdb/project_data.h"
#include "projectdb/project_db.h"
#include "sheetGrid/sheet_grid.h"
#include "systemdb/system_db.h"

// Huvudfönstret: programikon (stor först med verktygsfält), menyer och lägesknapp i namnlisten,
// arbetsyta och statusrad. Titeln visar projektnamnet och statusraden projektets sökväg.
// Regionen GlazeBuilder ägs av editorn. Allt efter #pragma endregion är eget.
// Klassen ligger utanför namespace: GlazeBuilder läser bara okvalificerade fönsterklasser.
class MainWindow : public glaze::AppWindow {
#pragma region GlazeBuilder
    // GENERERAD av GlazeBuilder — redigera i GlazeBuilder, inte för hand.
public:
    MainWindow();

private:
    glaze::Timer autosaveTimer;
    glaze::ToolButton themeBtn;
    glaze::TabControl flikar;
    glaze::TabPage byggPmSida;
    glaze::TabPage stolpSida;
    glaze::TabPage nsSida;
    glaze::TabPage lcSida;
    glaze::Panel byggPmPanel;
    glaze::Label byggPmRubrik;
    glaze::Label byggPmBla;
    glaze::Label byggPmGron;
    glaze::Label byggPmGul;
    glaze::Label byggPmFore;
    glaze::Label byggPmLila;
    glaze::Label byggPmEfter;
    glaze::Label byggPmRosa;
    glaze::DataGrid byggPmGrid;
    glaze::Panel stolpPanel;
    glaze::Label stolpRubrik;
    glaze::Label stolpInfo;
    glaze::DataGrid stolpGrid;
    glaze::Panel nsPanel;
    glaze::Panel lcPanel;
    glaze::Label nsRubrik;
    glaze::Button nsNyBtn;
    glaze::Button nsAndraBtn;
    glaze::Button nsKopieraBtn;
    glaze::Button nsTaBortBtn;
    glaze::ListView nsLista;
    glaze::Label lcRubrik;
    glaze::Button lcNyBtn;
    glaze::Button lcAndraBtn;
    glaze::Button lcKopieraBtn;
    glaze::Button lcTaBortBtn;
    glaze::ListView lcLista;
    void initializeComponent();
#pragma endregion

public:
    // Vad main gör när run() har returnerat.
    // Exit avslutar programmet, CloseProject går tillbaka till väljaren.
    enum class Outcome { Exit, CloseProject };

    // Anger systemdatabasen och det öppna projektet. Anropas före run().
    // Objekten ägs av main och måste leva längre än fönstret.
    void setContext(kalkyl::systemdb::SystemDb& sys, kalkyl::projectdb::ProjectDb& project);

    // Hur fönstret stängdes. Krysset och Arkiv > Avsluta ger Exit.
    // Arkiv > Stäng projekt ger CloseProject.
    Outcome outcome() const;

private:
    // Varför publiceringen startades: Ctrl+S, den automatiska sparningen eller en stängning
    // som användaren svarat Ja på. Valet styr hur fel och utfall visas.
    enum class SaveKind { Manual, Auto, Closing };

    // Hur formuläret Normalspannsberäkning öppnas: en ny beräkning, en befintlig eller en kopia.
    enum class NsOpen { New, Existing, Copy };

    // Hur formuläret Linberäkning öppnas: en ny lös manuell beräkning, en ny manuell för en rad,
    // en befintlig eller en kopia. En kopplad beräkning skapas utan formulär (addLinkedLineCalc).
    enum class LcOpen { New, NewManualForRow, Existing, Copy };

    // Startlogik: utseende, titel med projektnamn, projektets uppgifter, menyernas handlers,
    // listorna och den automatiska sparningen.
    void onLoad(glaze::Widget* sender, glaze::EventArgs& e);

    // Namnlistens lägesknapp: växlar System -> Mörkt -> Ljust -> System.
    // Läget sparas i systemdatabasen.
    void themeBtnClicked(glaze::Widget* sender, glaze::ClickArgs& e);

    // Arkiv > Stäng projekt: stänger fönstret och går tillbaka till väljaren.
    // Själva stängningen av projektet gör main.
    void closeProjectClicked();

    // Arkiv > Avsluta: stänger fönstret och avslutar programmet.
    // Samma resultat som krysset.
    void exitClicked();

    // Hjälp > Om Kalkyl: visar namn och version.
    // Rutan är GlazeUI:s egen meddelanderuta.
    void aboutClicked();

    // Inställningar > Lindata…: öppnar linregistret som en modal dialog.
    // Rullistorna och varningarna i Dokumentation räknas om efteråt. Projektets kopior ändras inte.
    void lindataClicked();

    // Arkiv > Spara (Ctrl+S): publicerar projektet till projektfilen i bakgrunden.
    // Gör inget skrivskyddat. Visar "Sparning pågår…" eller "Inget att spara." när det passar.
    void saveClicked();

    // Den automatiska sparningens tick: sparar bara när projektet är ändrat och inget annat
    // hindrar. Fel visas bara i statusraden.
    void autosaveTimerTick(glaze::Timer* sender, glaze::TimerArgs& e);

    // Inställningar > Automatisk sparning: sparar valet i systemdatabasen och startar om timern.
    // Ett fel visas i statusraden, och valet gäller ändå under sessionen.
    void autosaveChosen(int minutes);

    // Fönstret stängs (krysset, Stäng projekt, Avsluta). Ändrat projekt ger frågan Ja/Nej/Avbryt,
    // och en pågående sparning väntas in innan fönstret stängs.
    void onClosing(glaze::Widget* sender, glaze::CloseArgs& e);

    // En cell i Dokumentation sparades. Linval går via chooseConductor, andra celler sparas som
    // text i projektet. Ett fel ger en meddelanderuta och cellen får tillbaka sitt värde.
    void byggPmGridCommitted(glaze::Widget* sender, glaze::CellChangeArgs& e);

    // Högerklick i Dokumentation: meny för linraderna (uppdatera från linregistret) och för de
    // sex normalspannsraderna och de sex linberäkningsraderna (lägg till, koppla, ändra, ta bort).
    // Tom meny ger ingen meny.
    void byggPmGridContextMenu(glaze::Widget* sender, glaze::ContextMenuArgs& e);

    // Ny beräkning…: öppnar formuläret för en lös beräkning, utan koppling till en rad i
    // Dokumentation. Kopplingar görs bara i Dokumentations högerklicksmeny.
    void nsNyBtnClicked(glaze::Widget* sender, glaze::ClickArgs& e);

    // Ändra… (Visa… skrivskyddat): öppnar den valda beräkningen i formuläret.
    // Gör ingenting utan vald rad.
    void nsAndraBtnClicked(glaze::Widget* sender, glaze::ClickArgs& e);

    // Kopiera…: öppnar formuläret med en kopia av den valda beräkningen, som en ny lös beräkning.
    // Originalet och dess kopplingar rörs inte.
    void nsKopieraBtnClicked(glaze::Widget* sender, glaze::ClickArgs& e);

    // Ta bort: frågar och tar sedan bort den valda beräkningen med alla dess kopplingar.
    // Frågan listar raderna i Dokumentation som visar den.
    void nsTaBortBtnClicked(glaze::Widget* sender, glaze::ClickArgs& e);

    // Dubbelklick eller Enter i listan: öppnar den valda beräkningen.
    void nsListaActivated(glaze::Widget* sender, glaze::ActivateArgs& e);

    // Valet i listan ändrades: knapparna aktiveras bara när en rad är vald.
    void nsListaChanged(glaze::Widget* sender, glaze::ChangeArgs& e);

    // Högerklick i listan: Ändra (eller Visa), Kopiera och Ta bort för raden under pekaren.
    void nsListaContextMenu(glaze::Widget* sender, glaze::ContextMenuArgs& e);

    // Ny beräkning…: öppnar formuläret Linberäkning för en lös, manuell beräkning. Kopplingar till
    // en rad görs bara i Dokumentations högerklicksmeny.
    void lcNyBtnClicked(glaze::Widget* sender, glaze::ClickArgs& e);

    // Ändra… (Visa… skrivskyddat): öppnar den valda linberäkningen i formuläret.
    // Gör ingenting utan vald rad.
    void lcAndraBtnClicked(glaze::Widget* sender, glaze::ClickArgs& e);

    // Kopiera…: öppnar formuläret med en kopia av den valda beräkningen, som en ny lös och manuell
    // beräkning. Originalet och dess kopplingar rörs inte.
    void lcKopieraBtnClicked(glaze::Widget* sender, glaze::ClickArgs& e);

    // Ta bort: frågar och tar sedan bort den valda linberäkningen med alla dess kopplingar.
    void lcTaBortBtnClicked(glaze::Widget* sender, glaze::ClickArgs& e);

    // Dubbelklick eller Enter i listan: öppnar den valda linberäkningen.
    void lcListaActivated(glaze::Widget* sender, glaze::ActivateArgs& e);

    // Valet i listan ändrades: knapparna aktiveras bara när en rad är vald.
    void lcListaChanged(glaze::Widget* sender, glaze::ChangeArgs& e);

    // Högerklick i listan: Ändra (eller Visa), Kopiera och Ta bort för raden under pekaren.
    void lcListaContextMenu(glaze::Widget* sender, glaze::ContextMenuArgs& e);

    // Läser projektets uppgifter: Dokumentationens värden och referenser (med förval för det som
    // saknas) och projektets linkopior. Ett fel ger en meddelanderuta och förvalen.
    void loadProjectData();

    // Ett nytt projekt får Dokumentationens förval (kNewProjectDefaults) skrivna i projektet:
    // linor ur linregistret, konfiguration, uppspänningar och temperaturer. Ett förval som inte
    // går att skriva loggas och hoppas över. Befintliga projekt får aldrig förvalen.
    void applyNewProjectDefaults();

    // Sparar den ändrade cellen för raden def i kByggPmRows (kolumn 1 eller 2) i projektet.
    // Svarar om det lyckades. Vid fel visas en meddelanderuta och cellen återställs.
    bool saveDocValue(std::size_t def, int column);

    // Väljer linan name på linraden conductorRow (plats i kConductorRows): projektets kopia
    // återanvänds, annars kopieras registrets lina in. Svarar om det lyckades.
    bool chooseConductor(std::size_t conductorRow, const std::string& name);

    // Läser linregistret och ger linraderna rullistor, tooltips och varningsbilder efter
    // tillståndet: lika, avvikande kopia, lina som saknas i registret eller tomt register.
    void fillConductorChoices();

    // Läser om alla normalspannsberäkningar ur projektet, räknar dem och uppdaterar raderna i
    // Dokumentation och listan i fliken Normalspann.
    void refreshNormalSpans();

    // Värde-cellen på de sex normalspannsraderna: aₙ med Σ-bild när en beräkning är kopplad,
    // annars det manuella värdet eller "-". Tooltipen säger vad som gäller.
    void showNormalSpanRows();

    // Bygger raderna i listan i fliken Normalspann ur normalSpans_ och nsCalcs_, med gul växlande
    // radton. Den tidigare valda beräkningen väljs igen.
    void fillNormalSpanList();

    // Aktiverar knapparna i fliken Normalspann efter valet och skrivskyddat läge, och ger listan
    // en tooltip om den valda beräkningen (avvikelser per kontroll och var den visas).
    void updateNsButtons();

    // Öppnar formuläret modalt. rowKey kopplar en ny beräkning till en rad i Dokumentation när
    // användaren trycker OK. Läser om allt efteråt.
    void openNormalSpanDialog(NsOpen mode, std::int64_t id, const std::string& rowKey);

    // Frågar Ja/Nej och tar bort beräkningen med alla dess kopplingar. Frågan listar raderna i
    // Dokumentation som visar den.
    void removeNormalSpanAsked(std::int64_t id);

    // Läser om alla linberäkningar och Dokumentationens uppgifter, löser upp och räknar varje
    // beräkning och uppdaterar raderna i Dokumentation och listan i fliken Linberäkning.
    void refreshLineCalcs();

    // Värde-cellen på de sex linberäkningsraderna: stakningsmallen med Σ-bild när en beräkning är
    // kopplad, annars det manuella värdet eller "-". Tooltipen säger vad som gäller.
    void showLineCalcRows();

    // Bygger raderna i listan i fliken Linberäkning ur lineCalcs_, lcResolved_ och lcCalcs_, med gul
    // växlande radton. Den tidigare valda beräkningen väljs igen.
    void fillLineCalcList();

    // Aktiverar knapparna i fliken Linberäkning efter valet och skrivskyddat läge, och ger listan
    // en tooltip om den valda beräkningen (första upplysningen eller mallen, och var den visas).
    void updateLcButtons();

    // Öppnar formuläret Linberäkning modalt. rowKey kopplar en ny beräkning till en rad i
    // Dokumentation när användaren trycker OK. Läser om allt efteråt.
    void openLineCalcDialog(LcOpen mode, std::int64_t id, const std::string& rowKey);

    // Lägg till på en linberäkningsrad: skapar en kopplad beräkning med förvalen och indata ur
    // Dokumentation, utan formulär, så att raden visar mallen direkt. Ändra… öppnar den.
    void addLinkedLineCalc(const std::string& rowKey);

    // Frågar Ja/Nej och tar bort linberäkningen med alla dess kopplingar. Frågan listar raderna i
    // Dokumentation som visar den.
    void removeLineCalcAsked(std::int64_t id);

    // Efter en ändring i projektet: läser om uppgifterna och beräkningarna, uppdaterar
    // rullistorna och titeln. Fel visas av funktionerna som anropas.
    void reloadAfterChange();

    // Nyckel och radtext för de sex normalspannsraderna, till formuläret. Texten följer läget
    // (Samma eller egna topplinor).
    std::vector<std::pair<std::string, std::string>> docRowLabels() const;

    // Radens korta namn i listan, t.ex. "Fas före" eller "Topp h. efter (dold)".
    std::string shortDocLabel(const std::string& key) const;

    // Radens hela text i Dokumentation, t.ex. "Faslina normalspannsberäkning före ombyggnad".
    std::string fullDocLabel(const std::string& key) const;

    // De rader som visar beräkningen, med korta namn skilda med komma och i radordning.
    // Utan rader blir svaret "–". Används i listan och i menyn Koppla befintlig.
    std::string shortRowsText(const kalkyl::projectdb::NormalSpan& ns) const;

    // Platsen i normalSpans_ för beräkningen som raden key visar, eller -1.
    int linkedSpanIndex(const std::string& key) const;

    // Nyckel och radtext för de sex linberäkningsraderna, till formuläret. Texten följer läget.
    std::vector<std::pair<std::string, std::string>> lineCalcRowLabels() const;

    // De rader som visar linberäkningen, med korta namn skilda med komma och i radordning.
    // Utan rader blir svaret "–".
    std::string shortRowsText(const kalkyl::projectdb::LineCalc& lc) const;

    // Platsen i lineCalcs_ för beräkningen som linberäkningsraden key visar, eller -1.
    int linkedLineCalcIndex(const std::string& key) const;

    // Texten i undermenyn Koppla befintlig för beräkningen på plats index i lineCalcs_:
    // lina, aₙ, σ₀, mall och raderna den visas på.
    std::string lineCalcSummary(std::size_t index) const;

    // Startar publiceringen: bilden tas på UI-tråden och publiceras i en Worker. Utfallet tas
    // emot på UI-tråden och visas efter kind.
    void startSave(SaveKind kind);

    // Startar om (eller stoppar) den automatiska sparningen efter inställningen, skrivskyddat
    // läge och en eventuell konflikt.
    void restartAutosave();

    // Titeln (med * när projektet är ändrat) och sparpanelen i statusraden.
    void updateTitle();

    // Sant när projektet är öppnat skrivskyddat.
    bool readOnly() const;

    // En cell i Stolptabell före sparades. Ogiltigt tal i en talkolumn återställs tyst, annars
    // skrivs fältet om i visningsform och tabellen sparas. Skriver man i den tomma raden sist
    // läggs en ny stolpe till.
    void stolpGridCommitted(glaze::Widget* sender, glaze::CellChangeArgs& e);

    // Ett klick på radera-knappen i Stolptabell före. Frågan postas, så att tabellen inte ändras
    // mitt i sin egen händelse.
    void stolpGridCellClicked(glaze::Widget* sender, glaze::CellClickArgs& e);

    // Tangenter, musen och högerklick i Stolptabell före går vidare till Excel-markeringen.
    void stolpGridKeyDown(glaze::Widget* sender, glaze::KeyArgs& e);
    void stolpGridMouseUp(glaze::Widget* sender, glaze::MouseArgs& e);
    void stolpGridContextMenu(glaze::Widget* sender, glaze::ContextMenuArgs& e);

    // Adaptern mellan stolptabellen och SheetGrid: vad en cell kopieras som (poletable::copyText),
    // radtonen, inklistrade ändringar och snabbmenyns egna poster (Infoga rad ovanför, Radera rader…).
    class PoleSheetHost : public kalkyl::sheetgrid::SheetHost {
    public:
        explicit PoleSheetHost(MainWindow& window) : window_(window) {}
        std::string copyText(int row, int column) const override;
        std::optional<gfx::Color> baseColor(int row, int column) const override;
        void applyEdits(const std::vector<kalkyl::sheetgrid::CellEdit>& edits) override;
        void addMenuItems(glaze::Menu& menu, const kalkyl::sheetgrid::CellRect& selection) override;
        void showStatus(const std::string& text) override;

    private:
        MainWindow& window_;
    };

    // Läser stolptabellen ur projektet till poleRows_ och poleSaved_. Ett fel ger en
    // meddelanderuta och en tom tabell.
    void loadPoleTable();

    // Bygger om hela tabellen ur poleRows_ med den tomma raden sist, ger raderna knappar och
    // ton och sätter tillbaka den aktuella cellen, som setRows nollar.
    void fillPoleGrid();

    // Radera-kolumnen får radtonen (kolumn 0-7 målas av SheetGrid via baseColor). En datarad i
    // ett skrivbart projekt får en radera-knapp med papperskorg, annars är cellen låst.
    void decoratePoleRow(int row);

    // Radens gula ton: varannan rad en aning mörkare, som temperaturraderna i Dokumentation.
    gfx::Color poleTone(int row) const;

    // Skriver inklistrade och raderade celler i poleRows_ (nya rader läggs till), bygger om
    // tabellen och sparar.
    void applyPoleEdits(const std::vector<kalkyl::sheetgrid::CellEdit>& edits);

    // Skriver hela stolptabellen i projektets arbetskopia om den skiljer sig från det senast
    // sparade. Vid fel visas en ruta och tabellen läses om. Svarar om det lyckades.
    bool savePoleTable();

    // Infogar en tom rad i poleRows_ vid row, markerar den och sparar.
    void insertPoleRowAbove(int row);

    // Frågar "Radera stolpplats …?" och tar vid Ja bort raderna first till last (dataraderna).
    void removePoleRowsAsked(int first, int last);

    // Visar en kort text i statusradens panel för tabellens meddelanden.
    void setPoleStatus(const std::string& text);

    // Bygger raderna i Dokumentation efter läget. Är Topplina höger "Samma" döljs höger-raderna
    // och vänster-raderna heter som en topplina. Värdena läses ur byggPmValues_.
    void rebuildByggPm();

    kalkyl::systemdb::SystemDb*   sys_ = nullptr;
    kalkyl::projectdb::ProjectDb* project_ = nullptr;
    Outcome outcome_ = Outcome::Exit;
    std::vector<kalkyl::conductor::Conductor> conductors_;           // linregistret som det lästes senast
    std::vector<kalkyl::conductor::Conductor> projectConductors_;    // projektets kopior av linorna
    std::vector<std::array<std::string, 2>> byggPmValues_;           // Värde och Referens per rad i kByggPmRows
    std::vector<std::size_t> byggPmShown_;                           // synlig rad -> plats i kByggPmRows
    bool byggPmSplit_ = false;                                       // topplina höger är en egen lina
    std::vector<kalkyl::projectdb::NormalSpan> normalSpans_;         // beräkningarna i projektet
    std::vector<kalkyl::ancalc::SpanCalculation> nsCalcs_;             // resultat per beräkning, samma ordning
    std::vector<std::int64_t> nsListIds_;                            // beräkningens id per rad i listan
    glaze::Menu byggPmMenu_;                                         // snabbmenyn i Dokumentation
    glaze::Menu nsMenu_;                                             // snabbmenyn i listan
    std::vector<kalkyl::projectdb::LineCalc> lineCalcs_;             // linberäkningarna i projektet
    std::vector<kalkyl::projectdb::ResolvedLineInput> lcResolved_;   // upplösta indata, samma ordning
    std::vector<kalkyl::linecalc::LineCalculation> lcCalcs_;         // resultat per beräkning, samma ordning
    std::vector<std::int64_t> lcListIds_;                            // linberäkningens id per rad i listan
    glaze::Menu lcMenu_;                                             // snabbmenyn i listan Linberäkning
    bool saving_ = false;                                            // en publicering pågår
    bool closeAfterSave_ = false;                                    // stäng fönstret när sparningen är klar
    bool closingConfirmed_ = false;                                  // stängningen är bekräftad, ingen fråga
    bool autosaveBlocked_ = false;                                   // avstängd efter en konflikt
    int autosaveMinutes_ = 5;                                        // inställningen, 0 = av
    int savePanel_ = -1;                                             // sparpanelens plats i statusraden
    std::string saveStatus_;                                         // senaste sparutfallet
    bool saveStatusIsError_ = false;                                 // utfallet var ett fel
    std::vector<kalkyl::poletable::PoleRow> poleRows_;               // stolptabellens rader, utan den tomma raden sist
    std::vector<kalkyl::poletable::PoleRow> poleSaved_;              // det senast sparade
    std::unique_ptr<PoleSheetHost> poleHost_;                        // adaptern mot SheetGrid
    std::unique_ptr<kalkyl::sheetgrid::SheetGrid> poleSheet_;        // Excel-markeringen i stolpGrid
    int gridStatusPanel_ = -1;                                       // statusradens panel för tabellens meddelanden
    std::shared_ptr<bool> alive_ = std::make_shared<bool>(true);     // spärr för postade anrop
    glaze::threads::Worker saveWorker_;                              // sist, så att den rivs först
};
