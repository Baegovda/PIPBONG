#include "app/MainWindow.h"

#include "app/Application.h"
#include "app/FeatureHotkeyGate.h"
#include "app/PerfTrace.h"
#include "app/ProgramSettings.h"
#include "app/WindowsRunAsAdmin.h"
#include "app/MouseCenterLock.h"
#include "app/TargetWindowCenterPin.h"
#include "app/UserInputInterruptMonitor.h"
#include "ui/calculator/CalculatorDialog.h"
#include "ui/diagnostics/SpikeWatchDialog.h"
#include "ui/memo/MemoDialog.h"
#include "ui/ProgramSettingsDialog.h"
#include "model/UserInputInterruptMode.h"
#include "app/HotkeyManager.h"
#include "app/FeatureLibraryManager.h"
#include "core/capture/ScreenCapture.h"
#include "core/capture/CursorPositionPicker.h"
#include "core/capture/WindowPicker.h"
#include "core/input/InputSimulator.h"
#include "core/input/HotkeyBinding.h"
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
#include "storage/ProjectPackage.h"
#include "ui/FeatureListPanel.h"
#include "ui/BlockListWidget.h"
#include "ui/FeatureLibraryDialog.h"
#include "ui/WorkflowEditorPanel.h"
#include "ui/TargetWindowDetailPanel.h"
#include "ui/TargetWindowHighlightOverlay.h"
#include "app/UpdateChecker.h"
#include "ui/AppHelpDialog.h"
#include "ui/CustomTitleBar.h"
#include "ui/ProfileListWidget.h"
#include "ui/widgets/ReorderableListWidget.h"
#include "ui/FeatureDragMime.h"
#include "ui/LogPanelWidget.h"
#include "ui/UiResizeHandle.h"
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
#include <QAbstractItemView>
#include <QApplication>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDir>
#include <QEventLoop>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QMimeData>
#include <QFileIconProvider>
#include <QGroupBox>
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
#include <QSizePolicy>
#include <QSplitter>
#include <QStatusBar>
#include <QSystemTrayIcon>
#include <QMenu>
#include <QPointer>
#include <QShortcut>
#include <QStandardPaths>
#include <QTimer>
#include <QToolButton>
#include <QUuid>
#include <QVector>
#include <QVBoxLayout>

#include <optional>

#include <memory>

#include "model/FeatureRunMode.h"

#ifdef _WIN32
#include <windows.h>
#include <windowsx.h>
#include <dwmapi.h>

namespace {

constexpr UINT kForegroundProfileSyncMessage = WM_APP + 0x4A1;
HWND g_foregroundProfileSyncReceiver = nullptr;

void CALLBACK foregroundWindowEventProc(HWINEVENTHOOK,
                                        DWORD event,
                                        HWND,
                                        LONG,
                                        LONG,
                                        DWORD,
                                        DWORD) {
    if (event == EVENT_SYSTEM_FOREGROUND && g_foregroundProfileSyncReceiver
        && IsWindow(g_foregroundProfileSyncReceiver)) {
        PostMessageW(g_foregroundProfileSyncReceiver, kForegroundProfileSyncMessage, 0, 0);
    }
}

bool isShellTransientForegroundWindow(HWND hwnd) {
    if (!hwnd || !IsWindow(hwnd)) {
        return true;
    }
    wchar_t className[256]{};
    if (GetClassNameW(hwnd, className, 256) <= 0) {
        return false;
    }
    // Classic Alt+Tab switcher + common Win10/11 shell overlays during focus transit.
    static const wchar_t* kIgnoredClasses[] = {
        L"#32771",
        L"ForegroundStaging",
        L"MultitaskingViewFrame",
        L"XamlExplorerHostIslandWindow",
        L"Shell_TrayWnd",
        L"Shell_SecondaryTrayWnd",
        L"NotifyIconOverflowWindow",
        L"WorkerW",
        L"Progman",
    };
    for (const wchar_t* ignored : kIgnoredClasses) {
        if (_wcsicmp(className, ignored) == 0) {
            return true;
        }
    }
    return false;
}

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
    QString processPath;
    QString displayText;
    QIcon icon;
};

bool windowTitleMatchesSubTarget(const QString& windowTitle,
                                 const QString& mainBinding,
                                 const QString& subBinding) {
    if (subBinding.isEmpty() || windowTitle.isEmpty()) {
        return false;
    }
    const bool subHit = windowTitle.contains(subBinding, Qt::CaseInsensitive);
    const bool mainHit =
        !mainBinding.isEmpty() && windowTitle.contains(mainBinding, Qt::CaseInsensitive);
    return subHit && (!mainHit || subBinding.length() >= mainBinding.length());
}

bool windowTitleMatchesMainBinding(const QString& windowTitle, const QString& mainBinding) {
    return !mainBinding.isEmpty() && !windowTitle.isEmpty()
           && windowTitle.contains(mainBinding, Qt::CaseInsensitive);
}

void applyWindowListBindingMark(QListWidgetItem* item,
                                const QPalette& pal,
                                bool isMainBinding,
                                bool isSubBinding) {
    if (!item || (!isMainBinding && !isSubBinding)) {
        return;
    }

    const bool dark = isDarkTheme(pal);
    const QColor mainAccent = dark ? QColor(0x81, 0xc7, 0x84) : QColor(0x2f, 0x9e, 0x62);
    const QColor subAccent = dark ? QColor(0x64, 0xb5, 0xf6) : QColor(0x1e, 0x88, 0xe5);

    QColor tint = isMainBinding ? mainAccent : subAccent;
    if (isMainBinding && isSubBinding) {
        tint = QColor((mainAccent.red() + subAccent.red()) / 2,
                      (mainAccent.green() + subAccent.green()) / 2,
                      (mainAccent.blue() + subAccent.blue()) / 2);
    }
    tint.setAlpha(dark ? 48 : 40);

    QString badges;
    if (isMainBinding) {
        badges += QStringLiteral("● 주 대상");
    }
    if (isSubBinding) {
        if (!badges.isEmpty()) {
            badges += QStringLiteral("  ");
        }
        badges += QStringLiteral("● 서브 창");
    }

    item->setText(item->text() + QStringLiteral("    ") + badges);
    item->setToolTip(badges);
    item->setBackground(QBrush(tint));

    QFont font = item->font();
    font.setBold(true);
    item->setFont(font);

    const QColor accent =
        isMainBinding && isSubBinding ? primaryContentTextColor(pal)
        : isMainBinding                  ? mainAccent
                                         : subAccent;
    item->setForeground(QBrush(accent));
}

QString windowListBindingLegendHtml(const QPalette& pal) {
    const bool dark = isDarkTheme(pal);
    const QColor mainAccent = dark ? QColor(0x81, 0xc7, 0x84) : QColor(0x2f, 0x9e, 0x62);
    const QColor subAccent = dark ? QColor(0x64, 0xb5, 0xf6) : QColor(0x1e, 0x88, 0xe5);
    return QStringLiteral(
               "<span style='color:%1; font-weight:600'>● 주 대상</span>"
               "&nbsp;&nbsp;"
               "<span style='color:%2; font-weight:600'>● 서브 창</span>")
        .arg(mainAccent.name(), subAccent.name());
}

constexpr int kMinMainWorkspacePanePx = 252;
constexpr int kMinBottomPanelPx = 108;

void clampMainVerticalSplitterSizesImpl(QSplitter* splitter) {
    if (!splitter || splitter->count() < 2) {
        return;
    }

    QWidget* topPane = splitter->widget(0);
    QWidget* bottomPane = splitter->widget(1);
    if (!topPane || !bottomPane) {
        return;
    }

    const int handle = splitter->handleWidth();
    const int total = splitter->height();
    if (total <= handle) {
        return;
    }

    const int available = total - handle;
    const int minTop = qMax(kMinMainWorkspacePanePx, topPane->minimumHeight());
    const int minBottom = qMax(kMinBottomPanelPx, bottomPane->minimumHeight());
    if (available < minTop + minBottom) {
        return;
    }

    QList<int> sizes = splitter->sizes();
    int topSize = sizes.value(0, 0);
    int bottomSize = sizes.value(1, 0);
    if (topSize + bottomSize <= 0) {
        return;
    }

    topSize = qBound(minTop, topSize, available - minBottom);
    bottomSize = available - topSize;
    if (bottomSize < minBottom) {
        bottomSize = minBottom;
        topSize = available - bottomSize;
    }

    if (sizes.value(0) == topSize && sizes.value(1) == bottomSize) {
        return;
    }

    QSignalBlocker blocker(splitter);
    splitter->setSizes({topSize, bottomSize});
}

enum WindowListPickChoice {
    WindowListPickCancelled = 0,
    WindowListPickMain = QDialog::Accepted,
    WindowListPickSub = 2,
};

QIcon iconForProcessPath(const std::wstring& processPath) {
    if (processPath.empty()) {
        return {};
    }
    const QString path = QString::fromStdWString(processPath);
    if (!QFileInfo::exists(path)) {
        return {};
    }
    static QFileIconProvider iconProvider;
    const QIcon icon = iconProvider.icon(QFileInfo(path));
    return icon.isNull() ? QIcon{} : icon;
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

std::optional<MouseButton> mouseButtonFromVirtualKey(int virtualKey) {
    switch (virtualKey) {
    case VK_LBUTTON:
        return MouseButton::Left;
    case VK_RBUTTON:
        return MouseButton::Right;
    case VK_MBUTTON:
        return MouseButton::Middle;
    case VK_XBUTTON1:
        return MouseButton::Back;
    case VK_XBUTTON2:
        return MouseButton::Forward;
    default:
        return std::nullopt;
    }
}

bool shouldDeliverHoldHotkeyRelease(const FeatureRunSession& session, const Feature* feature) {
    if (!feature || feature->hotkey().isEmpty()) {
        return false;
    }
    const int vk = feature->hotkey().virtualKey;
    if (!session.sessionContext) {
        return false;
    }
    if (HotkeyBinding::isMouseVirtualKey(vk)) {
        const std::optional<MouseButton> button = mouseButtonFromVirtualKey(vk);
        return button && session.sessionContext->pipbongEverInjectedMouseButton(*button);
    }
    return session.sessionContext->pipbongEverInjectedVirtualKey(vk);
}

void releaseHoldHotkeyInputToTarget(FeatureRunSession& session, const Feature* feature) {
    if (session.holdHotkeyReleasedToTarget || !feature || feature->hotkey().isEmpty()) {
        return;
    }
    if (!shouldDeliverHoldHotkeyRelease(session, feature)) {
        session.holdHotkeyReleasedToTarget = true;
        return;
    }
#ifdef _WIN32
    HWND hwnd = ScreenCapture::findTargetWindow();
    InputSimulator::releaseHoldHotkeyToTarget(hwnd, feature->hotkey().virtualKey);
#else
    const int vk = feature->hotkey().virtualKey;
    if (HotkeyBinding::isMouseVirtualKey(vk)) {
        InputSimulator::forceHotkeyMouseButtonUp(vk);
    } else {
        InputSimulator::forceKeyUp(vk);
    }
#endif
    session.holdHotkeyReleasedToTarget = true;
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
            entry.processPath = QString::fromStdWString(processPath);
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

namespace {

void configureTargetWindowActionButton(QToolButton* button) {
    if (!button) {
        return;
    }
    button->setProperty("class", QStringLiteral("targetWindowActionButton"));
    button->setCursor(Qt::PointingHandCursor);
    button->setFocusPolicy(Qt::NoFocus);
    button->setAutoRaise(false);
    button->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Fixed);
    button->setFixedHeight(22);
}

int featureIndexById(const Project& project, const std::string& featureId) {
    const auto& features = project.features();
    for (int i = 0; i < static_cast<int>(features.size()); ++i) {
        if (features[static_cast<size_t>(i)]->id() == featureId) {
            return i;
        }
    }
    return -1;
}

QString profileDisplayName(ProfileManager* profileManager, const QString& profileId) {
    if (!profileManager) {
        return profileId;
    }
    for (const ProfileManager::Profile& profile : profileManager->profiles()) {
        if (profile.id == profileId) {
            return profile.name;
        }
    }
    return profileId;
}

QString libraryEntryDisplayName(FeatureLibraryManager* libraryManager, const QString& entryId) {
    if (!libraryManager) {
        return entryId;
    }
    for (const FeatureLibraryManager::Entry& entry : libraryManager->listEntries()) {
        if (entry.id == entryId) {
            return entry.name;
        }
    }
    return entryId;
}

} // namespace

MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent)
    , m_project(std::make_unique<Project>())
    , m_profileManager(std::make_unique<ProfileManager>(Application::dataDirectory()))
    , m_hotkeyManager(new HotkeyManager(this)) {
    m_autoSaveTimer = new QTimer(this);
    m_autoSaveTimer->setSingleShot(true);
    connect(m_autoSaveTimer, &QTimer::timeout, this, [this]() { autoSaveProject(); });

    m_profilePackageSealTimer = new QTimer(this);
    m_profilePackageSealTimer->setSingleShot(true);
    m_profilePackageSealTimer->setInterval(5000);
    connect(m_profilePackageSealTimer, &QTimer::timeout, this, &MainWindow::sealActiveProfilePackage);

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

    m_targetWindowCenterPinTimer = new QTimer(this);
    m_targetWindowCenterPinTimer->setInterval(200);
    connect(m_targetWindowCenterPinTimer, &QTimer::timeout, this, &MainWindow::syncTargetWindowCenterPin);

    m_profileAutoSwitchTimer = new QTimer(this);
    m_profileAutoSwitchTimer->setTimerType(Qt::PreciseTimer);
    m_profileAutoSwitchTimer->setInterval(50);
    connect(m_profileAutoSwitchTimer, &QTimer::timeout, this, &MainWindow::syncProfileToForegroundWindow);
    m_profileAutoSwitchTimer->start();
    if (ProgramSettings::pinTargetWindowToScreenCenter()) {
        applyTargetWindowCenterPin(true);
    }

    refreshUpdateButtonState();
    ProgramSettings::syncWindowsStartupRegistration();
    ProgramSettings::syncWindowsRunAsAdminRegistration();
    setupTrayIcon();

    m_profileManager->initialize();
    m_featureLibraryManager = std::make_unique<FeatureLibraryManager>(Application::dataDirectory());
    ProgramSettings::applyProfileSettings(
        m_profileManager->loadSettings(m_profileManager->activeProfileId()));
    refreshProfileList();
    loadActiveProfile();
    restoreMemoDialogOpenState();
    refreshFeatureLibraryPanel();
    scheduleProfilePackageSeal();

#ifdef _WIN32
    g_foregroundProfileSyncReceiver = reinterpret_cast<HWND>(winId());
    m_profileForegroundEventHook =
        SetWinEventHook(EVENT_SYSTEM_FOREGROUND,
                        EVENT_SYSTEM_FOREGROUND,
                        nullptr,
                        foregroundWindowEventProc,
                        0,
                        0,
                        WINEVENT_OUTOFCONTEXT | WINEVENT_SKIPOWNPROCESS);
#endif

    updateWindowTitle();
    syncHotkeys();
    updateTargetWindowDetails();
    scheduleRunWarmup();
}

MainWindow::~MainWindow() {
#ifdef _WIN32
    if (m_profileForegroundEventHook) {
        UnhookWinEvent(static_cast<HWINEVENTHOOK>(m_profileForegroundEventHook));
        m_profileForegroundEventHook = nullptr;
    }
    if (g_foregroundProfileSyncReceiver == reinterpret_cast<HWND>(winId())) {
        g_foregroundProfileSyncReceiver = nullptr;
    }
#endif
    stopAllSessions();
    clearGlobalUiUndoHistory();
    clearGlobalUiRedoHistory();
}

