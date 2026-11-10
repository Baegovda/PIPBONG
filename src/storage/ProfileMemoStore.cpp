#include "storage/ProfileMemoStore.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSaveFile>

QString ProfileMemoStore::memoFilePath(const QString& profileDirectory) {
    return QDir(profileDirectory).filePath(QStringLiteral("memo.txt"));
}

bool ProfileMemoStore::load(const QString& profileDirectory, QString* outText) {
    if (!outText) {
        return false;
    }
    if (profileDirectory.trimmed().isEmpty()) {
        outText->clear();
        return true;
    }

    QFile file(memoFilePath(profileDirectory));
    if (!file.exists()) {
        outText->clear();
        return true;
    }
    if (!file.open(QIODevice::ReadOnly)) {
        return false;
    }
    const QByteArray bytes = file.readAll();
    if (file.error() != QFileDevice::NoError) return false;
    *outText = QString::fromUtf8(bytes);
    return true;
}

bool ProfileMemoStore::save(const QString& profileDirectory, const QString& text) {
    if (profileDirectory.trimmed().isEmpty()) {
        return false;
    }
    QDir().mkpath(profileDirectory);

    QSaveFile file(memoFilePath(profileDirectory));
    if (!file.open(QIODevice::WriteOnly)) {
        return false;
    }
    const QByteArray bytes = text.toUtf8();
    if (file.write(bytes) != bytes.size()) {
        file.cancelWriting();
        return false;
    }
    return file.commit();
}
