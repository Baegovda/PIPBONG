#include "app/MainWindow.h"

#include "app/Application.h"
#include "app/ProgramSettings.h"
#include "app/WindowsRunAsAdmin.h"
#include "app/MouseCenterLock.h"
#include "app/UserInputInterruptMonitor.h"
#include "ui/calculator/CalculatorDialog.h"
#include "ui/ProgramSettingsDialog.h"
#include "model/UserInputInterruptMode.h"
#include "app/HotkeyManager.h"
#include "core/capture/ScreenCapture.h"
#include "core/capture/CursorPositionPicker.h"
#include "core/capture/WindowPicker.h"
#include "core/input/InputSimulator.h"
#include "ui/WindowPickerHoverOverlay.h"
#include "core/RunWarmup.h"
#include "core/workflow/ExecutionContext.h"
#include "core/workflow/Workflow.h"
#include "core/workflow/WorkflowEngine.h"
#include "core/workflow/WorkflowRunner.h"
#include "core/workflow/blocks/ImageFindBlock.h"
#include "model/Feature.h"
#include "model/Project.h"
#include "storage/JsonSerializer.h"
#include "ui/FeatureListPanel.h"
#include "ui/BlockListWidget.h"
#include "ui/WorkflowEditorPanel.h"
#include "ui/TargetWindowDetailPanel.h"
#include "ui/TargetWindowHighlightOverlay.h"
#include "app/UpdateChecker.h"
#include "ui/CustomTitleBar.h"
#include "ui/UiStateManager.h"
#include "ui/editors/MatchTestOverlay.h"
#include "ui/editors/WorkflowMatchFeedbackOverlay.h"
#include "ui/editors/WorkflowRoiFlashOverlay.h"
#include "ui/editors/RoiPreviewOverlay.h"
#include "ui/editors/ScreenRegionOverlay.h"

#include <QCheckBox>
#include <QCloseEvent>
#include <QCoreApplication>
#include <QAbstractItemView>
#include <QApplication>
#include <QDialog>
#include <QDialogButtonBox>
#include <QEventLoop>
#include <QFileDialog>
#include <QFileInfo>
#include <QFileIconProvider>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QIcon>
#include <QLabel>
#include <QListWidget>
#include <QMenuBar>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QSettings>
#include <QShowEvent>
#include <QSignalBlocker>
#include <QSizePolicy>
#include <QSplitter>
#include <QStatusBar>
#include <QSystemTrayIcon>
#include <QMenu>
#include <QPointer>
#include <QTimer>
#include <QVector>
#include <QVBoxLayout>

#include <memory>

#include "model/FeatureRunMode.h"

#ifdef _WIN32
#include <windows.h>
#include <windowsx.h>
#include <dwmapi.h>

namespace {

bool applyNativeAlwaysOnTop(QWidget* window, bool enabled) {
    if (!window || !window->internalWinId()) {
        return false;
    }

    const HWND hwnd = reinterpret_cast<HWND>(window->winId());
    if (!hwnd || !IsWindow(hwnd)) {
        return false;
    }

    return SetWindowPos(hwnd,
                        enabled ? HWND_TOPMOST : HWND_NOTOPMOST,
                        0,
                        0,
                        0,
                        0,
                        SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE) != FALSE;
}

struct WindowListEntry {
    HWND hwnd = nullptr;
    std::wstring title;
    QString displayText;
    QIcon icon;
};

QIcon iconForProcessPath(const std::wstring& processPath) {
    static QFileIconProvider iconProvider;
    if (!processPath.empty()) {
        const QIcon icon = iconProvider.icon(QFileInfo(QString::fromStdWString(processPath)));
        if (!icon.isNull()) {
            return icon;
        }
    }
    return iconProvider.icon(QFileIconProvider::File);
}

void captureRunStartCursorPosition(FeatureRunSession& session) {
    session.hasRunStartCursorPosition = false;
    if (!session.restoreMousePositionOnEnd) {
        return;
    }
    int screenX = 0;
    int screenY = 0;
    if (!InputSimulator::getCursorScreenPosition(screenX, screenY)) {
        return;
    }
    session.runStartCursorScreenX = screenX;
    session.runStartCursorScreenY = screenY;
    session.hasRunStartCursorPosition = true;
}

void restoreRunStartCursorPosition(const FeatureRunSession& session) {
    if (!session.hasRunStartCursorPosition) {
        return;
    }
    InputSimulator::moveMouse(session.runStartCursorScreenX, session.runStartCursorScreenY);
}

QVector<WindowListEntry> collectWindowListEntries() {
    QVector<WindowListEntry> entries;
    EnumWindows(
        [](HWND hwnd, LPARAM lParam) -> BOOL {
            if (!IsWindowVisible(hwnd)) {
                return TRUE;
            }
            if (GetWindow(hwnd, GW_OWNER) != nullptr) {
                return TRUE;
            }

            wchar_t titleBuffer[512]{};
            GetWindowTextW(hwnd, titleBuffer, 512);
            std::wstring title(titleBuffer);
            if (title.empty()) {
                return TRUE;
            }

            ScreenCapture::TargetWindowInfo info;
            QString processName = QStringLiteral("Unknown");
            std::wstring processPath;
            if (ScreenCapture::queryWindowInfo(hwnd, info) && !info.processPath.empty()) {
                processPath = info.processPath;
                const QFileInfo processInfo(QString::fromStdWString(processPath));
                processName = processInfo.fileName();
            }

            auto* out = reinterpret_cast<QVector<WindowListEntry>*>(lParam);
            WindowListEntry entry;
            entry.hwnd = hwnd;
            entry.title = title;
            entry.icon = iconForProcessPath(processPath);
            const QString titleText = QString::fromStdWString(title);
            const qulonglong hwndValue = reinterpret_cast<qulonglong>(hwnd);
            entry.displayText = QStringLiteral("[%1] %2 - %3")
                                    .arg(hwndValue, 0, 16)
                                    .toUpper()
                                    .arg(processName, titleText);
            out->push_back(std::move(entry));
            return TRUE;
        },
        reinterpret_cast<LPARAM>(&entries));
    return entries;
}

} // namespace
#endif

MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent)
    , m_project(std::make_unique<Project>())
    , m_hotkeyManager(new HotkeyManager(this)) {
    m_autoSaveTimer = new QTimer(this);
    m_autoSaveTimer->setSingleShot(true);
    connect(m_autoSaveTimer, &QTimer::timeout, this, &MainWindow::autoSaveProject);

    m_statusClearTimer = new QTimer(this);
    m_statusClearTimer->setSingleShot(true);
    connect(m_statusClearTimer, &QTimer::timeout, this, &MainWindow::clearStatusMessage);

    setupUi();
    setupUiState();
    setupMenus();
    connectSignals();
    setupUpdateChecker();
    m_mouseLockSyncTimer = new QTimer(this);
    m_mouseLockSyncTimer->setInterval(100);
    connect(m_mouseLockSyncTimer, &QTimer::timeout, this, &MainWindow::syncMouseLockPositions);
    refreshUpdateButtonState();
    ProgramSettings::syncWindowsStartupRegistration();
    ProgramSettings::syncWindowsRunAsAdminRegistration();
    setupTrayIcon();

    const QString autoSavePath = Application::autoSaveFilePath();
    if (QFileInfo::exists(autoSavePath)) {
        loadProjectFromFile(autoSavePath);
    } else {
        m_project->addFeature(QStringLiteral("예시 기능").toStdString());
        m_featureList->refresh();
        onFeatureSelectionChanged();
        m_projectFilePath = autoSavePath;
        autoSaveProject();
    }

    updateWindowTitle();
    syncHotkeys();
    updateTargetWindowDetails();
    scheduleRunWarmup();
}

MainWindow::~MainWindow() {
    stopAllSessions();
}

void MainWindow::setupUi() {
    setWindowTitle(QStringLiteral("PIPBONG %1").arg(QCoreApplication::applicationVersion()));
    setWindowFlag(Qt::FramelessWindowHint, true);
    resize(1280, 820);

    auto* central = new QWidget(this);
    auto* outerLayout = new QVBoxLayout(central);
    outerLayout->setContentsMargins(0, 0, 0, 0);
    outerLayout->setSpacing(0);

    m_titleBar = new CustomTitleBar(this);
    outerLayout->addWidget(m_titleBar);

    m_alwaysOnTopCheck = new QCheckBox(tr("항상 위"), m_titleBar);
    m_titleBar->setAlwaysOnTopCheckBox(m_alwaysOnTopCheck);

    auto* contentHost = new QWidget(central);
    auto* contentLayout = new QVBoxLayout(contentHost);
    contentLayout->setContentsMargins(0, 0, 0, 0);
    contentLayout->setSpacing(0);

    m_mainHorizontalSplitter = new QSplitter(Qt::Horizontal, contentHost);
    m_featureList = new FeatureListPanel(m_mainHorizontalSplitter);
    m_workflowEditor = new WorkflowEditorPanel(m_mainHorizontalSplitter);
    m_mainHorizontalSplitter->addWidget(m_featureList);
    m_mainHorizontalSplitter->addWidget(m_workflowEditor);
    m_mainHorizontalSplitter->setStretchFactor(0, 0);
    m_mainHorizontalSplitter->setStretchFactor(1, 1);
    m_mainHorizontalSplitter->setCollapsible(0, false);
    m_mainHorizontalSplitter->setCollapsible(1, false);
    m_mainHorizontalSplitter->setSizes({200, 1080});

    auto* bottomPanel = new QWidget(contentHost);
    auto* bottomLayout = new QVBoxLayout(bottomPanel);
    bottomLayout->setContentsMargins(8, 6, 8, 8);
    bottomLayout->setSpacing(6);

    auto* targetGroup = new QGroupBox(tr("대상 창"), bottomPanel);
    auto* targetLayout = new QVBoxLayout(targetGroup);
    targetLayout->setSpacing(4);

    m_targetWindowDetailPanel = new TargetWindowDetailPanel(targetGroup);
    m_pickWindowButton = new QPushButton(tr("창 지정"), targetGroup);
    m_pickWindowButton->setToolTip(tr("클릭한 뒤 대상 창을 눌러 지정합니다. 우클릭 또는 Esc로 취소."));
    m_pickWindowListButton = new QPushButton(tr("창 목록"), targetGroup);
    m_pickWindowListButton->setToolTip(tr("현재 표시 중인 창 목록에서 대상 창을 선택합니다."));
    m_showTargetWindowButton = new QPushButton(tr("창 표시"), targetGroup);
    m_showTargetWindowButton->setToolTip(tr("지정된 대상 창 테두리를 잠시 깜빡여 표시합니다."));

    auto* buttonColumn = new QVBoxLayout();
    buttonColumn->setSpacing(4);
    buttonColumn->addWidget(m_pickWindowButton);
    buttonColumn->addWidget(m_pickWindowListButton);
    buttonColumn->addWidget(m_showTargetWindowButton);

    auto* detailRow = new QHBoxLayout();
    detailRow->setSpacing(8);
    detailRow->addWidget(m_targetWindowDetailPanel, 1);
    detailRow->addLayout(buttonColumn, 0);

    targetLayout->addLayout(detailRow);

    m_exitButton = new QPushButton(tr("종료"), bottomPanel);
    m_updateButton = new QPushButton(bottomPanel);
    m_updateButton->setToolTip(tr("GitHub 릴리즈에서 업데이트를 확인합니다."));
    m_calculatorButton = new QPushButton(tr("계산기"), bottomPanel);
    m_calculatorButton->setToolTip(tr("poe.ninja 시세 계산기"));
    m_settingsButton = new QPushButton(tr("설정"), bottomPanel);
    m_settingsButton->setToolTip(tr("프로그램 설정"));
    auto* exitRow = new QHBoxLayout;
    exitRow->addWidget(m_updateButton);
    exitRow->addStretch();
    exitRow->addWidget(m_calculatorButton);
    exitRow->addWidget(m_settingsButton);
    exitRow->addWidget(m_exitButton);

    auto* logGroup = new QGroupBox(tr("로그"), bottomPanel);
    auto* logLayout = new QVBoxLayout(logGroup);
    logLayout->setContentsMargins(0, 4, 0, 0);
    logLayout->setSpacing(4);

    m_logView = new QPlainTextEdit(logGroup);
    m_logView->setObjectName(QStringLiteral("logPanelView"));
    m_logView->setReadOnly(true);
    m_logView->setMaximumBlockCount(2000);
    m_logView->setMinimumHeight(48);
    m_logView->setStyleSheet(QStringLiteral(
        "QPlainTextEdit#logPanelView {"
        "  background-color: palette(window);"
        "  border: 1px solid palette(mid);"
        "  border-radius: 8px;"
        "  padding: 6px;"
        "}"));

    logLayout->addWidget(m_logView);

    m_bottomHorizontalSplitter = new QSplitter(Qt::Horizontal, bottomPanel);
    m_bottomHorizontalSplitter->addWidget(logGroup);
    m_bottomHorizontalSplitter->addWidget(targetGroup);
    m_bottomHorizontalSplitter->setStretchFactor(0, 1);
    m_bottomHorizontalSplitter->setStretchFactor(1, 1);
    m_bottomHorizontalSplitter->setCollapsible(0, false);
    m_bottomHorizontalSplitter->setCollapsible(1, false);
    m_bottomHorizontalSplitter->setSizes({520, 420});

    bottomLayout->addWidget(m_bottomHorizontalSplitter, 1);
    bottomLayout->addLayout(exitRow);

    m_mainVerticalSplitter = new QSplitter(Qt::Vertical, contentHost);
    m_mainVerticalSplitter->addWidget(m_mainHorizontalSplitter);
    m_mainVerticalSplitter->addWidget(bottomPanel);
    m_mainVerticalSplitter->setStretchFactor(0, 1);
    m_mainVerticalSplitter->setStretchFactor(1, 0);
    m_mainVerticalSplitter->setCollapsible(0, false);
    m_mainVerticalSplitter->setCollapsible(1, false);
    m_mainVerticalSplitter->setSizes({640, 240});

    contentLayout->addWidget(m_mainVerticalSplitter, 1);
    outerLayout->addWidget(contentHost, 1);

    setCentralWidget(central);

    statusBar()->hide();
}