void MainWindow::ensureInitialWindowPlacement() {
    if (!internalWinId()) {
        winId();
    }
    if (m_uiState) {
        m_uiState->restoreMainWindowGeometry();
    }
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
    UiResizeHandle::configureSplitter(m_mainHorizontalSplitter);
    auto* profileGroup = new QGroupBox(tr("프로필"), m_mainHorizontalSplitter);
    profileGroup->setObjectName(QStringLiteral("profileListGroup"));
    profileGroup->setStyleSheet(QStringLiteral(
        "QGroupBox#profileListGroup {"
        "  border: none;"
        "  margin-top: 10px;"
        "  padding-top: 8px;"
        "}"
        "QGroupBox#profileListGroup::title {"
        "  subcontrol-origin: margin;"
        "  subcontrol-position: top left;"
        "  left: 4px;"
        "  padding: 0 4px;"
        "}"));
    auto* profileLayout = new QVBoxLayout(profileGroup);
    profileLayout->setContentsMargins(6, 4, 6, 6);
    profileLayout->setSpacing(6);
    m_profilePanel = profileGroup;
    m_profilePanel->setMinimumWidth(92);
    m_profileList = new ProfileListWidget(profileGroup);
    m_profileList->setObjectName(QStringLiteral("profileListView"));
    m_profileList->setSelectionMode(QAbstractItemView::SingleSelection);
    m_profileList->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_profileList->setIconSize(QSize(20, 20));
    m_profileList->setMinimumWidth(0);
    m_profileList->setFrameShape(QFrame::NoFrame);
    m_profileList->setStyleSheet(QStringLiteral(
        "QListWidget#profileListView {"
        "  border: none;"
        "  background: transparent;"
        "  outline: none;"
        "}"
        "QListWidget#profileListView::item {"
        "  background: transparent;"
        "  border: none;"
        "  padding: 0;"
        "}"
        "QListWidget#profileListView::item:selected {"
        "  background: transparent;"
        "}"));
    m_profileList->setToolTip(tr("활성 프로필의 기능과 단축키만 동작합니다. 포커스된 창에 맞춰 프로필이 자동으로 전환됩니다."));

    auto* profileButtonColumn = new QVBoxLayout();
    profileButtonColumn->setSpacing(4);
    m_addProfileButton = new QPushButton(tr("추가"), profileGroup);
    m_renameProfileButton = new QPushButton(tr("편집"), profileGroup);
    m_deleteProfileButton = new QPushButton(tr("삭제"), profileGroup);
    m_addProfileButton->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Fixed);
    m_renameProfileButton->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Fixed);
    m_deleteProfileButton->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Fixed);
    profileButtonColumn->addWidget(m_addProfileButton);
    profileButtonColumn->addWidget(m_renameProfileButton);
    profileButtonColumn->addWidget(m_deleteProfileButton);
    profileLayout->addWidget(m_profileList, 1);
    profileLayout->addLayout(profileButtonColumn);

    m_featureList = new FeatureListPanel(m_mainHorizontalSplitter);
    m_workflowEditor = new WorkflowEditorPanel(m_mainHorizontalSplitter);
    m_mainHorizontalSplitter->addWidget(m_profilePanel);
    m_mainHorizontalSplitter->addWidget(m_featureList);
    m_mainHorizontalSplitter->addWidget(m_workflowEditor);
    m_mainHorizontalSplitter->setStretchFactor(0, 0);
    m_mainHorizontalSplitter->setStretchFactor(1, 0);
    m_mainHorizontalSplitter->setStretchFactor(2, 1);
    m_mainHorizontalSplitter->setCollapsible(0, false);
    m_mainHorizontalSplitter->setCollapsible(1, false);
    m_mainHorizontalSplitter->setCollapsible(2, false);
    m_mainHorizontalSplitter->setSizes({130, 210, 940});

    auto* bottomPanel = new QWidget(contentHost);
    bottomPanel->setMinimumHeight(kMinBottomPanelPx);
    auto* bottomLayout = new QVBoxLayout(bottomPanel);
    bottomLayout->setContentsMargins(8, 6, 8, 8);
    bottomLayout->setSpacing(6);

    auto* targetGroup = new QGroupBox(tr("대상 창"), bottomPanel);
    targetGroup->setMinimumHeight(72);
    auto* targetLayout = new QVBoxLayout(targetGroup);
    targetLayout->setContentsMargins(8, 4, 8, 6);
    targetLayout->setSpacing(4);

    m_targetWindowDetailPanel = new TargetWindowDetailPanel(targetGroup);

    auto* actionBar = new QWidget(targetGroup);
    actionBar->setObjectName(QStringLiteral("targetWindowActionBar"));
    auto* actionLayout = new QHBoxLayout(actionBar);
    actionLayout->setContentsMargins(0, 0, 0, 0);
    actionLayout->setSpacing(3);

    m_pickWindowButton = new QToolButton(actionBar);
    m_pickWindowButton->setText(tr("지정"));
    m_pickWindowButton->setToolTip(tr("클릭한 뒤 대상 창을 눌러 지정합니다. 우클릭 또는 Esc로 취소."));
    configureTargetWindowActionButton(m_pickWindowButton);

    m_pickWindowListButton = new QToolButton(actionBar);
    m_pickWindowListButton->setText(tr("목록"));
    m_pickWindowListButton->setToolTip(tr("표시 중인 창 목록에서 주 대상 창을 선택하거나 서브 대상 창으로 지정합니다."));
    configureTargetWindowActionButton(m_pickWindowListButton);

    m_showTargetWindowButton = new QToolButton(actionBar);
    m_showTargetWindowButton->setText(tr("표시"));
    m_showTargetWindowButton->setToolTip(tr("지정된 대상 창 테두리를 잠시 깜빡여 표시합니다."));
    configureTargetWindowActionButton(m_showTargetWindowButton);

    m_pinTargetWindowCenterButton = new QToolButton(actionBar);
    m_pinTargetWindowCenterButton->setObjectName(QStringLiteral("targetPinCenterButton"));
    m_pinTargetWindowCenterButton->setText(tr("중앙 고정"));
    m_pinTargetWindowCenterButton->setToolTip(
        tr("지정된 대상 창이 현재 모니터 중앙에 유지되도록 위치를 자동으로 맞춥니다."));
    m_pinTargetWindowCenterButton->setCheckable(true);
    m_pinTargetWindowCenterButton->setChecked(ProgramSettings::pinTargetWindowToScreenCenter());
    configureTargetWindowActionButton(m_pinTargetWindowCenterButton);
#ifndef _WIN32
    m_pinTargetWindowCenterButton->setEnabled(false);
    m_pinTargetWindowCenterButton->setToolTip(tr("화면 중앙 고정은 Windows에서만 지원됩니다."));
#endif

    actionLayout->addWidget(m_pickWindowButton);
    actionLayout->addWidget(m_pickWindowListButton);
    actionLayout->addWidget(m_showTargetWindowButton);
    actionLayout->addStretch();
    actionLayout->addWidget(m_pinTargetWindowCenterButton);

    targetGroup->setStyleSheet(QStringLiteral(
        "QGroupBox {"
        "  border: 1px solid palette(mid);"
        "  border-radius: 8px;"
        "  margin-top: 8px;"
        "  padding-top: 8px;"
        "  font-size: 11px;"
        "}"
        "QGroupBox::title {"
        "  subcontrol-origin: margin;"
        "  left: 8px;"
        "  padding: 0 3px;"
        "}"
        "QToolButton.targetWindowActionButton {"
        "  padding: 1px 8px;"
        "  border: 1px solid palette(mid);"
        "  border-radius: 4px;"
        "  background-color: palette(button);"
        "  color: palette(button-text);"
        "  font-size: 11px;"
        "}"
        "QToolButton.targetWindowActionButton:hover:!checked {"
        "  background-color: palette(light);"
        "  border-color: palette(dark);"
        "}"
        "QToolButton.targetWindowActionButton:pressed:!checked {"
        "  background-color: palette(midlight);"
        "}"
        "QToolButton.targetWindowActionButton:checked {"
        "  background-color: palette(highlight);"
        "  color: palette(highlighted-text);"
        "  border-color: palette(highlight);"
        "}"
        "QToolButton.targetWindowActionButton:disabled {"
        "  color: palette(mid);"
        "  background-color: palette(button);"
        "  border-color: palette(mid);"
        "}"));

    targetLayout->addWidget(actionBar);
    targetLayout->addWidget(m_targetWindowDetailPanel, 1);

    m_exitButton = new QPushButton(tr("종료"), bottomPanel);
    m_updateButton = new QPushButton(bottomPanel);
    m_updateButton->setToolTip(tr("GitHub 릴리즈에서 업데이트를 확인합니다."));
    m_calculatorButton = new QPushButton(tr("계산기"), bottomPanel);
    m_calculatorButton->setToolTip(tr("poe.ninja 시세 계산기"));
    m_spikeWatchButton = new QPushButton(tr("CPU 감시"), bottomPanel);
    m_spikeWatchButton->setToolTip(tr("CPU 사용률 스파이크 진단"));
    m_memoButton = new QPushButton(tr("메모장"), bottomPanel);
    m_memoButton->setToolTip(tr("프로필별 메모"));
    m_settingsButton = new QPushButton(tr("설정"), bottomPanel);
    m_settingsButton->setToolTip(tr("프로그램 설정"));
    auto* exitRow = new QHBoxLayout;
    exitRow->addWidget(m_updateButton);
    exitRow->addStretch();
    exitRow->addWidget(m_memoButton);
    exitRow->addWidget(m_spikeWatchButton);
    exitRow->addWidget(m_calculatorButton);
    exitRow->addWidget(m_settingsButton);
    exitRow->addWidget(m_exitButton);

    m_logPanel = new LogPanelWidget(bottomPanel);
    m_logPanel->setMinimumHeight(48);
    m_logPanel->setMaxLines(ProgramSettings::logMaxLines());

    m_bottomHorizontalSplitter = new QSplitter(Qt::Horizontal, bottomPanel);
    UiResizeHandle::configureSplitter(m_bottomHorizontalSplitter);
    m_bottomHorizontalSplitter->addWidget(m_logPanel);
    m_bottomHorizontalSplitter->addWidget(targetGroup);
    m_bottomHorizontalSplitter->setStretchFactor(0, 1);
    m_bottomHorizontalSplitter->setStretchFactor(1, 1);
    m_bottomHorizontalSplitter->setCollapsible(0, false);
    m_bottomHorizontalSplitter->setCollapsible(1, false);
    m_bottomHorizontalSplitter->setSizes({520, 420});

    bottomLayout->addWidget(m_bottomHorizontalSplitter, 1);
    bottomLayout->addLayout(exitRow);

    m_mainVerticalSplitter = new QSplitter(Qt::Vertical, contentHost);
    UiResizeHandle::configureSplitter(m_mainVerticalSplitter);
    m_mainVerticalSplitter->addWidget(m_mainHorizontalSplitter);
    m_mainVerticalSplitter->addWidget(bottomPanel);
    m_mainVerticalSplitter->setStretchFactor(0, 1);
    m_mainVerticalSplitter->setStretchFactor(1, 0);
    m_mainVerticalSplitter->setCollapsible(0, false);
    m_mainVerticalSplitter->setCollapsible(1, false);
    m_mainVerticalSplitter->setSizes({640, 240});
    connect(m_mainVerticalSplitter, &QSplitter::splitterMoved, this, [this](int, int) {
        clampMainVerticalSplitterSizes();
    });

    contentLayout->addWidget(m_mainVerticalSplitter, 1);
    outerLayout->addWidget(contentHost, 1);

    setCentralWidget(central);

    statusBar()->hide();
}

void MainWindow::setupUiState() {
    m_uiState = new UiStateManager(this);
    m_uiState->registerMainWindow(this);
    m_uiState->registerSplitter(m_mainHorizontalSplitter, QStringLiteral("main/horizontalProfiles"));
    m_uiState->registerSplitter(m_mainVerticalSplitter, QStringLiteral("main/vertical"));
    m_uiState->registerSplitter(m_bottomHorizontalSplitter, QStringLiteral("main/bottomHorizontal"));
    m_uiState->registerSplitter(m_featureList->featureLibrarySplitter(),
                                QStringLiteral("featureList/libraryVertical_v1"));
    m_uiState->registerSplitter(m_workflowEditor->workflowSplitter(), QStringLiteral("workflowEditor/vertical_v2"));
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
    m_uiState->registerSettingsHooks(
        QStringLiteral("workflowBlockList/columns"),
        [this](QSettings& settings) {
            if (m_workflowEditor && m_workflowEditor->blockList()) {
                m_workflowEditor->blockList()->saveColumnLayout(
                    settings,
                    UiStateManager::settingsKey(QStringLiteral("workflowBlockList/columns")));
            }
        },
        [this](const QSettings& settings) {
            if (m_workflowEditor && m_workflowEditor->blockList()) {
                m_workflowEditor->blockList()->restoreColumnLayout(
                    settings,
                    UiStateManager::settingsKey(QStringLiteral("workflowBlockList/columns")));
            }
        });
    m_uiState->registerSettingsHooks(
        QStringLiteral("workflowBlockList/rowHeight"),
        [this](QSettings& settings) {
            if (m_workflowEditor && m_workflowEditor->blockList()) {
                m_workflowEditor->blockList()->saveColumnLayout(
                    settings,
                    UiStateManager::settingsKey(QStringLiteral("workflowBlockList/columns")));
            }
        },
        [this](const QSettings& settings) {
            if (m_workflowEditor && m_workflowEditor->blockList()) {
                const QString columnsKey =
                    UiStateManager::settingsKey(QStringLiteral("workflowBlockList/columns"));
                if (!settings.contains(columnsKey + QStringLiteral("/rowHeight"))) {
                    m_workflowEditor->blockList()->restoreRowHeight(
                        settings,
                        UiStateManager::settingsKey(QStringLiteral("workflowBlockList/rowHeight")));
                }
            }
        });
    connect(m_featureList,
            &FeatureListPanel::columnLayoutChanged,
            m_uiState,
            &UiStateManager::scheduleSave);
    if (m_workflowEditor && m_workflowEditor->blockList()) {
        connect(m_workflowEditor->blockList(),
                &BlockListWidget::columnLayoutChanged,
                m_uiState,
                &UiStateManager::scheduleSave);
        connect(m_workflowEditor->blockList(),
                &BlockListWidget::rowHeightChanged,
                m_uiState,
                &UiStateManager::scheduleSave);
    }
    m_uiState->restoreAll();
    m_workflowEditor->clampWorkflowSplitterSizes();
    if (m_featureList) {
        m_featureList->clampFeatureLibrarySplitterSizes();
    }
    clampMainVerticalSplitterSizes();
    restoreAlwaysOnTopPreference();
}

void MainWindow::clampMainVerticalSplitterSizes() {
    clampMainVerticalSplitterSizesImpl(m_mainVerticalSplitter);
}

void MainWindow::setupMenus() {
    QMenuBar* bar = m_titleBar ? m_titleBar->menuBar() : menuBar();
    auto* fileMenu = bar->addMenu(tr("파일(&F)"));
    fileMenu->addAction(tr("새 프로젝트(&N)"), this, &MainWindow::onNewProject);
    fileMenu->addAction(tr("프로젝트 열기(&O)..."), this, &MainWindow::onOpenProject);
    fileMenu->addAction(tr("프로젝트 저장(&S)"), this, &MainWindow::onSaveProject);
    fileMenu->addAction(tr("다른 이름으로 저장(&A)..."), this, &MainWindow::onSaveProjectAs);
    fileMenu->addSeparator();
    fileMenu->addAction(tr("PIPBONG 패키지 가져오기(&I)..."), this, &MainWindow::onImportProfilePackage);
    fileMenu->addAction(tr("PIPBONG 패키지 내보내기(&E)..."), this, &MainWindow::onExportProfilePackage);
    fileMenu->addSeparator();
    fileMenu->addAction(tr("업데이트(&U)..."), this, &MainWindow::onCheckForUpdates);
    fileMenu->addSeparator();
    fileMenu->addAction(tr("종료(&X)"), this, &MainWindow::onExitRequested);

    auto* helpMenu = bar->addMenu(tr("도움말(&H)"));
    helpMenu->addAction(tr("PIPBONG 도움말(&H)..."), this, [this]() { AppHelpDialog::showHelp(this); });
    helpMenu->addSeparator();
    helpMenu->addAction(tr("PIPBONG 정보(&A)..."), this, [this]() { AppHelpDialog::showAbout(this); });
}

