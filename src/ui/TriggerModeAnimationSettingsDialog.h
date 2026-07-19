#pragma once

#include "model/TriggerListAnimationSettings.h"

#include <QDialog>

class QCheckBox;
class QComboBox;
class QPushButton;
class QListWidget;
class QStackedWidget;
class DragAdjustDoubleSpinBox;
class TriggerListAnimationPreviewWidget;

class TriggerModeAnimationSettingsDialog : public QDialog {
    Q_OBJECT
public:
    explicit TriggerModeAnimationSettingsDialog(const TriggerModeListAnimationSettings& settings,
                                                QWidget* parent = nullptr);

    TriggerModeListAnimationSettings settings() const;

private:
    void setupUi();
    void populateStyleCombo(QComboBox* combo, TriggerAnimationState state);
    void loadStateEditor(int stateIndex);
    void applyCurrentEditorToDraft();
    void updatePreview();
    void updateSonarOptionVisibility();
    void onPickAccentColor();
    void onResetDefaults();

    TriggerModeListAnimationSettings m_draft;
    TriggerListAnimationPreviewWidget* m_preview = nullptr;
    QListWidget* m_stateList = nullptr;
    QStackedWidget* m_stateStack = nullptr;
    QComboBox* m_styleCombo = nullptr;
    QPushButton* m_colorButton = nullptr;
    DragAdjustDoubleSpinBox* m_speedSpin = nullptr;
    QCheckBox* m_showSonarRipplesCheck = nullptr;
    QCheckBox* m_showRadarSweepCheck = nullptr;
    int m_loadingStateIndex = -1;
};