void MainWindow::setupUiState() {
    m_uiState = new UiStateManager(this);
    m_uiState->registerMainWindow(this);
    m_uiState->registerSplitter(m_mainHorizontalSplitter, QStringLiteral("main/horizontal"));
    m_uiState->registerSplitter(m_mainVerticalSplitter, QStringLiteral("main/vertical"));
    m_uiState->registerSplitter(m_bottomHorizontalSplitter, QStringLiteral("main/bottomHorizontal"));
    m_uiState->registerSplitter(m_workflowEditor->workflowSplitter(), QStringLiteral("workflowEditor/vertical"));
    if (QHeaderView* blockHeader = m_workflowEditor->blockListHeader()) {
        m_uiState->registerHeader(blockHeader, QStringLiteral("workflowBlockList/header"));
    }
    m_uiState->registerSettingsHooks(
        QStringLiteral("featureList/columns"),
        [this](QSettings& settings) {
            if (m_featureList) {
                m_featureList->saveColumnLayout(settings,
                                                UiStateManager::settingsKey(
                                                    QStringLiteral("featureList/columns")));
            }
        },
        [this](const QSettings& settings) {
            if (m_featureList) {
                m_featureList->restoreColumnLayout(settings,
                                                   UiStateManager::settingsKey(
                                                       QStringLiteral("featureList/columns")));
            }
        });
    connect(m_featureList,
            &FeatureListPanel::columnLayoutChanged,
            m_uiState,
            &UiStateManager::scheduleSave);
    m_uiState->restoreAll();
    if (m_workflowEditor) {
        m_workflowEditor->applyBlockListHeaderResizeModes();
    }
    restoreAlwaysOnTopPreference();
}

void MainWindow::setupMenus() {
    QMenuBar* bar = m_titleBar ? m_titleBar->menuBar() : menuBar();
    auto* fileMenu = bar->addMenu(tr("파일(&F)"));
    fileMenu->addAction(tr("새 프로젝트(&N)"), this, &MainWindow::onNewProject);
    fileMenu->addAction(tr("프로젝트 열기(&O)..."), this, &MainWindow::onOpenProject);
    fileMenu->addAction(tr("프로젝트 저장(&S)"), this, &MainWindow::onSaveProject);
    fileMenu->addAction(tr("다른 이름으로 저장(&A)..."), this, &MainWindow::onSaveProjectAs);
    fileMenu->addSeparator();
    fileMenu->addAction(tr("업데이트(&U)..."), this, &MainWindow::onCheckForUpdates);
    fileMenu->addSeparator();
    fileMenu->addAction(tr("종료(&X)"), this, &MainWindow::onExitRequested);
}

void MainWindow::connectSignals() {
    m_featureList->setProject(m_project.get());
    connect(m_featureList, &FeatureListPanel::selectionChanged, this, &MainWindow::onFeatureSelectionChanged);
    connect(m_featureList, &FeatureListPanel::projectModified, this, &MainWindow::onProjectModified);
    connect(m_featureList, &FeatureListPanel::hotkeysChanged, this, &MainWindow::syncHotkeys);
    connect(m_featureList,
            &FeatureListPanel::featureRunRequested,
            this,
            &MainWindow::onFeatureRunRequested);

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

    connect(m_exitButton, &QPushButton::clicked, this, &MainWindow::onExitRequested);
    connect(m_settingsButton, &QPushButton::clicked, this, &MainWindow::onProgramSettings);
    connect(m_updateButton, &QPushButton::clicked, this, &MainWindow::onUpdateButtonClicked);
    connect(m_calculatorButton, &QPushButton::clicked, this, &MainWindow::onCalculator);
    connect(m_alwaysOnTopCheck, &QCheckBox::toggled, this, &MainWindow::onAlwaysOnTopToggled);
    connect(m_pickWindowButton, &QPushButton::clicked, this, &MainWindow::onPickTargetWindow);
    connect(m_pickWindowListButton, &QPushButton::clicked, this, &MainWindow::onPickTargetWindowFromList);
    connect(m_showTargetWindowButton, &QPushButton::clicked, this, &MainWindow::onShowTargetWindow);

    UserInputInterruptMonitor::instance().setHandler(
        [this](const std::string& featureId) { onUserInputInterrupt(featureId); });
}

void MainWindow::onExitRequested() {
    requestApplicationQuit();
}

void MainWindow::setupTrayIcon() {
    if (!QSystemTrayIcon::isSystemTrayAvailable()) {
        return;
    }

    m_trayIcon = new QSystemTrayIcon(windowIcon(), this);
    m_trayIcon->setToolTip(tr("PIPBONG"));

    m_trayMenu = new QMenu(this);
    m_trayMenu->addAction(tr("열기"), this, &MainWindow::showFromTray);
    m_trayMenu->addSeparator();
    m_trayMenu->addAction(tr("종료"), this, &MainWindow::requestApplicationQuit);
    m_trayIcon->setContextMenu(m_trayMenu);

    connect(m_trayIcon, &QSystemTrayIcon::activated, this, [this](QSystemTrayIcon::ActivationReason reason) {
        if (reason == QSystemTrayIcon::Trigger || reason == QSystemTrayIcon::DoubleClick) {
            showFromTray();
        }
    });

    applyCloseToTrayPolicy();
}

void MainWindow::applyCloseToTrayPolicy() {
    const bool closeToTray =
        ProgramSettings::closeToTray() && QSystemTrayIcon::isSystemTrayAvailable();
    QApplication::setQuitOnLastWindowClosed(!closeToTray);
    if (m_trayIcon) {
        m_trayIcon->setVisible(closeToTray);
    }
}

void MainWindow::hideToTray() {
    if (m_calculatorDialog) {
        m_calculatorDialog->hide();
    }
    hide();
    if (!m_trayIcon) {
        return;
    }
    m_trayIcon->show();
    if (!m_trayMinimizeNotified) {
        m_trayIcon->showMessage(tr("PIPBONG"),
                                tr("알림 영역에서 계속 실행 중입니다."),
                                QSystemTrayIcon::Information,
                                3000);
        m_trayMinimizeNotified = true;
    }
}

void MainWindow::showFromTray() {
    showNormal();
    raise();
    activateWindow();
}

void MainWindow::requestApplicationQuit() {
    if (hasAnyRunningSession()) {
        const auto reply = QMessageBox::question(this,
                                                 tr("종료"),
                                                 tr("워크플로가 실행 중입니다. 종료하시겠습니까?"),
                                                 QMessageBox::Yes | QMessageBox::No,
                                                 QMessageBox::No);
        if (reply != QMessageBox::Yes) {
            return;
        }
    }
    m_forceQuit = true;
    close();
}

void MainWindow::setupUpdateChecker() {
    m_updateChecker = new UpdateChecker(this);
    connect(m_updateChecker, &UpdateChecker::checkFinished, this, &MainWindow::onUpdateCheckFinished);
    connect(m_updateChecker, &UpdateChecker::readyToRestartForUpdate, this, &MainWindow::onReadyToRestartForUpdate);

    m_updateCheckTimer = new QTimer(this);
    connect(m_updateCheckTimer, &QTimer::timeout, this, &MainWindow::runSilentUpdateCheck);
    applyUpdateCheckInterval();
}

void MainWindow::applyUpdateCheckInterval() {
    if (!m_updateCheckTimer) {
        return;
    }
    const int minutes = ProgramSettings::updateCheckIntervalMinutes();
    if (minutes <= 0) {
        m_updateCheckTimer->stop();
        return;
    }
    m_updateCheckTimer->setInterval(minutes * 60 * 1000);
    m_updateCheckTimer->start();
}

void MainWindow::captureFeatureMouseLockPosition(FeatureRunSession& session) {
    session.hasMouseLockPosition = false;
    session.mouseLockAnchoredToTargetWindow = false;
    if (!session.lockMouseToCurrentPositionDuringRun) {
        return;
    }

    int screenX = 0;
    int screenY = 0;
    if (!InputSimulator::getCursorScreenPosition(screenX, screenY)) {
        return;
    }

#ifdef _WIN32
    const ScreenCapture::ScreenRect target = ScreenCapture::getTargetWindowScreenRect();
    if (target.valid) {
        session.mouseLockWindowOffsetX = screenX - target.x;
        session.mouseLockWindowOffsetY = screenY - target.y;
        session.mouseLockAnchoredToTargetWindow = true;
    } else
#endif
    {
        session.mouseLockScreenX = screenX;
        session.mouseLockScreenY = screenY;
    }
    session.hasMouseLockPosition = true;
}

bool MainWindow::hasFeatureMouseLock(const FeatureRunSession& session) {
    return session.lockMouseToCurrentPositionDuringRun || session.lockMouseToScreenCenterDuringRun;
}

void MainWindow::engageFeatureMouseLock(FeatureRunSession& session) {
    if (session.lockMouseToCurrentPositionDuringRun) {
        if (!session.hasMouseLockPosition) {
            captureFeatureMouseLockPosition(session);
        }
        if (!session.hasMouseLockPosition) {
            return;
        }
        if (session.mouseLockAnchoredToTargetWindow) {
            MouseCenterLock::engageAtTargetWindowOffset(session.mouseLockWindowOffsetX,
                                                        session.mouseLockWindowOffsetY);
        } else {
            MouseCenterLock::engageAt(session.mouseLockScreenX, session.mouseLockScreenY);
        }
        scheduleMouseLockPositionSync();
        return;
    }

    if (session.lockMouseToScreenCenterDuringRun) {
        MouseCenterLock::engageTargetWindowCenter();
        scheduleMouseLockPositionSync();
    }
}

void MainWindow::scheduleMouseLockPositionSync() {
    if (!m_mouseLockSyncTimer || !MouseCenterLock::isAnchoredToTargetWindow()) {
        return;
    }
    if (!m_mouseLockSyncTimer->isActive()) {
        m_mouseLockSyncTimer->start();
    }
}

void MainWindow::syncMouseLockPositions() {
    if (MouseCenterLock::isAnchoredToTargetWindow()) {
        MouseCenterLock::refreshAnchoredPosition();
    }
    if (!MouseCenterLock::isActive() && m_mouseLockSyncTimer) {
        m_mouseLockSyncTimer->stop();
    }
}

void MainWindow::refreshUpdateButtonState() {
    if (!m_updateButton || !m_updateChecker) {
        return;
    }

    if (m_updateChecker->hasPendingUpdate()) {
        const QString version = m_updateChecker->pendingUpdate().version.toString();
        m_updateButton->setText(tr("v%1 버전으로 업데이트").arg(version));
        m_updateButton->setEnabled(true);
        m_updateButton->setToolTip(tr("GitHub 릴리즈 v%1을(를) 다운로드해 설치합니다.").arg(version));
        return;
    }

    const QString currentVersion = QCoreApplication::applicationVersion();
    m_updateButton->setText(tr("v%1 - 최신 버전입니다").arg(currentVersion));
    m_updateButton->setEnabled(true);
    m_updateButton->setToolTip(tr("클릭하면 GitHub 릴리즈에서 업데이트를 다시 확인합니다."));
}

