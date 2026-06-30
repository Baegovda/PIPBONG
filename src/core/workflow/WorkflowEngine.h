#pragma once

#include "core/workflow/Workflow.h"
#include "core/workflow/ExecutionContext.h"

#include <QMutex>
#include <QObject>
#include <QPixmap>
#include <QString>
#include <QWaitCondition>

#include <functional>
#include <memory>

struct PreparedWorkflowRun {
    std::shared_ptr<Workflow> workflow;
    std::shared_ptr<ExecutionContext> context;
};

using WorkflowRunPreparer = std::function<PreparedWorkflowRun()>;

class WorkflowEngine : public QObject {
    Q_OBJECT
public:
    explicit WorkflowEngine(QObject* parent = nullptr);
    ~WorkflowEngine() override;

    bool isRunning() const;

public slots:
    void run(std::shared_ptr<Workflow> workflow, std::shared_ptr<ExecutionContext> context);
    void runPrepared(WorkflowRunPreparer preparer);
    void stop();
    void stopAndWait(int timeoutMs = 5000);

signals:
    void sessionPrepared(std::shared_ptr<Workflow> workflow, std::shared_ptr<ExecutionContext> context);
    void started();
    void blockStarted(int index, const QString& summary);
    void blockFinished(int index, bool success, const QString& message, qint64 durationMs,
                       qint64 imageFindMatchDurationMs, int imageFindPollAttempts);
    void blockImageFindAttempt(int index,
                               int attemptCount,
                               double matchThreshold,
                               double detectedConfidence,
                               bool matched);
    void blockProgress(int index, BlockProgressKind kind);
    void blockMatchResult(int index,
                          double matchThreshold,
                          double confidence,
                          const QPixmap& matchPreview,
                          bool matched,
                          bool hasClientPoint,
                          int clientX,
                          int clientY);
    void pointerFeedbackAtClientPoint(int clientX, int clientY);
    void logMessage(const QString& message);
    void finished(bool success, const QString& message);

private:
    void ensureWorkerRunning();

    class Worker;
    friend class Worker;

    mutable QMutex m_mutex;
    QWaitCondition m_jobReady;
    Worker* m_worker = nullptr;
    std::shared_ptr<Workflow> m_queuedWorkflow;
    std::shared_ptr<ExecutionContext> m_queuedContext;
    WorkflowRunPreparer m_queuedPreparer;
    std::shared_ptr<ExecutionContext> m_activeContext;
    bool m_hasQueuedJob = false;
    bool m_workerShutdown = false;
    bool m_running = false;
    bool m_hasPendingResult = false;
    bool m_pendingSuccess = false;
    QString m_pendingMessage;
};
