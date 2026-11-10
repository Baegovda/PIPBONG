#include "app/ProfileTransitionController.h"
#include "app/ProjectWorkspaceController.h"
#include "app/ProfileManager.h"
#include "model/Project.h"
#include "core/diagnostics/AppStutterProfiler.h"
#include <QTimer>

ProfileTransitionController::ProfileTransitionController(ProfileManager& profiles,ProjectWorkspaceController& workspace,QObject* parent)
    : QObject(parent),m_profiles(profiles),m_workspace(workspace) {}

void ProfileTransitionController::retryManual(int delayMs) {
    if(m_manualRetryScheduled || m_pendingManual.isEmpty()) return;
    m_manualRetryScheduled=true;
    QTimer::singleShot(delayMs,this,[this] {
        m_manualRetryScheduled=false;
        if(m_state.pipeline || m_pendingManual.isEmpty()) return;
        const auto id=m_pendingManual;m_pendingManual.clear();
        if(m_host.canBegin && !m_host.canBegin(false)) { m_pendingManual=id;retryManual(200);return; }
        start(id,false);
    });
}

void ProfileTransitionController::abort(bool resync) {
    ++m_generation;
    m_state={};
    if(m_host.lockUi) m_host.lockUi(false);
    if(resync && m_host.resyncSelection) m_host.resyncSelection();
    retryManual();
}

bool ProfileTransitionController::start(const QString& id,bool automatic) {
    if(id.isEmpty() || !m_profiles.profileById(id)) return false;
    if(m_state.pipeline) {
        if(automatic) { if(m_auto.defer) m_auto.defer(id); } else m_pendingManual=id;
        return false;
    }
    if(id==m_profiles.activeProfileId() && m_workspace.state().profileId==id) {
        if(!automatic)m_pendingManual.clear();
        return false;
    }
    if((m_host.canBegin && !m_host.canBegin(automatic)) || (m_host.dragActive && m_host.dragActive())) {
        if(automatic) { if(m_auto.defer) m_auto.defer(id); if(m_auto.schedule) m_auto.schedule(400); }
        else { m_pendingManual=id;retryManual(200); }
        return false;
    }
    if(!automatic) { m_pendingManual.clear();if(m_auto.clearDeferred) m_auto.clearDeferred(); }
    AppStutterOperationScope stutter("profile.switch",id+(automatic?QStringLiteral(" auto"):QString()));
    if(m_host.cancelDrags) m_host.cancelDrags();
    m_state={true,true,true};
    const auto generation=++m_generation;
    if(m_host.lockUi) m_host.lockUi(true);
    if(m_host.selectProfile) m_host.selectProfile(id);
    auto fail=[this](const QString& error) { abort();if(m_host.warning) m_host.warning(error);return false; };
    if(!m_workspace.checkpoint(true) || !m_workspace.saveSettings()) return fail(m_workspace.lastError());
    if(m_host.saveMemo && !m_host.saveMemo()) return fail(QStringLiteral("메모를 저장하지 못했어요."));
    auto staged=m_workspace.stageProfile(id);
    if(!staged) return fail(m_workspace.lastError());
    // All fallible saves and parsing finish before publishing a new active id.
    // No queued event can open an editor against the old model in this interval.
    if(!m_profiles.setActiveProfile(id)) return fail(QStringLiteral("프로필 선택을 저장하지 못했어요."));
    if(m_host.detach) m_host.detach();
    m_workspace.install(std::move(staged),true,false,ProjectWorkspaceController::InstallMode::ValidatedTransition);
    if(!automatic && m_auto.manualCommitted) m_auto.manualCommitted(id,m_profiles.isDefaultProfile(id));
    if(m_host.loaded) m_host.loaded(automatic);
    QTimer::singleShot(0,this,[this,automatic,generation] { complete(automatic,generation); });
    return true;
}

void ProfileTransitionController::complete(bool automatic,quint64 generation) {
    if(generation!=m_generation || !m_state.pipeline) return;
    AppStutterOperationScope stutter("profile.switch.complete",automatic?QStringLiteral("auto"):QStringLiteral("manual"));
    if(m_auto.committed) m_auto.committed();
    m_state={};
    if(m_host.lockUi) m_host.lockUi(false);
    if(!m_pendingManual.isEmpty() && m_pendingManual!=m_profiles.activeProfileId()) { retryManual();return; }
    m_pendingManual.clear();
    if(m_host.completed) m_host.completed(automatic);
    QTimer::singleShot(0,this,[this,automatic,generation] {
        if(generation!=m_generation) return;
        if(m_host.finishViews) m_host.finishViews(automatic);
        QTimer::singleShot(0,this,[this,automatic,generation] {
            if(generation==m_generation && m_host.restoreTriggersAndReconcile) m_host.restoreTriggersAndReconcile(automatic);
        });
        if(m_host.foregroundChanged) m_host.foregroundChanged();
        if(m_auto.hasDeferred && m_auto.hasDeferred() && m_auto.schedule) m_auto.schedule(400);
    });
}
