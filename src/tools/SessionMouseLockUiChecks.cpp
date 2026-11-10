#include "app/MainWindow.h"
#include "app/SessionMouseLockController.h"
#include "model/Project.h"
#include "model/Feature.h"
#include <QTimer>
#include <iostream>
class SessionMouseLockUiChecks {
public:
    static bool run(){
        std::optional<QPoint> physical;int releases=0;MainWindow window;
        auto* feature=window.m_project->addFeature("mouse interruption");feature->setId("mouse-owner");feature->setUserInputInterruptMode(UserInputInterruptMode::Pause);
        SessionMouseLockController::Ports ports;ports.locks=std::make_shared<MouseLockClaims>(MouseLockClaims::Backend{
            [&](QPoint p){physical=p;},[&]{physical.reset();++releases;}});
        ports.targetRect=[](const auto&)->std::optional<QRect>{return QRect(-500,-400,600,400);};window.m_mouseLocks->m_ports=ports;
        FeatureRunSession first;first.featureId="mouse-owner";first.profileId=window.m_profileManager->activeProfileId();first.runningMode=FeatureRunMode::RepeatInfinite;first.repeatSession=true;
        first.lockMouseDuringFirstLoopCount=2;first.hasRunStartCursorPosition=true;first.sessionContext=std::make_shared<ExecutionContext>();
        FeatureRunSession paused;paused.featureId="mouse-paused";paused.profileId="paused";paused.runningMode=FeatureRunMode::RepeatInfinite;paused.repeatSession=true;
        paused.lockMouseToScreenCenterDuringRun=true;paused.sessionContext=std::make_shared<ExecutionContext>();paused.sessionContext->setPaused(true);
        window.runSessions().emplace(first.featureId,std::move(first));window.runSessions().emplace(paused.featureId,std::move(paused));
        window.reconcileMouseLocksFromRunningSessions();auto* session=window.sessionFor("mouse-owner");auto* timer=window.m_mouseLocks->findChild<QTimer*>();
        if(!session || !timer || physical!=QPoint(0,0) || !session->earlyLoopMouseLockEngaged || !timer->isActive())return false;
        const auto token=session->earlyMouseLockToken;window.reconcileMouseLocksFromRunningSessions();if(token!=session->earlyMouseLockToken)return false;
        session->sessionContext->setRunLoopNumber(3);session->lockMouseToScreenCenterDuringRun=true;window.syncEarlyLoopMouseLock(*session);
        if(physical!=QPoint(-200,-200) || !timer->isActive() || session->earlyLoopMouseLockEngaged)return false;
        window.onUserInputInterrupt("mouse-owner",session->sessionContext);if(physical || timer->isActive())return false;
        window.onUserInputInterrupt("mouse-owner",session->sessionContext);
        if(!physical || !timer->isActive() || !window.prepareForShutdown() || timer->isActive() || physical || !releases)return false;
        std::cout<<"Session mouse lock MainWindow: actual registry/delegates retain owned claims, paused peer exclusion, input Pause/Resume, loop fallback and shutdown release OK (offscreen; recorded point backend; no native clipping/movement/hooks)\n";return true;
    }
};
bool runSessionMouseLockUiChecks(){return SessionMouseLockUiChecks::run();}
