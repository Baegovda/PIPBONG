#pragma once

#include "app/ProgramSettings.h"

#include <QString>
#include <QStringList>
#include <vector>

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
    QString activeProjectPath() const { return projectPath(m_activeProfileId); }
    QString activeProjectDirectory() const { return projectDirectory(m_activeProfileId); }
    QString targetWindowTitle(const QString& id) const;
    QString linkedTargetProcessPath(const QString& id) const;
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

private:
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
};
