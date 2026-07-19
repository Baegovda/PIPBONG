#include "core/workflow/WorkflowEngine.h"

#include "core/input/InputSimulator.h"
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
#ifdef _WIN32
        timeBeginPeriod(1);
#endif
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
                if (context->suppressRepeatUi()) {
                    return;
                }
                emit m_engine->blockStarted(i, QString::fromStdString(summary));
            };
            hooks.onBlockFinished = [this, context](int i,
                                                     bool success,
                                                     const std::string& message,
                                                     qint64 durationMs,
                                                     int64_t imageFindMatchDurationMs,
                                                     int imageFindPollAttempts) {
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
            int workerFastRepeatPass = 0;
            for (;;) {
                if (workerFastRepeatPass > 0) {
                    context->setSuppressRepeatUi(true);
                }

                const auto loopStart = std::chrono::steady_clock::now();
                const WorkflowRunResult runResult = WorkflowRunner::run(*workflow, *context, &hooks);
                const auto loopElapsedMs = std::chrono::duration_cast<std::chrono::milliseconds>(
                    std::chrono::steady_clock::now() - loopStart);

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
                if (delayMs > 0 && !context->interruptibleSleepMs(delayMs)) {
                    break;
                }
                if (context->shouldStop()) {
                    break;
                }

                context->prepareNextWorkerRepeatIteration();
                ++workerFastRepeatPass;
            }

            context->clearWorkerFastRepeatCallbacks();
            InputSimulator::setActiveExecutionContext(nullptr);

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
#ifdef _WIN32
        timeEndPeriod(1);
#endif
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
    stopAndWait();
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
    stop();

    QThread* worker = nullptr;
    {
        QMutexLocker lock(&m_mutex);
        m_workerShutdown = true;
        m_jobReady.wakeAll();
        worker = m_worker;
    }

    if (worker && worker->isRunning()) {
        const int boundedMs = timeoutMs > 0 ? timeoutMs : 5000;
        if (!worker->wait(boundedMs) && worker->isRunning()) {
            worker->wait();
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
