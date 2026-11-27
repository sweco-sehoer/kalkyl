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
#include "autoMode/auto_mode.h"
#include "conductor/conductor.h"
#include "lineCalc/line_calc.h"
#include "lineHeight/line_height.h"
#include "lineHeightSheet/line_height_sheet.h"
#include "poleGrid/pole_grid.h"
#include "poleTable/pole_table.h"
#include "poleView/pole_view.h"
#include "projectdb/project_data.h"
#include "projectdb/project_db.h"
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
    glaze::TabPage efterSida;
    glaze::TabPage linSida;
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
    glaze::Panel efterPanel;
    glaze::Label stolpRubrik;
    glaze::Button stolpImportBtn;
    glaze::Label stolpInfo;
    glaze::DataGrid stolpGrid;
    glaze::GroupBox stolpAutoGrp;
    glaze::Checkbox stolpAutoChk;
    glaze::Label stolpFranLbl;
    glaze::Label stolpTillLbl;
    glaze::Label stolpFasLbl;
    glaze::Dropdown stolpFasFran;
    glaze::Dropdown stolpFasTill;
    glaze::Label stolpFasStatus;
    glaze::Label stolpToppVLbl;
    glaze::Dropdown stolpToppVFran;
    glaze::Dropdown stolpToppVTill;
    glaze::Label stolpToppVStatus;
    glaze::Label stolpToppHLbl;
    glaze::Dropdown stolpToppHFran;
    glaze::Dropdown stolpToppHTill;
    glaze::Label stolpToppHStatus;
    glaze::Label efterRubrik;
    glaze::Button efterKopieraBtn;
    glaze::Label efterInfo;
    glaze::GroupBox efterAutoGrp;
    glaze::Checkbox efterAutoChk;
    glaze::Button efterSomForeBtn;
    glaze::Label efterFranLbl;
    glaze::Label efterTillLbl;
    glaze::Label efterFasLbl;
    glaze::Dropdown efterFasFran;
    glaze::Dropdown efterFasTill;
    glaze::Label efterFasStatus;
    glaze::Label efterToppVLbl;
    glaze::Dropdown efterToppVFran;
    glaze::Dropdown efterToppVTill;
    glaze::Label efterToppVStatus;
    glaze::Label efterToppHLbl;
    glaze::Dropdown efterToppHFran;
    glaze::Dropdown efterToppHTill;
    glaze::Label efterToppHStatus;
    glaze::Label efterSammaRuta;
    glaze::Label efterSammaText;
    glaze::Label efterAndradRuta;
    glaze::Label efterAndradText;
    glaze::Label efterNyRuta;
    glaze::Label efterNyText;
    glaze::Label efterFilterLbl;
    glaze::Dropdown efterFilterDrop;
    glaze::Button efterVisaBtn;
    glaze::Label efterFilterInfo;
    glaze::DataGrid efterGrid;
    glaze::Panel linPanel;
    glaze::Label linRubrik;
    glaze::Label linInfo;
    glaze::Label linOmrade;
    kalkyl::lineheightsheet::LineHeightSheet linBlad;
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
    // En rad som visar en automatisk beräkning har bara Visa… och Gå till tabellen. Tom meny ger
    // ingen meny.
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

    // Högerklick i listan: Ändra (eller Visa), Kopiera och Ta bort för raden under pekaren. För en
    // automatisk beräkning: Visa…, Kopiera… och Gå till tabellen.
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

    // Högerklick i listan: Ändra (eller Visa), Kopiera och Ta bort för raden under pekaren. För en
    // automatisk beräkning: Visa…, Kopiera… och Gå till tabellen.
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

    // Läser om alla normalspannsberäkningar ur projektet, räknar dem (projectdb::calculateSpan) och
    // uppdaterar raderna i Dokumentation och listan i fliken Normalspann.
    void refreshNormalSpans();

    // Värde-cellen på de sex normalspannsraderna: aₙ med Σ-bild när en beräkning är kopplad (Sync-
    // bild för en automatisk), annars det manuella värdet eller "-". Tooltipen säger vad som gäller.
    void showNormalSpanRows();

    // Bygger raderna i listan i fliken Normalspann ur normalSpans_ och nsCalcs_, med gul växlande
    // radton. De automatiska beräkningarna har Sync-bild, Indata Automatisk och lila eller rosa
    // ton. Den tidigare valda beräkningen väljs igen.
    void fillNormalSpanList();

    // Aktiverar knapparna i fliken Normalspann efter valet och skrivskyddat läge, och ger listan
    // en tooltip om den valda beräkningen (avvikelser per kontroll och var den visas). För en
    // automatisk beräkning heter Ändra… Visa… och Ta bort är släckt.
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
    // kopplad (Sync-bild för en automatisk), annars det manuella värdet eller "-". Tooltipen säger
    // vad som gäller.
    void showLineCalcRows();

    // Bygger raderna i listan i fliken Linberäkning ur lineCalcs_, lcResolved_ och lcCalcs_, med gul
    // växlande radton. De automatiska beräkningarna har Sync-bild, Indata Automatisk och lila eller
    // rosa ton. Den tidigare valda beräkningen väljs igen.
    void fillLineCalcList();

    // Aktiverar knapparna i fliken Linberäkning efter valet och skrivskyddat läge, och ger listan
    // en tooltip om den valda beräkningen (första upplysningen eller mallen, och var den visas).
    // För en automatisk beräkning heter Ändra… Visa… och Ta bort är släckt.
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

    // Rutan Automatiskt läge i Stolptabell före: slår läget på eller av och sparar det i projektet.
    void stolpAutoChkChanged(glaze::Widget* sender, glaze::ChangeArgs& e);

    // Rutan Automatiskt läge i Stolptabell efter: slår läget på eller av och sparar det i projektet.
    void efterAutoChkChanged(glaze::Widget* sender, glaze::ChangeArgs& e);

    // En av de tolv rullistorna Från och Till: skriver den valda Stolpplatsen i skedets sträcka
    // för linan och sparar inställningen. Avsändaren hittas bland panelernas rullistor.
    void rangeChanged(glaze::Widget* sender, glaze::ChangeArgs& e);

    // Importera stolptabell…: öppnar importen av en stolptabell från Banken till Stolptabell före.
    // Själva flödet står i importPolesAsked.
    void stolpImportBtnClicked(glaze::Widget* sender, glaze::ClickArgs& e);

    // Kopiera från Stolptabell före…: frågar när efter har rader och ersätter sedan efter med en
    // kopia av före, med sträckor och läge.
    void efterKopieraBtnClicked(glaze::Widget* sender, glaze::ClickArgs& e);

    // Sträckorna som i före: hämtar start och slut ur före utan att röra raderna i efter.
    void efterSomForeBtnClicked(glaze::Widget* sender, glaze::ClickArgs& e);

    // Filtret i Stolptabell efter: Förändrade ±2 eller Alla. Valet sparas i systemdatabasen, och
    // Förändrade ±2 rullar till de förändrade stolparna.
    void efterFilterDropChanged(glaze::Widget* sender, glaze::ChangeArgs& e);

    // Visa förändrade: rullar Stolptabell efter till de förändrade stolparna (PoleGrid::showChanged).
    void efterVisaBtnClicked(glaze::Widget* sender, glaze::ClickArgs& e);

    // Tabellernas händelser går vidare till PoleGrid: en sparad cell, radera-knappen, tangenterna,
    // musen och högerklicket. Stolptabell före först och Stolptabell efter sedan.
    void stolpGridCommitted(glaze::Widget* sender, glaze::CellChangeArgs& e);
    void stolpGridCellClicked(glaze::Widget* sender, glaze::CellClickArgs& e);
    void stolpGridKeyDown(glaze::Widget* sender, glaze::KeyArgs& e);
    void stolpGridMouseUp(glaze::Widget* sender, glaze::MouseArgs& e);
    void stolpGridContextMenu(glaze::Widget* sender, glaze::ContextMenuArgs& e);
    void efterGridCommitted(glaze::Widget* sender, glaze::CellChangeArgs& e);
    void efterGridCellClicked(glaze::Widget* sender, glaze::CellClickArgs& e);
    void efterGridKeyDown(glaze::Widget* sender, glaze::KeyArgs& e);
    void efterGridMouseUp(glaze::Widget* sender, glaze::MouseArgs& e);
    void efterGridContextMenu(glaze::Widget* sender, glaze::ContextMenuArgs& e);

    // Det PoleGrid frågar fönstret om för en av stolptabellerna: sparar tabellen i projektet,
    // läser om den efter ett fel, räknar om när den ändrats, följer en omdöpt Stolpplats i
    // sträckorna, lägger menyn Sträcka i snabbmenyn och ger noten i frågan Radera.
    class PoleHost : public kalkyl::polegrid::PoleGridHost {
    public:
        PoleHost(MainWindow& window, kalkyl::projectdb::Stage stage) : window_(window), stage_(stage) {}
        bool savePoles(const std::vector<kalkyl::poletable::PoleRow>& rows) override;
        std::vector<kalkyl::poletable::PoleRow> reloadPoles() override;
        void polesChanged() override;
        void poleRenamed(const std::string& from, const std::string& to) override;
        void addMenuItems(glaze::Menu& menu, int row) override;
        std::string removeNote(int first, int last) const override;
        void showStatus(const std::string& text) override;
        void filterChanged(kalkyl::poleview::Filter filter) override;

    private:
        MainWindow& window_;
        kalkyl::projectdb::Stage stage_;
    };

    // Panelen Automatiskt läge för ett skede: pekare till widgetarna i regionen. itemPoles har
    // varje rullistposts Stolpplats (post 0 är tom) och itemTexts det som visas, så att en
    // rullista bara byggs om när innehållet ändrats.
    struct AutoPanel {
        glaze::Checkbox* check = nullptr;
        std::array<glaze::Label*, 3> names{};
        std::array<glaze::Dropdown*, 6> drops{};
        std::array<glaze::Label*, 3> status{};
        std::array<std::vector<std::string>, 6> itemPoles;
        std::array<std::vector<std::string>, 6> itemTexts;
    };

    // Läser båda stolptabellerna och inställningarna för automatiskt läge ur projektet till
    // tabellerna och autoBefore_ och autoAfter_. Ett fel ger en meddelanderuta och en tom tabell.
    void loadPoleTables();

    // Sparar hela tabellen för skedet i projektet. Vid fel loggas det och en ruta visas.
    // Svarar om det lyckades.
    bool savePoles(kalkyl::projectdb::Stage stage, const std::vector<kalkyl::poletable::PoleRow>& rows);

    // Tabellens rader som de står i projektet. Vid fel loggas det och listan är tom.
    std::vector<kalkyl::poletable::PoleRow> reloadPoles(kalkyl::projectdb::Stage stage);

    // Läser om de automatiska beräkningarna ur projektet och uppdaterar Dokumentation, flikarna
    // Normalspann och Linberäkning, panelerna Automatiskt läge, tabellernas färger och fliken
    // Linhöjder.
    void refreshAutomatic();

    // Linhöjdsbladet sparade en cell: talet kontrolleras (lineheight::acceptHeight eller acceptAv)
    // och skrivs i projektet, och bladet räknas om. Ogiltig text visas i statusraden.
    void linBladCommitted(glaze::Widget* sender, glaze::ChangeArgs& e);

    // Bygger fliken Linhöjder ur stolptabellerna och de sparade linhöjderna: blocken, bladet och
    // områdestexten. Utan block döljs bladet.
    void refreshLineHeights();

    // Fyller panelerna Automatiskt läge för båda skedena.
    void updateAutoPanels();

    // Fyller panelen för skedet: rutan, linornas namn, rullistorna med tabellens Stolpplatser
    // och statustexten för varje sträcka. Höger-raden döljs när topplinorna är samma.
    void fillAutoPanel(kalkyl::projectdb::Stage stage);

    // Räknar om markeringarna och jämförelsen mot före och ger dem till tabellerna.
    void updatePoleLooks();

    // Sätter filtret i Stolptabell efter: rullistan, PoleGrid och, med save, systemdatabasen. center
    // rullar till de förändrade stolparna. Ett fel vid sparningen visas i statusraden.
    void applyAfterFilter(kalkyl::poleview::Filter filter, bool save, bool center);

    // Infotexten bredvid Visa förändrade och om knappen är tänd (poleview::summarize).
    void updateAfterFilterInfo();

    // Sparar inställningen för skedet. Vid fel visas en ruta och inställningen läses om.
    void saveAutoSettings(kalkyl::projectdb::Stage stage, const kalkyl::projectdb::AutoSettings& settings);

    // De linor som är automatiska i Dokumentations läge: faslinan, topplina vänster och topplina
    // höger när den är en egen lina.
    std::vector<kalkyl::projectdb::Wire> activeWires() const;

    // Skedets sträckor med linornas ord, för de aktiva linorna, till markeringarna i tabellen.
    std::vector<kalkyl::automode::NamedRange> namedRanges(kalkyl::projectdb::Stage stage) const;

    // Undermenyn Sträcka i tabellens snabbmeny för raden row: sätter start eller slut för en lina.
    void addRangeMenuItems(kalkyl::projectdb::Stage stage, glaze::Menu& menu, int row);

    // En Stolpplats döptes om: sträckor som börjar eller slutar där följer med.
    void renameInRanges(kalkyl::projectdb::Stage stage, const std::string& from, const std::string& to);

    // Noten i frågan Radera: vilka sträckor som börjar eller slutar på raderna first till last.
    // Tom text när ingen gör det.
    std::string rangeNote(kalkyl::projectdb::Stage stage, int first, int last) const;

    // Kopiera från Stolptabell före…: kontrollerna, frågan och kopieringen.
    void copyBeforeToAfterAsked();

    // Importera stolptabell…: väljer filen, öppnar dialogen PoleImportDialog och skriver sedan bara
    // raderna i Stolptabell före (projectdb::importPolesBefore). Sträckorna, läget, Stolptabell efter
    // och Dokumentation rörs inte. Visar felet i en ruta och annars resultatet i statusraden.
    void importPolesAsked();

    // Väljer fliken för skedet och gör raden row aktuell i tabellen (row < 0 väljer bara fliken).
    void showPoleTab(kalkyl::projectdb::Stage stage, int row);

    // Tabellen och inställningen för skedet.
    kalkyl::polegrid::PoleGrid& poleGridOf(kalkyl::projectdb::Stage stage) const;
    kalkyl::projectdb::AutoSettings& settingsOf(kalkyl::projectdb::Stage stage);
    const kalkyl::projectdb::AutoSettings& settingsOf(kalkyl::projectdb::Stage stage) const;
    AutoPanel& panelOf(kalkyl::projectdb::Stage stage);

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
    std::unique_ptr<PoleHost> beforeHost_;                           // PoleGrids värd för Stolptabell före
    std::unique_ptr<PoleHost> afterHost_;                            // PoleGrids värd för Stolptabell efter
    std::unique_ptr<kalkyl::polegrid::PoleGrid> beforeGrid_;         // Stolptabell före i stolpGrid
    std::unique_ptr<kalkyl::polegrid::PoleGrid> afterGrid_;          // Stolptabell efter i efterGrid
    kalkyl::projectdb::AutoSettings autoBefore_;                     // automatiskt läge och sträckor i före
    kalkyl::projectdb::AutoSettings autoAfter_;                      // automatiskt läge och sträckor i efter
    kalkyl::poleview::Filter afterFilter_ = kalkyl::poleview::Filter::Changed; // filtret i Stolptabell efter
    AutoPanel beforePanel_;                                          // panelen Automatiskt läge i före
    AutoPanel afterPanel_;                                           // panelen Automatiskt läge i efter
    int gridStatusPanel_ = -1;                                       // statusradens panel för tabellens meddelanden
    glaze::FileDialog importFileDlg_{glaze::FileDialogMode::Open};   // filväljaren för Importera stolptabell…
    glaze::threads::Worker saveWorker_;                              // sist, så att den rivs först
};
