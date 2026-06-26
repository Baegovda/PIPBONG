#pragma once

#include "core/workflow/Workflow.h"
#include "core/workflow/WorkflowLoopRegion.h"

#include <QDialog>

class QComboBox;
class QListWidget;
class DragAdjustSpinBox;

class WorkflowLoopRegionEditDialog : public QDialog {
    Q_OBJECT
public:
    WorkflowLoopRegionEditDialog(int blockCount,
                                 const WorkflowLoopRegion* existing,
                                 int defaultStartBlock,
                                 int defaultEndBlock,
                                 bool lockBlockRange = false,
                                 QWidget* parent = nullptr);

    WorkflowLoopRegion resultRegion() const;

private:
    void setupUi();
    LoopExitCondition exitConditionFromComboIndex(int index) const;
    int comboIndexForExitCondition(LoopExitCondition condition) const;
    void updateConditionDependentUi();

    int m_blockCount = 0;
    bool m_lockBlockRange = false;
    WorkflowLoopRegion m_region;
    QComboBox* m_exitConditionCombo = nullptr;
    DragAdjustSpinBox* m_startSpin = nullptr;
    DragAdjustSpinBox* m_endSpin = nullptr;
    DragAdjustSpinBox* m_missLimitSpin = nullptr;
    class QLabel* m_missLimitLabel = nullptr;
    class QLabel* m_rangeLabel = nullptr;
};

class WorkflowLoopRegionsDialog : public QDialog {
    Q_OBJECT
public:
    WorkflowLoopRegionsDialog(Workflow* workflow,
                              int defaultStartBlock,
                              int defaultEndBlock,
                              QWidget* parent = nullptr);

private:
    void setupUi();
    void refreshList();
    void addRegion();
    void editRegion();
    void deleteRegion();
    QString regionListLabel(const WorkflowLoopRegion& region) const;

    Workflow* m_workflow = nullptr;
    int m_defaultStartBlock = 1;
    int m_defaultEndBlock = 1;
    QListWidget* m_list = nullptr;
};
