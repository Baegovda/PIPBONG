#include "core/workflow/HoldKeyTapMultiplexer.h"

#include "core/input/InputSimulator.h"
#include "core/workflow/ExecutionContext.h"
#include "core/workflow/Workflow.h"
#include "core/workflow/blocks/KeyPressBlock.h"
#include "model/Feature.h"
#include "model/FeatureRunMode.h"

#include <QMetaObject>
#include <QPointer>

#include <chrono>
#include <condition_variable>
#include <mutex>
#include <thread>
#include <unordered_map>
#include <vector>

#ifdef _WIN32
#include <mmsystem.h>
#endif

namespace {

#ifdef _WIN32
std::atomic<int> g_holdTapTimerRefCount{0};

void acquireHoldTapTimer() {
    if (g_holdTapTimerRefCount.fetch_add(1, std::memory_order_acq_rel) == 0) {
        timeBeginPeriod(1);
    }
}

void releaseHoldTapTimer() {
    const int previous = g_holdTapTimerRefCount.fetch_sub(1, std::memory_order_acq_rel);
    if (previous == 1) {
        timeEndPeriod(1);
    }
}
#endif

using Clock = std::chrono::steady_clock;

struct Lane {
    quint64 token=0;
    std::string featureId;
    int virtualKey = 0;
    int intervalMs = 0;
    HoldKeyTapMultiplexer::ShouldContinueFn shouldContinue;
    std::shared_ptr<ExecutionContext> context;
    Clock::time_point nextFire{};
    bool nextFireValid = false;
};

} // namespace

bool holdKeyTapWorkflowVirtualKey(const Feature& feature, int& outVirtualKey) {
    if (feature.runMode() != FeatureRunMode::Hold) {
        return false;
    }
    const Workflow& workflow = feature.workflow();
    if (!workflow.loopRegions().empty()) {
        return false;
    }
    const auto& blocks = workflow.blocks();
    if (blocks.size() != 1) {
        return false;
    }
    const Block* block = blocks[0].get();
    if (!block || block->type() != BlockType::KeyPress) {
        return false;
    }
    const auto* keyPress = static_cast<const KeyPressBlock*>(block);
    if (!keyPress->useMainKey || keyPress->action != KeyAction::Tap) {
        return false;
    }
    if (keyPress->modifierActions.hasAny()) {
        return false;
    }
    outVirtualKey = keyPress->virtualKey;
    return true;
}

struct HoldKeyTapMultiplexer::Impl {
    HoldKeyTapMultiplexer* owner = nullptr;
    HoldKeyTapMultiplexer::TapEmitter tapEmitter;
    mutable std::mutex mutex;
    std::condition_variable cv;
    std::unordered_map<std::string, Lane> lanes;
    std::vector<std::shared_ptr<ExecutionContext>> retiredContexts;
    quint64 nextToken=0;
    std::atomic<bool> shutdown{false};
    std::thread worker;
    bool zeroIntervalTimerActive = false;

    explicit Impl(HoldKeyTapMultiplexer* mux, HoldKeyTapMultiplexer::TapEmitter emitter)
        : owner(mux), tapEmitter(std::move(emitter)) {
        worker = std::thread([this]() { workerMain(); });
    }

    ~Impl() {
        shutdown.store(true, std::memory_order_release);
        cv.notify_all();
        if (worker.joinable()) {
            worker.join();
        }
        // The worker has finished its in-flight tap before final cleanup.
        for(auto& context:retiredContexts){context->restoreRunHeldInput();context->endRunKeyboardSession();}
        for(auto& entry:lanes)if(entry.second.context){entry.second.context->restoreRunHeldInput();entry.second.context->endRunKeyboardSession();}
#ifdef _WIN32
        if (zeroIntervalTimerActive) {
            releaseHoldTapTimer();
            zeroIntervalTimerActive = false;
        }
#endif
    }

    bool laneActiveUnlocked(const std::string& featureId) const {
        return lanes.find(featureId) != lanes.end();
    }

    void updateZeroIntervalTimerUnlocked() {
#ifdef _WIN32
        bool needsTimer = false;
        for (const auto& entry : lanes) {
            if (entry.second.intervalMs <= 0) {
                needsTimer = true;
                break;
            }
        }
        if (needsTimer && !zeroIntervalTimerActive) {
            acquireHoldTapTimer();
            zeroIntervalTimerActive = true;
        } else if (!needsTimer && zeroIntervalTimerActive) {
            releaseHoldTapTimer();
            zeroIntervalTimerActive = false;
        }
#endif
    }

