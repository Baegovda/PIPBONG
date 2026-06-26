#include "ui/editors/FeatureEditDialog.h"

#include "app/HotkeyManager.h"
#include "model/Project.h"
#include "ui/UiStrings.h"
#include "ui/UiThemeColors.h"

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
#include "ui/widgets/DragAdjustSpinBox.h"
#include <QVBoxLayout>

FeatureEditDialog::FeatureEditDialog(const QString& name,
                                     const HotkeyBinding& hotkey,
                                     FeatureRunMode runMode,
                                     int repeatCount,
                                     int infiniteExitAfterConsecutiveMisses,
                                     UserInputInterruptMode userInputInterruptMode,
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
    m_repeatSpin->setValue(repeatCount);

    const FeatureRunMode displayMode = normalizeRunMode(runMode);
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

    const UserInputInterruptMode displayInterruptMode =
        userInputInterruptMode == UserInputInterruptMode::None ? UserInputInterruptMode::Stop
                                                               : userInputInterruptMode;
    const int interruptIndex =
        m_userInputInterruptCombo->findData(static_cast<int>(displayInterruptMode));
    if (interruptIndex >= 0) {
        m_userInputInterruptCombo->setCurrentIndex(interruptIndex);
    }

    updateModeDependentUi();
}

void FeatureEditDialog::setupUi() {
    auto* layout = new QVBoxLayout(this);

    auto* form = new QFormLayout();
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
    form->addRow(tr("단축키"), hotkeyRow);

    m_modeCombo = new QComboBox(this);
    m_modeCombo->addItem(tr("N회 반복"), static_cast<int>(FeatureRunMode::RepeatCount));
    m_modeCombo->addItem(tr("누를 동안"), static_cast<int>(FeatureRunMode::Hold));
    m_modeCombo->addItem(tr("무한 반복"), static_cast<int>(FeatureRunMode::RepeatInfinite));
    m_modeCombo->setItemData(1,
                             tr("단축키를 누르고 있는 동안 워크플로를 무한 반복합니다. 키를 떼면 중지됩니다."),
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

    m_userInputInterruptCombo = new QComboBox(this);
    m_userInputInterruptCombo->addItem(tr("완전 정지"), static_cast<int>(UserInputInterruptMode::Stop));
    m_userInputInterruptCombo->addItem(tr("일시정지"), static_cast<int>(UserInputInterruptMode::Pause));
    m_userInputInterruptCombo->setToolTip(
        tr("워크플로 실행 중 사용자가 직접 키보드를 누르거나 마우스 버튼을 클릭하면(이동 제외) "
           "일시정지하거나 완전히 정지합니다. 기능 단축키 입력은 제외됩니다."));
    form->addRow(tr("사용자 입력 시"), m_userInputInterruptCombo);

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    localizeDialogButtons(buttons);

    layout->addLayout(form);
    layout->addWidget(buttons);

    connect(clearButton, &QPushButton::clicked, this, [this]() {
        stopHotkeyCapture();
        m_hotkey = HotkeyBinding{};
        updateHotkeyDisplay();
        updateCaptureUi();
    });
    connect(m_modeCombo, qOverload<int>(&QComboBox::currentIndexChanged), this,
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
    m_listeningForHotkey = true;
    updateCaptureUi();
    setFocus();
}

void FeatureEditDialog::stopHotkeyCapture() {
    m_listeningForHotkey = false;
    updateCaptureUi();
}

void FeatureEditDialog::updateModeDependentUi() {
    const auto mode = static_cast<FeatureRunMode>(m_modeCombo->currentData().toInt());
    const bool repeatCountMode = mode == FeatureRunMode::RepeatCount;
    const bool infiniteStyle = mode == FeatureRunMode::RepeatInfinite || mode == FeatureRunMode::Hold;

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
    adjustSize();
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
            || widget == m_userInputInterruptCombo) {
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

HotkeyBinding FeatureEditDialog::hotkey() const {
    return m_hotkey;
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
