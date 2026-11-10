#include "ui/FeatureLibraryActionsController.h"
#include "ui/TargetPanelController.h"
#include "ui/DocumentActionsController.h"
#include "app/MainWindow.h"
#include "app/WindowDiagnosticsController.h"
#include "app/FeatureHotkeyController.h"
#include "app/FirstTemplateRoiController.h"
#include "app/TriggerMonitorController.h"
#include "app/TriggerPreemptionController.h"
#include "ui/WorkflowExecutionPresenter.h"
#include "app/WorkerRepeatController.h"
#include "app/RunSessionResourcesController.h"
#include "app/HoldTapSessionController.h"
#include "app/RunUiPublisher.h"
#include "app/FeatureTransferController.h"
#include "app/ShutdownCoordinator.h"
#include "ui/MainWorkspaceView.h"
#include "ui/WorkspaceEditorController.h"
#include "app/InputInterruptController.h"
#include "app/SessionMouseLockController.h"
#include "app/ForegroundSessionController.h"
#include "app/SessionInputCleanupController.h"
#include "app/WorkspaceHistoryController.h"
#include "app/ProfilePackageController.h"
#include "app/UpdateUiController.h"

#include <algorithm>

#include "app/ForegroundRunGate.h"
#include "app/ForegroundWindowMonitor.h"
#include "app/ProfileSwitchCoordinator.h"
#include "app/Application.h"
#include "app/FeatureHotkeyGate.h"
#include "app/ProgramSettings.h"
#include "app/SessionRunPolicy.h"
#include "app/RunLifecycleCoordinator.h"
#include "app/RunContextController.h"
#include "app/CaptureTargetController.h"
#include "app/ProjectWorkspaceController.h"
#include "app/ProfileTransitionController.h"
#include "app/WorkspaceHistoryActions.h"
#include "app/MouseCenterLock.h"
#include "app/TargetCenterPinController.h"
#include "app/TargetWindowActionsController.h"
#include "app/UserInputInterruptMonitor.h"
#include "ui/AuxiliaryToolsController.h"
#include "ui/WindowShellController.h"
#include "ui/WindowChromeController.h"
#include "app/ProgramSettingsController.h"
#include "model/UserInputInterruptMode.h"
#include "app/HotkeyManager.h"
#include "app/FeatureLibraryManager.h"
#include "core/capture/ScreenCapture.h"
#include "core/capture/CursorPositionPicker.h"
#include "core/capture/ClickContinuousInputRecorder.h"
#include "core/capture/WindowPicker.h"
#include "core/diagnostics/AppStutterProfiler.h"
#include "core/input/InputSimulator.h"
#include "core/input/HotkeyBinding.h"
#include "ui/WindowPickerHoverOverlay.h"
#include "core/RunWarmup.h"
#include "core/workflow/ExecutionContext.h"
#include "core/workflow/Workflow.h"
#include "core/workflow/WorkflowEngine.h"
#include "core/workflow/HoldKeyTapMultiplexer.h"
#include "core/workflow/WorkflowRunner.h"
#include "core/workflow/blocks/ImageFindBlock.h"
#include "model/Feature.h"
#include "model/FeatureCaptureTargetScope.h"
#include "model/Project.h"
#include "storage/JsonSerializer.h"
#include "storage/ProjectPackage.h"
#include "ui/FeatureListPanel.h"
#include "ui/BlockListWidget.h"
#include "ui/WorkflowEditorPanel.h"
#include "ui/TargetWindowBindingRole.h"
#include "ui/TargetWindowDetailPanel.h"
#include "ui/TargetWindowHighlightOverlay.h"
#include "ui/TargetWindowListPicker.h"
#include "app/UpdateChecker.h"
#include "core/diagnostics/CrashReporter.h"
#include "core/diagnostics/DiagnosticHub.h"
#include "ui/diagnostics/CrashReportDialog.h"
#include "ui/CustomTitleBar.h"
#include "ui/ProfileListWidget.h"
#include "ui/ProfileListPresenter.h"
#include "app/ProfilePresentationSnapshot.h"
#include "ui/widgets/ReorderableListWidget.h"
#include "ui/LogPanelWidget.h"
#include "ui/ProfileEditDialog.h"
#include "ui/UiStateManager.h"
#include "ui/UiThemeColors.h"
#include "ui/editors/MatchTestOverlay.h"
#include "ui/editors/WorkflowMatchFeedbackOverlay.h"
#include "ui/editors/WorkflowRoiFlashOverlay.h"
#include "ui/editors/RoiPreviewOverlay.h"
#include "ui/editors/ScreenRegionOverlay.h"

#include <QCheckBox>
#include <QCloseEvent>
#include <QCoreApplication>
#include <QAbstractButton>
#include <QApplication>
#include <QDateTime>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDir>
#include <QElapsedTimer>
#include <QEvent>
#include <QEventLoop>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QMimeData>
#include <QGroupBox>
#include <QHash>
#include <QHBoxLayout>
#include <QIcon>
#include <QInputDialog>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMenuBar>
#include <QMessageBox>
#include <QPushButton>
#include <QSettings>
#include <QShowEvent>
#include <QSignalBlocker>
#include <QScopedValueRollback>
#include <QSizePolicy>
#include <QSplitter>
#include <QStatusBar>
#include <QSystemTrayIcon>
#include <QMenu>
#include <QPointer>
#include <QShortcut>
#include <QTimer>
#include <QToolButton>
#include <QThread>
#include <QVector>
#include <QVBoxLayout>

#include <optional>

#include <atomic>
#include <memory>

#include "model/FeatureRunMode.h"

namespace {

QString diagnosticLogLevelTag(LogLineKind kind) {
    switch (kind) {
    case LogLineKind::Success:
        return QStringLiteral("OK");
    case LogLineKind::Warning:
        return QStringLiteral("WARN");
    case LogLineKind::Error:
        return QStringLiteral("ERR");
    case LogLineKind::Accent:
        return QStringLiteral("ACC");
    case LogLineKind::Info:
    default:
        return QStringLiteral("INFO");
    }
}

void assertMainWindowOnGuiThread(const char* where) {
    QCoreApplication* app = QCoreApplication::instance();
    if (app && QThread::currentThread() != app->thread()) {
        qWarning("MainWindow: %s called off GUI thread", where);
        Q_ASSERT(false);
    }
}

} // namespace

#ifdef _WIN32
#include <windows.h>

namespace {

bool isAltTabModifierHeld() {
    return ForegroundWindowMonitor::isAltTabModifierHeld();
}

bool isPipbongProcessForeground(HWND hwnd) {
    return ForegroundWindowMonitor::isPipbongProcessWindow(hwnd);
}

using ForegroundRunGate::findVisibleTopLevelWindowHwnd;
using ForegroundRunGate::foregroundHwndMatchesLinkedProcess;
using ForegroundRunGate::foregroundMatchesScopedMainTarget;
using ForegroundRunGate::foregroundMatchesScopedSubTarget;
} // namespace
#endif
namespace {

int featureIndexById(const Project& project, const std::string& featureId) {
    const auto& features = project.features();
    for (int i = 0; i < static_cast<int>(features.size()); ++i) {
        if (features[static_cast<size_t>(i)]->id() == featureId) {
            return i;
        }
    }
    return -1;
}

bool isTriggerMonitoring(const FeatureRunSession& session) {
    return session.runningMode == FeatureRunMode::Trigger
        && session.triggerPhase == TriggerSessionPhase::Monitoring;
}


} // namespace

MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent)
    , m_profileManager(std::make_unique<ProfileManager>(Application::dataDirectory()))
    , m_triggerMonitor(std::make_unique<TriggerMonitorController>(*m_profileManager,this))
    , m_projectWorkspace(std::make_unique<ProjectWorkspaceController>(*m_profileManager,Application::autoSaveFilePath(),this))
    , m_profileTransition(std::make_unique<ProfileTransitionController>(*m_profileManager,*m_projectWorkspace,this))
    , m_project(m_projectWorkspace->state().project)
    , m_projectFilePath(m_projectWorkspace->state().path)
    , m_modified(m_projectWorkspace->state().modified)
    , m_switchingProfile(m_profileTransition->state().switching)
    , m_profileSwitchPipelineActive(m_profileTransition->state().pipeline)
    , m_deferTargetDetailsProfileRefresh(m_profileTransition->state().deferTargetDetails)
    , m_hotkeyManager(new HotkeyManager(this))
    , m_holdKeyTapMux(new HoldKeyTapMultiplexer(this))
    , m_suppressTriggerArmedPersist(m_triggerMonitor->suppressed()) {
#ifdef _WIN32
    m_foregroundMonitor = std::make_unique<ForegroundWindowMonitor>(this);
    connect(m_foregroundMonitor.get(),
            &ForegroundWindowMonitor::foregroundChanged,
            this,
            &MainWindow::onForegroundStateChanged);
    connect(m_foregroundMonitor.get(),
            &ForegroundWindowMonitor::altModifierReleased,
            this,
            &MainWindow::onForegroundAltModifierReleased);
    m_profileSwitchCoordinator.setProfileManager(m_profileManager.get());
    m_profileSwitchCoordinator.setForegroundMonitor(m_foregroundMonitor.get());
    ProfileSwitchCoordinator::HostCallbacks profileSwitchHost;
    profileSwitchHost.isWorkspacePinned = [this] {
        return (m_workspaceHistory && m_workspaceHistory->isBusy())
            || (m_profileActions && m_profileActions->busy())
            || (m_shutdownCoordinator && (m_shutdownCoordinator->busy() || m_shutdownCoordinator->ready()))
            || FeatureHotkeyGate::isFeatureHotkeysBlocked() || QApplication::activePopupWidget();
    };
    profileSwitchHost.isSwitchingProfile = [this]() { return m_switchingProfile; };
    profileSwitchHost.isPipelineActive = [this]() { return m_profileSwitchPipelineActive; };
    profileSwitchHost.schedulePruneAbandonedEngines = [this]() { schedulePruneAbandonedEngines(); };
    profileSwitchHost.executeProfileSwitch = [this](const QString& profileId, bool automatic) {
        return executeProfileSwitch(profileId, automatic);
    };
    profileSwitchHost.applyForegroundCaptureHints = [this](HWND hwnd, const QString& title) {
        m_targetWindowController.applyForegroundCaptureHints(hwnd, title);
    };
    profileSwitchHost.onPipbongForegroundFocus = [this]() { handlePipbongForegroundFocus(); };
    profileSwitchHost.hasTriggerMonitoringSessions = [this]() {
        return hasTriggerMonitoringSessions();
    };
#ifdef _WIN32
    profileSwitchHost.flushDeferredProfileSwitchIfIdle = [this]() {
        if (!isAltTabModifierHeld()) {
            flushDeferredProfileSwitchIfIdle();
        }
    };
#else
    profileSwitchHost.flushDeferredProfileSwitchIfIdle = [this]() {
        flushDeferredProfileSwitchIfIdle();
    };
#endif
    m_profileSwitchCoordinator.setHostCallbacks(profileSwitchHost);

    m_targetWindowController.setProfileManager(m_profileManager.get());
    TargetWindowController::HostCallbacks targetHost;
    targetHost.isActiveDefaultProfile = [this]() { return isActiveDefaultProfile(); };
    targetHost.currentTargetWindowTitleW = [this]() { return currentTargetWindowTitleW(); };
    targetHost.scheduleAutoSave = [this]() { scheduleAutoSave(); };
    targetHost.syncEffectiveTargetWindowTitleToCapture = [this]() {
        syncEffectiveTargetWindowTitleToCapture();
    };
    targetHost.mayApplyCapture=[this]{return m_captureTargets && m_captureTargets->mayUpdateGlobal();};
    targetHost.mayPersist=[this]{return !FeatureHotkeyGate::isFeatureHotkeysBlocked() && !m_profileSwitchPipelineActive
        && (!m_workspaceHistory || !m_workspaceHistory->isBusy())
        && !(m_shutdownCoordinator && (m_shutdownCoordinator->busy() || m_shutdownCoordinator->ready()));};
    m_targetWindowController.setHostCallbacks(targetHost);
    setupCaptureTargets();
    setupForegroundSessions();

    TargetWindowDetailsController::Host detailHost;
    detailHost.current=[life=QPointer<MainWindow>(this)](const TargetWindowDetailsController::Request& source){
        return life && life->m_targetPanel && life->m_targetPanel->current(source);
    };
    detailHost.synchronizeIdleCapture = [this] { syncTargetWindowTitleToCapture(); };
#ifdef _WIN32
    detailHost.applyIdleTarget = [](HWND hwnd, bool sub) {
        if (!hwnd || !IsWindow(hwnd)) return;
        ScreenCapture::setTargetWindow(hwnd);
        if (sub) ScreenCapture::setForegroundHintWindow(hwnd);
    };
#endif
    detailHost.updateProcessPath = [this](const QString& id, const QString& title,
                                        const QString& path, bool sub) {
        if (!m_profileManager || id != m_profileManager->activeProfileId()) return false;
        return sub ? m_profileManager->updateProfileSubTargetBinding(id, title, path)
                   : m_profileManager->updateProfileTargetBinding(id, title, path);
    };
    detailHost.refreshProfiles = [this] { refreshProfileList(); };
    detailHost.workflowScaleChanged = [this] { if (m_workflowEditor) refreshWorkflowEditor(); };
    m_targetDetails.setHost(std::move(detailHost));


    m_runSessionController.setProject(m_project.get());
    m_runSessionController.setRunSessions(&runSessions());
    RunSessionController::HostCallbacks runHost;
    runHost.shouldSkipForegroundGateReconcile = [this]() {
        return m_profileSwitchPipelineActive || m_switchingProfile
               || ProgramSettings::runWithoutTargetWindow() || isActiveDefaultProfile()
               || RunLifecycleCoordinator::isHoldBurstUiActive(*this);
    };
    runHost.runForegroundGateActive = [this](Feature* feature) {
        return runForegroundGateActive(feature);
    };
    runHost.runForegroundGateActiveForSession = [this](FeatureRunSession& session, Feature* feature) {
        return runForegroundGateActiveForSession(session, feature);
    };
    runHost.updateRunUiState = [this]() {
        RunLifecycleCoordinator::requestRunUiRefresh(*this, false);
    };
    runHost.stopSessionEngine = [this](FeatureRunSession& session) {
        if (session.engine) {
            session.engine->stop();
        }
    };
    runHost.launchTriggerMonitor = [this](FeatureRunSession& session, Feature* feature, bool ui) {
        launchTriggerMonitor(session, feature, ui);
    };
    runHost.launchWorkflowRun = [this](FeatureRunSession& session, Feature* feature, bool repeat) {
        launchWorkflowRun(session, feature, repeat);
    };
    runHost.isHoldBindingDown = [this](const std::string& id) {
        return m_hotkeyManager && m_hotkeyManager->isHoldBindingStillActiveForRun(id);
    };
    runHost.featureForSession = [this](FeatureRunSession& session) {
        return featureForSession(session);
    };
    m_runSessionController.setHostCallbacks(runHost);
#endif
    m_autoSaveTimer = m_projectWorkspace->autoSaveTimer();

    m_packageController = std::make_unique<ProfilePackageController>(this);
    setupShutdownCoordinator();

    setupUi();
    AuxiliaryToolsController::Host auxiliaryHost;
    auxiliaryHost.featureRunning=[this]{return !runSessions().empty();};
    auxiliaryHost.warning=[this](const QString& message){showTransientStatus(message,5000);};
    m_auxiliaryTools=std::make_unique<AuxiliaryToolsController>(*m_profileManager,*this,std::move(auxiliaryHost));
    m_auxiliaryTools->setButtons(m_calculatorButton,m_spikeWatchButton,m_memoButton);
    WindowShellController::Host shellHost;
    shellHost.hideAuxiliary=[this]{m_auxiliaryTools->hideForTray();};
    shellHost.saveUiState=[this]{if(m_uiState)m_uiState->saveNow();};
    shellHost.checkpoint=[this]{return prepareForShutdown();};
    shellHost.lastError=[this]{return m_shutdownCoordinator->lastError();};
    shellHost.warning=[this](const QString& message){QMessageBox::warning(this,tr("종료 중단"),message);};
    m_windowShell=std::make_unique<WindowShellController>(*this,m_titleBar,m_alwaysOnTopCheck,std::move(shellHost));
    m_windowChrome=std::make_unique<WindowChromeController>(*this,m_titleBar);
    setupProgramSettings();
    m_profileListPresenter = std::make_unique<ProfileListPresenter>(*m_profileList, m_deleteProfileButton);
    setupWorkspaceEditor();
    setupUiState();
    setupMenus();
    setupTriggerMonitor();
    setupFeatureHotkeys();
    connectSignals();
    setupProjectWorkspace();
    setupTargetActions();
    setupTargetCenterPin();
    setupUpdateChecker();
    setupSessionMouseLocks();
    m_inputCleanup=std::make_unique<SessionInputCleanupController>();
    setupTriggerPreemption();
    setupWorkflowExecutionPresenter();

    setupRunUiPublisher();
    setupRunResources();
    setupHoldTapSessions();
    setupFirstTemplateRoi();

    m_runUiDebounceTimer = new QTimer(this);
    m_runUiDebounceTimer->setSingleShot(true);
    m_runUiDebounceTimer->setInterval(50);
    connect(m_runUiDebounceTimer, &QTimer::timeout, this, &MainWindow::applyRunUiState);

    m_targetPanel=std::make_unique<TargetPanelController>(*m_targetWindowDetailPanel,
        TargetPanelController::Controls{m_pickWindowButton,m_pickWindowListButton,m_pickSubWindowButton,m_pickSubWindowListButton,
            m_showTargetWindowButton,m_showSubTargetWindowButton,m_pinTargetWindowCenterButton,m_pinSubTargetWindowCenterButton},
        m_targetDetails,[this]{return targetDetailsRequest();},this);

    ProgramSettings::syncWindowsStartupRegistration();
    ProgramSettings::syncWindowsRunAsAdminRegistration();
    setupTrayIcon();

    m_profileManager->initialize();
    for(const auto& warning:m_profileManager->startupWarnings())appendLog(warning,LogLineKind::Warning);
    m_featureLibraryManager = std::make_unique<FeatureLibraryManager>(Application::dataDirectory());
    setupWorkspaceHistory();
    setupProfileActions();
    setupFeatureTransfers();
    ProgramSettings::applyProfileSettings(
        m_profileManager->loadSettings(m_profileManager->activeProfileId()));
    refreshProfileList();
    loadActiveProfile();
#ifdef _WIN32
    m_foregroundMonitor->start();
#endif
    syncMemoDialogProfile();
    refreshFeatureLibraryPanel();
    scheduleProfilePackageSeal();

#ifdef _WIN32
    syncEffectiveTargetWindowTitleToCapture();
#endif

    updateWindowTitle();
    syncHotkeys();
    updateTargetWindowDetails();
    scheduleRunWarmup();

    m_diagnostics=std::make_unique<WindowDiagnosticsController>(*this);
    if (QApplication* app = qobject_cast<QApplication*>(QCoreApplication::instance())) {
        app->installEventFilter(this);
    }
}

