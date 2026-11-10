#include "ui/WorkflowExecutionPresenter.h"
#include "ui/WorkflowEditorPanel.h"
#include "ui/LogPanelWidget.h"
#include "app/FeatureRunSession.h"
#include "core/workflow/blocks/ImageFindBlock.h"
#include <QCoreApplication>

namespace {
bool monitoring(const FeatureRunSession& s){return s.runningMode==FeatureRunMode::Trigger && s.triggerPhase==TriggerSessionPhase::Monitoring;}
QString text(const char* value){return QCoreApplication::translate("MainWindow",value);}
WorkflowExecutionPresenter::Ports nativePorts() {
    if(qEnvironmentVariableIntValue("PIPBONG_SELF_TEST_UI")==1 || qEnvironmentVariableIntValue("PIPBONG_SMOKE_TEST")==1)return {};
    return {[](const WorkflowExecutionPresenter::Pulse& p){
        if(p.context && p.frame)WorkflowMatchFeedbackOverlay::pulseAtFrame(
            {p.profileId,p.featureId,p.context},*p.frame,p.point.x(),p.point.y(),p.kind,p.settings);
    },[](const std::shared_ptr<ExecutionContext>& context)->std::optional<PointerFeedbackFrame>{
#ifdef _WIN32
        if(!context)return {};
        const auto binding=context->targetWindowBinding();
        const auto hwnd=context->targetWindow();ScreenCapture::TargetWindowInfo info;
        if(!ScreenCapture::queryWindowInfo(hwnd,info) || !pointerFeedbackBindingMatches(binding,info.title,info.processPath))return {};
        const auto frame=WorkflowMatchFeedbackOverlay::frameForWindow(reinterpret_cast<quintptr>(hwnd));
        if(binding==context->targetWindowBinding())return frame;
#endif
        return {};
    }};
}
}
struct WorkflowExecutionPresenter::Ticket {
    QPointer<WorkflowExecutionPresenter> life;
    QPointer<WorkflowEditorPanel> editor;
    QPointer<QObject> source;
    Host host;Ports ports;
    FeatureRunSession* original=nullptr;
    std::shared_ptr<ExecutionContext> context;
    QString owner;
    FeatureRunSession* valid() const {
        if(!life || !editor || !source || !original || !host.session)return nullptr;
        auto* now=host.session(source);
        return now==original && now->sessionContext==context && now->profileId==owner?now:nullptr;
    }
    bool displayed() const {const auto* s=valid();return s && host.displayed && host.displayed(*s);}
};
WorkflowExecutionPresenter::WorkflowExecutionPresenter(WorkflowEditorPanel& editor,QObject* parent)
    :WorkflowExecutionPresenter(editor,nativePorts(),parent){}
WorkflowExecutionPresenter::WorkflowExecutionPresenter(WorkflowEditorPanel& editor,Ports ports,QObject* parent)
    :QObject(parent),m_editor(&editor),m_ports(std::move(ports)){}
