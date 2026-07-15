#pragma once

#include <QString>

/// Per-profile plain-text memo persisted as @c memo.txt in the profile workspace folder.
class ProfileMemoStore {
public:
    static QString memoFilePath(const QString& profileDirectory);
    static bool load(const QString& profileDirectory, QString* outText);
    static bool save(const QString& profileDirectory, const QString& text);
};
