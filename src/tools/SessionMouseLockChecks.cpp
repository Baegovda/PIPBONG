#include "app/SessionMouseLockController.h"
#include "app/FeatureRunSession.h"
#include "app/MouseCenterLock.h"
#include <QTimer>
#include <iostream>
#include <limits>
#include <atomic>
#include <thread>
namespace {
using C=SessionMouseLockController;
bool check(bool ok,const char* text){if(!ok)std::cerr<<"Session mouse lock FAILED: "<<text<<'\n';return ok;}
struct Backend {
    std::optional<QPoint> cursor=QPoint(-1500,-800),physical;
    std::optional<QRect> frame=QRect(-1600,-900,800,600);
    std::map<ExecutionContext*,std::optional<QRect>> frames;std::vector<ExecutionContext*> queried;
    int applies=0,releases=0,reads=0;std::function<void()> onApply;
    std::shared_ptr<MouseLockClaims> locks=std::make_shared<MouseLockClaims>(MouseLockClaims::Backend{
        [this](QPoint p){++applies;physical=p;const auto callback=onApply;if(callback)callback();},[this]{++releases;physical.reset();}});
    C::Ports ports(){C::Ports p;p.locks=locks;p.cursor=[this]{++reads;return cursor;};
        p.targetRect=[this](const auto& ctx){queried.push_back(ctx.get());const auto it=frames.find(ctx.get());return it==frames.end()?frame:it->second;};return p;}
};
FeatureRunSession& add(C::Sessions& sessions,const char* id){auto& s=sessions[id];s.featureId=id;s.profileId=id;s.sessionContext=std::make_shared<ExecutionContext>();
    s.repeatSession=true;s.runningMode=FeatureRunMode::RepeatInfinite;s.hasRunStartCursorPosition=true;return s;}
}
bool runSessionMouseLockChecks(){
    {
        C::Sessions sessions;Backend backend;C controller(sessions,backend.ports());auto& older=add(sessions,"older");auto& newer=add(sessions,"newer");
        older.lockMouseDuringFirstLoopCount=newer.lockMouseDuringFirstLoopCount=2;older.runStartCursorScreenX=10;newer.runStartCursorScreenX=20;
        controller.engageEarly(older);const auto token=older.earlyMouseLockToken;controller.engageEarly(newer);
        older.sessionContext->setLastMatchScreenPoint(cv::Point(30,40));controller.updateFromMatch(older);
        if(!check(backend.physical==QPoint(20,0) && older.earlyMouseLockToken==token,"older match must not move newer owner's physical lock"))return false;
        newer.earlyLoopMouseLockReleased=true;controller.releaseEarly(newer);
        if(!check(backend.physical==QPoint(30,40),"releasing newest request restores previous owner's own updated point"))return false;
        newer.repeatSession=false;older.lockMouseToScreenCenterDuringRun=true;older.earlyLoopMouseLockReleased=true;controller.syncEarly(older);
        if(!check(backend.physical==QPoint(-1200,-600),"early-to-feature fallback stays scoped to its owner"))return false;
    }
    {
        C::Sessions sessions;Backend backend;C controller(sessions,backend.ports());auto& s=add(sessions,"current");auto* timer=controller.findChild<QTimer*>();if(!timer)return false;
        s.hasMouseLockPosition=true;controller.capturePosition(s);
        if(!check(!s.hasMouseLockPosition && !backend.reads,"disabled capture clears stale position without cursor read"))return false;
        s.lockMouseToCurrentPositionDuringRun=true;backend.cursor.reset();controller.capturePosition(s);
        if(!check(!s.hasMouseLockPosition,"failed cursor read cannot create a point"))return false;
        backend.cursor=QPoint(-1500,-800);controller.capturePosition(s);s.lockMouseToScreenCenterDuringRun=true;controller.engageFeature(s);
        if(!check(s.mouseLockAnchoredToTargetWindow && backend.physical==*backend.cursor && timer->isActive(),"negative owned-frame offset precedes center"))return false;
        const auto token=s.featureMouseLockToken;for(int i=0;i<30;++i)controller.schedule();controller.tick();
        if(!check(controller.findChildren<QTimer*>().size()==1 && s.featureMouseLockToken==token,"one timer and stable owner claim coalesce repeated refresh"))return false;
        backend.frame.reset();controller.capturePosition(s);controller.engageFeature(s);
        if(!check(!s.mouseLockAnchoredToTargetWindow && backend.physical==*backend.cursor,"missing owned frame falls back only to captured screen point"))return false;
        backend.frame=QRect(std::numeric_limits<int>::min(),0,1,1);backend.cursor=QPoint(std::numeric_limits<int>::max(),10);controller.capturePosition(s);controller.engageFeature(s);
        if(!check(!s.mouseLockAnchoredToTargetWindow && s.hasMouseLockPosition && !backend.physical,"overflow offset and one-pixel clip bounds fail closed"))return false;
        backend.cursor=QPoint(10,20);backend.frame=QRect(0,0,100,100);controller.capturePosition(s);controller.engageFeature(s);
        s.sessionContext->setTargetWindowTitleForWorker(L"changed binding");controller.tick();
        if(!check(!backend.physical,"captured offset cannot silently follow a changed context binding"))return false;
        controller.capturePosition(s);controller.engageFeature(s);if(!check(backend.physical==QPoint(10,20),"explicit new-run capture permits the new owned binding"))return false;
        controller.shutdown();controller.engageFeature(s);if(!check(!backend.physical && !timer->isActive(),"shutdown retires claims/timer and rejects reacquisition"))return false;
    }
    {
        C::Sessions sessions;Backend backend;C controller(sessions,backend.ports());auto& a=add(sessions,"a");auto& b=add(sessions,"b");
        a.lockMouseToScreenCenterDuringRun=b.lockMouseToScreenCenterDuringRun=true;
        backend.frames[a.sessionContext.get()]=QRect(-1000,-800,800,600);backend.frames[b.sessionContext.get()]=QRect(100,100,600,400);
        controller.engageFeature(a);controller.engageFeature(b);backend.frames[a.sessionContext.get()]=QRect(-1200,-900,800,600);controller.tick();
        if(!check(backend.physical==QPoint(400,300) && backend.queried.back()==b.sessionContext.get(),"newest owner reads its own frame rather than another capture target"))return false;
        b.sessionContext->setPaused(true);controller.reconcile();
        if(!check(backend.physical==QPoint(-800,-600),"pause restores previous owner using its moved frame"))return false;
        backend.frames[a.sessionContext.get()].reset();controller.tick();
        if(!check(!backend.physical && controller.findChild<QTimer*>()->isActive(),"lost window releases physical lock while retrying owned request"))return false;
        backend.frames[a.sessionContext.get()]=QRect(-600,-500,200,200);controller.tick();
        if(!check(backend.physical==QPoint(-500,-400),"recovered owned frame resumes without global fallback"))return false;
        b.sessionContext->setPaused(false);controller.reconcile();if(!check(backend.physical==QPoint(400,300),"resume preserves owner priority"))return false;
        a.awaitingFirstTemplateRoi=true;b.triggerReleasedOwnMouseLockForPreempt=true;controller.reconcile();
        if(!check(!backend.physical,"ROI waiting and trigger own-lock suspension cannot hold a request"))return false;
        a.awaitingFirstTemplateRoi=false;controller.reconcile();auto retained=a.sessionContext;
        a.sessionContext=std::make_shared<ExecutionContext>();backend.frames[a.sessionContext.get()].reset();controller.engageFeature(a);
        if(!check(!backend.physical,"same-ID replacement retires old context claim even when old context is still alive"))return false;
    }
    {
        C::Sessions sessions;Backend backend;C controller(sessions,backend.ports());auto& early=add(sessions,"early");
        early.lockMouseDuringFirstLoopCount=2;early.unlockMouseOnBlockFailureCount=2;early.runStartCursorScreenX=20;early.runStartCursorScreenY=30;
        controller.engageEarly(early);const auto token=early.earlyMouseLockToken;early.sessionContext->setLastMatchScreenPoint(cv::Point(50,60));controller.updateFromMatch(early);controller.reconcile();
        if(!check(backend.physical==QPoint(50,60) && early.earlyMouseLockToken==token,"match/reconcile updates existing claim without rebuilding unrelated owners"))return false;
        early.sessionContext->setPaused(true);controller.reconcile();if(!check(!backend.physical && !early.earlyLoopMouseLockEngaged,"pause releases early claim flag"))return false;
        early.sessionContext->setPaused(false);controller.reconcile();early.lockMouseToScreenCenterDuringRun=true;int failures=0;controller.setFailureHandler([&](const auto&){++failures;});
        controller.blockFailed(early);if(!check(backend.physical==QPoint(50,60) && failures==0,"failure threshold retains early request"))return false;
        controller.blockFailed(early);controller.blockFailed(early);
        if(!check(early.earlyLoopMouseLockReleased && !early.earlyLoopMouseLockEngaged && failures==1 && backend.physical==QPoint(-1200,-600),"failure threshold releases once and restores feature request"))return false;
        early.sessionContext->requestStop();controller.reconcile();controller.engageFeature(early);
        if(!check(!backend.physical && !controller.engageEarly(early) && !controller.findChild<QTimer*>()->isActive(),"stopped context cannot reacquire from progress/timer"))return false;
    }
    {
        C::Sessions sessions;Backend backend;auto controller=std::make_unique<C>(sessions,backend.ports());auto& s=add(sessions,"destroyed");s.lockMouseToScreenCenterDuringRun=true;
        backend.onApply=[&]{controller.reset();};auto* raw=controller.get();raw->engageFeature(s);
        if(!check(!controller && !backend.physical,"backend callback controller destruction releases only its deferred claims"))return false;
    }
    {
        Backend backend;auto control=std::make_shared<int>(1),context=std::make_shared<int>(2);
        MouseLockClaims::Owner owner{control,context,"profile","feature",{}};auto book=backend.locks;
        const auto token=book->upsert(0,owner,MouseLockClaims::Kind::Feature,[]{return QPoint(5,6);});
        book->upsert(token,owner,MouseLockClaims::Kind::Feature,[&]()->std::optional<QPoint>{book->release(token);return QPoint(9,9);});
        if(!check(!backend.physical,"provider retirement cannot publish an obsolete snapshot"))return false;
        auto otherControl=std::make_shared<int>(3);auto other=owner;other.controller=otherControl;
        book->upsert(0,owner,MouseLockClaims::Kind::Feature,[]{return QPoint(1,2);});book->upsert(0,other,MouseLockClaims::Kind::Feature,[]{return QPoint(3,4);});
        book->releaseController(control);if(!check(backend.physical==QPoint(3,4),"controller shutdown cannot release another controller's winner"))return false;
        otherControl.reset();book->refresh();if(!check(!backend.physical,"expired controller restores pre-lock backend state"))return false;
    }
    {
        Backend backend;auto control=std::make_shared<int>(1);std::vector<std::shared_ptr<int>> contexts;const int before=backend.applies;
        {auto batch=backend.locks->batch();for(int i=0;i<32;++i){contexts.push_back(std::make_shared<int>(i));MouseLockClaims::Owner owner{control,contexts.back(),"p",QString::number(i),{}};
            auto token=backend.locks->upsert(0,owner,MouseLockClaims::Kind::Feature,[i]{return QPoint(i,i);});
            for(int j=0;j<8;++j)backend.locks->upsert(token,owner,MouseLockClaims::Kind::Feature,[i]{return QPoint(i,i);});}}
        if(!check(backend.applies==before+1 && backend.physical==QPoint(31,31),"batched owner updates make one physical publication"))return false;
    }
    {
        C::Sessions sessions;Backend backend;backend.cursor.reset();C controller(sessions,backend.ports());FeatureRunSession s;
        if(!check(!controller.engageEarly(s),"missing context/point never creates an unowned lock"))return false;
        s.runningMode=FeatureRunMode::Trigger;s.lockMouseDuringFirstLoopCount=3;s.triggerPhase=TriggerSessionPhase::Monitoring;
        if(!check(!C::earlyWindow(s),"monitoring never qualifies as early action loop"))return false;
        s.triggerPhase=TriggerSessionPhase::RunningAction;s.sessionIteration=3;
        if(!check(!C::earlyWindow(s),"absent loop context falls back to session iteration"))return false;
    }
    {
        std::atomic<bool> valid{true};std::vector<std::thread> threads;
        for(int i=0;i<8;++i)threads.emplace_back([&]{for(int n=0;n<1000;++n){MouseCenterLock::beginSyntheticPointerOperation();if(!MouseCenterLock::syntheticPointerOperationActive())valid=false;MouseCenterLock::endSyntheticPointerOperation();}});
        for(auto& thread:threads)thread.join();MouseCenterLock::endSyntheticPointerOperation();
        if(!check(valid && !MouseCenterLock::syntheticPointerOperationActive(),"synthetic depth is balanced across workers and cannot underflow"))return false;
    }
    std::cout<<"Mouse ownership: older/newer isolation, prior-owner restoration, owned/moved/lost frames, context replacement, pause/ROI/preempt gates, failure threshold, backend/provider retirement, batched publication and8-worker synthetic-depth balance OK (recorded backend; no cursor movement/clipping/hooks)\n";
    return true;
}