void MainWindow::runSilentUpdateCheck() {
    if (!m_updateChecker) {
        return;
    }
    m_lastUpdateCheckWasSilent = true;
    m_updateChecker->checkForUpdates(UpdateChecker::CheckUiMode::Silent);
}

void MainWindow::onUpdateCheckFinished(bool success, bool updateAvailable, const QString& errorMessage) {
    refreshUpdateButtonState();
    if (!success && !updateAvailable && !errorMessage.isEmpty() && m_updateButton) {
        m_updateButton->setToolTip(errorMessage);
    }
    const bool wasSilentCheck = m_lastUpdateCheckWasSilent;
    if (success && !updateAvailable && !wasSilentCheck) {
        showTransientStatus(
            tr("현재 최신 버전입니다. (v%1)").arg(QCoreApplication::applicationVersion()),
            3000);
    }
    if (success && updateAvailable && wasSilentCheck && ProgramSettings::autoInstallUpdates()) {
        m_autoUpdateDeferred = true;
        maybeStartAutomaticUpdate();
    }
    m_lastUpdateCheckWasSilent = false;
}

void MainWindow::onUpdateButtonClicked() {
    if (hasAnyRunningSession()) {
        QMessageBox::warning(this,
                             tr("업데이트"),
                             tr("워크플로 실행 중에는 업데이트할 수 없습니다. 먼저 실행을 중지하세요."));
        return;
    }

    if (!m_updateChecker) {
        return;
    }

    if (m_updateChecker->hasPendingUpdate()) {
        const UpdateChecker::ReleaseInfo release = m_updateChecker->pendingUpdate();
        const auto reply =
            QMessageBox::question(this,
                                  tr("업데이트"),
                                  tr("v%1(으)로 업데이트하시겠습니까?\n현재 버전: v%2")
                                      .arg(release.version.toString())
                                      .arg(QCoreApplication::applicationVersion()),
                                  QMessageBox::Yes | QMessageBox::No,
                                  QMessageBox::Yes);
        if (reply == QMessageBox::Yes) {
            m_updateChecker->installPendingUpdate();
        }
        return;
    }

    m_lastUpdateCheckWasSilent = false;
    m_updateChecker->checkForUpdates(UpdateChecker::CheckUiMode::Interactive);
}

void MainWindow::onCheckForUpdates() {
    if (hasAnyRunningSession()) {
        QMessageBox::warning(this,
                             tr("업데이트"),
                             tr("워크플로 실행 중에는 업데이트할 수 없습니다. 먼저 실행을 중지하세요."));
        return;
    }

    if (!m_updateChecker) {
        return;
    }
    m_lastUpdateCheckWasSilent = false;
    m_updateChecker->checkForUpdates(UpdateChecker::CheckUiMode::Interactive);
}

void MainWindow::maybeStartAutomaticUpdate() {
    if (m_autoUpdateInstallStarted || !m_autoUpdateDeferred || !m_updateChecker
        || !m_updateChecker->hasPendingUpdate() || !ProgramSettings::autoInstallUpdates()) {
        return;
    }

    if (hasAnyRunningSession()) {
        const QString version = m_updateChecker->pendingUpdate().version.toString();
        showTransientStatus(tr("v%1 업데이트 감지 — 실행 종료 후 자동 업데이트").arg(version), 5000);
        return;
    }

    m_autoUpdateDeferred = false;
    m_autoUpdateInstallStarted = true;
    const QString version = m_updateChecker->pendingUpdate().version.toString();
    showTransientStatus(tr("v%1 자동 업데이트를 시작합니다.").arg(version), 5000);
    m_updateChecker->installPendingUpdate();
}

void MainWindow::onProgramSettings() {
    ProgramSettingsDialog dialog(this);
    if (dialog.exec() != QDialog::Accepted) {
        return;
    }
    applyCloseToTrayPolicy();
    applyUpdateCheckInterval();
    if (ProgramSettings::autoInstallUpdates() && m_updateChecker && m_updateChecker->hasPendingUpdate()) {
        m_autoUpdateDeferred = true;
        maybeStartAutomaticUpdate();
    }

    if (ProgramSettings::runAsAdministrator() && !WindowsRunAsAdmin::isProcessElevated()) {
        const auto reply = QMessageBox::question(
            this,
            tr("관리자 권한"),
            tr("관리자 권한으로 실행하려면 PIPBONG을 다시 시작해야 합니다.\n"
               "지금 관리자 권한으로 다시 시작하시겠습니까?"),
            QMessageBox::Yes | QMessageBox::No,
            QMessageBox::Yes);
        if (reply != QMessageBox::Yes) {
            return;
        }
        if (!WindowsRunAsAdmin::relaunchElevated()) {
            QMessageBox::warning(this,
                                 tr("관리자 권한"),
                                 tr("관리자 권한으로 다시 시작하지 못했습니다."));
            return;
        }
        m_forceQuit = true;
        prepareForShutdown();
        QApplication::quit();
    }
}

void MainWindow::onCalculator() {
    if (!m_calculatorDialog) {
        m_calculatorDialog = new CalculatorDialog(this);
        m_calculatorDialog->setAttribute(Qt::WA_DeleteOnClose);
        connect(m_calculatorDialog, &QObject::destroyed, this, [this]() { m_calculatorDialog = nullptr; });
    }
    m_calculatorDialog->show();
    m_calculatorDialog->raise();
    m_calculatorDialog->activateWindow();
}

void MainWindow::onAlwaysOnTopToggled(bool checked) {
    applyAlwaysOnTop(checked);
    QSettings settings;
    settings.setValue(alwaysOnTopPreferenceKey(), checked);
}

void MainWindow::applyAlwaysOnTop(bool enabled) {
#ifdef _WIN32
    applyNativeAlwaysOnTop(this, enabled);
#else
    Qt::WindowFlags flags = windowFlags();
    if (enabled) {
        flags |= Qt::WindowStaysOnTopHint;
    } else {
        flags &= ~Qt::WindowStaysOnTopHint;
    }
    setWindowFlags(flags);
    show();
#endif
}

void MainWindow::showEvent(QShowEvent* event) {
    QMainWindow::showEvent(event);
    if (!m_initialUpdateCheckDone) {
        m_initialUpdateCheckDone = true;
        QTimer::singleShot(0, this, &MainWindow::runSilentUpdateCheck);
    }
#ifdef _WIN32
    const HWND hwnd = reinterpret_cast<HWND>(winId());
    if (hwnd && IsWindow(hwnd)) {
        const DWM_WINDOW_CORNER_PREFERENCE cornerPreference = DWMWCP_ROUND;
        DwmSetWindowAttribute(hwnd,
                              DWMWA_WINDOW_CORNER_PREFERENCE,
                              &cornerPreference,
                              sizeof(cornerPreference));
    }
    if (m_alwaysOnTopCheck) {
        applyNativeAlwaysOnTop(this, m_alwaysOnTopCheck->isChecked());
    }
#endif
}

#if defined(Q_OS_WIN)
bool MainWindow::nativeEvent(const QByteArray& eventType, void* message, qintptr* result) {
    if (eventType == "windows_generic_MSG") {
        auto* msg = static_cast<MSG*>(message);
        if (msg->message == WM_NCHITTEST && !isMaximized()) {
            const int border = 8;
            const POINT pt = {GET_X_LPARAM(msg->lParam), GET_Y_LPARAM(msg->lParam)};
            RECT rect{};
            GetWindowRect(msg->hwnd, &rect);

            const bool canResizeWidth = minimumWidth() < maximumWidth();
            const bool canResizeHeight = minimumHeight() < maximumHeight();
            const bool atLeft = pt.x >= rect.left && pt.x < rect.left + border;
            const bool atRight = pt.x < rect.right && pt.x >= rect.right - border;
            const bool atTop = pt.y >= rect.top && pt.y < rect.top + border;
            const bool atBottom = pt.y < rect.bottom && pt.y >= rect.bottom - border;

            if (canResizeWidth && canResizeHeight && atTop && atLeft) {
                *result = HTTOPLEFT;
                return true;
            }
            if (canResizeWidth && canResizeHeight && atTop && atRight) {
                *result = HTTOPRIGHT;
                return true;
            }
            if (canResizeWidth && canResizeHeight && atBottom && atLeft) {
                *result = HTBOTTOMLEFT;
                return true;
            }
            if (canResizeWidth && canResizeHeight && atBottom && atRight) {
                *result = HTBOTTOMRIGHT;
                return true;
            }
            if (canResizeWidth && atLeft) {
                *result = HTLEFT;
                return true;
            }
            if (canResizeWidth && atRight) {
                *result = HTRIGHT;
                return true;
            }
            if (canResizeHeight && atTop) {
                *result = HTTOP;
                return true;
            }
            if (canResizeHeight && atBottom) {
                *result = HTBOTTOM;
                return true;
            }
        }
    }
    return QMainWindow::nativeEvent(eventType, message, result);
}
#endif

void MainWindow::restoreAlwaysOnTopPreference() {
    if (!m_alwaysOnTopCheck) {
        return;
    }

    QSettings settings;
    const bool enabled = settings.value(alwaysOnTopPreferenceKey(), false).toBool();
    QSignalBlocker blocker(m_alwaysOnTopCheck);
    m_alwaysOnTopCheck->setChecked(enabled);
    applyAlwaysOnTop(enabled);
}

QString MainWindow::alwaysOnTopPreferenceKey() const {
    return QStringLiteral("ui/state/mainWindow/alwaysOnTop");
}

void MainWindow::syncWindowTitleDisplay() {
    QString title = m_baseWindowTitle;
    if (!m_transientStatusMessage.isEmpty()) {
        title += QStringLiteral(" - ") + m_transientStatusMessage;
    } else if (!m_persistentStatusMessage.isEmpty()) {
        title += QStringLiteral(" - ") + m_persistentStatusMessage;
    }
    setWindowTitle(title);
    if (m_titleBar) {
        m_titleBar->syncFromWindowTitle();
    }
}

void MainWindow::refreshTitleBarStatus() {
    syncWindowTitleDisplay();
}

void MainWindow::showTransientStatus(const QString& message, int timeoutMs) {
    if (message.isEmpty()) {
        m_transientStatusMessage.clear();
        if (m_statusClearTimer) {
            m_statusClearTimer->stop();
        }
        refreshTitleBarStatus();
        return;
    }
    if (m_statusClearTimer) {
        m_statusClearTimer->stop();
    }
    m_transientStatusMessage = message;
    refreshTitleBarStatus();
    if (m_statusClearTimer && timeoutMs > 0) {
        m_statusClearTimer->start(timeoutMs);
    }
}

void MainWindow::setPersistentStatus(const QString& message) {
    m_persistentStatusMessage = message.trimmed();
    refreshTitleBarStatus();
}

void MainWindow::clearStatusMessage() {
    if (m_statusClearTimer) {
        m_statusClearTimer->stop();
    }
    m_transientStatusMessage.clear();
    refreshTitleBarStatus();
}

void MainWindow::onReadyToRestartForUpdate() {
    m_forceQuit = true;
    if (m_updateCheckTimer) {
        m_updateCheckTimer->stop();
    }
    if (m_trayIcon) {
        m_trayIcon->hide();
    }
    hide();
    prepareForShutdown();
    close();
    QTimer::singleShot(0, qApp, []() { QApplication::exit(0); });
}

void MainWindow::prepareForShutdown() {
    m_hotkeyManager->unregisterAll();
    UserInputInterruptMonitor::instance().unregisterAll();
    MouseCenterLock::releaseAll();
    stopAllSessions();
    QCoreApplication::processEvents(QEventLoop::ExcludeUserInputEvents);
    ScreenRegionOverlay::dismissAll();
    MatchTestOverlay::dismissAll();
    RoiPreviewOverlay::dismissAll();
    WorkflowMatchFeedbackOverlay::dismissAll();
    WorkflowRoiFlashOverlay::dismissAll();
    TargetWindowHighlightOverlay::dismissAll();
    if (m_calculatorDialog) {
        m_calculatorDialog->close();
    }
    WindowPicker::cancelPick();
    WindowPickerHoverOverlay::dismissAll();
    CursorPositionPicker::cancelPick();
    m_autoSaveTimer->stop();
    saveSelectedFeaturePreference();
    autoSaveProject();
    if (m_uiState) {
        m_uiState->saveNow();
    }
}

void MainWindow::closeEvent(QCloseEvent* event) {
    if (!m_forceQuit && ProgramSettings::closeToTray() && QSystemTrayIcon::isSystemTrayAvailable()) {
        hideToTray();
        event->ignore();
        return;
    }

    if (!m_forceQuit && hasAnyRunningSession()) {
        const auto reply = QMessageBox::question(
            this,
            tr("종료"),
            tr("워크플로가 실행 중입니다. 종료하시겠습니까?"),
            QMessageBox::Yes | QMessageBox::No,
            QMessageBox::No);
        if (reply != QMessageBox::Yes) {
            event->ignore();
            return;
        }
        m_forceQuit = true;
    }

    prepareForShutdown();
    event->accept();
    QMainWindow::closeEvent(event);
}

