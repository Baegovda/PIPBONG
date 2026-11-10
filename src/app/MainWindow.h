#pragma once

#include "app/FeatureRunSession.h"
#include "app/FeatureBatchController.h"
#include "app/PointerFeedbackSettings.h"
#include "app/ForegroundWindowState.h"
#include "app/ForegroundRunGate.h"
#include "app/ProfileManager.h"
#include "app/ProfileActionsController.h"
#include "app/ProfileSwitchCoordinator.h"
#include "app/RunSessionRegistry.h"
#include "app/RunSessionController.h"
#include "app/TargetWindowController.h"
#include "app/TargetWindowDetailsController.h"
#include "core/workflow/ExecutionContext.h"
#include "model/UserInputInterruptMode.h"
#include "model/WorkflowExecutionHighlight.h"
#include "ui/LogPanelWidget.h"

#include <QByteArray>
#include <QElapsedTimer>
#include <QMainWindow>
#include <QPointer>
#include <QSet>
#include <unordered_set>
#include <QMutex>
#include <atomic>
#include <map>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#ifdef _WIN32
#include <windows.h>
#endif

class Feature;
class ForegroundWindowMonitor;

enum class FeatureRunVisualKind;

class QPixmap;
class QIcon;
class QToolButton;

class FeatureListPanel;
class WorkflowEditorPanel;
class WorkflowEngine;
class Project;
class ProjectWorkspaceController;
class ProfileTransitionController;
class TargetWindowActionsController;
class TargetCenterPinController;
class CaptureTargetController;
class InputInterruptController;
class FeatureHotkeyController;
class TriggerMonitorController;
class TriggerPreemptionController;
class WorkflowExecutionPresenter;
class SessionMouseLockController;
class ForegroundSessionController;
class SessionInputCleanupController;
class HotkeyManager;
class HoldKeyTapMultiplexer;
class HoldTapSessionController;
class WindowChromeController;
class ProgramSettingsController;
class WindowDiagnosticsController;
class WorkspaceEditorController;
class DocumentActionsController;
class FeatureLibraryActionsController;
class TargetPanelController;
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
class ProfileListPresenter;
class TargetWindowDetailPanel;
class CustomTitleBar;
class AuxiliaryToolsController;
class WindowShellController;
class MainWorkspaceView;
class FirstTemplateRoiController;
class RunSessionResourcesController;
class WorkspaceHistoryActions;
class UpdateUiController;
class WorkspaceHistoryController;
class ShutdownCoordinator;
class FeatureTransferController;
class RunUiPublisher;
class ProfilePackageController;

class MainWindow : public QMainWindow {
    Q_OBJECT
    friend class RunLifecycleCoordinator;
    friend class RunLifecycleBoundaryChecks;
    friend class WorkspaceHistoryUiChecks;
    friend class ProjectWorkspaceUiChecks;
    friend class TargetWindowDetailsUiChecks;
    friend class TargetWindowActionsChecks;
    friend class TargetCenterPinUiChecks;
    friend class CaptureTargetUiChecks;
    friend class InputInterruptUiChecks;
    friend class SessionMouseLockUiChecks;
    friend class ForegroundSessionUiChecks;
    friend class SessionInputCleanupUiChecks;
    friend class ProfileListUiChecks;
    friend class ProfileActionsUiChecks;
    friend class FeatureBatchUiChecks;
    friend class FeatureLibraryActionsChecks;
    friend class FeatureTransferLifetimeChecks;
    friend class FeatureHotkeyUiChecks;
    friend class TriggerMonitorUiChecks;
    friend class TriggerPreemptionUiChecks;
    friend class WorkflowExecutionUiChecks;
    friend class WorkflowToolbarUiChecks;
    friend class AuxiliaryToolsUiChecks;
    friend class WindowShellUiChecks;
    friend class MainWorkspaceUiChecks;
    friend class FirstTemplateRoiChecks;
    friend class RunSessionResourcesChecks;
    friend class HoldTapSessionChecks;
    friend class WindowChromeChecks;
    friend class ProgramSettingsChecks;
    friend class WindowDiagnosticsController;
    friend class WindowDiagnosticsChecks;
    friend class WorkspaceEditorChecks;
    friend class DocumentActionsChecks;
    friend class NativeCaptionProbe;
public:
    explicit MainWindow(QWidget* parent = nullptr);
    ~MainWindow() override;

