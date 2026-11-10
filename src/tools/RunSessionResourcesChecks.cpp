#include <algorithm>
#include <QSemaphore>
#include <QCoreApplication>
#include <QTemporaryDir>
#include <QProcessEnvironment>
#include <QProcess>
#include "core/workflow/WorkflowEngine.h"
#include "app/RunSessionResourcesController.h"
#include "app/RunUiPublisher.h"
#include "app/WorkerRepeatController.h"
#include "app/RunContextController.h"
#include "app/MainWindow.h"
#include "app/ForegroundWindowMonitor.h"
#include "app/TriggerMonitorController.h"
#include "model/Project.h"
#include "core/workflow/Workflow.h"
#include "core/workflow/blocks/WaitBlock.h"
#include <QApplication>
#include <QElapsedTimer>
#include "core/diagnostics/DiagnosticHub.h"
#include <future>
#include <map>
#include <atomic>
#include <iostream>

int runEngineStopBoundaryProbe() {
    const auto workflow=[] {auto w=std::make_shared<Workflow>();auto block=std::make_unique<WaitBlock>();block->ms=5000;w->addBlock(std::move(block));return w;};
    qint64 slowest=0;
    for(int i=0;i<64;++i) {
        auto context=std::make_shared<ExecutionContext>();WorkflowEngine engine;
        if(i%2)engine.runPrepared([&]{return PreparedWorkflowRun{workflow(),context};});
        else engine.run(workflow(),context);
        QElapsedTimer timer;timer.start();const bool stopped=engine.stopAndWaitBounded(1000);slowest=std::max(slowest,timer.elapsed());
        if(!stopped){std::cerr<<"Engine stop probe FAILED: immediate="<<i<<" elapsed="<<timer.elapsed()<<" stop="<<context->shouldStop()
            <<" running="<<engine.isRunning()<<" worker="<<engine.hasLiveWorker()<<'\n'<<DiagnosticHub::buildWorkerStatusText().toStdString()<<'\n';return 71;}
    }
    for(int i=0;i<16;++i) {
        auto context=std::make_shared<ExecutionContext>();QSemaphore arrived,release;WorkflowEngine engine;
        QObject::connect(&engine,&WorkflowEngine::sessionPrepared,&engine,[&]{arrived.release();release.acquire();},Qt::DirectConnection);
        engine.run(workflow(),context);
        const bool ready=arrived.tryAcquire(1,2000);engine.stop();release.release();
        if(!ready || !engine.stopAndWaitBounded(1000) || !context->shouldStop()){
            std::cerr<<"Engine stop probe FAILED: prepared handoff="<<i<<" ready="<<ready<<" stop="<<context->shouldStop()<<'\n';return 72;}
    }
    std::cout<<"Engine stop boundary:64 immediate normal/prepared jobs and16 forced claimed-context handoffs stopped within1000ms; slowest="<<slowest<<"ms (Wait-only, no native input)\n";
    return 0;
}

