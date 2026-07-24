#include "ui/editors/FeatureEditDialog.h"

#include "ui/FeatureRunModeTheme.h"

#include "app/FeatureHotkeyGate.h"
#include "app/HotkeyManager.h"
#include "model/Project.h"
#include "model/Feature.h"
#include "model/FeatureCaptureTargetScope.h"
#include "ui/TriggerModeAnimationSettingsDialog.h"
#include "ui/UiSettingsLayout.h"
#include "ui/UiStrings.h"
#include "ui/UiThemeColors.h"
#include "ui/widgets/AnimatedTwoWaySwitch.h"

#include <QCheckBox>
#include <QAbstractButton>
#include <QApplication>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QFrame>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QMouseEvent>
#include <QPushButton>
#include <QScrollArea>
#include <QShowEvent>
#include <QStackedWidget>

#include <algorithm>
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
                                     FeatureCaptureTargetScope captureTargetScope,
                                     bool requireScopedTargetForeground,
                                     bool triggerRunWithoutTargetForeground,
                                     FeatureRunMode runMode,
                                     int repeatCount,
                                     int infiniteExitAfterConsecutiveMisses,
                                     UserInputInterruptMode userInputInterruptMode,
                                     bool pointerVisualFeedback,
                                     bool restoreMousePositionOnEnd,
                                     int lockMouseDuringFirstLoopCount,
                                     int unlockMouseOnBlockFailureCount,
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
    , m_hotkey(hotkey)
    , m_featureHotkeyGate() {
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
    if (lockMouseDuringFirstLoopCount > 0) {
        m_earlyLoopMouseLockCheck->setChecked(true);
        m_earlyLoopMouseLockLoopSpin->setValue(lockMouseDuringFirstLoopCount);
        m_earlyLoopMouseUnlockFailureSpin->setValue(
            unlockMouseOnBlockFailureCount > 0 ? unlockMouseOnBlockFailureCount : 1);
    }
    m_roiCorrectionCheck->setChecked(roiCorrection);
    m_roiCorrectionExpandSpin->setValue(snapRoiCorrectionExpandPercent(roiCorrectionExpandPercent));
    m_editFirstTemplateRoiOnStartCheck->setChecked(editFirstTemplateRoiOnStart);
    m_triggerCooldownSpin->setValue(triggerCooldownSecondsFromMs(triggerCooldownMs));
    m_hotkeyAllowExtraModifiersCheck->setChecked(hotkeyAllowExtraModifiers);
    const int captureScopeIndex =
        m_captureTargetScopeCombo->findData(static_cast<int>(captureTargetScope));
    if (captureScopeIndex >= 0) {
        m_captureTargetScopeCombo->setCurrentIndex(captureScopeIndex);
    }
    if (m_requireScopedTargetForegroundCheck) {
        m_requireScopedTargetForegroundCheck->setChecked(requireScopedTargetForeground);
    }
    if (m_triggerRunWithoutTargetForegroundCheck) {
        m_triggerRunWithoutTargetForegroundCheck->setChecked(triggerRunWithoutTargetForeground);
    }
    updateScopedTargetForegroundUi();
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
    auto* rootLayout = new QVBoxLayout(this);
    rootLayout->setSpacing(10);

    auto* scroll = new QScrollArea(this);
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    auto* scrollContent = new QWidget(scroll);
    auto* contentLayout = new QVBoxLayout(scrollContent);
    contentLayout->setSpacing(10);
    scroll->setWidget(scrollContent);

    // --- 기본 ---
    auto* basicGroup = new QGroupBox(tr("기본"), scrollContent);
    basicGroup->setToolTip(tr("기능 이름과 사용 여부입니다."));
    auto* basicForm = new QFormLayout(basicGroup);
    basicForm->setSpacing(6);
    m_enabledCheck = new QCheckBox(tr("기능 사용"), basicGroup);
    m_enabledCheck->setToolTip(tr("끄면 단축키·실행 버튼으로 이 기능을 시작할 수 없습니다."));
    basicForm->addRow(QString(), m_enabledCheck);
    m_nameEdit = new QLineEdit(basicGroup);
    m_nameEdit->setPlaceholderText(tr("기능 이름"));
    basicForm->addRow(tr("이름"), m_nameEdit);
    contentLayout->addWidget(basicGroup);

    // --- 단축키 ---
    auto* hotkeyGroup = new QGroupBox(tr("단축키"), scrollContent);
    hotkeyGroup->setToolTip(tr("이 기능을 시작·중지할 전역 단축키입니다."));
    auto* hotkeyGroupLayout = new QVBoxLayout(hotkeyGroup);
    hotkeyGroupLayout->setSpacing(6);

    auto* hotkeyRow = new QWidget(hotkeyGroup);
    auto* hotkeyLayout = new QVBoxLayout(hotkeyRow);
    hotkeyLayout->setContentsMargins(0, 0, 0, 0);
    hotkeyLayout->setSpacing(4);

    m_hotkeyLabel = new QLabel(hotkeyRow);
    m_hotkeyLabel->setAlignment(Qt::AlignCenter);
    m_hotkeyLabel->setCursor(Qt::PointingHandCursor);
    m_hotkeyLabel->installEventFilter(this);

    auto* clearButton = new QPushButton(tr("단축키 지우기"), hotkeyRow);
    auto* hotkeyButtons = new QHBoxLayout();
    hotkeyButtons->addWidget(clearButton);
    hotkeyButtons->addStretch();

    hotkeyLayout->addWidget(m_hotkeyLabel);
    hotkeyLayout->addLayout(hotkeyButtons);

    m_hotkeyAllowExtraModifiersCheck =
        new QCheckBox(tr("다른 조합키와 함께 눌러도 실행"), hotkeyRow);
    m_hotkeyAllowExtraModifiersCheck->setToolTip(
        tr("단축키에 없는 Ctrl, Alt, Shift가 함께 눌려 있어도 이 기능이 실행됩니다."));
    hotkeyLayout->addWidget(m_hotkeyAllowExtraModifiersCheck);
    hotkeyGroupLayout->addWidget(hotkeyRow);
    contentLayout->addWidget(hotkeyGroup);

    // --- 타겟 ---
    auto* targetGroup = new QGroupBox(tr("타겟"), scrollContent);
    targetGroup->setToolTip(
        tr("이 기능이 동작할 창 범위입니다. 프로필의 메인·서브 타겟과 함께 사용합니다."));
    auto* targetForm = new QFormLayout(targetGroup);
    targetForm->setSpacing(6);

    m_captureTargetScopeCombo = new QComboBox(targetGroup);
    m_captureTargetScopeCombo->addItem(tr("자동 (메인·서브)"),
                                       static_cast<int>(FeatureCaptureTargetScope::Auto));
    m_captureTargetScopeCombo->addItem(tr("메인 창만"),
                                       static_cast<int>(FeatureCaptureTargetScope::MainOnly));
    m_captureTargetScopeCombo->addItem(tr("서브 창만"),
                                       static_cast<int>(FeatureCaptureTargetScope::SubOnly));
    m_captureTargetScopeCombo->setToolTip(
        tr("자동은 포커스·실행 중인 창에 따라 메인 또는 서브를 선택합니다."));
    targetForm->addRow(tr("대상 창"), m_captureTargetScopeCombo);

    m_requireScopedTargetForegroundCheck =
        new QCheckBox(tr("지정한 타겟이 활성(포커스)일 때만 동작"), targetGroup);
    m_requireScopedTargetForegroundCheck->setToolTip(
        tr("메인 창만·서브 창만을 선택했을 때, 해당 창이 포커스되어 있지 않으면 감시·실행을 하지 않습니다."));
    targetForm->addRow(QString(), m_requireScopedTargetForegroundCheck);
    contentLayout->addWidget(targetGroup);

    // --- 동작 방식 ---
    auto* modeGroup = new QGroupBox(tr("동작 방식"), scrollContent);
    modeGroup->setToolTip(tr("반복·홀드·트리거 등 실행 방식과 루프 간격입니다."));
    auto* modeForm = new QFormLayout(modeGroup);
    modeForm->setSpacing(6);

    m_modeCombo = new QComboBox(modeGroup);
    m_modeCombo->addItem(tr("N회 반복"), static_cast<int>(FeatureRunMode::RepeatCount));
    m_modeCombo->addItem(tr("홀드"), static_cast<int>(FeatureRunMode::Hold));
    m_modeCombo->addItem(tr("무한 반복"), static_cast<int>(FeatureRunMode::RepeatInfinite));
    m_modeCombo->addItem(tr("트리거 모드"), static_cast<int>(FeatureRunMode::Trigger));
    m_modeCombo->setItemData(1,
                             tr("단축키를 누르고 있는 동안 워크플로를 무한 반복합니다. 키를 떼면 중지됩니다."),
                             Qt::ToolTipRole);
    m_modeCombo->setItemData(3,
                             tr("첫 번째 템플릿 매칭 블록만 상시 감시합니다. 감지되면 워크플로를 1회 실행하고, "
                                "성공한 뒤 쿨다운 시간이 지나면 다시 감시합니다. 단축키로 감시를 켜고 끕니다."),
                             Qt::ToolTipRole);
    m_modePreviewChip = new QLabel(modeGroup);
    m_modePreviewChip->setAlignment(Qt::AlignCenter);
    m_modePreviewChip->setFixedWidth(52);
    auto* modeRow = new QWidget(modeGroup);
    auto* modeRowLayout = new QHBoxLayout(modeRow);
    modeRowLayout->setContentsMargins(0, 0, 0, 0);
    modeRowLayout->setSpacing(8);
    modeRowLayout->addWidget(m_modePreviewChip, 0);
    modeRowLayout->addWidget(m_modeCombo, 1);
    modeForm->addRow(tr("방식"), modeRow);

    m_repeatSpin = new DragAdjustSpinBox(modeGroup);
    m_repeatSpin->setRange(1, 9999);
    m_repeatSpin->setToolTip(tr("워크플로를 실행할 횟수입니다. 1회면 한 번 실행 후 종료하며, 실행 중 같은 단축키를 누르면 중지됩니다."));
    m_repeatCountLabel = new QLabel(tr("반복 횟수"), modeGroup);
    modeForm->addRow(m_repeatCountLabel, m_repeatSpin);

    m_infiniteExitCheck = new QCheckBox(tr("연속 감지 실패 시 종료"), modeGroup);
    m_infiniteExitCheck->setToolTip(
        tr("무한 반복·홀드 실행에서 템플릿 매칭 감지에 연속으로 실패하면 자동으로 중지합니다."));
    modeForm->addRow(QString(), m_infiniteExitCheck);

    m_infiniteExitSpin = new DragAdjustSpinBox(modeGroup);
    m_infiniteExitSpin->setRange(1, 9999);
    m_infiniteExitSpin->setValue(3);
    m_infiniteExitSpin->setSuffix(tr(" 회"));
    m_infiniteExitSpin->setToolTip(tr("연속으로 감지에 실패한 루프 횟수가 이 값에 도달하면 실행을 종료합니다."));
    m_infiniteExitCountLabel = new QLabel(tr("연속 실패 허용"), modeGroup);
    modeForm->addRow(m_infiniteExitCountLabel, m_infiniteExitSpin);

    m_loopIntervalSection = new QWidget(modeGroup);
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

    modeForm->addRow(tr("루프 간격"), m_loopIntervalSection);
    m_loopIntervalSection->setToolTip(
        tr("한 루프(워크플로 전체 1회)가 끝난 뒤, 다음 루프를 시작하기 전에 대기합니다. "
           "블록 사이 대기가 아니라 사이클 사이 대기입니다. "
           "고정/랜덤, 0ms면 즉시 다음 루프. "
           "무한 반복·N회 반복(2회 이상)·홀드에서 사용합니다."));

    m_triggerCooldownSpin = new DragAdjustSpinBox(modeGroup);
    m_triggerCooldownSpin->setRange(0, kTriggerCooldownMaxSeconds);
    m_triggerCooldownSpin->setSingleStep(1);
    m_triggerCooldownSpin->setToolTip(
        tr("워크플로 1회 실행이 성공한 뒤, 다음 트리거 감시를 시작하기 전에 대기하는 시간(초)입니다. "
           "블록 편집기의 「탐지 재시도」 간격(감시 중 매칭 실패 시 재탐색)과는 별개입니다. "
           "0초면 성공 직후 바로 다시 감시합니다."));
    m_triggerCooldownLabel = new QLabel(tr("쿨다운"), modeGroup);
    m_triggerCooldownRow = new QWidget(modeGroup);
    auto* triggerCooldownLayout = new QHBoxLayout(m_triggerCooldownRow);
    triggerCooldownLayout->setContentsMargins(0, 0, 0, 0);
    triggerCooldownLayout->setSpacing(4);
    triggerCooldownLayout->addWidget(m_triggerCooldownSpin);
    triggerCooldownLayout->addWidget(new QLabel(tr("초"), m_triggerCooldownRow));
    modeForm->addRow(m_triggerCooldownLabel, m_triggerCooldownRow);
    connect(m_triggerCooldownSpin, &QAbstractSpinBox::editingFinished, this, [this]() {
        m_triggerCooldownSpin->setValue(
            std::clamp(m_triggerCooldownSpin->value(), 0, kTriggerCooldownMaxSeconds));
    });

    m_triggerRunWithoutTargetForegroundCheck =
        new QCheckBox(tr("타겟이 포커스가 아니어도 감지하고 동작"), modeGroup);
    m_triggerRunWithoutTargetForegroundCheck->setToolTip(
        tr("트리거 모드에서 타겟 창이 화면에 보이기만 하면 포커스가 없어도 감시·실행합니다. "
           "다른 창을 사용하는 동안에도 백그라운드로 템플릿을 찾습니다. "
           "「지정한 타겟이 활성(포커스)일 때만 동작」과 함께 켤 수 없습니다."));
    modeForm->addRow(QString(), m_triggerRunWithoutTargetForegroundCheck);
    connect(m_triggerRunWithoutTargetForegroundCheck, &QCheckBox::toggled, this, [this](bool checked) {
        if (checked && m_requireScopedTargetForegroundCheck
            && m_requireScopedTargetForegroundCheck->isChecked()) {
            m_requireScopedTargetForegroundCheck->setChecked(false);
        }
        updateScopedTargetForegroundUi();
    });
    connect(m_requireScopedTargetForegroundCheck, &QCheckBox::toggled, this, [this](bool checked) {
        if (checked && m_triggerRunWithoutTargetForegroundCheck
            && m_triggerRunWithoutTargetForegroundCheck->isChecked()) {
            m_triggerRunWithoutTargetForegroundCheck->setChecked(false);
        }
    });

    m_triggerAnimationRow = new QWidget(modeGroup);
    auto* triggerAnimationLayout = new QVBoxLayout(m_triggerAnimationRow);
    triggerAnimationLayout->setContentsMargins(0, 0, 0, 0);
    triggerAnimationLayout->setSpacing(4);
    m_triggerAnimationSummary = new QLabel(m_triggerAnimationRow);
    m_triggerAnimationSummary->setWordWrap(true);
    auto* triggerAnimationButtonRow = new QHBoxLayout();
    m_triggerAnimationButton = new QPushButton(tr("애니메이션 설정…"), m_triggerAnimationRow);
    triggerAnimationButtonRow->addWidget(m_triggerAnimationButton);
    triggerAnimationButtonRow->addStretch();
    triggerAnimationLayout->addWidget(m_triggerAnimationSummary);
    triggerAnimationLayout->addLayout(triggerAnimationButtonRow);
    m_triggerAnimationRow->setToolTip(
        tr("기능 목록에서 트리거 감시·쿨다운·동작 상태별로 표시되는 아이콘과 강조 애니메이션을 설정합니다."));
    modeForm->addRow(tr("목록 표시"), m_triggerAnimationRow);
    connect(m_triggerAnimationButton,
            &QPushButton::clicked,
            this,
            &FeatureEditDialog::openTriggerAnimationSettings);
    updateTriggerAnimationSummary();
    contentLayout->addWidget(modeGroup);

    // --- 실행 중 동작 ---
    auto* runBehaviorGroup = new QGroupBox(tr("실행 중 동작"), scrollContent);
    runBehaviorGroup->setToolTip(tr("워크플로 실행 중 사용자 입력·화면 표시 동작입니다."));
    auto* runBehaviorForm = new QFormLayout(runBehaviorGroup);
    runBehaviorForm->setSpacing(6);

    m_userInputInterruptCombo = new QComboBox(runBehaviorGroup);
    m_userInputInterruptCombo->addItem(tr("완전 정지"), static_cast<int>(UserInputInterruptMode::Stop));
    m_userInputInterruptCombo->addItem(tr("일시정지"), static_cast<int>(UserInputInterruptMode::Pause));
    m_userInputInterruptCombo->addItem(tr("영향 없음"), static_cast<int>(UserInputInterruptMode::None));
    m_userInputInterruptCombo->setToolTip(
        tr("워크플로 실행 중 사용자가 직접 키보드를 누르거나 마우스 버튼을 클릭하면(이동 제외) "
           "일시정지하거나 완전히 정지합니다. 영향 없음은 사용자 입력을 무시합니다. "
           "기능 단축키 입력은 항상 제외됩니다."));
    runBehaviorForm->addRow(tr("사용자 입력 시"), m_userInputInterruptCombo);

    m_pointerVisualFeedbackCheck = new QCheckBox(tr("실행 위치 표시"), runBehaviorGroup);
    m_pointerVisualFeedbackCheck->setToolTip(tr("실행 중 타겟에 클릭·감지 위치를 펄스로 표시합니다."));
    runBehaviorForm->addRow(QString(), m_pointerVisualFeedbackCheck);

    m_restoreMousePositionOnEndCheck = new QCheckBox(tr("마우스 위치 복귀"), runBehaviorGroup);
    m_restoreMousePositionOnEndCheck->setToolTip(
        tr("기능 실행이 끝나면 워크플로가 시작될 때의 마우스 커서 위치로 되돌립니다."));
    runBehaviorForm->addRow(QString(), m_restoreMousePositionOnEndCheck);
    contentLayout->addWidget(runBehaviorGroup);

    // --- 마우스·탐색 ---
    auto* mouseRoiGroup = new QGroupBox(tr("마우스·탐색"), scrollContent);
    mouseRoiGroup->setToolTip(tr("마우스 잠금과 템플릿 탐색 ROI 보정입니다."));
    auto* mouseRoiForm = new QFormLayout(mouseRoiGroup);
    mouseRoiForm->setSpacing(6);

    m_earlyLoopMouseLockCheck = new QCheckBox(tr("초기 루프 마우스 잠금"), mouseRoiGroup);
    m_earlyLoopMouseLockCheck->setToolTip(
        tr("처음 N회 루프 동안 마우스를 직전 템플릿 매칭 위치에 고정해 커서가 어긋나지 않게 합니다. "
           "매칭 성공마다 잠금 위치가 갱신됩니다. 잠금 구간 안에서 어떤 블록이든 실패가 누적되면 해제됩니다. "
           "트리거 모드에서는 감시·쿨다운 중에는 잠기지 않고, 매칭 후 워크플로 동작 페이즈에서만 적용됩니다."));
    mouseRoiForm->addRow(QString(), m_earlyLoopMouseLockCheck);

    m_earlyLoopMouseLockDetailsRow = new QWidget(mouseRoiGroup);
    auto* earlyLockLayout = new QHBoxLayout(m_earlyLoopMouseLockDetailsRow);
    earlyLockLayout->setContentsMargins(0, 0, 0, 0);
    earlyLockLayout->setSpacing(8);

    m_earlyLoopMouseLockLoopSpin = new DragAdjustSpinBox(m_earlyLoopMouseLockDetailsRow);
    m_earlyLoopMouseLockLoopSpin->setRange(1, 9999);
    m_earlyLoopMouseLockLoopSpin->setValue(3);
    m_earlyLoopMouseLockLoopSpin->setSuffix(tr(" 루프"));
    m_earlyLoopMouseLockLoopSpin->setToolTip(tr("마우스를 잠글 초기 루프 횟수입니다."));

    m_earlyLoopMouseUnlockFailureSpin = new DragAdjustSpinBox(m_earlyLoopMouseLockDetailsRow);
    m_earlyLoopMouseUnlockFailureSpin->setRange(1, 9999);
    m_earlyLoopMouseUnlockFailureSpin->setValue(1);
    m_earlyLoopMouseUnlockFailureSpin->setSuffix(tr(" 회"));
    m_earlyLoopMouseUnlockFailureSpin->setToolTip(
        tr("잠금 구간(초기 N루프) 안에서 어떤 블록이든 실패가 이 횟수에 도달하면 마우스 잠금을 해제합니다."));

    earlyLockLayout->addWidget(new QLabel(tr("루프"), m_earlyLoopMouseLockDetailsRow));
    earlyLockLayout->addWidget(m_earlyLoopMouseLockLoopSpin);
    earlyLockLayout->addSpacing(8);
    earlyLockLayout->addWidget(new QLabel(tr("실패 시 해제"), m_earlyLoopMouseLockDetailsRow));
    earlyLockLayout->addWidget(m_earlyLoopMouseUnlockFailureSpin);
    earlyLockLayout->addStretch();
    mouseRoiForm->addRow(QString(), m_earlyLoopMouseLockDetailsRow);

    connect(m_earlyLoopMouseLockCheck, &QCheckBox::toggled, this, [this](bool) {
        updateModeDependentUi();
    });

    m_roiCorrectionCheck = new QCheckBox(tr("전체 ROI 보정"), mouseRoiGroup);
    m_roiCorrectionCheck->setToolTip(
        tr("무한 반복·N회 반복(2회 이상) 실행 시 모든 템플릿 매칭 블록에 ROI 보정을 적용합니다. "
           "해제하면 워크플로 목록의 ROI 보정 열 또는 각 템플릿 매칭 블록 편집에서 블록별로 설정할 수 있습니다. "
           "보정 ROI는 실행 중에만 사용되며 저장되지 않습니다."));
    mouseRoiForm->addRow(QString(), m_roiCorrectionCheck);

    m_roiCorrectionExpandRow = new QWidget(mouseRoiGroup);
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
    mouseRoiForm->addRow(QString(), m_roiCorrectionExpandRow);

    m_editFirstTemplateRoiOnStartCheck =
        new QCheckBox(tr("첫 시작 시 첫 번째 템플릿의 ROI 수정한 뒤 바로 시작"), mouseRoiGroup);
    m_editFirstTemplateRoiOnStartCheck->setToolTip(
        tr("기능 실행 직전에 워크플로 첫 템플릿 매칭 블록의 탐색 ROI를 타겟에서 조정합니다. "
           "확인 후 워크플로가 시작되며, Esc는 실행을 취소합니다. "
           "탐색 ROI가 지정된 템플릿 매칭 블록이 있을 때만 동작합니다."));
    mouseRoiForm->addRow(QString(), m_editFirstTemplateRoiOnStartCheck);
    contentLayout->addWidget(mouseRoiGroup);

    contentLayout->addStretch();
    rootLayout->addWidget(scroll, 1);

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    localizeDialogButtons(buttons);
    rootLayout->addWidget(buttons);

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
    connect(m_captureTargetScopeCombo, qOverload<int>(&QComboBox::currentIndexChanged), this,
            [this](int) { updateScopedTargetForegroundUi(); });
    connect(m_repeatSpin, qOverload<int>(&QSpinBox::valueChanged), this,
            [this](int) { updateModeDependentUi(); });
    connect(m_infiniteExitCheck, &QCheckBox::toggled, this, [this](bool) { updateModeDependentUi(); });
    connect(buttons, &QDialogButtonBox::accepted, this, &FeatureEditDialog::tryAccept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);

    resize(520, 640);
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

void FeatureEditDialog::updateScopedTargetForegroundUi() {
    if (!m_requireScopedTargetForegroundCheck || !m_captureTargetScopeCombo) {
        return;
    }
    const auto scope = static_cast<FeatureCaptureTargetScope>(
        m_captureTargetScopeCombo->currentData().toInt());
    const bool fixedScope = scope != FeatureCaptureTargetScope::Auto;
    if (!fixedScope && m_requireScopedTargetForegroundCheck->isChecked()) {
        m_requireScopedTargetForegroundCheck->setChecked(false);
    }
    m_requireScopedTargetForegroundCheck->setEnabled(fixedScope);
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
    if (m_triggerAnimationRow) {
        m_triggerAnimationRow->setVisible(triggerMode);
    }
    if (m_triggerRunWithoutTargetForegroundCheck) {
        m_triggerRunWithoutTargetForegroundCheck->setVisible(triggerMode);
    }

    const bool roiCorrectionEligible = mode == FeatureRunMode::RepeatInfinite
                                       || triggerMode
                                       || (repeatCountMode && m_repeatSpin->value() >= 2);
    m_roiCorrectionCheck->setVisible(roiCorrectionEligible);
    if (m_roiCorrectionExpandRow) {
        m_roiCorrectionExpandRow->setVisible(roiCorrectionEligible);
    }

    if (m_earlyLoopMouseLockDetailsRow) {
        m_earlyLoopMouseLockDetailsRow->setVisible(
            m_earlyLoopMouseLockCheck && m_earlyLoopMouseLockCheck->isChecked());
    }

    if (m_modePreviewChip) {
        applyFeatureRunModeChipLabel(m_modePreviewChip, mode, m_repeatSpin->value());
    }
    applyFeatureRunModeComboAccent(m_modeCombo, mode);
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

    // Commit in-progress spin edits (typing without Enter left the old value).
    if (m_loopIntervalMsSpin) {
        m_loopIntervalMsSpin->interpretText();
    }
    if (m_loopIntervalMinSpin) {
        m_loopIntervalMinSpin->interpretText();
    }
    if (m_loopIntervalMaxSpin) {
        m_loopIntervalMaxSpin->interpretText();
    }
    if (m_repeatSpin) {
        m_repeatSpin->interpretText();
    }
    if (m_triggerCooldownSpin) {
        m_triggerCooldownSpin->interpretText();
    }
    if (m_infiniteExitSpin) {
        m_infiniteExitSpin->interpretText();
    }
    if (m_earlyLoopMouseLockLoopSpin) {
        m_earlyLoopMouseLockLoopSpin->interpretText();
    }
    if (m_earlyLoopMouseUnlockFailureSpin) {
        m_earlyLoopMouseUnlockFailureSpin->interpretText();
    }

    if (m_loopIntervalMinSpin && m_loopIntervalMaxSpin
        && m_loopIntervalMaxSpin->value() < m_loopIntervalMinSpin->value()) {
        const int minValue = m_loopIntervalMinSpin->value();
        const int maxValue = m_loopIntervalMaxSpin->value();
        m_loopIntervalMinSpin->setValue(maxValue);
        m_loopIntervalMaxSpin->setValue(minValue);
    }

    // Random selected with 0~0 and a fixed value set: seed random bounds from fixed
    // so switching 고정→랜덤 without editing min/max does not silently become 0 ms.
    if (loopIntervalRandomRange() && m_loopIntervalMinSpin && m_loopIntervalMaxSpin
        && m_loopIntervalMinSpin->value() <= 0 && m_loopIntervalMaxSpin->value() <= 0
        && m_loopIntervalMsSpin && m_loopIntervalMsSpin->value() > 0) {
        const int fixedMs = snapWaitDelayMs(m_loopIntervalMsSpin->value());
        m_loopIntervalMinSpin->setValue(fixedMs);
        m_loopIntervalMaxSpin->setValue(fixedMs);
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
            || widget == m_restoreMousePositionOnEndCheck || widget == m_earlyLoopMouseLockCheck
            || widget == m_earlyLoopMouseLockLoopSpin
            || widget == m_earlyLoopMouseUnlockFailureSpin || widget == m_roiCorrectionCheck
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

FeatureCaptureTargetScope FeatureEditDialog::captureTargetScope() const {
    if (!m_captureTargetScopeCombo) {
        return FeatureCaptureTargetScope::Auto;
    }
    return static_cast<FeatureCaptureTargetScope>(
        m_captureTargetScopeCombo->currentData().toInt());
}

bool FeatureEditDialog::requireScopedTargetForeground() const {
    return m_requireScopedTargetForegroundCheck && m_requireScopedTargetForegroundCheck->isChecked();
}

bool FeatureEditDialog::triggerRunWithoutTargetForeground() const {
    return m_triggerRunWithoutTargetForegroundCheck
           && m_triggerRunWithoutTargetForegroundCheck->isChecked();
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

int FeatureEditDialog::lockMouseDuringFirstLoopCount() const {
    if (!m_earlyLoopMouseLockCheck || !m_earlyLoopMouseLockCheck->isChecked()) {
        return 0;
    }
    return m_earlyLoopMouseLockLoopSpin ? m_earlyLoopMouseLockLoopSpin->value() : 0;
}

int FeatureEditDialog::unlockMouseOnBlockFailureCount() const {
    if (!m_earlyLoopMouseLockCheck || !m_earlyLoopMouseLockCheck->isChecked()) {
        return 1;
    }
    return m_earlyLoopMouseUnlockFailureSpin ? m_earlyLoopMouseUnlockFailureSpin->value() : 1;
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
    return triggerCooldownMsFromSeconds(m_triggerCooldownSpin->value());
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

void FeatureEditDialog::setTriggerListAnimations(const TriggerModeListAnimationSettings& settings) {
    m_triggerListAnimations = clampTriggerModeListAnimations(settings);
    updateTriggerAnimationSummary();
}

TriggerModeListAnimationSettings FeatureEditDialog::triggerListAnimations() const {
    return m_triggerListAnimations;
}

void FeatureEditDialog::updateTriggerAnimationSummary() {
    if (!m_triggerAnimationSummary) {
        return;
    }
    m_triggerAnimationSummary->setText(triggerModeListAnimationSummary(m_triggerListAnimations));
}

void FeatureEditDialog::openTriggerAnimationSettings() {
    TriggerModeAnimationSettingsDialog dialog(m_triggerListAnimations, this);
    if (dialog.exec() != QDialog::Accepted) {
        return;
    }
    m_triggerListAnimations = dialog.settings();
    updateTriggerAnimationSummary();
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
