#include "app/RunSessionResourcesController.h"
#include "app/FeatureRunSession.h"
#include "app/RunSessionPolicySnapshot.h"
#include "app/RunContextController.h"
#include "app/RunUiPublisher.h"
#include "app/WorkerRepeatController.h"
#include "model/Project.h"
#include "core/workflow/Workflow.h"

using Controller=RunSessionResourcesController;
namespace {
bool same(const Controller::Workspace& a,const Controller::Workspace& b){return a.project==b.project && a.owner==b.owner && a.generation==b.generation;}
bool idle(const FeatureRunSession& s){return !s.holdKeyTapLaneActive && (!s.engine || !s.engine->isRunning());}
}
struct Controller::Ticket {
    QPointer<const Controller> life;Host host;Workspace source;
    std::string id;QString owner;FeatureRunSession* original=nullptr;
    std::shared_ptr<ExecutionContext> context;QPointer<WorkflowEngine> engine;
    FeatureRunSession* current()const{
        if(!life || !host.session)return nullptr;
        auto* s=host.session(id);
        return life && s==original && s && s->profileId==owner && s->sessionContext==context && s->engine.get()==engine.data()?s:nullptr;
    }
    bool workspace()const{if(!life || !host.workspace)return false;const auto next=host.workspace();return life && same(source,next);}
    Feature* feature()const{auto* s=current();if(!s || !workspace() || !host.feature)return nullptr;auto* f=host.feature(*s);return current() && workspace()?f:nullptr;}
};
Controller::RunSessionResourcesController(RunUiPublisher& publisher,Host host,QObject* parent)
    :QObject(parent),m_host(std::move(host)),m_publisher(&publisher){}
