#pragma once

#include "core/workflow/blocks/WaitBlock.h"

#include <QDialog>

class QStackedWidget;
class AnimatedTwoWaySwitch;
class DragAdjustSpinBox;

class WaitEditor : public QDialog {
    Q_OBJECT
public:
    explicit WaitEditor(WaitBlock* block, QWidget* parent = nullptr, bool embedded = false);

    void setBlock(WaitBlock* block);
    void reload();
    void apply();

signals:
    void layoutChanged();

private:
    void setupUi();
    void updateInputUi();
    QWidget* makeMsInputRow(DragAdjustSpinBox* spin);

    WaitBlock* m_block = nullptr;
    bool m_embedded = false;
    DragAdjustSpinBox* m_msSpin = nullptr;
    DragAdjustSpinBox* m_minSpin = nullptr;
    DragAdjustSpinBox* m_maxSpin = nullptr;
    AnimatedTwoWaySwitch* m_modeSwitch = nullptr;
    QStackedWidget* m_inputStack = nullptr;
};
