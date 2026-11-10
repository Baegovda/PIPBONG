#include "app/CaptureTargetController.h"
#include "app/ProfileManager.h"
#include "app/ProfileForegroundResolver.h"
#include "app/TargetWindowController.h"
#include "app/FeatureRunSession.h"
#include "app/RunContextController.h"
#include "model/Feature.h"
#include "model/Project.h"
#include "core/capture/ScreenCapture.h"
#include "core/workflow/Workflow.h"
#include "core/workflow/blocks/WaitBlock.h"
#include <QTemporaryDir>
#include <iostream>
#include <map>
namespace {
using Controller=CaptureTargetController;
using Scope=FeatureCaptureTargetScope;
bool check(bool value,const char* message) {if(!value)std::cerr<<"Capture target FAILED: "<<message<<'\n';return value;}
Controller::Handle marker(int id){return reinterpret_cast<Controller::Handle>(quintptr(id));}
struct Fixture {
    std::map<Controller::Handle,Controller::Window> windows;
    ForegroundWindowState fg;
    QString main="GameWindow";
    bool allowed=true,write=true,wrongLookup=false;
    int applies=0,notifications=0;
    Controller::Applied last;
    Controller::Ports ports() {
        return {[this](Controller::Handle hwnd)->std::optional<Controller::Window> {
            auto found=windows.find(hwnd);return found==windows.end()?std::nullopt:std::optional<Controller::Window>(found->second);
        },[this](const QString& title,const QString& path)->std::optional<Controller::Window> {
            if(wrongLookup)return windows.at(marker(3));
            for(const auto& entry:windows)if((path.isEmpty()?entry.second.title.contains(title,Qt::CaseInsensitive)
                :entry.second.path.compare(path,Qt::CaseInsensitive)==0) && entry.second.visible)return entry.second;
            return std::nullopt;
        },[this](const Controller::Applied& value){++applies;last=value;}};
    }
    Controller::Host host() {
        Controller::Host value;value.mainTitle=[this]{return main;};value.foreground=[this]{return fg;};
        value.foregroundAllowed=[this](const QString&){return allowed;};value.mayApply=[this]{return write;};
        value.monitorChanged=[this]{++notifications;};return value;
    }
};
bool nativeConstraints(ProfileManager& profiles) {
#ifdef _WIN32
    const HWND hwnd=GetForegroundWindow();ScreenCapture::TargetWindowInfo info;
    if(hwnd && ScreenCapture::queryWindowInfo(hwnd,info) && !info.title.empty() && !info.processPath.empty()) {
        const QString title=QString::fromStdWString(info.title),path=QString::fromStdWString(info.processPath);
        const QString absent="C:/__capture_absent__/not-the-foreground.exe";
        if(!check(ForegroundRunGate::foregroundMatchesScopedMainTarget(title,hwnd,title,"unrelated sub",path,absent)
            && ForegroundRunGate::foregroundMatchesScopedSubTarget(title,hwnd,"unrelated main",title,absent,path),
            "native positive control accepts the actual executable for each role"))return false;
        const bool rejected=!ForegroundRunGate::foregroundMatchesScopedMainTarget(title,hwnd,title,"unrelated sub",absent,absent)
            && !ForegroundRunGate::foregroundMatchesScopedSubTarget(title,hwnd,"unrelated main",title,absent,absent);
        ForegroundRunGate::GateInput input;input.bindings={"known",false,title,"unrelated sub",absent,absent};
        input.foreground.rootHwnd=hwnd;input.foreground.title=title;input.foreground.processPath=path;
        if(!check(rejected && !ForegroundRunGate::activeProfileForegroundBindingMatches(input)
            && !ForegroundRunGate::profileMainOrSubForegroundActive(input),"native known-process mismatch cannot fall back to matching title"))return false;
        const auto id=profiles.addProfile("native constraint",false);
        if(id.isEmpty() || !profiles.updateProfileTargetBinding(id,title,absent,true) || !profiles.setActiveProfile(id))return false;
        ProfileForegroundResolver::healProfileBindingsFromForeground(profiles,input.foreground);
        TargetWindowController target;target.setProfileManager(&profiles);
        TargetWindowController::HostCallbacks host;host.currentTargetWindowTitleW=[title]{return title.toStdWString();};
        host.isActiveDefaultProfile=[]{return false;};target.setHostCallbacks(host);
        target.healLinkedTargetProcessPathFromForeground(hwnd,title);target.rememberProfileLinkedForeground(hwnd,title);
        if(!check(profiles.linkedTargetProcessPath(id)==absent && !target.lastProfileLinkedForegroundHwnd(),
            "neither healer nor remembered-target cache trusts another executable with the same title"))return false;
        std::cout<<"Capture native constraints: existing foreground read accepts actual executable, rejects wrong executable in main/sub/run gates and preserves known metadata/cache OK (no activation/input)\n";
    } else std::cout<<"Capture native constraints: existing window metadata unavailable; native check not verified\n";
#endif
    return true;
}
}
bool runCaptureTargetChecks() {
    const bool oldWithout=ProgramSettings::runWithoutTargetWindow();ProgramSettings::setRunWithoutTargetWindow(false);
    struct Restore {bool value;~Restore(){ProgramSettings::setRunWithoutTargetWindow(value);}} restore{oldWithout};
    QTemporaryDir directory;ProfileManager profiles(directory.path());if(!profiles.initialize())return false;
    Fixture fixture;
    fixture.windows[marker(1)]={marker(1),"GameWindow","main.exe",11,true,false};
    fixture.windows[marker(2)]={marker(2),"Game","sub.exe",22,true,false};
    fixture.windows[marker(3)]={marker(3),"GameWindow","wrong.exe",33,true,false};
    fixture.fg.rootHwnd=marker(1);
    Controller controller(profiles,fixture.ports());controller.setHost(fixture.host());
    if(!check(controller.choose().title.isEmpty() && controller.bindings().mainBinding.isEmpty(),"global profile never acquires an ambient target"))return false;
    const auto owner=profiles.addProfile("owner",false),other=profiles.addProfile("other",false);
    if(owner.isEmpty() || other.isEmpty() || !profiles.updateProfileTargetBinding(owner,"GameWindow","main.exe",true)
        || !profiles.updateProfileSubTargetBinding(owner,"Game","sub.exe",true) || !profiles.setActiveProfile(owner))return false;
    auto chosen=controller.choose();
    if(!check(chosen.title=="GameWindow" && chosen.path=="main.exe" && !chosen.sub,"main title containing sub text retains main executable"))return false;
    RunContextController::TargetBinding binding{L"Game",L"main.exe",L"sub.exe",L"GameWindow"};
    if(!check(RunContextController::processPath(binding,Scope::Auto,L"GameWindow")==L"main.exe"
        && RunContextController::processPath(binding,Scope::Auto,L" gameWINDOW ")==L"main.exe"
        && RunContextController::processPath(binding,Scope::Auto,L"GameWindow - client")==L"main.exe",
        "legacy title-to-path routing prefers exact/specific main binding over shorter sub text"))return false;
    fixture.fg.rootHwnd=marker(3);fixture.allowed=false;
    if(!check(controller.choose().title.isEmpty() && controller.choose(Scope::Auto,true).path=="main.exe",
        "wrong foreground blocks normal auto but explicit background mode finds constrained visible target"))return false;
    fixture.wrongLookup=true;
    if(!check(!controller.backgroundVisible(Scope::MainOnly),"background visibility cannot accept another program sharing its title"))return false;
    fixture.wrongLookup=false;fixture.allowed=true;fixture.fg.rootHwnd=nullptr;
    fixture.windows[marker(1)].minimized=true;
    if(!check(controller.choose().path=="sub.exe","minimized main yields to visible sub"))return false;
    fixture.windows[marker(1)].minimized=false;
    fixture.write=false;controller.sync();if(!check(fixture.applies==0,"capture owner guard blocks global writes"))return false;
    fixture.write=true;controller.sync();if(!check(fixture.last.target.path=="main.exe","idle capture applies coherent title/process"))return false;
    fixture.windows.erase(marker(1));fixture.windows.erase(marker(2));controller.applyTitle("GameWindow");
    if(!check(!fixture.last.target.hwnd,"unresolved target explicitly clears stale HWND"))return false;

    fixture.main="Same";profiles.updateProfileTargetBinding(owner,"Same","main.exe",true);profiles.updateProfileSubTargetBinding(owner,"Same","sub.exe",true);
    fixture.windows[marker(1)]={marker(1),"Same","main.exe",11,true,false};fixture.windows[marker(2)]={marker(2),"Same","sub.exe",22,true,false};
    fixture.fg.rootHwnd=marker(2);chosen=controller.choose();
    if(!check(chosen.title=="Same" && chosen.path=="sub.exe" && chosen.sub,"identical titles carry the selected executable rather than re-inferring it"))return false;
    Feature feature("watch");feature.setRunMode(FeatureRunMode::Trigger);feature.setTriggerRunWithoutTargetForeground(true);
    FeatureRunSession session;session.profileId=owner;session.runningMode=FeatureRunMode::Trigger;session.triggerPhase=TriggerSessionPhase::Monitoring;
    session.lockedCaptureTargetTitle=L"Same";session.lockedCaptureTargetProcessPath=L"main.exe";session.sessionContext=std::make_shared<ExecutionContext>();
    session.sessionContext->setTargetWindowBindingForWorker(L"Same",L"main.exe");
    if(!check(controller.refreshSession(session,feature) && session.sessionContext->targetWindowBinding()==std::make_pair(std::wstring(L"Same"),std::wstring(L"sub.exe")),
        "same-title trigger migration atomically updates its executable"))return false;
    if(!profiles.updateProfileTargetBinding(other,"Second","second.exe",true) || !profiles.setActiveProfile(other))return false;
    fixture.main="Second";fixture.windows[marker(4)]={marker(4),"Second","second.exe",44,true,false};fixture.fg.rootHwnd=marker(1);
    std::map<std::string,FeatureRunSession> sessions;sessions.emplace("watch",std::move(session));
    auto host=fixture.host();host.feature=[&](FeatureRunSession&)->Feature*{return &feature;};controller.setHost(host);controller.setSessions(&sessions);
    fixture.write=false;const int oldApplies=fixture.applies,oldNotifications=fixture.notifications;controller.sync();
    if(!check(sessions.at("watch").lockedCaptureTargetProcessPath==L"main.exe" && fixture.applies==oldApplies && fixture.notifications==oldNotifications+1,
        "background owner session uses its own main/sub settings while active profile differs; global capture remains unchanged"))return false;
    auto& running=sessions.at("watch");running.runningMode=FeatureRunMode::RepeatCount;feature.setRunMode(FeatureRunMode::RepeatCount);
    running.engine=std::make_unique<WorkflowEngine>();auto flow=std::make_shared<Workflow>();auto wait=std::make_unique<WaitBlock>();wait->ms=5000;flow->addBlock(std::move(wait));
    running.engine->run(flow,running.sessionContext);fixture.windows.erase(marker(1));fixture.fg.rootHwnd=marker(2);
    const bool stable=!controller.refreshSession(running,feature) && running.lockedCaptureTargetProcessPath==L"main.exe";
    running.engine->stopAndWaitBounded(1000);
    if(!check(stable && controller.refreshSession(running,feature) && running.sessionContext->targetWindowBinding().second==L"sub.exe",
        "live normal engine stays locked; idle missing-target adoption updates its context coherently"))return false;
    running.runningMode=FeatureRunMode::Trigger;running.triggerPhase=TriggerSessionPhase::RunningAction;
    fixture.windows[marker(1)]={marker(1),"Same","main.exe",11,true,false};fixture.windows.erase(marker(2));fixture.fg.rootHwnd=marker(1);
    if(!check(!controller.refreshSession(running,feature) && running.lockedCaptureTargetProcessPath==L"sub.exe",
        "queued trigger action stays locked even if its prior window disappears before engine launch"))return false;
    if(!profiles.removeProfile(owner) || !check(!controller.refreshSession(running,feature) && running.lockedCaptureTargetProcessPath==L"sub.exe",
        "deleted owner cannot fall back to active-profile target"))return false;
    FeatureRunSession missing;missing.profileId=owner;
    int lookups=0;auto fastHost=fixture.host();fastHost.feature=[&](FeatureRunSession&)->Feature*{++lookups;return &feature;};
    controller.setHost(fastHost);
    if(!check(controller.sessionTitleForRun(running,false)==running.lockedCaptureTargetTitle && lookups==0,
        "locked-title repeat lookup returns without loading or searching owner model"))return false;
    if(!check(controller.sessionTitle(missing,&feature,false).isEmpty() && controller.sessionTitle(missing,nullptr,false).isEmpty(),
        "uninitialized missing-owner title lookup cannot inherit active-profile target"))return false;
    if(!nativeConstraints(profiles))return false;
    const auto unlinked=profiles.addProfile("unlinked",false),known=profiles.addProfile("known process",false);
    if(unlinked.isEmpty() || known.isEmpty() || !profiles.updateProfileTargetBinding(unlinked,"Game",{},true)
        || !profiles.updateProfileSubTargetBinding(unlinked,"Game Launcher",{},true))return false;
    ForegroundWindowState learned;learned.rootHwnd=marker(99);learned.title="Game Launcher";learned.processPath="new-launcher.exe";
    ProfileForegroundResolver::healProfileBindingsFromForeground(profiles,learned);
    if(!check(profiles.linkedTargetProcessPath(unlinked).isEmpty() && profiles.subLinkedTargetProcessPath(unlinked)=="new-launcher.exe",
        "unknown metadata learns the more specific sub binding without overwriting main"))return false;
    profiles.updateProfileTargetBinding(unlinked,"Shared",{},true);profiles.updateProfileSubTargetBinding(unlinked,{}, {},true);
    profiles.updateProfileTargetBinding(known,"Shared","owned.exe",true);
    learned.title="Shared";learned.processPath="owned.exe";
    ProfileForegroundResolver::healProfileBindingsFromForeground(profiles,learned);
    if(!check(profiles.linkedTargetProcessPath(unlinked).isEmpty() && profiles.linkedTargetProcessPath(known)=="owned.exe",
        "unknown binding cannot learn an executable already owned by another profile"))return false;
    std::cout<<"Capture targets: process constraints, background visibility, exact/identical titles, owner-scoped session migration, coherent context pair, live-engine/global isolation and stale-HWND clear OK (scripted windows; real Wait engine; no native activation/input)\n";
    return true;
}
