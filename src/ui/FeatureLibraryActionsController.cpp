#include "ui/FeatureLibraryActionsController.h"
#include "app/ProfileManager.h"
#include "app/ProjectWorkspaceController.h"
#include "app/FeatureHotkeyGate.h"
#include "ui/FeatureListPanel.h"
#include "ui/FeatureLibraryDialog.h"
#include "ui/FeatureDragMime.h"
#include "storage/JsonSerializer.h"
#include <QInputDialog>
#include <QMessageBox>
#include <QLineEdit>
#include <QMimeData>
#include <QWidget>
#include <algorithm>

using C=FeatureLibraryActionsController;
struct C::Ticket {
    QPointer<C> life;QPointer<QWidget> owner;QPointer<ProjectWorkspaceController> workspace;
    QPointer<FeatureBatchController> batches;QPointer<FeatureListPanel> view;
    Host host;Ports ports;std::shared_ptr<Project> project;
    QString profile,profileName,path,directory;quint64 generation=0,libraryRevision=0;int insertIndex=-1;
    bool current(bool library=true,bool ready=true)const {
        if(!life || !owner || !workspace || !batches || !view)return false;
        const auto& state=workspace->state();
        return state.project==project && state.profileId==profile && state.path==path && state.directory==directory
            && workspace->generation()==generation && life->m_profiles.activeProfileId()==profile
            && (!library || life->m_library.revision()==libraryRevision) && (!ready || !host.ready || host.ready())
            && life && owner && workspace && batches && view;
    }
    void status(const QString& text,int ms=3000)const{if(life && owner && host.status)host.status(text,ms);}
    FeatureBatchController::Result run(FeatureBatchController::Request request)const {
        if(!request.validate){const auto source=*this;request.validate=[source]{return source.current();};}
        return batches->run(request);
    }
};
struct C::Action {
    QPointer<C> life;bool accepted=false;
    explicit Action(C* c):life(c){if(!c->m_busy){c->m_busy=true;accepted=true;}}
    ~Action(){if(life && accepted)life->m_busy=false;}
};
C::Ticket C::ticket()const {
    Ticket t;t.life=const_cast<C*>(this);t.owner=m_owner;t.workspace=m_workspace;t.batches=m_batches;t.view=m_view;t.host=m_host;t.ports=m_ports;
    if(!t.workspace || !t.view)return t;
    const auto& state=t.workspace->state();t.project=state.project;t.profile=state.profileId;t.path=state.path;t.directory=state.directory;
    t.generation=t.workspace->generation();t.libraryRevision=m_library.revision();
    if(const auto* p=m_profiles.activeProfile())t.profileName=p->name;
    t.insertIndex=t.view->selectedIndex()>=0?t.view->selectedIndex()+1:t.project?int(t.project->features().size()):-1;
    return t;
}
C::FeatureLibraryActionsController(QWidget& owner,ProfileManager& profiles,FeatureLibraryManager& library,
    ProjectWorkspaceController& workspace,FeatureBatchController& batches,FeatureListPanel& view,Host host)
    :QObject(&owner),m_owner(&owner),m_profiles(profiles),m_library(library),m_workspace(&workspace),m_batches(&batches),m_view(&view),m_host(std::move(host)) {
    m_ports.name=[](QWidget* parent,const QString& suggested)->std::optional<QString>{
        QPointer<QInputDialog> dialog=new QInputDialog(parent);dialog->setWindowTitle(tr("라이브러리에 저장"));
        dialog->setLabelText(tr("라이브러리 이름"));dialog->setTextValue(suggested);const int result=dialog->exec();
        if(!dialog)return {};const auto name=dialog->textValue();delete dialog;return result==QDialog::Accepted?std::optional<QString>(name):std::nullopt;
    };
    m_ports.choose=[](QWidget* parent,const std::vector<FeatureLibraryManager::Entry>& entries){
        std::vector<FeatureLibraryDialog::EntryUi> rows;for(const auto& e:entries)rows.push_back({e.id,e.name,e.templateCount});
        QPointer<FeatureLibraryDialog> dialog=new FeatureLibraryDialog(rows,parent);const int result=dialog->exec();
        if(!dialog)return QString();const auto id=dialog->selectedEntryId();delete dialog;return result==QDialog::Accepted?id:QString();
    };
    m_ports.confirm=[](QWidget* parent,const QString& title,const QString& text){
        QPointer<QMessageBox> dialog=new QMessageBox(QMessageBox::Question,title,text,QMessageBox::Yes|QMessageBox::No,parent);
        dialog->setTextFormat(Qt::PlainText);
        dialog->setDefaultButton(QMessageBox::No);const int result=dialog->exec();if(dialog)delete dialog;return result==QMessageBox::Yes;
    };
    m_ports.notice=[](QWidget* parent,const QString& title,const QString& text,bool warning){
        QPointer<QMessageBox> dialog=new QMessageBox(warning?QMessageBox::Warning:QMessageBox::Information,title,text,QMessageBox::Ok,parent);
        dialog->setTextFormat(Qt::PlainText);
        dialog->exec();if(dialog)delete dialog;
    };
}
void C::cancelled(const Ticket& t){t.status(tr("문서나 라이브러리가 바뀌어 작업을 취소했어요. 다시 선택해 주세요."));}
void C::present(const Ticket& t,const FeatureBatchController::Result& result,const QString& targetName) {
    if(!t.life || !t.owner || !result.requested)return;
    using Kind=FeatureBatchController::Kind;QString action;
    switch(result.kind){case Kind::Import:action=tr("라이브러리 가져오기");break;case Kind::Save:action=tr("라이브러리 저장");break;
        case Kind::Copy:action=tr("기능 복사");break;case Kind::Remove:action=tr("라이브러리 삭제");break;}
    const int completed=result.completed.size();
    const auto context=!targetName.isEmpty()?targetName:result.kind==Kind::Import?t.profileName:QString();
    if(!result.failed.isEmpty() || result.cancelled){
        auto message=tr("%1: 요청 %2개 중 %3개 처리, %4개 실패, %5개 취소됐어요.")
            .arg(action).arg(result.requested).arg(completed).arg(result.failed.size()).arg(std::max(0,result.requested-completed-int(result.failed.size())));
        if(!context.isEmpty())message+=tr(" · 프로필 '%1'").arg(context);
        t.status(message,5000);if(!t.life || !t.owner)return;
        if(t.host.warning)t.host.warning(message);if(!t.life || !t.owner)return;
        if(!result.failed.isEmpty() && t.ports.notice){FeatureHotkeyGateScope gate;t.ports.notice(t.owner,action,message,true);}return;
    }
    const auto name=result.names.isEmpty()?result.completed.value(0):result.names.first();
    auto message=completed==1?tr("%1 완료: %2").arg(action,name):tr("%1: %2개 완료").arg(action).arg(completed);
    if(!context.isEmpty())message+=tr(" · 프로필 '%1'").arg(context);t.status(message,2500);
}
void C::save(const QString& id) {
    Action action(this);if(!action.accepted)return;const auto t=ticket();if(!t.current() || !t.project || id.isEmpty())return;
    const auto* feature=t.project->featureById(id.toStdString());if(!feature)return;
    const auto signature=JsonSerializer::featureToJson(*feature);const auto suggested=QString::fromStdString(feature->name());
    std::optional<QString> name;{FeatureHotkeyGateScope gate;if(t.ports.name)name=t.ports.name(t.owner,suggested);}
    if(!name)return;if(!t.current()){cancelled(t);return;}
    feature=t.project->featureById(id.toStdString());if(!feature || JsonSerializer::featureToJson(*feature)!=signature){cancelled(t);return;}
    FeatureBatchController::Request request;request.kind=FeatureBatchController::Kind::Save;request.ids={id};request.sourceProfile=t.profile;
    request.name=*name;request.undoReason="library-save-entry";
    request.validate=[t,id,signature]{if(!t.current())return false;const auto* f=t.project->featureById(id.toStdString());return f && JsonSerializer::featureToJson(*f)==signature;};
    const auto result=t.run(std::move(request));present(t,result);
}
bool C::executeImport(const Ticket& t,const QStringList& ids) {
    if(!t.current() || !t.project || ids.isEmpty())return false;
    FeatureBatchController::Request request;request.ids=ids;request.targetProfile=t.profile;request.insertIndex=t.insertIndex;request.undoReason="feature-import-library";
    const auto result=t.run(std::move(request));present(t,result);return !result.completed.isEmpty();
}
bool C::importEntries(const QStringList& ids){Action action(this);return action.accepted && executeImport(ticket(),ids);}
void C::chooseImport() {
    Action action(this);if(!action.accepted)return;const auto t=ticket();if(!t.current())return;
    const auto entries=m_library.listEntries();
    if(entries.empty()){if(t.ports.notice){FeatureHotkeyGateScope gate;t.ports.notice(t.owner,tr("라이브러리"),tr("아직 저장된 기능 라이브러리가 없습니다. 먼저 기능을 '라이브러리에 저장'으로 저장해 주세요."),false);}return;}
    QString id;{FeatureHotkeyGateScope gate;if(t.ports.choose)id=t.ports.choose(t.owner,entries);}
    if(id.isEmpty())return;if(!t.current()){cancelled(t);return;}
    if(std::none_of(entries.begin(),entries.end(),[&](const auto& e){return e.id==id;})){cancelled(t);return;}
    executeImport(t,{id});
}
void C::remove(const QStringList& requested) {
    Action action(this);if(!action.accepted)return;const auto t=ticket();if(!t.current())return;
    QStringList ids;for(const auto& id:requested)if(!id.isEmpty() && !ids.contains(id))ids<<id;if(ids.isEmpty())return;
    QString name=ids.first();for(const auto& entry:m_library.listEntries())if(entry.id==name){name=entry.name;break;}
    const auto message=ids.size()>1?tr("선택한 라이브러리 항목 %1개를 삭제하시겠습니까?\n이미 가져온 기능에는 영향이 없습니다.").arg(ids.size())
        :tr("'%1' 항목을 라이브러리에서 삭제하시겠습니까?\n이미 가져온 기능에는 영향이 없습니다.").arg(name);
    bool accepted=false;{FeatureHotkeyGateScope gate;if(t.ports.confirm)accepted=t.ports.confirm(t.owner,tr("라이브러리에서 삭제"),message);}
    if(!accepted)return;if(!t.current()){cancelled(t);return;}
    FeatureBatchController::Request request;request.kind=FeatureBatchController::Kind::Remove;request.ids=ids;request.undoReason="library-remove-entries";
    const auto result=t.run(std::move(request));if(!t.life || !t.owner)return;
    if(t.host.previewId && result.completed.contains(t.host.previewId()) && t.host.preview)t.host.preview({},{});
    present(t,result);
}
void C::preview(const QString& id) {
    const auto epoch=++m_previewEpoch;
    const auto t=ticket();if(!t.current(false,false) || !t.host.preview)return;
    auto feature=id.isEmpty()?std::unique_ptr<Feature>():m_library.loadEntryFeature(id);if(!t.current(true,false))return;
    if(!id.isEmpty() && !feature){t.status(tr("라이브러리 항목을 미리볼 수 없습니다."),2500);if(!t.life)return;}
    if(!t.current(true,false) || t.life->m_previewEpoch!=epoch)return;
    const auto selected=feature?id:QString();auto lease=std::shared_ptr<Feature>(std::move(feature));
    t.host.preview(selected,std::move(lease));
}
void C::refreshPanel() {
    const auto t=ticket();if(!t.current(false,false))return;
    std::vector<FeatureListPanel::LibraryEntryUi> rows;for(const auto& e:m_library.listEntries())rows.push_back({e.id,e.name,e.templateCount});
    if(t.current(false,false))t.view->setLibraryEntries(rows);
}
void C::dropOnFeatureList(const QMimeData* mime,int index) {
    Action action(this);if(!action.accepted || !mime)return;const auto t=ticket();if(!t.current())return;
    const auto payload=FeatureDragMime::parse(mime);if(!payload.isValid() || payload.source!=FeatureDragMime::Source::Library)return;
    FeatureBatchController::Request request;request.ids=payload.allIds();request.targetProfile=t.profile;request.insertIndex=index;request.undoReason="feature-import-from-library";
    present(t,t.run(std::move(request)));
}
void C::dropOnLibrary(const QMimeData* mime) {
    Action action(this);if(!action.accepted || !mime)return;const auto t=ticket();if(!t.current())return;
    const auto payload=FeatureDragMime::parse(mime);if(!payload.isValid() || payload.source!=FeatureDragMime::Source::Profile)return;
    FeatureBatchController::Request request;request.kind=FeatureBatchController::Kind::Save;request.ids=payload.allIds();request.sourceProfile=payload.profileId;request.undoReason="feature-save-to-library";
    present(t,t.run(std::move(request)));
}
void C::dropOnProfile(const QString& id,const QMimeData* mime) {
    Action action(this);if(!action.accepted || !mime || id.isEmpty())return;const auto t=ticket();if(!t.current())return;
    const auto payload=FeatureDragMime::parse(mime);if(!payload.isValid())return;
    FeatureBatchController::Request request;request.ids=payload.allIds();request.targetProfile=id;
    QString name=id;if(const auto* p=m_profiles.profileById(id))name=p->name;
    if(payload.source==FeatureDragMime::Source::Library)request.undoReason="profile-import-library-feature";
    else{request.kind=FeatureBatchController::Kind::Copy;request.sourceProfile=payload.profileId;if(request.sourceProfile==id)return;request.undoReason="profile-copy-feature";}
    const auto result=t.run(std::move(request));present(t,result,name);
}
void C::presentOrder(FeatureBatchController::OrderResult result) {
    const auto t=ticket();using R=FeatureBatchController::OrderResult;
    if(result==R::Changed)t.status(tr("라이브러리 순서를 저장했습니다."),1500);
    else if(result==R::Failed)t.status(tr("순서를 저장하지 못해 이전 순서로 되돌렸어요."),5000);
}
void C::reorder(int from,int to) {
    Action action(this);if(!action.accepted){refreshPanel();return;}const auto t=ticket();if(!t.current()){refreshPanel();return;}
    const auto result=t.batches->reorderOne(from,to,[t]{return t.current();});if(t.life)presentOrder(result);
}
void C::reorder(const QList<int>& rows,int index) {
    Action action(this);if(!action.accepted){refreshPanel();return;}const auto t=ticket();if(!t.current()){refreshPanel();return;}
    const auto result=t.batches->reorder(rows,index,[t]{return t.current();});if(t.life)presentOrder(result);
}
