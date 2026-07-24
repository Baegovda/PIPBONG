#include "ui/editors/WaitEditor.h"

#include "ui/UiStrings.h"
#include "ui/widgets/AnimatedTwoWaySwitch.h"
#include "ui/widgets/DragAdjustSpinBox.h"

#include <QDialogButtonBox>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QStackedWidget>
#include <QVBoxLayout>

namespace {

void configureWaitMsSpin(DragAdjustSpinBox* spin) {
    if (!spin) {
        return;
    }
    spin->setRange(0, 600000);
    spin->setSingleStep(kWaitDelayStepMs);
    QObject::connect(spin, &QAbstractSpinBox::editingFinished, spin, [spin]() {
        const int snapped = snapWaitDelayMs(spin->value());
        if (snapped != spin->value()) {
            spin->setValue(snapped);
        }
    });
}

} // namespace

WaitEditor::WaitEditor(WaitBlock* block, QWidget* parent, bool embedded)
    : QDialog(parent)
    , m_block(block)
    , m_embedded(embedded) {
    if (!m_embedded) {
        setWindowTitle(tr("딜레이 블록 편집"));
    }
    setupUi();
}

void WaitEditor::setBlock(WaitBlock* block) {
    m_block = block;
    reload();
}

void WaitEditor::reload() {
    if (!m_block) {
        return;
    }
    m_msSpin->setValue(snapWaitDelayMs(m_block->ms));
    m_minSpin->setValue(snapWaitDelayMs(m_block->minMs));
    m_maxSpin->setValue(snapWaitDelayMs(m_block->maxMs));
    m_modeSwitch->setRightSelected(m_block->randomRange, false);
    updateInputUi();
}

void WaitEditor::apply() {
    if (!m_block) {
        return;
    }
    m_block->ms = snapWaitDelayMs(m_msSpin->value());
    m_block->minMs = snapWaitDelayMs(m_minSpin->value());
    m_block->maxMs = snapWaitDelayMs(m_maxSpin->value());
    m_block->randomRange = m_modeSwitch->isRightSelected();
}

void WaitEditor::updateInputUi() {
    const bool random = m_modeSwitch->isRightSelected();
    m_inputStack->setCurrentIndex(random ? 1 : 0);
    emit layoutChanged();
}

QWidget* WaitEditor::makeMsInputRow(DragAdjustSpinBox* spin) {
    auto* row = new QWidget(this);
    auto* layout = new QHBoxLayout(row);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(4);
    spin->setMinimumWidth(88);
    layout->addWidget(spin);
    layout->addWidget(new QLabel(QStringLiteral("ms"), row));
    return row;
}

void WaitEditor::setupUi() {
    auto* layout = new QVBoxLayout(this);
    layout->setSpacing(10);

    auto* delayGroup = new QGroupBox(tr("대기 시간"), this);
    delayGroup->setToolTip(tr("고정 또는 랜덤 범위로 워크플로를 일시 정지합니다."));
    auto* delayLayout = new QVBoxLayout(delayGroup);
    delayLayout->setSpacing(8);

    m_modeSwitch = new AnimatedTwoWaySwitch(tr("고정"), tr("랜덤"), delayGroup);

    auto* switchRow = new QHBoxLayout();
    switchRow->addStretch();
    switchRow->addWidget(m_modeSwitch);
    switchRow->addStretch();
    delayLayout->addLayout(switchRow);

    m_msSpin = new DragAdjustSpinBox(this);
    configureWaitMsSpin(m_msSpin);
    m_msSpin->setValue(snapWaitDelayMs(m_block->ms));

    auto* fixedInputRow = new QWidget(this);
    auto* fixedLayout = new QHBoxLayout(fixedInputRow);
    fixedLayout->setContentsMargins(0, 0, 0, 0);
    fixedLayout->setSpacing(0);
    fixedLayout->addStretch();
    fixedLayout->addWidget(makeMsInputRow(m_msSpin));
    fixedLayout->addStretch();

    m_minSpin = new DragAdjustSpinBox(this);
    m_maxSpin = new DragAdjustSpinBox(this);
    configureWaitMsSpin(m_minSpin);
    configureWaitMsSpin(m_maxSpin);
    m_minSpin->setValue(snapWaitDelayMs(m_block->minMs));
    m_maxSpin->setValue(snapWaitDelayMs(m_block->maxMs));

    auto* randomInputRow = new QWidget(this);
    auto* rangeLayout = new QHBoxLayout(randomInputRow);
    rangeLayout->setContentsMargins(0, 0, 0, 0);
    rangeLayout->setSpacing(6);
    rangeLayout->addStretch();
    rangeLayout->addWidget(makeMsInputRow(m_minSpin));
    rangeLayout->addWidget(new QLabel(QStringLiteral("~"), randomInputRow));
    rangeLayout->addWidget(makeMsInputRow(m_maxSpin));
    rangeLayout->addStretch();

    m_inputStack = new QStackedWidget(delayGroup);
    m_inputStack->addWidget(fixedInputRow);
    m_inputStack->addWidget(randomInputRow);
    delayLayout->addWidget(m_inputStack);
    layout->addWidget(delayGroup);

    m_modeSwitch->setRightSelected(m_block->randomRange, false);

    connect(m_modeSwitch, &AnimatedTwoWaySwitch::rightSelectedChanged, this, &WaitEditor::updateInputUi);
    updateInputUi();

    if (!m_embedded) {
        auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
        localizeDialogButtons(buttons);
        layout->addWidget(buttons);
        connect(buttons, &QDialogButtonBox::accepted, this, [this]() {
            apply();
            accept();
        });
        connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    }
}
