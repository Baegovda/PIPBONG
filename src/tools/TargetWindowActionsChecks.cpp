#include "app/TargetWindowActionsController.h"
#include "app/FeatureHotkeyGate.h"
#include "app/MainWindow.h"
#include "app/ForegroundWindowMonitor.h"
#include "app/ProjectWorkspaceController.h"
#include "core/capture/WindowPicker.h"
#include "core/capture/ScreenCapture.h"
#include "core/workflow/Workflow.h"
#include "core/workflow/blocks/WaitBlock.h"
#include "model/Project.h"
#include "storage/JsonSerializer.h"
#include <QApplication>
#include <QDir>
#include <QElapsedTimer>
#include <QThread>
#include <QTimer>
#include <QWidget>
#include <iostream>
#include <vector>

namespace {
using Controller=TargetWindowActionsController;
using Role=Controller::Role;
bool check(bool value,const char* what) {if(!value)std::cerr<<"Target actions FAILED: "<<what<<'\n';return value;}
Controller::Handle marker(int id) {return reinterpret_cast<Controller::Handle>(quintptr(id));}
struct Harness {
    Controller::Snapshot source{"one","Old","OldSub","old.exe","old-sub.exe",7,false,marker(99)};
    Controller::Selection picked{marker(1),"New","new.exe",42};
    Controller::Selection live=picked;
    std::vector<Controller::Completion> completions;
    QString persistent,lastStatus,lastWarning,error,foundTitle,foundPath;
    Role flashedRole=Role::Main;
    int attempts=0,commits=0,flashes=0,cancels=0,reads=0;
    bool readable=true,blocked=false,checkpoint=true,gateObserved=false;
    std::function<void()> duringCheckpoint;
    Controller::Ports ports() {
        return {
            [this](Role,Controller::Completion done) {gateObserved=FeatureHotkeyGate::isFeatureHotkeysBlocked();completions.push_back(std::move(done));},
            [this] {++cancels;},
            [this](const Controller::Snapshot&,Role)->std::optional<Controller::Selection> {gateObserved=FeatureHotkeyGate::isFeatureHotkeysBlocked();return picked;},
            [this](Controller::Handle)->std::optional<Controller::Selection> {++reads;return readable?std::optional<Controller::Selection>(live):std::nullopt;},
            [this](const QString& title,const QString& path) {foundTitle=title;foundPath=path;return live.hwnd;},
            [this](const Controller::Selection&,Role role,bool) {++flashes;flashedRole=role;return true;}};
    }
    Controller::Host host() {
        Controller::Host host;
        host.snapshot=[this]{return source;};
        host.blockedReason=[this](bool){return blocked || FeatureHotkeyGate::isFeatureHotkeysBlocked()?QStringLiteral("blocked"):QString();};
        host.checkpoint=[this]{if(duringCheckpoint)duringCheckpoint();return checkpoint;};
        host.commit=[this](const Controller::Snapshot&,Role role,const Controller::Selection& value) {
            ++attempts;if(!error.isEmpty())return error;
            (role==Role::Sub?source.subTitle:source.mainTitle)=value.title;
            (role==Role::Sub?source.subProcessPath:source.mainProcessPath)=value.processPath;
            return QString();
        };
        host.committed=[this](const Controller::Selection&,Role){++commits;};
        host.persistent=[this](const QString& text){persistent=text;};
        host.status=[this](const QString& text){lastStatus=text;};
        host.warning=[this](const QString& text){lastWarning=text;};
        return host;
    }
    void complete() {auto done=completions.back();done(picked);}
};
bool standalone() {
    Harness h;Controller controller(h.ports());controller.setHost(h.host());
    controller.pick(Role::Main);
    if(!check(controller.pending() && h.gateObserved && !h.persistent.isEmpty(),"click picker pins hotkeys and owns persistent prompt"))return false;
    h.complete();QApplication::processEvents();
    if(!check(!controller.pending() && !FeatureHotkeyGate::isFeatureHotkeysBlocked() && h.persistent.isEmpty()
        && h.attempts==1 && h.commits==1 && h.flashes==1,"successful commit releases gate and queues one wave"))return false;
    const auto baseline=h.attempts;
    controller.pick(Role::Main);++h.source.generation;h.complete();
    if(!check(h.attempts==baseline,"same-profile reload rejects stale completion"))return false;
    controller.pick(Role::Main);h.source.profileId="other";h.complete();
    if(!check(h.attempts==baseline,"different profile rejects stale completion"))return false;
    controller.pick(Role::Main);auto old=h.completions.back();controller.pick(Role::Sub);
    old(h.picked);
    if(!check(controller.pending() && FeatureHotkeyGate::isFeatureHotkeysBlocked(),"older completion cannot clear newer pending gate"))return false;
    controller.cancel();h.complete();
    if(!check(h.attempts==baseline && !controller.pending() && !FeatureHotkeyGate::isFeatureHotkeysBlocked(),"cancelled completion is inert"))return false;
    controller.pick(Role::Main);
    {FeatureHotkeyGateScope editor;h.complete();}
    if(!check(h.attempts==baseline,"editor opened during pick rejects completion"))return false;
    h.live.processId=43;controller.pick(Role::Main);h.complete();
    if(!check(h.attempts==baseline,"HWND reused by another process is rejected"))return false;
    h.live=h.picked;h.live.processPath="other.exe";controller.pick(Role::Main);h.complete();
    if(!check(h.attempts==baseline,"list executable identity is revalidated"))return false;
    h.live=h.picked;h.readable=false;controller.pick(Role::Main);h.complete();h.readable=true;
    if(!check(h.attempts==baseline,"closed/unreadable window cannot commit"))return false;
    h.checkpoint=false;controller.pick(Role::Main);h.complete();h.checkpoint=true;
    if(!check(h.attempts==baseline,"snapshot failure aborts binding"))return false;
    h.duringCheckpoint=[&]{++h.source.generation;};controller.pick(Role::Main);h.complete();h.duringCheckpoint={};
    if(!check(h.attempts==baseline,"document change inside checkpoint event loop is rejected"))return false;
    h.duringCheckpoint=[&]{h.live.processId=44;};controller.pick(Role::Main);h.complete();h.duringCheckpoint={};h.live=h.picked;
    if(!check(h.attempts==baseline,"window identity change inside checkpoint is rejected"))return false;
    h.error="save refused";controller.pick(Role::Main);h.complete();QApplication::processEvents();
    if(!check(h.attempts==baseline+1 && h.commits==1 && h.flashes==1 && h.lastWarning==h.error,"save failure emits no success or highlight"))return false;
    h.error.clear();controller.pick(Role::Sub,true);QApplication::processEvents();
    if(!check(h.commits==2 && h.gateObserved && h.flashedRole==Role::Sub,"list picker uses same gated commit path"))return false;
    controller.show(Role::Sub);
    if(!check(h.foundTitle==h.source.subTitle && h.foundPath==h.source.subProcessPath && h.flashedRole==Role::Sub,"sub show passes executable constraint"))return false;
    h.live.processPath="wrong.exe";const auto flashes=h.flashes;controller.show(Role::Sub);
    if(!check(h.flashes==flashes,"wrong-process show result is not highlighted"))return false;
    h.live=h.picked;controller.pick(Role::Main);h.complete();++h.source.generation;QApplication::processEvents();
    if(!check(h.flashes==flashes,"queued wave is dropped after model change"))return false;
    {
        Harness dead;auto owner=std::make_unique<Controller>(dead.ports());owner->setHost(dead.host());owner->pick(Role::Main);
        auto callback=dead.completions.back();owner.reset();callback(dead.picked);
        if(!check(dead.attempts==0 && dead.cancels==1 && !FeatureHotkeyGate::isFeatureHotkeysBlocked(),"destroyed owner cancels pick and drops completion"))return false;
    }
    {
        Harness dead;auto owner=std::make_unique<Controller>(dead.ports());owner->setHost(dead.host());
        dead.duringCheckpoint=[&]{owner.reset();};owner->pick(Role::Main);dead.complete();
        if(!check(!owner && dead.attempts==0,"owner destruction during nested checkpoint is safe"))return false;
    }
    int deadCalls=0,liveCalls=0;
    auto* widget=new QWidget;
    WindowPicker::dispatchCompletion(widget,[&](const auto&){++deadCalls;},{});delete widget;
    QWidget alive;WindowPicker::dispatchCompletion(&alive,[&](const auto&){++liveCalls;},{});
    QApplication::processEvents();
    return check(deadCalls==0 && liveCalls==1,"picker dispatcher is queued and widget-lifetime-bound without installing hooks");
}
}

