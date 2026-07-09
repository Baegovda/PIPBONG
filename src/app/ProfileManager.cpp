#include "app/ProfileManager.h"

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QUuid>

#include <algorithm>

namespace {

constexpr const char* kManifestFileName = "manifest.json";
constexpr const char* kProjectFileName = "project.json";
constexpr const char* kSettingsFileName = "profile-settings.json";

bool copyFileIfMissing(const QString& from, const QString& to) {
    if (!QFileInfo::exists(from) || QFileInfo::exists(to)) {
        return false;
    }
    QDir().mkpath(QFileInfo(to).absolutePath());
    return QFile::copy(from, to);
}

void copyDirectoryMissingFiles(const QString& from, const QString& to) {
    QDir source(from);
    if (!source.exists()) {
        return;
    }
    QDir().mkpath(to);
    const QFileInfoList entries =
        source.entryInfoList(QDir::Files | QDir::Dirs | QDir::NoDotAndDotDot);
    for (const QFileInfo& entry : entries) {
        const QString target = QDir(to).filePath(entry.fileName());
        if (entry.isDir()) {
            copyDirectoryMissingFiles(entry.absoluteFilePath(), target);
        } else {
            copyFileIfMissing(entry.absoluteFilePath(), target);
        }
    }
}

} // namespace

ProfileManager::ProfileManager(const QString& dataDirectory)
    : m_dataDirectory(dataDirectory) {}

bool ProfileManager::initialize() {
    QDir().mkpath(profilesDirectory());
    const bool loaded = loadManifest();
    ensureAtLeastOneProfile();
    migrateLegacyProjectIfNeeded();
    ensureDefaultProfileConstraints();
    if (profileById(m_defaultProfileId)) {
        m_activeProfileId = m_defaultProfileId;
    }
    saveManifest();
    return loaded || !m_profiles.empty();
}

const ProfileManager::Profile* ProfileManager::activeProfile() const {
    return profileById(m_activeProfileId);
}

const ProfileManager::Profile* ProfileManager::profileById(const QString& id) const {
    for (const Profile& profile : m_profiles) {
        if (profile.id == id) {
            return &profile;
        }
    }
    return nullptr;
}

QString ProfileManager::profilesDirectory() const {
    return QDir(m_dataDirectory).filePath(QStringLiteral("profiles"));
}

QString ProfileManager::profileDirectory(const QString& id) const {
    return QDir(profilesDirectory()).filePath(id);
}

QString ProfileManager::projectPath(const QString& id) const {
    return QDir(profileDirectory(id)).filePath(QString::fromLatin1(kProjectFileName));
}

QString ProfileManager::projectDirectory(const QString& id) const {
    return profileDirectory(id);
}

QString ProfileManager::targetWindowTitle(const QString& id) const {
    const Profile* profile = profileById(id);
    if (!profile || id == m_defaultProfileId) {
        return {};
    }
    return profile->targetWindowTitle;
}

QString ProfileManager::linkedTargetProcessPath(const QString& id) const {
    if (id.isEmpty() || id == m_defaultProfileId) {
        return {};
    }
    return loadSettings(id).linkedTargetProcessPath;
}

bool ProfileManager::setTargetWindowTitle(const QString& id, const QString& title) {
    Profile* profile = nullptr;
    for (Profile& candidate : m_profiles) {
        if (candidate.id == id) {
            profile = &candidate;
            break;
        }
    }
    if (!profile) {
        return false;
    }
    const QString effectiveTitle = id == m_defaultProfileId ? QString() : title;
    QDir().mkpath(profileDirectory(id));
    const QString path = projectPath(id);
    QJsonObject root;
    QFile file(path);
    if (file.open(QIODevice::ReadOnly)) {
        root = QJsonDocument::fromJson(file.readAll()).object();
        file.close();
    }
    root.insert(QStringLiteral("version"), root.value(QStringLiteral("version")).toInt(1));
    root.insert(QStringLiteral("targetWindowTitle"), effectiveTitle);
    if (!root.contains(QStringLiteral("projectDirectory"))) {
        root.insert(QStringLiteral("projectDirectory"), profileDirectory(id));
    }
    if (!root.contains(QStringLiteral("features")) || !root.value(QStringLiteral("features")).isArray()) {
        root.insert(QStringLiteral("features"), QJsonArray{});
    }
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        return false;
    }
    file.write(QJsonDocument(root).toJson(QJsonDocument::Indented));
    profile->targetWindowTitle = effectiveTitle;
    if (effectiveTitle.isEmpty() && id != m_defaultProfileId) {
        ProgramSettings::ProfileSettings settings = loadSettings(id);
        settings.linkedTargetProcessPath.clear();
        saveSettings(id, settings, true);
    }
    return true;
}

