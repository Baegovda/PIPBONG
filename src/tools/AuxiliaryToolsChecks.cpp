#include "ui/AuxiliaryToolsController.h"
#include "app/MainWindow.h"
#include "model/Project.h"
#include "app/ForegroundWindowMonitor.h"
#include "app/TriggerMonitorController.h"
#include "ui/memo/MemoDialog.h"
#include "storage/ProfileMemoStore.h"
#include <QApplication>
#include <QDir>
#include <QFile>
#include <QPushButton>
#include <QSettings>
#include <QTemporaryDir>
#include <QTextEdit>
#include <QMessageBox>
#include <QTimer>
#include <iostream>

class AuxiliaryToolsUiChecks {
public:
static bool run(){
    if(QApplication::platformName()!="offscreen")return false;
    const auto fail=[](const char* label){std::cerr<<"Auxiliary tools FAILED: "<<label<<'\n';return false;};
    QTemporaryDir data;ProfileManager profiles(data.path());if(!profiles.initialize())return fail("initialize");
    const QString one=profiles.activeProfileId(),two=profiles.addProfile("Auxiliary second",false);
    const QString keyOne=QStringLiteral("memo/open/%1").arg(one),keyTwo=QStringLiteral("memo/open/%1").arg(two);
    QSettings settings;const auto priorOne=settings.value(keyOne),priorTwo=settings.value(keyTwo);
    struct Restore{QString a,b;QVariant av,bv;~Restore(){QSettings s;for(const auto& p:{qMakePair(a,av),qMakePair(b,bv)})if(p.second.isValid())s.setValue(p.first,p.second);else s.remove(p.first);}}restore{keyOne,keyTwo,priorOne,priorTwo};
    settings.setValue(keyOne,false);settings.setValue(keyTwo,false);
    if(two.isEmpty() || !ProfileMemoStore::save(profiles.projectDirectory(one),"first")
        || !ProfileMemoStore::save(profiles.projectDirectory(two),"second"))return fail("memo setup");
    QWidget owner;QPushButton calculator,spike,memo;calculator.setCheckable(true);spike.setCheckable(true);memo.setCheckable(true);
    int warnings=0,toggles=0;
    AuxiliaryToolsController::Host host;host.warning=[&](const QString&){++warnings;};
    AuxiliaryToolsController controller(profiles,owner,std::move(host));controller.setButtons(&calculator,&spike,&memo);
    QObject::connect(&memo,&QPushButton::toggled,[&]{++toggles;});
    if(!controller.syncMemoProfile() || controller.memo())return fail("closed profile stays lazy");
    controller.toggleMemo();auto* first=controller.memo();auto* editor=first?first->findChild<QTextEdit*>():nullptr;
    if(!first || !editor || !controller.memoVisible() || !memo.isChecked() || editor->toPlainText()!="first" || toggles)return fail("open and button state");
    first->hide();if(memo.isChecked() || controller.memo()!=first)return fail("hide retains instance");
    first->show();if(!memo.isChecked())return fail("external show");
    controller.hideForTray();if(!controller.memoVisible())return fail("tray retains memo policy");
    editor->setPlainText("unsaved first");
    const QString path=ProfileMemoStore::memoFilePath(profiles.projectDirectory(one));
    HANDLE lock=CreateFileW(reinterpret_cast<LPCWSTR>(path.utf16()),GENERIC_READ,FILE_SHARE_READ,nullptr,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,nullptr);
    if(lock==INVALID_HANDLE_VALUE)return fail("lock old memo");
    if(!profiles.setActiveProfile(two)){CloseHandle(lock);return fail("activate second");}
    const bool retained=!controller.syncMemoProfile() && !controller.saveMemo() && warnings==1 && editor->toPlainText()=="unsaved first" && controller.memoVisible();
    bool closeWarning=false;
    QTimer::singleShot(0,&owner,[&]{for(auto* widget:QApplication::topLevelWidgets())if(auto* message=qobject_cast<QMessageBox*>(widget)){closeWarning=true;message->accept();}});
    controller.toggleMemo();
    const bool closeRetained=closeWarning && controller.memo()==first && controller.memoVisible() && memo.isChecked();
    CloseHandle(lock);if(!retained || !closeRetained)return fail("failed save/close retains prior text and visibility");
    const QString destination=ProfileMemoStore::memoFilePath(profiles.projectDirectory(two));
    lock=CreateFileW(reinterpret_cast<LPCWSTR>(destination.utf16()),GENERIC_READ,0,nullptr,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,nullptr);
    if(lock==INVALID_HANDLE_VALUE)return fail("lock destination memo");
    const bool loadRetained=!controller.syncMemoProfile() && warnings==2 && editor->toPlainText()=="unsaved first" && controller.memoVisible();
    CloseHandle(lock);if(!loadRetained)return fail("failed destination load retains prior binding");
    if(!controller.syncMemoProfile() || controller.memoVisible() || memo.isChecked() || editor->toPlainText()!="second")return fail("retry loads closed second");
    QString stored;if(!ProfileMemoStore::load(profiles.projectDirectory(one),&stored) || stored!="unsaved first")return fail("prior memo saved before rebind");
    controller.toggleMemo();
    if(!controller.memoVisible() || !settings.value(keyTwo).toBool())return fail("second reopen flag");
    const QString image=QDir(qEnvironmentVariable("PIPBONG_DATA_DIR")).filePath("auxiliary-memo-detail.png");
    if(!controller.memo()->grab().save(image))return fail("memo render");
    QPointer<MemoDialog> retired=controller.memo();
    if(!retired->close() || controller.memo() || memo.isChecked())return fail("external accepted close retires");
    // Reopen before DeferredDelete: the retired instance must not be reused,
    // and its eventual destroyed callback must not clear the replacement.
    controller.toggleMemo();auto* replacement=controller.memo();
    if(!replacement || replacement==retired.data())return fail("synchronous reopen creates replacement");
    QCoreApplication::sendPostedEvents(nullptr,QEvent::DeferredDelete);
    if(retired || controller.memo()!=replacement || !controller.memoVisible() || !memo.isChecked())return fail("retired deletion isolates replacement");
    controller.closeForShutdown();QCoreApplication::sendPostedEvents(nullptr,QEvent::DeferredDelete);
    if(controller.memo() || memo.isChecked() || !settings.value(keyTwo).toBool())return fail("shutdown preserves open preference");
    controller.syncMemoProfile();
    if(!controller.memoVisible() || !memo.isChecked())return fail("persisted open restores");
    controller.toggleMemo();QCoreApplication::sendPostedEvents(nullptr,QEvent::DeferredDelete);
    if(settings.value(keyTwo).toBool() || controller.memo())return fail("user close clears preference");
    {
        MainWindow window;window.m_foregroundMonitor->stop();window.m_triggerMonitor->stop();
        if(window.m_auxiliaryTools->memoVisible())window.m_auxiliaryTools->toggleMemo();
        QCoreApplication::sendPostedEvents(nullptr,QEvent::DeferredDelete);
        if(!QMetaObject::invokeMethod(&window,"onMemo",Qt::DirectConnection)
            || !window.m_auxiliaryTools->memoVisible() || !window.m_memoButton->isChecked())return fail("actual shell slot/button wiring");
        window.m_auxiliaryTools->memo()->hide();if(window.m_memoButton->isChecked())return fail("shell external hide updates button");
        if(!window.prepareForShutdown())return fail("shell shutdown checkpoint");
    }
    std::cout<<"Auxiliary tools: real memo lazy/open/hidden/rebind, OS-locked save/close/load retry, accepted close/reopen before deferred delete, replacement lifetime, persisted user/shutdown state and MainWindow slot/buttons OK (offscreen; no calculator network or native input claim)\n";
    return true;
}
};
bool runAuxiliaryToolsChecks(){return AuxiliaryToolsUiChecks::run();}
