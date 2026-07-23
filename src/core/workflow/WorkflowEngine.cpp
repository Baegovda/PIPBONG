#include "core/workflow/WorkflowEngine.h"

#include "core/input/InputSimulator.h"
#include "core/diagnostics/CrashReporter.h"
#include "core/diagnostics/DiagnosticHub.h"
#include "core/diagnostics/WorkflowRunProfiler.h"
#include "core/workflow/Block.h"
#include "core/workflow/Block.h"
#include "core/workflow/ExecutionContext.h"
#include "core/workflow/WorkflowRunner.h"
#include "ui/OpenCvQtUtil.h"

#include <algorithm>
#include <chrono>
#include <QMetaType>
#include <QMutexLocker>
#include <QThread>
#include <QString>

#include <algorithm>
#ifdef _WIN32
#include <mmsystem.h>
#endif

Q_DECLARE_METATYPE(BlockProgressKind)
Q_DECLARE_METATYPE(std::shared_ptr<Workflow>)
Q_DECLARE_METATYPE(std::shared_ptr<ExecutionContext>)

class WorkflowEngine::Worker : public QThread {
public:
    explicit Worker(WorkflowEngine* engine)
        : m_engine(engine) {}

protected:
    void run() override {
        DiagnosticHub::setWorkerLabel(QStringLiteral("WorkflowEngine"));
        DiagnosticHub::touchWorkerHeartbeat();
        while (true) {
            std::shared_ptr<Workflow> workflow;
            std::shared_ptr<ExecutionContext> context;
            WorkflowRunPreparer preparer;

            {
                QMutexLocker lock(&m_engine->m_mutex);
                while (!m_engine->m_workerShutdown && !m_engine->m_hasQueuedJob) {
                    m_engine->m_jobReady.wait(&m_engine->m_mutex);
                }
                if (m_engine->m_workerShutdown && !m_engine->m_hasQueuedJob) {
                    break;
                }

                preparer = std::move(m_engine->m_queuedPreparer);
                if (!preparer) {
                    workflow = m_engine->m_queuedWorkflow;
                    context = m_engine->m_queuedContext;
                }
                m_engine->m_queuedWorkflow.reset();
                m_engine->m_queuedContext.reset();
                m_engine->m_hasQueuedJob = false;
            }

            if (preparer) {
                PreparedWorkflowRun prepared = preparer();
                workflow = std::move(prepared.workflow);
                context = std::move(prepared.context);
            }

            if (!workflow || !context) {
                bool emitFinished = false;
                const bool pendingSuccess = false;
                const QString pendingMessage = QStringLiteral("잘못된 워크플로우");
                {
                    QMutexLocker lock(&m_engine->m_mutex);
                    m_engine->m_activeContext.reset();
                    m_engine->m_running = false;
                    emitFinished = m_engine->m_hasPendingResult;
                    m_engine->m_hasPendingResult = false;
                    m_engine->m_pendingSuccess = pendingSuccess;
                    m_engine->m_pendingMessage = pendingMessage;
                }

                if (emitFinished) {
                    QMetaObject::invokeMethod(
                        m_engine,
                        [this, pendingSuccess, pendingMessage]() {
                            emit m_engine->finished(pendingSuccess, pendingMessage);
                        },
                        Qt::QueuedConnection);
                }
                continue;
            }

            emit m_engine->sessionPrepared(workflow, context);
            {
                QMutexLocker lock(&m_engine->m_mutex);
                m_engine->m_activeContext = context;
            }

            emit m_engine->started();
            context->resetStop();
            context->setLogCallback([this, context](const std::string& message) {
                if (context->suppressRepeatUi()) {
                    return;
                }
                emit m_engine->logMessage(QString::fromStdString(message));
            });

            context->beginRunKeyboardSessionIfNeeded();
            InputSimulator::setActiveExecutionContext(context.get());

            bool overallSuccess = true;
            QString finalMessage = QStringLiteral("완료");

            WorkflowRunHooks hooks;
            hooks.onBlockStarted = [this, context](int i, const std::string& summary) {
                DiagnosticHub::touchWorkerHeartbeat();
                CrashReporter::noteBreadcrumb(
                    QStringLiteral("workflow"),
                    QStringLiteral("block start #%1 %2")
                        .arg(i + 1)
                        .arg(QString::fromStdString(summary)));
                if (context->suppressRepeatUi()) {
                    return;
                }
                emit m_engine->blockStarted(i, QString::fromStdString(summary));
            };
            hooks.onBlockFinished = [this, context, workflow](int i,
                                                     bool success,
                                                     const std::string& message,
                                                     qint64 durationMs,
                                                     int64_t imageFindMatchDurationMs,
                                                     int imageFindPollAttempts) {
                Q_UNUSED(message);
                Q_UNUSED(imageFindMatchDurationMs);
                Q_UNUSED(imageFindPollAttempts);
                if (WorkflowRunProfiler::isEnabled() && workflow) {
                    const auto& blocks = workflow->blocks();
                    std::string blockTypeStr = "unknown";
                    if (i >= 0 && i < static_cast<int>(blocks.size()) && blocks[static_cast<size_t>(i)]) {
                        blockTypeStr = blockTypeToString(blocks[static_cast<size_t>(i)]->type());
                    }
                    WorkflowRunProfiler::recordBlockEnd(
                        i, blockTypeStr.c_str(), success, durationMs * 1000);
                }
                CrashReporter::noteBreadcrumb(
                    QStringLiteral("workflow"),
                    QStringLiteral("block finish #%1 success=%2 attempts=%3")
                        .arg(i + 1)
                        .arg(success ? QStringLiteral("yes") : QStringLiteral("no"))
                        .arg(imageFindPollAttempts));
                if (context->suppressRepeatUi()) {
                    return;
                }
                emit m_engine->blockFinished(i,
                                             success,
                                             QString::fromStdString(message),
                                             durationMs,
                                             static_cast<qint64>(imageFindMatchDurationMs),
                                             imageFindPollAttempts);
            };
            hooks.onBlockImageFindAttempt = [this, context](int blockIndex,
                                                  int attemptCount,
                                                  double matchThreshold,
                                                  double detectedConfidence,
                                                  bool matched) {
                if (context->suppressRepeatUi()) {
                    return;
                }
                emit m_engine->blockImageFindAttempt(
                    blockIndex, attemptCount, matchThreshold, detectedConfidence, matched);
            };
            hooks.onImageFindFailureHandling = [this, context](int blockIndex,
                                                       int returnToPreviousCount,
                                                       int retryAfterNextCount) {
                if (context->suppressRepeatUi()) {
                    return;
                }
                emit m_engine->imageFindFailureHandling(
                    blockIndex, returnToPreviousCount, retryAfterNextCount);
            };
            hooks.onImageFindReturnToPrevious = [this, context](int sourceBlockIndex, int targetBlockIndex) {
                if (context->suppressRepeatUi()) {
                    return;
                }
                emit m_engine->imageFindReturnToPrevious(sourceBlockIndex, targetBlockIndex);
            };
            hooks.onBlockProgress = [this, context](int i, BlockProgressKind kind) {
                if (context->suppressRepeatUi()) {
                    return;
                }
                emit m_engine->blockProgress(i, kind);
            };
            hooks.onBlockMatchResult = [this, context](int i,
                                              double matchThreshold,
                                              double confidence,
                                              const QPixmap& matchPreview,
                                              bool matched,
                                              bool hasClientPoint,
                                              int clientX,
                                              int clientY) {
                if (context->suppressRepeatUi()) {
                    return;
                }
                emit m_engine->blockMatchResult(
                    i, matchThreshold, confidence, matchPreview, matched, hasClientPoint, clientX, clientY);
            };
            hooks.onPointerFeedbackAtClientPoint = [this, context](int clientX, int clientY) {
                if (context->suppressRepeatUi()) {
                    return;
                }
                emit m_engine->pointerFeedbackAtClientPoint(clientX, clientY);
            };

            const bool workerFastRepeat = context->hasWorkerFastRepeat();
#ifdef _WIN32
            bool timerPeriodRaised = false;
            const auto setHighResolutionTimer = [&](bool enabled) {
                if (enabled) {
                    if (!timerPeriodRaised) {
                        timeBeginPeriod(1);
                        timerPeriodRaised = true;
                    }
                } else if (timerPeriodRaised) {
                    timeEndPeriod(1);
                    timerPeriodRaised = false;
                }
            };
#else
            const auto setHighResolutionTimer = [](bool) {};
#endif
            if (workerFastRepeat) {
                setHighResolutionTimer(context->workerFastRepeatDelayMs() <= 0);
            } else {
                setHighResolutionTimer(true);
            }

            int workerFastRepeatPass = 0;
            for (;;) {
                DiagnosticHub::touchWorkerHeartbeat();
                if (workerFastRepeatPass > 0) {
                    context->setSuppressRepeatUi(true);
                }

                WorkflowRunProfiler::recordLoopBegin(workerFastRepeatPass);

                const auto loopStart = std::chrono::steady_clock::now();
                const WorkflowRunResult runResult = WorkflowRunner::run(*workflow, *context, &hooks);
                const auto loopEnd = std::chrono::steady_clock::now();
                const auto loopElapsedMs = std::chrono::duration_cast<std::chrono::milliseconds>(
                    loopEnd - loopStart);
                const auto loopElapsedUs =
                    std::chrono::duration_cast<std::chrono::microseconds>(loopEnd - loopStart).count();

                WorkflowRunProfiler::recordLoopEnd(workerFastRepeatPass, loopElapsedUs);

                overallSuccess = runResult.success;
                finalMessage = QString::fromStdString(runResult.message);

                context->restoreRunHeldInput();

                if (workerFastRepeat) {
                    context->notifyWorkerFastRepeatIteration(
                        runResult.success,
                        static_cast<std::int64_t>(loopElapsedMs.count()),
                        runResult.message);
                }

                if (!workerFastRepeat || context->shouldStop()
                    || !context->shouldContinueWorkerFastRepeat(runResult.success)) {
                    break;
                }

                const int delayMs = context->workerFastRepeatDelayMs();
                if (workerFastRepeat) {
                    setHighResolutionTimer(delayMs <= 0);
                }
                if (delayMs > 0) {
                    if (workerFastRepeat) {
                        setHighResolutionTimer(false);
                    }
                    const auto sleepStart = std::chrono::steady_clock::now();
                    if (!context->interruptibleSleepMs(delayMs)) {
                        break;
                    }
                    const auto sleepUs = std::chrono::duration_cast<std::chrono::microseconds>(
                        std::chrono::steady_clock::now() - sleepStart)
                                             .count();
                    WorkflowRunProfiler::recordLoopIntervalSleep(delayMs, sleepUs);
                }
                if (context->shouldStop()) {
                    break;
                }

                context->prepareNextWorkerRepeatIteration();
                ++workerFastRepeatPass;
            }

            context->clearWorkerFastRepeatCallbacks();
            InputSimulator::setActiveExecutionContext(nullptr);
            setHighResolutionTimer(false);

            bool emitFinished = false;
            const bool pendingSuccess = overallSuccess;
            const QString pendingMessage = finalMessage;
            {
                QMutexLocker lock(&m_engine->m_mutex);
                m_engine->m_activeContext.reset();
                m_engine->m_running = false;
                emitFinished = m_engine->m_hasPendingResult;
                m_engine->m_hasPendingResult = false;
                m_engine->m_pendingSuccess = pendingSuccess;
                m_engine->m_pendingMessage = pendingMessage;
            }

            if (emitFinished) {
                QMetaObject::invokeMethod(
                    m_engine,
                    [this, pendingSuccess, pendingMessage]() {
                        emit m_engine->finished(pendingSuccess, pendingMessage);
                    },
                    Qt::QueuedConnection);
            }
        }
        DiagnosticHub::clearWorkerLabel();
    }

private:
    WorkflowEngine* m_engine = nullptr;
};