bool ProfileManager::updateProfileTargetBinding(const QString& id,
                                                const QString& title,
                                                const QString& processPath) {
    if (id.isEmpty() || id == m_defaultProfileId) {
        return false;
    }
    Profile* profile = nullptr;
    for (Profile& candidate : m_profiles) {
        if (candidate.id == id) {
            profile = &candidate;
            break;
        }
    }
    if (!profile) {
        return false;
    }
    profile->targetWindowTitle = title.trimmed();
    ProgramSettings::ProfileSettings settings = loadSettings(id);
    const QString trimmedPath = processPath.trimmed();
    if (!trimmedPath.isEmpty()) {
        settings.linkedTargetProcessPath = trimmedPath;
        return saveSettings(id, settings, true);
    }
    if (profile->targetWindowTitle.isEmpty()) {
        settings.linkedTargetProcessPath.clear();
        return saveSettings(id, settings, true);
    }
    // Title still set but path lookup failed — keep previously saved exe path.
    return saveSettings(id, settings, false);
}

void ProfileManager::setProfileTargetWindowTitleInMemory(const QString& id, const QString& title) {
    if (id.isEmpty() || id == m_defaultProfileId) {
        return;
    }
    for (Profile& profile : m_profiles) {
        if (profile.id == id) {
            profile.targetWindowTitle = title.trimmed();
            return;
        }
    }
}

bool ProfileManager::setActiveProfile(const QString& id) {
    if (!profileById(id)) {
        return false;
    }
    m_activeProfileId = id;
    return saveManifest();
}

bool ProfileManager::setDefaultProfile(const QString& id) {
    if (!profileById(id)) {
        return false;
    }
    const QString previousDefaultId = m_defaultProfileId;
    m_defaultProfileId = id;
    if (previousDefaultId != id) {
        for (Profile& profile : m_profiles) {
            if (profile.id == previousDefaultId && profile.name == QStringLiteral("기본")) {
                profile.name = QStringLiteral("프로필");
            }
            if (profile.id == id) {
                profile.name = QStringLiteral("기본");
            }
        }
    }
    if (!saveManifest()) {
        return false;
    }
    pinDefaultProfileFirst();
    ensureDefaultProfileConstraints();
    return true;
}

bool ProfileManager::reorderProfiles(const QStringList& orderedIds) {
    if (orderedIds.size() != static_cast<int>(m_profiles.size())) {
        return false;
    }

    std::vector<Profile> reordered;
    reordered.reserve(m_profiles.size());
    for (const QString& id : orderedIds) {
        const auto it = std::find_if(m_profiles.begin(),
                                     m_profiles.end(),
                                     [&id](const Profile& profile) { return profile.id == id; });
        if (it == m_profiles.end()) {
            return false;
        }
        reordered.push_back(*it);
    }
    m_profiles = std::move(reordered);
    pinDefaultProfileFirst();
    return saveManifest();
}

QString ProfileManager::addProfile(const QString& name) {
    Profile profile;
    profile.id = createProfileId();
    profile.name = sanitizedProfileName(name);
    QDir().mkpath(profileDirectory(profile.id));
    saveSettings(profile.id, ProgramSettings::profileSettings());
    m_profiles.push_back(profile);
    m_activeProfileId = profile.id;
    saveManifest();
    return profile.id;
}

bool ProfileManager::renameProfile(const QString& id, const QString& name) {
    if (id == m_defaultProfileId) {
        for (Profile& profile : m_profiles) {
            if (profile.id == id) {
                profile.name = QStringLiteral("기본");
                return saveManifest();
            }
        }
        return false;
    }
    for (Profile& profile : m_profiles) {
        if (profile.id != id) {
            continue;
        }
        profile.name = sanitizedProfileName(name);
        return saveManifest();
    }
    return false;
}

bool ProfileManager::removeProfile(const QString& id) {
    if (id == m_defaultProfileId) {
        return false;
    }
    if (m_profiles.size() <= 1) {
        return false;
    }
    const auto oldSize = m_profiles.size();
    m_profiles.erase(
        std::remove_if(m_profiles.begin(),
                       m_profiles.end(),
                       [&id](const Profile& profile) { return profile.id == id; }),
        m_profiles.end());
    if (m_profiles.size() == oldSize) {
        return false;
    }
    QDir(profileDirectory(id)).removeRecursively();
    if (m_activeProfileId == id) {
        m_activeProfileId = m_profiles.front().id;
    }
    if (m_defaultProfileId == id) {
        m_defaultProfileId = m_profiles.front().id;
    }
    if (!saveManifest()) {
        return false;
    }
    ensureDefaultProfileConstraints();
    return true;
}

