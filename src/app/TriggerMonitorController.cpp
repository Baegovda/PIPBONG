#include "app/TriggerMonitorController.h"
#include "app/ProfileManager.h"
#include "app/FeatureRunSession.h"
#include "model/Project.h"
#include "model/Feature.h"
#include "core/workflow/WorkflowRunner.h"
#include <QCoreApplication>
#include <QPointer>
#include <QTimer>

namespace {
bool eligible(const Feature* feature) {
    return feature && feature->enabled() && feature->runMode()==FeatureRunMode::Trigger
        && WorkflowRunner::firstImageFindBlockIndex(feature->workflow())>=0;
}
struct Finally {std::function<void()> done;~Finally(){done();}};
QString saveWarning(){return QCoreApplication::translate("MainWindow","트리거 감시 상태를 저장하지 못했어요. 저장 위치와 파일 권한을 확인해 주세요.");}
}

TriggerMonitorController::TriggerMonitorController(ProfileManager& profiles,QObject* parent)
    :QObject(parent),m_profiles(profiles),m_restore(new QTimer(this)),m_ensure(new QTimer(this)) {
    m_restore->setSingleShot(true);m_ensure->setSingleShot(true);
    connect(m_restore,&QTimer::timeout,this,[this]{restore();});
    connect(m_ensure,&QTimer::timeout,this,[this]{ensure();});
}
void TriggerMonitorController::scheduleRestore(){if(!m_stopped && !m_restore->isActive())m_restore->start(0);}
void TriggerMonitorController::scheduleEnsure(){if(!m_stopped && !m_ensure->isActive())m_ensure->start(0);}
void TriggerMonitorController::stop(){m_stopped=true;m_restore->stop();m_ensure->stop();}

bool TriggerMonitorController::current(const Workspace& source,const Host& host) const {
    if(m_stopped || !m_profiles.storageReady() || !host.workspace || !source.project)return false;
    const auto now=host.workspace();
    return !now.blocked && now.project==source.project && now.owner==source.owner
        && now.generation==source.generation && m_profiles.activeProfileId()==source.owner;
}

bool TriggerMonitorController::persist(const QString& id,bool armed) {
    if(m_stopped || m_suppressed || id.isEmpty())return true;
    const auto host=m_host;
    const auto* session=host.session?host.session(id.toStdString()):nullptr;
    const auto owner=session && !session->profileId.isEmpty()?session->profileId:m_profiles.activeProfileId();
    if(!m_profiles.profileById(owner))return true;
    const bool saved=m_profiles.updateTriggerArmedFeature(owner,id,armed);
    if(!saved && host.warning)host.warning(saveWarning());
    return saved;
}
bool TriggerMonitorController::restore(){return filter(true);}
bool TriggerMonitorController::prune(){return filter(false);}

bool TriggerMonitorController::filter(bool restoring) {
    if(m_stopped || m_busy || !m_profiles.storageReady())return false;
    const Host host=m_host;const QPointer<TriggerMonitorController> life(this);
    const auto source=host.workspace?host.workspace():Workspace{};
    if(!current(source,host))return false;
    const bool previous=m_suppressed;m_busy=true;m_suppressed=true;
    Finally guard{[life,previous]{if(life){
        life->m_suppressed=previous;life->m_busy=false;
        const bool deferred=life->m_ensureDeferred;life->m_ensureDeferred=false;
        if(deferred)life->scheduleEnsure();
    }}};
    if(restoring && host.prepare)host.prepare();
    if(!life || !current(source,host))return false;
    const auto armed=m_profiles.triggerArmedFeatureIds(source.owner);
    if(armed.isEmpty())return true;
    QStringList kept;
    for(const auto& id:armed) {
        if(!life || !current(source,host))return false;
        if(m_profiles.triggerArmedFeatureIds(source.owner)!=armed)return false;
        const auto* selected=source.project->featureById(id.toStdString());
        if(!eligible(selected)) {
            if(!restoring && host.session && host.stop) {
                const auto* session=host.session(id.toStdString());
                if(session && (session->profileId.isEmpty() || session->profileId==source.owner)
                    && host.running && host.running(id.toStdString()))host.stop(id);
            }
            continue;
        }
        kept.append(id);
        if(!restoring)continue;
        const auto* session=host.session?host.session(id.toStdString()):nullptr;
        // Another profile owns the shared ID: never tear down its session to restore this one.
        if(session && !session->profileId.isEmpty() && session->profileId!=source.owner)continue;
        if(session && host.active && host.active(*session))continue;
        if(host.running && host.running(id.toStdString()))continue;
        if(host.start)host.start(id);
    }
    if(!life || !current(source,host))return false;
    // Callback-driven persisted changes take precedence over this older snapshot.
    if(m_profiles.triggerArmedFeatureIds(source.owner)!=armed)return false;
    for(auto entry=kept.begin();entry!=kept.end();) {
        if(!eligible(source.project->featureById(entry->toStdString())))entry=kept.erase(entry);
        else ++entry;
    }
    if(kept!=armed && !m_profiles.setTriggerArmedFeatureIds(source.owner,kept)) {
        if(host.warning)host.warning(saveWarning());return false;
    }
    if(restoring && host.resume)host.resume();
    return true;
}

void TriggerMonitorController::ensure() {
    if(m_stopped || !m_profiles.storageReady())return;
    if(m_busy){m_ensureDeferred=true;return;}
    const Host host=m_host;const QPointer<TriggerMonitorController> life(this);
    if(!host.workspace || host.workspace().blocked || !host.sessions || !host.session || !host.feature)return;
    // Snapshot IDs so a callback may safely retire/insert registry entries.
    const auto ids=host.sessions();
    for(const auto& id:ids) {
        if(!life || m_stopped || host.workspace().blocked)return;
        auto* session=host.session(id);
        if(!session || session->runningMode!=FeatureRunMode::Trigger
            || session->triggerPhase!=TriggerSessionPhase::Monitoring || session->waitingForScopedTargetForeground || !SessionRunPolicy::allowsPreparedLaunch(session->awaitingFirstTemplateRoi)
            || session->userStopRequested || !session->repeatSession)continue;
        auto* selected=host.feature(*session);
        if(!selected || !host.mayContinue || !host.mayContinue(*session,selected))continue;
        if(session->engine && session->engine->isRunning())continue;
        const auto* before=session;const auto context=session->sessionContext;const auto owner=session->profileId;
        if(!session->engine && host.ensureEngine)host.ensureEngine(*session);
        if(!life || m_stopped || host.workspace().blocked)return;
        session=host.session(id);
        if(!session || session!=before || session->sessionContext!=context || session->profileId!=owner
            || session->runningMode!=FeatureRunMode::Trigger || session->triggerPhase!=TriggerSessionPhase::Monitoring
            || session->userStopRequested || !session->repeatSession || session->waitingForScopedTargetForeground || !SessionRunPolicy::allowsPreparedLaunch(session->awaitingFirstTemplateRoi))continue;
        selected=host.feature(*session);
        if(selected && session->engine && !session->engine->isRunning() && host.launch)host.launch(*session,selected);
    }
}
