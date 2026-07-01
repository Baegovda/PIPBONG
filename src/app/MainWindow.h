#pragma once

#include "app/FeatureRunSession.h"
#include "core/workflow/ExecutionContext.h"
#include "model/UserInputInterruptMode.h"
#include "ui/BlockListWidget.h"

#include <QByteArray>
#include <QElapsedTimer>
#include <QMainWindow>
#include <QPointer>
#include <QSet>
#include <map>
#include <memory>
#include <string>

class Feature;

class QPixmap;

class FeatureListPanel;
class WorkflowEditorPanel;
class WorkflowEngine;
class Project;
class HotkeyManager;
class Feature;
class UiStateManager;
class QCheckBox;
class QCloseEvent;
class QShowEvent;
class QPlainTextEdit;
class QTimer;
class QPushButton;
class QLabel;
class QMenuBar;
class QSplitter;
class TargetWindowDetailPanel;
class CustomTitleBar;
class CalculatorDialog;

class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    explicit MainWindow(QWidget* parent = nullptr);
    ~MainWindow() override;

protected:
    void closeEvent(QCloseEvent* event) override;
    void showEvent(QShowEvent* event) override;
#if defined(Q_OS_WIN)
    bool nativeEvent(const QByteArray& eventType, void* message, qintptr* result) override;
#endif

private slots:
    void onNewProject();
    void onOpenProject();
    void onSaveProject();
    void onSaveProjectAs();
    void onFeatureSelectionChanged();
    void onProjectModified();
    void onFeatureRunRequested(const QString& featureId);
    void onStopWorkflow();
    void onExitRequested();
    void onCheckForUpdates();
    void onProgramSettings();
    void onCalculator();
    void onAlwaysOnTopToggled(bool checked);
    void onPickTargetWindow();
    void onShowTargetWindow();
    void onEngineLog(const QString& message);
    void onEngineStarted();
    void onEngineFinished(bool success, const QString& message);
    void onBlockStarted(int index, const QString& summary);
    void onBlockProgress(int index, BlockProgressKind kind);
    void onBlockMatchResult(int index,
                            double matchThreshold,
                            double confidence,
                            const QPixmap& matchPreview,
                            bool matched,
                            bool hasClientPoint,
                            int clientX,
                            int clientY);
    void onPointerFeedbackAtClientPoint(int clientX, int clientY);
    void onBlockFinished(int index, bool success, const QString& message, qint64 durationMs,
                         qint64 imageFindMatchDurationMs, int imageFindPollAttempts);
    void onBlockImageFindAttempt(int index,
                                 int attemptCount,
                                 double matchThreshold,
                                 double detectedConfidence,
                                 bool matched);
    void onHotkeyTriggered(const QString& featureId);
    void onHotkeyHoldStarted(const QString& featureId);
    void onHotkeyHoldEnded(const QString& featureId);
    void syncHotkeys();