    void emitLaneFinished(const std::string& featureId,quint64 token, bool success, const QString& message) {
        const QString qFeatureId = QString::fromStdString(featureId);
        QMetaObject::invokeMethod(
            owner,
            [owner = QPointer<HoldKeyTapMultiplexer>(owner), qFeatureId, token, success, message]() {
                if(!owner)return;
                emit owner->ownedLaneFinished(qFeatureId,token,success,message);
                if(owner)emit owner->laneFinished(qFeatureId, success, message);
            },
            Qt::QueuedConnection);
    }

    void finishLaneUnlocked(const std::string& featureId,quint64 expectedToken, bool success, const QString& message) {
        auto it = lanes.find(featureId);
        if (it == lanes.end() || it->second.token!=expectedToken) {
            return;
        }
        Lane lane = std::move(it->second);
        lanes.erase(it);
        updateZeroIntervalTimerUnlocked();

        if (lane.context) {
            lane.context->restoreRunHeldInput();
            lane.context->endRunKeyboardSession();
        }

        emitLaneFinished(featureId,lane.token,success,message);
    }

    bool laneShouldStop(const Lane& lane, bool& outSuccess, QString& outMessage) {
        if (!lane.context) {
            outSuccess = false;
            outMessage = QStringLiteral("중지됨");
            return true;
        }
        if (lane.context->shouldStop()) {
            outSuccess = false;
            outMessage = QStringLiteral("중지됨");
            return true;
        }
        if (!lane.shouldContinue || !lane.shouldContinue()) {
            outSuccess = true;
            outMessage = QStringLiteral("완료");
            return true;
        }
        return false;
    }

    void workerMain() {
        while (!shutdown.load(std::memory_order_acquire)) {
            std::vector<std::shared_ptr<ExecutionContext>> retired;
            {
                std::unique_lock<std::mutex> lock(mutex);
                cv.wait(lock,[this]{return shutdown.load(std::memory_order_acquire) || !lanes.empty() || !retiredContexts.empty();});
                retired.swap(retiredContexts);
                for(auto& context:retired){
                    bool reused=false;for(const auto& entry:lanes)if(entry.second.context==context){reused=true;break;}
                    if(!reused){context->restoreRunHeldInput();context->endRunKeyboardSession();}
                }
            }
            if(shutdown.load(std::memory_order_acquire))break;
            const auto now = Clock::now();
            struct TapJob {
                std::string featureId;
                quint64 token=0;
                int virtualKey = 0;
                std::shared_ptr<ExecutionContext> context;
                HoldKeyTapMultiplexer::ShouldContinueFn shouldContinue;
            };
            std::vector<TapJob> ready;
            struct Finished {std::string id;quint64 token;bool success;QString message;};
            std::vector<Finished> finishing;

            {
                std::unique_lock<std::mutex> lock(mutex);
                for (auto& entry : lanes) {
                    Lane& lane = entry.second;
                    bool success = true;
                    QString message = QStringLiteral("완료");
                    if (laneShouldStop(lane, success, message)) {
                        finishing.push_back({entry.first,lane.token,success,message});
                        continue;
                    }

                    if(lane.context->isPaused())continue;
                    if (!lane.nextFireValid || now >= lane.nextFire) {
                        ready.push_back(TapJob{entry.first,lane.token,
                                               lane.virtualKey,
                                               lane.context,
                                               lane.shouldContinue});
                        if (lane.intervalMs > 0) {
                            lane.nextFire = now + std::chrono::milliseconds(lane.intervalMs);
                            lane.nextFireValid = true;
                        } else {
                            lane.nextFireValid = false;
                        }
                    }
                }

                for (const auto& finish : finishing) {
                    finishLaneUnlocked(finish.id,finish.token,finish.success,finish.message);
                }
            }

            for (const TapJob& job : ready) {
                {
                    std::lock_guard<std::mutex> lock(mutex);const auto found=lanes.find(job.featureId);
                    if(found==lanes.end() || found->second.token!=job.token)continue;
                }
                bool success = true;
                QString message = QStringLiteral("완료");
                Lane lane;
                lane.featureId = job.featureId;
                lane.virtualKey = job.virtualKey;
                lane.context = job.context;
                lane.shouldContinue = job.shouldContinue;
                if (laneShouldStop(lane, success, message)) {
                    std::lock_guard<std::mutex> lock(mutex);
                    finishLaneUnlocked(job.featureId,job.token,success,message);
                    continue;
                }

                if(job.context->isPaused())continue;
                InputSimulator::setActiveExecutionContext(job.context.get());
                if (tapEmitter) {
                    tapEmitter(job.virtualKey, *job.context);
                } else {
                    InputSimulator::sendKey(job.virtualKey, KeyAction::Tap, {}, true);
                }
                job.context->restoreRunHeldInput();
                InputSimulator::setActiveExecutionContext(nullptr);

                if (laneShouldStop(lane, success, message)) {
                    std::lock_guard<std::mutex> lock(mutex);
                    finishLaneUnlocked(job.featureId,job.token,success,message);
                }
            }

            if (ready.empty() && finishing.empty()) {
                Clock::time_point wakeAt = Clock::time_point::max();
                bool hasZeroInterval = false;
                {
                    std::lock_guard<std::mutex> lock(mutex);
                    for (const auto& entry : lanes) {
                        if(entry.second.context && entry.second.context->isPaused())continue;
                        if (entry.second.intervalMs <= 0) {
                            hasZeroInterval = true;
                            break;
                        }
                        if (entry.second.nextFireValid && entry.second.nextFire < wakeAt) {
                            wakeAt = entry.second.nextFire;
                        }
                    }
                }

                if (hasZeroInterval) {
                    std::this_thread::sleep_for(std::chrono::milliseconds(1));
                } else if (wakeAt != Clock::time_point::max()) {
                    const auto delay = wakeAt - Clock::now();
                    if (delay.count() > 0) {
                        std::this_thread::sleep_for(std::min(delay,std::chrono::duration_cast<Clock::duration>(std::chrono::milliseconds(8))));
                    } else {
                        std::this_thread::sleep_for(std::chrono::milliseconds(1));
                    }
                } else {
                    std::unique_lock<std::mutex> lock(mutex);
                    cv.wait_for(lock, std::chrono::milliseconds(8), [this]() {
                        return shutdown.load(std::memory_order_acquire) || lanes.empty();
                    });
                }
            }
        }
    }
};

