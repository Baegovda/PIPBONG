#include "ui/MainWorkspaceView.h"
#include "app/ProgramSettings.h"
#include "ui/UiAppearance.h"
#include "ui/UiResizeHandle.h"
#include "ui/UiStateManager.h"
#include "ui/CustomTitleBar.h"
#include "ui/ProfileListWidget.h"
#include "ui/FeatureListPanel.h"
#include "ui/WorkflowEditorPanel.h"
#include "ui/BlockListWidget.h"
#include "ui/LogPanelWidget.h"
#include "ui/TargetWindowDetailPanel.h"
#include <QMainWindow>
#include <QCoreApplication>
#include <QCheckBox>
#include <QGroupBox>
#include <QLabel>
#include <QPushButton>
#include <QToolButton>
#include <QSplitter>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QStatusBar>
#include <QSettings>
#include <QSignalBlocker>

namespace {
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

void configureBottomAuxiliaryToggleButton(QPushButton* button) {
    if (!button) {
        return;
    }
    button->setCheckable(true);
    button->setProperty("class", QStringLiteral("bottomAuxiliaryToggleButton"));
    button->setCursor(Qt::PointingHandCursor);
    button->setFocusPolicy(Qt::NoFocus);
}

}
MainWorkspaceView::MainWorkspaceView(QMainWindow& window):QObject(&window),m_window(&window){build();}
MainWorkspaceView::~MainWorkspaceView(){delete m_uiState;}
void MainWorkspaceView::clampVertical(){clampMainVerticalSplitterSizesImpl(m_mainVerticalSplitter);}

void MainWorkspaceView::build() {
    m_window->setWindowTitle(QStringLiteral("PIPBONG %1").arg(QCoreApplication::applicationVersion()));
    m_window->setWindowFlag(Qt::FramelessWindowHint, true);
    m_window->resize(1280, 820);

    auto* central = UiAppearance::createWorkspaceSurface(m_window);
    auto* outerLayout = new QVBoxLayout(central);
    outerLayout->setContentsMargins(0, 0, 0, 0);
    outerLayout->setSpacing(0);

    m_titleBar = new CustomTitleBar(m_window);
    outerLayout->addWidget(m_titleBar);

    m_alwaysOnTopCheck = new QCheckBox(tr("항상 위"), m_titleBar);
    m_titleBar->setAlwaysOnTopCheckBox(m_alwaysOnTopCheck);

    auto* contentHost = new QWidget(central);
    auto* contentLayout = new QVBoxLayout(contentHost);
    contentLayout->setContentsMargins(10, 8, 10, 10);
    contentLayout->setSpacing(0);

    m_mainHorizontalSplitter = new QSplitter(Qt::Horizontal, contentHost);
    UiResizeHandle::configureSplitter(m_mainHorizontalSplitter);
    auto* profileGroup = new QGroupBox(tr("프로필"), m_mainHorizontalSplitter);
    profileGroup->setObjectName(QStringLiteral("profileListGroup"));
    profileGroup->setStyleSheet(UiAppearance::glassPanelStyle(QStringLiteral("QGroupBox#profileListGroup")) + QStringLiteral(
        "QGroupBox#profileListGroup {"
        "  margin-top: 14px;"
        "  padding-top: 6px;"
        "}"
        "QGroupBox#profileListGroup::title {"
        "  subcontrol-origin: margin;"
        "  subcontrol-position: top left;"
        "  left: 4px;"
        "  padding: 0 4px;"
        "}"));
    auto* profileLayout = new QVBoxLayout(profileGroup);
    profileLayout->setContentsMargins(7, 5, 7, 7);
    profileLayout->setSpacing(4);
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
    m_addProfileButton->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Fixed);
    m_renameProfileButton->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Fixed);
    m_deleteProfileButton->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Fixed);
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
    m_mainHorizontalSplitter->setSizes({130, 300, 850});

    auto* bottomPanel = new QWidget(contentHost);
    bottomPanel->setMinimumHeight(kMinBottomPanelPx);
    auto* bottomLayout = new QVBoxLayout(bottomPanel);
    bottomLayout->setContentsMargins(4, 2, 4, 4);
    bottomLayout->setSpacing(4);

    auto* targetGroup = new QGroupBox(tr("타겟"), bottomPanel);
    targetGroup->setMinimumHeight(108);
    targetGroup->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    auto* targetLayout = new QVBoxLayout(targetGroup);
    targetLayout->setContentsMargins(4, 2, 4, 4);
    targetLayout->setSpacing(2);

    m_targetWindowDetailPanel = new TargetWindowDetailPanel(targetGroup);

    auto* actionBar = new QWidget(targetGroup);
    actionBar->setObjectName(QStringLiteral("targetWindowActionBar"));
    auto* actionOuter = new QVBoxLayout(actionBar);
    actionOuter->setContentsMargins(0, 0, 0, 0);
    actionOuter->setSpacing(2);

    auto* mainRow = new QWidget(actionBar);
    auto* actionLayout = new QHBoxLayout(mainRow);
    actionLayout->setContentsMargins(0, 0, 0, 0);
    actionLayout->setSpacing(3);

    auto* mainCaption = new QLabel(tr("메인 창"), mainRow);
    mainCaption->setObjectName(QStringLiteral("targetWindowGroupCaption"));
    mainCaption->setMinimumWidth(48);
    mainCaption->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);

    m_pickWindowButton = new QToolButton(mainRow);
    m_pickWindowButton->setText(tr("지정"));
    m_pickWindowButton->setToolTip(
        tr("클릭한 뒤 메인 창을 눌러 지정합니다. 마우스를 올리면 초록색 테두리가 표시됩니다. 우클릭 또는 Esc로 취소."));
    configureTargetWindowActionButton(m_pickWindowButton);

    m_pickWindowListButton = new QToolButton(mainRow);
    m_pickWindowListButton->setText(tr("메인 목록"));
    m_pickWindowListButton->setToolTip(
        tr("메인 창 목록에서 선택합니다. 항목을 고르면 해당 창에 테두리 애니메이션이 표시됩니다."));
    configureTargetWindowActionButton(m_pickWindowListButton);

    m_showTargetWindowButton = new QToolButton(mainRow);
    m_showTargetWindowButton->setText(tr("표시"));
    m_showTargetWindowButton->setToolTip(tr("지정된 메인 창 테두리를 잠시 깜빡여 표시합니다(초록색)."));
    configureTargetWindowActionButton(m_showTargetWindowButton);

    m_pinTargetWindowCenterButton = new QToolButton(mainRow);
    m_pinTargetWindowCenterButton->setObjectName(QStringLiteral("targetPinCenterButton"));
    m_pinTargetWindowCenterButton->setText(tr("중앙 고정"));
    m_pinTargetWindowCenterButton->setToolTip(
        tr("메인 창을 드래그해도 현재 모니터 중앙으로 자동 복귀합니다. 트리거 감시 중에도 동작합니다."));
    m_pinTargetWindowCenterButton->setCheckable(true);
    m_pinTargetWindowCenterButton->setChecked(ProgramSettings::pinTargetWindowToScreenCenter());
    configureTargetWindowActionButton(m_pinTargetWindowCenterButton);