ProgramSettings::ProfileSettings ProfileManager::loadSettings(const QString& id) const {
    ProgramSettings::ProfileSettings settings;
    QFile file(QDir(profileDirectory(id)).filePath(QString::fromLatin1(kSettingsFileName)));
    if (!file.open(QIODevice::ReadOnly)) {
        return settings;
    }
    const QJsonObject root = QJsonDocument::fromJson(file.readAll()).object();
    settings.autoSelectRunningFeature =
        root.value(QStringLiteral("autoSelectRunningFeature")).toBool(settings.autoSelectRunningFeature);
    settings.pinTargetWindowToScreenCenter =
        root.value(QStringLiteral("pinTargetWindowToScreenCenter")).toBool(settings.pinTargetWindowToScreenCenter);
    settings.imageFindCaptureMode =
        root.value(QStringLiteral("imageFindCaptureMode")).toInt(
            static_cast<int>(settings.imageFindCaptureMode))
            == static_cast<int>(ProgramSettings::ImageFindCaptureMode::ClientOnly)
            ? ProgramSettings::ImageFindCaptureMode::ClientOnly
            : ProgramSettings::ImageFindCaptureMode::Hybrid;
    settings.runWithoutTargetWindow =
        root.value(QStringLiteral("runWithoutTargetWindow")).toBool(settings.runWithoutTargetWindow);
    settings.linkedTargetProcessPath =
        root.value(QStringLiteral("linkedTargetProcessPath")).toString(settings.linkedTargetProcessPath);
    return settings;
}

bool ProfileManager::saveSettings(const QString& id,
                                  const ProgramSettings::ProfileSettings& settings,
                                  bool replaceLinkedProcessPath) const {
    QDir().mkpath(profileDirectory(id));
    ProgramSettings::ProfileSettings toWrite = settings;
    // Callers that snapshot QSettings-backed options often omit the exe path.
    // Preserve a previously saved linkedTargetProcessPath unless the caller
    // explicitly replaces/clears it (window pick, clear target title).
    if (!replaceLinkedProcessPath && toWrite.linkedTargetProcessPath.isEmpty()) {
        toWrite.linkedTargetProcessPath = loadSettings(id).linkedTargetProcessPath;
    }
    QJsonObject root;
    root.insert(QStringLiteral("version"), 1);
    root.insert(QStringLiteral("autoSelectRunningFeature"), toWrite.autoSelectRunningFeature);
    root.insert(QStringLiteral("pinTargetWindowToScreenCenter"), toWrite.pinTargetWindowToScreenCenter);
    root.insert(QStringLiteral("imageFindCaptureMode"), static_cast<int>(toWrite.imageFindCaptureMode));
    root.insert(QStringLiteral("runWithoutTargetWindow"), toWrite.runWithoutTargetWindow);
    if (!toWrite.linkedTargetProcessPath.isEmpty()) {
        root.insert(QStringLiteral("linkedTargetProcessPath"), toWrite.linkedTargetProcessPath);
    }
    QFile file(QDir(profileDirectory(id)).filePath(QString::fromLatin1(kSettingsFileName)));
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        return false;
    }
    file.write(QJsonDocument(root).toJson(QJsonDocument::Indented));
    return true;
}

QString ProfileManager::profileIdForForegroundTitle(const QString& foregroundTitle) const {
    const QString trimmed = foregroundTitle.trimmed();
    if (trimmed.isEmpty()) {
        return m_defaultProfileId;
    }

    QString bestId;
    int bestLength = 0;
    for (const Profile& profile : m_profiles) {
        if (profile.id == m_defaultProfileId) {
            continue;
        }
        const QString binding = profile.targetWindowTitle;
        if (binding.isEmpty()) {
            continue;
        }
        if (trimmed.contains(binding, Qt::CaseInsensitive) && binding.length() > bestLength) {
            bestLength = binding.length();
            bestId = profile.id;
        }
    }
    return bestId.isEmpty() ? m_defaultProfileId : bestId;
}

bool ProfileManager::loadManifest() {
    QFile file(QDir(profilesDirectory()).filePath(QString::fromLatin1(kManifestFileName)));
    if (!file.open(QIODevice::ReadOnly)) {
        return false;
    }
    const QJsonObject root = QJsonDocument::fromJson(file.readAll()).object();
    m_activeProfileId = root.value(QStringLiteral("activeProfileId")).toString();
    m_defaultProfileId = root.value(QStringLiteral("defaultProfileId")).toString();
    m_profiles.clear();
    const QJsonArray profiles = root.value(QStringLiteral("profiles")).toArray();
    for (const QJsonValue& value : profiles) {
        const QJsonObject obj = value.toObject();
        Profile profile;
        profile.id = obj.value(QStringLiteral("id")).toString();
        profile.name = sanitizedProfileName(obj.value(QStringLiteral("name")).toString());
        profile.targetWindowTitle = loadProfileTargetWindowTitleFromProject(profile.id);
        if (!profile.id.isEmpty()) {
            m_profiles.push_back(profile);
        }
    }
    if (!profileById(m_activeProfileId) && !m_profiles.empty()) {
        m_activeProfileId = m_profiles.front().id;
    }
    if (!profileById(m_defaultProfileId) && !m_profiles.empty()) {
        m_defaultProfileId = m_profiles.front().id;
    }
    pinDefaultProfileFirst();
    ensureDefaultProfileConstraints();
    return !m_profiles.empty();
}

