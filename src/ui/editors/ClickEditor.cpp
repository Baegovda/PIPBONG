#include "ui/editors/ClickEditor.h"

#include "app/ClickCursorHotkeySettings.h"
#include "core/capture/CursorPositionPicker.h"
#include "core/capture/ScreenCapture.h"
#include "core/input/HotkeyBinding.h"
#include "core/input/InputSimulator.h"
#include "ui/UiStrings.h"
#include "ui/UiThemeColors.h"
#include "ui/widgets/HintLabel.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QEvent>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QHideEvent>
#include <QLabel>
#include <QMessageBox>
#include <QPalette>
#include <QPointer>
#include <QPushButton>
#include <QStackedWidget>
#include <QSizePolicy>
#include "ui/widgets/DragAdjustSpinBox.h"
#include <QTimer>
#include <QVBoxLayout>

#ifdef _WIN32
#include <windows.h>
#endif

namespace {

#ifdef _WIN32
ClickEditor* g_clickCursorHotkeyEditor = nullptr;
HHOOK g_clickCursorHotkeyHook = nullptr;

void uninstallClickCursorHotkeyHook() {
    if (g_clickCursorHotkeyHook) {
        UnhookWindowsHookEx(g_clickCursorHotkeyHook);
        g_clickCursorHotkeyHook = nullptr;
    }
    g_clickCursorHotkeyEditor = nullptr;
}

LRESULT CALLBACK clickCursorHotkeyHookProc(int code, WPARAM wParam, LPARAM lParam) {
    if (code != HC_ACTION || !g_clickCursorHotkeyEditor
        || !g_clickCursorHotkeyEditor->isClickCursorHotkeyHookActive()) {
        return CallNextHookEx(g_clickCursorHotkeyHook, code, wParam, lParam);
    }

    const bool keyDown = wParam == WM_KEYDOWN || wParam == WM_SYSKEYDOWN;
    if (!keyDown) {
        return CallNextHookEx(g_clickCursorHotkeyHook, code, wParam, lParam);
    }

    if (g_clickCursorHotkeyEditor->isCapturingCursorHotkey()
        || CursorPositionPicker::isActive()) {
        return CallNextHookEx(g_clickCursorHotkeyHook, code, wParam, lParam);
    }

    const HotkeyBinding binding = ClickCursorHotkeySettings::binding();
    const auto* info = reinterpret_cast<KBDLLHOOKSTRUCT*>(lParam);
    if (info->flags & LLKHF_INJECTED) {
        return CallNextHookEx(g_clickCursorHotkeyHook, code, wParam, lParam);
    }
    if (!binding.matchesVirtualKey(static_cast<int>(info->vkCode)) || !binding.modifiersMatch()) {
        return CallNextHookEx(g_clickCursorHotkeyHook, code, wParam, lParam);
    }

    QMetaObject::invokeMethod(
        g_clickCursorHotkeyEditor, "triggerCursorPositionFromHotkey", Qt::QueuedConnection);

    return CallNextHookEx(g_clickCursorHotkeyHook, code, wParam, lParam);
}

void installClickCursorHotkeyHookIfNeeded(ClickEditor* editor) {
    uninstallClickCursorHotkeyHook();
    if (!editor || !editor->isClickCursorHotkeyHookActive()) {
        return;
    }
    g_clickCursorHotkeyEditor = editor;
    g_clickCursorHotkeyHook = SetWindowsHookExW(
        WH_KEYBOARD_LL, clickCursorHotkeyHookProc, GetModuleHandleW(nullptr), 0);
}
#endif

constexpr int kCoordinatePickDelaySeconds = 3;
constexpr int kWheelButtonCategory = 100;
constexpr int kBackButtonCategory = 101;
constexpr int kForwardButtonCategory = 102;

bool isWheelCategoryButton(MouseButton button) {
    return button == MouseButton::WheelUp || button == MouseButton::WheelDown
           || button == MouseButton::Middle;
}

bool usesStandardClickActions(int buttonComboData) {
    return buttonComboData != kWheelButtonCategory;
}

MouseButton mouseButtonFromComboData(int buttonComboData) {
    switch (buttonComboData) {
    case kBackButtonCategory:
        return MouseButton::Back;
    case kForwardButtonCategory:
        return MouseButton::Forward;
    default:
        return static_cast<MouseButton>(buttonComboData);
    }
}

int buttonComboDataForMouseButton(MouseButton button) {
    switch (button) {
    case MouseButton::Back:
        return kBackButtonCategory;
    case MouseButton::Forward:
        return kForwardButtonCategory;
    case MouseButton::WheelUp:
    case MouseButton::WheelDown:
    case MouseButton::Middle:
        return kWheelButtonCategory;
    default:
        return static_cast<int>(button);
    }
}

void populateStandardClickActions(QComboBox* combo) {
    combo->addItem(QObject::tr("클릭"), static_cast<int>(ClickAction::Tap));
    combo->addItem(QObject::tr("누름"), static_cast<int>(ClickAction::Down));
    combo->addItem(QObject::tr("뗌"), static_cast<int>(ClickAction::Up));
}

int wheelActionComboIndex(MouseButton button, ClickAction action) {
    if (button == MouseButton::WheelUp) {
        return 0;
    }
    if (button == MouseButton::WheelDown) {
        return 1;
    }
    if (button == MouseButton::Middle) {
        switch (action) {
        case ClickAction::Down:
            return 3;
        case ClickAction::Up:
            return 4;
        case ClickAction::Tap:
        default:
            return 2;
        }
    }
    return 2;
}

void wheelButtonActionFromComboIndex(int index, MouseButton& button, ClickAction& action) {
    switch (index) {
    case 0:
        button = MouseButton::WheelUp;
        action = ClickAction::Tap;
        break;
    case 1:
        button = MouseButton::WheelDown;
        action = ClickAction::Tap;
        break;
    case 3:
        button = MouseButton::Middle;
        action = ClickAction::Down;
        break;
    case 4:
        button = MouseButton::Middle;
        action = ClickAction::Up;
        break;
    case 2:
    default:
        button = MouseButton::Middle;
        action = ClickAction::Tap;
        break;
    }
}

} // namespace