void MainWindow::connectSignals() {
    m_featureList->setProject(m_project.get());
    connect(m_featureList, &FeatureListPanel::selectionChanged, this, &MainWindow::onFeatureSelectionChanged);
    connect(m_featureList,
            &FeatureListPanel::mutationAboutToCommit,
            this,
            [this](const QString& reason) { pushGlobalUiUndoSnapshot(reason); });
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
                pushGlobalUiUndoSnapshot(QStringLiteral("profile-reorder"));
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
            Qt::DirectConnection);
    connect(m_hotkeyManager,
            &HotkeyManager::hotkeyHoldEnded,
            this,
            &MainWindow::onHotkeyHoldEnded,
            Qt::DirectConnection);

    connect(m_workflowEditor, &WorkflowEditorPanel::workflowModified, this, &MainWindow::onProjectModified);

    connect(m_exitButton, &QPushButton::clicked, this, &MainWindow::onExitRequested);
    connect(m_settingsButton, &QPushButton::clicked, this, &MainWindow::onProgramSettings);
    connect(m_updateButton, &QPushButton::clicked, this, &MainWindow::onUpdateButtonClicked);
    connect(m_calculatorButton, &QPushButton::clicked, this, &MainWindow::onCalculator);
    connect(m_spikeWatchButton, &QPushButton::clicked, this, &MainWindow::onSpikeWatch);
    connect(m_memoButton, &QPushButton::clicked, this, &MainWindow::onMemo);
    connect(m_alwaysOnTopCheck, &QCheckBox::toggled, this, &MainWindow::onAlwaysOnTopToggled);
    connect(m_pickWindowButton, &QToolButton::clicked, this, &MainWindow::onPickTargetWindow);
    connect(m_pickWindowListButton, &QToolButton::clicked, this, &MainWindow::onPickTargetWindowFromList);
    connect(m_showTargetWindowButton, &QToolButton::clicked, this, &MainWindow::onShowTargetWindow);
    connect(m_pinTargetWindowCenterButton,
            &QToolButton::toggled,
            this,
            &MainWindow::onPinTargetWindowCenterToggled);

    UserInputInterruptMonitor::instance().setHandler(
        [this](const std::string& featureId) { onUserInputInterrupt(featureId); });
    UserInputInterruptMonitor::instance().setHotkeyExemptionCheck(
        [this](int virtualKey) {
            return m_hotkeyManager && m_hotkeyManager->matchesAnyRegisteredFeatureHotkey(virtualKey);
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
    if (m_spikeWatchDialog) {
        m_spikeWatchDialog->hide();
    }
    if (m_uiState) {
        m_uiState->saveNow();
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

bool MainWindow::isEarlyLoopMouseLockWindow(const FeatureRunSession& session) const {
    if (session.lockMouseDuringFirstLoopCount <= 0 || session.earlyLoopMouseLockReleased) {
        return false;
    }
    const int loopNumber = session.sessionContext ? session.sessionContext->runLoopNumber()
                                                  : session.sessionIteration + 1;
    return loopNumber <= session.lockMouseDuringFirstLoopCount;
}

bool MainWindow::engageEarlyLoopMouseLockAtBestPoint(FeatureRunSession& session) {
#ifdef _WIN32
    int screenX = 0;
    int screenY = 0;
    bool havePoint = false;

    if (session.sessionContext && session.sessionContext->hasLastMatchScreenPoint()) {
        const cv::Point matchPoint = session.sessionContext->lastMatchScreenPoint();
        screenX = matchPoint.x;
        screenY = matchPoint.y;
        havePoint = true;
    } else if (session.hasRunStartCursorPosition) {
        screenX = session.runStartCursorScreenX;
        screenY = session.runStartCursorScreenY;
        havePoint = true;
    } else if (InputSimulator::getCursorScreenPosition(screenX, screenY)) {
        havePoint = true;
    }

    if (!havePoint) {
        return false;
    }

    if (!session.earlyLoopMouseLockEngaged) {
        MouseCenterLock::engageAt(screenX, screenY);
        session.earlyLoopMouseLockEngaged = true;
    } else {
        MouseCenterLock::updateFixedLockPoint(screenX, screenY);
    }
    return true;
#else
    (void)session;
    return false;
#endif
}

void MainWindow::updateEarlyLoopMouseLockFromMatch(FeatureRunSession& session) {
    if (!isEarlyLoopMouseLockWindow(session)) {
        return;
    }
    engageEarlyLoopMouseLockAtBestPoint(session);
}

void MainWindow::releaseEarlyLoopMouseLockIfEngaged(FeatureRunSession& session) {
#ifdef _WIN32
    if (!session.earlyLoopMouseLockEngaged) {
        return;
    }
    MouseCenterLock::release();
    session.earlyLoopMouseLockEngaged = false;
#else
    (void)session;
#endif
}

void MainWindow::syncEarlyLoopMouseLock(FeatureRunSession& session) {
#ifdef _WIN32
    if (isEarlyLoopMouseLockWindow(session)) {
        engageEarlyLoopMouseLockAtBestPoint(session);
        return;
    }

    releaseEarlyLoopMouseLockIfEngaged(session);
    if (hasFeatureMouseLock(session) && !MouseCenterLock::isActive()) {
        engageFeatureMouseLock(session);
    }
#else
    (void)session;
#endif
}

void MainWindow::handleEarlyLoopMouseLockBlockFailure(FeatureRunSession& session, int blockIndex) {
    Q_UNUSED(blockIndex);
    if (!isEarlyLoopMouseLockWindow(session)) {
        return;
    }

    ++session.earlyLoopMouseLockFailureCount;
    if (session.earlyLoopMouseLockFailureCount < session.unlockMouseOnBlockFailureCount) {
        return;
    }

    session.earlyLoopMouseLockReleased = true;
    releaseEarlyLoopMouseLockIfEngaged(session);
    if (shouldLogRunDetails(session)) {
        appendSessionLog(session,
                         tr("초기 루프 마우스 잠금 해제: 블록 실패 %1회")
                             .arg(session.earlyLoopMouseLockFailureCount),
                         LogLineKind::Warning);
    }
    if (hasFeatureMouseLock(session)) {
        engageFeatureMouseLock(session);
    }
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

void MainWindow::reconcileMouseLocksFromRunningSessions() {
#ifdef _WIN32
    if (MouseCenterLock::isActive()) {
        MouseCenterLock::releaseAll();
    }
    for (const auto& entry : m_runSessions) {
        const FeatureRunSession& session = entry.second;
        if (!isFeatureSessionActive(session) || !hasFeatureMouseLock(session)) {
            continue;
        }
        if (session.sessionContext && session.sessionContext->isPaused()) {
            continue;
        }
        FeatureRunSession& mutableSession = m_runSessions.at(entry.first);
        if (isEarlyLoopMouseLockWindow(mutableSession)) {
            engageEarlyLoopMouseLockAtBestPoint(mutableSession);
            continue;
        }
        if (mutableSession.lockMouseToCurrentPositionDuringRun) {
            if (!mutableSession.hasMouseLockPosition) {
                captureFeatureMouseLockPosition(mutableSession);
            }
            if (!mutableSession.hasMouseLockPosition) {
                continue;
            }
            if (mutableSession.mouseLockAnchoredToTargetWindow) {
                MouseCenterLock::engageAtTargetWindowOffset(mutableSession.mouseLockWindowOffsetX,
                                                            mutableSession.mouseLockWindowOffsetY);
            } else {
                MouseCenterLock::engageAt(mutableSession.mouseLockScreenX, mutableSession.mouseLockScreenY);
            }
        } else if (mutableSession.lockMouseToScreenCenterDuringRun) {
            MouseCenterLock::engageTargetWindowCenter();
        }
    }
    if (MouseCenterLock::isAnchoredToTargetWindow()) {
        scheduleMouseLockPositionSync();
    }
#endif
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
    FeatureHotkeyGateScope hotkeyGate;
    ProgramSettingsDialog dialog(this);
    if (dialog.exec() != QDialog::Accepted) {
        return;
    }
    saveActiveProfileSettings();
    applyCloseToTrayPolicy();
    applyUpdateCheckInterval();
    if (m_logPanel) {
        m_logPanel->setMaxLines(ProgramSettings::logMaxLines());
    }
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

void MainWindow::onSpikeWatch() {
    if (!m_spikeWatchDialog) {
        m_spikeWatchDialog = new SpikeWatchDialog(this);
        m_spikeWatchDialog->setAttribute(Qt::WA_DeleteOnClose);
        connect(m_spikeWatchDialog, &QObject::destroyed, this, [this]() { m_spikeWatchDialog = nullptr; });
        m_spikeWatchDialog->setFeatureRunningCallback([this]() { return !m_runSessions.empty(); });
    } else {
        m_spikeWatchDialog->setFeatureRunningCallback([this]() { return !m_runSessions.empty(); });
    }
    m_spikeWatchDialog->show();
    m_spikeWatchDialog->raise();
    m_spikeWatchDialog->activateWindow();
    m_spikeWatchDialog->startMonitoringIfIdle();
}

void MainWindow::onMemo() {
    if (!m_memoDialog) {
        m_memoDialog = new MemoDialog(this);
        m_memoDialog->setAttribute(Qt::WA_DeleteOnClose);
        connect(m_memoDialog, &QObject::destroyed, this, [this]() { m_memoDialog = nullptr; });
    }
    syncMemoDialogProfile();
    m_memoDialog->show();
    m_memoDialog->raise();
    m_memoDialog->activateWindow();
}

void MainWindow::syncMemoDialogProfile() {
    if (!m_memoDialog || !m_profileManager) {
        return;
    }
    const ProfileManager::Profile* profile = m_profileManager->activeProfile();
    const QString displayName =
        profile ? (m_profileManager->isDefaultProfile(profile->id) ? tr("기본") : profile->name)
                : QString();
    m_memoDialog->setProfile(m_profileManager->activeProfileId(),
                             m_profileManager->activeProjectDirectory(),
                             displayName);
}

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
    if (!m_profileManager || !maybeSave()) {
        return;
    }
    bool ok = false;
    const QString name = QInputDialog::getText(this,
                                               tr("프로필 추가"),
                                               tr("프로필 이름"),
                                               QLineEdit::Normal,
                                               tr("새 프로필"),
                                               &ok);
    if (!ok) {
        return;
    }
    saveActiveProfileSettings();
    stopAllSessions();
    pushGlobalUiUndoSnapshot(QStringLiteral("profile-add"));
    const QString id = m_profileManager->addProfile(name);
    refreshProfileList();
    switchToProfile(id);
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
    const bool fixedDefault = m_profileManager->isDefaultProfile(id);
    ProfileEditDialog dialog(profile->name,
                             m_profileManager->targetWindowTitle(id),
                             m_profileManager->subTargetWindowTitle(id),
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
    pushGlobalUiUndoSnapshot(QStringLiteral("profile-edit"));
    const ProfileEditDialog::Result edited = dialog.result();
    m_profileManager->renameProfile(id, edited.name);
    QString effectiveTargetTitle = edited.targetWindowTitle;
    QString effectiveSubTargetTitle = edited.subTargetWindowTitle;
    if (edited.defaultProfile) {
        // Default profile is global: no bound target window title.
        effectiveTargetTitle.clear();
        effectiveSubTargetTitle.clear();
    }
    m_profileManager->setTargetWindowTitle(id, effectiveTargetTitle);
    m_profileManager->setSubTargetWindowTitle(id, effectiveSubTargetTitle);
    if (edited.defaultProfile) {
        m_profileManager->setDefaultProfile(id);
        ProgramSettings::ProfileSettings profileSettings = m_profileManager->loadSettings(id);
        profileSettings.runWithoutTargetWindow = true;
        profileSettings.subTargetWindowTitle.clear();
        profileSettings.subLinkedTargetProcessPath.clear();
        m_profileManager->saveSettings(id, profileSettings, false, true);
    } else if (id == m_profileManager->defaultProfileId()
               && !m_profileManager->profiles().empty()) {
        m_profileManager->setDefaultProfile(m_profileManager->profiles().front().id);
    }

    if (id == m_profileManager->activeProfileId() && m_project) {
        m_project->setTargetWindowTitle(effectiveTargetTitle.toStdString());
        if (edited.defaultProfile) {
            ProgramSettings::setRunWithoutTargetWindow(true);
        }
        ScreenCapture::setTargetWindow(nullptr);
        ScreenCapture::setTargetWindowTitle(effectiveTargetTitle.toStdWString());
        updateTargetWindowDetails();
        scheduleAutoSave();
        scheduleRunWarmup();
        syncTargetWindowCenterPin();
    }
    refreshProfileList();
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
    const auto reply = QMessageBox::question(this,
                                             tr("프로필 삭제"),
                                             tr("'%1' 프로필을 삭제하시겠습니까?").arg(profile->name),
                                             QMessageBox::Yes | QMessageBox::No,
                                             QMessageBox::No);
    if (reply != QMessageBox::Yes || !maybeSave()) {
        return;
    }
    saveActiveProfileSettings();
    stopAllSessions();
    pushGlobalUiUndoSnapshot(QStringLiteral("profile-remove"));
    m_profileManager->removeProfile(id);
    refreshProfileList();
    loadActiveProfile();
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
        if (msg->message == kForegroundProfileSyncMessage) {
            syncProfileToForegroundWindow();
            *result = 0;
            return true;
        }
        if (msg->message == WM_NCHITTEST && !isMaximized()) {
            const int border = UiResizeHandle::kWindowResizeBorderPx;
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
    if (m_spikeWatchDialog) {
        m_spikeWatchDialog->close();
    }
    if (m_memoDialog) {
        m_memoDialog->saveNow();
        m_memoDialog->close();
    }
    WindowPicker::cancelPick();
    WindowPickerHoverOverlay::dismissAll();
    CursorPositionPicker::cancelPick();
    m_autoSaveTimer->stop();
    if (m_profilePackageSealTimer) {
        m_profilePackageSealTimer->stop();
    }
    saveSelectedFeaturePreference();
    autoSaveProject();
    saveActiveProfileSettings();
    sealActiveProfilePackage();
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

    prepareProjectUnload();
    m_project = std::make_unique<Project>();
    m_projectFilePath = m_profileManager ? m_profileManager->activeProjectPath()
                                         : Application::autoSaveFilePath();
    if (m_profileManager) {
        Application::instance()->setProjectDirectory(m_profileManager->activeProjectDirectory());
    }
    ScreenCapture::setTargetWindow(nullptr);
    ScreenCapture::setTargetWindowTitle(L"");
    m_featureList->setProject(m_project.get());
    if (m_profileManager) {
        m_featureList->setActiveProfileId(m_profileManager->activeProfileId());
    }
    m_featureList->refresh();
    onFeatureSelectionChanged();
    m_modified = false;
    updateWindowTitle();
    syncHotkeys();
    updateTargetWindowDetails();
    autoSaveProject();
    appendLog(tr("새 프로젝트를 준비했습니다."), LogLineKind::Info);
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
        appendLog(tr("변경 내용을 저장했습니다."), LogLineKind::Success);
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
    m_libraryPreviewFeature.reset();
    m_libraryPreviewEntryId.clear();
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

void MainWindow::restoreMemoDialogOpenState() {
    if (!QSettings().value(QStringLiteral("memo/open"), false).toBool()) {
        return;
    }
    if (!m_memoDialog) {
        m_memoDialog = new MemoDialog(this);
        m_memoDialog->setAttribute(Qt::WA_DeleteOnClose);
        connect(m_memoDialog, &QObject::destroyed, this, [this]() { m_memoDialog = nullptr; });
    }
    syncMemoDialogProfile();
    m_memoDialog->show();
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

void MainWindow::autoSaveProject(bool quiet) {
    if (!ensureProjectFilePath()) {
        return;
    }

    if (JsonSerializer::saveToFile(*m_project, m_projectFilePath, Application::instance()->projectDirectory())) {
        m_modified = false;
        updateWindowTitle();
        if (m_profileManager) {
            m_profileManager->invalidateCachedProject(m_profileManager->activeProfileId());
        }
        if (!quiet) {
            showTransientStatus(tr("자동 저장됨"), 2000);
        }
        scheduleProfilePackageSeal();
    }
}

void MainWindow::scheduleProfilePackageSeal() {
    if (!m_profileManager || !m_profilePackageSealTimer) {
        return;
    }
    m_profilePackageSealTimer->start();
}

void MainWindow::sealActiveProfilePackage() {
    if (!m_profileManager) {
        return;
    }
    saveActiveProfileSettings();
    m_profileManager->sealProfilePackage(m_profileManager->activeProfileId());
}

void MainWindow::onImportProfilePackage() {
    if (!m_profileManager) {
        return;
    }

    const QString path = QFileDialog::getOpenFileName(
        this,
        tr("PIPBONG 패키지 가져오기"),
        ProjectPackage::defaultExportDirectory(),
        tr("PIPBONG 패키지 (*%1)").arg(QString::fromLatin1(ProjectPackage::kExtension)));
    if (path.isEmpty()) {
        return;
    }

    const QString profileId = m_profileManager->importProfileFromPackage(path);
    if (profileId.isEmpty()) {
        QMessageBox::critical(this,
                              tr("패키지 가져오기"),
                              tr("PIPBONG 패키지를 불러오지 못했습니다."));
        return;
    }

    refreshProfileList();
    loadActiveProfile(false);
    syncProfileListSelection();
    appendLog(tr("PIPBONG 패키지를 새 프로필로 가져왔습니다: %1").arg(QFileInfo(path).fileName()),
              LogLineKind::Success);
}

void MainWindow::onExportProfilePackage() {
    if (!m_profileManager) {
        return;
    }

    if (!maybeSave(true)) {
        return;
    }
    saveActiveProfileSettings();
    sealActiveProfilePackage();

    const ProfileManager::Profile* profile = m_profileManager->activeProfile();
    const QString defaultBaseName =
        profile ? (m_profileManager->isDefaultProfile(profile->id) ? QStringLiteral("PIPBONG")
                                                                   : profile->name)
                : QStringLiteral("PIPBONG");
    const QString defaultPath =
        QDir(ProjectPackage::defaultExportDirectory())
            .filePath(defaultBaseName + QString::fromLatin1(ProjectPackage::kExtension));

    const QString path = QFileDialog::getSaveFileName(
        this,
        tr("PIPBONG 패키지 내보내기"),
        defaultPath,
        tr("PIPBONG 패키지 (*%1)").arg(QString::fromLatin1(ProjectPackage::kExtension)));
    if (path.isEmpty()) {
        return;
    }

    QString exportPath = path;
    if (!exportPath.endsWith(QString::fromLatin1(ProjectPackage::kExtension), Qt::CaseInsensitive)) {
        exportPath += QString::fromLatin1(ProjectPackage::kExtension);
    }

    if (!ProjectPackage::packDirectory(m_profileManager->activeProjectDirectory(), exportPath)) {
        QMessageBox::critical(this,
                              tr("패키지 내보내기"),
                              tr("PIPBONG 패키지를 저장하지 못했습니다."));
        return;
    }

    QSettings settings;
    settings.setValue(QStringLiteral("project/lastExportDirectory"),
                      QFileInfo(exportPath).absolutePath());
    showTransientStatus(tr("패키지 내보냄: %1").arg(QFileInfo(exportPath).fileName()), 2500);
    appendLog(tr("PIPBONG 패키지를 내보냈습니다: %1").arg(exportPath), LogLineKind::Success);
}

void MainWindow::refreshWorkflowEditor() {
    PIPBONG_PERF_SCOPE("refreshWorkflowEditor");
    if (m_libraryPreviewFeature && !m_libraryPreviewEntryId.isEmpty()) {
        m_workflowEditor->setProjectDirectory(m_featureLibraryManager
                                                  ? m_featureLibraryManager->entryProjectDirectory(m_libraryPreviewEntryId)
                                                  : Application::instance()->projectDirectory());
        m_workflowEditor->setFeature(m_libraryPreviewFeature.get());
        m_workflowEditor->clearLoopTiming();
        m_workflowEditor->setEditingEnabled(false);
        updateRunUiState();
        return;
    }

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

bool MainWindow::isFeatureSessionActive(const FeatureRunSession& session) const {
    if (session.engine && session.engine->isRunning()) {
        return true;
    }
    if (session.runningMode == FeatureRunMode::Hold && session.holdRunActive) {
        return true;
    }
    if (session.runningMode == FeatureRunMode::Trigger && session.repeatSession) {
        return true;
    }
    if (session.repeatSession
        && (session.runningMode == FeatureRunMode::RepeatInfinite
            || session.runningMode == FeatureRunMode::RepeatCount)) {
        return true;
    }
    return false;
}

bool MainWindow::isFeatureRunning(const std::string& featureId) const {
    const FeatureRunSession* session = sessionFor(featureId);
    return session && isFeatureSessionActive(*session);
}

bool MainWindow::hasAnyRunningSession() const {
    return !m_runSessions.empty();
}

QSet<QString> MainWindow::runningFeatureIds() const {
    QSet<QString> ids;
    for (const auto& entry : m_runSessions) {
        if (isFeatureSessionActive(entry.second)) {
            ids.insert(QString::fromStdString(entry.first));
        }
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

void MainWindow::appendSessionLog(const FeatureRunSession& session,
                                  const QString& message,
                                  LogLineKind kind) {
    if (m_logPanel) {
        m_logPanel->appendSessionLine(featureDisplayName(session.featureId), kind, message);
    }
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
        m_featureList->setEditControlsEnabled(!selectedRunning && !hasAnyRunningSession());
    }
    if (m_profileList) {
        m_profileList->setFeatureDropEnabled(canTransferFeatures());
    }
    if (m_workflowEditor) {
        m_workflowEditor->setEditingEnabled(!m_libraryPreviewFeature && !selectedRunning);
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
    ++session->holdRepeatGeneration;
    ++session->triggerCooldownGeneration;
    Feature* feature = m_project ? m_project->featureById(featureId) : nullptr;
    if (session->runningMode == FeatureRunMode::Hold) {
        releaseHoldHotkeyInputToTarget(*session, feature);
    }
    session->engine->stop();
    UserInputInterruptMonitor::instance().unregisterSession(featureId);
    appendSessionLog(*session, tr("실행 중지를 요청했습니다."), LogLineKind::Warning);
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
            ++entry.second.holdRepeatGeneration;
            ++entry.second.triggerCooldownGeneration;
            entry.second.engine->stopAndWait();
        }
        restoreRunStartCursorPosition(entry.second);
    }
    m_runSessions.clear();
    updateRunUiState();
}

void MainWindow::stopAllSessionsForProfileSwitch() {
    PIPBONG_PERF_SCOPE("stopAllSessionsForProfileSwitch");
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
            ++entry.second.holdRepeatGeneration;
            ++entry.second.triggerCooldownGeneration;
            Feature* feature = m_project ? m_project->featureById(entry.first) : nullptr;
            if (entry.second.runningMode == FeatureRunMode::Hold) {
                releaseHoldHotkeyInputToTarget(entry.second, feature);
            }
            entry.second.engine->stopAndWait(250);
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

void MainWindow::onSaveFeatureToLibraryRequested(const QString& featureId) {
    if (!m_project || !m_featureLibraryManager) {
        return;
    }
    if (featureId.isEmpty()) {
        return;
    }

    Feature* feature = m_project->featureById(featureId.toStdString());
    if (!feature) {
        return;
    }

    const QString suggestedName = QString::fromStdString(feature->name());
    bool ok = false;
    const QString entryName = QInputDialog::getText(this,
                                                     tr("라이브러리에 저장"),
                                                     tr("라이브러리 이름"),
                                                     QLineEdit::Normal,
                                                     suggestedName,
                                                     &ok);
    if (!ok) {
        return;
    }

    QString entryId;
    QStringList missingTemplates;
    const QString sourceProjectDir = Application::instance()->projectDirectory();
    pushGlobalUiUndoSnapshot(QStringLiteral("library-save-entry"));
    if (!m_featureLibraryManager->saveFeatureToLibrary(*feature,
                                                        sourceProjectDir,
                                                        entryName,
                                                        &entryId,
                                                        &missingTemplates)) {
        QMessageBox::critical(this,
                              tr("라이브러리 저장 실패"),
                              tr("기능을 라이브러리에 저장하지 못했습니다."));
        return;
    }

    if (!missingTemplates.empty()) {
        appendLog(tr("템플릿 일부를 라이브러리에 복사하지 못했습니다: %1")
                       .arg(missingTemplates.join(QStringLiteral(", "))),
                  LogLineKind::Warning);
    }

    showTransientStatus(tr("라이브러리에 저장됨: %1").arg(entryName), 2500);
    refreshFeatureLibraryPanel();
}

void MainWindow::onImportFeatureFromLibraryRequested() {
    if (!m_project || !m_featureLibraryManager || !m_featureList) {
        return;
    }

    const std::vector<FeatureLibraryManager::Entry> entries = m_featureLibraryManager->listEntries();
    if (entries.empty()) {
        QMessageBox::information(this,
                                 tr("라이브러리"),
                                 tr("아직 저장된 기능 라이브러리가 없습니다. 먼저 기능을 '라이브러리에 저장'으로 저장해 주세요."));
        return;
    }

    std::vector<FeatureLibraryDialog::EntryUi> uiEntries;
    uiEntries.reserve(entries.size());
    for (const auto& e : entries) {
        FeatureLibraryDialog::EntryUi ue;
        ue.id = e.id;
        ue.name = e.name;
        ue.templateCount = e.templateCount;
        uiEntries.push_back(std::move(ue));
    }

    FeatureLibraryDialog dialog(uiEntries, this);
    if (dialog.exec() != QDialog::Accepted) {
        return;
    }

    const QString entryId = dialog.selectedEntryId();
    if (entryId.isEmpty()) {
        return;
    }
    importLibraryEntry(entryId);
}

void MainWindow::onImportLibraryEntriesRequested(const QStringList& entryIds) {
    importLibraryEntries(entryIds);
}

void MainWindow::onLibraryEntrySelected(const QString& entryId) {
    m_libraryPreviewFeature.reset();
    m_libraryPreviewEntryId.clear();

    if (!m_featureLibraryManager || entryId.isEmpty()) {
        refreshWorkflowEditor();
        return;
    }

    auto preview = m_featureLibraryManager->loadEntryFeature(entryId);
    if (!preview) {
        showTransientStatus(tr("라이브러리 항목을 미리볼 수 없습니다."), 2500);
        refreshWorkflowEditor();
        return;
    }

    m_libraryPreviewEntryId = entryId;
    m_libraryPreviewFeature = std::move(preview);
    refreshWorkflowEditor();
}

void MainWindow::onDeleteLibraryEntriesRequested(const QStringList& entryIds) {
    if (!m_featureLibraryManager || entryIds.isEmpty()) {
        return;
    }

    const bool multiple = entryIds.size() > 1;
    const QString singleEntryName =
        multiple ? QString()
                 : libraryEntryDisplayName(m_featureLibraryManager.get(), entryIds.front());
    QMessageBox box(QMessageBox::Question,
                    tr("라이브러리에서 삭제"),
                    multiple ? tr("선택한 라이브러리 항목 %1개를 삭제하시겠습니까?\n"
                                  "이미 가져온 기능에는 영향이 없습니다.")
                                   .arg(entryIds.size())
                             : tr("'%1' 항목을 라이브러리에서 삭제하시겠습니까?\n"
                                  "이미 가져온 기능에는 영향이 없습니다.")
                                   .arg(singleEntryName),
                    QMessageBox::Yes | QMessageBox::No,
                    this);
    box.setDefaultButton(QMessageBox::No);
    if (static_cast<QMessageBox::StandardButton>(box.exec()) != QMessageBox::Yes) {
        return;
    }
    pushGlobalUiUndoSnapshot(QStringLiteral("library-remove-entries"));

    int removed = 0;
    bool previewCleared = false;
    for (const QString& entryId : entryIds) {
        if (entryId.isEmpty()) {
            continue;
        }
        if (!m_featureLibraryManager->removeEntry(entryId)) {
            continue;
        }
        ++removed;
        if (m_libraryPreviewEntryId == entryId) {
            m_libraryPreviewEntryId.clear();
            m_libraryPreviewFeature.reset();
            previewCleared = true;
        }
    }

    if (previewCleared) {
        refreshWorkflowEditor();
    }
    if (removed > 0) {
        showTransientStatus(multiple ? tr("라이브러리에서 %1개 항목 삭제됨").arg(removed)
                                     : tr("라이브러리에서 삭제됨: %1").arg(singleEntryName),
                          2500);
    }
    refreshFeatureLibraryPanel();
}

bool MainWindow::canTransferFeatures() const {
    return !hasAnyRunningSession();
}

bool MainWindow::removeFeatureFromProfile(const QString& profileId, const QString& featureId) {
    if (!m_profileManager || featureId.isEmpty()) {
        return false;
    }

    if (isFeatureRunning(featureId.toStdString())) {
        stopFeatureRun(featureId.toStdString());
    }

    if (profileId == m_profileManager->activeProfileId()) {
        if (!m_project) {
            return false;
        }
        const int index = featureIndexById(*m_project, featureId.toStdString());
        if (index < 0) {
            return false;
        }
        m_project->removeFeature(index);
        if (m_featureList) {
            m_featureList->refresh();
            onFeatureSelectionChanged();
        }
        syncHotkeys();
        scheduleAutoSave();
        return true;
    }

    const QString path = m_profileManager->projectPath(profileId);
    QString projectDir = m_profileManager->projectDirectory(profileId);
    auto project = JsonSerializer::loadFromFile(path, &projectDir);
    if (!project) {
        return false;
    }
    const int index = featureIndexById(*project, featureId.toStdString());
    if (index < 0) {
        return false;
    }
    project->removeFeature(index);
    return JsonSerializer::saveToFile(*project, path, projectDir);
}

bool MainWindow::importLibraryEntryToProfile(const QString& entryId,
                                             const QString& profileId,
                                             int insertIndex,
                                             bool refreshListUi,
                                             QString* insertedFeatureId) {
    if (!m_featureLibraryManager || !m_profileManager || entryId.isEmpty() || profileId.isEmpty()) {
        return false;
    }

    const QString targetProjectDir = m_profileManager->projectDirectory(profileId);
    FeatureLibraryManager::ImportResult res =
        m_featureLibraryManager->importEntryToProfile(entryId, targetProjectDir);
    if (!res.feature) {
        return false;
    }

    const QString newFeatureId = QString::fromStdString(res.feature->id());
    if (profileId == m_profileManager->activeProfileId()) {
        if (!m_project || !m_featureList) {
            return false;
        }
        const int count = static_cast<int>(m_project->features().size());
        const int idx = insertIndex < 0 ? count : qBound(0, insertIndex, count);
        m_project->insertFeature(idx, std::move(res.feature));
        if (refreshListUi) {
            m_featureList->refresh();
            m_featureList->selectFeatureById(newFeatureId);
            syncHotkeys();
            scheduleAutoSave();
        }
    } else {
        const QString path = m_profileManager->projectPath(profileId);
        QString projectDir = targetProjectDir;
        auto project = JsonSerializer::loadFromFile(path, &projectDir);
        if (!project) {
            return false;
        }
        const int count = static_cast<int>(project->features().size());
        const int idx = insertIndex < 0 ? count : qBound(0, insertIndex, count);
        project->insertFeature(idx, std::move(res.feature));
        if (!JsonSerializer::saveToFile(*project, path, projectDir)) {
            return false;
        }
    }

    if (insertedFeatureId) {
        *insertedFeatureId = newFeatureId;
    }

    if (!res.missingTemplatePaths.empty()) {
        appendLog(tr("가져온 기능의 템플릿 일부를 복사하지 못했습니다: %1")
                      .arg(res.missingTemplatePaths.join(QStringLiteral(", "))),
                  LogLineKind::Warning);
    }
    return true;
}

bool MainWindow::saveFeatureToLibraryFromDrag(const QString& featureId, const QString& profileId) {
    if (!m_featureLibraryManager || !m_profileManager || featureId.isEmpty()) {
        return false;
    }
    if (profileId != m_profileManager->activeProfileId() || !m_project) {
        return false;
    }

    Feature* feature = m_project->featureById(featureId.toStdString());
    if (!feature) {
        return false;
    }

    QString entryId;
    QStringList missingTemplates;
    const QString sourceProjectDir = m_profileManager->activeProjectDirectory();
    const QString entryName = QString::fromStdString(feature->name());
    if (!m_featureLibraryManager->saveFeatureToLibrary(*feature,
                                                        sourceProjectDir,
                                                        entryName,
                                                        &entryId,
                                                        &missingTemplates)) {
        return false;
    }

    if (!missingTemplates.empty()) {
        appendLog(tr("템플릿 일부를 라이브러리에 복사하지 못했습니다: %1")
                      .arg(missingTemplates.join(QStringLiteral(", "))),
                  LogLineKind::Warning);
    }

    refreshFeatureLibraryPanel();
    return true;
}

bool MainWindow::copyFeatureBetweenProfiles(const QString& featureId,
                                            const QString& sourceProfileId,
                                            const QString& targetProfileId,
                                            int insertIndex) {
    if (!canTransferFeatures() || !m_profileManager || featureId.isEmpty()) {
        return false;
    }
    if (sourceProfileId.isEmpty() || targetProfileId.isEmpty()
        || sourceProfileId == targetProfileId) {
        return false;
    }

    QString sourceProjectDir;
    const Feature* sourceFeature = nullptr;
    std::unique_ptr<Project> loadedSourceProject;

    if (sourceProfileId == m_profileManager->activeProfileId()) {
        if (!m_project) {
            return false;
        }
        sourceProjectDir = m_profileManager->activeProjectDirectory();
        sourceFeature = m_project->featureById(featureId.toStdString());
    } else {
        const QString sourcePath = m_profileManager->projectPath(sourceProfileId);
        sourceProjectDir = m_profileManager->projectDirectory(sourceProfileId);
        loadedSourceProject = JsonSerializer::loadFromFile(sourcePath, &sourceProjectDir);
        if (!loadedSourceProject) {
            return false;
        }
        sourceFeature = loadedSourceProject->featureById(featureId.toStdString());
    }

    if (!sourceFeature) {
        return false;
    }

    std::unique_ptr<Feature> copiedFeature = sourceFeature->duplicateAsNewInstance();
    if (!copiedFeature) {
        return false;
    }

    const QString targetProjectDir = m_profileManager->projectDirectory(targetProfileId);
    const QStringList missingTemplates =
        FeatureLibraryManager::copyFeatureTemplatesBetweenDirectories(*sourceFeature,
                                                                     sourceProjectDir,
                                                                     targetProjectDir);
    if (!missingTemplates.empty()) {
        appendLog(tr("프로필 복사 시 템플릿 일부를 복사하지 못했습니다: %1")
                      .arg(missingTemplates.join(QStringLiteral(", "))),
                  LogLineKind::Warning);
    }

    const QString newFeatureId = QString::fromStdString(copiedFeature->id());
    if (targetProfileId == m_profileManager->activeProfileId()) {
        if (!m_project || !m_featureList) {
            return false;
        }
        const int count = static_cast<int>(m_project->features().size());
        const int idx = insertIndex < 0 ? count : qBound(0, insertIndex, count);
        m_project->insertFeature(idx, std::move(copiedFeature));
        m_featureList->refresh();
        m_featureList->selectFeatureById(newFeatureId);
        syncHotkeys();
        scheduleAutoSave();
    } else {
        const QString targetPath = m_profileManager->projectPath(targetProfileId);
        QString loadedProjectDir = targetProjectDir;
        auto targetProject = JsonSerializer::loadFromFile(targetPath, &loadedProjectDir);
        if (!targetProject) {
            return false;
        }
        const int count = static_cast<int>(targetProject->features().size());
        const int idx = insertIndex < 0 ? count : qBound(0, insertIndex, count);
        targetProject->insertFeature(idx, std::move(copiedFeature));
        if (!JsonSerializer::saveToFile(*targetProject, targetPath, loadedProjectDir)) {
            return false;
        }
    }

    return true;
}

void MainWindow::onFeatureDroppedOnFeatureList(const QMimeData* mime, int insertIndex) {
    if (!canTransferFeatures() || !mime || !m_profileManager) {
        return;
    }

    const FeatureDragMime::Payload payload = FeatureDragMime::parse(mime);
    if (!payload.isValid() || payload.source != FeatureDragMime::Source::Library) {
        return;
    }

    const QStringList ids = payload.allIds();
    if (ids.isEmpty()) {
        return;
    }
    pushGlobalUiUndoSnapshot(QStringLiteral("feature-import-from-library"));

    int nextInsert = insertIndex;
    int imported = 0;
    QStringList names;
    for (const QString& entryId : ids) {
        const QString entryName = libraryEntryDisplayName(m_featureLibraryManager.get(), entryId);
        if (!importLibraryEntryToProfile(entryId, m_profileManager->activeProfileId(), nextInsert)) {
            QMessageBox::critical(this,
                                  tr("라이브러리 가져오기 실패"),
                                  tr("라이브러리 항목을 불러오지 못했습니다."));
            return;
        }
        ++imported;
        names.push_back(entryName);
        if (nextInsert >= 0) {
            ++nextInsert;
        }
    }

    if (imported == 1) {
        showTransientStatus(tr("라이브러리에서 가져옴: %1").arg(names.first()), 2500);
    } else {
        showTransientStatus(tr("라이브러리에서 %1개 가져옴").arg(imported), 2500);
    }
}

void MainWindow::onFeatureDroppedOnLibrary(const QMimeData* mime) {
    if (!canTransferFeatures() || !mime || !m_profileManager) {
        return;
    }

    const FeatureDragMime::Payload payload = FeatureDragMime::parse(mime);
    if (!payload.isValid() || payload.source != FeatureDragMime::Source::Profile) {
        return;
    }
    if (payload.profileId != m_profileManager->activeProfileId()) {
        return;
    }

    const QStringList ids = payload.allIds();
    if (ids.isEmpty()) {
        return;
    }
    pushGlobalUiUndoSnapshot(QStringLiteral("feature-save-to-library"));

    int saved = 0;
    QStringList names;
    for (const QString& featureId : ids) {
        Feature* feature = m_project ? m_project->featureById(featureId.toStdString()) : nullptr;
        if (!feature) {
            continue;
        }
        const QString featureName = QString::fromStdString(feature->name());
        if (!saveFeatureToLibraryFromDrag(featureId, payload.profileId)) {
            QMessageBox::critical(this,
                                  tr("라이브러리 저장 실패"),
                                  tr("기능을 라이브러리에 저장하지 못했습니다."));
            return;
        }
        ++saved;
        names.push_back(featureName);
    }

    if (saved <= 0) {
        return;
    }
    if (saved == 1) {
        showTransientStatus(tr("라이브러리에 저장됨: %1").arg(names.first()), 2500);
    } else {
        showTransientStatus(tr("라이브러리에 %1개 저장됨").arg(saved), 2500);
    }
}

void MainWindow::onLibraryEntriesReordered(int fromRow, int toRow) {
    if (!m_featureLibraryManager || fromRow == toRow) {
        return;
    }

    const std::vector<FeatureLibraryManager::Entry> entries = m_featureLibraryManager->listEntries();
    if (fromRow < 0 || toRow < 0 || fromRow >= static_cast<int>(entries.size())
        || toRow >= static_cast<int>(entries.size())) {
        return;
    }

    QStringList orderedIds;
    orderedIds.reserve(static_cast<int>(entries.size()));
    for (const FeatureLibraryManager::Entry& entry : entries) {
        orderedIds.push_back(entry.id);
    }
    const QString movedId = orderedIds.takeAt(fromRow);
    orderedIds.insert(toRow, movedId);
    pushGlobalUiUndoSnapshot(QStringLiteral("library-reorder"));
    if (m_featureLibraryManager->reorderEntries(orderedIds)) {
        refreshFeatureLibraryPanel();
        showTransientStatus(tr("라이브러리 순서를 저장했습니다."), 1500);
    }
}

void MainWindow::onLibraryEntriesMultiReordered(const QList<int>& selectedRows, int insertIndex) {
    if (!m_featureLibraryManager || selectedRows.isEmpty()) {
        return;
    }

    const std::vector<FeatureLibraryManager::Entry> entries = m_featureLibraryManager->listEntries();
    QStringList orderedIds;
    orderedIds.reserve(static_cast<int>(entries.size()));
    for (const FeatureLibraryManager::Entry& entry : entries) {
        orderedIds.push_back(entry.id);
    }

    QStringList movedIds;
    movedIds.reserve(selectedRows.size());
    for (int row : selectedRows) {
        if (row < 0 || row >= orderedIds.size()) {
            return;
        }
        movedIds.push_back(orderedIds.at(row));
    }
    for (int i = selectedRows.size() - 1; i >= 0; --i) {
        orderedIds.removeAt(selectedRows.at(i));
    }
    int dest = insertIndex;
    for (int row : selectedRows) {
        if (row < insertIndex) {
            --dest;
        }
    }
    dest = qBound(0, dest, orderedIds.size());
    for (int i = 0; i < movedIds.size(); ++i) {
        orderedIds.insert(dest + i, movedIds.at(i));
    }

    pushGlobalUiUndoSnapshot(QStringLiteral("library-reorder-multi"));
    if (m_featureLibraryManager->reorderEntries(orderedIds)) {
        refreshFeatureLibraryPanel();
        showTransientStatus(tr("라이브러리 순서를 저장했습니다."), 1500);
    }
}

void MainWindow::onFeatureDroppedOnProfile(const QString& targetProfileId, const QMimeData* mime) {
    if (!canTransferFeatures() || !mime || !m_profileManager || targetProfileId.isEmpty()) {
        return;
    }

    const FeatureDragMime::Payload payload = FeatureDragMime::parse(mime);
    if (!payload.isValid()) {
        return;
    }

    const QString profileName = profileDisplayName(m_profileManager.get(), targetProfileId);
    const QStringList ids = payload.allIds();
    if (ids.isEmpty()) {
        return;
    }

    if (payload.source == FeatureDragMime::Source::Library) {
        pushGlobalUiUndoSnapshot(QStringLiteral("profile-import-library-feature"));
        int imported = 0;
        for (const QString& entryId : ids) {
            if (!importLibraryEntryToProfile(entryId, targetProfileId, -1)) {
                QMessageBox::critical(this,
                                      tr("라이브러리 가져오기 실패"),
                                      tr("라이브러리 항목을 불러오지 못했습니다."));
                return;
            }
            ++imported;
        }
        if (imported == 1) {
            const QString entryName = libraryEntryDisplayName(m_featureLibraryManager.get(), ids.first());
            showTransientStatus(tr("프로필 '%1'에 라이브러리 기능을 가져옴: %2")
                                    .arg(profileName, entryName),
                                2500);
        } else {
            showTransientStatus(tr("프로필 '%1'에 라이브러리 기능 %2개 가져옴")
                                    .arg(profileName)
                                    .arg(imported),
                                2500);
        }
        return;
    }

    if (payload.profileId == targetProfileId) {
        return;
    }

    int copied = 0;
    QString firstName;
    pushGlobalUiUndoSnapshot(QStringLiteral("profile-copy-feature"));
    for (const QString& featureId : ids) {
        if (!copyFeatureBetweenProfiles(featureId, payload.profileId, targetProfileId, -1)) {
            QMessageBox::critical(this,
                                  tr("기능 복사 실패"),
                                  tr("기능을 다른 프로필로 복사하지 못했습니다."));
            return;
        }
        if (copied == 0) {
            Feature* sourceFeature = nullptr;
            if (payload.profileId == m_profileManager->activeProfileId() && m_project) {
                sourceFeature = m_project->featureById(featureId.toStdString());
            }
            firstName = sourceFeature ? QString::fromStdString(sourceFeature->name()) : featureId;
        }
        ++copied;
    }

    if (copied == 1) {
        showTransientStatus(tr("프로필 '%1'로 기능을 복사했습니다: %2")
                                .arg(profileName, firstName),
                            2500);
    } else {
        showTransientStatus(tr("프로필 '%1'로 기능 %2개를 복사했습니다")
                                .arg(profileName)
                                .arg(copied),
                            2500);
    }
}

bool MainWindow::importLibraryEntry(const QString& entryId) {
    return importLibraryEntries(QStringList{entryId});
}

bool MainWindow::importLibraryEntries(const QStringList& entryIds) {
    if (!m_project || !m_featureLibraryManager || !m_featureList || !m_profileManager
        || entryIds.isEmpty()) {
        return false;
    }

    int insertIndex = m_featureList->selectedIndex() >= 0
                          ? m_featureList->selectedIndex() + 1
                          : static_cast<int>(m_project->features().size());
    const QString profileId = m_profileManager->activeProfileId();
    QString lastFeatureId;
    pushGlobalUiUndoSnapshot(QStringLiteral("feature-import-library"));
    int imported = 0;
    for (const QString& entryId : entryIds) {
        if (entryId.isEmpty()) {
            continue;
        }
        QString insertedFeatureId;
        if (!importLibraryEntryToProfile(entryId, profileId, insertIndex, false, &insertedFeatureId)) {
            continue;
        }
        lastFeatureId = insertedFeatureId;
        ++insertIndex;
        ++imported;
    }

    if (imported == 0) {
        QMessageBox::critical(this,
                              tr("라이브러리 가져오기 실패"),
                              tr("라이브러리 항목을 불러오지 못했습니다."));
        return false;
    }

    m_featureList->refresh();
    if (!lastFeatureId.isEmpty()) {
        m_featureList->selectFeatureById(lastFeatureId);
    }
    syncHotkeys();
    scheduleAutoSave();

    if (imported == 1) {
        showTransientStatus(tr("라이브러리에서 가져옴: %1")
                                .arg(libraryEntryDisplayName(m_featureLibraryManager.get(),
                                                             entryIds.front())),
                            2500);
    } else {
        showTransientStatus(tr("라이브러리에서 %1개 항목을 가져왔습니다.").arg(imported), 2500);
    }
    return true;
}

void MainWindow::refreshFeatureLibraryPanel() {
    if (!m_featureLibraryManager || !m_featureList) {
        return;
    }
    const std::vector<FeatureLibraryManager::Entry> entries = m_featureLibraryManager->listEntries();
    std::vector<FeatureListPanel::LibraryEntryUi> uiEntries;
    uiEntries.reserve(entries.size());
    for (const auto& e : entries) {
        FeatureListPanel::LibraryEntryUi ue;
        ue.id = e.id;
        ue.name = e.name;
        ue.templateCount = e.templateCount;
        uiEntries.push_back(std::move(ue));
    }
    m_featureList->setLibraryEntries(uiEntries);
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
    if (!feature->enabled()) {
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
#ifdef _WIN32
    if (!ProgramSettings::runWithoutTargetWindow() && !ScreenCapture::hasResolvableTargetWindow()) {
        QMessageBox::information(
            this,
            tr("실행"),
            tr("대상 창이 지정되지 않았습니다. '창 지정'으로 대상 창을 선택하거나, "
               "프로그램 설정에서 '창을 지정하지 않은 상태에서도 동작'을 켜세요."));
        return;
    }
#endif

    const std::string featureId = feature->id();
    if (FeatureRunSession* existing = sessionFor(featureId)) {
        if (isFeatureSessionActive(*existing)) {
            return;
        }
        if (existing->sessionContext) {
            existing->sessionContext->endRunInputSession();
        }
        UserInputInterruptMonitor::instance().unregisterSession(featureId);
        m_runSessions.erase(featureId);
    }

    FeatureRunSession session;
    session.featureId = featureId;
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
    if (session.runningMode == FeatureRunMode::Hold
        || session.runningMode == FeatureRunMode::RepeatInfinite
        || session.runningMode == FeatureRunMode::RepeatCount) {
        ++session.holdRepeatGeneration;
    }
    session.restoreMousePositionOnEnd = feature->restoreMousePositionOnEnd();
    session.lockMouseToScreenCenterDuringRun = feature->lockMouseToScreenCenterDuringRun();
    session.lockMouseToCurrentPositionDuringRun = feature->lockMouseToCurrentPositionDuringRun();
    session.lockMouseDuringFirstLoopCount = feature->lockMouseDuringFirstLoopCount();
    session.unlockMouseOnBlockFailureCount = feature->unlockMouseOnBlockFailureCount();

    connectSessionEngine(session);
    m_runSessions.emplace(featureId, std::move(session));
    FeatureRunSession& activeSession = m_runSessions.at(featureId);
    const bool hotkeyHoldStart = fromHotkey && feature->runMode() == FeatureRunMode::Hold;
    if (!hotkeyHoldStart) {
        selectRunningFeatureForDisplay(feature);
    }
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
        if (imageFind->customRegionsWindowPercent.empty()) {
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
        physical.reserve(block->customRegionsWindowPercent.size());
        for (const PercentRegion& percent : block->customRegionsWindowPercent) {
            if (percent.width <= 0.0 || percent.height <= 0.0) {
                continue;
            }
            physical.push_back(ScreenCapture::resolveWindowPercentRegion(percent));
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
    // Keep the title chosen at session start (main vs sub auto-detect). Re-resolving on
    // every repeat would flip back to the main game window after the launcher loses focus.
    if (session.sessionContext->targetWindowTitle().empty()) {
        session.sessionContext->setTargetWindowTitle(resolveEffectiveTargetTitleW());
    }
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
    session.lockMouseDuringFirstLoopCount = feature->lockMouseDuringFirstLoopCount();
    session.unlockMouseOnBlockFailureCount = feature->unlockMouseOnBlockFailureCount();

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
    session.sessionContext->setFeatureRoiCorrectionExpandPercent(
        feature->roiCorrectionExpandPercent());
    session.sessionContext->setRunLoopNumber(session.sessionIteration + 1);
}

void MainWindow::accumulateLoopCompletionStats(FeatureRunSession& session,
                                               bool success,
                                               std::int64_t elapsedOverrideMs) {
    const qint64 elapsedMs =
        elapsedOverrideMs >= 0
            ? static_cast<qint64>(elapsedOverrideMs)
            : (session.loopTimer.isValid() ? session.loopTimer.elapsed() : 0);
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
}

void MainWindow::flushWorkerFastRepeatUi(const std::string& featureId) {
    FeatureRunSession* session = sessionFor(featureId);
    if (!session) {
        return;
    }

    auto coalesceIt = m_fastRepeatUiCoalesce.find(featureId);
    if (coalesceIt == m_fastRepeatUiCoalesce.end() || !coalesceIt->second) {
        return;
    }
    WorkerFastRepeatUiCoalesce& coalesce = *coalesceIt->second;

    int iterations = 0;
    qint64 totalElapsedMs = 0;
    bool lastSuccess = false;
    qint64 lastElapsedMs = 0;
    QString lastMessage;
    {
        QMutexLocker lock(&coalesce.mutex);
        coalesce.flushScheduled = false;
        iterations = coalesce.pendingIterations;
        if (iterations <= 0) {
            return;
        }
        totalElapsedMs = coalesce.pendingTotalElapsedMs;
        lastSuccess = coalesce.pendingLastSuccess;
        lastElapsedMs = coalesce.pendingLastElapsedMs;
        lastMessage = coalesce.pendingLastMessage;
        coalesce.pendingIterations = 0;
        coalesce.pendingTotalElapsedMs = 0;
    }

    session->totalLoopElapsedMs += totalElapsedMs;
    session->completedLoopCount += iterations;
    session->sessionIteration += iterations;
    session->hasLastLoopTiming = true;
    session->lastLoopNumber = session->sessionIteration;
    session->lastLoopElapsedMs = lastElapsedMs;
    session->lastLoopAverageMs =
        session->completedLoopCount > 0
            ? session->totalLoopElapsedMs / session->completedLoopCount
            : 0;
    session->lastLoopSuccess = lastSuccess;
    session->loopsSinceLastLogPublish += iterations;

    if (session->sessionContext) {
        session->sessionContext->setRunLoopNumber(session->sessionIteration + 1);
    }

    publishLoopCompletionUi(*session, lastSuccess, lastMessage);

    Feature* feature = m_project ? m_project->featureById(featureId) : nullptr;
    if (feature) {
        syncEarlyLoopMouseLock(*session);
    }

    {
        QMutexLocker lock(&coalesce.mutex);
        if (coalesce.pendingIterations > 0 && !coalesce.flushScheduled) {
            coalesce.flushScheduled = true;
            QTimer::singleShot(0, this, [this, featureId]() {
                flushWorkerFastRepeatUi(featureId);
            });
        }
    }
}

WorkerFastRepeatUiCoalesce& MainWindow::fastRepeatUiCoalesceFor(const std::string& featureId) {
    auto& slot = m_fastRepeatUiCoalesce[featureId];
    if (!slot) {
        slot = std::make_unique<WorkerFastRepeatUiCoalesce>();
    }
    return *slot;
}

void MainWindow::configureWorkerFastRepeat(FeatureRunSession& session, Feature* feature) {
    if (!session.sessionContext || !feature) {
        return;
    }

    const bool eligible = session.repeatSession
                          && (session.runningMode == FeatureRunMode::Hold
                              || session.runningMode == FeatureRunMode::RepeatInfinite
                              || session.runningMode == FeatureRunMode::RepeatCount);
    if (!eligible) {
        session.sessionContext->clearWorkerFastRepeatCallbacks();
        return;
    }

    const std::string featureId = session.featureId;
    Feature* featurePtr = feature;
    HotkeyManager* hotkeyMgr = m_hotkeyManager;

    ExecutionContext::WorkerFastRepeatCallbacks callbacks;
    callbacks.delayMs = [featurePtr]() { return featurePtr->resolvedLoopIntervalMs(); };
    callbacks.onIterationComplete =
        [this, featureId](bool success, std::int64_t elapsedMs, const std::string& message) {
            FeatureRunSession* active = sessionFor(featureId);
            if (!active) {
                return;
            }
            WorkerFastRepeatUiCoalesce& coalesce = fastRepeatUiCoalesceFor(featureId);
            const QString qMessage = QString::fromStdString(message);
            bool scheduleFlush = false;
            {
                QMutexLocker lock(&coalesce.mutex);
                ++coalesce.pendingIterations;
                coalesce.pendingTotalElapsedMs += elapsedMs;
                coalesce.pendingLastSuccess = success;
                coalesce.pendingLastElapsedMs = elapsedMs;
                coalesce.pendingLastMessage = qMessage;
                if (!coalesce.flushScheduled) {
                    coalesce.flushScheduled = true;
                    scheduleFlush = true;
                }
            }
            if (scheduleFlush) {
                QTimer::singleShot(0, this, [this, featureId]() {
                    flushWorkerFastRepeatUi(featureId);
                });
            }
        };
    callbacks.shouldContinue = [this, featureId, featurePtr, hotkeyMgr](bool success,
                                                                         bool detectionFailed) {
        FeatureRunSession* active = sessionFor(featureId);
        if (!active || !featurePtr) {
            return false;
        }
        if (active->userStopRequested) {
            return false;
        }

        const bool infiniteExitEnabled =
            featurePtr->infiniteExitAfterConsecutiveMisses() > 0
            && (active->runningMode == FeatureRunMode::RepeatInfinite
                || active->runningMode == FeatureRunMode::Hold);
        if (infiniteExitEnabled) {
            if (success) {
                active->consecutiveDetectionFailLoops = 0;
            } else if (detectionFailed) {
                ++active->consecutiveDetectionFailLoops;
                if (active->consecutiveDetectionFailLoops
                    >= featurePtr->infiniteExitAfterConsecutiveMisses()) {
                    return false;
                }
            } else if (!success) {
                return false;
            }
        }

        if (active->runningMode == FeatureRunMode::RepeatCount && !success) {
            return false;
        }
        if (success && active->runningMode == FeatureRunMode::RepeatCount && active->repeatSession) {
            --active->repeatRemaining;
        }

        switch (active->runningMode) {
        case FeatureRunMode::Hold:
            return active->repeatSession && active->holdRunActive && hotkeyMgr
                   && hotkeyMgr->isHoldBindingDown(featureId);
        case FeatureRunMode::RepeatInfinite:
            return active->repeatSession;
        case FeatureRunMode::RepeatCount:
            return active->repeatSession && active->repeatRemaining > 0;
        default:
            return false;
        }
    };

    session.sessionContext->setWorkerFastRepeatCallbacks(std::move(callbacks));
}

void MainWindow::publishLoopCompletionUi(FeatureRunSession& session,
                                         bool success,
                                         const QString& message) {
    const bool fastRepeat =
        session.sessionContext && session.sessionContext->suppressRepeatUi();

    if (isDisplayedRunningFeature(&session)) {
        syncLoopTimingToWorkflowEditor(&session);
    }

    if (fastRepeat) {
        constexpr int kMinLogIntervalMs = 500;
        if (session.loopLogPublishTimer.isValid()
            && session.loopLogPublishTimer.elapsed() < kMinLogIntervalMs) {
            return;
        }
        session.loopLogPublishTimer.restart();
    }

    QString line = tr("%1회째 루프 · %2ms · %3")
                       .arg(session.lastLoopNumber)
                       .arg(session.lastLoopElapsedMs)
                       .arg(success ? tr("성공") : tr("실패"));
    if (!success && !message.isEmpty()) {
        line += QStringLiteral(" · ") + message;
    }
    if (fastRepeat && session.loopsSinceLastLogPublish > 1) {
        line += tr(" (+%1회)").arg(session.loopsSinceLastLogPublish - 1);
    }
    session.loopsSinceLastLogPublish = 0;

    appendSessionLog(session, line, success ? LogLineKind::Success : LogLineKind::Warning);
}

void MainWindow::logLoopCompletion(FeatureRunSession& session, bool success, const QString& message) {
    accumulateLoopCompletionStats(session, success);
    publishLoopCompletionUi(session, success, message);
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

    const bool hotkeyHoldFirstStart = !repeatIteration && session.hotkeyLaunchedSession
                                      && session.runningMode == FeatureRunMode::Hold;

    if (!repeatIteration) {
        syncEffectiveTargetWindowTitleToCapture();
        session.sessionIteration = 0;
        session.hasLastLoopTiming = false;
        session.totalLoopElapsedMs = 0;
        session.completedLoopCount = 0;
        session.lastLoopAverageMs = 0;
        session.earlyLoopMouseLockEngaged = false;
        session.earlyLoopMouseLockReleased = false;
        session.earlyLoopMouseLockFailureCount = 0;
        session.loopLogPublishTimer.invalidate();
        session.loopsSinceLastLogPublish = 0;
        m_fastRepeatUiCoalesce.erase(session.featureId);
        captureRunStartCursorPosition(session);
        captureFeatureMouseLockPosition(session);
        if (session.lockMouseDuringFirstLoopCount > 0) {
            syncEarlyLoopMouseLock(session);
        } else {
            engageFeatureMouseLock(session);
        }
        if (!hotkeyHoldFirstStart) {
            if (isDisplayedRunningFeature(&session)) {
                syncLoopTimingToWorkflowEditor(&session);
                m_workflowEditor->clearBlockMatchResults();
                m_workflowEditor->clearExecutionHighlight();
                m_workflowEditor->persistRunFeedbackForCurrentFeature();
            }

            if (shouldLogRunDetails(session)) {
                appendSessionLog(session, tr("기능 실행을 시작합니다"), LogLineKind::Accent);
            }
            updateRunUiState();
        } else {
            const std::string featureId = session.featureId;
            QTimer::singleShot(0, this, [this, featureId]() { deferHoldSessionUiAfterStart(featureId); });
        }
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
    syncEarlyLoopMouseLock(session);
    if (!repeatIteration && session.sessionContext) {
        session.sessionContext->clearCorrectedRois();
        session.sessionContext->clearRememberedPositions();
    }
    syncUserInputInterruptForSession(session, feature);

    const bool suppressRepeatUi =
        repeatIteration
        && (session.runningMode == FeatureRunMode::Hold
            || session.runningMode == FeatureRunMode::RepeatInfinite);
    if (session.sessionContext) {
        session.sessionContext->setSuppressRepeatUi(suppressRepeatUi);
    }

    configureWorkerFastRepeat(session, feature);

    const std::wstring targetTitle = resolveEffectiveTargetTitleW();
    const std::string projectDir = Application::instance()->projectDirectory().toStdString();
    const bool skipTargetActivation = session.hotkeyLaunchedSession && !repeatIteration;
    WorkflowEngine* engine = session.engine.get();

    // The workflow cannot be edited while its session is running. Reuse the session
    // clone instead of allocating a fresh block graph on every fast Hold/infinite loop.
    ensureRunSessionResources(session, feature, false);

    if (repeatIteration) {
        engine->runPrepared([&session]() {
            PreparedWorkflowRun run;
            run.workflow = session.sessionWorkflow;
            run.context = session.sessionContext;
            run.context->resetStop();
            return run;
        });
        return;
    }

    Feature* featurePtr = feature;
    engine->runPrepared([featurePtr, &session, targetTitle, projectDir, skipTargetActivation]() {
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
#ifdef _WIN32
        if (!skipTargetActivation) {
            ScreenCapture::activateTargetWindow();
        }
#endif
        return run;
    });
}

void MainWindow::deferHoldSessionUiAfterStart(const std::string& featureId) {
    FeatureRunSession* session = sessionFor(featureId);
    if (!session) {
        return;
    }
    Feature* feature = m_project ? m_project->featureById(featureId) : nullptr;
    if (feature) {
        selectRunningFeatureForDisplay(feature);
    }
    if (isDisplayedRunningFeature(session)) {
        syncLoopTimingToWorkflowEditor(session);
        m_workflowEditor->clearBlockMatchResults();
        m_workflowEditor->clearExecutionHighlight();
        m_workflowEditor->persistRunFeedbackForCurrentFeature();
    }
    if (shouldLogRunDetails(*session)) {
        appendSessionLog(*session, tr("기능 실행을 시작합니다"), LogLineKind::Accent);
    }
    updateRunUiState();
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

void MainWindow::scheduleRepeatIteration(FeatureRunSession& session,
                                         Feature* feature,
                                         bool success,
                                         const QString& message) {
    // Do not reconcileHoldBindingDown here: same-key KeyPress Tap during Hold clears
    // GetAsyncKeyState and would falsely end the session before the loop-interval timer.
    if (!shouldContinueRunSession(session, feature)) {
        finishRunSession(session.featureId, success, message);
        return;
    }

    const int delayMs = feature ? feature->resolvedLoopIntervalMs() : 0;
    if (delayMs > 0) {
        appendSessionLog(session,
                         tr("루프 간격 %1ms 대기").arg(delayMs),
                         LogLineKind::Info);
    }
    if (delayMs <= 0) {
        continueRepeatSession(session, feature, success, message);
        return;
    }
    const quint64 generation = ++session.holdRepeatGeneration;
    const std::string featureId = session.featureId;
    QTimer::singleShot(qMax(0, delayMs), this, [this, featureId, success, message, generation]() {
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
        syncEffectiveTargetWindowTitleToCapture();
        session.sessionIteration = 0;
        session.hasLastLoopTiming = false;
        session.totalLoopElapsedMs = 0;
        session.completedLoopCount = 0;
        session.lastLoopAverageMs = 0;
        session.earlyLoopMouseLockEngaged = false;
        session.earlyLoopMouseLockReleased = false;
        session.earlyLoopMouseLockFailureCount = 0;
        captureRunStartCursorPosition(session);
        captureFeatureMouseLockPosition(session);
        if (session.lockMouseDuringFirstLoopCount > 0) {
            syncEarlyLoopMouseLock(session);
        } else {
            engageFeatureMouseLock(session);
        }
        if (isDisplayedRunningFeature(&session)) {
            syncLoopTimingToWorkflowEditor(&session);
            m_workflowEditor->clearBlockMatchResults();
            m_workflowEditor->clearExecutionHighlight();
            m_workflowEditor->persistRunFeedbackForCurrentFeature();
        }
        appendSessionLog(session, tr("트리거 감시를 시작합니다"), LogLineKind::Accent);
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

    const std::wstring targetTitle = resolveEffectiveTargetTitleW();
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
    appendSessionLog(session, tr("화면에서 찾음 — 워크플로 실행"), LogLineKind::Success);
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
            snap.releasedMouseLock = true;
        }

        if (snap.pausedByTrigger || snap.releasedMouseLock) {
            triggerSession.triggerPreemptedSessions.push_back(std::move(snap));
            appendSessionLog(other, tr("다른 기능 실행을 잠시 멈춤"), LogLineKind::Warning);
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
        triggerSession.triggerReleasedOwnMouseLockForPreempt = true;
    }

    reconcileMouseLocksFromRunningSessions();

    appendSessionLog(triggerSession,
                     tr("다른 기능 %1개를 잠시 멈춤").arg(triggerSession.triggerPreemptedSessions.size()),
                     LogLineKind::Warning);
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
        triggerSession.triggerReleasedOwnMouseLockForPreempt = false;
    }

    for (const TriggerPreemptedSession& snap : triggerSession.triggerPreemptedSessions) {
        FeatureRunSession* other = sessionFor(snap.featureId);
        if (!other || !other->sessionContext) {
            continue;
        }

        if (snap.pausedByTrigger) {
            other->sessionContext->setPaused(false);
            appendSessionLog(*other, tr("일시정지했던 기능을 다시 실행합니다"), LogLineKind::Success);
        }
    }

    triggerSession.triggerPreemptedSessions.clear();
    reconcileMouseLocksFromRunningSessions();
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

    appendSessionLog(session, tr("다시 감시하기까지 %1ms 대기").arg(cooldownMs), LogLineKind::Info);
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
            appendSessionLog(session, tr("실행 실패 — 감시를 다시 시작합니다"), LogLineKind::Warning);
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

    if (session && session->runningMode == FeatureRunMode::Hold) {
        Feature* feature = m_project ? m_project->featureById(featureId) : nullptr;
        releaseHoldHotkeyInputToTarget(*session, feature);
    }

    if (session && session->sessionContext) {
        session->sessionContext->endRunInputSession();
    }

    if (session && session->runningMode == FeatureRunMode::Trigger) {
        resumePreemptedSessionsForTrigger(*session);
    }

    if (session) {
        ++session->triggerCooldownGeneration;
        ++session->holdRepeatGeneration;
        restoreRunStartCursorPosition(*session);
    }

    UserInputInterruptMonitor::instance().unregisterSession(featureId);
    m_fastRepeatUiCoalesce.erase(featureId);
    m_runSessions.erase(featureId);
    reconcileMouseLocksFromRunningSessions();
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
    if (!m_project || shouldSuppressFeatureHotkeyExecution()) {
        return;
    }

    Feature* feature = m_project->featureById(featureId.toStdString());
    if (!feature || !feature->enabled()) {
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
    if (!m_project || shouldSuppressFeatureHotkeyExecution()) {
        return;
    }

    Feature* feature = m_project->featureById(featureId.toStdString());
    if (!feature || !feature->enabled() || feature->runMode() != FeatureRunMode::Hold) {
        return;
    }

    const std::string id = featureId.toStdString();
    if (FeatureRunSession* session = sessionFor(id)) {
        if (session->holdRunActive) {
            if (session->engine && !session->engine->isRunning()) {
                session->repeatSession = true;
                scheduleRepeatIteration(*session, feature, session->lastLoopSuccess, QString());
            }
            return;
        }
        if (isFeatureSessionActive(*session)) {
            return;
        }
        if (session->sessionContext) {
            session->sessionContext->endRunInputSession();
        }
        UserInputInterruptMonitor::instance().unregisterSession(id);
        m_runSessions.erase(id);
    }

    startFeatureRun(feature, true);
}

void MainWindow::onHotkeyHoldEnded(const QString& featureId) {
    const std::string id = featureId.toStdString();
    FeatureRunSession* session = sessionFor(id);
    if (!session || session->runningMode != FeatureRunMode::Hold) {
        return;
    }

    ++session->holdRepeatGeneration;

    if (!session->holdRunActive) {
        return;
    }

    session->holdRunActive = false;
    session->repeatSession = false;

    Feature* feature = m_project ? m_project->featureById(id) : nullptr;
    releaseHoldHotkeyInputToTarget(*session, feature);

    if (session->engine && session->engine->isRunning()) {
        session->userStopRequested = true;
        session->engine->stop();
        updateRunUiState();
        return;
    }

    session->userStopRequested = false;
    finishRunSession(id, true, QString());
    updateRunUiState();
}

bool MainWindow::shouldSuppressFeatureHotkeyExecution() const {
    if (FeatureHotkeyGate::isFeatureHotkeysBlocked()) {
        return true;
    }
#ifdef _WIN32
    const HWND foreground = GetForegroundWindow();
    if (foreground && IsWindow(foreground)) {
        DWORD pid = 0;
        GetWindowThreadProcessId(foreground, &pid);
        if (pid == GetCurrentProcessId()) {
            return true;
        }
    }
#endif
    return false;
}

void MainWindow::syncHotkeys() {
    PIPBONG_PERF_SCOPE("syncHotkeys");
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
            appendLog(tr("키보드 단축키를 등록하지 못했습니다. 백업 방식으로 다시 시도합니다. "
                         "계속 안 되면 관리자 권한으로 실행하거나 보안 프로그램 예외를 확인하세요."),
                      LogLineKind::Warning);
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
            appendLog(tr("마우스 단축키를 등록하지 못했습니다. 마우스 단축키가 동작하지 않을 수 있습니다."),
                      LogLineKind::Warning);
        }
    }
#endif
    for (const auto& failure : failures) {
        appendLog(tr("단축키 등록 실패 (%1) — 다른 프로그램이 쓰 중이거나 권한이 부족할 수 있습니다.")
                      .arg(QString::fromStdString(failure.featureName)),
                  LogLineKind::Error);
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
    if (isActiveDefaultProfile()) {
        return;
    }
#ifdef _WIN32
    setPersistentStatus(tr("대상 창을 클릭하세요. 우클릭 또는 Esc로 취소"));
    WindowPicker::startPick(this, [this](const WindowPicker::Result& result) {
        setPersistentStatus(QString());
        if (!result.accepted || !result.hwnd) {
            showTransientStatus(tr("창 지정이 취소되었습니다."), 3000);
            return;
        }

        const QString title = QString::fromStdWString(result.title);
        commitActiveProfileTargetWindow(result.hwnd, title);
        appendLog(tr("대상 창을 지정했습니다: %1")
                      .arg(title.isEmpty() ? tr("(제목 없음)") : title),
                  LogLineKind::Success);
        showTransientStatus(tr("대상 창을 지정했습니다."), 3000);
        updateTargetWindowDetails();
        refreshProfileList();
        TargetWindowHighlightOverlay::flashSelectionWave(this);
        scheduleRunWarmup();
        syncTargetWindowCenterPin();
    });
#else
    QMessageBox::information(this, tr("창 지정"), tr("창 지정은 Windows에서만 지원됩니다."));
#endif
}

void MainWindow::onPickTargetWindowFromList() {
    if (isActiveDefaultProfile()) {
        return;
    }
#ifdef _WIN32
    QDialog dialog(this);
    dialog.setWindowTitle(tr("창 목록"));
    dialog.resize(760, 460);

    auto* layout = new QVBoxLayout(&dialog);
    layout->setContentsMargins(12, 12, 12, 12);
    layout->setSpacing(8);

    auto* hintLabel = new QLabel(
        tr("현재 데스크톱에 표시된 최상위 창 목록입니다. 더블클릭하거나 선택 후 확인을 누르세요. "
           "서브 대상 창은 「서브 창으로 지정」을 사용하세요."),
        &dialog);
    hintLabel->setWordWrap(true);
    layout->addWidget(hintLabel);

    auto* legendLabel = new QLabel(&dialog);
    legendLabel->setTextFormat(Qt::RichText);
    legendLabel->setText(windowListBindingLegendHtml(dialog.palette()));
    layout->addWidget(legendLabel);

    auto* listWidget = new QListWidget(&dialog);
    listWidget->setSelectionMode(QAbstractItemView::SingleSelection);
    listWidget->setIconSize(QSize(24, 24));
    layout->addWidget(listWidget, 1);

    auto* refreshButton = new QPushButton(tr("새로고침"), &dialog);
    auto* subPickButton = new QPushButton(tr("서브 창으로 지정"), &dialog);
    subPickButton->setEnabled(false);
    subPickButton->setToolTip(tr("선택한 창을 이 프로필의 서브 대상 창으로 저장합니다."));
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
    buttonRow->addWidget(subPickButton);
    buttonRow->addWidget(buttonBox);
    layout->addLayout(buttonRow);

    const QString mainBinding =
        QString::fromStdString(m_project ? m_project->targetWindowTitle() : std::string{}).trimmed();
    const QString subBinding =
        m_profileManager
            ? m_profileManager->subTargetWindowTitle(m_profileManager->activeProfileId()).trimmed()
            : QString();

    auto populateList = [listWidget, mainBinding, subBinding]() {
        const HWND currentTarget = ScreenCapture::targetWindow();
        const QVector<WindowListEntry> entries = collectWindowListEntries();
        const QPalette listPalette = listWidget->palette();
        listWidget->clear();

        int selectedIndex = -1;
        int subMatchIndex = -1;
        for (int i = 0; i < entries.size(); ++i) {
            const auto& entry = entries[i];
            auto* item = new QListWidgetItem(entry.displayText, listWidget);
            item->setIcon(entry.icon);
            item->setData(Qt::UserRole, QVariant::fromValue(reinterpret_cast<qulonglong>(entry.hwnd)));
            item->setData(Qt::UserRole + 1, QString::fromStdWString(entry.title));

            const QString entryTitle = QString::fromStdWString(entry.title);
            const bool isMainBinding =
                windowTitleMatchesMainBinding(entryTitle, mainBinding)
                || (currentTarget && currentTarget == entry.hwnd);
            const bool isSubBinding =
                windowTitleMatchesSubTarget(entryTitle, mainBinding, subBinding);
            applyWindowListBindingMark(item, listPalette, isMainBinding, isSubBinding);

            if (currentTarget && currentTarget == entry.hwnd) {
                selectedIndex = i;
            }
            if (subMatchIndex < 0 && isSubBinding) {
                subMatchIndex = i;
            }
        }

        if (selectedIndex >= 0) {
            listWidget->setCurrentRow(selectedIndex);
        } else if (subMatchIndex >= 0) {
            listWidget->setCurrentRow(subMatchIndex);
        } else if (listWidget->count() > 0) {
            listWidget->setCurrentRow(0);
        }
    };

    connect(refreshButton, &QPushButton::clicked, &dialog, populateList);
    connect(listWidget, &QListWidget::itemDoubleClicked, &dialog,
            [&dialog](QListWidgetItem*) { dialog.done(WindowListPickMain); });
    connect(listWidget, &QListWidget::currentItemChanged, &dialog,
            [listWidget, okButton, subPickButton](QListWidgetItem* current) {
                const bool hasItem = current != nullptr;
                if (okButton) {
                    okButton->setEnabled(hasItem);
                }
                if (subPickButton) {
                    subPickButton->setEnabled(hasItem);
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
    connect(buttonBox, &QDialogButtonBox::accepted, &dialog, [&dialog]() { dialog.done(WindowListPickMain); });
    connect(buttonBox, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    connect(subPickButton, &QPushButton::clicked, &dialog, [&dialog, listWidget]() {
        if (listWidget->currentItem()) {
            dialog.done(WindowListPickSub);
        }
    });

    struct ListPickerHoverGuard {
        ~ListPickerHoverGuard() { WindowPickerHoverOverlay::dismissAll(); }
    } hoverGuard;

    populateList();
    if (listWidget->count() == 0) {
        QMessageBox::information(this, tr("창 목록"), tr("표시 중인 창을 찾지 못했습니다."));
        return;
    }

    const int pickResult = dialog.exec();
    if (pickResult == WindowListPickCancelled) {
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
    if (pickResult == WindowListPickSub) {
        commitActiveProfileSubTargetWindow(selectedHwnd, title);
        appendLog(tr("서브 대상 창을 지정했습니다: %1")
                      .arg(title.isEmpty() ? tr("(제목 없음)") : title),
                  LogLineKind::Success);
        showTransientStatus(tr("서브 대상 창을 목록에서 지정했습니다."), 3000);
        updateTargetWindowDetails();
        QTimer::singleShot(0, this, [this, selectedHwnd]() {
            TargetWindowHighlightOverlay::flashSelectionWaveForHwnd(selectedHwnd, this);
        });
        return;
    }

    commitActiveProfileTargetWindow(selectedHwnd, title);
    appendLog(tr("대상 창을 지정했습니다: %1").arg(title.isEmpty() ? tr("(제목 없음)") : title),
              LogLineKind::Success);
    showTransientStatus(tr("대상 창을 목록에서 지정했습니다."), 3000);
    updateTargetWindowDetails();
    refreshProfileList();
    QTimer::singleShot(0, this, [this, selectedHwnd]() {
        TargetWindowHighlightOverlay::flashSelectionWaveForHwnd(selectedHwnd, this);
    });
    scheduleRunWarmup();
    syncTargetWindowCenterPin();
#else
    QMessageBox::information(this, tr("창 목록"), tr("창 목록은 Windows에서만 지원됩니다."));
#endif
}

void MainWindow::onShowTargetWindow() {
    if (isActiveDefaultProfile()) {
        return;
    }
#ifdef _WIN32
    if (TargetWindowHighlightOverlay::flash(this)) {
        showTransientStatus(tr("대상 창을 표시했습니다."), 2500);
    }
#else
    QMessageBox::information(this, tr("창 표시"), tr("창 표시는 Windows에서만 지원됩니다."));
#endif
}

void MainWindow::onPinTargetWindowCenterToggled(bool checked) {
    if (isActiveDefaultProfile()) {
        return;
    }
    applyTargetWindowCenterPin(checked);
    saveActiveProfileSettings();
}

void MainWindow::applyTargetWindowCenterPin(bool enabled) {
    ProgramSettings::setPinTargetWindowToScreenCenter(enabled);
    if (!m_targetWindowCenterPinTimer) {
        return;
    }
    if (enabled) {
        const bool alreadyRunning = m_targetWindowCenterPinTimer->isActive();
        if (!alreadyRunning) {
            m_targetWindowCenterPinTimer->start();
        }
        // Defer so profile load can resolve the new target HWND first; avoids a sync on the old window.
        QTimer::singleShot(0, this, [this]() {
            if (ProgramSettings::pinTargetWindowToScreenCenter()) {
                syncTargetWindowCenterPin();
            }
        });
        return;
    }
    m_targetWindowCenterPinTimer->stop();
}

void MainWindow::syncTargetWindowCenterPin() {
#ifdef _WIN32
    if (!ProgramSettings::pinTargetWindowToScreenCenter()) {
        return;
    }
    if (TargetWindowCenterPin::sync()) {
        updateTargetWindowDetails();
    }
#endif
}

void MainWindow::onEngineLog(const QString& message) {
    appendLog(message, logKindForWorkflowMessage(message));
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
    if (!(session->sessionContext && session->sessionContext->suppressRepeatUi())) {
        updateRunUiState();
    }
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

    const bool workerDrivenRepeat =
        session->runningMode == FeatureRunMode::Hold
        || session->runningMode == FeatureRunMode::RepeatInfinite
        || session->runningMode == FeatureRunMode::RepeatCount;

    if (!workerDrivenRepeat) {
        logLoopCompletion(*session, success, message);
        finishRunSession(session->featureId, success, message);
        return;
    }

    if (session->completedLoopCount == 0) {
        logLoopCompletion(*session, success, message);
    } else {
        publishLoopCompletionUi(*session, success, message);
    }

    if (feature && feature->infiniteExitAfterConsecutiveMisses() > 0
        && (session->runningMode == FeatureRunMode::RepeatInfinite
            || session->runningMode == FeatureRunMode::Hold)
        && session->consecutiveDetectionFailLoops >= feature->infiniteExitAfterConsecutiveMisses()) {
        finishRunSession(session->featureId,
                         false,
                         tr("연속 감지 실패 %1회 — 실행 종료")
                             .arg(feature->infiniteExitAfterConsecutiveMisses()));
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
        appendSessionLog(*session,
                         tr("단계 %1 · %2")
                             .arg(index + 1)
                             .arg(summary),
                         LogLineKind::Info);
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
        updateEarlyLoopMouseLockFromMatch(*session);
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
    if (session && session->sessionContext && session->sessionContext->suppressRepeatUi()) {
        return;
    }
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
    if (session->sessionContext && session->sessionContext->suppressRepeatUi()) {
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
                         tr("단계 %1 · %2 · %3")
                             .arg(index + 1)
                             .arg(success ? tr("성공") : tr("실패"))
                             .arg(message),
                         success ? LogLineKind::Success : LogLineKind::Warning);
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
        handleEarlyLoopMouseLockBlockFailure(*session, index);
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

void MainWindow::appendLog(const QString& message, LogLineKind kind) {
    if (m_logPanel) {
        m_logPanel->appendLine(kind, message);
    }
}

bool MainWindow::maybeSave(bool quiet) {
    if (!m_modified) {
        return true;
    }

    m_autoSaveTimer->stop();
    autoSaveProject(quiet);
    return true;
}

void MainWindow::saveActiveProfileSettings() {
    if (!m_profileManager) {
        return;
    }
    const QString profileId = m_profileManager->activeProfileId();
    ProgramSettings::ProfileSettings settings = ProgramSettings::profileSettings();
    settings.linkedTargetProcessPath = m_profileManager->linkedTargetProcessPath(profileId);
    if (m_lastPersistedProfileSettingsValid && profileId == m_lastPersistedProfileSettingsProfileId
        && profileSettingsEqual(settings, m_lastPersistedProfileSettings)) {
        return;
    }
    if (!m_profileManager->saveSettings(profileId, settings)) {
        return;
    }
    m_lastPersistedProfileSettings = settings;
    m_lastPersistedProfileSettingsProfileId = profileId;
    m_lastPersistedProfileSettingsValid = true;
}

bool MainWindow::profileSettingsEqual(const ProgramSettings::ProfileSettings& a,
                                      const ProgramSettings::ProfileSettings& b) const {
    return a.autoSelectRunningFeature == b.autoSelectRunningFeature
           && a.pinTargetWindowToScreenCenter == b.pinTargetWindowToScreenCenter
           && a.imageFindCaptureMode == b.imageFindCaptureMode
           && a.runWithoutTargetWindow == b.runWithoutTargetWindow
           && a.linkedTargetProcessPath == b.linkedTargetProcessPath;
}

void MainWindow::loadActiveProfile(bool quiet) {
    PIPBONG_PERF_SCOPE("loadActiveProfile");
    if (!m_profileManager) {
        return;
    }

    const QString profileId = m_profileManager->activeProfileId();
    ProgramSettings::applyProfileSettings(m_profileManager->loadSettings(profileId));
    m_lastPersistedProfileSettings = ProgramSettings::profileSettings();
    m_lastPersistedProfileSettings.linkedTargetProcessPath =
        m_profileManager->linkedTargetProcessPath(profileId);
    m_lastPersistedProfileSettingsProfileId = profileId;
    m_lastPersistedProfileSettingsValid = true;
    if (profileId == m_profileManager->defaultProfileId() && m_project) {
        m_project->setTargetWindowTitle({});
        ScreenCapture::setTargetWindow(nullptr);
        ScreenCapture::setTargetWindowTitle(L"");
    }
    if (m_pinTargetWindowCenterButton) {
        const QSignalBlocker blocker(m_pinTargetWindowCenterButton);
        m_pinTargetWindowCenterButton->setChecked(ProgramSettings::pinTargetWindowToScreenCenter());
    }
    applyTargetWindowCenterPin(ProgramSettings::pinTargetWindowToScreenCenter());

    const QString projectPath = m_profileManager->activeProjectPath();
    Application::instance()->setProjectDirectory(m_profileManager->activeProjectDirectory());
    if (QFileInfo::exists(projectPath)) {
        loadProjectFromFile(projectPath, quiet);
        Application::instance()->setProjectDirectory(m_profileManager->activeProjectDirectory());
        updateTargetWindowControlsForActiveProfile();
        return;
    }

    prepareProjectUnload();
    m_project = std::make_unique<Project>();
    m_project->addFeature(QStringLiteral("예시 기능").toStdString());
    m_projectFilePath = projectPath;
    ScreenCapture::setTargetWindow(nullptr);
    ScreenCapture::setTargetWindowTitle(L"");
    m_featureList->setProject(m_project.get());
    if (m_profileManager) {
        m_featureList->setActiveProfileId(m_profileManager->activeProfileId());
    }
    onFeatureSelectionChanged();
    m_modified = false;
    updateWindowTitle();
    syncHotkeys();
    if (m_deferTargetDetailsProfileRefresh) {
        QTimer::singleShot(0, this, &MainWindow::updateTargetWindowDetails);
    } else {
        updateTargetWindowDetails();
    }
    updateTargetWindowControlsForActiveProfile();
    autoSaveProject(quiet);
}

void MainWindow::refreshProfileList() {
    if (!m_profileManager || !m_profileList) {
        return;
    }
    m_refreshingProfileList = true;
    m_profileList->clear();
    int activeRow = -1;
    const QString activeId = m_profileManager->activeProfileId();
    const QString defaultId = m_profileManager->defaultProfileId();
    const auto& profiles = m_profileManager->profiles();
#ifdef _WIN32
    bool needsLiveWindowIcons = false;
    for (const ProfileManager::Profile& profile : profiles) {
        if (profile.id == defaultId || profile.targetWindowTitle.isEmpty()) {
            continue;
        }
        if (m_profileManager->linkedTargetProcessPath(profile.id).isEmpty()) {
            needsLiveWindowIcons = true;
            break;
        }
    }
    const QVector<WindowListEntry> windows =
        needsLiveWindowIcons ? collectWindowListEntries() : QVector<WindowListEntry>{};
    const auto resolveProfileIcon = [this, &defaultId, &windows](const ProfileManager::Profile& profile) {
        if (profile.id == defaultId) {
            return windowIcon();
        }
        const QIcon cachedIcon = m_profileManager->linkedTargetIcon(profile.id);
        if (!cachedIcon.isNull()) {
            return cachedIcon;
        }
        const QString storedProcessPath = m_profileManager->linkedTargetProcessPath(profile.id);
        if (!storedProcessPath.isEmpty() && QFileInfo::exists(storedProcessPath)) {
            const QIcon liveIcon = iconForProcessPath(storedProcessPath.toStdWString());
            if (!liveIcon.isNull()) {
                m_profileManager->cacheLinkedTargetIcon(profile.id, liveIcon);
                return liveIcon;
            }
        }
        const QString targetTitle = profile.targetWindowTitle;
        if (!targetTitle.isEmpty()) {
            for (const WindowListEntry& entry : windows) {
                if (!QString::fromStdWString(entry.title).contains(targetTitle, Qt::CaseInsensitive)) {
                    continue;
                }
                if (storedProcessPath.isEmpty() && !entry.processPath.isEmpty()) {
                    m_profileManager->updateProfileTargetBinding(profile.id, targetTitle, entry.processPath);
                }
                if (!entry.icon.isNull()) {
                    m_profileManager->cacheLinkedTargetIcon(profile.id, entry.icon);
                    return entry.icon;
                }
            }
        }
        return windowIcon();
    };
#endif
    m_profileList->setDefaultProfileId(defaultId);
    constexpr int kDefaultProfileRole = Qt::UserRole + 2;
    for (int i = 0; i < static_cast<int>(profiles.size()); ++i) {
        const ProfileManager::Profile& profile = profiles[static_cast<size_t>(i)];
        const bool isDefaultProfile = profile.id == defaultId;
        const QString displayName = isDefaultProfile ? QStringLiteral("기본") : profile.name;
#ifdef _WIN32
        auto* item = new QListWidgetItem(resolveProfileIcon(profile), displayName, m_profileList);
#else
        auto* item = new QListWidgetItem(windowIcon(), displayName, m_profileList);
#endif
        item->setData(Qt::UserRole, profile.id);
        item->setData(kDefaultProfileRole, isDefaultProfile);
        if (isDefaultProfile) {
            item->setFlags(item->flags() & ~Qt::ItemIsDragEnabled);
        }
        const QString targetTitle = isDefaultProfile ? QString() : profile.targetWindowTitle;
        QString targetTooltip;
        if (isDefaultProfile) {
            targetTooltip = tr("시스템 기본 프로필\n이름·순서 변경 불가 · 대상 창 미지정 · 연결된 프로그램이 없을 때 자동 선택");
        } else if (targetTitle.isEmpty()) {
            targetTooltip = tr("대상 창 없음");
        } else {
            const QString storedProcessPath = m_profileManager->linkedTargetProcessPath(profile.id);
            const QString processName =
                storedProcessPath.isEmpty() ? QString() : QFileInfo(storedProcessPath).fileName();
            targetTooltip = processName.isEmpty()
                                ? tr("대상 창: %1").arg(targetTitle)
                                : tr("대상 창: %1\n프로세스: %2").arg(targetTitle, processName);
        }
        item->setToolTip(targetTooltip);
        if (profile.id == activeId) {
            activeRow = i;
        }
    }
    if (activeRow >= 0) {
        m_profileList->setCurrentRow(activeRow);
    }
    if (m_deleteProfileButton) {
        const bool canDeleteSelected = m_profileList->currentItem()
                                       && !m_profileManager->isDefaultProfile(
                                           m_profileList->currentItem()->data(Qt::UserRole).toString());
        m_deleteProfileButton->setEnabled(profiles.size() > 1 && canDeleteSelected);
    }
    updateTargetWindowControlsForActiveProfile();
    m_refreshingProfileList = false;
}

void MainWindow::syncProfileListSelection() {
    if (!m_profileManager || !m_profileList) {
        return;
    }
    const QSignalBlocker blocker(m_profileList);
    const QString activeId = m_profileManager->activeProfileId();
    for (int row = 0; row < m_profileList->count(); ++row) {
        QListWidgetItem* item = m_profileList->item(row);
        if (item && item->data(Qt::UserRole).toString() == activeId) {
            m_profileList->setCurrentRow(row);
            return;
        }
    }
}

bool MainWindow::switchToProfile(const QString& profileId, bool automatic) {
    if (!m_profileManager || profileId.isEmpty()
        || profileId == m_profileManager->activeProfileId()) {
        return false;
    }
    if (m_switchingProfile) {
        m_deferredProfileSwitchId = profileId;
        return false;
    }
    m_switchingProfile = true;
    m_deferTargetDetailsProfileRefresh = automatic;
    struct SwitchGuard {
        MainWindow* window = nullptr;
        ~SwitchGuard() {
            if (!window) {
                return;
            }
            window->m_switchingProfile = false;
            window->m_deferTargetDetailsProfileRefresh = false;
            const QString deferred = window->m_deferredProfileSwitchId;
            if (!deferred.isEmpty()) {
                window->m_deferredProfileSwitchId.clear();
                window->switchToProfile(deferred, true);
            }
        }
    } guard{this};

    PIPBONG_PERF_SCOPE("switchToProfile");

    if (m_profileList) {
        for (int row = 0; row < m_profileList->count(); ++row) {
            QListWidgetItem* item = m_profileList->item(row);
            if (item && item->data(Qt::UserRole).toString() == profileId) {
                const QSignalBlocker blocker(m_profileList);
                m_profileList->setCurrentRow(row);
                break;
            }
        }
    }

    {
        PIPBONG_PERF_SCOPE("switchToProfile.maybeSave");
        if (!maybeSave(true)) {
            syncProfileListSelection();
            return false;
        }
    }
    {
        PIPBONG_PERF_SCOPE("switchToProfile.saveSettings");
        saveActiveProfileSettings();
    }
    {
        PIPBONG_PERF_SCOPE("switchToProfile.stopSessions");
        stopAllSessionsForProfileSwitch();
    }
    if (!m_profileManager->setActiveProfile(profileId)) {
        syncProfileListSelection();
        return false;
    }
    {
        PIPBONG_PERF_SCOPE("switchToProfile.loadActiveProfile");
        loadActiveProfile(true);
    }
    syncMemoDialogProfile();
    syncProfileListSelection();
    scheduleRunWarmup();
    if (!automatic) {
        const ProfileManager::Profile* profile = m_profileManager->activeProfile();
        const QString name = profile
                                 ? (m_profileManager->isDefaultProfile(profile->id)
                                        ? tr("기본")
                                        : profile->name)
                                 : QString();
        showTransientStatus(tr("프로필 전환: %1").arg(name), 1200);
    }
    return true;
}

void MainWindow::syncProfileToForegroundWindow() {
    if (!m_profileManager || m_switchingProfile) {
        return;
    }
#ifdef _WIN32
    HWND hwnd = GetForegroundWindow();
    if (!hwnd || !IsWindow(hwnd)) {
        return;
    }

    DWORD pid = 0;
    GetWindowThreadProcessId(hwnd, &pid);
    // Alt+Tab back to PIPBONG (or any of our dialogs/tray popups): keep the current profile.
    if (pid == GetCurrentProcessId()) {
        m_pendingDefaultProfileSwitchTimer.invalidate();
        return;
    }

    hwnd = GetAncestor(hwnd, GA_ROOT);
    if (!hwnd || !IsWindow(hwnd)) {
        return;
    }
    if (isShellTransientForegroundWindow(hwnd)) {
        return;
    }

    wchar_t titleBuffer[512]{};
    GetWindowTextW(hwnd, titleBuffer, 512);
    const QString foregroundTitle = QString::fromWCharArray(titleBuffer).trimmed();
    const QString targetProfileId = m_profileManager->profileIdForForegroundTitle(foregroundTitle);
    if (targetProfileId == m_profileManager->activeProfileId()) {
        m_pendingDefaultProfileSwitchTimer.invalidate();
        return;
    }

    // Matching a linked-window profile: switch immediately.
    // Falling back to the default profile (unmatched title): require a short stable delay so
    // Alt+Tab / shell overlays do not yank the profile away while returning to PIPBONG.
    const bool fallingBackToDefault =
        !m_profileManager->defaultProfileId().isEmpty()
        && targetProfileId == m_profileManager->defaultProfileId();
    if (fallingBackToDefault) {
        constexpr int kDefaultFallbackStableMs = 400;
        if (!m_pendingDefaultProfileSwitchTimer.isValid()) {
            m_pendingDefaultProfileSwitchTimer.start();
            return;
        }
        if (m_pendingDefaultProfileSwitchTimer.elapsed() < kDefaultFallbackStableMs) {
            return;
        }
    } else {
        m_pendingDefaultProfileSwitchTimer.invalidate();
    }

    ScreenCapture::setForegroundHintWindow(hwnd);
    ScreenCapture::setTargetWindow(hwnd);

    switchToProfile(targetProfileId, true);
#else
    Q_UNUSED(this);
#endif
}

void MainWindow::loadProjectFromFile(const QString& path, bool quiet) {
    PIPBONG_PERF_SCOPE("loadProjectFromFile");
    QString projectDirectory;
    std::unique_ptr<Project> loaded;
    if (m_profileManager) {
        loaded = m_profileManager->cloneCachedProject(m_profileManager->activeProfileId(), path,
                                                     &projectDirectory);
    }
    if (!loaded) {
        loaded = JsonSerializer::loadFromFile(path, &projectDirectory);
        if (!loaded) {
            QMessageBox::critical(this, tr("프로젝트 열기"), tr("프로젝트를 불러오지 못했습니다."));
            return;
        }
        if (m_profileManager) {
            if (auto cacheClone = loaded->clone()) {
                m_profileManager->storeCachedProject(m_profileManager->activeProfileId(),
                                                     path,
                                                     std::move(cacheClone),
                                                     projectDirectory);
            }
        }
    }
    if (projectDirectory.isEmpty() && m_profileManager) {
        projectDirectory = m_profileManager->activeProjectDirectory();
    }

    prepareProjectUnload();
    m_project = std::move(loaded);
    m_projectFilePath = path;
    if (!projectDirectory.isEmpty()) {
        Application::instance()->setProjectDirectory(projectDirectory);
    }

    ScreenCapture::setTargetWindow(nullptr);
    // Prefer the foreground window when it matches this profile's main or sub binding
    // (e.g. auto-switch landed on the launcher); otherwise fall back to the main title.
    syncEffectiveTargetWindowTitleToCapture();
#ifdef _WIN32
    if (HWND hwnd = ScreenCapture::targetWindow(); !hwnd || !IsWindow(hwnd)) {
        ScreenCapture::setTargetWindowTitle(currentTargetWindowTitleW());
        hwnd = ScreenCapture::findTargetWindow();
        if (hwnd) {
            ScreenCapture::setTargetWindow(hwnd);
        }
    }
#endif

    m_featureList->setProject(m_project.get());
    if (m_profileManager) {
        m_featureList->setActiveProfileId(m_profileManager->activeProfileId());
    }
    restoreSelectedFeaturePreference();
    refreshWorkflowEditor();

    m_modified = false;
    updateWindowTitle();
    syncHotkeys();
    if (m_deferTargetDetailsProfileRefresh) {
        QTimer::singleShot(0, this, &MainWindow::updateTargetWindowDetails);
    } else {
        updateTargetWindowDetails();
    }
    updateTargetWindowControlsForActiveProfile();
    if (m_profileManager) {
        m_profileManager->setProfileTargetWindowTitleInMemory(
            m_profileManager->activeProfileId(),
            QString::fromStdString(m_project->targetWindowTitle()));
    }

    QSettings settings;
    settings.setValue(QStringLiteral("project/lastFile"), m_projectFilePath);
    if (!quiet) {
        appendLog(tr("프로젝트 파일을 불러왔습니다: %1").arg(path), LogLineKind::Success);
    }
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

std::wstring MainWindow::resolveEffectiveTargetTitleW() const {
    const std::wstring mainTitle = currentTargetWindowTitleW();
    if (!m_profileManager || isActiveDefaultProfile()) {
        return mainTitle;
    }

    const QString profileId = m_profileManager->activeProfileId();
    const QString subBinding = m_profileManager->subTargetWindowTitle(profileId).trimmed();
    if (subBinding.isEmpty()) {
        return mainTitle;
    }

    const QString mainBinding = QString::fromStdWString(mainTitle).trimmed();

#ifdef _WIN32
    HWND hwnd = GetForegroundWindow();
    if (!hwnd || !IsWindow(hwnd)) {
        return mainTitle;
    }
    hwnd = GetAncestor(hwnd, GA_ROOT);
    if (!hwnd || !IsWindow(hwnd)) {
        return mainTitle;
    }
    wchar_t titleBuffer[512]{};
    GetWindowTextW(hwnd, titleBuffer, 512);
    const QString foregroundTitle = QString::fromWCharArray(titleBuffer).trimmed();
    if (foregroundTitle.isEmpty()) {
        return mainTitle;
    }

    const bool subHit = foregroundTitle.contains(subBinding, Qt::CaseInsensitive);
    const bool mainHit =
        !mainBinding.isEmpty() && foregroundTitle.contains(mainBinding, Qt::CaseInsensitive);
    if (subHit && (!mainHit || subBinding.length() >= mainBinding.length())) {
        return subBinding.toStdWString();
    }
#else
    Q_UNUSED(mainBinding);
#endif
    return mainTitle;
}

void MainWindow::syncTargetWindowTitleToCapture() {
    ScreenCapture::setTargetWindowTitle(currentTargetWindowTitleW());
}

void MainWindow::syncEffectiveTargetWindowTitleToCapture() {
    const std::wstring title = resolveEffectiveTargetTitleW();
    ScreenCapture::setTargetWindowTitle(title);
#ifdef _WIN32
    HWND hwnd = GetForegroundWindow();
    if (!hwnd || !IsWindow(hwnd)) {
        return;
    }
    hwnd = GetAncestor(hwnd, GA_ROOT);
    if (!hwnd || !IsWindow(hwnd)) {
        return;
    }
    wchar_t titleBuffer[512]{};
    GetWindowTextW(hwnd, titleBuffer, 512);
    const QString foregroundTitle = QString::fromWCharArray(titleBuffer).trimmed();
    const QString binding = QString::fromStdWString(title).trimmed();
    if (!binding.isEmpty() && foregroundTitle.contains(binding, Qt::CaseInsensitive)) {
        ScreenCapture::setTargetWindow(hwnd);
        ScreenCapture::setForegroundHintWindow(hwnd);
    }
#endif
}

bool MainWindow::isActiveDefaultProfile() const {
    return m_profileManager
           && m_profileManager->activeProfileId() == m_profileManager->defaultProfileId();
}

#ifdef _WIN32
void MainWindow::commitActiveProfileTargetWindow(HWND hwnd, const QString& title) {
    if (!m_profileManager || !m_project || isActiveDefaultProfile()) {
        return;
    }

    QString processPath;
    if (hwnd && IsWindow(hwnd)) {
        ScreenCapture::TargetWindowInfo info;
        if (ScreenCapture::queryWindowInfo(hwnd, info)) {
            processPath = QString::fromStdWString(info.processPath);
        }
    }

    ScreenCapture::setTargetWindowTitle(title.toStdWString());
    ScreenCapture::setTargetWindow(hwnd);
    m_project->setTargetWindowTitle(title.toStdString());
    const QString profileId = m_profileManager->activeProfileId();
    m_profileManager->updateProfileTargetBinding(profileId, title, processPath);
    QIcon bindIcon = iconForProcessPath(processPath.toStdWString());
    if (bindIcon.isNull() && hwnd && IsWindow(hwnd)) {
        ScreenCapture::TargetWindowInfo info;
        if (ScreenCapture::queryWindowInfo(hwnd, info) && !info.processPath.empty()) {
            bindIcon = iconForProcessPath(info.processPath);
        }
    }
    if (!bindIcon.isNull()) {
        m_profileManager->cacheLinkedTargetIcon(profileId, bindIcon);
    }
    scheduleAutoSave();
}

void MainWindow::commitActiveProfileSubTargetWindow(HWND hwnd, const QString& title) {
    if (!m_profileManager || isActiveDefaultProfile()) {
        return;
    }

    QString processPath;
    if (hwnd && IsWindow(hwnd)) {
        ScreenCapture::TargetWindowInfo info;
        if (ScreenCapture::queryWindowInfo(hwnd, info)) {
            processPath = QString::fromStdWString(info.processPath);
        }
    }

    const QString profileId = m_profileManager->activeProfileId();
    m_profileManager->updateProfileSubTargetBinding(profileId, title, processPath);
}
#endif

void MainWindow::updateTargetWindowControlsForActiveProfile() {
    const bool lockedDefault = isActiveDefaultProfile();
    const QString lockedTooltip = tr("전역 기본 프로필은 대상 창을 지정할 수 없습니다.");

    if (m_pickWindowButton) {
        m_pickWindowButton->setEnabled(!lockedDefault);
        m_pickWindowButton->setToolTip(lockedDefault
                                           ? lockedTooltip
                                           : tr("클릭한 뒤 대상 창을 눌러 지정합니다. 우클릭 또는 Esc로 취소."));
    }
    if (m_pickWindowListButton) {
        m_pickWindowListButton->setEnabled(!lockedDefault);
        m_pickWindowListButton->setToolTip(lockedDefault
                                               ? lockedTooltip
                                               : tr("표시 중인 창 목록에서 주 대상 창을 선택하거나 서브 대상 창으로 지정합니다."));
    }
    if (m_showTargetWindowButton) {
        m_showTargetWindowButton->setEnabled(!lockedDefault);
        m_showTargetWindowButton->setToolTip(lockedDefault
                                                 ? lockedTooltip
                                                 : tr("지정된 대상 창 테두리를 잠시 깜빡여 표시합니다."));
    }
    if (m_pinTargetWindowCenterButton) {
#ifdef _WIN32
        m_pinTargetWindowCenterButton->setEnabled(!lockedDefault);
        m_pinTargetWindowCenterButton->setToolTip(
            lockedDefault ? lockedTooltip
                          : tr("지정된 대상 창이 현재 모니터 중앙에 유지되도록 위치를 자동으로 맞춥니다."));
#else
        m_pinTargetWindowCenterButton->setEnabled(false);
#endif
    }
}

void MainWindow::updateTargetWindowDetails() {
    if (!m_targetWindowDetailPanel) {
        return;
    }

    if (isActiveDefaultProfile()) {
        m_targetWindowDetailPanel->showGlobalDefaultProfile();
        return;
    }

#ifdef _WIN32
    const QString savedTitle =
        QString::fromStdString(m_project ? m_project->targetWindowTitle() : std::string{}).trimmed();
    const QString subBinding =
        m_profileManager
            ? m_profileManager->subTargetWindowTitle(m_profileManager->activeProfileId()).trimmed()
            : QString();
    syncTargetWindowTitleToCapture();

    HWND hwnd = ScreenCapture::targetWindow();
    if (!hwnd || !IsWindow(hwnd)) {
        hwnd = ScreenCapture::findTargetWindow();
        if (hwnd) {
            ScreenCapture::setTargetWindow(hwnd);
        }
    }

    if (!subBinding.isEmpty()) {
        HWND fg = GetForegroundWindow();
        if (fg && IsWindow(fg)) {
            fg = GetAncestor(fg, GA_ROOT);
        }
        if (fg && IsWindow(fg)) {
            wchar_t fgTitleBuffer[512]{};
            GetWindowTextW(fg, fgTitleBuffer, 512);
            const QString fgTitle = QString::fromWCharArray(fgTitleBuffer).trimmed();
            if (windowTitleMatchesSubTarget(fgTitle, savedTitle, subBinding)) {
                hwnd = fg;
            }
        }
    }

    if (!hwnd || !IsWindow(hwnd)) {
        if (savedTitle.isEmpty()) {
            m_targetWindowDetailPanel->showMessage(tr("'창 지정'으로 대상 창을 선택하세요."));
        } else {
            QString processPath;
            if (m_profileManager) {
                processPath = m_profileManager->linkedTargetProcessPath(m_profileManager->activeProfileId());
            }
            const QString processName =
                processPath.isEmpty() ? QString() : QFileInfo(processPath).fileName();
            m_targetWindowDetailPanel->showStoredTargetBinding(savedTitle, processName, processPath);
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

    // Persist exe path once resolved so profile list icons survive settings saves
    // and restarts (even if an earlier save wiped linkedTargetProcessPath).
    const bool isSubTarget =
        windowTitleMatchesSubTarget(title, savedTitle, subBinding);
    if (m_profileManager && !processPath.isEmpty() && !isActiveDefaultProfile()) {
        const QString profileId = m_profileManager->activeProfileId();
        if (isSubTarget) {
            const QString storedSubPath = m_profileManager->subLinkedTargetProcessPath(profileId);
            if (storedSubPath.compare(processPath, Qt::CaseInsensitive) != 0) {
                m_profileManager->updateProfileSubTargetBinding(
                    profileId, subBinding.isEmpty() ? title : subBinding, processPath);
            }
        } else {
            const QString storedPath = m_profileManager->linkedTargetProcessPath(profileId);
            if (storedPath.compare(processPath, Qt::CaseInsensitive) != 0) {
                m_profileManager->updateProfileTargetBinding(profileId,
                                                             savedTitle.isEmpty() ? title : savedTitle,
                                                             processPath);
                if (!m_deferTargetDetailsProfileRefresh) {
                    refreshProfileList();
                }
            }
        }
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
    data.subTarget = isSubTarget;
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
    if (!feature || !session.sessionContext) {
        monitor.unregisterSession(session.featureId);
        return;
    }
    if (feature->userInputInterruptMode() == UserInputInterruptMode::None) {
        monitor.unregisterSession(session.featureId);
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
        ++session->holdRepeatGeneration;
        session->sessionContext->requestStop();
        if (session->engine) {
            session->engine->stop();
        }
        appendSessionLog(*session, tr("사용자 입력으로 실행을 멈췄습니다"), LogLineKind::Warning);
        showTransientStatus(
            tr("[%1] 사용자 입력 — 정지").arg(featureDisplayName(featureId)), 3000);
        return;
    }

    if (mode == UserInputInterruptMode::Pause) {
        session->sessionContext->togglePaused();
        const bool paused = session->sessionContext->isPaused();
        appendSessionLog(*session,
                         paused ? tr("사용자 입력으로 잠시 멈춤") : tr("사용자 입력으로 다시 실행"),
                         paused ? LogLineKind::Warning : LogLineKind::Success);
        updateRunUiState();
    }
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

MainWindow::GlobalUiHistorySnapshot MainWindow::createGlobalUiSnapshot(const QString& reason) const {
    GlobalUiHistorySnapshot snapshot;
    const QString tempRoot =
        QDir(QStandardPaths::writableLocation(QStandardPaths::TempLocation))
            .filePath(QStringLiteral("pipbong-global-history"));
    const QString snapshotId = QUuid::createUuid().toString(QUuid::WithoutBraces);
    const QString backupRoot = QDir(tempRoot).filePath(snapshotId);
    const QString profilesSourceDir =
        m_profileManager ? m_profileManager->profilesDirectory()
                         : QDir(Application::dataDirectory()).filePath(QStringLiteral("profiles"));
    const QString librarySourceDir =
        QDir(Application::dataDirectory()).filePath(QStringLiteral("featureLibrary"));
    const QString profilesBackupDir = QDir(backupRoot).filePath(QStringLiteral("profiles"));
    const QString libraryBackupDir = QDir(backupRoot).filePath(QStringLiteral("featureLibrary"));

    bool ok = QDir().mkpath(backupRoot);
    if (!ok) {
        return {};
    }
    if (QDir(profilesSourceDir).exists()) {
        ok = copyDirectoryRecursive(profilesSourceDir, profilesBackupDir);
    }
    if (ok && QDir(librarySourceDir).exists()) {
        ok = copyDirectoryRecursive(librarySourceDir, libraryBackupDir);
    }
    if (!ok) {
        removeDirectoryRecursive(backupRoot);
        return {};
    }

    snapshot.backupRootPath = backupRoot;
    snapshot.reason = reason;
    return snapshot;
}

bool MainWindow::copyDirectoryRecursive(const QString& sourceDir, const QString& targetDir) {
    QDir source(sourceDir);
    if (!source.exists()) {
        return true;
    }
    if (!QDir().mkpath(targetDir)) {
        return false;
    }
    const QFileInfoList entries =
        source.entryInfoList(QDir::NoDotAndDotDot | QDir::Files | QDir::Dirs | QDir::Hidden | QDir::System);
    for (const QFileInfo& entry : entries) {
        const QString sourcePath = entry.absoluteFilePath();
        const QString targetPath = QDir(targetDir).filePath(entry.fileName());
        if (entry.isDir()) {
            if (!copyDirectoryRecursive(sourcePath, targetPath)) {
                return false;
            }
            continue;
        }
        if (QFileInfo::exists(targetPath) && !QFile::remove(targetPath)) {
            return false;
        }
        if (!QFile::copy(sourcePath, targetPath)) {
            return false;
        }
    }
    return true;
}

void MainWindow::removeDirectoryRecursive(const QString& path) {
    if (path.isEmpty()) {
        return;
    }
    QDir dir(path);
    if (!dir.exists()) {
        return;
    }
    dir.removeRecursively();
}

void MainWindow::clearGlobalUiRedoHistory() {
    for (const GlobalUiHistorySnapshot& snapshot : m_globalUiRedoHistory) {
        removeDirectoryRecursive(snapshot.backupRootPath);
    }
    m_globalUiRedoHistory.clear();
}

void MainWindow::clearGlobalUiUndoHistory() {
    for (const GlobalUiHistorySnapshot& snapshot : m_globalUiUndoHistory) {
        removeDirectoryRecursive(snapshot.backupRootPath);
    }
    m_globalUiUndoHistory.clear();
}

bool MainWindow::pushGlobalUiUndoSnapshot(const QString& reason) {
    if (m_restoringGlobalUiHistory) {
        return false;
    }
    const GlobalUiHistorySnapshot snapshot = createGlobalUiSnapshot(reason);
    if (snapshot.backupRootPath.isEmpty()) {
        return false;
    }
    m_globalUiUndoHistory.push_back(snapshot);
    constexpr int kMaxGlobalUiHistoryDepth = 20;
    if (static_cast<int>(m_globalUiUndoHistory.size()) > kMaxGlobalUiHistoryDepth) {
        removeDirectoryRecursive(m_globalUiUndoHistory.front().backupRootPath);
        m_globalUiUndoHistory.erase(m_globalUiUndoHistory.begin());
    }
    clearGlobalUiRedoHistory();
    return true;
}

bool MainWindow::restoreGlobalUiSnapshot(const GlobalUiHistorySnapshot& snapshot) {
    if (snapshot.backupRootPath.isEmpty() || !m_profileManager) {
        return false;
    }

    const QString profilesSourceDir = QDir(snapshot.backupRootPath).filePath(QStringLiteral("profiles"));
    const QString librarySourceDir = QDir(snapshot.backupRootPath).filePath(QStringLiteral("featureLibrary"));
    const QString profilesTargetDir = m_profileManager->profilesDirectory();
    const QString libraryTargetDir =
        QDir(Application::dataDirectory()).filePath(QStringLiteral("featureLibrary"));

    stopAllSessions();
    maybeSave(true);

    struct RestoreGuard {
        MainWindow* window = nullptr;
        ~RestoreGuard() {
            if (window) {
                window->m_restoringGlobalUiHistory = false;
            }
        }
    } guard{this};
    m_restoringGlobalUiHistory = true;

    removeDirectoryRecursive(profilesTargetDir);
    removeDirectoryRecursive(libraryTargetDir);
    if (QDir(profilesSourceDir).exists() && !copyDirectoryRecursive(profilesSourceDir, profilesTargetDir)) {
        return false;
    }
    if (QDir(librarySourceDir).exists() && !copyDirectoryRecursive(librarySourceDir, libraryTargetDir)) {
        return false;
    }

    m_profileManager->initialize();
    refreshProfileList();
    loadActiveProfile(true);
    refreshFeatureLibraryPanel();
    syncProfileListSelection();
    showTransientStatus(tr("실행 취소/다시 실행 적용됨"), 1200);
    return true;
}

void MainWindow::onGlobalUndoRequested() {
    if (m_globalUiUndoHistory.empty()) {
        return;
    }
    const GlobalUiHistorySnapshot redoSnapshot = createGlobalUiSnapshot(QStringLiteral("redo"));
    if (redoSnapshot.backupRootPath.isEmpty()) {
        return;
    }
    const GlobalUiHistorySnapshot target = m_globalUiUndoHistory.back();
    m_globalUiUndoHistory.pop_back();
    if (!restoreGlobalUiSnapshot(target)) {
        removeDirectoryRecursive(redoSnapshot.backupRootPath);
        return;
    }
    m_globalUiRedoHistory.push_back(redoSnapshot);
    constexpr int kMaxGlobalUiHistoryDepth = 20;
    if (static_cast<int>(m_globalUiRedoHistory.size()) > kMaxGlobalUiHistoryDepth) {
        removeDirectoryRecursive(m_globalUiRedoHistory.front().backupRootPath);
        m_globalUiRedoHistory.erase(m_globalUiRedoHistory.begin());
    }
    removeDirectoryRecursive(target.backupRootPath);
}

void MainWindow::onGlobalRedoRequested() {
    if (m_globalUiRedoHistory.empty()) {
        return;
    }
    const GlobalUiHistorySnapshot undoSnapshot = createGlobalUiSnapshot(QStringLiteral("undo"));
    if (undoSnapshot.backupRootPath.isEmpty()) {
        return;
    }
    const GlobalUiHistorySnapshot target = m_globalUiRedoHistory.back();
    m_globalUiRedoHistory.pop_back();
    if (!restoreGlobalUiSnapshot(target)) {
        removeDirectoryRecursive(undoSnapshot.backupRootPath);
        return;
    }
    m_globalUiUndoHistory.push_back(undoSnapshot);
    constexpr int kMaxGlobalUiHistoryDepth = 20;
    if (static_cast<int>(m_globalUiUndoHistory.size()) > kMaxGlobalUiHistoryDepth) {
        removeDirectoryRecursive(m_globalUiUndoHistory.front().backupRootPath);
        m_globalUiUndoHistory.erase(m_globalUiUndoHistory.begin());
    }
    removeDirectoryRecursive(target.backupRootPath);
}