void MainWindow::onNewProject() {
    if (!maybeSave()) {
        return;
    }

    m_project = std::make_unique<Project>();
    m_projectFilePath = Application::autoSaveFilePath();
    ScreenCapture::setTargetWindow(nullptr);
    ScreenCapture::setTargetWindowTitle(L"");
    m_featureList->setProject(m_project.get());
    m_featureList->refresh();
    onFeatureSelectionChanged();
    m_modified = false;
    updateWindowTitle();
    syncHotkeys();
    updateTargetWindowDetails();
    autoSaveProject();
    appendLog(tr("새 프로젝트를 만들었습니다."));
}

void MainWindow::onOpenProject() {
    if (!maybeSave()) {
        return;
    }

    const QString path = QFileDialog::getOpenFileName(
        this, tr("프로젝트 열기"), Application::instance()->projectDirectory(),
        tr("PIPBONG 프로젝트 (*.json)"));
    if (path.isEmpty()) {
        return;
    }
    loadProjectFromFile(path);
}

void MainWindow::onSaveProject() {
    if (!ensureProjectFilePath()) {
        onSaveProjectAs();
        return;
    }

    if (JsonSerializer::saveToFile(*m_project, m_projectFilePath, Application::instance()->projectDirectory())) {
        m_modified = false;
        updateWindowTitle();
        appendLog(tr("프로젝트를 저장했습니다."));
        showTransientStatus(tr("저장됨"), 2000);
    } else {
        QMessageBox::critical(this, tr("프로젝트 저장"), tr("프로젝트 저장에 실패했습니다."));
    }
}

void MainWindow::onSaveProjectAs() {
    const QString path = QFileDialog::getSaveFileName(
        this, tr("다른 이름으로 저장"), Application::instance()->projectDirectory() + QStringLiteral("/project.json"),
        tr("PIPBONG 프로젝트 (*.json)"));
    if (path.isEmpty()) {
        return;
    }

    m_projectFilePath = path;
    QSettings settings;
    settings.setValue(QStringLiteral("project/lastFile"), m_projectFilePath);
    onSaveProject();
}

void MainWindow::onFeatureSelectionChanged() {
    saveSelectedFeaturePreference();
    refreshWorkflowEditor();
}

void MainWindow::saveSelectedFeaturePreference() {
    if (!m_featureList || m_projectFilePath.isEmpty()) {
        return;
    }
    const QString featureId = m_featureList->selectedFeatureId();
    if (featureId.isEmpty()) {
        return;
    }
    QSettings settings;
    settings.setValue(selectedFeaturePreferenceKey(), featureId);
}

void MainWindow::restoreSelectedFeaturePreference() {
    if (!m_featureList || m_projectFilePath.isEmpty()) {
        return;
    }
    QSettings settings;
    const QString featureId = settings.value(selectedFeaturePreferenceKey()).toString();
    if (!featureId.isEmpty()) {
        m_featureList->selectFeatureById(featureId);
    }
}

QString MainWindow::selectedFeaturePreferenceKey() const {
    const QString projectKey = QFileInfo(m_projectFilePath).absoluteFilePath();
    return QStringLiteral("project/selectedFeatureId/%1").arg(QString::number(qHash(projectKey)));
}

void MainWindow::onProjectModified() {
    scheduleAutoSave();
}

void MainWindow::scheduleAutoSave() {
    m_modified = true;
    updateWindowTitle();
    m_autoSaveTimer->start(800);
}

bool MainWindow::ensureProjectFilePath() {
    if (m_projectFilePath.isEmpty()) {
        m_projectFilePath = Application::autoSaveFilePath();
    }
    return !m_projectFilePath.isEmpty();
}

void MainWindow::autoSaveProject() {
    if (!ensureProjectFilePath()) {
        return;
    }

    if (JsonSerializer::saveToFile(*m_project, m_projectFilePath, Application::instance()->projectDirectory())) {
        m_modified = false;
        updateWindowTitle();
        showTransientStatus(tr("자동 저장됨"), 2000);
    }
}

void MainWindow::refreshWorkflowEditor() {
    Feature* feature = m_featureList->selectedFeature();
    m_workflowEditor->setProjectDirectory(Application::instance()->projectDirectory());
    m_workflowEditor->setFeature(feature);
    if (feature) {
        if (FeatureRunSession* session = sessionFor(feature->id())) {
            if (session->runningBlockIndex >= 0) {
                m_workflowEditor->setActiveBlockIndex(session->runningBlockIndex, session->runningBlockHighlight);
            }
            syncLoopTimingToWorkflowEditor(session);
        }
    } else {
        m_workflowEditor->clearLoopTiming();
    }
    updateRunUiState();
}

bool MainWindow::isDisplayedRunningFeature(const FeatureRunSession* session) const {
    if (!session || !m_featureList) {
        return false;
    }
    const Feature* selected = m_featureList->selectedFeature();
    return selected && selected->id() == session->featureId;
}

void MainWindow::applyRunningBlockVisuals(FeatureRunSession& session,
                                          int index,
                                          BlockListWidget::ExecutionHighlight highlight) {
    session.runningBlockIndex = index;
    session.runningBlockHighlight = highlight;
    if (isDisplayedRunningFeature(&session)) {
        m_workflowEditor->setActiveBlockIndex(index, highlight);
    }
}

FeatureRunSession* MainWindow::sessionFor(const std::string& featureId) {
    const auto it = m_runSessions.find(featureId);
    return it == m_runSessions.end() ? nullptr : &it->second;
}

const FeatureRunSession* MainWindow::sessionFor(const std::string& featureId) const {
    const auto it = m_runSessions.find(featureId);
    return it == m_runSessions.end() ? nullptr : &it->second;
}

FeatureRunSession* MainWindow::sessionForEngine(const QObject* sender) {
    const auto* engine = qobject_cast<const WorkflowEngine*>(sender);
    if (!engine) {
        return nullptr;
    }
    for (auto& entry : m_runSessions) {
        if (entry.second.engine.get() == engine) {
            return &entry.second;
        }
    }
    return nullptr;
}

bool MainWindow::isFeatureRunning(const std::string& featureId) const {
    return m_runSessions.find(featureId) != m_runSessions.end();
}

bool MainWindow::hasAnyRunningSession() const {
    return !m_runSessions.empty();
}

QSet<QString> MainWindow::runningFeatureIds() const {
    QSet<QString> ids;
    for (const auto& entry : m_runSessions) {
        ids.insert(QString::fromStdString(entry.first));
    }
    return ids;
}

QString MainWindow::featureDisplayName(const std::string& featureId) const {
    if (!m_project) {
        return QString::fromStdString(featureId);
    }
    if (Feature* feature = m_project->featureById(featureId)) {
        return QString::fromStdString(feature->name());
    }
    return QString::fromStdString(featureId);
}

void MainWindow::appendSessionLog(const FeatureRunSession& session, const QString& message) {
    appendLog(tr("[%1] %2").arg(featureDisplayName(session.featureId), message));
}

void MainWindow::connectSessionEngine(FeatureRunSession& session) {
    WorkflowEngine* engine = session.engine.get();
    if (!engine) {
        return;
    }

    connect(engine, &WorkflowEngine::logMessage, this, &MainWindow::onEngineLog);
    connect(engine, &WorkflowEngine::sessionPrepared, this, &MainWindow::onEngineSessionPrepared);
    connect(engine, &WorkflowEngine::started, this, &MainWindow::onEngineStarted);
    connect(engine, &WorkflowEngine::finished, this, &MainWindow::onEngineFinished);
    connect(engine, &WorkflowEngine::blockStarted, this, &MainWindow::onBlockStarted);
    connect(engine, &WorkflowEngine::blockFinished, this, &MainWindow::onBlockFinished);
    connect(engine, &WorkflowEngine::blockProgress, this, &MainWindow::onBlockProgress);
    connect(engine, &WorkflowEngine::blockMatchResult, this, &MainWindow::onBlockMatchResult);
    connect(engine, &WorkflowEngine::blockImageFindAttempt, this, &MainWindow::onBlockImageFindAttempt);
    connect(engine, &WorkflowEngine::imageFindFailureHandling, this,
            &MainWindow::onImageFindFailureHandling);
    connect(engine, &WorkflowEngine::imageFindReturnToPrevious, this,
            &MainWindow::onImageFindReturnToPrevious);
    connect(engine, &WorkflowEngine::pointerFeedbackAtClientPoint, this,
            &MainWindow::onPointerFeedbackAtClientPoint);
}

void MainWindow::updateRunUiState() {
    if (m_featureList) {
        m_featureList->setRunningFeatureIds(runningFeatureIds());
    }

    Feature* selected = m_featureList ? m_featureList->selectedFeature() : nullptr;
    const bool selectedRunning = selected && isFeatureRunning(selected->id());
    if (m_featureList) {
        m_featureList->setEditControlsEnabled(!selectedRunning);
    }
    if (m_workflowEditor) {
        m_workflowEditor->setEditingEnabled(!selectedRunning);
    }

    if (hasAnyRunningSession()) {
        bool anyPaused = false;
        bool anyTriggerMonitoring = false;
        for (const auto& entry : m_runSessions) {
            if (entry.second.sessionContext && entry.second.sessionContext->isPaused()) {
                anyPaused = true;
            }
            if (entry.second.runningMode == FeatureRunMode::Trigger
                && entry.second.triggerPhase == TriggerSessionPhase::Monitoring) {
                anyTriggerMonitoring = true;
            }
        }
        if (anyPaused) {
            setPersistentStatus(tr("일시정지 — 입력하여 재개"));
        } else if (anyTriggerMonitoring) {
            setPersistentStatus(tr("트리거 감시 중 (%1)").arg(m_runSessions.size()));
        } else {
            setPersistentStatus(tr("실행 중 (%1)").arg(m_runSessions.size()));
        }
    } else {
        m_persistentStatusMessage.clear();
        if (m_transientStatusMessage.isEmpty()) {
            refreshTitleBarStatus();
        }
    }

    maybeStartAutomaticUpdate();
}

void MainWindow::stopFeatureRun(const std::string& featureId) {
    FeatureRunSession* session = sessionFor(featureId);
    if (!session || !session->engine) {
        return;
    }

    session->userStopRequested = true;
    session->repeatSession = false;
    session->holdRunActive = false;
    ++session->triggerCooldownGeneration;
    session->engine->stop();
    UserInputInterruptMonitor::instance().unregisterSession(featureId);
    appendSessionLog(*session, tr("중지 요청됨."));
}

void MainWindow::stopAllSessions() {
    UserInputInterruptMonitor::instance().unregisterAll();
    MouseCenterLock::releaseAll();
    for (auto& entry : m_runSessions) {
        if (entry.second.sessionContext) {
            entry.second.sessionContext->endRunInputSession();
        }
        if (entry.second.engine) {
            entry.second.userStopRequested = true;
            entry.second.repeatSession = false;
            entry.second.holdRunActive = false;
            ++entry.second.triggerCooldownGeneration;
            entry.second.engine->stopAndWait();
        }
        restoreRunStartCursorPosition(entry.second);
    }
    m_runSessions.clear();
    updateRunUiState();
}

void MainWindow::onFeatureRunRequested(const QString& featureId) {
    if (!m_project || featureId.isEmpty()) {
        return;
    }
    Feature* feature = m_project->featureById(featureId.toStdString());
    if (!feature) {
        return;
    }
    if (feature->runMode() == FeatureRunMode::Hold) {
        QMessageBox::information(this, tr("실행"),
                                 tr("누를 동안 방식은 단축키를 누르고 있는 동안 워크플로가 무한 반복됩니다. 키를 떼면 중지됩니다."));
        return;
    }
    if (isFeatureRunning(feature->id())) {
        stopFeatureRun(feature->id());
        return;
    }
    startFeatureRun(feature);
}

void MainWindow::selectRunningFeatureForDisplay(Feature* feature) {
    if (!feature || !m_featureList || !ProgramSettings::autoSelectRunningFeature()) {
        return;
    }
    const QString featureId = QString::fromStdString(feature->id());
    if (m_featureList->selectedFeatureId() == featureId) {
        return;
    }
    m_featureList->selectFeatureById(featureId);
}

