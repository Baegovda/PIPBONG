#pragma once

#include <QHash>
#include <QIcon>
#include <QString>

class QFileIconProvider;

class ProcessIconCache {
public:
    ProcessIconCache();
    ~ProcessIconCache();

    QIcon iconForExecutablePath(const QString& executablePath);
    QIcon iconForProcessName(const QString& processName);

private:
    QHash<QString, QIcon> m_pathCache;
    QHash<QString, QIcon> m_nameCache;
    QFileIconProvider* m_provider = nullptr;
};
