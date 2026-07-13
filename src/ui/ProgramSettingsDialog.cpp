#include "ui/ProgramSettingsDialog.h"

#include "app/PointerFeedbackSettings.h"
#include "app/ProgramSettings.h"
#include "app/WindowsRunAsAdmin.h"
#include "ui/ClickPointerFeedbackSettingsDialog.h"
#include "ui/WindowSelectionFeedbackSettingsDialog.h"
#include "ui/UiStrings.h"
#include "ui/widgets/DragAdjustSpinBox.h"
#include "ui/widgets/HintLabel.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>

ProgramSettingsDialog::ProgramSettingsDialog(QWidget* parent)
    : QDialog(parent) {
    setWindowTitle(tr("프로그램 설정"));
    setModal(true);
    resize(500, 680);
    setupUi();
}

void ProgramSettingsDialog::setupUi() {
    auto* layout = new QVBoxLayout(this);

    auto* hint = new HintLabel(
        tr("일부 실행 옵션은 현재 프로필에 저장되고, 시작·트레이·관리자·업데이트 옵션은 전체 프로그램에 적용됩니다."),
        this);
    layout->addWidget(hint);

    const auto addCheckWithHint = [this, layout](QCheckBox*& check,
                                                 const QString& label,
                                                 const QString& hintText,
                                                 bool checked) {
        check = new QCheckBox(label, this);
        check->setChecked(checked);
        layout->addWidget(check);

        auto* optionHint = new HintLabel(hintText, this);
        optionHint->setWordWrap(true);
        layout->addWidget(optionHint);
    };

    addCheckWithHint(
        m_autoSelectRunningFeatureCheck,
        tr("기능 실행 시 해당 기능을 자동으로 선택"),
        tr("단축키나 실행 메뉴로 기능이 시작되면 워크플로우 패널에 그 기능을 표시합니다."),
        ProgramSettings::autoSelectRunningFeature());

    addCheckWithHint(
        m_launchAtWindowsStartupCheck,
        tr("Windows 시작 시 PIPBONG 자동 실행"),
        tr("Windows에 로그인하면 PIPBONG이 자동으로 실행됩니다."),
        ProgramSettings::launchAtWindowsStartup());

    addCheckWithHint(
        m_closeToTrayCheck,
        tr("닫기 시 트레이로 최소화 (백그라운드 실행)"),
        tr("창 닫기(X)를 누르면 종료하지 않고 알림 영역에서 계속 실행합니다. 단축키와 워크플로가 유지됩니다."),
        ProgramSettings::closeToTray());

    addCheckWithHint(
        m_autoInstallUpdatesCheck,
        tr("새 버전 감지 시 자동 업데이트"),
        tr("프로그램 실행 중 새 버전이 감지되면 자동으로 다운로드하고 설치합니다. "
           "워크플로 실행 중이면 실행이 끝난 뒤 자동 업데이트합니다."),
        ProgramSettings::autoInstallUpdates());

    const int savedIntervalMinutes = ProgramSettings::updateCheckIntervalMinutes();

    addCheckWithHint(
        m_updateCheckEnabledCheck,
        tr("주기적으로 업데이트 확인"),
        tr("백그라운드에서 GitHub 릴리즈를 주기적으로 확인합니다. 끄면 자동 확인을 하지 않습니다."),
        savedIntervalMinutes > 0);

    auto* intervalRow = new QHBoxLayout();
    intervalRow->addSpacing(20);
    intervalRow->addWidget(new QLabel(tr("확인 간격"), this));
    m_updateCheckIntervalSpin = new DragAdjustSpinBox(this);
    m_updateCheckIntervalSpin->setRange(
        ProgramSettings::kMinUpdateCheckIntervalMinutes < 1 ? 1 : ProgramSettings::kMinUpdateCheckIntervalMinutes,
        ProgramSettings::kMaxUpdateCheckIntervalMinutes);
    m_updateCheckIntervalSpin->setSingleStep(1);
    m_updateCheckIntervalSpin->setValue(
        savedIntervalMinutes > 0 ? savedIntervalMinutes
                                 : ProgramSettings::kDefaultUpdateCheckIntervalMinutes);
    intervalRow->addWidget(m_updateCheckIntervalSpin);
    m_updateCheckIntervalLabel = new QLabel(tr("분마다"), this);
    intervalRow->addWidget(m_updateCheckIntervalLabel);
    intervalRow->addStretch();
    layout->addLayout(intervalRow);

    auto* intervalHint =
        new HintLabel(tr("업데이트를 확인하는 간격(분)입니다. (1~1440분)"), this);
    intervalHint->setWordWrap(true);
    layout->addWidget(intervalHint);

    auto updateIntervalEnabledState = [this, intervalHint]() {
        const bool enabled = m_updateCheckEnabledCheck->isChecked();
        m_updateCheckIntervalSpin->setEnabled(enabled);
        m_updateCheckIntervalLabel->setEnabled(enabled);
        intervalHint->setEnabled(enabled);
    };
    connect(m_updateCheckEnabledCheck, &QCheckBox::toggled, this, [updateIntervalEnabledState](bool) {
        updateIntervalEnabledState();
    });
    updateIntervalEnabledState();

    addCheckWithHint(
        m_runAsAdministratorCheck,
        tr("항상 관리자 권한으로 실행"),
        tr("대상 프로그램이 관리자 권한으로 실행될 때 입력·화면 캡처가 동작합니다. "
           "실행할 때마다 Windows UAC 확인이 표시될 수 있습니다."),
        ProgramSettings::runAsAdministrator());

    addCheckWithHint(
        m_runWithoutTargetWindowCheck,
        tr("창을 지정하지 않은 상태에서도 동작"),
        tr("대상 창을 지정하지 않아도 키보드·딜레이·텍스트 블록을 실행할 수 있습니다. "
           "템플릿 매칭의 대상 창 탐색은 전체 화면으로 대체됩니다. "
           "끄면 기능 실행 전에 '창 지정'이 필요합니다."),
        ProgramSettings::runWithoutTargetWindow());

    auto* captureGroup = new QGroupBox(tr("템플릿 매칭 캡처"), this);
    auto* captureLayout = new QVBoxLayout(captureGroup);

    auto* captureModeRow = new QHBoxLayout();
    captureModeRow->addWidget(new QLabel(tr("캡처 방식"), captureGroup));
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

    m_imageFindCaptureModeHint = new HintLabel(QString(), captureGroup);
    m_imageFindCaptureModeHint->setWordWrap(true);
    captureLayout->addWidget(m_imageFindCaptureModeHint);
    connect(m_imageFindCaptureModeCombo,
            QOverload<int>::of(&QComboBox::currentIndexChanged),
            this,
            [this](int) { updateImageFindCaptureModeHint(); });
    updateImageFindCaptureModeHint();

    layout->addWidget(captureGroup);

    if (WindowsRunAsAdmin::isProcessElevated()) {
        auto* elevatedHint =
            new HintLabel(tr("현재 관리자 권한으로 실행 중입니다."), this);
        layout->addWidget(elevatedHint);
    }

    auto* clickGroup = new QGroupBox(tr("마우스 클릭 피드백 애니메이션"), this);
    auto* clickLayout = new QVBoxLayout(clickGroup);

    m_clickFeedbackSummary = new QLabel(clickGroup);
    m_clickFeedbackSummary->setWordWrap(true);
    updateClickFeedbackSummary();

    auto* clickRow = new QHBoxLayout();
    clickRow->addWidget(m_clickFeedbackSummary, 1);
    m_clickFeedbackButton = new QPushButton(tr("설정…"), clickGroup);
    m_clickFeedbackButton->setCursor(Qt::PointingHandCursor);
    connect(m_clickFeedbackButton, &QPushButton::clicked, this, &ProgramSettingsDialog::onOpenClickFeedbackSettings);
    clickRow->addWidget(m_clickFeedbackButton, 0, Qt::AlignTop);
    clickLayout->addLayout(clickRow);

    auto* clickHint = new HintLabel(
        tr("기능 편집의 실행 위치 표시가 켜진 기능에서 클릭 시 대상 창에 표시되는 포인터 애니메이션입니다."),
        clickGroup);
    clickHint->setWordWrap(true);
    clickLayout->addWidget(clickHint);
    layout->addWidget(clickGroup);

    auto* windowSelectionGroup = new QGroupBox(tr("창 지정 애니메이션"), this);
    auto* windowSelectionLayout = new QVBoxLayout(windowSelectionGroup);

    m_windowSelectionFeedbackSummary = new QLabel(windowSelectionGroup);
    m_windowSelectionFeedbackSummary->setWordWrap(true);
    updateWindowSelectionFeedbackSummary();

    auto* windowSelectionRow = new QHBoxLayout();
    windowSelectionRow->addWidget(m_windowSelectionFeedbackSummary, 1);
    m_windowSelectionFeedbackButton = new QPushButton(tr("설정…"), windowSelectionGroup);
    m_windowSelectionFeedbackButton->setCursor(Qt::PointingHandCursor);
    connect(m_windowSelectionFeedbackButton,
            &QPushButton::clicked,
            this,
            &ProgramSettingsDialog::onOpenWindowSelectionFeedbackSettings);
    windowSelectionRow->addWidget(m_windowSelectionFeedbackButton, 0, Qt::AlignTop);
    windowSelectionLayout->addLayout(windowSelectionRow);

    auto* windowSelectionHint = new HintLabel(
        tr("창 지정 또는 창 목록에서 대상 창을 선택했을 때 대상 창 위에 재생되는 확인 애니메이션입니다."),
        windowSelectionGroup);
    windowSelectionHint->setWordWrap(true);
    windowSelectionLayout->addWidget(windowSelectionHint);
    layout->addWidget(windowSelectionGroup);

    layout->addStretch();

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    localizeDialogButtons(buttons);
    connect(buttons, &QDialogButtonBox::accepted, this, [this]() {
        ProgramSettings::setAutoSelectRunningFeature(m_autoSelectRunningFeatureCheck->isChecked());
        ProgramSettings::setLaunchAtWindowsStartup(m_launchAtWindowsStartupCheck->isChecked());
        ProgramSettings::setCloseToTray(m_closeToTrayCheck->isChecked());
        ProgramSettings::setAutoInstallUpdates(m_autoInstallUpdatesCheck->isChecked());
        ProgramSettings::setUpdateCheckIntervalMinutes(
            m_updateCheckEnabledCheck->isChecked() ? m_updateCheckIntervalSpin->value() : 0);
        ProgramSettings::setRunAsAdministrator(m_runAsAdministratorCheck->isChecked());
        ProgramSettings::setRunWithoutTargetWindow(m_runWithoutTargetWindowCheck->isChecked());
        ProgramSettings::setImageFindCaptureMode(
            static_cast<ProgramSettings::ImageFindCaptureMode>(
                m_imageFindCaptureModeCombo->currentData().toInt()));
        accept();
    });
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    layout->addWidget(buttons);
}