ClickEditor::ClickEditor(ClickBlock* block, QWidget* parent, bool embedded)
    : QDialog(parent)
    , m_block(block)
    , m_embedded(embedded) {
    if (!m_embedded) {
        setWindowTitle(tr("마우스 블록 편집"));
    }
    setupUi();
    reload();
}

ClickEditor::~ClickEditor() {
    CursorPositionPicker::cancelPick();
    syncClickCursorHotkeyHook();
    if (m_cursorPollTimer) {
        m_cursorPollTimer->stop();
    }
}

void ClickEditor::setBlock(ClickBlock* block) {
    CursorPositionPicker::cancelPick();
    m_block = block;
    reload();
    syncClickCursorHotkeyHook();
}

void ClickEditor::reload() {
    if (!m_block) {
        return;
    }
    m_targetCombo->setCurrentIndex(
        m_targetCombo->findData(static_cast<int>(m_block->target)));
    if (m_moveOnlyCheck) {
        m_moveOnlyCheck->setChecked(m_block->action == ClickAction::MoveOnly);
    }
    m_xSpin->setValue(m_block->x);
    m_ySpin->setValue(m_block->y);
    m_buttonCombo->setCurrentIndex(buttonComboIndexForBlock());
    updateButtonUi();
    m_actionCombo->setCurrentIndex(actionComboIndexForBlock());
    m_clientCoordsCheck->setChecked(m_block->useClientCoordinates);
    if (m_lastMatchRelativeCheck) {
        m_lastMatchRelativeCheck->setChecked(m_block->lastMatchRelativeOffset);
    }
    if (m_ctrlCheck) {
        m_ctrlCheck->setChecked(m_block->modifiers.ctrl);
        m_altCheck->setChecked(m_block->modifiers.alt);
        m_shiftCheck->setChecked(m_block->modifiers.shift);
    }
    updateTargetUi();
    updateActionDependentUi();
}

int ClickEditor::buttonComboIndexForBlock() const {
    if (!m_block) {
        return 0;
    }
    return m_buttonCombo->findData(buttonComboDataForMouseButton(m_block->button));
}

int ClickEditor::actionComboIndexForBlock() const {
    if (!m_block || !m_actionCombo) {
        return 0;
    }
    const int buttonData = m_buttonCombo->currentData().toInt();
    if (buttonData == kWheelButtonCategory) {
        return wheelActionComboIndex(m_block->button, m_block->action);
    }
    if (m_block->action == ClickAction::MoveOnly) {
        return 0;
    }
    if (usesStandardClickActions(buttonData)) {
        const int idx = m_actionCombo->findData(static_cast<int>(m_block->action));
        return idx >= 0 ? idx : 0;
    }
    return 0;
}

