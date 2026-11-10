#include "app/TriggerPreemptionController.h"
#include "app/FeatureRunSession.h"
#include "app/SessionMouseLockController.h"
#include "core/input/InputSimulator.h"
#include <QCoreApplication>
#include <QPointer>
#include <vector>
#include <iterator>

namespace {
TriggerPreemptionController::Ports nativePorts() {
    if(qEnvironmentVariableIntValue("PIPBONG_SELF_TEST_UI")==1 || qEnvironmentVariableIntValue("PIPBONG_SMOKE_TEST")==1)return {};
    return {[]()->std::optional<QPoint>{int x=0,y=0;if(InputSimulator::getCursorScreenPosition(x,y))return QPoint(x,y);return std::nullopt;},
        [](QPoint p){InputSimulator::moveMouse(p.x(),p.y());}};
}
QString text(const char* value){return QCoreApplication::translate("MainWindow",value);}
}
TriggerPreemptionController::TriggerPreemptionController(Sessions& sessions,QObject* parent)
    :TriggerPreemptionController(sessions,nativePorts(),parent){}
TriggerPreemptionController::TriggerPreemptionController(Sessions& sessions,Ports ports,QObject* parent)
    :QObject(parent),m_sessions(sessions),m_ports(std::move(ports)){}
TriggerPreemptionController::~TriggerPreemptionController(){shutdown();}
bool TriggerPreemptionController::active(quint64 token) const {
    const auto frame=m_frames.find(token);return frame!=m_frames.end() && !frame->second.finished;
}

void TriggerPreemptionController::pause(FeatureRunSession& trigger) {
    if(m_stopped || trigger.triggerPreemptToken)return;
    const Host host=m_host;const auto ports=m_ports;const QPointer<TriggerPreemptionController> life(this);
    if(trigger.sessionContext) {
        const Weak key=trigger.sessionContext;const auto incoming=m_claims.find(key);
        if(incoming!=m_claims.end()) {
            const auto claim=incoming->second;m_claims.erase(incoming);
            // A newer triggered action takes over from paused monitor/older actions.
            // Remove incoming trigger edges so mutual preemption cannot form a cycle.
            for(auto& [id,session]:m_sessions)for(auto& snapshot:session.triggerPreemptedSessions) {
                if(!snapshot.context.owner_before(key) && !key.owner_before(snapshot.context))snapshot.pausedByTrigger=false;
            }
            if(claim.profile==trigger.profileId)trigger.sessionContext->resumeIfPauseUnchanged(claim.pause);
        }
    }
    trigger.triggerPreemptedSessions.clear();trigger.triggerReleasedOwnMouseLockForPreempt=false;
    std::vector<std::string> notices;
    for(auto& [id,other]:m_sessions) {
        if(&other==&trigger || id==trigger.featureId || other.userStopRequested
            || !other.sessionContext || other.sessionContext->shouldStop())continue;
        const auto context=other.sessionContext;const Weak key=context;
        auto claim=m_claims.find(key);
        bool owned=claim!=m_claims.end();
        if(const auto token=context->pauseIfRunning()) {
            if(!owned)claim=m_claims.emplace(key,Claim{0,*token,other.profileId}).first;
            claim->second.pause=*token;owned=true;
        }
        if(owned)++claim->second.owners;
        const bool locked=SessionMouseLockController::hasFeatureLock(other);
        if(!owned && !locked)continue; // An independent user pause is never ours to release.
        TriggerPreemptedSession snapshot;
        snapshot.featureId=id;snapshot.profileId=other.profileId;snapshot.context=context;
        snapshot.pausedByTrigger=owned;snapshot.releasedMouseLock=locked;
        trigger.triggerPreemptedSessions.push_back(std::move(snapshot));notices.push_back(id);
    }
    if(trigger.triggerPreemptedSessions.empty())return;
    const auto id=trigger.featureId;
    trigger.triggerPreemptToken=++m_next;if(!trigger.triggerPreemptToken)trigger.triggerPreemptToken=++m_next;
    const auto token=trigger.triggerPreemptToken;
    m_frames.emplace(token,Frame{});
    trigger.triggerReleasedOwnMouseLockForPreempt=SessionMouseLockController::hasFeatureLock(trigger);
    const auto count=trigger.triggerPreemptedSessions.size();
    if(ports.cursor) {
        const auto point=ports.cursor();
        if(!life || m_stopped || !active(token))return;
        m_frames.at(token).cursor=point;
    }
    for(const auto& peer:notices) {
        if(host.log)host.log(peer,text("다른 기능 실행을 잠시 멈춤"),false);
        if(!life || m_stopped || !active(token))return;
    }
    if(host.reconcile)host.reconcile();
    if(!life || m_stopped || !active(token))return;
    if(host.log)host.log(id,text("다른 기능 %1개를 잠시 멈춤").arg(count),false);
    if(life && !m_stopped && host.changed)host.changed();
}

