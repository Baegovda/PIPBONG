#include "storage/ProfileMemoStore.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>

QString ProfileMemoStore::memoFilePath(const QString& profileDirectory) {
    return QDir(profileDirectory).filePath(QStringLiteral("memo.txt"));
}

bool ProfileMemoStore::load(const QString& profileDirectory, QString* outText) {
    if (!outText) {
        return false;
    }
    outText->clear();
    if (profileDirectory.trimmed().isEmpty()) {
        return true;
    }

    QFile file(memoFilePath(profileDirectory));
    if (!file.exists()) {
        return true;
    }
    if (!file.open(QIODevice::ReadOnly)) {
        return false;
    }
    *outText = QString::fromUtf8(file.readAll());
    return true;
}

bool ProfileMemoStore::save(const QString& profileDirectory, const QString& text) {
    if (profileDirectory.trimmed().isEmpty()) {
        return false;
    }
    QDir().mkpath(profileDirectory);

    QFile file(memoFilePath(profileDirectory));
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        return false;
    }
    return file.write(text.toUtf8()) >= 0;
}
