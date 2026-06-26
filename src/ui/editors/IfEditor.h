#pragma once

#include "core/workflow/blocks/IfBlock.h"

#include <QDialog>

class QCheckBox;
class QComboBox;
class QPushButton;
class BranchWorkflowEditor;
class DragAdjustSpinBox;
class HintLabel;

class IfEditor : public QDialog {
    Q_OBJECT
public:
    explicit IfEditor(IfBlock* block, const QString& projectDirectory, QWidget* parent = nullptr,
                      bool embedded = false);

    void setBlock(IfBlock* block);
    void setWorkflowEditorContext(int blockCount, int editingBlockIndex);
    void setGotoBlockPickAvailable(bool available);
    void applyPickedGotoBlock(int branch, int blockNumber);
    void reload();
    void apply();

signals:
    void layoutChanged();
    void requestGotoBlockPick(int branch);

private:
    void setupUi();
    void updateGotoSpinRanges();
    IfCondition conditionFromComboIndex(int index) const;
    int comboIndexForCondition(IfCondition condition) const;

    IfBlock* m_block = nullptr;
    QString m_projectDirectory;
    bool m_embedded = false;
    bool m_gotoBlockPickAvailable = false;
    int m_workflowBlockCount = 0;
    int m_editingBlockIndex = -1;
    QComboBox* m_conditionCombo = nullptr;
    QCheckBox* m_negateCheck = nullptr;
    QCheckBox* m_thenGotoCheck = nullptr;
    DragAdjustSpinBox* m_thenGotoSpin = nullptr;
    QPushButton* m_thenGotoPickButton = nullptr;
    QCheckBox* m_elseGotoCheck = nullptr;
    DragAdjustSpinBox* m_elseGotoSpin = nullptr;
    QPushButton* m_elseGotoPickButton = nullptr;
    BranchWorkflowEditor* m_thenEditor = nullptr;
    BranchWorkflowEditor* m_elseEditor = nullptr;
};