MainWindow::~MainWindow() {
    m_libraryActions.reset();
    if(m_featureTransfers)m_featureTransfers->retire();
    if(m_targetPanel)m_targetPanel->stop();
    m_diagnostics.reset();
#ifdef _WIN32
    if (m_foregroundMonitor) {
        m_foregroundMonitor->stop();
    }
#endif
    stopAllSessions();
    m_holdTapSessions.reset();
    m_workspaceHistory.reset();
    m_runResources.reset();
    m_firstTemplateRoi.reset();
    m_programSettings.reset();
    m_windowChrome.reset();
    m_windowShell.reset();
    m_auxiliaryTools.reset();
    m_uiState=nullptr;m_workspaceView.reset();
    if(m_triggerPreemption)m_triggerPreemption->shutdown();
    m_mouseLocks.reset();
}

void MainWindow::ensureInitialWindowPlacement() {
    if (!internalWinId()) {
        winId();
    }
    if (m_uiState) {
        m_uiState->restoreMainWindowGeometry();
    }
}

void MainWindow::showPendingCrashReportIfAny() {
    if (!CrashReporter::interactiveReportsAllowed()) return;
    CrashReportDialog::showPendingIfAny(this);
}


bool MainWindow::eventFilter(QObject* watched, QEvent* event) {
    if (event->type() == QEvent::MouseButtonRelease) {
        if (auto* button = qobject_cast<QAbstractButton*>(watched)) {
            const QString text = button->text().trimmed();
            const QString label = text.isEmpty()
                                      ? QString::fromUtf8(button->metaObject()->className())
                                      : QStringLiteral("%1 \"%2\"")
                                            .arg(QString::fromUtf8(button->metaObject()->className()), text);
            CrashReporter::noteUserAction(QStringLiteral("click %1").arg(label));
        }
    }
    return QMainWindow::eventFilter(watched, event);
}

void MainWindow::setupUi() {
    m_workspaceView=std::make_unique<MainWorkspaceView>(*this);
    const auto& ui=m_workspaceView->widgets();
    m_featureList=ui.m_featureList;
    m_profilePanel=ui.m_profilePanel;
    m_profileList=ui.m_profileList;
    m_addProfileButton=ui.m_addProfileButton;
    m_renameProfileButton=ui.m_renameProfileButton;
    m_deleteProfileButton=ui.m_deleteProfileButton;
    m_workflowEditor=ui.m_workflowEditor;
    m_mainHorizontalSplitter=ui.m_mainHorizontalSplitter;
    m_mainVerticalSplitter=ui.m_mainVerticalSplitter;
    m_bottomHorizontalSplitter=ui.m_bottomHorizontalSplitter;
    m_logPanel=ui.m_logPanel;
    m_pickWindowButton=ui.m_pickWindowButton;
    m_pickWindowListButton=ui.m_pickWindowListButton;
    m_pickSubWindowButton=ui.m_pickSubWindowButton;
    m_pickSubWindowListButton=ui.m_pickSubWindowListButton;
    m_showSubTargetWindowButton=ui.m_showSubTargetWindowButton;
    m_showTargetWindowButton=ui.m_showTargetWindowButton;
    m_pinTargetWindowCenterButton=ui.m_pinTargetWindowCenterButton;
    m_pinSubTargetWindowCenterButton=ui.m_pinSubTargetWindowCenterButton;
    m_alwaysOnTopCheck=ui.m_alwaysOnTopCheck;
    m_exitButton=ui.m_exitButton;
    m_updateButton=ui.m_updateButton;
    m_calculatorButton=ui.m_calculatorButton;
    m_spikeWatchButton=ui.m_spikeWatchButton;
    m_memoButton=ui.m_memoButton;
    m_settingsButton=ui.m_settingsButton;
    m_targetWindowDetailPanel=ui.m_targetWindowDetailPanel;
    m_titleBar=ui.m_titleBar;
}
void MainWindow::setupUiState() {
    m_uiState=m_workspaceView->restoreState();
    restoreAlwaysOnTopPreference();
}
void MainWindow::clampMainVerticalSplitterSizes() {m_workspaceView->clampVertical();}

void MainWindow::setupMenus() {
    DocumentActionsController::Host host;
    host.ready=[this]{return !m_switchingProfile && !m_profileSwitchPipelineActive
        && (!m_workspaceHistory || !m_workspaceHistory->isBusy())
        && !(m_shutdownCoordinator && (m_shutdownCoordinator->busy() || m_shutdownCoordinator->ready()))
        && !(m_updateController && m_updateController->installInProgress())
        && !FeatureHotkeyGate::isFeatureHotkeysBlocked();};
    host.directory=[] {return Application::instance()->projectDirectory();};
    host.checkpoint=[this](bool quiet){return maybeSave(quiet);};
    host.prepareImport=[this]{const QPointer<MainWindow> life(this);if(!m_projectWorkspace->saveSettings())return false;
        return life && (!m_auxiliaryTools || m_auxiliaryTools->saveMemo());};
    host.imported=[this]{const QPointer<MainWindow> life(this);refreshProfileList();if(!life)return;
        loadActiveProfile(false);if(life)syncProfileListSelection();};
    host.stopSessions=[this]{stopAllSessions();};host.update=[this]{onCheckForUpdates();};host.exit=[this]{onExitRequested();};
    host.log=[this](const QString& message,bool success){appendLog(message,success?LogLineKind::Success:LogLineKind::Info);};
    host.status=[this](const QString& message){showTransientStatus(message,2500);};
    m_documentActions=std::make_unique<DocumentActionsController>(*this,*m_profileManager,*m_projectWorkspace,*m_packageController,std::move(host));
    m_documentActions->installMenus(*(m_titleBar?m_titleBar->menuBar():menuBar()));
}

void MainWindow::connectSignals() {
    m_featureList->setProject(m_project.get());
    m_featureList->setActiveWorkflowRunQuery([this](const QString& featureId) {
        return isFeatureInActiveWorkflowRun(featureId.toStdString());
    });
    connect(m_featureList, &FeatureListPanel::selectionChanged, this, &MainWindow::onFeatureSelectionChanged);
    m_featureList->setMutationCheckpoint([this](const QString& reason) {
        return pushGlobalUiUndoSnapshot(reason);
    });
    connect(m_featureList, &FeatureListPanel::projectModified, this, &MainWindow::onProjectModified);
    connect(m_featureList, &FeatureListPanel::hotkeysChanged, this, &MainWindow::syncHotkeys);
    connect(m_featureList,
            &FeatureListPanel::saveFeatureToLibraryRequested,
            this,
            &MainWindow::onSaveFeatureToLibraryRequested);
    connect(m_featureList,
            &FeatureListPanel::importFeatureFromLibraryRequested,
            this,
            &MainWindow::onImportFeatureFromLibraryRequested);
    connect(m_featureList,
            &FeatureListPanel::importLibraryEntriesRequested,
            this,
            &MainWindow::onImportLibraryEntriesRequested);
    connect(m_featureList,
            &FeatureListPanel::deleteLibraryEntriesRequested,
            this,
            &MainWindow::onDeleteLibraryEntriesRequested);
    connect(m_featureList,
            &FeatureListPanel::featureRunRequested,
            this,
            &MainWindow::onFeatureRunRequested);
    connect(m_featureList,
            &FeatureListPanel::featureEnabledChanged,
            this,
            &MainWindow::onFeatureEnabledChanged);
    connect(m_featureList,
            &FeatureListPanel::featureDropped,
            this,
            &MainWindow::onFeatureDroppedOnFeatureList);
    connect(m_featureList,
            &FeatureListPanel::featureDroppedOnLibrary,
            this,
            &MainWindow::onFeatureDroppedOnLibrary);
    connect(m_featureList,
            &FeatureListPanel::libraryEntriesReordered,
            this,
            &MainWindow::onLibraryEntriesReordered);
    connect(m_featureList,
            &FeatureListPanel::libraryEntriesMultiReordered,
            this,
            &MainWindow::onLibraryEntriesMultiReordered);
    connect(m_featureList,
            &FeatureListPanel::libraryEntrySelected,
            this,
            &MainWindow::onLibraryEntrySelected);
    connect(m_profileList,
            &ProfileListWidget::featureDroppedOnProfile,
            this,
            &MainWindow::onFeatureDroppedOnProfile);
    connect(m_profileList,
            &QListWidget::currentRowChanged,
            this,
            &MainWindow::onProfileSelectionChanged);
    connect(m_profileList,
            &QListWidget::itemDoubleClicked,
            this,
            [this](QListWidgetItem* item) {
                if (!item || !m_profileManager) {
                    return;
                }
                const QString id = item->data(Qt::UserRole).toString();
                if (m_profileManager->isDefaultProfile(id)) {
                    return;
                }
                onRenameProfile();
            });
    connect(m_profileList,
            &ReorderableListWidget::rowsReordered,
            this,
            [this](int fromRow, int toRow) {
                if (!m_profileManager || !m_profileList || m_refreshingProfileList || fromRow == toRow) {
                    return;
                }
                QStringList orderedIds;
                orderedIds.reserve(m_profileList->count());
                for (int row = 0; row < m_profileList->count(); ++row) {
                    QListWidgetItem* item = m_profileList->item(row);
                    if (!item) {
                        continue;
                    }
                    orderedIds.push_back(item->data(Qt::UserRole).toString());
                }
                if (fromRow < 0 || toRow < 0 || fromRow >= orderedIds.size() || toRow >= orderedIds.size()) {
                    return;
                }
                const QString movedId = orderedIds.takeAt(fromRow);
                orderedIds.insert(toRow, movedId);
                if (!pushGlobalUiUndoSnapshot(QStringLiteral("profile-reorder"))) { refreshProfileList(); return; }
                if (m_profileManager->reorderProfiles(orderedIds)) {
                    showTransientStatus(tr("프로필 순서를 저장했습니다."), 1500);
                    refreshProfileList();
                }
            });
    connect(m_addProfileButton, &QPushButton::clicked, this, &MainWindow::onAddProfile);
    connect(m_renameProfileButton, &QPushButton::clicked, this, &MainWindow::onRenameProfile);
    connect(m_deleteProfileButton, &QPushButton::clicked, this, &MainWindow::onDeleteProfile);

    connect(m_hotkeyManager,
            &HotkeyManager::hotkeyTriggered,
            this,
            &MainWindow::onHotkeyTriggered,
            Qt::QueuedConnection);
    connect(m_hotkeyManager,
            &HotkeyManager::hotkeyHoldStarted,
            this,
            &MainWindow::onHotkeyHoldStarted,
            Qt::QueuedConnection);
    connect(m_hotkeyManager,
            &HotkeyManager::hotkeyHoldEnded,
            this,
            &MainWindow::onHotkeyHoldEnded,
            Qt::QueuedConnection);


    connect(m_workflowEditor, &WorkflowEditorPanel::workflowModified, this, &MainWindow::onProjectModified);
    connect(m_workflowEditor, &WorkflowEditorPanel::featureRunRequested, this, &MainWindow::onFeatureRunRequested);

    connect(m_exitButton, &QPushButton::clicked, this, &MainWindow::onExitRequested);
    connect(m_settingsButton, &QPushButton::clicked, this, &MainWindow::onProgramSettings);
    connect(m_updateButton, &QPushButton::clicked, this, &MainWindow::onUpdateButtonClicked);
    connect(m_calculatorButton, &QPushButton::clicked, this, &MainWindow::onCalculator);
    connect(m_spikeWatchButton, &QPushButton::clicked, this, &MainWindow::onSpikeWatch);
    connect(m_memoButton, &QPushButton::clicked, this, &MainWindow::onMemo);
    connect(m_alwaysOnTopCheck, &QCheckBox::toggled, this, &MainWindow::onAlwaysOnTopToggled);
    connect(m_pickWindowButton, &QToolButton::clicked, this, &MainWindow::onPickTargetWindow);
    connect(m_pickWindowListButton, &QToolButton::clicked, this, &MainWindow::onPickMainTargetWindowFromList);
    connect(m_pickSubWindowButton, &QToolButton::clicked, this, &MainWindow::onPickSubTargetWindow);
    connect(m_pickSubWindowListButton, &QToolButton::clicked, this, &MainWindow::onPickSubTargetWindowFromList);
    connect(m_showTargetWindowButton, &QToolButton::clicked, this, &MainWindow::onShowTargetWindow);
    connect(m_showSubTargetWindowButton, &QToolButton::clicked, this, &MainWindow::onShowSubTargetWindow);
    connect(m_pinTargetWindowCenterButton,
            &QToolButton::toggled,
            this,
            &MainWindow::onPinTargetWindowCenterToggled);
    connect(m_pinSubTargetWindowCenterButton,
            &QToolButton::toggled,
            this,
            &MainWindow::onPinSubTargetWindowCenterToggled);

    setupInputInterrupts();
    UserInputInterruptMonitor::instance().setHandler(
        [self = QPointer<MainWindow>(this)](const std::string& featureId, const std::shared_ptr<ExecutionContext>& context) {
            if (self) self->onUserInputInterrupt(featureId, context);
        });
    UserInputInterruptMonitor::instance().setHotkeyExemptionCheck(
        [self = QPointer<MainWindow>(this)](int virtualKey) {
            return self && self->m_hotkeyManager && self->m_hotkeyManager->matchesAnyRegisteredFeatureHotkey(virtualKey);
        });

    auto* globalUndoShortcut = new QShortcut(QKeySequence::Undo, this);
    globalUndoShortcut->setContext(Qt::WidgetWithChildrenShortcut);
    connect(globalUndoShortcut, &QShortcut::activated, this, &MainWindow::onGlobalUndoRequested);

    auto* globalRedoShortcut = new QShortcut(QKeySequence::Redo, this);
    globalRedoShortcut->setContext(Qt::WidgetWithChildrenShortcut);
    connect(globalRedoShortcut, &QShortcut::activated, this, &MainWindow::onGlobalRedoRequested);

    auto* globalRedoAltShortcut = new QShortcut(QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_Z), this);
    globalRedoAltShortcut->setContext(Qt::WidgetWithChildrenShortcut);
    connect(globalRedoAltShortcut, &QShortcut::activated, this, &MainWindow::onGlobalRedoRequested);

    FeatureHotkeyGate::setKeyboardHookDeferPredicate([this](int vkCode, bool keyDown) {
#ifdef _WIN32
        if (vkCode != VK_F2 || !keyDown) {
            return false;
        }
#else
        Q_UNUSED(vkCode);
        Q_UNUSED(keyDown);
        return false;
#endif
        return m_featureList && m_featureList->canInlineRenameFromShortcut();
    });

    auto* featureRenameShortcut = new QShortcut(QKeySequence(Qt::Key_F2), this);
    featureRenameShortcut->setContext(Qt::WindowShortcut);
    connect(featureRenameShortcut, &QShortcut::activated, this, [this]() {
        if (m_featureList) {
            m_featureList->requestInlineRename();
        }
    });
}

