#include "ui/ProgramSettingsDialog.h"

#include "app/PointerFeedbackSettings.h"
#include "app/ProgramSettings.h"
#include "core/diagnostics/CursorStutterProfiler.h"
#include "app/WindowsRunAsAdmin.h"
#include "ui/ClickPointerFeedbackSettingsDialog.h"
#include "ui/WindowSelectionFeedbackSettingsDialog.h"
#include "ui/UiStrings.h"
#include "ui/UiThemeColors.h"
#include "ui/widgets/DragAdjustSpinBox.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>

namespace {

void applyOptionToolTip(QWidget* widget, const QString& toolTip) {
    widget->setToolTip(toolTip);
}

QCheckBox* addGroupCheck(QVBoxLayout* groupLayout,
                         QWidget* parent,
                         const QString& label,
                         const QString& toolTip,
                         bool checked) {
    auto* check = new QCheckBox(label, parent);
    check->setChecked(checked);
    applyOptionToolTip(check, toolTip);
    groupLayout->addWidget(check);
    return check;
}

QGroupBox* addSettingsGroup(QVBoxLayout* rootLayout, const QString& title, const QString& toolTip) {
    auto* group = new QGroupBox(title);
    applyOptionToolTip(group, toolTip);
    rootLayout->addWidget(group);
    return group;
}

} // namespace

ProgramSettingsDialog::ProgramSettingsDialog(QWidget* parent)
    : QDialog(parent) {
    setWindowTitle(tr("프로그램 설정"));
    setModal(true);
    resize(480, 520);
    setupUi();
}

