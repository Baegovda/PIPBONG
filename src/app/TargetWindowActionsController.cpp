#include "TargetWindowActionsController.h"
#include "app/FeatureHotkeyGate.h"
#include "core/capture/WindowPicker.h"
#include "core/capture/ScreenCapture.h"
#include "ui/TargetWindowListPicker.h"
#include "ui/TargetWindowHighlightOverlay.h"
#include <QPointer>
#include <QTimer>
#include <QWidget>

namespace {
using Controller=TargetWindowActionsController;
Controller::Ports nativePorts(QWidget* parent) {
    Controller::Ports ports;
#ifdef _WIN32
    QPointer<QWidget> host(parent);
    ports.pick=[host](Controller::Role role,Controller::Completion done) {
        if(!host) { done(std::nullopt);return; }
        WindowPicker::startPick(host,[done=std::move(done)](const WindowPicker::Result& result) {
            if(!result.accepted || !result.hwnd) {done(std::nullopt);return;}
            done(Controller::Selection{result.hwnd,QString::fromStdWString(result.title),{},result.processId});
        },role);
    };
    ports.cancel=[] {WindowPicker::cancelPick();};
    ports.list=[host](const Controller::Snapshot& snapshot,Controller::Role role)->std::optional<Controller::Selection> {
        if(!host) return std::nullopt;
        TargetWindowListPickOptions options;
        options.role=role;options.mainBinding=snapshot.mainTitle;options.subBinding=snapshot.subTitle;
        options.preferredHwnd=snapshot.preferred;
        const auto selected=::pickTargetWindowFromList(host,options);
        if(!selected) return std::nullopt;
        return Controller::Selection{selected->hwnd,selected->title,selected->processPath,selected->processId};
    };
    ports.read=[](HWND hwnd)->std::optional<Controller::Selection> {
        ScreenCapture::TargetWindowInfo info;
        DWORD pid=0;
        if(!hwnd || !IsWindow(hwnd) || !GetWindowThreadProcessId(hwnd,&pid)
            || !ScreenCapture::queryWindowInfo(hwnd,info)) return std::nullopt;
        DWORD after=0;
        if(!GetWindowThreadProcessId(hwnd,&after) || after!=pid)return std::nullopt;
        return Controller::Selection{hwnd,QString::fromStdWString(info.title),QString::fromStdWString(info.processPath),pid};
    };
    ports.find=[](const QString& title,const QString& path) {
        return ScreenCapture::findVisibleWindowMatchingTitle(title.toStdWString(),path.toStdWString());
    };
    ports.flash=[host](const Controller::Selection& selection,Controller::Role role,bool wave) {
        if(!host) return false;
        return wave ? TargetWindowHighlightOverlay::flashSelectionWaveForHwnd(selection.hwnd,host,role)
                    : TargetWindowHighlightOverlay::flashForHwnd(selection.hwnd,host,role);
    };
#endif
    return ports;
}
}
TargetWindowActionsController::TargetWindowActionsController(QWidget* parent)
    : TargetWindowActionsController(nativePorts(parent),parent) {}
TargetWindowActionsController::TargetWindowActionsController(Ports ports,QObject* parent)
    : QObject(parent),m_ports(std::move(ports)) {}