void MainWindow::onExitRequested() {
    requestApplicationQuit();
}

void MainWindow::setupTrayIcon() {m_windowShell->applyClosePolicy();}
void MainWindow::applyCloseToTrayPolicy() {m_windowShell->applyClosePolicy();}
void MainWindow::requestApplicationQuit() {m_windowShell->requestQuit();}

void MainWindow::setupUpdateChecker() {
    auto* checker = new UpdateChecker(this);
    checker->setInstallPreflight([this] { return m_shutdownCoordinator->checkpoint(); });
    UpdateUiController::HostCallbacks host;
    host.stopSessions = [this] { stopRunningSessionsForUpdate(); };
    host.hasActiveEngine = [this] {
        return (m_workspaceHistory && m_workspaceHistory->isBusy()) || hasAnyActiveWorkflowEngine();
    };
    host.autoInstallEnabled = [] { return ProgramSettings::autoInstallUpdates(); };
    host.showStatus = [this](const QString& text, int duration) { showTransientStatus(text, duration); };
    m_updateController = std::make_unique<UpdateUiController>(checker, m_updateButton, this, std::move(host), this);
    connect(checker, &UpdateChecker::readyToRestartForUpdate, this, &MainWindow::onReadyToRestartForUpdate);
    applyUpdateCheckInterval();
}

void MainWindow::applyUpdateCheckInterval() {
    if (m_updateController) m_updateController->applySettings(ProgramSettings::updateCheckIntervalMinutes());
}

void MainWindow::setupSessionMouseLocks() {
    m_mouseLocks = std::make_unique<SessionMouseLockController>(runSessions(), this);
    m_mouseLocks->setFailureHandler([this](const FeatureRunSession& session) {
        if (shouldLogRunDetails(session)) appendSessionLog(session,
            tr("초기 루프 마우스 잠금 해제: 블록 실패 %1회").arg(session.earlyLoopMouseLockFailureCount), LogLineKind::Warning);
    });
}
void MainWindow::captureFeatureMouseLockPosition(FeatureRunSession& s) {if(m_mouseLocks)m_mouseLocks->capturePosition(s);}
bool MainWindow::isEarlyLoopMouseLockWindow(const FeatureRunSession& s) const {return SessionMouseLockController::earlyWindow(s);}
bool MainWindow::engageEarlyLoopMouseLockAtBestPoint(FeatureRunSession& s) {return m_mouseLocks && m_mouseLocks->engageEarly(s);}
void MainWindow::updateEarlyLoopMouseLockFromMatch(FeatureRunSession& s) {if(m_mouseLocks)m_mouseLocks->updateFromMatch(s);}
void MainWindow::releaseEarlyLoopMouseLockIfEngaged(FeatureRunSession& s) {if(m_mouseLocks)m_mouseLocks->releaseEarly(s);}
void MainWindow::syncEarlyLoopMouseLock(FeatureRunSession& s) {if(m_mouseLocks)m_mouseLocks->syncEarly(s);}
void MainWindow::handleEarlyLoopMouseLockBlockFailure(FeatureRunSession& s, int) {if(m_mouseLocks)m_mouseLocks->blockFailed(s);}
bool MainWindow::hasFeatureMouseLock(const FeatureRunSession& s) {return SessionMouseLockController::hasFeatureLock(s);}
void MainWindow::engageFeatureMouseLock(FeatureRunSession& s) {if(m_mouseLocks)m_mouseLocks->engageFeature(s);}
void MainWindow::reconcileMouseLocksFromRunningSessions() {if(m_mouseLocks)m_mouseLocks->reconcile();}
void MainWindow::scheduleMouseLockPositionSync() {if(m_mouseLocks)m_mouseLocks->schedule();}
void MainWindow::syncMouseLockPositions() {if(m_mouseLocks)m_mouseLocks->tick();}

void MainWindow::onUpdateButtonClicked() {
    if (m_updateController) m_updateController->onButtonClicked();
}

void MainWindow::onCheckForUpdates() {
    if (m_updateController) m_updateController->checkInteractively();
}

void MainWindow::maybeStartAutomaticUpdate() {
    if (m_updateController) m_updateController->maybeStartAutomaticUpdate();
}

void MainWindow::onProgramSettings() {m_programSettings->open();}
void MainWindow::setupProgramSettings() {
    ProgramSettingsController::Host host;
    host.allowed=[this]{return m_profileManager->storageReady() && !m_shutdownCoordinator->busy() && !m_shutdownCoordinator->ready()
        && !m_profileSwitchPipelineActive && (!m_workspaceHistory || !m_workspaceHistory->isBusy()) && (!m_profileActions || !m_profileActions->busy());};
    host.source=[this]{return ProgramSettingsController::Source{m_project.get(),m_profileManager->activeProfileId(),m_projectWorkspace->generation()};};
    host.saveProfile=[this]{return saveActiveProfileSettings();};
    host.worker=[this]{synchronizeRunWorkerState();};host.closePolicy=[this]{applyCloseToTrayPolicy();};
    host.updatePolicy=[this]{applyUpdateCheckInterval();};
    host.logLimit=[this]{if(m_logPanel)m_logPanel->setMaxLines(ProgramSettings::logMaxLines());};
    host.handoff=[this](const auto& launch){return m_shutdownCoordinator->handoff(launch);};
    host.lastError=[this]{return m_shutdownCoordinator->lastError();};
    host.quit=[this]{m_windowShell->commitExit();QApplication::quit();};
    m_programSettings=std::make_unique<ProgramSettingsController>(*this,std::move(host));
}

void MainWindow::onCalculator() {m_auxiliaryTools->toggleCalculator();}
void MainWindow::onSpikeWatch() {m_auxiliaryTools->toggleSpikeWatch();}
void MainWindow::onMemo() {m_auxiliaryTools->toggleMemo();}
void MainWindow::syncMemoDialogProfile() {m_auxiliaryTools->syncMemoProfile();}

void MainWindow::onProfileSelectionChanged() {
    if (m_refreshingProfileList || !m_profileList || !m_profileManager) {
        return;
    }
    QListWidgetItem* item = m_profileList->currentItem();
    if (!item) {
        return;
    }
    switchToProfile(item->data(Qt::UserRole).toString());
}

void MainWindow::onAddProfile() {
    if (!m_profileActions) return;
    ProfileActionsController::Request request;
    request.action=ProfileActionsController::Action::Add;
    request.owner=m_profileManager->activeProfileId(); request.generation=m_projectWorkspace->generation();
    bool ok=false;
    request.edit.name=QInputDialog::getText(this,tr("프로필 추가"),tr("프로필 이름"),QLineEdit::Normal,tr("새 프로필"),&ok);
    if(ok)showProfileActionResult(m_profileActions->execute(request));
}

void MainWindow::onRenameProfile() {
    if (!m_profileManager || !m_profileList || !m_profileList->currentItem()) {
        return;
    }
    const QString id = m_profileList->currentItem()->data(Qt::UserRole).toString();
    const ProfileManager::Profile* profile = m_profileManager->profileById(id);
    if (!profile) {
        return;
    }
    ProfileActionsController::Request request;
    request.id=id; request.owner=m_profileManager->activeProfileId(); request.generation=m_projectWorkspace->generation();
    const bool fixedDefault = m_profileManager->isDefaultProfile(id);
    ProfileEditDialog dialog(profile->name,
                             m_profileManager->targetWindowTitle(id),
                             m_profileManager->subTargetWindowTitle(id),
                             m_profileManager->linkedTargetProcessPath(id),
                             m_profileManager->subLinkedTargetProcessPath(id),
                             fixedDefault,
                             fixedDefault,
                             QString::fromStdString(m_project ? m_project->targetWindowTitle() : std::string{}),
                             this);
    if (dialog.exec() != QDialog::Accepted) {
        return;
    }
    if (fixedDefault) {
        return;
    }
    const auto edited=dialog.result();
    request.edit={edited.name,edited.targetWindowTitle,edited.subTargetWindowTitle,
                  edited.linkedTargetProcessPath,edited.subLinkedTargetProcessPath,edited.defaultProfile};
    showProfileActionResult(m_profileActions->execute(request));
}

void MainWindow::onDeleteProfile() {
    if (!m_profileManager || !m_profileList || !m_profileList->currentItem()) {
        return;
    }
    const QString id = m_profileList->currentItem()->data(Qt::UserRole).toString();
    const ProfileManager::Profile* profile = m_profileManager->profileById(id);
    if (!profile) {
        return;
    }
    if (m_profileManager->isDefaultProfile(id)) {
        QMessageBox::information(this, tr("프로필 삭제"), tr("기본 프로필은 삭제할 수 없습니다."));
        return;
    }
    if (m_profileManager->profiles().size() <= 1) {
        QMessageBox::information(this, tr("프로필 삭제"), tr("마지막 프로필은 삭제할 수 없습니다."));
        return;
    }
    ProfileActionsController::Request request;
    request.action=ProfileActionsController::Action::Remove;
    request.id=id; request.owner=m_profileManager->activeProfileId(); request.generation=m_projectWorkspace->generation();
    const auto reply = QMessageBox::question(this,
                                             tr("프로필 삭제"),
                                             tr("'%1' 프로필을 삭제하시겠습니까?").arg(profile->name),
                                             QMessageBox::Yes | QMessageBox::No,
                                             QMessageBox::No);
    if(reply==QMessageBox::Yes)showProfileActionResult(m_profileActions->execute(request));
}

void MainWindow::showProfileActionResult(const ProfileActionsController::Result& result) {
    if (!result.message.isEmpty()) {
        showTransientStatus(result.message,7000);appendLog(result.message,LogLineKind::Warning);
    }
}

void MainWindow::setupProfileActions() {
    ProfileActionsController::Host host;
    const auto packagesPaused=std::make_shared<bool>(false);
    host.canBegin=[this]{return !m_switchingProfile && !m_profileSwitchPipelineActive
        && (!m_workspaceHistory || !m_workspaceHistory->isBusy())
        && !(m_shutdownCoordinator && (m_shutdownCoordinator->busy() || m_shutdownCoordinator->ready()));};
    host.generation=[this]{return m_projectWorkspace->generation();};
    host.checkpoint=[this,packagesPaused](const QString& reason,bool stopRuns){
        const QPointer<MainWindow> alive(this);
        const auto owner=m_profileManager->activeProfileId();const auto generation=m_projectWorkspace->generation();
        if(!m_packageController->pauseAndWait())return false;
        if(!alive)return false;
        *packagesPaused=true;
        if(stopRuns) {
            if(!maybeSave() || !saveActiveProfileSettings())return false;
            stopAllSessions();
            if(!alive)return false;
            if(owner!=m_profileManager->activeProfileId() || generation!=m_projectWorkspace->generation())return false;
        }
        return pushGlobalUiUndoSnapshot(reason);
    };
    host.changed=[this](const ProfileActionsController::Result& result){
        using Action=ProfileActionsController::Action;
        if(result.action==Action::Add) {refreshProfileList();switchToProfile(result.id);return;}
        if(result.action==Action::Remove) {
            m_packageController->discardPending(m_profileManager->profilePackagePath(result.id));
            refreshProfileList();loadActiveProfile();return;
        }
        if(result.id==m_profileManager->activeProfileId() && m_project) {
            m_project->setTargetWindowTitle(m_profileManager->targetWindowTitle(result.id).toStdString());
            ProgramSettings::applyProfileSettings(m_profileManager->loadSettings(result.id));
            syncEffectiveTargetWindowTitleToCapture();updateTargetWindowDetails();
            scheduleAutoSave();scheduleRunWarmup();syncTargetWindowCenterPin();
        }
        refreshProfileList();
    };
    host.finished=[this,packagesPaused]{
        if(*packagesPaused){*packagesPaused=false;if(!m_profileManager->storageReady())m_packageController->discardPending();m_packageController->resume();}
        schedulePostProfileSwitchForegroundReconcile(false);
    };
    m_profileActions=std::make_unique<ProfileActionsController>(*m_profileManager,std::move(host),this);
}

void MainWindow::onAlwaysOnTopToggled(bool checked) {m_windowShell->toggleTopmost(checked);}

void MainWindow::showEvent(QShowEvent* event) {
    QMainWindow::showEvent(event);
    if (m_updateController) m_updateController->onWindowShown();
    if (m_windowShell) m_windowShell->shown();
    if(m_windowChrome)m_windowChrome->shown();
}

void MainWindow::changeEvent(QEvent* event) {
    QMainWindow::changeEvent(event);
    if (event->type() == QEvent::ActivationChange && isActiveWindow() && m_foregroundMonitor) {
        m_foregroundMonitor->syncFromDesktopForeground();
        onForegroundStateChanged(m_foregroundMonitor->currentState());
    }
}

#if defined(Q_OS_WIN)
bool MainWindow::nativeEvent(const QByteArray& eventType, void* message, qintptr* result) {
    if(m_windowChrome && m_windowChrome->nativeEvent(eventType,message,result))return true;
    return QMainWindow::nativeEvent(eventType, message, result);
}
#endif

void MainWindow::restoreAlwaysOnTopPreference() {m_windowShell->restoreTopmost();}
void MainWindow::showTransientStatus(const QString& message,int timeoutMs) {m_windowShell->transient(message,timeoutMs);}
void MainWindow::setPersistentStatus(const QString& message) {m_windowShell->persistent(message);}
void MainWindow::clearStatusMessage() {m_windowShell->clearTransient();}

void MainWindow::onReadyToRestartForUpdate() {
    if (!prepareForShutdown()) {
        QMessageBox::warning(this,tr("업데이트 종료 중단"),m_shutdownCoordinator->lastError());
        return;
    }
    if(m_updateController)m_updateController->stop();
    m_windowShell->commitExit();
    hide();close();
    QTimer::singleShot(0,qApp,[]{QApplication::exit(0);});
}

void MainWindow::setupShutdownCoordinator() {
    ShutdownCoordinator::HostCallbacks host;
    host.canBegin = [this] { return (!m_workspaceHistory || !m_workspaceHistory->isBusy()) && (!m_profileActions || !m_profileActions->busy()); };
    host.quiesce = [this] {
        m_autoSaveTimer->stop();
        stopAllSessions();
        QCoreApplication::processEvents(QEventLoop::ExcludeUserInputEvents);
    };
    host.saveProject = [this] { return (!m_profileManager->storageReady() && !m_modified) || maybeSave(true); };
    host.saveSettings = [this] { return (!m_profileManager->storageReady() && !m_modified) || saveActiveProfileSettings(); };
    host.saveMemo = [this] { return !m_auxiliaryTools || m_auxiliaryTools->saveMemo(); };
    host.finalize = [this] { finalizeShutdown(); };
    m_shutdownCoordinator = std::make_unique<ShutdownCoordinator>(std::move(host));
}

bool MainWindow::prepareForShutdown() {
    return m_shutdownCoordinator->prepare();
}