HoldKeyTapMultiplexer::HoldKeyTapMultiplexer(QObject* parent, TapEmitter emitter)
    : QObject(parent)
    , m_impl(std::make_unique<Impl>(this, std::move(emitter))) {}

HoldKeyTapMultiplexer::~HoldKeyTapMultiplexer() = default;

bool HoldKeyTapMultiplexer::isLaneActive(const std::string& featureId) const {
    std::lock_guard<std::mutex> lock(m_impl->mutex);
    return m_impl->laneActiveUnlocked(featureId);
}

bool HoldKeyTapMultiplexer::hasAnyActiveLane() const {
    std::lock_guard<std::mutex> lock(m_impl->mutex);
    return !m_impl->lanes.empty();
}

quint64 HoldKeyTapMultiplexer::startLane(const std::string& featureId,
                                      int virtualKey,
                                      int intervalMs,
                                      ShouldContinueFn shouldContinue,
                                      const std::shared_ptr<ExecutionContext>& context) {
    if (featureId.empty() || virtualKey == 0 || !shouldContinue || !context) {
        return 0;
    }

    quint64 token=0;
    {
        std::lock_guard<std::mutex> lock(m_impl->mutex);
        if (m_impl->laneActiveUnlocked(featureId)) {
            const auto old=m_impl->lanes.at(featureId).context;
            if(old && old!=context){old->requestStop();m_impl->retiredContexts.push_back(old);}
            m_impl->lanes.erase(featureId);
        }

        token=++m_impl->nextToken;if(!token)token=++m_impl->nextToken;
        Lane lane;lane.token=token;
        lane.featureId = featureId;
        lane.virtualKey = virtualKey;
        lane.intervalMs = intervalMs;
        lane.shouldContinue = std::move(shouldContinue);
        lane.context = context;
        lane.nextFireValid = false;

        context->resetStop(false);
        context->beginRunKeyboardSessionIfNeeded();

        m_impl->lanes.emplace(featureId, std::move(lane));
        m_impl->updateZeroIntervalTimerUnlocked();
    }

    m_impl->cv.notify_one();
    return token;
}

void HoldKeyTapMultiplexer::stopLane(const std::string& featureId) {
    {
        std::lock_guard<std::mutex> lock(m_impl->mutex);
        const auto it=m_impl->lanes.find(featureId);
        if(it!=m_impl->lanes.end() && it->second.context)it->second.context->requestStop();
    }
    m_impl->cv.notify_one();
}

void HoldKeyTapMultiplexer::stopLaneIfToken(const std::string& id,quint64 token) {
    {std::lock_guard<std::mutex> lock(m_impl->mutex);const auto it=m_impl->lanes.find(id);if(it!=m_impl->lanes.end() && it->second.token==token && it->second.context)it->second.context->requestStop();}
    m_impl->cv.notify_one();
}

void HoldKeyTapMultiplexer::stopAll() {
    {
        std::lock_guard<std::mutex> lock(m_impl->mutex);
        for(const auto& entry:m_impl->lanes)if(entry.second.context)entry.second.context->requestStop();
    }
    m_impl->cv.notify_all();
}
