#pragma once
#include <QByteArray>
#include <QString>
#include <QStringList>

class ProjectHistoryStore {
public:
    static constexpr int kMaxSnapshots = 8;
    static QString directoryFor(const QString& projectPath);
    static QStringList snapshots(const QString& projectPath);
    static bool preserveBeforeSave(const QString& projectPath, const QByteArray& replacement);
};
