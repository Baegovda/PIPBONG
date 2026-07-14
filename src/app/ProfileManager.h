#pragma once

#include "app/ProgramSettings.h"

#include <QString>
#include <QStringList>

#include <memory>
#include <unordered_map>
#include <vector>

class QIcon;
class Project;

class ProfileManager {
public:
    struct Profile {
        QString id;
        QString name;
        QString targetWindowTitle;
    };

    explicit ProfileManager(const QString& dataDirectory);

    bool initialize();
    const std::vector<Profile>& profiles() const { return m_profiles; }
    QString activeProfileId() const { return m_activeProfileId; }
    QString defaultProfileId() const { return m_defaultProfileId; }
    const Profile* activeProfile() const;
    const Profile* profileById(const QString& id) const;

    QString profilesDirectory() const;
    QString profileDirectory(const QString& id) const;
    QString projectPath(const QString& id) const;
    QString projectDirectory(const QString& id) const;
    QString profilePackagePath(const QString& id) const;
    QString activeProjectPath() const { return projectPath(m_activeProfileId); }
    QString activeProjectDirectory() const { return projectDirectory(m_activeProfileId); }
    QString activeProfilePackagePath() const { return profilePackagePath(m_activeProfileId); }
    QString targetWindowTitle(const QString& id) const;
    QString linkedTargetProcessPath(const QString& id) const;
    /// Cached exe icon PNG under the profile folder (survives app uninstall).
    QIcon linkedTargetIcon(const QString& id) const;
    bool cacheLinkedTargetIcon(const QString& id, const QIcon& icon) const;
    void clearLinkedTargetIcon(const QString& id) const;
    bool setTargetWindowTitle(const QString& id, const QString& title);
    bool updateProfileTargetBinding(const QString& id,
                                    const QString& title,
                                    const QString& processPath);
    void setProfileTargetWindowTitleInMemory(const QString& id, const QString& title);

    bool setActiveProfile(const QString& id);
    bool setDefaultProfile(const QString& id);
    bool reorderProfiles(const QStringList& orderedIds);
    QString addProfile(const QString& name);
    bool renameProfile(const QString& id, const QString& name);
    bool removeProfile(const QString& id);
    bool isDefaultProfile(const QString& id) const { return id == m_defaultProfileId; }

    /// Ensures workspace folder exists; unpacks the local @c .pipbong when @c project.json is missing.
    bool ensureProfileWorkspace(const QString& id) const;
    bool sealProfilePackage(const QString& id) const;
    /// Imports a shared @c .pipbong as a new profile; returns new profile id or empty on failure.
    QString importProfileFromPackage(const QString& packagePath, const QString& preferredName = {});

    ProgramSettings::ProfileSettings loadSettings(const QString& id) const;
    /// When @p replaceLinkedProcessPath is false (default), an empty
    /// linkedTargetProcessPath in @p settings keeps any previously saved path
    /// (QSettings snapshots omit that field). Pass true to clear or overwrite.
    bool saveSettings(const QString& id,
                      const ProgramSettings::ProfileSettings& settings,
                      bool replaceLinkedProcessPath = false) const;

    /// Resolves the profile whose linked target-window title best matches the foreground title.
    /// Returns the default profile when nothing matches or the title is empty.
    QString profileIdForForegroundTitle(const QString& foregroundTitle) const;

    /// Returns a clone of a cached project when the on-disk file mtime is unchanged.
    std::unique_ptr<Project> cloneCachedProject(const QString& id,
                                                const QString& projectPath,
                                                QString* projectDirectoryOut = nullptr) const;
    void storeCachedProject(const QString& id,
                            const QString& projectPath,
                            std::unique_ptr<Project> project,
                            const QString& projectDirectory);
    void invalidateCachedProject(const QString& id);

private:
    struct CachedProjectEntry {
        std::unique_ptr<Project> project;
        QString projectDirectory;
        QString projectPath;
        qint64 fileMtime = 0;
    };

    qint64 projectFileMtime(const QString& projectPath) const;
    void touchProjectCacheLru(const QString& id) const;
    void evictProjectCacheIfNeeded() const;

    bool loadManifest();
    bool saveManifest() const;
    void ensureAtLeastOneProfile();
    void migrateLegacyProjectIfNeeded();
    void ensureDefaultProfileConstraints();
    void pinDefaultProfileFirst();
    QString loadProfileTargetWindowTitleFromProject(const QString& id) const;
    static QString sanitizedProfileName(const QString& name);
    static QString createProfileId();

    QString m_dataDirectory;
    std::vector<Profile> m_profiles;
    QString m_activeProfileId;
    QString m_defaultProfileId;

    mutable std::unordered_map<QString, ProgramSettings::ProfileSettings> m_settingsCache;
    mutable std::unordered_map<QString, qint64> m_settingsFileMtime;

    mutable std::unordered_map<QString, CachedProjectEntry> m_projectCache;
    mutable std::vector<QString> m_projectCacheLru;
    static constexpr int kMaxCachedProjects = 4;
};
