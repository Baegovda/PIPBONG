#include "app/ProfilePresentationSnapshot.h"
#include "app/ProfileManager.h"
#include "model/Project.h"

ProfilePresentationSnapshot::List ProfilePresentationSnapshot::list(const ProfileManager& manager){
    List result;result.activeId=manager.activeProfileId();
    const auto defaultId=manager.defaultProfileId();const auto profiles=manager.profiles();
    result.rows.reserve(static_cast<qsizetype>(profiles.size()));
    for(const auto& profile:profiles)
        result.rows.push_back({profile.id,profile.name,profile.targetWindowTitle,
            manager.linkedTargetProcessPath(profile.id),manager.linkedTargetIcon(profile.id),profile.id==defaultId});
    return result;
}
bool ProfilePresentationSnapshot::isDefault(const ProfileManager* manager){
    return manager && manager->activeProfileId()==manager->defaultProfileId();
}
QString ProfilePresentationSnapshot::documentTitle(const Project* project){
    return project?QString::fromStdString(project->targetWindowTitle()):QString();
}
TargetWindowDetailsController::Request ProfilePresentationSnapshot::targetDetails(
    const ProfileManager* manager,const Project* project,TargetWindowDetailsController::Request result){
    result.profileId=manager?manager->activeProfileId():QString();
    result.globalDefault=isDefault(manager);
    result.bindings={};result.bindings.mainTitle=documentTitle(project).trimmed();
    if(manager){
        result.bindings.subTitle=manager->subTargetWindowTitle(result.profileId).trimmed();
        if(!result.globalDefault && !result.profileId.isEmpty()){
            const auto settings=manager->loadSettings(result.profileId);
            result.bindings.mainProcessPath=settings.linkedTargetProcessPath;
            result.bindings.subProcessPath=settings.subLinkedTargetProcessPath;
        }
    }
    return result;
}
