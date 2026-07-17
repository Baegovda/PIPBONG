#pragma once

#include "model/FeatureRunMode.h"

#include <QFrame>

class QLabel;
class QToolButton;
class QWidget;

class WorkflowRunStatusBar : public QFrame {
    Q_OBJECT
public:
    explicit WorkflowRunStatusBar(QWidget* parent = nullptr);

    void setFeatureName(const QString& name);
    void setRunMode(FeatureRunMode mode, int repeatCount);
    void clearRunMode();
    void setRunButtonState(bool showStop, bool enabled, const QString& disabledToolTip = QString());
    void setLoopTiming(int loopNumber, qint64 elapsedMs, qint64 averageMs, bool success);
    void clearLoopTiming();

signals:
    void runToggleRequested();

private:
    void applyTerminalChrome();
    void refreshStatsVisibility();
    void refreshRunButtonChrome();

    QToolButton* m_runButton = nullptr;
    QLabel* m_captionLabel = nullptr;
    QLabel* m_featureNameLabel = nullptr;
    QLabel* m_modeChip = nullptr;
    QWidget* m_statsRow = nullptr;
    QLabel* m_loopChip = nullptr;
    QLabel* m_lastChip = nullptr;
    QLabel* m_averageChip = nullptr;
    QLabel* m_statusChip = nullptr;
    bool m_hasLoopTiming = false;
    bool m_runButtonShowStop = false;
    bool m_runButtonEnabled = false;
};
