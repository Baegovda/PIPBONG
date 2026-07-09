#pragma once

#include <QDialog>

class QCheckBox;
class QLineEdit;
class QLabel;
class QWidget;

class ProfileEditDialog : public QDialog {
    Q_OBJECT
public:
    struct Result {
        QString name;
        QString targetWindowTitle;
        bool defaultProfile = false;
    };

    explicit ProfileEditDialog(const QString& profileName,
                               const QString& targetWindowTitle,
                               bool defaultProfile,
                               bool fixedDefaultProfile,
                               const QString& currentTargetWindowTitle,
                               QWidget* parent = nullptr);

    Result result() const;

private:
    void setupUi(const QString& currentTargetWindowTitle);
    void openWindowListPicker();
    void tryAccept();
    void updateDefaultProfileUi();

    QLineEdit* m_nameEdit = nullptr;
    QLabel* m_fixedDefaultNameLabel = nullptr;
    QLineEdit* m_targetWindowTitleEdit = nullptr;
    QCheckBox* m_defaultProfileCheck = nullptr;
    QWidget* m_linkedProgramSection = nullptr;
    bool m_fixedDefaultProfile = false;
};
