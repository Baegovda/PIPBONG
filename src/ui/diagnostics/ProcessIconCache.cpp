#include "ui/diagnostics/ProcessIconCache.h"

#include <QFileIconProvider>
#include <QFileInfo>

ProcessIconCache::ProcessIconCache()
    : m_provider(new QFileIconProvider) {}

ProcessIconCache::~ProcessIconCache() {
    delete m_provider;
}

QIcon ProcessIconCache::iconForExecutablePath(const QString& executablePath) {
    if (executablePath.isEmpty()) {
        return {};
    }

    const auto cached = m_pathCache.constFind(executablePath);
    if (cached != m_pathCache.constEnd()) {
        return cached.value();
    }

    QIcon icon;
    if (QFileInfo::exists(executablePath)) {
        icon = m_provider->icon(QFileInfo(executablePath));
    }
    m_pathCache.insert(executablePath, icon);
    return icon;
}

QIcon ProcessIconCache::iconForProcessName(const QString& processName) {
    if (processName.isEmpty()) {
        return {};
    }

    const auto cached = m_nameCache.constFind(processName);
    if (cached != m_nameCache.constEnd()) {
        return cached.value();
    }

    const QIcon icon = m_provider->icon(QFileIconProvider::File);
    m_nameCache.insert(processName, icon);
    return icon;
}
