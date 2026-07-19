#pragma once

#include "core/diagnostics/CrashReporter.h"

#include <QDialog>

class CrashReportDialog : public QDialog {
    Q_OBJECT
public:
    /// Shows a modal dialog when the previous run left a pending crash report.
    static void showPendingIfAny(QWidget* parent);

    explicit CrashReportDialog(const QString& reportText,
                               const QString& folderPath,
                               QWidget* parent = nullptr,
                               bool immediateCrash = false,
                               CrashReportKind kind = CrashReportKind::Crash);

private:
    void setupUi(const QString& reportText, const QString& folderPath, bool immediateCrash, CrashReportKind kind);

    QString m_folderPath;
    QString m_reportText;
};
