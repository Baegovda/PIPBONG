#include "app/MainWindow.h"
#include "app/SessionInputCleanupController.h"
#include "app/RunLifecycleCoordinator.h"
#include "app/ForegroundWindowMonitor.h"
#include "model/Project.h"
#include "model/Feature.h"
#include "storage/JsonSerializer.h"
#include "core/workflow/blocks/WaitBlock.h"
#include "core/input/InputSimulator.h"
#include <QApplication>
#include <QElapsedTimer>
#include <QThread>
#include <iostream>

class SessionInputCleanupUiChecks {
public:
    static bool run(bool physicalRelease) {
        int releases=0,cleanups=0,lastKey=0,moves=0,reads=0;QString path;QPoint moved;
        MainWindow window;window.m_foregroundMonitor->stop();
        const auto failed=[&](const char* stage){std::cerr<<"Session input cleanup UI FAILED: "<<stage
            <<" mode="<<physicalRelease<<" releases="<<releases<<" reads="<<reads<<" cleanup="<<cleanups
            <<" key="<<lastKey<<" moves="<<moves<<'\n';return false;};
        const auto original=window.m_profileManager->activeProfileId();
        const auto owner=window.m_profileManager->addProfile("release owner",false);
        const auto other=window.m_profileManager->addProfile("release other",false);
        constexpr auto id="release-collision";
        Project first;first.setTargetWindowTitle("Owner");auto* feature=first.addFeature("Owner run");feature->setId(id);
        feature->setRunMode(FeatureRunMode::Hold);feature->setPointerVisualFeedback(false);feature->setRestoreMousePositionOnEnd(true);
        HotkeyBinding binding;binding.virtualKey=VK_XBUTTON1;feature->setHotkey(binding);
        auto wait=std::make_unique<WaitBlock>();wait->ms=10000;feature->workflow().addBlock(std::move(wait));
        Project second;auto* collision=second.addFeature("Other run");collision->setId(id);collision->setPointerVisualFeedback(false);
        if(owner.isEmpty() || other.isEmpty()
            || !JsonSerializer::saveToFile(first,window.m_profileManager->projectPath(owner),window.m_profileManager->projectDirectory(owner))
            || !JsonSerializer::saveToFile(second,window.m_profileManager->projectPath(other),window.m_profileManager->projectDirectory(other)))return failed("fixture persistence");
        const auto settle=[&]{QElapsedTimer t;t.start();while(window.m_profileSwitchPipelineActive && t.elapsed()<4000){QApplication::processEvents();QThread::msleep(1);}return !window.m_profileSwitchPipelineActive;};
        if(!window.executeProfileSwitch(other,false) || !settle())return failed("activate other profile");
        // Change model values directly, without registering a desktop hotkey.
        auto* active=window.m_project->featureById(id);binding.virtualKey=VK_LBUTTON;active->setHotkey(binding);
        SessionInputCleanupController::Ports ports;
        ports.find=[&](const auto& selected)->std::optional<SessionInputCleanupController::Window>{
            ++reads;path=QString::fromStdWString(selected.path);
            return SessionInputCleanupController::Window{reinterpret_cast<HWND>(quintptr(1)),44,L"Owner",L"C:/fixture/owner.exe",true};
        };
        ports.release=[&](const auto&,int key){++releases;lastKey=key;};ports.cleanup=[&](int key){++cleanups;lastKey=key;};
        ports.cursor=[]()->std::optional<QPoint>{return QPoint(-2010,250);};ports.move=[&](QPoint point){++moves;moved=point;};
        window.m_inputCleanup->m_ports=std::move(ports);
        auto prepared=RunLifecycleCoordinator::prepareNewSession(window,*feature,owner,false,true);
        auto& started=prepared.session;started.sessionContext=std::make_shared<ExecutionContext>();
        started.lockedCaptureTargetTitle=L"Owner";started.lockedCaptureTargetProcessPath=L"C:/fixture/owner.exe";
        started.sessionContext->setTargetWindowBindingForWorker(L"foreign context",L"foreign.exe");
        window.captureRunStartCursorPosition(started);
        binding.virtualKey=VK_MBUTTON;feature->setHotkey(binding);feature->setName("Edited after start");
        started.engine->run(std::shared_ptr<Workflow>(feature->workflow().clone()),started.sessionContext);
        // Record ownership only; the fixture never injects a real mouse down.
        started.sessionContext->noteSyntheticMouseDown(MouseButton::Back);started.sessionContext->noteSyntheticMouseUp(MouseButton::Back);
        window.runSessions().emplace(id,std::move(started));
        if(!window.m_profileManager->removeProfile(owner) || window.featureDisplayName(id)!="Owner run")return failed("owner deletion / frozen display name");
        if(physicalRelease)window.onHotkeyHoldEnded(QString::fromLatin1(id));
        else window.stopFeatureRun(id);
        QElapsedTimer drained;drained.start();while(window.sessionFor(id) && drained.elapsed()<4000){window.pruneAbandonedEngines();QApplication::processEvents();QThread::msleep(1);}
        if(window.sessionFor(id) || releases!=1 || reads!=1 || cleanups!=0 || lastKey!=VK_XBUTTON1
            || path!="C:/fixture/owner.exe" || moves!=1 || moved!=QPoint(-2010,250))return failed("release / finalization");
        window.stopAllSessions();if(releases!=1 || moves!=1)return failed("idempotent stop");
        if(!settle())return failed("settle after stop");
        if(window.m_profileManager->activeProfileId()!=original && (!window.executeProfileSwitch(original,false) || !settle()))return failed("restore original profile");
        if(!window.m_profileManager->removeProfile(other))return failed("remove fixture profile");
        if(!window.prepareForShutdown())return failed("shutdown checkpoint");
        std::cout<<"Session input cleanup MainWindow: "<<(physicalRelease?"Hold-ended slot":"explicit stop")
            <<" retains started key/name and owned target after edit/switch/owner deletion; real Hold/Wait finalization releases once and restores cursor through recorded ports OK (offscreen; no native input/movement)\n";
        return true;
    }
};
bool runSessionInputCleanupUiChecks(){return SessionInputCleanupUiChecks::run(false) && SessionInputCleanupUiChecks::run(true);}
