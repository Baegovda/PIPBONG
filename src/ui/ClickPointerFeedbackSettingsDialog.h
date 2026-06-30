#pragma once

#include "app/PointerFeedbackSettings.h"

#include <QDialog>

class QComboBox;
class QPushButton;
class QWidget;
class ClickPointerFeedbackPreviewWidget;
class DragAdjustDoubleSpinBox;
class DragAdjustSpinBox;

class ClickPointerFeedbackSettingsDialog : public QDialog {
    Q_OBJECT
public:
    explicit ClickPointerFeedbackSettingsDialog(QWidget* parent = nullptr);

    static QString shapeDisplayName(ClickPointerFeedbackShape shape);
    static QString settingsSummary(const ClickPointerFeedbackSettings& settings);

private:
    void setupUi();
    void loadFromSettings(const ClickPointerFeedbackSettings& settings);
    ClickPointerFeedbackSettings readDraftFromUi() const;
    void applyDraftToPreview();
    void updateFieldVisibility();
    void updateColorButton();
    void onPickColor();
    void onResetDefaults();

    ClickPointerFeedbackPreviewWidget* m_preview = nullptr;
    QPushButton* m_replayButton = nullptr;

    QWidget* m_ringCountLabel = nullptr;
    QWidget* m_ringCountField = nullptr;
    QWidget* m_ringThicknessLabel = nullptr;
    QWidget* m_ringThicknessField = nullptr;
    QWidget* m_coreSizeLabel = nullptr;
    QWidget* m_coreSizeField = nullptr;

    QComboBox* m_shapeCombo = nullptr;
    QPushButton* m_colorButton = nullptr;
    DragAdjustSpinBox* m_durationSpin = nullptr;
    DragAdjustDoubleSpinBox* m_speedSpin = nullptr;
    DragAdjustSpinBox* m_coreSizeSpin = nullptr;
    DragAdjustSpinBox* m_maxExpandSpin = nullptr;
    DragAdjustSpinBox* m_ringCountSpin = nullptr;
    DragAdjustSpinBox* m_ringThicknessSpin = nullptr;
    DragAdjustSpinBox* m_maxAlphaSpin = nullptr;

    QColor m_color;
};
