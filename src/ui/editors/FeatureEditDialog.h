#pragma once

#include "app/FeatureHotkeyGate.h"
#include "core/input/HotkeyBinding.h"
#include "model/FeatureRunMode.h"
#include "model/FeatureCaptureTargetScope.h"
#include "model/UserInputInterruptMode.h"

#include <QDialog>
#include <QEvent>
#include <memory>
#include <string>

class Project;
class QCheckBox;
class QComboBox;
class QLineEdit;
class QLabel;
class QPushButton;
class QShowEvent;
class QWidget;
class QStackedWidget;
class AnimatedTwoWaySwitch;
class DragAdjustSpinBox;

class FeatureEditDialog : public QDialog {
    Q_OBJECT
public:
    FeatureEditDialog(const QString& name,
                      bool enabled,
                      const HotkeyBinding& hotkey,
                      bool hotkeyAllowExtraModifiers,
                      FeatureCaptureTargetScope captureTargetScope,
                      bool requireScopedTargetForeground,
                      FeatureRunMode runMode,
                      int repeatCount,
                      int infiniteExitAfterConsecutiveMisses,
                      UserInputInterruptMode userInputInterruptMode,
                      bool pointerVisualFeedback,
                      bool restoreMousePositionOnEnd,
                      int lockMouseDuringFirstLoopCount,
                      int unlockMouseOnBlockFailureCount,
                      bool roiCorrection,
                      int roiCorrectionExpandPercent,
                      bool editFirstTemplateRoiOnStart,
                      int triggerCooldownMs,
                      int loopIntervalMs,
                      bool loopIntervalRandomRange,
                      int loopIntervalMinMs,
                      int loopIntervalMaxMs,
                      Project* project,
                      const std::string& featureId,
                      QWidget* parent = nullptr);

    QString featureName() const;
    bool featureEnabled() const;
    HotkeyBinding hotkey() const;
    bool hotkeyAllowExtraModifiers() const;
    FeatureCaptureTargetScope captureTargetScope() const;
    bool requireScopedTargetForeground() const;
    FeatureRunMode runMode() const;
    int repeatCount() const;
    int infiniteExitAfterConsecutiveMisses() const;
    UserInputInterruptMode userInputInterruptMode() const;
    bool pointerVisualFeedback() const;
    bool restoreMousePositionOnEnd() const;
    int lockMouseDuringFirstLoopCount() const;
    int unlockMouseOnBlockFailureCount() const;
    bool roiCorrection() const;
    int roiCorrectionExpandPercent() const;
    bool editFirstTemplateRoiOnStart() const;
    int triggerCooldownMs() const;
    int loopIntervalMs() const;
    bool loopIntervalRandomRange() const;
    int loopIntervalMinMs() const;
    int loopIntervalMaxMs() const;

protected:
    void showEvent(QShowEvent* event) override;
    void keyPressEvent(QKeyEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    bool eventFilter(QObject* watched, QEvent* event) override;
    void reject() override;

private:
    void setupUi();
    void updateHotkeyDisplay();
    void updateCaptureUi();
    void updateScopedTargetForegroundUi();
    void updateHotkeyOptionUi();
    void updateModeDependentUi();
    void updateLoopIntervalInputUi();
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
    std::unique_ptr<FeatureHotkeyGateScope> m_hotkeyCaptureGate;

    QLineEdit* m_nameEdit = nullptr;
    QCheckBox* m_enabledCheck = nullptr;
    QLabel* m_hotkeyLabel = nullptr;
    QCheckBox* m_hotkeyAllowExtraModifiersCheck = nullptr;
    QComboBox* m_captureTargetScopeCombo = nullptr;
    QCheckBox* m_requireScopedTargetForegroundCheck = nullptr;
    QComboBox* m_modeCombo = nullptr;
    QLabel* m_repeatCountLabel = nullptr;
    DragAdjustSpinBox* m_repeatSpin = nullptr;
    QCheckBox* m_infiniteExitCheck = nullptr;
    QLabel* m_infiniteExitCountLabel = nullptr;
    DragAdjustSpinBox* m_infiniteExitSpin = nullptr;
    QComboBox* m_userInputInterruptCombo = nullptr;
    QCheckBox* m_pointerVisualFeedbackCheck = nullptr;
    QCheckBox* m_restoreMousePositionOnEndCheck = nullptr;
    QCheckBox* m_earlyLoopMouseLockCheck = nullptr;
    QWidget* m_earlyLoopMouseLockDetailsRow = nullptr;
    DragAdjustSpinBox* m_earlyLoopMouseLockLoopSpin = nullptr;
    DragAdjustSpinBox* m_earlyLoopMouseUnlockFailureSpin = nullptr;
    QCheckBox* m_roiCorrectionCheck = nullptr;
    QWidget* m_roiCorrectionExpandRow = nullptr;
    DragAdjustSpinBox* m_roiCorrectionExpandSpin = nullptr;
    QCheckBox* m_editFirstTemplateRoiOnStartCheck = nullptr;
    QLabel* m_triggerCooldownLabel = nullptr;
    QWidget* m_triggerCooldownRow = nullptr;
    DragAdjustSpinBox* m_triggerCooldownSpin = nullptr;
    QWidget* m_loopIntervalSection = nullptr;
    AnimatedTwoWaySwitch* m_loopIntervalModeSwitch = nullptr;
    QStackedWidget* m_loopIntervalInputStack = nullptr;
    DragAdjustSpinBox* m_loopIntervalMsSpin = nullptr;
    DragAdjustSpinBox* m_loopIntervalMinSpin = nullptr;
    DragAdjustSpinBox* m_loopIntervalMaxSpin = nullptr;
};
