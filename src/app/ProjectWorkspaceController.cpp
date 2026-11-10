#include "app/ProjectWorkspaceController.h"
#include "app/ProfileManager.h"
#include "app/ProfilePackageController.h"
#include "model/Project.h"
#include "storage/JsonSerializer.h"
#include <QDir>
#include <QFileInfo>
#include <QSettings>
#include <QTimer>
#include <QPointer>

namespace {
bool sameDocumentPath(const QString& a,const QString& b) {
    return QDir::cleanPath(QFileInfo(a).absoluteFilePath()).compare(
        QDir::cleanPath(QFileInfo(b).absoluteFilePath()),Qt::CaseInsensitive)==0;
}
}
struct ProjectWorkspaceController::WriteTicket {
    QPointer<ProjectWorkspaceController> life;HostCallbacks host;std::shared_ptr<Project> project;
    QString owner,selected,path,directory,canonical;quint64 generation=0;
    bool current(bool pathMatches=true)const {
        return life && life->m_state.project==project && life->ownerProfile()==owner
            && life->m_profiles.activeProfileId()==selected && life->m_generation==generation
            && (!pathMatches || life->m_state.path==path) && life->m_state.directory==directory;
    }
};
ProjectWorkspaceController::WriteTicket ProjectWorkspaceController::writeTicket()const {
    const auto owner=ownerProfile();return {const_cast<ProjectWorkspaceController*>(this),m_host,m_state.project,
        owner,m_profiles.activeProfileId(),m_state.path,m_state.directory,m_profiles.projectPath(owner),m_generation};
}

