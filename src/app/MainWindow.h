#pragma once

#include "app/FeatureRunSession.h"
#include "app/PointerFeedbackSettings.h"
#include "app/ProfileManager.h"
#include "core/workflow/ExecutionContext.h"
#include "model/UserInputInterruptMode.h"
#include "ui/BlockListWidget.h"
#include "ui/LogPanelWidget.h"

#include <QByteArray>
#include <QElapsedTimer>
#include <QMainWindow>
#include <QPointer>
#include <QSet>
#include <QMutex>
#include <atomic>
#include <map>
#include <memory>
#include <string>
#include <vector>

#ifdef _WIN32
#include <windows.h>
#endif

class Feature;

struct WorkerFastRepeatUiCoalesce {
    QMutex mutex;
    bool flushScheduled = false;
    int pendingIterations = 0;
    qint64 pendingTotalElapsedMs = 0;
    bool pendingLastSuccess = false;
    qint64 pendingLastElapsedMs = 0;
    QString pendingLastMessage;
};

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
class SpikeWatchDialog;
class MemoDialog;
class UpdateChecker;

class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    explicit MainWindow(QWidget* parent = nullptr);
    ~MainWindow() override;

    void ensureInitialWindowPlacement();
    void showPendingCrashReportIfAny();
    /// Refreshes main/sub target bindings in ScreenCapture before block editors or overlays open.
    void refreshCaptureTargetForEditing();

protected:
    void closeEvent(QCloseEvent* event) override;
    void showEvent(QShowEvent* event) override;
    bool eventFilter(QObject* watched, QEvent* event) override;
#if defined(Q_OS_WIN)
    bool nativeEvent(const QByteArray& eventType, void* message, qintptr* result) override;
#endif

