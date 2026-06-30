#include "app/MainWindow.h"

#include "app/Application.h"
#include "app/ProgramSettings.h"
#include "app/UserInputInterruptMonitor.h"
#include "ui/ProgramSettingsDialog.h"
#include "model/UserInputInterruptMode.h"
#include "app/HotkeyManager.h"
#include "core/capture/ScreenCapture.h"
#include "core/capture/CursorPositionPicker.h"
#include "core/capture/WindowPicker.h"
#include "core/RunWarmup.h"
#include "core/workflow/ExecutionContext.h"
#include "core/workflow/Workflow.h"
#include "core/workflow/WorkflowEngine.h"
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
#include <QEventLoop>
#include <QFileDialog>
#include <QFileInfo>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
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
#include <QTimer>
#include <QVBoxLayout>

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
    setWindowTitle(QStringLiteral("SuckbongMachine %1").arg(QCoreApplication::applicationVersion()));
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
    m_pickWindowButton->setToolTip(tr("클릭한 뒤 대상 창을 눌러 지정합니다. Esc로 취소."));
    m_showTargetWindowButton = new QPushButton(tr("창 표시"), targetGroup);
    m_showTargetWindowButton->setToolTip(tr("지정된 대상 창 테두리를 잠시 깜빡여 표시합니다."));

    auto* buttonColumn = new QVBoxLayout();
    buttonColumn->setSpacing(4);
    buttonColumn->addWidget(m_pickWindowButton);
    buttonColumn->addWidget(m_showTargetWindowButton);

    auto* detailRow = new QHBoxLayout();
    detailRow->setSpacing(8);
    detailRow->addWidget(m_targetWindowDetailPanel, 1);
    detailRow->addLayout(buttonColumn, 0);

    targetLayout->addLayout(detailRow);

    m_exitButton = new QPushButton(tr("종료"), bottomPanel);
    m_settingsButton = new QPushButton(tr("설정"), bottomPanel);
    m_settingsButton->setToolTip(tr("프로그램 설정"));
    auto* exitRow = new QHBoxLayout;
    exitRow->addStretch();
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
    connect(m_alwaysOnTopCheck, &QCheckBox::toggled, this, &MainWindow::onAlwaysOnTopToggled);
    connect(m_pickWindowButton, &QPushButton::clicked, this, &MainWindow::onPickTargetWindow);
    connect(m_showTargetWindowButton, &QPushButton::clicked, this, &MainWindow::onShowTargetWindow);

    UserInputInterruptMonitor::instance().setHandler(
        [this](const std::string& featureId) { onUserInputInterrupt(featureId); });
}

void MainWindow::onExitRequested() {
    close();
}

void MainWindow::onCheckForUpdates() {
    if (hasAnyRunningSession()) {
        QMessageBox::warning(this,
                             tr("업데이트"),
                             tr("워크플로 실행 중에는 업데이트할 수 없습니다. 먼저 실행을 중지하세요."));
        return;
    }

    auto* checker = new UpdateChecker(this);
    connect(checker, &UpdateChecker::readyToRestartForUpdate, this, &MainWindow::prepareForShutdown);
    checker->checkForUpdates();
}

void MainWindow::onProgramSettings() {
    ProgramSettingsDialog dialog(this);
    dialog.exec();
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

void MainWindow::prepareForShutdown() {
    m_hotkeyManager->unregisterAll();
    UserInputInterruptMonitor::instance().unregisterAll();
    stopAllSessions();
    QCoreApplication::processEvents(QEventLoop::ExcludeUserInputEvents);
    ScreenRegionOverlay::dismissAll();
    MatchTestOverlay::dismissAll();
    RoiPreviewOverlay::dismissAll();
    WorkflowMatchFeedbackOverlay::dismissAll();
    WorkflowRoiFlashOverlay::dismissAll();
    TargetWindowHighlightOverlay::dismissAll();
    WindowPicker::cancelPick();
    CursorPositionPicker::cancelPick();
    m_autoSaveTimer->stop();
    saveSelectedFeaturePreference();
    autoSaveProject();
    if (m_uiState) {
        m_uiState->saveNow();
    }
}

void MainWindow::closeEvent(QCloseEvent* event) {
    if (hasAnyRunningSession()) {
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
        tr("SuckbongMachine 프로젝트 (*.json)"));
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
        tr("SuckbongMachine 프로젝트 (*.json)"));
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
        } else {
            m_workflowEditor->clearLoopTiming();
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
        for (const auto& entry : m_runSessions) {
            if (entry.second.sessionContext && entry.second.sessionContext->isPaused()) {
                anyPaused = true;
                break;
            }
        }
        if (anyPaused) {
            setPersistentStatus(tr("일시정지 — 입력하여 재개"));
        } else {
            setPersistentStatus(tr("실행 중 (%1)").arg(m_runSessions.size()));
        }
    } else {
        m_persistentStatusMessage.clear();
        if (m_transientStatusMessage.isEmpty()) {
            refreshTitleBarStatus();
        }
    }
}

