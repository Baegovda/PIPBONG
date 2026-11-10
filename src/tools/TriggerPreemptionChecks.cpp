#include "app/MainWindow.h"
#include "app/ForegroundWindowMonitor.h"
#include "app/TriggerMonitorController.h"
#include "app/TriggerPreemptionController.h"
#include "app/RunLifecycleCoordinator.h"
#include "model/Project.h"
#include "model/Feature.h"
#include "core/workflow/blocks/WaitBlock.h"
#include "core/workflow/HoldKeyTapMultiplexer.h"
#include <QApplication>
#include <QElapsedTimer>
#include <QThread>
#include <thread>
#include <vector>
#include <iostream>

namespace {
bool check(bool value,const char* stage){if(!value)std::cerr<<"Trigger preemption FAILED: "<<stage<<'\n';return value;}
template<class Predicate> bool waitFor(Predicate predicate) {
    QElapsedTimer t;t.start();while(!predicate() && t.elapsed()<3000){QApplication::processEvents();QThread::msleep(1);}return predicate();
}
bool pauseState() {
    ExecutionContext context;
    const auto first=context.pauseIfRunning();
    if(!check(first && context.isPaused() && !context.pauseIfRunning() && context.resumeIfPauseUnchanged(*first)
        && !context.isPaused() && !context.resumeIfPauseUnchanged(*first),"pause lease once"))return false;
    const auto manual=context.pauseIfRunning();context.setPaused(true);
    if(!check(manual && !context.resumeIfPauseUnchanged(*manual) && context.isPaused(),"same-value manual intent invalidates lease"))return false;
    context.setPaused(false);const auto toggled=context.pauseIfRunning();context.togglePaused();context.togglePaused();
    if(!check(toggled && !context.resumeIfPauseUnchanged(*toggled) && context.isPaused(),"toggle intent invalidates lease"))return false;
    context.resetStop();if(!check(!context.isPaused(),"reset invalidates pause"))return false;
    std::vector<std::thread> threads;
    for(int i=0;i<4;++i)threads.emplace_back([&]{for(int j=0;j<501;++j)context.togglePaused();});
    for(auto& thread:threads)thread.join();
    if(!check(!context.isPaused(),"concurrent toggle parity; no lost CAS changes"))return false;
    auto paused=std::make_shared<ExecutionContext>();const auto lease=paused->pauseIfRunning();
    WorkflowEngine engine;auto workflow=std::make_shared<Workflow>();auto wait=std::make_unique<WaitBlock>();wait->ms=10000;workflow->addBlock(std::move(wait));
    engine.run(workflow,paused);
    const bool retained=lease && paused->isPaused() && paused->resumeIfPauseUnchanged(*lease);
    engine.stopAndWaitBounded(1000);
    return check(retained,"real worker restart preserves current pause lease");
}
struct Fixture {
    TriggerPreemptionController::Sessions sessions;
    int reads=0,reconciles=0,changed=0;std::vector<QPoint> moves;bool movedBeforeResume=true;
    std::function<void()> onMove;
    TriggerPreemptionController c;
    Fixture():c(sessions,{[this]()->std::optional<QPoint>{++reads;return QPoint(reads*10,reads*20);},
        [this](QPoint p){moves.push_back(p);if(onMove)onMove();}}) {
        TriggerPreemptionController::Host h;h.reconcile=[this]{++reconciles;};h.changed=[this]{++changed;};c.setHost(h);
    }
    FeatureRunSession& actor(const std::string& id){auto& s=sessions[id];s.featureId=id;s.profileId="owner";return s;}
    FeatureRunSession& peer(){auto& s=actor("peer");if(!s.sessionContext)s.sessionContext=std::make_shared<ExecutionContext>();return s;}
};
bool controller() {
    {
        Fixture f;auto& peer=f.peer();auto& actor=f.actor("one");
        auto& manual=f.actor("manual");manual.sessionContext=std::make_shared<ExecutionContext>();manual.sessionContext->setPaused(true);
        f.onMove=[&]{f.movedBeforeResume &= peer.sessionContext->isPaused();};
        f.c.pause(actor);f.c.pause(actor);
        if(!check(peer.sessionContext->isPaused() && f.reads==1,"idempotent begin"))return false;
        f.c.resume(actor);f.c.resume(actor);
        if(!check(!peer.sessionContext->isPaused() && manual.sessionContext->isPaused() && f.moves==std::vector<QPoint>{QPoint(10,20)} && f.movedBeforeResume,"owned pause release after cursor; manual pause retained"))return false;
    }
    {
        Fixture f;auto& peer=f.peer();auto& one=f.actor("one");auto old=peer.sessionContext;
        f.c.pause(one);peer.sessionContext=std::make_shared<ExecutionContext>();peer.sessionContext->setPaused(true);f.c.resume(one);
        if(!check(peer.sessionContext->isPaused(),"same-ID replacement context"))return false;
        old->requestStop();
    }
    {
        Fixture f;auto& peer=f.peer();auto& one=f.actor("one");f.c.pause(one);peer.sessionContext->setPaused(true);f.c.resume(one);
        if(!check(peer.sessionContext->isPaused(),"manual pause during preemption"))return false;
    }
    for(const bool lifo:{false,true}) {
        Fixture f;auto& peer=f.peer();auto& one=f.actor("one");auto& two=f.actor("two");
        f.c.pause(one);f.c.pause(two);
        f.c.resume(lifo?two:one);
        if(!check(peer.sessionContext->isPaused() && f.moves.size()==(lifo?1:0),"overlap retained until last claim"))return false;
        if(lifo && !check(f.moves.back()==QPoint(20,40),"nested cursor returns to interrupted action"))return false;
        f.c.resume(lifo?one:two);
        if(!check(!peer.sessionContext->isPaused() && f.moves.back()==QPoint(10,20),"out-of-order frames unwind to original cursor"))return false;
    }
    {
        Fixture f;auto& peer=f.peer();auto& one=f.actor("one");auto& two=f.actor("two");
        one.sessionContext=std::make_shared<ExecutionContext>();two.sessionContext=std::make_shared<ExecutionContext>();
        f.c.pause(one);
        if(!check(two.sessionContext->isPaused(),"older action paused pending trigger"))return false;
        f.c.pause(two);
        if(!check(!two.sessionContext->isPaused() && one.sessionContext->isPaused(),"newer trigger takeover cannot form a mutual-pause cycle"))return false;
        f.c.resume(two);
        if(!check(!one.sessionContext->isPaused() && peer.sessionContext->isPaused(),"nested real-context action resumes predecessor only"))return false;
        two.sessionContext->setPaused(true);f.c.resume(one);
        if(!check(!peer.sessionContext->isPaused() && two.sessionContext->isPaused(),"older frame cannot release detached newer/manual pause"))return false;
    }
    {
        Fixture f;auto& peer=f.peer();auto& one=f.actor("one");f.c.pause(one);peer.profileId="new owner";f.c.resume(one);
        if(!check(peer.sessionContext->isPaused(),"same context with replaced owner"))return false;
    }
    {
        Fixture f;auto& peer=f.peer();auto& one=f.actor("one");f.c.pause(one);
        f.onMove=[&]{peer.sessionContext=std::make_shared<ExecutionContext>();peer.sessionContext->setPaused(true);};f.c.resume(one);
        if(!check(peer.sessionContext->isPaused(),"cursor callback context replacement"))return false;
    }
    {
        Fixture f;auto& peer=f.peer();auto& one=f.actor("one");f.c.pause(one);f.c.shutdown();f.c.pause(one);
        if(!check(!peer.sessionContext->isPaused() && !one.triggerPreemptToken && f.moves.empty(),"shutdown releases owned pause without cursor movement"))return false;
    }
    {
        TriggerPreemptionController::Sessions sessions;auto& p=sessions["peer"];p.featureId="peer";p.sessionContext=std::make_shared<ExecutionContext>();
        auto& actor=sessions["one"];actor.featureId="one";TriggerPreemptionController* dying=nullptr;
        dying=new TriggerPreemptionController(sessions,{[&]()->std::optional<QPoint>{delete dying;dying=nullptr;return QPoint(1,2);},[](QPoint){}});
        dying->pause(actor);
        if(!check(!dying && !p.sessionContext->isPaused() && !actor.triggerPreemptToken,"deletion during cursor read releases ownership"))return false;
    }
    return true;
}
bool holdPause() {
    Fixture f;auto& peer=f.peer();auto& actor=f.actor("trigger");
    std::atomic<int> taps{0},otherTaps{0};std::atomic<bool> held{true},otherHeld{true};
    HoldKeyTapMultiplexer mux(nullptr,[&](int key,ExecutionContext&){if(key=='A')++taps;else ++otherTaps;});
    const auto lease=peer.sessionContext->pauseIfRunning();
    mux.startLane("paused",'A',3,[&]{return held.load();},peer.sessionContext);
    mux.startLane("other",'B',2,[&]{return otherHeld.load();},std::make_shared<ExecutionContext>());
    if(!waitFor([&]{return otherTaps.load()>=5;}) || !check(taps==0 && peer.sessionContext->isPaused(),"Hold starts paused while other lane runs"))return false;
    if(!lease || !peer.sessionContext->resumeIfPauseUnchanged(*lease) || !waitFor([&]{return taps.load()>=4;}))return false;
    f.c.pause(actor);int beforeOther=otherTaps.load();
    if(!waitFor([&]{return otherTaps.load()>=beforeOther+5;}))return false;
    const int stable=taps.load();beforeOther=otherTaps.load();
    if(!waitFor([&]{return otherTaps.load()>=beforeOther+5;}) || !check(taps==stable,"trigger pause stops Hold emissions without stalling peer lane"))return false;
    otherHeld=false;if(!waitFor([&]{return !mux.isLaneActive("other");}))return false;
    held=false;if(!waitFor([&]{return !mux.isLaneActive("paused");}))return false;
    f.c.resume(actor);
    if(!check(!peer.sessionContext->isPaused() && taps==stable,"physical release finishes paused Hold lane"))return false;
    std::cout<<"Trigger preemption Hold: real multiplexer retains startup pause, suspends recorded taps, keeps peer lane running and observes release while paused OK (no native input)\n";
    return true;
}
}