void ProgramSettingsDialog::setupUi() {
    auto* layout = new QVBoxLayout(this);
    layout->setSpacing(10);

    auto* intro = new QLabel(
        tr("옵션 위에 마우스를 올리면 자세한 설명이 표시됩니다. "
           "일부 항목은 현재 프로필에, 나머지는 전체 프로그램에 적용됩니다."),
        this);
    intro->setWordWrap(true);
    {
        QPalette pal = intro->palette();
        pal.setColor(QPalette::WindowText, secondaryHintTextColor(pal));
        intro->setPalette(pal);
    }
    layout->addWidget(intro);

    // --- 기능 실행 ---
    auto* runGroup = addSettingsGroup(
        layout,
        tr("기능 실행"),
        tr("기능을 실행할 때 워크플로·타겟·로그와 관련된 동작입니다."));
    auto* runLayout = new QVBoxLayout(runGroup);
    runLayout->setSpacing(6);

    m_autoSelectRunningFeatureCheck = addGroupCheck(
        runLayout,
        runGroup,
        tr("실행 중인 기능을 워크플로에 표시"),
        tr("단축키나 실행으로 기능이 켜지면 오른쪽 워크플로 패널에 그 기능을 보여 줍니다."),
        ProgramSettings::autoSelectRunningFeature());

    m_focusTargetWindowOnProfileSelectCheck = addGroupCheck(
        runLayout,
        runGroup,
        tr("프로필 전환 시 타겟으로 이동"),
        tr("프로필을 바꿀 때 연결된 게임·프로그램 창을 앞으로 가져옵니다. "
           "끄면 PIPBONG 화면에 머무른 채로 프로필만 바뀝니다."),
        ProgramSettings::focusTargetWindowOnProfileSelect());

    m_runWithoutTargetWindowCheck = addGroupCheck(
        runLayout,
        runGroup,
        tr("타겟 없이도 실행 허용"),
        tr("타겟 지정 없이 키보드·딜레이·텍스트 블록을 실행할 수 있습니다. "
           "템플릿 매칭은 전체 화면을 봅니다. 끄면 실행 전에 타겟 지정이 필요합니다."),
        ProgramSettings::runWithoutTargetWindow());

    auto* logRow = new QHBoxLayout();
    auto* logLabel = new QLabel(tr("실행 로그 최대 줄 수"), runGroup);
    applyOptionToolTip(
        logLabel,
        tr("로그가 이 줄 수를 넘으면 오래된 줄부터 지웁니다. (100~10000, 기본 1000)"));
    logRow->addWidget(logLabel);
    m_logMaxLinesSpin = new DragAdjustSpinBox(runGroup);
    m_logMaxLinesSpin->setRange(ProgramSettings::kMinLogMaxLines, ProgramSettings::kMaxLogMaxLines);
    m_logMaxLinesSpin->setSingleStep(50);
    m_logMaxLinesSpin->setValue(ProgramSettings::logMaxLines());
    m_logMaxLinesSpin->setMinimumWidth(96);
    applyOptionToolTip(m_logMaxLinesSpin, logLabel->toolTip());
    logRow->addWidget(m_logMaxLinesSpin);
    auto* logUnit = new QLabel(tr("줄"), runGroup);
    logRow->addWidget(logUnit);
    logRow->addStretch();
    runLayout->addLayout(logRow);

    // --- 시작·종료 ---
    auto* lifecycleGroup = addSettingsGroup(
        layout,
        tr("시작·종료"),
        tr("Windows 로그인 시 자동 실행과 창 닫기 동작입니다."));
    auto* lifecycleLayout = new QVBoxLayout(lifecycleGroup);
    lifecycleLayout->setSpacing(6);

    m_launchAtWindowsStartupCheck = addGroupCheck(
        lifecycleLayout,
        lifecycleGroup,
        tr("Windows 시작 시 자동 실행"),
        tr("Windows에 로그인하면 PIPBONG이 자동으로 시작됩니다."),
        ProgramSettings::launchAtWindowsStartup());

    m_closeToTrayCheck = addGroupCheck(
        lifecycleLayout,
        lifecycleGroup,
        tr("닫기 시 트레이로 최소화"),
        tr("창 닫기(X)를 눌러도 종료하지 않고 알림 영역에서 계속 실행합니다. "
           "단축키와 워크플로가 유지됩니다."),
        ProgramSettings::closeToTray());

    // --- 업데이트 ---
    auto* updateGroup = addSettingsGroup(
        layout,
        tr("업데이트"),
        tr("새 버전 확인과 자동 설치입니다."));
    auto* updateLayout = new QVBoxLayout(updateGroup);
    updateLayout->setSpacing(6);

    const int savedIntervalMinutes = ProgramSettings::updateCheckIntervalMinutes();

    m_updateCheckEnabledCheck = addGroupCheck(
        updateLayout,
        updateGroup,
        tr("주기적으로 업데이트 확인"),
        tr("백그라운드에서 GitHub 릴리즈를 주기적으로 확인합니다. 끄면 자동 확인을 하지 않습니다."),
        savedIntervalMinutes > 0);

    auto* intervalRow = new QHBoxLayout();
    intervalRow->addSpacing(16);
    auto* intervalLabel = new QLabel(tr("확인 간격"), updateGroup);
    applyOptionToolTip(intervalLabel, tr("몇 분마다 업데이트를 확인할지 설정합니다. (1~1440분)"));
    intervalRow->addWidget(intervalLabel);
    m_updateCheckIntervalSpin = new DragAdjustSpinBox(updateGroup);
    m_updateCheckIntervalSpin->setRange(
        ProgramSettings::kMinUpdateCheckIntervalMinutes < 1 ? 1 : ProgramSettings::kMinUpdateCheckIntervalMinutes,
        ProgramSettings::kMaxUpdateCheckIntervalMinutes);
    m_updateCheckIntervalSpin->setSingleStep(1);
    m_updateCheckIntervalSpin->setValue(savedIntervalMinutes > 0 ? savedIntervalMinutes
                                                                 : ProgramSettings::kDefaultUpdateCheckIntervalMinutes);
    applyOptionToolTip(m_updateCheckIntervalSpin, intervalLabel->toolTip());
    intervalRow->addWidget(m_updateCheckIntervalSpin);
    m_updateCheckIntervalLabel = new QLabel(tr("분마다"), updateGroup);
    intervalRow->addWidget(m_updateCheckIntervalLabel);
    intervalRow->addStretch();
    updateLayout->addLayout(intervalRow);

    m_autoInstallUpdatesCheck = addGroupCheck(
        updateLayout,
        updateGroup,
        tr("새 버전 자동 설치"),
        tr("새 버전이 나오면 자동으로 받아 설치합니다. 기능 실행 중이면 끝난 뒤 설치합니다."),
        ProgramSettings::autoInstallUpdates());

    auto updateIntervalEnabledState = [this]() {
        const bool enabled = m_updateCheckEnabledCheck->isChecked();
        m_updateCheckIntervalSpin->setEnabled(enabled);
        m_updateCheckIntervalLabel->setEnabled(enabled);
    };
    connect(m_updateCheckEnabledCheck, &QCheckBox::toggled, this, [updateIntervalEnabledState](bool) {
        updateIntervalEnabledState();
    });
    updateIntervalEnabledState();

    // --- 권한·호환 ---
    auto* compatGroup = addSettingsGroup(
        layout,
        tr("권한·호환"),
        tr("관리자 권한이 필요한 게임·프로그램과 함께 쓸 때의 설정입니다."));
    auto* compatLayout = new QVBoxLayout(compatGroup);
    compatLayout->setSpacing(6);

    m_runAsAdministratorCheck = addGroupCheck(
        compatLayout,
        compatGroup,
        tr("항상 관리자 권한으로 실행"),
        tr("타겟 프로그램이 관리자 권한으로 실행될 때 입력·화면 캡처가 동작합니다. "
           "실행할 때마다 Windows UAC 확인이 뜰 수 있습니다."),
        ProgramSettings::runAsAdministrator());

    if (WindowsRunAsAdmin::isProcessElevated()) {
        auto* elevatedStatus = new QLabel(tr("현재 관리자 권한으로 실행 중입니다."), compatGroup);
        QPalette pal = elevatedStatus->palette();
        pal.setColor(QPalette::WindowText, secondaryHintTextColor(pal));
        elevatedStatus->setPalette(pal);
        compatLayout->addWidget(elevatedStatus);
    }

    // --- 템플릿 매칭 ---
    auto* captureGroup = addSettingsGroup(
        layout,
        tr("템플릿 매칭"),
        tr("템플릿 캡처와 화면에서 찾기에 쓰는 캡처 방식입니다."));
    auto* captureLayout = new QVBoxLayout(captureGroup);
    captureLayout->setSpacing(6);

    auto* captureModeRow = new QHBoxLayout();
    auto* captureModeLabel = new QLabel(tr("캡처 방식"), captureGroup);
    applyOptionToolTip(captureModeLabel, tr("템플릿 매칭이 화면을 가져오는 방법입니다."));
    captureModeRow->addWidget(captureModeLabel);
    m_imageFindCaptureModeCombo = new QComboBox(captureGroup);
    m_imageFindCaptureModeCombo->addItem(
        tr("기본 — 화면 그대로"),
        static_cast<int>(ProgramSettings::ImageFindCaptureMode::Hybrid));
    m_imageFindCaptureModeCombo->addItem(
        tr("창 안만 — 커서·표시 제외"),
        static_cast<int>(ProgramSettings::ImageFindCaptureMode::ClientOnly));
    const int modeIndex = m_imageFindCaptureModeCombo->findData(
        static_cast<int>(ProgramSettings::imageFindCaptureMode()));
    if (modeIndex >= 0) {
        m_imageFindCaptureModeCombo->setCurrentIndex(modeIndex);
    }
    captureModeRow->addWidget(m_imageFindCaptureModeCombo, 1);
    captureLayout->addLayout(captureModeRow);
    connect(m_imageFindCaptureModeCombo,
            QOverload<int>::of(&QComboBox::currentIndexChanged),
            this,
            [this](int) { updateImageFindCaptureModeToolTip(); });
    updateImageFindCaptureModeToolTip();

    // --- 화면 표시 ---
    auto* visualGroup = addSettingsGroup(
        layout,
        tr("화면 표시"),
        tr("클릭·타겟 지정 시 타겟 위에 보이는 효과입니다."));
    auto* visualLayout = new QVBoxLayout(visualGroup);
    visualLayout->setSpacing(8);

    auto* clickSection = new QWidget(visualGroup);
    auto* clickSectionLayout = new QVBoxLayout(clickSection);
    clickSectionLayout->setContentsMargins(0, 0, 0, 0);
    clickSectionLayout->setSpacing(4);
    auto* clickTitle = new QLabel(tr("실행 위치 피드백 (기본)"), clickSection);
    applyOptionToolTip(
        clickTitle,
        tr("기능 편집에서 실행 위치 표시가 켜진 기능의 클릭·실행 위치를 타겟에 보여 주는 기본 애니메이션입니다. "
           "템플릿 감지 피드백은 템플릿 매칭 블록 편집에서 블록별로 지정합니다."));
    clickSectionLayout->addWidget(clickTitle);
    m_clickFeedbackSummary = new QLabel(clickSection);
    m_clickFeedbackSummary->setWordWrap(true);
    updateClickFeedbackSummary();
    auto* clickRow = new QHBoxLayout();
    clickRow->addWidget(m_clickFeedbackSummary, 1);
    m_clickFeedbackButton = new QPushButton(tr("설정…"), clickSection);
    m_clickFeedbackButton->setCursor(Qt::PointingHandCursor);
    applyOptionToolTip(m_clickFeedbackButton, clickTitle->toolTip());
    connect(m_clickFeedbackButton, &QPushButton::clicked, this, &ProgramSettingsDialog::onOpenClickFeedbackSettings);
    clickRow->addWidget(m_clickFeedbackButton, 0, Qt::AlignTop);
    clickSectionLayout->addLayout(clickRow);
    visualLayout->addWidget(clickSection);

    auto* windowSection = new QWidget(visualGroup);
    auto* windowSectionLayout = new QVBoxLayout(windowSection);
    windowSectionLayout->setContentsMargins(0, 0, 0, 0);
    windowSectionLayout->setSpacing(4);
    auto* windowTitle = new QLabel(tr("타겟 지정 애니메이션"), windowSection);
    applyOptionToolTip(
        windowTitle,
        tr("타겟 지정 또는 창 목록에서 타겟을 고를 때 그 창 위에 재생되는 확인 효과입니다."));
    windowSectionLayout->addWidget(windowTitle);
    m_windowSelectionFeedbackSummary = new QLabel(windowSection);
    m_windowSelectionFeedbackSummary->setWordWrap(true);
    updateWindowSelectionFeedbackSummary();
    auto* windowSelectionRow = new QHBoxLayout();
    windowSelectionRow->addWidget(m_windowSelectionFeedbackSummary, 1);
    m_windowSelectionFeedbackButton = new QPushButton(tr("설정…"), windowSection);
    m_windowSelectionFeedbackButton->setCursor(Qt::PointingHandCursor);
    applyOptionToolTip(m_windowSelectionFeedbackButton, windowTitle->toolTip());
    connect(m_windowSelectionFeedbackButton,
            &QPushButton::clicked,
            this,
            &ProgramSettingsDialog::onOpenWindowSelectionFeedbackSettings);
    windowSelectionRow->addWidget(m_windowSelectionFeedbackButton, 0, Qt::AlignTop);
    windowSectionLayout->addLayout(windowSelectionRow);
    visualLayout->addWidget(windowSection);

    // --- 진단 ---
    auto* diagnosticsGroup = addSettingsGroup(
        layout,
        tr("진단"),
        tr("성능 스터터·지연 원인 분석용 로그입니다. 일반 사용 시에는 끄세요."));
    auto* diagnosticsLayout = new QVBoxLayout(diagnosticsGroup);
    diagnosticsLayout->setSpacing(6);

    m_cursorStutterProfilingCheck = addGroupCheck(
        diagnosticsLayout,
        diagnosticsGroup,
        tr("커서 스터터 진단"),
        tr("마우스 커서 점프·튐 현상(QWER 연타 등) 원인 분석용입니다. "
           "커서 샘플링은 앱 실행 중 항상 동작합니다. 이 옵션을 켜면 SetCursorPos·마우스 잠금 훅·키보드 훅 상세 이벤트도 기록합니다(기본 켜짐). "
           "보고서: 저장소 cursor-stutter\\latest.md 및 AppData 백업. Hold Q/W/E/R 키를 떼거나 앱 종료 시 갱신. "
           "환경 변수 PIPBONG_CURSOR_STUTTER_PROFILE=1 로도 켤 수 있습니다."),
        ProgramSettings::cursorStutterProfiling());

    layout->addStretch();

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    localizeDialogButtons(buttons);
    connect(buttons, &QDialogButtonBox::accepted, this, [this]() {
        ProgramSettings::setAutoSelectRunningFeature(m_autoSelectRunningFeatureCheck->isChecked());
        ProgramSettings::setFocusTargetWindowOnProfileSelect(
            m_focusTargetWindowOnProfileSelectCheck->isChecked());
        ProgramSettings::setLaunchAtWindowsStartup(m_launchAtWindowsStartupCheck->isChecked());
        ProgramSettings::setCloseToTray(m_closeToTrayCheck->isChecked());
        ProgramSettings::setAutoInstallUpdates(m_autoInstallUpdatesCheck->isChecked());
        ProgramSettings::setUpdateCheckIntervalMinutes(
            m_updateCheckEnabledCheck->isChecked() ? m_updateCheckIntervalSpin->value() : 0);
        ProgramSettings::setRunAsAdministrator(m_runAsAdministratorCheck->isChecked());
        ProgramSettings::setRunWithoutTargetWindow(m_runWithoutTargetWindowCheck->isChecked());
        ProgramSettings::setLogMaxLines(m_logMaxLinesSpin->value());
        ProgramSettings::setCursorStutterProfiling(m_cursorStutterProfilingCheck->isChecked());
        CursorStutterProfiler::reloadFromSettings();
        ProgramSettings::setImageFindCaptureMode(
            static_cast<ProgramSettings::ImageFindCaptureMode>(
                m_imageFindCaptureModeCombo->currentData().toInt()));
        accept();
    });
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    layout->addWidget(buttons);
}

