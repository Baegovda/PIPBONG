#pragma once

#include "app/FeatureRunSession.h"
#include "app/ProfileManager.h"
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

#ifdef _WIN32
#include <windows.h>
#endif

class Feature;

class QPixmap;
class QIcon;
class QToolButton;

class FeatureListPanel;
class WorkflowEditorPanel;
class WorkflowEngine;
class Project;
class HotkeyManager;
class Feature;
class FeatureLibraryManager;
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
class QSystemTrayIcon;
class QMenu;
class QListWidget;
class QMimeData;
class ProfileListWidget;
class TargetWindowDetailPanel;
class CustomTitleBar;
class CalculatorDialog;
class UpdateChecker;

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
    void onFeatureEnabledChanged(const QString& featureId, bool enabled);
    void onSaveFeatureToLibraryRequested(const QString& featureId);
    void onImportFeatureFromLibraryRequested();
    void onImportLibraryEntryRequested(const QString& entryId);
    void onDeleteLibraryEntryRequested(const QString& entryId);
    void onStopWorkflow();
    void onExitRequested();
    void onCheckForUpdates();
    void onUpdateButtonClicked();
    void onUpdateCheckFinished(bool success, bool updateAvailable, const QString& errorMessage);
    void onReadyToRestartForUpdate();
    void runSilentUpdateCheck();
    void refreshUpdateButtonState();
    void maybeStartAutomaticUpdate();
    void onProgramSettings();
    void onCalculator();
    void onAlwaysOnTopToggled(bool checked);
    void onPickTargetWindow();
    void onPickTargetWindowFromList();
    void onShowTargetWindow();
    void onPinTargetWindowCenterToggled(bool checked);
    void onProfileSelectionChanged();
    void onAddProfile();
    void onRenameProfile();
    void onDeleteProfile();
    void syncTargetWindowCenterPin();
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
    void onImageFindFailureHandling(int index, int returnToPreviousCount, int retryAfterNextCount);
    void onImageFindReturnToPrevious(int sourceIndex, int targetIndex);
    void onHotkeyTriggered(const QString& featureId);
    void onHotkeyHoldStarted(const QString& featureId);
    void onHotkeyHoldEnded(const QString& featureId);
    void syncHotkeys();

private:
    void setupUi();
    void setupUpdateChecker();
    void applyUpdateCheckInterval();
    void engageFeatureMouseLock(FeatureRunSession& session);
    void reconcileMouseLocksFromRunningSessions();
    bool isFeatureSessionActive(const FeatureRunSession& session) const;
    void captureFeatureMouseLockPosition(FeatureRunSession& session);
    static bool hasFeatureMouseLock(const FeatureRunSession& session);
    void scheduleMouseLockPositionSync();
    void syncMouseLockPositions();
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
    void loadActiveProfile();
    void refreshProfileList();
    void refreshFeatureLibraryPanel();
    bool importLibraryEntry(const QString& entryId);
    bool importLibraryEntryToProfile(const QString& entryId,
                                     const QString& profileId,
                                     int insertIndex);
    bool saveFeatureToLibraryFromDrag(const QString& featureId, const QString& profileId);
    bool moveFeatureBetweenProfiles(const QString& featureId,
                                    const QString& sourceProfileId,
                                    const QString& targetProfileId,
                                    int insertIndex);
    bool removeFeatureFromProfile(const QString& profileId, const QString& featureId);
    bool canTransferFeatures() const;
    void onFeatureDroppedOnFeatureList(const QMimeData* mime, int insertIndex);
    void onFeatureDroppedOnLibrary(const QMimeData* mime);
    void onLibraryEntriesReordered(int fromRow, int toRow);
    void onFeatureDroppedOnProfile(const QString& targetProfileId, const QMimeData* mime);
    void syncProfileListSelection();
    bool switchToProfile(const QString& profileId, bool automatic = false);
    void saveActiveProfileSettings();
    void syncProfileToForegroundWindow();
    void updateWindowTitle();
    void syncWindowTitleDisplay();
    void refreshWorkflowEditor();
    void scheduleAutoSave();
    void autoSaveProject();
    bool ensureProjectFilePath();
    void prepareForShutdown();
    void setupTrayIcon();
    void applyCloseToTrayPolicy();
    void hideToTray();
    void showFromTray();
    void requestApplicationQuit();
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
    void launchTriggerMonitor(FeatureRunSession& session, Feature* feature, bool firstSessionStart);
    void launchTriggerActionRun(FeatureRunSession& session, Feature* feature);
    void scheduleTriggerCooldown(FeatureRunSession& session, Feature* feature);
    void handleTriggerEngineFinished(FeatureRunSession& session,
                                     Feature* feature,
                                     bool success,
                                     const QString& message);
    void pauseOtherSessionsForTrigger(FeatureRunSession& triggerSession);
    void resumePreemptedSessionsForTrigger(FeatureRunSession& triggerSession);
    void runFeature(Feature* feature);
    bool isDisplayedRunningFeature(const FeatureRunSession* session) const;
    void applyRunningBlockVisuals(FeatureRunSession& session,
                                  int index,
                                  BlockListWidget::ExecutionHighlight highlight);
    void updateTargetWindowDetails();
    void updateTargetWindowControlsForActiveProfile();
