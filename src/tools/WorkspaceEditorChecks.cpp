#include "ui/WorkspaceEditorController.h"
#include "ui/WorkflowEditorPanel.h"
#include "app/MainWindow.h"
#include "app/FeatureRunSession.h"
#include "app/ForegroundWindowMonitor.h"
#include "app/TriggerMonitorController.h"
#include "model/Project.h"
#include "core/workflow/blocks/WaitBlock.h"
#include <QApplication>
#include <QPushButton>
#include <QSettings>
#include <QTemporaryDir>
#include <iostream>
#include <map>

class WorkspaceEditorChecks {
    using C=WorkspaceEditorController;
    static bool check(bool value,const char* label) {
        if(!value)std::cerr<<"Workspace editor FAILED: "<<label<<'\n';return value;
    }
    static std::shared_ptr<Feature> feature(const char* name) {
        auto f=std::make_shared<Feature>(name);f->setId("same");
        f->workflow().addBlock(std::make_unique<WaitBlock>());return f;
    }
    struct Fixture {
        WorkflowEditorPanel editor;FeatureListPanel list;C::Source source;
        std::map<std::string,FeatureRunSession> sessions;std::vector<std::string> pending;
        std::unique_ptr<C> controller;int timing=0,changed=0,reads=0;bool autoSelect=true;
        std::function<void()> onTiming;
        Fixture() {
            source.project=std::make_shared<Project>();source.owner="owner";source.modelOwner="owner";source.generation=1;
            auto* f=source.project->addFeature("owned");f->setId("same");f->workflow().addBlock(std::make_unique<WaitBlock>());
            auto* second=source.project->addFeature("second");second->setId("second");
            list.setProject(source.project.get());list.selectFeatureById("same");
            C::Host h;h.source=[this]{++reads;auto s=source;s.selected=list.selectedFeature();s.selectedId=list.selectedFeatureId();return s;};
            h.session=[this](const auto& id)->FeatureRunSession*{auto it=sessions.find(id);return it==sessions.end()?nullptr:&it->second;};
            h.sessions=[this]{std::vector<std::string> ids;for(const auto& p:sessions)ids.push_back(p.first);return ids;};
            h.pending=[this]{return pending;};h.autoSelect=[this]{return autoSelect;};
            h.clearPreview=[this]{source.preview.reset();source.previewId.clear();};
            h.changed=[this]{++changed;};h.timing=[this](const auto* s){++timing;editor.setLoopTiming(s->lastLoopNumber,10,10,true);auto call=std::move(onTiming);if(call)call();};
            controller=std::make_unique<C>(editor,list,std::move(h));
            QObject::connect(&list,&FeatureListPanel::selectionChanged,controller.get(),[this]{controller->selectionChanged();});
        }
    };
    static bool bindings() {
        WorkflowEditorPanel panel;auto a=feature("profile-A"),b=feature("profile-B");
        panel.bindFeature(a.get(),a,"profile:A");panel.setLoopTiming(7,70,60,true);
        panel.setActiveBlockIndex(0);panel.setBlockDuration(0,91);
        panel.bindFeature(b.get(),b,"profile:B");
        if(!check(!panel.m_hasLoopTiming && panel.m_activeBlockIndex==-1 && panel.m_rowBlockDurations.value(0)==0,"same-ID profiles have separate feedback"))return false;
        panel.setLoopTiming(9,90,80,false);panel.bindFeature(a.get(),a,"profile:A");
        if(!check(panel.m_loopNumber==7 && panel.m_activeBlockIndex==0 && panel.m_rowBlockDurations.value(0)==91,"completed feedback restored for its owner"))return false;
        panel.setLoopTiming(11,110,100,true);panel.setFeature(a.get());
        if(!check(panel.m_loopNumber==11,"same binding refresh retains newest feedback"))return false;
        panel.bindFeature(b.get(),b,"library:A");
        if(!check(!panel.m_hasLoopTiming && panel.m_activeBlockIndex==-1,"library scope differs from profile"))return false;
        auto preview=feature("retired");std::weak_ptr<Feature> weak=preview;
        panel.bindFeature(preview.get(),preview,"library:retired");panel.setLoopRegionPickMode(true);preview.reset();
        if(!check(!weak.expired(),"displayed preview retained after host retirement"))return false;
        panel.bindFeature(a.get(),a,"profile:A");
        if(!check(weak.expired() && !panel.m_loopRegionPickActive,"retired preview released after safe rebind"))return false;
        auto raw=feature("raw-retired");panel.setFeature(raw.get());panel.setLoopRegionPickMode(true);raw.reset();panel.setFeature(nullptr);
        if(!check(!panel.displayedFeature(),"raw retired model detaches using cached ID"))return false;
        panel.bindFeature(a.get(),a,"profile:A");panel.setLoopRegionPickMode(true);
        bool replaced=false;
        const auto connection=QObject::connect(panel.m_loopRegionsButton,&QPushButton::toggled,&panel,[&](bool active){
            if(!active && !replaced){replaced=true;panel.bindFeature(b.get(),b,"profile:B");}
        });
        panel.bindFeature(a.get(),a,"replacement:outer");QObject::disconnect(connection);
        return check(replaced && panel.displayedFeature()==b.get() && panel.m_feedbackScope=="profile:B" && panel.m_loopNumber==9,"synchronous Qt toggle rebind supersedes outer binding");
    }
    static bool projection() {
        Fixture f;f.controller->refresh();f.editor.setLoopTiming(3,30,20,true);
        auto& s=f.sessions["same"];s.featureId="same";s.profileId="foreign";s.lastLoopNumber=99;s.runningBlockIndex=0;
        f.controller->refresh();
        if(!check(f.timing==0 && f.editor.m_loopNumber==3 && !f.controller->displayed(&s),"foreign same-ID session cannot overwrite owned cached feedback"))return false;
        s.profileId="owner";s.lastLoopNumber=12;f.controller->refresh();
        if(!check(f.timing==1 && f.editor.m_loopNumber==12 && f.editor.m_activeBlockIndex==0 && f.controller->displayed(&s),"owned live timing/highlight"))return false;
        f.source.preview=feature("preview");f.source.previewId="library-entry";f.controller->refresh();
        if(!check(!f.editor.m_editingEnabled && !f.editor.m_hasLoopTiming && f.editor.m_activeBlockIndex==-1 && !f.controller->displayed(&s) && f.timing==1,"preview read-only and isolated from live progress"))return false;
        auto* owned=f.source.project->featureById("same");auto foreign=feature("foreign");
        f.controller->selectRunning(foreign.get());f.autoSelect=false;f.controller->selectRunning(owned);
        if(!check(bool(f.source.preview),"foreign pointer and disabled auto-select preserve preview"))return false;
        f.autoSelect=true;f.controller->selectRunning(owned);
        if(!check(!f.source.preview && f.editor.displayedFeature()==owned && f.controller->displayed(&s),"auto-select leaves preview even when list ID is already selected"))return false;
        s.runningMode=FeatureRunMode::Trigger;s.repeatSession=true;s.triggerPhase=TriggerSessionPhase::Monitoring;
        if(!check(f.controller->visuals().value("same")==FeatureRunVisualKind::TriggerWatch,"trigger watch visual"))return false;
        s.triggerPhase=TriggerSessionPhase::Cooldown;
        if(!check(f.controller->visuals().value("same")==FeatureRunVisualKind::TriggerCooldown,"trigger cooldown visual"))return false;
        s.triggerPhase=TriggerSessionPhase::RunningAction;
        if(!check(f.controller->visuals().value("same")==FeatureRunVisualKind::ActiveRun,"trigger action visual"))return false;
        s.profileId="foreign";
        if(!check(f.controller->visuals().isEmpty(),"foreign visuals filtered"))return false;
        s.profileId="owner";s.runningMode=FeatureRunMode::Hold;s.holdRunActive=false;f.pending={"queued"};
        if(!check(!f.controller->visuals().contains("same") && f.controller->visuals().contains("queued"),"idle Hold absent, owned pending visible"))return false;
        s.holdRunActive=true;
        if(!check(f.controller->visuals().contains("same"),"active Hold visible"))return false;
        f.source.modelOwner="old-model";const int count=f.timing;f.controller->refresh();
        if(!check(f.timing==count && !f.controller->displayed(&s) && f.controller->visuals().isEmpty(),"incomplete profile transition frozen"))return false;
        f.source.modelOwner="owner";const int changed=f.changed;
        f.onTiming=[&]{s.sessionContext=std::make_shared<ExecutionContext>();};f.controller->refresh();
        if(!check(f.changed==changed,"session replacement during timing prevents stale continuation"))return false;
        std::weak_ptr<Project> old=f.source.project;bool held=false;
        f.onTiming=[&]{f.source.project=std::make_shared<Project>();auto* next=f.source.project->addFeature("next");next->setId("same");
            ++f.source.generation;f.list.setProject(f.source.project.get());f.list.selectFeatureById("same");f.controller->refresh();held=!old.expired();};
        f.controller->refresh();
        if(!check(held && old.expired() && f.editor.displayedFeature()==f.source.project->featureById("same"),"reentrant document replacement pins old model only through callback"))return false;
        f.onTiming=[&]{f.controller.reset();};f.controller->refresh();
        return check(!f.controller,"controller deletion in timing callback is safe");
    }
    static bool preferences() {
        Fixture f;QTemporaryDir dir;f.source.path=dir.filePath("document.json");
        const auto key=C::preferenceKey(f.source.path);QSettings settings;
        struct Cleanup{QString key;~Cleanup(){QSettings().remove(key);}} cleanup{key};
        settings.setValue(key,"deleted-feature");f.controller->restoreSelection();
        if(!check(f.list.selectedFeatureId()=="same","stale saved selection falls back to first feature"))return false;
        f.list.selectFeatureById("second");
        if(!check(QSettings().value(key).toString()=="second","selection saved using legacy preference key"))return false;
        f.source.preview=feature("preview");f.source.previewId="preview";f.controller->saveSelection();
        if(!check(QSettings().value(key).toString()=="second","preview does not overwrite selection preference"))return false;
        f.source.preview.reset();f.source.previewId.clear();f.source.modelOwner="incomplete";settings.setValue(key,"same");f.controller->restoreSelection();
        if(!check(f.list.selectedFeatureId()=="second","partial model transition cannot restore selection"))return false;
        f.source.modelOwner="owner";f.controller->restoreSelection();
        return check(f.list.selectedFeatureId()=="same","valid saved selection restored");
    }
    static bool deletedViews() {
        auto editor=std::make_unique<WorkflowEditorPanel>();auto list=std::make_unique<FeatureListPanel>();int reads=0;
        C::Host h;h.source=[&]{++reads;return C::Source{};};C c(*editor,*list,std::move(h));editor.reset();list.reset();
        c.refresh();c.saveSelection();c.restoreSelection();c.selectRunning(nullptr);c.visuals();
        return check(reads==0 && !c.displayed(nullptr),"destroyed views suppress host callbacks");
    }
    static bool shell() {
        MainWindow w;w.m_foregroundMonitor->stop();w.m_triggerMonitor->stop();
        auto* owned=w.m_project->addFeature("workspace-editor-owner");owned->setId("workspace-editor-check");
        owned->workflow().addBlock(std::make_unique<WaitBlock>());w.m_featureList->refresh();w.m_featureList->selectFeatureById("workspace-editor-check");
        auto& s=w.runSessions()[owned->id()];s.featureId=owned->id();s.profileId="background-foreign";
        s.hasLastLoopTiming=true;s.lastLoopNumber=42;s.lastLoopElapsedMs=10;s.lastLoopAverageMs=10;s.runningBlockIndex=0;
        w.refreshWorkflowEditor();
        if(!check(!w.m_workflowEditor->m_hasLoopTiming && !w.isDisplayedRunningFeature(&s),"actual shell rejects foreign timing"))return false;
        s.profileId=w.m_profileManager->activeProfileId();w.refreshWorkflowEditor();
        if(!check(w.m_workflowEditor->m_loopNumber==42 && w.isDisplayedRunningFeature(&s),"actual publisher shows owned timing"))return false;
        auto preview=feature("library-preview");preview->setId(owned->id());std::weak_ptr<Feature> weak=preview;
        w.m_libraryPreviewFeature=preview;w.m_libraryPreviewEntryId="isolated-fixture";preview.reset();w.refreshWorkflowEditor();
        w.syncLoopTimingToWorkflowEditor(&s);
        if(!check(!w.m_workflowEditor->m_hasLoopTiming && !w.isDisplayedRunningFeature(&s) && !w.m_workflowEditor->m_editingEnabled,"actual publisher cannot write into preview"))return false;
        w.m_libraryPreviewFeature.reset();w.m_libraryPreviewEntryId.clear();
        if(!check(!weak.expired(),"actual preview remains owned by editor until detach"))return false;
        w.prepareProjectUnload();
        if(!check(weak.expired() && !w.m_workflowEditor->displayedFeature(),"actual unload safely detaches retired preview"))return false;
        w.prepareForShutdown();return true;
    }
public:
    static bool run() {
        if(!check(QApplication::platformName()=="offscreen","offscreen required"))return false;
        if(!bindings() || !projection() || !preferences() || !deletedViews() || !shell())return false;
        std::cout<<"Workspace editor: scoped completed feedback, retired model leases, nested Qt binding, owner/preview filtering, queued visuals, stale selection fallback, source/session replacement and actual shell publisher/unload OK\n";
        return true;
    }
};
bool runWorkspaceEditorChecks(){return WorkspaceEditorChecks::run();}