private slots:
    void onNewProject();
    void onOpenProject();
    void onSaveProject();
    void onSaveProjectAs();
    void onImportProfilePackage();
    void onExportProfilePackage();
    void onFeatureSelectionChanged();
    void onProjectModified();
    void onFeatureRunRequested(const QString& featureId);
    void onFeatureEnabledChanged(const QString& featureId, bool enabled);
    void onSaveFeatureToLibraryRequested(const QString& featureId);
    void onImportFeatureFromLibraryRequested();
    void onImportLibraryEntriesRequested(const QStringList& entryIds);
    void onDeleteLibraryEntriesRequested(const QStringList& entryIds);
    void onLibraryEntrySelected(const QString& entryId);
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
    void onSpikeWatch();
    void onMemo();
    void onAlwaysOnTopToggled(bool checked);
    void onPickTargetWindow();
    void onPickMainTargetWindowFromList();
    void onPickSubTargetWindowFromList();
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
    struct GlobalUiHistorySnapshot {
        QString backupRootPath;
        QString reason;
    };

    void setupUi();
    void setupUpdateChecker();
    void applyUpdateCheckInterval();
    void engageFeatureMouseLock(FeatureRunSession& session);
    void reconcileMouseLocksFromRunningSessions();
    bool isFeatureSessionActive(const FeatureRunSession& session) const;
    void captureFeatureMouseLockPosition(FeatureRunSession& session);
    static bool hasFeatureMouseLock(const FeatureRunSession& session);
    bool isEarlyLoopMouseLockWindow(const FeatureRunSession& session) const;
    bool engageEarlyLoopMouseLockAtBestPoint(FeatureRunSession& session);
    void updateEarlyLoopMouseLockFromMatch(FeatureRunSession& session);
    void syncEarlyLoopMouseLock(FeatureRunSession& session);
    void releaseEarlyLoopMouseLockIfEngaged(FeatureRunSession& session);
    void handleEarlyLoopMouseLockBlockFailure(FeatureRunSession& session, int blockIndex);
    void scheduleMouseLockPositionSync();
    void syncMouseLockPositions();
    void setupMenus();
    void clampMainVerticalSplitterSizes();
    void setupUiState();
    void connectSignals();
    void updateRunUiState();
    void connectSessionEngine(FeatureRunSession& session);
    FeatureRunSession* sessionFor(const std::string& featureId);
    const FeatureRunSession* sessionFor(const std::string& featureId) const;
    FeatureRunSession* sessionForEngine(const QObject* sender);
    bool isFeatureRunning(const std::string& featureId) const;
    bool isFeatureInActiveWorkflowRun(const std::string& featureId) const;
    bool hasAnyRunningSession() const;
    bool hasAnyActiveWorkflowEngine() const;
    QSet<QString> activeWorkflowFeatureIds() const;
    QSet<QString> runningFeatureIds() const;
    QString featureDisplayName(const std::string& featureId) const;
    void appendSessionLog(const FeatureRunSession& session,
                          const QString& message,
                          LogLineKind kind = LogLineKind::Info);
    void stopFeatureRun(const std::string& featureId);
    void stopAllSessions();
    void stopRunningSessionsForUpdate();
    void stopAllSessionsForProfileSwitch();
    void stopSessionEngineForProfileSwitch(FeatureRunSession& session, Feature* feature);
    void abandonSessionEngine(FeatureRunSession& session);
    void pruneAbandonedEngines();
    void flushDeferredProfileSwitchIfIdle();
    bool hasTriggerMonitoringSessions() const;
    void appendLog(const QString& message, LogLineKind kind = LogLineKind::Info);
    bool maybeSave(bool quiet = false);
    void loadProjectFromFile(const QString& path, bool quiet = false);
    void loadActiveProfile(bool quiet = false);
    void refreshProfileList();
    void refreshFeatureLibraryPanel();
    bool importLibraryEntry(const QString& entryId);
    bool importLibraryEntries(const QStringList& entryIds);
    bool importLibraryEntryToProfile(const QString& entryId,
                                     const QString& profileId,
                                     int insertIndex,
                                     bool refreshListUi = true,
                                     QString* insertedFeatureId = nullptr);
    bool saveFeatureToLibraryFromDrag(const QString& featureId, const QString& profileId);
    bool copyFeatureBetweenProfiles(const QString& featureId,
                                    const QString& sourceProfileId,
                                    const QString& targetProfileId,
                                    int insertIndex);
    bool removeFeatureFromProfile(const QString& profileId, const QString& featureId);
    bool canTransferFeatures() const;
    void onFeatureDroppedOnFeatureList(const QMimeData* mime, int insertIndex);
    void onFeatureDroppedOnLibrary(const QMimeData* mime);
    void onLibraryEntriesReordered(int fromRow, int toRow);
    void onLibraryEntriesMultiReordered(const QList<int>& selectedRows, int insertIndex);
    void onFeatureDroppedOnProfile(const QString& targetProfileId, const QMimeData* mime);
    void syncProfileListSelection();
    void syncMemoDialogProfile();
    bool switchToProfile(const QString& profileId, bool automatic = false);
    void saveActiveProfileSettings();
    bool profileSettingsEqual(const ProgramSettings::ProfileSettings& a,
                              const ProgramSettings::ProfileSettings& b) const;
    void syncProfileToForegroundWindow();
    void updateWindowTitle();
    void syncWindowTitleDisplay();
    void refreshWorkflowEditor();
    void scheduleAutoSave();
    void autoSaveProject(bool quiet = false);
    void scheduleProfilePackageSeal();
    void sealActiveProfilePackage(bool synchronous = false);
    bool ensureProjectFilePath();
    void prepareForShutdown();
    void setupTrayIcon();
    void applyCloseToTrayPolicy();
    void hideToTray();
    void showFromTray();
    void requestApplicationQuit();
    void startFeatureRun(Feature* feature,
                         bool fromHotkey = false,
                         bool skipTargetActivationOnStart = false);
    bool tryBeginFirstTemplateRoiEdit(FeatureRunSession& session, Feature* feature);
    void selectRunningFeatureForDisplay(Feature* feature);
    void launchWorkflowRun(FeatureRunSession& session, Feature* feature, bool repeatIteration = false);
    void deferHoldSessionUiAfterStart(const std::string& featureId);
    void ensureRunSessionResources(FeatureRunSession& session, Feature* feature, bool refreshWorkflow = false);
    void syncRunSessionContext(FeatureRunSession& session);
    void applyFeatureRunPoliciesToContext(FeatureRunSession& session, Feature* feature);
    bool shouldLogRunDetails(const FeatureRunSession& session) const;
    void continueRepeatSession(FeatureRunSession& session, Feature* feature, bool success, const QString& message);
    bool shouldContinueRunSession(const FeatureRunSession& session, Feature* feature) const;
    void finishRunSession(const std::string& featureId, bool success, const QString& message);
    void scheduleRepeatIteration(FeatureRunSession& session,
                                 Feature* feature,
                                 bool success,
                                 const QString& message);
    void launchTriggerMonitor(FeatureRunSession& session, Feature* feature, bool firstSessionStart);
    void scheduleEnsureTriggerMonitorEnginesRunning();
    void ensureTriggerMonitorEnginesRunning();
    void launchTriggerActionRun(FeatureRunSession& session, Feature* feature);
    void refreshSessionWorkflowFromProject(FeatureRunSession& session, Feature* feature);
    void applyDeferredSessionWorkflowRefresh(FeatureRunSession& session, Feature* feature);
    void requestSessionWorkflowRefresh(const std::string& featureId);
    void scheduleTriggerCooldown(FeatureRunSession& session, Feature* feature);
    void handleTriggerEngineFinished(FeatureRunSession& session,
                                     Feature* feature,
                                     bool success,
                                     const QString& message);
    void pauseOtherSessionsForTrigger(FeatureRunSession& triggerSession);
    void resumePreemptedSessionsForTrigger(FeatureRunSession& triggerSession);
    void persistTriggerArmedState(const QString& featureId, bool armed);
    void scheduleRestorePersistedTriggerSessions();
    void restorePersistedTriggerSessions();
    void prunePersistedTriggerArmedFeatures();
    void runFeature(Feature* feature);
    bool isDisplayedRunningFeature(const FeatureRunSession* session) const;
    void applyRunningBlockVisuals(FeatureRunSession& session,
                                  int index,
                                  BlockListWidget::ExecutionHighlight highlight);
    void updateTargetWindowDetails();
    void updateTargetWindowControlsForActiveProfile();
