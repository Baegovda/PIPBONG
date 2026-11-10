#include "app/InputInterruptController.h"
#include "app/UserInputInterruptMonitor.h"
#include "app/FeatureRunSession.h"
#include "app/FeatureHotkeyGate.h"
#include "model/Feature.h"
#include "core/workflow/blocks/WaitBlock.h"
#include "core/diagnostics/CrashReporter.h"
#include <QCoreApplication>
#include <QEvent>
#include <iostream>

namespace {
bool check(bool value, const char* message) {
    if (!value) std::cerr << "Input interrupt FAILED: " << message << '\n';
    return value;
}
bool monitorChecks() {
    auto& monitor = UserInputInterruptMonitor::instance();
    struct Cleanup { ~Cleanup() { auto& m=UserInputInterruptMonitor::instance(); m.unregisterAll(); m.setHandler({}); m.setHotkeyExemptionCheck({}); } } cleanup;
    monitor.unregisterAll(); monitor.setHotkeyExemptionCheck({});
    int calls=0; std::shared_ptr<ExecutionContext> delivered;
    monitor.setHandler([&](const std::string&, const auto& context) { ++calls; delivered=context; });
    const auto dispatch=[] { QCoreApplication::sendPostedEvents(QCoreApplication::instance(), QEvent::MetaCall); };
    auto first=std::make_shared<ExecutionContext>(), second=std::make_shared<ExecutionContext>();
    const auto bind=[&](const auto& context, bool keyboard=true, UserInputInterruptMode mode=UserInputInterruptMode::Pause) {
        monitor.registerSession("same-id", mode, {}, context, keyboard);
    };
    bind(first); monitor.notifyPhysicalInput('A'); bind(second); dispatch();
    if (!check(calls==0, "old queued event cannot affect replacement context with same feature id")) return false;
    monitor.notifyPhysicalInput('A'); bind(second); dispatch();
    if (!check(calls==1 && delivered==second, "idempotent same-context sync retains current queued delivery")) return false;
    monitor.notifyPhysicalInput('A'); dispatch();
    if (!check(calls==1, "cooldown suppresses duplicate physical edge")) return false;
    monitor.unregisterAll(); bind(first); monitor.notifyPhysicalInput(1); monitor.unregisterSession("same-id"); dispatch();
    if (!check(calls==1, "unregistered session drops its queued event")) return false;
    bind(second); monitor.notifyPhysicalInput(1); monitor.unregisterSession("same-id"); bind(second); dispatch();
    if (!check(calls==1, "unregister/re-register invalidates old event even for the same context")) return false;
    monitor.unregisterAll();
    bind(first); monitor.notifyPhysicalInput(1); monitor.setHandler([&](const std::string&, const auto&) { calls+=100; }); dispatch();
    if (!check(calls==1, "handler replacement invalidates already queued callbacks")) return false;
    monitor.setHandler([&](const std::string&, const auto&) { ++calls; });
    monitor.unregisterAll(); bind(first); monitor.notifyPhysicalInput(1); first.reset(); delivered.reset(); dispatch();
    if (!check(calls==1, "expired context is never dereferenced or retained by subscription")) return false;
    monitor.unregisterAll(); bind(second,false); monitor.notifyPhysicalInput('B'); dispatch();
    if (!check(calls==1, "keyboard-disabled Hold subscription ignores keyboard edges")) return false;
    monitor.notifyPhysicalInput(1); dispatch();
    if (!check(calls==2, "keyboard-disabled subscription still accepts mouse edges")) return false;
    monitor.unregisterAll(); bind(second); monitor.notifyPhysicalInput(1); bind(second,true,UserInputInterruptMode::None); dispatch();
    if (!check(calls==2, "None mode removes subscription and cancels queued delivery")) return false;
    monitor.unregisterAll(); bind(second); monitor.notifyPhysicalInput(1); second->requestStop(); dispatch();
    if (!check(calls==2, "stopped context ignores queued interrupt")) return false;
    second->resetStop(); monitor.unregisterAll(); bind(second);
    monitor.setHotkeyExemptionCheck([](int key) { return key=='C'; }); monitor.notifyPhysicalInput('C'); dispatch();
    { FeatureHotkeyGateScope blocked; monitor.notifyPhysicalInput(1); } dispatch();
    return check(calls==2, "registered hotkeys and explicit feature-hotkey blocker suppress dispatch");
}
bool controllerChecks() {
    using Controller=InputInterruptController;
    Feature feature("owner feature");
    FeatureRunSession session; session.featureId="session"; session.profileId="owner";
    session.sessionContext=std::make_shared<ExecutionContext>();
    int registrations=0, removals=0, changes=0, disarms=0; bool keyboard=false, missing=false;
    Controller controller({[&](const Controller::Registration& r) { ++registrations; keyboard=r.keyboard; },
        [&](const std::string&) { ++removals; }});
    Controller::Host host; host.feature=[&](auto&)->Feature* { return missing?nullptr:&feature; };
    host.disarm=[&](const auto& s) { if(s.profileId=="owner") ++disarms; };
    host.changed=[&](const auto&, const QString& name, auto) { if(name=="owner feature") ++changes; };
    controller.setHost(host);
    feature.setRunMode(FeatureRunMode::Hold); controller.sync(session);
    if(!check(registrations==1 && !keyboard,"Hold registration omits keyboard polling"))return false;
    feature.setRunMode(FeatureRunMode::Trigger); controller.sync(session);
    if(!check(registrations==2 && keyboard,"non-Hold registration enables keyboard polling"))return false;
    missing=true;controller.sync(session);missing=false;
    feature.setUserInputInterruptMode(UserInputInterruptMode::None);controller.sync(session);
    if(!check(removals==2 && controller.interrupt(session,session.sessionContext)==Controller::Outcome::Ignored,
        "missing owner or None mode unregisters without interruption"))return false;
    feature.setUserInputInterruptMode(UserInputInterruptMode::Pause);
    if(!check(controller.interrupt(session,std::make_shared<ExecutionContext>())==Controller::Outcome::Ignored && changes==0,
        "mismatched run context is rejected"))return false;
    if(!check(controller.interrupt(session,session.sessionContext)==Controller::Outcome::Paused && session.sessionContext->isPaused()
        && controller.interrupt(session,session.sessionContext)==Controller::Outcome::Resumed && !session.sessionContext->isPaused(),
        "owner Pause toggles pause/resume"))return false;
    feature.setUserInputInterruptMode(UserInputInterruptMode::Stop);
    session.runningMode=FeatureRunMode::Trigger;session.repeatSession=true;session.holdRunActive=true;session.holdRepeatGeneration=8;
    session.engine=std::make_unique<WorkflowEngine>();auto workflow=std::make_shared<Workflow>();
    auto wait=std::make_unique<WaitBlock>();wait->ms=10000;workflow->addBlock(std::move(wait));
    session.engine->run(workflow,session.sessionContext);
    if(!check(controller.interrupt(session,session.sessionContext)==Controller::Outcome::Stopped && session.userStopRequested
        && !session.repeatSession && !session.holdRunActive && session.holdRepeatGeneration==9 && session.disarmPersistedTrigger
        && session.sessionContext->shouldStop() && disarms==1 && changes==3,"Stop invalidates continuation, disarms owner and stops context"))return false;
    if(!check(session.engine->stopAndWaitBounded(1000),"real Wait engine drains after interrupt"))return false;
    return check(controller.interrupt(session,session.sessionContext)==Controller::Outcome::Ignored && disarms==1 && changes==3,
        "stopped run cannot be toggled or disarmed twice");
}
}
bool runInputInterruptChecks() {
    if(!monitorChecks() || !controllerChecks())return false;
    std::cout<<"Input interrupt: queued registration/context/handler lifetime, weak ownership, cooldown, Hold keyboard isolation, pause/resume and real Wait-engine stop OK (no native polling/input)\n";
    return true;
}

