#include "app/MainWindow.h"
#include "app/ProjectWorkspaceController.h"
#include "app/ProfileTransitionController.h"
#include "app/FeatureHotkeyGate.h"
#include "app/ForegroundWindowMonitor.h"
#include "app/ProfileSwitchCoordinator.h"
#include "model/Project.h"
#include "storage/JsonSerializer.h"
#include "ui/FeatureListPanel.h"
#include "ui/editors/FeatureEditDialog.h"
#include <QApplication>
#include <QDir>
#include <QElapsedTimer>
#include <QTemporaryDir>
#include <QThread>
#include <QTimer>
#include <iostream>

namespace {
bool until(const std::function<bool()>& predicate) {
    QElapsedTimer timer;timer.start();
    while(!predicate() && timer.elapsed()<4000) { QCoreApplication::processEvents();QThread::msleep(1); }
    return predicate();
}
bool saveProfile(ProfileManager& profiles,const QString& id,const char* name) {
    Project project;project.addFeature(name);project.setTargetWindowTitle(name);
    return JsonSerializer::saveToFile(project,profiles.projectPath(id),profiles.projectDirectory(id));
}
}

class ProjectWorkspaceUiChecks {
public:
    static bool run() {
        if(QApplication::platformName()!=QStringLiteral("offscreen")) return false;
        const auto oldFocus=ProgramSettings::focusTargetWindowOnProfileSelect();
        ProgramSettings::setFocusTargetWindowOnProfileSelect(false);
        struct Restore { bool value;~Restore(){ProgramSettings::setFocusTargetWindowOnProfileSelect(value);} } restore{oldFocus};
        {
            QTemporaryDir fixture;ProfileManager profiles(fixture.path());
            if(!profiles.initialize()) return false;
            const auto one=profiles.addProfile("one",false),two=profiles.addProfile("two",false);
            if(!saveProfile(profiles,one,"one") || !saveProfile(profiles,two,"two")
                || !profiles.updateProfileTargetBinding(one,"one","C:/__test_one__.exe")
                || !profiles.updateProfileTargetBinding(two,"two","C:/__test_two__.exe")) return false;
            bool pinned=true,accept=true;int executions=0;
            ForegroundWindowState live;
            live.rootHwnd=reinterpret_cast<HWND>(quintptr(1)); // Invalid marker: no native window is controlled.
            live.processPath="C:/__test_one__.exe";live.title="one";live.monotonicSeq=7;
            ProfileSwitchCoordinator coordinator;
            coordinator.setProfileManager(&profiles);
            ProfileSwitchCoordinator::HostCallbacks host;
            host.isWorkspacePinned=[&]{return pinned;};
            host.foregroundState=[&]{return live;};host.transitionModifierHeld=[] {return false;};
            host.flushDeferredProfileSwitchIfIdle=[&]{coordinator.flushDeferredProfileSwitch();};
            QString last;
            host.executeProfileSwitch=[&](const QString& id,bool){++executions;last=id;return accept;};
            coordinator.setHostCallbacks(host);
            coordinator.applyFromForegroundState(live);
            for(int i=0;i<30;++i) coordinator.flushDeferredProfileSwitch();
            if(executions || coordinator.deferredProfileSwitchId()!=one
                || coordinator.findChildren<QTimer*>().size()!=1) return false;
            live.processPath="C:/__test_two__.exe";live.title="two";live.monotonicSeq=8;pinned=false;
            coordinator.flushDeferredProfileSwitch();
            if(!until([&]{return executions==1;}) || last!=two || !coordinator.deferredProfileSwitchId().isEmpty()) return false;
            accept=false;
            live.processPath="C:/__test_one__.exe";live.title="one";live.monotonicSeq=9;
            coordinator.setDeferredProfileSwitchId(one);coordinator.flushDeferredProfileSwitch();
            if(!until([&]{return executions==2;}) || last!=one || coordinator.deferredProfileSwitchId()!=one
                || coordinator.findChild<QTimer*>()->isActive()) return false;
        }
        {
            MainWindow window;
            window.m_foregroundMonitor->stop();
            // Keep unrelated real desktop foreground changes out of this offscreen fixture.
            ProfileSwitchCoordinator::HostCallbacks quiet;
            quiet.isWorkspacePinned=[] {return true;};
            quiet.flushDeferredProfileSwitchIfIdle=[] {};
            window.m_profileSwitchCoordinator.setHostCallbacks(quiet);
            const auto original=window.m_profileManager->activeProfileId();
            const auto other=window.m_profileManager->addProfile("modal target",false);
            if(!saveProfile(*window.m_profileManager,other,"modal target")) return false;
            if(window.m_project->features().empty()) window.m_project->addFeature("editor-owned feature");
            const auto featureId=window.m_project->features().front()->id();
            window.m_featureList->refresh();window.m_featureList->selectFeatureById(QString::fromStdString(featureId));
            auto* current=window.m_project.get();
            bool inspected=false,blocked=false;
            QTimer::singleShot(0,&window,[&] {
                for(auto* top:QApplication::topLevelWidgets()) {
                    if(auto* editor=qobject_cast<FeatureEditDialog*>(top);editor && editor->isVisible()) {
                        inspected=true;
                        blocked=FeatureHotkeyGate::isFeatureHotkeysBlocked()
                            && !window.executeProfileSwitch(other,true)
                            && !window.m_projectWorkspace->loadActive(true,false)
                            && window.m_project.get()==current && window.m_profileManager->activeProfileId()==original;
                        QMetaObject::invokeMethod(editor,"reject",Qt::DirectConnection);break;
                    }
                }
            });
            if(!QMetaObject::invokeMethod(window.m_featureList,"onEditFeature",Qt::DirectConnection)
                || !inspected || !blocked || window.m_project->featureById(featureId)==nullptr) return false;
            window.m_profileSwitchCoordinator.clearDeferredProfileSwitchId();
            if(!window.executeProfileSwitch(other,false) || window.m_profileManager->activeProfileId()!=other
                || window.m_projectWorkspace->state().profileId!=other
                || window.m_project->features().front()->name()!="modal target") return false;
            if(!until([&]{return !window.m_profileSwitchPipelineActive;})) return false;
            if(!window.executeProfileSwitch(original,false)
                || !until([&]{return !window.m_profileSwitchPipelineActive;}) || !window.prepareForShutdown()) return false;
        }
        std::cout<<"Project workspace UI: actual feature editor pins model/selection, safe manual commit after close, latest foreground candidate, one deferred timer and retained failed candidate OK (offscreen; synthetic foreground; no native activation/input)\n";
        return true;
    }
};
bool runProjectWorkspaceUiChecks() {return ProjectWorkspaceUiChecks::run();}
