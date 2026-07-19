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

    QLineEdit* m_nameEdit = nullptr;
    QLabel* m_fixedDefaultNameLabel = nullptr;
    QLineEdit* m_targetWindowTitleEdit = nullptr;
    QLineEdit* m_subTargetWindowTitleEdit = nullptr;
    QCheckBox* m_defaultProfileCheck = nullptr;
    QWidget* m_linkedProgramSection = nullptr;
    bool m_fixedDefaultProfile = false;
};