void ClickEditor::apply() {
    if (!m_block) {
        return;
    }
    m_block->target = static_cast<ClickBlock::ClickTarget>(m_targetCombo->currentData().toInt());
    if (m_block->target == ClickBlock::ClickTarget::Fixed) {
        m_block->x = m_xSpin->value();
        m_block->y = m_ySpin->value();
        m_block->lastMatchRelativeOffset = false;
    } else if (m_block->target == ClickBlock::ClickTarget::LastMatch) {
        m_block->lastMatchRelativeOffset =
            m_lastMatchRelativeCheck && m_lastMatchRelativeCheck->isChecked();
        if (m_block->lastMatchRelativeOffset) {
            m_block->x = m_xSpin->value();
            m_block->y = m_ySpin->value();
        } else {
            m_block->x = 0;
            m_block->y = 0;
        }
    } else {
        m_block->lastMatchRelativeOffset = false;
    }
    m_block->useClientCoordinates = m_clientCoordsCheck->isChecked();
    if (m_moveOnlyCheck && m_moveOnlyCheck->isChecked()) {
        m_block->action = ClickAction::MoveOnly;
        m_block->modifiers = {};
        return;
    }

    const int buttonData = m_buttonCombo->currentData().toInt();
    if (buttonData == kWheelButtonCategory) {
        MouseButton button = MouseButton::Middle;
        ClickAction action = ClickAction::Tap;
        wheelButtonActionFromComboIndex(m_actionCombo->currentIndex(), button, action);
        m_block->button = button;
        m_block->action = action;
    } else if (usesStandardClickActions(buttonData)) {
        m_block->button = mouseButtonFromComboData(buttonData);
        m_block->action = static_cast<ClickAction>(m_actionCombo->currentData().toInt());
    }
    if (m_ctrlCheck) {
        m_block->modifiers.ctrl = m_ctrlCheck->isChecked();
        m_block->modifiers.alt = m_altCheck->isChecked();
        m_block->modifiers.shift = m_shiftCheck->isChecked();
    }
}

void ClickEditor::setFeatureRunOptions(bool lockMouseToScreenCenterDuringRun,
                                       bool lockMouseToCurrentPositionDuringRun) {
    if (m_lockMouseToScreenCenterCheck) {
        m_lockMouseToScreenCenterCheck->setChecked(lockMouseToScreenCenterDuringRun);
    }
    if (m_lockMouseToCurrentPositionCheck) {
        m_lockMouseToCurrentPositionCheck->setChecked(lockMouseToCurrentPositionDuringRun);
    }
}

bool ClickEditor::lockMouseToScreenCenterDuringRun() const {
    return m_lockMouseToScreenCenterCheck && m_lockMouseToScreenCenterCheck->isChecked();
}

bool ClickEditor::lockMouseToCurrentPositionDuringRun() const {
    return m_lockMouseToCurrentPositionCheck && m_lockMouseToCurrentPositionCheck->isChecked();
}

bool ClickEditor::isMoveOnlySelected() const {
    return m_moveOnlyCheck && m_moveOnlyCheck->isChecked();
}

void ClickEditor::updateActionDependentUi() {
    if (m_actionGroup) {
        m_actionGroup->setVisible(!isMoveOnlySelected());
    }
    emit layoutChanged();
}

void ClickEditor::updateTargetUi() {
    const auto target =
        static_cast<ClickBlock::ClickTarget>(m_targetCombo->currentData().toInt());
    const bool fixedTarget = target == ClickBlock::ClickTarget::Fixed;
    const bool lastMatchTarget = target == ClickBlock::ClickTarget::LastMatch;
    const bool currentPositionTarget = target == ClickBlock::ClickTarget::CurrentPosition;
    const bool lastMatchOffsetEnabled =
        lastMatchTarget && m_lastMatchRelativeCheck && m_lastMatchRelativeCheck->isChecked();

    m_fixedCoordGroup->setVisible(fixedTarget || lastMatchOffsetEnabled);
    if (m_lastMatchRelativeCheck) {
        m_lastMatchRelativeCheck->setVisible(lastMatchTarget);
    }
    m_lastMatchHint->setVisible(lastMatchTarget && !lastMatchOffsetEnabled);
    m_currentPositionHint->setVisible(currentPositionTarget);
    m_liveCursorLabel->setVisible(fixedTarget || lastMatchOffsetEnabled);
    m_clientCoordsRow->setVisible(fixedTarget || currentPositionTarget || lastMatchOffsetEnabled);
    emit layoutChanged();
}

void ClickEditor::updateButtonUi() {
    const int buttonData = m_buttonCombo->currentData().toInt();
    const bool wheelCategory = buttonData == kWheelButtonCategory;

    m_actionCombo->blockSignals(true);
    m_actionCombo->clear();
    if (wheelCategory) {
        m_actionCombo->addItem(tr("올림"));
        m_actionCombo->addItem(tr("내림"));
        m_actionCombo->addItem(tr("클릭"));
        m_actionCombo->addItem(tr("누름"));
        m_actionCombo->addItem(tr("뗌"));
        m_actionCombo->setCurrentIndex(2);
    } else {
        populateStandardClickActions(m_actionCombo);
        m_actionCombo->setCurrentIndex(0);
    }
    m_actionCombo->blockSignals(false);

    if (m_actionLabel) {
        m_actionLabel->setVisible(true);
    }
    m_actionCombo->setVisible(true);
}

