#pragma once

#include <QDialog>

class QCheckBox;
class QLabel;
class QPushButton;

class ProgramSettingsDialog : public QDialog {
    Q_OBJECT
public:
    explicit ProgramSettingsDialog(QWidget* parent = nullptr);

private:
    void setupUi();
    void updateClickFeedbackSummary();
    void onOpenClickFeedbackSettings();

    QCheckBox* m_autoSelectRunningFeatureCheck = nullptr;
    QCheckBox* m_launchAtWindowsStartupCheck = nullptr;
    QCheckBox* m_closeToTrayCheck = nullptr;
    QCheckBox* m_runAsAdministratorCheck = nullptr;
    QLabel* m_clickFeedbackSummary = nullptr;
    QPushButton* m_clickFeedbackButton = nullptr;
};