void MainWindow::finalizeShutdown() {
    if(m_featureTransfers)m_featureTransfers->retire();
    if(m_targetPanel)m_targetPanel->stop();
    m_triggerMonitor->stop();
    m_triggerPreemption->shutdown();
    if(m_foregroundSessions)m_foregroundSessions->stop();
    if (m_updateController) m_updateController->stop();
    m_hotkeyManager->unregisterAll();
    if (m_holdKeyTapMux) {
        m_holdKeyTapMux->stopAll();
    }
    UserInputInterruptMonitor::instance().unregisterAll();
    if (m_mouseLocks) m_mouseLocks->shutdown();
    ScreenRegionOverlay::dismissAll();
    MatchTestOverlay::dismissAll();
    RoiPreviewOverlay::dismissAll();
    WorkflowMatchFeedbackOverlay::dismissAll();
    WorkflowRoiFlashOverlay::dismissAll();
    WorkflowImageFindSelectionRoiOverlay::dismissAll();
    TargetWindowHighlightOverlay::dismissAll();
    m_auxiliaryTools->closeForShutdown();
    if(m_centerPin)m_centerPin->stop();
    if(m_targetActions)m_targetActions->cancel();
    WindowPicker::cancelPick();
    WindowPickerHoverOverlay::dismissAll();
    CursorPositionPicker::cancelPick();
    ClickContinuousInputRecorder::endSession();
    m_autoSaveTimer->stop();
    saveSelectedFeaturePreference();
    sealActiveProfilePackage(true);
    pruneAbandonedEngines();
    AppStutterProfiler::stopAndWriteReport(QStringLiteral("app_shutdown"));
    if (m_uiState) {
        m_uiState->saveNow();
    }
}

void MainWindow::closeEvent(QCloseEvent* event) {
    if(!m_windowShell->handleClose()){event->ignore();return;}
    event->accept();QMainWindow::closeEvent(event);
}

void MainWindow::onNewProject() {m_documentActions->newProject();}

void MainWindow::onOpenProject() {m_documentActions->openProject();}

void MainWindow::onSaveProject() {m_documentActions->saveProject();}

void MainWindow::onSaveProjectAs() {m_documentActions->saveProjectAs();}

void MainWindow::onFeatureSelectionChanged() {m_workspaceEditor->selectionChanged();}
void MainWindow::saveSelectedFeaturePreference() {m_workspaceEditor->saveSelection();}
void MainWindow::restoreSelectedFeaturePreference() {m_workspaceEditor->restoreSelection();}
QString MainWindow::selectedFeaturePreferenceKey()const {return WorkspaceEditorController::preferenceKey(m_projectFilePath);}

void MainWindow::setupWorkspaceEditor() {
    WorkspaceEditorController::Host host;
    host.source=[this]{
        WorkspaceEditorController::Source source;source.project=m_project;source.owner=m_profileManager->activeProfileId();
        source.modelOwner=m_projectWorkspace->state().profileId;
        source.generation=m_projectWorkspace->generation();source.path=m_projectFilePath;source.directory=Application::instance()->projectDirectory();
        source.selected=m_featureList->selectedFeature();source.selectedId=m_featureList->selectedFeatureId();
        source.preview=m_libraryPreviewFeature;source.previewId=m_libraryPreviewEntryId;
        source.previewDirectory=m_featureLibraryManager?m_featureLibraryManager->entryProjectDirectory(m_libraryPreviewEntryId):source.directory;
        const auto* profile=m_profileManager->activeProfile();source.profileName=profile?(m_profileManager->isDefaultProfile(profile->id)?tr("기본"):profile->name):QString();return source;
    };
    host.session=[this](const auto& id){return sessionFor(id);};
    host.sessions=[this]{std::vector<std::string> ids;for(const auto& item:runSessions())ids.push_back(item.first);return ids;};
    host.pending=[this]{std::vector<std::string> ids;if(m_featureHotkeys)for(const auto& id:m_featureHotkeys->visibleQueuedStarts())ids.push_back(id.toStdString());return ids;};
    host.timing=[this](const auto* session){syncLoopTimingToWorkflowEditor(session);};
    host.clearPreview=[this]{m_libraryPreviewEntryId.clear();m_libraryPreviewFeature.reset();};
    host.autoSelect=ProgramSettings::autoSelectRunningFeature;
    host.changed=[this]{updateRunUiState();};
    m_workspaceEditor=std::make_unique<WorkspaceEditorController>(*m_workflowEditor,*m_featureList,std::move(host),this);
}

void MainWindow::onProjectModified() {
    synchronizeRunWorkerState();
    scheduleAutoSave();
    if (m_featureList) {
        if (Feature* feature = m_featureList->selectedFeature()) {
            requestSessionWorkflowRefresh(feature->id());
        }
    }
}

void MainWindow::scheduleAutoSave() { m_projectWorkspace->markModified(); }
bool MainWindow::ensureProjectFilePath() { return m_projectWorkspace->ensurePath(); }
void MainWindow::autoSaveProject(bool quiet) { m_projectWorkspace->save(quiet); }
void MainWindow::scheduleProfilePackageSeal() { m_projectWorkspace->scheduleSeal(); }
void MainWindow::sealActiveProfilePackage(bool synchronous) { m_projectWorkspace->seal(synchronous); }

void MainWindow::onImportProfilePackage() {m_documentActions->importPackage();}

void MainWindow::onExportProfilePackage() {m_documentActions->exportPackage();}

void MainWindow::refreshWorkflowEditor() {m_workspaceEditor->refresh();}
bool MainWindow::isDisplayedRunningFeature(const FeatureRunSession* session)const {return m_workspaceEditor && m_workspaceEditor->displayed(session);}

bool MainWindow::sessionBelongsToActiveProfile(const FeatureRunSession& session) const {
    if (!m_profileManager) {
        return true;
    }
    if (!session.profileId.isEmpty()) {
        return session.profileId == m_profileManager->activeProfileId();
    }
    return m_project && m_project->featureById(session.featureId) != nullptr;
}

Feature* MainWindow::featureForSession(FeatureRunSession& session) {
    return const_cast<Feature*>(static_cast<const MainWindow*>(this)->featureForSession(session));
}

const Feature* MainWindow::featureForSession(const FeatureRunSession& session) const {
    if (!session.profileId.isEmpty() && (!m_profileManager || !m_profileManager->profileById(session.profileId))) return nullptr;
    if (m_profileManager && !session.profileId.isEmpty()
        && session.profileId == m_profileManager->activeProfileId() && m_project) {
        return m_project->featureById(session.featureId);
    }
    if (!session.profileId.isEmpty() && m_profileManager) {
        auto ownedIt = m_sessionOwnerProjects.find(session.profileId);
        if (ownedIt == m_sessionOwnerProjects.end() || !ownedIt->second) {
            const QString path = m_profileManager->projectPath(session.profileId);
            QString projectDirectory;
            std::shared_ptr<Project> loaded =
                m_profileManager->cloneCachedProject(session.profileId, path, &projectDirectory);
            if (!loaded) {
                loaded = JsonSerializer::loadFromFile(path, &projectDirectory);
            }
            if (loaded) {
                ownedIt = m_sessionOwnerProjects.emplace(session.profileId, std::move(loaded)).first;
            }
        }
        if (ownedIt != m_sessionOwnerProjects.end() && ownedIt->second) {
            return ownedIt->second->featureById(session.featureId);
        }
        return nullptr;
    }
    return m_project ? m_project->featureById(session.featureId) : nullptr;
}

void MainWindow::captureRunStartCursorPosition(FeatureRunSession& session) {m_inputCleanup->captureCursor(session);}
void MainWindow::restoreRunStartCursorPosition(const FeatureRunSession& session) {m_inputCleanup->restoreCursor(session);}
void MainWindow::releaseHoldHotkeyInputToTarget(FeatureRunSession& session) {m_inputCleanup->releaseHold(session);}

void MainWindow::pruneSessionOwnerProjects() {
    if (m_sessionOwnerProjects.empty()) {
        return;
    }
    for (auto it = m_sessionOwnerProjects.begin(); it != m_sessionOwnerProjects.end();) {
        bool stillNeeded = false;
        for (const auto& entry : runSessions()) {
            if (entry.second.profileId == it->first) {
                stillNeeded = true;
                break;
            }
        }
        if (!stillNeeded && m_profileManager
            && it->first != m_profileManager->activeProfileId()) {
            it = m_sessionOwnerProjects.erase(it);
        } else {
            ++it;
        }
    }
}

void MainWindow::applyRunningBlockVisuals(FeatureRunSession& session,
                                          int index,
                                          BlockListWidget::ExecutionHighlight highlight) { m_executionPresenter->highlight(session,index,highlight); }

FeatureRunSession* MainWindow::sessionFor(const std::string& featureId) {
    return RunLifecycleCoordinator::sessionForId(runSessions(), featureId);
}

const FeatureRunSession* MainWindow::sessionFor(const std::string& featureId) const {
    return RunLifecycleCoordinator::sessionForId(runSessions(), featureId);
}

std::map<std::string, FeatureRunSession>& MainWindow::runSessions() {
    return m_runSessionRegistry.sessions();
}

const std::map<std::string, FeatureRunSession>& MainWindow::runSessions() const {
    return m_runSessionRegistry.sessions();
}

FeatureRunSession* MainWindow::sessionForEngine(const QObject* sender) {
    return RunLifecycleCoordinator::sessionForEngine(*this, sender);
}

bool MainWindow::isFeatureSessionActive(const FeatureRunSession& session) const {
    return SessionRunPolicy::isSessionActive(RunLifecycleCoordinator::policyInputFrom(session));
}

bool MainWindow::isFeatureRunning(const std::string& featureId) const {
    const FeatureRunSession* session = sessionFor(featureId);
    return session && isFeatureSessionActive(*session) && sessionBelongsToActiveProfile(*session);
}

bool MainWindow::isFeatureInActiveWorkflowRun(const std::string& featureId) const {
    const FeatureRunSession* session = sessionFor(featureId);
    if (!session || !sessionBelongsToActiveProfile(*session)) {
        return false;
    }
    return SessionRunPolicy::isInActiveWorkflowRun(RunLifecycleCoordinator::policyInputFrom(*session));
}

bool MainWindow::hasAnyRunningSession() const {
    return SessionRunPolicy::hasAnyRunningSession(
        RunLifecycleCoordinator::policyInputsFromSessions(runSessions()));
}

bool MainWindow::hasAnyActiveWorkflowEngine() const {
    return SessionRunPolicy::hasAnyActiveWorkflowEngine(
        RunLifecycleCoordinator::policyInputsFromSessions(runSessions()));
}

QSet<QString> MainWindow::activeWorkflowFeatureIds() const {
    QSet<QString> ids;
    for (const auto& entry : runSessions()) {
        if (!sessionBelongsToActiveProfile(entry.second)) {
            continue;
        }
        if (isFeatureInActiveWorkflowRun(entry.first)) {
            ids.insert(QString::fromStdString(entry.first));
        }
    }
    return ids;
}

QSet<QString> MainWindow::runningFeatureIds() const {
    QSet<QString> ids;
    for (const auto& entry : runSessions()) {
        if (!sessionBelongsToActiveProfile(entry.second)) {
            continue;
        }
        if (isFeatureRunHighlighted(entry.second)) {
            ids.insert(QString::fromStdString(entry.first));
        }
    }
    for (const std::string& pendingId : m_pendingHoldFeatureStartIds) {
        ids.insert(QString::fromStdString(pendingId));
    }
    return ids;
}

bool MainWindow::isFeatureRunHighlighted(const FeatureRunSession& session) const {
    if (session.runningMode == FeatureRunMode::Hold) {
        return session.holdRunActive;
    }
    return isFeatureSessionActive(session);
}

QHash<QString, FeatureRunVisualKind> MainWindow::buildFeatureListRunVisualKinds()const {return m_workspaceEditor->visuals();}

void MainWindow::refreshFeatureListHoldVisuals() {
    if (!m_featureList) {
        return;
    }
    const QSet<QString> runningIds = runningFeatureIds();
    m_featureList->beginRunStateBatch();
    if (!runningIds.isEmpty()) {
        m_featureList->setRunAnimationLowCpu(false);
    }
    m_featureList->setRunningFeatureIds(runningIds);
    m_featureList->setFeatureRunVisualKinds(buildFeatureListRunVisualKinds());
    m_featureList->endRunStateBatch();
}

void MainWindow::scheduleFeatureListHoldVisualRefresh() {
    if (m_holdBurstDepth == 0 && !shouldCoalesceRunUiUpdates()) {
        refreshFeatureListHoldVisuals();
        return;
    }
    if (m_holdVisualRefreshScheduled) {
        return;
    }
    m_holdVisualRefreshScheduled = true;
    QTimer::singleShot(0, this, [this]() {
        m_holdVisualRefreshScheduled = false;
        refreshFeatureListHoldVisuals();
    });
}

QString MainWindow::featureDisplayName(const std::string& featureId) const {
    if(const auto* session=sessionFor(featureId)) {
        if(!session->startedFeatureName.empty())return QString::fromStdString(session->startedFeatureName);
        if(const auto* owned=featureForSession(*session))return QString::fromStdString(owned->name());
        return QString::fromStdString(featureId);
    }
    if (!m_project) {
        return QString::fromStdString(featureId);
    }
    if (Feature* feature = m_project->featureById(featureId)) {
        return QString::fromStdString(feature->name());
    }
    return QString::fromStdString(featureId);
}

void MainWindow::appendSessionLog(const FeatureRunSession& session,
                                  const QString& message,
                                  LogLineKind kind) {
    const QString featureName = featureDisplayName(session.featureId);
    DiagnosticHub::appendAppLogSession(featureName, diagnosticLogLevelTag(kind), message);
    if (m_logPanel) {
        m_logPanel->appendSessionLine(featureName, kind, message);
    }
}

void MainWindow::connectSessionEngine(FeatureRunSession& session) {
    WorkflowEngine* engine = session.engine.get();
    if (!engine) {
        return;
    }

    connect(engine, &WorkflowEngine::logMessage, this, &MainWindow::onEngineLog,
            Qt::QueuedConnection);
    connect(engine, &WorkflowEngine::sessionPrepared, this, &MainWindow::onEngineSessionPrepared,
            Qt::QueuedConnection);
    connect(engine, &WorkflowEngine::started, this, &MainWindow::onEngineStarted,
            Qt::QueuedConnection);
    connect(engine, &WorkflowEngine::finished, this, &MainWindow::onEngineFinished,
            Qt::QueuedConnection);
    connect(engine, &WorkflowEngine::blockStarted, this, &MainWindow::onBlockStarted,
            Qt::QueuedConnection);
    connect(engine, &WorkflowEngine::blockFinished, this, &MainWindow::onBlockFinished,
            Qt::QueuedConnection);
    connect(engine, &WorkflowEngine::blockProgress, this, &MainWindow::onBlockProgress,
            Qt::QueuedConnection);
    connect(engine, &WorkflowEngine::blockMatchResult, this, &MainWindow::onBlockMatchResult,
            Qt::QueuedConnection);
    connect(engine, &WorkflowEngine::blockImageFindAttempt, this, &MainWindow::onBlockImageFindAttempt,
            Qt::QueuedConnection);
    connect(engine, &WorkflowEngine::imageFindFailureHandling, this,
            &MainWindow::onImageFindFailureHandling, Qt::QueuedConnection);
    connect(engine, &WorkflowEngine::imageFindReturnToPrevious, this,
            &MainWindow::onImageFindReturnToPrevious, Qt::QueuedConnection);
    connect(engine, &WorkflowEngine::pointerFeedbackAtClientPoint, this,
            &MainWindow::onPointerFeedbackAtClientPoint, Qt::QueuedConnection);
}

void MainWindow::updateRunUiState(bool immediate) {
    RunLifecycleCoordinator::requestRunUiRefresh(*this, immediate);
}

bool MainWindow::shouldCoalesceRunUiUpdates() const {
    return RunLifecycleCoordinator::shouldCoalesceRunUi(runSessions(), false);
}

int MainWindow::concurrentActiveRepeatSessionCount() const {
    return SessionRunPolicy::countActiveRepeatSessions(
        RunLifecycleCoordinator::policyInputsFromSessions(runSessions()));
}

void MainWindow::applyRunUiState() {
    assertMainWindowOnGuiThread("applyRunUiState");
    RunLifecycleCoordinator::applyRunUiState(*this);
}

void MainWindow::abandonSessionEngine(FeatureRunSession& session) {
    RunLifecycleCoordinator::abandonSessionEngine(*this, session);
}

void MainWindow::stopFeatureRun(const std::string& featureId) {
    RunLifecycleCoordinator::stopFeatureRun(*this, featureId);
}

void MainWindow::stopRunningSessionsForUpdate() {
    if (!hasAnyActiveWorkflowEngine()) {
        return;
    }
    stopAllSessions();
    showTransientStatus(tr("실행을 중지하고 업데이트를 진행합니다."), 3000);
}

void MainWindow::stopAllSessions() {
    RunLifecycleCoordinator::stopAllSessions(*this);
}

void MainWindow::detachUiForProfileSwitch() {
    RunLifecycleCoordinator::detachUiForProfileSwitch(*this);
}

bool MainWindow::hasTriggerMonitoringSessions() const {
    for (const auto& entry : runSessions()) {
        if (isTriggerMonitoring(entry.second)) {
            return true;
        }
    }
    return false;
}

