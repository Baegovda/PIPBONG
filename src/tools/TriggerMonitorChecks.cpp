#include "app/TriggerMonitorController.h"
#include "app/MainWindow.h"
#include "app/ProjectWorkspaceController.h"
#include "app/ForegroundWindowMonitor.h"
#include "app/ProfileManager.h"
#include "model/Project.h"
#include "model/Feature.h"
#include "core/workflow/blocks/ImageFindBlock.h"
#include "core/workflow/blocks/WaitBlock.h"
#include <QApplication>
#include <QFile>
#include <QTemporaryDir>
#include <QTimer>
#include <iostream>

namespace {
using C=TriggerMonitorController;
bool check(bool value,const char* stage){if(!value)std::cerr<<"Trigger monitor FAILED: "<<stage<<'\n';return value;}
Feature* trigger(Project& project,const std::string& id) {
    auto* f=project.addFeature(id);f->setId(id);f->setRunMode(FeatureRunMode::Trigger);f->setPointerVisualFeedback(false);
    auto image=std::make_unique<ImageFindBlock>();image->templatePaths={"fixture-only-never-executed.png"};f->workflow().addBlock(std::move(image));return f;
}
bool core() {
    QTemporaryDir folder;ProfileManager profiles(folder.path());if(!profiles.initialize())return false;
    const auto owner=profiles.activeProfileId(),other=profiles.addProfile("other",false);
    Project project;auto* a=trigger(project,"a");trigger(project,"b");
    auto* disabled=trigger(project,"disabled");disabled->setEnabled(false);
    auto* empty=project.addFeature("empty");empty->setId("empty");empty->setRunMode(FeatureRunMode::Trigger);
    auto* wrong=trigger(project,"wrong");wrong->setRunMode(FeatureRunMode::RepeatCount);
    C c(profiles);C::Workspace source{&project,owner,1,false};C::Host host;
    std::map<std::string,FeatureRunSession> sessions;int starts=0,stops=0,prepares=0,resumes=0,warnings=0,created=0,launches=0,scans=0;
    std::function<void(const QString&)> onStart;std::function<void()> onPrepare;
    host.workspace=[&]{return source;};host.prepare=[&]{++prepares;c.ensure();if(onPrepare)onPrepare();};host.resume=[&]{++resumes;};
    host.session=[&](const auto& id)->FeatureRunSession*{const auto it=sessions.find(id);return it==sessions.end()?nullptr:&it->second;};
    host.feature=[&](auto& s){return project.featureById(s.featureId);};
    host.active=[](const auto& s){return s.repeatSession;};host.running=[&](const auto& id){const auto* s=host.session(id);return s && s->repeatSession;};
    host.start=[&](const QString& id){++starts;if(!c.suppressed())++warnings;c.persist(id,false);if(onStart)onStart(id);};
    host.stop=[&](const QString& id){++stops;c.persist(id,false);sessions.erase(id.toStdString());};
    host.warning=[&](const auto&){++warnings;};host.sessions=[&]{++scans;std::vector<std::string> ids;for(const auto& e:sessions)ids.push_back(e.first);return ids;};
    host.mayContinue=[](auto&,auto*){return true;};host.ensureEngine=[&](auto& s){++created;s.engine=std::make_unique<WorkflowEngine>();};
    host.launch=[&](auto& s,auto*){++launches;auto wf=std::make_shared<Workflow>();auto wait=std::make_unique<WaitBlock>();wait->ms=10000;wf->addBlock(std::move(wait));s.engine->run(wf,s.sessionContext);};
    c.setHost(host);
    const QStringList original{"a","b","disabled","empty","wrong","missing"};
    if(other.isEmpty() || !profiles.setTriggerArmedFeatureIds(owner,original) || !c.restore()
        || !check(starts==2 && resumes==1 && warnings==0 && !c.suppressed()
            && profiles.triggerArmedFeatureIds(owner)==QStringList{"a","b"},"filter, suppressed persistence and valid restart requests"))return false;
    QApplication::processEvents();
    if(!check(scans==1,"ensure requested during restore runs after suppression unwinds"))return false;
    auto& foreign=sessions["a"];foreign.featureId="a";foreign.profileId=other;foreign.repeatSession=true;
    profiles.setTriggerArmedFeatureIds(other,{"a"});profiles.setTriggerArmedFeatureIds(owner,{"a"});
    if(!c.restore() || !check(starts==2,"foreign session restoration collision"))return false;
    a->setEnabled(false);
    if(!c.prune() || !check(stops==0 && profiles.triggerArmedFeatureIds(owner).isEmpty()
        && profiles.triggerArmedFeatureIds(other)==QStringList{"a"},"invalid active feature cannot stop/disarm foreign owner"))return false;
    if(!c.persist("a",false) || !check(profiles.triggerArmedFeatureIds(other).isEmpty(),"explicit persist resolves started owner"))return false;
    foreign.profileId=owner;profiles.setTriggerArmedFeatureIds(owner,{"a"});
    if(!c.prune() || !check(stops==1 && profiles.triggerArmedFeatureIds(owner).isEmpty(),"invalid owned running trigger stopped"))return false;
    a->setEnabled(true);profiles.setTriggerArmedFeatureIds(owner,original);
    onPrepare=[&]{++source.generation;};const auto before=starts;
    if(!check(!c.restore() && starts==before && !c.suppressed() && profiles.triggerArmedFeatureIds(owner)==original,"prepare generation abort"))return false;
    onPrepare={};onStart=[&](const auto&){++source.generation;};
    if(!check(!c.restore() && starts==before+1 && !c.suppressed() && profiles.triggerArmedFeatureIds(owner)==original,"mid-batch generation abort"))return false;
    onStart=[&](const auto&){profiles.setTriggerArmedFeatureIds(owner,{"b"});};const int beforeChange=starts;
    if(!check(!c.restore() && starts==beforeChange+1 && profiles.triggerArmedFeatureIds(owner)==QStringList{"b"},"callback persisted change wins before next launch"))return false;
    onStart=[&](const auto&){if(c.restore())++warnings;};
    if(!c.restore() || !check(warnings==0 && !c.suppressed(),"reentrant restore suppressed"))return false;
    onStart={};profiles.setTriggerArmedFeatureIds(owner,{"missing"});
    const auto settingsPath=profiles.profileDirectory(owner)+"/profile-settings.json";
    QFile input(settingsPath);if(!input.open(QIODevice::ReadOnly))return false;const auto bytes=input.readAll();input.close();
    HANDLE lock=CreateFileW(reinterpret_cast<LPCWSTR>(settingsPath.utf16()),GENERIC_READ,FILE_SHARE_READ,nullptr,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,nullptr);
    if(lock==INVALID_HANDLE_VALUE)return false;
    const bool blocked=!c.prune();CloseHandle(lock);
    if(!input.open(QIODevice::ReadOnly))return false;const bool exact=input.readAll()==bytes;input.close();
    if(!check(blocked && exact && warnings==1 && profiles.triggerArmedFeatureIds(owner)==QStringList{"missing"} && !c.suppressed(),"locked settings preserve state and report failure")
        || !c.prune() || !check(profiles.triggerArmedFeatureIds(owner).isEmpty(),"unlocked prune retry"))return false;
    auto& monitor=sessions["a"];monitor.featureId="a";monitor.profileId=owner;monitor.runningMode=FeatureRunMode::Trigger;
    monitor.triggerPhase=TriggerSessionPhase::Monitoring;monitor.repeatSession=true;monitor.sessionContext=std::make_shared<ExecutionContext>();
    c.ensure();c.ensure();
    if(!check(created==1 && launches==1 && monitor.engine && monitor.engine->isRunning(),"real Wait engine restart/running skip"))return false;
    monitor.engine->stopAndWaitBounded(1000);monitor.engine.reset();monitor.waitingForScopedTargetForeground=true;c.ensure();
    if(!check(created==1,"waiting monitor skip"))return false;
    monitor.waitingForScopedTargetForeground=false;
    host.ensureEngine=[&](auto& s){++created;s.engine=std::make_unique<WorkflowEngine>();s.sessionContext=std::make_shared<ExecutionContext>();};c.setHost(host);c.ensure();
    if(!check(created==2 && launches==1,"replacement context during engine creation"))return false;
    sessions.clear();profiles.setTriggerArmedFeatureIds(owner,{"b"});const int scheduled=prepares;
    c.scheduleRestore();c.scheduleRestore();QApplication::processEvents();
    if(!check(prepares==scheduled+1,"coalesced restore timer"))return false;
    c.scheduleRestore();c.scheduleEnsure();c.stop();QApplication::processEvents();
    if(!check(prepares==scheduled+1,"stop cancels queued work"))return false;
    auto* dying=new C(profiles);auto destructive=host;destructive.prepare=[&]{delete dying;dying=nullptr;};dying->setHost(destructive);dying->restore();
    if(!check(!dying,"lifetime during prepare"))return false;
    std::cout<<"Trigger monitor: persisted filtering/owner collision, callback generation/reentry/lifetime, real settings lock/retry, coalesced stop and real Wait-engine restart/context replacement OK (recorded launch ports; no ImageFind capture/native input)\n";
    return true;
}
}