WorkflowEngine::WorkflowEngine(QObject* parent)
    : QObject(parent) {
    qRegisterMetaType<BlockProgressKind>("BlockProgressKind");
    qRegisterMetaType<std::shared_ptr<Workflow>>("std::shared_ptr<Workflow>");
    qRegisterMetaType<std::shared_ptr<ExecutionContext>>("std::shared_ptr<ExecutionContext>");
}

WorkflowEngine::~WorkflowEngine() {
    stopAndWaitBounded(250);
}

bool WorkflowEngine::isRunning() const {
    QMutexLocker lock(&m_mutex);
    return m_running;
}

void WorkflowEngine::ensureWorkerRunning() {
    if (m_worker != nullptr) {
        return;
    }

    m_worker = new Worker(this);
    m_worker->start();
}

void WorkflowEngine::run(std::shared_ptr<Workflow> workflow, std::shared_ptr<ExecutionContext> context) {
    QMutexLocker lock(&m_mutex);
    if (m_running || !workflow || !context) {
        return;
    }

    m_running = true;
    m_hasPendingResult = true;
    m_queuedWorkflow = std::move(workflow);
    m_queuedContext = std::move(context);
    m_queuedPreparer = nullptr;
    m_hasQueuedJob = true;
    ensureWorkerRunning();
    m_jobReady.wakeOne();
}