void MainWindow::schedulePruneAbandonedEngines() {
    RunLifecycleCoordinator::schedulePruneAbandonedEngines(*this);
}

void MainWindow::pruneAbandonedEngines() {
    assertMainWindowOnGuiThread("pruneAbandonedEngines");
    RunLifecycleCoordinator::pruneAbandonedEngines(*this);
}

void MainWindow::flushDeferredProfileSwitchIfIdle() {
    m_profileSwitchCoordinator.flushDeferredProfileSwitch();
}

void MainWindow::onFeatureRunRequested(const QString& featureId) {
    if (!m_project || featureId.isEmpty()) {
        return;
    }
    Feature* feature = m_project->featureById(featureId.toStdString());
    if (!feature) {
        return;
    }
    if (!feature->enabled()) {
        return;
    }
    if (feature->runMode() == FeatureRunMode::Hold) {
        QMessageBox::information(this, tr("실행"),
                                 tr("홀드 방식은 단축키를 누르고 있는 동안 워크플로가 무한 반복됩니다. 키를 떼면 중지됩니다."));
        return;
    }
    if (isFeatureRunning(feature->id())) {
        stopFeatureRun(feature->id());
        return;
    }
    startFeatureRun(feature);
}

void MainWindow::onFeatureEnabledChanged(const QString& featureId, bool enabled) {
    if (!enabled && isFeatureRunning(featureId.toStdString())) {
        stopFeatureRun(featureId.toStdString());
    }
}

void MainWindow::onSaveFeatureToLibraryRequested(const QString& id) {if(m_libraryActions)m_libraryActions->save(id);}
void MainWindow::onImportFeatureFromLibraryRequested() {if(m_libraryActions)m_libraryActions->chooseImport();}
void MainWindow::onImportLibraryEntriesRequested(const QStringList& ids) {importLibraryEntries(ids);}
void MainWindow::onLibraryEntrySelected(const QString& id) {if(m_libraryActions)m_libraryActions->preview(id);}
void MainWindow::onDeleteLibraryEntriesRequested(const QStringList& ids) {if(m_libraryActions)m_libraryActions->remove(ids);}

void MainWindow::setupFeatureTransfers() {
    FeatureTransferController::HostCallbacks host;
    host.activeProject = [this] { return m_project; };
    host.generation=[this]{return m_projectWorkspace->generation();};
    host.activeDirectory=[this]{return m_projectWorkspace->state().directory;};
    host.stopFeature = [this](const QString& profile,const QString& id) {
        const auto* session=sessionFor(id.toStdString());
        if(session && session->profileId==profile && isFeatureRunning(id.toStdString()))stopFeatureRun(id.toStdString());
    };
    host.activeChanged = [this](const QString& selectedId) {
        if (m_featureList) {
            m_featureList->refresh();
            if (!selectedId.isEmpty()) m_featureList->selectFeatureById(selectedId);
            else onFeatureSelectionChanged();
        }
        syncHotkeys();
        scheduleAutoSave();
    };
    host.profileSaved = [this](const QString& id) {
        m_packageController->schedule(m_profileManager->profileDirectory(id), m_profileManager->profilePackagePath(id));
    };
    host.libraryChanged = [this] { refreshFeatureLibraryPanel(); };
    host.warning = [this](const QString& text) { appendLog(text, LogLineKind::Warning); };
    m_featureTransfers = std::make_unique<FeatureTransferController>(*m_profileManager, *m_featureLibraryManager, std::move(host));
    FeatureBatchController::Host batches;
    batches.canBegin = [this] {
        return m_project && !m_switchingProfile && !m_profileSwitchPipelineActive
            && m_projectWorkspace->state().profileId == m_profileManager->activeProfileId()
            && (!m_workspaceHistory || !m_workspaceHistory->isBusy())
            && !(m_shutdownCoordinator && (m_shutdownCoordinator->busy() || m_shutdownCoordinator->ready()));
    };
    batches.generation = [this] { return m_projectWorkspace->generation(); };
    batches.activeProject = [this] { return m_project.get(); };
    batches.checkpoint = [this](const QString& reason) { return pushGlobalUiUndoSnapshot(reason); };
    batches.activeChanged = [this](const QString& selectedId) {
        m_featureList->refresh();
        if (!selectedId.isEmpty()) m_featureList->selectFeatureById(selectedId);
        syncHotkeys(); scheduleAutoSave();
    };
    batches.libraryChanged = [this] { refreshFeatureLibraryPanel(); };
    m_featureBatches = std::make_unique<FeatureBatchController>(*m_profileManager, *m_featureLibraryManager,
                                                              *m_featureTransfers, std::move(batches), this);
    FeatureLibraryActionsController::Host libraryHost;
    libraryHost.ready=[this]{return m_project && !m_switchingProfile && !m_profileSwitchPipelineActive
        && (!m_workspaceHistory || !m_workspaceHistory->isBusy())
        && !(m_shutdownCoordinator && (m_shutdownCoordinator->busy() || m_shutdownCoordinator->ready()));};
    libraryHost.previewId=[this]{return m_libraryPreviewEntryId;};
    libraryHost.preview=[this](const QString& id,std::shared_ptr<Feature> feature){
        m_libraryPreviewEntryId=id;m_libraryPreviewFeature=std::move(feature);refreshWorkflowEditor();};
    libraryHost.status=[this](const QString& text,int ms){showTransientStatus(text,ms);};
    libraryHost.warning=[this](const QString& text){appendLog(text,LogLineKind::Warning);};
    m_libraryActions=std::make_unique<FeatureLibraryActionsController>(*this,*m_profileManager,*m_featureLibraryManager,
        *m_projectWorkspace,*m_featureBatches,*m_featureList,std::move(libraryHost));
}

void MainWindow::onFeatureDroppedOnFeatureList(const QMimeData* mime,int index) {if(m_libraryActions)m_libraryActions->dropOnFeatureList(mime,index);}
void MainWindow::onFeatureDroppedOnLibrary(const QMimeData* mime) {if(m_libraryActions)m_libraryActions->dropOnLibrary(mime);}
void MainWindow::onLibraryEntriesReordered(int from,int to) {if(m_libraryActions)m_libraryActions->reorder(from,to);}
void MainWindow::onLibraryEntriesMultiReordered(const QList<int>& rows,int index) {if(m_libraryActions)m_libraryActions->reorder(rows,index);}
void MainWindow::onFeatureDroppedOnProfile(const QString& id,const QMimeData* mime) {if(m_libraryActions)m_libraryActions->dropOnProfile(id,mime);}
bool MainWindow::importLibraryEntry(const QString& id) {return importLibraryEntries({id});}
bool MainWindow::importLibraryEntries(const QStringList& ids) {return m_libraryActions && m_libraryActions->importEntries(ids);}
void MainWindow::refreshFeatureLibraryPanel() {if(m_libraryActions)m_libraryActions->refreshPanel();}

void MainWindow::selectRunningFeatureForDisplay(Feature* feature) {m_workspaceEditor->selectRunning(feature);}

void MainWindow::startFeatureRun(Feature* feature, bool fromHotkey, bool skipTargetActivationOnStart) {
    if(!m_profileManager->storageReady()){showTransientStatus(tr("프로필 복구가 끝난 뒤 실행해 주세요."),5000);return;}
    AppStutterOperationScope stutterScope(
        "run.start",
        feature ? QString::fromStdString(feature->name()) : QStringLiteral("(null)"));
    RunLifecycleCoordinator::startFeatureRun(*this, feature, fromHotkey, skipTargetActivationOnStart);
}

bool MainWindow::tryBeginFirstTemplateRoiEdit(FeatureRunSession& session,Feature* feature) {return m_firstTemplateRoi->begin(session,feature);}

void MainWindow::setupFirstTemplateRoi() {
    FirstTemplateRoiController::Host host;
    host.workspace=[this]{return FirstTemplateRoiController::Workspace{m_project.get(),m_profileManager->activeProfileId(),m_projectWorkspace->generation()};};
    host.session=[this](const std::string& id){return sessionFor(id);};
    host.cancel=[this](const std::string& id){RunLifecycleCoordinator::tearDownAndEraseSessionEntry(*this,id);RunLifecycleCoordinator::requestRunUiRefresh(*this,false);};
    host.changed=[this]{m_modified=true;scheduleAutoSave();refreshWorkflowEditor();};
    host.launch=[this](FeatureRunSession& session,Feature& feature){
        if(feature.runMode()==FeatureRunMode::Trigger){persistTriggerArmedState(QString::fromStdString(session.featureId),true);launchTriggerMonitor(session,&feature,true);}
        else launchWorkflowRun(session,&feature,false);
    };
    m_firstTemplateRoi=std::make_unique<FirstTemplateRoiController>(*this,std::move(host));
}

void MainWindow::refreshSessionWorkflowFromProject(FeatureRunSession& s,Feature* f) {m_runResources->refreshWorkflow(s,f);}
void MainWindow::applyDeferredSessionWorkflowRefresh(FeatureRunSession& s,Feature* f) {m_runResources->applyDeferred(s,f);}
void MainWindow::requestSessionWorkflowRefresh(const std::string& id) {m_runResources->requestRefresh(id);}
void MainWindow::ensureRunSessionResources(FeatureRunSession& s,Feature* f,bool refresh,bool defer) {m_runResources->ensure(s,f,refresh,defer);}
void MainWindow::syncRunSessionContext(FeatureRunSession& s,bool refresh) {m_runResources->syncContext(s,refresh);}
void MainWindow::applyFeatureRunPoliciesToContext(FeatureRunSession& s,Feature* f) {m_runResources->applyPolicies(s,f);}

void MainWindow::setupRunResources() {
    RunSessionResourcesController::Host host;
    host.workspace=[this]{return RunSessionResourcesController::Workspace{m_project.get(),m_profileManager->activeProfileId(),m_projectWorkspace->generation()};};
    host.sessions=[this]{std::vector<std::string> ids;for(const auto& entry:runSessions())ids.push_back(entry.first);return ids;};
    host.session=[this](const std::string& id){return sessionFor(id);};
    host.feature=[this](FeatureRunSession& s){return featureForSession(s);};
    host.binding=[this](FeatureRunSession& s,bool refresh){
        RunSessionResourcesController::Binding binding;binding.title=sessionCaptureTargetTitleW(s,refresh);
        const auto* feature=featureForSession(s);binding.process=feature?RunLifecycleCoordinator::captureTargetProcessPath(*this,s,*feature,binding.title):std::wstring();
        binding.directory=m_profileManager && !s.profileId.isEmpty()?m_profileManager->projectDirectory(s.profileId).toStdString():Application::instance()->projectDirectory().toStdString();return binding;
    };
    host.targetAllowed=[this](FeatureRunSession& s,Feature* feature){
        if(!feature)return false;
        if(sessionBelongsToActiveProfile(s)){
            if(!ProgramSettings::runWithoutTargetWindow() && !isActiveDefaultProfile())return runForegroundGateActive(feature);
            if(feature->requireScopedTargetForeground() && feature->captureTargetScope()!=FeatureCaptureTargetScope::Auto)return scopedTargetForegroundActive(feature);
        }return true;
    };
    host.captureAllowed=[this](const FeatureRunSession& s){return !hasAnyCapturingWorkflowBurstExcept(s.featureId);};
    host.repeatAllowed=[this](FeatureRunSession& s,Feature* feature){return feature && runForegroundGateActiveForSession(s,feature);};
    host.holdReader=[this](const std::string& id){return m_hotkeyManager?m_hotkeyManager->holdStateReader(id):std::function<bool()>{};};
    host.warning=[this](const QString& message){showTransientStatus(message,2500);};
    host.afterBatch=[this](FeatureRunSession& s,Feature* owner){
        if(owner && s.engine && s.engine->isRunning() && (s.lockMouseDuringFirstLoopCount>0 || hasFeatureMouseLock(s)) && concurrentActiveRepeatSessionCount()<=1)syncEarlyLoopMouseLock(s);
    };
    m_runResources=std::make_unique<RunSessionResourcesController>(*m_runUiPublisher,std::move(host),this);
}

void MainWindow::setupRunUiPublisher() {
    RunUiPublisher::HostCallbacks host;
    host.session = [this](const std::string& id) { return sessionFor(id); };
    host.activeRepeatCount = [this] { return concurrentActiveRepeatSessionCount(); };
    host.isDisplayed = [this](const FeatureRunSession* session) { return isDisplayedRunningFeature(session); };
    host.timing = [this](std::optional<RunUiPublisher::Timing> value) {
        if (!m_workflowEditor) return;
        if (!value) m_workflowEditor->clearLoopTiming();
        else m_workflowEditor->setLoopTiming(value->number, value->elapsedMs, value->averageMs, value->success);
    };
    host.log = [this](const FeatureRunSession& session, const QString& text, bool success) {
        appendSessionLog(session, text, success ? LogLineKind::Success : LogLineKind::Warning);
    };
    host.afterBatch = [this](FeatureRunSession& session){if(m_runResources)m_runResources->afterBatch(session);};
    host.refreshRunUi = [this] {
        if (m_runUiDebounceTimer && !m_runUiDebounceTimer->isActive()) m_runUiDebounceTimer->start();
    };
    m_runUiPublisher = std::make_unique<RunUiPublisher>(std::move(host), this);
}

void MainWindow::flushWorkerFastRepeatUi(const std::string& featureId) {
    const QPointer<MainWindow> life(this);
    m_runUiPublisher->flush(featureId);
    if(!life)return;
    if (auto* session = sessionFor(featureId); session && session->workerRepeat)
        session->consecutiveDetectionFailLoops = session->workerRepeat->consecutiveFailures();
}

void MainWindow::configureWorkerFastRepeat(FeatureRunSession& s,Feature* f) {m_runResources->configureRepeat(s,f);}
void MainWindow::synchronizeRunWorkerState() {if(m_runResources)m_runResources->synchronize();}

void MainWindow::publishLoopCompletionUi(FeatureRunSession& session, bool success, const QString& message) {
    m_runUiPublisher->publish(session, success, message);
}

void MainWindow::logLoopCompletion(FeatureRunSession& session, bool success, const QString& message) {
    m_runUiPublisher->completeLoop(session, success, message);
}

void MainWindow::syncLoopTimingToWorkflowEditor(const FeatureRunSession* session) {
    m_runUiPublisher->syncTiming(session);
}

bool MainWindow::shouldLogRunDetails(const FeatureRunSession& session) const {
    if (!RunLifecycleCoordinator::shouldLogSessionDetailsInBurst(*this)) {
        return false;
    }
    return !session.repeatSession || session.sessionIteration <= 0;
}

void MainWindow::continueRepeatSession(FeatureRunSession& session,
                                       Feature* feature,
                                       bool success,
                                       const QString& message) {
    RunLifecycleCoordinator::continueRepeatSession(*this, session, feature, success, message);
}

void MainWindow::launchWorkflowRun(FeatureRunSession& session, Feature* feature, bool repeatIteration) {
    if(session.usesHoldKeyTapFastPath){
        int key=0;if(feature)holdKeyTapWorkflowVirtualKey(*feature,key);
        launchHoldKeyTapRun(session,feature,key);return;
    }
    RunLifecycleCoordinator::launchWorkflowRun(*this, session, feature, repeatIteration);
}

void MainWindow::launchHoldKeyTapRun(FeatureRunSession& session, Feature* feature, int virtualKey) {
    m_holdTapSessions->launch(session,feature,virtualKey);
}

