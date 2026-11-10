#include "app/WorkspaceHistoryActions.h"
#include "app/ProfilePackageController.h"
#include <QApplication>
#include <QWidget>
#include "app/FeatureHotkeyGate.h"
#include "ui/widgets/WorkspaceHistoryActivity.h"
#include "ui/memo/MemoDialog.h"
#include <QPointer>
#include <QTimer>
#include <QElapsedTimer>
#include <QThread>
#include <QSemaphore>
#include <atomic>
#include <QTemporaryDir>
#include <QDir>
#include <QFile>
#include <iostream>

class WorkspaceHistoryActionsChecks {
    static bool write(const QString& path,const QByteArray& data){
        if(!QDir().mkpath(QFileInfo(path).absolutePath()))return false;
        QFile file(path);return file.open(QIODevice::WriteOnly) && file.write(data)==data.size();
    }
    static QByteArray read(const QString& path){QFile file(path);return file.open(QIODevice::ReadOnly)?file.readAll():QByteArray();}
    struct Fixture {
        QTemporaryDir root;
        std::unique_ptr<QWidget> owner=std::make_unique<QWidget>();
        std::shared_ptr<std::atomic<int>> packed=std::make_shared<std::atomic<int>>(0);
        std::unique_ptr<ProfilePackageController> packages;
        std::unique_ptr<MemoDialog> memo;
        std::unique_ptr<WorkspaceHistoryActions> actions;
        WorkspaceHistoryController::Paths paths;
        std::function<void()> onCanBegin,onSuspend,onMemo,onPrepare,onPaths,onRestored;
        std::function<void(const QString&)> onReport;
        int errors=0,finished=0,restored=0;bool allowSave=true,ready=false;
        Fixture(){
            packages=std::make_unique<ProfilePackageController>(nullptr,[count=packed](const auto&,const auto&){++*count;return true;});
            paths.dataRoot=root.path();paths.profilesDirectory=root.filePath("profiles");paths.libraryDirectory=root.filePath("featureLibrary");
            paths.profileId="one";paths.projectPath=root.filePath("profiles/one/project.json");
            ready=root.isValid() && write(paths.projectPath,"{\"version\":1,\"features\":[],\"targetWindowTitle\":\"before\"}")
                && write(root.filePath("featureLibrary/entry.json"),"before");
            WorkspaceHistoryActions::HostCallbacks host;
            host.canBegin=[this](bool){const auto f=onCanBegin;if(f)f();return true;};
            host.suspendAutoSave=[this]{const auto f=onSuspend;if(f)f();};
            host.memo=[this]{const auto f=onMemo;if(f)f();return memo.get();};
            host.history.prepare=[this](bool){const auto f=onPrepare;if(f)f();return allowSave;};
            host.history.paths=[this](const auto&){const auto f=onPaths;if(f)f();return paths;};
            host.history.restored=[this](auto,const auto&){++restored;const auto f=onRestored;if(f)f();};
            host.reportError=[this](const QString& error){++errors;const auto f=onReport;if(f)f(error);};
            host.finished=[this]{++finished;};
            actions=std::make_unique<WorkspaceHistoryActions>(owner.get(),*packages,std::move(host));
        }
        ~Fixture(){actions.reset();memo.reset();owner.reset();packages.reset();}
        bool payload(){for(int i=0;i<128;++i)if(!write(root.filePath(QString("featureLibrary/payload-%1.bin").arg(i)),QByteArray(64*1024,'x')))return false;return true;}
        void fastPackages(){packages->findChild<QTimer*>()->setInterval(0);}
    };
    template<class Predicate> static bool spin(Predicate done,int timeout=2000){
        QElapsedTimer timer;timer.start();
        while(!done() && timer.elapsed()<timeout){QCoreApplication::processEvents();QThread::msleep(1);}
        return done();
    }
public:
    static bool run(){
        if(QApplication::platformName()!="offscreen")return false;
        const auto fail=[](const char* label){std::cerr<<"History commands FAILED: "<<label<<'\n';return false;};
        QTemporaryDir root;QWidget parent;
        if(!root.isValid())return false;
        ProfilePackageController packages(nullptr,[](const auto&,const auto&){return true;});
        WorkspaceHistoryController::Paths paths;
        paths.dataRoot=root.path();paths.profilesDirectory=root.filePath("profiles");paths.libraryDirectory=root.filePath("featureLibrary");
        paths.profileId="one";paths.projectPath=root.filePath("profiles/one/project.json");
        const auto library=root.filePath("featureLibrary/entry.json");
        const QByteArray first="{\"version\":1,\"features\":[],\"targetWindowTitle\":\"one\"}";
        const QByteArray second="{\"version\":1,\"features\":[],\"targetWindowTitle\":\"two\"}";
        if(!write(paths.projectPath,first) || !write(library,"before"))return false;
        QString reason="feature-add";
        WorkspaceHistoryActions::HostCallbacks host;
        host.history.paths=[&](const auto&){return paths;};
        host.history.prepare=[&](bool restoring){if(!restoring)reason="profile-edit";return true;};
        WorkspaceHistoryActions actions(&parent,packages,std::move(host));
        if(!actions.push(reason) || !write(paths.projectPath,second) || !write(library,"after") || !actions.undo())return false;
        if(read(paths.projectPath)!=first || read(library)!="after")return fail("command reason must freeze before preparation callback");
        {
            Fixture f;QString reason="feature-add";WorkspaceHistoryController::HostCallbacks host;
            host.prepare=[&](bool restoring){if(!restoring)reason="profile-edit";return true;};host.paths=[&](const auto&){return f.paths;};
            WorkspaceHistoryController history(std::move(host));
            if(!history.push(reason) || !write(f.root.filePath("featureLibrary/entry.json"),"after") || !history.undo()
                || read(f.root.filePath("featureLibrary/entry.json"))!="after")return fail("direct history API freezes scope too");
        }
        {
            Fixture f;bool valid=true;int prepared=0;WorkspaceHistoryController::HostCallbacks host;
            host.validate=[&]{return valid;};host.beginOperation=[&](bool){valid=false;return true;};
            host.prepare=[&](bool){++prepared;return true;};host.paths=[&](const auto&){return f.paths;};
            WorkspaceHistoryController history(std::move(host));
            if(history.push("feature-add") || prepared || history.undoCount())return fail("controller validation stops retired preparation");
        }
        {
            auto owner=std::make_unique<QWidget>();auto activity=std::make_unique<WorkspaceHistoryActivity>(owner.get(),false);
            owner.reset();activity.reset();
            if(FeatureHotkeyGate::isFeatureHotkeysBlocked())return fail("parent-deleted progress releases hotkey gate without double deletion");
        }
        for(int boundary=0;boundary<5;++boundary){
            Fixture f;if(!f.ready)return false;
            const auto retire=[&]{f.actions.reset();};
            if(boundary==0)f.onCanBegin=retire;
            if(boundary==1)f.onSuspend=retire;
            if(boundary==2)f.onMemo=retire;
            if(boundary==3)f.onPrepare=retire;
            if(boundary==4)f.onPaths=retire;
            if(f.actions->push("feature-add") || f.actions || f.errors || FeatureHotkeyGate::isFeatureHotkeysBlocked())return fail("retired preparation callback cannot capture/report or retain gate");
        }
        {
            Fixture f;f.onPrepare=[&]{f.owner.reset();};
            if(f.actions->push("feature-add") || f.owner || f.errors || FeatureHotkeyGate::isFeatureHotkeysBlocked())return fail("parent deletion during preparation");
        }
        {
            Fixture f;f.allowSave=false;int nested=0;
            f.onReport=[&](const QString&){nested+=!f.actions->push("nested");nested+=!f.actions->undo();};
            if(f.actions->push("feature-add") || f.errors!=1 || nested!=2)return fail("error publication blocks nested commands");
            f.onReport={};if(f.actions->undo() || f.errors!=1 || !f.actions->lastError().isEmpty())return fail("empty history cannot republish stale error");
            f.onReport=[&](const QString& error){const QString retained=error;f.actions.reset();f.owner.reset();if(error!=retained)nested=-100;};
            if(f.actions->push("feature-add") || f.actions || f.errors!=2 || nested<0)return fail("error callback owns frozen text after action/parent destruction");
        }
        {
            Fixture f;if(!f.actions->push("feature-add"))return false;f.actions.reset();QCoreApplication::processEvents();
            if(f.finished || FeatureHotkeyGate::isFeatureHotkeysBlocked())return fail("queued finished callback retired with its action");
        }
        {
            Fixture f;if(!f.actions->push("feature-add") || !f.actions->push("feature-add"))return false;
            QCoreApplication::processEvents();if(f.finished!=1)return fail("only latest completed operation publishes deferred resume");
        }
        {
            Fixture f;f.memo=std::make_unique<MemoDialog>();
            if(!f.memo->setProfile("one",f.root.filePath("profiles/one"),"memo"))return false;
            f.memo->setAutoSaveSuspended(true);
            if(!f.actions->push("feature-add") || !f.memo->autoSaveSuspended())return fail("preexisting memo suspension retained");
            f.memo->setAutoSaveSuspended(false);
            if(!f.actions->push("feature-add") || f.memo->autoSaveSuspended())return fail("borrowed memo autosave resumes");
        }
        {
            Fixture f;f.fastPackages();
            if(!f.packages->pauseAndWait() || !f.actions->push("feature-add"))return false;
            f.packages->schedule("source","destination");
            QElapsedTimer timer;timer.start();while(timer.elapsed()<40){QCoreApplication::processEvents();QThread::msleep(1);}
            if(f.packed->load()!=0)return fail("history cannot release another package pause");
            f.packages->resume();if(!spin([&]{return f.packed->load()==1;}))return fail("history releases exactly its own package pause");
        }
        for(bool destroyPackage:{false,true}){
            Fixture f;auto semaphore=std::make_shared<QSemaphore>();auto first=std::make_shared<std::atomic<bool>>(true);auto count=f.packed;
            f.actions.reset();f.packages=std::make_unique<ProfilePackageController>(nullptr,[semaphore,first,count](const auto&,const auto&){if(first->exchange(false))semaphore->acquire();++*count;return true;});
            WorkspaceHistoryActions::HostCallbacks host;host.history.paths=[&](const auto&){return f.paths;};host.history.prepare=[](bool){return true;};host.reportError=[&](const auto&){++f.errors;};
            f.actions=std::make_unique<WorkspaceHistoryActions>(f.owner.get(),*f.packages,std::move(host));f.fastPackages();
            f.packages->schedule("source","one");QMetaObject::invokeMethod(f.packages->findChild<QTimer*>(),"timeout",Qt::DirectConnection);
            if(!f.packages->busy())return fail("package wait fixture did not dispatch");
            QTimer::singleShot(0,qApp,[&]{if(destroyPackage)f.packages.reset();else f.actions.reset();});
            QTimer::singleShot(25,qApp,[semaphore]{semaphore->release();});
            if(f.actions->push("feature-add") || f.errors || FeatureHotkeyGate::isFeatureHotkeysBlocked())return fail("retirement inside package nested wait");
            if(!spin([&]{return count->load()==1;}))return fail("detached package worker completion");
            if(!destroyPackage){f.packages->schedule("source","two");if(!spin([&]{return count->load()==2;}))return fail("late acquired package pause released after action retirement");}
        }
        for(bool idle:{false,true}){
            auto package=std::make_unique<ProfilePackageController>(nullptr,[](const auto&,const auto&){QThread::msleep(30);return true;});
            if(idle)package->schedule("source","output");
            QTimer::singleShot(0,qApp,[&]{package.reset();});
            const bool result=idle?package->waitForIdle():package->packNow("source","output");
            if(result || package)return fail("packNow/waitForIdle reject destroyed package owner");
        }
        {
            Fixture f;if(!f.payload())return false;bool retired=false;
            f.onPaths=[&]{QTimer::singleShot(0,f.owner.get(),[&]{if(f.actions && f.actions->isBusy() && !f.actions->isPreparing()){retired=true;f.actions.reset();}});};
            if(f.actions->push("profile-edit") || !retired || f.actions || f.errors || FeatureHotkeyGate::isFeatureHotkeysBlocked())return fail("real capture I/O keeps history alive but rejects retired publication");
        }
        {
            Fixture f;if(!f.payload() || !f.actions->push("profile-edit"))return false;
            const auto original=read(f.paths.projectPath);if(!write(f.paths.projectPath,"{\"version\":1,\"features\":[],\"targetWindowTitle\":\"after\"}"))return false;
            bool retired=false;f.onPaths=[&]{QTimer::singleShot(0,f.owner.get(),[&]{if(f.actions && f.actions->isBusy() && !f.actions->isPreparing()){retired=true;f.actions.reset();}});};
            if(!f.actions->undo() || !retired || f.actions || f.restored || read(f.paths.projectPath)!=original || FeatureHotkeyGate::isFeatureHotkeysBlocked())return fail("in-flight restore commits safely and suppresses retired UI publication");
        }
        {
            Fixture f;if(!f.actions->push("feature-add"))return false;
            const auto original=read(f.paths.projectPath);if(!write(f.paths.projectPath,"{\"version\":1,\"features\":[],\"targetWindowTitle\":\"after\"}"))return false;
            f.onRestored=[&]{f.actions.reset();};
            if(!f.actions->undo() || f.actions || f.restored!=1 || read(f.paths.projectPath)!=original || FeatureHotkeyGate::isFeatureHotkeysBlocked())return fail("committed restore remains true after callback destroys action");
        }
        std::cout<<"History commands: frozen scope/error publication, nested commands, callback/parent/queued retirement, memo/package borrow balance and real capture/restore I/O lifetime OK (offscreen; temporary files; recorded package workers; no native input)\n";
        return true;
    }
};
bool runWorkspaceHistoryActionsChecks(){return WorkspaceHistoryActionsChecks::run();}