void WorkflowEngine::runPrepared(WorkflowRunPreparer preparer) {
    if (!preparer) {
        return;
    }

    QMutexLocker lock(&m_mutex);
    if (m_running) {
        return;
    }

    m_running = true;
    m_hasPendingResult = true;
    m_queuedWorkflow.reset();
    m_queuedContext.reset();
    m_queuedPreparer = std::move(preparer);
    m_hasQueuedJob = true;
    ensureWorkerRunning();
    m_jobReady.wakeOne();
}

void WorkflowEngine::stop() {
    std::shared_ptr<ExecutionContext> context;
    {
        QMutexLocker lock(&m_mutex);
        context = m_activeContext;
    }
    if (context) {
        context->requestStop();
    }
}

void WorkflowEngine::stopAndWait(int timeoutMs) {
    const int boundedMs = timeoutMs > 0 ? timeoutMs : 5000;
    stopAndWaitBounded(boundedMs);
}

bool WorkflowEngine::hasLiveWorker() const {
    QMutexLocker lock(&m_mutex);
    return m_worker != nullptr && m_worker->isRunning();
}

bool WorkflowEngine::stopAndWaitBounded(int timeoutMs) {
    stop();

    QThread* worker = nullptr;
    {
        QMutexLocker lock(&m_mutex);
        m_workerShutdown = true;
        m_jobReady.wakeAll();
        worker = m_worker;
    }

    if (worker && worker->isRunning()) {
        const int boundedMs = timeoutMs > 0 ? timeoutMs : 250;
        if (!worker->wait(boundedMs) && worker->isRunning()) {
            QObject::connect(worker, &QThread::finished, worker, &QObject::deleteLater);
            QMutexLocker lock(&m_mutex);
            m_worker = nullptr;
            m_workerShutdown = false;
            m_hasQueuedJob = false;
            m_queuedWorkflow.reset();
            m_queuedContext.reset();
            m_queuedPreparer = nullptr;
            m_activeContext.reset();
            m_running = false;
            m_hasPendingResult = false;
            return false;
        }
    }

    {
        QMutexLocker lock(&m_mutex);
        delete m_worker;
        m_worker = nullptr;
        m_workerShutdown = false;
        m_hasQueuedJob = false;
        m_queuedWorkflow.reset();
        m_queuedContext.reset();
        m_queuedPreparer = nullptr;
        m_activeContext.reset();
        m_running = false;
        m_hasPendingResult = false;
    }
    return true;
}
