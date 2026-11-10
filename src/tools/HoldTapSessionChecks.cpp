#include "app/HoldTapSessionController.h"
#include "app/MainWindow.h"
#include "app/RunLifecycleCoordinator.h"
#include "app/RunUiPublisher.h"
#include "app/SessionInputCleanupController.h"
#include "app/ForegroundWindowMonitor.h"
#include "app/TriggerMonitorController.h"
#include "model/Project.h"
#include "core/workflow/blocks/KeyPressBlock.h"
#include <QApplication>
#include <QElapsedTimer>
#include <QThread>
#include <atomic>
#include <chrono>
#include <future>
#include <iostream>

namespace {
bool waitFor(const std::function<bool()>& condition,bool events=true) {
    QElapsedTimer t;t.start();
    while(!condition() && t.elapsed()<4000){if(events)QApplication::processEvents();QThread::msleep(1);}
    return condition();
}
Feature* addTap(Project& p,const std::string& id,const std::string& name="tap") {
    auto* f=p.addFeature(name);f->setId(id);f->setRunMode(FeatureRunMode::Hold);f->setPointerVisualFeedback(false);
    auto b=std::make_unique<KeyPressBlock>();b->virtualKey='A';f->workflow().addBlock(std::move(b));return f;
}
using Controller=HoldTapSessionController;
struct Fixture {
    Project active,background;std::map<std::string,FeatureRunSession> sessions;
    std::shared_ptr<std::atomic<bool>> held=std::make_shared<std::atomic<bool>>(true);
    std::unique_ptr<Controller> controller;
    std::function<void()> prepare,select,decorate,log,start,synchronize;
    quint64 nextToken=0;int starts=0,finishes=0,selected=0,decorated=0,logs=0,refreshes=0,persists=0;
    bool deferred=false,deferStopped=false,failStart=false,emptyReader=false;std::vector<quint64> stopped;
    std::vector<std::string> pendingStart,pendingEnd;Controller::Reader workerReader;
    Controller::Host host() {
        Controller::Host h;h.session=[this](const auto& id){return find(id);};
        h.feature=[this](auto& s){return (s.profileId=="background"?background:active).featureById(s.featureId);};
        h.activeOwner=[](const auto& s){return s.profileId=="active";};h.displayed=h.activeOwner;h.logDetails=[](const auto&){return true;};
        h.deferForeground=[this](auto&,auto*){return deferred;};h.syncContext=[this](auto&,auto*){if(prepare)prepare();};
        h.holdReader=[this](const auto&)->Controller::Reader{if(emptyReader)return {};return [v=held]{return v->load();};};
        h.synchronize=[this]{if(synchronize)synchronize();};
        h.select=[this](auto*){++selected;if(select)select();};
        h.decorate=[this](auto&,auto current){++decorated;if(decorate)decorate();if(current())++persists;};
        h.logStart=[this](auto&){++logs;if(log)log();};h.persist=[this]{++persists;};h.refresh=[this]{++refreshes;};
        h.scheduleStart=[this](const auto& id){pendingStart.push_back(id);};h.scheduleEnd=[this](const auto& id){pendingEnd.push_back(id);};
        h.deferStopped=[this]{return deferStopped;};
        h.finish=[this](const auto& id,bool,const auto&,bool){++finishes;sessions.erase(id);};return h;
    }
    Controller::Ports ports() {
        return {[this](const auto&,int,int,auto reader,const auto& context){
            ++starts;workerReader=std::move(reader);context->resetStop(false);const auto token=failStart?0:++nextToken;
            if(start)start();return token;
        },[this](const auto&,quint64 token){stopped.push_back(token);}};
    }
    Fixture(){addTap(active,"run");addTap(background,"run","background tap");controller=std::make_unique<Controller>(host(),ports());}
    FeatureRunSession* find(const std::string& id="run"){const auto it=sessions.find(id);return it==sessions.end()?nullptr:&it->second;}
    FeatureRunSession& make(QString profile="active") {
        sessions.erase("run");auto& s=sessions["run"];s.featureId="run";s.profileId=profile;
        s.sessionContext=std::make_shared<ExecutionContext>();s.runningMode=FeatureRunMode::Hold;
        s.usesHoldKeyTapFastPath=true;s.holdRunActive=true;s.repeatSession=true;s.hotkeyLaunchedSession=true;return s;
    }
    void launch(){auto& s=*find();controller->launch(s,(s.profileId=="background"?background:active).featureById("run"),'A');}
};
bool muxOwnership() {
    std::atomic<int> oldTaps{0},newTaps{0};std::atomic<bool> held{true};
    std::promise<void> entered,released;const auto release=released.get_future().share();auto entry=entered.get_future();
    auto old=std::make_shared<ExecutionContext>(),next=std::make_shared<ExecutionContext>();
    HoldKeyTapMultiplexer mux(nullptr,[&](int key,ExecutionContext&){
        if(key=='A'){if(++oldTaps==1){entered.set_value();release.wait_for(std::chrono::seconds(3));}}
        else ++newTaps;
    });
    std::vector<quint64> completed;QObject sink;
    QObject::connect(&mux,&HoldKeyTapMultiplexer::ownedLaneFinished,&sink,[&](const auto&,quint64 token,bool,const auto&){completed.push_back(token);});
    const auto first=mux.startLane("same",'A',1,[]{return true;},old);
    if(entry.wait_for(std::chrono::seconds(2))!=std::future_status::ready){released.set_value();return false;}
    const auto second=mux.startLane("same",'B',1,[&]{return held.load();},next);
    mux.stopLaneIfToken("same",first);released.set_value();
    if(!first || !second || first==second || !old->shouldStop() || next->shouldStop()
        || !waitFor([&]{return newTaps>=4;}) || !mux.isLaneActive("same"))return false;
    held=false;if(!waitFor([&]{return !mux.isLaneActive("same") && !completed.empty();}))return false;
    if(completed.size()!=1 || completed[0]!=second)return false;
    std::cout<<"Hold tap mux ownership: replaced in-flight lane cannot erase successor; token stop ignores old owner and queued completion identifies new lane OK (recorded taps)\n";
    return true;
}
}