void MainWindow::startFeatureRun(Feature* feature, bool fromHotkey) {
    if (!feature) {
        return;
    }
    if (feature->workflow().blocks().empty()) {
        QMessageBox::information(this, tr("실행"), tr("선택한 기능에 블록이 없습니다."));
        return;
    }
    if (feature->runMode() == FeatureRunMode::Trigger
        && WorkflowRunner::firstImageFindBlockIndex(feature->workflow()) < 0) {
        QMessageBox::information(this,
                                 tr("실행"),
                                 tr("트리거 모드에는 템플릿이 지정된 템플릿 매칭 블록이 최소 하나 필요합니다."));
        return;
    }
    if (isFeatureRunning(feature->id())) {
        return;
    }

    FeatureRunSession session;
    session.featureId = feature->id();
    session.engine = std::make_unique<WorkflowEngine>(this);
    session.userStopRequested = false;
    session.runningMode = feature->runMode();
    session.hotkeyLaunchedSession = fromHotkey;
    session.repeatSession = session.runningMode == FeatureRunMode::RepeatInfinite
                            || session.runningMode == FeatureRunMode::RepeatCount
                            || session.runningMode == FeatureRunMode::Hold
                            || session.runningMode == FeatureRunMode::Trigger;
    session.repeatRemaining = feature->repeatCount();
    session.holdRunActive = session.runningMode == FeatureRunMode::Hold;
    if (session.runningMode == FeatureRunMode::Trigger) {
        session.triggerPhase = TriggerSessionPhase::Monitoring;
        session.triggerBlockIndex = WorkflowRunner::firstImageFindBlockIndex(feature->workflow());
    }
    if (session.runningMode == FeatureRunMode::Hold) {
        ++session.holdRepeatGeneration;
    }
    session.restoreMousePositionOnEnd = feature->restoreMousePositionOnEnd();
    session.lockMouseToScreenCenterDuringRun = feature->lockMouseToScreenCenterDuringRun();
    session.lockMouseToCurrentPositionDuringRun = feature->lockMouseToCurrentPositionDuringRun();

    const std::string featureId = session.featureId;
    connectSessionEngine(session);
    m_runSessions.emplace(featureId, std::move(session));
    selectRunningFeatureForDisplay(feature);
    FeatureRunSession& activeSession = m_runSessions.at(featureId);
    if (tryBeginFirstTemplateRoiEdit(activeSession, feature)) {
        return;
    }
    if (feature->runMode() == FeatureRunMode::Trigger) {
        launchTriggerMonitor(activeSession, feature, true);
        return;
    }
    launchWorkflowRun(activeSession, feature, false);
}

ImageFindBlock* firstImageFindWithEditableRoi(Workflow& workflow) {
    for (const auto& block : workflow.blocks()) {
        if (!block || block->type() != BlockType::ImageFind) {
            continue;
        }
        auto* imageFind = static_cast<ImageFindBlock*>(block.get());
        if (!imageFind->hasTemplates()) {
            continue;
        }
        if (imageFind->customRegionsAnchoredToTargetWindow) {
            if (imageFind->customRegionsWindowPercent.empty()) {
                continue;
            }
        } else if (imageFind->customRegions.empty()) {
            continue;
        }
        return imageFind;
    }
    return nullptr;
}

bool MainWindow::tryBeginFirstTemplateRoiEdit(FeatureRunSession& session, Feature* feature) {
    if (!feature || !feature->editFirstTemplateRoiOnStart()) {
        return false;
    }

    ImageFindBlock* block = firstImageFindWithEditableRoi(feature->workflow());
    if (!block) {
        return false;
    }

    QPointer<MainWindow> self = this;
    const std::string featureId = session.featureId;
    auto confirmed = std::make_shared<bool>(false);

    RoiPreviewOverlay::EditableOptions options;
    options.enabled = true;
    options.activeIndex = 0;
    options.onRoiEdited = [block](int index, const CaptureRegion& region) {
        if (index < 0) {
            return;
        }
        block->customRegionsAnchoredToTargetWindow = true;
        block->customRegions.clear();
        block->customRegion = {};
        block->searchArea = SearchArea::CustomRegion;
        if (index >= static_cast<int>(block->customRegionsWindowPercent.size())) {
            return;
        }
        block->customRegionsWindowPercent[static_cast<size_t>(index)] =
            ScreenCapture::storeWindowPercentFromPhysical(region);
    };
    options.onConfirm = [self, featureId, confirmed]() {
        if (!self) {
            return;
        }
        *confirmed = true;
        RoiPreviewOverlay::dismissAll();

        FeatureRunSession* activeSession = self->sessionFor(featureId);
        Feature* activeFeature = self->m_project ? self->m_project->featureById(featureId) : nullptr;
        if (!activeSession || !activeFeature) {
            self->m_runSessions.erase(featureId);
            self->updateRunUiState();
            return;
        }

        self->m_modified = true;
        self->scheduleAutoSave();
        self->refreshWorkflowEditor();
        if (activeFeature->runMode() == FeatureRunMode::Trigger) {
            self->launchTriggerMonitor(*activeSession, activeFeature, true);
        } else {
            self->launchWorkflowRun(*activeSession, activeFeature, false);
        }
    };

    const std::vector<CaptureRegion> physicalRegions = [&]() {
        std::vector<CaptureRegion> physical;
        if (block->customRegionsAnchoredToTargetWindow) {
            physical.reserve(block->customRegionsWindowPercent.size());
            for (const PercentRegion& percent : block->customRegionsWindowPercent) {
                if (percent.width <= 0.0 || percent.height <= 0.0) {
                    continue;
                }
                physical.push_back(ScreenCapture::resolveWindowPercentRegion(percent));
            }
        } else {
            physical.reserve(block->customRegions.size());
            for (const CaptureRegion& region : block->customRegions) {
                if (region.width < 2 || region.height < 2) {
                    continue;
                }
                physical.push_back(region);
            }
        }
        return physical;
    }();
    const CaptureRegion physicalLegacy = physicalRegions.empty()
                                           ? CaptureRegion{}
                                           : physicalRegions.front();

    const bool shown = RoiPreviewOverlay::show(
        block->searchArea,
        physicalLegacy,
        block->percentRegion,
        physicalRegions,
        this,
        [self, featureId, confirmed](bool visible) {
            if (visible || !self) {
                return;
            }
            if (*confirmed) {
                return;
            }
            self->m_runSessions.erase(featureId);
            self->updateRunUiState();
        },
        options);
    return shown;
}

void MainWindow::ensureRunSessionResources(FeatureRunSession& session,
                                           Feature* feature,
                                           bool refreshWorkflow) {
    if (!feature) {
        return;
    }
    if (refreshWorkflow || !session.sessionWorkflow) {
        session.sessionWorkflow = std::shared_ptr<Workflow>(feature->workflow().clone());
    }
    if (!session.sessionContext) {
        session.sessionContext = std::make_shared<ExecutionContext>();
    }
    syncRunSessionContext(session);
}

void MainWindow::syncRunSessionContext(FeatureRunSession& session) {
    if (!session.sessionContext) {
        return;
    }
    session.sessionContext->setTargetWindowTitle(currentTargetWindowTitleW());
    session.sessionContext->setProjectDirectory(Application::instance()->projectDirectory().toStdString());
}

void MainWindow::applyFeatureRunPoliciesToContext(FeatureRunSession& session, Feature* feature) {
    if (!session.sessionContext || !feature) {
        return;
    }

    session.pointerVisualFeedback = feature->pointerVisualFeedback();
    session.sessionContext->setPointerVisualFeedback(feature->pointerVisualFeedback());
    session.restoreMousePositionOnEnd = feature->restoreMousePositionOnEnd();
    session.lockMouseToScreenCenterDuringRun = feature->lockMouseToScreenCenterDuringRun();
    session.lockMouseToCurrentPositionDuringRun = feature->lockMouseToCurrentPositionDuringRun();

    const bool infiniteStyle = session.runningMode == FeatureRunMode::RepeatInfinite
                             || session.runningMode == FeatureRunMode::Hold
                             || session.runningMode == FeatureRunMode::Trigger;
    const int exitAfter = feature->infiniteExitAfterConsecutiveMisses();
    if (infiniteStyle && exitAfter > 0) {
        session.sessionContext->setImageFindMaxMissAttempts(1);
    } else {
        session.sessionContext->setImageFindMaxMissAttempts(0);
    }

    session.sessionContext->setRoiCorrectionSession(feature->roiCorrectionSessionEligible(),
                                                    feature->roiCorrection());
    session.sessionContext->setRunLoopNumber(session.sessionIteration + 1);
}

void MainWindow::logLoopCompletion(FeatureRunSession& session, bool success, const QString& message) {
    const qint64 elapsedMs = session.loopTimer.isValid() ? session.loopTimer.elapsed() : 0;
    session.loopTimer.invalidate();

    const int loopNumber = session.sessionIteration + 1;
    session.hasLastLoopTiming = true;
    session.lastLoopNumber = loopNumber;
    session.lastLoopElapsedMs = elapsedMs;
    session.totalLoopElapsedMs += elapsedMs;
    ++session.completedLoopCount;
    session.lastLoopAverageMs =
        session.completedLoopCount > 0 ? session.totalLoopElapsedMs / session.completedLoopCount : 0;
    session.lastLoopSuccess = success;

    QString line = tr("루프 %1: %2 ms (%3)")
                       .arg(loopNumber)
                       .arg(elapsedMs)
                       .arg(success ? tr("성공") : tr("실패"));
    if (!success && !message.isEmpty()) {
        line += QStringLiteral(" — ") + message;
    }
    appendSessionLog(session, line);
    if (isDisplayedRunningFeature(&session)) {
        syncLoopTimingToWorkflowEditor(&session);
    }
}

void MainWindow::syncLoopTimingToWorkflowEditor(const FeatureRunSession* session) {
    if (!m_workflowEditor) {
        return;
    }
    if (!session || !session->hasLastLoopTiming || !isDisplayedRunningFeature(session)) {
        m_workflowEditor->clearLoopTiming();
        return;
    }
    m_workflowEditor->setLoopTiming(session->lastLoopNumber,
                                    session->lastLoopElapsedMs,
                                    session->lastLoopAverageMs,
                                    session->lastLoopSuccess);
}

bool MainWindow::shouldLogRunDetails(const FeatureRunSession& session) const {
    return !session.repeatSession || session.sessionIteration <= 0;
}

void MainWindow::continueRepeatSession(FeatureRunSession& session,
                                       Feature* feature,
                                       bool success,
                                       const QString& message) {
    if (!shouldContinueRunSession(session, feature)) {
        finishRunSession(session.featureId, success, message);
        return;
    }
    if (session.engine && session.engine->isRunning()) {
        return;
    }
    launchWorkflowRun(session, feature, true);
}

void MainWindow::launchWorkflowRun(FeatureRunSession& session, Feature* feature, bool repeatIteration) {
    if (!feature || !session.engine || session.engine->isRunning()) {
        return;
    }

    if (!repeatIteration) {
        syncTargetWindowTitleToCapture();
        session.sessionIteration = 0;
        session.hasLastLoopTiming = false;
        session.totalLoopElapsedMs = 0;
        session.completedLoopCount = 0;
        session.lastLoopAverageMs = 0;
        captureRunStartCursorPosition(session);
        captureFeatureMouseLockPosition(session);
        engageFeatureMouseLock(session);
        if (isDisplayedRunningFeature(&session)) {
            syncLoopTimingToWorkflowEditor(&session);
            m_workflowEditor->clearBlockMatchResults();
            m_workflowEditor->clearExecutionHighlight();
            m_workflowEditor->persistRunFeedbackForCurrentFeature();
        }

        if (shouldLogRunDetails(session)) {
            appendSessionLog(session, tr("기능 실행"));
        }
        updateRunUiState();
        session.runningBlockIndex = -1;
        session.runningBlockHighlight = BlockListWidget::ExecutionHighlight::None;
    } else {
        ++session.sessionIteration;
    }

    if (!session.sessionContext) {
        session.sessionContext = std::make_shared<ExecutionContext>();
    }
    syncRunSessionContext(session);
    applyFeatureRunPoliciesToContext(session, feature);
    if (!repeatIteration && session.sessionContext) {
        session.sessionContext->clearCorrectedRois();
    }
    syncUserInputInterruptForSession(session, feature);

    const std::wstring targetTitle = currentTargetWindowTitleW();
    const std::string projectDir = Application::instance()->projectDirectory().toStdString();
    const bool skipTargetActivation = session.hotkeyLaunchedSession && !repeatIteration;
    WorkflowEngine* engine = session.engine.get();

    if (repeatIteration) {
        ensureRunSessionResources(session, feature, false);
        engine->runPrepared([&session]() {
            PreparedWorkflowRun run;
            run.workflow = session.sessionWorkflow;
            run.context = session.sessionContext;
            run.context->resetStop();
#ifdef _WIN32
            ScreenCapture::activateTargetWindow();
#endif
            return run;
        });
        return;
    }

    Feature* featurePtr = feature;
    engine->runPrepared([this, featurePtr, &session, targetTitle, projectDir, skipTargetActivation]() {
        PreparedWorkflowRun run;
        run.workflow = std::shared_ptr<Workflow>(featurePtr->workflow().clone());
        run.context = session.sessionContext;
        run.context->setTargetWindowTitle(targetTitle);
        run.context->setProjectDirectory(projectDir);
        ScreenCapture::setTargetWindowTitle(targetTitle);
        run.context->resetStop();
#ifdef _WIN32
        if (!skipTargetActivation) {
            ScreenCapture::activateTargetWindow();
        }
#endif
        return run;
    });
}

