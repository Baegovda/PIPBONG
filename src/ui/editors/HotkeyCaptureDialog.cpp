#include "ui/editors/HotkeyCaptureDialog.h"

#include "app/FeatureHotkeyGate.h"
#include "ui/UiStrings.h"
#include "ui/UiThemeColors.h"

#include <QAbstractButton>
#include <QApplication>
#include <QDialogButtonBox>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QLabel>
#include <QMouseEvent>
#include <QPushButton>
#include <QVBoxLayout>

HotkeyCaptureDialog::HotkeyCaptureDialog(const HotkeyBinding& current, QWidget* parent)
    : QDialog(parent)
    , m_binding(current) {
    setWindowTitle(tr("단축키 설정"));
    setModal(true);
    setupUi();
    updateDisplay();
    updateCaptureUi();
}

void HotkeyCaptureDialog::setupUi() {
    auto* layout = new QVBoxLayout(this);

    m_bindingLabel = new QLabel(this);
    m_bindingLabel->setAlignment(Qt::AlignCenter);
    m_bindingLabel->setCursor(Qt::PointingHandCursor);
    m_bindingLabel->installEventFilter(this);

    auto* clearButton = new QPushButton(tr("지우기"), this);
    auto* actionRow = new QHBoxLayout();
    actionRow->addWidget(clearButton);
    actionRow->addStretch();

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    localizeDialogButtons(buttons);

    layout->addWidget(m_bindingLabel);
    layout->addLayout(actionRow);
    layout->addWidget(buttons);

    connect(clearButton, &QPushButton::clicked, this, [this]() {
        stopCapture();
        m_binding = HotkeyBinding{};
        m_cleared = true;
        updateDisplay();
        updateCaptureUi();
    });
    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
}

void HotkeyCaptureDialog::applyBindingLabelIdleStyle() {
    m_bindingLabel->setStyleSheet(hotkeyBindingLabelLargeIdleStyleSheet(palette()));
}

void HotkeyCaptureDialog::updateDisplay() {
    if (m_binding.isEmpty()) {
        m_bindingLabel->setText(tr("(없음 — 클릭하여 설정)"));
    } else {
        m_bindingLabel->setText(m_binding.displayString());
    }
}

void HotkeyCaptureDialog::updateCaptureUi() {
    if (m_listeningForHotkey) {
        m_bindingLabel->setText(tr("입력 대기 중..."));
        m_bindingLabel->setStyleSheet(hotkeyBindingLabelLargeCaptureStyleSheet(palette()));
    } else {
        updateDisplay();
        applyBindingLabelIdleStyle();
    }
}

void HotkeyCaptureDialog::startCapture() {
    m_hotkeyCaptureGate = std::make_unique<FeatureHotkeyGateScope>();
    m_listeningForHotkey = true;
    updateCaptureUi();
    setFocus();
}

void HotkeyCaptureDialog::stopCapture() {
    m_listeningForHotkey = false;
    m_hotkeyCaptureGate.reset();
    updateCaptureUi();
}

void HotkeyCaptureDialog::applyCapturedBinding(int virtualKey,
                                               Qt::KeyboardModifiers modifiers) {
    if (virtualKey == 0 || HotkeyBinding::isModifierOnlyVirtualKey(virtualKey)) {
        return;
    }

    m_binding.virtualKey = virtualKey;
    if (HotkeyBinding::isMouseVirtualKey(virtualKey)) {
        m_binding.ctrl = false;
        m_binding.alt = false;
        m_binding.shift = false;
    } else {
        m_binding.ctrl = modifiers.testFlag(Qt::ControlModifier);
        m_binding.alt = modifiers.testFlag(Qt::AltModifier);
        m_binding.shift = modifiers.testFlag(Qt::ShiftModifier);
    }
    m_cleared = false;
    stopCapture();
}

bool HotkeyCaptureDialog::eventFilter(QObject* watched, QEvent* event) {
    if (watched == m_bindingLabel && event->type() == QEvent::MouseButtonPress && !m_listeningForHotkey) {
        startCapture();
        return true;
    }
    return QDialog::eventFilter(watched, event);
}

bool HotkeyCaptureDialog::isInteractiveWidget(const QWidget* widget) const {
    while (widget && widget != this) {
        if (qobject_cast<const QAbstractButton*>(widget)) {
            return true;
        }
        widget = widget->parentWidget();
    }
    return false;
}

void HotkeyCaptureDialog::reject() {
    stopCapture();
    QDialog::reject();
}

void HotkeyCaptureDialog::keyPressEvent(QKeyEvent* event) {
    if (!m_listeningForHotkey) {
        QDialog::keyPressEvent(event);
        return;
    }

    if (event->key() == Qt::Key_Escape) {
        stopCapture();
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

void HotkeyCaptureDialog::mousePressEvent(QMouseEvent* event) {
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
