#include "app/MainWindow.h"
#include "app/TargetCenterPinController.h"
#include "app/ForegroundWindowMonitor.h"
#include "app/WorkspaceHistoryActions.h"
#include "app/ProjectWorkspaceController.h"
#include "model/Project.h"
#include "storage/JsonSerializer.h"
#include "core/workflow/Workflow.h"
#include "core/workflow/blocks/WaitBlock.h"
#include <QApplication>
#include <QDir>
#include <QElapsedTimer>
#include <QThread>
#include <QTimer>
#include <QToolButton>
#include <iostream>

class TargetCenterPinUiChecks {
public:
    static bool run(const QString& folder) {
        if(QApplication::platformName()!=QStringLiteral("offscreen"))return false;
#ifdef _WIN32
        const bool oldFocus=ProgramSettings::focusTargetWindowOnProfileSelect();ProgramSettings::setFocusTargetWindowOnProfileSelect(false);
        struct Restore {bool focus;~Restore(){ProgramSettings::setFocusTargetWindowOnProfileSelect(focus);}} restore{oldFocus};
        MainWindow window;window.m_foregroundMonitor->stop();
        ProfileSwitchCoordinator::HostCallbacks quiet;quiet.isWorkspacePinned=[]{return true;};quiet.flushDeferredProfileSwitchIfIdle=[]{};
        window.m_profileSwitchCoordinator.setHostCallbacks(quiet);
        int moves=0,resolves=0;bool forced=false;
        window.m_centerPin->m_ports={
            [&](const QString&,const QString& path)->std::optional<TargetCenterPinController::Window> {
                ++resolves;return TargetCenterPinController::Window{reinterpret_cast<HWND>(quintptr(1)),path,42,true,false};
            },[&](const auto&,bool force){++moves;forced=force;return true;}};
        const auto original=window.m_profileManager->activeProfileId();
        const auto id=window.m_profileManager->addProfile("center pin integration",false);
        Project project;project.setTargetWindowTitle("Center");project.addFeature("preserved");
        if(id.isEmpty() || !JsonSerializer::saveToFile(project,window.m_profileManager->projectPath(id),window.m_profileManager->projectDirectory(id))
            || !window.m_profileManager->updateProfileTargetBinding(id,"Center","C:/__center_fixture__/main.exe",true))return false;
        auto settings=window.m_profileManager->loadSettings(id);settings.pinTargetWindowToScreenCenter=false;settings.pinSubTargetWindowToScreenCenter=false;
        if(!window.m_profileManager->saveSettings(id,settings) || !window.executeProfileSwitch(id,false))return false;
        const auto settle=[&] {
            QElapsedTimer timer;timer.start();
            while(window.m_profileSwitchPipelineActive && timer.elapsed()<4000){QApplication::processEvents();QThread::msleep(1);}
            return !window.m_profileSwitchPipelineActive;
        };
        if(!settle())return false;
        const auto path=QDir(window.m_profileManager->profileDirectory(id)).filePath("profile-settings.json");
        HANDLE locked=CreateFileW(reinterpret_cast<LPCWSTR>(path.utf16()),GENERIC_READ,FILE_SHARE_READ,nullptr,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,nullptr);
        if(locked==INVALID_HANDLE_VALUE)return false;
        window.m_pinTargetWindowCenterButton->setChecked(true); // Real production Qt signal, no OS input.
        const bool retained=!window.m_pinTargetWindowCenterButton->isChecked() && !ProgramSettings::pinTargetWindowToScreenCenter()
            && !window.m_profileManager->loadSettings(id).pinTargetWindowToScreenCenter && moves==0;
        CloseHandle(locked);
        if(!retained){std::cerr<<"Center pin UI FAILED: locked preference changed checkbox/runtime or moved a window\n";return false;}
        if(!window.grab().save(QDir(folder).filePath("ui-center-pin-save-failure.png")))return false;
        // Even a session from another profile must block forced enable and timer corrections.
        FeatureRunSession session;session.featureId="center-background";session.profileId=original;
        session.engine=std::make_unique<WorkflowEngine>();session.sessionContext=std::make_shared<ExecutionContext>();
        auto flow=std::make_shared<Workflow>();auto wait=std::make_unique<WaitBlock>();wait->ms=5000;flow->addBlock(std::move(wait));
        session.engine->run(flow,session.sessionContext);auto* engine=session.engine.get();window.runSessions().emplace(session.featureId,std::move(session));
        const int beforeResolve=resolves;
        window.m_pinTargetWindowCenterButton->setChecked(true);window.syncTargetWindowCenterPin();
        const bool deferred=window.m_pinTargetWindowCenterButton->isChecked() && ProgramSettings::pinTargetWindowToScreenCenter()
            && window.m_profileManager->loadSettings(id).pinTargetWindowToScreenCenter && resolves==beforeResolve && moves==0;
        engine->stopAndWaitBounded(1000);window.runSessions().erase("center-background");
        if(!deferred){std::cerr<<"Center pin UI FAILED: background engine did not defer forced enable\n";return false;}
        window.syncTargetWindowCenterPin();
        if(moves!=1 || !forced)return false;
        bool observed=false,preserved=false;
        QTimer::singleShot(0,&window,[&] {
            if(!window.m_workspaceHistory->isBusy())return;
            observed=true;const int before=moves;
            window.syncTargetWindowCenterPin();window.m_pinTargetWindowCenterButton->setChecked(false);
            preserved=moves==before && window.m_pinTargetWindowCenterButton->isChecked()
                && ProgramSettings::pinTargetWindowToScreenCenter();
        });
        if(!window.pushGlobalUiUndoSnapshot("center-pin-guard") || !observed || !preserved)return false;
        if(!window.executeProfileSwitch(original,false) || !settle() || !window.prepareForShutdown())return false;
        const int beforeReady=moves;window.syncTargetWindowCenterPin();
        if(moves!=beforeReady)return false;
        for(auto* timer:window.m_centerPin->findChildren<QTimer*>())if(timer->isActive())return false;
        std::cout<<"Center pin MainWindow: real toggle signal and locked-settings retention, background Wait engine defers forced enable, idle resume, history edit/move guards and shutdown timer stop OK (offscreen; scripted move ports; no native movement/input)\n";
#endif
        return true;
    }
};
bool runTargetCenterPinUiChecks(const QString& folder){return TargetCenterPinUiChecks::run(folder);}
