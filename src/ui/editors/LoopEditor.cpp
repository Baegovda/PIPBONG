#include "ui/editors/LoopEditor.h"

#include "ui/editors/BranchWorkflowEditor.h"
#include "ui/widgets/DragAdjustSpinBox.h"

#include <QComboBox>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QVBoxLayout>

LoopEditor::LoopEditor(LoopBlock* block, const QString& projectDirectory, QWidget* parent,
                       bool embedded)
    : QDialog(parent)
    , m_block(block)
    , m_projectDirectory(projectDirectory)
    , m_embedded(embedded) {
    if (!m_embedded) {
        setWindowTitle(tr("구간 반복 블록 편집"));
    }
    setupUi();
    reload();
}

void LoopEditor::setupUi() {
    auto* layout = new QVBoxLayout(this);

    auto* form = new QFormLayout();
    m_exitConditionCombo = new QComboBox(this);
    m_exitConditionCombo->addItem(tr("탐지 실패 시까지"));
    m_exitConditionCombo->addItem(tr("탐지 성공 시까지"));
    m_exitConditionCombo->addItem(tr("직전 매칭 성공 시까지"));
    m_exitConditionCombo->addItem(tr("직전 매칭 실패 시까지"));
    m_exitConditionCombo->setToolTip(
        tr("반복 본문을 실행한 뒤 조건이 충족되면 반복을 멈추고 다음 블록으로 진행합니다."));
    form->addRow(tr("종료 조건"), m_exitConditionCombo);

    m_missLimitLabel = new QLabel(tr("탐지 실패 허용"), this);
    m_missLimitSpin = new DragAdjustSpinBox(this);
    m_missLimitSpin->setRange(1, 1000);
    m_missLimitSpin->setSingleStep(1);
    m_missLimitSpin->setValue(1);
    m_missLimitSpin->setSuffix(tr(" 회"));
    m_missLimitSpin->setToolTip(
        tr("반복 본문 안의 템플릿 매칭이 연속으로 이 횟수만큼 실패하면 탐지 실패로 간주합니다."));
    form->addRow(m_missLimitLabel, m_missLimitSpin);
    layout->addLayout(form);

    connect(m_exitConditionCombo, qOverload<int>(&QComboBox::currentIndexChanged), this,
            [this](int) { updateConditionDependentUi(); });

    m_bodyEditor =
        new BranchWorkflowEditor(&m_block->body, m_projectDirectory, tr("반복 본문"), this);
    layout->addWidget(m_bodyEditor);
    connect(m_bodyEditor, &BranchWorkflowEditor::changed, this, &LoopEditor::layoutChanged);

    if (!m_embedded) {
        auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
        connect(buttons, &QDialogButtonBox::accepted, this, [this]() {
            apply();
            accept();
        });
        connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
        layout->addWidget(buttons);
    }
}

void LoopEditor::setBlock(LoopBlock* block) {
    m_block = block;
    if (m_bodyEditor) {
        m_bodyEditor->setWorkflow(&m_block->body);
    }
    reload();
}

LoopExitCondition LoopEditor::exitConditionFromComboIndex(int index) const {
    switch (index) {
    case 1:
        return LoopExitCondition::DetectionSucceeded;
    case 2:
        return LoopExitCondition::LastMatchSuccess;
    case 3:
        return LoopExitCondition::LastMatchFailed;
    default:
        return LoopExitCondition::DetectionFailed;
    }
}

int LoopEditor::comboIndexForExitCondition(LoopExitCondition condition) const {
    switch (condition) {
    case LoopExitCondition::DetectionSucceeded:
        return 1;
    case LoopExitCondition::LastMatchSuccess:
        return 2;
    case LoopExitCondition::LastMatchFailed:
        return 3;
    default:
        return 0;
    }
}

void LoopEditor::updateConditionDependentUi() {
    const bool showMissLimit =
        exitConditionFromComboIndex(m_exitConditionCombo->currentIndex())
        == LoopExitCondition::DetectionFailed;
    m_missLimitLabel->setVisible(showMissLimit);
    m_missLimitSpin->setVisible(showMissLimit);
    emit layoutChanged();
}

void LoopEditor::reload() {
    if (!m_block) {
        return;
    }
    m_exitConditionCombo->setCurrentIndex(comboIndexForExitCondition(m_block->exitCondition));
    m_missLimitSpin->setValue(m_block->detectionMissLimit);
    m_bodyEditor->reload();
    updateConditionDependentUi();
}

void LoopEditor::apply() {
    if (!m_block) {
        return;
    }
    m_block->exitCondition = exitConditionFromComboIndex(m_exitConditionCombo->currentIndex());
    m_block->detectionMissLimit = m_missLimitSpin->value();
}