class TriggerPreemptionUiChecks {
public:
    static bool run() {
        MainWindow w;w.m_foregroundMonitor->stop();w.m_triggerMonitor->stop();
        int reads=0;std::vector<QPoint> moves;
        w.m_triggerPreemption->m_ports={[&]()->std::optional<QPoint>{++reads;return QPoint(-123,456);},[&](QPoint p){moves.push_back(p);}};
        auto& one=w.runSessions()["preempt-one"];one.featureId="preempt-one";
        auto& peer=w.runSessions()["preempt-peer"];peer.featureId="preempt-peer";peer.sessionContext=std::make_shared<ExecutionContext>();
        w.pauseOtherSessionsForTrigger(one);
        const bool paused=peer.sessionContext->isPaused();
        peer.sessionContext=std::make_shared<ExecutionContext>();peer.sessionContext->setPaused(true);
        w.resumePreemptedSessionsForTrigger(one);
        const bool replacement=paused && peer.sessionContext->isPaused();
        peer.sessionContext->setPaused(false);
        w.pauseOtherSessionsForTrigger(one);
        auto& two=w.runSessions()["preempt-two"];two.featureId="preempt-two";
        w.pauseOtherSessionsForTrigger(two);
        w.resumePreemptedSessionsForTrigger(one);
        const bool overlap=peer.sessionContext->isPaused();
        w.resumePreemptedSessionsForTrigger(two);
        const bool restored=!peer.sessionContext->isPaused();
        // Start a real Wait worker, then acquire the trigger pause after startup reset.
        auto wf=std::make_shared<Workflow>();auto wait=std::make_unique<WaitBlock>();wait->ms=10000;wf->addBlock(std::move(wait));
        peer.engine=std::make_unique<WorkflowEngine>();peer.runningMode=FeatureRunMode::RepeatInfinite;peer.repeatSession=true;
        bool began=false;QObject::connect(peer.engine.get(),&WorkflowEngine::started,&w,[&]{began=true;});
        peer.engine->run(wf,peer.sessionContext);QElapsedTimer deadline;deadline.start();
        while(!began && deadline.elapsed()<3000){QApplication::processEvents();QThread::msleep(1);}
        if(!check(began,"real Wait worker startup"))return false;
        w.pauseOtherSessionsForTrigger(one);
        RunLifecycleCoordinator::tearDownAndEraseSessionEntry(w,"preempt-one");
        const bool teardown=!peer.sessionContext->isPaused();
        w.pauseOtherSessionsForTrigger(two);
        RunLifecycleCoordinator::eraseSessionEntryAndRefreshRunUi(w,"preempt-two");
        const bool erased=!peer.sessionContext->isPaused();
        peer.engine->stopAndWaitBounded(1000);w.runSessions().clear();
        if(!w.prepareForShutdown())return false;
        if(!check(replacement && overlap && restored && teardown && erased && reads==5 && moves.size()==4,"actual owner/overlap/teardown and recorded cursor"))return false;
        // Destroy a live shell without prepareForShutdown: registry must outlive controller cleanup.
        auto dying=std::make_unique<MainWindow>();dying->m_foregroundMonitor->stop();dying->m_triggerMonitor->stop();
        dying->m_triggerPreemption->m_ports={[]()->std::optional<QPoint>{return QPoint(1,2);},[](QPoint){}};
        auto& source=dying->runSessions()["destructor-trigger"];source.featureId="destructor-trigger";source.runningMode=FeatureRunMode::Trigger;
        auto& target=dying->runSessions()["destructor-peer"];target.featureId="destructor-peer";target.sessionContext=std::make_shared<ExecutionContext>();
        const auto retired=target.sessionContext;target.engine=std::make_unique<WorkflowEngine>();
        bool workerBegan=false;QObject::connect(target.engine.get(),&WorkflowEngine::started,dying.get(),[&]{workerBegan=true;});
        auto work=std::make_shared<Workflow>();auto delay=std::make_unique<WaitBlock>();delay->ms=10000;work->addBlock(std::move(delay));target.engine->run(work,retired);
        if(!waitFor([&]{return workerBegan;}))return false;
        dying->pauseOtherSessionsForTrigger(source);dying.reset();
        if(!check(retired->shouldStop(),"shell destruction without prepare shuts down owned contexts"))return false;
        std::cout<<"Trigger preemption MainWindow: actual replacement/overlap, real Wait worker, both lifecycle erase paths release claims and cursor moves recorded OK (offscreen; no native cursor/input)\n";
        return true;
    }
};
bool runTriggerPreemptionChecks(){
    if(!pauseState() || !controller() || !holdPause() || !TriggerPreemptionUiChecks::run())return false;
    std::cout<<"Trigger preemption: atomic pause leases/manual intent, replacement owner/context, overlapping pause claims, nested/out-of-order cursor, shutdown and lifetime OK (recorded ports)\n";
    return true;
}
