#include "ui/DocumentActionsController.h"
#include "app/MainWindow.h"
#include "app/ProjectWorkspaceController.h"
#include "app/ProfileManager.h"
#include "app/ProfilePackageController.h"
#include "app/FeatureHotkeyGate.h"
#include "app/ForegroundWindowMonitor.h"
#include "app/TriggerMonitorController.h"
#include "model/Project.h"
#include "storage/JsonSerializer.h"
#include "storage/ProjectPackage.h"
#include <QApplication>
#include <QMenuBar>
#include <QMenu>
#include <QAction>
#include <QSettings>
#include <QTemporaryDir>
#include <QFile>
#include <QDir>
#include <QElapsedTimer>
#include <QThread>
#include <QTimer>
#include <QSemaphore>
#include <QThreadPool>
#include <iostream>
#ifdef _WIN32
#include <windows.h>
#endif

class DocumentActionsChecks {
    using C=DocumentActionsController;
    static bool check(bool result,const char* text){if(!result)std::cerr<<"Document actions FAILED: "<<text<<'\n';return result;}
    static bool named(const QString& path,const std::string& name){auto p=JsonSerializer::loadFromFile(path);auto* f=p?p->featureById("doc-actions"):nullptr;return f && f->name()==name;}
    static bool until(const std::function<bool()>& done){QElapsedTimer timer;timer.start();
        while(!done() && timer.elapsed()<15000){QApplication::processEvents();QThread::msleep(1);}return done();}
    struct PrepareGate {
        QSemaphore entered,release,finished;
        std::shared_ptr<PreparedProfileImport> prepare(const ProfileImportRequest& request){entered.release();
            if(!release.tryAcquire(1,10000)){finished.release();return {};}
            auto result=prepareProfileImport(request);finished.release();return result;}
    };
    struct Fixture {
        QTemporaryDir folder;QWidget owner;ProfileManager profiles{folder.path()};
        ProjectWorkspaceController workspace{profiles,QString()};
        std::vector<std::pair<QString,QString>> packed;
        ProfilePackageController packages{nullptr,[this](const auto& from,const auto& to){packed.emplace_back(from,to);return true;}};
        std::unique_ptr<C> controller;QString chosen;QStringList errors,statuses,logs;
        int dialogs=0,stops=0,updates=0,exits=0;bool gate=false,blocked=false,importReady=true;
        std::function<void()> onFile;
        std::function<bool()> onCheckpoint;
        bool initialize(){
            if(!profiles.initialize() || !workspace.loadActive(true,false))return false;
            ProjectWorkspaceController::HostCallbacks document;
            document.mayReplace=[] {return !FeatureHotkeyGate::isFeatureHotkeysBlocked();};document.mayWrite=[] {return true;};
            workspace.setHost(document);workspace.setPackages(&packages);
            auto* f=workspace.state().project->addFeature("before");f->setId("doc-actions");workspace.state().modified=true;
            C::Host h;h.ready=[this]{return !blocked;};h.directory=[this]{return workspace.state().directory;};
            h.checkpoint=[this](bool quiet){return onCheckpoint?onCheckpoint():workspace.checkpoint(quiet);};h.prepareImport=[this]{return importReady;};
            h.stopSessions=[this]{++stops;};h.update=[this]{++updates;};h.exit=[this]{++exits;};
            h.log=[this](const auto& text,bool){logs.push_back(text);};h.status=[this](const auto& text){statuses.push_back(text);};
            h.imported=[this]{workspace.loadActive(true,false);};
            controller=std::make_unique<C>(owner,profiles,workspace,packages,std::move(h));
            controller->m_ports.file=[this](auto*,const auto&,const auto&,const auto&,bool){++dialogs;gate=FeatureHotkeyGate::isFeatureHotkeysBlocked();auto callback=std::move(onFile);if(callback)callback();return chosen;};
            controller->m_ports.error=[this](auto*,const auto&,const auto& text){errors.push_back(text);};
            return true;
        }
        ~Fixture(){controller.reset();packages.waitForIdle();QThreadPool::globalInstance()->waitForDone(15000);}
    };
public:
    static bool run(){
        if(!check(QApplication::platformName()=="offscreen","offscreen required"))return false;
        const auto original=ProgramSettings::profileSettings();QSettings settings;
        const auto last=settings.value("project/lastExportDirectory"),file=settings.value("project/lastFile");
        struct Restore{ProgramSettings::ProfileSettings value;QVariant last,file;~Restore(){ProgramSettings::applyProfileSettings(value);QSettings s;
            if(last.isValid())s.setValue("project/lastExportDirectory",last);else s.remove("project/lastExportDirectory");
            if(file.isValid())s.setValue("project/lastFile",file);else s.remove("project/lastFile");}} restore{original,last,file};
        {
            Fixture f;if(!f.initialize())return false;f.chosen=f.folder.filePath("external.json");f.controller->saveProjectAs();
            if(!check(f.gate && f.dialogs==1 && !FeatureHotkeyGate::isFeatureHotkeysBlocked() && named(f.chosen,"before")
                && named(f.profiles.activeProjectPath(),"before") && f.workspace.state().path==f.chosen && f.errors.isEmpty(),"Save As modal gate releases before durable external/canonical commit"))return false;
            f.workspace.state().project->featureById("doc-actions")->setName("after");f.workspace.state().modified=true;f.controller->saveProject();
            if(!check(named(f.chosen,"after") && named(f.profiles.activeProjectPath(),"after") && !f.logs.isEmpty(),"Save command persists both current document copies"))return false;
            QMenuBar bar(&f.owner);f.controller->installMenus(bar);
            if(!check(bar.actions().size()==2 && bar.actions()[0]->menu()->actions().size()==12,"File/Help menu composition retained"))return false;
            for(auto* action:bar.actions()[0]->menu()->actions()) {
                if(action->text().startsWith(QStringLiteral("업데이트")))action->trigger();
                if(action->text().startsWith(QStringLiteral("종료")))action->trigger();
            }
            if(!check(f.updates==1 && f.exits==1,"actual QAction routes to supplied app commands"))return false;
            f.chosen=f.folder.filePath("out");f.controller->exportPackage();
            if(!check(f.errors.isEmpty() && !f.packed.empty() && f.packed.back().second==f.chosen+".pipbong"
                && f.packed.back().first==f.profiles.activeProjectDirectory(),"export extension and frozen canonical package source"))return false;
            f.controller->m_ports.restore=[&](auto*,const auto& path,const auto& directory,const auto& prepare){
                if(!prepare())return false;auto p=f.workspace.state().project->clone();p->featureById("doc-actions")->setName("recovered");return JsonSerializer::saveToFile(*p,path,directory);};
            f.controller->restoreProject();
            if(!check(f.stops==1 && f.workspace.state().project->featureById("doc-actions")->name()=="recovered"
                && f.workspace.checkpoint() && named(f.profiles.activeProjectPath(),"recovered"),"recovery prepares current source, reloads and checkpoints canonical"))return false;
        }
        {
            Fixture f;if(!f.initialize())return false;f.chosen=f.folder.filePath("stale.json");
            f.onFile=[&]{auto staged=std::make_unique<ProjectWorkspaceController::Staged>();staged->project=std::make_unique<Project>();
                staged->profileId=f.profiles.activeProfileId();staged->path=f.workspace.state().path;staged->directory=f.workspace.state().directory;
                f.workspace.install(std::move(staged),true,false,ProjectWorkspaceController::InstallMode::ValidatedTransition);};
            f.controller->saveProjectAs();
            if(!check(!QFileInfo::exists(f.chosen) && !f.statuses.isEmpty() && f.errors.isEmpty(),"source replacement during dialog cancels old destination"))return false;
        }
        {
            Fixture f;if(!f.initialize())return false;bool promptBeforePack=false;
            f.chosen=QString();f.onFile=[&]{promptBeforePack=f.packed.empty();};f.controller->exportPackage();
            if(!check(promptBeforePack && f.packed.empty() && f.errors.isEmpty(),"cancelled export prompts before synchronous packing"))return false;
        }
        {
            Fixture f;if(!f.initialize())return false;f.chosen=f.folder.filePath("nested.json");
            f.onFile=[&]{f.controller->saveProjectAs();};f.controller->saveProjectAs();
            if(!check(f.dialogs==1 && named(f.chosen,"before"),"nested command does not open second dialog"))return false;
            f.blocked=true;f.controller->openProject();if(!check(f.dialogs==1,"blocked workspace does not begin modal command"))return false;
        }
        {
            Fixture f;if(!f.initialize())return false;f.chosen=f.folder.filePath("deleted.json");
            f.onFile=[&]{f.controller.reset();};auto* raw=f.controller.get();raw->saveProjectAs();
            if(!check(!f.controller && !QFileInfo::exists(f.chosen) && !FeatureHotkeyGate::isFeatureHotkeysBlocked(),"controller deletion during modal callback releases gate and cancels write"))return false;
        }
        {
            Fixture f;if(!f.initialize())return false;f.chosen=f.folder.filePath("not-imported.pipbong");f.importReady=false;
            const auto before=f.profiles.activeProfileId();const auto count=f.profiles.profiles().size();f.controller->importPackage();
            if(!check(f.profiles.activeProfileId()==before && f.profiles.profiles().size()==count && !f.errors.isEmpty(),"outgoing settings/memo failure blocks import selection change"))return false;
        }
        {
            Fixture f;if(!f.initialize())return false;const auto outgoing=f.profiles.activeProfileId();
            const auto incoming=f.folder.filePath("incoming");QDir().mkpath(incoming);Project project;
            auto* feature=project.addFeature("incoming");feature->setId("doc-actions");
            if(!JsonSerializer::saveToFile(project,QDir(incoming).filePath("project.json"),incoming))return false;
            f.chosen=f.folder.filePath("incoming.pipbong");if(!ProjectPackage::packDirectory(incoming,f.chosen))return false;
            f.controller->importPackage();
            if(!check(until([&]{return !f.controller->busy();}),"asynchronous import completes"))return false;
            if(!check(f.errors.isEmpty() && f.profiles.activeProfileId()!=outgoing
                && f.workspace.state().profileId==f.profiles.activeProfileId()
                && f.workspace.state().project->featureById("doc-actions")
                && f.workspace.state().project->featureById("doc-actions")->name()=="incoming"
                && named(f.profiles.projectPath(outgoing),"before"),"real package import checkpoints outgoing source and loads new owning profile"))return false;
        }
        for(int scenario=0;scenario<4;++scenario) {
            Fixture f;if(!f.initialize())return false;
            const auto outgoing=f.profiles.activeProfileId();const auto count=f.profiles.profiles().size();
            const auto incoming=f.folder.filePath("incoming");QDir().mkpath(incoming);Project project;project.addFeature("incoming");
            if(!JsonSerializer::saveToFile(project,QDir(incoming).filePath("project.json"),incoming))return false;
            f.chosen=f.folder.filePath("delayed.pipbong");if(!ProjectPackage::packDirectory(incoming,f.chosen))return false;
            auto gate=std::make_shared<PrepareGate>();f.controller->m_ports.prepare=[gate](const auto& request){return gate->prepare(request);};
            int beats=0;QTimer heartbeat;QObject::connect(&heartbeat,&QTimer::timeout,&f.owner,[&]{++beats;});heartbeat.start(1);
            f.controller->importPackage();
            if(!check(until([&]{return gate->entered.available()>0 && beats>=3;}),"GUI heartbeat continues while archive worker is blocked"))return false;
            if(!check(f.profiles.profiles().size()==count && f.profiles.activeProfileId()==outgoing,"preparation does not publish registration"))return false;
            if(scenario==0){auto staged=std::make_unique<ProjectWorkspaceController::Staged>();staged->project=std::make_unique<Project>();
                staged->profileId=outgoing;staged->path=f.workspace.state().path;staged->directory=f.workspace.state().directory;
                f.workspace.install(std::move(staged),true,false,ProjectWorkspaceController::InstallMode::ValidatedTransition);}
            if(scenario==1)f.blocked=true;
            if(scenario==2)f.controller.reset();
            if(scenario==3)f.onCheckpoint=[&]{f.controller.reset();return false;};
            gate->release.release();
            if(!check(until([&]{return gate->finished.available()>0;}),"delayed worker terminates"))return false;
            if(scenario==1){if(!check(until([&]{return f.statuses.size()>=2;}) && f.controller->busy()
                    && f.profiles.activeProfileId()==outgoing,"editing state defers ready import publication"))return false;
                f.blocked=false;}
            if(!check(until([&]{return !f.controller || !f.controller->busy();}),"deferred/stale/deleted completion settles"))return false;
            if(scenario==1){if(!check(f.profiles.profiles().size()==count+1 && f.profiles.activeProfileId()!=outgoing,"ready transition completes prepared import"))return false;}
            else if(!check(f.profiles.profiles().size()==count && f.profiles.activeProfileId()==outgoing,"stale or deleted controller never publishes worker result")){
                std::cerr<<"Document import scenario="<<scenario<<" count="<<f.profiles.profiles().size()<<" expected="<<count<<'\n';return false;}
        }
        {
            Fixture f;if(!f.initialize())return false;f.chosen=f.folder.filePath("null.pipbong");
            f.controller->m_ports.prepare=[](const auto&){return std::shared_ptr<PreparedProfileImport>{};};
            f.controller->importPackage();
            if(!check(until([&]{return !f.controller->busy();}) && !f.errors.isEmpty(),"null worker result reports failure and releases command"))return false;
        }
        {
            QTemporaryDir folder;MainWindow w;w.m_foregroundMonitor->stop();w.m_triggerMonitor->stop();
            auto* f=w.m_project->addFeature("shell-before");f->setId("doc-actions");w.m_projectWorkspace->markModified();
            const auto path=folder.filePath("shell-copy.json");bool gate=false;QStringList errors;
            w.m_documentActions->m_ports.file=[&](auto*,const auto&,const auto&,const auto&,bool){gate=FeatureHotkeyGate::isFeatureHotkeysBlocked();return path;};
            w.m_documentActions->m_ports.error=[&](auto*,const auto&,const auto& error){errors.push_back(error);};
            w.onSaveProjectAs();
            if(!check(gate && errors.isEmpty() && w.m_projectFilePath==path && named(path,"shell-before")
                && named(w.m_profileManager->activeProjectPath(),"shell-before"),"actual MainWindow slot/controller/canonical Save As wiring"))return false;
            if(!w.m_projectWorkspace->loadActive(true,false) || !check(w.m_project->featureById("doc-actions")
                && w.m_project->featureById("doc-actions")->name()=="shell-before","actual MainWindow canonical reload retains external-saved model"))return false;
            if(!w.prepareForShutdown())return false;
        }
        std::cout<<"Document actions: actual QAction/modal/save/export/recovery, async import heartbeat/deferred editing/stale source/controller and checkpoint deletion/null result, outgoing checkpoint and actual MainWindow reload/shutdown OK (offscreen; recorded dialogs, no native input)\n";
        return true;
    }
};
bool runDocumentActionsChecks(){return DocumentActionsChecks::run();}