#ifndef _WIN32
    m_pinTargetWindowCenterButton->setEnabled(false);
    m_pinTargetWindowCenterButton->setToolTip(tr("화면 중앙 고정은 Windows에서만 지원됩니다."));
#endif

    actionLayout->addWidget(mainCaption);
    actionLayout->addWidget(m_pickWindowButton);
    actionLayout->addWidget(m_pickWindowListButton);
    actionLayout->addWidget(m_showTargetWindowButton);
    actionLayout->addStretch();
    actionLayout->addWidget(m_pinTargetWindowCenterButton);

    auto* subRow = new QWidget(actionBar);
    auto* subLayout = new QHBoxLayout(subRow);
    subLayout->setContentsMargins(0, 0, 0, 0);
    subLayout->setSpacing(3);

    auto* subCaption = new QLabel(tr("서브 창"), subRow);
    subCaption->setObjectName(QStringLiteral("targetWindowGroupCaption"));
    subCaption->setMinimumWidth(48);
    subCaption->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);

    m_pickSubWindowButton = new QToolButton(subRow);
    m_pickSubWindowButton->setText(tr("지정"));
    m_pickSubWindowButton->setToolTip(
        tr("클릭한 뒤 서브 창을 눌러 지정합니다. 마우스를 올리면 파란색 테두리가 표시됩니다. 우클릭 또는 Esc로 취소."));
    configureTargetWindowActionButton(m_pickSubWindowButton);

    m_pickSubWindowListButton = new QToolButton(subRow);
    m_pickSubWindowListButton->setText(tr("서브 목록"));
    m_pickSubWindowListButton->setToolTip(
        tr("서브 창 목록에서 선택합니다. 항목을 고르면 파란색 테두리 애니메이션이 표시됩니다."));
    configureTargetWindowActionButton(m_pickSubWindowListButton);

    m_showSubTargetWindowButton = new QToolButton(subRow);
    m_showSubTargetWindowButton->setText(tr("표시"));
    m_showSubTargetWindowButton->setToolTip(tr("지정된 서브 창 테두리를 잠시 깜빡여 표시합니다(파란색)."));
    configureTargetWindowActionButton(m_showSubTargetWindowButton);

    m_pinSubTargetWindowCenterButton = new QToolButton(subRow);
    m_pinSubTargetWindowCenterButton->setObjectName(QStringLiteral("targetPinSubCenterButton"));
    m_pinSubTargetWindowCenterButton->setText(tr("중앙 고정"));
    m_pinSubTargetWindowCenterButton->setToolTip(
        tr("서브 창을 드래그해도 현재 모니터 중앙으로 자동 복귀합니다. 트리거 감시 중에도 동작합니다."));
    m_pinSubTargetWindowCenterButton->setCheckable(true);
    m_pinSubTargetWindowCenterButton->setChecked(ProgramSettings::pinSubTargetWindowToScreenCenter());
    configureTargetWindowActionButton(m_pinSubTargetWindowCenterButton);
