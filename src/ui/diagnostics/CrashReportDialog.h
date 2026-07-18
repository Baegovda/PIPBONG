#pragma once

#include <QDialog>

class CrashReportDialog : public QDialog {
    Q_OBJECT
public:
    /// Shows a modal dialog when the previous run left a pending crash report.
    static void showPendingIfAny(QWidget* parent);

    explicit CrashReportDialog(const QString& reportText,
                               const QString& folderPath,
                               QWidget* parent = nullptr);

private:
    void setupUi(const QString& reportText, const QString& folderPath);

    QString m_folderPath;
};