bool MainWindow::shouldContinueRunSession(const FeatureRunSession& session, Feature* feature) const {
    if (!feature || session.userStopRequested) {
        return false;
    }

    switch (session.runningMode) {
    case FeatureRunMode::Hold:
        return session.repeatSession && session.holdRunActive && m_hotkeyManager
               && m_hotkeyManager->isHoldBindingDown(session.featureId);
    case FeatureRunMode::RepeatInfinite:
        return session.repeatSession;
    case FeatureRunMode::Trigger:
        return session.repeatSession;
    case FeatureRunMode::RepeatCount:
        return session.repeatSession && session.repeatRemaining > 0;
    default:
        return false;
    }
}

void MainWindow::scheduleHoldRepeat(FeatureRunSession& session,
                                    Feature* feature,
                                    bool success,
                                    const QString& message) {
    if (!shouldContinueRunSession(session, feature)) {
        finishRunSession(session.featureId, success, message);
        return;
    }

    const quint64 generation = ++session.holdRepeatGeneration;
    const std::string featureId = session.featureId;
    QTimer::singleShot(0, this, [this, featureId, success, message, generation]() {
        FeatureRunSession* session = sessionFor(featureId);
        if (!session || generation != session->holdRepeatGeneration) {
            return;
        }

        Feature* current = m_project ? m_project->featureById(featureId) : nullptr;
        if (!current) {
            finishRunSession(featureId, success, message);
            return;
        }
        continueRepeatSession(*session, current, success, message);
    });
}

void MainWindow::launchTriggerMonitor(FeatureRunSession& session, Feature* feature, bool firstSessionStart) {
    if (!feature || !session.engine || session.engine->isRunning()) {
        return;
    }

    if (firstSessionStart) {
        syncTargetWindowTitleToCapture();
        session.sessionIteration = 0;
        session.hasLastLoopTiming = false;
        session.totalLoopElapsedMs = 0;
        session.completedLoopCount = 0;
        session.lastLoopAverageMs = 0;
        captureRunStartCursorPosition(session);
        captureFeatureMouseLockPosition(session);
        engageFeatureMouseLock(session);
        if (isDisplayedRunningFeature(&session)) {
            syncLoopTimingToWorkflowEditor(&session);
            m_workflowEditor->clearBlockMatchResults();
            m_workflowEditor->clearExecutionHighlight();
            m_workflowEditor->persistRunFeedbackForCurrentFeature();
        }
        appendSessionLog(session, tr("트리거 감시 시작"));
        updateRunUiState();
        session.runningBlockIndex = -1;
        session.runningBlockHighlight = BlockListWidget::ExecutionHighlight::None;
    }

    session.triggerPhase = TriggerSessionPhase::Monitoring;
    if (!session.sessionContext) {
        session.sessionContext = std::make_shared<ExecutionContext>();
    }
    syncRunSessionContext(session);
    applyFeatureRunPoliciesToContext(session, feature);
    session.sessionContext->setTriggerMonitorBlockIndex(session.triggerBlockIndex);
    session.sessionContext->setImageFindPrimedBlockIndex(-1);
    session.sessionContext->clearConsumedMatchRegions();
    session.sessionContext->clearLastMatch();
    session.sessionContext->clearLastMatchAttempt();
    syncUserInputInterruptForSession(session, feature);

    ensureRunSessionResources(session, feature, session.sessionIteration > 0);

    const std::wstring targetTitle = currentTargetWindowTitleW();
    const std::string projectDir = Application::instance()->projectDirectory().toStdString();
    const bool skipTargetActivation = session.hotkeyLaunchedSession && firstSessionStart;
    WorkflowEngine* engine = session.engine.get();

    Feature* featurePtr = feature;
    engine->runPrepared([this, featurePtr, &session, targetTitle, projectDir, skipTargetActivation]() {
        PreparedWorkflowRun run;
        run.workflow = session.sessionWorkflow;
        if (!run.workflow) {
            run.workflow = std::shared_ptr<Workflow>(featurePtr->workflow().clone());
        }
        run.context = session.sessionContext;
        run.context->setTargetWindowTitle(targetTitle);
        run.context->setProjectDirectory(projectDir);
        ScreenCapture::setTargetWindowTitle(targetTitle);
        run.context->resetStop();
        run.context->setTriggerMonitorBlockIndex(session.triggerBlockIndex);
        run.context->setImageFindPrimedBlockIndex(-1);
#ifdef _WIN32
        if (!skipTargetActivation) {
            ScreenCapture::activateTargetWindow();
        }
#endif
        return run;
    });
}

void MainWindow::launchTriggerActionRun(FeatureRunSession& session, Feature* feature) {
    if (!feature || !session.engine || session.engine->isRunning()) {
        return;
    }

    pauseOtherSessionsForTrigger(session);

    session.triggerPhase = TriggerSessionPhase::RunningAction;
    if (session.sessionContext) {
        session.sessionContext->setTriggerMonitorBlockIndex(-1);
        session.sessionContext->setImageFindPrimedBlockIndex(session.triggerBlockIndex);
    }
    appendSessionLog(session, tr("트리거 발동 — 워크플로 실행"));
    launchWorkflowRun(session, feature, session.sessionIteration > 0);
}

void MainWindow::pauseOtherSessionsForTrigger(FeatureRunSession& triggerSession) {
    triggerSession.triggerPreemptedSessions.clear();
    triggerSession.triggerPreemptSavedCursor = false;
    triggerSession.triggerReleasedOwnMouseLockForPreempt = false;

    for (auto& entry : m_runSessions) {
        if (entry.first == triggerSession.featureId) {
            continue;
        }

        FeatureRunSession& other = entry.second;
        if (other.userStopRequested || !other.sessionContext) {
            continue;
        }

        TriggerPreemptedSession snap;
        snap.featureId = other.featureId;

        if (!other.sessionContext->isPaused()) {
            other.sessionContext->setPaused(true);
            snap.pausedByTrigger = true;
        }

        if (hasFeatureMouseLock(other)) {
            MouseCenterLock::release();
            snap.releasedMouseLock = true;
        }

        if (snap.pausedByTrigger || snap.releasedMouseLock) {
            triggerSession.triggerPreemptedSessions.push_back(std::move(snap));
            appendSessionLog(other, tr("트리거 발동 — 일시정지"));
        }
    }

    if (triggerSession.triggerPreemptedSessions.empty()) {
        return;
    }

    int screenX = 0;
    int screenY = 0;
    if (InputSimulator::getCursorScreenPosition(screenX, screenY)) {
        triggerSession.triggerPreemptCursorScreenX = screenX;
        triggerSession.triggerPreemptCursorScreenY = screenY;
        triggerSession.triggerPreemptSavedCursor = true;
    }

    if (hasFeatureMouseLock(triggerSession)) {
        MouseCenterLock::release();
        triggerSession.triggerReleasedOwnMouseLockForPreempt = true;
    }

    appendSessionLog(triggerSession,
                     tr("다른 기능 %1개 일시정지").arg(triggerSession.triggerPreemptedSessions.size()));
    updateRunUiState();
}

void MainWindow::resumePreemptedSessionsForTrigger(FeatureRunSession& triggerSession) {
    if (triggerSession.triggerPreemptedSessions.empty()
        && !triggerSession.triggerPreemptSavedCursor
        && !triggerSession.triggerReleasedOwnMouseLockForPreempt) {
        return;
    }

    if (triggerSession.triggerPreemptSavedCursor) {
        InputSimulator::moveMouse(triggerSession.triggerPreemptCursorScreenX,
                                  triggerSession.triggerPreemptCursorScreenY);
        triggerSession.triggerPreemptSavedCursor = false;
    }

    if (triggerSession.triggerReleasedOwnMouseLockForPreempt) {
        engageFeatureMouseLock(triggerSession);
        triggerSession.triggerReleasedOwnMouseLockForPreempt = false;
    }

    for (const TriggerPreemptedSession& snap : triggerSession.triggerPreemptedSessions) {
        FeatureRunSession* other = sessionFor(snap.featureId);
        if (!other || !other->sessionContext) {
            continue;
        }

        if (snap.releasedMouseLock) {
            engageFeatureMouseLock(*other);
        }
        if (snap.pausedByTrigger) {
            other->sessionContext->setPaused(false);
            appendSessionLog(*other, tr("트리거 완료 — 재개"));
        }
    }

    triggerSession.triggerPreemptedSessions.clear();
    updateRunUiState();
}

void MainWindow::scheduleTriggerCooldown(FeatureRunSession& session, Feature* feature) {
    if (!shouldContinueRunSession(session, feature)) {
        finishRunSession(session.featureId, session.lastLoopSuccess, QString());
        return;
    }
    if (!feature) {
        finishRunSession(session.featureId, false, tr("기능을 찾을 수 없음"));
        return;
    }

    session.triggerPhase = TriggerSessionPhase::Cooldown;
    updateRunUiState();

    const int cooldownMs = feature->triggerCooldownMs();
    if (cooldownMs <= 0) {
        launchTriggerMonitor(session, feature, false);
        return;
    }

    appendSessionLog(session, tr("재감지 대기 %1 ms").arg(cooldownMs));
    const quint64 generation = ++session.triggerCooldownGeneration;
    const std::string featureId = session.featureId;
    QTimer::singleShot(cooldownMs, this, [this, featureId, generation]() {
        FeatureRunSession* activeSession = sessionFor(featureId);
        if (!activeSession || generation != activeSession->triggerCooldownGeneration) {
            return;
        }
        if (activeSession->userStopRequested || !activeSession->repeatSession) {
            finishRunSession(featureId, activeSession->lastLoopSuccess, QString());
            return;
        }
        Feature* current = m_project ? m_project->featureById(featureId) : nullptr;
        if (!current) {
            finishRunSession(featureId, false, tr("기능을 찾을 수 없음"));
            return;
        }
        launchTriggerMonitor(*activeSession, current, false);
    });
}

void MainWindow::handleTriggerEngineFinished(FeatureRunSession& session,
                                             Feature* feature,
                                             bool success,
                                             const QString& message) {
    if (session.triggerPhase == TriggerSessionPhase::Monitoring) {
        if (session.sessionContext) {
            session.sessionContext->setTriggerMonitorBlockIndex(-1);
        }
        if (!success) {
            finishRunSession(session.featureId, false, message);
            return;
        }
        launchTriggerActionRun(session, feature);
        return;
    }

    if (session.triggerPhase == TriggerSessionPhase::RunningAction) {
        logLoopCompletion(session, success, message);
        if (!success && !session.userStopRequested) {
            appendSessionLog(session, tr("워크플로 실패 — 감시 재개"));
        }
        resumePreemptedSessionsForTrigger(session);
        scheduleTriggerCooldown(session, feature);
    }
}

void MainWindow::finishRunSession(const std::string& featureId, bool success, const QString& message) {
    FeatureRunSession* session = sessionFor(featureId);
    if (session && isDisplayedRunningFeature(session)) {
        m_workflowEditor->clearExecutionHighlight();
        m_workflowEditor->persistRunFeedbackForCurrentFeature();
    }

    if (session) {
        showTransientStatus(tr("[%1] %2")
                                .arg(featureDisplayName(featureId), success ? tr("완료") : tr("실패")),
                            3000);
    }

    if (session && session->sessionContext) {
        session->sessionContext->endRunInputSession();
    }

    if (session && session->runningMode == FeatureRunMode::Trigger) {
        resumePreemptedSessionsForTrigger(*session);
    }

    if (session) {
        ++session->triggerCooldownGeneration;
        if (hasFeatureMouseLock(*session)) {
            MouseCenterLock::release();
        }
        restoreRunStartCursorPosition(*session);
    }

    UserInputInterruptMonitor::instance().unregisterSession(featureId);
    m_runSessions.erase(featureId);
    if (!hasAnyRunningSession()) {
        WorkflowMatchFeedbackOverlay::dismissAll();
        WorkflowRoiFlashOverlay::dismissAll();
    }
    updateRunUiState();
    Q_UNUSED(message);
}

