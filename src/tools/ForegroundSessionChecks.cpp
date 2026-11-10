#include "app/ForegroundSessionController.h"
#include "app/CaptureTargetController.h"
#include "app/FeatureRunSession.h"
#include "app/MainWindow.h"
#include "app/RunSessionController.h"
#include "model/Feature.h"
#include "model/Project.h"
#include "core/capture/ScreenCapture.h"
#include "core/workflow/blocks/WaitBlock.h"
#include <QCoreApplication>
#include <QTemporaryDir>
#include <QTimer>
#include <iostream>
#include <map>

namespace {
using Controller=ForegroundSessionController;
using Handle=Controller::Handle;
Handle handle(int n){return reinterpret_cast<Handle>(quintptr(n));}
bool check(bool value,const char* text){if(!value)std::cerr<<"Foreground session FAILED: "<<text<<'\n';return value;}
struct Fixture {
    QTemporaryDir dir;ProfileManager profiles{dir.path()};
    QString owner,other;Controller::Snapshot state;ForegroundWindowState fg,lastSwitch;
    std::map<Handle,CaptureTargetController::Window> windows;
    int synchronize=0,flush=0,reconcile=0,resume=0,ensure=0,details=0,latches=0,hints=0,switches=0,desktop=0,applies=0,gateCalls=0;
    bool modifier=false,allowed=true;QStringList order;
    ForegroundRunGate::GateInput lastInput;std::optional<ForegroundRunGate::FeatureGateOptions> lastOptions;
    std::unique_ptr<CaptureTargetController> targets;std::unique_ptr<Controller> controller;
    bool initialize() {
        if(!profiles.initialize())return false;owner=profiles.addProfile("fg owner",true);other=profiles.addProfile("fg other",false);
        if(owner.isEmpty() || other.isEmpty() || !profiles.updateProfileTargetBinding(owner,"Main","main.exe",true)
            || !profiles.updateProfileSubTargetBinding(owner,"Sub","sub.exe",true)
            || !profiles.updateProfileTargetBinding(other,"Other","other.exe",true))return false;
        windows={{handle(1),{handle(1),"Main","main.exe",1,true,false}},
            {handle(2),{handle(2),"Sub","sub.exe",2,true,false}},{handle(3),{handle(3),"Other","other.exe",3,true,false}}};
        fg.rootHwnd=handle(1);fg.title="Main";fg.processPath="main.exe";fg.monotonicSeq=1;state.shutdown=false;
        CaptureTargetController::Ports capture;
        capture.read=[&](Handle hwnd)->std::optional<CaptureTargetController::Window>{const auto it=windows.find(hwnd);return it==windows.end()?std::nullopt:std::optional<CaptureTargetController::Window>(it->second);};
        capture.find=[&](const QString& title,const QString& path)->std::optional<CaptureTargetController::Window>{for(const auto& [id,w]:windows)if(path.isEmpty()?w.title==title:w.path==path)return w;return std::nullopt;};
        capture.apply=[&](const auto&){++applies;};
        targets=std::make_unique<CaptureTargetController>(profiles,capture);
        CaptureTargetController::Host targetHost;targetHost.foreground=[&]{return fg;};targetHost.foregroundAllowed=[](const QString&){return true;};
        targetHost.mainTitle=[&]{const auto* p=profiles.profileById(profiles.activeProfileId());return p?p->targetWindowTitle:QString{};};
        targetHost.mayApply=[&]{return !state.shutdown && !state.pipeline && !state.historyBusy;};targets->setHost(targetHost);
        Controller::Ports ports;
        ports.resolve=[&](const auto& value){return ProfileForegroundResolver::ResolveResult{ProfileForegroundResolver::profileIdForProcessPath(profiles,value.processPath),ProfileForegroundResolver::MatchKind::ProcessPath};};
        ports.readWindow=[&](Handle hwnd,bool full){const auto it=windows.find(hwnd);return Controller::Window{it!=windows.end(),hwnd==handle(9),hwnd==handle(8),full && it!=windows.end()?it->second.path:QString{}};};
        ports.gate=[&](Controller::Gate,const auto& input){++gateCalls;lastInput=input;lastInput.feature=nullptr;
            lastOptions=input.feature?std::optional<ForegroundRunGate::FeatureGateOptions>(*input.feature):std::nullopt;return allowed;};
        controller=std::make_unique<Controller>(profiles,*targets,ports);controller->setHost(host());return true;
    }
    Controller::Host host() {
        Controller::Host host;host.snapshot=[&]{return state;};host.foreground=[&]{return fg;};host.modifierHeld=[&]{return modifier;};
        host.synchronize=[&]{++synchronize;order<<"sync";};host.flushDeferred=[&]{++flush;order<<"flush";};
        host.reconcileSessions=[&]{++reconcile;order<<"reconcile";};host.resumeSessions=[&]{++resume;order<<"resume";};
        host.ensureTriggers=[&]{++ensure;order<<"ensure";};host.updateDetails=[&]{++details;order<<"details";};
        host.resetLatch=[&]{++latches;};host.syncDesktop=[&]{++desktop;};
        host.applySwitch=[&](const auto& value){++switches;lastSwitch=value;};
        host.executeSwitch=[&](const QString& id,bool){++switches;return profiles.setActiveProfile(id);};
        host.captureHints=[&](Handle,const QString&){++hints;};return host;
    }
};
bool notifications() {
    Fixture f;if(!f.initialize())return false;
    f.controller->changed(f.fg);f.controller->changed(f.fg);
    if(!check(f.hints==2 && f.switches==0 && f.latches==1 && f.reconcile==2,"same-profile fast path resets latch once per HWND and finishes gate"))return false;
    f.fg.rootHwnd=handle(2);f.fg.title="Sub";f.fg.processPath="sub.exe";++f.fg.monotonicSeq;f.controller->changed(f.fg);
    if(!check(f.latches==2 && f.hints==3,"same-profile main-to-sub foreground change cannot bypass latch reset"))return false;
    f.controller->resetLatchFor(handle(9));f.controller->resetLatchFor(handle(8));f.controller->resetLatchFor(handle(2));
    if(!check(f.latches==3,"PIPBONG clears prior latch identity; transient shell never resets a feature latch"))return false;
    f.allowed=false;f.controller->resetLatchFor(handle(3));
    if(!check(f.latches==3,"foreign/unmatched window cannot reset registered feature latch"))return false;
    f.state.historyBusy=true;const auto before=f.hints+f.switches+f.reconcile;f.controller->changed(f.fg);
    if(!check(before==f.hints+f.switches+f.reconcile,"history transaction exports worker state without mutating foreground routing"))return false;
    f.state.historyBusy=false;f.fg.rootHwnd=handle(3);f.fg.title="Other";f.fg.processPath="other.exe";f.fg.monotonicSeq=10;
    ForegroundWindowState stale;stale.rootHwnd=handle(1);stale.processPath="main.exe";stale.monotonicSeq=1;
    f.controller->changed(stale);
    return check(f.switches==1 && f.lastSwitch.monotonicSeq==10 && f.lastSwitch.processPath=="other.exe",
        "newer cached foreground supersedes a stale queued notification");
}
bool scheduling() {
    Fixture f;if(!f.initialize())return false;
    auto manualHost=f.host();manualHost.syncDesktop=[&] {
        ++f.desktop;f.fg.rootHwnd=handle(3);f.fg.title="Other";f.fg.processPath="other.exe";++f.fg.monotonicSeq;
        f.controller->changed(f.fg); // A synchronous signal variant.
        const auto notification=f.fg;
        QTimer::singleShot(0,f.controller.get(),[&,notification]{f.controller->changed(notification);}); // Production monitor coalesces its signal.
    };f.controller->setHost(manualHost);
    for(int n=0;n<20;++n)f.controller->postSwitch(true);f.controller->postSwitch(false);f.controller->postSwitch(true);
    QCoreApplication::processEvents();QCoreApplication::processEvents();
    if(!check(f.desktop==1 && f.switches==0 && f.controller->findChildren<QTimer*>().size()==1,
        "manual completion dominates automatic burst and synchronous/queued desktop-read signals using one owned timer"))return false;
    f.controller->postSwitch(true);QCoreApplication::processEvents();
    if(!check(f.switches==1,"a later independent automatic request is retained"))return false;
    f.order.clear();auto host=f.host();host.flushDeferred=[&]{++f.flush;f.state.pipeline=true;};f.controller->setHost(host);
    const int oldResume=f.resume;f.controller->finishGate();
    if(!check(f.resume==oldResume && f.state.pipeline,"flush starting a profile pipeline cancels immediate reconciliation/resume"))return false;
    f.state.pipeline=false;f.controller->setHost(f.host());f.order.clear();f.controller->finishGate();
    if(!check(f.order==QStringList{"sync","flush","reconcile","resume","ensure","details"},"idle finish order exports gates before resuming and details"))return false;
    f.modifier=true;const int oldFlush=f.flush;f.controller->finishGate();
    if(!check(f.flush==oldFlush,"modifier-held finish does not flush deferred profile switch"))return false;
    f.controller->altReleased();if(!check(f.flush==oldFlush+1,"Alt release explicitly flushes deferred transition"))return false;
    f.controller->postSwitch(true);const int oldSwitch=f.switches;f.controller->stop();QCoreApplication::processEvents();
    if(!check(f.switches==oldSwitch,"shutdown cancels queued foreground reconciliation"))return false;
    Fixture dead;if(!dead.initialize())return false;auto deadHost=dead.host();deadHost.syncDesktop=[&]{dead.controller.reset();};dead.controller->setHost(deadHost);
    dead.controller->postSwitch(false);QCoreApplication::processEvents();
    return check(!dead.controller && dead.reconcile==0 && dead.details==0,"owner deletion during queued desktop sync drops remaining callbacks");
}
bool composition() {
    Fixture f;if(!f.initialize() || !f.profiles.setActiveProfile(f.other))return false;
    Feature feature("scoped trigger");feature.setRunMode(FeatureRunMode::Trigger);feature.setTriggerRunWithoutTargetForeground(true);
    feature.setCaptureTargetScope(FeatureCaptureTargetScope::SubOnly);feature.setRequireScopedTargetForeground(true);
    const auto input=f.controller->gateInput(&feature,f.owner);
    if(!check(input.bindings.activeProfileId==f.owner && input.bindings.mainProcessPath=="main.exe"
        && input.bindings.subProcessPath=="sub.exe" && input.foregroundResolvedProfileId==f.owner && input.triggerBackgroundVisible,
        "gate composition uses requested owner's titles/processes/background visibility while another profile is active"))return false;
    if(!check(f.controller->runGate(&feature) && f.lastOptions && f.lastOptions->runMode==FeatureRunMode::Trigger
        && f.lastOptions->captureTargetScope==FeatureCaptureTargetScope::SubOnly && f.lastOptions->requireScopedTargetForeground,
        "feature options are composed and evaluated synchronously"))return false;
    FeatureRunSession background;background.profileId=f.owner;
    f.allowed=false;const int calls=f.gateCalls;
    if(!check(f.controller->sessionGate(background,&feature) && f.gateCalls==calls,"existing concurrent background-profile session policy stays independent of active foreground gate"))return false;
    f.state.pipeline=true;
    if(!check(!f.controller->sessionGate(background,&feature) && !f.controller->runGate(&feature),"profile install blocks session resume including background owner"))return false;
    f.state.pipeline=false;f.state.historyBusy=true;
    if(!check(!f.controller->sessionGate(background,&feature),"history transaction blocks pending foreground resume"))return false;
    f.state.historyBusy=false;
    if(!check(f.controller->profileForWindow(handle(1))==f.owner && f.controller->profileForWindow(handle(99)).isEmpty(),"validated HWND process lookup maps to owner or no profile"))return false;
    if(!f.profiles.removeProfile(f.owner) || !check(!f.controller->sessionGate(background,&feature),"deleted background owner cannot bypass gate"))return false;
    return true;
}
bool stoppedResume() {
    RunSessionController runs;std::map<std::string,FeatureRunSession> sessions;Feature feature("stopped poll");int launches=0;
    FeatureRunSession session;session.featureId="stopped";session.repeatSession=true;session.userStopRequested=true;
    session.waitingForScopedTargetForeground=true;sessions.emplace("stopped",std::move(session));runs.setRunSessions(&sessions);
    RunSessionController::HostCallbacks host;host.featureForSession=[&](auto&){return &feature;};
    host.runForegroundGateActive=[](Feature*){return true;};host.launchWorkflowRun=[&](auto&,Feature*,bool){++launches;};runs.setHostCallbacks(host);
    runs.resumeWaitingScopedTargetForegroundSessions();
    return check(launches==0 && !sessions.at("stopped").waitingForScopedTargetForeground,"queued resume never relaunches explicitly stopped session");
}
}
bool runForegroundSessionChecks() {
    if(!notifications() || !scheduling() || !composition() || !stoppedResume())return false;
    std::cout<<"Foreground session: same-profile latch reset, stale-state selection, manual-first coalescing, transition/history/shutdown gates, callback lifetime, owner-scoped composition and stopped resume OK (scripted ports; no native activation/input)\n";
    return true;
}

