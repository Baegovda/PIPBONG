#include "app/RunUiPublisher.h"
#include "app/SessionRunPolicy.h"
#include <QCoreApplication>
#include <QMutex>
#include <QMutexLocker>
#include <QThread>
#include <QTimer>
#include <QPointer>
#include <atomic>
#include <utility>

struct RunUiPublisher::Dispatcher {
    QMutex mutex;
    RunUiPublisher* owner = nullptr;
    std::atomic<quint64> wakes{0};
};
struct RunUiPublisher::ChannelState {
    QMutex mutex;
    std::string featureId;
    QString owner;
    std::weak_ptr<ExecutionContext> context;
    bool ownerBound=false,hadContext=false;
    std::shared_ptr<Dispatcher> dispatcher;
    bool retired = false;
    bool wakeOutstanding = false;
    int iterations = 0;
    qint64 totalMs = 0;
    bool success = false;
    qint64 lastMs = 0;
    QString message;
};

RunUiPublisher::RunUiPublisher(HostCallbacks host, QObject* parent)
    : QObject(parent), m_host(std::move(host)), m_timer(new QTimer(this)), m_dispatch(std::make_shared<Dispatcher>()) {
    m_dispatch->owner = this;
    m_timer->setSingleShot(true);
    connect(m_timer, &QTimer::timeout, this, [this] { flushAll(); });
}

RunUiPublisher::~RunUiPublisher() {
    // Synchronize worker enqueue with object destruction. Queued functors are
    // bound to this QObject and Qt removes them if the receiver is destroyed.
    { QMutexLocker lock(&m_dispatch->mutex); m_dispatch->owner = nullptr; }
    for (auto& entry : m_channels) {
        QMutexLocker lock(&entry.second->mutex); entry.second->retired = true;
    }
}

RunUiPublisher::Channel RunUiPublisher::begin(const std::string& id) {
    Q_ASSERT(QThread::currentThread() == thread());
    retire(id);
    auto channel = std::make_shared<ChannelState>();
    channel->featureId = id;
    channel->dispatcher = m_dispatch;
    if(auto* session=m_host.session(id)){channel->owner=session->profileId;channel->context=session->sessionContext;channel->hadContext=bool(session->sessionContext);channel->ownerBound=true;}
    m_channels[id] = channel;
    return channel;
}

void RunUiPublisher::record(const Channel& channel, bool success, qint64 elapsed, const QString& message) {
    if (!channel) return;
    QMutexLocker lock(&channel->mutex);
    if (channel->retired) return;
    ++channel->iterations;
    channel->totalMs += elapsed;
    channel->success = success;
    channel->lastMs = elapsed;
    channel->message = message;
    if (channel->wakeOutstanding) return;
    channel->wakeOutstanding = true;
    const auto dispatcher = channel->dispatcher;
    QMutexLocker receiverLock(&dispatcher->mutex);
    if (!dispatcher->owner) return;
    auto* owner = dispatcher->owner;
    ++dispatcher->wakes;
    QMetaObject::invokeMethod(owner, [owner, channel] { owner->arm(channel); }, Qt::QueuedConnection);
}

void RunUiPublisher::retire(const std::string& id) {
    Q_ASSERT(QThread::currentThread() == thread());
    const auto found = m_channels.find(id);
    if (found == m_channels.end()) return;
    { QMutexLocker lock(&found->second->mutex); found->second->retired = true; }
    m_channels.erase(found);
}

void RunUiPublisher::arm(const Channel& channel) {
    Q_ASSERT(QThread::currentThread() == thread());
    const auto current = m_channels.find(channel->featureId);
    if (current == m_channels.end() || current->second != channel) return;
    if (!m_timer->isActive()) m_timer->start(SessionRunPolicy::fastRepeatUiFlushIntervalMs(m_host.activeRepeatCount()));
}

