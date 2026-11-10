#include "storage/ProjectHistoryStore.h"
#include <QCryptographicHash>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QSaveFile>
#include <QMutex>
#include <QMutexLocker>

namespace {
QMutex historyWriteMutex;
const QString snapshotDateFormat = QStringLiteral("yyyyMMdd'T'HHmmsszzz'Z'");
}

QString ProjectHistoryStore::directoryFor(const QString& path) {
    return QFileInfo(path).absoluteFilePath() + QStringLiteral(".history");
}

QStringList ProjectHistoryStore::snapshots(const QString& path) {
    const QDir dir(directoryFor(path));
    QStringList paths;
    for (const QFileInfo& file : dir.entryInfoList({QStringLiteral("snapshot-*.json")},
                                                   QDir::Files | QDir::NoSymLinks, QDir::Name | QDir::Reversed)) {
        paths.append(file.absoluteFilePath());
    }
    return paths;
}

bool ProjectHistoryStore::preserveBeforeSave(const QString& path, const QByteArray& replacement) {
    QMutexLocker lock(&historyWriteMutex);
    if (!QFileInfo::exists(path)) return true;
    QFile current(path);
    if (!current.open(QIODevice::ReadOnly)) return false;
    const QByteArray previous = current.readAll();
    if (current.error() != QFileDevice::NoError) return false;
    current.close();
    if (previous == replacement || !QJsonDocument::fromJson(previous).isObject()) return true;

    const QString hash = QString::fromLatin1(QCryptographicHash::hash(previous, QCryptographicHash::Sha256).toHex().left(12));
    const QStringList existing = snapshots(path);
    for (const QString& snapshot : existing) {
        if (snapshot.endsWith(QLatin1Char('-') + hash + QStringLiteral(".json"))) return true;
    }
    const QDir history(directoryFor(path));
    if (!QDir().mkpath(history.absolutePath())) return false;
    QDateTime timestamp = QDateTime::currentDateTimeUtc();
    if (!existing.isEmpty()) {
        QDateTime latest = QDateTime::fromString(QFileInfo(existing.first()).fileName().mid(9, 19), snapshotDateFormat);
        latest.setTimeSpec(Qt::UTC);
        if (latest.isValid() && latest >= timestamp) timestamp = latest.addMSecs(1);
    }
    const QString filename = QStringLiteral("snapshot-%1-%2.json").arg(timestamp.toString(snapshotDateFormat), hash);
    QSaveFile backup(history.filePath(filename));
    if (!backup.open(QIODevice::WriteOnly) || backup.write(previous) != previous.size() || !backup.commit()) return false;

    const QStringList all = snapshots(path);
    for (int i = kMaxSnapshots; i < all.size(); ++i) {
        QFile::remove(all.at(i));
    }
    return true;
}
