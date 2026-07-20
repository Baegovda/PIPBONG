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

    void setProfileName(const QString& name);
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
    void refreshModeChipChrome();
    void refreshBreadcrumb();
    QString runModeText(FeatureRunMode mode, int repeatCount) const;
    QString runModeToolTip(FeatureRunMode mode, int repeatCount) const;

    QToolButton* m_runButton = nullptr;
    QLabel* m_profileLabel = nullptr;
    QLabel* m_breadcrumbSep1 = nullptr;
    QLabel* m_featureNameLabel = nullptr;
    QLabel* m_breadcrumbSep2 = nullptr;
    QLabel* m_modeChip = nullptr;
    QWidget* m_statsRow = nullptr;
    QLabel* m_loopChip = nullptr;
    QLabel* m_lastChip = nullptr;
    QLabel* m_averageChip = nullptr;
    QLabel* m_statusChip = nullptr;
    bool m_hasLoopTiming = false;
    bool m_runButtonShowStop = false;
    bool m_runButtonEnabled = false;
    QString m_profileName;
    FeatureRunMode m_runMode = FeatureRunMode::RepeatCount;
    int m_repeatCount = 1;
    bool m_hasRunMode = false;
};
