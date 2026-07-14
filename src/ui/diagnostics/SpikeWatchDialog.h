#pragma once

#include "core/diagnostics/CpuCulpritAnalyzer.h"
#include "core/diagnostics/CpuMonitorTypes.h"

#include "ui/diagnostics/ProcessIconCache.h"

#include <QDialog>
#include <functional>

class CpuMonitorWorker;
class DragAdjustDoubleSpinBox;
class DragAdjustSpinBox;
class HintLabel;
class QLabel;
class QPushButton;
class QTableWidget;
class QTextEdit;
class QThread;

class SpikeWatchDialog : public QDialog {
    Q_OBJECT
public:
    explicit SpikeWatchDialog(QWidget* parent = nullptr);
    ~SpikeWatchDialog() override;

    void setFeatureRunningCallback(std::function<bool()> callback);

protected:
    void closeEvent(QCloseEvent* event) override;
    void showEvent(QShowEvent* event) override;

private slots:
    void onStartClicked();
    void onStopClicked();
    void onClearLogClicked();
    void onCopyLogClicked();
    void onSampleReady(CpuSampleSnapshot snapshot);
    void onSpikeDetected(CpuSpikeEvent event);
    void onMonitoringStopped();

private:
    void setupUi();
    void loadPersistedState();
    void persistState();
    void setMonitoringUiActive(bool active);
    void stopMonitoringAndWait();
    CpuSpikeDetectorConfig buildDetectorConfig() const;
    void appendSpikeLog(const CpuSpikeEvent& event);
    QString formatSpikeEventText(const CpuSpikeEvent& event) const;
    static QString triggerKindDisplayName(SpikeTriggerKind kind);
    void refreshCulpritTable();
    void updateCulpritSummary();
    QString formatCulpritReportPlainText() const;

    std::function<bool()> m_featureRunningCallback;

    QPushButton* m_startButton = nullptr;
    QPushButton* m_stopButton = nullptr;
    QPushButton* m_clearLogButton = nullptr;
    QPushButton* m_copyLogButton = nullptr;

    DragAdjustSpinBox* m_intervalSpin = nullptr;
    DragAdjustDoubleSpinBox* m_systemThresholdSpin = nullptr;
    DragAdjustDoubleSpinBox* m_processThresholdSpin = nullptr;
    DragAdjustSpinBox* m_topNSpin = nullptr;
    DragAdjustDoubleSpinBox* m_deltaMarginSpin = nullptr;

    QLabel* m_summaryLabel = nullptr;
    QTableWidget* m_processTable = nullptr;
    QLabel* m_culpritSummaryLabel = nullptr;
    QTableWidget* m_culpritTable = nullptr;
    QTextEdit* m_eventLog = nullptr;
    HintLabel* m_hintLabel = nullptr;

    CpuCulpritAnalyzer m_culpritAnalyzer;

    QThread* m_workerThread = nullptr;
    CpuMonitorWorker* m_worker = nullptr;
    ProcessIconCache m_iconCache;

    bool m_monitoringActive = false;
    QString m_eventLogPlainText;
};
