#include "app/FirstTemplateRoiController.h"
#include "app/FeatureRunSession.h"
#include "app/MainWindow.h"
#include "app/ForegroundWindowMonitor.h"
#include "app/TriggerMonitorController.h"
#include "model/Project.h"
#include "core/workflow/Workflow.h"
#include "core/workflow/blocks/ImageFindBlock.h"
#include <QApplication>
#include <QTimer>
#include <cmath>
#include <unordered_map>
#include <iostream>

class FirstTemplateRoiChecks {
    using Controller=FirstTemplateRoiController;
    struct Fixture {
        std::unique_ptr<Project> project=std::make_unique<Project>();quint64 generation=1;QString owner="owner";
        std::unordered_map<std::string,std::unique_ptr<FeatureRunSession>> sessions;
        std::vector<Controller::Presentation> previews;QRect bounds{-500,-300,1000,800};
        int changed=0,launched=0,cancelled=0,dismissed=0;bool showResult=true;
        std::function<void()> onChanged,onDismiss,onShow,onWorkspace,onLaunch;
        std::function<QRect(const std::shared_ptr<ExecutionContext>&)> readBounds;
        std::unique_ptr<Controller> controller;
        Fixture(){
            Controller::Host host;host.workspace=[this]{if(onWorkspace)onWorkspace();return Controller::Workspace{project.get(),owner,generation};};
            host.session=[this](const std::string& id){auto it=sessions.find(id);return it==sessions.end()?nullptr:it->second.get();};
            host.cancel=[this](const std::string& id){++cancelled;sessions.erase(id);};
            host.changed=[this]{++changed;if(onChanged)onChanged();};host.launch=[this](auto&,auto&){++launched;if(onLaunch)onLaunch();};
            Controller::Ports ports;ports.bounds=[this](const auto& context){return readBounds?readBounds(context):bounds;};
            ports.show=[this](const auto& p){previews.push_back(p);if(onShow)onShow();return showResult;};
            ports.dismiss=[this]{++dismissed;const auto hidden=previews.empty()?RoiPreviewOverlay::VisibilityHandler{}:previews.back().visibility;if(onDismiss)onDismiss();if(hidden)hidden(false);};
            controller=std::make_unique<Controller>(std::move(host),std::move(ports));
        }
        Feature* add(const std::string& id="roi"){
            auto* f=project->addFeature(id);f->setId(id);f->setEditFirstTemplateRoiOnStart(true);f->setPointerVisualFeedback(false);
            auto block=std::make_unique<ImageFindBlock>();block->templatePaths={"fixture-never-executed.png"};
            block->customRegionsWindowPercent={{0,0,0,0},{10,20,30,40},{100,0,10,10}};
            f->workflow().addBlock(std::move(block));resetSession(id);return f;
        }
        void resetSession(const std::string& id="roi"){
            auto s=std::make_unique<FeatureRunSession>();s->featureId=id;s->profileId=owner;s->engine=std::make_unique<WorkflowEngine>();sessions[id]=std::move(s);
        }
        ImageFindBlock* block(const std::string& id="roi"){return static_cast<ImageFindBlock*>(project->featureById(id)->workflow().blocks()[0].get());}
        bool begin(const std::string& id="roi"){return controller->begin(*sessions.at(id),project->featureById(id));}
    };
public:
static bool run(){
    if(QApplication::platformName()!="offscreen")return false;
    const auto fail=[](const char* label){std::cerr<<"First template ROI FAILED: "<<label<<'\n';return false;};
    const auto edit=CaptureRegion{-300,-60,200,160};
    {
        Fixture f;f.add();f.add("other");
        f.sessions.at("roi")->lockedCaptureTargetTitle=L"owner-A";f.sessions.at("roi")->lockedCaptureTargetProcessPath=L"C:/A.exe";
        f.sessions.at("other")->lockedCaptureTargetTitle=L"owner-B";f.sessions.at("other")->lockedCaptureTargetProcessPath=L"C:/B.exe";
        const QRect a(-800,-400,1000,600),b(200,100,1200,800);int reads=0;
        f.readBounds=[&](const auto& context){++reads;return context && context->targetWindowBinding().first==L"owner-A"?a:b;};
        f.begin();const auto first=f.previews.back();f.bounds=QRect(9000,9000,10,10);f.begin("other");
        if(reads!=2 || first.editable.fixedTargetBounds!=a || f.previews.back().editable.fixedTargetBounds!=b
            || f.sessions.at("other")->sessionContext->targetWindowBinding().second!=L"C:/B.exe")return fail("pending context binds frozen session target before owned frame query");
    }
    {
        Fixture f;f.add();f.readBounds=[&](const auto& context){context->setTargetWindowBindingForWorker(L"replacement",L"C:/new.exe");return f.bounds;};
        if(!f.begin() || !f.previews.empty() || f.cancelled!=1 || !f.sessions.empty())return fail("binding replacement during frame read cancels only owned pending launch");
    }
    {
        Fixture f;f.add();f.begin();const auto old=f.previews.back();const auto before=f.block()->toJson();
        f.sessions.at("roi")->sessionContext->setTargetWindowBindingForWorker(L"replacement",L"C:/new.exe");
        old.editable.onRoiEdited(0,edit);old.editable.onConfirm();
        if(f.cancelled!=1 || f.changed || f.launched || !f.sessions.empty() || f.block()->toJson()!=before)return fail("changed binding cannot commit a frozen ROI frame");
    }
    {
        Fixture f;f.add();f.readBounds=[&](const auto&){f.sessions.at("roi")->sessionContext=std::make_shared<ExecutionContext>();return f.bounds;};
        if(!f.begin() || f.cancelled || !f.previews.empty() || f.sessions.size()!=1)return fail("frame callback replacement retains successor context");
    }
    {
        Fixture f;f.add();f.readBounds=[&](const auto&){f.controller.reset();return f.bounds;};
        if(!f.begin() || f.controller || !f.previews.empty())return fail("frame callback controller retirement");
    }
    {
        Fixture f;f.add();const auto before=f.block()->toJson();
        if(!f.begin() || !f.sessions.at("roi")->awaitingFirstTemplateRoi || f.previews.size()!=1 || f.previews[0].regions.size()!=1 || f.previews[0].regions[0].x!=-400
            || f.previews[0].regions[0].y!=-140 || f.previews[0].editable.fixedTargetBounds!=f.bounds)return fail("filtered source mapping and frozen negative-coordinate bounds");
        const auto callbacks=f.previews.back();callbacks.editable.onRoiEdited(-1,edit);callbacks.editable.onRoiEdited(99,edit);callbacks.editable.onRoiEdited(0,{1,1,0,0});
        f.bounds=QRect(100,100,2000,1000);callbacks.editable.onRoiEdited(0,edit);
        if(f.block()->toJson()!=before || f.changed || f.launched)return fail("preview stages without model publication");
        callbacks.editable.onConfirm();callbacks.editable.onConfirm();callbacks.visibility(false);
        const auto& regions=f.block()->customRegionsWindowPercent;
        if(f.changed!=1 || f.launched!=1 || f.cancelled || f.sessions.at("roi")->awaitingFirstTemplateRoi || regions[0].width!=0 || regions[1].x!=20 || regions[1].y!=30 || regions[1].width!=20 || regions[1].height!=20
            || regions[2].x!=100 || f.block()->searchArea!=SearchArea::CustomRegion)return fail("confirm commits original source index once in frozen frame");
    }
    {
        Fixture f;f.add();const auto before=f.block()->toJson();f.begin();const auto callbacks=f.previews.back();
        callbacks.editable.onRoiEdited(0,edit);callbacks.visibility(false);callbacks.visibility(false);callbacks.editable.onConfirm();
        if(f.cancelled!=1 || !f.sessions.empty() || f.block()->toJson()!=before || f.changed || f.launched)return fail("cancel discards staged edits and stops owned wait once");
    }
    {
        Fixture f;f.add();f.begin();const auto callbacks=f.previews.back();f.resetSession();
        callbacks.editable.onRoiEdited(0,edit);callbacks.editable.onConfirm();callbacks.visibility(false);
        if(f.cancelled || f.changed || f.launched || f.sessions.size()!=1)return fail("same-id replacement session isolation");
    }
    {
        Fixture f;f.add();f.begin();const auto callbacks=f.previews.back();
        auto next=std::make_unique<Project>();auto* feature=next->addFeature("collision");feature->setId("roi");f.project=std::move(next);++f.generation;
        QTimer::singleShot(0,qApp,[callbacks,edit]{callbacks.editable.onRoiEdited(0,edit);callbacks.editable.onConfirm();callbacks.visibility(false);});
        QApplication::processEvents();
        if(f.cancelled!=1 || f.changed || f.launched || !feature->workflow().blocks().empty())return fail("queued callbacks after actual source destruction");
    }
    {
        Fixture f;f.add();f.begin();const auto callbacks=f.previews.back();
        f.project->featureById("roi")->workflow().replaceBlock(0,std::make_unique<ImageFindBlock>());
        callbacks.editable.onRoiEdited(0,edit);callbacks.editable.onConfirm();if(f.cancelled!=1 || f.changed || f.launched)return fail("replaced block cannot receive old pointer edits");
    }
    {
        Fixture f;f.add();f.add("other");f.begin();const auto old=f.previews.back();f.begin("other");const auto fresh=f.previews.back();
        old.editable.onConfirm();old.visibility(false);fresh.editable.onConfirm();
        if(f.cancelled!=1 || f.launched!=1 || f.sessions.count("roi") || !f.sessions.count("other"))return fail("preview replacement isolates old callbacks");
    }
    {
        Fixture f;f.add();f.showResult=false;if(f.begin() || f.sessions.empty() || f.sessions.at("roi")->awaitingFirstTemplateRoi)return fail("unshown valid preview preserves normal launch fallback");
        const auto old=f.previews.back();old.editable.onConfirm();if(f.launched)return fail("failed-show callbacks inert");
    }
    {
        Fixture f;f.add();f.onShow=[&]{f.previews.back().editable.onConfirm();};
        if(!f.begin() || f.launched!=1)return fail("synchronous confirm consumes launch once");
    }
    {
        Fixture f;f.add();f.add("other");f.begin();const auto old=f.previews.back();bool replaced=false;
        f.onDismiss=[&]{if(!replaced){replaced=true;f.begin("other");}};
        old.editable.onConfirm();
        if(!replaced || f.cancelled!=1 || f.launched || f.previews.size()!=2 || !f.sessions.count("other"))return fail("dismissal reentry cancels only old confirming session");
        f.previews.back().editable.onConfirm();if(f.launched!=1)return fail("dismissal replacement remains confirmable");
    }
    {
        Fixture f;f.add();f.begin();const auto old=f.previews.back();bool reentered=false;
        f.onDismiss=[&]{if(!reentered){reentered=true;f.begin();}};old.editable.onConfirm();
        if(!reentered || f.cancelled || f.launched!=1 || f.previews.size()!=1)return fail("same-session confirmation reentry does not duplicate preview or launch");
    }
    {
        Fixture f;f.add();f.begin();const auto callbacks=f.previews.back();f.onChanged=[&]{f.resetSession();};
        callbacks.editable.onRoiEdited(0,edit);callbacks.editable.onConfirm();if(f.changed!=1 || f.launched || f.cancelled)return fail("publication callback revalidates replacement context");
    }
    {
        Fixture f;f.add();f.begin();const auto callbacks=f.previews.back();f.controller.reset();
        callbacks.editable.onRoiEdited(0,edit);callbacks.editable.onConfirm();callbacks.visibility(false);
        if(f.changed || f.launched || f.cancelled)return fail("controller lifetime");
    }
    {
        Fixture f;f.add();f.add("other");f.begin();const auto old=f.previews.back();
        f.controller->retire(*f.sessions.at("roi"));
        if(f.sessions.at("roi")->awaitingFirstTemplateRoi || f.cancelled)return fail("lifecycle retirement clears owned gate without recursive erase");
        f.sessions.erase("roi");f.begin("other");QApplication::processEvents();
        old.editable.onRoiEdited(0,edit);old.editable.onConfirm();old.visibility(false);
        if(f.dismissed || f.changed || f.launched || !f.sessions.count("other"))return fail("queued retirement dismissal cannot close replacement preview");
        f.previews.back().editable.onConfirm();if(f.launched!=1)return fail("replacement survives old lifecycle retirement");
    }
    {
        Fixture f;f.add();f.begin();f.onLaunch=[&]{f.controller->retire(*f.sessions.at("roi"));};
        f.previews.back().editable.onConfirm();QApplication::processEvents();
        if(f.launched!=1 || f.dismissed!=1)return fail("settled launch retirement does not schedule another global dismissal");
    }
    {
        Fixture f;f.add();f.begin();const auto callbacks=f.previews.back();f.onWorkspace=[&]{f.controller.reset();};
        callbacks.editable.onRoiEdited(0,edit);callbacks.editable.onConfirm();
        if(f.controller || f.changed || f.launched || f.cancelled)return fail("destruction during callback validation");
    }
    {
        Fixture f;f.add();f.onWorkspace=[&]{f.controller.reset();};
        if(!f.begin() || f.controller || f.launched || !f.previews.empty())return fail("initial source read lifetime consumes stale launch");
    }
    {
        Controller::Presentation preview;int shown=0,launched=0,dismissals=0;
        MainWindow window;window.m_foregroundMonitor->stop();window.m_triggerMonitor->stop();
        auto* feature=window.m_project->addFeature("first ROI integration");feature->setId("first-roi-ui");feature->setEditFirstTemplateRoiOnStart(true);feature->setPointerVisualFeedback(false);
        feature->setRunMode(FeatureRunMode::Trigger);
        auto block=std::make_unique<ImageFindBlock>();block->templatePaths={"fixture-never-executed.png"};block->customRegionsWindowPercent={{10,20,30,40}};auto* raw=block.get();feature->workflow().addBlock(std::move(block));
        auto* controller=window.m_firstTemplateRoi.get();controller->m_ports.bounds=[](const auto&) {return QRect(-500,-300,1000,800);};
        controller->m_ports.show=[&](const auto& p){preview=p;++shown;return true;};controller->m_ports.dismiss=[&]{++dismissals;preview.visibility(false);};
        controller->m_host.launch=[&](auto& session,auto& launchedFeature){if(session.featureId=="first-roi-ui" && &launchedFeature==feature)++launched;};
        window.m_modified=false;window.onFeatureRunRequested("first-roi-ui");
        auto* pending=window.sessionFor("first-roi-ui");
        if(shown!=1 || !pending || !pending->awaitingFirstTemplateRoi || launched)return fail("actual lifecycle waits for recorded overlay");
        int automatic=0,resumed=0;
        TriggerMonitorController supervisor(*window.m_profileManager);TriggerMonitorController::Host watch;
        watch.workspace=[&]{return TriggerMonitorController::Workspace{window.m_project.get(),window.m_profileManager->activeProfileId(),1,false};};
        watch.sessions=[] {return std::vector<std::string>{"first-roi-ui"};};watch.session=[&](const auto& id){return window.sessionFor(id);};
        watch.feature=[&](auto& session){return window.featureForSession(session);};watch.mayContinue=[](auto&,auto*){return true;};watch.launch=[&](auto&,auto*){++automatic;};supervisor.setHost(std::move(watch));
        RunSessionController scoped;scoped.setRunSessions(&window.runSessions());scoped.setProject(window.m_project.get());RunSessionController::HostCallbacks resume;
        resume.featureForSession=[&](auto& session){return window.featureForSession(session);};resume.runForegroundGateActiveForSession=[](auto&,auto*){return true;};
        resume.launchTriggerMonitor=[&](auto&,auto*,bool){++resumed;};resume.launchWorkflowRun=[&](auto&,auto*,bool){++resumed;};scoped.setHostCallbacks(std::move(resume));
        supervisor.ensure();pending->waitingForScopedTargetForeground=true;scoped.resumeWaitingScopedTargetForegroundSessions();
        if(automatic || resumed || !pending->waitingForScopedTargetForeground)return fail("automatic monitor/scoped resume cannot bypass ROI confirmation");
        const auto before=raw->toJson();preview.editable.onRoiEdited(0,edit);
        if(window.m_modified || raw->toJson()!=before)return fail("actual dirty state unchanged during preview");
        preview.editable.onConfirm();if(!window.m_modified || launched!=1 || pending->awaitingFirstTemplateRoi || raw->customRegionsWindowPercent[0].x!=20)return fail("actual confirmation publishes and dispatches owned session");
        scoped.resumeWaitingScopedTargetForegroundSessions();supervisor.ensure();
        if(resumed!=1 || automatic!=1)return fail("confirmation releases automatic monitor/scoped gates");
        if(!window.tryBeginFirstTemplateRoiEdit(*pending,feature))return fail("stop-preview setup");
        const auto stopped=preview;const auto committed=raw->toJson();window.stopFeatureRun("first-roi-ui");QApplication::processEvents();
        stopped.editable.onRoiEdited(0,edit);stopped.editable.onConfirm();
        if(controller->m_current || dismissals!=2 || launched!=1 || raw->toJson()!=committed)return fail("actual stop retires preview and old callbacks without dispatch");
        if(!window.prepareForShutdown())return fail("actual checkpoint");
    }
    std::cout<<"First template ROI: owned context frame/binding replacement, staged/cancelled/confirmed edits, filtered source index/frozen negative geometry, queued source/block destruction, same-id context and preview replacement, failed-show/synchronous confirm, callback/lifetime and actual MainWindow waiting/dirty/launch/checkpoint OK (offscreen; recorded preview/launch; no native overlay/input)\n";
    return true;
}
};
bool runFirstTemplateRoiChecks(){return FirstTemplateRoiChecks::run();}
