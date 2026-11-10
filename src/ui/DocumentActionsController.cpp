#include "ui/DocumentActionsController.h"
#include "app/ProjectWorkspaceController.h"
#include "app/ProfileManager.h"
#include "app/ProfilePackageController.h"
#include "app/ProjectRecoveryController.h"
#include "app/FeatureHotkeyGate.h"
#include "core/diagnostics/CrashReporter.h"
#include "ui/diagnostics/CrashReportDialog.h"
#include "ui/AppHelpDialog.h"
#include "storage/ProjectPackage.h"
#include "model/Project.h"
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QFileDialog>
#include <QMenuBar>
#include <QMenu>
#include <QMessageBox>
#include <QSettings>
#include <QWidget>
#include <QTimer>
#include <QtConcurrent/QtConcurrent>

using Controller=DocumentActionsController;
struct Controller::Ticket {
    QPointer<Controller> life;QPointer<QWidget> owner;QPointer<ProjectWorkspaceController> workspace;
    QPointer<ProfilePackageController> packages;Host host;Ports ports;std::shared_ptr<Project> project;
    QString profile,path,directory,modelDirectory,name;quint64 generation=0;
    bool global=false,ready=false;
    bool current(bool pathMatches=true)const {
        if(!life || !owner || !workspace || !packages)return false;
        const auto& s=workspace->state();const auto* p=life->m_profiles.activeProfile();
        return p && p->id==profile && s.profileId==profile && s.project==project && (!pathMatches || s.path==path)
            && s.directory==modelDirectory && workspace->generation()==generation && p->name==name
            && (!host.directory || host.directory()==directory);
    }
    void status(const QString& message)const {if(life && host.status)host.status(message);}
};
struct Controller::Action {
    QPointer<Controller> life;bool accepted=false;
    explicit Action(Controller* value):life(value){if(!value->m_busy){value->m_busy=true;accepted=true;}}
    ~Action(){if(accepted && life)life->m_busy=false;}
};
Controller::Ticket Controller::ticket()const {
    Ticket t;t.life=const_cast<Controller*>(this);t.owner=m_owner;t.workspace=m_workspace;t.packages=m_packages;t.host=m_host;t.ports=m_ports;
    if(!t.owner || !t.workspace || !t.packages)return t;
    const auto& s=t.workspace->state();t.project=s.project;t.profile=s.profileId;t.path=s.path;t.modelDirectory=s.directory;
    t.directory=t.host.directory?t.host.directory():s.directory;t.generation=t.workspace->generation();
    if(const auto* p=m_profiles.activeProfile()){t.name=p->name;t.global=m_profiles.isDefaultProfile(p->id);}
    t.ready=!t.host.ready || t.host.ready();return t;
}
Controller::DocumentActionsController(QWidget& owner,ProfileManager& profiles,ProjectWorkspaceController& workspace,
    ProfilePackageController& packages,Host host):QObject(&owner),m_owner(&owner),m_profiles(profiles),m_workspace(&workspace),m_packages(&packages),m_host(std::move(host)) {
    m_ports.file=[](QWidget* parent,const QString& title,const QString& initial,const QString& filter,bool save){
        return save?QFileDialog::getSaveFileName(parent,title,initial,filter):QFileDialog::getOpenFileName(parent,title,initial,filter);};
    m_ports.error=[](QWidget* parent,const QString& title,const QString& message){QMessageBox::critical(parent,title,message);};
    m_ports.restore=ProjectRecoveryController::restore;
    m_ports.prepare=prepareProfileImport;
    m_importRetry=new QTimer(this);m_importRetry->setSingleShot(true);m_importRetry->setInterval(100);
    connect(m_importRetry,&QTimer::timeout,this,&Controller::completeImport);
    connect(&m_importWatcher,&QFutureWatcher<std::shared_ptr<PreparedProfileImport>>::finished,this,[this]{
        // Clearing a watcher also reports its default finished, resultless future.
        if(m_importWatcher.future().resultCount()==0)return;
        m_preparedImport=m_importWatcher.result();m_importWatcher.setFuture({});
        if(!m_preparedImport){m_preparedImport=std::make_shared<PreparedProfileImport>();m_preparedImport->error=tr("패키지를 준비하지 못했어요. 다시 시도해 주세요.");}
        completeImport();
    });
}
Controller::~DocumentActionsController(){
    if(m_importWatcher.future().resultCount()>0)m_preparedImport=m_importWatcher.result();
    m_importWatcher.setFuture({});retireImport();
}
void Controller::retireImport() {
    m_importRetry->stop();m_importSource.reset();m_importNotice=false;
    m_preparedImport.reset(); // Staging directory's deleter always runs off GUI.
}
void Controller::completeImport() {
    const auto source=m_importSource;const auto prepared=m_preparedImport;if(!source || !prepared)return;
    const auto t=*source;
    if(!t.current()){retireImport();m_busy=false;t.status(tr("프로젝트가 바뀌어 패키지 가져오기를 취소했어요."));return;}
    if(!prepared->ready){const auto message=prepared->error;retireImport();Action release(this);release.accepted=true;
        report(t,tr("패키지 가져오기"),message);return;}
    if(t.host.ready && !t.host.ready()) {
        m_importRetry->start();if(!m_importNotice){m_importNotice=true;t.status(tr("편집을 마치면 패키지 가져오기를 계속해요."));}return;
    }
    const bool saved=!t.host.checkpoint || t.host.checkpoint(true);
    if(!t.life)return;
    const bool preparedSource=saved && t.current() && (!t.host.prepareImport || t.host.prepareImport());
    if(!t.life)return;
    if(!preparedSource || !t.current()) {
        const auto message=t.workspace?t.workspace->lastError():QString();retireImport();Action release(this);release.accepted=true;
        if(t.current())report(t,tr("패키지 가져오기"),message.isEmpty()?tr("현재 프로필의 설정이나 메모를 저장하지 못했어요. 기존 프로필을 유지했어요."):message);
        return;
    }
    QString error;const auto id=m_profiles.commitImportedProfile(prepared,&error);
    retireImport();Action release(this);release.accepted=true;
    if(!t.life)return;
    if(id.isEmpty()){if(t.current())report(t,tr("패키지 가져오기"),error);return;}
    if(t.host.imported)t.host.imported();if(!t.life)return;
    if(t.host.log)t.host.log(tr("PIPBONG 패키지를 새 프로필로 가져왔습니다."),true);
}
QString Controller::selectFile(const Ticket& t,const QString& title,const QString& initial,const QString& filter,bool save) {
    if(!t.current() || !t.ports.file)return {};
    QString path;{FeatureHotkeyGateScope gate;path=t.ports.file(t.owner,title,initial,filter,save);}
    if(!t.current()){t.status(tr("프로젝트가 바뀌어 파일 작업을 취소했어요. 다시 선택해 주세요."));return {};}
    return path;
}
void Controller::report(const Ticket& t,const QString& title,const QString& message)const {
    if(t.life && t.owner && t.ports.error)t.ports.error(t.owner,title,message);
}
void Controller::newProject() {
    Action action(this);if(!action.accepted)return;const auto t=ticket();if(!t.ready || !t.current())return;
    if(t.host.checkpoint && !t.host.checkpoint(false)){if(t.current())report(t,tr("새 프로젝트"),t.workspace->lastError());return;}
    if(!t.current())return;
    const bool made=t.workspace->createEmpty();if(!t.life || !t.workspace)return;
    if(!made){report(t,tr("새 프로젝트"),t.workspace->lastError());return;}
    if(t.host.log)t.host.log(tr("새 프로젝트를 준비했습니다."),false);
}
void Controller::openProject() {
    Action action(this);if(!action.accepted)return;const auto t=ticket();if(!t.ready || !t.current())return;
    if(t.host.checkpoint && !t.host.checkpoint(false)){if(t.current())report(t,tr("프로젝트 열기"),t.workspace->lastError());return;}
    if(!t.current())return;
    const auto path=selectFile(t,tr("프로젝트 열기"),t.directory,tr("PIPBONG 프로젝트 (*.json)"),false);
    if(path.isEmpty() || !t.current())return;
    const bool loaded=t.workspace->load(path,false);if(t.life && t.workspace && !loaded)report(t,tr("프로젝트 열기"),t.workspace->lastError());
}
void Controller::saveProject() {
    Action action(this);if(!action.accepted)return;const auto t=ticket();if(!t.ready || !t.current())return;
    const bool saved=t.workspace->save(true);if(!t.life || !t.workspace)return;
    if(!saved){report(t,tr("프로젝트 저장"),t.workspace->lastError());return;}
    if(!t.current())return;
    if(t.host.log)t.host.log(tr("변경 내용을 저장했습니다."),true);
    if(t.current())t.status(tr("저장됨"));
}
void Controller::saveProjectAs() {
    Action action(this);if(!action.accepted)return;const auto t=ticket();if(!t.ready || !t.current())return;
    const auto path=selectFile(t,tr("다른 이름으로 저장"),QDir(t.directory).filePath("project.json"),tr("PIPBONG 프로젝트 (*.json)"),true);
    if(path.isEmpty() || !t.current())return;
    const bool saved=t.workspace->saveAs(path);if(!t.life || !t.workspace)return;
    if(!saved){report(t,tr("프로젝트 저장"),t.workspace->lastError());return;}
    // Successful Save As deliberately changes the path, invalidating the old ticket.
    if(t.current(false) && t.workspace->state().path==path)t.status(tr("저장됨"));
}
void Controller::importPackage() {
    Action action(this);if(!action.accepted)return;const auto t=ticket();if(!t.ready || !t.current())return;
    const auto path=selectFile(t,tr("PIPBONG 패키지 가져오기"),ProjectPackage::defaultExportDirectory(),
        tr("PIPBONG 패키지 (*%1)").arg(QString::fromLatin1(ProjectPackage::kExtension)),false);
    if(path.isEmpty() || !t.current())return;
    // Persist the outgoing document before the package importer changes selection.
    if(t.host.checkpoint && !t.host.checkpoint(true)){if(t.current())report(t,tr("패키지 가져오기"),t.workspace->lastError());return;}
    if(!t.current())return;
    if(t.host.prepareImport && !t.host.prepareImport()){if(t.current())report(t,tr("패키지 가져오기"),tr("현재 프로필의 설정이나 메모를 저장하지 못했어요. 기존 프로필을 유지했어요."));return;}
    if(!t.current())return;t.workspace->scheduleSeal();if(!t.current())return;
    const auto request=m_profiles.importRequest(path);const auto prepare=t.ports.prepare;if(!prepare)return;
    m_importSource=std::make_shared<Ticket>(t);action.accepted=false; // transfer busy ownership to completion
    m_importWatcher.setFuture(QtConcurrent::run([prepare,request]{return prepare(request);}));
    t.status(tr("패키지를 준비하고 있어요. 다른 작업도 계속할 수 있어요."));
}
void Controller::exportPackage() {
    Action action(this);if(!action.accepted)return;const auto t=ticket();if(!t.ready || !t.current())return;
    if(t.host.checkpoint && !t.host.checkpoint(true)){if(t.current())report(t,tr("패키지 내보내기"),t.workspace->lastError());return;}
    if(!t.current())return;
    if(!t.workspace->saveSettings()){if(t.current())report(t,tr("패키지 내보내기"),t.workspace->lastError());return;}
    if(!t.current())return;
    const auto base=t.global?QStringLiteral("PIPBONG"):t.name;
    const auto path=selectFile(t,tr("PIPBONG 패키지 내보내기"),QDir(ProjectPackage::defaultExportDirectory()).filePath(base+ProjectPackage::kExtension),
        tr("PIPBONG 패키지 (*%1)").arg(QString::fromLatin1(ProjectPackage::kExtension)),true);
    if(path.isEmpty() || !t.current())return;
    if(!t.workspace->seal(true)){if(t.current())report(t,tr("패키지 내보내기"),t.workspace->lastError());return;}
    if(!t.current())return;
    auto destination=path;if(!destination.endsWith(ProjectPackage::kExtension,Qt::CaseInsensitive))destination+=ProjectPackage::kExtension;
    const auto source=m_profiles.profileDirectory(t.profile);
    const bool packed=t.packages->packNow(source,destination);if(!t.life || !t.current())return;
    if(!packed){report(t,tr("패키지 내보내기"),tr("PIPBONG 패키지를 저장하지 못했습니다."));return;}
    QSettings().setValue("project/lastExportDirectory",QFileInfo(destination).absolutePath());
    t.status(tr("패키지 내보냄: %1").arg(QFileInfo(destination).fileName()));
    if(t.current() && t.host.log)t.host.log(tr("PIPBONG 패키지를 내보냈습니다: %1").arg(destination),true);
}
void Controller::restoreProject() {
    Action action(this);if(!action.accepted)return;const auto t=ticket();if(!t.ready || !t.current() || !t.ports.restore)return;
    bool restored=false;
    {FeatureHotkeyGateScope gate;restored=t.ports.restore(t.owner,t.path,t.directory,[t]{
        if(!t.current())return false;if(t.host.stopSessions)t.host.stopSessions();if(!t.current())return false;
        return (!t.host.checkpoint || t.host.checkpoint(true)) && t.current();
    });}
    if(!restored || !t.current())return;
    m_profiles.invalidateCachedProject(t.profile);const bool loaded=t.workspace->load(t.path,false);
    if(t.life && loaded)t.status(tr("저장본을 복구했어요. 복구 전 내용도 자동 보관됐어요."));
    else if(t.life && t.workspace)report(t,tr("저장본 복구"),t.workspace->lastError());
}
void Controller::installMenus(QMenuBar& bar) {
    auto* file=bar.addMenu(tr("파일(&F)"));
    file->addAction(tr("새 프로젝트(&N)"),this,&Controller::newProject);
    file->addAction(tr("프로젝트 열기(&O)..."),this,&Controller::openProject);
    file->addAction(tr("프로젝트 저장(&S)"),this,&Controller::saveProject);
    file->addAction(tr("다른 이름으로 저장(&A)..."),this,&Controller::saveProjectAs);
    file->addAction(tr("저장본 복구..."),this,&Controller::restoreProject);file->addSeparator();
    file->addAction(tr("PIPBONG 패키지 가져오기(&I)..."),this,&Controller::importPackage);
    file->addAction(tr("PIPBONG 패키지 내보내기(&E)..."),this,&Controller::exportPackage);file->addSeparator();
    file->addAction(tr("업데이트(&U)..."),this,[this]{const auto update=m_host.update;if(update)update();});file->addSeparator();
    file->addAction(tr("종료(&X)"),this,[this]{const auto exit=m_host.exit;if(exit)exit();});
    auto* help=bar.addMenu(tr("도움말(&H)"));
    help->addAction(tr("PIPBONG 도움말(&H)..."),this,[this]{if(m_owner)AppHelpDialog::showHelp(m_owner);});
    help->addAction(tr("오류 보고서(&R)..."),this,[this]{
        const auto t=ticket();if(!t.owner || !t.life)return;
        const auto folder=CrashReporter::latestReportDirectory();
        if(folder.isEmpty()){t.status(tr("저장된 오류 보고서가 없습니다."));return;}
        QFile file(QDir(folder).filePath("report.txt"));if(!file.open(QIODevice::ReadOnly|QIODevice::Text)){t.status(tr("오류 보고서를 열 수 없습니다."));return;}
        const auto text=QString::fromUtf8(file.readAll());QPointer<CrashReportDialog> dialog=new CrashReportDialog(text,folder,t.owner,false,CrashReporter::kindFromReportText(text));
        dialog->exec();if(dialog)delete dialog;
    });help->addSeparator();
    help->addAction(tr("PIPBONG 정보(&A)..."),this,[this]{if(m_owner)AppHelpDialog::showAbout(m_owner);});
}
