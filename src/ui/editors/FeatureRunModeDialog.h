#pragma once

#include "model/FeatureRunMode.h"

#include <QDialog>

class QComboBox;
class QLabel;
class DragAdjustSpinBox;

class FeatureRunModeDialog : public QDialog {
    Q_OBJECT
public:
    explicit FeatureRunModeDialog(FeatureRunMode mode, int repeatCount, QWidget* parent = nullptr);

    FeatureRunMode runMode() const;
    int repeatCount() const;

private:
    void setupUi();
    void updateRepeatCountVisibility();

    FeatureRunMode m_mode;
    int m_repeatCount;
    QComboBox* m_modeCombo = nullptr;
    QLabel* m_repeatCountLabel = nullptr;
    DragAdjustSpinBox* m_repeatSpin = nullptr;
};
