#include "app/HoldTapSessionController.h"
#include "app/RunSessionPolicySnapshot.h"
#include "model/Feature.h"

using Controller=HoldTapSessionController;
FeatureRunSession* Controller::Ticket::current(bool checkGeneration,bool checkToken) const {
    if(!life || !host.session)return nullptr;
    auto* s=host.session(id);
    return life && s && s==original && s->profileId==owner && s->sessionContext==context
        && s->engine.get()==engine && (!checkToken || s->holdKeyTapLaneToken==token)
        && (!checkGeneration || s->holdRepeatGeneration==generation)?s:nullptr;
}
Feature* Controller::Ticket::feature(bool checkGeneration,bool checkToken) const {
    auto* s=current(checkGeneration,checkToken);if(!s || !host.feature)return nullptr;
    auto* f=host.feature(*s);return current(checkGeneration,checkToken)?f:nullptr;
}
Controller::Ticket Controller::ticket(FeatureRunSession& s) {
    // Pin an identity before any mutating callback, including foreground defer.
    if(!s.sessionContext)s.sessionContext=std::make_shared<ExecutionContext>();
    return {this,m_host,s.featureId,s.profileId,&s,s.sessionContext,s.engine.get(),s.holdRepeatGeneration,s.holdKeyTapLaneToken};
}
Controller::HoldTapSessionController(Host host,Ports ports,QObject* parent)
    :QObject(parent),m_host(std::move(host)),m_ports(std::move(ports)){}
