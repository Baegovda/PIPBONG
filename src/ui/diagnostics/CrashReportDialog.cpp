#include "ui/diagnostics/CrashReportDialog.h"

#include "core/diagnostics/CrashReporter.h"
#include "ui/widgets/HintLabel.h"

#include <QApplication>
#include <QClipboard>
#include <QDialogButtonBox>
#include <QFileDialog>
#include <QFont>
#include <QHBoxLayout>
#include <QLabel>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QTextEdit>
#include <QVBoxLayout>

namespace {

QString dialogTitleForKind(CrashReportKind kind, bool immediateCrash) {
    if (!immediateCrash) {
        return CrashReportDialog::tr("이전 실행 오류 보고서");
    }
    switch (kind) {
    case CrashReportKind::Hang:
        return CrashReportDialog::tr("응답 없음 오류 보고서");
    case CrashReportKind::QtFatal:
    case CrashReportKind::Crash:
    default:
        return CrashReportDialog::tr("오류 보고서");
    }
}

QString headlineForKind(CrashReportKind kind, bool immediateCrash) {
    if (!immediateCrash) {
        return CrashReportDialog::tr("이전 실행에서 오류가 발생했습니다.");
    }
    switch (kind) {
    case CrashReportKind::Hang:
        return CrashReportDialog::tr("PIPBONG이 응답하지 않아 오류 보고서를 저장했습니다.");
    case CrashReportKind::QtFatal:
    case CrashReportKind::Crash:
    default:
        return CrashReportDialog::tr("오류가 발생했습니다.");
    }
}

} // namespace

void CrashReportDialog::showPendingIfAny(QWidget* parent) {
    if (!CrashReporter::hasPendingReport()) {
        return;
    }
    const CrashReportSummary summary = CrashReporter::pendingReport();
    if (summary.reportText.trimmed().isEmpty()) {
        CrashReporter::dismissPendingReport();
        return;
    }

    CrashReportDialog dialog(summary.reportText, summary.folderPath, parent, false, summary.kind);
    dialog.exec();
    CrashReporter::dismissPendingReport();
}

CrashReportDialog::CrashReportDialog(const QString& reportText,
                                     const QString& folderPath,
                                     QWidget* parent,
                                     bool immediateCrash,
                                     CrashReportKind kind)
    : QDialog(parent)
    , m_folderPath(folderPath)
    , m_reportText(reportText) {
    setWindowTitle(dialogTitleForKind(kind, immediateCrash));
    setModal(true);
    resize(760, 560);
    setupUi(reportText, folderPath, immediateCrash, kind);
}

void CrashReportDialog::setupUi(const QString& reportText,
                                const QString& folderPath,
                                bool immediateCrash,
                                CrashReportKind kind) {
    auto* layout = new QVBoxLayout(this);

    auto* title = new QLabel(headlineForKind(kind, immediateCrash), this);
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

    auto* noteLabel = new QLabel(tr("무엇을 하고 있었나요? (선택)"), this);
    layout->addWidget(noteLabel);

    auto* noteEditor = new QPlainTextEdit(this);
    noteEditor->setPlaceholderText(tr("재현 상황이나 증상을 간단히 적어 주세요."));
    noteEditor->setMaximumHeight(72);
    layout->addWidget(noteEditor);

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Close, this);
    auto* copyButton = buttons->addButton(tr("복사"), QDialogButtonBox::ActionRole);
    auto* zipButton = buttons->addButton(tr("ZIP으로 저장"), QDialogButtonBox::ActionRole);
    auto* openFolderButton = buttons->addButton(tr("폴더 열기"), QDialogButtonBox::ActionRole);
    connect(copyButton, &QPushButton::clicked, this, [this, noteEditor]() {
        QString text = m_reportText;
        const QString note = noteEditor->toPlainText().trimmed();
        if (!note.isEmpty()) {
            text += QStringLiteral("\n\n--- user note ---\n");
            text += note;
        }
        if (QClipboard* clipboard = QApplication::clipboard()) {
            clipboard->setText(text);
        }
    });
    connect(zipButton, &QPushButton::clicked, this, [this, noteEditor]() {
        if (m_folderPath.isEmpty()) {
            return;
        }
        const QString note = noteEditor->toPlainText().trimmed();
        if (!note.isEmpty()) {
            CrashReporter::writeUserNote(m_folderPath, note);
        }
        const QString defaultName =
            QStringLiteral("PIPBONG-crash-%1.zip")
                .arg(QFileInfo(m_folderPath).fileName());
        const QString zipPath = QFileDialog::getSaveFileName(
            this,
            tr("오류 보고서 ZIP 저장"),
            defaultName,
            tr("ZIP 파일 (*.zip)"));
        if (zipPath.isEmpty()) {
            return;
        }
        if (!CrashReporter::exportReportFolderAsZip(m_folderPath, zipPath)) {
            QMessageBox::warning(this,
                                 tr("ZIP 저장 실패"),
                                 tr("ZIP 파일을 만들지 못했습니다. tar.exe가 없거나 보고서 폴더에 접근할 수 없습니다."));
        }
    });
    connect(openFolderButton, &QPushButton::clicked, this, [folderPath]() {
        const QString target = folderPath.isEmpty() ? CrashReporter::crashRootDirectory() : folderPath;
        CrashReporter::openPathInExplorer(target);
    });
    connect(buttons, &QDialogButtonBox::rejected, this, [this, noteEditor]() {
        const QString note = noteEditor->toPlainText().trimmed();
        if (!note.isEmpty() && !m_folderPath.isEmpty()) {
            CrashReporter::writeUserNote(m_folderPath, note);
        }
        reject();
    });
    layout->addWidget(buttons);
}