void ProgramSettingsDialog::updateImageFindCaptureModeHint() {
    if (!m_imageFindCaptureModeCombo || !m_imageFindCaptureModeHint) {
        return;
    }

    const auto mode = static_cast<ProgramSettings::ImageFindCaptureMode>(
        m_imageFindCaptureModeCombo->currentData().toInt());
    if (mode == ProgramSettings::ImageFindCaptureMode::ClientOnly) {
        m_imageFindCaptureModeHint->setText(
            tr("대상 프로그램 창 안의 그림만 가져옵니다. "
               "마우스 커서나 클릭·매칭 표시가 템플릿 찾기를 방해할 때 이쪽을 쓰세요. "
               "일부 게임·프로그램에서는 검은 화면이 나올 수 있습니다."));
    } else {
        m_imageFindCaptureModeHint->setText(
            tr("지금까지 쓰던 방식입니다. 화면에 보이는 대로 캡처해서 "
               "대부분의 프로그램에서 잘 맞습니다. 처음이거나 잘 모르겠으면 이걸 쓰세요."));
    }
}

void ProgramSettingsDialog::updateClickFeedbackSummary() {
    m_clickFeedbackSummary->setText(ClickPointerFeedbackSettingsDialog::settingsSummary(
        PointerFeedbackSettings::click()));
}

void ProgramSettingsDialog::onOpenClickFeedbackSettings() {
    ClickPointerFeedbackSettingsDialog dialog(this);
    if (dialog.exec() == QDialog::Accepted) {
        updateClickFeedbackSummary();
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
