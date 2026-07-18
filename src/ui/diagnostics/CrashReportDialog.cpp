#include "ui/diagnostics/CrashReportDialog.h"

#include "core/diagnostics/CrashReporter.h"
#include "ui/widgets/HintLabel.h"

#include <QApplication>
#include <QClipboard>
#include <QDialogButtonBox>
#include <QFont>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QTextEdit>
#include <QVBoxLayout>

void CrashReportDialog::showPendingIfAny(QWidget* parent) {
    if (!CrashReporter::hasPendingReport()) {
        return;
    }
    const CrashReportSummary summary = CrashReporter::pendingReport();
    if (summary.reportText.trimmed().isEmpty()) {
        CrashReporter::dismissPendingReport();
        return;
    }

    CrashReportDialog dialog(summary.reportText, summary.folderPath, parent);
    dialog.exec();
    CrashReporter::dismissPendingReport();
}

CrashReportDialog::CrashReportDialog(const QString& reportText,
                                     const QString& folderPath,
                                     QWidget* parent)
    : QDialog(parent)
    , m_folderPath(folderPath) {
    setWindowTitle(tr("이전 실행 오류 보고서"));
    setModal(true);
    resize(760, 520);
    setupUi(reportText, folderPath);
}

void CrashReportDialog::setupUi(const QString& reportText, const QString& folderPath) {
    auto* layout = new QVBoxLayout(this);

    auto* title = new QLabel(tr("이전 실행에서 오류가 발생했습니다."), this);
    QFont titleFont = title->font();
    titleFont.setBold(true);
    title->setFont(titleFont);
    layout->addWidget(title);

    auto* hint = new HintLabel(
        tr("아래 내용을 복사해 개발자에게 전달하면 원인 파악에 도움이 됩니다. 보고서 파일은 %LOCALAPPDATA%\\PIPBONG\\PIPBONG\\crash\\ 아래에 저장됩니다."),
        this);
    hint->setWordWrap(true);
    layout->addWidget(hint);

    if (!folderPath.isEmpty()) {
        auto* pathLabel = new QLabel(tr("저장 위치: %1").arg(folderPath), this);
        pathLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
        pathLabel->setWordWrap(true);
        layout->addWidget(pathLabel);
    }

    auto* editor = new QTextEdit(this);
    editor->setReadOnly(true);
    editor->setFontFamily(QStringLiteral("Consolas"));
    editor->setPlainText(reportText);
    layout->addWidget(editor, 1);

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Close, this);
    auto* copyButton = buttons->addButton(tr("복사"), QDialogButtonBox::ActionRole);
    auto* openFolderButton = buttons->addButton(tr("폴더 열기"), QDialogButtonBox::ActionRole);
    connect(copyButton, &QPushButton::clicked, this, [reportText]() {
        if (QClipboard* clipboard = QApplication::clipboard()) {
            clipboard->setText(reportText);
        }
    });
    connect(openFolderButton, &QPushButton::clicked, this, [folderPath]() {
        const QString target = folderPath.isEmpty() ? CrashReporter::crashRootDirectory() : folderPath;
        CrashReporter::openPathInExplorer(target);
    });
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    layout->addWidget(buttons);
}