#ifndef _WIN32
    m_pickSubWindowButton->setEnabled(false);
    m_pickSubWindowButton->setToolTip(tr("타겟 지정은 Windows에서만 지원됩니다."));
    m_pickSubWindowListButton->setEnabled(false);
    m_pickSubWindowListButton->setToolTip(tr("서브 창 목록은 Windows에서만 지원됩니다."));
    m_showSubTargetWindowButton->setEnabled(false);
    m_showSubTargetWindowButton->setToolTip(tr("창 표시는 Windows에서만 지원됩니다."));
    m_pinSubTargetWindowCenterButton->setEnabled(false);
    m_pinSubTargetWindowCenterButton->setToolTip(tr("화면 중앙 고정은 Windows에서만 지원됩니다."));
#endif

    subLayout->addWidget(subCaption);
    subLayout->addWidget(m_pickSubWindowButton);
    subLayout->addWidget(m_pickSubWindowListButton);
    subLayout->addWidget(m_showSubTargetWindowButton);
    subLayout->addStretch();
    subLayout->addWidget(m_pinSubTargetWindowCenterButton);

    actionOuter->addWidget(mainRow);
    actionOuter->addWidget(subRow);

    targetGroup->setStyleSheet(QStringLiteral(
        "QGroupBox {"
        "  border: none;"
        "  margin-top: 14px;"
        "  padding-top: 6px;"
        "  font-size: 11px;"
        "}"
        "QGroupBox::title {"
        "  subcontrol-origin: margin;"
        "  left: 2px;"
        "  padding: 0 2px;"
        "}"
        "QLabel#targetWindowGroupCaption {"
        "  color: palette(mid);"
        "  font-size: 10px;"
        "  font-weight: 600;"
        "  padding-right: 2px;"
        "}"
        "QToolButton.targetWindowActionButton {"
        "  padding: 1px 6px;"
        "  border: none;"
        "  border-radius: 3px;"
        "  background-color: transparent;"
        "  color: palette(button-text);"
        "  font-size: 11px;"
        "}"
        "QToolButton.targetWindowActionButton:hover:!checked {"
        "  background-color: palette(light);"
        "}"
        "QToolButton.targetWindowActionButton:pressed:!checked {"
        "  background-color: palette(midlight);"
        "}"
        "QToolButton.targetWindowActionButton:checked {"
        "  background-color: palette(highlight);"
        "  color: palette(highlighted-text);"
        "}"
        "QToolButton.targetWindowActionButton:disabled {"
        "  color: palette(mid);"
        "  background-color: transparent;"
        "}"));

    targetLayout->addWidget(actionBar);
    targetLayout->addWidget(m_targetWindowDetailPanel, 1);

    m_exitButton = new QPushButton(tr("종료"), bottomPanel);
    m_updateButton = new QPushButton(bottomPanel);
    m_updateButton->setToolTip(tr("GitHub 릴리즈에서 업데이트를 확인합니다."));
    m_calculatorButton = new QPushButton(tr("계산기"), bottomPanel);
    m_calculatorButton->setToolTip(tr("poe.ninja 시세 계산기"));
    configureBottomAuxiliaryToggleButton(m_calculatorButton);
    m_spikeWatchButton = new QPushButton(tr("CPU 감시"), bottomPanel);
    m_spikeWatchButton->setToolTip(tr("CPU 사용률 스파이크 진단"));
    configureBottomAuxiliaryToggleButton(m_spikeWatchButton);
    m_memoButton = new QPushButton(tr("메모장"), bottomPanel);
    m_memoButton->setToolTip(tr("프로필별 메모"));
    configureBottomAuxiliaryToggleButton(m_memoButton);
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
    bottomPanel->setStyleSheet(QStringLiteral(
        "QPushButton.bottomAuxiliaryToggleButton {"
        "  padding: 2px 8px;"
        "  border: none;"
        "  border-radius: 3px;"
        "  background-color: transparent;"
        "  color: palette(button-text);"
        "}"
        "QPushButton.bottomAuxiliaryToggleButton:hover:!checked {"
        "  background-color: palette(light);"
        "}"
        "QPushButton.bottomAuxiliaryToggleButton:pressed:!checked {"
        "  background-color: palette(midlight);"
        "}"
        "QPushButton.bottomAuxiliaryToggleButton:checked {"
        "  background-color: palette(highlight);"
        "  color: palette(highlighted-text);"
        "}"));

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
        clampVertical();
    });

    contentLayout->addWidget(m_mainVerticalSplitter, 1);
    outerLayout->addWidget(contentHost, 1);

    m_window->setCentralWidget(central);

    m_window->statusBar()->hide();
}

UiStateManager* MainWorkspaceView::restoreState() {
    if(m_uiState)return m_uiState;
    m_uiState = new UiStateManager(this);
    m_uiState->registerMainWindow(m_window);
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
    clampVertical();
    return m_uiState;
}

