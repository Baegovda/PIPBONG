#pragma once

#include "core/input/HotkeyBinding.h"
#include "model/FeatureRunMode.h"
#include "model/UserInputInterruptMode.h"

#include <QDialog>
#include <QEvent>
#include <string>

class Project;
class QCheckBox;
class QComboBox;
class QLineEdit;
class QLabel;
class QPushButton;
class QWidget;
class DragAdjustSpinBox;

class FeatureEditDialog : public QDialog {
    Q_OBJECT
public:
    FeatureEditDialog(const QString& name,
                      const HotkeyBinding& hotkey,
                      FeatureRunMode runMode,
                      int repeatCount,
                      int infiniteExitAfterConsecutiveMisses,
                      UserInputInterruptMode userInputInterruptMode,
                      bool pointerVisualFeedback,
                      bool restoreMousePositionOnEnd,
                      bool roiCorrection,
                      int roiCorrectionExpandPercent,
                      bool editFirstTemplateRoiOnStart,
                      int triggerCooldownMs,
                      Project* project,
                      const std::string& featureId,
                      QWidget* parent = nullptr);

    QString featureName() const;
    HotkeyBinding hotkey() const;
    FeatureRunMode runMode() const;
    int repeatCount() const;
    int infiniteExitAfterConsecutiveMisses() const;
    UserInputInterruptMode userInputInterruptMode() const;
    bool pointerVisualFeedback() const;
    bool restoreMousePositionOnEnd() const;
    bool roiCorrection() const;
    int roiCorrectionExpandPercent() const;
    bool editFirstTemplateRoiOnStart() const;
    int triggerCooldownMs() const;

protected:
    void keyPressEvent(QKeyEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    bool eventFilter(QObject* watched, QEvent* event) override;

private:
    void setupUi();
    void updateHotkeyDisplay();
    void updateCaptureUi();
    void updateModeDependentUi();
    void startHotkeyCapture();
    void stopHotkeyCapture();
    void applyCapturedBinding(int virtualKey, Qt::KeyboardModifiers modifiers);
    void tryAccept();
    bool isInteractiveWidget(const QWidget* widget) const;
    void applyHotkeyLabelIdleStyle();

    Project* m_project = nullptr;
    std::string m_featureId;
    HotkeyBinding m_hotkey;
    bool m_listeningForHotkey = false;

    QLineEdit* m_nameEdit = nullptr;
    QLabel* m_hotkeyLabel = nullptr;
    QComboBox* m_modeCombo = nullptr;
    QLabel* m_repeatCountLabel = nullptr;
    DragAdjustSpinBox* m_repeatSpin = nullptr;
    QCheckBox* m_infiniteExitCheck = nullptr;
    QLabel* m_infiniteExitCountLabel = nullptr;
    DragAdjustSpinBox* m_infiniteExitSpin = nullptr;
    QComboBox* m_userInputInterruptCombo = nullptr;
    QCheckBox* m_pointerVisualFeedbackCheck = nullptr;
    QCheckBox* m_restoreMousePositionOnEndCheck = nullptr;
    QCheckBox* m_roiCorrectionCheck = nullptr;
    QWidget* m_roiCorrectionExpandRow = nullptr;
    DragAdjustSpinBox* m_roiCorrectionExpandSpin = nullptr;
    QCheckBox* m_editFirstTemplateRoiOnStartCheck = nullptr;
    QLabel* m_triggerCooldownLabel = nullptr;
    QWidget* m_triggerCooldownRow = nullptr;
    DragAdjustSpinBox* m_triggerCooldownSpin = nullptr;
};
