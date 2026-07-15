#include "ui/memo/MemoDialog.h"

#include "storage/ProfileMemoStore.h"

#include <QCloseEvent>
#include <QSettings>
#include <QShowEvent>
#include <QTextEdit>
#include <QTimer>
#include <QVBoxLayout>

MemoDialog::MemoDialog(QWidget* parent)
    : QDialog(parent) {
    setWindowTitle(tr("메모장"));
    setMinimumSize(360, 280);
    resize(480, 360);

    m_editor = new QTextEdit(this);
    m_editor->setAcceptRichText(false);
    m_editor->setPlaceholderText(tr("프로필별 메모를 입력하세요."));

    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(8, 8, 8, 8);
    layout->addWidget(m_editor);

    m_saveTimer = new QTimer(this);
    m_saveTimer->setSingleShot(true);
    m_saveTimer->setInterval(600);
    connect(m_saveTimer, &QTimer::timeout, this, &MemoDialog::saveNow);
    connect(m_editor, &QTextEdit::textChanged, this, &MemoDialog::scheduleSave);
}

void MemoDialog::setProfile(const QString& profileId,
                            const QString& profileDirectory,
                            const QString& profileDisplayName) {
    if (profileId == m_profileId && profileDirectory == m_profileDirectory) {
        m_profileDisplayName = profileDisplayName;
        updateWindowTitle();
        return;
    }

    saveNow();

    m_profileId = profileId;
    m_profileDirectory = profileDirectory;
    m_profileDisplayName = profileDisplayName;
    updateWindowTitle();

    m_loading = true;
    QString text;
    ProfileMemoStore::load(profileDirectory, &text);
    m_editor->setPlainText(text);
    m_loading = false;
}

void MemoDialog::saveNow() {
    if (m_loading || m_profileDirectory.isEmpty()) {
        return;
    }
    m_saveTimer->stop();
    ProfileMemoStore::save(m_profileDirectory, m_editor->toPlainText());
}

void MemoDialog::scheduleSave() {
    if (m_loading || m_profileDirectory.isEmpty()) {
        return;
    }
    m_saveTimer->start();
}

void MemoDialog::closeEvent(QCloseEvent* event) {
    saveNow();
    persistGeometry();
    QDialog::closeEvent(event);
}

void MemoDialog::showEvent(QShowEvent* event) {
    QDialog::showEvent(event);
    restorePersistedGeometry();
}

void MemoDialog::persistGeometry() {
    QSettings settings;
    settings.setValue(QStringLiteral("memo/geometry"), saveGeometry());
}

void MemoDialog::restorePersistedGeometry() {
    const QByteArray geometry =
        QSettings().value(QStringLiteral("memo/geometry")).toByteArray();
    if (!geometry.isEmpty()) {
        QDialog::restoreGeometry(geometry);
    }
}

void MemoDialog::updateWindowTitle() {
    if (m_profileDisplayName.isEmpty()) {
        setWindowTitle(tr("메모장"));
        return;
    }
    setWindowTitle(tr("메모장 — %1").arg(m_profileDisplayName));
}
