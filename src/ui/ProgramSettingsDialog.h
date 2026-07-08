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
    void updateImageFindCaptureModeHint();
    void onOpenClickFeedbackSettings();

    QCheckBox* m_autoSelectRunningFeatureCheck = nullptr;
    QCheckBox* m_launchAtWindowsStartupCheck = nullptr;
    QCheckBox* m_closeToTrayCheck = nullptr;
    QCheckBox* m_runAsAdministratorCheck = nullptr;
    QCheckBox* m_autoInstallUpdatesCheck = nullptr;
    QCheckBox* m_updateCheckEnabledCheck = nullptr;
    DragAdjustSpinBox* m_updateCheckIntervalSpin = nullptr;
    QLabel* m_updateCheckIntervalLabel = nullptr;
    QComboBox* m_imageFindCaptureModeCombo = nullptr;
    QLabel* m_imageFindCaptureModeHint = nullptr;
    QCheckBox* m_runWithoutTargetWindowCheck = nullptr;
    QLabel* m_clickFeedbackSummary = nullptr;
    QPushButton* m_clickFeedbackButton = nullptr;
};
