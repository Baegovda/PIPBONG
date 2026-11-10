#include "app/MainWindow.h"
#include "app/ForegroundWindowMonitor.h"
#include "app/ProjectWorkspaceController.h"
#include "app/ProfilePackageController.h"
#include "ui/ProfileEditDialog.h"
#include "ui/ProfileListWidget.h"
#include "core/workflow/Workflow.h"
#include "core/capture/ScreenCapture.h"
#include "core/workflow/WorkflowEngine.h"
#include "core/workflow/blocks/WaitBlock.h"
#include "model/Project.h"
#include <QApplication>
#include <QAbstractButton>
#include <QDir>
#include <QFile>
#include <QTemporaryDir>
#include <QCheckBox>
#include <QElapsedTimer>
#include <QInputDialog>
#include <QLineEdit>
#include <QMessageBox>
#include <QThread>
#include <QTimer>
#include <QTextEdit>
#include <iostream>

class ProfileActionsUiChecks {
public:
static bool run() {
    if(QApplication::platformName()!="offscreen")return false;
    const auto fail=[](const char* label){std::cerr<<"Profile actions MainWindow FAILED: "<<label<<'\n';return false;};
    const bool focus=ProgramSettings::focusTargetWindowOnProfileSelect();ProgramSettings::setFocusTargetWindowOnProfileSelect(false);
    struct Restore{bool focus;~Restore(){ProgramSettings::setFocusTargetWindowOnProfileSelect(focus);}}restore{focus};
    ProfileEditDialog form("name","Main","Sub","C:/main.exe","C:/sub.exe",false,false,{});
    auto result=form.result();auto* main=form.findChild<QLineEdit*>("profileMainTargetEdit");
    if(!main || result.linkedTargetProcessPath!="C:/main.exe" || result.subLinkedTargetProcessPath!="C:/sub.exe")return fail("dialog retains executable selections");
    main->setText("Typed new main");QMetaObject::invokeMethod(main,"textEdited",Qt::DirectConnection,Q_ARG(QString,main->text()));
    if(!form.result().linkedTargetProcessPath.isEmpty() || form.result().subLinkedTargetProcessPath!="C:/sub.exe")return fail("typed title clears only its stale executable");
    MainWindow window;window.m_foregroundMonitor->stop();
    ProfileSwitchCoordinator::HostCallbacks quiet;quiet.isWorkspacePinned=[] {return true;};quiet.flushDeferredProfileSwitchIfIdle=[]{};
    window.m_profileSwitchCoordinator.setHostCallbacks(quiet);
    const auto original=window.m_profileManager->activeProfileId();bool dialogSeen=false;
    QTimer::singleShot(0,&window,[&]{for(auto* widget:QApplication::topLevelWidgets())if(auto* dialog=qobject_cast<QInputDialog*>(widget)) {
        dialogSeen=true;dialog->setTextValue("UI added");dialog->accept();}});
    window.onAddProfile();
    QElapsedTimer elapsed;elapsed.start();while(window.m_profileSwitchPipelineActive && elapsed.elapsed()<4000){QApplication::processEvents();QThread::msleep(1);}
    const QString id=window.m_profileManager->activeProfileId();
    if(!dialogSeen || id==original || window.m_profileSwitchPipelineActive)return fail("modal add and selected profile installation");
    FeatureRunSession session;session.featureId="profile-edit-capture-owner";session.profileId=id;
    session.engine=std::make_unique<WorkflowEngine>();session.sessionContext=std::make_shared<ExecutionContext>();
    auto workflow=std::make_shared<Workflow>();auto wait=std::make_unique<WaitBlock>();wait->ms=10000;workflow->addBlock(std::move(wait));
    session.engine->run(workflow,session.sessionContext);auto* engine=session.engine.get();window.runSessions().emplace(session.featureId,std::move(session));
    ScreenCapture::setTargetWindow(nullptr);ScreenCapture::setTargetWindowTitle(L"running-edit-main");ScreenCapture::setSubTargetWindowTitle(L"running-edit-sub");
    bool edited=false;
    QTimer::singleShot(0,&window,[&]{for(auto* widget:QApplication::topLevelWidgets())if(auto* dialog=qobject_cast<ProfileEditDialog*>(widget)) {
        if(!dialog->isVisible())continue;
        auto* name=dialog->findChild<QLineEdit*>("profileNameEdit");auto* target=dialog->findChild<QLineEdit*>("profileMainTargetEdit");
        auto* sub=dialog->findChild<QLineEdit*>("profileSubTargetEdit");
        if(name && target && sub){name->setText("UI edited");target->setText("New Main");sub->setText("New Sub");edited=true;dialog->accept();}
    }});
    window.onRenameProfile();
    const bool owned=ScreenCapture::targetWindowTitle()==L"running-edit-main" && ScreenCapture::subTargetWindowTitle()==L"running-edit-sub";
    if(!edited || window.m_profileManager->profileById(id)->name!="UI edited"
        || window.m_project->targetWindowTitle()!="New Main" || window.m_profileManager->subTargetWindowTitle(id)!="New Sub" || !owned)
        return fail("modal edit publication and running capture ownership");
    engine->stopAndWaitBounded(1000);window.runSessions().erase("profile-edit-capture-owner");
    bool confirmed=false;
    QTimer::singleShot(0,&window,[&]{for(auto* widget:QApplication::topLevelWidgets())if(auto* box=qobject_cast<QMessageBox*>(widget)) {
        if(box->windowTitle()==QStringLiteral("프로필 삭제")){confirmed=true;if(auto* yes=box->button(QMessageBox::Yes))yes->click();}}});
    window.onDeleteProfile();
    if(!confirmed || window.m_profileManager->profileById(id) || window.m_profileManager->activeProfileId()!=original) {
        std::cerr<<"Profile removal UI diagnostic: confirmed="<<confirmed<<" remains="<<bool(window.m_profileManager->profileById(id))
            <<" original="<<(window.m_profileManager->activeProfileId()==original)<<" pipeline="<<window.m_profileSwitchPipelineActive
            <<" workspace="<<window.m_projectWorkspace->lastError().toStdString()<<'\n';
        if(auto* log=window.m_logPanel->findChild<QTextEdit*>())std::cerr<<log->toPlainText().right(1600).toStdString()<<'\n';
        return fail("confirmed removal and restored active document");
    }
    if(!window.m_packageController->waitForIdle())return fail("settle packages before blocked-storage fixture");
    QTemporaryDir invalid(QDir(window.m_profileManager->profilesDirectory()).filePath(".profile-edit-ui-XXXXXX"));
    QFile journal(QDir(invalid.path()).filePath("journal.json"));
    if(!invalid.isValid() || !journal.open(QIODevice::WriteOnly))return fail("blocked-storage fixture");
    journal.write("invalid journal");journal.close();
    if(window.m_profileManager->initialize() || window.m_profileManager->storageReady())return fail("detect blocked loaded profile");
    window.m_modified=true;
    if(window.prepareForShutdown())return fail("dirty loaded model must not bypass persistence");
    window.m_modified=false;
    if(!window.prepareForShutdown())return fail("unchanged loaded model can close for recovery");
    std::cout<<"Profile actions MainWindow: actual modal add/edit/delete, executable result/typed-title semantics, model publication, real Wait-engine capture ownership and blocked loaded-model dirty/clean shutdown guards OK (offscreen; no native input)\n";
    return true;
}
};
bool runProfileActionsUiChecks(){return ProfileActionsUiChecks::run();}