void RunUiPublisher::flush(const std::string& id) {
    Q_ASSERT(QThread::currentThread() == thread());
    const auto found = m_channels.find(id);
    if (found == m_channels.end()) return;
    const auto channel=found->second;const auto ownerContext=channel->context.lock();
    auto* session = m_host.session(id);
    if (!session || !channel->ownerBound || (channel->hadContext && !ownerContext)
        || session->profileId!=channel->owner || session->sessionContext!=ownerContext) { retire(id); return; }
    const auto original=session;const auto host=m_host;const QPointer<RunUiPublisher> life(this);
    const auto current=[life,host,id,original,ownerContext,owner=channel->owner,channel]()->FeatureRunSession*{
        if(!life)return nullptr;const auto active=life->m_channels.find(id);
        if(active==life->m_channels.end() || active->second!=channel)return nullptr;
        auto* s=host.session(id);return life && s==original && s && s->profileId==owner && s->sessionContext==ownerContext?s:nullptr;
    };
    int iterations;
    qint64 total, last;
    bool success;
    QString message;
    {
        QMutexLocker lock(&found->second->mutex);
        auto& pending = *found->second;
        iterations = pending.iterations;
        total = pending.totalMs;
        last = pending.lastMs;
        success = pending.success;
        message = pending.message;
        pending.iterations = 0;
        pending.totalMs = 0;
        pending.wakeOutstanding = false;
    }
    if (iterations <= 0) return;
    session->totalLoopElapsedMs += total;
    session->completedLoopCount += iterations;
    session->sessionIteration += iterations;
    session->hasLastLoopTiming = true;
    session->lastLoopNumber = session->sessionIteration;
    session->lastLoopElapsedMs = last;
    session->lastLoopAverageMs = session->totalLoopElapsedMs / session->completedLoopCount;
    session->lastLoopSuccess = success;
    session->loopsSinceLastLogPublish += iterations;
    if (session->sessionContext && session->runningMode == FeatureRunMode::RepeatCount)
        session->repeatRemaining = session->sessionContext->workerRepeatRemaining();
    publish(*session, success, message);
    if(auto* active=current();active && host.afterBatch)host.afterBatch(*active);
}

void RunUiPublisher::flushAll() {
    Q_ASSERT(QThread::currentThread() == thread());
    std::vector<std::string> ids;
    for (const auto& entry : m_channels) ids.push_back(entry.first);
    const auto host=m_host;const QPointer<RunUiPublisher> life(this);
    for (const auto& id : ids) {if(!life)return;flush(id);}
    if(life && host.activeRepeatCount()>=2 && life)host.refreshRunUi();
}

void RunUiPublisher::accumulate(FeatureRunSession& session, bool success, qint64 elapsed) {
    if (elapsed < 0) elapsed = session.loopTimer.isValid() ? session.loopTimer.elapsed() : 0;
    session.loopTimer.invalidate();
    session.hasLastLoopTiming = true;
    session.lastLoopNumber = session.sessionIteration + 1;
    session.lastLoopElapsedMs = elapsed;
    session.totalLoopElapsedMs += elapsed;
    ++session.completedLoopCount;
    session.lastLoopAverageMs = session.totalLoopElapsedMs / session.completedLoopCount;
    session.lastLoopSuccess = success;
}

void RunUiPublisher::syncTiming(const FeatureRunSession* session) {
    Q_ASSERT(QThread::currentThread()==thread());
    const auto host=m_host;const QPointer<RunUiPublisher> life(this);
    const bool shown=session && session->hasLastLoopTiming && host.isDisplayed(session);
    if(!life)return;
    const auto timing=shown?std::optional<Timing>{Timing{session->lastLoopNumber,session->lastLoopElapsedMs,session->lastLoopAverageMs,session->lastLoopSuccess}}:std::nullopt;
    host.timing(timing);
}

void RunUiPublisher::publish(FeatureRunSession& session,bool success,const QString& message) {
    Q_ASSERT(QThread::currentThread()==thread());
    const auto host=m_host;const QPointer<RunUiPublisher> life(this);
    const auto id=session.featureId;const auto owner=session.profileId;const auto context=session.sessionContext;const auto original=&session;
    const auto current=[life,host,id,owner,context,original]()->FeatureRunSession*{
        if(!life)return nullptr;auto* s=host.session(id);return life && s==original && s && s->profileId==owner && s->sessionContext==context?s:nullptr;
    };
    if(!current())return;
    const int active=host.activeRepeatCount();if(!life)return;
    const bool displayed=host.isDisplayed(&session);if(!life)return;
    const auto timing=session.hasLastLoopTiming?std::optional<Timing>{Timing{session.lastLoopNumber,session.lastLoopElapsedMs,session.lastLoopAverageMs,session.lastLoopSuccess}}:std::nullopt;
    const bool fast=context && context->suppressRepeatUi();
    bool log=!(fast && (active>=2 || !displayed));
    if(log && fast){if(session.loopLogPublishTimer.isValid() && session.loopLogPublishTimer.elapsed()<500)log=false;else session.loopLogPublishTimer.restart();}
    QString line;
    if(log){
        line=tr("%1회째 루프 · %2ms · %3").arg(session.lastLoopNumber).arg(session.lastLoopElapsedMs).arg(success?tr("성공"):tr("실패"));
        if(!success && !message.isEmpty())line+=QStringLiteral(" · ")+message;
        if(fast && session.loopsSinceLastLogPublish>1)line+=tr(" (+%1회)").arg(session.loopsSinceLastLogPublish-1);
        session.loopsSinceLastLogPublish=0;
    }
    if(displayed && active<2)host.timing(timing);
    if(log)if(auto* selected=current())host.log(*selected,line,success);
}

void RunUiPublisher::completeLoop(FeatureRunSession& session, bool success, const QString& message) {
    accumulate(session, success);
    publish(session, success, message);
}

quint64 RunUiPublisher::queuedWakeCount() const { return m_dispatch->wakes.load(); }
