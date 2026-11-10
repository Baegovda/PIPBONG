#include "app/TargetCenterPinController.h"
#include "app/TargetWindowCenterPin.h"
#include "app/ProjectWorkspaceController.h"
#include "app/ProfileManager.h"
#include "model/Project.h"
#include "storage/JsonSerializer.h"
#include <QCoreApplication>
#include <QDir>
#include <QTemporaryDir>
#include <QTimer>
#include <iostream>
#include <memory>
namespace {
using Controller=TargetCenterPinController;
bool check(bool value,const char* message) {if(!value)std::cerr<<"Center pin FAILED: "<<message<<'\n';return value;}
struct Harness {
    Controller::Snapshot state{"one","Main","Sub","main.exe","sub.exe",1,false,false,false,false,true};
    QString error;bool mainView=false,subView=false,wrongPath=false,hidden=false,minimized=false,duplicate=false;
    int resolves=0,moves=0,changes=0;bool forced=false;
    std::function<void()> resolving,moving;
    Controller::Ports ports() {
        return {[this](const QString& title,const QString& path)->std::optional<Controller::Window> {
            ++resolves;if(resolving)resolving();
            return Controller::Window{reinterpret_cast<Controller::Handle>(quintptr(duplicate || title=="Main"?1:2)),wrongPath?QStringLiteral("wrong.exe"):path,42,!hidden,minimized};
        },[this](const Controller::Window&,bool force){++moves;forced=force;if(moving)moving();return true;}};
    }
    Controller::Host host() {
        Controller::Host host;host.snapshot=[this]{return state;};
        host.save=[this](const auto&,bool main,bool sub){if(error.isEmpty()){state.main=main;state.sub=sub;}return error;};
        host.view=[this](bool main,bool sub){mainView=main;subView=sub;};host.changed=[this]{++changes;};
        return host;
    }
};
bool geometry() {
    const QRect monitor(-1920,-1080,1920,1080),frame(-1800,-1000,1280,720),raw(-1808,-1008,1296,736);
    const auto point=TargetWindowCenterPin::placement(frame,raw,monitor);
    if(!check(point && *point==QPoint(-1608,-908),"negative monitor and invisible frame offset produce centered visible frame"))return false;
    if(!check(!TargetWindowCenterPin::placement(QRect(-1600,-900,1280,720),QRect(-1608,-908,1296,736),monitor)
        && TargetWindowCenterPin::placement(QRect(-1600,-900,1280,720),QRect(-1608,-908,1296,736),monitor,true)==point,
        "already centered tolerance versus explicit force"))return false;
    if(!check(!TargetWindowCenterPin::placement(QRect(-1598,-898,1280,720),raw,monitor)
        && TargetWindowCenterPin::placement(QRect(-1597,-897,1280,720),raw,monitor).has_value(),"two-pixel frame tolerance"))return false;
    if(!check(TargetWindowCenterPin::placement(QRect(-1800,-1000,1281,721),raw,monitor)==QPoint(-1609,-909)
        && TargetWindowCenterPin::placement(QRect(-1800,-1000,2560,1440),raw,monitor)==QPoint(-2248,-1268),
        "odd and larger-than-monitor frames preserve offsets without resizing"))return false;
    return check(!TargetWindowCenterPin::placement(QRect(),raw,monitor)
        && !TargetWindowCenterPin::placement(frame,QRect(),monitor)
        && !TargetWindowCenterPin::placement(QRect(-1000000000,-1000000000,100,100),QRect(2000000000,2000000000,100,100),QRect(1000000000,1000000000,100,100)),
        "invalid or overflowing geometry is rejected");
}
bool scheduling() {
    Harness h;Controller controller(h.ports());controller.setHost(h.host());
    h.error="save failed";controller.toggle(Controller::Role::Main,true);controller.tick();
    if(!check(!h.mainView && !h.state.main && h.moves==0,"failed preference save restores view and never moves"))return false;
    h.error.clear();h.state.movementAllowed=false;controller.toggle(Controller::Role::Main,true);controller.tick();
    if(!check(h.mainView && h.state.main && h.resolves==0,"saving enabled preference during burst defers every native call"))return false;
    h.state.movementAllowed=true;controller.tick();
    if(!check(h.moves==1 && h.forced && h.changes==1,"deferred enable applies one forced correction when idle"))return false;
    for(int i=0;i<40;++i)controller.reconcile();
    const auto timers=controller.findChildren<QTimer*>();
    if(!check(timers.size()==2,"reconcile retains only periodic and one coalesced queued timer"))return false;
    for(auto* timer:timers)if(!timer->isSingleShot())timer->stop();
    const int before=h.moves;QCoreApplication::processEvents();
    if(!check(h.moves==before+1,"forty reconcile calls yield one queued tick"))return false;
    h.state.blocked=true;controller.tick();
    if(!check(h.moves==before+1,"transaction/editor gate suppresses movement"))return false;
    h.state.blocked=false;h.hidden=true;controller.tick();h.hidden=false;h.minimized=true;controller.tick();h.minimized=false;
    h.wrongPath=true;controller.tick();h.wrongPath=false;
    if(!check(h.moves==before+1,"hidden/minimized/wrong-process candidates are ignored"))return false;
    h.state.sub=true;h.state.subPath=h.state.mainPath;h.duplicate=true;controller.tick();
    if(!check(h.moves==before+2,"main/sub sharing one HWND is moved once"))return false;
    h.resolving=[&]{++h.state.generation;};controller.tick();h.resolving={};
    if(!check(h.moves==before+2,"document change during lookup invalidates planned movement"))return false;
    h.duplicate=false;h.state.subPath="sub.exe";h.moving=[&]{h.state.mainTitle="changed";};controller.tick();h.moving={};
    if(!check(h.moves==before+3,"binding change after first move cancels remaining plan"))return false;
    h.state.globalDefault=true;controller.reconcile();
    if(!check(!h.mainView && !h.subView,"global profile shows no center-pin options"))return false;
    for(auto* timer:timers)if(!check(!timer->isActive(),"global profile stops both timers"))return false;
    Harness dead;auto owner=std::make_unique<Controller>(dead.ports());owner->setHost(dead.host());dead.state.main=true;
    dead.resolving=[&]{owner.reset();};owner->tick();
    return check(!owner && dead.moves==0,"destroyed controller drops pending lookup safely");
}
bool persistence() {
    const bool oldMain=ProgramSettings::pinTargetWindowToScreenCenter(),oldSub=ProgramSettings::pinSubTargetWindowToScreenCenter();
    struct Restore {bool main,sub;~Restore(){ProgramSettings::setPinTargetWindowToScreenCenter(main);ProgramSettings::setPinSubTargetWindowToScreenCenter(sub);}} restore{oldMain,oldSub};
    QTemporaryDir fixture;ProfileManager profiles(fixture.path());if(!profiles.initialize())return false;
    ProjectWorkspaceController workspace(profiles,profiles.activeProjectPath());
    bool allowed=true;ProjectWorkspaceController::HostCallbacks host;host.mayReplace=[&]{return allowed;};host.mayWrite=[&]{return allowed;};workspace.setHost(host);
    if(!workspace.loadActive(true,false) || !check(!workspace.saveCenterPinOptions(profiles.activeProfileId(),workspace.generation(),true,false),"default preference writes are rejected"))return false;
    const auto id=profiles.addProfile("center pin",true);Project project;project.setTargetWindowTitle("Main");
    if(id.isEmpty() || !JsonSerializer::saveToFile(project,profiles.projectPath(id),profiles.projectDirectory(id)) || !workspace.loadActive(true,false))return false;
    const auto epoch=workspace.generation();
    allowed=false;if(!check(!workspace.saveCenterPinOptions(id,epoch,true,false),"editor/write gate blocks preference save"))return false;allowed=true;
#ifdef _WIN32
    const auto path=QDir(profiles.profileDirectory(id)).filePath("profile-settings.json");
    const auto locked=CreateFileW(reinterpret_cast<LPCWSTR>(path.utf16()),GENERIC_READ,FILE_SHARE_READ,nullptr,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,nullptr);
    if(locked==INVALID_HANDLE_VALUE)return false;
    const bool retained=!workspace.saveCenterPinOptions(id,epoch,true,false) && !ProgramSettings::pinTargetWindowToScreenCenter()
        && !profiles.loadSettings(id).pinTargetWindowToScreenCenter;CloseHandle(locked);
    if(!check(retained,"locked settings retain runtime/cache flags before publication"))return false;
#endif
    if(!check(workspace.saveCenterPinOptions(id,epoch,true,true) && ProgramSettings::pinTargetWindowToScreenCenter()
        && ProgramSettings::pinSubTargetWindowToScreenCenter(),"successful save publishes both runtime flags"))return false;
    ProfileManager fresh(fixture.path());
    if(!check(fresh.initialize() && fresh.loadSettings(id).pinTargetWindowToScreenCenter && fresh.loadSettings(id).pinSubTargetWindowToScreenCenter,
        "fresh manager reads saved options"))return false;
    if(!workspace.loadActive(true,false) || !check(!workspace.saveCenterPinOptions(id,epoch,false,false),"old document epoch cannot save preferences"))return false;
    return check(workspace.saveCenterPinOptions(id,workspace.generation(),false,false) && !ProgramSettings::pinTargetWindowToScreenCenter()
        && !ProgramSettings::pinSubTargetWindowToScreenCenter(),"disable persists and publishes both flags");
}
}
bool runTargetCenterPinChecks() {
    if(!geometry() || !scheduling() || !persistence())return false;
    std::cout<<"Center pin: frame-offset/negative-monitor/odd/oversized geometry, tolerance, coalesced timers, deferred force, context/visibility/exe guards, dedup, owner lifetime and locked-settings publication OK (pure geometry/scripted move ports; no native movement/input)\n";
    return true;
}