Controller::HoldTapSessionController(HoldKeyTapMultiplexer& mux,Host host,QObject* parent)
    :Controller(std::move(host),Ports{
        [mux=QPointer<HoldKeyTapMultiplexer>(&mux)](const auto& id,int key,int interval,Reader reader,const auto& context){return mux?mux->startLane(id,key,interval,std::move(reader),context):0;},
        [mux=QPointer<HoldKeyTapMultiplexer>(&mux)](const auto& id,quint64 token){if(mux)mux->stopLaneIfToken(id,token);}},parent){
    connect(&mux,&HoldKeyTapMultiplexer::ownedLaneFinished,this,[this](const QString& id,quint64 token,bool success,const QString& message){finished(id.toStdString(),token,success,message);});
}
void Controller::launch(FeatureRunSession& s,Feature* feature,int key) {
    const auto t=ticket(s);
    if(!t.current() || s.holdKeyTapLaneActive || s.userStopRequested)return;
    Reader reader;
    const auto prepared=m_prepared.find(t.id);
    if(prepared!=m_prepared.end() && prepared->second.ticket.current())reader=prepared->second.reader;
    else reader=t.host.holdReader?t.host.holdReader(t.id):Reader{};
    if(!t.current())return;
    m_prepared[t.id]={t,reader};launch(s,feature,key,std::move(reader));
}
void Controller::launch(FeatureRunSession& s,Feature* feature,int key,Reader reader) {
    auto t=ticket(s);const auto ports=m_ports;
    if(!t.current() || t.feature()!=feature || !feature)return;
    if(!SessionRunPolicy::allowsPreparedLaunch(s.awaitingFirstTemplateRoi) || s.holdKeyTapLaneActive || s.userStopRequested)return;
    int ownedKey=0;
    if(!s.usesHoldKeyTapFastPath || !holdKeyTapWorkflowVirtualKey(*feature,ownedKey) || key!=ownedKey || !ports.start){
        if(t.host.finish)t.host.finish(t.id,false,tr("연타 실행 구성이 변경되었습니다"),false);return;
    }
    const int interval=feature->resolvedLoopIntervalMs();
    if(t.host.deferForeground){const bool deferred=t.host.deferForeground(s,feature);if(!t.current() || deferred)return;}
    if(t.feature()!=feature)return;
    if(t.host.syncContext)t.host.syncContext(s,feature);
    if(!t.current() || t.feature()!=feature)return;
    if(t.host.applyPolicies)t.host.applyPolicies(s,feature);
    if(!t.current() || t.feature()!=feature)return;
    t.context->setSuppressRepeatUi(true);
    if(t.host.syncInterrupt)t.host.syncInterrupt(s,feature);
    if(!t.current() || t.feature()!=feature)return;
    s.holdKeyTapLaneActive=true;
    const auto token=ports.start(t.id,key,interval,[reader,weak=std::weak_ptr<ExecutionContext>(t.context)]{
        const auto context=weak.lock();return context && !context->shouldStop() && reader && reader();
    },t.context);
    if(!t.current()) {if(token && ports.stop)ports.stop(t.id,token);return;}
    s.holdKeyTapLaneToken=token;t.token=token;
    if(!token){s.holdKeyTapLaneActive=false;if(t.host.finish)t.host.finish(t.id,false,tr("연타를 시작할 수 없습니다"),false);return;}
    m_prepared.erase(t.id);m_lanes[t.id]={t,std::move(reader)};
    if(t.host.synchronize)t.host.synchronize();
    auto* current=t.current();if(!current)return;
    if(current->hotkeyLaunchedSession){
        if(!current->holdKeyTapStartUiDone){current->holdKeyTapStartUiDone=true;queueStarted(*current);}
    } else {
        m_starts[t.id]=t;presentStarts({t.id});
    }
}
void Controller::finished(const std::string& id,quint64 token,bool success,const QString& message) {
    const auto found=m_lanes.find(id);if(found==m_lanes.end() || found->second.ticket.token!=token)return;
    const auto lane=found->second;m_lanes.erase(found);auto t=lane.ticket;
    // A user stop advances repeat generation; its own lane completion still owns
    // teardown, whereas another lane/context with the same ID never does.
    auto* s=t.current(false);if(!s)return;
    s->holdKeyTapLaneActive=false;s->holdKeyTapLaneToken=0;
    t.token=0;t.generation=s->holdRepeatGeneration;
    if(t.host.synchronize)t.host.synchronize();
    s=t.current();if(!s)return;
    if(s->userStopRequested){
        if(t.host.deferStopped && t.host.deferStopped()){
            if(t.current() && t.host.scheduleEnd)t.host.scheduleEnd(id);return;
        }
        if(t.current() && t.host.finish)t.host.finish(id,success,message,false);return;
    }
    auto* feature=t.feature();const bool held=lane.reader && lane.reader();
    s=t.current();if(!s)return;
    if(feature && !s->userStopRequested && SessionRunPolicy::shouldContinueSession(runSessionPolicyInput(*s),held)){
        int key=0;if(t.feature()==feature && holdKeyTapWorkflowVirtualKey(*feature,key)){launch(*s,feature,key,lane.reader);return;}
    }
    if(t.current() && t.host.finish)t.host.finish(id,success,message,false);
}
void Controller::queueStarted(FeatureRunSession& s) {
    auto t=ticket(s);if(!t.current())return;m_starts[t.id]=t;
    if(t.host.scheduleStart)t.host.scheduleStart(t.id);
}
void Controller::presentStarts(const std::vector<std::string>& ids) {
    const QPointer<Controller> life(this);const Host host=m_host;
    bool selected=false,logged=false;
    for(const auto& id:ids){
        if(!life)return;const auto found=m_starts.find(id);if(found==m_starts.end())continue;
        const auto t=found->second;m_starts.erase(found);auto* s=t.current(true,false);if(!s || s->userStopRequested)continue;
        auto* feature=t.feature(true,false);
        if(!selected && feature && host.activeOwner && host.activeOwner(*s) && host.select){host.select(feature);selected=true;}
        s=t.current(true,false);if(!s || s->userStopRequested)continue;
        if(host.displayed && host.displayed(*s) && host.decorate)host.decorate(*s,[t]{return t.current(true,false)!=nullptr;});
        s=t.current(true,false);if(!s || s->userStopRequested)continue;
        if(!logged && host.logDetails && host.logDetails(*s) && host.logStart){host.logStart(*s);logged=true;}
    }
    if(!life)return;
    if(selected && host.persist)host.persist();
    if(life && host.refresh)host.refresh();
}
void Controller::rememberEnd(FeatureRunSession& s) {auto t=ticket(s);if(t.current())m_ends[t.id]=std::move(t);}
void Controller::finishPending(const std::string& id) {
    const auto found=m_ends.find(id);if(found==m_ends.end())return;
    const auto t=found->second;m_ends.erase(found);auto* s=t.current();
    if(!s || s->holdRunActive || s->holdKeyTapLaneActive)return;
    if(t.host.finish)t.host.finish(id,true,QString(),true);
}
void Controller::retire(FeatureRunSession& s) {
    const auto matches=[&](const Ticket& t){return t.original==&s && t.owner==s.profileId && t.context==s.sessionContext;};
    if(const auto it=m_lanes.find(s.featureId);it!=m_lanes.end() && matches(it->second.ticket))m_lanes.erase(it);
    if(const auto it=m_prepared.find(s.featureId);it!=m_prepared.end() && matches(it->second.ticket))m_prepared.erase(it);
    if(const auto it=m_starts.find(s.featureId);it!=m_starts.end() && matches(it->second))m_starts.erase(it);
    if(const auto it=m_ends.find(s.featureId);it!=m_ends.end() && matches(it->second))m_ends.erase(it);
}