#ifdef _WIN32
    void commitActiveProfileTargetWindow(HWND hwnd, const QString& title);
    void commitActiveProfileSubTargetWindow(HWND hwnd, const QString& title);
    enum class TargetWindowListPickMode { Main, Sub };
    void pickTargetWindowFromList(TargetWindowListPickMode mode);
#endif
    bool isActiveDefaultProfile() const;
    std::wstring currentTargetWindowTitleW() const;
    /// Main project title, or active profile's sub title when the foreground window matches it.
    std::wstring resolveEffectiveTargetTitleW() const;
    /// Run/trigger capture target locked at session start (foreground-aware; sub binding supported).
    std::wstring resolveRunCaptureTargetTitleW(const Feature* feature = nullptr) const;
    std::wstring resolveAutoRunCaptureTargetTitleW() const;
    bool scopedTargetForegroundActive(const Feature* feature) const;
    bool deferRunUntilScopedTargetForeground(FeatureRunSession& session, Feature* feature);
    void scheduleScopedTargetForegroundResumePoll();
    void resumeWaitingScopedTargetForegroundSessions();
    void refreshSessionCaptureTarget(FeatureRunSession& session);
    std::wstring sessionCaptureTargetTitleW(FeatureRunSession& session);
    void applySessionCaptureTarget(const std::wstring& title) const;
    void syncTargetWindowTitleToCapture();
    void prepareCenterPinTargetWindow();
    /// Applies resolveEffectiveTargetTitleW() to ScreenCapture (used at feature run start).
    void syncEffectiveTargetWindowTitleToCapture();
    void saveSelectedFeaturePreference();
    void restoreSelectedFeaturePreference();
    QString selectedFeaturePreferenceKey() const;
    void scheduleRunWarmup();
    void prepareProjectUnload();
    bool pushGlobalUiUndoSnapshot(const QString& reason);
    void clearGlobalUiRedoHistory();
    void clearGlobalUiUndoHistory();
    bool restoreGlobalUiSnapshot(const GlobalUiHistorySnapshot& snapshot);
    GlobalUiHistorySnapshot createGlobalUiSnapshot(const QString& reason) const;
    static bool copyDirectoryRecursive(const QString& sourceDir, const QString& targetDir);
    static void removeDirectoryRecursive(const QString& path);
    void onGlobalUndoRequested();
    void onGlobalRedoRequested();
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
    void accumulateLoopCompletionStats(FeatureRunSession& session,
                                         bool success,
                                         std::int64_t elapsedOverrideMs = -1);
    void configureWorkerFastRepeat(FeatureRunSession& session, Feature* feature);
    void flushWorkerFastRepeatUi(const std::string& featureId);
    WorkerFastRepeatUiCoalesce& fastRepeatUiCoalesceFor(const std::string& featureId);
    void publishLoopCompletionUi(FeatureRunSession& session, bool success, const QString& message);
    void     syncLoopTimingToWorkflowEditor(const FeatureRunSession* session);

    void onUserInputInterrupt(const std::string& featureId);
    void syncUserInputInterruptForSession(FeatureRunSession& session, Feature* feature);
    bool shouldSuppressFeatureHotkeyExecution() const;
    void notifyFeatureHotkeySuppressed();

    QString buildCrashReportContextSnapshot() const;

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
    LogPanelWidget* m_logPanel = nullptr;
    QToolButton* m_pickWindowButton = nullptr;
    QToolButton* m_pickWindowListButton = nullptr;
    QToolButton* m_pickSubWindowListButton = nullptr;
    QToolButton* m_showTargetWindowButton = nullptr;
    QToolButton* m_pinTargetWindowCenterButton = nullptr;
    QCheckBox* m_alwaysOnTopCheck = nullptr;
    QPushButton* m_exitButton = nullptr;
    QPushButton* m_updateButton = nullptr;
    QPushButton* m_calculatorButton = nullptr;
    QPushButton* m_spikeWatchButton = nullptr;
    QPushButton* m_memoButton = nullptr;
    QPushButton* m_settingsButton = nullptr;
    TargetWindowDetailPanel* m_targetWindowDetailPanel = nullptr;
    CustomTitleBar* m_titleBar = nullptr;

    HotkeyManager* m_hotkeyManager = nullptr;
    UiStateManager* m_uiState = nullptr;
    std::unique_ptr<FeatureLibraryManager> m_featureLibraryManager;
    std::unique_ptr<Feature> m_libraryPreviewFeature;
    QString m_libraryPreviewEntryId;
    QTimer* m_autoSaveTimer = nullptr;
    QTimer* m_profilePackageSealTimer = nullptr;
    std::atomic_bool m_profilePackageSealRunning{false};
    QTimer* m_statusClearTimer = nullptr;
    QTimer* m_updateCheckTimer = nullptr;
    QTimer* m_mouseLockSyncTimer = nullptr;
    QTimer* m_targetWindowCenterPinTimer = nullptr;
    QTimer* m_profileAutoSwitchTimer = nullptr;
    void* m_profileForegroundEventHook = nullptr;
    UpdateChecker* m_updateChecker = nullptr;
    bool m_initialUpdateCheckDone = false;
    bool m_lastUpdateCheckWasSilent = false;
    bool m_autoUpdateDeferred = false;
    bool m_autoUpdateInstallStarted = false;
    QString m_persistentStatusMessage;
    QString m_transientStatusMessage;
    std::map<std::string, FeatureRunSession> m_runSessions;
    std::map<std::string, std::unique_ptr<WorkerFastRepeatUiCoalesce>> m_fastRepeatUiCoalesce;
    QPointer<CalculatorDialog> m_calculatorDialog;
    QPointer<SpikeWatchDialog> m_spikeWatchDialog;
    QPointer<MemoDialog> m_memoDialog;
    QSystemTrayIcon* m_trayIcon = nullptr;
    QMenu* m_trayMenu = nullptr;
    bool m_forceQuit = false;
    bool m_trayMinimizeNotified = false;
    bool m_modified = false;
    bool m_refreshingProfileList = false;
    bool m_switchingProfile = false;
    bool m_deferTargetDetailsProfileRefresh = false;
    bool m_lastPersistedProfileSettingsValid = false;
    QString m_lastPersistedProfileSettingsProfileId;
    ProgramSettings::ProfileSettings m_lastPersistedProfileSettings{};
    bool m_restoringGlobalUiHistory = false;
    bool m_suppressTriggerArmedPersist = false;
    std::vector<GlobalUiHistorySnapshot> m_globalUiUndoHistory;
    std::vector<GlobalUiHistorySnapshot> m_globalUiRedoHistory;
    QString m_deferredProfileSwitchId;
    QElapsedTimer m_lastAutomaticProfileSwitchTimer;
    std::vector<std::unique_ptr<WorkflowEngine>> m_abandonedEngines;
    QElapsedTimer m_pendingDefaultProfileSwitchTimer;
    QString m_lastLinkedForegroundProfileId;
    QElapsedTimer m_recentAutomaticDefaultProfileSwitchTimer;
    bool m_scopedTargetForegroundResumePending = false;
    bool m_ensureTriggerMonitorPending = false;
};
