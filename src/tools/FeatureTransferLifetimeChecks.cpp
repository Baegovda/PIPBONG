#include "app/FeatureTransferController.h"
#include "app/ProfileManager.h"
#include "app/FeatureLibraryManager.h"
#include "app/MainWindow.h"
#include "app/ProjectWorkspaceController.h"
#include "app/ForegroundWindowMonitor.h"
#include "app/TriggerMonitorController.h"
#include "app/ProgramSettings.h"
#include "storage/JsonSerializer.h"
#include "core/workflow/blocks/ImageFindBlock.h"
#include "core/workflow/blocks/WaitBlock.h"
#include "core/workflow/Workflow.h"
#include "core/workflow/WorkflowEngine.h"
#include <QTemporaryDir>
#include <QApplication>
#include <QElapsedTimer>
#include <QSettings>
#include <QThread>
#include <QFile>
#include <QFileInfo>
#include <iostream>

class FeatureTransferLifetimeChecks {
    using C=FeatureTransferController;
    static bool check(bool ok,const char* text){if(!ok)std::cerr<<"Transfer lifetime FAILED: "<<text<<'\n';return ok;}
    static QByteArray bytes(const QString& path){QFile f(path);return f.open(QIODevice::ReadOnly)?f.readAll():QByteArray();}
    struct Fixture {
        QTemporaryDir root;ProfileManager profiles{root.path()};FeatureLibraryManager library{root.path()};
        std::shared_ptr<Project> model=std::make_shared<Project>();quint64 generation=1;
        QString activeId,selected,tail,directory,output="unchanged";int stopped=0,changed=0,saved=0,warnings=0,libraryChanged=0,queries=0;
        std::function<void(const QString&,const QString&)> onStop;
        std::function<void()> onChanged,onSaved,onWarning,onLibrary;
        std::unique_ptr<C> controller;
        bool initialize(){
            if(!root.isValid() || !profiles.initialize())return false;activeId=profiles.activeProfileId();directory=profiles.projectDirectory(activeId);
            model->addFeature("earlier row");selected=QString::fromStdString(model->addFeature("selected")->id());tail=QString::fromStdString(model->addFeature("tail")->id());
            C::HostCallbacks host;host.activeProject=[this]{++queries;return model;};host.activeDirectory=[this]{return directory;};host.generation=[this]{return generation;};
            host.stopFeature=[this](const auto& profile,const auto& id){++stopped;auto callback=onStop;if(callback)callback(profile,id);};
            host.activeChanged=[this](const auto&){++changed;auto callback=onChanged;if(callback)callback();};
            host.profileSaved=[this](const auto&){++saved;auto callback=onSaved;if(callback)callback();};
            host.warning=[this](const auto&){++warnings;auto callback=onWarning;if(callback)callback();};
            host.libraryChanged=[this]{++libraryChanged;auto callback=onLibrary;if(callback)callback();};
            controller=std::make_unique<C>(profiles,library,host);return true;
        }
        QString other(bool populated=false){const auto id=profiles.addProfile("other",false);if(id.isEmpty())return {};
            Project p;if(populated)p.addFeature("inactive selected")->setId(selected.toStdString());
            return JsonSerializer::saveToFile(p,profiles.projectPath(id),profiles.projectDirectory(id))?id:QString();}
        void missingAsset(){auto b=std::make_unique<ImageFindBlock>();b->templatePaths={"templates/missing.png"};model->featureById(selected.toStdString())->workflow().addBlock(std::move(b));}
        QString entry(){QString id;return library.saveFeatureToLibrary(*model->featureById(selected.toStdString()),directory,"entry",&id,nullptr)?id:QString();}
    };
public:
    static bool run(){
        {
            Fixture f;if(!f.initialize())return false;
            f.onStop=[&](const auto& profile,const auto& id){if(profile!=f.activeId || id!=f.selected)return;f.model->removeFeature(0);};
            if(!check(f.controller->remove(f.activeId,f.selected) && !f.model->featureById(f.selected.toStdString()) && f.model->featureById(f.tail.toStdString()),
                "stop callback reordered rows; delete must reacquire selected ID rather than erase old index"))return false;
        }
        {
            Fixture f;if(!f.initialize())return false;std::weak_ptr<Project> old=f.model;bool leased=false;
            f.onStop=[&](const auto&,const auto&){f.model=std::make_shared<Project>();f.model->addFeature("replacement")->setId(f.selected.toStdString());++f.generation;leased=!old.expired();};
            if(!check(!f.controller->remove(f.activeId,f.selected) && leased && old.expired() && f.model->featureById(f.selected.toStdString()) && !f.changed,
                "replacement is not modified and retired source stays leased through callback"))return false;
        }
        {
            Fixture f;if(!f.initialize())return false;f.onStop=[&](const auto&,const auto&){f.model->featureById(f.selected.toStdString())->setName("edited");};
            if(!check(!f.controller->remove(f.activeId,f.selected) && f.model->featureById(f.selected.toStdString()) && !f.changed,"same-model feature edit invalidates delete intent"))return false;
        }
        {
            auto f=std::make_unique<Fixture>();if(!f->initialize())return false;std::weak_ptr<Project> old=f->model;bool leased=false;
            f->onStop=[&](const auto&,const auto&){f.reset();leased=!old.expired();};auto* raw=f->controller.get();
            const bool removed=raw->remove(f->activeId,f->selected);
            if(!check(!removed && !f && leased && old.expired(),"owner/controller/dependencies and referenced input strings may die during stop"))return false;
        }
        for(int scenario=0;scenario<2;++scenario){
            Fixture f;if(!f.initialize())return false;const auto other=f.other(true);if(other.isEmpty())return false;QByteArray expected;
            f.onStop=[&](const auto& profile,const auto&){if(profile!=other)return;
                if(scenario==0){Project replacement;replacement.addFeature("new persisted document");JsonSerializer::saveToFile(replacement,f.profiles.projectPath(other),f.profiles.projectDirectory(other));expected=bytes(f.profiles.projectPath(other));}
                else f.profiles.removeProfile(other);};
            if(!check(!f.controller->remove(other,f.selected) && !f.saved && (scenario==0?bytes(f.profiles.projectPath(other))==expected:!QFileInfo::exists(f.profiles.profileDirectory(other))),
                "inactive target replacement/deletion cannot be overwritten or resurrected after stop"))return false;
        }
        {
            Fixture f;if(!f.initialize())return false;bool nested=true;f.onStop=[&](const auto&,const auto&){nested=f.controller->remove(f.activeId,f.tail);};
            if(!check(f.controller->remove(f.activeId,f.selected) && !nested && f.model->featureById(f.tail.toStdString()) && f.stopped==1,"nested operation cannot steal outer mutation ownership"))return false;
        }
        {
            Fixture f;if(!f.initialize())return false;const auto entry=f.entry(),other=f.other();if(entry.isEmpty() || other.isEmpty())return false;
            f.controller->retire();const int queries=f.queries;
            if(!check(!f.controller->remove(f.activeId,f.selected) && !f.controller->saveLibrary(f.selected,f.activeId)
                && !f.controller->importLibrary(entry,f.activeId,-1) && !f.controller->copy(f.selected,f.activeId,other,-1) && queries==f.queries && !f.stopped,"retired controller rejects every operation before host reads"))return false;
        }
        {
            Fixture f;if(!f.initialize())return false;f.missingAsset();f.onWarning=[&]{f.controller.reset();};auto* raw=f.controller.get();
            if(!check(raw->saveLibrary(f.selected,f.activeId) && !f.controller && f.library.listEntries().size()==1 && f.warnings==1 && !f.libraryChanged,
                "committed library save survives warning deletion and suppresses later publication"))return false;
        }
        {
            Fixture f;if(!f.initialize())return false;f.missingAsset();f.onLibrary=[&]{f.controller.reset();};
            if(!check(f.controller->saveLibrary(f.selected,f.activeId) && !f.controller && f.libraryChanged==1,"copied publication callback can destroy controller after committed save"))return false;
        }
        {
            auto f=std::make_unique<Fixture>();if(!f->initialize())return false;f->missingAsset();const auto entry=f->entry();if(entry.isEmpty())return false;
            bool outputReady=false,inserted=false;f->onChanged=[&]{outputReady=f->output!="unchanged";inserted=f->model->featureById(f->output.toStdString())!=nullptr;f.reset();};
            auto* raw=f->controller.get();const bool committed=raw->importLibrary(entry,f->activeId,-1,true,&f->output);
            if(!check(committed && !f && outputReady && inserted,"import output is set before callback can destroy its owning string/model/controller"))return false;
        }
        {
            Fixture f;if(!f.initialize())return false;f.missingAsset();const auto other=f.other();if(other.isEmpty())return false;Project empty;
            f.profiles.storeCachedProject(other,f.profiles.projectPath(other),empty.clone(),f.profiles.projectDirectory(other));
            auto output=std::make_unique<QString>("unchanged");bool outputReady=false,cacheRetired=false;
            f.onSaved=[&]{outputReady=*output!="unchanged";cacheRetired=!f.profiles.cloneCachedProject(other,f.profiles.projectPath(other));output.reset();f.controller.reset();};
            const bool committed=f.controller->copy(f.selected,f.activeId,other,-1,true,output.get());auto saved=JsonSerializer::loadFromFile(f.profiles.projectPath(other));
            if(!check(committed && !f.controller && !output && outputReady && cacheRetired && saved && saved->features().size()==1 && !f.warnings,
                "copy commits/invalidate cache/output before saved callback destroys controller/output; no later warning"))return false;
        }
        if(!shell())return false;
        std::cout<<"Transfer lifetime: row-ID reacquire, leased/replaced/edited models, inactive file/profile retention, nested/retired operations, copied callbacks, output-before-publication and real MainWindow owner-scoped Wait stop OK (offscreen; no native input)\n";
        return true;
    }
    static bool shell(){
        const auto settings=ProgramSettings::profileSettings();const bool focus=ProgramSettings::focusTargetWindowOnProfileSelect();QSettings ini;const auto last=ini.value("project/lastFile");
        struct Restore{ProgramSettings::ProfileSettings settings;bool focus;QVariant last;~Restore(){ProgramSettings::applyProfileSettings(settings);ProgramSettings::setFocusTargetWindowOnProfileSelect(focus);QSettings s;if(last.isValid())s.setValue("project/lastFile",last);else s.remove("project/lastFile");}} restore{settings,focus,last};
        ProgramSettings::setFocusTargetWindowOnProfileSelect(false);MainWindow window;window.m_foregroundMonitor->stop();window.m_triggerMonitor->stop();
        QElapsedTimer deadline;deadline.start();while(window.m_profileSwitchPipelineActive && deadline.elapsed()<4000){QApplication::processEvents();QThread::msleep(1);}if(window.m_profileSwitchPipelineActive)return false;
        const auto active=window.m_profileManager->activeProfileId();auto* feature=window.m_project->addFeature("transfer owner");const auto id=QString::fromStdString(feature->id());window.m_projectWorkspace->markModified();
        const auto survivor=QString::fromStdString(window.m_project->addFeature("transfer survivor")->id());
        const auto other=window.m_profileManager->addProfile("same feature ID elsewhere",false);Project inactive;inactive.addFeature("other owner")->setId(id.toStdString());
        if(other.isEmpty() || !JsonSerializer::saveToFile(inactive,window.m_profileManager->projectPath(other),window.m_profileManager->projectDirectory(other)))return false;
        FeatureRunSession session;session.featureId=id.toStdString();session.profileId=active;session.sessionContext=std::make_shared<ExecutionContext>();session.engine=std::make_unique<WorkflowEngine>();
        auto context=session.sessionContext;auto workflow=std::make_shared<Workflow>();auto wait=std::make_unique<WaitBlock>();wait->ms=5000;workflow->addBlock(std::move(wait));
        session.engine->run(workflow,context);auto* engine=session.engine.get();window.runSessions().emplace(id.toStdString(),std::move(session));
        if(!check(window.m_featureTransfers->remove(other,id) && !context->shouldStop() && engine->isRunning(),"inactive same-ID deletion must not stop a different profile's real Wait engine"))return false;
        if(!check(window.m_featureTransfers->remove(active,id) && context->shouldStop() && !window.m_project->featureById(id.toStdString()),"own-profile deletion stops its actual Wait engine and removes requested feature"))return false;
        window.m_profileManager->removeProfile(other);if(!window.prepareForShutdown())return false;
        if(!check(!window.m_featureTransfers->saveLibrary(survivor,active),"successful shutdown retires real transfer controller even for a surviving feature"))return false;
        return true;
    }
};
bool runFeatureTransferLifetimeChecks(){return FeatureTransferLifetimeChecks::run();}