void ClickEditor::updateLiveCursorLabelStyle() {
    if (!m_liveCursorLabel) {
        return;
    }

    QColor accent = palette().color(QPalette::Highlight);
    const bool darkTheme = palette().color(QPalette::Window).lightness() < 128;
    if (darkTheme && accent.lightness() < 125) {
        accent = QColor(0x8e, 0xc5, 0xff);
    } else if (!darkTheme && (accent.lightness() > 165 || accent.lightness() < 50)) {
        accent = QColor(0x4a, 0x90, 0xd9);
    }

    QPalette labelPalette = m_liveCursorLabel->palette();
    labelPalette.setColor(QPalette::WindowText, accent);
    m_liveCursorLabel->setPalette(labelPalette);
}

void ClickEditor::updateLiveCursorPosition() {
    if (!m_liveCursorLabel) {
        return;
    }

#ifdef _WIN32
    int screenX = 0;
    int screenY = 0;
    if (!InputSimulator::getCursorScreenPosition(screenX, screenY)) {
        m_liveCursorLabel->setText(tr("현재 커서: —"));
        return;
    }

    const bool preferClient = m_clientCoordsCheck && m_clientCoordsCheck->isChecked();
    if (preferClient) {
        HWND hwnd = ScreenCapture::findTargetWindow();
        if (hwnd) {
            int clientX = 0;
            int clientY = 0;
            if (InputSimulator::screenToClient(hwnd, screenX, screenY, clientX, clientY)) {
                m_liveCursorLabel->setText(
                    tr("현재 커서: X %1, Y %2 (클라이언트)").arg(clientX).arg(clientY));
                return;
            }
        }
        m_liveCursorLabel->setText(tr("현재 커서: X %1, Y %2 (화면 · 대상 창 없음)")
                                       .arg(screenX)
                                       .arg(screenY));
        return;
    }

    m_liveCursorLabel->setText(tr("현재 커서: X %1, Y %2 (화면)").arg(screenX).arg(screenY));
#else
    m_liveCursorLabel->setText(tr("현재 커서: —"));
#endif
}

void ClickEditor::showEvent(QShowEvent* event) {
    QDialog::showEvent(event);
    updateLiveCursorLabelStyle();
    updateLiveCursorPosition();
    updateCursorHotkeyDisplay();
    syncClickCursorHotkeyHook();
    if (m_cursorPollTimer) {
        m_cursorPollTimer->start();
    }
}

void ClickEditor::hideEvent(QHideEvent* event) {
    if (m_cursorPollTimer) {
        m_cursorPollTimer->stop();
    }
    stopCursorHotkeyCapture();
    syncClickCursorHotkeyHook();
    QDialog::hideEvent(event);
}

void ClickEditor::changeEvent(QEvent* event) {
    QDialog::changeEvent(event);
    switch (event->type()) {
    case QEvent::PaletteChange:
    case QEvent::ApplicationPaletteChange:
        updateLiveCursorLabelStyle();
        break;
    default:
        break;
    }
}

void ClickEditor::updatePickCoordButton(bool pickActive) {
    if (!m_pickCoordButton) {
        return;
    }
    m_pickCoordButton->setText(pickActive ? tr("좌표 기록 취소")
                                          : tr("좌표 기록 (%1초)").arg(kCoordinatePickDelaySeconds));
}

void ClickEditor::onPickCoordinates() {
#ifdef _WIN32
    if (CursorPositionPicker::isActive()) {
        CursorPositionPicker::cancelPick();
        return;
    }

    const bool useClient = m_clientCoordsCheck->isChecked();
    if (useClient && !ScreenCapture::findTargetWindow()) {
        const int answer = QMessageBox::warning(
            this,
            tr("좌표 지정"),
            tr("대상 창을 찾을 수 없습니다. 화면 좌표로 기록하시겠습니까?"),
            QMessageBox::Yes | QMessageBox::No,
            QMessageBox::No);
        if (answer != QMessageBox::Yes) {
            return;
        }
    }

    updatePickCoordButton(true);
    QPointer<ClickEditor> self = this;
    CursorPositionPicker::startPick(
        window(),
        useClient,
        kCoordinatePickDelaySeconds,
        [self](const CursorPositionPicker::Result& result) {
            if (!self) {
                return;
            }
            self->updatePickCoordButton(false);
            if (!result.accepted) {
                return;
            }
            self->m_xSpin->setValue(result.x);
            self->m_ySpin->setValue(result.y);
        });
#else
    QMessageBox::information(this, tr("좌표 지정"), tr("좌표 지정은 Windows에서만 지원됩니다."));
#endif
}

