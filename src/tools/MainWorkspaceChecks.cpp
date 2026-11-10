#include "ui/MainWorkspaceView.h"
#include "ui/UiStateManager.h"
#include "ui/FeatureListPanel.h"
#include "ui/WorkflowEditorPanel.h"
#include "ui/BlockListWidget.h"
#include "app/MainWindow.h"
#include "app/ForegroundWindowMonitor.h"
#include "app/TriggerMonitorController.h"
#include <QApplication>
#include <QMainWindow>
#include <QSplitter>
#include <QPushButton>
#include <QCheckBox>
#include <QElapsedTimer>
#include <QTimer>
#include <QThread>
#include <QDir>
#include <QSettings>
#include <QMap>
#include <iostream>

class MainWorkspaceUiChecks {
public:
static bool run(){
    if(QApplication::platformName()!="offscreen")return false;
    const auto fail=[](const char* label){std::cerr<<"Main workspace FAILED: "<<label<<'\n';return false;};
    QSettings settings;QMap<QString,QVariant> previous;
    for(const auto& key:settings.allKeys())if(key.startsWith("ui/state/"))previous.insert(key,settings.value(key));
    struct Restore{QMap<QString,QVariant> values;~Restore(){QSettings s;s.remove("ui/state");for(auto it=values.cbegin();it!=values.cend();++it)s.setValue(it.key(),it.value());}}restore{previous};
    settings.remove("ui/state");
    const QString columnKey=UiStateManager::settingsKey("workflowBlockList/columns");
    settings.setValue(UiStateManager::settingsKey("workflowBlockList/rowHeight")+"/rowHeight",38);
    QByteArray horizontal,vertical,bottom,workflow;
    {
        QMainWindow window;MainWorkspaceView view(window);const auto& ui=view.widgets();
        if(!window.centralWidget() || !ui.m_titleBar || !ui.m_featureList || !ui.m_workflowEditor
            || !ui.m_targetWindowDetailPanel || !ui.m_logPanel || !ui.m_memoButton->isCheckable()
            || !ui.m_calculatorButton->isCheckable() || !ui.m_spikeWatchButton->isCheckable())return fail("independent widget tree");
        auto* state=view.restoreState();
        if(!state || state!=view.restoreState() || state->parent()!=&view || view.findChildren<UiStateManager*>().size()!=1)return fail("one component-owned state manager");
        window.ensurePolished();window.grab();
        auto* table=ui.m_workflowEditor->blockList();
        if(table->columnLayout().rowHeight!=38)return fail("legacy row height fallback");
        auto blockLayout=table->columnLayout();blockLayout.scoreWidth=123;blockLayout.rowHeight=44;table->setColumnLayout(blockLayout);
        auto featureLayout=ui.m_featureList->columnLayout();featureLayout.hotkeyColumnWidth=75;featureLayout.rowHeight=31;ui.m_featureList->setColumnLayout(featureLayout);
        auto* timer=state->findChild<QTimer*>();
        if(!timer || !timer->isActive())return fail("layout signal schedules owned save");
        QElapsedTimer deadline;deadline.start();while(timer->isActive() && deadline.elapsed()<2000){QApplication::processEvents();QThread::msleep(1);}
        if(timer->isActive() || settings.value(columnKey+"/scoreWidth").toInt()!=123 || settings.value(columnKey+"/rowHeight").toInt()!=44)return fail("real timer persists canonical columns");
        ui.m_mainHorizontalSplitter->setSizes({160,320,780});ui.m_mainVerticalSplitter->setSizes({480,260});view.clampVertical();
        ui.m_bottomHorizontalSplitter->setSizes({520,450});ui.m_workflowEditor->workflowSplitter()->setSizes({300,110});
        state->saveNow();horizontal=ui.m_mainHorizontalSplitter->saveState();vertical=ui.m_mainVerticalSplitter->saveState();
        bottom=ui.m_bottomHorizontalSplitter->saveState();workflow=ui.m_workflowEditor->workflowSplitter()->saveState();
    }
    // A new component and window must restore the original keys, not aliases
    // that exist only for MainWindow's wiring.
    {
        QMainWindow window;MainWorkspaceView view(window);const auto& ui=view.widgets();view.restoreState();window.ensurePolished();window.grab();
        const auto blocks=ui.m_workflowEditor->blockList()->columnLayout();const auto features=ui.m_featureList->columnLayout();
        if(blocks.scoreWidth!=123 || blocks.rowHeight!=44 || features.hotkeyColumnWidth!=75 || features.rowHeight!=31)return fail("canonical restore overrides legacy fallback");
        if(ui.m_mainHorizontalSplitter->saveState()!=horizontal || ui.m_mainVerticalSplitter->saveState()!=vertical
            || ui.m_bottomHorizontalSplitter->saveState()!=bottom || ui.m_workflowEditor->workflowSplitter()->saveState()!=workflow)return fail("saved splitter ratios restore");
        ui.m_mainVerticalSplitter->setSizes({1,9999});view.clampVertical();
        const auto sizes=ui.m_mainVerticalSplitter->sizes();
        if(sizes.value(0)<252 || sizes.value(1)<108)return fail("vertical minimum clamp");
        if(!window.grab().save(QDir(qEnvironmentVariable("PIPBONG_DATA_DIR")).filePath("main-workspace-detail.png")))return fail("component render");
    }
    {
        QMainWindow owner;auto* view=new MainWorkspaceView(owner);QPointer<UiStateManager> state=view->restoreState();
        state->scheduleSave();if(!state->findChild<QTimer*>()->isActive())return fail("pending save setup");
        delete view;if(state)return fail("component destruction cancels manager lifetime");
        QApplication::processEvents();
    }
    {
        MainWindow window;window.m_foregroundMonitor->stop();window.m_triggerMonitor->stop();
        const auto& ui=window.m_workspaceView->widgets();
        if(window.m_uiState!=window.m_workspaceView->state() || window.m_workflowEditor!=ui.m_workflowEditor
            || window.m_featureList!=ui.m_featureList || window.m_profileList!=ui.m_profileList
            || window.m_memoButton!=ui.m_memoButton || window.m_titleBar!=ui.m_titleBar)return fail("actual shell aliases and state ownership");
        if(!window.prepareForShutdown())return fail("actual shell checkpoint");
    }
    std::cout<<"Main workspace: independent Qt tree, one owned state manager, real save timer, legacy/canonical columns+rows, fresh-window splitter restore/minimums, pending-save lifetime and actual MainWindow aliases/checkpoint OK (offscreen; no native movement/input)\n";
    return true;
}
};
bool runMainWorkspaceChecks(){return MainWorkspaceUiChecks::run();}