    void ensureInitialWindowPlacement();
    void showPendingCrashReportIfAny();
    /// Refreshes main/sub target bindings in ScreenCapture before block editors or overlays open.
    void refreshCaptureTargetForEditing();
    /// Persistence must succeed before the caller closes or restarts the app.
    bool prepareForShutdown();

protected:
    void closeEvent(QCloseEvent* event) override;
    bool eventFilter(QObject* watched, QEvent* event) override;
    void showEvent(QShowEvent* event) override;
    void changeEvent(QEvent* event) override;
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
    void onReadyToRestartForUpdate();
    void maybeStartAutomaticUpdate();
    void onProgramSettings();
    void onCalculator();
    void onSpikeWatch();
    void onMemo();
    void onAlwaysOnTopToggled(bool checked);
    void onPickTargetWindow();
    void onPickSubTargetWindow();
    void onPickMainTargetWindowFromList();
    void onPickSubTargetWindowFromList();
    void onShowTargetWindow();
    void onShowSubTargetWindow();
    void onPinTargetWindowCenterToggled(bool checked);
    void onPinSubTargetWindowCenterToggled(bool checked);
    void onForegroundStateChanged(const ForegroundWindowState& state);
    void onForegroundAltModifierReleased();
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
    void setupRunUiPublisher();
    void setupRunResources();
    void setupHoldTapSessions();
    void setupProgramSettings();
    void setupWorkspaceEditor();
    void setupFeatureHotkeys();
    void setupTriggerMonitor();
    void setupTriggerPreemption();
    void setupWorkflowExecutionPresenter();
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
    void updateRunUiState(bool immediate = false);
    bool shouldCoalesceRunUiUpdates() const;
    void applyRunUiState();
    bool isFeatureRunHighlighted(const FeatureRunSession& session) const;
    QHash<QString, FeatureRunVisualKind> buildFeatureListRunVisualKinds() const;
    void refreshFeatureListHoldVisuals();
    void scheduleFeatureListHoldVisualRefresh();
    int concurrentActiveRepeatSessionCount() const;
    void connectSessionEngine(FeatureRunSession& session);
    FeatureRunSession* sessionFor(const std::string& featureId);
    const FeatureRunSession* sessionFor(const std::string& featureId) const;
    std::map<std::string, FeatureRunSession>& runSessions();
    const std::map<std::string, FeatureRunSession>& runSessions() const;
    FeatureRunSession* sessionForEngine(const QObject* sender);
    bool isFeatureRunning(const std::string& featureId) const;
    bool isFeatureInActiveWorkflowRun(const std::string& featureId) const;
    bool hasAnyRunningSession() const;
    bool hasAnyActiveWorkflowEngine() const;
    bool hasAnyCapturingWorkflowBurstExcept(const std::string& excludeFeatureId) const;
    QSet<QString> activeWorkflowFeatureIds() const;
    QSet<QString> runningFeatureIds() const;
    QString featureDisplayName(const std::string& featureId) const;
    void appendSessionLog(const FeatureRunSession& session,
                          const QString& message,
                          LogLineKind kind = LogLineKind::Info);
    void stopFeatureRun(const std::string& featureId);
    void stopAllSessions();
    void stopRunningSessionsForUpdate();
    void detachUiForProfileSwitch();
    void abandonSessionEngine(FeatureRunSession& session);
    void pruneAbandonedEngines();
    void schedulePruneAbandonedEngines();
    void flushDeferredProfileSwitchIfIdle();
    bool hasTriggerMonitoringSessions() const;
    void appendLog(const QString& message, LogLineKind kind = LogLineKind::Info);
    bool maybeSave(bool quiet = false);
    void loadProjectFromFile(const QString& path, bool quiet = false);
    void loadActiveProfile(bool quiet = false, bool scheduleTriggerRestore = true);
    void refreshProfileList();
    void refreshFeatureLibraryPanel();
    bool importLibraryEntry(const QString& entryId);
    bool importLibraryEntries(const QStringList& entryIds);
    void setupFeatureTransfers();
    void setupProfileActions();
    void showProfileActionResult(const ProfileActionsController::Result&);
    void onFeatureDroppedOnFeatureList(const QMimeData* mime, int insertIndex);
    void onFeatureDroppedOnLibrary(const QMimeData* mime);
    void onLibraryEntriesReordered(int fromRow, int toRow);
    void onLibraryEntriesMultiReordered(const QList<int>& selectedRows, int insertIndex);
    void onFeatureDroppedOnProfile(const QString& targetProfileId, const QMimeData* mime);
    void syncProfileListSelection();
    void syncMemoDialogProfile();
    bool switchToProfile(const QString& profileId, bool automatic = false);
    bool executeProfileSwitch(const QString& profileId, bool automatic);
    void setupProjectWorkspace();
    void setupTargetActions();
    void setupTargetCenterPin();
    void setupCaptureTargets();
    void setupInputInterrupts();
    void setupSessionMouseLocks();
    void setupForegroundSessions();
    void onWorkspaceInstalled(bool fresh, bool clearTarget, bool quiet, bool restoreTriggers);
    void setProfileSwitchUiLocked(bool locked);
    void schedulePostProfileSwitchForegroundReconcile(bool allowAutoProfileSwitch);
    bool saveActiveProfileSettings();
    void applyProfileSwitchFromForegroundState(const ForegroundWindowState& state);
#ifdef _WIN32
    void applyForegroundCaptureHints(HWND hwnd, const QString& foregroundTitle);
    void handlePipbongForegroundFocus();
#endif
    void updateWindowTitle();
    void refreshWorkflowEditor();
    void scheduleAutoSave();
    void autoSaveProject(bool quiet = false);
    void scheduleProfilePackageSeal();
    void sealActiveProfilePackage(bool synchronous = false);
    bool ensureProjectFilePath();
    void setupShutdownCoordinator();
    void finalizeShutdown();
    void setupTrayIcon();
    void applyCloseToTrayPolicy();
    void requestApplicationQuit();
    void startFeatureRun(Feature* feature,
                         bool fromHotkey = false,
                         bool skipTargetActivationOnStart = false);
    void setupFirstTemplateRoi();
    bool tryBeginFirstTemplateRoiEdit(FeatureRunSession& session, Feature* feature);
    void selectRunningFeatureForDisplay(Feature* feature);
    void launchWorkflowRun(FeatureRunSession& session, Feature* feature, bool repeatIteration = false);
    void launchHoldKeyTapRun(FeatureRunSession& session, Feature* feature, int virtualKey);
    void ensureRunSessionResources(FeatureRunSession& session,
                                   Feature* feature,
                                   bool refreshWorkflow = false,
                                   bool deferWorkflowCloneToWorker = false);
    void syncRunSessionContext(FeatureRunSession& session, bool refreshCaptureBinding = true);
    void applyFeatureRunPoliciesToContext(FeatureRunSession& session, Feature* feature);
    bool shouldLogRunDetails(const FeatureRunSession& session) const;
    void continueRepeatSession(FeatureRunSession& session, Feature* feature, bool success, const QString& message);
    bool shouldContinueRunSession(const FeatureRunSession& session, Feature* feature);
    void reconcileHoldLatchForActiveHoldSessions();
    void finishRunSession(const std::string& featureId,
                          bool success,
                          const QString& message,
                          bool deferUiUpdate = false);
    void scheduleRepeatIteration(FeatureRunSession& session,
                                 Feature* feature,
                                 bool success,
                                 const QString& message);
    void launchTriggerMonitor(FeatureRunSession& session, Feature* feature, bool firstSessionStart);
    void scheduleDeferredTriggerMonitorRestart(FeatureRunSession& session,
                                             Feature* feature,
                                             int delayMs);
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
    bool sessionBelongsToActiveProfile(const FeatureRunSession& session) const;
    Feature* featureForSession(FeatureRunSession& session);
    const Feature* featureForSession(const FeatureRunSession& session) const;
    void pruneSessionOwnerProjects();
    void releaseHoldHotkeyInputToTarget(FeatureRunSession& session);
    void restoreRunStartCursorPosition(const FeatureRunSession& session);
    void captureRunStartCursorPosition(FeatureRunSession& session);
    void applyRunningBlockVisuals(FeatureRunSession& session,
                                  int index,
                                  WorkflowExecutionHighlight highlight);
    void updateTargetWindowDetails();
    void updateTargetWindowControlsForActiveProfile();
    TargetWindowDetailsController::Request targetDetailsRequest()const;
#ifdef _WIN32
    void healLinkedTargetProcessPathFromForeground(HWND hwnd, const QString& foregroundTitle);
#endif
    bool isActiveDefaultProfile() const;
    std::wstring currentTargetWindowTitleW() const;
    /// Main project title, or active profile's sub title when the foreground window matches it.
    std::wstring resolveEffectiveTargetTitleW() const;
    /// Run/trigger capture target locked at session start (foreground-aware; sub binding supported).
    std::wstring resolveRunCaptureTargetTitleW(const Feature* feature = nullptr) const;
    std::wstring resolveAutoRunCaptureTargetTitleW() const;
    /// Stored main/sub binding for HWND lookup — not gated by foreground (display + idle sync).
    std::wstring linkedTargetLookupTitleW() const;
    bool scopedTargetForegroundActive(const Feature* feature) const;
    bool runForegroundGateActive(const Feature* feature) const;
    bool runForegroundGateActiveForSession(const FeatureRunSession& session,
                                           Feature* feature) const;
    bool triggerBackgroundRunGateActive(const Feature* feature) const;
    bool foregroundProfileMatchesActive() const;
    bool switchToForegroundLinkedProfileIfNeeded(bool forceImmediate);
    QString foregroundProfileIdForActiveWindow() const;
    ForegroundRunGate::GateInput buildForegroundGateInput(const Feature* feature = nullptr,
                                                          const QString& bindingsProfileId =
                                                              QString()) const;
    ForegroundRunGate::ProfileBindings buildForegroundProfileBindings(
        const QString& profileId = QString()) const;
    bool profileMainOrSubForegroundActive() const;
    bool deferRunUntilScopedTargetForeground(FeatureRunSession& session, Feature* feature);
    void resumeWaitingScopedTargetForegroundSessions();
    void reconcileRunSessionsWithForegroundGate();
    /// After foreground HWND/profile/capture binding is updated, resume gated sessions and hotkey context.
    void finishForegroundSessionGate();
#ifdef _WIN32
    void ensureForegroundReadyForFeatureHotkey();
    void maybeResetHotkeyLatchForForeground(HWND foregroundHwnd);
#endif
    void refreshSessionCaptureTarget(FeatureRunSession& session);
    /// When `refreshCaptureBinding` is false, returns locked title (or resolve) without HWND lookup / global ScreenCapture sync (R3.3 repeat loops).
    std::wstring sessionCaptureTargetTitleW(FeatureRunSession& session, bool refreshCaptureBinding = true);
    void applySessionCaptureTarget(const std::wstring& title) const;
    void syncTargetWindowTitleToCapture();
#ifdef _WIN32
    void rememberProfileLinkedForeground(HWND hwnd, const QString& foregroundTitle);
    /// Bind ScreenCapture to the desktop foreground HWND when its process path matches the active profile's linked main/sub exe (authoritative over title heuristics).
    bool adoptForegroundLinkedCaptureIfMatched();
    QString profileIdForForegroundHwnd(HWND hwnd) const;
#endif
    /// Applies resolveEffectiveTargetTitleW() to ScreenCapture (used at feature run start).
    void syncEffectiveTargetWindowTitleToCapture();
    void saveSelectedFeaturePreference();
    void restoreSelectedFeaturePreference();
    QString selectedFeaturePreferenceKey() const;
    void scheduleRunWarmup();
    void prepareProjectUnload();
    bool pushGlobalUiUndoSnapshot(const QString& reason);
    void setupWorkspaceHistory();
    void onGlobalUndoRequested();
    void onGlobalRedoRequested();
    void onEngineSessionPrepared(std::shared_ptr<Workflow> workflow, std::shared_ptr<ExecutionContext> context);
    void restoreAlwaysOnTopPreference();
    void applyTargetWindowCenterPin();
    void showTransientStatus(const QString& message, int timeoutMs = 3000);
    void setPersistentStatus(const QString& message);
    void clearStatusMessage();
    void logLoopCompletion(FeatureRunSession& session, bool success, const QString& message);
    void configureWorkerFastRepeat(FeatureRunSession& session, Feature* feature);
    void synchronizeRunWorkerState();
    void flushWorkerFastRepeatUi(const std::string& featureId);
    void publishLoopCompletionUi(FeatureRunSession& session, bool success, const QString& message);
    void     syncLoopTimingToWorkflowEditor(const FeatureRunSession* session);