class HoldTapSessionChecks {
public:
static bool run() {
    const auto fail=[](const char* label){std::cerr<<"Hold tap session FAILED: "<<label<<'\n';return false;};
    if(!muxOwnership())return fail("actual worker token replacement");
    {
        Fixture f;f.make();f.launch();const auto first=f.find()->holdKeyTapLaneToken;const auto old=f.find()->sessionContext;
        f.make();f.launch();const auto second=f.find()->holdKeyTapLaneToken;
        f.controller->finished("run",first,true,"late");
        if(first==second || f.finishes || !f.find()->holdKeyTapLaneActive || f.find()->sessionContext==old)return fail("old completion cannot finish same-ID replacement");
        *f.held=false;f.controller->finished("run",second,true,"done");
        if(f.find() || f.finishes!=1)return fail("owned release finishes once");
        f.controller->finished("run",second,true,"duplicate");if(f.finishes!=1)return fail("duplicate completion");
    }
    {
        Fixture f;f.make();f.launch();auto* s=f.find();const auto first=s->holdKeyTapLaneToken;
        auto nextHeld=std::make_shared<std::atomic<bool>>(false);f.held=nextHeld;
        // Completion continuation reads the original run's binding, not a new
        // active profile's same-ID registration.
        f.controller->finished("run",first,true,"iteration");
        if(!f.find() || f.starts!=2 || f.find()->holdKeyTapLaneToken==first)return fail("frozen binding continuation");
        f.controller->presentStarts(f.pendingStart);
        if(f.selected!=1 || f.logs!=1)return fail("one session start survives lane relaunch");
    }
    {
        Fixture f;f.make();f.emptyReader=true;f.launch();
        if(!f.workerReader || f.workerReader())return fail("empty binding fails closed without bad_function_call");
        f.controller->finished("run",f.find()->holdKeyTapLaneToken,true,"");if(f.finishes!=1)return fail("empty reader terminal");
    }
    {
        Fixture f;auto& s=f.make();s.awaitingFirstTemplateRoi=true;f.launch();
        if(f.starts)return fail("ROI gate");s.awaitingFirstTemplateRoi=false;f.deferred=true;f.launch();
        if(f.starts)return fail("foreground defer");f.held=std::make_shared<std::atomic<bool>>(false);
        f.deferred=false;f.launch();f.launch();if(f.starts!=1 || !f.workerReader())return fail("duplicate launch / prepared binding survives foreground defer");
        f.controller->presentStarts(f.pendingStart);if(f.selected!=1 || f.decorated!=1 || f.logs!=1)return fail("normal deferred UI");
    }
    {
        Fixture f;f.make("background");f.launch();f.controller->presentStarts(f.pendingStart);
        if(f.selected || f.decorated || f.logs!=1)return fail("background collision never selects active same ID");
    }
    {
        Fixture f;f.make();f.prepare=[&]{f.make();};f.launch();if(f.starts || f.find()->holdKeyTapLaneActive)return fail("prepare replacement");
    }
    {
        Fixture f;f.make();f.start=[&]{f.make();};f.launch();
        if(f.stopped!=std::vector<quint64>{1} || f.find()->holdKeyTapLaneActive || !f.pendingStart.empty())return fail("start replacement stops only orphaned token");
    }
    {
        Fixture f;f.make();f.failStart=true;f.launch();if(f.find() || f.finishes!=1 || !f.pendingStart.empty())return fail("failed backend start cleanup");
    }
    {
        Fixture f;f.make();f.start=[&]{f.controller.reset();};auto* controller=f.controller.get();
        controller->launch(*f.find(),f.active.featureById("run"),'A');
        if(f.controller || f.stopped!=std::vector<quint64>{1} || !f.pendingStart.empty())return fail("controller deletion inside backend start stops returned token safely");
    }
    {
        Fixture f;f.make();f.synchronize=[&]{f.make();};f.launch();
        if(f.find()->holdKeyTapLaneActive || !f.pendingStart.empty())return fail("worker-state callback replacement cannot publish successor UI");
    }
    {
        Fixture f;f.make();f.launch();f.select=[&]{f.make();};f.controller->presentStarts(f.pendingStart);
        if(f.decorated || f.logs)return fail("selection callback replaces run");
    }
    {
        Fixture f;f.make();f.launch();f.decorate=[&]{f.make();};f.controller->presentStarts(f.pendingStart);
        if(f.logs || f.decorated!=1)return fail("decoration replacement suppresses stale log");
    }
    {
        Fixture f;f.make();f.launch();f.log=[&]{f.controller.reset();};auto* controller=f.controller.get();controller->presentStarts(f.pendingStart);
        if(f.controller || f.logs!=1 || f.refreshes)return fail("controller deletion during UI publication");
    }
    {
        Fixture f;auto& s=f.make();s.holdRunActive=false;f.controller->rememberEnd(s);
        ++s.holdRepeatGeneration;f.controller->finishPending("run");if(!f.find() || f.finishes)return fail("same-context generation protects pending end");
        f.controller->rememberEnd(s);s.holdKeyTapLaneActive=true;s.holdKeyTapLaneToken=88;f.controller->finishPending("run");
        if(!f.find() || f.finishes)return fail("new lane protects pending end");
        s.holdKeyTapLaneActive=false;s.holdKeyTapLaneToken=0;f.controller->rememberEnd(s);f.controller->finishPending("run");
        if(f.find() || f.finishes!=1)return fail("owned pending end");
    }
    {
        Fixture f;f.make();f.launch();auto* s=f.find();s->userStopRequested=true;s->holdRunActive=false;++s->holdRepeatGeneration;f.deferStopped=true;
        f.controller->finished("run",s->holdKeyTapLaneToken,true,"");
        if(!f.find() || f.pendingEnd!=std::vector<std::string>{"run"})return fail("own user stop completion accepts generation change");
        f.controller->rememberEnd(*f.find());f.make();f.controller->finishPending("run");if(!f.find() || f.finishes)return fail("pending end context replacement");
    }
    {
        Fixture f;f.make();f.launch();f.make();f.controller->presentStarts(f.pendingStart);
        if(f.selected || f.logs || f.decorated)return fail("pending UI owner replacement");
    }
    {
        Fixture f;auto& s=f.make();s.usesHoldKeyTapFastPath=false;s.engine=std::make_unique<WorkflowEngine>();
        f.controller->queueStarted(s);++s.holdRepeatGeneration;f.controller->presentStarts(f.pendingStart);
        if(f.selected || f.logs)return fail("nonfast Hold UI generation identity");
        f.controller->queueStarted(s);f.controller->presentStarts(f.pendingStart);if(f.logs!=1)return fail("nonfast Hold owned UI");
    }
    {
        Fixture f;f.make();std::atomic<int> taps{0};
        HoldKeyTapMultiplexer mux(nullptr,[&](int,ExecutionContext&){++taps;});
        f.controller=std::make_unique<Controller>(mux,f.host());*f.held=false;f.launch();
        if(!waitFor([&]{return !mux.isLaneActive("run");},false))return fail("queued old completion preparation");
        f.make();*f.held=true;f.launch();const auto token=f.find()->holdKeyTapLaneToken;
        QApplication::processEvents();
        if(!f.find() || f.finishes || f.find()->holdKeyTapLaneToken!=token || !waitFor([&]{return taps>=3;}))return fail("real queued old completion after restart");
        *f.held=false;if(!waitFor([&]{return !f.find();}))return fail("real successor completion");
    }
    if(!shell())return false;
    std::cout<<"Hold tap sessions: owned launch/completion, frozen binding, ROI/foreground gates, empty reader, deferred start/end identities, reentrant publication and real queued restart OK (offscreen; recorded ports, no OS input)\n";
    return true;
}
private:
static bool shell() {
    const auto fail=[](const char* s){std::cerr<<"Hold tap shell FAILED: "<<s<<'\n';return false;};
    if(QApplication::platformName()!="offscreen")return fail("offscreen required");
    {
        MainWindow w;w.m_foregroundMonitor->stop();w.m_triggerMonitor->stop();
        w.m_inputCleanup=std::make_unique<SessionInputCleanupController>(SessionInputCleanupController::Ports{});
        auto* feature=addTap(*w.m_project,"hold-shell");auto& s=w.runSessions()["hold-shell"];
        s.featureId="hold-shell";s.profileId=w.m_profileManager->activeProfileId();s.runningMode=FeatureRunMode::Hold;
        s.usesHoldKeyTapFastPath=true;s.holdRunActive=true;s.hotkeyLaunchedSession=true;s.repeatSession=true;s.waitingForScopedTargetForeground=true;
        int starts=0;w.m_holdTapSessions->m_ports={
            [&](const auto&,int key,int,auto,const auto&)->quint64{if(key=='A')++starts;return 111;},[](const auto&,quint64){}};
        w.m_holdTapSessions->m_host.holdReader=[](const auto&)->Controller::Reader{return []{return true;};};
        w.m_holdTapSessions->m_host.deferForeground=[](auto&,auto*){return false;};
        w.m_holdTapSessions->m_host.syncInterrupt=[](auto&,auto*){};
        RunSessionController::HostCallbacks h;h.featureForSession=[&](auto& session){return w.featureForSession(session);};
        h.runForegroundGateActiveForSession=[](auto&,auto*){return true;};h.isHoldBindingDown=[](const auto&){return true;};
        h.launchWorkflowRun=[&](auto& session,auto* f,bool repeat){w.launchWorkflowRun(session,f,repeat);};
        w.m_runSessionController.setHostCallbacks(h);w.m_runSessionController.resumeWaitingScopedTargetForegroundSessions();
        if(starts!=1 || s.engine || s.waitingForScopedTargetForeground || !s.holdKeyTapLaneActive || s.holdKeyTapLaneToken!=111 || !s.sessionContext)return fail("engine-less foreground resume starts real shell delegate");
        RunLifecycleCoordinator::flushCoalescedHoldStartUi(w);
        s.holdRunActive=false;s.userStopRequested=true;++s.holdRepeatGeneration;
        w.m_holdTapSessions->finished("hold-shell",111,true,"");RunLifecycleCoordinator::flushCoalescedHoldFeatureEndFinishes(w);
        if(w.sessionFor("hold-shell"))return fail("actual shell completion");
        // A resume callback may remove its own map entry (edited fast-path
        // workflow); the controller must still visit independent waiting runs.
        for(const auto& id:{"resume-erase","resume-peer"}){
            auto* f=addTap(*w.m_project,id);auto& next=w.runSessions()[id];next.featureId=id;
            next.profileId=w.m_profileManager->activeProfileId();next.runningMode=FeatureRunMode::Hold;
            next.usesHoldKeyTapFastPath=true;next.holdRunActive=true;next.repeatSession=true;next.hotkeyLaunchedSession=true;next.waitingForScopedTargetForeground=true;
            if(std::string(id)=="resume-erase")f->workflow().clear();
        }
        w.m_runSessionController.resumeWaitingScopedTargetForegroundSessions();
        if(w.sessionFor("resume-erase") || starts!=2 || !w.sessionFor("resume-peer")->holdKeyTapLaneActive)return fail("resume iteration survives edited lane removal");
        auto* peer=w.sessionFor("resume-peer");peer->holdRunActive=false;peer->userStopRequested=true;
        w.m_holdTapSessions->finished("resume-peer",111,true,"");RunLifecycleCoordinator::flushCoalescedHoldFeatureEndFinishes(w);
        if(w.sessionFor("resume-peer") || !w.prepareForShutdown())return fail("resume peer cleanup");
    }
    {
        MainWindow w;w.m_foregroundMonitor->stop();w.m_triggerMonitor->stop();
        auto& old=w.runSessions()["flush-replace"];old.featureId="flush-replace";old.sessionContext=std::make_shared<ExecutionContext>();
        const auto previous=old.sessionContext;bool replaced=false;
        RunUiPublisher::HostCallbacks h;h.session=[&](const auto& id){return w.sessionFor(id);};h.activeRepeatCount=[]{return 1;};h.isDisplayed=[](auto*){return true;};
        h.timing=[&](auto){if(replaced)return;replaced=true;w.runSessions().erase("flush-replace");auto& next=w.runSessions()["flush-replace"];next.featureId="flush-replace";next.sessionContext=std::make_shared<ExecutionContext>();};
        w.m_runUiPublisher=std::make_unique<RunUiPublisher>(h,&w);const auto channel=w.m_runUiPublisher->begin("flush-replace");RunUiPublisher::record(channel,true,4,"tail");
        w.finishRunSession("flush-replace",true,"");
        if(!replaced || !w.sessionFor("flush-replace") || w.sessionFor("flush-replace")->sessionContext==previous)return fail("final flush cannot erase new same-ID run");
        w.runSessions().clear();if(!w.prepareForShutdown())return fail("flush fixture checkpoint");
    }
    std::cout<<"Hold tap shell: actual engine-less foreground resume and start/end delegates, final-flush same-ID replacement lease and shutdown checkpoint OK (offscreen; injected backend)\n";
    return true;
}
};
bool runHoldTapSessionChecks(){return HoldTapSessionChecks::run();}
