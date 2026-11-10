#include "app/ProjectWorkspaceController.h"
#include "app/ProfileManager.h"
#include "model/Project.h"
#include "storage/JsonSerializer.h"
#include <QDir>
#include <QFile>
#include <QTemporaryDir>
#include <iostream>
#ifdef _WIN32
#include <windows.h>
#endif
namespace {
bool check(bool value,const char* what) {if(!value)std::cerr<<"Target binding FAILED: "<<what<<'\n';return value;}
#ifdef _WIN32
struct Lock {
    HANDLE handle=INVALID_HANDLE_VALUE;
    explicit Lock(const QString& path) {handle=CreateFileW(reinterpret_cast<LPCWSTR>(path.utf16()),GENERIC_READ,FILE_SHARE_READ,nullptr,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,nullptr);}
    ~Lock() {if(handle!=INVALID_HANDLE_VALUE)CloseHandle(handle);}
    bool valid() const {return handle!=INVALID_HANDLE_VALUE;}
};
#endif
QString titleOnDisk(const QString& path) {
    const auto project=JsonSerializer::loadFromFile(path);
    return project?QString::fromStdString(project->targetWindowTitle()):QStringLiteral("__unreadable__");
}
}
bool runTargetBindingChecks() {
    QTemporaryDir fixture;ProfileManager profiles(fixture.path());
    if(!profiles.initialize())return false;
    ProjectWorkspaceController workspace(profiles,profiles.activeProjectPath());
    bool allowed=true;
    ProjectWorkspaceController::HostCallbacks host;
    host.mayReplace=[&]{return allowed;};host.mayWrite=[&]{return allowed;};
    workspace.setHost(host);
    if(!workspace.loadActive(true,false))return false;
    if(!check(!workspace.commitTargetBinding(profiles.activeProfileId(),workspace.generation(),false,"New","new.exe"),"global default cannot be rebound"))return false;
    const auto id=profiles.addProfile("target binding",false);
    Project project;project.setTargetWindowTitle("Old");project.addFeature("preserved feature");
    if(id.isEmpty() || !JsonSerializer::saveToFile(project,profiles.projectPath(id),profiles.projectDirectory(id))
        || !profiles.setActiveProfile(id) || !profiles.updateProfileTargetBinding(id,"Old","old.exe",true)
        || !profiles.updateProfileSubTargetBinding(id,"OldSub","old-sub.exe",true)
        || !workspace.loadActive(true,false))return false;
    const auto generation=workspace.generation();
    const auto settings=QDir(profiles.profileDirectory(id)).filePath("profile-settings.json");
    if(!check(!workspace.commitTargetBinding("other",generation,false,"New","new.exe")
        && !workspace.commitTargetBinding(id,generation-1,false,"New","new.exe"),"owner/generation mismatch is rejected"))return false;
    allowed=false;
    if(!check(!workspace.commitTargetBinding(id,generation,false,"New","new.exe"),"editor/write gate rejects commit"))return false;
    allowed=true;
#ifdef _WIN32
    workspace.state().project->addFeature("unsaved feature");workspace.state().modified=true;
    {
        Lock locked(profiles.projectPath(id));
        if(!locked.valid() || !check(!workspace.commitTargetBinding(id,generation,false,"New","new.exe")
            && workspace.state().project->targetWindowTitle()=="Old" && workspace.state().modified
            && workspace.state().project->features().size()==2 && profiles.linkedTargetProcessPath(id)=="old.exe"
            && titleOnDisk(profiles.projectPath(id))=="Old","locked project retains title, dirty edits and metadata"))return false;
    }
    {
        Lock locked(settings);
        if(!locked.valid() || !check(!workspace.commitTargetBinding(id,generation,false,"New","new.exe")
            && titleOnDisk(profiles.projectPath(id))=="Old" && profiles.targetWindowTitle(id)=="Old"
            && profiles.linkedTargetProcessPath(id)=="old.exe"
            && !workspace.commitTargetBinding(id,generation,true,"NewSub","new-sub.exe")
            && profiles.subTargetWindowTitle(id)=="OldSub" && profiles.subLinkedTargetProcessPath(id)=="old-sub.exe",
            "locked settings roll main JSON back and retain both in-memory bindings"))return false;
    }
    // Force the narrower double failure: settings refuse commit and an external reader locks
    // project.json after its first successful write. Keep the original model dirty for retry.
    std::unique_ptr<Lock> lateProjectLock;
    host.changed=[&] {
        if(workspace.state().project->targetWindowTitle()=="Retry" && !lateProjectLock)
            lateProjectLock=std::make_unique<Lock>(profiles.projectPath(id));
    };
    workspace.setHost(host);
    {
        Lock locked(settings);
        const bool failed=!workspace.commitTargetBinding(id,generation,false,"Retry","retry.exe");
        if(!check(locked.valid() && lateProjectLock && lateProjectLock->valid() && failed
            && workspace.state().project->targetWindowTitle()=="Old" && workspace.state().modified
            && workspace.lastError().contains(QStringLiteral("복원 저장"))
            && titleOnDisk(profiles.projectPath(id))=="Retry" && profiles.targetWindowTitle(id)=="Old",
            "rollback save failure retains old dirty model and reports the partial disk state"))return false;
    }
    lateProjectLock.reset();host.changed={};workspace.setHost(host);
    if(!check(workspace.save(true) && titleOnDisk(profiles.projectPath(id))=="Old","unlock and save repairs the explicitly reported partial write"))return false;
#endif
    if(!check(workspace.commitTargetBinding(id,generation,false,"New","new.exe")
        && titleOnDisk(profiles.projectPath(id))=="New" && profiles.targetWindowTitle(id)=="New"
        && profiles.linkedTargetProcessPath(id)=="new.exe" && !workspace.state().modified,
        "main title and process publish after successful persistence"))return false;
    if(!check(workspace.commitTargetBinding(id,generation,true,"NewSub","new-sub.exe")
        && profiles.subTargetWindowTitle(id)=="NewSub" && profiles.subLinkedTargetProcessPath(id)=="new-sub.exe"
        && titleOnDisk(profiles.projectPath(id))=="New","sub binding persists independently of main JSON"))return false;
    if(!check(workspace.commitTargetBinding(id,generation,false,"UnknownProcess",{})
        && profiles.linkedTargetProcessPath(id).isEmpty(),"explicit new selection clears an unknown executable instead of retaining another program"))return false;
    if(!workspace.loadActive(true,false))return false;
    if(!check(workspace.generation()!=generation && !workspace.commitTargetBinding(id,generation,false,"Late","late.exe"),"same-profile model reload invalidates old picker ticket"))return false;
    ProfileManager restarted(fixture.path());
    if(!check(restarted.initialize() && restarted.targetWindowTitle(id)=="UnknownProcess"
        && restarted.linkedTargetProcessPath(id).isEmpty() && restarted.subTargetWindowTitle(id)=="NewSub",
        "fresh manager reload agrees with committed disk metadata"))return false;
    std::cout<<"Target binding: owner/generation/default/editor gates, locked project/settings retention, main rollback, explicit rollback-save failure/retry, sub persistence and fresh reload OK (real fixture file locks; no native input)\n";
    return true;
}
