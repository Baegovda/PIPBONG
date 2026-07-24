#pragma once

#include <QObject>

#include <atomic>
#include <functional>
#include <memory>
#include <thread>

class ExecutionContext;

/// Headless hold-mode runner for single-block KeyPress Tap workflows (e.g. Q/W/E/R hold macros).
/// Avoids WorkflowEngine / WorkflowRunner / per-loop Qt signal churn during multi-hold sessions.
class HoldKeyTapRunner : public QObject {
    Q_OBJECT

public:
    using ShouldContinueFn = std::function<bool()>;

    explicit HoldKeyTapRunner(QObject* parent = nullptr);
    ~HoldKeyTapRunner() override;

    bool isRunning() const;
    void stop();
    void run(int virtualKey,
             int intervalMs,
             ShouldContinueFn shouldContinue,
             const std::shared_ptr<ExecutionContext>& context);

signals:
    void finished(bool success, const QString& message);

private:
    struct Job;

    std::atomic<bool> m_running{false};
    std::shared_ptr<ExecutionContext> m_context;
    std::unique_ptr<std::thread> m_thread;
};

bool holdKeyTapWorkflowVirtualKey(const class Feature& feature, int& outVirtualKey);
