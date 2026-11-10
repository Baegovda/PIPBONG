#include "app/ProjectWorkspaceController.h"
#include "app/ProfileTransitionController.h"
#include "app/ProfileManager.h"
#include "model/Project.h"
#include "storage/JsonSerializer.h"
#include "storage/ProjectPackage.h"
#include "app/ProfilePackageController.h"
#include "core/workflow/blocks/ImageFindBlock.h"
#include <QImage>
#include <QColor>
#include <QCoreApplication>
#include <QDir>
#include <QElapsedTimer>
#include <QEventLoop>
#include <QFile>
#include <QTemporaryDir>
#include <QThread>
#include <QTimer>
#include <iostream>
bool runProfilePackageImportChecks();
#ifdef _WIN32
#include <windows.h>
#endif

namespace {
bool expect(bool value,const char* label) {
    if(!value) std::cerr<<"Project workspace check failed: "<<label<<'\n';
    return value;
}
bool until(const std::function<bool()>& predicate) {
    QElapsedTimer timer;timer.start();
    while(!predicate() && timer.elapsed()<4000) { QCoreApplication::processEvents();QThread::msleep(1); }
    return predicate();
}
bool saveProfile(ProfileManager& profiles,const QString& id,const char* name) {
    Project project;project.addFeature(name);
    return JsonSerializer::saveToFile(project,profiles.projectPath(id),profiles.projectDirectory(id));
}
#ifdef _WIN32
struct ReadLock {
    HANDLE handle=INVALID_HANDLE_VALUE;
    explicit ReadLock(const QString& path) {
        handle=CreateFileW(reinterpret_cast<LPCWSTR>(path.utf16()),GENERIC_READ,FILE_SHARE_READ,nullptr,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,nullptr);
    }
    ~ReadLock() { if(handle!=INVALID_HANDLE_VALUE) CloseHandle(handle); }
    bool valid() const { return handle!=INVALID_HANDLE_VALUE; }
};
#endif
}

