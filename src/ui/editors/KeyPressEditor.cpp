#include "ui/editors/KeyPressEditor.h"

#include "ui/UiStrings.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QKeySequenceEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QVBoxLayout>
#include <QWidget>

#ifdef _WIN32
#include <windows.h>
#endif

namespace {

#ifdef _WIN32
int qtKeyToVirtualKey(int qtKey) {
    if (qtKey >= Qt::Key_A && qtKey <= Qt::Key_Z) {
        return 'A' + (qtKey - Qt::Key_A);
    }
    if (qtKey >= Qt::Key_0 && qtKey <= Qt::Key_9) {
        return '0' + (qtKey - Qt::Key_0);
    }
    switch (qtKey) {
    case Qt::Key_Space:
        return VK_SPACE;
    case Qt::Key_Return:
    case Qt::Key_Enter:
        return VK_RETURN;
    case Qt::Key_Escape:
        return VK_ESCAPE;
    case Qt::Key_Tab:
        return VK_TAB;
    case Qt::Key_Backspace:
        return VK_BACK;
    case Qt::Key_Delete:
        return VK_DELETE;
    case Qt::Key_Home:
        return VK_HOME;
    case Qt::Key_End:
        return VK_END;
    case Qt::Key_PageUp:
        return VK_PRIOR;
    case Qt::Key_PageDown:
        return VK_NEXT;
    case Qt::Key_Left:
        return VK_LEFT;
    case Qt::Key_Right:
        return VK_RIGHT;
    case Qt::Key_Up:
        return VK_UP;
    case Qt::Key_Down:
        return VK_DOWN;
    case Qt::Key_F1:
        return VK_F1;
    case Qt::Key_F2:
        return VK_F2;
    case Qt::Key_F3:
        return VK_F3;
    case Qt::Key_F4:
        return VK_F4;
    case Qt::Key_F5:
        return VK_F5;
    case Qt::Key_F6:
        return VK_F6;
    case Qt::Key_F7:
        return VK_F7;
    case Qt::Key_F8:
        return VK_F8;
    case Qt::Key_F9:
        return VK_F9;
    case Qt::Key_F10:
        return VK_F10;
    case Qt::Key_F11:
        return VK_F11;
    case Qt::Key_F12:
        return VK_F12;
    default:
        return qtKey & 0xFF;
    }
}

int virtualKeyToQtKey(int vk) {
    if (vk >= 'A' && vk <= 'Z') {
        return Qt::Key_A + (vk - 'A');
    }
    if (vk >= '0' && vk <= '9') {
        return Qt::Key_0 + (vk - '0');
    }
    switch (vk) {
    case VK_SPACE:
        return Qt::Key_Space;
    case VK_RETURN:
        return Qt::Key_Return;
    case VK_ESCAPE:
        return Qt::Key_Escape;
    case VK_TAB:
        return Qt::Key_Tab;
    case VK_BACK:
        return Qt::Key_Backspace;
    case VK_DELETE:
        return Qt::Key_Delete;
    case VK_HOME:
        return Qt::Key_Home;
    case VK_END:
        return Qt::Key_End;
    case VK_PRIOR:
        return Qt::Key_PageUp;
    case VK_NEXT:
        return Qt::Key_PageDown;
    case VK_LEFT:
        return Qt::Key_Left;
    case VK_RIGHT:
        return Qt::Key_Right;
    case VK_UP:
        return Qt::Key_Up;
    case VK_DOWN:
        return Qt::Key_Down;
    case VK_F1:
        return Qt::Key_F1;
    case VK_F2:
        return Qt::Key_F2;
    case VK_F3:
        return Qt::Key_F3;
    case VK_F4:
        return Qt::Key_F4;
    case VK_F5:
        return Qt::Key_F5;
    case VK_F6:
        return Qt::Key_F6;
    case VK_F7:
        return Qt::Key_F7;
    case VK_F8:
        return Qt::Key_F8;
    case VK_F9:
        return Qt::Key_F9;
    case VK_F10:
        return Qt::Key_F10;
    case VK_F11:
        return Qt::Key_F11;
    case VK_F12:
        return Qt::Key_F12;
    default:
        return vk;
    }
}
#endif

} // namespace