bool ProfileManager::saveManifest() const {
    QDir().mkpath(profilesDirectory());
    QJsonObject root;
    root.insert(QStringLiteral("version"), 1);
    root.insert(QStringLiteral("activeProfileId"), m_activeProfileId);
    root.insert(QStringLiteral("defaultProfileId"), m_defaultProfileId);
    QJsonArray profiles;
    for (const Profile& profile : m_profiles) {
        QJsonObject obj;
        obj.insert(QStringLiteral("id"), profile.id);
        obj.insert(QStringLiteral("name"), profile.name);
        profiles.push_back(obj);
    }
    root.insert(QStringLiteral("profiles"), profiles);

    QFile file(QDir(profilesDirectory()).filePath(QString::fromLatin1(kManifestFileName)));
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        return false;
    }
    file.write(QJsonDocument(root).toJson(QJsonDocument::Indented));
    return true;
}

void ProfileManager::ensureAtLeastOneProfile() {
    if (!m_profiles.empty()) {
        return;
    }
    Profile profile;
    profile.id = QStringLiteral("default");
    profile.name = QStringLiteral("기본");
    QDir().mkpath(profileDirectory(profile.id));
    m_profiles.push_back(profile);
    m_activeProfileId = profile.id;
    m_defaultProfileId = profile.id;
    ProgramSettings::ProfileSettings settings;
    settings.runWithoutTargetWindow = true;
    saveSettings(profile.id, settings);
    setTargetWindowTitle(profile.id, QString());
}

void ProfileManager::migrateLegacyProjectIfNeeded() {
    const QString legacyProject = QDir(m_dataDirectory).filePath(QString::fromLatin1(kProjectFileName));
    if (!QFileInfo::exists(legacyProject)) {
        return;
    }
    const QString profileProject = projectPath(m_activeProfileId);
    copyFileIfMissing(legacyProject, profileProject);
    copyDirectoryMissingFiles(QDir(m_dataDirectory).filePath(QStringLiteral("templates")),
                              QDir(profileDirectory(m_activeProfileId)).filePath(QStringLiteral("templates")));
}

QString ProfileManager::sanitizedProfileName(const QString& name) {
    const QString trimmed = name.trimmed();
    return trimmed.isEmpty() ? QStringLiteral("새 프로필") : trimmed;
}

QString ProfileManager::createProfileId() {
    return QUuid::createUuid().toString(QUuid::WithoutBraces);
}

void ProfileManager::ensureDefaultProfileConstraints() {
    if (!profileById(m_defaultProfileId)) {
        return;
    }
    for (Profile& profile : m_profiles) {
        if (profile.id == m_defaultProfileId) {
            profile.name = QStringLiteral("기본");
            break;
        }
    }
    setTargetWindowTitle(m_defaultProfileId, QString());
    ProgramSettings::ProfileSettings settings = loadSettings(m_defaultProfileId);
    if (!settings.runWithoutTargetWindow) {
        settings.runWithoutTargetWindow = true;
        saveSettings(m_defaultProfileId, settings);
    }
}

void ProfileManager::pinDefaultProfileFirst() {
    if (!profileById(m_defaultProfileId)) {
        return;
    }
    const auto it = std::find_if(m_profiles.begin(),
                                 m_profiles.end(),
                                 [this](const Profile& profile) {
                                     return profile.id == m_defaultProfileId;
                                 });
    if (it == m_profiles.end() || it == m_profiles.begin()) {
        return;
    }
    Profile pinned = *it;
    m_profiles.erase(it);
    m_profiles.insert(m_profiles.begin(), pinned);
}

QString ProfileManager::loadProfileTargetWindowTitleFromProject(const QString& id) const {
    if (id.isEmpty() || id == m_defaultProfileId) {
        return {};
    }
    QFile file(projectPath(id));
    if (!file.open(QIODevice::ReadOnly)) {
        return {};
    }
    return QJsonDocument::fromJson(file.readAll()).object().value(QStringLiteral("targetWindowTitle")).toString();
}