bool runPassiveDiagnosticsChecks() {
    struct Environment {
        const char* name;QByteArray value;bool present;
        explicit Environment(const char* key):name(key),value(qgetenv(key)),present(qEnvironmentVariableIsSet(key)){}
        ~Environment(){if(present)qputenv(name,value);else qunsetenv(name);}
    } data("PIPBONG_DATA_DIR"),smoke("PIPBONG_SMOKE_TEST"),ui("PIPBONG_SELF_TEST_UI");
    qputenv("PIPBONG_SMOKE_TEST","1");qputenv("PIPBONG_SELF_TEST_UI","0");qunsetenv("PIPBONG_DATA_DIR");
    if(!check(CrashReporter::interactiveReportsAllowed(),"flag alone cannot suppress ordinary diagnostics"))return false;
    qputenv("PIPBONG_DATA_DIR","isolated-diagnostic-fixture");
    if(!check(!CrashReporter::interactiveReportsAllowed(),"isolated smoke inspection suppresses diagnostic UI"))return false;
    qputenv("PIPBONG_SMOKE_TEST","0");qputenv("PIPBONG_SELF_TEST_UI","1");
    if(!check(!CrashReporter::interactiveReportsAllowed(),"isolated offscreen check suppresses diagnostic UI"))return false;
    qputenv("PIPBONG_SELF_TEST_UI","0");
    if(!check(CrashReporter::interactiveReportsAllowed(),"custom data directory alone retains ordinary diagnostics"))return false;
    std::cout<<"Passive diagnostics: isolated inspection suppresses modal/viewer presentation; ordinary execution retains diagnostics OK (policy only)\n";
    return true;
}
