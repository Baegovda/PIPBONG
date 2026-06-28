#pragma once

#include <QDialog>

class QCheckBox;

class ProgramSettingsDialog : public QDialog {
    Q_OBJECT
public:
    explicit ProgramSettingsDialog(QWidget* parent = nullptr);

private:
    void setupUi();

    QCheckBox* m_autoSelectRunningFeatureCheck = nullptr;
    QCheckBox* m_showWorkflowRunFeedbackCheck = nullptr;
};
