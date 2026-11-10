#include "core/diagnostics/GuiContextCache.h"
#include "core/diagnostics/CrashReporter.h"
#include "app/WindowDiagnosticsController.h"
#include "app/MainWindow.h"
#include "app/ForegroundWindowMonitor.h"
#include "app/TriggerMonitorController.h"
#include "model/Project.h"
#include "core/workflow/blocks/WaitBlock.h"
#include <QApplication>
#include <QProcess>
#include <QProcessEnvironment>
#include <QTemporaryDir>
#include <QEvent>
#include <future>
#include <atomic>
#include <stdexcept>
#include <iostream>

int runGuiContextCacheProbe() {
    const auto fail=[](const char* s){std::cerr<<"Gui context cache FAILED: "<<s<<'\n';return 69;};
    GuiContextCache cache;int count=0;
    auto first=cache.set([&]{++count;return QStringLiteral("first");});
    auto second=cache.set([&]{++count;return QStringLiteral("second");});
    if(!first || first==second || cache.retire(first) || cache.cached()!="second" || !cache.refresh() || !cache.retire(second)
        || cache.refresh() || cache.cached()!="second")return fail("owned retire and retained last-good snapshot");
    bool replace=false;quint64 next=0;
    auto old=cache.set([&]{if(replace){next=cache.set([]{return QStringLiteral("new-owner");});return QStringLiteral("late-old");}return QStringLiteral("old-owner");});
    replace=true;if(cache.refresh() || !next || cache.cached()!="new-owner" || cache.retire(old))return fail("replacement inside unlocked refresh rejects old publication");
    bool recursiveRejected=false;auto recursive=cache.set([&]{recursiveRejected=!cache.refresh();return QStringLiteral("recursive");});
    if(!recursiveRejected || cache.cached()!="recursive")return fail("same-provider recursive refresh is bounded");
    auto throwing=cache.set([]()->QString{throw std::runtime_error("fixture");});
    if(cache.refresh() || cache.cached()!="recursive" || !cache.retire(throwing))return fail("provider failure preserves last-good text");
    std::unique_ptr<QObject> owner=std::make_unique<QObject>();QPointer<QObject> weak(owner.get());bool retireOwner=false;quint64 owned=0;
    owned=cache.set([&]{if(retireOwner){owner.reset();cache.retire(owned);return QStringLiteral("late-owner");}return QStringLiteral("live-owner");});
    retireOwner=true;if(cache.refresh() || weak || cache.cached()!="live-owner")return fail("owner deletion plus retirement during callback");
    struct Cleanup {std::function<void()> call;~Cleanup(){if(call)call();}};
    int destroyed=0;auto cleanup=std::make_shared<Cleanup>();cleanup->call=[&]{++destroyed;(void)cache.cached();};
    auto cleanupToken=cache.set([cleanup]{return QStringLiteral("cleanup");});cleanup.reset();
    if(!cache.retire(cleanupToken) || destroyed!=1)return fail("retired callback destructor is outside mutex");
    cleanup=std::make_shared<Cleanup>();cleanup->call=[&]{++destroyed;(void)cache.cached();};
    cache.set([cleanup]{return QStringLiteral("replace-cleanup");});cleanup.reset();
    int sequence=0;auto gui=cache.set([&]{return QStringLiteral("gui-%1").arg(++sequence);});
    if(destroyed!=2)return fail("replaced callback destructor is outside mutex");
    auto worker=std::async(std::launch::async,[&]{
        if(cache.set([]{return QStringLiteral("worker-bad");})!=0 || cache.refresh())return false;
        for(int i=0;i<20000;++i)if(!cache.cached().startsWith("gui-"))return false;return true;
    });
    for(int i=0;i<500;++i)if(!cache.refresh())return fail("GUI publication");
    if(!worker.get() || !cache.retire(gui))return fail("background readers never invoke/replace GUI provider");
    auto dying=std::make_unique<GuiContextCache>();auto* raw=dying.get();bool closed=false;
    cleanup=std::make_shared<Cleanup>();cleanup->call=[&]{closed=raw->set([]{return QString();})==0;(void)raw->cached();};
    dying->set([cleanup]{return QStringLiteral("destructor");});cleanup.reset();dying.reset();
    if(!closed)return fail("cache destruction rejects re-registration without holding mutex");
    std::cout<<"Gui diagnostic context: owned retire, callback replacement/recursive refresh/exception, GUI-owner deletion, callback destructor reentry and20000 background copies/GUI-only publication OK\n";
    return 0;
}

