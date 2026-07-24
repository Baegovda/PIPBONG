#include "core/workflow/HoldKeyTapRunner.h"

#include "core/input/InputSimulator.h"
#include "core/workflow/ExecutionContext.h"
#include "core/workflow/Workflow.h"
#include "core/workflow/blocks/KeyPressBlock.h"
#include "model/Feature.h"
#include "model/FeatureRunMode.h"
#include "core/workflow/Workflow.h"

#include <QMetaObject>

#include <atomic>
#include <memory>
#include <thread>

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

struct HoldKeyTapRunner::Job {
    int virtualKey = 0;
    int intervalMs = 0;
    HoldKeyTapRunner::ShouldContinueFn shouldContinue;
    std::shared_ptr<ExecutionContext> context;
};

HoldKeyTapRunner::HoldKeyTapRunner(QObject* parent)
    : QObject(parent) {}

HoldKeyTapRunner::~HoldKeyTapRunner() {
    stop();
    if (m_thread && m_thread->joinable()) {
        m_thread->join();
    }
}

bool HoldKeyTapRunner::isRunning() const {
    return m_running.load(std::memory_order_acquire);
}

void HoldKeyTapRunner::stop() {
    if (m_context) {
        m_context->requestStop();
    }
}

void HoldKeyTapRunner::run(int virtualKey,
                           int intervalMs,
                           ShouldContinueFn shouldContinue,
                           const std::shared_ptr<ExecutionContext>& context) {
    if (m_running.load(std::memory_order_acquire) || !context || virtualKey == 0 || !shouldContinue) {
        return;
    }

    if (m_thread && m_thread->joinable()) {
        m_thread->join();
    }

    m_context = context;
    m_running.store(true, std::memory_order_release);

    auto job = std::make_shared<Job>();
    job->virtualKey = virtualKey;
    job->intervalMs = intervalMs;
    job->shouldContinue = std::move(shouldContinue);
    job->context = context;

    HoldKeyTapRunner* self = this;
    m_thread = std::make_unique<std::thread>([self, job]() {
        bool success = true;
        QString message = QStringLiteral("완료");

#ifdef _WIN32
        const bool raiseTimer = job->intervalMs <= 0;
        if (raiseTimer) {
            acquireHoldTapTimer();
        }
#endif

        job->context->resetStop();
        job->context->beginRunKeyboardSessionIfNeeded();
        InputSimulator::setActiveExecutionContext(job->context.get());

        while (true) {
            if (job->context->shouldStop()) {
                success = false;
                message = QStringLiteral("중지됨");
                break;
            }
            if (!job->shouldContinue || !job->shouldContinue()) {
                break;
            }

            InputSimulator::sendKey(job->virtualKey, KeyAction::Tap, {}, true);
            job->context->restoreRunHeldInput();

            if (job->context->shouldStop()) {
                success = false;
                message = QStringLiteral("중지됨");
                break;
            }
            if (!job->shouldContinue || !job->shouldContinue()) {
                break;
            }

            if (job->intervalMs > 0) {
                if (!job->context->interruptibleSleepMs(job->intervalMs)) {
                    success = false;
                    message = QStringLiteral("중지됨");
                    break;
                }
            }
        }

        job->context->restoreRunHeldInput();
        job->context->endRunKeyboardSession();
        InputSimulator::setActiveExecutionContext(nullptr);

#ifdef _WIN32
        if (raiseTimer) {
            releaseHoldTapTimer();
        }
#endif

        const bool finalSuccess = success;
        const QString finalMessage = message;
        QMetaObject::invokeMethod(
            self,
            [self, finalSuccess, finalMessage]() {
                self->m_running.store(false, std::memory_order_release);
                self->m_context.reset();
                emit self->finished(finalSuccess, finalMessage);
            },
            Qt::QueuedConnection);
    });
}
