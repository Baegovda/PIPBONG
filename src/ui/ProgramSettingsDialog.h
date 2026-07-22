#pragma once

#include <QDialog>

class QCheckBox;
class QComboBox;
class QLabel;
class QPushButton;
class DragAdjustSpinBox;

class ProgramSettingsDialog : public QDialog {
    Q_OBJECT
public:
    explicit ProgramSettingsDialog(QWidget* parent = nullptr);

private:
    void setupUi();
    void updateClickFeedbackSummary();
    void updateWindowSelectionFeedbackSummary();
    void updateImageFindCaptureModeToolTip();
    void onOpenClickFeedbackSettings();
    void onOpenWindowSelectionFeedbackSettings();

    QCheckBox* m_autoSelectRunningFeatureCheck = nullptr;
    QCheckBox* m_focusTargetWindowOnProfileSelectCheck = nullptr;
    QCheckBox* m_launchAtWindowsStartupCheck = nullptr;
    QCheckBox* m_closeToTrayCheck = nullptr;
    QCheckBox* m_runAsAdministratorCheck = nullptr;
    QCheckBox* m_autoInstallUpdatesCheck = nullptr;
    QCheckBox* m_updateCheckEnabledCheck = nullptr;
    DragAdjustSpinBox* m_updateCheckIntervalSpin = nullptr;
    QLabel* m_updateCheckIntervalLabel = nullptr;
    QComboBox* m_imageFindCaptureModeCombo = nullptr;
    QCheckBox* m_runWithoutTargetWindowCheck = nullptr;
    DragAdjustSpinBox* m_logMaxLinesSpin = nullptr;
    QCheckBox* m_workflowRunProfilingCheck = nullptr;
    QComboBox* m_workflowRunProfilingDepthCombo = nullptr;
    QLabel* m_clickFeedbackSummary = nullptr;
    QPushButton* m_clickFeedbackButton = nullptr;
    QLabel* m_windowSelectionFeedbackSummary = nullptr;
    QPushButton* m_windowSelectionFeedbackButton = nullptr;
};