TargetWindowActionsController::~TargetWindowActionsController() {cancel();}
void TargetWindowActionsController::status(const QString& text) const {if(m_host.status)m_host.status(text);}
void TargetWindowActionsController::warning(const QString& text) const {if(m_host.warning)m_host.warning(text);}
QString TargetWindowActionsController::blocked(bool changing) const {
    if(!m_host.snapshot) return tr("프로젝트를 먼저 열어 주세요.");
    if(m_host.snapshot().globalDefault) return tr("전역 기본 프로필은 타겟을 지정할 수 없습니다.");
    return m_host.blockedReason?m_host.blockedReason(changing):QString();
}
bool TargetWindowActionsController::sameDocument(const Snapshot& source) const {
    if(!m_host.snapshot) return false;
    const auto current=m_host.snapshot();
    return !current.globalDefault && current.profileId==source.profileId && current.generation==source.generation;
}
bool TargetWindowActionsController::sameProcess(const Selection& expected,const Selection& actual) {
    return expected.hwnd==actual.hwnd && (!expected.processId || expected.processId==actual.processId)
        && (expected.processPath.isEmpty() || expected.processPath.compare(actual.processPath,Qt::CaseInsensitive)==0);
}
void TargetWindowActionsController::cancel() {
    const bool active=m_pending;
    m_pending=false;++m_sequence;m_gate.reset();
    if(active && m_ports.cancel)m_ports.cancel();
    if(active && m_host.persistent)m_host.persistent({});
}
void TargetWindowActionsController::pick(Role role,bool fromList) {
    if(m_pending)cancel();
    const auto reason=blocked(true);
    if(!reason.isEmpty()) {status(reason);return;}
    if((fromList && !m_ports.list) || (!fromList && !m_ports.pick)) {
        warning(tr("타겟 지정은 Windows에서만 지원됩니다."));return;
    }
    const auto source=m_host.snapshot();const auto sequence=++m_sequence;
    m_pending=true;m_gate=std::make_unique<FeatureHotkeyGateScope>();
    QPointer<TargetWindowActionsController> self(this);
    if(fromList) {
        const auto list=m_ports.list;
        const auto selected=list(source,role);
        if(self)self->finish(sequence,source,role,true,selected);
    } else {
        if(m_host.persistent)m_host.persistent(role==Role::Sub?tr("서브 창을 클릭하세요. 우클릭 또는 Esc로 취소"):tr("메인 창을 클릭하세요. 우클릭 또는 Esc로 취소"));
        const auto pick=m_ports.pick;
        pick(role,[self,sequence,source,role](std::optional<Selection> selected) {
            if(self)self->finish(sequence,source,role,false,std::move(selected));
        });
    }
}
void TargetWindowActionsController::finish(quint64 sequence,Snapshot source,Role role,bool fromList,
                                          std::optional<Selection> selected) {
    if(!m_pending || sequence!=m_sequence)return;
    m_pending=false;m_gate.reset();
    if(m_host.persistent)m_host.persistent({});
    if(!selected) {if(!fromList)status(tr("타겟 지정이 취소되었습니다."));return;}
    auto reason=blocked(true);
    if(!reason.isEmpty()) {status(reason);return;}
    if(!sameDocument(source)) {warning(tr("프로필이나 프로젝트가 바뀌었어요. 타겟을 다시 선택해 주세요."));return;}
    const auto live=m_ports.read?m_ports.read(selected->hwnd):std::nullopt;
    if(!live || !sameProcess(*selected,*live)) {warning(tr("선택한 창이 닫혔거나 다른 프로그램으로 바뀌었어요. 다시 선택해 주세요."));return;}
    QPointer<TargetWindowActionsController> self(this);
    const auto checkpoint=m_host.checkpoint;
    const bool saved=!checkpoint || checkpoint();
    if(!self || sequence!=m_sequence)return;
    if(!saved) {warning(tr("현재 내용을 보관하지 못해 타겟 지정을 중단했어요."));return;}
    // Checkpointing can process events while copying files. Revalidate both document and HWND.
    reason=blocked(true);
    const auto checked=m_ports.read?m_ports.read(live->hwnd):std::nullopt;
    if(!reason.isEmpty()) {status(reason);return;}
    if(!sameDocument(source) || !checked || !sameProcess(*live,*checked)) {
        warning(tr("선택 중 프로젝트나 창이 바뀌었어요. 타겟을 다시 선택해 주세요."));return;
    }
    Selection value=*checked;
    if(value.title.trimmed().isEmpty())value.title=selected->title;
    value.title=value.title.trimmed();
    if(value.title.isEmpty()) {warning(tr("창 제목을 읽을 수 없어요. 다른 창을 선택해 주세요."));return;}
    if(!m_host.commit) {warning(tr("타겟 저장 기능을 사용할 수 없어요."));return;}
    const auto error=m_host.commit(source,role,value);
    if(!error.isEmpty()) {warning(error);return;}
    if(m_host.committed)m_host.committed(value,role);
    status(role==Role::Sub?tr("서브 창을 지정했습니다."):tr("메인 창을 지정했습니다."));
    const auto installed=m_host.snapshot();
    QTimer::singleShot(0,this,[this,installed,value,role] {
        if(!sameDocument(installed) || !blocked(false).isEmpty())return;
        const auto current=m_host.snapshot();
        const auto title=role==Role::Sub?current.subTitle:current.mainTitle;
        if(title!=value.title)return;
        const auto live=m_ports.read?m_ports.read(value.hwnd):std::nullopt;
        if(live && sameProcess(value,*live) && m_ports.flash)m_ports.flash(*live,role,true);
    });
}
void TargetWindowActionsController::show(Role role) {
    const auto reason=blocked(false);
    if(!reason.isEmpty()) {status(reason);return;}
    const auto snapshot=m_host.snapshot();
    const auto& title=role==Role::Sub?snapshot.subTitle:snapshot.mainTitle;
    const auto& path=role==Role::Sub?snapshot.subProcessPath:snapshot.mainProcessPath;
    if(title.isEmpty()) {status(role==Role::Sub?tr("서브 창이 지정되지 않았습니다. 먼저 지정 또는 서브 목록에서 선택하세요."):tr("메인 창이 지정되지 않았습니다."));return;}
    const auto hwnd=m_ports.find?m_ports.find(title,path):nullptr;
    const auto live=m_ports.read?m_ports.read(hwnd):std::nullopt;
    if(!live || (!path.isEmpty() && path.compare(live->processPath,Qt::CaseInsensitive)!=0)) {
        warning(tr("연결된 창을 찾을 수 없어요. 해당 프로그램이 실행 중인지 확인해 주세요."));return;
    }
    if(m_ports.flash && m_ports.flash(*live,role,false))status(role==Role::Sub?tr("서브 창을 표시했습니다."):tr("메인 창을 표시했습니다."));
}