void TriggerPreemptionController::resume(FeatureRunSession& trigger) {
    const auto token=trigger.triggerPreemptToken;
    if(!token)return;
    const Host host=m_host;const auto ports=m_ports;const QPointer<TriggerPreemptionController> life(this);
    auto snapshots=std::move(trigger.triggerPreemptedSessions);trigger.triggerPreemptedSessions.clear();
    trigger.triggerPreemptToken=0;trigger.triggerReleasedOwnMouseLockForPreempt=false;
    if(m_stopped)return;
    struct Release {std::shared_ptr<ExecutionContext> context;std::uint64_t pause;std::string id;QString owner;};
    std::vector<Release> releases;
    for(const auto& snapshot:snapshots) {
        if(!snapshot.pausedByTrigger)continue;
        const auto claim=m_claims.find(snapshot.context);
        if(claim==m_claims.end())continue;
        if(--claim->second.owners)continue;
        const auto pause=claim->second.pause;
        const auto context=snapshot.context.lock();
        if(!context){m_claims.erase(claim);continue;}
        releases.push_back({context,pause,snapshot.featureId,snapshot.profileId});
    }
    std::optional<QPoint> restore;
    if(const auto frame=m_frames.find(token);frame!=m_frames.end())frame->second.finished=true;
    // LIFO restores the interrupted action's cursor. Out-of-order completions wait
    // until their successors finish, then unwind once to the oldest finished frame.
    while(!m_frames.empty() && m_frames.rbegin()->second.finished) {
        const auto last=std::prev(m_frames.end());if(last->second.cursor)restore=last->second.cursor;
        m_frames.erase(last);
    }
    if(restore && ports.move)ports.move(*restore);
    if(!life || m_stopped)return;
    for(const auto& release:releases) {
        const auto claim=m_claims.find(Weak(release.context));
        if(claim==m_claims.end() || claim->second.owners || claim->second.pause!=release.pause)continue;
        m_claims.erase(claim);
        const auto peer=m_sessions.find(release.id);
        if(peer==m_sessions.end() || peer->second.sessionContext!=release.context || peer->second.profileId!=release.owner
            || peer->second.userStopRequested || release.context->shouldStop())continue;
        if(release.context->resumeIfPauseUnchanged(release.pause) && host.log)
            host.log(release.id,text("일시정지했던 기능을 다시 실행합니다"),true);
        if(!life || m_stopped)return;
    }
    if(host.reconcile)host.reconcile();
    if(life && !m_stopped && host.changed)host.changed();
}

void TriggerPreemptionController::shutdown() {
    if(m_stopped)return;
    m_stopped=true;
    for(const auto& [weak,claim]:m_claims) {
        const auto context=weak.lock();if(!context || context->shouldStop())continue;
        for(const auto& [id,session]:m_sessions) {
            if(session.sessionContext==context && session.profileId==claim.profile && !session.userStopRequested) {
                context->resumeIfPauseUnchanged(claim.pause);break;
            }
        }
    }
    m_claims.clear();m_frames.clear();
    for(auto& [id,session]:m_sessions) {
        session.triggerPreemptedSessions.clear();session.triggerPreemptToken=0;
        session.triggerReleasedOwnMouseLockForPreempt=false;
    }
}