void MainWindow::stopFeatureRun(const std::string& featureId) {
    FeatureRunSession* session = sessionFor(featureId);
    if (!session || !session->engine) {
        return;
    }

    session->userStopRequested = true;
    session->repeatSession = false;
    session->holdRunActive = false;
    session->engine->stop();
    UserInputInterruptMonitor::instance().unregisterSession(featureId);
    appendSessionLog(*session, tr("중지 요청됨."));
}

void MainWindow::stopAllSessions() {
    UserInputInterruptMonitor::instance().unregisterAll();
    for (auto& entry : m_runSessions) {
        if (entry.second.engine) {
            entry.second.userStopRequested = true;
            entry.second.repeatSession = false;
            entry.second.holdRunActive = false;
            entry.second.engine->stopAndWait();
        }
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
    if (isFeatureRunning(feature->id())) {
        return;
    }

    FeatureRunSession session;
    session.featureId = feature->id();
    session.engine = std::make_unique<WorkflowEngine>(this);
    session.userStopRequested = false;
    session.runningMode = normalizeRunMode(feature->runMode());
    session.hotkeyLaunchedSession = fromHotkey;
    session.repeatSession = session.runningMode == FeatureRunMode::RepeatInfinite
                            || session.runningMode == FeatureRunMode::RepeatCount
                            || session.runningMode == FeatureRunMode::Hold;
    session.repeatRemaining = feature->repeatCount();
    session.holdRunActive = session.runningMode == FeatureRunMode::Hold;
    if (session.runningMode == FeatureRunMode::Hold) {
        ++session.holdRepeatGeneration;
    }

    const std::string featureId = session.featureId;
    connectSessionEngine(session);
    m_runSessions.emplace(featureId, std::move(session));
    selectRunningFeatureForDisplay(feature);
    launchWorkflowRun(*sessionFor(featureId), feature, false);
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

    const bool infiniteStyle = session.runningMode == FeatureRunMode::RepeatInfinite
                             || session.runningMode == FeatureRunMode::Hold;
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
        if (isDisplayedRunningFeature(&session)) {
            syncLoopTimingToWorkflowEditor(&session);
        }

        if (shouldLogRunDetails(session)) {
            appendSessionLog(session, tr("기능 실행"));
        }
        updateRunUiState();
        session.runningBlockIndex = -1;
        session.runningBlockHighlight = BlockListWidget::ExecutionHighlight::None;
        if (isDisplayedRunningFeature(&session)) {
            m_workflowEditor->clearBlockMatchResults();
            m_workflowEditor->clearExecutionHighlight();
        }
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
    case FeatureRunMode::RepeatCount:
        return session.repeatSession && session.repeatRemaining > 0;
    case FeatureRunMode::Toggle:
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

void MainWindow::finishRunSession(const std::string& featureId, bool success, const QString& message) {
    FeatureRunSession* session = sessionFor(featureId);
    if (session && isDisplayedRunningFeature(session)) {
        m_workflowEditor->clearExecutionHighlight();
        m_workflowEditor->clearLoopTiming();
    }

    if (session) {
        showTransientStatus(tr("[%1] %2")
                                .arg(featureDisplayName(featureId), success ? tr("완료") : tr("실패")),
                            3000);
    }

    if (session && session->sessionContext) {
        session->sessionContext->endRunKeyboardSession();
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
    for (const auto& failure : failures) {
        appendLog(tr("단축키 등록 실패 (%1): 시스템에서 이미 사용 중일 수 있습니다.")
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
    setPersistentStatus(tr("대상 창을 클릭하세요. Esc로 취소"));
    WindowPicker::startPick(this, [this](const WindowPicker::Result& result) {
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
        scheduleRunWarmup();
    });
#else
    QMessageBox::information(this, tr("창 지정"), tr("창 지정은 Windows에서만 지원됩니다."));
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
    session->loopTimer.start();
    updateRunUiState();
}

void MainWindow::onEngineFinished(bool success, const QString& message) {
    FeatureRunSession* session = sessionForEngine(sender());
    if (!session) {
        return;
    }

    Feature* feature = m_project ? m_project->featureById(session->featureId) : nullptr;

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
    m_baseWindowTitle = QStringLiteral("SuckbongMachine %1").arg(QCoreApplication::applicationVersion());
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
    if (!feature || feature->userInputInterruptMode() == UserInputInterruptMode::None
        || !session.sessionContext) {
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
