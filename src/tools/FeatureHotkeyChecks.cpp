#include "app/FeatureHotkeyController.h"
#include "app/MainWindow.h"
#include "app/HotkeyManager.h"
#include "app/ForegroundWindowMonitor.h"
#include "app/ForegroundSessionController.h"
#include "app/ProjectWorkspaceController.h"
#include "app/RunLifecycleCoordinator.h"
#include "app/FeatureHotkeyGate.h"
#include "model/Project.h"
#include "model/Feature.h"
#include "core/workflow/blocks/WaitBlock.h"
#include <QApplication>
#include <QElapsedTimer>
#include <QThread>
#include <iostream>

namespace {
using C=FeatureHotkeyController;
bool check(bool result,const char* message){if(!result)std::cerr<<"Feature hotkey FAILED: "<<message<<'\n';return result;}
bool core() {
    C c;Project project,other;auto* f=project.addFeature("first");f->setId("same");
    auto* collision=other.addFeature("second");collision->setId("same");
    C::Workspace source{&project,"owner",1};C::Gate gate;gate.unavailable=false;
    C::Host h;int starts=0,stops=0,queued=0,canceled=0,finished=0,released=0,lanes=0,repeats=0,depth=0,drains=0;
    C::Block denied=C::Block::None;bool running=false,holdBlocked=false,available=false,replaceOnRelease=false;
    FeatureRunSession s;s.featureId="same";s.profileId="owner";
    h.workspace=[&]{return source;};h.gate=[&](const auto&){return gate;};h.blocked=[&](auto reason){denied=reason;};
    h.running=[&](const auto&){return running;};h.start=[&](const auto&){++starts;};h.stop=[&](const auto&){++stops;};
    h.session=[&](const auto&)->FeatureRunSession*{return available?&s:nullptr;};
    h.holdBlocked=[&](const auto&){return holdBlocked;};h.erase=[&](const auto&){available=false;};
    h.enterBurst=[&]{++depth;};h.leaveBurst=[&]{--depth;++drains;};
    h.queueStart=[&](const auto&){++queued;};h.cancelStart=[&](const auto&){++canceled;};
    h.queueFinish=[&](const auto&){++finished;};h.repeat=[&](auto&){++repeats;};
    h.stopLane=[&](auto&){++lanes;};h.release=[&](auto&){++released;if(replaceOnRelease){s.sessionContext=std::make_shared<ExecutionContext>();s.holdRunActive=true;}};
    c.setHost(h);c.triggered("missing");f->setEnabled(false);c.triggered("same");f->setEnabled(true);
    if(!check(starts==0,"missing/disabled"))return false;
    c.triggered("same");running=true;c.triggered("same");running=false;
    if(!check(starts==1 && stops==1,"toggle start/stop"))return false;
    gate.editor=true;c.triggered("same");if(!check(denied==C::Block::Editor && starts==1,"editor feedback"))return false;
    gate.editor=false;gate.targetRequired=true;gate.profileMatches=false;c.triggered("same");
    if(!check(denied==C::Block::Target && starts==1,"target mismatch"))return false;
    gate.backgroundAllowed=true;c.triggered("same");gate.ownWindow=true;c.triggered("same");
    if(!check(starts==2,"background trigger exception still rejects own window"))return false;
    gate={};gate.unavailable=false;h.foregroundReady=[&]{source={&other,"other",2};};c.setHost(h);c.triggered("same");
    if(!check(starts==2,"foreground owner replacement cannot run same-ID collision"))return false;
    source={&project,"owner",1};h.foregroundReady=[&]{++source.generation;};c.setHost(h);c.triggered("same");
    if(!check(starts==2,"same pointer with new generation"))return false;
    h.foregroundReady={};c.setHost(h);f->setRunMode(FeatureRunMode::Hold);c.triggered("same");c.holdStarted("same");
    if(!check(c.visibleQueuedStarts()==QStringList{"same"} && c.visibleQueuedStarts()==QStringList{"same"},"pending visual lookup does not consume owned ticket"))return false;
    if(!check(starts==2 && queued==1 && depth==0 && c.acceptQueuedStart("same") && !c.acceptQueuedStart("same"),"Hold queues once, accepted once"))return false;
    c.holdStarted("same");++source.generation;if(!check(c.visibleQueuedStarts().isEmpty() && !c.acceptQueuedStart("same"),"queued Hold generation"))return false;
    c.holdStarted("same");c.holdEnded("same");if(!check(!c.acceptQueuedStart("same") && canceled==1,"release cancels pending start"))return false;
    available=true;s.runningMode=FeatureRunMode::Hold;s.holdRunActive=true;c.holdStarted("same");
    if(!check(repeats==1 && s.repeatSession,"idle existing Hold restarts iteration"))return false;
    s.holdRunActive=false;holdBlocked=true;const int before=queued;c.holdStarted("same");
    if(!check(queued==before,"still-running prior Hold blocks new start"))return false;
    holdBlocked=false;s.holdRunActive=true;s.holdKeyTapLaneActive=true;c.holdEnded("same");
    if(!check(lanes==1 && s.userStopRequested && !s.holdRunActive && !s.repeatSession,"lane release"))return false;
    s.holdKeyTapLaneActive=false;s.holdRunActive=true;s.userStopRequested=false;c.holdEnded("same");
    if(!check(finished==1 && released==2 && !s.userStopRequested,"idle release schedules finish"))return false;
    s.holdRunActive=true;replaceOnRelease=true;c.holdEnded("same");
    if(!check(finished==1 && s.holdRunActive,"replacement context untouched after release callback"))return false;
    replaceOnRelease=false;s.holdRunActive=true;s.engine=std::make_unique<WorkflowEngine>();s.sessionContext=std::make_shared<ExecutionContext>();
    auto wf=std::make_shared<Workflow>();auto wait=std::make_unique<WaitBlock>();wait->ms=5000;wf->addBlock(std::move(wait));
    s.engine->run(wf,s.sessionContext);c.holdEnded("same");s.engine->stopAndWaitBounded(1000);
    if(!check(s.userStopRequested && finished==1 && depth==0 && drains>0,"real Wait-engine Hold release"))return false;
    HotkeyBinding key;key.virtualKey='A';f->setHotkey(key);key.virtualKey=VK_XBUTTON1;collision=project.addFeature("mouse");collision->setHotkey(key);
    int warnings=0,errors=0,pruned=0;
    h.registerBindings=[](const auto&){return C::Registration{false,false,{"first"}};};
    h.log=[&](const auto&,bool error){error?++errors:++warnings;};h.prune=[&]{++pruned;};c.setHost(h);c.sync();
    if(!check(warnings==2 && errors==1 && pruned==1,"registration feedback"))return false;
    h.registerBindings=[&](const auto&){++source.generation;return C::Registration{false,false,{"stale"}};};c.setHost(h);c.sync();
    if(!check(warnings==2 && errors==1 && pruned==1,"registration source changed"))return false;
    auto* deleted=new C;C::Host destructive=h;destructive.foregroundReady=[&]{delete deleted;deleted=nullptr;};
    f->setRunMode(FeatureRunMode::RepeatCount);destructive.registerBindings={};deleted->setHost(destructive);deleted->triggered("same");
    if(!check(!deleted && starts==2,"controller lifetime"))return false;
    std::cout<<"Feature hotkeys: gate/toggle/Hold, frozen owner/generation queued start, release replacement/lifetime, real Wait engine and registration feedback OK (recorded ports; no native hooks/input)\n";
    return true;
}
}