void MainWindow::setupHoldTapSessions() {
    HoldTapSessionController::Host host;
    host.session=[this](const auto& id){return sessionFor(id);};
    host.feature=[this](auto& s){return featureForSession(s);};
    host.activeOwner=[this](const auto& s){return sessionBelongsToActiveProfile(s);};
    host.displayed=[this](const auto& s){return isDisplayedRunningFeature(&s);};
    host.logDetails=[this](const auto& s){return shouldLogRunDetails(s);};
    host.deferForeground=[this](auto& s,auto* f){return deferRunUntilScopedTargetForeground(s,f);};
    host.syncContext=[this](auto& s,auto*){syncRunSessionContext(s);};
    host.applyPolicies=[this](auto& s,auto* f){applyFeatureRunPoliciesToContext(s,f);};
    host.syncInterrupt=[this](auto& s,auto* f){syncUserInputInterruptForSession(s,f);};
    host.holdReader=[this](const auto& id){return m_hotkeyManager?m_hotkeyManager->holdStateReader(id):std::function<bool()>{};};
    host.synchronize=[this]{synchronizeRunWorkerState();};
    host.select=[this](auto* f){selectRunningFeatureForDisplay(f);};
    host.decorate=[this](auto& s,auto current){
        const int index=s.runningBlockIndex;const auto highlight=s.runningBlockHighlight;
        syncLoopTimingToWorkflowEditor(&s);if(!current() || !m_workflowEditor)return;
        m_workflowEditor->clearBlockMatchResults();if(!current())return;
        m_workflowEditor->clearExecutionHighlight();if(!current())return;
        if(index>=0 && highlight!=BlockListWidget::ExecutionHighlight::None)m_workflowEditor->setActiveBlockIndex(index,highlight);
    };
    host.logStart=[this](auto& s){appendSessionLog(s,tr("기능 실행을 시작합니다"),LogLineKind::Accent);};
    host.persist=[this]{if(m_workflowEditor)m_workflowEditor->persistRunFeedbackForCurrentFeature();};
    host.refresh=[this]{RunLifecycleCoordinator::requestRunUiRefresh(*this,false);};
    host.scheduleStart=[this](const auto& id){RunLifecycleCoordinator::scheduleCoalescedHoldStartUi(*this,id);};
    host.scheduleEnd=[this](const auto& id){RunLifecycleCoordinator::scheduleCoalescedHoldFeatureEndFinish(*this,id);};
    host.deferStopped=[this]{return runSessions().size()>1 || RunLifecycleCoordinator::isHoldBurstUiActive(*this) || !m_pendingHoldFeatureEndFinishes.empty();};
    host.finish=[this](const auto& id,bool success,const auto& message,bool defer){finishRunSession(id,success,message,defer);};
    m_holdTapSessions=std::make_unique<HoldTapSessionController>(*m_holdKeyTapMux,std::move(host),this);
}

bool MainWindow::shouldContinueRunSession(const FeatureRunSession& session, Feature* feature) {
    if (!feature) {
        return false;
    }
    bool holdBindingStillActive = false;
    if (session.runningMode == FeatureRunMode::Hold && m_hotkeyManager) {
        holdBindingStillActive =
            m_hotkeyManager->isHoldBindingStillActiveForRun(session.featureId);
    }
    return RunLifecycleCoordinator::shouldContinueSession(session, holdBindingStillActive);
}

void MainWindow::reconcileHoldLatchForActiveHoldSessions() {
    if (!m_hotkeyManager) {
        return;
    }
    for (const auto& entry : runSessions()) {
        const FeatureRunSession& session = entry.second;
        if (session.runningMode != FeatureRunMode::Hold || !session.holdRunActive) {
            continue;
        }
        if (session.sessionContext && session.sessionContext->suppressRepeatUi()
            && session.engine && session.engine->isRunning()) {
            continue;
        }
        m_hotkeyManager->reconcileHoldBindingDown(session.featureId);
    }
}

void MainWindow::scheduleRepeatIteration(FeatureRunSession& session,
                                         Feature* feature,
                                         bool success,
                                         const QString& message) {
    RunLifecycleCoordinator::scheduleRepeatIteration(*this, session, feature, success, message);
}

void MainWindow::scheduleEnsureTriggerMonitorEnginesRunning() {m_triggerMonitor->scheduleEnsure();}
void MainWindow::ensureTriggerMonitorEnginesRunning() {m_triggerMonitor->ensure();}

void MainWindow::scheduleDeferredTriggerMonitorRestart(FeatureRunSession& session,
                                                       Feature* feature,
                                                       int delayMs) {
    RunLifecycleCoordinator::scheduleDeferredTriggerMonitorRestart(*this, session, feature, delayMs);
}

void MainWindow::launchTriggerMonitor(FeatureRunSession& session, Feature* feature, bool firstSessionStart) {
    RunLifecycleCoordinator::launchTriggerMonitor(*this, session, feature, firstSessionStart);
}

void MainWindow::launchTriggerActionRun(FeatureRunSession& session, Feature* feature) {
    RunLifecycleCoordinator::launchTriggerActionRun(*this, session, feature);
}

void MainWindow::pauseOtherSessionsForTrigger(FeatureRunSession& session) {m_triggerPreemption->pause(session);}
void MainWindow::resumePreemptedSessionsForTrigger(FeatureRunSession& session) {m_triggerPreemption->resume(session);}

void MainWindow::setupTriggerPreemption() {
    m_triggerPreemption=std::make_unique<TriggerPreemptionController>(runSessions(),this);
    TriggerPreemptionController::Host host;
    host.reconcile=[this]{reconcileMouseLocksFromRunningSessions();};
    host.changed=[this]{updateRunUiState();};
    host.log=[this](const std::string& id,const QString& message,bool resumed){
        if(auto* session=sessionFor(id))appendSessionLog(*session,message,resumed?LogLineKind::Success:LogLineKind::Warning);
    };
    m_triggerPreemption->setHost(std::move(host));
}

void MainWindow::handleTriggerEngineFinished(FeatureRunSession& session,
                                             Feature* feature,
                                             bool success,
                                             const QString& message) {
    RunLifecycleCoordinator::handleTriggerEngineFinished(*this, session, feature, success, message);
}

void MainWindow::finishRunSession(const std::string& featureId,
                                  bool success,
                                  const QString& message,
                                  bool deferUiUpdate) {
    RunLifecycleCoordinator::finishRunSession(*this, featureId, success, message, deferUiUpdate);
}

void MainWindow::runFeature(Feature* feature) {
    startFeatureRun(feature);
}

void MainWindow::persistTriggerArmedState(const QString& id,bool armed) {m_triggerMonitor->persist(id,armed);}
void MainWindow::scheduleRestorePersistedTriggerSessions() {m_triggerMonitor->scheduleRestore();}
void MainWindow::restorePersistedTriggerSessions() {m_triggerMonitor->restore();}
void MainWindow::prunePersistedTriggerArmedFeatures() {m_triggerMonitor->prune();}

void MainWindow::setupTriggerMonitor() {
    TriggerMonitorController::Host host;
    host.workspace=[this]{
        const bool blocked=m_switchingProfile || m_profileSwitchPipelineActive
            || (m_shutdownCoordinator && (m_shutdownCoordinator->busy() || m_shutdownCoordinator->ready()))
            || (m_updateController && m_updateController->installInProgress())
            || (m_profileActions && m_profileActions->busy());
        return TriggerMonitorController::Workspace{m_project.get(),m_profileManager->activeProfileId(),m_projectWorkspace->generation(),blocked};
    };
    host.prepare=[this]{syncTargetWindowTitleToCapture();reconcileRunSessionsWithForegroundGate();};
    host.resume=[this]{resumeWaitingScopedTargetForegroundSessions();};
    host.session=[this](const auto& id){return sessionFor(id);};
    host.feature=[this](auto& session){return featureForSession(session);};
    host.active=[this](const auto& session){return isFeatureSessionActive(session);};
    host.running=[this](const auto& id){return isFeatureRunning(id);};
    host.start=[this](const QString& id){
        startFeatureRun(m_project?m_project->featureById(id.toStdString()):nullptr,false,!ProgramSettings::focusTargetWindowOnProfileSelect());
    };
    host.stop=[this](const QString& id){stopFeatureRun(id.toStdString());};
    host.sessions=[this]{std::vector<std::string> ids;ids.reserve(runSessions().size());for(const auto& entry:runSessions())ids.push_back(entry.first);return ids;};
    host.mayContinue=[this](auto& session,auto* feature){return shouldContinueRunSession(session,feature);};
    host.ensureEngine=[this](auto& session){session.engine=std::make_unique<WorkflowEngine>(this);connectSessionEngine(session);};
    host.launch=[this](auto& session,auto* feature){launchTriggerMonitor(session,feature,false);};
    host.warning=[this](const QString& message){appendLog(message,LogLineKind::Warning);showTransientStatus(message,5000);};
    m_triggerMonitor->setHost(std::move(host));
}

void MainWindow::onHotkeyTriggered(const QString& id) {m_featureHotkeys->triggered(id);}
void MainWindow::onHotkeyHoldStarted(const QString& id) {m_featureHotkeys->holdStarted(id);}
void MainWindow::onHotkeyHoldEnded(const QString& id) {m_featureHotkeys->holdEnded(id);}
void MainWindow::syncHotkeys() {m_featureHotkeys->sync();}

void MainWindow::setupFeatureHotkeys() {
    using Controller=FeatureHotkeyController;
    m_featureHotkeys=std::make_unique<Controller>(this);
    Controller::Host host;
    host.workspace=[this]{return Controller::Workspace{m_project.get(),m_profileManager->activeProfileId(),m_projectWorkspace->generation()};};
    host.gate=[this](const QString& id) {
        Controller::Gate value;
        value.unavailable=!m_profileManager->storageReady() || m_switchingProfile || m_profileSwitchPipelineActive
            || (m_shutdownCoordinator && (m_shutdownCoordinator->busy() || m_shutdownCoordinator->ready()))
            || (m_updateController && m_updateController->installInProgress());
        value.editor=FeatureHotkeyGate::isFeatureHotkeysBlocked();
        if(value.unavailable || value.editor)return value;
#ifdef _WIN32
        const auto* feature=m_project?m_project->featureById(id.toStdString()):nullptr;
        value.targetRequired=!ProgramSettings::runWithoutTargetWindow() && !isActiveDefaultProfile();
        value.profileMatches=!value.targetRequired || foregroundProfileMatchesActive();
        value.backgroundAllowed=!value.profileMatches && feature && triggerBackgroundRunGateActive(feature);
        const HWND foreground=m_foregroundMonitor?m_foregroundMonitor->currentRootHwnd():nullptr;
        value.ownWindow=foreground && IsWindow(foreground) && isPipbongProcessForeground(foreground);
#endif
        return value;
    };
    host.blocked=[this](Controller::Block reason) {
        if(reason==Controller::Block::Editor)showTransientStatus(tr("편집 창이 열려 있으면 기능 단축키를 사용할 수 없습니다."),2500);
        else if(reason==Controller::Block::Target)showTransientStatus(tr("타겟을 활성화한 뒤 단축키를 누르세요."),2500);
    };
    host.running=[this](const auto& id){return isFeatureRunning(id);};
    host.session=[this](const auto& id){return sessionFor(id);};
    host.holdBlocked=[this](const auto& s){return RunLifecycleCoordinator::holdSessionBlocksNewPhysicalStart(s,m_holdKeyTapMux);};
    host.enterBurst=[this]{++m_holdBurstDepth;};
    host.leaveBurst=[this]{--m_holdBurstDepth;RunLifecycleCoordinator::scheduleHoldBurstScopeDrain(*this);};
    host.foregroundReady=[this]{
#ifdef _WIN32
        ensureForegroundReadyForFeatureHotkey();
#endif
    };
    host.start=[this](const auto& id){startFeatureRun(m_project?m_project->featureById(id):nullptr,true);};
    host.stop=[this](const auto& id){stopFeatureRun(id);};
    host.erase=[this](const auto& id){RunLifecycleCoordinator::tearDownAndEraseSessionEntry(*this,id);};
    host.queueStart=[this](const auto& id){RunLifecycleCoordinator::scheduleCoalescedHoldFeatureStart(*this,id);};
    host.cancelStart=[this](const auto& id){
        m_pendingHoldFeatureStartIds.erase(id);
        m_pendingHoldFeatureStartOrder.erase(std::remove(m_pendingHoldFeatureStartOrder.begin(),m_pendingHoldFeatureStartOrder.end(),id),m_pendingHoldFeatureStartOrder.end());
    };
    host.queueFinish=[this](const auto& id){RunLifecycleCoordinator::scheduleCoalescedHoldFeatureEndFinish(*this,id);};
    host.repeat=[this](auto& s){scheduleRepeatIteration(s,featureForSession(s),s.lastLoopSuccess,QString());};
    host.release=[this](auto& s){releaseHoldHotkeyInputToTarget(s);};
    host.stopLane=[this](auto& s){if(m_holdKeyTapMux)m_holdKeyTapMux->stopLaneIfToken(s.featureId,s.holdKeyTapLaneToken);};
    host.holdVisuals=[this]{scheduleFeatureListHoldVisualRefresh();};
    host.prune=[this]{prunePersistedTriggerArmedFeatures();};
    host.registerBindings=[this](const Project& project){
        Controller::Registration value;
        if(!m_hotkeyManager)return value;
        for(const auto& failure:m_hotkeyManager->syncFromProject(project))value.failedNames.append(QString::fromStdString(failure.featureName));
#ifdef _WIN32
        value.keyboardHook=m_hotkeyManager->isKeyboardHookActive();value.mouseHook=m_hotkeyManager->isMouseHookActive();
#endif
        return value;
    };
    host.log=[this](const QString& message,bool error){appendLog(message,error?LogLineKind::Error:LogLineKind::Warning);};
    m_featureHotkeys->setHost(std::move(host));
}

void MainWindow::onStopWorkflow() {
    Feature* feature = m_featureList ? m_featureList->selectedFeature() : nullptr;
    if (!feature) {
        return;
    }
    if (isFeatureRunning(feature->id())) {
        stopFeatureRun(feature->id());
    }
}

void MainWindow::onPickTargetWindow() { m_targetActions->pick(TargetWindowBindingRole::Main); }
void MainWindow::onPickSubTargetWindow() { m_targetActions->pick(TargetWindowBindingRole::Sub); }
void MainWindow::onPickMainTargetWindowFromList() { m_targetActions->pick(TargetWindowBindingRole::Main,true); }
void MainWindow::onPickSubTargetWindowFromList() { m_targetActions->pick(TargetWindowBindingRole::Sub,true); }
void MainWindow::onShowTargetWindow() { m_targetActions->show(TargetWindowBindingRole::Main); }
void MainWindow::onShowSubTargetWindow() { m_targetActions->show(TargetWindowBindingRole::Sub); }

void MainWindow::onPinTargetWindowCenterToggled(bool checked) {m_centerPin->toggle(TargetCenterPinController::Role::Main,checked);}
void MainWindow::onPinSubTargetWindowCenterToggled(bool checked) {m_centerPin->toggle(TargetCenterPinController::Role::Sub,checked);}
void MainWindow::applyTargetWindowCenterPin() {m_centerPin->reconcile();}
void MainWindow::syncTargetWindowCenterPin() {m_centerPin->tick();}

bool MainWindow::hasAnyCapturingWorkflowBurstExcept(const std::string& excludeFeatureId) const {
    return SessionRunPolicy::hasAnyCapturingWorkflowBurst(
        RunLifecycleCoordinator::policyInputsFromSessions(runSessions(), &excludeFeatureId));
}

void MainWindow::setupWorkflowExecutionPresenter() {
    m_executionPresenter=std::make_unique<WorkflowExecutionPresenter>(*m_workflowEditor,this);
    WorkflowExecutionPresenter::Host host;
    host.session=[this](QObject* engine)->FeatureRunSession* {
        return m_shutdownCoordinator && m_shutdownCoordinator->ready()?nullptr:sessionForEngine(engine);
    };
    host.displayed=[this](const auto& session){return isDisplayedRunningFeature(&session);};
    host.details=[this](const auto& session){return shouldLogRunDetails(session);};
    host.log=[this](const auto& session,const QString& message,LogLineKind kind){appendSessionLog(session,message,kind);};
    host.refresh=[this]{updateRunUiState();};
    host.matched=[this](auto& session){updateEarlyLoopMouseLockFromMatch(session);};
    host.failed=[this](auto& session,int index){handleEarlyLoopMouseLockBlockFailure(session,index);};
    m_executionPresenter->setHost(std::move(host));
}

void MainWindow::onEngineLog(const QString& message) { m_executionPresenter->log(sender(),message); }

void MainWindow::onEngineSessionPrepared(std::shared_ptr<Workflow> workflow,
                                         std::shared_ptr<ExecutionContext> context) { m_executionPresenter->prepared(sender(),std::move(workflow),std::move(context)); }

void MainWindow::onEngineStarted() { m_executionPresenter->started(sender()); }

void MainWindow::onEngineFinished(bool success, const QString& message) {
    RunLifecycleCoordinator::onEngineFinished(*this, sender(), success, message);
}

void MainWindow::onBlockStarted(int index, const QString& summary) { m_executionPresenter->blockStarted(sender(),index,summary); }

void MainWindow::onBlockProgress(int index, BlockProgressKind kind) { m_executionPresenter->progress(sender(),index,kind); }

void MainWindow::onBlockMatchResult(int index,
                                    double matchThreshold,
                                    double confidence,
                                    const QPixmap& matchPreview,
                                    bool matched,
                                    bool hasClientPoint,
                                    int clientX,
                                    int clientY) { m_executionPresenter->match(sender(),index,matchThreshold,confidence,matchPreview,matched,hasClientPoint,clientX,clientY); }

