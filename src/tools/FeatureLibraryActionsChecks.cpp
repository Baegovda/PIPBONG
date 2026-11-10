#include "ui/FeatureLibraryActionsController.h"
#include "app/MainWindow.h"
#include "app/FeatureTransferController.h"
#include "app/ProjectWorkspaceController.h"
#include "app/ProfileManager.h"
#include "app/ForegroundWindowMonitor.h"
#include "app/FeatureHotkeyGate.h"
#include "ui/FeatureListPanel.h"
#include "ui/FeatureDragMime.h"
#include "storage/JsonSerializer.h"
#include "core/workflow/blocks/ImageFindBlock.h"
#include "core/workflow/ExecutionContext.h"
#include "core/vision/TemplateCache.h"
#include <QImage>
#include <QColor>
#include <QDir>
#include <QFile>
#include <QApplication>
#include <QTemporaryDir>
#include <QSettings>
#include <QTimer>
#include <QMessageBox>
#include <QMimeData>
#include <iostream>
#include <algorithm>

class FeatureLibraryActionsChecks {
    using C=FeatureLibraryActionsController;
    static bool check(bool ok,const char* message){if(!ok)std::cerr<<"Library actions FAILED: "<<message<<'\n';return ok;}
    static bool pixels(const Feature& feature,const QString& root){
        if(feature.workflow().blocks().empty())return false;
        const auto* block=dynamic_cast<ImageFindBlock*>(feature.workflow().blocks().front().get());if(!block || block->templatePaths.empty())return false;
        ExecutionContext context;context.setProjectDirectory(root.toStdString());const auto image=TemplateCache::getOrLoad(context.resolvePath(block->templatePaths.front()));
        return image && !image->empty() && image->bgr.rows==8 && image->bgr.cols==8 && image->bgr.at<cv::Vec3b>(0,0)==cv::Vec3b(130,150,80);
    }
    struct Fixture {
        QTemporaryDir root;std::unique_ptr<QWidget> owner=std::make_unique<QWidget>();
        ProfileManager profiles{root.path()};FeatureLibraryManager library{root.path()};ProjectWorkspaceController workspace{profiles,QString()};
        std::unique_ptr<FeatureTransferController> transfers;std::unique_ptr<FeatureBatchController> batches;
        QPointer<FeatureListPanel> view;QPointer<C> controller;C::Ports defaults;
        QString featureId,choice,previewId;std::shared_ptr<Feature> preview;QStringList statuses,notices,warnings;
        bool blocked=false,modalGate=false,checkpointGate=false,noticeGate=false;int dialogs=0,checkpoints=0;
        std::function<void()> onModal,onStatus;std::function<bool()> onCheckpoint;
        bool initialize(){
            if(!root.isValid() || !profiles.initialize() || !workspace.loadActive(true,false))return false;
            auto* feature=workspace.state().project->addFeature("source <A&B>");feature->setPointerVisualFeedback(false);featureId=QString::fromStdString(feature->id());workspace.markModified();
            view=new FeatureListPanel(owner.get());view->setProject(workspace.state().project.get());
            FeatureTransferController::HostCallbacks transfer;transfer.activeProject=[this]{return workspace.state().project;};
            transfer.activeDirectory=[this]{return workspace.state().directory;};
            transfer.generation=[this]{return workspace.generation();};
            transfer.stopFeature=[](const auto&,const auto&){};transfer.warning=[this](const auto& text){warnings<<text;};
            transfer.activeChanged=[](const auto&){};transfer.profileSaved=[](const auto&){};transfer.libraryChanged=[]{};
            transfers=std::make_unique<FeatureTransferController>(profiles,library,transfer);
            FeatureBatchController::Host batch;batch.canBegin=[this]{return !blocked;};batch.generation=[this]{return workspace.generation();};batch.activeProject=[this]{return workspace.state().project.get();};
            batch.checkpoint=[this](const auto&){++checkpoints;checkpointGate=FeatureHotkeyGate::isFeatureHotkeysBlocked();return onCheckpoint?onCheckpoint():workspace.checkpoint(true);};
            batch.activeChanged=[this](const auto& id){workspace.markModified();if(view){view->refresh();if(!id.isEmpty())view->selectFeatureById(id);}};
            batch.libraryChanged=[this]{if(controller)controller->refreshPanel();};
            batches=std::make_unique<FeatureBatchController>(profiles,library,*transfers,batch);
            C::Host host;host.ready=[this]{return !blocked;};host.previewId=[this]{return previewId;};
            host.preview=[this](const auto& id,auto feature){previewId=id;preview=std::move(feature);};
            host.status=[this](const auto& text,int){statuses<<text;auto callback=std::move(onStatus);if(callback)callback();};
            host.warning=[this](const auto& text){warnings<<text;};
            controller=new C(*owner,profiles,library,workspace,*batches,*view,host);defaults=controller->m_ports;
            const auto modal=[this]{++dialogs;modalGate=FeatureHotkeyGate::isFeatureHotkeysBlocked();auto callback=std::move(onModal);if(callback)callback();};
            controller->m_ports.name=[this,modal](auto*,const auto&)->std::optional<QString>{modal();return QStringLiteral("saved name");};
            controller->m_ports.choose=[this,modal](auto*,const auto&){modal();return choice;};
            controller->m_ports.confirm=[modal](auto*,const auto&,const auto&){modal();return true;};
            controller->m_ports.notice=[this](auto*,const auto&,const auto& text,bool){noticeGate=FeatureHotkeyGate::isFeatureHotkeysBlocked();notices<<text;};
            return true;
        }
        QString entry(const QString& name="entry") {QString id;auto* feature=workspace.state().project->featureById(featureId.toStdString());
            if(!feature || !library.saveFeatureToLibrary(*feature,workspace.state().directory,name,&id,nullptr))return {};return id;}
        void replace(){auto staged=std::make_unique<ProjectWorkspaceController::Staged>();staged->project=std::make_unique<Project>();
            staged->profileId=profiles.activeProfileId();staged->path=workspace.state().path;staged->directory=workspace.state().directory;
            workspace.install(std::move(staged),true,false,ProjectWorkspaceController::InstallMode::ValidatedTransition);if(view)view->setProject(workspace.state().project.get());}
        ~Fixture(){owner.reset();}
    };
public:
    static bool run(){
        if(!check(QApplication::platformName()=="offscreen","offscreen required"))return false;
        const auto original=ProgramSettings::profileSettings();QSettings settings;const auto last=settings.value("project/lastFile");
        struct Restore{ProgramSettings::ProfileSettings profile;QVariant last;~Restore(){ProgramSettings::applyProfileSettings(profile);QSettings s;if(last.isValid())s.setValue("project/lastFile",last);else s.remove("project/lastFile");}} restore{original,last};
        {
            Fixture f;if(!f.initialize())return false;
            f.onModal=[&]{f.controller->save(f.featureId);f.controller->importEntries({"missing"});f.controller->remove({"missing"});f.controller->reorder(0,0);};
            f.controller->save(f.featureId);const auto entries=f.library.listEntries();
            if(!check(entries.size()==1 && entries.front().name=="saved name" && f.dialogs==1 && f.modalGate
                && f.checkpoints==1 && !f.checkpointGate && !FeatureHotkeyGate::isFeatureHotkeysBlocked(),"save modal gates hotkeys, blocks nested commands and releases gate before real checkpoint/write"))return false;
            f.choice=entries.front().id;const auto count=f.workspace.state().project->features().size();f.controller->chooseImport();
            if(!check(f.workspace.state().project->features().size()==count+1 && f.view->selectedFeatureId()!=f.featureId,"chooser imports real entry and selects new feature"))return false;
            f.controller->preview(f.choice);if(!check(f.previewId==f.choice && f.preview,"preview publishes matching owned model/id"))return false;
            f.onStatus=[&]{f.controller->preview(f.choice);};f.controller->preview("missing");
            if(!check(f.previewId==f.choice && f.preview,"nested preview publication retires older failed request"))return false;
            f.controller->remove({f.choice,f.choice});
            if(!check(f.library.listEntries().empty() && f.previewId.isEmpty() && !f.preview
                && f.workspace.state().project->features().size()==count+1,"deduplicated confirmed deletion clears owning preview and retains imported copy"))return false;
        }
        {
            Fixture f;if(!f.initialize())return false;
            const auto external=f.root.filePath(QStringLiteral("외부 문서 자산"));QDir().mkpath(QDir(external).filePath("templates"));
            const auto imagePath=QDir(external).filePath(QStringLiteral("templates/한글.png"));QImage image(8,8,QImage::Format_RGB32);image.fill(QColor(80,150,130));if(!image.save(imagePath))return false;
            auto block=std::make_unique<ImageFindBlock>();block->templatePaths={QStringLiteral("templates/한글.png").toStdString()};
            f.workspace.state().project->featureById(f.featureId.toStdString())->workflow().addBlock(std::move(block));
            const auto document=f.root.filePath("external.json");
            if(!JsonSerializer::saveToFile(*f.workspace.state().project,document,external) || !f.workspace.load(document,true))return false;
            f.view->setProject(f.workspace.state().project.get());f.controller->save(f.featureId);
            const auto entries=f.library.listEntries();if(!check(entries.size()==1,"external-root save creates one entry"))return false;
            const auto id=entries.front().id;auto saved=f.library.loadEntryFeature(id);
            if(!check(saved && pixels(*saved,f.library.entryProjectDirectory(id)),"library owns actual Korean PNG from external runtime asset root"))return false;
            if(!QFile::remove(imagePath))return false;
            f.choice=id;f.controller->chooseImport();const auto inserted=f.view->selectedFeatureId();
            const auto* active=f.workspace.state().project->featureById(inserted.toStdString());
            if(!check(active && inserted!=f.featureId && pixels(*active,external),"active external document import resolves copied pixels after original PNG disappears"))return false;
            const auto other=f.profiles.addProfile("external destination",false);const auto destination=f.root.filePath(QStringLiteral("다른 문서 자산"));QDir().mkpath(destination);Project empty;
            if(other.isEmpty() || !JsonSerializer::saveToFile(empty,f.profiles.projectPath(other),destination))return false;
            FeatureDragMime::Payload payload;payload.source=FeatureDragMime::Source::Library;payload.ids={id};std::unique_ptr<QMimeData> mime(FeatureDragMime::createMimeData(payload));
            f.controller->dropOnProfile(other,mime.get());QString root;auto inactive=JsonSerializer::loadFromFile(f.profiles.projectPath(other),&root);
            if(!check(inactive && inactive->features().size()==1 && root==destination && pixels(*inactive->featureAt(0),root),"inactive destination import copies under persisted runtime root"))return false;
            payload.source=FeatureDragMime::Source::Profile;payload.profileId=f.profiles.activeProfileId();payload.ids={inserted};mime.reset(FeatureDragMime::createMimeData(payload));
            f.controller->dropOnProfile(other,mime.get());inactive=JsonSerializer::loadFromFile(f.profiles.projectPath(other),&root);
            if(!check(inactive && inactive->features().size()==2 && pixels(*inactive->featureAt(0),root) && pixels(*inactive->featureAt(1),root),"active external source copy retains both collision-remapped target templates"))return false;
            payload.profileId=other;payload.ids={QString::fromStdString(inactive->featureAt(0)->id())};mime.reset(FeatureDragMime::createMimeData(payload));
            f.controller->dropOnProfile(f.profiles.activeProfileId(),mime.get());
            active=f.workspace.state().project->featureById(f.view->selectedFeatureId().toStdString());
            if(!check(active && pixels(*active,external) && f.warnings.empty(),"inactive external source copies back into active runtime root without missing templates"))return false;
        }
        for(int scenario=0;scenario<6;++scenario){
            Fixture f;if(!f.initialize())return false;
            f.onModal=[&]{switch(scenario){case 0:f.replace();break;case 1:f.workspace.state().project->featureById(f.featureId.toStdString())->setName("changed");break;
                case 2:f.entry("concurrent");break;case 3:f.library.removeEntry("missing");break;case 4:delete f.controller.data();break;case 5:f.owner.reset();break;}};
            f.controller->save(f.featureId);
            if(!check(f.checkpoints==0 && f.library.listEntries().size()==(scenario==2?1:0)
                && !FeatureHotkeyGate::isFeatureHotkeysBlocked(),"modal source/feature/library/failed-mutation/controller/owner changes cancel before writes"))return false;
        }
        for(int scenario=0;scenario<3;++scenario){
            Fixture f;if(!f.initialize())return false;const auto id=f.entry();if(id.isEmpty())return false;
            f.onCheckpoint=[&]{if(scenario==0)f.entry("during checkpoint");else if(scenario==1)delete f.controller.data();else f.workspace.state().project->featureById(f.featureId.toStdString())->setName("checkpoint edit");return true;};
            f.controller->save(f.featureId);
            if(!check(f.checkpoints==1 && f.library.listEntries().size()==(scenario==0?2:1),"post-checkpoint source guard prevents consent from authorizing a replacement write"))return false;
        }
        {
            Fixture f;if(!f.initialize())return false;const auto id=f.entry();const auto count=f.workspace.state().project->features().size();
            f.controller->importEntries({id,"missing"});
            if(!check(f.workspace.state().project->features().size()==count+1 && f.notices.size()==1 && f.noticeGate
                && f.notices.front().contains(QStringLiteral("1개 처리, 1개 실패")),"real partial batch retains success and publishes exact failed count with gated warning"))return false;
            f.choice="not-in-chooser";const int checkpoints=f.checkpoints;f.controller->chooseImport();if(!check(f.checkpoints==checkpoints,"chooser cannot import an entry it did not offer"))return false;
            f.onModal=[&]{f.library.reorderEntries({id});};f.controller->remove({id});
            if(!check(!f.library.listEntries().empty() && f.checkpoints==checkpoints,"changed library revision cancels old deletion consent"))return false;
        }
        {
            Fixture f;if(!f.initialize())return false;const auto one=f.entry("one"),two=f.entry("two");
            f.onCheckpoint=[&]{delete f.controller.data();return true;};f.controller->reorder(0,1);
            const auto entries=f.library.listEntries();if(!check(entries.size()==2 && entries[0].id==one && entries[1].id==two,"orchestrator deletion during reorder checkpoint preserves persisted order"))return false;
        }
        {
            Fixture f;if(!f.initialize())return false;const auto id=f.entry();
            f.onStatus=[&]{f.owner.reset();};f.controller->importEntries({id,"missing"});
            if(!check(!f.owner && f.warnings.empty() && f.notices.empty(),"status callback owner destruction suppresses subsequent warning/dialog publication"))return false;
        }
        for(int port=0;port<4;++port){
            Fixture f;if(!f.initialize())return false;QString id;if(port==1 || port==2)id=f.entry();
            f.controller->m_ports=f.defaults;QTimer::singleShot(0,f.owner.get(),[&]{f.owner.reset();});auto* raw=f.controller.data();
            if(port==0)raw->save(f.featureId);else if(port==1)raw->chooseImport();else if(port==2)raw->remove({id});else raw->chooseImport();
            if(!check(!f.owner && !f.controller && !f.view && f.checkpoints==0 && !FeatureHotkeyGate::isFeatureHotkeysBlocked(),"actual Qt name/chooser/confirm/info dialogs survive parent destruction without committing"))return false;
        }
        {
            MainWindow window;window.m_foregroundMonitor->stop();
            auto* feature=window.m_project->addFeature("library shell");const auto id=QString::fromStdString(feature->id());window.m_projectWorkspace->markModified();
            int dialogs=0;window.m_libraryActions->m_ports.name=[&](auto*,const auto&)->std::optional<QString>{++dialogs;return QStringLiteral("shell saved");};
            window.m_libraryActions->m_ports.notice=[](auto*,const auto&,const auto&,bool){};
            const auto before=window.m_featureLibraryManager->listEntries();window.onSaveFeatureToLibraryRequested(id);const auto after=window.m_featureLibraryManager->listEntries();
            QString created;for(const auto& e:after)if(std::none_of(before.begin(),before.end(),[&](const auto& old){return old.id==e.id;}))created=e.id;
            if(!check(dialogs==1 && !created.isEmpty(),"actual MainWindow save slot delegates to recorded modal and real library files"))return false;
            window.m_libraryActions->m_ports.choose=[&](auto*,const auto&){return created;};const auto count=window.m_project->features().size();window.onImportFeatureFromLibraryRequested();
            window.onLibraryEntrySelected(created);
            if(!check(window.m_project->features().size()==count+1 && window.m_libraryPreviewEntryId==created && window.m_libraryPreviewFeature,"actual shell chooser/import/preview wiring"))return false;
            window.m_libraryActions->m_ports.confirm=[](auto*,const auto&,const auto&){return true;};window.onDeleteLibraryEntriesRequested({created});
            if(!check(window.m_libraryPreviewEntryId.isEmpty() && !window.m_libraryPreviewFeature && window.prepareForShutdown(),"actual shell confirmed delete/preview retirement/shutdown"))return false;
        }
        std::cout<<"Library actions: modal/source/feature/revision/lifetime and post-checkpoint guards, actual external-root Korean template save/import/active+inactive copy pixels, nested commands, exact partial results, preview epochs, reorder cancellation, four parent-deleted Qt dialogs and real MainWindow commands OK (offscreen; no native input)\n";
        return true;
    }
};
bool runFeatureLibraryActionsChecks(){return FeatureLibraryActionsChecks::run();}
