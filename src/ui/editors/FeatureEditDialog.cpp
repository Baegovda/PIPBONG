#include "ui/editors/FeatureEditDialog.h"

#include "app/FeatureHotkeyGate.h"
#include "app/HotkeyManager.h"
#include "model/Project.h"
#include "model/Feature.h"
#include "ui/UiStrings.h"
#include "ui/UiThemeColors.h"
#include "ui/widgets/AnimatedTwoWaySwitch.h"

#include <QCheckBox>
#include <QAbstractButton>
#include <QApplication>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QMouseEvent>
#include <QPushButton>
#include <QShowEvent>
#include <QStackedWidget>
#include "ui/widgets/DragAdjustSpinBox.h"
#include <QVBoxLayout>

#include "core/workflow/blocks/ImageFindBlock.h"
#include "core/workflow/blocks/WaitBlock.h"

namespace {

void configureLoopIntervalSpin(DragAdjustSpinBox* spin) {
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

QWidget* makeLoopIntervalMsRow(QWidget* parent, DragAdjustSpinBox* spin) {
    auto* row = new QWidget(parent);
    auto* layout = new QHBoxLayout(row);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(4);
    spin->setMinimumWidth(88);
    layout->addWidget(spin);
    layout->addWidget(new QLabel(QStringLiteral("ms"), row));
    return row;
}

} // namespace

FeatureEditDialog::FeatureEditDialog(const QString& name,
                                     bool enabled,
                                     const HotkeyBinding& hotkey,
                                     bool hotkeyAllowExtraModifiers,
                                     FeatureRunMode runMode,
                                     int repeatCount,
                                     int infiniteExitAfterConsecutiveMisses,
                                     UserInputInterruptMode userInputInterruptMode,
                                     bool pointerVisualFeedback,
                                     bool restoreMousePositionOnEnd,
                                     bool roiCorrection,
                                     int roiCorrectionExpandPercent,
                                     bool editFirstTemplateRoiOnStart,
                                     int triggerCooldownMs,
                                     int loopIntervalMs,
                                     bool loopIntervalRandomRange,
                                     int loopIntervalMinMs,
                                     int loopIntervalMaxMs,
                                     Project* project,
                                     const std::string& featureId,
                                     QWidget* parent)
    : QDialog(parent)
    , m_project(project)
    , m_featureId(featureId)
    , m_hotkey(hotkey) {
    setWindowTitle(tr("기능 편집"));
    setModal(true);
    setupUi();

    m_nameEdit->setText(name);
    m_enabledCheck->setChecked(enabled);
    m_repeatSpin->setValue(repeatCount);

    const FeatureRunMode displayMode = runMode;
    const int modeIndex = m_modeCombo->findData(static_cast<int>(displayMode));
    if (modeIndex >= 0) {
        m_modeCombo->setCurrentIndex(modeIndex);
    }

    updateHotkeyDisplay();
    updateCaptureUi();

    if (infiniteExitAfterConsecutiveMisses > 0) {
        m_infiniteExitCheck->setChecked(true);
        m_infiniteExitSpin->setValue(infiniteExitAfterConsecutiveMisses);
    }

    const int interruptIndex =
        m_userInputInterruptCombo->findData(static_cast<int>(userInputInterruptMode));
    if (interruptIndex >= 0) {
        m_userInputInterruptCombo->setCurrentIndex(interruptIndex);
    }

    m_pointerVisualFeedbackCheck->setChecked(pointerVisualFeedback);
    m_restoreMousePositionOnEndCheck->setChecked(restoreMousePositionOnEnd);
    m_roiCorrectionCheck->setChecked(roiCorrection);
    m_roiCorrectionExpandSpin->setValue(snapRoiCorrectionExpandPercent(roiCorrectionExpandPercent));
    m_editFirstTemplateRoiOnStartCheck->setChecked(editFirstTemplateRoiOnStart);
    m_triggerCooldownSpin->setValue(snapTriggerCooldownMs(triggerCooldownMs));
    m_hotkeyAllowExtraModifiersCheck->setChecked(hotkeyAllowExtraModifiers);
    if (m_loopIntervalMsSpin) {
        m_loopIntervalMsSpin->setValue(snapWaitDelayMs(loopIntervalMs));
    }
    if (m_loopIntervalMinSpin) {
        m_loopIntervalMinSpin->setValue(snapWaitDelayMs(loopIntervalMinMs));
    }
    if (m_loopIntervalMaxSpin) {
        m_loopIntervalMaxSpin->setValue(snapWaitDelayMs(loopIntervalMaxMs));
    }
    if (m_loopIntervalModeSwitch) {
        m_loopIntervalModeSwitch->setRightSelected(loopIntervalRandomRange, false);
    }

    updateModeDependentUi();
    updateHotkeyOptionUi();
}

void FeatureEditDialog::showEvent(QShowEvent* event) {
    QDialog::showEvent(event);
    if (m_nameEdit && m_nameEdit->text().isEmpty()) {
        m_nameEdit->setFocus(Qt::OtherFocusReason);
    }
}

void FeatureEditDialog::setupUi() {
    auto* layout = new QVBoxLayout(this);

    auto* form = new QFormLayout();
    m_enabledCheck = new QCheckBox(tr("기능 사용"), this);
    m_enabledCheck->setToolTip(tr("끄면 단축키·실행 버튼으로 이 기능을 시작할 수 없습니다."));
    form->addRow(QString(), m_enabledCheck);

    m_nameEdit = new QLineEdit(this);
    m_nameEdit->setPlaceholderText(tr("기능 이름"));
    form->addRow(tr("이름"), m_nameEdit);

    auto* hotkeyRow = new QWidget(this);
    auto* hotkeyLayout = new QVBoxLayout(hotkeyRow);
    hotkeyLayout->setContentsMargins(0, 0, 0, 0);
    hotkeyLayout->setSpacing(4);

    m_hotkeyLabel = new QLabel(this);
    m_hotkeyLabel->setAlignment(Qt::AlignCenter);
    m_hotkeyLabel->setCursor(Qt::PointingHandCursor);
    m_hotkeyLabel->installEventFilter(this);

    auto* clearButton = new QPushButton(tr("단축키 지우기"), this);
    auto* hotkeyButtons = new QHBoxLayout();
    hotkeyButtons->addWidget(clearButton);
    hotkeyButtons->addStretch();

    hotkeyLayout->addWidget(m_hotkeyLabel);
    hotkeyLayout->addLayout(hotkeyButtons);

    m_hotkeyAllowExtraModifiersCheck =
        new QCheckBox(tr("다른 조합키와 함께 눌러도 실행"), this);
    m_hotkeyAllowExtraModifiersCheck->setToolTip(
        tr("단축키에 없는 Ctrl, Alt, Shift가 함께 눌려 있어도 이 기능이 실행됩니다."));
    hotkeyLayout->addWidget(m_hotkeyAllowExtraModifiersCheck);

    form->addRow(tr("단축키"), hotkeyRow);

    m_modeCombo = new QComboBox(this);
    m_modeCombo->addItem(tr("N회 반복"), static_cast<int>(FeatureRunMode::RepeatCount));
    m_modeCombo->addItem(tr("누를 동안"), static_cast<int>(FeatureRunMode::Hold));
    m_modeCombo->addItem(tr("무한 반복"), static_cast<int>(FeatureRunMode::RepeatInfinite));
    m_modeCombo->addItem(tr("트리거 모드"), static_cast<int>(FeatureRunMode::Trigger));
    m_modeCombo->setItemData(1,
                             tr("단축키를 누르고 있는 동안 워크플로를 무한 반복합니다. 키를 떼면 중지됩니다."),
                             Qt::ToolTipRole);
    m_modeCombo->setItemData(3,
                             tr("첫 번째 템플릿 매칭 블록만 상시 감시합니다. 감지되면 워크플로를 1회 실행한 뒤 "
                                "재감지 대기 시간 후 다시 감시합니다. 단축키로 감시를 켜고 끕니다."),
                             Qt::ToolTipRole);
    form->addRow(tr("동작 방식"), m_modeCombo);

    m_repeatSpin = new DragAdjustSpinBox(this);
    m_repeatSpin->setRange(1, 9999);
    m_repeatSpin->setToolTip(tr("워크플로를 실행할 횟수입니다. 1회면 한 번 실행 후 종료하며, 실행 중 같은 단축키를 누르면 중지됩니다."));
    m_repeatCountLabel = new QLabel(tr("반복 횟수"), this);
    form->addRow(m_repeatCountLabel, m_repeatSpin);

    m_infiniteExitCheck = new QCheckBox(tr("연속 감지 실패 시 종료"), this);
    m_infiniteExitCheck->setToolTip(
        tr("무한 반복·누를 동안 실행에서 템플릿 매칭 감지에 연속으로 실패하면 자동으로 중지합니다."));
    form->addRow(QString(), m_infiniteExitCheck);

    m_infiniteExitSpin = new DragAdjustSpinBox(this);
    m_infiniteExitSpin->setRange(1, 9999);
    m_infiniteExitSpin->setValue(3);
    m_infiniteExitSpin->setSuffix(tr(" 회"));
    m_infiniteExitSpin->setToolTip(tr("연속으로 감지에 실패한 루프 횟수가 이 값에 도달하면 실행을 종료합니다."));
    m_infiniteExitCountLabel = new QLabel(tr("연속 실패 허용"), this);
    form->addRow(m_infiniteExitCountLabel, m_infiniteExitSpin);

    m_loopIntervalSection = new QWidget(this);
    auto* loopIntervalLayout = new QVBoxLayout(m_loopIntervalSection);
    loopIntervalLayout->setContentsMargins(0, 0, 0, 0);
    loopIntervalLayout->setSpacing(8);

    m_loopIntervalModeSwitch = new AnimatedTwoWaySwitch(tr("고정"), tr("랜덤"), m_loopIntervalSection);
    auto* loopIntervalSwitchRow = new QHBoxLayout();
    loopIntervalSwitchRow->addStretch();
    loopIntervalSwitchRow->addWidget(m_loopIntervalModeSwitch);
    loopIntervalSwitchRow->addStretch();
    loopIntervalLayout->addLayout(loopIntervalSwitchRow);

    m_loopIntervalMsSpin = new DragAdjustSpinBox(m_loopIntervalSection);
    configureLoopIntervalSpin(m_loopIntervalMsSpin);
    auto* fixedLoopIntervalRow = new QWidget(m_loopIntervalSection);
    auto* fixedLoopIntervalLayout = new QHBoxLayout(fixedLoopIntervalRow);
    fixedLoopIntervalLayout->setContentsMargins(0, 0, 0, 0);
    fixedLoopIntervalLayout->addStretch();
    fixedLoopIntervalLayout->addWidget(makeLoopIntervalMsRow(fixedLoopIntervalRow, m_loopIntervalMsSpin));
    fixedLoopIntervalLayout->addStretch();

    m_loopIntervalMinSpin = new DragAdjustSpinBox(m_loopIntervalSection);
    m_loopIntervalMaxSpin = new DragAdjustSpinBox(m_loopIntervalSection);
    configureLoopIntervalSpin(m_loopIntervalMinSpin);
    configureLoopIntervalSpin(m_loopIntervalMaxSpin);
    auto* randomLoopIntervalRow = new QWidget(m_loopIntervalSection);
    auto* randomLoopIntervalLayout = new QHBoxLayout(randomLoopIntervalRow);
    randomLoopIntervalLayout->setContentsMargins(0, 0, 0, 0);
    randomLoopIntervalLayout->setSpacing(6);
    randomLoopIntervalLayout->addStretch();
    randomLoopIntervalLayout->addWidget(
        makeLoopIntervalMsRow(randomLoopIntervalRow, m_loopIntervalMinSpin));
    randomLoopIntervalLayout->addWidget(new QLabel(QStringLiteral("~"), randomLoopIntervalRow));
    randomLoopIntervalLayout->addWidget(
        makeLoopIntervalMsRow(randomLoopIntervalRow, m_loopIntervalMaxSpin));
    randomLoopIntervalLayout->addStretch();

    m_loopIntervalInputStack = new QStackedWidget(m_loopIntervalSection);
    m_loopIntervalInputStack->addWidget(fixedLoopIntervalRow);
    m_loopIntervalInputStack->addWidget(randomLoopIntervalRow);
    loopIntervalLayout->addWidget(m_loopIntervalInputStack);

    connect(m_loopIntervalModeSwitch,
            &AnimatedTwoWaySwitch::rightSelectedChanged,
            this,
            &FeatureEditDialog::updateLoopIntervalInputUi);

    form->addRow(tr("루프 간격"), m_loopIntervalSection);
    m_loopIntervalSection->setToolTip(
        tr("무한 반복·N회 반복(2회 이상)·누를 동안 실행에서 한 루프가 끝난 뒤 다음 루프를 시작하기 전에 대기합니다. "
           "0ms이면 즉시 다음 루프를 시작합니다."));

    m_triggerCooldownSpin = new DragAdjustSpinBox(this);
    m_triggerCooldownSpin->setRange(0, 600000);
    m_triggerCooldownSpin->setSingleStep(kTriggerCooldownStepMs);
    m_triggerCooldownSpin->setToolTip(
        tr("트리거 발동 후 같은 트리거가 바로 다시 잡히지 않도록 대기하는 시간입니다."));
    m_triggerCooldownLabel = new QLabel(tr("재감지 대기"), this);
    m_triggerCooldownRow = new QWidget(this);
    auto* triggerCooldownLayout = new QHBoxLayout(m_triggerCooldownRow);
    triggerCooldownLayout->setContentsMargins(0, 0, 0, 0);
    triggerCooldownLayout->setSpacing(4);
    triggerCooldownLayout->addWidget(m_triggerCooldownSpin);
    triggerCooldownLayout->addWidget(new QLabel(QStringLiteral("ms"), m_triggerCooldownRow));
    form->addRow(m_triggerCooldownLabel, m_triggerCooldownRow);
    connect(m_triggerCooldownSpin, &QAbstractSpinBox::editingFinished, this, [this]() {
        m_triggerCooldownSpin->setValue(snapTriggerCooldownMs(m_triggerCooldownSpin->value()));
    });

    m_userInputInterruptCombo = new QComboBox(this);
    m_userInputInterruptCombo->addItem(tr("완전 정지"), static_cast<int>(UserInputInterruptMode::Stop));
    m_userInputInterruptCombo->addItem(tr("일시정지"), static_cast<int>(UserInputInterruptMode::Pause));
    m_userInputInterruptCombo->addItem(tr("영향 없음"), static_cast<int>(UserInputInterruptMode::None));
    m_userInputInterruptCombo->setToolTip(
        tr("워크플로 실행 중 사용자가 직접 키보드를 누르거나 마우스 버튼을 클릭하면(이동 제외) "
           "일시정지하거나 완전히 정지합니다. 영향 없음은 사용자 입력을 무시합니다. "
           "기능 단축키 입력은 항상 제외됩니다."));
    form->addRow(tr("사용자 입력 시"), m_userInputInterruptCombo);

    m_pointerVisualFeedbackCheck =
        new QCheckBox(tr("실행 위치 표시"), this);
    m_pointerVisualFeedbackCheck->setToolTip(
        tr("실행 중 대상 창에 클릭·매칭 위치를 펄스로 표시합니다."));
    form->addRow(QString(), m_pointerVisualFeedbackCheck);

    m_restoreMousePositionOnEndCheck = new QCheckBox(tr("마우스 위치 복귀"), this);
    m_restoreMousePositionOnEndCheck->setToolTip(
        tr("기능 실행이 끝나면 워크플로가 시작될 때의 마우스 커서 위치로 되돌립니다."));
    form->addRow(QString(), m_restoreMousePositionOnEndCheck);

    m_roiCorrectionCheck = new QCheckBox(tr("전체 ROI 보정"), this);
    m_roiCorrectionCheck->setToolTip(
        tr("무한 반복·N회 반복(2회 이상) 실행 시 모든 템플릿 매칭 블록에 ROI 보정을 적용합니다. "
           "해제하면 워크플로 목록의 ROI 보정 열 또는 각 템플릿 매칭 블록 편집에서 블록별로 설정할 수 있습니다. "
           "보정 ROI는 실행 중에만 사용되며 저장되지 않습니다."));
    form->addRow(QString(), m_roiCorrectionCheck);

    m_roiCorrectionExpandRow = new QWidget(this);
    auto* roiCorrectionExpandLayout = new QHBoxLayout(m_roiCorrectionExpandRow);
    roiCorrectionExpandLayout->setContentsMargins(0, 0, 0, 0);
    roiCorrectionExpandLayout->setSpacing(4);
    m_roiCorrectionExpandSpin = new DragAdjustSpinBox(m_roiCorrectionExpandRow);
    m_roiCorrectionExpandSpin->setRange(kRoiCorrectionExpandPercentMin, kRoiCorrectionExpandPercentMax);
    m_roiCorrectionExpandSpin->setSingleStep(kRoiCorrectionExpandPercentStep);
    m_roiCorrectionExpandSpin->setValue(kDefaultRoiCorrectionExpandPercent);
    m_roiCorrectionExpandSpin->setMinimumWidth(72);
    m_roiCorrectionExpandSpin->setMaximumWidth(96);
    m_roiCorrectionExpandSpin->setToolTip(
        tr("두 번째 루프부터 매칭된 템플릿 크기 대비 보정 탐색 영역 비율입니다. "
           "110% = 가로·세로 각각 10% 확장(기본값)."));
    roiCorrectionExpandLayout->addWidget(new QLabel(tr("보정 영역 (템플릿 대비)"), m_roiCorrectionExpandRow));
    roiCorrectionExpandLayout->addWidget(m_roiCorrectionExpandSpin);
    roiCorrectionExpandLayout->addWidget(new QLabel(QStringLiteral("%"), m_roiCorrectionExpandRow));
    roiCorrectionExpandLayout->addStretch(1);
    form->addRow(QString(), m_roiCorrectionExpandRow);

    m_editFirstTemplateRoiOnStartCheck =
        new QCheckBox(tr("첫 시작 시 첫 번째 템플릿의 ROI 수정한 뒤 바로 시작"), this);
    m_editFirstTemplateRoiOnStartCheck->setToolTip(
        tr("기능 실행 직전에 워크플로 첫 템플릿 매칭 블록의 탐색 ROI를 대상 창에서 조정합니다. "
           "확인 후 워크플로가 시작되며, Esc는 실행을 취소합니다. "
           "탐색 ROI가 지정된 템플릿 매칭 블록이 있을 때만 동작합니다."));
    form->addRow(QString(), m_editFirstTemplateRoiOnStartCheck);

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    localizeDialogButtons(buttons);

    layout->addLayout(form);
    layout->addWidget(buttons);

    connect(clearButton, &QPushButton::clicked, this, [this]() {
        stopHotkeyCapture();
        m_hotkey = HotkeyBinding{};
        if (m_hotkeyAllowExtraModifiersCheck) {
            m_hotkeyAllowExtraModifiersCheck->setChecked(false);
        }
        updateHotkeyDisplay();
        updateCaptureUi();
        updateHotkeyOptionUi();
    });
    connect(m_modeCombo, qOverload<int>(&QComboBox::currentIndexChanged), this,
            [this](int) { updateModeDependentUi(); });
    connect(m_repeatSpin, qOverload<int>(&QSpinBox::valueChanged), this,
            [this](int) { updateModeDependentUi(); });
    connect(m_infiniteExitCheck, &QCheckBox::toggled, this, [this](bool) { updateModeDependentUi(); });
    connect(buttons, &QDialogButtonBox::accepted, this, &FeatureEditDialog::tryAccept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
}

void FeatureEditDialog::applyHotkeyLabelIdleStyle() {
    m_hotkeyLabel->setStyleSheet(hotkeyBindingLabelIdleStyleSheet(palette()));
}

void FeatureEditDialog::updateHotkeyDisplay() {
    if (m_hotkey.isEmpty()) {
        m_hotkeyLabel->setText(tr("(없음 — 클릭하여 설정)"));
    } else {
        m_hotkeyLabel->setText(m_hotkey.displayString());
    }
}

void FeatureEditDialog::updateCaptureUi() {
    if (m_listeningForHotkey) {
        m_hotkeyLabel->setText(tr("입력 대기 중..."));
        m_hotkeyLabel->setStyleSheet(hotkeyBindingLabelCaptureStyleSheet(palette()));
    } else {
        updateHotkeyDisplay();
        applyHotkeyLabelIdleStyle();
    }
}

void FeatureEditDialog::startHotkeyCapture() {
    m_hotkeyCaptureGate = std::make_unique<FeatureHotkeyGateScope>();
    m_listeningForHotkey = true;
    updateCaptureUi();
    setFocus();
}

void FeatureEditDialog::stopHotkeyCapture() {
    m_listeningForHotkey = false;
    m_hotkeyCaptureGate.reset();
    updateCaptureUi();
}

void FeatureEditDialog::updateHotkeyOptionUi() {
    if (!m_hotkeyAllowExtraModifiersCheck) {
        return;
    }
    const bool hasHotkey = !m_hotkey.isEmpty();
    m_hotkeyAllowExtraModifiersCheck->setEnabled(hasHotkey);
    if (!hasHotkey) {
        m_hotkeyAllowExtraModifiersCheck->setChecked(false);
    }
}

void FeatureEditDialog::updateModeDependentUi() {
    const auto mode = static_cast<FeatureRunMode>(m_modeCombo->currentData().toInt());
    const bool repeatCountMode = mode == FeatureRunMode::RepeatCount;
    const bool infiniteStyle = mode == FeatureRunMode::RepeatInfinite || mode == FeatureRunMode::Hold;
    const bool triggerMode = mode == FeatureRunMode::Trigger;
    const bool loopIntervalEligible = infiniteStyle || (repeatCountMode && m_repeatSpin->value() >= 2);

    if (m_repeatCountLabel) {
        m_repeatCountLabel->setVisible(repeatCountMode);
    }
    m_repeatSpin->setVisible(repeatCountMode);
    m_infiniteExitCheck->setEnabled(infiniteStyle);
    m_infiniteExitCheck->setVisible(infiniteStyle);

    const bool showInfiniteExitCount = infiniteStyle && m_infiniteExitCheck->isChecked();
    if (m_infiniteExitCountLabel) {
        m_infiniteExitCountLabel->setVisible(showInfiniteExitCount);
    }
    m_infiniteExitSpin->setVisible(showInfiniteExitCount);

    if (m_loopIntervalSection) {
        m_loopIntervalSection->setVisible(loopIntervalEligible);
    }
    if (loopIntervalEligible) {
        updateLoopIntervalInputUi();
    }

    if (m_triggerCooldownLabel) {
        m_triggerCooldownLabel->setVisible(triggerMode);
    }
    if (m_triggerCooldownRow) {
        m_triggerCooldownRow->setVisible(triggerMode);
    }

    const bool roiCorrectionEligible = mode == FeatureRunMode::RepeatInfinite
                                       || triggerMode
                                       || (repeatCountMode && m_repeatSpin->value() >= 2);
    m_roiCorrectionCheck->setVisible(roiCorrectionEligible);
    if (m_roiCorrectionExpandRow) {
        m_roiCorrectionExpandRow->setVisible(roiCorrectionEligible);
    }

    adjustSize();
}

void FeatureEditDialog::updateLoopIntervalInputUi() {
    if (!m_loopIntervalInputStack || !m_loopIntervalModeSwitch) {
        return;
    }
    m_loopIntervalInputStack->setCurrentIndex(m_loopIntervalModeSwitch->isRightSelected() ? 1 : 0);
}

void FeatureEditDialog::applyCapturedBinding(int virtualKey, Qt::KeyboardModifiers modifiers) {
    if (virtualKey == 0 || HotkeyBinding::isModifierOnlyVirtualKey(virtualKey)) {
        return;
    }

    m_hotkey.virtualKey = virtualKey;
    if (HotkeyBinding::isMouseVirtualKey(virtualKey)) {
        m_hotkey.ctrl = false;
        m_hotkey.alt = false;
        m_hotkey.shift = false;
    } else {
        m_hotkey.ctrl = modifiers.testFlag(Qt::ControlModifier);
        m_hotkey.alt = modifiers.testFlag(Qt::AltModifier);
        m_hotkey.shift = modifiers.testFlag(Qt::ShiftModifier);
    }
    stopHotkeyCapture();
    updateHotkeyOptionUi();
}

bool FeatureEditDialog::eventFilter(QObject* watched, QEvent* event) {
    if (watched == m_hotkeyLabel && event->type() == QEvent::MouseButtonPress && !m_listeningForHotkey) {
        startHotkeyCapture();
        return true;
    }
    return QDialog::eventFilter(watched, event);
}

void FeatureEditDialog::tryAccept() {
    stopHotkeyCapture();

    const QString name = m_nameEdit->text().trimmed();
    if (name.isEmpty()) {
        QMessageBox::warning(this, tr("기능 편집"), tr("기능 이름을 입력하세요."));
        return;
    }

    if (!m_hotkey.isEmpty() && m_project) {
        if (const Feature* duplicate =
                HotkeyManager::findDuplicateHotkey(*m_project, m_featureId, m_hotkey)) {
            QMessageBox::warning(
                this,
                tr("기능 편집"),
                tr("'%1' 기능에서 이미 '%2' 단축키를 사용 중입니다.")
                    .arg(QString::fromStdString(duplicate->name()))
                    .arg(m_hotkey.displayString()));
            return;
        }
    }

    accept();
}

bool FeatureEditDialog::isInteractiveWidget(const QWidget* widget) const {
    while (widget && widget != this) {
        if (widget == m_nameEdit || widget == m_modeCombo || widget == m_repeatSpin
            || widget == m_infiniteExitCheck || widget == m_infiniteExitSpin
            || widget == m_loopIntervalMsSpin || widget == m_loopIntervalMinSpin
            || widget == m_loopIntervalMaxSpin || widget == m_loopIntervalModeSwitch
            || widget == m_triggerCooldownSpin
            || widget == m_userInputInterruptCombo || widget == m_pointerVisualFeedbackCheck
            || widget == m_restoreMousePositionOnEndCheck || widget == m_roiCorrectionCheck
            || widget == m_editFirstTemplateRoiOnStartCheck
            || widget == m_hotkeyAllowExtraModifiersCheck) {
            return true;
        }
        if (qobject_cast<const QAbstractButton*>(widget)) {
            return true;
        }
        widget = widget->parentWidget();
    }
    return false;
}

QString FeatureEditDialog::featureName() const {
    return m_nameEdit->text().trimmed();
}

bool FeatureEditDialog::featureEnabled() const {
    return m_enabledCheck && m_enabledCheck->isChecked();
}

HotkeyBinding FeatureEditDialog::hotkey() const {
    return m_hotkey;
}

bool FeatureEditDialog::hotkeyAllowExtraModifiers() const {
    return m_hotkeyAllowExtraModifiersCheck && m_hotkeyAllowExtraModifiersCheck->isChecked();
}

FeatureRunMode FeatureEditDialog::runMode() const {
    return static_cast<FeatureRunMode>(m_modeCombo->currentData().toInt());
}

int FeatureEditDialog::repeatCount() const {
    return m_repeatSpin->value();
}

int FeatureEditDialog::infiniteExitAfterConsecutiveMisses() const {
    const auto mode = static_cast<FeatureRunMode>(m_modeCombo->currentData().toInt());
    if (mode != FeatureRunMode::RepeatInfinite && mode != FeatureRunMode::Hold) {
        return 0;
    }
    if (!m_infiniteExitCheck->isChecked()) {
        return 0;
    }
    return m_infiniteExitSpin->value();
}

UserInputInterruptMode FeatureEditDialog::userInputInterruptMode() const {
    return static_cast<UserInputInterruptMode>(m_userInputInterruptCombo->currentData().toInt());
}

bool FeatureEditDialog::pointerVisualFeedback() const {
    return m_pointerVisualFeedbackCheck->isChecked();
}

bool FeatureEditDialog::restoreMousePositionOnEnd() const {
    return m_restoreMousePositionOnEndCheck->isChecked();
}

bool FeatureEditDialog::roiCorrection() const {
    return m_roiCorrectionCheck->isChecked();
}

int FeatureEditDialog::roiCorrectionExpandPercent() const {
    return m_roiCorrectionExpandSpin ? m_roiCorrectionExpandSpin->value()
                                       : kDefaultRoiCorrectionExpandPercent;
}

bool FeatureEditDialog::editFirstTemplateRoiOnStart() const {
    return m_editFirstTemplateRoiOnStartCheck->isChecked();
}

int FeatureEditDialog::triggerCooldownMs() const {
    return snapTriggerCooldownMs(m_triggerCooldownSpin->value());
}

int FeatureEditDialog::loopIntervalMs() const {
    return m_loopIntervalMsSpin ? snapWaitDelayMs(m_loopIntervalMsSpin->value()) : 0;
}

bool FeatureEditDialog::loopIntervalRandomRange() const {
    return m_loopIntervalModeSwitch && m_loopIntervalModeSwitch->isRightSelected();
}

int FeatureEditDialog::loopIntervalMinMs() const {
    return m_loopIntervalMinSpin ? snapWaitDelayMs(m_loopIntervalMinSpin->value()) : 0;
}

int FeatureEditDialog::loopIntervalMaxMs() const {
    return m_loopIntervalMaxSpin ? snapWaitDelayMs(m_loopIntervalMaxSpin->value()) : 0;
}

void FeatureEditDialog::reject() {
    stopHotkeyCapture();
    QDialog::reject();
}

void FeatureEditDialog::keyPressEvent(QKeyEvent* event) {
    if (!m_listeningForHotkey) {
        QDialog::keyPressEvent(event);
        return;
    }

    if (event->key() == Qt::Key_Escape) {
        stopHotkeyCapture();
        event->accept();
        return;
    }

#ifdef _WIN32
    applyCapturedBinding(HotkeyBinding::qtKeyToVirtualKey(event->key()), event->modifiers());
    event->accept();
#else
    QDialog::keyPressEvent(event);
#endif
}

void FeatureEditDialog::mousePressEvent(QMouseEvent* event) {
    if (!m_listeningForHotkey || isInteractiveWidget(childAt(event->pos()))) {
        QDialog::mousePressEvent(event);
        return;
    }

#ifdef _WIN32
    const int vk = HotkeyBinding::qtMouseButtonToVirtualKey(event->button());
    if (vk == 0) {
        QDialog::mousePressEvent(event);
        return;
    }

    applyCapturedBinding(vk, QApplication::keyboardModifiers());
    event->accept();
#else
    QDialog::mousePressEvent(event);
#endif
}