void MainWindow::onPointerFeedbackAtClientPoint(int clientX, int clientY) { m_executionPresenter->pointer(sender(),clientX,clientY); }

void MainWindow::onBlockFinished(int index, bool success, const QString& message, qint64 durationMs,
                                 qint64 imageFindMatchDurationMs, int imageFindPollAttempts) { m_executionPresenter->blockFinished(sender(),index,success,message,durationMs,imageFindMatchDurationMs,imageFindPollAttempts); }

void MainWindow::onBlockImageFindAttempt(int index,
                                         int attemptCount,
                                         double matchThreshold,
                                         double detectedConfidence,
                                         bool matched) { m_executionPresenter->attempt(sender(),index,attemptCount,matchThreshold,detectedConfidence,matched); }

void MainWindow::onImageFindFailureHandling(int index,
                                            int returnToPreviousCount,
                                            int retryAfterNextCount) { m_executionPresenter->failureHandling(sender(),index,returnToPreviousCount,retryAfterNextCount); }

void MainWindow::onImageFindReturnToPrevious(int sourceIndex, int targetIndex) { m_executionPresenter->returnToPrevious(sender(),sourceIndex,targetIndex); }

void MainWindow::appendLog(const QString& message, LogLineKind kind) {
    DiagnosticHub::appendAppLog(diagnosticLogLevelTag(kind), message);
    if (m_logPanel) {
        m_logPanel->appendLine(kind, message);
    }
}

bool MainWindow::maybeSave(bool quiet) { return m_projectWorkspace->checkpoint(quiet); }
bool MainWindow::saveActiveProfileSettings() { return m_projectWorkspace->saveSettings(); }
void MainWindow::loadActiveProfile(bool quiet,bool scheduleTriggerRestore) {
    if(!m_projectWorkspace->loadActive(quiet,scheduleTriggerRestore)) appendLog(m_projectWorkspace->lastError(),LogLineKind::Warning);
}

void MainWindow::refreshProfileList() {
    if (!m_profileManager || !m_profileListPresenter) return;
    const QScopedValueRollback<bool> refreshing(m_refreshingProfileList, true);
    const auto snapshot=ProfilePresentationSnapshot::list(*m_profileManager);
    m_profileListPresenter->refresh(snapshot.rows,snapshot.activeId,windowIcon());
    updateTargetWindowControlsForActiveProfile();
}

void MainWindow::syncProfileListSelection() {
    if (m_profileManager && m_profileListPresenter)
        m_profileListPresenter->syncSelection(m_profileManager->activeProfileId());
}

void MainWindow::setProfileSwitchUiLocked(bool locked) {
    if (m_profileList) {
        m_profileList->setReorderEnabled(!locked);
        m_profileList->setFeatureDropEnabled(!locked);
    }
    if (m_featureList) {
        m_featureList->setListDragEnabled(!locked);
    }
}

void MainWindow::schedulePostProfileSwitchForegroundReconcile(bool allowAutoProfileSwitch) {
    m_foregroundSessions->postSwitch(allowAutoProfileSwitch);
}

bool MainWindow::executeProfileSwitch(const QString& profileId,bool automatic) {
    return m_profileTransition->start(profileId,automatic);
}

bool MainWindow::switchToProfile(const QString& profileId, bool automatic) {
    if (!m_profileManager || profileId.isEmpty()
        || profileId == m_profileManager->activeProfileId()) {
        return false;
    }
    if (automatic) {
        applyProfileSwitchFromForegroundState(m_foregroundMonitor->currentState());
        return m_profileManager->activeProfileId() == profileId;
    }
    return executeProfileSwitch(profileId, false);
}

void MainWindow::onForegroundAltModifierReleased() {
    m_foregroundSessions->altReleased();
}

void MainWindow::onForegroundStateChanged(const ForegroundWindowState& state) {
    m_foregroundSessions->changed(state);
}

void MainWindow::applyForegroundCaptureHints(HWND hwnd, const QString& foregroundTitle) {
    m_targetWindowController.applyForegroundCaptureHints(hwnd, foregroundTitle);
}

void MainWindow::handlePipbongForegroundFocus() {
    m_foregroundSessions->pipbongFocused();
}

void MainWindow::applyProfileSwitchFromForegroundState(const ForegroundWindowState& state) {
    m_foregroundSessions->applySwitch(state);
}

void MainWindow::finishForegroundSessionGate() {
    m_foregroundSessions->finishGate();
}

void MainWindow::maybeResetHotkeyLatchForForeground(HWND foregroundHwnd) {
    m_foregroundSessions->resetLatchFor(foregroundHwnd);
}

void MainWindow::ensureForegroundReadyForFeatureHotkey() {
    m_foregroundSessions->ensureHotkeyReady();
}

void MainWindow::reconcileRunSessionsWithForegroundGate() {
    synchronizeRunWorkerState();
    m_runSessionController.reconcileWithForegroundGate();
}

void MainWindow::loadProjectFromFile(const QString& path,bool quiet) {
    if(m_projectWorkspace->load(path,quiet)) return;
    appendLog(m_projectWorkspace->lastError(),LogLineKind::Warning);
    if(!quiet) QMessageBox::critical(this,tr("프로젝트 열기"),m_projectWorkspace->lastError());
}

void MainWindow::updateWindowTitle() {m_windowShell->setDocument(m_projectFilePath,m_modified);}

std::wstring MainWindow::currentTargetWindowTitleW() const {return ProfilePresentationSnapshot::documentTitle(m_project.get()).toStdWString();}

std::wstring MainWindow::resolveEffectiveTargetTitleW() const {return m_captureTargets->effective().toStdWString();}
std::wstring MainWindow::resolveAutoRunCaptureTargetTitleW() const {return m_captureTargets->resolve().toStdWString();}
std::wstring MainWindow::linkedTargetLookupTitleW() const {return m_captureTargets->lookup().toStdWString();}
#ifdef _WIN32
void MainWindow::rememberProfileLinkedForeground(HWND hwnd,const QString& title) {m_targetWindowController.rememberProfileLinkedForeground(hwnd,title);}
bool MainWindow::adoptForegroundLinkedCaptureIfMatched() {return m_captureTargets->adoptForeground();}
#endif

QString MainWindow::foregroundProfileIdForActiveWindow() const {
    return m_foregroundSessions->foregroundProfile();
}

ForegroundRunGate::ProfileBindings MainWindow::buildForegroundProfileBindings(const QString& id) const {
    return m_captureTargets->bindings(id);
}

ForegroundRunGate::GateInput MainWindow::buildForegroundGateInput(const Feature* feature,
                                                                  const QString& bindingsProfileId) const {
    return m_foregroundSessions->gateInput(feature,bindingsProfileId);
}

QString MainWindow::profileIdForForegroundHwnd(HWND hwnd) const {
    return m_foregroundSessions->profileForWindow(hwnd);
}

bool MainWindow::foregroundProfileMatchesActive() const {
    return m_foregroundSessions->profileMatches();
}

bool MainWindow::switchToForegroundLinkedProfileIfNeeded(bool forceImmediate) {
    Q_UNUSED(forceImmediate);return m_foregroundSessions->switchToForeground();
}

bool MainWindow::profileMainOrSubForegroundActive() const {
    return m_foregroundSessions->mainOrSub();
}

bool MainWindow::triggerBackgroundRunGateActive(const Feature* feature) const {
    return m_foregroundSessions->backgroundGate(feature);
}

bool MainWindow::runForegroundGateActive(const Feature* feature) const {
    return m_foregroundSessions->runGate(feature);
}

bool MainWindow::runForegroundGateActiveForSession(const FeatureRunSession& session,
                                                    Feature* feature) const {
    return m_foregroundSessions->sessionGate(session,feature);
}

std::wstring MainWindow::resolveRunCaptureTargetTitleW(const Feature* feature) const {
    return m_captureTargets->resolve(feature?feature->captureTargetScope():FeatureCaptureTargetScope::Auto,
        feature && feature->runMode()==FeatureRunMode::Trigger && feature->triggerRunWithoutTargetForeground()).toStdWString();
}

bool MainWindow::scopedTargetForegroundActive(const Feature* feature) const {
    return m_foregroundSessions->scopedGate(feature);
}

bool MainWindow::deferRunUntilScopedTargetForeground(FeatureRunSession& session, Feature* feature) {
    return m_runSessionController.deferRunUntilScopedTargetForeground(session, feature);
}

void MainWindow::resumeWaitingScopedTargetForegroundSessions() {
    m_runSessionController.resumeWaitingScopedTargetForegroundSessions();
}

std::wstring MainWindow::sessionCaptureTargetTitleW(FeatureRunSession& session,bool refresh) {
    return m_captureTargets->sessionTitleForRun(session,refresh);
}
void MainWindow::refreshSessionCaptureTarget(FeatureRunSession& session) {
    if(const auto* feature=featureForSession(session))m_captureTargets->refreshSession(session,*feature);
}
void MainWindow::applySessionCaptureTarget(const std::wstring& title) const {m_captureTargets->applyTitle(QString::fromStdWString(title));}
void MainWindow::refreshCaptureTargetForEditing() {syncTargetWindowTitleToCapture();}
void MainWindow::syncTargetWindowTitleToCapture() {syncEffectiveTargetWindowTitleToCapture();}
void MainWindow::syncEffectiveTargetWindowTitleToCapture() {m_captureTargets->sync();}

bool MainWindow::isActiveDefaultProfile() const {return ProfilePresentationSnapshot::isDefault(m_profileManager.get());}

#ifdef _WIN32
void MainWindow::healLinkedTargetProcessPathFromForeground(HWND hwnd, const QString& foregroundTitle) {
    m_targetWindowController.healLinkedTargetProcessPathFromForeground(hwnd, foregroundTitle);
}

#endif

void MainWindow::updateTargetWindowControlsForActiveProfile() {if(m_targetPanel)m_targetPanel->applyControls(isActiveDefaultProfile());}

void MainWindow::updateTargetWindowDetails() {if(m_targetPanel)m_targetPanel->refresh();}
TargetWindowDetailsController::Request MainWindow::targetDetailsRequest()const {
    TargetWindowDetailsController::Request request;
    request.revision=m_projectWorkspace?m_projectWorkspace->generation():0;
    request.captureOwnedBySession = hasAnyRunningSession();
    request.allowIdleUpdates = (!m_workspaceHistory || !m_workspaceHistory->isBusy())
        && !(m_shutdownCoordinator && (m_shutdownCoordinator->busy() || m_shutdownCoordinator->ready()));
    request.deferProfileRefresh = m_deferTargetDetailsProfileRefresh;
#ifdef _WIN32
    request.foreground = m_foregroundMonitor->currentRootHwnd();
    request.pipbongForeground = m_foregroundMonitor->currentState().pipbong;
    request.remembered = m_targetWindowController.lastProfileLinkedForegroundHwnd();
    request.hint = ScreenCapture::foregroundHintWindow();
#endif
    return ProfilePresentationSnapshot::targetDetails(m_profileManager.get(),m_project.get(),std::move(request));
}

void MainWindow::syncUserInputInterruptForSession(FeatureRunSession& session, Feature*) {
    m_inputInterrupts->sync(session);
}

void MainWindow::onUserInputInterrupt(const std::string& featureId,
                                     const std::shared_ptr<ExecutionContext>& expected) {
    if (auto* session = sessionFor(featureId)) m_inputInterrupts->interrupt(*session, expected);
}

void MainWindow::setupInputInterrupts() {
    m_inputInterrupts = std::make_unique<InputInterruptController>();
    InputInterruptController::Host host;
    host.feature = [this](FeatureRunSession& session) { return featureForSession(session); };
    host.disarm = [this](const FeatureRunSession& session) {
        persistTriggerArmedState(QString::fromStdString(session.featureId), false);
    };
    host.changed = [this](const FeatureRunSession& session, const QString& name, InputInterruptController::Outcome outcome) {
        using Outcome = InputInterruptController::Outcome;
        if (outcome == Outcome::Stopped) {
            appendSessionLog(session, tr("사용자 입력으로 실행을 멈췄습니다"), LogLineKind::Warning);
            showTransientStatus(tr("[%1] 사용자 입력 — 정지").arg(name), 3000);
        } else {
            const bool paused = outcome == Outcome::Paused;
            appendSessionLog(session, paused ? tr("사용자 입력으로 잠시 멈춤") : tr("사용자 입력으로 다시 실행"),
                paused ? LogLineKind::Warning : LogLineKind::Success);
            reconcileMouseLocksFromRunningSessions();
            updateRunUiState();
        }
    };
    m_inputInterrupts->setHost(std::move(host));
}

void MainWindow::scheduleRunWarmup() {
    QTimer::singleShot(0, this, [this]() {
        if (!m_project) {
            return;
        }
        const Project* project = m_project.get();
        const std::string projectDirectory =
            Application::instance()->projectDirectory().toStdString();
        std::string priorityFeatureId;
        if (m_featureList) {
            priorityFeatureId = m_featureList->selectedFeatureId().toStdString();
        }
        RunWarmup::prefetch(project, projectDirectory, priorityFeatureId);
    });
}

void MainWindow::prepareProjectUnload() {
    m_libraryPreviewFeature.reset();
    m_libraryPreviewEntryId.clear();
    if (m_workflowEditor) {
        m_workflowEditor->setFeature(nullptr);
    }
}

void MainWindow::setupWorkspaceHistory() {
    WorkspaceHistoryActions::HostCallbacks actions;
    actions.canBegin = [this](bool restoring) {
        return m_profileManager->storageReady() && (!restoring || !FeatureHotkeyGate::isFeatureHotkeysBlocked())
            && !m_switchingProfile && !m_profileSwitchPipelineActive
            && !(m_shutdownCoordinator && (m_shutdownCoordinator->busy() || m_shutdownCoordinator->ready()))
            && !(m_updateController && m_updateController->installInProgress());
    };
    actions.reportError = [this](const QString& error) {
        appendLog(error,LogLineKind::Warning);showTransientStatus(error,5000);
    };
    actions.memo = [this] { return m_auxiliaryTools->memo(); };
    actions.suspendAutoSave = [this] { m_autoSaveTimer->stop(); };
    actions.finished = [this] {
        if ((m_workspaceHistory && m_workspaceHistory->isBusy())
            || (m_shutdownCoordinator && (m_shutdownCoordinator->busy() || m_shutdownCoordinator->ready()))) return;
        if (m_modified) m_autoSaveTimer->start(800);
        scheduleProfilePackageSeal();
        schedulePostProfileSwitchForegroundReconcile(true);
    };
    auto& host=actions.history;
    host.paths = [this](const QString& requestedProfile) {
        WorkspaceHistoryController::Paths paths;
        paths.dataRoot = Application::dataDirectory();
        paths.profilesDirectory = m_profileManager->profilesDirectory();
        paths.libraryDirectory = QDir(paths.dataRoot).filePath(QStringLiteral("featureLibrary"));
        paths.profileId = requestedProfile.isEmpty() ? m_profileManager->activeProfileId() : requestedProfile;
        paths.projectPath = m_profileManager->projectPath(paths.profileId);
        return paths;
    };
    host.prepare = [this](bool restoring) {
        if (restoring) stopAllSessions();
        return maybeSave(true) && saveActiveProfileSettings();
    };
    host.restored = [this](WorkspaceHistoryController::Scope scope, const QString& profileId) {
        // Input was blocked and current edits were checkpointed before the transaction.
        // Reload committed files without flushing stale runtime feedback over them.
        m_modified = false;
        if (scope == WorkspaceHistoryController::Scope::Project) {
            m_profileManager->invalidateCachedProject(profileId);
            if (m_profileManager->activeProfileId() == profileId) {
                loadProjectFromFile(m_profileManager->projectPath(profileId), true);
            }
            m_packageController->schedule(m_profileManager->profileDirectory(profileId),
                                          m_profileManager->profilePackagePath(profileId));
        } else {
            m_packageController->discardPending();
            m_profileManager->initialize();
            refreshProfileList();
            loadActiveProfile(true);
            refreshFeatureLibraryPanel();
            syncProfileListSelection();
            syncMemoDialogProfile();
            scheduleProfilePackageSeal();
        }
        showTransientStatus(tr("실행 취소/다시 실행 적용됨"), 1200);
    };
    m_workspaceHistory = std::make_unique<WorkspaceHistoryActions>(this,*m_packageController,std::move(actions));
}