class RunSessionResourcesChecks {
    using Controller=RunSessionResourcesController;
    struct Fixture {
        Project active,background;quint64 generation=1;QString owner="active";
        std::map<std::string,FeatureRunSession> sessions;
        std::shared_ptr<std::atomic<bool>> held=std::make_shared<std::atomic<bool>>(true);
        bool target=true,capture=true,repeat=true;int warnings=0,batches=0;QString batchOwner;
        std::function<void()> onBind,onWarning;
        std::unique_ptr<RunUiPublisher> publisher;
        std::unique_ptr<Controller> controller;
        Fixture(){
            RunUiPublisher::HostCallbacks ui;ui.session=[this](const auto& id){return find(id);};ui.activeRepeatCount=[] {return 1;};ui.isDisplayed=[](auto*){return true;};
            ui.timing=[](auto){};ui.log=[](const auto&,const auto&,bool){};ui.refreshRunUi=[]{};
            ui.afterBatch=[this](auto& s){++batches;if(controller)controller->afterBatch(s);};publisher=std::make_unique<RunUiPublisher>(std::move(ui));
            Controller::Host host;host.workspace=[this]{return Controller::Workspace{&active,owner,generation};};
            host.sessions=[this]{std::vector<std::string> ids;for(const auto& e:sessions)ids.push_back(e.first);return ids;};host.session=[this](const auto& id){return find(id);};
            host.feature=[this](auto& s){return (s.profileId=="background"?background:active).featureById(s.featureId);};
            host.binding=[this](auto& s,bool){Controller::Binding value{L"owned-title",L"C:/owned.exe",s.profileId.toStdString()+"/directory"};if(onBind)onBind();return value;};
            host.targetAllowed=[this](auto&,auto*){return target;};host.captureAllowed=[this](const auto&){return capture;};host.repeatAllowed=[this](auto&,auto*){return repeat;};
            host.holdReader=[this](const auto&){return std::function<bool()>([value=held]{return value->load();});};
            host.warning=[this](const auto&){++warnings;if(onWarning)onWarning();};host.afterBatch=[this](auto&,auto* f){batchOwner=f?QString::fromStdString(f->name()):QString();};
            controller=std::make_unique<Controller>(*publisher,std::move(host));
        }
        FeatureRunSession* find(const std::string& id){auto it=sessions.find(id);return it==sessions.end()?nullptr:&it->second;}
        Feature* add(Project& project,const std::string& id,const std::string& name,int ms){auto* f=project.addFeature(name);f->setId(id);f->setPointerVisualFeedback(false);auto w=std::make_unique<WaitBlock>();w->ms=ms;f->workflow().addBlock(std::move(w));return f;}
        FeatureRunSession& make(const std::string& id="run",const QString& profile="active"){
            sessions.erase(id);auto& s=sessions[id];s.featureId=id;s.profileId=profile;s.engine=std::make_unique<WorkflowEngine>();s.repeatSession=true;s.runningMode=FeatureRunMode::RepeatCount;s.repeatRemaining=4;return s;
        }
        static int waitMs(const FeatureRunSession& s){return static_cast<WaitBlock*>(s.sessionWorkflow->blocks()[0].get())->ms;}
    };
public:
static bool run(){
    {
        QTemporaryDir folder;QProcess child;auto env=QProcessEnvironment::systemEnvironment();
        env.insert("QT_QPA_PLATFORM","offscreen");env.insert("PIPBONG_DATA_DIR",folder.path());env.insert("PIPBONG_SELF_TEST_ENGINE_STOP","1");
        child.setProcessEnvironment(env);child.setProcessChannelMode(QProcess::MergedChannels);child.start(QCoreApplication::applicationFilePath(),QStringList());
        if(!child.waitForFinished(15000)){child.kill();child.waitForFinished(1000);std::cerr<<"Engine stop probe subprocess timeout\n";return false;}
        const auto output=child.readAll();std::cout<<output.toStdString();
        if(child.exitStatus()!=QProcess::NormalExit || child.exitCode()!=0 || !output.contains("Engine stop boundary:"))return false;
    }

    const auto fail=[](const char* label){std::cerr<<"Run resources FAILED: "<<label<<'\n';return false;};
    {
        Fixture f;auto* feature=f.add(f.active,"run","active",25);auto& s=f.make();
        if(!f.controller->ensure(s,feature,true,false) || !s.sessionContext || !s.sessionWorkflow || Fixture::waitMs(s)!=25
            || s.sessionWorkflow->blocks()[0].get()==feature->workflow().blocks()[0].get()
            || s.sessionContext->projectDirectory()!="active/directory" || s.sessionContext->targetWindowBinding()!=std::make_pair(std::wstring(L"owned-title"),std::wstring(L"C:/owned.exe")))return fail("clone/context/binding ownership");
        static_cast<WaitBlock*>(feature->workflow().blocks()[0].get())->ms=99;
        if(Fixture::waitMs(s)!=25)return fail("running snapshot isolated from model edits");
        f.controller->requestRefresh("run");if(Fixture::waitMs(s)!=99)return fail("idle owner refresh");
        auto wait=std::make_shared<Workflow>();auto block=std::make_unique<WaitBlock>();block->ms=2000;wait->addBlock(std::move(block));
        s.engine->run(wait,s.sessionContext);f.controller->requestRefresh("run");f.controller->requestRefresh("run");
        if(!s.deferredSessionWorkflowRefresh || f.warnings!=1 || f.controller->applyDeferred(s,feature) || f.controller->applyPolicies(s,feature))return fail("live engine defers clone/policy mutation once");
        QElapsedTimer stopTimer;stopTimer.start();
        if(!s.engine->stopAndWaitBounded(1000)){
            std::cerr<<"Stop boundary detail: elapsed="<<stopTimer.elapsed()<<" running="<<s.engine->isRunning()
                <<" worker="<<s.engine->hasLiveWorker()<<" stop="<<s.sessionContext->shouldStop()<<" paused="<<s.sessionContext->isPaused()
                <<'\n'<<DiagnosticHub::buildWorkerStatusText().toStdString()<<'\n';
            return fail("real Wait stop");
        }
        if(!f.controller->applyDeferred(s,feature) || s.deferredSessionWorkflowRefresh || !f.controller->applyPolicies(s,feature))return fail("idle deferred application and gates");
        f.target=false;f.capture=true;f.controller->synchronize();if(s.sessionContext->scopedTargetPollAllowed() || !s.workerGates->captureAllowed())return fail("atomic worker gates");
    }
    {
        Fixture f;f.add(f.active,"run","same-id active",900);auto* owned=f.add(f.background,"run","background owner",222);owned->setLoopIntervalMs(35);auto& s=f.make("run","background");
        if(!f.controller->ensure(s,owned,true,false) || !f.controller->applyPolicies(s,owned) || !f.controller->configureRepeat(s,owned))return fail("background resource configuration");
        f.controller->requestRefresh("run");if(Fixture::waitMs(s)!=222 || s.deferredSessionWorkflowRefresh)return fail("active same-id edit cannot replace background workflow");
        if(s.sessionContext->workerFastRepeatDelayMs()!=35 || !s.sessionContext->hasWorkerFastRepeat())return fail("background repeat options");
        owned->setLoopIntervalMs(45);f.controller->afterBatch(s);
        if(f.batchOwner!="background owner" || s.sessionContext->workerFastRepeatDelayMs()!=45)return fail("batch owner is not active same-id feature");
        auto context=s.sessionContext;f.controller.reset();
        auto worker=std::async(std::launch::async,[context]{for(int i=0;i<10;++i)context->notifyWorkerFastRepeatIteration(true,2,"detached control");});worker.get();f.publisher->flush("run");
        if(s.completedLoopCount!=10 || context->workerFastRepeatDelayMs()!=45)return fail("worker callbacks own values after configuration controller destruction");
    }
    {
        Fixture f;auto* feature=f.add(f.active,"run","replace binding",1);auto& s=f.make();f.controller->ensure(s,feature,true,false);const auto old=s.sessionContext;
        f.onBind=[&]{auto& next=f.make();next.sessionContext=std::make_shared<ExecutionContext>();next.sessionContext->setProjectDirectory("replacement");};
        if(f.controller->syncContext(s) || f.find("run")->sessionContext->projectDirectory()!="replacement" || old->projectDirectory()!="active/directory")return fail("binding callback replacement isolation");
    }
    {
        Fixture f;auto* feature=f.add(f.active,"run","warning reentry",1);auto& s=f.make();f.controller->ensure(s,feature,true,false);
        auto workflow=std::make_shared<Workflow>();auto block=std::make_unique<WaitBlock>();block->ms=2000;workflow->addBlock(std::move(block));s.engine->run(workflow,s.sessionContext);
        bool marked=false;f.onWarning=[&]{marked=f.find("run")->deferredSessionWorkflowRefresh;f.make();};
        f.controller->requestRefresh("run");if(!marked || f.find("run")->deferredSessionWorkflowRefresh || f.warnings!=1)return fail("defer mutation completes before reentrant status publication");
    }
    {
        Fixture f;auto* feature=f.add(f.active,"run","defer clone",5);auto& s=f.make();
        if(!f.controller->ensure(s,feature,true,true) || s.sessionWorkflow || !s.sessionContext)return fail("worker-deferred clone does not force GUI clone");
        if(!f.controller->ensure(s,feature,true,false) || !f.controller->configureRepeat(s,feature))return fail("repeat setup");
        s.repeatSession=false;if(!f.controller->configureRepeat(s,feature) || s.sessionContext->hasWorkerFastRepeat() || s.workerRepeat)return fail("nonrepeat clears control/channel");
    }
    {
        if(QApplication::platformName()!="offscreen")return fail("UI fixture requires offscreen");
        MainWindow window;window.m_foregroundMonitor->stop();window.m_triggerMonitor->stop();
        auto* feature=window.m_project->addFeature("resource integration");feature->setId("resource-ui");feature->setPointerVisualFeedback(false);auto block=std::make_unique<WaitBlock>();block->ms=25;feature->workflow().addBlock(std::move(block));
        FeatureRunSession created;created.featureId="resource-ui";created.profileId=window.m_profileManager->activeProfileId();created.runningMode=FeatureRunMode::RepeatCount;created.repeatSession=true;created.repeatRemaining=3;created.engine=std::make_unique<WorkflowEngine>();
        auto& s=window.runSessions().emplace(created.featureId,std::move(created)).first->second;
        window.ensureRunSessionResources(s,feature,true);window.configureWorkerFastRepeat(s,feature);
        if(!s.sessionContext || !s.sessionContext->hasWorkerFastRepeat() || s.sessionContext->projectDirectory()!=window.m_profileManager->projectDirectory(s.profileId).toStdString())return fail("actual shell resource delegates");
        s.sessionContext->notifyWorkerFastRepeatIteration(true,11,"resource integration");window.flushWorkerFastRepeatUi(s.featureId);
        if(s.completedLoopCount!=1 || s.totalLoopElapsedMs!=11)return fail("actual configured worker-to-GUI channel");
        const QString owner=window.m_profileManager->addProfile("resource owner",false);if(owner.isEmpty())return fail("actual background profile");
        auto background=std::make_shared<Project>();auto* backgroundFeature=background->addFeature("background resource");backgroundFeature->setId("resource-ui");auto ownedWait=std::make_unique<WaitBlock>();ownedWait->ms=222;backgroundFeature->workflow().addBlock(std::move(ownedWait));
        window.m_sessionOwnerProjects[owner]=background;s.profileId=owner;s.sessionWorkflow=std::shared_ptr<Workflow>(backgroundFeature->workflow().clone());
        window.syncRunSessionContext(s,false);window.requestSessionWorkflowRefresh("resource-ui");
        if(Fixture::waitMs(s)!=222 || s.sessionContext->projectDirectory()!=window.m_profileManager->projectDirectory(owner).toStdString())return fail("actual same-id background edit/directory retention");
        if(!window.prepareForShutdown())return fail("actual resource shutdown");
    }
    std::cout<<"Run resources: cloned/deferred workflows, real Wait idle boundary, owned binding/directory, same-id background isolation, atomic gates/options, reentrant bind/warning, detached worker values and actual MainWindow worker channel/checkpoint OK (offscreen; no native input)\n";
    return true;
}
};
bool runRunSessionResourcesChecks(){return RunSessionResourcesChecks::run();}