WorkflowExecutionPresenter::Ticket WorkflowExecutionPresenter::ticket(QObject* source) const {
    auto* s=m_host.session?m_host.session(source):nullptr;
    return {const_cast<WorkflowExecutionPresenter*>(this),m_editor,source,m_host,m_ports,s,
        s?s->sessionContext:nullptr,s?s->profileId:QString()};
}
WorkflowExecutionHighlight WorkflowExecutionPresenter::mapHighlight(const FeatureRunSession& s,int index,WorkflowExecutionHighlight value) {
    using H=WorkflowExecutionHighlight;
    if(s.runningMode==FeatureRunMode::Trigger && index==s.triggerBlockIndex && (value==H::Running || value==H::ImageFindMiss)) {
        if(s.triggerPhase==TriggerSessionPhase::Monitoring)return H::TriggerWatch;
        if(s.triggerPhase==TriggerSessionPhase::Cooldown)return H::TriggerCooldown;
    }
    return value;
}
void WorkflowExecutionPresenter::highlight(FeatureRunSession& s,int index,WorkflowExecutionHighlight value) {
    const Host host=m_host;const auto editor=m_editor;const QPointer<WorkflowExecutionPresenter> life(this);
    const auto mapped=mapHighlight(s,index,value);s.runningBlockIndex=index;s.runningBlockHighlight=mapped;
    if(host.displayed && host.displayed(s) && life && editor)editor->setActiveBlockIndex(index,mapped);
}
void WorkflowExecutionPresenter::show(Ticket& t,int index,WorkflowExecutionHighlight value) {
    auto* s=t.valid();if(!s)return;
    const auto mapped=mapHighlight(*s,index,value);s->runningBlockIndex=index;s->runningBlockHighlight=mapped;
    if(t.displayed() && t.valid())t.editor->setActiveBlockIndex(index,mapped);
}
void WorkflowExecutionPresenter::pulse(Ticket& t,Pulse p) {
    if(!t.valid() || !t.ports.pulse)return;
    p.context=t.context;
    if(t.ports.frame)p.frame=t.ports.frame(p.context);
    if(t.valid())t.ports.pulse(p);
}
void WorkflowExecutionPresenter::log(QObject* source,const QString& message) {
    auto t=ticket(source);const auto* s=t.valid();
    if(s && t.host.log)t.host.log(*s,message,logKindForWorkflowMessage(message));
}
void WorkflowExecutionPresenter::prepared(QObject* source,std::shared_ptr<Workflow> workflow,std::shared_ptr<ExecutionContext> context) {
    auto t=ticket(source);auto* s=t.valid();if(!s)return;
    if(context && s->sessionContext && context!=s->sessionContext)return;
    if(workflow)s->sessionWorkflow=std::move(workflow);
    if(context && !s->sessionContext)s->sessionContext=std::move(context);
}
void WorkflowExecutionPresenter::started(QObject* source) {
    auto t=ticket(source);auto* s=t.valid();if(!s)return;
    if(s->runningMode==FeatureRunMode::Trigger && s->triggerPhase!=TriggerSessionPhase::RunningAction) {
        if(t.host.refresh)t.host.refresh();return;
    }
    s->loopTimer.start();
    if(!(s->sessionContext && s->sessionContext->suppressRepeatUi()) && t.host.refresh)t.host.refresh();
}
void WorkflowExecutionPresenter::blockStarted(QObject* source,int index,const QString& summary) {
    auto t=ticket(source);auto* s=t.valid();if(!s)return;
    s->runningBlockSummary=summary;
    s->runningBlockSummaryIndex=index;
    if(t.host.details && t.host.details(*s) && t.host.log)t.host.log(*s,text("단계 %1 · %2").arg(index+1).arg(summary),LogLineKind::Info);
    show(t,index,WorkflowExecutionHighlight::Running);
}
void WorkflowExecutionPresenter::progress(QObject* source,int index,BlockProgressKind kind) {
    auto t=ticket(source);auto* s=t.valid();if(!s)return;
    if(kind==BlockProgressKind::ImageFindMiss) {
        if(monitoring(*s) || (t.displayed() && t.editor->isBlockMatchSuccessCommitted(index)))return;
        s->runningBlockIndex=index;s->runningBlockHighlight=WorkflowExecutionHighlight::ImageFindMiss;
        if(t.displayed() && t.valid())t.editor->notifyImageFindRetry(index);
    } else if(kind==BlockProgressKind::ImageFindSuccess) {
        show(t,index,WorkflowExecutionHighlight::Success);
        if(t.displayed() && t.valid())t.editor->markBlockMatchSuccess(index);
        if(auto* current=t.valid();current && t.host.matched)t.host.matched(*current);
    }
}
void WorkflowExecutionPresenter::match(QObject* source,int index,double threshold,double confidence,const QPixmap& preview,
    bool matched,bool hasPoint,int x,int y) {
    auto t=ticket(source);auto* s=t.valid();
    if(!s || (s->sessionContext && s->sessionContext->suppressRepeatUi()))return;
    if(hasPoint && s->pointerVisualFeedback && t.ports.pulse) {
        Pulse p{s->profileId,s->featureId,QPoint(x,y),matched?RunPointerFeedbackKind::MatchSuccess:
            (monitoring(*s)?RunPointerFeedbackKind::TriggerScan:RunPointerFeedbackKind::MatchMiss),std::nullopt};
        if(p.kind!=RunPointerFeedbackKind::TriggerScan && s->sessionWorkflow && index>=0
            && index<static_cast<int>(s->sessionWorkflow->blocks().size())) {
            if(const auto* image=dynamic_cast<ImageFindBlock*>(s->sessionWorkflow->blocks()[index].get());image && image->hasCustomMatchPointerFeedback())p.settings=image->customMatchPointerFeedback();
        }
        pulse(t,std::move(p));
    }
    if(t.displayed() && t.valid())t.editor->setBlockMatchResult(index,threshold,confidence,preview,matched,hasPoint,x,y);
}
void WorkflowExecutionPresenter::pointer(QObject* source,int x,int y) {
    auto t=ticket(source);const auto* s=t.valid();
    if(!s || !s->pointerVisualFeedback || (s->sessionContext && s->sessionContext->suppressRepeatUi()) || !t.ports.pulse)return;
    pulse(t,{s->profileId,s->featureId,QPoint(x,y),RunPointerFeedbackKind::Click,PointerFeedbackSettings::click()});
}
void WorkflowExecutionPresenter::blockFinished(QObject* source,int index,bool success,const QString& message,
    qint64 duration,qint64 matchDuration,int attempts) {
    auto t=ticket(source);auto* s=t.valid();if(!s)return;
    if((!success || (t.host.details && t.host.details(*s))) && t.host.log)
        t.host.log(*s,text("단계 %1 · %2 · %3").arg(index+1).arg(success?text("성공"):text("실패")).arg(message),success?LogLineKind::Success:LogLineKind::Warning);
    if(!t.valid())return;
    if(t.displayed() && t.valid()) {
        t.editor->setBlockDuration(index,duration);t.editor->setBlockImageFindMatchDuration(index,matchDuration);
        if(attempts>0)t.editor->setBlockImageFindAttemptCount(index,attempts);
    }
    show(t,index,success?WorkflowExecutionHighlight::Success:WorkflowExecutionHighlight::Failed);
    if(!success) {if(auto* current=t.valid();current && t.host.failed)t.host.failed(*current,index);}
    else if(t.displayed() && t.valid())t.editor->markBlockMatchSuccess(index);
}
void WorkflowExecutionPresenter::attempt(QObject* source,int index,int attempts,double threshold,double confidence,bool matched) {
    auto t=ticket(source);if(!t.displayed() || !t.valid())return;
    t.editor->setBlockImageFindAttemptCount(index,attempts);
    if(!matched)t.editor->setBlockMatchResult(index,threshold,confidence,QPixmap(),false);
}
void WorkflowExecutionPresenter::failureHandling(QObject* source,int index,int returns,int retries) {
    auto t=ticket(source);if(t.displayed() && t.valid())t.editor->setBlockImageFindFailureHandlingCounts(index,returns,retries);
}
void WorkflowExecutionPresenter::returnToPrevious(QObject* source,int from,int to) {
    auto t=ticket(source);if(t.displayed() && t.valid())t.editor->notifyImageFindReturnToPrevious(from,to);
}
