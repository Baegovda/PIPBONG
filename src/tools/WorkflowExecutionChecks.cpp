#include "app/MainWindow.h"
#include "app/RunLifecycleCoordinator.h"
#include "app/ForegroundWindowMonitor.h"
#include "app/TriggerMonitorController.h"
#include "ui/WorkflowExecutionPresenter.h"
#include "ui/WorkflowEditorPanel.h"
#include "ui/FeatureListPanel.h"
#include "ui/LogPanelWidget.h"
#include "model/Project.h"
#include "model/Feature.h"
#include "core/workflow/blocks/ImageFindBlock.h"
#include "core/workflow/blocks/WaitBlock.h"
#include <QApplication>
#include <QDir>
#include <QElapsedTimer>
#include <QEvent>
#include <QFile>
#include <QImage>
#include <QTextEdit>
#include <QThread>
#include <iostream>

namespace {
bool check(bool value,const char* stage){if(!value)std::cerr<<"Workflow events FAILED: "<<stage<<'\n';return value;}
QByteArray bytes(const QString& path){QFile file(path);return file.open(QIODevice::ReadOnly)?file.readAll():QByteArray();}
}
class WorkflowExecutionUiChecks {
public:
    static bool run() {
        using H=WorkflowExecutionHighlight;using P=WorkflowExecutionPresenter;
        MainWindow w;w.m_foregroundMonitor->stop();w.m_triggerMonitor->stop();
        const auto samplePath=QDir(w.m_profileManager->projectDirectory(w.m_profileManager->activeProfileId())).filePath("workflow-events-fixture.png");
        QImage sample(16,16,QImage::Format_RGB32);sample.fill(QColor(110,180,150));if(!sample.save(samplePath))return false;
        auto* feature=w.m_project->addFeature("매칭 진행 표시 검사");feature->setId("workflow-events");feature->setPointerVisualFeedback(true);
        auto image=std::make_unique<ImageFindBlock>();image->templatePaths={"workflow-events-fixture.png"};
        ClickPointerFeedbackSettings custom;custom.displayDurationMs=333;image->matchPointerFeedback=custom;
        feature->workflow().addBlock(std::move(image));auto wait=std::make_unique<WaitBlock>();wait->ms=250;feature->workflow().addBlock(std::move(wait));
        w.m_featureList->setProject(w.m_project.get());w.m_featureList->selectFeatureById("workflow-events");
        auto prepared=RunLifecycleCoordinator::prepareNewSession(w,*feature,w.m_profileManager->activeProfileId(),false,true);
        prepared.session.repeatSession=false;prepared.session.sessionContext=std::make_shared<ExecutionContext>();
        prepared.session.sessionWorkflow=std::shared_ptr<Workflow>(feature->workflow().clone());
        auto* engine=prepared.session.engine.get();w.runSessions().emplace(feature->id(),std::move(prepared.session));
        auto* session=w.sessionFor(feature->id());auto* panel=w.m_workflowEditor;
        const auto owner=session->profileId,projectPath=w.m_projectFilePath;const auto before=bytes(projectPath);const bool modified=w.m_modified;
        int refresh=0,matched=0,failed=0;std::vector<P::Pulse> pulses;std::vector<QString> logs;
        auto host=w.m_executionPresenter->m_host;const auto originalLog=host.log;
        host.refresh=[&]{++refresh;};host.matched=[&](auto&){++matched;};host.failed=[&](auto&,int){++failed;};
        host.log=[&](const auto& s,const QString& message,LogLineKind kind){logs.push_back(message);originalLog(s,message,kind);};
        w.m_executionPresenter->setHost(host);w.m_executionPresenter->m_ports.pulse=[&](const auto& pulse){pulses.push_back(pulse);};
        std::vector<std::shared_ptr<ExecutionContext>> frameContexts;
        w.m_executionPresenter->m_ports.frame=[&](const auto& ctx)->std::optional<PointerFeedbackFrame>{
            frameContexts.push_back(ctx);return PointerFeedbackFrame{99,88,QRect(-1500,-900,1280,720)};};
        const auto drain=[&]{QCoreApplication::sendPostedEvents(&w,QEvent::MetaCall);};
        const auto workflow=session->sessionWorkflow;const auto context=session->sessionContext;
        if(!check(!before.isEmpty(),"existing project bytes available for read-only check"))return false;
        engine->sessionPrepared(std::make_shared<Workflow>(),std::make_shared<ExecutionContext>());drain();
        if(!check(session->sessionWorkflow==workflow && session->sessionContext==context,"prepared payload context mismatch rejected"))return false;
        auto accepted=std::shared_ptr<Workflow>(feature->workflow().clone());engine->sessionPrepared(accepted,context);engine->started();engine->blockStarted(0,"matching");drain();
        if(!check(session->sessionWorkflow==accepted && session->loopTimer.isValid() && refresh==1 && panel->m_activeBlockIndex==0 && session->runningBlockHighlight==H::Running,"prepared/start/block production wiring"))return false;
        engine->blockImageFindAttempt(0,3,0.85,0.2,false);engine->imageFindFailureHandling(0,2,4);drain();
        if(!check(panel->m_rowImageFindAttemptCounts.value(0)==3 && panel->m_rowImageFindReturnCounts.value(0)==2 && panel->m_rowImageFindRetryCounts.value(0)==4,"attempt/failure metrics"))return false;
        QPixmap preview(16,16);preview.fill(QColor(110,180,150));
        engine->blockMatchResult(0,0.85,0.97,preview,true,true,12,34);engine->blockProgress(0,BlockProgressKind::ImageFindSuccess);drain();
        if(!check(pulses.size()==1 && pulses.back().profileId==owner && pulses.back().featureId==feature->id()
            && pulses.back().point==QPoint(12,34) && pulses.back().kind==RunPointerFeedbackKind::MatchSuccess
            && pulses.back().settings && pulses.back().settings->displayDurationMs==333
            && pulses.back().context==context && pulses.back().frame && pulses.back().frame->clientBounds==QRect(-1500,-900,1280,720)
            && !frameContexts.empty() && frameContexts.back()==context
            && matched==1 && panel->isBlockMatchSuccessCommitted(0),"match owner/custom feedback and committed success"))return false;
        engine->blockProgress(0,BlockProgressKind::ImageFindMiss);engine->blockImageFindAttempt(0,4,0.85,0.1,false);drain();
        if(!check(panel->m_rowMatchConfidences.value(0)==0.97 && session->runningBlockHighlight==H::Success,"late miss cannot erase committed success"))return false;
        engine->blockFinished(0,true,"matched",1234,123,8);engine->blockStarted(1,"wait");engine->blockFinished(1,false,"failed",456,-1,0);engine->imageFindReturnToPrevious(1,0);drain();
        if(!check(panel->m_rowBlockDurations.value(0)==1234 && panel->m_rowImageFindMatchDurations.value(0)==123
            && panel->m_rowImageFindAttemptCounts.value(0)==8 && panel->m_rowBlockDurations.value(1)==456
            && failed==1 && session->runningBlockHighlight==H::Failed && panel->m_activeBlockIndex==1
            && panel->m_blockList->m_flashKind==H::ReturnToPrevious && panel->m_blockList->m_flashRow==1
            && panel->m_blockList->m_flashRowSecondary==0,"finished timing/failure/dual-row return flash presentation"))return false;
        engine->pointerFeedbackAtClientPoint(4,5);drain();
        if(!check(pulses.size()==2 && pulses.back().kind==RunPointerFeedbackKind::Click && pulses.back().settings.has_value(),"click feedback port"))return false;
        context->setSuppressRepeatUi(true);engine->pointerFeedbackAtClientPoint(1,1);engine->blockMatchResult(0,0.8,0.3,preview,false,true,1,1);engine->started();drain();
        if(!check(pulses.size()==2 && refresh==1,"fast-repeat feedback suppression"))return false;
        context->setSuppressRepeatUi(false);session->runningMode=FeatureRunMode::Trigger;session->triggerPhase=TriggerSessionPhase::Monitoring;session->triggerBlockIndex=0;session->loopTimer.invalidate();
        engine->started();engine->blockStarted(0,"watch");engine->blockProgress(0,BlockProgressKind::ImageFindMiss);engine->blockMatchResult(0,0.85,0.4,preview,false,true,6,7);drain();
        if(!check(!session->loopTimer.isValid() && refresh==2 && session->runningBlockHighlight==H::TriggerWatch
            && pulses.back().kind==RunPointerFeedbackKind::TriggerScan && !pulses.back().settings,"trigger watch and default scan feedback"))return false;
        session->triggerPhase=TriggerSessionPhase::Cooldown;w.applyRunningBlockVisuals(*session,0,H::Running);
        if(!check(session->runningBlockHighlight==H::TriggerCooldown,"cooldown highlight mapping"))return false;
        session->runningMode=FeatureRunMode::RepeatCount;session->triggerPhase=TriggerSessionPhase::None;
        session->profileId="background-owner";const auto activeRow=panel->m_activeBlockIndex;const auto scores=panel->m_rowMatchConfidences;
        engine->blockStarted(1,"background");engine->blockMatchResult(0,0.85,0.1,preview,false,true,8,9);drain();
        if(!check(panel->m_activeBlockIndex==activeRow && panel->m_rowMatchConfidences==scores && pulses.back().profileId=="background-owner","background session cannot overwrite selected view; pulse retains owner tag"))return false;
        session->profileId=owner;
        auto retired=RunLifecycleCoordinator::prepareNewSession(w,*feature,owner,false,true);auto* old=retired.session.engine.get();
        w.m_abandonedEngineFeatureIds[old]=feature->id();w.m_abandonedEngines.push_back(std::move(retired.session.engine));
        const auto logsBefore=logs.size(),pulsesBefore=pulses.size();const int indexBefore=session->runningBlockIndex;
        old->logMessage("retired-ghost-marker");old->sessionPrepared(std::make_shared<Workflow>(),context);old->started();old->blockStarted(7,"retired");
        old->blockProgress(0,BlockProgressKind::ImageFindSuccess);old->blockMatchResult(0,0.8,0.2,preview,false,true,1,2);old->pointerFeedbackAtClientPoint(3,4);
        old->blockFinished(0,false,"retired",999,999,999);old->blockImageFindAttempt(0,99,0.5,0.1,false);old->imageFindFailureHandling(0,99,99);old->imageFindReturnToPrevious(1,0);drain();
        if(!check(logs.size()==logsBefore && pulses.size()==pulsesBefore && session->runningBlockIndex==indexBefore
            && session->sessionWorkflow==accepted && panel->m_rowImageFindAttemptCounts.value(0)==8,"all retired engine events isolated"))return false;
        // A feedback callback may replace the current context before its match reaches the view.
        const auto oldContext=session->sessionContext;const auto oldScores=panel->m_rowMatchConfidences;
        const auto savedFrame=w.m_executionPresenter->m_ports.frame;const auto beforeFrameReplacement=pulses.size();
        w.m_executionPresenter->m_ports.frame=[&](const auto&)->std::optional<PointerFeedbackFrame>{
            session->sessionContext=std::make_shared<ExecutionContext>();return PointerFeedbackFrame{77,66,QRect(1,2,800,600)};};
        engine->blockMatchResult(0,0.7,0.6,preview,true,true,1,2);drain();
        if(!check(pulses.size()==beforeFrameReplacement && panel->m_rowMatchConfidences==oldScores,"context replacement during frame query blocks stale pulse and view"))return false;
        session->sessionContext=oldContext;w.m_executionPresenter->m_ports.frame=savedFrame;
        w.m_executionPresenter->m_ports.pulse=[&](const auto&){session->sessionContext=std::make_shared<ExecutionContext>();};
        engine->blockMatchResult(0,0.7,0.6,preview,true,true,1,2);drain();
        if(!check(panel->m_rowMatchConfidences==oldScores,"context replacement during feedback blocks later view mutation"))return false;
        session->sessionContext=oldContext;w.m_executionPresenter->m_ports.pulse=[&](const auto& p){pulses.push_back(p);};
        engine->logMessage("current-valid-marker");drain();
        auto* logView=w.m_logPanel->findChild<QTextEdit*>();
        QElapsedTimer timer;timer.start();while(logView && !logView->toPlainText().contains("current-valid-marker") && timer.elapsed()<2000){QApplication::processEvents();QThread::msleep(1);}
        if(!check(logView && logView->toPlainText().contains("current-valid-marker") && !logView->toPlainText().contains("retired-ghost-marker"),"actual log view has current events only"))return false;
        if(!check(w.m_modified==modified && bytes(projectPath)==before,"presentation leaves document persistence unchanged"))return false;
        w.grab().save(QDir(qEnvironmentVariable("PIPBONG_DATA_DIR")).filePath("workflow-events-state.png"));
        w.stopAllSessions();if(!w.prepareForShutdown())return false;
        // Standalone presenter deletion during a notification must not touch the view afterward.
        WorkflowEditorPanel standalone;QObject source;FeatureRunSession dummy;dummy.featureId="lifetime";
        WorkflowExecutionPresenter* dying=new P(standalone,P::Ports{});P::Host destructive;
        destructive.session=[&](QObject* value){return value==&source?&dummy:nullptr;};destructive.details=[](const auto&){return true;};
        destructive.log=[&](const auto&,const auto&,auto){delete dying;dying=nullptr;};dying->setHost(destructive);dying->blockStarted(&source,3,"delete");
        if(!check(!dying && dummy.runningBlockIndex==-1,"presenter lifetime during log callback"))return false;
        dummy.sessionContext=std::make_shared<ExecutionContext>();int latePulse=0;P::Ports ports;
        ports.pulse=[&](const auto&){++latePulse;};ports.frame=[&](const auto&)->std::optional<PointerFeedbackFrame>{delete dying;dying=nullptr;return PointerFeedbackFrame{1,2,QRect(0,0,50,50)};};
        dying=new P(standalone,std::move(ports));dying->setHost(destructive);dying->pointer(&source,1,2);
        if(!check(!dying && latePulse==0,"presenter deletion during frame query blocks late pulse"))return false;
        std::cout<<"Workflow events MainWindow: actual queued prepared/progress/match/timing/log connections, committed success, trigger mapping, background/retired isolation, callback context/lifetime and unchanged project bytes OK (offscreen; recorded feedback; no native overlay/capture/input)\n";
        return true;
    }
};
bool runWorkflowExecutionChecks(){return WorkflowExecutionUiChecks::run();}
