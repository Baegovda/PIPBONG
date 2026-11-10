#include "ui/TargetPanelController.h"
#include "ui/TargetWindowDetailPanel.h"
#include <QToolButton>
#include <QTimer>
#include <QFileInfo>

TargetPanelController::TargetPanelController(TargetWindowDetailPanel& panel,Controls controls,
    TargetWindowDetailsController& details,Snapshot snapshot,QObject* parent)
    :QObject(parent),m_panel(&panel),m_controls(std::move(controls)),m_details(details),m_snapshot(std::move(snapshot)),m_timer(new QTimer(this)) {
    m_timer->setInterval(250);connect(m_timer,&QTimer::timeout,this,&TargetPanelController::refresh);start();
    connect(&panel,&QObject::destroyed,this,[this]{stop();});
}
void TargetPanelController::start(){m_stopped=false;m_timer->start();}
void TargetPanelController::stop(){m_stopped=true;m_timer->stop();}
bool TargetPanelController::sameSource(const Request& a,const Request& b) {
    return a.profileId==b.profileId && a.revision==b.revision && a.globalDefault==b.globalDefault
        && a.captureOwnedBySession==b.captureOwnedBySession && a.allowIdleUpdates==b.allowIdleUpdates
        && a.deferProfileRefresh==b.deferProfileRefresh && a.bindings.mainTitle==b.bindings.mainTitle
        && a.bindings.subTitle==b.bindings.subTitle && a.bindings.mainProcessPath==b.bindings.mainProcessPath
        && a.bindings.subProcessPath==b.bindings.subProcessPath;
}
bool TargetPanelController::current(const Request& source)const {
    if(m_stopped || !m_panel || !m_snapshot)return false;
    QPointer<const TargetPanelController> life(this);const auto snapshot=m_snapshot;const auto now=snapshot();
    return life && !life->m_stopped && sameSource(source,now);
}
void TargetPanelController::applyControls(bool globalDefault) {
    const auto controls=m_controls;QPointer<TargetPanelController> life(this);
    const auto locked=tr("전역 기본 프로필은 타겟을 지정할 수 없습니다.");
    const auto apply=[&](QPointer<QToolButton> button,const QString& tooltip,bool supported=true){
        if(!life || !button)return;button->setEnabled(!globalDefault && supported);
        if(life && button)button->setToolTip(globalDefault?locked:tooltip);
    };
    apply(controls.pickMain,tr("클릭한 뒤 메인 창을 눌러 지정합니다. 마우스를 올리면 초록색 테두리가 표시됩니다. 우클릭 또는 Esc로 취소."));
    apply(controls.listMain,tr("메인 창 목록에서 선택합니다. 항목을 고르면 초록색 테두리 애니메이션이 표시됩니다."));
    apply(controls.pickSub,tr("클릭한 뒤 서브 창을 눌러 지정합니다. 마우스를 올리면 파란색 테두리가 표시됩니다. 우클릭 또는 Esc로 취소."));
    apply(controls.listSub,tr("서브 창 목록에서 선택합니다. 항목을 고르면 파란색 테두리 애니메이션이 표시됩니다."));
    apply(controls.showMain,tr("지정된 메인 창 테두리를 잠시 깜빡여 표시합니다(초록색)."));
    apply(controls.showSub,tr("지정된 서브 창 테두리를 잠시 깜빡여 표시합니다(파란색)."));
#ifdef _WIN32
    constexpr bool supportsPin=true;
#else
    constexpr bool supportsPin=false;
#endif
    apply(controls.pinMain,tr("메인 창을 드래그해도 현재 모니터 중앙으로 자동 복귀합니다. 트리거 감시 중에도 동작합니다."),supportsPin);
    apply(controls.pinSub,tr("서브 창을 드래그해도 현재 모니터 중앙으로 자동 복귀합니다. 트리거 감시 중에도 동작합니다."),supportsPin);
}
void TargetPanelController::refresh() {
    if(m_stopped || m_refreshing || !m_panel || !m_snapshot)return;
    m_refreshing=true;QPointer<TargetPanelController> life(this);
    struct Reset{QPointer<TargetPanelController> life;~Reset(){if(life)life->m_refreshing=false;}} reset{life};
    const auto snapshot=m_snapshot;const auto source=snapshot();if(!life)return;
    applyControls(source.globalDefault);if(!life || !current(source))return;
    const auto result=m_details.refresh(source);if(!life || !current(source))return;
    const auto panel=m_panel;if(!panel)return;
    using State=TargetWindowDetailsController::Result::State;
    switch(result.state){
    case State::GlobalDefault:panel->showGlobalDefaultProfile();break;
    case State::Unbound:panel->showMessage(tr("메인 창 또는 서브 창의 '지정' 버튼으로 타겟을 선택하세요."));break;
    case State::Stored:panel->showStoredTargetBinding(result.storedTitle,QFileInfo(result.storedProcessPath).fileName(),result.storedProcessPath,result.subTarget);break;
    case State::Live:panel->showDetails(result.data);break;
    case State::Unsupported:panel->showMessage(tr("타겟 정보는 Windows에서만 표시됩니다."));break;
    }
}
