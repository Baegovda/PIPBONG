#pragma once

#include "core/workflow/blocks/KeyPressBlock.h"

#include <QDialog>

class QCheckBox;
class QComboBox;
class QKeySequenceEdit;
class QPushButton;
class QWidget;

class KeyPressEditor : public QDialog {
    Q_OBJECT
public:
    explicit KeyPressEditor(KeyPressBlock* block, QWidget* parent = nullptr, bool embedded = false);

    void setBlock(KeyPressBlock* block);
    void reload();
    bool apply();

private:
    void setupUi();
    void populateModifierActionCombo(QComboBox* combo, ModifierKeyAction current);
    ModifierKeyAction modifierActionFromCombo(const QComboBox* combo) const;
    void updateMainKeyRowEnabled();

    KeyPressBlock* m_block = nullptr;
    bool m_embedded = false;
    QCheckBox* m_modifiersOnlyCheck = nullptr;
    QKeySequenceEdit* m_keyEdit = nullptr;
    QPushButton* m_clearKeyButton = nullptr;
    QComboBox* m_actionCombo = nullptr;
    QWidget* m_mainKeyRow = nullptr;
    QComboBox* m_ctrlActionCombo = nullptr;
    QComboBox* m_altActionCombo = nullptr;
    QComboBox* m_shiftActionCombo = nullptr;
};