bool MainWindow::pushGlobalUiUndoSnapshot(const QString& reason) {return m_workspaceHistory && m_workspaceHistory->push(reason);}
void MainWindow::onGlobalUndoRequested() {if(m_workspaceHistory)m_workspaceHistory->undo();}
void MainWindow::onGlobalRedoRequested() {if(m_workspaceHistory)m_workspaceHistory->redo();}

void MainWindow::setupProjectWorkspace() {
    m_projectWorkspace->setPackages(m_packageController.get());
    ProjectWorkspaceController::HostCallbacks host;
    host.mayReplace=[this] {
        if(!m_profileManager->storageReady())return false;
        if(m_shutdownCoordinator && m_shutdownCoordinator->ready()) return false;
        if(m_workspaceHistory && m_workspaceHistory->isBusy()) return m_workspaceHistory->isReloading();
        return !FeatureHotkeyGate::isFeatureHotkeysBlocked();
    };
    host.mayWrite=[this] {
        return m_profileManager->storageReady() && !(m_shutdownCoordinator && m_shutdownCoordinator->ready())
            && (!m_workspaceHistory || !m_workspaceHistory->isBusy() || m_workspaceHistory->isPreparing());
    };
    host.maySchedule=[this] {
        return m_profileManager->storageReady() && (!m_workspaceHistory || !m_workspaceHistory->isBusy())
            && !(m_shutdownCoordinator && (m_shutdownCoordinator->busy() || m_shutdownCoordinator->ready()));
    };
    host.beforeReplace=[this] { prepareProjectUnload(); };
    host.changed=[this] { updateWindowTitle(); };
    host.currentDirectory=[] { return Application::instance()->projectDirectory(); };
    host.directoryChanged=[](const QString& path) { Application::instance()->setProjectDirectory(path); };
    host.installed=[this](const ProjectWorkspaceController::Change& change) {
        onWorkspaceInstalled(change.fresh,change.clearTarget,change.quiet,change.restoreTriggers);
    };
    host.status=[this](const QString& text) { showTransientStatus(text,2000); };
    host.warning=[this](const QString& text) { appendLog(text,LogLineKind::Warning); };
    m_projectWorkspace->setHost(std::move(host));
    ProfileTransitionController::AutomaticCallbacks automatic;
    automatic.defer=[this](const QString& id) { m_profileSwitchCoordinator.setDeferredProfileSwitchId(id); };
    automatic.schedule=[this](int delay) { m_profileSwitchCoordinator.scheduleDeferredProfileSwitchFlush(delay); };
    automatic.committed=[this] { m_profileSwitchCoordinator.markAutomaticProfileSwitchCommitted(); };
    automatic.manualCommitted=[this](const QString& id,bool global) { m_profileSwitchCoordinator.onManualProfileSwitchCommitted(id,global); };
    automatic.hasDeferred=[this] { return !m_profileSwitchCoordinator.deferredProfileSwitchId().isEmpty(); };
    automatic.clearDeferred=[this] { m_profileSwitchCoordinator.clearDeferredProfileSwitchId(); };
    m_profileTransition->setAutomatic(std::move(automatic));
    ProfileTransitionController::HostCallbacks transition;
    transition.canBegin=[this](bool automatic) {
        return (!m_workspaceHistory || !m_workspaceHistory->isBusy())
            && !(m_shutdownCoordinator && (m_shutdownCoordinator->busy() || m_shutdownCoordinator->ready()))
            && !(m_updateController && m_updateController->installInProgress())
            && !FeatureHotkeyGate::isFeatureHotkeysBlocked() && (!automatic || !QApplication::activePopupWidget());
    };
    transition.dragActive=[] { return ReorderableListWidget::isAnyListDragActive(); };
    transition.cancelDrags=[] { ReorderableListWidget::cancelActiveListDrags(); };
    transition.saveMemo=[this] { return !m_auxiliaryTools || m_auxiliaryTools->saveMemo(); };
    transition.lockUi=[this](bool locked) { setProfileSwitchUiLocked(locked); };
    transition.selectProfile=[this](const QString& id) {
        for(int row=0;row<m_profileList->count();++row) if(auto* item=m_profileList->item(row);item && item->data(Qt::UserRole).toString()==id) {
            const QSignalBlocker block(m_profileList);m_profileList->setCurrentRow(row);break;
        }
    };
    transition.resyncSelection=[this] { syncProfileListSelection(); };
    transition.detach=[this] { detachUiForProfileSwitch(); };
    transition.loaded=[this](bool automatic) {
#ifdef _WIN32
        if(automatic) { if(!adoptForegroundLinkedCaptureIfMatched()) syncEffectiveTargetWindowTitleToCapture(); }
        else m_targetWindowController.clearLastProfileLinkedForeground();
#endif
        syncMemoDialogProfile();syncProfileListSelection();
    };
    transition.completed=[this](bool automatic) {
        scheduleRunWarmup();
        if(!automatic) if(const auto* profile=m_profileManager->activeProfile())
            showTransientStatus(tr("프로필 전환: %1").arg(m_profileManager->isDefaultProfile(profile->id)?tr("기본"):profile->name),1200);
    };
    transition.finishViews=[this](bool automatic) {
#ifdef _WIN32
        if(!automatic && !FeatureHotkeyGate::isFeatureHotkeysBlocked()
            && ProgramSettings::focusTargetWindowOnProfileSelect() && !isActiveDefaultProfile()) {
            syncTargetWindowTitleToCapture();if(ScreenCapture::findTargetWindow()) ScreenCapture::activateTargetWindow();
        }
#endif
        updateTargetWindowDetails();
    };
    transition.restoreTriggersAndReconcile=[this](bool automatic) { restorePersistedTriggerSessions();schedulePostProfileSwitchForegroundReconcile(automatic); };
    transition.foregroundChanged=[this] { onForegroundStateChanged(m_foregroundMonitor->currentState()); };
    transition.warning=[this](const QString& text) { appendLog(text,LogLineKind::Warning);showTransientStatus(text,5000); };
    m_profileTransition->setHost(std::move(transition));
}

void MainWindow::onWorkspaceInstalled(bool fresh,bool clearTarget,bool quiet,bool restoreTriggers) {
    m_runSessionController.setProject(m_project.get());
    ScreenCapture::setTargetWindow(nullptr);
    if(clearTarget) ScreenCapture::setTargetWindowTitle(L"");else syncTargetWindowTitleToCapture();
    applyTargetWindowCenterPin();
    m_featureList->setProject(m_project.get());m_featureList->setActiveProfileId(m_profileManager->activeProfileId());
    if(fresh) onFeatureSelectionChanged();else restoreSelectedFeaturePreference();
    updateWindowTitle();
    if(m_profileSwitchPipelineActive) {
        QTimer::singleShot(0,this,[this] { refreshWorkflowEditor();syncHotkeys(); });
    } else { refreshWorkflowEditor();syncHotkeys(); }
    if(m_deferTargetDetailsProfileRefresh) QTimer::singleShot(0,this,&MainWindow::updateTargetWindowDetails);
    else updateTargetWindowDetails();
    updateTargetWindowControlsForActiveProfile();
    if(restoreTriggers) scheduleRestorePersistedTriggerSessions();
    if(!m_profileSwitchPipelineActive && restoreTriggers) onForegroundStateChanged(m_foregroundMonitor->currentState());
    if(!quiet) appendLog(tr("프로젝트 파일을 불러왔습니다: %1").arg(m_projectFilePath),LogLineKind::Success);
}

void MainWindow::setupTargetActions() {
    m_targetActions=std::make_unique<TargetWindowActionsController>(this);
    TargetWindowActionsController::Host host;
    host.snapshot=[this] {
        TargetWindowActionsController::Snapshot value;
        value.profileId=m_projectWorkspace->state().profileId;
        value.generation=m_projectWorkspace->generation();value.globalDefault=isActiveDefaultProfile();
        value.mainTitle=QString::fromStdWString(currentTargetWindowTitleW()).trimmed();
        value.subTitle=m_profileManager->subTargetWindowTitle(value.profileId).trimmed();
        value.mainProcessPath=m_profileManager->linkedTargetProcessPath(value.profileId);
        value.subProcessPath=m_profileManager->subLinkedTargetProcessPath(value.profileId);
#ifdef _WIN32
        value.preferred=ScreenCapture::targetWindow();
#endif
        return value;
    };
    host.blockedReason=[this](bool changing) {
        if(FeatureHotkeyGate::isFeatureHotkeysBlocked())return tr("편집이나 창 선택을 마친 뒤 다시 시도해 주세요.");
        if(m_profileSwitchPipelineActive || (m_workspaceHistory && m_workspaceHistory->isBusy())
            || (m_shutdownCoordinator && (m_shutdownCoordinator->busy() || m_shutdownCoordinator->ready()))
            || (m_updateController && m_updateController->installInProgress()))
            return tr("현재 저장·전환 작업을 마친 뒤 다시 시도해 주세요.");
        if(changing && !SessionRunPolicy::allowsTargetBindingChange(RunLifecycleCoordinator::policyInputsFromSessions(runSessions())))
            return tr("실행 중인 기능을 중지한 뒤 타겟을 지정해 주세요.");
        return QString();
    };
    host.checkpoint=[this] { return pushGlobalUiUndoSnapshot(QStringLiteral("target-binding")); };
    host.commit=[this](const TargetWindowActionsController::Snapshot& source,TargetWindowBindingRole role,
                       const TargetWindowActionsController::Selection& value) {
        return m_projectWorkspace->commitTargetBinding(source.profileId,source.generation,
            role==TargetWindowBindingRole::Sub,value.title,value.processPath)?QString():m_projectWorkspace->lastError();
    };
    host.committed=[this](const TargetWindowActionsController::Selection& value,TargetWindowBindingRole role) {
        syncTargetWindowTitleToCapture();synchronizeRunWorkerState();
        updateTargetWindowDetails();refreshProfileList();
        appendLog((role==TargetWindowBindingRole::Sub?tr("서브 창을 지정했습니다: %1"):tr("타겟을 지정했습니다: %1")).arg(value.title),LogLineKind::Success);
        if(role==TargetWindowBindingRole::Main) {scheduleRunWarmup();syncTargetWindowCenterPin();}
    };
    host.persistent=[this](const QString& text) {setPersistentStatus(text);};
    host.status=[this](const QString& text) {showTransientStatus(text,3000);};
    host.warning=[this](const QString& text) {appendLog(text,LogLineKind::Warning);showTransientStatus(text,6000);};
    m_targetActions->setHost(std::move(host));
}

void MainWindow::setupTargetCenterPin() {
    m_centerPin=std::make_unique<TargetCenterPinController>(this);
    TargetCenterPinController::Host host;
    host.snapshot=[this] {
        TargetCenterPinController::Snapshot value;
        value.profileId=m_projectWorkspace->state().profileId;value.generation=m_projectWorkspace->generation();
        value.globalDefault=isActiveDefaultProfile();value.mainTitle=QString::fromStdWString(currentTargetWindowTitleW()).trimmed();
        value.subTitle=m_profileManager->subTargetWindowTitle(value.profileId).trimmed();
        value.mainPath=m_profileManager->linkedTargetProcessPath(value.profileId);
        value.subPath=m_profileManager->subLinkedTargetProcessPath(value.profileId);
        value.main=ProgramSettings::pinTargetWindowToScreenCenter();value.sub=ProgramSettings::pinSubTargetWindowToScreenCenter();
        value.blocked=FeatureHotkeyGate::isFeatureHotkeysBlocked() || m_profileSwitchPipelineActive
            || (m_workspaceHistory && m_workspaceHistory->isBusy())
            || (m_shutdownCoordinator && (m_shutdownCoordinator->busy() || m_shutdownCoordinator->ready()))
            || (m_updateController && m_updateController->installInProgress());
        value.movementAllowed=SessionRunPolicy::allowsTargetCenterPin(RunLifecycleCoordinator::policyInputsFromSessions(runSessions()));
        return value;
    };
    host.save=[this](const TargetCenterPinController::Snapshot& source,bool main,bool sub) {
        return m_projectWorkspace->saveCenterPinOptions(source.profileId,source.generation,main,sub)?QString():m_projectWorkspace->lastError();
    };
    host.view=[this](bool main,bool sub) {
        const QSignalBlocker one(m_pinTargetWindowCenterButton),two(m_pinSubTargetWindowCenterButton);
        m_pinTargetWindowCenterButton->setChecked(main);m_pinSubTargetWindowCenterButton->setChecked(sub);
    };
    host.changed=[this]{updateTargetWindowDetails();};
    host.status=[this](const QString& text){showTransientStatus(text,4000);};
    host.warning=[this](const QString& text){appendLog(text,LogLineKind::Warning);showTransientStatus(text,6000);};
    m_centerPin->setHost(std::move(host));m_centerPin->reconcile();
}

void MainWindow::setupCaptureTargets() {
    m_captureTargets=std::make_unique<CaptureTargetController>(*m_profileManager);
    m_captureTargets->setSessions(&runSessions());
    CaptureTargetController::Host host;
    host.mainTitle=[this]{return QString::fromStdWString(currentTargetWindowTitleW());};
    host.foreground=[this]{return m_foregroundMonitor->currentState();};
    host.foregroundAllowed=[this](const QString& id){return ForegroundRunGate::foregroundProfileMatchesActive(buildForegroundGateInput(nullptr,id));};
    host.mayApply=[this]{return !hasAnyRunningSession() && (!m_workspaceHistory || !m_workspaceHistory->isBusy())
        && !(m_shutdownCoordinator && (m_shutdownCoordinator->busy() || m_shutdownCoordinator->ready()));};
    host.feature=[this](FeatureRunSession& session){return featureForSession(session);};
#ifdef _WIN32
    host.remember=[this](HWND hwnd,const QString& title){m_targetWindowController.rememberProfileLinkedForeground(hwnd,title);};
#endif
    host.monitorChanged=[this]{scheduleEnsureTriggerMonitorEnginesRunning();};
    host.sessionChanged=[](const FeatureRunSession& session){CrashReporter::noteBreadcrumb(QStringLiteral("capture"),
        QStringLiteral("session target adopt %1").arg(QString::fromStdWString(session.lockedCaptureTargetTitle)));};
    m_captureTargets->setHost(std::move(host));
}

void MainWindow::setupForegroundSessions() {
    m_foregroundSessions=std::make_unique<ForegroundSessionController>(*m_profileManager,*m_captureTargets,this);
    ForegroundSessionController::Host host;
    host.snapshot=[this] {
        ForegroundSessionController::Snapshot value;
        value.switching=m_switchingProfile;value.pipeline=m_profileSwitchPipelineActive;
        value.historyBusy=!m_profileManager->storageReady() || (m_workspaceHistory && m_workspaceHistory->isBusy()) || (m_profileActions && m_profileActions->busy());
        value.runWithout=ProgramSettings::runWithoutTargetWindow();
        value.shutdown=m_shutdownCoordinator && (m_shutdownCoordinator->busy() || m_shutdownCoordinator->ready());
        value.deferred=m_profileSwitchCoordinator.deferredProfileSwitchId();
        value.lastLinked=m_profileSwitchCoordinator.lastLinkedForegroundProfileId();
        value.restoreRecent=m_profileSwitchCoordinator.shouldRestoreLinkedProfileOnPipbongFocus(2500);
        return value;
    };
    host.foreground=[this]{return m_foregroundMonitor?m_foregroundMonitor->currentState():ForegroundWindowState{};};
    host.syncDesktop=[this]{if(m_foregroundMonitor)m_foregroundMonitor->syncFromDesktopForeground();};
    host.modifierHeld=[]{return isAltTabModifierHeld();};
    host.synchronize=[this]{synchronizeRunWorkerState();};
    host.flushDeferred=[this]{flushDeferredProfileSwitchIfIdle();};
    host.ensureTriggers=[this]{scheduleEnsureTriggerMonitorEnginesRunning();};
    host.updateDetails=[this]{updateTargetWindowDetails();};
    host.resetLatch=[this]{if(m_hotkeyManager)m_hotkeyManager->resetHookLatchState();};
    host.reconcileSessions=[this]{m_runSessionController.reconcileWithForegroundGate();};
    host.resumeSessions=[this]{m_runSessionController.resumeWaitingScopedTargetForegroundSessions();};
    host.applySwitch=[this](const auto& state){m_profileSwitchCoordinator.applyFromForegroundState(state);};
    host.executeSwitch=[this](const QString& id,bool automatic){return executeProfileSwitch(id,automatic);};
    host.captureHints=[this](HWND hwnd,const QString& title){m_targetWindowController.applyForegroundCaptureHints(hwnd,title);};
    m_foregroundSessions->setHost(std::move(host));
}
