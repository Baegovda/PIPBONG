#include "TargetCenterPinController.h"
#include "app/TargetWindowCenterPin.h"
#include "core/capture/ScreenCapture.h"
#include <QPointer>
#include <QTimer>
#include <vector>

namespace {
using Controller=TargetCenterPinController;
Controller::Ports nativePorts() {
    Controller::Ports ports;
    // Passive startup/offscreen QA must never reposition an existing desktop window.
    if(qEnvironmentVariableIntValue("PIPBONG_SELF_TEST_UI")==1 || qEnvironmentVariableIntValue("PIPBONG_SMOKE_TEST")==1)return ports;
#ifdef _WIN32
    ports.resolve=[](const QString& title,const QString& path)->std::optional<Controller::Window> {
        const HWND hwnd=ScreenCapture::findVisibleWindowMatchingTitle(title.toStdWString(),path.toStdWString());
        ScreenCapture::TargetWindowInfo info;DWORD before=0,after=0;
        if(!hwnd || !IsWindow(hwnd) || !GetWindowThreadProcessId(hwnd,&before)
            || !ScreenCapture::queryWindowInfo(hwnd,info) || !GetWindowThreadProcessId(hwnd,&after)
            || before!=after)return std::nullopt;
        return Controller::Window{hwnd,QString::fromStdWString(info.processPath),before,info.visible,info.minimized};
    };
    ports.move=[](const Controller::Window& window,bool force) {
        DWORD pid=0;
        if(!window.hwnd || !GetWindowThreadProcessId(window.hwnd,&pid) || pid!=window.processId)return false;
        return TargetWindowCenterPin::syncWindow(window.hwnd,force);
    };
#endif
    return ports;
}
struct OnExit {std::function<void()> cleanup;~OnExit(){cleanup();}};
}
TargetCenterPinController::TargetCenterPinController(QObject* parent)
    : TargetCenterPinController(nativePorts(),parent) {}
TargetCenterPinController::TargetCenterPinController(Ports ports,QObject* parent)
    : QObject(parent),m_ports(std::move(ports)),m_periodic(new QTimer(this)),m_queued(new QTimer(this)) {
    m_periodic->setInterval(200);m_queued->setSingleShot(true);
    connect(m_periodic,&QTimer::timeout,this,&TargetCenterPinController::tick);
    connect(m_queued,&QTimer::timeout,this,&TargetCenterPinController::tick);
}
bool TargetCenterPinController::sameScope(const Snapshot& a,const Snapshot& b) {
    return a.profileId==b.profileId && a.generation==b.generation && a.globalDefault==b.globalDefault
        && a.main==b.main && a.sub==b.sub && a.mainTitle==b.mainTitle && a.subTitle==b.subTitle
        && a.mainPath==b.mainPath && a.subPath==b.subPath;
}
void TargetCenterPinController::remember(const Snapshot& state) {
    if(state.profileId!=m_profileId || state.generation!=m_generation)m_forceMask=0;
    m_profileId=state.profileId;m_generation=state.generation;
    m_forceMask&=(state.main?1u:0u)|(state.sub?2u:0u);
}
void TargetCenterPinController::stop() {m_periodic->stop();m_queued->stop();m_forceMask=0;}
void TargetCenterPinController::reconcile() {
    if(!m_host.snapshot) {stop();return;}
    const auto state=m_host.snapshot();remember(state);
    if(m_host.view)m_host.view(!state.globalDefault && state.main,!state.globalDefault && state.sub);
    if(state.globalDefault || (!state.main && !state.sub) || !m_ports.resolve || !m_ports.move) {stop();return;}
    if(!m_periodic->isActive())m_periodic->start();
    if(!m_queued->isActive())m_queued->start(0);
}
void TargetCenterPinController::toggle(Role role,bool checked) {
    if(!m_host.snapshot)return;
    const auto before=m_host.snapshot();
    if(before.globalDefault || before.blocked) {
        reconcile();if(m_host.status)m_host.status(tr("현재 편집이나 저장 작업을 마친 뒤 중앙 고정을 설정해 주세요."));return;
    }
    const bool main=role==Role::Main?checked:before.main,sub=role==Role::Sub?checked:before.sub;
    if(main==before.main && sub==before.sub) {reconcile();return;}
    QPointer<TargetCenterPinController> self(this);
    const auto save=m_host.save;
    const auto error=save?save(before,main,sub):tr("중앙 고정 설정을 저장할 수 없어요.");
    if(!self)return;
    reconcile();
    if(!error.isEmpty()) {if(m_host.warning)m_host.warning(error);return;}
    const auto after=m_host.snapshot();
    if(checked && after.profileId==before.profileId && after.generation==before.generation) {
        m_forceMask|=role==Role::Main?1u:2u;
        if(!after.movementAllowed && m_host.status)m_host.status(tr("현재 기능 실행이 끝나면 중앙 고정이 적용돼요."));
    }
}
void TargetCenterPinController::tick() {
    if(m_ticking || !m_host.snapshot)return;
    m_queued->stop();
    const auto state=m_host.snapshot();remember(state);
    if(state.globalDefault || (!state.main && !state.sub)) {stop();return;}
    if(state.blocked || !state.movementAllowed || !m_ports.resolve || !m_ports.move)return;
    m_ticking=true;
    QPointer<TargetCenterPinController> self(this);
    OnExit guard{[self]{if(self)self->m_ticking=false;}};
    const unsigned force=std::exchange(m_forceMask,0u);
    struct Plan {Window window;bool force=false;};std::vector<Plan> plans;
    for(unsigned role=0;role<2;++role) {
        if(role==0?!state.main:!state.sub)continue;
        const auto& title=role==0?state.mainTitle:state.subTitle;
        const auto& path=role==0?state.mainPath:state.subPath;
        if(title.isEmpty())continue;
        const auto resolve=m_ports.resolve;
        const auto window=resolve(title,path);
        if(!self)return;
        if(!window || !window->hwnd || !window->visible || window->minimized
            || (!path.isEmpty() && path.compare(window->processPath,Qt::CaseInsensitive)!=0))continue;
        bool duplicate=false;
        for(auto& plan:plans)if(plan.window.hwnd==window->hwnd) {plan.force|=(force&(1u<<role))!=0;duplicate=true;break;}
        if(!duplicate)plans.push_back({*window,(force&(1u<<role))!=0});
    }
    bool moved=false;
    for(const auto& plan:plans) {
        const auto current=m_host.snapshot();
        if(!sameScope(state,current) || current.blocked || !current.movementAllowed)break;
        const auto move=m_ports.move;
        moved=move(plan.window,plan.force)||moved;
        if(!self)return;
    }
    if(moved && m_host.changed)m_host.changed();
}
