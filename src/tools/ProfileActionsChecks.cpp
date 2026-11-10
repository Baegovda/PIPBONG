#include "app/ProfileActionsController.h"
#include "storage/ProfileMetadataTransaction.h"
#include "storage/JsonSerializer.h"
#include "model/Project.h"
#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTemporaryDir>
#include <QCoreApplication>
#include <QProcess>
#include <QProcessEnvironment>
#include <iostream>
#ifdef Q_OS_WIN
#include <windows.h>
#endif
namespace {
QByteArray bytes(const QString& path){QFile f(path);return f.open(QIODevice::ReadOnly)?f.readAll():QByteArray();}
bool expect(bool ok,const char* label){if(!ok)std::cerr<<"Profile actions FAILED: "<<label<<'\n';return ok;}
}
bool runProfileActionsChecks() {
    using C=ProfileActionsController;QTemporaryDir root;ProfileManager profiles(root.path());if(!profiles.initialize())return false;
    const auto owner=profiles.activeProfileId(),id=profiles.addProfile("before",false);Project project;project.addFeature("retain feature");
    if(id.isEmpty() || !JsonSerializer::saveToFile(project,profiles.projectPath(id),profiles.projectDirectory(id)))return false;
    const QString paths[]{profiles.projectPath(id),QDir(profiles.profileDirectory(id)).filePath("profile-settings.json"),QDir(profiles.profilesDirectory()).filePath("manifest.json")};
    QByteArray original[3];for(int i=0;i<3;++i)original[i]=bytes(paths[i]);
    quint64 generation=1;bool ready=true;int snapshots=0,publications=0;std::function<bool()> checkpoint=[]{return true;};
    C::Host host;host.canBegin=[&]{return ready;};host.generation=[&]{return generation;};
    host.checkpoint=[&](const QString&,bool){++snapshots;return checkpoint();};host.changed=[&](const auto&){++publications;};
    C actions(profiles,host);
    C::Request request;request.id=id;request.owner=owner;request.generation=generation;
    request.edit={"after","New main","New sub","C:/main/new.exe","C:/sub/new.exe",false};
#ifdef Q_OS_WIN
    for(int locked=0;locked<3;++locked) {
        HANDLE file=CreateFileW(reinterpret_cast<LPCWSTR>(paths[locked].utf16()),GENERIC_READ,FILE_SHARE_READ,nullptr,OPEN_EXISTING,0,nullptr);
        if(file==INVALID_HANDLE_VALUE)return false;
        const auto result=actions.execute(request);CloseHandle(file);
        bool preserved=true;for(int i=0;i<3;++i)preserved=preserved && bytes(paths[i])==original[i];
        if(!expect(!result.success && !result.message.isEmpty() && preserved && publications==0
            && profiles.profileById(id)->name=="before" && profiles.targetWindowTitle(id).isEmpty()
            && profiles.subTargetWindowTitle(id).isEmpty() && profiles.storageReady(),"locked metadata at each commit position restores all files and model"))return false;
    }
#endif
    checkpoint=[&]{++generation;return true;};
    if(!expect(actions.execute(request).cancelled && publications==0,"checkpoint replacement rejects stale modal request"))return false;
    request.generation=generation;checkpoint=[] {return false;};
    if(!expect(actions.execute(request).cancelled && publications==0,"failed snapshot prevents edit"))return false;
    bool nested=false;checkpoint=[&]{nested=actions.execute(request).cancelled;return true;};
    const auto result=actions.execute(request);
    auto loaded=JsonSerializer::loadFromFile(paths[0]);
    if(!expect(nested && result.success && publications==1 && loaded && loaded->features().size()==1
        && profiles.profileById(id)->name=="after" && profiles.targetWindowTitle(id)=="New main"
        && profiles.subTargetWindowTitle(id)=="New sub" && profiles.linkedTargetProcessPath(id)=="C:/main/new.exe"
        && profiles.subLinkedTargetProcessPath(id)=="C:/sub/new.exe", "committed metadata and executable bindings publish once, retain workflow"))return false;
    checkpoint=[]{return true;};request.edit.makeDefault=true;
    if(!expect(actions.execute(request).success && profiles.defaultProfileId()==id && profiles.profiles().front().id==id
        && profiles.targetWindowTitle(id).isEmpty() && profiles.subTargetWindowTitle(id).isEmpty()
        && profiles.linkedTargetProcessPath(id).isEmpty() && profiles.loadSettings(id).runWithoutTargetWindow,
        "default promotion commits role/order/name/empty bindings and global mode together"))return false;
    if(!expect(!actions.execute(request).success,"fixed default cannot be edited again"))return false;
    C::Request add;add.action=C::Action::Add;add.owner=owner;add.generation=generation;add.edit.name="new profile";
    const auto added=actions.execute(add);
    if(!expect(added.success && profiles.profileById(added.id),"add operation preserves current owner"))return false;
    add.action=C::Action::Remove;add.id=added.id;
    if(!expect(actions.execute(add).success && !profiles.profileById(added.id),"remove uses shared guarded mutation policy"))return false;
    // Only stack-owned callback snapshots/guard state survive checkpoint-driven deletion.
    auto* temporary=new C(profiles,host);checkpoint=[&]{delete temporary;temporary=nullptr;return true;};
    add.action=C::Action::Add;
    const auto deleted=temporary->execute(add);
    if(!expect(!temporary && deleted.cancelled,"checkpoint deletion safely cancels"))return false;
    if(!expect(recoverProfileMetadata(profiles.profilesDirectory()).success
        && QDir(profiles.profilesDirectory()).entryList({".profile-edit-*"},QDir::Dirs|QDir::Hidden|QDir::NoDotAndDotDot).isEmpty(),
        "normal commits and rollbacks leave no pending edit transactions"))return false;
#ifdef Q_OS_WIN
    if(!profiles.setActiveProfile(profiles.defaultProfileId()))return false;
    for(int interrupted=0;interrupted<3;++interrupted) {
        QVector<QByteArray> before;for(const auto& path:paths)before.push_back(bytes(path));
        const QString phase="edit"+QString::number(interrupted);
        QProcess child;auto environment=QProcessEnvironment::systemEnvironment();
        environment.insert("QT_QPA_PLATFORM","offscreen");environment.insert("PIPBONG_SELF_TEST_UI","1");
        environment.insert("PIPBONG_DATA_DIR",root.path());environment.insert("PIPBONG_SELF_TEST_PROFILE_REMOVAL_PHASE",phase);
        environment.insert("PIPBONG_SELF_TEST_PROFILE_REMOVAL_ID",id);child.setProcessEnvironment(environment);
        child.start(QCoreApplication::applicationFilePath(),QStringList{});
        if(!child.waitForStarted(5000) || !child.waitForFinished(10000)){child.kill();child.waitForFinished(3000);return false;}
        if(!expect(child.exitCode()==197 && bytes(QDir(root.path()).filePath("crash-marker"))==phase.toUtf8(),"child terminated at requested metadata commit boundary"))return false;
        ProfileManager restarted(root.path());
        if(!expect(restarted.initialize() && restarted.storageReady(),"fresh manager recovers interrupted edit before profile loading"))return false;
        for(int i=0;i<3;++i)if(!expect(bytes(paths[i])==before[i],"restart recovery restores every original metadata byte"))return false;
    }
    QVector<ProfileMetadataFile> replacements;
    for(const auto& path:paths){auto object=QJsonDocument::fromJson(bytes(path)).object();object["qa-marker"]="committed";replacements.push_back({path,QJsonDocument(object).toJson()});}
    HANDLE backupLock=INVALID_HANDLE_VALUE;
    const auto committed=writeProfileMetadata(profiles.profilesDirectory(),replacements,[&](int index){
        if(index!=2)return;
        const auto folders=QDir(profiles.profilesDirectory()).entryList({".profile-edit-*"},QDir::Dirs|QDir::Hidden|QDir::NoDotAndDotDot);
        if(folders.size()!=1)return;
        const QString path=QDir(profiles.profilesDirectory()).filePath(folders.first()+"/0");
        backupLock=CreateFileW(reinterpret_cast<LPCWSTR>(path.utf16()),GENERIC_READ,FILE_SHARE_READ,nullptr,OPEN_EXISTING,0,nullptr);
    });
    if(backupLock==INVALID_HANDLE_VALUE)return false;
    const auto retained=QDir(profiles.profilesDirectory()).entryList({".profile-edit-*"},QDir::Dirs|QDir::Hidden|QDir::NoDotAndDotDot);
    const bool journalRetained=retained.size()==1 && QFileInfo::exists(QDir(profiles.profilesDirectory()).filePath(retained.first()+"/journal.json"));
    CloseHandle(backupLock);
    if(!expect(committed.success && !committed.message.isEmpty() && journalRetained
        && recoverProfileMetadata(profiles.profilesDirectory()).success,"cleanup lock preserves final journal and retry does not roll back committed edit"))return false;
    for(int i=0;i<3;++i)if(!expect(bytes(paths[i])==replacements[i].bytes,"finalized cleanup preserves new metadata"))return false;
#endif
    std::cout<<"Profile actions: three metadata commit locks/actual child terminations restore bytes/model, finalized cleanup lock/retry retains journal/new files, frozen modal generation, reentry/deletion, executable bindings, default role/order and add/remove publication OK\n";
    return true;
}
