#include "core/workflow/WorkflowEngine.h"

#include "core/input/InputSimulator.h"
#include "core/workflow/Block.h"
#include "core/workflow/ExecutionContext.h"
#include "core/workflow/WorkflowRunner.h"
#include "ui/OpenCvQtUtil.h"

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
            context->setLogCallback([this](const std::string& message) {
                emit m_engine->logMessage(QString::fromStdString(message));
            });

            context->beginRunKeyboardSessionIfNeeded();
            InputSimulator::setActiveExecutionContext(context.get());

            bool overallSuccess = true;
            QString finalMessage = QStringLiteral("완료");

            WorkflowRunHooks hooks;
            hooks.onBlockStarted = [this](int i, const std::string& summary) {
                emit m_engine->blockStarted(i, QString::fromStdString(summary));
            };
            hooks.onBlockFinished = [this](int i, bool success, const std::string& message, qint64 durationMs,
                                           int64_t imageFindMatchDurationMs, int imageFindPollAttempts) {
                emit m_engine->blockFinished(i,
                                             success,
                                             QString::fromStdString(message),
                                             durationMs,
                                             static_cast<qint64>(imageFindMatchDurationMs),
                                             imageFindPollAttempts);
            };
            hooks.onBlockImageFindAttempt = [this](int blockIndex,
                                                  int attemptCount,
                                                  double matchThreshold,
                                                  double detectedConfidence,
                                                  bool matched) {
                emit m_engine->blockImageFindAttempt(
                    blockIndex, attemptCount, matchThreshold, detectedConfidence, matched);
            };
            hooks.onImageFindFailureHandling = [this](int blockIndex,
                                                       int returnToPreviousCount,
                                                       int retryAfterNextCount) {
                emit m_engine->imageFindFailureHandling(
                    blockIndex, returnToPreviousCount, retryAfterNextCount);
            };
            hooks.onImageFindReturnToPrevious = [this](int sourceBlockIndex, int targetBlockIndex) {
                emit m_engine->imageFindReturnToPrevious(sourceBlockIndex, targetBlockIndex);
            };
            hooks.onBlockProgress = [this](int i, BlockProgressKind kind) {
                emit m_engine->blockProgress(i, kind);
            };
            hooks.onBlockMatchResult = [this](int i,
                                              double matchThreshold,
                                              double confidence,
                                              const QPixmap& matchPreview,
                                              bool matched,
                                              bool hasClientPoint,
                                              int clientX,
                                              int clientY) {
                emit m_engine->blockMatchResult(
                    i, matchThreshold, confidence, matchPreview, matched, hasClientPoint, clientX, clientY);
            };
            hooks.onPointerFeedbackAtClientPoint = [this](int clientX, int clientY) {
                emit m_engine->pointerFeedbackAtClientPoint(clientX, clientY);
            };

            const WorkflowRunResult runResult = WorkflowRunner::run(*workflow, *context, &hooks);
            overallSuccess = runResult.success;
            finalMessage = QString::fromStdString(runResult.message);

            context->restoreRunHeldInput();
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
        worker->wait(timeoutMs);
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