class TargetWindowActionsChecks {
public:
    static bool run(const QString& folder) {
        if(QApplication::platformName()!=QStringLiteral("offscreen") || !standalone())return false;
#ifdef _WIN32
        const auto oldFocus=ProgramSettings::focusTargetWindowOnProfileSelect();ProgramSettings::setFocusTargetWindowOnProfileSelect(false);
        struct Restore {bool value;~Restore(){ProgramSettings::setFocusTargetWindowOnProfileSelect(value);}} restore{oldFocus};
        MainWindow window;window.m_foregroundMonitor->stop();
        ProfileSwitchCoordinator::HostCallbacks quiet;quiet.isWorkspacePinned=[]{return true;};quiet.flushDeferredProfileSwitchIfIdle=[]{};
        window.m_profileSwitchCoordinator.setHostCallbacks(quiet);
        const auto original=window.m_profileManager->activeProfileId();
        const auto id=window.m_profileManager->addProfile("target action integration",false);
        Project project;project.setTargetWindowTitle("Old");project.addFeature("preserved");
        if(id.isEmpty() || !JsonSerializer::saveToFile(project,window.m_profileManager->projectPath(id),window.m_profileManager->projectDirectory(id))
            || !window.m_profileManager->updateProfileTargetBinding(id,"Old","old.exe",true)
            || !window.executeProfileSwitch(id,false))return false;
        QElapsedTimer timer;timer.start();
        while(window.m_profileSwitchPipelineActive && timer.elapsed()<4000) {QApplication::processEvents();QThread::msleep(1);}
        if(window.m_profileSwitchPipelineActive)return false;
        Harness ports;window.m_targetActions->m_ports=ports.ports();
        window.onPickTargetWindow();
        if(!check(ports.gateObserved && !window.executeProfileSwitch(original,false),"actual main-window picker pins profile transition"))return false;
        // That manual request is deferred, rather than discarded. Explicitly keep this profile
        // before continuing the binding assertions; slow history I/O must not race its retry.
        if(window.executeProfileSwitch(id,false))return false;
        ports.complete();QApplication::processEvents();
        auto stored=JsonSerializer::loadFromFile(window.m_profileManager->projectPath(id));
        if(!check(stored && stored->targetWindowTitle()=="New" && window.m_project->targetWindowTitle()=="New"
            && window.m_profileManager->linkedTargetProcessPath(id)=="new.exe" && ports.flashes==1,
            "production wiring persists main title/process before wave"))return false;
        // Lock settings only after the real history checkpoint has completed.
        const auto settings=QDir(window.m_profileManager->profileDirectory(id)).filePath("profile-settings.json");
        HANDLE locked=INVALID_HANDLE_VALUE;
        auto reader=ports.ports().read;int reads=0;
        window.m_targetActions->m_ports.read=[&](HWND hwnd) {
            if(++reads==2)locked=CreateFileW(reinterpret_cast<LPCWSTR>(settings.utf16()),GENERIC_READ,FILE_SHARE_READ,nullptr,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,nullptr);
            return reader(hwnd);
        };
        ports.picked.title="Rejected";ports.live=ports.picked;
        window.onPickTargetWindow();ports.complete();
        const bool validLock=locked!=INVALID_HANDLE_VALUE;if(validLock)CloseHandle(locked);
        stored=JsonSerializer::loadFromFile(window.m_profileManager->projectPath(id));
        if(!check(validLock && stored && stored->targetWindowTitle()=="New" && window.m_project->targetWindowTitle()=="New"
            && window.m_profileManager->targetWindowTitle(id)=="New" && ports.flashes==1,
            "actual settings failure rolls main project/model back and emits no wave"))return false;
        window.grab().save(QDir(folder).filePath("ui-target-save-failure.png"));
        window.m_targetActions->m_ports=ports.ports();ports.picked.title="Sub";ports.live=ports.picked;
        window.onPickSubTargetWindow();ports.complete();QApplication::processEvents();
        if(!check(window.m_profileManager->subTargetWindowTitle(id)=="Sub" && ports.flashedRole==Role::Sub,"actual sub commit updates saved metadata"))return false;
        FeatureRunSession session;session.featureId="binding-owner";session.profileId=id;session.engine=std::make_unique<WorkflowEngine>();
        session.sessionContext=std::make_shared<ExecutionContext>();auto flow=std::make_shared<Workflow>();
        auto wait=std::make_unique<WaitBlock>();wait->ms=5000;flow->addBlock(std::move(wait));
        session.engine->run(flow,session.sessionContext);auto* engine=session.engine.get();window.runSessions().emplace(session.featureId,std::move(session));
        const auto picks=ports.completions.size();window.onPickTargetWindow();
        const bool prevented=ports.completions.size()==picks;
        engine->stopAndWaitBounded(1000);window.runSessions().erase("binding-owner");
        if(!check(prevented,"actual running engine rejects rebind before opening picker") || !window.prepareForShutdown())return false;
#endif
        std::cout<<"Target actions: pending/editor/owner/epoch/PID/exe/checkpoint/save guards, main/sub constrained highlights, queued-wave cancellation and owner lifetime OK (offscreen; scripted ports; no native hooks/activation/move/input)\n";
        std::cout<<"Target actions MainWindow: production history/workspace binding, actual locked-settings rollback, success-only wave, profile pin and real Wait-engine rebind rejection OK (offscreen; scripted windows)\n";
        return true;
    }
};
bool runTargetWindowActionsChecks(const QString& folder) {return TargetWindowActionsChecks::run(folder);}