class TriggerMonitorUiChecks {
public:
    static bool run() {
        MainWindow w;w.m_foregroundMonitor->stop();auto* selected=trigger(*w.m_project,"trigger-ui");
        const auto owner=w.m_profileManager->activeProfileId();
        int starts=0,warnings=0;auto host=w.m_triggerMonitor->m_host;
        host.prepare=[]{};host.resume=[]{};host.start=[&](const auto&){++starts;w.persistTriggerArmedState("trigger-ui",false);};
        host.warning=[&](const auto&){++warnings;};w.m_triggerMonitor->setHost(host);
        if(!w.m_profileManager->setTriggerArmedFeatureIds(owner,{"trigger-ui","missing-ui"}))return false;
        w.restorePersistedTriggerSessions();
        if(!check(starts==1 && w.m_profileManager->triggerArmedFeatureIds(owner)==QStringList{"trigger-ui"} && !w.m_suppressTriggerArmedPersist,"MainWindow restore wiring/suppression alias"))return false;
        selected->setEnabled(false);w.prunePersistedTriggerArmedFeatures();
        if(!check(w.m_profileManager->triggerArmedFeatureIds(owner).isEmpty(),"MainWindow prune wiring"))return false;
        selected->setEnabled(true);w.m_profileManager->setTriggerArmedFeatureIds(owner,{"trigger-ui","missing-ui"});
        host.start=[&](const auto&){
            ++starts;auto staged=w.m_projectWorkspace->stageProfile(owner);if(!staged)return;
            staged->project=std::make_unique<Project>();trigger(*staged->project,"trigger-ui");
            w.m_projectWorkspace->install(std::move(staged),true,false,ProjectWorkspaceController::InstallMode::ValidatedTransition);
        };w.m_triggerMonitor->setHost(host);
        w.restorePersistedTriggerSessions();
        if(!check(starts==2 && !w.m_suppressTriggerArmedPersist && w.m_profileManager->triggerArmedFeatureIds(owner)==QStringList{"trigger-ui","missing-ui"},"actual workspace replacement preserves old armed snapshot"))return false;
        w.m_profileManager->setTriggerArmedFeatureIds(owner,{});
        w.scheduleRestorePersistedTriggerSessions();
        if(!w.prepareForShutdown())return false;
        if(!check(w.m_triggerMonitor->m_stopped && !w.m_triggerMonitor->m_restore->isActive()
            && !w.m_triggerMonitor->m_ensure->isActive(),"actual shutdown stops controller timers"))return false;
        QApplication::processEvents();
        if(!check(starts==2 && warnings==0,"shutdown cancels timer"))return false;
        std::cout<<"Trigger monitor MainWindow: restore/prune slots, suppression alias, actual mid-restore workspace replacement and shutdown timer cancellation OK (offscreen; recorded starts; no native capture/input)\n";
        return true;
    }
};
bool runTriggerMonitorChecks(){return core() && TriggerMonitorUiChecks::run();}
