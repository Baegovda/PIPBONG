#include "ui/editors/IfEditor.h"

#include "ui/editors/BranchWorkflowEditor.h"
#include "ui/widgets/DragAdjustSpinBox.h"
#include "ui/widgets/HintLabel.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>

#include <algorithm>

IfEditor::IfEditor(IfBlock* block, const QString& projectDirectory, QWidget* parent, bool embedded)
    : QDialog(parent)
    , m_block(block)
    , m_projectDirectory(projectDirectory)
    , m_embedded(embedded) {
    if (!m_embedded) {
        setWindowTitle(tr("만약 블록 편집"));
    }
    setupUi();
    reload();
}

void IfEditor::setupUi() {
    auto* layout = new QVBoxLayout(this);

    auto* conditionRow = new QHBoxLayout();
    conditionRow->addWidget(new QLabel(tr("조건"), this));
    m_conditionCombo = new QComboBox(this);
    m_conditionCombo->addItem(tr("직전 감지 성공"));
    m_conditionCombo->addItem(tr("직전 감지 실패"));
    m_conditionCombo->addItem(tr("감지 실패"));
    conditionRow->addWidget(m_conditionCombo, 1);
    m_negateCheck = new QCheckBox(tr("반전"), this);
    conditionRow->addWidget(m_negateCheck);
    layout->addLayout(conditionRow);

    auto* gotoForm = new QFormLayout();
    m_thenGotoCheck = new QCheckBox(tr("맞으면 분기 후 이동"), this);
    m_thenGotoSpin = new DragAdjustSpinBox(this);
    m_thenGotoSpin->setRange(1, 9999);
    m_thenGotoSpin->setSingleStep(1);
    m_thenGotoSpin->setPrefix(tr("#"));
    m_thenGotoSpin->setEnabled(false);
    m_thenGotoSpin->setToolTip(tr("워크플로 # 열 블록 번호"));
    auto* thenGotoRow = new QHBoxLayout();
    thenGotoRow->addWidget(m_thenGotoCheck);
    thenGotoRow->addWidget(m_thenGotoSpin);
    thenGotoRow->addWidget(new QLabel(tr("블록"), this));
    m_thenGotoPickButton = new QPushButton(tr("목록에서 선택"), this);
    m_thenGotoPickButton->setToolTip(tr("워크플로 목록에서 클릭 또는 드래그하여 이동 지점을 지정합니다."));
    thenGotoRow->addWidget(m_thenGotoPickButton);
    thenGotoRow->addStretch(1);
    gotoForm->addRow(tr("이후 이동"), thenGotoRow);

    m_elseGotoCheck = new QCheckBox(tr("아니면 분기 후 이동"), this);
    m_elseGotoSpin = new DragAdjustSpinBox(this);
    m_elseGotoSpin->setRange(1, 9999);
    m_elseGotoSpin->setSingleStep(1);
    m_elseGotoSpin->setPrefix(tr("#"));
    m_elseGotoSpin->setEnabled(false);
    m_elseGotoSpin->setToolTip(tr("워크플로 # 열 블록 번호"));
    auto* elseGotoRow = new QHBoxLayout();
    elseGotoRow->addWidget(m_elseGotoCheck);
    elseGotoRow->addWidget(m_elseGotoSpin);
    elseGotoRow->addWidget(new QLabel(tr("블록"), this));
    m_elseGotoPickButton = new QPushButton(tr("목록에서 선택"), this);
    m_elseGotoPickButton->setToolTip(tr("워크플로 목록에서 클릭 또는 드래그하여 이동 지점을 지정합니다."));
    elseGotoRow->addWidget(m_elseGotoPickButton);
    elseGotoRow->addStretch(1);
    gotoForm->addRow(QString(), elseGotoRow);
    layout->addLayout(gotoForm);

    layout->addWidget(new HintLabel(
        tr("이동 대상은 이 만약 블록이 속한 워크플로의 # 열 번호입니다. 목록에서 선택으로 구간 반복처럼 블록을 클릭·드래그해 지정할 수 있습니다."),
        this,
        HintLabel::Tone::Secondary));

    connect(m_thenGotoPickButton, &QPushButton::clicked, this, [this]() { emit requestGotoBlockPick(0); });
    connect(m_elseGotoPickButton, &QPushButton::clicked, this, [this]() { emit requestGotoBlockPick(1); });

    connect(m_thenGotoCheck, &QCheckBox::toggled, this, [this](bool checked) {
        m_thenGotoSpin->setEnabled(checked);
        emit layoutChanged();
    });
    connect(m_elseGotoCheck, &QCheckBox::toggled, this, [this](bool checked) {
        m_elseGotoSpin->setEnabled(checked);
        emit layoutChanged();
    });
    connect(m_thenGotoSpin, qOverload<int>(&QSpinBox::valueChanged), this, &IfEditor::layoutChanged);
    connect(m_elseGotoSpin, qOverload<int>(&QSpinBox::valueChanged), this, &IfEditor::layoutChanged);

    m_thenEditor =
        new BranchWorkflowEditor(&m_block->thenBranch, m_projectDirectory, tr("맞으면 (then)"), this);
    m_elseEditor =
        new BranchWorkflowEditor(&m_block->elseBranch, m_projectDirectory, tr("아니면 (else)"), this);
    layout->addWidget(m_thenEditor);
    layout->addWidget(m_elseEditor);

    connect(m_thenEditor, &BranchWorkflowEditor::changed, this, &IfEditor::layoutChanged);
    connect(m_elseEditor, &BranchWorkflowEditor::changed, this, &IfEditor::layoutChanged);

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

void IfEditor::setGotoBlockPickAvailable(bool available) {
    m_gotoBlockPickAvailable = available;
    if (m_thenGotoPickButton) {
        m_thenGotoPickButton->setVisible(available);
    }
    if (m_elseGotoPickButton) {
        m_elseGotoPickButton->setVisible(available);
    }
}

void IfEditor::applyPickedGotoBlock(int branch, int blockNumber) {
    const int clamped = std::max(1, blockNumber);
    if (branch == 0) {
        m_thenGotoCheck->setChecked(true);
        m_thenGotoSpin->setEnabled(true);
        m_thenGotoSpin->setValue(clamped);
    } else {
        m_elseGotoCheck->setChecked(true);
        m_elseGotoSpin->setEnabled(true);
        m_elseGotoSpin->setValue(clamped);
    }
    emit layoutChanged();
}

void IfEditor::setWorkflowEditorContext(int blockCount, int editingBlockIndex) {
    m_workflowBlockCount = std::max(0, blockCount);
    m_editingBlockIndex = editingBlockIndex;
    updateGotoSpinRanges();
}

void IfEditor::updateGotoSpinRanges() {
    const int maxBlock = m_workflowBlockCount > 0 ? m_workflowBlockCount : 9999;
    m_thenGotoSpin->setRange(1, maxBlock);
    m_elseGotoSpin->setRange(1, maxBlock);
    if (m_editingBlockIndex >= 0 && m_workflowBlockCount > 0) {
        const int defaultBlock = std::clamp(m_editingBlockIndex, 0, m_workflowBlockCount - 1) + 1;
        if (!m_thenGotoCheck->isChecked()) {
            m_thenGotoSpin->setValue(defaultBlock);
        }
        if (!m_elseGotoCheck->isChecked()) {
            m_elseGotoSpin->setValue(defaultBlock);
        }
    }
}

void IfEditor::setBlock(IfBlock* block) {
    m_block = block;
    if (m_thenEditor) {
        m_thenEditor->setWorkflow(&m_block->thenBranch);
    }
    if (m_elseEditor) {
        m_elseEditor->setWorkflow(&m_block->elseBranch);
    }
    reload();
}

IfCondition IfEditor::conditionFromComboIndex(int index) const {
    switch (index) {
    case 1:
        return IfCondition::LastMatchFailed;
    case 2:
        return IfCondition::DetectionFailed;
    default:
        return IfCondition::LastMatchSuccess;
    }
}

int IfEditor::comboIndexForCondition(IfCondition condition) const {
    switch (condition) {
    case IfCondition::LastMatchFailed:
        return 1;
    case IfCondition::DetectionFailed:
        return 2;
    default:
        return 0;
    }
}

void IfEditor::reload() {
    if (!m_block) {
        return;
    }
    m_conditionCombo->setCurrentIndex(comboIndexForCondition(m_block->condition));
    m_negateCheck->setChecked(m_block->negate);

    const bool thenGoto = m_block->thenGotoBlockNumber > 0;
    m_thenGotoCheck->setChecked(thenGoto);
    m_thenGotoSpin->setEnabled(thenGoto);
    if (thenGoto) {
        m_thenGotoSpin->setValue(m_block->thenGotoBlockNumber);
    }

    const bool elseGoto = m_block->elseGotoBlockNumber > 0;
    m_elseGotoCheck->setChecked(elseGoto);
    m_elseGotoSpin->setEnabled(elseGoto);
    if (elseGoto) {
        m_elseGotoSpin->setValue(m_block->elseGotoBlockNumber);
    }

    updateGotoSpinRanges();
    m_thenEditor->reload();
    m_elseEditor->reload();
}

void IfEditor::apply() {
    if (!m_block) {
        return;
    }
    m_block->condition = conditionFromComboIndex(m_conditionCombo->currentIndex());
    m_block->negate = m_negateCheck->isChecked();
    m_block->thenGotoBlockNumber =
        m_thenGotoCheck->isChecked() ? std::max(1, m_thenGotoSpin->value()) : 0;
    m_block->elseGotoBlockNumber =
        m_elseGotoCheck->isChecked() ? std::max(1, m_elseGotoSpin->value()) : 0;
}