#ifdef _WIN32
    void commitActiveProfileTargetWindow(HWND hwnd, const QString& title);
#endif
    bool isActiveDefaultProfile() const;
    std::wstring currentTargetWindowTitleW() const;
    void syncTargetWindowTitleToCapture();
    void saveSelectedFeaturePreference();
    void restoreSelectedFeaturePreference();
    QString selectedFeaturePreferenceKey() const;
    void scheduleRunWarmup();
    void onEngineSessionPrepared(std::shared_ptr<Workflow> workflow, std::shared_ptr<ExecutionContext> context);
    void applyAlwaysOnTop(bool enabled);
    void restoreAlwaysOnTopPreference();
    void applyTargetWindowCenterPin(bool enabled);
    QString alwaysOnTopPreferenceKey() const;
    void showTransientStatus(const QString& message, int timeoutMs = 3000);
    void setPersistentStatus(const QString& message);
    void clearStatusMessage();
    void refreshTitleBarStatus();
    void logLoopCompletion(FeatureRunSession& session, bool success, const QString& message);
    void     syncLoopTimingToWorkflowEditor(const FeatureRunSession* session);

    void onUserInputInterrupt(const std::string& featureId);
    void syncUserInputInterruptForSession(FeatureRunSession& session, Feature* feature);
    bool shouldSuppressFeatureHotkeyExecution() const;

    std::unique_ptr<Project> m_project;
    std::unique_ptr<ProfileManager> m_profileManager;
    QString m_projectFilePath;
    QString m_baseWindowTitle;

    FeatureListPanel* m_featureList = nullptr;
    QWidget* m_profilePanel = nullptr;
    ProfileListWidget* m_profileList = nullptr;
    QPushButton* m_addProfileButton = nullptr;
    QPushButton* m_renameProfileButton = nullptr;
    QPushButton* m_deleteProfileButton = nullptr;
    WorkflowEditorPanel* m_workflowEditor = nullptr;
    QSplitter* m_mainHorizontalSplitter = nullptr;
    QSplitter* m_mainVerticalSplitter = nullptr;
    QSplitter* m_bottomHorizontalSplitter = nullptr;
    QPlainTextEdit* m_logView = nullptr;
    QToolButton* m_pickWindowButton = nullptr;
    QToolButton* m_pickWindowListButton = nullptr;
    QToolButton* m_showTargetWindowButton = nullptr;
    QToolButton* m_pinTargetWindowCenterButton = nullptr;
    QCheckBox* m_alwaysOnTopCheck = nullptr;
    QPushButton* m_exitButton = nullptr;
    QPushButton* m_updateButton = nullptr;
    QPushButton* m_calculatorButton = nullptr;
    QPushButton* m_settingsButton = nullptr;
    TargetWindowDetailPanel* m_targetWindowDetailPanel = nullptr;
    CustomTitleBar* m_titleBar = nullptr;

    HotkeyManager* m_hotkeyManager = nullptr;
    UiStateManager* m_uiState = nullptr;
    std::unique_ptr<FeatureLibraryManager> m_featureLibraryManager;
    QTimer* m_autoSaveTimer = nullptr;
    QTimer* m_statusClearTimer = nullptr;
    QTimer* m_updateCheckTimer = nullptr;
    QTimer* m_mouseLockSyncTimer = nullptr;
    QTimer* m_targetWindowCenterPinTimer = nullptr;
    QTimer* m_profileAutoSwitchTimer = nullptr;
    UpdateChecker* m_updateChecker = nullptr;
    bool m_initialUpdateCheckDone = false;
    bool m_lastUpdateCheckWasSilent = false;
    bool m_autoUpdateDeferred = false;
    bool m_autoUpdateInstallStarted = false;
    QString m_persistentStatusMessage;
    QString m_transientStatusMessage;
    std::map<std::string, FeatureRunSession> m_runSessions;
    QPointer<CalculatorDialog> m_calculatorDialog;
    QSystemTrayIcon* m_trayIcon = nullptr;
    QMenu* m_trayMenu = nullptr;
    bool m_forceQuit = false;
    bool m_trayMinimizeNotified = false;
    bool m_modified = false;
    bool m_refreshingProfileList = false;
    QString m_pendingProfileSwitchId;
    int m_pendingProfileSwitchPolls = 0;
};
