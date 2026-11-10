#include "app/ShutdownCoordinator.h"
#include <QPointer>
#include <QScopeGuard>
#include <utility>

ShutdownCoordinator::ShutdownCoordinator(HostCallbacks host) : m_host(std::move(host)) {}

bool ShutdownCoordinator::persist() {
    m_error.clear();
    const auto host=m_host;const QPointer<ShutdownCoordinator> life(this);
    if (host.canBegin) {
        const bool allowed=host.canBegin();if(!life)return false;
        if(!allowed){m_error=QStringLiteral("현재 작업을 마친 뒤 다시 시도해 주세요.");return false;}
    }
    if(host.quiesce)host.quiesce();if(!life)return false;
    const auto save=[life](const auto& writer,const QString& error){
        const bool ok=writer && writer();if(!life)return false;if(!ok)life->m_error=error;return ok;
    };
    return save(host.saveProject,QStringLiteral("프로젝트 내용을 저장하지 못했어요."))
        && save(host.saveSettings,QStringLiteral("프로필 설정을 저장하지 못했어요."))
        && save(host.saveMemo,QStringLiteral("메모를 저장하지 못했어요."));
}

bool ShutdownCoordinator::checkpoint() {
    if (m_ready) return true;
    if (m_busy) return false;
    m_busy=true;const QPointer<ShutdownCoordinator> life(this);
    const auto guard=qScopeGuard([life]{if(life)life->m_busy=false;});
    return persist();
}

bool ShutdownCoordinator::prepare() {
    if (m_ready) return true;
    if (m_busy) return false;
    m_busy=true;const QPointer<ShutdownCoordinator> life(this);
    const auto guard=qScopeGuard([life]{if(life)life->m_busy=false;});
    if (!persist()) return false;
    if(!life)return false;
    const auto finalize=m_host.finalize;if(finalize)finalize();if(!life)return false;
    m_ready = true;
    return true;
}

bool ShutdownCoordinator::handoff(const std::function<bool()>& launch) {
    if(m_busy || m_ready || !launch)return false;
    m_busy=true;const QPointer<ShutdownCoordinator> life(this);const auto start=launch;
    const auto guard=qScopeGuard([life]{if(life)life->m_busy=false;});
    if(!persist() || !life)return false;
    const bool launched=start();if(!life)return false;
    if(!launched){m_error=QStringLiteral("관리자 권한으로 다시 시작하지 못했어요. 현재 창에서 계속 사용할 수 있어요.");return false;}
    const auto finalize=m_host.finalize;if(finalize)finalize();if(!life)return false;
    m_ready=true;return true;
}