class WindowDiagnosticsChecks {
public:
static bool run() {
    const auto fail=[](const char* s){std::cerr<<"Window diagnostics FAILED: "<<s<<'\n';return false;};
    if(QApplication::platformName()!="offscreen")return fail("offscreen required");
    {
        QTemporaryDir folder;QProcess child;auto env=QProcessEnvironment::systemEnvironment();
        env.insert("QT_QPA_PLATFORM","offscreen");env.insert("PIPBONG_DATA_DIR",folder.path());env.insert("PIPBONG_SELF_TEST_CONTEXT_CACHE","1");
        child.setProcessEnvironment(env);child.setProcessChannelMode(QProcess::MergedChannels);
        child.start(QCoreApplication::applicationFilePath(),QStringList());
        if(!child.waitForFinished(5000)){child.kill();child.waitForFinished(1000);return fail("bounded cache subprocess timeout");}
        const auto output=child.readAll();std::cout<<output.toStdString();
        if(child.exitStatus()!=QProcess::NormalExit || child.exitCode()!=0 || !output.contains("Gui diagnostic context:"))return fail("cache subprocess");
    }
    {
        ExecutionContext context;std::atomic<bool> done{false};
        auto writer=std::async(std::launch::async,[&]{
            for(int i=1;i<=30000;++i){context.setImageFindPollAttempt(i);context.setLastMatchAttempt(i,-double(i),{i,-i},true);if(i%7==0)context.clearLastMatchAttempt();}done=true;
        });
        bool coherent=true;int reads=0;
        do {const auto sample=context.matchAttemptSnapshot();++reads;
            if(sample.present && (sample.confidence!=-sample.threshold || !sample.hasPoint || sample.point.x!=sample.threshold || sample.point.y!=-sample.threshold))coherent=false;
            if(!sample.present && (sample.confidence!=0 || sample.threshold!=0 || sample.hasPoint))coherent=false;
            if(context.imageFindPollAttempt()<0)coherent=false;
        }while(!done || reads<30000);
        writer.get();context.setImageFindPollAttempt(-1);
        if(!coherent || context.imageFindPollAttempt()!=0)return fail("coherent match tuple and atomic diagnostic poll during worker publication");
    }
    auto older=std::make_unique<MainWindow>();older->m_foregroundMonitor->stop();older->m_triggerMonitor->stop();older->setPersistentStatus("older-provider");
    if(!CrashReporter::refreshContext() || !CrashReporter::cachedContext().contains("older-provider"))return fail("actual initial registration");
    auto current=std::make_unique<MainWindow>();current->m_foregroundMonitor->stop();current->m_triggerMonitor->stop();current->setPersistentStatus("current-provider");
    auto& w=*current;auto* collision=w.m_project->addFeature("wrong-active-name");collision->setId("diagnostic-owner");
    auto background=std::make_shared<Project>();auto* feature=background->addFeature("real-background-name");feature->setId("diagnostic-owner");
    w.m_sessionOwnerProjects["background-diagnostic"]=background;
    auto& s=w.runSessions()["diagnostic-owner"];s.featureId="diagnostic-owner";s.profileId="background-diagnostic";s.startedFeatureName="started-owner";
    s.runningMode=FeatureRunMode::Hold;s.usesHoldKeyTapFastPath=true;s.holdKeyTapLaneActive=true;s.holdKeyTapLaneToken=77;s.awaitingFirstTemplateRoi=true;
    s.sessionContext=std::make_shared<ExecutionContext>();s.sessionContext->setImageFindPollAttempt(42);s.sessionContext->setLastMatchAttempt(0.8,0.7,{10,20},true);
    auto text=w.m_diagnostics->snapshot();
    if(!text.contains("    - started-owner owner=background-diagnostic") || text.contains("    - wrong-active-name ")
        || !text.contains("lane=1/77 roiWait=1") || !text.contains("engine=hold-tap") || !text.contains("imageFindPoll=42") || !text.contains("lastMatchConf=0.700/0.800"))return fail("owner/fast-lane/ROI and coherent context snapshot");
    s.holdKeyTapLaneActive=false;s.holdKeyTapLaneToken=0;s.awaitingFirstTemplateRoi=false;s.runningMode=FeatureRunMode::RepeatCount;
    s.engine=std::make_unique<WorkflowEngine>();w.connectSessionEngine(s);s.engine->blockStarted(0,"captured-worker-label");
    QCoreApplication::sendPostedEvents(&w,QEvent::MetaCall);s.sessionWorkflow.reset();
    text=w.m_diagnostics->snapshot();
    if(!text.contains("runningBlock: captured-worker-label"))return fail("validated queued block label needs no worker workflow read");
    ++s.runningBlockIndex;if(w.m_diagnostics->snapshot().contains("runningBlock: captured-worker-label"))return fail("stale label index rejected");
    older.reset();if(!CrashReporter::refreshContext() || !CrashReporter::cachedContext().contains("current-provider"))return fail("old window destructor cannot retire newer provider");
    auto worker=std::async(std::launch::async,[&]{return w.m_diagnostics->snapshot().isEmpty() && !CrashReporter::refreshContext() && !CrashReporter::cachedContext().isEmpty();});
    if(!worker.get())return fail("actual off-thread access does not inspect GUI");
    if(!w.prepareForShutdown())return fail("actual window checkpoint");
    const auto last=CrashReporter::cachedContext();current.reset();
    if(CrashReporter::refreshContext() || CrashReporter::cachedContext()!=last)return fail("latest window destruction retires callback but preserves text");
    std::cout<<"Window diagnostics: actual provider replacement/window destruction, GUI-only access, owned background name, Hold token/ROI, coherent match tuple/atomic poll, queued cached block label/index and shutdown OK (offscreen; no native input)\n";
    return true;
}
};
bool runWindowDiagnosticsChecks(){return WindowDiagnosticsChecks::run();}