KeyPressEditor::KeyPressEditor(KeyPressBlock* block, QWidget* parent, bool embedded)
    : QDialog(parent)
    , m_block(block)
    , m_embedded(embedded) {
    if (!m_embedded) {
        setWindowTitle(tr("키보드 블록 편집"));
    }
    setupUi();
}

void KeyPressEditor::setBlock(KeyPressBlock* block) {
    m_block = block;
    reload();
}

void KeyPressEditor::reload() {
    if (!m_block) {
        return;
    }
    m_modifiersOnlyCheck->setChecked(!m_block->useMainKey);
#ifdef _WIN32
    if (m_block->useMainKey) {
        m_keyEdit->setKeySequence(QKeySequence(static_cast<int>(virtualKeyToQtKey(m_block->virtualKey))));
    } else {
        m_keyEdit->clear();
    }
#endif
    m_actionCombo->setCurrentIndex(m_actionCombo->findData(static_cast<int>(m_block->action)));
    populateModifierActionCombo(m_ctrlActionCombo, m_block->modifierActions.ctrl);
    populateModifierActionCombo(m_altActionCombo, m_block->modifierActions.alt);
    populateModifierActionCombo(m_shiftActionCombo, m_block->modifierActions.shift);
    updateMainKeyRowEnabled();
}

bool KeyPressEditor::apply() {
    if (!m_block) {
        return false;
    }

    m_block->useMainKey = !m_modifiersOnlyCheck->isChecked();
    m_block->modifierActions.ctrl = modifierActionFromCombo(m_ctrlActionCombo);
    m_block->modifierActions.alt = modifierActionFromCombo(m_altActionCombo);
    m_block->modifierActions.shift = modifierActionFromCombo(m_shiftActionCombo);

    if (m_block->useMainKey) {
#ifdef _WIN32
        const QKeySequence sequence = m_keyEdit->keySequence();
        if (sequence.isEmpty()) {
            QMessageBox::warning(this, tr("키보드"), tr("키를 입력하거나 '수정키만'을 선택하세요."));
            return false;
        }
        m_block->virtualKey = qtKeyToVirtualKey(sequence[0].key());
#endif
        m_block->action = static_cast<KeyAction>(m_actionCombo->currentData().toInt());
    } else if (!m_block->modifierActions.hasAny()) {
        QMessageBox::warning(this, tr("키보드"), tr("수정키만 사용할 때는 Ctrl, Alt, Shift 중 하나 이상의 동작을 지정하세요."));
        return false;
    }

    return true;
}

void KeyPressEditor::updateMainKeyRowEnabled() {
    const bool modifiersOnly = m_modifiersOnlyCheck && m_modifiersOnlyCheck->isChecked();
    if (m_mainKeyRow) {
        m_mainKeyRow->setEnabled(!modifiersOnly);
    }
    if (modifiersOnly && m_keyEdit) {
        m_keyEdit->clear();
    }
}

void KeyPressEditor::populateModifierActionCombo(QComboBox* combo, ModifierKeyAction current) {
    if (!combo) {
        return;
    }
    if (combo->count() == 0) {
        combo->addItem(tr("사용 안 함"), static_cast<int>(ModifierKeyAction::None));
        combo->addItem(tr("탭"), static_cast<int>(ModifierKeyAction::Tap));
        combo->addItem(tr("누름"), static_cast<int>(ModifierKeyAction::Down));
        combo->addItem(tr("뗌"), static_cast<int>(ModifierKeyAction::Up));
    }
    const int index = combo->findData(static_cast<int>(current));
    combo->setCurrentIndex(index >= 0 ? index : 0);
}

ModifierKeyAction KeyPressEditor::modifierActionFromCombo(const QComboBox* combo) const {
    if (!combo) {
        return ModifierKeyAction::None;
    }
    return static_cast<ModifierKeyAction>(combo->currentData().toInt());
}