bool runProjectWorkspaceChecks() {
    if(!runProfilePackageImportChecks())return false;
    const auto original=ProgramSettings::profileSettings();
    struct Restore { ProgramSettings::ProfileSettings value;~Restore(){ProgramSettings::applyProfileSettings(value);} } restore{original};
    QTemporaryDir fixture;
    ProfileManager profiles(fixture.path());
    if(!fixture.isValid() || !profiles.initialize()) return false;
    const auto initial=profiles.activeProfileId();
    auto inherited=ProgramSettings::profileSettings();
    inherited.linkedTargetProcessPath="C:/old.exe";inherited.subTargetWindowTitle="old launcher";
    inherited.subLinkedTargetProcessPath="C:/old-launcher.exe";inherited.triggerArmedFeatureIds={QStringLiteral("old-feature")};
    ProgramSettings::applyProfileSettings(inherited);
    const auto one=profiles.addProfile("one",false),two=profiles.addProfile("two",false);
    if(!expect(!one.isEmpty() && !two.isEmpty() && profiles.activeProfileId()==initial
        && profiles.linkedTargetProcessPath(one).isEmpty() && profiles.subTargetWindowTitle(one).isEmpty()
        && profiles.triggerArmedFeatureIds(one).isEmpty(),"inactive profile creation preserves selection without inheriting bindings/armed ids")) return false;
    if(!saveProfile(profiles,one,"one-feature") || !saveProfile(profiles,two,"two-feature")) return false;
    bool pinned=false,writable=true,memoSaved=true;
    int views=0,detach=0;
    ProjectWorkspaceController workspace(profiles,profiles.projectPath(initial));
    ProjectWorkspaceController::HostCallbacks document;
    document.mayReplace=[&]{return !pinned;};document.mayWrite=[&]{return writable;};
    workspace.setHost(document);
    if(!workspace.loadActive(true,false)) return false;
    auto* originalProject=workspace.state().project.get();
    ProfileTransitionController transition(profiles,workspace);
    ProfileTransitionController::HostCallbacks host;
    host.canBegin=[&](bool){return !pinned;};host.saveMemo=[&]{return memoSaved;};
    host.detach=[&]{++detach;};host.finishViews=[&](bool){++views;};
    transition.setHost(host);
    QString deferred;
    ProfileTransitionController::AutomaticCallbacks automatic;
    automatic.defer=[&](const QString& id){deferred=id;};transition.setAutomatic(automatic);
    pinned=true;
    if(!expect(!transition.start(one,true) && deferred==one && !workspace.createEmpty()
        && profiles.activeProfileId()==initial && workspace.state().project.get()==originalProject,
        "editing pins both automatic transition and direct replacement")) return false;
    pinned=false;writable=false;workspace.state().modified=true;
    const auto originalPath=workspace.state().path;
    if(!expect(!workspace.checkpoint() && workspace.state().modified
        && !workspace.saveAs(fixture.filePath("never.json")) && workspace.state().path==originalPath,
        "write gate retains dirty state and original save path")) return false;
    writable=true;
    if(!workspace.checkpoint() || !workspace.saveSettings()) return false;
#ifdef _WIN32
    {
        ReadLock settings(QDir(profiles.profileDirectory(initial)).filePath("profile-settings.json"));
        if(!settings.valid()) return false;
        ProgramSettings::setAutoSelectRunningFeature(!ProgramSettings::autoSelectRunningFeature());
        if(!expect(!transition.start(one,false) && profiles.activeProfileId()==initial
            && workspace.state().project.get()==originalProject && !transition.state().pipeline,
            "locked source settings prevent model/selection replacement")) return false;
    }
#endif
    memoSaved=false;
    if(!expect(!transition.start(one,false) && profiles.activeProfileId()==initial && detach==0,"memo failure aborts before detach")) return false;
    memoSaved=true;
#ifdef _WIN32
    {
        ReadLock manifest(QDir(profiles.profilesDirectory()).filePath("manifest.json"));
        if(!manifest.valid()) return false;
        const auto count=profiles.profiles().size();
        if(!expect(!profiles.setActiveProfile(one) && profiles.activeProfileId()==initial
            && profiles.addProfile("failed",false).isEmpty() && profiles.profiles().size()==count
            && !transition.start(one,false) && workspace.state().project.get()==originalProject,
            "manifest failure rolls back in-memory selection/list and keeps old document")) return false;
    }
#endif
    {
        QFile corrupt(profiles.projectPath(one));
        if(!corrupt.open(QIODevice::WriteOnly) || corrupt.write("{invalid")<0) return false;
        corrupt.close();profiles.invalidateCachedProject(one);
        if(!expect(!transition.start(one,false) && profiles.activeProfileId()==initial
            && workspace.state().project.get()==originalProject,"corrupt target rejected before manifest commit")) return false;
    }
    if(!saveProfile(profiles,one,"one-feature")) return false;
    profiles.invalidateCachedProject(one);
    if(!expect(transition.start(one,false) && profiles.activeProfileId()==one && workspace.state().profileId==one
        && workspace.state().project->features().front()->name()=="one-feature",
        "active id and staged model install together before return")) return false;
    transition.start(two,false);transition.start(one,false);
    if(!expect(until([&]{return !transition.state().pipeline && views>0;}) && profiles.activeProfileId()==one,
        "latest manual request can cancel an earlier queued request")) return false;
    if(!transition.start(two,false)) return false;
    transition.start(one,false);
    if(!expect(until([&]{return !transition.state().pipeline && profiles.activeProfileId()==one;})
        && workspace.state().project->features().front()->name()=="one-feature","rapid manual requests retain the final selected profile")) return false;
    pinned=true;const int oldDetach=detach;
    if(transition.start(two,false) || transition.start(one,false))return false;
    pinned=false;
    QEventLoop retryWindow;QTimer::singleShot(260,&retryWindow,&QEventLoop::quit);retryWindow.exec();
    if(!expect(profiles.activeProfileId()==one && workspace.state().profileId==one && detach==oldDetach,
        "selecting current profile cancels a blocked manual retry even after its timer expires"))return false;
    workspace.state().modified=true;
    const auto copy=fixture.filePath("copy.json");
    { QFile existing(copy);if(!existing.open(QIODevice::WriteOnly) || existing.write("previous")<0) return false; }
#ifdef _WIN32
    {
        ReadLock destination(copy);
        if(!destination.valid() || !expect(!workspace.saveAs(copy) && workspace.state().path==profiles.projectPath(one)
            && workspace.state().modified,"failed Save As retains path and dirty document")) return false;
    }
#endif
    workspace.state().project->featureAt(0)->setName("external-saved-current");
    if(!expect(workspace.saveAs(fixture.filePath("copy.json")) && !workspace.state().modified
        && workspace.state().path==fixture.filePath("copy.json"),"successful Save As publishes its path only after commit")) return false;
    const auto canonical=JsonSerializer::loadFromFile(profiles.projectPath(one));
    if(!expect(canonical && canonical->featureAt(0)->name()=="external-saved-current",
        "external Save As synchronizes canonical profile used by reload and packages"))return false;
    const auto named=[](const QString& path,const char* name){const auto p=JsonSerializer::loadFromFile(path);return p && p->featureAt(0)->name()==name;};
    const auto bytes=[](const QString& path){QFile f(path);return f.open(QIODevice::ReadOnly)?f.readAll():QByteArray();};
    workspace.state().project->featureAt(0)->setName("autosaved-after-as");workspace.state().modified=true;
    const auto asset=QDir(profiles.projectDirectory(one)).filePath("templates/saveas.png");QDir().mkpath(QFileInfo(asset).absolutePath());
    QImage image(8,8,QImage::Format_RGB32);image.fill(QColor(90,160,140));if(!image.save(asset))return false;
    auto find=std::make_unique<ImageFindBlock>();find->templatePaths={"templates/saveas.png"};workspace.state().project->featureAt(0)->workflow().addBlock(std::move(find));
    if(!expect(workspace.save(true) && named(copy,"autosaved-after-as") && named(profiles.projectPath(one),"autosaved-after-as")
        && workspace.state().directory==profiles.projectDirectory(one),"subsequent external autosave updates both files without moving asset root"))return false;
    ProfilePackageController packages;workspace.setPackages(&packages);
    const auto roundtrip=fixture.filePath("roundtrip");
    if(!expect(workspace.seal(true) && ProjectPackage::unpackToDirectory(profiles.profilePackagePath(one),roundtrip)
        && named(QDir(roundtrip).filePath("project.json"),"autosaved-after-as")
        && bytes(QDir(roundtrip).filePath("templates/saveas.png"))==bytes(asset),"real package roundtrip retains external-saved current document and template bytes"))return false;
#ifdef _WIN32
    const auto nextCopy=fixture.filePath("next-copy.json");const auto priorCanonical=bytes(profiles.projectPath(one));
    workspace.state().project->featureAt(0)->setName("partial-canonical-lock");workspace.state().modified=true;
    {
        ReadLock locked(profiles.projectPath(one));if(!locked.valid())return false;
        if(!expect(!workspace.saveAs(nextCopy) && workspace.state().path==copy && workspace.state().modified
            && named(nextCopy,"partial-canonical-lock") && bytes(profiles.projectPath(one))==priorCanonical
            && workspace.lastError().contains(QStringLiteral("선택한 파일은 저장")),"canonical lock reports partial external save, retains path/dirty and old canonical"))return false;
    }
    if(!expect(workspace.saveAs(nextCopy) && named(profiles.projectPath(one),"partial-canonical-lock"),"unlocked retry commits both destinations"))return false;
    if(!packages.waitForIdle())return false;
    const auto readOnly=fixture.filePath("read-only-input.json");auto external=workspace.state().project->clone();external->featureAt(0)->setName("read-only-open-current");
    if(!JsonSerializer::saveToFile(*external,readOnly,profiles.projectDirectory(one)))return false;
    {
        ReadLock locked(readOnly);const auto before=bytes(readOnly);
        if(!locked.valid() || !workspace.load(readOnly,true) || !expect(!workspace.state().modified && workspace.checkpoint()
            && bytes(readOnly)==before && named(profiles.projectPath(one),"read-only-open-current"),"clean external open checkpoints canonical without rewriting read-only source"))return false;
    }
    if(!expect(workspace.loadActive(true,false) && workspace.state().project->featureAt(0)->name()=="read-only-open-current",
        "profile re-entry reads current canonical snapshot after external open"))return false;
#endif
    {
        const auto blocked=fixture.filePath("reentered-save.json");bool replace=true;auto host=document;
        host.mayWrite=[&]{if(replace){replace=false;workspace.loadActive(true,false);}return true;};workspace.setHost(host);
        if(!expect(!workspace.saveAs(blocked) && !QFileInfo::exists(blocked),"reentrant source replacement during write gate cannot save successor into old destination"))return false;
        workspace.setHost(document);
    }
    {
        auto dying=std::make_unique<ProjectWorkspaceController>(profiles,profiles.projectPath(one));if(!dying->loadActive(true,false))return false;
        auto host=document;host.changed=[&]{dying.reset();};dying->setHost(host);
        const auto destination=fixture.filePath("deleted-owner-save.json");
        if(!expect(dying->saveAs(destination) && !dying && QFileInfo::exists(destination),"owner deletion during saved publication cannot schedule through retired controller"))return false;
    }
    {
        auto late=workspace.state().project->clone();const auto source=fixture.filePath("deleted-profile-source.json");
        if(!JsonSerializer::saveToFile(*late,source,profiles.projectDirectory(one)) || !workspace.load(source,true))return false;
        if(!packages.waitForIdle() || !profiles.removeProfile(one))return false;
        if(!expect(!workspace.checkpoint() && !QFileInfo::exists(profiles.profileDirectory(one)),
            "pending external snapshot cannot resurrect a removed profile directory"))return false;
    }
    std::cout<<"Project external documents: Save As/autosave canonical synchronization, real package/template roundtrip, locked canonical partial result/retry, read-only open checkpoint, profile re-entry and write callback replacement/deletion OK\n";
    std::cout<<"Project workspace: owned model/path/dirty state, editor/write gates, settings/memo/manifest failures, corrupt target staging, coherent install, latest manual transition and Save As retention OK (no native input)\n";
    return true;
}
