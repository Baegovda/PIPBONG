#pragma once

#include <QFrame>

class QLabel;
class QWidget;

class WorkflowRunStatusBar : public QFrame {
    Q_OBJECT
public:
    explicit WorkflowRunStatusBar(QWidget* parent = nullptr);

    void setFeatureName(const QString& name);
    void setLoopTiming(int loopNumber, qint64 elapsedMs, qint64 averageMs, bool success);
    void clearLoopTiming();

private:
    void applyTerminalChrome();
    void refreshStatsVisibility();

    QLabel* m_promptLabel = nullptr;
    QLabel* m_captionLabel = nullptr;
    QLabel* m_featureNameLabel = nullptr;
    QWidget* m_statsRow = nullptr;
    QLabel* m_loopChip = nullptr;
    QLabel* m_lastChip = nullptr;
    QLabel* m_averageChip = nullptr;
    QLabel* m_statusChip = nullptr;
    bool m_hasLoopTiming = false;
};