void ClickEditor::setupUi() {
    setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred);

    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(10);

    auto* targetForm = new QFormLayout();
    targetForm->setFieldGrowthPolicy(QFormLayout::FieldsStayAtSizeHint);
    targetForm->setFormAlignment(Qt::AlignLeft | Qt::AlignTop);

    m_targetCombo = new QComboBox(this);
    m_targetCombo->setSizeAdjustPolicy(QComboBox::AdjustToContents);
    m_targetCombo->addItem(tr("직전 매칭"), static_cast<int>(ClickBlock::ClickTarget::LastMatch));
    m_targetCombo->addItem(tr("현재 위치"), static_cast<int>(ClickBlock::ClickTarget::CurrentPosition));
    m_targetCombo->addItem(tr("고정 좌표"), static_cast<int>(ClickBlock::ClickTarget::Fixed));
    connect(m_targetCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
            [this](int) { updateTargetUi(); });

    auto* targetRow = new QWidget(this);
    auto* targetRowLayout = new QHBoxLayout(targetRow);
    targetRowLayout->setContentsMargins(0, 0, 0, 0);
    targetRowLayout->setSpacing(8);
    targetRowLayout->addWidget(m_targetCombo, 1);
    m_moveOnlyCheck = new QCheckBox(tr("이동만"), targetRow);
    m_moveOnlyCheck->setToolTip(tr("체크 시 커서만 이동하고 버튼 입력은 하지 않습니다."));
    targetRowLayout->addWidget(m_moveOnlyCheck);
    connect(m_moveOnlyCheck, &QCheckBox::toggled, this, &ClickEditor::updateActionDependentUi);
    targetForm->addRow(tr("대상"), targetRow);

    m_lastMatchHint =
        new HintLabel(tr("직전 템플릿 매칭 블록의 매칭 중심 위치에 클릭합니다."), this);
    m_lastMatchHint->setWordWrap(false);
    m_lastMatchHint->setContentsMargins(0, 2, 0, 2);

    m_lastMatchRelativeCheck = new QCheckBox(tr("매칭 중심 기준 상대 좌표"), this);
    m_lastMatchRelativeCheck->setToolTip(
        tr("체크 시 매칭 영역 중심에서 X/Y만큼 이동한 위치에 클릭합니다."));
    connect(m_lastMatchRelativeCheck, &QCheckBox::toggled, this, &ClickEditor::updateTargetUi);

    m_currentPositionHint =
        new HintLabel(tr("블록 실행 시점의 마우스 커서 위치에 클릭합니다."), this);
    m_currentPositionHint->setContentsMargins(0, 2, 0, 2);

    m_liveCursorLabel = new QLabel(this);
    updateLiveCursorLabelStyle();
    m_cursorPollTimer = new QTimer(this);
    m_cursorPollTimer->setInterval(50);
    connect(m_cursorPollTimer, &QTimer::timeout, this, &ClickEditor::updateLiveCursorPosition);

    m_clientCoordsCheck = new QCheckBox(tr("클라이언트 좌표"), this);
    m_clientCoordsCheck->setToolTip(tr("체크 시 대상 창 기준 좌표입니다. 해제 시 화면 좌표입니다."));
    connect(m_clientCoordsCheck, &QCheckBox::toggled, this, &ClickEditor::updateLiveCursorPosition);
    m_clientCoordsRow = new QWidget(this);
    auto* clientCoordsLayout = new QHBoxLayout(m_clientCoordsRow);
    clientCoordsLayout->setContentsMargins(0, 0, 0, 0);
    clientCoordsLayout->addWidget(m_clientCoordsCheck);
    clientCoordsLayout->addStretch();

    m_fixedCoordGroup = new QGroupBox(tr("좌표"), this);
    m_fixedCoordGroup->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred);
    auto* coordForm = new QFormLayout(m_fixedCoordGroup);
    coordForm->setFieldGrowthPolicy(QFormLayout::FieldsStayAtSizeHint);
    coordForm->setFormAlignment(Qt::AlignLeft | Qt::AlignTop);

    m_xSpin = new DragAdjustSpinBox(m_fixedCoordGroup);
    m_ySpin = new DragAdjustSpinBox(m_fixedCoordGroup);
    m_xSpin->setRange(-100000, 100000);
    m_ySpin->setRange(-100000, 100000);
    m_xSpin->setMaximumWidth(96);
    m_ySpin->setMaximumWidth(96);

    auto* xyWidget = new QWidget(m_fixedCoordGroup);
    auto* xyLayout = new QHBoxLayout(xyWidget);
    xyLayout->setContentsMargins(0, 0, 0, 0);
    xyLayout->setSpacing(8);
    xyLayout->addWidget(new QLabel(tr("X"), xyWidget));
    xyLayout->addWidget(m_xSpin);
    xyLayout->addSpacing(4);
    xyLayout->addWidget(new QLabel(tr("Y"), xyWidget));
    xyLayout->addWidget(m_ySpin);
    coordForm->addRow(tr("위치"), xyWidget);

    m_pickCoordButton = new QPushButton(tr("좌표 기록 (%1초)").arg(kCoordinatePickDelaySeconds), m_fixedCoordGroup);
    m_pickCoordButton->setToolTip(
        tr("%1초 후 현재 마우스 커서 위치를 좌표로 기록합니다. 다시 누르거나 Esc로 취소할 수 있습니다.")
            .arg(kCoordinatePickDelaySeconds));
    connect(m_pickCoordButton, &QPushButton::clicked, this, &ClickEditor::onPickCoordinates);

    auto* pickRow = new QHBoxLayout();
    pickRow->setContentsMargins(0, 0, 0, 0);
    pickRow->addWidget(m_pickCoordButton);
    coordForm->addRow(QString(), pickRow);

    m_cursorHotkeyLabel = new QLabel(m_fixedCoordGroup);
    m_cursorHotkeyLabel->setAlignment(Qt::AlignCenter);
    m_cursorHotkeyLabel->setCursor(Qt::PointingHandCursor);
    m_cursorHotkeyLabel->installEventFilter(this);
    m_cursorHotkeyLabel->setToolTip(
        tr("마우스 블록 편집 중에만 동작합니다. 단축키를 누르면 현재 커서 좌표를 입력합니다. 클릭하여 변경."));
    auto* clearCursorHotkeyButton = new QPushButton(tr("단축키 지우기"), m_fixedCoordGroup);
    clearCursorHotkeyButton->setToolTip(tr("기본값 F1로 되돌립니다."));
    auto* cursorHotkeyRow = new QHBoxLayout();
    cursorHotkeyRow->setSpacing(8);
    cursorHotkeyRow->addWidget(m_cursorHotkeyLabel, 3);
    cursorHotkeyRow->addWidget(clearCursorHotkeyButton, 1);
    coordForm->addRow(tr("좌표 입력 단축키"), cursorHotkeyRow);
    connect(clearCursorHotkeyButton, &QPushButton::clicked, this, [this]() {
        stopCursorHotkeyCapture();
        ClickCursorHotkeySettings::setBinding({});
        updateCursorHotkeyDisplay();
        syncClickCursorHotkeyHook();
    });
    updateCursorHotkeyDisplay();

    m_actionGroup = new QGroupBox(tr("동작 설정"), this);
    m_actionGroup->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred);
    auto* clickVBox = new QVBoxLayout(m_actionGroup);
    clickVBox->setContentsMargins(9, 9, 9, 9);
    clickVBox->setSpacing(8);

    auto* clickLayout = new QHBoxLayout();
    clickLayout->setContentsMargins(0, 0, 0, 0);
    clickLayout->setSpacing(8);

    m_buttonCombo = new QComboBox(m_actionGroup);
    m_buttonCombo->setSizeAdjustPolicy(QComboBox::AdjustToContents);
    m_buttonCombo->addItem(tr("왼쪽"), static_cast<int>(MouseButton::Left));
    m_buttonCombo->addItem(tr("오른쪽"), static_cast<int>(MouseButton::Right));
    m_buttonCombo->addItem(tr("휠"), kWheelButtonCategory);
    m_buttonCombo->addItem(tr("뒤로 가기"), kBackButtonCategory);
    m_buttonCombo->addItem(tr("앞으로 가기"), kForwardButtonCategory);
    connect(m_buttonCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
            [this](int) { updateButtonUi(); });

    m_actionCombo = new QComboBox(m_actionGroup);
    m_actionCombo->setSizeAdjustPolicy(QComboBox::AdjustToContents);

    m_actionLabel = new QLabel(tr("동작"), m_actionGroup);

    m_buttonLabel = new QLabel(tr("버튼"), m_actionGroup);
    clickLayout->addWidget(m_buttonLabel);
    clickLayout->addWidget(m_buttonCombo);
    clickLayout->addSpacing(8);
    clickLayout->addWidget(m_actionLabel);
    clickLayout->addWidget(m_actionCombo);

    m_ctrlCheck = new QCheckBox(tr("Ctrl"), m_actionGroup);
    m_altCheck = new QCheckBox(tr("Alt"), m_actionGroup);
    m_shiftCheck = new QCheckBox(tr("Shift"), m_actionGroup);

    m_modifierRow = new QWidget(m_actionGroup);
    auto* modifierRow = new QHBoxLayout(m_modifierRow);
    modifierRow->setContentsMargins(0, 0, 0, 0);
    modifierRow->setSpacing(8);
    modifierRow->addWidget(new QLabel(tr("조합키"), m_modifierRow));
    modifierRow->addWidget(m_ctrlCheck);
    modifierRow->addWidget(m_altCheck);
    modifierRow->addWidget(m_shiftCheck);
    modifierRow->addStretch();

    clickVBox->addLayout(clickLayout);
    clickVBox->addWidget(m_modifierRow);

    layout->addLayout(targetForm);
    layout->addWidget(m_lastMatchHint);
    layout->addWidget(m_lastMatchRelativeCheck);
    layout->addWidget(m_currentPositionHint);
    layout->addWidget(m_liveCursorLabel);
    layout->addWidget(m_clientCoordsRow);
    layout->addWidget(m_fixedCoordGroup);
    layout->addWidget(m_actionGroup);

    m_featureRunGroup = new QGroupBox(tr("기능 실행"), this);
    m_featureRunGroup->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred);
    auto* featureRunLayout = new QVBoxLayout(m_featureRunGroup);
    featureRunLayout->setContentsMargins(9, 9, 9, 9);
    featureRunLayout->setSpacing(6);

    m_lockMouseToScreenCenterCheck =
        new QCheckBox(tr("기능이 켜져 있는 동안 마우스를 대상 창 중앙에 고정"), m_featureRunGroup);
    m_lockMouseToScreenCenterCheck->setToolTip(
        tr("이 기능이 실행되는 동안 실제 마우스 커서를 대상 창 중앙에 고정합니다. "
           "대상 창을 옮기면 고정 위치도 함께 따라갑니다. "
           "워크플로가 보내는 클릭 이동은 그대로 동작합니다."));
    featureRunLayout->addWidget(m_lockMouseToScreenCenterCheck);

    m_lockMouseToCurrentPositionCheck =
        new QCheckBox(tr("기능이 켜져 있는 동안 마우스 위치 잠금"), m_featureRunGroup);
    m_lockMouseToCurrentPositionCheck->setToolTip(
        tr("기능 시작 시점의 실제 마우스 위치에 커서를 고정합니다. "
           "대상 창이 지정되어 있으면 창을 옮겨도 같은 상대 위치를 유지합니다. "
           "워크플로가 보내는 클릭 이동은 그대로 동작하지만, 사용자가 마우스를 움직일 수 없게 합니다."));
    featureRunLayout->addWidget(m_lockMouseToCurrentPositionCheck);
    connect(m_lockMouseToCurrentPositionCheck, &QCheckBox::toggled, this, [this](bool checked) {
        if (checked && m_lockMouseToScreenCenterCheck) {
            m_lockMouseToScreenCenterCheck->setChecked(false);
        }
        emit layoutChanged();
    });
    connect(m_lockMouseToScreenCenterCheck, &QCheckBox::toggled, this, [this](bool checked) {
        if (checked && m_lockMouseToCurrentPositionCheck) {
            m_lockMouseToCurrentPositionCheck->setChecked(false);
        }
        emit layoutChanged();
    });

    layout->addWidget(m_featureRunGroup);

    if (!m_embedded) {
        auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
        localizeDialogButtons(buttons);
        layout->addWidget(buttons);
        connect(buttons, &QDialogButtonBox::accepted, this, [this]() {
            apply();
            accept();
        });
        connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
        adjustSize();
    }
}