private:
    void setupUi();
    void setupMenus();
    void setupUiState();
    void connectSignals();
    void updateRunUiState();
    void connectSessionEngine(FeatureRunSession& session);
    FeatureRunSession* sessionFor(const std::string& featureId);
    const FeatureRunSession* sessionFor(const std::string& featureId) const;
    FeatureRunSession* sessionForEngine(const QObject* sender);
    bool isFeatureRunning(const std::string& featureId) const;
    bool hasAnyRunningSession() const;
    QSet<QString> runningFeatureIds() const;
    QString featureDisplayName(const std::string& featureId) const;
    void appendSessionLog(const FeatureRunSession& session, const QString& message);
    void stopFeatureRun(const std::string& featureId);
    void stopAllSessions();
    void appendLog(const QString& message);
    bool maybeSave();
    void loadProjectFromFile(const QString& path);
    void updateWindowTitle();
    void syncWindowTitleDisplay();
    void refreshWorkflowEditor();
    void scheduleAutoSave();
    void autoSaveProject();
    bool ensureProjectFilePath();
    void prepareForShutdown();
    void startFeatureRun(Feature* feature, bool fromHotkey = false);
    bool tryBeginFirstTemplateRoiEdit(FeatureRunSession& session, Feature* feature);
    void selectRunningFeatureForDisplay(Feature* feature);
    void launchWorkflowRun(FeatureRunSession& session, Feature* feature, bool repeatIteration = false);
    void ensureRunSessionResources(FeatureRunSession& session, Feature* feature, bool refreshWorkflow = false);
    void syncRunSessionContext(FeatureRunSession& session);
    void applyFeatureRunPoliciesToContext(FeatureRunSession& session, Feature* feature);
    bool shouldLogRunDetails(const FeatureRunSession& session) const;
    void continueRepeatSession(FeatureRunSession& session, Feature* feature, bool success, const QString& message);
    bool shouldContinueRunSession(const FeatureRunSession& session, Feature* feature) const;
    void finishRunSession(const std::string& featureId, bool success, const QString& message);
    void scheduleHoldRepeat(FeatureRunSession& session, Feature* feature, bool success, const QString& message);
    void runFeature(Feature* feature);
    bool isDisplayedRunningFeature(const FeatureRunSession* session) const;
    void applyRunningBlockVisuals(FeatureRunSession& session,
                                  int index,
                                  BlockListWidget::ExecutionHighlight highlight);
    void updateTargetWindowDetails();
    std::wstring currentTargetWindowTitleW() const;
    void syncTargetWindowTitleToCapture();
    void saveSelectedFeaturePreference();
    void restoreSelectedFeaturePreference();
    QString selectedFeaturePreferenceKey() const;
    void scheduleRunWarmup();
    void onEngineSessionPrepared(std::shared_ptr<Workflow> workflow, std::shared_ptr<ExecutionContext> context);
    void applyAlwaysOnTop(bool enabled);
    void restoreAlwaysOnTopPreference();
    QString alwaysOnTopPreferenceKey() const;
    void showTransientStatus(const QString& message, int timeoutMs = 3000);
    void setPersistentStatus(const QString& message);
    void clearStatusMessage();
    void refreshTitleBarStatus();
    void logLoopCompletion(FeatureRunSession& session, bool success, const QString& message);
    void     syncLoopTimingToWorkflowEditor(const FeatureRunSession* session);

    void onUserInputInterrupt(const std::string& featureId);
    void syncUserInputInterruptForSession(FeatureRunSession& session, Feature* feature);

    std::unique_ptr<Project> m_project;
    QString m_projectFilePath;
    QString m_baseWindowTitle;

    FeatureListPanel* m_featureList = nullptr;
    WorkflowEditorPanel* m_workflowEditor = nullptr;
    QSplitter* m_mainHorizontalSplitter = nullptr;
    QSplitter* m_mainVerticalSplitter = nullptr;
    QSplitter* m_bottomHorizontalSplitter = nullptr;
    QPlainTextEdit* m_logView = nullptr;
    QPushButton* m_pickWindowButton = nullptr;
    QPushButton* m_showTargetWindowButton = nullptr;
    QCheckBox* m_alwaysOnTopCheck = nullptr;
    QPushButton* m_exitButton = nullptr;
    QPushButton* m_calculatorButton = nullptr;
    QPushButton* m_settingsButton = nullptr;
    TargetWindowDetailPanel* m_targetWindowDetailPanel = nullptr;
    CustomTitleBar* m_titleBar = nullptr;

    HotkeyManager* m_hotkeyManager = nullptr;
    UiStateManager* m_uiState = nullptr;
    QTimer* m_autoSaveTimer = nullptr;
    QTimer* m_statusClearTimer = nullptr;
    QString m_persistentStatusMessage;
    QString m_transientStatusMessage;
    std::map<std::string, FeatureRunSession> m_runSessions;
    QPointer<CalculatorDialog> m_calculatorDialog;
    bool m_modified = false;
};
