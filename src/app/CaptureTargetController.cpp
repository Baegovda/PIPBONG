#include "CaptureTargetController.h"
#include "app/ProfileManager.h"
#include "app/ProgramSettings.h"
#include "app/FeatureRunSession.h"
#include "app/RunContextController.h"
#include "app/SessionRunPolicy.h"
#include "model/Feature.h"
#include "core/capture/ScreenCapture.h"

namespace {
CaptureTargetController::Ports nativePorts() {
    using Controller=CaptureTargetController;Controller::Ports ports;
#ifdef _WIN32
    ports.read=[](HWND hwnd)->std::optional<Controller::Window> {
        ScreenCapture::TargetWindowInfo info;DWORD before=0,after=0;
        if(!hwnd || !IsWindow(hwnd) || !GetWindowThreadProcessId(hwnd,&before)
            || !ScreenCapture::queryWindowInfo(hwnd,info) || !GetWindowThreadProcessId(hwnd,&after)
            || before!=after)return std::nullopt;
        return Controller::Window{hwnd,QString::fromStdWString(info.title),QString::fromStdWString(info.processPath),before,info.visible,info.minimized};
    };
    const auto read=ports.read;
    ports.find=[read](const QString& title,const QString& path)->std::optional<Controller::Window> {
        return read(ScreenCapture::findVisibleWindowMatchingTitle(title.toStdWString(),path.toStdWString()));
    };
    ports.apply=[](const Controller::Applied& value) {
        ScreenCapture::setSubTargetWindowTitle(value.bindings.subBinding.toStdWString());
        ScreenCapture::setLinkedTargetProcessPaths(value.bindings.mainProcessPath.toStdWString(),value.bindings.subProcessPath.toStdWString());
        ScreenCapture::invalidateTargetWindowCache();ScreenCapture::setTargetWindowTitle(value.target.title.toStdWString());
        ScreenCapture::setTargetWindow(value.target.hwnd);ScreenCapture::setForegroundHintWindow(value.target.hwnd);
    };
#endif
    return ports;
}
}
CaptureTargetController::CaptureTargetController(ProfileManager& profiles):CaptureTargetController(profiles,nativePorts()) {}
CaptureTargetController::CaptureTargetController(ProfileManager& profiles,Ports ports):m_profiles(profiles),m_ports(std::move(ports)) {}
CaptureTargetController::Bindings CaptureTargetController::bindings(const QString& requested) const {
    Bindings value;value.activeProfileId=requested.isEmpty()?m_profiles.activeProfileId():requested;
    value.activeIsDefault=!m_profiles.profileById(value.activeProfileId) || m_profiles.isDefaultProfile(value.activeProfileId);
    if(value.activeIsDefault)return value;
    value.mainBinding=value.activeProfileId==m_profiles.activeProfileId() && m_host.mainTitle
        ?m_host.mainTitle().trimmed():m_profiles.targetWindowTitle(value.activeProfileId).trimmed();
    value.subBinding=m_profiles.subTargetWindowTitle(value.activeProfileId).trimmed();
    value.mainProcessPath=m_profiles.linkedTargetProcessPath(value.activeProfileId);
    value.subProcessPath=m_profiles.subLinkedTargetProcessPath(value.activeProfileId);return value;
}
CaptureTargetController::Selection CaptureTargetController::selected(const Bindings& value,bool sub,Handle hwnd,bool foreground) {
    return {sub?value.subBinding:value.mainBinding,sub?value.subProcessPath:value.mainProcessPath,hwnd,sub,foreground};
}
std::optional<bool> CaptureTargetController::classify(const Bindings& value,const Window& window) const {
    if(!window.hwnd || !window.visible || window.minimized)return std::nullopt;
    const auto matches=[&](const QString& title,const QString& path) {
        return !title.isEmpty() && (!path.isEmpty()?path.compare(window.path,Qt::CaseInsensitive)==0
            :window.title.contains(title,Qt::CaseInsensitive));
    };
    const bool main=matches(value.mainBinding,value.mainProcessPath),sub=matches(value.subBinding,value.subProcessPath);
    if(!main && !sub)return std::nullopt;
    if(main && sub) {
        return ForegroundRunGate::preferSubTitle(window.title,value.mainBinding,value.subBinding);
    }
    return sub;
}
std::optional<CaptureTargetController::Selection> CaptureTargetController::foreground(const Bindings& value) const {
    if(value.activeIsDefault || !m_host.foreground || !m_ports.read)return std::nullopt;
    const auto state=m_host.foreground();
#ifdef _WIN32
    if(state.pipbong || state.shellTransient || !state.rootHwnd)return std::nullopt;
    const auto live=m_ports.read(state.rootHwnd);
    if(!live)return std::nullopt;
    const auto role=classify(value,*live);
    if(role)return selected(value,*role,live->hwnd,true);
#endif
    return std::nullopt;
}
std::optional<CaptureTargetController::Window> CaptureTargetController::visible(const QString& title,const QString& path,bool allowMinimized) const {
    if(title.isEmpty() || !m_ports.find)return std::nullopt;
    const auto window=m_ports.find(title,path);
    if(!window || !window->hwnd || !window->visible || (!allowMinimized && window->minimized)
        || (!path.isEmpty() && path.compare(window->path,Qt::CaseInsensitive)!=0)
        || (path.isEmpty() && !window->title.contains(title,Qt::CaseInsensitive)))return std::nullopt;
    return window;
}
QString CaptureTargetController::effective() const {
    const auto value=bindings();
    if(const auto chosen=foreground(value))return chosen->title;
    return value.mainBinding.isEmpty()?value.subBinding:value.mainBinding;
}
QString CaptureTargetController::lookup() const {
    const auto value=bindings();return value.mainBinding.isEmpty()?value.subBinding:value.mainBinding;
}
QString CaptureTargetController::resolve(FeatureCaptureTargetScope scope,bool background,const QString& profileId) const {
    return choose(scope,background,profileId).title;
}
CaptureTargetController::Selection CaptureTargetController::choose(FeatureCaptureTargetScope scope,bool background,const QString& profileId) const {
    const auto value=bindings(profileId);
    if(value.activeIsDefault)return {};
    if(scope==FeatureCaptureTargetScope::MainOnly)return selected(value,false);
    if(scope==FeatureCaptureTargetScope::SubOnly)return selected(value,true);
    const bool without=value.activeProfileId==m_profiles.activeProfileId()?ProgramSettings::runWithoutTargetWindow()
        :m_profiles.loadSettings(value.activeProfileId).runWithoutTargetWindow;
    if(!without && !background && m_host.foregroundAllowed && !m_host.foregroundAllowed(value.activeProfileId))return {};
    if(const auto chosen=foreground(value))return *chosen;
    if(const auto window=visible(value.mainBinding,value.mainProcessPath))return selected(value,false,window->hwnd);
    if(const auto window=visible(value.subBinding,value.subProcessPath))return selected(value,true,window->hwnd);
    return {};
}
bool CaptureTargetController::mayApply() const {return !m_host.mayApply || m_host.mayApply();}
bool CaptureTargetController::backgroundVisible(FeatureCaptureTargetScope scope,const QString& profileId) const {
    const auto chosen=choose(scope,true,profileId);
    return visible(chosen.title,chosen.path).has_value();
}
void CaptureTargetController::apply(const Bindings& value,const Selection& chosen) const {
    if(mayApply() && m_ports.apply)m_ports.apply({value,chosen});
}
bool CaptureTargetController::adoptForeground() {
    const auto value=bindings();const auto chosen=foreground(value);
    if(!chosen)return false;
    apply(value,*chosen);
    if(m_host.remember && m_host.foreground)m_host.remember(chosen->hwnd,m_host.foreground().title);
    refreshMonitors();return true;
}
void CaptureTargetController::applyTitle(const QString& title) const {
    const auto value=bindings();Selection chosen;chosen.title=value.activeIsDefault?QString():title;
    chosen.path=pathForTitle(value,FeatureCaptureTargetScope::Auto,chosen.title);
    if(const auto window=visible(chosen.title,chosen.path,true))chosen.hwnd=window->hwnd;
    apply(value,chosen);
}
void CaptureTargetController::sync() {
    const auto value=bindings();auto chosen=foreground(value);
    if(!chosen) {
        Selection fallback;fallback.title=resolve();if(fallback.title.isEmpty())fallback.title=lookup();
        fallback.path=pathForTitle(value,FeatureCaptureTargetScope::Auto,fallback.title);
        if(const auto window=visible(fallback.title,fallback.path,true))fallback.hwnd=window->hwnd;
        chosen=fallback;
    }
    apply(value,*chosen);
    if(chosen->foreground && m_host.remember && m_host.foreground)m_host.remember(chosen->hwnd,m_host.foreground().title);
    refreshMonitors();
}
QString CaptureTargetController::pathForTitle(const Bindings& value,FeatureCaptureTargetScope scope,const QString& title) {
    RunContextController::TargetBinding binding{value.subBinding.toStdWString(),value.mainProcessPath.toStdWString(),
        value.subProcessPath.toStdWString(),value.mainBinding.toStdWString()};
    return QString::fromStdWString(RunContextController::processPath(binding,scope,title.toStdWString()));
}
bool CaptureTargetController::refreshSession(FeatureRunSession& session,const Feature& feature) {
    const auto value=bindings(session.profileId);
    const auto scope=feature.captureTargetScope();
    const bool watch=session.runningMode==FeatureRunMode::Trigger && session.triggerPhase==TriggerSessionPhase::Monitoring;
    const auto candidate=choose(scope,watch && feature.triggerRunWithoutTargetForeground(),value.activeProfileId);
    if(candidate.title.isEmpty())return false;
    const QString locked=QString::fromStdWString(session.lockedCaptureTargetTitle);
    const QString lockedPath=session.lockedCaptureTargetProcessPath.empty()?pathForTitle(value,scope,locked)
        :QString::fromStdWString(session.lockedCaptureTargetProcessPath);
    const bool missing=!locked.isEmpty() && !visible(locked,lockedPath,true);
    SessionRunPolicyInput input;input.runningMode=session.runningMode;input.triggerPhase=session.triggerPhase;
    input.engineRunning=(session.engine && (session.engine->isRunning() || session.engine->hasLiveWorker())) || session.holdKeyTapLaneActive;
    if(scope!=FeatureCaptureTargetScope::Auto && !locked.isEmpty())return false;
    const bool differs=locked!=candidate.title || session.lockedCaptureTargetProcessPath!=candidate.path.toStdWString();
    if(!SessionRunPolicy::shouldAdoptCaptureTarget(input,locked.isEmpty(),missing,differs))return false;
    if(!differs)return false;
    session.lockedCaptureTargetTitle=candidate.title.toStdWString();
    session.lockedCaptureTargetProcessPath=candidate.path.toStdWString();
    if(session.sessionContext)session.sessionContext->setTargetWindowBindingForWorker(session.lockedCaptureTargetTitle,
        session.lockedCaptureTargetProcessPath);
    if(m_host.sessionChanged)m_host.sessionChanged(session);
    if(watch && m_host.monitorChanged)m_host.monitorChanged();
    return true;
}
std::wstring CaptureTargetController::sessionTitleForRun(FeatureRunSession& session,bool refresh) {
    if(!refresh && !session.lockedCaptureTargetTitle.empty())return session.lockedCaptureTargetTitle;
    const auto* feature=m_host.feature?m_host.feature(session):nullptr;
    return sessionTitle(session,feature,refresh).toStdWString();
}
QString CaptureTargetController::sessionTitle(FeatureRunSession& session,const Feature* feature,bool refresh) {
    if(feature && refresh)refreshSession(session,*feature);
    if(!session.lockedCaptureTargetTitle.empty())return QString::fromStdWString(session.lockedCaptureTargetTitle);
    if(!feature)return {};
    return resolve(feature->captureTargetScope(),feature->runMode()==FeatureRunMode::Trigger
        && feature->triggerRunWithoutTargetForeground(),session.profileId);
}
void CaptureTargetController::refreshMonitors() {
    if(!m_sessions || !m_host.feature)return;
    for(auto& entry:*m_sessions) {
        auto& session=entry.second;
        if(session.runningMode!=FeatureRunMode::Trigger || session.triggerPhase!=TriggerSessionPhase::Monitoring)continue;
        if(const auto* feature=m_host.feature(session))refreshSession(session,*feature);
    }
}