    void onUserInputInterrupt(const std::string& featureId, const std::shared_ptr<ExecutionContext>& expected);
    void syncUserInputInterruptForSession(FeatureRunSession& session, Feature* feature);


    std::unique_ptr<ProfileManager> m_profileManager;
    std::unique_ptr<TriggerMonitorController> m_triggerMonitor;
    ProfileSwitchCoordinator m_profileSwitchCoordinator;
    std::unique_ptr<ProjectWorkspaceController> m_projectWorkspace;
    std::unique_ptr<ProfileTransitionController> m_profileTransition;
    std::unique_ptr<ProfileActionsController> m_profileActions;
    // View aliases; authoritative document and transition state live in controllers.
    std::shared_ptr<Project>& m_project;
    QString& m_projectFilePath;
    bool& m_modified;
    bool& m_switchingProfile;
    bool& m_profileSwitchPipelineActive;
    bool& m_deferTargetDetailsProfileRefresh;

    std::unique_ptr<FirstTemplateRoiController> m_firstTemplateRoi;
    std::unique_ptr<MainWorkspaceView> m_workspaceView;
    // View aliases for application signal wiring; the component owns assembly/state.
    FeatureListPanel* m_featureList = nullptr;
    QWidget* m_profilePanel = nullptr;
    ProfileListWidget* m_profileList = nullptr;
    std::unique_ptr<ProfileListPresenter> m_profileListPresenter;
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
    QToolButton* m_pickSubWindowButton = nullptr;
    QToolButton* m_pickSubWindowListButton = nullptr;
    QToolButton* m_showSubTargetWindowButton = nullptr;
    QToolButton* m_showTargetWindowButton = nullptr;
    QToolButton* m_pinTargetWindowCenterButton = nullptr;
    QToolButton* m_pinSubTargetWindowCenterButton = nullptr;
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
    HoldKeyTapMultiplexer* m_holdKeyTapMux = nullptr;
    UiStateManager* m_uiState = nullptr;
    std::unique_ptr<FeatureLibraryManager> m_featureLibraryManager;
    std::unique_ptr<FeatureBatchController> m_featureBatches;
    std::shared_ptr<Feature> m_libraryPreviewFeature;
    QString m_libraryPreviewEntryId;
    QTimer* m_autoSaveTimer = nullptr;
    std::unique_ptr<ProfilePackageController> m_packageController;
    std::unique_ptr<DocumentActionsController> m_documentActions;
    QTimer* m_runUiDebounceTimer = nullptr;
    std::unique_ptr<ForegroundWindowMonitor> m_foregroundMonitor;
    TargetWindowController m_targetWindowController;
    std::unique_ptr<CaptureTargetController> m_captureTargets;
    std::unique_ptr<InputInterruptController> m_inputInterrupts;
    std::unique_ptr<FeatureHotkeyController> m_featureHotkeys;
    std::unique_ptr<SessionMouseLockController> m_mouseLocks;
    std::unique_ptr<ForegroundSessionController> m_foregroundSessions;
    std::unique_ptr<SessionInputCleanupController> m_inputCleanup;
    TargetWindowDetailsController m_targetDetails;
    std::unique_ptr<TargetPanelController> m_targetPanel;
    std::unique_ptr<TargetWindowActionsController> m_targetActions;
    std::unique_ptr<TargetCenterPinController> m_centerPin;
    RunSessionController m_runSessionController;
    std::unique_ptr<UpdateUiController> m_updateController;
    RunSessionRegistry m_runSessionRegistry;
    // Declared after the registry: the controller releases leases in its destructor.
    std::unique_ptr<TriggerPreemptionController> m_triggerPreemption;
    /// Project snapshots for sessions running on non-active profiles after a profile switch.
    mutable std::unordered_map<QString, std::shared_ptr<Project>> m_sessionOwnerProjects;
    std::unique_ptr<RunUiPublisher> m_runUiPublisher;
    std::unique_ptr<RunSessionResourcesController> m_runResources;
    std::unique_ptr<HoldTapSessionController> m_holdTapSessions;
    std::unique_ptr<WorkflowExecutionPresenter> m_executionPresenter;
    std::unique_ptr<AuxiliaryToolsController> m_auxiliaryTools;
    std::unique_ptr<WindowShellController> m_windowShell;
    std::unique_ptr<WindowChromeController> m_windowChrome;
    std::unique_ptr<ProgramSettingsController> m_programSettings;
    std::unique_ptr<WindowDiagnosticsController> m_diagnostics;
    std::unique_ptr<WorkspaceEditorController> m_workspaceEditor;
    bool m_refreshingProfileList = false;
    bool& m_suppressTriggerArmedPersist;
    std::unique_ptr<WorkspaceHistoryActions> m_workspaceHistory;
    std::unique_ptr<ShutdownCoordinator> m_shutdownCoordinator;
    std::unique_ptr<FeatureTransferController> m_featureTransfers;
    std::unique_ptr<FeatureLibraryActionsController> m_libraryActions;
    std::vector<std::unique_ptr<WorkflowEngine>> m_abandonedEngines;
    std::unordered_map<const WorkflowEngine*, std::string> m_abandonedEngineFeatureIds;
#ifdef _WIN32
#endif
    bool m_pruneAbandonedEnginesPending = false;
    bool m_holdVisualRefreshScheduled = false;
    int m_holdBurstDepth = 0;
    bool m_holdBurstScopeDrainScheduled = false;
    QElapsedTimer m_holdBurstForegroundPrepTimer;
    bool m_holdBurstForegroundPrepared = false;
    std::shared_ptr<std::atomic<bool>> m_holdBurstTargetActivated = std::make_shared<std::atomic<bool>>(false);
    bool m_holdBurstCaptureTitleValid = false;
    bool m_holdBurstCaptureAppliedToCapture = false;
    std::wstring m_holdBurstCaptureTitle;
    std::vector<std::string> m_pendingHoldFeatureStartOrder;
    std::unordered_set<std::string> m_pendingHoldFeatureStartIds;
    bool m_holdFeatureStartFlushScheduled = false;
    std::unordered_set<std::string> m_pendingHoldFeatureEndFinishes;
    bool m_holdFeatureEndFinishFlushScheduled = false;
    std::unordered_set<std::string> m_pendingHoldStartUiFeatureIds;
    bool m_holdStartUiFlushScheduled = false;
    bool m_holdEndCleanupScheduled = false;
    bool m_deferredBurstTriggerRestore = false;
    bool m_deferredBurstPruneEngines = false;
};
