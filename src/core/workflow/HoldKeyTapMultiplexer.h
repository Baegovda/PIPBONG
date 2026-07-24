#pragma once

#include <QObject>
#include <QString>

#include <atomic>
#include <functional>
#include <memory>
#include <string>

class ExecutionContext;

/// Single worker thread multiplexing all hold-mode KeyPress Tap lanes (Q/W/E/R macros).
/// Replaces per-session std::thread runners that contended on SendInput and flooded the GUI queue.
class HoldKeyTapMultiplexer : public QObject {
    Q_OBJECT

public:
    using ShouldContinueFn = std::function<bool()>;

    explicit HoldKeyTapMultiplexer(QObject* parent = nullptr);
    ~HoldKeyTapMultiplexer() override;

    bool isLaneActive(const std::string& featureId) const;
    bool hasAnyActiveLane() const;

    void startLane(const std::string& featureId,
                   int virtualKey,
                   int intervalMs,
                   ShouldContinueFn shouldContinue,
                   const std::shared_ptr<ExecutionContext>& context);

    void stopLane(const std::string& featureId);
    void stopAll();

signals:
    void laneFinished(const QString& featureId, bool success, const QString& message);

private:
    struct Impl;
    std::unique_ptr<Impl> m_impl;
};

bool holdKeyTapWorkflowVirtualKey(const class Feature& feature, int& outVirtualKey);