void MainWindow::runFeature(Feature* feature) {
    startFeatureRun(feature);
}

void MainWindow::onHotkeyTriggered(const QString& featureId) {
    if (!m_project) {
        return;
    }

    Feature* feature = m_project->featureById(featureId.toStdString());
    if (!feature) {
        return;
    }

    if (feature->runMode() == FeatureRunMode::Hold) {
        return;
    }

    if (isFeatureRunning(featureId.toStdString())) {
        stopFeatureRun(featureId.toStdString());
        return;
    }

    startFeatureRun(feature, true);
}

void MainWindow::onHotkeyHoldStarted(const QString& featureId) {
    if (!m_project) {
        return;
    }

    Feature* feature = m_project->featureById(featureId.toStdString());
    if (!feature || feature->runMode() != FeatureRunMode::Hold) {
        return;
    }

    if (isFeatureRunning(featureId.toStdString())) {
        return;
    }

    startFeatureRun(feature, true);
}

void MainWindow::onHotkeyHoldEnded(const QString& featureId) {
    FeatureRunSession* session = sessionFor(featureId.toStdString());
    if (!session || !session->holdRunActive) {
        return;
    }

    ++session->holdRepeatGeneration;
    session->holdRunActive = false;
    session->repeatSession = false;

    if (session->engine && session->engine->isRunning()) {
        session->userStopRequested = true;
        session->engine->stop();
        return;
    }

    session->userStopRequested = false;
    finishRunSession(featureId.toStdString(), true, QString());
}