void KeyPressEditor::setupUi() {
    auto* layout = new QVBoxLayout(this);
    auto* form = new QFormLayout();

    m_modifiersOnlyCheck = new QCheckBox(tr("수정키만"), this);
    m_modifiersOnlyCheck->setToolTip(tr("체크하면 Space 등 일반 키 없이 Ctrl/Alt/Shift 동작만 실행합니다."));
    m_modifiersOnlyCheck->setChecked(!m_block->useMainKey);

    m_keyEdit = new QKeySequenceEdit(this);
#ifdef _WIN32
    if (m_block->useMainKey) {
        m_keyEdit->setKeySequence(QKeySequence(static_cast<int>(virtualKeyToQtKey(m_block->virtualKey))));
    }
#endif
    m_clearKeyButton = new QPushButton(tr("지우기"), this);
    m_clearKeyButton->setToolTip(tr("입력된 키를 지웁니다."));

    m_actionCombo = new QComboBox(this);
    m_actionCombo->addItem(tr("탭"), static_cast<int>(KeyAction::Tap));
    m_actionCombo->addItem(tr("누름"), static_cast<int>(KeyAction::Down));
    m_actionCombo->addItem(tr("뗌"), static_cast<int>(KeyAction::Up));
    m_actionCombo->setCurrentIndex(m_actionCombo->findData(static_cast<int>(m_block->action)));

    m_ctrlActionCombo = new QComboBox(this);
    m_altActionCombo = new QComboBox(this);
    m_shiftActionCombo = new QComboBox(this);
    populateModifierActionCombo(m_ctrlActionCombo, m_block->modifierActions.ctrl);
    populateModifierActionCombo(m_altActionCombo, m_block->modifierActions.alt);
    populateModifierActionCombo(m_shiftActionCombo, m_block->modifierActions.shift);

    m_mainKeyRow = new QWidget(this);
    auto* keyActionLayout = new QHBoxLayout(m_mainKeyRow);
    keyActionLayout->setContentsMargins(0, 0, 0, 0);
    keyActionLayout->setSpacing(8);
    keyActionLayout->addWidget(m_keyEdit, 1);
    keyActionLayout->addWidget(m_clearKeyButton);
    keyActionLayout->addWidget(new QLabel(tr("동작"), m_mainKeyRow));
    keyActionLayout->addWidget(m_actionCombo);

    auto* modifierRow = new QWidget(this);
    auto* modifierLayout = new QHBoxLayout(modifierRow);
    modifierLayout->setContentsMargins(0, 0, 0, 0);
    modifierLayout->setSpacing(8);
    modifierLayout->addWidget(new QLabel(tr("Ctrl"), modifierRow));
    modifierLayout->addWidget(m_ctrlActionCombo);
    modifierLayout->addWidget(new QLabel(tr("Alt"), modifierRow));
    modifierLayout->addWidget(m_altActionCombo);
    modifierLayout->addWidget(new QLabel(tr("Shift"), modifierRow));
    modifierLayout->addWidget(m_shiftActionCombo);
    modifierLayout->addStretch();

    form->addRow(QString(), m_modifiersOnlyCheck);
    form->addRow(tr("키보드"), m_mainKeyRow);
    form->addRow(tr("조합키"), modifierRow);

    connect(m_modifiersOnlyCheck, &QCheckBox::toggled, this, [this](bool) { updateMainKeyRowEnabled(); });
    connect(m_clearKeyButton, &QPushButton::clicked, this, [this]() {
        if (m_keyEdit) {
            m_keyEdit->clear();
        }
        if (m_modifiersOnlyCheck) {
            m_modifiersOnlyCheck->setChecked(true);
        }
    });

    updateMainKeyRowEnabled();

    layout->addLayout(form);

    if (!m_embedded) {
        auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
        localizeDialogButtons(buttons);
        layout->addWidget(buttons);
        connect(buttons, &QDialogButtonBox::accepted, this, [this]() {
            if (!apply()) {
                return;
            }
            accept();
        });
        connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    }
}
