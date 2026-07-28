#pragma once

#include "ui/TargetWindowBindingRole.h"

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
        QString subTargetWindowTitle;
        bool defaultProfile = false;
    };

    explicit ProfileEditDialog(const QString& profileName,
                               const QString& targetWindowTitle,
                               const QString& subTargetWindowTitle,
                               const QString& linkedTargetProcessPath,
                               const QString& subLinkedTargetProcessPath,
                               bool defaultProfile,
                               bool fixedDefaultProfile,
                               const QString& currentTargetWindowTitle,
                               QWidget* parent = nullptr);

    Result result() const;

private:
    void setupUi(const QString& currentTargetWindowTitle);
    void openWindowListPicker(QLineEdit* targetEdit, bool subTarget);
    void pickTargetWindowByClick(QLineEdit* targetEdit, TargetWindowBindingRole role);
    void tryAccept();
    void updateDefaultProfileUi();
    void refreshTargetBindingDetails();
    QString bindingDetailText(const QString& binding, const QString& storedProcessPath, bool subRole) const;

    QLineEdit* m_nameEdit = nullptr;
    QLabel* m_fixedDefaultNameLabel = nullptr;
    QLineEdit* m_targetWindowTitleEdit = nullptr;
    QLineEdit* m_subTargetWindowTitleEdit = nullptr;
    QCheckBox* m_defaultProfileCheck = nullptr;
    QWidget* m_linkedProgramSection = nullptr;
    QLabel* m_mainTargetDetailLabel = nullptr;
    QLabel* m_subTargetDetailLabel = nullptr;
    QString m_linkedTargetProcessPath;
    QString m_subLinkedTargetProcessPath;
    bool m_fixedDefaultProfile = false;
};