void MainWindow::syncHotkeys() {
    if (!m_hotkeyManager || !m_project) {
        return;
    }

    const auto failures = m_hotkeyManager->syncFromProject(*m_project);
#ifdef _WIN32
    if (!m_hotkeyManager->isKeyboardHookActive()
        && (!failures.empty() || m_project->features().size() > 0)) {
        bool hasKeyboardBinding = false;
        for (const auto& feature : m_project->features()) {
            if (!feature->hotkey().isEmpty() && !feature->hotkey().isMouseButton()) {
                hasKeyboardBinding = true;
                break;
            }
        }
        if (hasKeyboardBinding) {
            appendLog(tr("키보드 단축키 후크를 설치하지 못했습니다. RegisterHotKey 백업을 시도합니다. "
                         "계속 동작하지 않으면 PIPBONG을 관리자 권한으로 실행하거나 보안 프로그램 예외를 확인하세요."));
        }
    }
    if (!m_hotkeyManager->isMouseHookActive()) {
        bool hasMouseBinding = false;
        for (const auto& feature : m_project->features()) {
            if (!feature->hotkey().isEmpty() && feature->hotkey().isMouseButton()) {
                hasMouseBinding = true;
                break;
            }
        }
        if (hasMouseBinding) {
            appendLog(tr("마우스 단축키 후크를 설치하지 못했습니다. 마우스 단축키가 동작하지 않을 수 있습니다."));
        }
    }
#endif
    for (const auto& failure : failures) {
        appendLog(tr("단축키 등록 실패 (%1): 시스템에서 이미 사용 중이거나 권한이 부족할 수 있습니다.")
                      .arg(QString::fromStdString(failure.featureName)));
    }
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

void MainWindow::onPickTargetWindow() {
#ifdef _WIN32
    setPersistentStatus(tr("대상 창을 클릭하세요. 우클릭 또는 Esc로 취소"));
    WindowPicker::startPick(this, [this](const WindowPicker::Result& result) {
        setPersistentStatus(QString());
        if (!result.accepted || !result.hwnd) {
            showTransientStatus(tr("창 지정이 취소되었습니다."), 3000);
            return;
        }

        const QString title = QString::fromStdWString(result.title);
        ScreenCapture::setTargetWindowTitle(result.title);
        ScreenCapture::setTargetWindow(result.hwnd);
        m_project->setTargetWindowTitle(title.toStdString());
        scheduleAutoSave();
        appendLog(tr("창 지정: %1").arg(title.isEmpty() ? tr("(제목 없음)") : title));
        showTransientStatus(tr("대상 창을 지정했습니다."), 3000);
        updateTargetWindowDetails();
        TargetWindowHighlightOverlay::flashSelectionWave(this);
        scheduleRunWarmup();
    });
#else
    QMessageBox::information(this, tr("창 지정"), tr("창 지정은 Windows에서만 지원됩니다."));
#endif
}

void MainWindow::onPickTargetWindowFromList() {
#ifdef _WIN32
    QDialog dialog(this);
    dialog.setWindowTitle(tr("창 목록"));
    dialog.resize(760, 460);

    auto* layout = new QVBoxLayout(&dialog);
    layout->setContentsMargins(12, 12, 12, 12);
    layout->setSpacing(8);

    auto* hintLabel = new QLabel(
        tr("현재 데스크톱에 표시된 최상위 창 목록입니다. 더블클릭하거나 선택 후 확인을 누르세요."),
        &dialog);
    hintLabel->setWordWrap(true);
    layout->addWidget(hintLabel);

    auto* listWidget = new QListWidget(&dialog);
    listWidget->setSelectionMode(QAbstractItemView::SingleSelection);
    listWidget->setIconSize(QSize(24, 24));
    layout->addWidget(listWidget, 1);

    auto* refreshButton = new QPushButton(tr("새로고침"), &dialog);
    auto* buttonRow = new QHBoxLayout();
    buttonRow->addWidget(refreshButton);
    buttonRow->addStretch();

    auto* buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog);
    auto* okButton = buttonBox->button(QDialogButtonBox::Ok);
    if (okButton) {
        okButton->setText(tr("선택"));
        okButton->setEnabled(false);
    }
    if (auto* cancelButton = buttonBox->button(QDialogButtonBox::Cancel)) {
        cancelButton->setText(tr("취소"));
    }
    buttonRow->addWidget(buttonBox);
    layout->addLayout(buttonRow);

    auto populateList = [listWidget]() {
        const HWND currentTarget = ScreenCapture::targetWindow();
        const QVector<WindowListEntry> entries = collectWindowListEntries();
        listWidget->clear();

        int selectedIndex = -1;
        for (int i = 0; i < entries.size(); ++i) {
            const auto& entry = entries[i];
            auto* item = new QListWidgetItem(entry.displayText, listWidget);
            item->setIcon(entry.icon);
            item->setData(Qt::UserRole, QVariant::fromValue(reinterpret_cast<qulonglong>(entry.hwnd)));
            item->setData(Qt::UserRole + 1, QString::fromStdWString(entry.title));
            if (currentTarget && currentTarget == entry.hwnd) {
                selectedIndex = i;
            }
        }

        if (selectedIndex >= 0) {
            listWidget->setCurrentRow(selectedIndex);
        } else if (listWidget->count() > 0) {
            listWidget->setCurrentRow(0);
        }
    };

    connect(refreshButton, &QPushButton::clicked, &dialog, populateList);
    connect(listWidget, &QListWidget::itemDoubleClicked, &dialog, [&dialog](QListWidgetItem*) { dialog.accept(); });
    connect(listWidget, &QListWidget::currentItemChanged, &dialog, [listWidget, okButton](QListWidgetItem* current) {
        if (okButton) {
            okButton->setEnabled(current != nullptr);
        }
        if (!current) {
            WindowPickerHoverOverlay::dismissAll();
            return;
        }
        const HWND hwnd = reinterpret_cast<HWND>(current->data(Qt::UserRole).toULongLong());
        if (hwnd && IsWindow(hwnd)) {
            WindowPickerHoverOverlay::updateHover(hwnd);
        } else {
            WindowPickerHoverOverlay::dismissAll();
        }
    });
    connect(buttonBox, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    connect(buttonBox, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);

    struct ListPickerHoverGuard {
        ~ListPickerHoverGuard() { WindowPickerHoverOverlay::dismissAll(); }
    } hoverGuard;

    populateList();
    if (listWidget->count() == 0) {
        QMessageBox::information(this, tr("창 목록"), tr("표시 중인 창을 찾지 못했습니다."));
        return;
    }

    if (dialog.exec() != QDialog::Accepted) {
        return;
    }

    QListWidgetItem* currentItem = listWidget->currentItem();
    if (!currentItem) {
        return;
    }

    const qulonglong hwndValue = currentItem->data(Qt::UserRole).toULongLong();
    const HWND hwnd = reinterpret_cast<HWND>(hwndValue);
    if (!hwnd || !IsWindow(hwnd)) {
        QMessageBox::warning(this, tr("창 목록"), tr("선택한 창이 더 이상 유효하지 않습니다. 다시 선택하세요."));
        return;
    }

    const QString title = currentItem->data(Qt::UserRole + 1).toString();
    const HWND selectedHwnd = hwnd;
    ScreenCapture::setTargetWindowTitle(title.toStdWString());
    ScreenCapture::setTargetWindow(selectedHwnd);
    m_project->setTargetWindowTitle(title.toStdString());
    scheduleAutoSave();
    appendLog(tr("창 지정: %1").arg(title.isEmpty() ? tr("(제목 없음)") : title));
    showTransientStatus(tr("대상 창을 목록에서 지정했습니다."), 3000);
    updateTargetWindowDetails();
    QTimer::singleShot(0, this, [this, selectedHwnd]() {
        TargetWindowHighlightOverlay::flashSelectionWaveForHwnd(selectedHwnd, this);
    });
    scheduleRunWarmup();
#else
    QMessageBox::information(this, tr("창 목록"), tr("창 목록은 Windows에서만 지원됩니다."));
#endif
}

void MainWindow::onShowTargetWindow() {
#ifdef _WIN32
    if (TargetWindowHighlightOverlay::flash(this)) {
        showTransientStatus(tr("대상 창을 표시했습니다."), 2500);
    }
#else
    QMessageBox::information(this, tr("창 표시"), tr("창 표시는 Windows에서만 지원됩니다."));
#endif
}

void MainWindow::onEngineLog(const QString& message) {
    appendLog(message);
}

void MainWindow::onEngineSessionPrepared(std::shared_ptr<Workflow> workflow,
                                         std::shared_ptr<ExecutionContext> context) {
    FeatureRunSession* session = sessionForEngine(sender());
    if (!session) {
        return;
    }
    if (workflow) {
        session->sessionWorkflow = std::move(workflow);
    }
    if (context && !session->sessionContext) {
        session->sessionContext = std::move(context);
    }
}

void MainWindow::onEngineStarted() {
    FeatureRunSession* session = sessionForEngine(sender());
    if (!session) {
        return;
    }
    if (session->runningMode == FeatureRunMode::Trigger
        && session->triggerPhase != TriggerSessionPhase::RunningAction) {
        updateRunUiState();
        return;
    }
    session->loopTimer.start();
    updateRunUiState();
}

void MainWindow::onEngineFinished(bool success, const QString& message) {
    FeatureRunSession* session = sessionForEngine(sender());
    if (!session) {
        return;
    }

    Feature* feature = m_project ? m_project->featureById(session->featureId) : nullptr;

    if (session->runningMode == FeatureRunMode::Trigger) {
        handleTriggerEngineFinished(*session, feature, success, message);
        return;
    }

    logLoopCompletion(*session, success, message);

    if (success && feature && session->runningMode == FeatureRunMode::RepeatCount && session->repeatSession) {
        --session->repeatRemaining;
    }

    const bool infiniteExitEnabled =
        feature && feature->infiniteExitAfterConsecutiveMisses() > 0
        && (session->runningMode == FeatureRunMode::RepeatInfinite
            || session->runningMode == FeatureRunMode::Hold);

    if (infiniteExitEnabled) {
        if (success) {
            session->consecutiveDetectionFailLoops = 0;
        } else if (session->sessionContext && session->sessionContext->detectionFailedThisRun()) {
            ++session->consecutiveDetectionFailLoops;
            if (session->consecutiveDetectionFailLoops >= feature->infiniteExitAfterConsecutiveMisses()) {
                finishRunSession(session->featureId,
                                 false,
                                 tr("연속 감지 실패 %1회 — 실행 종료")
                                     .arg(feature->infiniteExitAfterConsecutiveMisses()));
                return;
            }
        } else if (!success) {
            finishRunSession(session->featureId, success, message);
            return;
        }
    }

    if (session->runningMode == FeatureRunMode::Hold) {
        scheduleHoldRepeat(*session, feature, success, message);
        return;
    }

    if (shouldContinueRunSession(*session, feature)
        && (success || infiniteExitEnabled)) {
        continueRepeatSession(*session, feature, success, message);
        return;
    }

    finishRunSession(session->featureId, success, message);
}

void MainWindow::onBlockStarted(int index, const QString& summary) {
    FeatureRunSession* session = sessionForEngine(sender());
    if (!session) {
        return;
    }
    if (shouldLogRunDetails(*session)) {
        appendSessionLog(*session, tr("블록 %1 시작: %2").arg(index + 1).arg(summary));
    }
    applyRunningBlockVisuals(*session, index, BlockListWidget::ExecutionHighlight::Running);
}

void MainWindow::onBlockProgress(int index, BlockProgressKind kind) {
    FeatureRunSession* session = sessionForEngine(sender());
    if (!session) {
        return;
    }

    switch (kind) {
    case BlockProgressKind::ImageFindMiss:
        if (isDisplayedRunningFeature(session) && m_workflowEditor->isBlockMatchSuccessCommitted(index)) {
            break;
        }
        session->runningBlockIndex = index;
        session->runningBlockHighlight = BlockListWidget::ExecutionHighlight::ImageFindMiss;
        if (isDisplayedRunningFeature(session)) {
            m_workflowEditor->notifyImageFindRetry(index);
        }
        break;
    case BlockProgressKind::ImageFindSuccess:
        applyRunningBlockVisuals(*session, index, BlockListWidget::ExecutionHighlight::Success);
        if (isDisplayedRunningFeature(session)) {
            m_workflowEditor->markBlockMatchSuccess(index);
        }
        break;
    }
}

void MainWindow::onBlockMatchResult(int index,
                                    double matchThreshold,
                                    double confidence,
                                    const QPixmap& matchPreview,
                                    bool matched,
                                    bool hasClientPoint,
                                    int clientX,
                                    int clientY) {
    FeatureRunSession* session = sessionForEngine(sender());
    if (hasClientPoint && session && session->pointerVisualFeedback) {
        WorkflowMatchFeedbackOverlay::pulseAtClientPoint(
            clientX,
            clientY,
            matched ? RunPointerFeedbackKind::MatchSuccess : RunPointerFeedbackKind::MatchMiss);
    }

    if (!session || !isDisplayedRunningFeature(session)) {
        return;
    }
    m_workflowEditor->setBlockMatchResult(index, matchThreshold, confidence, matchPreview, matched);
}

void MainWindow::onPointerFeedbackAtClientPoint(int clientX, int clientY) {
    FeatureRunSession* session = sessionForEngine(sender());
    if (!session || !session->pointerVisualFeedback) {
        return;
    }
    WorkflowMatchFeedbackOverlay::pulseAtClientPoint(clientX, clientY, RunPointerFeedbackKind::Click);
}

void MainWindow::onBlockFinished(int index, bool success, const QString& message, qint64 durationMs,
                                 qint64 imageFindMatchDurationMs, int imageFindPollAttempts) {
    FeatureRunSession* session = sessionForEngine(sender());
    if (!session) {
        return;
    }

    if (shouldLogRunDetails(*session) || !success) {
        appendSessionLog(*session,
                         tr("블록 %1 %2: %3")
                             .arg(index + 1)
                             .arg(success ? tr("성공") : tr("실패"))
                             .arg(message));
    }
    if (isDisplayedRunningFeature(session)) {
        m_workflowEditor->setBlockDuration(index, durationMs);
        m_workflowEditor->setBlockImageFindMatchDuration(index, imageFindMatchDurationMs);
        if (imageFindPollAttempts > 0) {
            m_workflowEditor->setBlockImageFindAttemptCount(index, imageFindPollAttempts);
        }
    }
    if (!success) {
        applyRunningBlockVisuals(*session, index, BlockListWidget::ExecutionHighlight::Failed);
    } else {
        applyRunningBlockVisuals(*session, index, BlockListWidget::ExecutionHighlight::Success);
        if (isDisplayedRunningFeature(session)) {
            m_workflowEditor->markBlockMatchSuccess(index);
        }
    }
}

void MainWindow::onBlockImageFindAttempt(int index,
                                         int attemptCount,
                                         double matchThreshold,
                                         double detectedConfidence,
                                         bool matched) {
    FeatureRunSession* session = sessionForEngine(sender());
    if (!session || !isDisplayedRunningFeature(session)) {
        return;
    }
    m_workflowEditor->setBlockImageFindAttemptCount(index, attemptCount);
    if (!matched) {
        m_workflowEditor->setBlockMatchResult(index, matchThreshold, detectedConfidence, QPixmap(), false);
    }
}

void MainWindow::onImageFindFailureHandling(int index,
                                            int returnToPreviousCount,
                                            int retryAfterNextCount) {
    FeatureRunSession* session = sessionForEngine(sender());
    if (!session || !isDisplayedRunningFeature(session)) {
        return;
    }
    m_workflowEditor->setBlockImageFindFailureHandlingCounts(
        index, returnToPreviousCount, retryAfterNextCount);
}

void MainWindow::onImageFindReturnToPrevious(int sourceIndex, int targetIndex) {
    FeatureRunSession* session = sessionForEngine(sender());
    if (!session || !isDisplayedRunningFeature(session)) {
        return;
    }
    m_workflowEditor->notifyImageFindReturnToPrevious(sourceIndex, targetIndex);
}

void MainWindow::appendLog(const QString& message) {
    m_logView->appendPlainText(message);
}

bool MainWindow::maybeSave() {
    if (!m_modified) {
        return true;
    }

    m_autoSaveTimer->stop();
    autoSaveProject();
    return true;
}

void MainWindow::loadProjectFromFile(const QString& path) {
    QString projectDirectory;
    auto loaded = JsonSerializer::loadFromFile(path, &projectDirectory);
    if (!loaded) {
        QMessageBox::critical(this, tr("프로젝트 열기"), tr("프로젝트를 불러오지 못했습니다."));
        return;
    }

    m_project = std::move(loaded);
    m_projectFilePath = path;
    if (!projectDirectory.isEmpty()) {
        Application::instance()->setProjectDirectory(projectDirectory);
    }

    ScreenCapture::setTargetWindow(nullptr);
    syncTargetWindowTitleToCapture();
#ifdef _WIN32
    if (HWND hwnd = ScreenCapture::findTargetWindow()) {
        ScreenCapture::setTargetWindow(hwnd);
    }
#endif

    m_featureList->setProject(m_project.get());
    m_featureList->refresh();
    restoreSelectedFeaturePreference();
    onFeatureSelectionChanged();

    m_modified = false;
    updateWindowTitle();
    syncHotkeys();
    updateTargetWindowDetails();

    QSettings settings;
    settings.setValue(QStringLiteral("project/lastFile"), m_projectFilePath);
    appendLog(tr("프로젝트 불러옴: %1").arg(path));
}

void MainWindow::updateWindowTitle() {
    m_baseWindowTitle = QStringLiteral("PIPBONG %1").arg(QCoreApplication::applicationVersion());
    if (!m_projectFilePath.isEmpty()) {
        m_baseWindowTitle += QStringLiteral(" - ") + QFileInfo(m_projectFilePath).fileName();
    } else {
        m_baseWindowTitle += QStringLiteral(" - ") + tr("제목 없음");
    }
    if (m_modified) {
        m_baseWindowTitle += QStringLiteral(" *");
    }
    syncWindowTitleDisplay();
}

std::wstring MainWindow::currentTargetWindowTitleW() const {
    if (!m_project) {
        return {};
    }
    return QString::fromStdString(m_project->targetWindowTitle()).toStdWString();
}

void MainWindow::syncTargetWindowTitleToCapture() {
    ScreenCapture::setTargetWindowTitle(currentTargetWindowTitleW());
}

void MainWindow::updateTargetWindowDetails() {
    if (!m_targetWindowDetailPanel) {
        return;
    }

#ifdef _WIN32
    const QString savedTitle =
        QString::fromStdString(m_project ? m_project->targetWindowTitle() : std::string{}).trimmed();
    syncTargetWindowTitleToCapture();

    HWND hwnd = ScreenCapture::targetWindow();
    if (!hwnd || !IsWindow(hwnd)) {
        hwnd = ScreenCapture::findTargetWindow();
        if (hwnd) {
            ScreenCapture::setTargetWindow(hwnd);
        }
    }

    if (!hwnd || !IsWindow(hwnd)) {
        if (savedTitle.isEmpty()) {
            m_targetWindowDetailPanel->showMessage(tr("'창 지정'으로 대상 창을 선택하세요."));
        } else {
            m_targetWindowDetailPanel->showMessage(tr("감지된 창 없음 — '창 지정'으로 다시 선택하세요."));
        }
        return;
    }

    ScreenCapture::TargetWindowInfo info;
    if (!ScreenCapture::queryWindowInfo(hwnd, info)) {
        m_targetWindowDetailPanel->showMessage(tr("대상 창 정보를 읽을 수 없습니다."));
        return;
    }

    const QString title = QString::fromStdWString(info.title);
    const QString className = QString::fromStdWString(info.className);
    QString processPath = QString::fromStdWString(info.processPath);
    const int slash = processPath.lastIndexOf(QLatin1Char('\\'));
    const QString processName =
        slash >= 0 ? processPath.mid(slash + 1)
                   : (processPath.isEmpty() ? tr("알 수 없음") : processPath);

    QString stateText;
    if (info.minimized) {
        stateText = tr("● 최소화");
    } else if (info.visible) {
        stateText = tr("● 표시");
    } else {
        stateText = tr("● 숨김");
    }

    QString monitorText;
    if (info.monitorNumber > 0 && info.monitorWidth > 0 && info.monitorHeight > 0) {
        if (info.monitorDpi > 0) {
            const int scalePercent = qRound(info.monitorDpi * 100.0 / 96.0);
            monitorText = tr("%1번 · %2×%3 · %4%")
                              .arg(info.monitorNumber)
                              .arg(info.monitorWidth)
                              .arg(info.monitorHeight)
                              .arg(scalePercent);
        } else {
            monitorText = tr("%1번 · %2×%3")
                              .arg(info.monitorNumber)
                              .arg(info.monitorWidth)
                              .arg(info.monitorHeight);
        }
    } else {
        monitorText = tr("알 수 없음");
    }

    TargetWindowDetailData data;
    data.hwnd = QStringLiteral("0x%1").arg(info.hwndValue, 0, 16);
    data.title = title.isEmpty() ? tr("(제목 없음)") : title;
    data.className = className.isEmpty() ? tr("(없음)") : className;
    data.frameBounds =
        tr("%1×%2 @ (%3, %4)").arg(info.width).arg(info.height).arg(info.x).arg(info.y);
    data.clientSize = tr("%1×%2 px").arg(info.clientWidth).arg(info.clientHeight);
    data.monitor = monitorText;
    data.processName = processName;
    data.processPath = processPath;
    data.stateText = stateText;
    data.minimized = info.minimized;
    data.visible = info.visible;
    data.monitorDpi = info.monitorDpi;
    m_targetWindowDetailPanel->showDetails(data);
#else
    m_targetWindowDetailPanel->showMessage(tr("대상 창 정보는 Windows에서만 표시됩니다."));
#endif
}

void MainWindow::syncUserInputInterruptForSession(FeatureRunSession& session, Feature* feature) {
    UserInputInterruptMonitor& monitor = UserInputInterruptMonitor::instance();
    monitor.unregisterSession(session.featureId);
    if (!feature || !session.sessionContext) {
        return;
    }
    monitor.registerSession(session.featureId,
                            feature->userInputInterruptMode(),
                            feature->hotkey(),
                            session.sessionContext.get());
}

void MainWindow::onUserInputInterrupt(const std::string& featureId) {
    FeatureRunSession* session = sessionFor(featureId);
    Feature* feature = m_project ? m_project->featureById(featureId) : nullptr;
    if (!session || !feature || !session->sessionContext) {
        return;
    }

    const UserInputInterruptMode mode = feature->userInputInterruptMode();
    if (mode == UserInputInterruptMode::Stop) {
        session->userStopRequested = true;
        session->repeatSession = false;
        session->holdRunActive = false;
        session->sessionContext->requestStop();
        if (session->engine) {
            session->engine->stop();
        }
        appendSessionLog(*session, tr("사용자 입력으로 완전 정지"));
        showTransientStatus(
            tr("[%1] 사용자 입력 — 정지").arg(featureDisplayName(featureId)), 3000);
        return;
    }

    if (mode == UserInputInterruptMode::Pause) {
        session->sessionContext->togglePaused();
        const bool paused = session->sessionContext->isPaused();
        appendSessionLog(*session, paused ? tr("사용자 입력으로 일시정지") : tr("사용자 입력으로 재개"));
        updateRunUiState();
    }
}

void MainWindow::scheduleRunWarmup() {
    QTimer::singleShot(0, this, [this]() {
        if (!m_project) {
            return;
        }
        RunWarmup::prefetch(m_project.get(),
                            Application::instance()->projectDirectory().toStdString());
    });
}
