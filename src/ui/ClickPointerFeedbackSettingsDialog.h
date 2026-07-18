#pragma once

#include "app/PointerFeedbackSettings.h"

#include <QDialog>

class QComboBox;
class QPushButton;
class QSplitter;
class QWidget;
class ClickPointerFeedbackPreviewWidget;
class DragAdjustDoubleSpinBox;
class DragAdjustSpinBox;
class HintLabel;

enum class PointerFeedbackDialogKind {
    Click,
    Detection,
};

class ClickPointerFeedbackSettingsDialog : public QDialog {
    Q_OBJECT
public:
    explicit ClickPointerFeedbackSettingsDialog(QWidget* parent = nullptr);

    void setDialogKind(PointerFeedbackDialogKind kind);
    void setPersistToGlobalSettingsOnAccept(bool persist);
    ClickPointerFeedbackSettings acceptedSettings() const { return m_acceptedSettings; }
    void loadFromSettings(const ClickPointerFeedbackSettings& settings);

    static QString shapeDisplayName(ClickPointerFeedbackShape shape);
    static QString settingsSummary(const ClickPointerFeedbackSettings& settings);

private:
    void setupUi();
    ClickPointerFeedbackSettings readDraftFromUi() const;
    void applyDraftToPreview();
    void updateFieldVisibility();
    void updateColorButtons();
    void onPickColor();
    void onPickSuccessCoreColor();
    void onPickSuccessRingColor();
    void onPickMissCoreColor();
    void onPickMissRingColor();
    void onResetDefaults();
    void applyDialogKindUi();

    ClickPointerFeedbackPreviewWidget* m_preview = nullptr;
    HintLabel* m_hintLabel = nullptr;
    QPushButton* m_replayButton = nullptr;
    QSplitter* m_bodySplitter = nullptr;

    QWidget* m_ringCountLabel = nullptr;
    QWidget* m_ringCountField = nullptr;
    QWidget* m_ringThicknessLabel = nullptr;
    QWidget* m_ringThicknessField = nullptr;
    QWidget* m_coreSizeLabel = nullptr;
    QWidget* m_coreSizeField = nullptr;

    QComboBox* m_shapeCombo = nullptr;
    QWidget* m_clickColorLabel = nullptr;
    QPushButton* m_colorButton = nullptr;
    QWidget* m_successCoreColorLabel = nullptr;
    QPushButton* m_successCoreColorButton = nullptr;
    QWidget* m_successRingColorLabel = nullptr;
    QPushButton* m_successRingColorButton = nullptr;
    QWidget* m_missCoreColorLabel = nullptr;
    QPushButton* m_missCoreColorButton = nullptr;
    QWidget* m_missRingColorLabel = nullptr;
    QPushButton* m_missRingColorButton = nullptr;
    DragAdjustSpinBox* m_durationSpin = nullptr;
    DragAdjustDoubleSpinBox* m_speedSpin = nullptr;
    DragAdjustSpinBox* m_coreSizeSpin = nullptr;
    DragAdjustSpinBox* m_maxExpandSpin = nullptr;
    DragAdjustSpinBox* m_ringCountSpin = nullptr;
    DragAdjustSpinBox* m_ringThicknessSpin = nullptr;
    DragAdjustSpinBox* m_maxAlphaSpin = nullptr;

    QColor m_color;
    QColor m_successCoreColor;
    QColor m_successRingColor;
    QColor m_missCoreColor;
    QColor m_missRingColor;
    PointerFeedbackDialogKind m_dialogKind = PointerFeedbackDialogKind::Click;
    bool m_persistToGlobalSettingsOnAccept = true;
    ClickPointerFeedbackSettings m_acceptedSettings{};
};
