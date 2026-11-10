#include "ui/WindowShellController.h"
#include "app/MainWindow.h"
#include "model/Feature.h"
#include "model/Project.h"
#include "storage/JsonSerializer.h"
#include "core/workflow/blocks/WaitBlock.h"
#include <QApplication>
#include <QElapsedTimer>
#include <QThread>
#include <iostream>

class InputInterruptUiChecks {
public:
    static bool run() {
        MainWindow window;
        const auto original=window.m_profileManager->activeProfileId();
        const auto owner=window.m_profileManager->addProfile("interrupt owner",false);
        const auto other=window.m_profileManager->addProfile("interrupt other",false);
        constexpr auto id="interrupt-collision";
        Project first; auto* ownedFeature=first.addFeature("owner interrupt");ownedFeature->setId(id);
        ownedFeature->setRunMode(FeatureRunMode::Trigger);ownedFeature->setUserInputInterruptMode(UserInputInterruptMode::Pause);
        ownedFeature->setPointerVisualFeedback(false);
        Project second;auto* currentFeature=second.addFeature("active collision");currentFeature->setId(id);
        currentFeature->setUserInputInterruptMode(UserInputInterruptMode::Stop);currentFeature->setPointerVisualFeedback(false);
        if(owner.isEmpty() || other.isEmpty()
            || !JsonSerializer::saveToFile(first,window.m_profileManager->projectPath(owner),window.m_profileManager->projectDirectory(owner))
            || !JsonSerializer::saveToFile(second,window.m_profileManager->projectPath(other),window.m_profileManager->projectDirectory(other)))return false;
        const auto settle=[&] { QElapsedTimer t;t.start();while(window.m_profileSwitchPipelineActive && t.elapsed()<4000){QApplication::processEvents();QThread::msleep(1);}return !window.m_profileSwitchPipelineActive; };
        if(!window.executeProfileSwitch(other,false) || !settle()
            || !window.m_profileManager->updateTriggerArmedFeature(owner,id,true)
            || !window.m_profileManager->updateTriggerArmedFeature(other,id,true))return false;
        FeatureRunSession session;session.featureId=id;session.profileId=owner;
        session.runningMode=FeatureRunMode::Trigger;session.triggerPhase=TriggerSessionPhase::Monitoring;session.repeatSession=true;
        session.holdRepeatGeneration=12;session.sessionContext=std::make_shared<ExecutionContext>();
        session.engine=std::make_unique<WorkflowEngine>();auto workflow=std::make_shared<Workflow>();
        auto wait=std::make_unique<WaitBlock>();wait->ms=10000;workflow->addBlock(std::move(wait));
        session.engine->run(workflow,session.sessionContext);
        const auto context=session.sessionContext;
        window.runSessions().emplace(id,std::move(session));
        auto* active=window.sessionFor(id);
        window.onUserInputInterrupt(id,std::make_shared<ExecutionContext>());
        if(context->isPaused() || context->shouldStop())return false;
        window.onUserInputInterrupt(id,context);
        if(!context->isPaused() || context->shouldStop() || active->userStopRequested)return false;
        window.onUserInputInterrupt(id,context);
        if(context->isPaused())return false;
        auto* ownerModel=window.featureForSession(*active);
        if(!ownerModel || ownerModel==window.m_project->featureById(id))return false;
        ownerModel->setUserInputInterruptMode(UserInputInterruptMode::Stop);
        window.onUserInputInterrupt(id,context);
        if(!context->shouldStop() || !active->userStopRequested || active->repeatSession
            || active->holdRepeatGeneration!=13 || !active->disarmPersistedTrigger
            || window.m_profileManager->loadSettings(owner).triggerArmedFeatureIds.contains(id)
            || !window.m_profileManager->loadSettings(other).triggerArmedFeatureIds.contains(id)
            || !window.m_windowShell->transient().contains("owner interrupt")
            || !active->engine->stopAndWaitBounded(1000))return false;
        // A deleted owner must not be revived from the retained owner-project cache.
        if(!window.m_profileManager->removeProfile(owner))return false;
        active->sessionContext=std::make_shared<ExecutionContext>();active->userStopRequested=false;
        const auto replacement=active->sessionContext;
        window.onUserInputInterrupt(id,replacement);
        const bool retained=!replacement->shouldStop() && !active->userStopRequested
            && window.m_profileManager->loadSettings(other).triggerArmedFeatureIds.contains(id);
        window.runSessions().erase(id);
        if(!retained || !window.executeProfileSwitch(original,false) || !settle()
            || !window.m_profileManager->removeProfile(other) || !window.prepareForShutdown())return false;
        std::cout<<"Input interrupt MainWindow: background owner Pause/Stop versus same-id active feature, stale context rejection, owner-only trigger disarm, owner-name feedback and deleted cached-owner rejection OK (offscreen; real Wait engine; no physical polling/input)\n";
        return true;
    }
};
bool runInputInterruptUiChecks() { return InputInterruptUiChecks::run(); }