class FeatureHotkeyUiChecks {
public:
    static bool run() {
        MainWindow w;w.m_foregroundMonitor->stop();
        auto* f=w.m_project->addFeature("hotkey wait");f->setId("hotkey-ui");f->setPointerVisualFeedback(false);
        auto wait=std::make_unique<WaitBlock>();wait->ms=10000;f->workflow().addBlock(std::move(wait));
        auto host=w.m_featureHotkeys->m_host;host.foregroundReady=[]{};w.m_featureHotkeys->setHost(host);
        int foregroundReads=0;const auto originalGate=w.m_foregroundSessions->m_ports.gate;
        w.m_foregroundSessions->m_ports.gate=[&](auto,const auto&){++foregroundReads;return false;};
        {FeatureHotkeyGateScope blocked;const auto value=host.gate("hotkey-ui");
            if(!check(value.editor && foregroundReads==0,"editor shortcut avoids foreground gate work"))return false;}
        host.gate("hotkey-ui");
        if(!check(w.isActiveDefaultProfile() && foregroundReads==0,"default profile avoids unnecessary target gate work"))return false;
        w.m_foregroundSessions->m_ports.gate=originalGate;
        const auto settle=[&](auto predicate){QElapsedTimer t;t.start();while(!predicate() && t.elapsed()<3000){QApplication::processEvents();QThread::msleep(1);}return predicate();};
        w.m_hotkeyManager->hotkeyTriggered("hotkey-ui");
        if(!check(settle([&]{return w.isFeatureRunning("hotkey-ui");}),"queued real signal starts Wait"))return false;
        w.m_hotkeyManager->hotkeyTriggered("hotkey-ui");
        if(!check(settle([&]{return !w.isFeatureRunning("hotkey-ui");}),"queued real signal stops Wait"))return false;
        {FeatureHotkeyGateScope blocked;w.onHotkeyTriggered("hotkey-ui");}
        if(!check(!w.isFeatureRunning("hotkey-ui"),"actual editor gate"))return false;
        f->setRunMode(FeatureRunMode::Hold);w.onHotkeyHoldStarted("hotkey-ui");w.onHotkeyHoldEnded("hotkey-ui");
        RunLifecycleCoordinator::flushCoalescedHoldFeatureStarts(w);
        if(!check(!w.isFeatureRunning("hotkey-ui") && w.m_pendingHoldFeatureStartIds.empty() && w.m_holdBurstDepth==0,"quick Hold press/release cancels pending"))return false;
        w.onHotkeyHoldStarted("hotkey-ui");
        // The queued Hold retains the old document; replacing it before drain drops the request.
        auto staged=w.m_projectWorkspace->stageProfile(w.m_profileManager->activeProfileId());
        if(!staged)return false;
        staged->project=std::make_unique<Project>();auto* other=staged->project->addFeature("collision");other->setId("hotkey-ui");other->setRunMode(FeatureRunMode::Hold);
        if(!w.m_projectWorkspace->install(std::move(staged),true,false,ProjectWorkspaceController::InstallMode::ValidatedTransition))return false;
        RunLifecycleCoordinator::flushCoalescedHoldFeatureStarts(w);
        if(!check(!w.sessionFor("hotkey-ui"),"actual queued Hold cannot migrate to replacement document"))return false;
        // Real lifecycle foreground preparation replaces the model while holding a raw Feature*.
        // The replacement has the same ID; execution must abort before dereferencing the old feature.
        auto longWait=std::make_unique<WaitBlock>();longWait->ms=10000;other->workflow().addBlock(std::move(longWait));
        bool replaced=false;auto foreground=w.m_foregroundSessions->m_host;
        foreground.snapshot=[] {ForegroundSessionController::Snapshot s;s.shutdown=false;s.runWithout=false;return s;};
        foreground.applySwitch=[](const auto&){};
        foreground.flushDeferred=[&]{
            if(replaced)return;
            auto next=w.m_projectWorkspace->stageProfile(w.m_profileManager->activeProfileId());
            if(!next)return;
            next->project=std::make_unique<Project>();auto* duplicate=next->project->addFeature("foreground collision");duplicate->setId("hotkey-ui");
            auto block=std::make_unique<WaitBlock>();block->ms=10000;duplicate->workflow().addBlock(std::move(block));
            replaced=w.m_projectWorkspace->install(std::move(next),true,false,ProjectWorkspaceController::InstallMode::ValidatedTransition);
        };
        w.m_foregroundSessions->setHost(foreground);
        w.m_holdBurstForegroundPrepared=false;w.m_holdBurstForegroundPrepTimer.invalidate();
        RunLifecycleCoordinator::startFeatureRun(w,other,true,true);
        if(!check(replaced && !w.sessionFor("hotkey-ui"),"real foreground prepare replacement aborts stale Hold"))return false;
        w.m_foregroundSessions->stop();
        if(!w.prepareForShutdown())return false;
        std::cout<<"Feature hotkeys MainWindow: queued manager signals start/stop real Wait, editor gate, quick Hold cancellation, queued document and actual foreground-prepare replacement rejection OK (offscreen; no native hooks/input)\n";
        return true;
    }
};
bool runFeatureHotkeyChecks(){return core() && FeatureHotkeyUiChecks::run();}
