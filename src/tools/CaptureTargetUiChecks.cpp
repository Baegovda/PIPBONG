#include "app/MainWindow.h"
#include "app/CaptureTargetController.h"
#include "app/ForegroundWindowMonitor.h"
#include "app/ProjectWorkspaceController.h"
#include "model/Project.h"
#include "model/Feature.h"
#include "storage/JsonSerializer.h"
#include "core/capture/ScreenCapture.h"
#include "core/workflow/Workflow.h"
#include "core/workflow/blocks/WaitBlock.h"
#include <QApplication>
#include <QDir>
#include <QElapsedTimer>
#include <QThread>
#include <iostream>

class CaptureTargetUiChecks {
public:
    static bool run(const QString& folder) {
        if(QApplication::platformName()!=QStringLiteral("offscreen"))return false;
#ifdef _WIN32
        using Controller=CaptureTargetController;
        const bool oldFocus=ProgramSettings::focusTargetWindowOnProfileSelect();ProgramSettings::setFocusTargetWindowOnProfileSelect(false);
        struct Restore {bool focus;~Restore(){ProgramSettings::setFocusTargetWindowOnProfileSelect(focus);}} restore{oldFocus};
        MainWindow window;window.m_foregroundMonitor->stop();
        ProfileSwitchCoordinator::HostCallbacks quiet;quiet.isWorkspacePinned=[]{return true;};quiet.flushDeferredProfileSwitchIfIdle=[]{};
        window.m_profileSwitchCoordinator.setHostCallbacks(quiet);
        const auto original=window.m_profileManager->activeProfileId();
        const auto owner=window.m_profileManager->addProfile("capture owner",false),other=window.m_profileManager->addProfile("capture other",false);
        const QString mainPath="C:/__capture_fixture__/main.exe",subPath="C:/__capture_fixture__/sub.exe",otherPath="C:/__capture_fixture__/other.exe";
        Project project;project.setTargetWindowTitle("Same");
        auto* watch=project.addFeature("background watch");watch->setId("capture-watch");watch->setRunMode(FeatureRunMode::Trigger);watch->setTriggerRunWithoutTargetForeground(true);watch->setPointerVisualFeedback(false);
        auto* normal=project.addFeature("locked normal");normal->setId("capture-normal");normal->setPointerVisualFeedback(false);
        Project second;second.setTargetWindowTitle("Second");second.addFeature("other document");
        if(owner.isEmpty() || other.isEmpty() || !JsonSerializer::saveToFile(project,window.m_profileManager->projectPath(owner),window.m_profileManager->projectDirectory(owner))
            || !JsonSerializer::saveToFile(second,window.m_profileManager->projectPath(other),window.m_profileManager->projectDirectory(other))
            || !window.m_profileManager->updateProfileTargetBinding(owner,"Same",mainPath,true)
            || !window.m_profileManager->updateProfileSubTargetBinding(owner,"Same",subPath,true)
            || !window.m_profileManager->updateProfileTargetBinding(other,"Second",otherPath,true))return false;
        const HWND main=reinterpret_cast<HWND>(quintptr(1)),sub=reinterpret_cast<HWND>(quintptr(2)),third=reinterpret_cast<HWND>(quintptr(3));
        std::map<HWND,Controller::Window> windows{{main,{main,"Same",mainPath,11,true,false}},
            {sub,{sub,"Same",subPath,22,true,false}},{third,{third,"Second",otherPath,33,true,false}}};
        ForegroundWindowState fg;fg.rootHwnd=sub;fg.title="Same";fg.processPath=subPath;
        int applies=0,notifications=0;Controller::Applied applied;
        window.m_captureTargets->m_ports={
            [&](HWND hwnd)->std::optional<Controller::Window>{const auto found=windows.find(hwnd);return found==windows.end()?std::nullopt:std::optional<Controller::Window>(found->second);},
            [&](const QString& title,const QString& path)->std::optional<Controller::Window>{
                for(const auto& entry:windows)if(path.isEmpty()?entry.second.title==title:entry.second.path==path)return entry.second;return std::nullopt;
            },[&](const Controller::Applied& value){++applies;applied=value;ScreenCapture::setTargetWindowTitle(value.target.title.toStdWString());ScreenCapture::setSubTargetWindowTitle(value.bindings.subBinding.toStdWString());ScreenCapture::setTargetWindow(nullptr);}};
        window.m_captureTargets->m_host.foreground=[&]{return fg;};
        window.m_captureTargets->m_host.foregroundAllowed=[](const QString&){return true;};
        window.m_captureTargets->m_host.monitorChanged=[&]{++notifications;}; // Observe notification; never launch a native capture workflow.
        const auto settle=[&]{QElapsedTimer t;t.start();while(window.m_profileSwitchPipelineActive && t.elapsed()<4000){QApplication::processEvents();QThread::msleep(1);}return !window.m_profileSwitchPipelineActive;};
        if(!window.executeProfileSwitch(owner,false) || !settle())return false;
        if(window.m_captureTargets->choose(FeatureCaptureTargetScope::MainOnly).path!=mainPath
            || window.m_captureTargets->choose(FeatureCaptureTargetScope::SubOnly).path!=subPath)return false;
        window.syncEffectiveTargetWindowTitleToCapture();
        if(applied.target.title!="Same" || applied.target.path!=subPath)return false;
        auto makeSession=[&](const char* featureId,FeatureRunMode mode,TriggerSessionPhase phase) {
            FeatureRunSession session;session.featureId=featureId;session.profileId=owner;session.runningMode=mode;session.triggerPhase=phase;session.repeatSession=true;
            session.lockedCaptureTargetTitle=L"Same";session.lockedCaptureTargetProcessPath=mainPath.toStdWString();
            session.sessionContext=std::make_shared<ExecutionContext>();session.sessionContext->setTargetWindowBindingForWorker(L"Same",mainPath.toStdWString());
            session.engine=std::make_unique<WorkflowEngine>();auto flow=std::make_shared<Workflow>();auto wait=std::make_unique<WaitBlock>();wait->ms=10000;flow->addBlock(std::move(wait));session.engine->run(flow,session.sessionContext);
            return session;
        };
        window.runSessions().emplace("capture-watch",makeSession("capture-watch",FeatureRunMode::Trigger,TriggerSessionPhase::Monitoring));
        window.runSessions().emplace("capture-normal",makeSession("capture-normal",FeatureRunMode::RepeatCount,TriggerSessionPhase::None));
        ScreenCapture::setTargetWindowTitle(L"global-main-sentinel");ScreenCapture::setSubTargetWindowTitle(L"global-sub-sentinel");
        const int before=applies;window.syncEffectiveTargetWindowTitleToCapture();
        auto* monitored=window.sessionFor("capture-watch");auto* locked=window.sessionFor("capture-normal");
        if(!monitored || !locked || monitored->sessionContext->targetWindowBinding().second!=subPath.toStdWString()
            || locked->lockedCaptureTargetProcessPath!=mainPath.toStdWString() || applies!=before
            || ScreenCapture::targetWindowTitle()!=L"global-main-sentinel" || ScreenCapture::subTargetWindowTitle()!=L"global-sub-sentinel")return false;
        if(!window.executeProfileSwitch(other,false) || !settle())return false;
        FeatureRunSession missing;missing.profileId="missing-owner";missing.featureId="missing-feature";
        if(!window.sessionCaptureTargetTitleW(missing,false).empty())return false;
        fg.rootHwnd=main;fg.processPath=mainPath;
        window.syncEffectiveTargetWindowTitleToCapture();monitored=window.sessionFor("capture-watch");locked=window.sessionFor("capture-normal");
        const bool owned=monitored && locked && monitored->sessionContext->targetWindowBinding().second==mainPath.toStdWString()
            && monitored->profileId==owner && locked->lockedCaptureTargetProcessPath==mainPath.toStdWString()
            && window.m_profileManager->activeProfileId()==other && applies==before && notifications>0;
        const bool rendered=window.grab().save(QDir(folder).filePath("ui-capture-owner.png"));
        for(const auto& id:{"capture-watch","capture-normal"})if(auto* session=window.sessionFor(id);session && session->engine)session->engine->stopAndWaitBounded(1000);
        window.runSessions().erase("capture-watch");window.runSessions().erase("capture-normal");
        if(!owned || !rendered || !window.executeProfileSwitch(original,false) || !settle() || !window.prepareForShutdown()) {
            std::cerr<<"Capture UI FAILED: owner/context/global isolation or cleanup\n";return false;
        }
        std::cout<<"Capture MainWindow: identical-title executable selection, real Wait-engine lock, background owner migration after profile switch, coherent context pair and global main/sub sentinel retention OK (offscreen; scripted windows; no native activation/input)\n";
#endif
        return true;
    }
};
bool runCaptureTargetUiChecks(const QString& folder){return CaptureTargetUiChecks::run(folder);}
