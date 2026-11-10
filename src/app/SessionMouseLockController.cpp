#include "app/SessionMouseLockController.h"
#include "app/RunSessionPolicySnapshot.h"
#include "app/MouseCenterLock.h"
#include "core/input/InputSimulator.h"
#include "core/capture/ScreenCapture.h"
#include "core/capture/TargetWindowBinding.h"
#include <QTimer>
#include <QPointer>
#include <limits>
namespace {
SessionMouseLockController::Ports nativePorts(){
    SessionMouseLockController::Ports ports;
    if(qEnvironmentVariableIntValue("PIPBONG_SELF_TEST_UI")==1 || qEnvironmentVariableIntValue("PIPBONG_SMOKE_TEST")==1
        || qEnvironmentVariableIntValue("PIPBONG_WORKFLOW_DRY_RUN")==1)return ports;
#ifdef _WIN32
    ports.cursor=[]()->std::optional<QPoint>{int x=0,y=0;if(InputSimulator::getCursorScreenPosition(x,y))return QPoint(x,y);return {};};
    ports.targetRect=[](const std::shared_ptr<ExecutionContext>& context)->std::optional<QRect>{
        if(!context)return {};const auto binding=context->targetWindowBinding();ScreenCapture::TargetWindowInfo info;
        if(!ScreenCapture::queryWindowInfo(context->targetWindow(),info) || !info.visible || info.minimized || info.width<=0 || info.height<=0
            || !targetWindowBindingMatches(binding,info.title,info.processPath) || binding!=context->targetWindowBinding())return {};
        return QRect(info.x,info.y,info.width,info.height);
    };
    ports.locks=MouseCenterLock::claims();
#endif
    return ports;
}
bool same(const std::weak_ptr<ExecutionContext>& a,const std::shared_ptr<ExecutionContext>& b){return !a.owner_before(b) && !b.owner_before(a);}
}
SessionMouseLockController::SessionMouseLockController(Sessions& sessions,QObject* parent):SessionMouseLockController(sessions,nativePorts(),parent){}
SessionMouseLockController::SessionMouseLockController(Sessions& sessions,Ports ports,QObject* parent)
    :QObject(parent),m_sessions(sessions),m_ports(std::move(ports)),m_timer(new QTimer(this)){
    m_timer->setInterval(100);connect(m_timer,&QTimer::timeout,this,&SessionMouseLockController::tick);
}
SessionMouseLockController::~SessionMouseLockController(){shutdown();m_lifetime.reset();}
bool SessionMouseLockController::hasFeatureLock(const FeatureRunSession& s){return s.lockMouseToCurrentPositionDuringRun || s.lockMouseToScreenCenterDuringRun;}
bool SessionMouseLockController::earlyWindow(const FeatureRunSession& s){return SessionRunPolicy::isEarlyLoopMouseLockWindow(runSessionPolicyInput(s));}
bool SessionMouseLockController::eligible(const FeatureRunSession& s){return !s.userStopRequested && !s.awaitingFirstTemplateRoi && !s.triggerReleasedOwnMouseLockForPreempt
    && s.sessionContext && !s.sessionContext->shouldStop() && !s.sessionContext->isPaused();}