bool ClickEditor::isClickCursorHotkeyHookActive() const {
    if (!isVisible()) {
        return false;
    }

    QWidget* hostWindow = window();
    if (!hostWindow || !hostWindow->isVisible()) {
        return false;
    }

    if (!m_embedded) {
        return true;
    }

    if (const auto* stack = qobject_cast<const QStackedWidget*>(parent())) {
        return stack->currentWidget() == this;
    }

    return true;
}

void ClickEditor::refreshClickCursorHotkeyHook() {
    syncClickCursorHotkeyHook();
}

void ClickEditor::syncClickCursorHotkeyHook() {
#ifdef _WIN32
    installClickCursorHotkeyHookIfNeeded(this);
#else
    (void)this;
#endif
}

void ClickEditor::triggerCursorPositionFromHotkey() {
    applyCurrentCursorToCoordinateFields();
}

bool ClickEditor::queryCurrentCursorCoordinates(int& x, int& y) const {
#ifdef _WIN32
    int screenX = 0;
    int screenY = 0;
    if (!InputSimulator::getCursorScreenPosition(screenX, screenY)) {
        return false;
    }

    const bool preferClient = m_clientCoordsCheck && m_clientCoordsCheck->isChecked();
    if (preferClient) {
        HWND hwnd = ScreenCapture::findTargetWindow();
        if (hwnd && InputSimulator::screenToClient(hwnd, screenX, screenY, x, y)) {
            return true;
        }
    }

    x = screenX;
    y = screenY;
    return true;
#else
    (void)x;
    (void)y;
    return false;
#endif
}

