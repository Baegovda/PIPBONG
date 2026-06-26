#include "ui/editors/FeatureRunModeDialog.h"

#include "ui/UiStrings.h"

#include <QComboBox>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QLabel>
#include "ui/widgets/DragAdjustSpinBox.h"
#include <QVBoxLayout>

FeatureRunModeDialog::FeatureRunModeDialog(FeatureRunMode mode, int repeatCount, QWidget* parent)
    : QDialog(parent)
    , m_mode(normalizeRunMode(mode))
    , m_repeatCount(repeatCount) {
    setWindowTitle(tr("동작 방식"));
    setModal(true);
    setupUi();
}

void FeatureRunModeDialog::setupUi() {
    auto* layout = new QVBoxLayout(this);

    auto* form = new QFormLayout();
    m_modeCombo = new QComboBox(this);
    m_modeCombo->addItem(tr("N회 반복"), static_cast<int>(FeatureRunMode::RepeatCount));
    m_modeCombo->addItem(tr("누를 동안"), static_cast<int>(FeatureRunMode::Hold));
    m_modeCombo->addItem(tr("무한 반복"), static_cast<int>(FeatureRunMode::RepeatInfinite));

    const int modeIndex = m_modeCombo->findData(static_cast<int>(m_mode));
    if (modeIndex >= 0) {
        m_modeCombo->setCurrentIndex(modeIndex);
    }

    m_repeatSpin = new DragAdjustSpinBox(this);
    m_repeatSpin->setRange(1, 9999);
    m_repeatSpin->setValue(m_repeatCount);
    m_repeatSpin->setToolTip(tr("워크플로를 실행할 횟수입니다. 1회면 한 번 실행 후 종료하며, 실행 중 같은 단축키를 누르면 중지됩니다."));

    form->addRow(tr("방식"), m_modeCombo);
    m_repeatCountLabel = new QLabel(tr("반복 횟수"), this);
    form->addRow(m_repeatCountLabel, m_repeatSpin);

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    localizeDialogButtons(buttons);

    layout->addLayout(form);
    layout->addWidget(buttons);

    connect(m_modeCombo, qOverload<int>(&QComboBox::currentIndexChanged), this,
            [this](int) { updateRepeatCountVisibility(); });
    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);

    updateRepeatCountVisibility();
}

void FeatureRunModeDialog::updateRepeatCountVisibility() {
    const auto mode = static_cast<FeatureRunMode>(m_modeCombo->currentData().toInt());
    const bool repeatCountMode = mode == FeatureRunMode::RepeatCount;
    if (m_repeatCountLabel) {
        m_repeatCountLabel->setVisible(repeatCountMode);
    }
    m_repeatSpin->setVisible(repeatCountMode);
    adjustSize();
}

FeatureRunMode FeatureRunModeDialog::runMode() const {
    return static_cast<FeatureRunMode>(m_modeCombo->currentData().toInt());
}

int FeatureRunModeDialog::repeatCount() const {
    return m_repeatSpin->value();
}