void ProgramSettingsDialog::updateClickFeedbackSummary() {
    if (!m_clickFeedbackSummary) {
        return;
    }
    m_clickFeedbackSummary->setText(ClickPointerFeedbackSettingsDialog::settingsSummary(
        PointerFeedbackSettings::click()));
}

void ProgramSettingsDialog::onOpenClickFeedbackSettings() {
    ClickPointerFeedbackSettingsDialog dialog(this);
    dialog.setWindowTitle(tr("실행 위치 피드백 (기본)"));
    if (dialog.exec() == QDialog::Accepted) {
        updateClickFeedbackSummary();
    }
}

void ProgramSettingsDialog::updateImageFindCaptureModeToolTip() {
    if (!m_imageFindCaptureModeCombo) {
        return;
    }

    const auto mode = static_cast<ProgramSettings::ImageFindCaptureMode>(
        m_imageFindCaptureModeCombo->currentData().toInt());
    if (mode == ProgramSettings::ImageFindCaptureMode::ClientOnly) {
        m_imageFindCaptureModeCombo->setToolTip(
            tr("게임·프로그램 창 안만 캡처합니다. 마우스 커서나 클릭 표시가 방해할 때 사용하세요. "
               "일부 프로그램에서는 검은 화면이 나올 수 있습니다."));
    } else {
        m_imageFindCaptureModeCombo->setToolTip(
            tr("화면에 보이는 그대로 캡처합니다. 대부분의 프로그램에서 잘 맞습니다. "
               "처음이거나 잘 모르겠으면 이 방식을 쓰세요."));
    }
}

void ProgramSettingsDialog::updateWindowSelectionFeedbackSummary() {
    m_windowSelectionFeedbackSummary->setText(
        WindowSelectionFeedbackSettingsDialog::settingsSummary(PointerFeedbackSettings::windowSelection()));
}

void ProgramSettingsDialog::onOpenWindowSelectionFeedbackSettings() {
    WindowSelectionFeedbackSettingsDialog dialog(this);
    if (dialog.exec() == QDialog::Accepted) {
        updateWindowSelectionFeedbackSummary();
    }
}