void ClickEditor::applyCurrentCursorToCoordinateFields() {
    int x = 0;
    int y = 0;
    if (!queryCurrentCursorCoordinates(x, y)) {
        return;
    }

    const auto target =
        static_cast<ClickBlock::ClickTarget>(m_targetCombo->currentData().toInt());
    if (target == ClickBlock::ClickTarget::Fixed
        || (target == ClickBlock::ClickTarget::LastMatch && m_lastMatchRelativeCheck
            && m_lastMatchRelativeCheck->isChecked())) {
        m_xSpin->setValue(x);
        m_ySpin->setValue(y);
        return;
    }

    const int fixedIndex =
        m_targetCombo->findData(static_cast<int>(ClickBlock::ClickTarget::Fixed));
    if (fixedIndex >= 0) {
        m_targetCombo->setCurrentIndex(fixedIndex);
        updateTargetUi();
        m_xSpin->setValue(x);
        m_ySpin->setValue(y);
    }
}

void ClickEditor::updateCursorHotkeyDisplay() {
    if (!m_cursorHotkeyLabel) {
        return;
    }
    const HotkeyBinding binding = ClickCursorHotkeySettings::binding();
    m_cursorHotkeyLabel->setText(binding.displayString());
    if (!m_capturingCursorHotkey) {
        m_cursorHotkeyLabel->setStyleSheet(hotkeyBindingLabelIdleStyleSheet(palette()));
    }
}