class ForegroundSessionUiChecks {
public:
    static bool run() {
        int latches=0,hints=0,reconciles=0,resumes=0;bool allowed=true;
        ForegroundWindowState fg;fg.rootHwnd=handle(101);fg.title="fixture";fg.processPath="fixture.exe";fg.monotonicSeq=1;
        MainWindow window;auto& controller=*window.m_foregroundSessions;
        controller.m_ports.resolve=[&](const auto&){return ProfileForegroundResolver::ResolveResult{window.m_profileManager->activeProfileId(),ProfileForegroundResolver::MatchKind::ProcessPath};};
        controller.m_ports.readWindow=[](Handle hwnd,bool){return Controller::Window{hwnd!=nullptr,false,false,{}};};
        controller.m_ports.gate=[&](auto,const auto&){return allowed;};
        controller.m_host.foreground=[&]{return fg;};controller.m_host.modifierHeld=[] {return false;};
        controller.m_host.resetLatch=[&]{++latches;};controller.m_host.captureHints=[&](Handle,const QString&){++hints;};
        controller.m_host.flushDeferred=[]{};controller.m_host.applySwitch=[](const auto&){};
        const auto originalReconcile=controller.m_host.reconcileSessions,originalResume=controller.m_host.resumeSessions;
        controller.m_host.reconcileSessions=[&]{++reconciles;originalReconcile();};controller.m_host.resumeSessions=[&]{++resumes;originalResume();};
        auto* feature=window.m_project->addFeature("foreground Wait fixture");feature->setId("foreground-wait");feature->setPointerVisualFeedback(false);
        FeatureRunSession session;session.featureId=feature->id();session.profileId=window.m_profileManager->activeProfileId();
        session.runningMode=FeatureRunMode::RepeatInfinite;session.repeatSession=true;session.waitingForScopedTargetForeground=true;
        session.sessionContext=std::make_shared<ExecutionContext>();session.engine=std::make_unique<WorkflowEngine>();
        auto workflow=std::make_shared<Workflow>();auto wait=std::make_unique<WaitBlock>();wait->ms=10000;workflow->addBlock(std::move(wait));
        session.engine->run(workflow,session.sessionContext);window.runSessions().emplace(feature->id(),std::move(session));
        auto* running=window.sessionFor("foreground-wait");
        window.onForegroundStateChanged(fg);fg.rootHwnd=handle(102);++fg.monotonicSeq;window.onForegroundStateChanged(fg);
        if(latches!=2 || hints!=2 || running->waitingForScopedTargetForeground || !running->engine->isRunning())return false;
        running->waitingForScopedTargetForeground=true;controller.m_host.flushDeferred=[&]{window.m_profileSwitchPipelineActive=true;};
        const int oldReconcile=reconciles,oldResume=resumes;window.finishForegroundSessionGate();
        if(reconciles!=oldReconcile || resumes!=oldResume || !window.m_profileSwitchPipelineActive)return false;
        window.m_runSessionController.resumeWaitingScopedTargetForegroundSessions();
        if(!running->waitingForScopedTargetForeground)return false;
        window.m_profileSwitchPipelineActive=false;controller.m_host.flushDeferred=[]{};
        window.finishForegroundSessionGate();
        if(running->waitingForScopedTargetForeground || !running->engine->isRunning())return false;
        running->userStopRequested=true;running->waitingForScopedTargetForeground=true;window.m_runSessionController.resumeWaitingScopedTargetForegroundSessions();
        if(running->waitingForScopedTargetForeground || !running->engine->stopAndWaitBounded(1000))return false;
        window.runSessions().erase("foreground-wait");
        if(!window.prepareForShutdown())return false;
        const int stopped=latches;window.onForegroundStateChanged(fg);
        if(latches!=stopped)return false;
        std::cout<<"Foreground MainWindow: actual notification delegates, same-profile HWND latch change, real Wait-engine resume, pipeline-start/poll deferral, stopped-session skip and shutdown guard OK (offscreen; scripted foreground; no native activation/input)\n";
        return true;
    }
};
bool runForegroundSessionUiChecks(){return ForegroundSessionUiChecks::run();}
