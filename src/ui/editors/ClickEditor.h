#pragma once

#include "core/workflow/blocks/ClickBlock.h"

#include <QDialog>
#include <QEvent>
#include <QKeyEvent>

#include <functional>
#include <memory>

class QComboBox;
class QGroupBox;
class QShowEvent;
class QHideEvent;
class DragAdjustSpinBox;
class QCheckBox;
class QPushButton;
class QLabel;
class QTimer;
class QWidget;

class ClickEditor : public QDialog {
    Q_OBJECT
public:
    explicit ClickEditor(ClickBlock* block, QWidget* parent = nullptr, bool embedded = false);
    ~ClickEditor() override;

    void setBlock(ClickBlock* block);
    void reload();
    void apply();

    void setFeatureRunOptions(bool lockMouseToScreenCenterDuringRun,
                              bool lockMouseToCurrentPositionDuringRun);
    bool lockMouseToScreenCenterDuringRun() const;
    bool lockMouseToCurrentPositionDuringRun() const;

    using ContinuousInputBlockHandler = std::function<void(std::unique_ptr<ClickBlock>)>;
    void setContinuousInputBlockHandler(ContinuousInputBlockHandler handler);

signals:
    void layoutChanged();

public slots:
    void triggerCursorPositionFromHotkey();
    void toggleContinuousInputArmed();
    void refreshClickCursorHotkeyHook();

    bool isClickCursorHotkeyHookActive() const;
    bool isCapturingCursorHotkey() const { return m_capturingCursorHotkey; }
    bool isCapturingContinuousInputHotkey() const { return m_capturingContinuousInputHotkey; }
    bool isContinuousInputHotkeyHookEligible() const;

private slots:
    void onPickCoordinates();
    void updateTargetUi();
    void updateButtonUi();
    void updateActionDependentUi();
    void updateLiveCursorPosition();
    void updateLiveCursorLabelStyle();
    bool isMoveOnlySelected() const;

protected:
    void showEvent(QShowEvent* event) override;
    void hideEvent(QHideEvent* event) override;
    void changeEvent(QEvent* event) override;
    void keyPressEvent(QKeyEvent* event) override;
    bool eventFilter(QObject* watched, QEvent* event) override;

private:
    void updatePickCoordButton(bool pickActive);

    void setupUi();
    void syncClickCursorHotkeyHook();
    bool queryCurrentCursorCoordinates(int& x, int& y) const;
    void applyCurrentCursorToCoordinateFields();
    void updateCursorHotkeyDisplay();
    void updateCursorHotkeyCaptureUi();
    void startCursorHotkeyCapture();
    void stopCursorHotkeyCapture();
    void applyCursorHotkeyCapture(int virtualKey, Qt::KeyboardModifiers modifiers);
    void updateContinuousInputUi();
    void syncContinuousInputSession();
    void startContinuousInputHotkeyCapture();
    void stopContinuousInputHotkeyCapture();
    void applyContinuousInputHotkeyCapture(int virtualKey, Qt::KeyboardModifiers modifiers);
    void updateContinuousInputHotkeyDisplay();
    void updateContinuousInputHotkeyCaptureUi();
    std::unique_ptr<ClickBlock> buildFixedClickBlockAt(int x, int y) const;
    int buttonComboIndexForBlock() const;
    int actionComboIndexForBlock() const;

    ClickBlock* m_block = nullptr;
    bool m_embedded = false;

    QComboBox* m_targetCombo = nullptr;
    QCheckBox* m_moveOnlyCheck = nullptr;
    QLabel* m_lastMatchHint = nullptr;
    QCheckBox* m_lastMatchRelativeCheck = nullptr;
    QLabel* m_currentPositionHint = nullptr;
    QLabel* m_liveCursorLabel = nullptr;
    QTimer* m_cursorPollTimer = nullptr;
    QWidget* m_clientCoordsRow = nullptr;
    QGroupBox* m_fixedCoordGroup = nullptr;
    DragAdjustSpinBox* m_xSpin = nullptr;
    DragAdjustSpinBox* m_ySpin = nullptr;
    QPushButton* m_pickCoordButton = nullptr;
    QLabel* m_cursorHotkeyLabel = nullptr;
    bool m_capturingCursorHotkey = false;
    QWidget* m_continuousInputRow = nullptr;
    QLabel* m_continuousInputStatusLabel = nullptr;
    QLabel* m_continuousInputHotkeyLabel = nullptr;
    bool m_capturingContinuousInputHotkey = false;
    bool m_continuousInputArmed = false;
    ContinuousInputBlockHandler m_continuousInputBlockHandler;
    QCheckBox* m_clientCoordsCheck = nullptr;
    QGroupBox* m_actionGroup = nullptr;
    QComboBox* m_buttonCombo = nullptr;
    QLabel* m_buttonLabel = nullptr;
    QLabel* m_actionLabel = nullptr;
    QComboBox* m_actionCombo = nullptr;
    QCheckBox* m_ctrlCheck = nullptr;
    QCheckBox* m_altCheck = nullptr;
    QCheckBox* m_shiftCheck = nullptr;
    QWidget* m_modifierRow = nullptr;

    QGroupBox* m_featureRunGroup = nullptr;
    QCheckBox* m_lockMouseToScreenCenterCheck = nullptr;
    QCheckBox* m_lockMouseToCurrentPositionCheck = nullptr;
};