MouseLockClaims::Owner SessionMouseLockController::owner(const FeatureRunSession& s)const{
    return {m_lifetime,s.sessionContext,s.profileId,QString::fromStdString(s.featureId),s.sessionContext?s.sessionContext->targetWindowBinding():std::pair<std::wstring,std::wstring>(),s.mouseLockGeneration};
}
void SessionMouseLockController::retain(const FeatureRunSession* extra){
    if(!m_ports.locks)return;std::vector<MouseLockClaims::Owner> known;
    for(const auto& pair:m_sessions)if(pair.second.sessionContext)known.push_back(owner(pair.second));
    if(extra && extra->sessionContext)known.push_back(owner(*extra));m_ports.locks->retain(m_lifetime,known);
}
MouseLockClaims::Provider SessionMouseLockController::pointProvider(const MouseLockClaims::Owner& key,std::optional<QPoint> fixed,bool center,QPoint offset)const{
    const auto rect=m_ports.targetRect;return [key,fixed,center,offset,rect]()->std::optional<QPoint>{
        const auto raw=key.context.lock();if(!key.live() || !raw)return {};
        const auto context=std::static_pointer_cast<ExecutionContext>(raw);
        if(context->shouldStop() || context->isPaused() || context->targetWindowBinding()!=key.binding)return {};
        if(fixed)return fixed;if(!rect)return {};const auto frame=rect(context);
        if(!key.live() || context->shouldStop() || context->isPaused() || context->targetWindowBinding()!=key.binding || !frame || !frame->isValid())return {};
        const qint64 x=qint64(frame->x())+(center?frame->width()/2:offset.x()),y=qint64(frame->y())+(center?frame->height()/2:offset.y());
        if(x<std::numeric_limits<int>::min() || x>=std::numeric_limits<int>::max() || y<std::numeric_limits<int>::min() || y>=std::numeric_limits<int>::max())return {};
        return QPoint(int(x),int(y));
    };
}
void SessionMouseLockController::capturePosition(FeatureRunSession& s){
    if(m_stopped)return;
    const auto capture=[&]{++s.mouseLockGeneration;s.featureMouseLockToken=0;s.earlyMouseLockToken=0;s.earlyLoopMouseLockEngaged=false;retain(&s);capturePositionImpl(s);};
    if(m_ports.locks){auto locks=m_ports.locks;auto batch=locks->batch();capture();}else capture();
}
void SessionMouseLockController::capturePositionImpl(FeatureRunSession& s){
    if(m_stopped)return;s.hasMouseLockPosition=false;s.mouseLockAnchoredToTargetWindow=false;s.mouseLockCapturedContext=s.sessionContext;
    s.mouseLockCapturedBinding=s.sessionContext?s.sessionContext->targetWindowBinding():std::pair<std::wstring,std::wstring>();
    if(!s.lockMouseToCurrentPositionDuringRun || !s.sessionContext || !m_ports.cursor)return;
    const auto point=m_ports.cursor();if(!point)return;
    const auto target=m_ports.targetRect?m_ports.targetRect(s.sessionContext):std::nullopt;
    if(s.sessionContext->targetWindowBinding()!=s.mouseLockCapturedBinding)return;
    if(target && target->isValid()){
        const qint64 dx=qint64(point->x())-target->x(),dy=qint64(point->y())-target->y();
        if(dx>=std::numeric_limits<int>::min() && dx<=std::numeric_limits<int>::max() && dy>=std::numeric_limits<int>::min() && dy<=std::numeric_limits<int>::max()){
            s.mouseLockWindowOffsetX=int(dx);s.mouseLockWindowOffsetY=int(dy);s.mouseLockAnchoredToTargetWindow=true;}}
    s.mouseLockScreenX=point->x();s.mouseLockScreenY=point->y();s.hasMouseLockPosition=true;
}
bool SessionMouseLockController::engageEarly(FeatureRunSession& s){
    if(m_stopped || !m_ports.locks)return false;
    if(!eligible(s)){releaseSession(s);return false;}
    auto locks=m_ports.locks;QPointer<SessionMouseLockController> life(this);bool acquired=false;
    {auto batch=locks->batch();retain(&s);std::optional<QPoint> point;
        if(s.sessionContext->hasLastMatchScreenPoint()){const auto p=s.sessionContext->lastMatchScreenPoint();point=QPoint(p.x,p.y);}
        else if(s.hasRunStartCursorPosition)point=QPoint(s.runStartCursorScreenX,s.runStartCursorScreenY);
        else if(m_ports.cursor)point=m_ports.cursor();
        if(point){s.earlyMouseLockToken=locks->upsert(s.earlyMouseLockToken,owner(s),MouseLockClaims::Kind::Early,pointProvider(owner(s),point,false,{}));
            acquired=s.earlyMouseLockToken!=0;s.earlyLoopMouseLockEngaged=acquired;}}
    if(life)life->schedule();return acquired;
}
void SessionMouseLockController::updateFromMatch(FeatureRunSession& s){if(!m_stopped && earlyWindow(s))engageEarly(s);}
void SessionMouseLockController::releaseEarly(FeatureRunSession& s){
    if(!m_ports.locks){s.earlyLoopMouseLockEngaged=false;s.earlyMouseLockToken=0;return;}
    auto locks=m_ports.locks;auto batch=locks->batch();const auto token=s.earlyMouseLockToken;s.earlyMouseLockToken=0;s.earlyLoopMouseLockEngaged=false;locks->release(token);
}
void SessionMouseLockController::engageFeature(FeatureRunSession& s){
    if(m_stopped || !m_ports.locks)return;
    if(!eligible(s)){releaseSession(s);return;}
    auto locks=m_ports.locks;QPointer<SessionMouseLockController> life(this);
    {auto batch=locks->batch();retain(&s);const auto key=owner(s);MouseLockClaims::Provider provider;
        if(s.lockMouseToCurrentPositionDuringRun){
            if(!s.hasMouseLockPosition)capturePositionImpl(s);
            if(s.hasMouseLockPosition && same(s.mouseLockCapturedContext,s.sessionContext) && s.mouseLockCapturedBinding==key.binding)
                provider=pointProvider(key,s.mouseLockAnchoredToTargetWindow?std::nullopt:std::optional<QPoint>(QPoint(s.mouseLockScreenX,s.mouseLockScreenY)),false,QPoint(s.mouseLockWindowOffsetX,s.mouseLockWindowOffsetY));
        }else if(s.lockMouseToScreenCenterDuringRun)provider=pointProvider(key,{},true,{});
        if(provider)s.featureMouseLockToken=locks->upsert(s.featureMouseLockToken,key,MouseLockClaims::Kind::Feature,std::move(provider));
        else{locks->release(s.featureMouseLockToken);s.featureMouseLockToken=0;}}
    if(life)life->schedule();
}
void SessionMouseLockController::syncEarly(FeatureRunSession& s){
    if(m_stopped || !m_ports.locks)return;auto locks=m_ports.locks;QPointer<SessionMouseLockController> life(this);
    {auto batch=locks->batch();engageFeature(s);if(earlyWindow(s) && eligible(s))engageEarly(s);else releaseEarly(s);}
    if(life)life->schedule();
}
void SessionMouseLockController::blockFailed(FeatureRunSession& s){
    if(m_stopped || !earlyWindow(s))return;if(++s.earlyLoopMouseLockFailureCount<s.unlockMouseOnBlockFailureCount)return;
    s.earlyLoopMouseLockReleased=true;auto locks=m_ports.locks;const auto failed=m_failed;
    if(locks){auto batch=locks->batch();releaseEarly(s);engageFeature(s);if(failed)failed(s);}
    else{ s.earlyLoopMouseLockEngaged=false;if(failed)failed(s); }
}
void SessionMouseLockController::reconcile(){
    if(m_stopped || !m_ports.locks)return;auto locks=m_ports.locks;QPointer<SessionMouseLockController> life(this);
    {auto batch=locks->batch();retain();
        for(auto& pair:m_sessions){auto& s=pair.second;
            if(!SessionRunPolicy::isSessionActive(runSessionPolicyInput(s)) || !eligible(s)){
                releaseSession(s);continue;}
            syncEarly(s);}}
    if(life)life->schedule();
}
void SessionMouseLockController::releaseSession(FeatureRunSession& s){
    auto locks=m_ports.locks;if(!locks)return;auto batch=locks->batch();
    const auto token=s.featureMouseLockToken;s.featureMouseLockToken=0;locks->release(token);releaseEarly(s);
}
void SessionMouseLockController::schedule(){
    if(m_stopped || !m_ports.locks || !m_ports.locks->hasClaims(m_lifetime)){m_timer->stop();return;}
    if(!m_timer->isActive())m_timer->start();
}
void SessionMouseLockController::tick(){reconcile();}
void SessionMouseLockController::shutdown(){
    m_stopped=true;m_timer->stop();auto locks=m_ports.locks;
    for(auto& pair:m_sessions){pair.second.earlyLoopMouseLockEngaged=false;pair.second.earlyMouseLockToken=0;pair.second.featureMouseLockToken=0;}
    if(locks)locks->releaseController(m_lifetime);
}
