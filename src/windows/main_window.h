#pragma once

#include <glaze/glaze.h>
#include <glaze_threads/threads.h>

#include <array>
#include <cstdint>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "calc/normal_span.h"
#include "conductor/conductor.h"
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
    glaze::TabPage nsSida;
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
    glaze::DataGrid stolpGrid;
    glaze::Panel nsPanel;
    glaze::Label nsRubrik;
    glaze::Button nsNyBtn;
    glaze::Button nsAndraBtn;
    glaze::Button nsKopieraBtn;
    glaze::Button nsTaBortBtn;
    glaze::ListView nsLista;
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
    // sex normalspannsraderna (lägg till, koppla, ändra, ta bort). Tom meny ger ingen meny.
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

    // Läser projektets uppgifter: Dokumentationens värden och referenser (med förval för det som
    // saknas) och projektets linkopior. Ett fel ger en meddelanderuta och förvalen.
    void loadProjectData();

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

    // Efter en ändring i projektet: läser om uppgifterna och beräkningarna, uppdaterar
    // rullistorna och titeln. Fel visas av funktionerna som anropas.
    void reloadAfterChange();

    // Nyckel och radtext för de sex normalspannsraderna, till formuläret. Texten följer läget
    // (Samma eller egna topplinor).
    std::vector<std::pair<std::string, std::string>> docRowLabels() const;

    // Radens korta namn i listan, t.ex. "Fas före" eller "Topp h. efter (dold)".
    std::string shortDocLabel(const std::string& key) const;

    // Radens hela text i Dokumentation, t.ex. "Normalspannsberäkning faslina före ombyggnation".
    std::string fullDocLabel(const std::string& key) const;

    // De rader som visar beräkningen, med korta namn skilda med komma och i radordning.
    // Utan rader blir svaret "–". Används i listan och i menyn Koppla befintlig.
    std::string shortRowsText(const kalkyl::projectdb::NormalSpan& ns) const;

    // Platsen i normalSpans_ för beräkningen som raden key visar, eller -1.
    int linkedSpanIndex(const std::string& key) const;

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

    // Ger stolptabellens rader samma gula, växlande ton som temperaturraderna i Dokumentation
    // (varannan rad en aning mörkare), så att raderna är lätta att följa.
    void shadeStolpRows();

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
    std::vector<kalkyl::calc::SpanCalculation> nsCalcs_;             // resultat per beräkning, samma ordning
    std::vector<std::int64_t> nsListIds_;                            // beräkningens id per rad i listan
    glaze::Menu byggPmMenu_;                                         // snabbmenyn i Dokumentation
    glaze::Menu nsMenu_;                                             // snabbmenyn i listan
    bool saving_ = false;                                            // en publicering pågår
    bool closeAfterSave_ = false;                                    // stäng fönstret när sparningen är klar
    bool closingConfirmed_ = false;                                  // stängningen är bekräftad, ingen fråga
    bool autosaveBlocked_ = false;                                   // avstängd efter en konflikt
    int autosaveMinutes_ = 5;                                        // inställningen, 0 = av
    int savePanel_ = -1;                                             // sparpanelens plats i statusraden
    std::string saveStatus_;                                         // senaste sparutfallet
    bool saveStatusIsError_ = false;                                 // utfallet var ett fel
    glaze::threads::Worker saveWorker_;                              // sist, så att den rivs först
};
