#include "app/ProfileActionsController.h"
#include <QPointer>

ProfileActionsController::ProfileActionsController(ProfileManager& profiles,Host host,QObject* parent)
    :QObject(parent),m_profiles(profiles),m_host(std::move(host)){}

ProfileActionsController::Result ProfileActionsController::execute(Request request) {
    const auto host=m_host;QPointer<ProfileActionsController> life(this);
    Result result;result.action=request.action;result.id=request.id;
    const auto cancelled=[&]{result.cancelled=true;result.message=tr("프로필이나 작업 상태가 바뀌었어요. 다시 시도해 주세요.");return result;};
    const auto valid=[this,life,host,&request]{return life && m_profiles.storageReady() && host.canBegin() && life
        && m_profiles.activeProfileId()==request.owner && host.generation()==request.generation && life;};
    if(*m_busy || !valid())return cancelled();
    struct Guard{std::shared_ptr<bool> busy;std::function<void()> finished;~Guard(){*busy=false;if(finished)finished();}}
        guard{m_busy,[life,host]{if(life && host.finished)host.finished();}};*guard.busy=true;
    if(request.action!=Action::Add && (!m_profiles.profileById(request.id) || m_profiles.isDefaultProfile(request.id))) {
        result.message=tr("기본 프로필이거나 더 이상 존재하지 않아 변경할 수 없어요.");return result;
    }
    const QString reason=request.action==Action::Add?"profile-add":request.action==Action::Edit?"profile-edit":"profile-remove";
    if(!host.checkpoint(reason,request.action!=Action::Edit)) {
        result.cancelled=true;result.message=tr("현재 내용을 저장하지 못해 프로필 변경을 중단했어요.");return result;
    }
    if(!valid())return cancelled();
    if(request.action==Action::Add) {
        result.id=m_profiles.addProfile(request.edit.name,false);result.success=!result.id.isEmpty();
    } else if(request.action==Action::Edit)result.success=m_profiles.applyEdit(request.id,request.edit,&result.message);
    else result.success=m_profiles.removeProfile(request.id,&result.message);
    if(!result.success && result.message.isEmpty())result.message=tr("프로필을 저장하지 못했어요.");
    if(result.success)host.changed(result);
    return result;
}
