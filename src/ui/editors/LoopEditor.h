#pragma once

#include "core/workflow/blocks/LoopBlock.h"

#include <QDialog>

class QComboBox;
class QLabel;
class BranchWorkflowEditor;
class DragAdjustSpinBox;

class LoopEditor : public QDialog {
    Q_OBJECT
public:
    explicit LoopEditor(LoopBlock* block, const QString& projectDirectory, QWidget* parent = nullptr,
                        bool embedded = false);

    void setBlock(LoopBlock* block);
    void reload();
    void apply();

signals:
    void layoutChanged();

private:
    void setupUi();
    void updateConditionDependentUi();
    LoopExitCondition exitConditionFromComboIndex(int index) const;
    int comboIndexForExitCondition(LoopExitCondition condition) const;

    LoopBlock* m_block = nullptr;
    QString m_projectDirectory;
    bool m_embedded = false;
    QComboBox* m_exitConditionCombo = nullptr;
    QLabel* m_missLimitLabel = nullptr;
    DragAdjustSpinBox* m_missLimitSpin = nullptr;
    BranchWorkflowEditor* m_bodyEditor = nullptr;
};
