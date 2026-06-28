#include "ui/ProgramSettingsDialog.h"

#include "app/ProgramSettings.h"
#include "ui/UiStrings.h"
#include "ui/widgets/HintLabel.h"

#include <QCheckBox>
#include <QDialogButtonBox>
#include <QLabel>
#include <QVBoxLayout>

ProgramSettingsDialog::ProgramSettingsDialog(QWidget* parent)
    : QDialog(parent) {
    setWindowTitle(tr("프로그램 설정"));
    setModal(true);
    resize(440, 200);
    setupUi();
}

void ProgramSettingsDialog::setupUi() {
    auto* layout = new QVBoxLayout(this);

    auto* hint = new HintLabel(
        tr("SuckbongMachine 전체에 적용되는 설정입니다. 프로젝트 파일에는 저장되지 않습니다."), this);
    layout->addWidget(hint);

    m_autoSelectRunningFeatureCheck =
        new QCheckBox(tr("기능 실행 시 해당 기능을 자동으로 선택"), this);
    m_autoSelectRunningFeatureCheck->setChecked(ProgramSettings::autoSelectRunningFeature());
    m_autoSelectRunningFeatureCheck->setToolTip(
        tr("단축키나 실행 메뉴로 기능이 시작되면 워크플로우 패널에 그 기능을 표시합니다."));
    layout->addWidget(m_autoSelectRunningFeatureCheck);

    m_showWorkflowRunFeedbackCheck =
        new QCheckBox(tr("워크플로 실행 시 대상 창에 위치 표시"), this);
    m_showWorkflowRunFeedbackCheck->setChecked(ProgramSettings::showWorkflowRunFeedback());
    m_showWorkflowRunFeedbackCheck->setToolTip(
        tr("템플릿 매칭 시도마다 대상 창에 녹색/적색 펄스로 매칭·클릭 위치를 표시합니다."));
    layout->addWidget(m_showWorkflowRunFeedbackCheck);

    layout->addStretch();

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    localizeDialogButtons(buttons);
    connect(buttons, &QDialogButtonBox::accepted, this, [this]() {
        ProgramSettings::setAutoSelectRunningFeature(m_autoSelectRunningFeatureCheck->isChecked());
        ProgramSettings::setShowWorkflowRunFeedback(m_showWorkflowRunFeedbackCheck->isChecked());
        accept();
    });
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    layout->addWidget(buttons);
}
