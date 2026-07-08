#pragma once

#include "core/workflow/blocks/KeyPressBlock.h"

#include <QDialog>
#include <QEvent>

class QCheckBox;
class QComboBox;
class QLineEdit;
class QPushButton;
class QWidget;

class KeyPressEditor : public QDialog {
    Q_OBJECT
public:
    explicit KeyPressEditor(KeyPressBlock* block, QWidget* parent = nullptr, bool embedded = false);

    void setBlock(KeyPressBlock* block);
    void reload();
    bool apply();

protected:
    bool eventFilter(QObject* watched, QEvent* event) override;

private:
    void setupUi();
    void populateModifierActionCombo(QComboBox* combo, ModifierKeyAction current);
    ModifierKeyAction modifierActionFromCombo(const QComboBox* combo) const;
    void updateMainKeyRowEnabled();
    void updateKeyDisplay();

    KeyPressBlock* m_block = nullptr;
    bool m_embedded = false;
    bool m_listeningForKey = false;
    int m_editedVirtualKey = 0;
    QCheckBox* m_modifiersOnlyCheck = nullptr;
    QLineEdit* m_keyDisplay = nullptr;
    QPushButton* m_clearKeyButton = nullptr;
    QComboBox* m_actionCombo = nullptr;
    QWidget* m_mainKeyRow = nullptr;
    QComboBox* m_ctrlActionCombo = nullptr;
    QComboBox* m_altActionCombo = nullptr;
    QComboBox* m_shiftActionCombo = nullptr;
};
