#pragma once

#include "app/PointerFeedbackSettings.h"

#include <QDialog>

class QCheckBox;
class QComboBox;
class QPushButton;
class QSplitter;
class QWidget;
class WindowSelectionFeedbackPreviewWidget;
class DragAdjustDoubleSpinBox;
class DragAdjustSpinBox;

class WindowSelectionFeedbackSettingsDialog : public QDialog {
    Q_OBJECT
public:
    explicit WindowSelectionFeedbackSettingsDialog(QWidget* parent = nullptr);

    static QString styleDisplayName(WindowSelectionFeedbackStyle style);
    static QString settingsSummary(const WindowSelectionFeedbackSettings& settings);

private:
    void setupUi();
    void loadFromSettings(const WindowSelectionFeedbackSettings& settings);
    WindowSelectionFeedbackSettings readDraftFromUi() const;
    void applyDraftToPreview();
    void updateFieldVisibility();
    void updateColorButton();
    void onPickColor();
    void onResetDefaults();

    WindowSelectionFeedbackPreviewWidget* m_preview = nullptr;
    QPushButton* m_replayButton = nullptr;
    QSplitter* m_bodySplitter = nullptr;

    QCheckBox* m_enabledCheck = nullptr;
    QWidget* m_effectsGroup = nullptr;

    QWidget* m_pingRingWidthLabel = nullptr;
    QWidget* m_pingRingWidthField = nullptr;
    QWidget* m_edgeGlowWidthLabel = nullptr;
    QWidget* m_edgeGlowWidthField = nullptr;
    QWidget* m_bracketScaleLabel = nullptr;
    QWidget* m_bracketScaleField = nullptr;
    QWidget* m_echoRingLabel = nullptr;
    QWidget* m_echoRingField = nullptr;
    QWidget* m_centerBloomLabel = nullptr;
    QWidget* m_centerBloomField = nullptr;
    QWidget* m_edgeGlowToggleLabel = nullptr;
    QWidget* m_edgeGlowToggleField = nullptr;
    QWidget* m_cornerBracketsLabel = nullptr;
    QWidget* m_cornerBracketsField = nullptr;

    QComboBox* m_styleCombo = nullptr;
    QPushButton* m_colorButton = nullptr;
    DragAdjustSpinBox* m_durationSpin = nullptr;
    DragAdjustDoubleSpinBox* m_speedSpin = nullptr;
    DragAdjustSpinBox* m_pingRingWidthSpin = nullptr;
    DragAdjustSpinBox* m_edgeGlowWidthSpin = nullptr;
    DragAdjustSpinBox* m_bracketScaleSpin = nullptr;
    DragAdjustSpinBox* m_maxAlphaSpin = nullptr;
    QCheckBox* m_echoRingCheck = nullptr;
    QCheckBox* m_centerBloomCheck = nullptr;
    QCheckBox* m_edgeGlowCheck = nullptr;
    QCheckBox* m_cornerBracketsCheck = nullptr;

    QColor m_color;
};
