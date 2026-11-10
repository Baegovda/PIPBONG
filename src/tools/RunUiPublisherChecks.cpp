#include "app/RunUiPublisher.h"
#include "core/workflow/blocks/WaitBlock.h"
#include <QCoreApplication>
#include <QElapsedTimer>
#include <QThread>
#include <atomic>
#include <future>
#include <iostream>
#include <map>
#include <vector>

namespace {
bool expect(bool value, const char* message) {
    if (!value) std::cerr << "Run UI publisher check failed: " << message << '\n';
    return value;
}
bool waitFor(const std::function<bool()>& predicate) {
    QElapsedTimer elapsed; elapsed.start();
    while (!predicate() && elapsed.elapsed() < 5000) {
        QCoreApplication::processEvents(); QThread::msleep(1);
    }
    return predicate();
}
void resetSession(FeatureRunSession& session, const std::string& id) {
    session = FeatureRunSession{};
    session.featureId = id;
    session.sessionContext = std::make_shared<ExecutionContext>();
    session.sessionContext->setSuppressRepeatUi(true);
    session.repeatSession = true;
}
}

bool runRunUiPublisherChecks() {
    std::map<std::string, FeatureRunSession> sessions;
    resetSession(sessions["burst"], "burst");
    const QThread* gui = QThread::currentThread();
    int logs = 0, timing = 0, refreshes = 0, batches = 0, activeCount = 1;
    bool correctThread = true;
    RunUiPublisher::HostCallbacks host;
    host.session = [&](const std::string& id) { const auto it = sessions.find(id); return it == sessions.end() ? nullptr : &it->second; };
    host.activeRepeatCount = [&] { return activeCount; };
    host.isDisplayed = [](const FeatureRunSession* session) { return session != nullptr; };
    host.timing = [&](auto) { ++timing; correctThread &= QThread::currentThread() == gui; };
    host.log = [&](const auto&, const QString&, bool) { ++logs; correctThread &= QThread::currentThread() == gui; };
    host.afterBatch = [&](auto&) { ++batches; correctThread &= QThread::currentThread() == gui; };
    host.refreshRunUi = [&] { ++refreshes; };
    RunUiPublisher publisher(host);
    auto channel = publisher.begin("burst");
    std::vector<std::future<void>> workers;
    for (int n = 0; n < 4; ++n) workers.push_back(std::async(std::launch::async, [channel] {
        for (int i = 0; i < 25000; ++i) RunUiPublisher::record(channel, true, 1, QStringLiteral("burst"));
    }));
    for (auto& worker : workers) worker.get();
    if (!expect(publisher.queuedWakeCount() == 1 && sessions["burst"].completedLoopCount == 0,
                "100000 worker updates reduce to one queued GUI wake")) return false;
    publisher.flush("burst");
    if (!expect(sessions["burst"].completedLoopCount == 100000 && sessions["burst"].lastLoopNumber == 100000
                && sessions["burst"].totalLoopElapsedMs == 100000 && sessions["burst"].lastLoopAverageMs == 1
                && logs == 1 && timing == 1 && batches == 1 && correctThread, "lossless GUI-thread reduction")) return false;
    const auto old = channel;
    resetSession(sessions["burst"], "burst");
    channel = publisher.begin("burst");
    RunUiPublisher::record(old, false, 999, QStringLiteral("old run"));
    for (int i = 0; i < 20; ++i) RunUiPublisher::record(channel, true, 3, QStringLiteral("new run"));
    publisher.flush("burst");
    if (!expect(sessions["burst"].completedLoopCount == 20 && sessions["burst"].totalLoopElapsedMs == 60,
                "retired run cannot contaminate restarted feature")) return false;
    activeCount = 4;
    const int oldLogs = logs;
    const int oldTiming = timing;
    for (int i = 0; i < 10; ++i) RunUiPublisher::record(channel, true, 1, QString());
    publisher.flushAll();
    if (!expect(logs == oldLogs && timing == oldTiming && refreshes == 1, "multi-repeat batches suppress per-loop UI/log churn")) return false;
    activeCount = 1;

    // A worker may retain a channel after the publisher/window is gone.
    auto detached = std::make_unique<RunUiPublisher>(host);
    const auto surviving = detached->begin("burst");
    auto recording = std::async(std::launch::async, [surviving] {
        for (int i = 0; i < 10000; ++i) RunUiPublisher::record(surviving, true, 1, QString());
    });
    detached.reset();
    recording.get();
    QCoreApplication::processEvents();

    resetSession(sessions["engine"], "engine");
    auto& engineSession = sessions["engine"];
    engineSession.engine = std::make_unique<WorkflowEngine>();
    const auto engineChannel = publisher.begin("engine");
    auto context = engineSession.sessionContext;
    std::atomic<int> completed{0};
    std::vector<int> loopNumbers;
    ExecutionContext::WorkerFastRepeatCallbacks callbacks;
    callbacks.delayMs = [] { return 0; };
    callbacks.onIterationComplete = [engineChannel, &completed, &loopNumbers, raw = context.get()](bool success, std::int64_t ms, const std::string&) {
        loopNumbers.push_back(raw->runLoopNumber());
        ++completed;
        RunUiPublisher::record(engineChannel, success, ms, QString());
    };
    callbacks.shouldContinue = [&](bool success, bool) { return success && completed.load() < 300; };
    context->setWorkerFastRepeatCallbacks(std::move(callbacks));
    auto workflow = std::make_shared<Workflow>();
    auto block = std::make_unique<WaitBlock>(); block->ms = 0;
    workflow->addBlock(std::move(block));
    bool finished = false;
    QObject::connect(engineSession.engine.get(), &WorkflowEngine::finished, &publisher, [&](bool, const QString&) {
        publisher.flush("engine"); finished = true;
    });
    engineSession.engine->run(workflow, context);
    if (!expect(waitFor([&] { return finished; }) && engineSession.engine->stopAndWaitBounded(1000)
                && engineSession.completedLoopCount == 300 && engineSession.lastLoopNumber == 300
                && loopNumbers.size() == 300, "real engine final tail flushed without losing iterations")) return false;
    for (int i = 0; i < 300; ++i) if (!expect(loopNumbers[i] == i + 1, "worker loop number advances without waiting for UI")) return false;
    ExecutionContext stopped;
    stopped.requestStop();
    stopped.setPaused(true);
    stopped.prepareNextWorkerRepeatIteration();
    if (!expect(stopped.shouldStop() && stopped.isPaused() && stopped.runLoopNumber() == 2,
                "iteration preparation preserves concurrent stop and pause requests")) return false;
    // Registry replacement must be isolated even if a caller did not start a
    // new channel. A matching string ID alone is not the old run.
    resetSession(sessions["unbegun"],"unbegun");auto unbegun=publisher.begin("unbegun");
    RunUiPublisher::record(unbegun,true,7,"stale context");resetSession(sessions["unbegun"],"unbegun");
    publisher.flush("unbegun");
    if(!expect(sessions["unbegun"].completedLoopCount==0,"context replacement without begin rejects old channel"))return false;
    sessions["unbegun"].profileId="before";unbegun=publisher.begin("unbegun");RunUiPublisher::record(unbegun,true,7,"stale profile");sessions["unbegun"].profileId="after";publisher.flush("unbegun");
    if(!expect(sessions["unbegun"].completedLoopCount==0,"profile ownership change rejects old channel"))return false;
    {
        resetSession(sessions["reentry"],"reentry");int lateLogs=0,lateBatches=0;
        auto changingHost=host;changingHost.timing=[&](auto){sessions.erase("reentry");resetSession(sessions["reentry"],"reentry");};
        changingHost.log=[&](const auto&,const auto&,bool){++lateLogs;};changingHost.afterBatch=[&](auto&){++lateBatches;};
        RunUiPublisher changing(changingHost);auto c=changing.begin("reentry");RunUiPublisher::record(c,true,9,"replace from timing");changing.flush("reentry");
        if(!expect(sessions["reentry"].completedLoopCount==0 && lateLogs==0 && lateBatches==0,"timing replacement drops remaining log/afterBatch publication"))return false;
    }
    for(bool deleteFromTiming:{true,false}){
        resetSession(sessions["dying"],"dying");int lateBatches=0;
        std::unique_ptr<RunUiPublisher> dying;auto dyingHost=host;
        dyingHost.timing=[&](auto){if(deleteFromTiming)dying.reset();};
        dyingHost.log=[&](const auto&,const auto&,bool){if(!deleteFromTiming)dying.reset();};
        dyingHost.afterBatch=[&](auto&){++lateBatches;};
        dying=std::make_unique<RunUiPublisher>(dyingHost);auto c=dying->begin("dying");RunUiPublisher::record(c,true,1,"destroy from callback");dying->flushAll();
        if(!expect(!dying && lateBatches==0,"publisher destruction during timing/log has no later batch or flushAll access"))return false;
    }
    std::cout << "Run UI publisher: 100000 updates/one wake, restart/lifetime isolation, GUI-only callbacks and 300 real fast-repeat iterations/cancel preservation OK\n";
    std::cout << "Run UI publisher ownership: context/profile replacement without begin, timing registry replacement and publisher deletion during timing/log/flushAll isolated OK\n";
    return true;
}