Controller::Ticket Controller::ticket(FeatureRunSession& s)const{
    Ticket t;t.life=this;t.host=m_host;t.id=s.featureId;t.owner=s.profileId;t.original=&s;t.context=s.sessionContext;t.engine=s.engine.get();
    if(t.host.workspace)t.source=t.host.workspace();return t;
}
bool Controller::refreshWorkflow(FeatureRunSession& s,Feature* feature){
    const auto t=ticket(s);if(!feature || t.feature()!=feature)return false;
    auto cloned=std::shared_ptr<Workflow>(feature->workflow().clone());
    auto* current=t.current();if(!current || !t.workspace())return false;
    current->sessionWorkflow=std::move(cloned);current->deferredSessionWorkflowRefresh=false;return true;
}
bool Controller::applyDeferred(FeatureRunSession& s,Feature* feature){
    if(!s.deferredSessionWorkflowRefresh || !idle(s))return false;return refreshWorkflow(s,feature);
}
void Controller::requestRefresh(const std::string& id){
    const Host host=m_host;const QPointer<Controller> life(this);if(!host.session)return;
    auto* s=host.session(id);if(!life || !s)return;
    const auto t=ticket(*s);
    // This is an edit of the active document. A background owner may reuse ID.
    if(!t.workspace() || (!t.owner.isEmpty() && t.owner!=t.source.owner) || !t.source.project)return;
    auto* feature=t.source.project->featureById(id);if(!feature || t.feature()!=feature)return;
    switch(SessionRunPolicy::workflowRefreshOnProjectEdit(runSessionPolicyInput(*s))){
    case WorkflowRefreshDecision::SkipInactive:return;
    case WorkflowRefreshDecision::Defer:{
        const bool notify=!s->deferredSessionWorkflowRefresh;s->deferredSessionWorkflowRefresh=true;
        if(notify && host.warning)host.warning(tr("워크플로 변경은 현재 감시·동작이 끝난 뒤 반영됩니다"));return;
    }
    case WorkflowRefreshDecision::Immediate:refreshWorkflow(*s,feature);return;
    }
}
bool Controller::ensure(FeatureRunSession& s,Feature* feature,bool refresh,bool deferClone){
    const auto t=ticket(s);if(!feature || t.feature()!=feature)return false;
    if((refresh || !s.sessionWorkflow) && !deferClone){s.sessionWorkflow=std::shared_ptr<Workflow>(feature->workflow().clone());s.deferredSessionWorkflowRefresh=false;}
    if(!s.sessionContext)s.sessionContext=std::make_shared<ExecutionContext>();
    return syncContext(s);
}
bool Controller::syncContext(FeatureRunSession& s,bool refresh){
    const auto t=ticket(s);const auto context=t.context;if(!context || !t.host.binding || !t.life)return false;
    const bool registered=t.host.session && t.host.session(t.id)==&s;
    if(!t.life)return false;
    const auto binding=t.host.binding(s,refresh);
    if(!t.life || !t.workspace() || (registered && !t.current()))return false;
    // Borrowed caller-owned contexts are pinned by value; no borrowed session
    // is dereferenced after the binding callback.
    context->setTargetWindowBindingForWorker(binding.title,binding.process);context->setProjectDirectory(binding.directory);return true;
}
bool Controller::applyPolicies(FeatureRunSession& s,Feature* feature){
    const auto t=ticket(s);if(!t.context || !idle(s) || !feature || t.feature()!=feature)return false;
    RunContextController::apply(s,*feature);synchronize();return true;
}
bool Controller::configureRepeat(FeatureRunSession& s,Feature* feature){
    const auto t=ticket(s);if(!t.context || !idle(s) || !feature || t.feature()!=feature || !m_publisher)return false;
    if(!s.repeatSession || (s.runningMode!=FeatureRunMode::Hold && s.runningMode!=FeatureRunMode::RepeatInfinite && s.runningMode!=FeatureRunMode::RepeatCount)){
        t.context->clearWorkerFastRepeatCallbacks();s.workerRepeat.reset();m_publisher->retire(s.featureId);return true;
    }
    WorkerRepeatController::Config config;config.policy=runSessionPolicyInput(s);config.initialFailures=s.consecutiveDetectionFailLoops;
    config.options=WorkerRepeatController::optionsFromFeature(*feature);
    auto hold=t.host.holdReader?t.host.holdReader(s.featureId):std::function<bool()>{};
    if(!t.current() || !t.workspace())return false;
    const bool foreground=t.host.repeatAllowed && t.host.repeatAllowed(s,feature);
    if(!t.current() || !t.workspace() || !m_publisher)return false;
    t.context->setWorkerRepeatRemaining(s.runningMode==FeatureRunMode::RepeatCount?s.repeatRemaining:0);
    auto control=std::make_shared<WorkerRepeatController>(config,t.context,std::move(hold));control->setForegroundAllowed(foreground);s.workerRepeat=control;
    const auto channel=m_publisher->begin(s.featureId);
    ExecutionContext::WorkerFastRepeatCallbacks callbacks;
    callbacks.delayMs=[control]{return control->delayMs();};callbacks.shouldContinue=[control](bool success,bool failed){return control->shouldContinue(success,failed);};
    callbacks.onIterationComplete=[channel](bool success,std::int64_t elapsed,const std::string& message){RunUiPublisher::record(channel,success,elapsed,QString::fromStdString(message));};
    t.context->setWorkerFastRepeatCallbacks(std::move(callbacks));return true;
}
void Controller::synchronize(){
    const Host host=m_host;const QPointer<Controller> life(this);if(!host.sessions || !host.session)return;
    const auto ids=host.sessions();
    for(const auto& id:ids){
        if(!life)return;auto* s=host.session(id);if(!life || !s)continue;
        const auto t=ticket(*s);auto* feature=t.feature();if(!t.current() || !t.workspace())continue;
        const auto gates=s->workerGates;const auto repeat=s->workerRepeat;
        const bool target=feature && host.targetAllowed && host.targetAllowed(*s,feature);
        if(!t.current() || !t.workspace())continue;
        const bool capture=host.captureAllowed && host.captureAllowed(*s);
        if(!t.current() || !t.workspace())continue;
        const bool foreground=feature && host.repeatAllowed && host.repeatAllowed(*s,feature);
        if(!t.current() || !t.workspace())continue;
        if(gates)gates->publish(target,capture);
        if(repeat){repeat->setForegroundAllowed(foreground);if(feature)repeat->updateOptions(WorkerRepeatController::optionsFromFeature(*feature));}
    }
}
void Controller::afterBatch(FeatureRunSession& s){
    const auto t=ticket(s);auto* feature=t.feature();if(!t.current() || !t.workspace())return;
    if(s.workerRepeat){
        const bool allowed=feature && t.host.repeatAllowed && t.host.repeatAllowed(s,feature);
        if(!t.current() || !t.workspace())return;
        s.workerRepeat->setForegroundAllowed(allowed);if(feature)s.workerRepeat->updateOptions(WorkerRepeatController::optionsFromFeature(*feature));
    }
    if(t.current() && t.workspace() && t.host.afterBatch)t.host.afterBatch(s,feature);
}
