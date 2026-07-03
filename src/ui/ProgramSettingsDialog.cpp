#include "ui/ProgramSettingsDialog.h"

#include "app/PointerFeedbackSettings.h"
#include "app/ProgramSettings.h"
#include "app/WindowsRunAsAdmin.h"
#include "ui/ClickPointerFeedbackSettingsDialog.h"
#include "ui/UiStrings.h"
#include "ui/widgets/DragAdjustSpinBox.h"
#include "ui/widgets/HintLabel.h"

#include <QCheckBox>
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
    resize(460, 340);
    setupUi();
}

void ProgramSettingsDialog::setupUi() {
    auto* layout = new QVBoxLayout(this);

    auto* hint = new HintLabel(
        tr("PIPBONG 전체에 적용되는 설정입니다. 프로젝트 파일에는 저장되지 않습니다."), this);
    layout->addWidget(hint);

    m_autoSelectRunningFeatureCheck =
        new QCheckBox(tr("기능 실행 시 해당 기능을 자동으로 선택"), this);
    m_autoSelectRunningFeatureCheck->setChecked(ProgramSettings::autoSelectRunningFeature());
    m_autoSelectRunningFeatureCheck->setToolTip(
        tr("단축키나 실행 메뉴로 기능이 시작되면 워크플로우 패널에 그 기능을 표시합니다."));
    layout->addWidget(m_autoSelectRunningFeatureCheck);

    m_launchAtWindowsStartupCheck =
        new QCheckBox(tr("Windows 시작 시 PIPBONG 자동 실행"), this);
    m_launchAtWindowsStartupCheck->setChecked(ProgramSettings::launchAtWindowsStartup());
    m_launchAtWindowsStartupCheck->setToolTip(
        tr("Windows에 로그인하면 PIPBONG이 자동으로 실행됩니다."));
    layout->addWidget(m_launchAtWindowsStartupCheck);

    m_closeToTrayCheck = new QCheckBox(tr("닫기 시 트레이로 최소화 (백그라운드 실행)"), this);
    m_closeToTrayCheck->setChecked(ProgramSettings::closeToTray());
    m_closeToTrayCheck->setToolTip(
        tr("창 닫기(X)를 누르면 종료하지 않고 알림 영역에서 계속 실행합니다. 단축키와 워크플로가 유지됩니다."));
    layout->addWidget(m_closeToTrayCheck);

    m_autoInstallUpdatesCheck =
        new QCheckBox(tr("새 버전 감지 시 자동 업데이트"), this);
    m_autoInstallUpdatesCheck->setChecked(ProgramSettings::autoInstallUpdates());
    m_autoInstallUpdatesCheck->setToolTip(
        tr("프로그램 실행 중 새 버전이 감지되면 자동으로 다운로드하고 설치합니다. "
           "워크플로 실행 중이면 실행이 끝난 뒤 자동 업데이트합니다."));
    layout->addWidget(m_autoInstallUpdatesCheck);

    const int savedIntervalMinutes = ProgramSettings::updateCheckIntervalMinutes();

    m_updateCheckEnabledCheck =
        new QCheckBox(tr("주기적으로 업데이트 확인"), this);
    m_updateCheckEnabledCheck->setChecked(savedIntervalMinutes > 0);
    m_updateCheckEnabledCheck->setToolTip(
        tr("백그라운드에서 GitHub 릴리즈를 주기적으로 확인합니다. 끄면 자동 확인을 하지 않습니다."));
    layout->addWidget(m_updateCheckEnabledCheck);

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
    m_updateCheckIntervalSpin->setToolTip(tr("업데이트를 확인하는 간격(분)입니다. (1~1440분)"));
    intervalRow->addWidget(m_updateCheckIntervalSpin);
    m_updateCheckIntervalLabel = new QLabel(tr("분마다"), this);
    intervalRow->addWidget(m_updateCheckIntervalLabel);
    intervalRow->addStretch();
    layout->addLayout(intervalRow);

    auto updateIntervalEnabledState = [this]() {
        const bool enabled = m_updateCheckEnabledCheck->isChecked();
        m_updateCheckIntervalSpin->setEnabled(enabled);
        m_updateCheckIntervalLabel->setEnabled(enabled);
    };
    connect(m_updateCheckEnabledCheck, &QCheckBox::toggled, this, [updateIntervalEnabledState](bool) {
        updateIntervalEnabledState();
    });
    updateIntervalEnabledState();

    m_runAsAdministratorCheck =
        new QCheckBox(tr("항상 관리자 권한으로 실행"), this);
    m_runAsAdministratorCheck->setChecked(ProgramSettings::runAsAdministrator());
    m_runAsAdministratorCheck->setToolTip(
        tr("대상 프로그램이 관리자 권한으로 실행될 때 입력·화면 캡처가 동작합니다. "
           "실행할 때마다 Windows UAC 확인이 표시될 수 있습니다."));
    layout->addWidget(m_runAsAdministratorCheck);

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
        accept();
    });
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    layout->addWidget(buttons);
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