void ClickEditor::updateCursorHotkeyCaptureUi() {
    if (!m_cursorHotkeyLabel) {
        return;
    }
    if (m_capturingCursorHotkey) {
        m_cursorHotkeyLabel->setText(tr("입력 대기 중..."));
        m_cursorHotkeyLabel->setStyleSheet(hotkeyBindingLabelCaptureStyleSheet(palette()));
    } else {
        updateCursorHotkeyDisplay();
    }
}

void ClickEditor::startCursorHotkeyCapture() {
    m_capturingCursorHotkey = true;
    updateCursorHotkeyCaptureUi();
    setFocus();
}

void ClickEditor::stopCursorHotkeyCapture() {
    m_capturingCursorHotkey = false;
    updateCursorHotkeyCaptureUi();
}

void ClickEditor::applyCursorHotkeyCapture(int virtualKey, Qt::KeyboardModifiers modifiers) {
    if (virtualKey == 0 || HotkeyBinding::isModifierOnlyVirtualKey(virtualKey)
        || HotkeyBinding::isMouseVirtualKey(virtualKey)) {
        return;
    }

    HotkeyBinding binding;
    binding.virtualKey = virtualKey;
    binding.ctrl = modifiers.testFlag(Qt::ControlModifier);
    binding.alt = modifiers.testFlag(Qt::AltModifier);
    binding.shift = modifiers.testFlag(Qt::ShiftModifier);
    ClickCursorHotkeySettings::setBinding(binding);
    stopCursorHotkeyCapture();
    updateCursorHotkeyDisplay();
    syncClickCursorHotkeyHook();
}

bool ClickEditor::eventFilter(QObject* watched, QEvent* event) {
    if (watched == m_cursorHotkeyLabel && event->type() == QEvent::MouseButtonPress) {
        startCursorHotkeyCapture();
        return true;
    }
    return QDialog::eventFilter(watched, event);
}

void ClickEditor::keyPressEvent(QKeyEvent* event) {
    if (m_capturingCursorHotkey) {
        if (event->key() == Qt::Key_Escape) {
            stopCursorHotkeyCapture();
            event->accept();
            return;
        }
#ifdef _WIN32
        const int vk = HotkeyBinding::qtKeyToVirtualKey(event->key());
        if (vk != 0) {
            applyCursorHotkeyCapture(vk, event->modifiers());
            event->accept();
            return;
        }
#endif
    }
    QDialog::keyPressEvent(event);
}