ProjectWorkspaceController::ProjectWorkspaceController(ProfileManager& profiles,QString fallbackPath,QObject* parent)
    : QObject(parent),m_profiles(profiles),m_fallbackPath(std::move(fallbackPath)),m_autoSave(new QTimer(this)) {
    m_state.project=std::make_unique<Project>();
    m_autoSave->setSingleShot(true);
    connect(m_autoSave,&QTimer::timeout,this,[this] { save(); });
}
ProjectWorkspaceController::~ProjectWorkspaceController() = default;
bool ProjectWorkspaceController::fail(const QString& text) { m_error=text; return false; }
QString ProjectWorkspaceController::directory() const {
    const auto value=m_host.currentDirectory ? m_host.currentDirectory() : QString();
    return value.isEmpty()?m_state.directory:value;
}
QString ProjectWorkspaceController::ownerProfile() const {
    return m_state.profileId.isEmpty()?m_profiles.activeProfileId():m_state.profileId;
}
void ProjectWorkspaceController::markModified() {
    m_state.modified=true;
    if(m_host.changed) m_host.changed();
    if(!m_host.maySchedule || m_host.maySchedule()) m_autoSave->start(800);
}
bool ProjectWorkspaceController::ensurePath() {
    if(m_state.path.isEmpty()) m_state.path=m_fallbackPath;
    return !m_state.path.isEmpty();
}
bool ProjectWorkspaceController::write(const QString& path,bool updatePath,bool canonicalOnly) {
    const auto t=writeTicket();
    m_error.clear();
    if(!m_profiles.storageReady() || !m_profiles.profileById(t.owner))return fail(QStringLiteral("프로필 복구를 마친 뒤 저장해 주세요."));
    const bool allowed=!t.host.mayWrite || t.host.mayWrite();if(!t.life)return false;
    if(!allowed || !t.current())return fail(QStringLiteral("현재 작업이나 프로필 전환을 마친 뒤 저장해 주세요."));
    const auto requested=t.host.currentDirectory?t.host.currentDirectory():t.directory;if(!t.life)return false;
    const auto directory=requested.isEmpty()?t.directory:requested;
    if(!t.current())return fail(QStringLiteral("프로젝트가 바뀌었어요. 다시 저장해 주세요."));
    if(canonicalOnly && m_state.modified)return fail(QStringLiteral("편집 내용이 바뀌었어요. 다시 저장해 주세요."));
    if(!t.project || path.isEmpty() || !JsonSerializer::saveToFile(*t.project,path,directory))
        return fail(QStringLiteral("프로젝트를 저장하지 못했어요."));
    // Save As retains its normal external document path. The profile's canonical
    // snapshot remains the source for reload/sealing, so success requires both.
    if(!sameDocumentPath(path,t.canonical) && !JsonSerializer::saveToFile(*t.project,t.canonical,directory)) {
        m_profileSnapshotPending=true;m_state.modified=true;
        m_error=QStringLiteral("선택한 파일은 저장했지만 프로필 저장본을 갱신하지 못했어요. 편집 내용과 이전 저장 경로를 유지했어요. 파일 잠금을 해제한 뒤 다시 저장해 주세요.");
        if(t.host.changed)t.host.changed();return false;
    }
    if(updatePath) { m_state.path=path; QSettings().setValue(QStringLiteral("project/lastFile"),path); }
    if(!canonicalOnly)m_state.modified=false;
    m_profileSnapshotPending=false;
    m_profiles.invalidateCachedProject(t.owner);
    if(t.host.changed) t.host.changed();
    return true;
}
bool ProjectWorkspaceController::syncCanonical() {
    if(!m_profileSnapshotPending)return true;
    const auto canonical=m_profiles.projectPath(ownerProfile());return write(canonical,false,true);
}
bool ProjectWorkspaceController::save(bool quiet) {
    if(!ensurePath())return false;
    const auto t=writeTicket();
    if(!write(m_state.path,false)) return false;
    if(!t.current())return true;
    if(!quiet && t.host.status)t.host.status(QStringLiteral("자동 저장됨"));
    if(t.current())scheduleSeal();
    return true;
}
bool ProjectWorkspaceController::saveAs(const QString& path) {
    const auto t=writeTicket();const bool allowed=!t.host.mayReplace || t.host.mayReplace();if(!t.life)return false;
    if(!allowed || !t.current())return fail(QStringLiteral("편집이나 프로필 전환을 마친 뒤 저장 위치를 바꿔 주세요."));
    if(!write(path,true)) return false;
    if(t.current(false) && m_state.path==path)scheduleSeal();return true;
}
bool ProjectWorkspaceController::checkpoint(bool quiet) {
    const auto t=writeTicket();m_autoSave->stop();
    const bool saved=m_state.modified?save(quiet):syncCanonical();
    if(!saved || !t.life)return false;
    if(!t.current() || m_state.modified || m_profileSnapshotPending)
        return fail(QStringLiteral("저장 중 프로젝트나 편집 내용이 바뀌었어요. 다시 저장해 주세요."));
    return true;
}
bool ProjectWorkspaceController::commitTargetBinding(const QString& profileId,quint64 generation,
                                                     bool sub,const QString& title,const QString& processPath) {
    m_error.clear();
    if(profileId.isEmpty() || profileId!=m_state.profileId || profileId!=m_profiles.activeProfileId()
        || generation!=m_generation || !m_state.project || m_profiles.isDefaultProfile(profileId))
        return fail(QStringLiteral("프로필이나 프로젝트가 바뀌었어요. 타겟을 다시 선택해 주세요."));
    if((m_host.mayReplace && !m_host.mayReplace()) || (m_host.mayWrite && !m_host.mayWrite()))
        return fail(QStringLiteral("현재 편집이나 저장 작업을 마친 뒤 타겟을 지정해 주세요."));
    if(sub) {
        if(!m_profiles.updateProfileSubTargetBinding(profileId,title,processPath,true))
            return fail(QStringLiteral("서브 창 연결을 저장하지 못했어요. 기존 연결을 유지합니다."));
    } else {
        const auto oldTitle=m_state.project->targetWindowTitle();
        const bool oldModified=m_state.modified;
        m_state.project->setTargetWindowTitle(title.trimmed().toStdString());
        if(!ensurePath() || !write(m_state.path,false)) {
            m_state.project->setTargetWindowTitle(oldTitle);m_state.modified=oldModified;
            if(m_host.changed) m_host.changed();
            return fail(QStringLiteral("타겟 프로젝트를 저장하지 못했어요. 기존 연결을 유지합니다."));
        }
        if(!m_profiles.updateProfileTargetBinding(profileId,title,processPath,true)) {
            m_state.project->setTargetWindowTitle(oldTitle);m_state.modified=true;
            if(!write(m_state.path,false)) {
                markModified();
                return fail(QStringLiteral("연결 설정 저장과 이전 타겟 복원 저장에 실패했어요. 기존 타겟은 편집기에 남아 있어요. 파일 잠금을 해제한 뒤 다시 저장해 주세요."));
            }
            scheduleSeal();
            return fail(QStringLiteral("연결 설정을 저장하지 못했어요. 이전 타겟으로 복원했습니다."));
        }
    }
    scheduleSeal();
    return true;
}
bool ProjectWorkspaceController::sameSettings(const ProgramSettings::ProfileSettings& a,const ProgramSettings::ProfileSettings& b) {
    return a.autoSelectRunningFeature==b.autoSelectRunningFeature
        && a.pinTargetWindowToScreenCenter==b.pinTargetWindowToScreenCenter
        && a.pinSubTargetWindowToScreenCenter==b.pinSubTargetWindowToScreenCenter
        && a.imageFindCaptureMode==b.imageFindCaptureMode && a.runWithoutTargetWindow==b.runWithoutTargetWindow
        && a.linkedTargetProcessPath==b.linkedTargetProcessPath && a.subTargetWindowTitle==b.subTargetWindowTitle
        && a.subLinkedTargetProcessPath==b.subLinkedTargetProcessPath && a.triggerArmedFeatureIds==b.triggerArmedFeatureIds;
}
bool ProjectWorkspaceController::saveSettings() {
    return saveSettingsValue(ProgramSettings::profileSettings());
}
bool ProjectWorkspaceController::saveCenterPinOptions(const QString& profileId,quint64 generation,bool main,bool sub) {
    m_error.clear();
    if(profileId.isEmpty() || profileId!=m_state.profileId || profileId!=m_profiles.activeProfileId()
        || generation!=m_generation || m_profiles.isDefaultProfile(profileId))
        return fail(QStringLiteral("프로필이 바뀌었어요. 중앙 고정을 다시 설정해 주세요."));
    if((m_host.mayReplace && !m_host.mayReplace()) || (m_host.mayWrite && !m_host.mayWrite()))
        return fail(QStringLiteral("현재 편집이나 저장 작업을 마친 뒤 중앙 고정을 설정해 주세요."));
    auto settings=ProgramSettings::profileSettings();
    settings.pinTargetWindowToScreenCenter=main;settings.pinSubTargetWindowToScreenCenter=sub;
    if(!saveSettingsValue(settings))return false;
    ProgramSettings::setPinTargetWindowToScreenCenter(main);
    ProgramSettings::setPinSubTargetWindowToScreenCenter(sub);
    scheduleSeal();return true;
}
bool ProjectWorkspaceController::saveSettingsValue(ProgramSettings::ProfileSettings settings) {
    if(!m_profiles.storageReady())return fail(QStringLiteral("프로필 복구가 끝나기 전에는 설정을 저장할 수 없어요."));
    const auto id=ownerProfile();
    settings.linkedTargetProcessPath=m_profiles.linkedTargetProcessPath(id);
    settings.subTargetWindowTitle=m_profiles.subTargetWindowTitle(id);
    settings.subLinkedTargetProcessPath=m_profiles.subLinkedTargetProcessPath(id);
    settings.triggerArmedFeatureIds=m_profiles.triggerArmedFeatureIds(id);
    if(m_settingsValid && m_settingsProfile==id && sameSettings(settings,m_settings)) return true;
    if(!m_profiles.saveSettings(id,settings)) return fail(QStringLiteral("프로필 설정을 저장하지 못했어요."));
    m_settingsValid=true;m_settingsProfile=id;m_settings=settings;return true;
}
void ProjectWorkspaceController::scheduleSeal() {
    const auto t=writeTicket();auto* packages=m_packages;
    if(!packages || m_state.modified)return;
    const bool allowed=!t.host.maySchedule || t.host.maySchedule();if(!t.life || !allowed || !t.current())return;
    if(!syncCanonical()){if(t.life && t.host.warning)t.host.warning(m_error);return;}
    if(!t.life || !t.current())return;
    if(!saveSettings()) { if(m_host.warning) m_host.warning(m_error);return; }
    packages->schedule(m_profiles.profileDirectory(t.owner),m_profiles.profilePackagePath(t.owner));
}
bool ProjectWorkspaceController::seal(bool synchronous) {
    const auto t=writeTicket();
    if(!m_profiles.storageReady())return fail(QStringLiteral("프로필 복구가 끝나기 전에는 패키지를 저장할 수 없어요."));
    if(!synchronous) { scheduleSeal();return true; }
    if(!checkpoint(true) || !t.life || !t.current())return false;
    if(!m_packages || !saveSettings()) return false;
    const auto directory=m_profiles.profileDirectory(t.owner),package=m_profiles.profilePackagePath(t.owner);
    const bool packed=m_packages->packNow(directory,package);if(!t.life)return packed;
    if(!t.current())return false;
    if(!packed) {
        fail(QStringLiteral("프로필 패키지를 저장하지 못했어요. 이전 패키지는 유지돼요."));
        if(m_host.warning) m_host.warning(m_error);return false;
    }
    return true;
}
std::unique_ptr<ProjectWorkspaceController::Staged> ProjectWorkspaceController::stageFile(const QString& profile,const QString& path) {
    auto staged=std::make_unique<Staged>();staged->profileId=profile;staged->path=path;
    staged->project=m_profiles.cloneCachedProject(profile,path,&staged->directory);
    if(!staged->project) {
        staged->project=JsonSerializer::loadFromFile(path,&staged->directory);
        if(!staged->project) { fail(QStringLiteral("프로젝트를 불러오지 못했어요."));return {}; }
        if(auto clone=staged->project->clone()) m_profiles.storeCachedProject(profile,path,std::move(clone),staged->directory);
    }
    if(staged->directory.isEmpty()) staged->directory=m_profiles.projectDirectory(profile);
    return staged;
}
std::unique_ptr<ProjectWorkspaceController::Staged> ProjectWorkspaceController::stageProfile(const QString& profile) {
    m_error.clear();
    if(!m_profiles.profileById(profile)) { fail(QStringLiteral("프로필을 찾지 못했어요."));return {}; }
    if(!m_profiles.ensureProfileWorkspace(profile)) { fail(QStringLiteral("프로필 패키지를 읽지 못했어요."));return {}; }
    const auto path=m_profiles.projectPath(profile);
    std::unique_ptr<Staged> staged;
    if(QFileInfo::exists(path)) staged=stageFile(profile,path);
    else {
        staged=std::make_unique<Staged>();staged->profileId=profile;staged->path=path;
        staged->directory=m_profiles.projectDirectory(profile);staged->fresh=true;
        staged->project=std::make_unique<Project>();staged->project->addFeature("예시 기능");
        if(!QDir().mkpath(staged->directory) || !JsonSerializer::saveToFile(*staged->project,path,staged->directory)) {
            fail(QStringLiteral("프로필 프로젝트를 준비하지 못했어요."));return {};
        }
    }
    if(!staged) return {};
    staged->settings=m_profiles.loadSettings(profile);staged->applySettings=true;
    staged->clearTarget=m_profiles.isDefaultProfile(profile);
    if(staged->clearTarget) staged->project->setTargetWindowTitle({});
    return staged;
}
bool ProjectWorkspaceController::install(std::unique_ptr<Staged> staged,bool quiet,bool restoreTriggers,InstallMode mode) {
    const auto t=writeTicket();
    m_error.clear();
    if(!staged || !staged->project) return fail(QStringLiteral("프로젝트 준비가 끝나지 않았어요."));
    const bool allowed=mode!=InstallMode::Guarded || !t.host.mayReplace || t.host.mayReplace();if(!t.life)return false;
    if(!allowed || !t.current())return fail(QStringLiteral("편집이나 프로필 전환을 마친 뒤 프로젝트를 바꿔 주세요."));
    if(t.host.beforeReplace)t.host.beforeReplace();if(!t.life)return false;
    if(!t.current())return fail(QStringLiteral("프로젝트가 바뀌어 불러오기를 취소했어요. 다시 선택해 주세요."));
    if(staged->applySettings) {
        ProgramSettings::applyProfileSettings(staged->settings);
        m_settingsProfile=staged->profileId;m_settings=staged->settings;m_settingsValid=true;
    }
    m_state.profileId=staged->profileId;
    m_state.project=std::move(staged->project);m_state.path=staged->path;m_state.directory=staged->directory;m_state.modified=false;
    m_profileSnapshotPending=!sameDocumentPath(m_state.path,m_profiles.projectPath(m_state.profileId));
    ++m_generation;
    const auto installed=m_state.project;const auto epoch=m_generation;
    const auto current=[&]{return t.life && m_generation==epoch && m_state.project==installed;};
    if(t.host.directoryChanged)t.host.directoryChanged(m_state.directory);if(!current())return true;
    m_profiles.setProfileTargetWindowTitleInMemory(staged->profileId,QString::fromStdString(m_state.project->targetWindowTitle()));
    QSettings().setValue(QStringLiteral("project/lastFile"),m_state.path);
    if(t.host.installed)t.host.installed({staged->fresh,staged->clearTarget,quiet,restoreTriggers});
    return true;
}
bool ProjectWorkspaceController::load(const QString& path,bool quiet) {
    if(m_host.mayReplace && !m_host.mayReplace()) return fail(QStringLiteral("편집을 마친 뒤 프로젝트를 바꿔 주세요."));
    m_error.clear();
    auto staged=stageFile(m_profiles.activeProfileId(),path);
    return staged && install(std::move(staged),quiet,false);
}
bool ProjectWorkspaceController::loadActive(bool quiet,bool restoreTriggers) {
    if(m_host.mayReplace && !m_host.mayReplace()) return fail(QStringLiteral("편집을 마친 뒤 프로필을 바꿔 주세요."));
    auto staged=stageProfile(m_profiles.activeProfileId());
    return staged && install(std::move(staged),quiet,restoreTriggers);
}
bool ProjectWorkspaceController::createEmpty() {
    if(m_host.mayReplace && !m_host.mayReplace()) return fail(QStringLiteral("편집을 마친 뒤 프로젝트를 바꿔 주세요."));
    auto staged=std::make_unique<Staged>();staged->project=std::make_unique<Project>();
    staged->profileId=m_profiles.activeProfileId();staged->path=m_profiles.activeProjectPath();staged->directory=m_profiles.activeProjectDirectory();
    staged->fresh=true;staged->clearTarget=true;
    if(!JsonSerializer::saveToFile(*staged->project,staged->path,staged->directory)) return fail(QStringLiteral("새 프로젝트를 저장하지 못했어요."));
    return install(std::move(staged),false,false);
}
