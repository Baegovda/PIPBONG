#include "app/ProfileManager.h"

#include "model/Project.h"
#include "storage/ProjectPackage.h"
#include "storage/ProfileRemovalTransaction.h"
#include "storage/ProfileRemovalRecovery.h"
#include "storage/ProfileMetadataTransaction.h"

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileIconProvider>
#include <QFileInfo>
#include <QSaveFile>
#include <QTemporaryDir>
#include <QSet>
#include <QIcon>
#include <QJsonArray>
#include <QPixmap>
#include <QJsonDocument>
#include <QJsonObject>
#include <QUuid>

#include <algorithm>

namespace {

constexpr const char* kManifestFileName = "manifest.json";
constexpr const char* kProjectFileName = "project.json";
constexpr const char* kSettingsFileName = "profile-settings.json";
constexpr const char* kLinkedTargetIconFileName = "linked-target-icon.png";
constexpr const char* kProfilePackageSuffix = ".pipbong";
constexpr int kLinkedTargetIconCachePx = 32;

QIcon iconFromExecutablePath(const QString& processPath) {
    if (processPath.isEmpty() || !QFileInfo::exists(processPath)) {
        return {};
    }
    static QFileIconProvider iconProvider;
    const QIcon icon = iconProvider.icon(QFileInfo(processPath));
    return icon.isNull() ? QIcon{} : icon;
}

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
    m_storageReady=false;
    const auto metadata=recoverProfileMetadata(profilesDirectory());
    m_startupWarnings.clear();
    if(!metadata.message.isEmpty())m_startupWarnings<<metadata.message;
    if(!metadata.success)return false;
    const auto recovery=recoverProfileRemovals(profilesDirectory());
    m_startupWarnings+=recovery.warnings;
    if(!recovery.complete)return false;
    m_storageReady=true;
    const bool loaded = loadManifest();
    ensureAtLeastOneProfile();
    migrateLegacyProjectIfNeeded();
    ensureDefaultProfileConstraints();
    if (profileById(m_defaultProfileId)) {
        m_activeProfileId = m_defaultProfileId;
    }
    for (const Profile& profile : m_profiles) {
        ensureProfileWorkspace(profile.id);
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

QString ProfileManager::profilePackagePath(const QString& id) const {
    if (id.isEmpty()) {
        return {};
    }
    return QDir(profilesDirectory()).filePath(id + QString::fromLatin1(kProfilePackageSuffix));
}

bool ProfileManager::ensureProfileWorkspace(const QString& id) const {
    if(!m_storageReady)return false;
    if (id.isEmpty()) {
        return false;
    }
    const QString directory = profileDirectory(id);
    const QString projectFile = projectPath(id);
    const QString packageFile = profilePackagePath(id);

    if (QFileInfo::exists(projectFile)) {
        QDir().mkpath(directory);
        return true;
    }

    if (QFileInfo::exists(packageFile)) {
        if(!profileById(id) || !recoverProfilePackage(profilesDirectory(),id,packageFile))return false;
        m_settingsCache.erase(id);m_settingsFileMtime.erase(id);m_projectCache.erase(id);
        m_projectCacheLru.erase(std::remove(m_projectCacheLru.begin(),m_projectCacheLru.end(),id),m_projectCacheLru.end());
        return true;
    }

    QDir().mkpath(directory);
    return true;
}

bool ProfileManager::sealProfilePackage(const QString& id) const {
    if(!m_storageReady)return false;
    if (id.isEmpty() || !QFileInfo::exists(projectPath(id))) {
        return false;
    }
    return ProjectPackage::packDirectory(profileDirectory(id), profilePackagePath(id));
}

ProfileImportRequest ProfileManager::importRequest(const QString& path,const QString& preferred)const {
    return {profilesDirectory(),createProfileId(),sanitizedProfileName(preferred.trimmed().isEmpty()?QFileInfo(path).completeBaseName():preferred),path,false};
}
QString ProfileManager::commitImportedProfile(const std::shared_ptr<PreparedProfileImport>& p,QString* error) {
    const auto fail=[&](const QString& message){if(error)*error=message;return QString();};
    if(error)error->clear();
    if(!m_storageReady || !p || !p->ready || !p->temporary || !p->temporary->isValid()
        || QUuid(p->id).isNull() || profileById(p->id)
        || QDir::cleanPath(QFileInfo(p->root).absoluteFilePath())!=QDir::cleanPath(QFileInfo(profilesDirectory()).absoluteFilePath())
        || p->directory!=profileDirectory(p->id) || p->package!=profilePackagePath(p->id)
        || p->workspace!=p->temporary->filePath("workspace") || p->archive!=p->temporary->filePath("prepared.pipbong")
        || QFileInfo::exists(p->directory) || QFileInfo::exists(p->package))return fail(QStringLiteral("프로필 상태나 가져오기 위치가 바뀌었어요. 다시 선택해 주세요."));
    bool ownDirectory=false,ownPackage=false;
    const auto resolvedRoot=QFileInfo(profilesDirectory()).canonicalFilePath();
    const auto cleanup=[&] {
        // Only freshly moved paths belonging to this generated profile can be removed.
        const auto root=QDir::cleanPath(QFileInfo(profilesDirectory()).absoluteFilePath());
        const auto child=QDir::cleanPath(QFileInfo(p->directory).absoluteFilePath());
        const auto resolvedChild=QFileInfo(child).canonicalFilePath();
        if(ownDirectory && !resolvedRoot.isEmpty() && QFileInfo(root).canonicalFilePath()==resolvedRoot
            && resolvedChild.startsWith(resolvedRoot+QLatin1Char('/'),Qt::CaseInsensitive)
            && child.startsWith(root+QLatin1Char('/'),Qt::CaseInsensitive)
            && !QFileInfo(child).isSymLink() && !QFileInfo(child).isJunction())QDir(child).removeRecursively();
        if(ownPackage && QFileInfo(p->package).absolutePath()==root && !QFileInfo(p->package).isSymLink())QFile::remove(p->package);
        m_settingsCache.erase(p->id);m_settingsFileMtime.erase(p->id);m_projectCache.erase(p->id);
    };
    if(!QDir().rename(p->workspace,p->directory))return fail(QStringLiteral("프로필 파일을 설치하지 못했어요. 기존 프로필을 유지했어요."));
    ownDirectory=true;
    if(!QFile::rename(p->archive,p->package)){cleanup();return fail(QStringLiteral("복구 패키지를 저장하지 못했어요. 기존 프로필을 유지했어요."));}
    ownPackage=true;
    Profile profile;profile.id=p->id;profile.name=sanitizedProfileName(p->name);profile.targetWindowTitle=p->projectTitle;
    profile.subTargetWindowTitle=loadSettings(p->id).subTargetWindowTitle;
    const auto previous=m_activeProfileId;m_profiles.push_back(profile);m_activeProfileId=profile.id;
    if(!saveManifest()){m_profiles.pop_back();m_activeProfileId=previous;cleanup();return fail(QStringLiteral("프로필 목록을 저장하지 못했어요. 기존 프로필을 유지했어요."));}
    return profile.id;
}
QString ProfileManager::importProfileFromPackage(const QString& path,const QString& preferred) {
    if(!m_storageReady)return {};
    return commitImportedProfile(prepareProfileImport(importRequest(path,preferred)));
}

QString ProfileManager::targetWindowTitle(const QString& id) const {
    const Profile* profile = profileById(id);
    if (!profile || id == m_defaultProfileId) {
        return {};
    }
    return profile->targetWindowTitle;
}

QString ProfileManager::subTargetWindowTitle(const QString& id) const {
    const Profile* profile = profileById(id);
    if (!profile || id == m_defaultProfileId) {
        return {};
    }
    return profile->subTargetWindowTitle;
}

QString ProfileManager::linkedTargetProcessPath(const QString& id) const {
    if (id.isEmpty() || id == m_defaultProfileId) {
        return {};
    }
    return loadSettings(id).linkedTargetProcessPath;
}

QString ProfileManager::subLinkedTargetProcessPath(const QString& id) const {
    if (id.isEmpty() || id == m_defaultProfileId) {
        return {};
    }
    return loadSettings(id).subLinkedTargetProcessPath;
}

QIcon ProfileManager::linkedTargetIcon(const QString& id) const {
    if (id.isEmpty() || id == m_defaultProfileId) {
        return {};
    }
    const QString path = QDir(profileDirectory(id)).filePath(QString::fromLatin1(kLinkedTargetIconFileName));
    if (!QFileInfo::exists(path)) {
        return {};
    }
    const QPixmap pixmap(path);
    return pixmap.isNull() ? QIcon{} : QIcon(pixmap);
}

bool ProfileManager::cacheLinkedTargetIcon(const QString& id, const QIcon& icon) const {
    if (id.isEmpty() || id == m_defaultProfileId || icon.isNull()) {
        return false;
    }
    QDir().mkpath(profileDirectory(id));
    const QString path = QDir(profileDirectory(id)).filePath(QString::fromLatin1(kLinkedTargetIconFileName));
    const QPixmap pixmap = icon.pixmap(kLinkedTargetIconCachePx, kLinkedTargetIconCachePx);
    if (pixmap.isNull()) {
        return false;
    }
    return pixmap.save(path, "PNG");
}

void ProfileManager::clearLinkedTargetIcon(const QString& id) const {
    if (id.isEmpty()) {
        return;
    }
    QFile::remove(QDir(profileDirectory(id)).filePath(QString::fromLatin1(kLinkedTargetIconFileName)));
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
        clearLinkedTargetIcon(id);
    }
    return true;
}

bool ProfileManager::updateProfileTargetBinding(const QString& id,const QString& title,
                                                const QString& processPath,bool replaceProcessPath) {
    if(id.isEmpty() || id==m_defaultProfileId) return false;
    auto found=std::find_if(m_profiles.begin(),m_profiles.end(),[&](const Profile& p){return p.id==id;});
    if(found==m_profiles.end()) return false;
    const QString trimmed=title.trimmed(),path=processPath.trimmed();
    auto settings=loadSettings(id);
    const bool replace=replaceProcessPath || !path.isEmpty() || trimmed.isEmpty();
    if(replace) settings.linkedTargetProcessPath=trimmed.isEmpty()?QString():path;
    if(!saveSettings(id,settings,replace)) return false;
    found->targetWindowTitle=trimmed;
    if(replace) {
        if(settings.linkedTargetProcessPath.isEmpty()) clearLinkedTargetIcon(id);
        else {
            const QIcon icon=iconFromExecutablePath(settings.linkedTargetProcessPath);
            if(!icon.isNull()) cacheLinkedTargetIcon(id,icon);
        }
    }
    return true;
}

bool ProfileManager::setSubTargetWindowTitle(const QString& id, const QString& title) {
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
    const QString trimmed = title.trimmed();
    profile->subTargetWindowTitle = trimmed;
    ProgramSettings::ProfileSettings settings = loadSettings(id);
    settings.subTargetWindowTitle = trimmed;
    if (trimmed.isEmpty()) {
        settings.subLinkedTargetProcessPath.clear();
        return saveSettings(id, settings, false, true);
    }
    return saveSettings(id, settings, false, false);
}

bool ProfileManager::updateProfileSubTargetBinding(const QString& id,const QString& title,
                                                   const QString& processPath,bool replaceProcessPath) {
    if(id.isEmpty() || id==m_defaultProfileId) return false;
    auto found=std::find_if(m_profiles.begin(),m_profiles.end(),[&](const Profile& p){return p.id==id;});
    if(found==m_profiles.end()) return false;
    const QString trimmed=title.trimmed(),path=processPath.trimmed();
    auto settings=loadSettings(id);settings.subTargetWindowTitle=trimmed;
    const bool replace=replaceProcessPath || !path.isEmpty() || trimmed.isEmpty();
    if(replace) settings.subLinkedTargetProcessPath=trimmed.isEmpty()?QString():path;
    if(!saveSettings(id,settings,false,replace)) return false;
    found->subTargetWindowTitle=trimmed;
    return true;
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

void ProfileManager::setProfileSubTargetWindowTitleInMemory(const QString& id, const QString& title) {
    if (id.isEmpty() || id == m_defaultProfileId) {
        return;
    }
    for (Profile& profile : m_profiles) {
        if (profile.id == id) {
            profile.subTargetWindowTitle = title.trimmed();
            return;
        }
    }
}

bool ProfileManager::setActiveProfile(const QString& id) {
    if (!profileById(id)) {
        return false;
    }
    const auto previous = m_activeProfileId;
    m_activeProfileId = id;
    if (saveManifest()) return true;
    m_activeProfileId = previous;
    return false;
}

bool ProfileManager::setDefaultProfile(const QString& id) {
    const auto* profile=profileById(id);
    if(!m_storageReady || !profile)return false;
    if(id==m_defaultProfileId)return true;
    Edit edit;edit.name=profile->name;edit.makeDefault=true;
    return applyEdit(id,edit);
}

bool ProfileManager::reorderProfiles(const QStringList& orderedIds) {
    if (!m_storageReady || orderedIds.size() != static_cast<int>(m_profiles.size())) {
        return false;
    }

    std::vector<Profile> reordered;
    reordered.reserve(m_profiles.size());
    QSet<QString> seen;
    for (const QString& id : orderedIds) {
        if(seen.contains(id))return false;
        seen.insert(id);
        const auto it = std::find_if(m_profiles.begin(),
                                     m_profiles.end(),
                                     [&id](const Profile& profile) { return profile.id == id; });
        if (it == m_profiles.end()) {
            return false;
        }
        reordered.push_back(*it);
    }
    const auto previous=m_profiles;
    m_profiles = std::move(reordered);
    pinDefaultProfileFirst();
    if(saveManifest())return true;
    m_profiles=previous;return false;
}

QString ProfileManager::addProfile(const QString& name, bool activate) {
    if(!m_storageReady)return {};
    Profile profile;
    profile.id = createProfileId();
    profile.name = sanitizedProfileName(name);
    const auto directory = profileDirectory(profile.id);
    const auto cleanup = [&] {
        m_settingsCache.erase(profile.id);m_settingsFileMtime.erase(profile.id);
        const auto child=QDir::cleanPath(QFileInfo(directory).absoluteFilePath());
        const auto root=QDir::cleanPath(QFileInfo(profilesDirectory()).absoluteFilePath());
        if(child.startsWith(root+QLatin1Char('/'),Qt::CaseInsensitive)
            && !QFileInfo(child).isSymLink() && !QFileInfo(child).isJunction()) QDir(child).removeRecursively();
    };
    if (!QDir().mkpath(directory)) return {};
    auto settings=ProgramSettings::profileSettings();
    settings.linkedTargetProcessPath.clear();settings.subTargetWindowTitle.clear();
    settings.subLinkedTargetProcessPath.clear();settings.triggerArmedFeatureIds.clear();
    if (!saveSettings(profile.id,settings)) { cleanup();return {}; }
    const auto previous=m_activeProfileId;
    m_profiles.push_back(profile);
    if(activate) m_activeProfileId=profile.id;
    if(saveManifest()) return profile.id;
    m_profiles.pop_back();m_activeProfileId=previous;cleanup();
    return {};
}

bool ProfileManager::renameProfile(const QString& id, const QString& name) {
    if(!m_storageReady)return false;
    for (auto& profile : m_profiles) {
        if (profile.id != id) continue;
        const auto previous = profile.name;
        profile.name = id == m_defaultProfileId ? QStringLiteral("기본") : sanitizedProfileName(name);
        if (saveManifest()) return true;
        profile.name = previous;
        return false;
    }
    return false;
}

bool ProfileManager::applyEdit(const QString& id,const Edit& edit,QString* error) {
    if(error)error->clear();
    const auto fail=[&](const QString& message){if(error)*error=message;return false;};
    if(!m_storageReady || !profileById(id) || isDefaultProfile(id) || edit.name.trimmed().isEmpty())
        return fail(QObject::tr("프로필이 바뀌었거나 편집할 수 없는 상태예요."));
    const auto object=[](const QString& path,QJsonObject& value,bool optional){
        QFile file(path);if(!file.exists())return optional;
        if(!file.open(QIODevice::ReadOnly))return false;
        const auto json=QJsonDocument::fromJson(file.readAll());if(!json.isObject())return false;
        value=json.object();return true;
    };
    QJsonObject manifest,project,settings;
    const QString manifestPath=QDir(profilesDirectory()).filePath(kManifestFileName);
    if(!object(manifestPath,manifest,false) || !object(projectPath(id),project,true)
        || !object(QDir(profileDirectory(id)).filePath(kSettingsFileName),settings,false))
        return fail(QObject::tr("프로필 저장 파일을 읽지 못했어요. 기존 파일을 유지해요."));
    const auto persisted=manifest["profiles"].toArray();
    if(persisted.size()!=static_cast<int>(m_profiles.size()) || manifest["defaultProfileId"].toString()!=m_defaultProfileId
        || manifest["activeProfileId"].toString()!=m_activeProfileId)
        return fail(QObject::tr("저장된 프로필 목록이 바뀌었어요. 다시 불러온 뒤 편집해 주세요."));
    for(int i=0;i<persisted.size();++i)if(persisted[i].toObject()["id"].toString()!=m_profiles[i].id
        || persisted[i].toObject()["name"].toString()!=m_profiles[i].name)
        return fail(QObject::tr("저장된 프로필 목록이 바뀌었어요. 다시 불러온 뒤 편집해 주세요."));
    if(!project.isEmpty() && !project["features"].isArray())return fail(QObject::tr("프로젝트 파일이 손상되어 편집을 중단했어요."));
    auto profiles=m_profiles;
    const QString main=edit.makeDefault?QString():edit.mainTitle.trimmed(), sub=edit.makeDefault?QString():edit.subTitle.trimmed();
    for(auto& profile:profiles) {
        if(profile.id==id){profile.name=edit.makeDefault?QStringLiteral("기본"):edit.name.trimmed();profile.targetWindowTitle=main;profile.subTargetWindowTitle=sub;}
        else if(edit.makeDefault && profile.id==m_defaultProfileId && profile.name==QStringLiteral("기본"))profile.name=QStringLiteral("프로필");
    }
    if(edit.makeDefault) {
        const auto found=std::find_if(profiles.begin(),profiles.end(),[&](const Profile& profile){return profile.id==id;});
        std::rotate(profiles.begin(),found,found+1);manifest["defaultProfileId"]=id;settings["runWithoutTargetWindow"]=true;
    }
    QJsonArray rows;for(const auto& profile:profiles)rows.push_back(QJsonObject{{"id",profile.id},{"name",profile.name}});
    manifest["profiles"]=rows;
    if(project.isEmpty()){project["version"]=1;project["projectDirectory"]=profileDirectory(id);project["features"]=QJsonArray{};}
    project["targetWindowTitle"]=main;
    settings["linkedTargetProcessPath"]=main.isEmpty()?QString():edit.mainProcessPath.trimmed();
    settings["subTargetWindowTitle"]=sub;settings["subLinkedTargetProcessPath"]=sub.isEmpty()?QString():edit.subProcessPath.trimmed();
    const auto committed=writeProfileMetadata(profilesDirectory(),{{projectPath(id),QJsonDocument(project).toJson()},
        {QDir(profileDirectory(id)).filePath(kSettingsFileName),QJsonDocument(settings).toJson()},
        {manifestPath,QJsonDocument(manifest).toJson()}});
    if(!committed.success) {
        if(committed.recoveryRequired)m_storageReady=false;
        return fail(committed.message.isEmpty()?QObject::tr("프로필을 저장하지 못했어요."):committed.message);
    }
    m_profiles=std::move(profiles);if(edit.makeDefault)m_defaultProfileId=id;
    m_settingsCache.erase(id);m_settingsFileMtime.erase(id);invalidateCachedProject(id);clearLinkedTargetIcon(id);
    if(error)*error=committed.message;
    return true;
}

bool ProfileManager::removeProfile(const QString& id, QString* error) {
    if(!m_storageReady)return false;
    if (error) error->clear();
    if (id == m_defaultProfileId || m_profiles.size() <= 1 || !profileById(id)) return false;
    const auto previous = m_profiles;
    const auto previousActive = m_activeProfileId;
    const bool removed = removeProfileFilesTransaction(profilesDirectory(), profileDirectory(id), profilePackagePath(id), [&] {
        m_profiles.erase(std::remove_if(m_profiles.begin(), m_profiles.end(),
            [&id](const Profile& profile) { return profile.id == id; }), m_profiles.end());
        if (m_activeProfileId == id) m_activeProfileId = m_profiles.front().id;
        if (saveManifest()) return true;
        m_profiles = previous; m_activeProfileId = previousActive;
        return false;
    }, error);
    if (!removed) return false;
    m_settingsCache.erase(id); m_settingsFileMtime.erase(id);
    invalidateCachedProject(id);
    return true;
}

ProgramSettings::ProfileSettings ProfileManager::loadSettings(const QString& id) const {
    const QString settingsPath = QDir(profileDirectory(id)).filePath(QString::fromLatin1(kSettingsFileName));
    const QFileInfo fileInfo(settingsPath);
    const qint64 mtime = fileInfo.exists() ? fileInfo.lastModified().toMSecsSinceEpoch() : 0;
    const auto cachedIt = m_settingsCache.find(id);
    const auto mtimeIt = m_settingsFileMtime.find(id);
    if (cachedIt != m_settingsCache.end() && mtimeIt != m_settingsFileMtime.end()
        && mtimeIt->second == mtime) {
        return cachedIt->second;
    }

    ProgramSettings::ProfileSettings settings;
    QFile file(settingsPath);
    if (!file.open(QIODevice::ReadOnly)) {
        m_settingsCache[id] = settings;
        m_settingsFileMtime[id] = mtime;
        return settings;
    }
    const QJsonObject root = QJsonDocument::fromJson(file.readAll()).object();
    settings.autoSelectRunningFeature =
        root.value(QStringLiteral("autoSelectRunningFeature")).toBool(settings.autoSelectRunningFeature);
    settings.pinTargetWindowToScreenCenter =
        root.value(QStringLiteral("pinTargetWindowToScreenCenter")).toBool(settings.pinTargetWindowToScreenCenter);
    settings.pinSubTargetWindowToScreenCenter =
        root.value(QStringLiteral("pinSubTargetWindowToScreenCenter"))
            .toBool(settings.pinSubTargetWindowToScreenCenter);
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
    settings.subTargetWindowTitle =
        root.value(QStringLiteral("subTargetWindowTitle")).toString(settings.subTargetWindowTitle);
    settings.subLinkedTargetProcessPath =
        root.value(QStringLiteral("subLinkedTargetProcessPath")).toString(settings.subLinkedTargetProcessPath);
    settings.triggerArmedFeatureIds.clear();
    const QJsonValue armedValue = root.value(QStringLiteral("triggerArmedFeatureIds"));
    if (armedValue.isArray()) {
        for (const QJsonValue& entry : armedValue.toArray()) {
            const QString featureId = entry.toString().trimmed();
            if (!featureId.isEmpty()) {
                settings.triggerArmedFeatureIds.append(featureId);
            }
        }
    }
    m_settingsCache[id] = settings;
    m_settingsFileMtime[id] = mtime;
    return settings;
}

bool ProfileManager::saveSettings(const QString& id,
                                  const ProgramSettings::ProfileSettings& settings,
                                  bool replaceLinkedProcessPath,
                                  bool replaceSubLinkedProcessPath,
                                  bool replaceTriggerArmedFeatureIds) const {
    if(!m_storageReady)return false;
    QDir().mkpath(profileDirectory(id));
    ProgramSettings::ProfileSettings toWrite = settings;
    // Callers that snapshot QSettings-backed options often omit the exe path.
    // Preserve previously saved process paths unless the caller explicitly replaces/clears them.
    if (!replaceLinkedProcessPath && toWrite.linkedTargetProcessPath.isEmpty()) {
        toWrite.linkedTargetProcessPath = loadSettings(id).linkedTargetProcessPath;
    }
    if (!replaceSubLinkedProcessPath && toWrite.subLinkedTargetProcessPath.isEmpty()) {
        toWrite.subLinkedTargetProcessPath = loadSettings(id).subLinkedTargetProcessPath;
    }
    // Preserve subTargetWindowTitle when the caller snapshot omitted it (empty) but the
    // in-memory / on-disk value still exists — unless replaceSub clears the binding.
    if (toWrite.subTargetWindowTitle.isEmpty() && !replaceSubLinkedProcessPath) {
        const QString previousSubTitle = loadSettings(id).subTargetWindowTitle;
        if (!previousSubTitle.isEmpty()) {
            toWrite.subTargetWindowTitle = previousSubTitle;
        }
    }
    if (!replaceTriggerArmedFeatureIds) {
        toWrite.triggerArmedFeatureIds = loadSettings(id).triggerArmedFeatureIds;
    }
    QJsonObject root;
    root.insert(QStringLiteral("version"), 1);
    root.insert(QStringLiteral("autoSelectRunningFeature"), toWrite.autoSelectRunningFeature);
    root.insert(QStringLiteral("pinTargetWindowToScreenCenter"), toWrite.pinTargetWindowToScreenCenter);
    if (toWrite.pinSubTargetWindowToScreenCenter) {
        root.insert(QStringLiteral("pinSubTargetWindowToScreenCenter"), true);
    }
    root.insert(QStringLiteral("imageFindCaptureMode"), static_cast<int>(toWrite.imageFindCaptureMode));
    root.insert(QStringLiteral("runWithoutTargetWindow"), toWrite.runWithoutTargetWindow);
    if (!toWrite.linkedTargetProcessPath.isEmpty()) {
        root.insert(QStringLiteral("linkedTargetProcessPath"), toWrite.linkedTargetProcessPath);
    }
    if (!toWrite.subTargetWindowTitle.isEmpty()) {
        root.insert(QStringLiteral("subTargetWindowTitle"), toWrite.subTargetWindowTitle);
    }
    if (!toWrite.subLinkedTargetProcessPath.isEmpty()) {
        root.insert(QStringLiteral("subLinkedTargetProcessPath"), toWrite.subLinkedTargetProcessPath);
    }
    {
        QJsonArray armedArray;
        for (const QString& featureId : toWrite.triggerArmedFeatureIds) {
            if (!featureId.isEmpty()) {
                armedArray.append(featureId);
            }
        }
        root.insert(QStringLiteral("triggerArmedFeatureIds"), armedArray);
    }
    QSaveFile file(QDir(profileDirectory(id)).filePath(QString::fromLatin1(kSettingsFileName)));
    if (!file.open(QIODevice::WriteOnly)) {
        return false;
    }
    const QByteArray bytes = QJsonDocument(root).toJson(QJsonDocument::Indented);
    if (file.write(bytes) != bytes.size() || !file.commit()) {
        return false;
    }
    const QFileInfo writtenInfo(file.fileName());
    m_settingsCache[id] = toWrite;
    m_settingsFileMtime[id] = writtenInfo.lastModified().toMSecsSinceEpoch();
    return true;
}

QStringList ProfileManager::triggerArmedFeatureIds(const QString& id) const {
    return loadSettings(id).triggerArmedFeatureIds;
}

bool ProfileManager::setTriggerArmedFeatureIds(const QString& id, const QStringList& featureIds) const {
    ProgramSettings::ProfileSettings settings = loadSettings(id);
    settings.triggerArmedFeatureIds = featureIds;
    return saveSettings(id, settings, false, false, true);
}

bool ProfileManager::updateTriggerArmedFeature(const QString& profileId,
                                               const QString& featureId,
                                               bool armed) const {
    if (profileId.isEmpty() || featureId.isEmpty()) {
        return false;
    }
    ProgramSettings::ProfileSettings settings = loadSettings(profileId);
    QStringList ids = settings.triggerArmedFeatureIds;
    if (armed) {
        if (!ids.contains(featureId)) {
            ids.append(featureId);
        }
    } else {
        ids.removeAll(featureId);
    }
    if (ids == settings.triggerArmedFeatureIds) {
        return true;
    }
    settings.triggerArmedFeatureIds = ids;
    return saveSettings(profileId, settings, false, false, true);
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
            const ProgramSettings::ProfileSettings settings = loadSettings(profile.id);
            profile.subTargetWindowTitle = settings.subTargetWindowTitle;
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
    if(!m_storageReady)return false;
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

    QSaveFile file(QDir(profilesDirectory()).filePath(QString::fromLatin1(kManifestFileName)));
    if (!file.open(QIODevice::WriteOnly)) {
        return false;
    }
    const QByteArray bytes = QJsonDocument(root).toJson(QJsonDocument::Indented);
    return file.write(bytes) == bytes.size() && file.commit();
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
    for (Profile& profile : m_profiles) {
        if (profile.id == m_defaultProfileId) {
            profile.subTargetWindowTitle.clear();
            break;
        }
    }
    ProgramSettings::ProfileSettings settings = loadSettings(m_defaultProfileId);
    if (!settings.runWithoutTargetWindow
        || !settings.subTargetWindowTitle.isEmpty()
        || !settings.subLinkedTargetProcessPath.isEmpty()) {
        settings.runWithoutTargetWindow = true;
        settings.subTargetWindowTitle.clear();
        settings.subLinkedTargetProcessPath.clear();
        saveSettings(m_defaultProfileId, settings, false, true);
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

qint64 ProfileManager::projectFileMtime(const QString& projectPath) const {
    const QFileInfo info(projectPath);
    return info.exists() ? info.lastModified().toMSecsSinceEpoch() : 0;
}

void ProfileManager::touchProjectCacheLru(const QString& id) const {
    m_projectCacheLru.erase(std::remove(m_projectCacheLru.begin(), m_projectCacheLru.end(), id),
                            m_projectCacheLru.end());
    m_projectCacheLru.insert(m_projectCacheLru.begin(), id);
}

void ProfileManager::evictProjectCacheIfNeeded() const {
    while (static_cast<int>(m_projectCache.size()) > kMaxCachedProjects && !m_projectCacheLru.empty()) {
        const QString evictId = m_projectCacheLru.back();
        m_projectCacheLru.pop_back();
        m_projectCache.erase(evictId);
    }
}

std::unique_ptr<Project> ProfileManager::cloneCachedProject(const QString& id,
                                                            const QString& projectPath,
                                                            QString* projectDirectoryOut) const {
    const qint64 mtime = projectFileMtime(projectPath);
    const auto it = m_projectCache.find(id);
    if (it == m_projectCache.end() || it->second.fileMtime != mtime
        || it->second.projectPath != projectPath || !it->second.project) {
        return nullptr;
    }
    touchProjectCacheLru(id);
    if (projectDirectoryOut) {
        *projectDirectoryOut = it->second.projectDirectory;
    }
    return it->second.project->clone();
}

void ProfileManager::storeCachedProject(const QString& id,
                                         const QString& projectPath,
                                         std::unique_ptr<Project> project,
                                         const QString& projectDirectory) {
    if (!project) {
        return;
    }
    CachedProjectEntry entry;
    entry.project = std::move(project);
    entry.projectDirectory = projectDirectory;
    entry.projectPath = projectPath;
    entry.fileMtime = projectFileMtime(projectPath);
    m_projectCache[id] = std::move(entry);
    touchProjectCacheLru(id);
    evictProjectCacheIfNeeded();
}

void ProfileManager::invalidateCachedProject(const QString& id) {
    m_projectCache.erase(id);
    m_projectCacheLru.erase(std::remove(m_projectCacheLru.begin(), m_projectCacheLru.end(), id),
                            m_projectCacheLru.end());
}
