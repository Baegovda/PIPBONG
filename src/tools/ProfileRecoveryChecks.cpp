#include "app/ProfileManager.h"
#include "app/MainWindow.h"
#include "storage/ProfileRemovalTransaction.h"
#include "storage/ProfileRemovalRecovery.h"
#include "storage/ProfileMetadataTransaction.h"
#include "model/Project.h"
#include "storage/JsonSerializer.h"
#include <QCoreApplication>
#include <QGuiApplication>
#include <QFileInfo>
#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QProcess>
#include <QProcessEnvironment>
#include <QMetaObject>
#include <QSaveFile>
#include <QTemporaryDir>
#include <iostream>
#ifdef Q_OS_WIN
#include <windows.h>
#endif
namespace {
QByteArray read(const QString& path){QFile f(path);return f.open(QIODevice::ReadOnly)?f.readAll():QByteArray();}
bool write(const QString& path,const QByteArray& bytes){QDir().mkpath(QFileInfo(path).absolutePath());QSaveFile f(path);return f.open(QIODevice::WriteOnly) && f.write(bytes)==bytes.size() && f.commit();}
bool expect(bool result,const char* label){if(!result)std::cerr<<"Profile recovery FAILED: "<<label<<'\n';return result;}
}
int runProfileRemovalCrashProbe() {
#ifdef Q_OS_WIN
    const QString root=qEnvironmentVariable("PIPBONG_DATA_DIR");
    const QString phase=qEnvironmentVariable("PIPBONG_SELF_TEST_PROFILE_REMOVAL_PHASE");
    const QString id=qEnvironmentVariable("PIPBONG_SELF_TEST_PROFILE_REMOVAL_ID");
    const bool metadata=phase=="edit0" || phase=="edit1" || phase=="edit2";
    if(QGuiApplication::platformName()!="offscreen" || root.isEmpty() || id.isEmpty() || id.contains('/') || id.contains('\\')
        || id.contains(':') || id.startsWith('.') || (!metadata && phase!="before" && phase!="after"))return 61;
    const QString profiles=QDir(root).filePath("profiles");
    const QString workspace=QDir(profiles).filePath(id);
    if(metadata) {
        QVector<ProfileMetadataFile> files;
        for(const auto& path:QStringList{QDir(workspace).filePath("project.json"),QDir(workspace).filePath("profile-settings.json"),QDir(profiles).filePath("manifest.json")}) {
            auto object=QJsonDocument::fromJson(read(path)).object();if(object.isEmpty())return 63;
            object["qa-marker"]=phase;files.push_back({path,QJsonDocument(object).toJson()});
        }
        writeProfileMetadata(profiles,files,[&](int index){if(index==phase.right(1).toInt()) {
            if(write(QDir(root).filePath("crash-marker"),phase.toUtf8()))TerminateProcess(GetCurrentProcess(),197);
        }});
        return 64;
    }
    QString error;
    removeProfileFilesTransaction(profiles,workspace,workspace+".pipbong",[&] {
        if(phase=="after") {
            const QString path=QDir(profiles).filePath("manifest.json");
            auto manifest=QJsonDocument::fromJson(read(path)).object();QJsonArray keep;
            bool found=false;
            for(const auto& entry:manifest["profiles"].toArray()) {
                if(entry.toObject()["id"].toString()==id)found=true;else keep.push_back(entry);
            }
            if(!found)return false;
            manifest["profiles"]=keep;
            if(!write(path,QJsonDocument(manifest).toJson()))return false;
        }
        if(!write(QDir(root).filePath("crash-marker"),phase.toUtf8()))return false;
        TerminateProcess(GetCurrentProcess(),197);
        return false;
    },&error);
#endif
    return 62;
}

bool runProfileRecoveryChecks() {
#ifdef Q_OS_WIN
    for(const QString phase:{QStringLiteral("before"),QStringLiteral("after")}) {
        QTemporaryDir root;ProfileManager original(root.path());if(!original.initialize())return false;
        const auto id=original.addProfile("crash owner",false);Project project;project.addFeature("recover this feature");
        if(id.isEmpty() || !JsonSerializer::saveToFile(project,original.projectPath(id),original.projectDirectory(id))
            || !write(original.profilePackagePath(id),"original package"))return false;
        const auto savedProject=read(original.projectPath(id)),savedSettings=read(QDir(original.profileDirectory(id)).filePath("profile-settings.json"));
        QProcess child;auto environment=QProcessEnvironment::systemEnvironment();
        environment.insert("QT_QPA_PLATFORM","offscreen");environment.insert("PIPBONG_SELF_TEST_UI","1");
        environment.insert("PIPBONG_DATA_DIR",root.path());environment.insert("PIPBONG_SELF_TEST_PROFILE_REMOVAL_PHASE",phase);
        environment.insert("PIPBONG_SELF_TEST_PROFILE_REMOVAL_ID",id);child.setProcessEnvironment(environment);
        child.start(QCoreApplication::applicationFilePath(),QStringList{});
        if(!child.waitForStarted(5000) || !child.waitForFinished(10000)) {child.kill();child.waitForFinished(3000);return false;}
        if(!expect(child.exitCode()==197 && read(QDir(root.path()).filePath("crash-marker"))==phase.toUtf8(),"child reached actual termination phase"))return false;
        ProfileManager restarted(root.path());
        if(!expect(restarted.initialize() && restarted.storageReady(),"restart recovery completed before workspace loading"))return false;
        if(phase=="before") {
            if(!expect(restarted.profileById(id) && read(restarted.projectPath(id))==savedProject
                && read(QDir(restarted.profileDirectory(id)).filePath("profile-settings.json"))==savedSettings
                && read(restarted.profilePackagePath(id))=="original package","uncommitted deletion restored original bytes"))return false;
        } else if(!expect(!restarted.profileById(id) && !QFileInfo::exists(restarted.profileDirectory(id))
            && !QFileInfo::exists(restarted.profilePackagePath(id)),"committed deletion does not resurrect profile"))return false;
        if(!expect(QDir(restarted.profilesDirectory()).entryList({".profile-remove-*"},QDir::Dirs|QDir::Hidden|QDir::NoDotAndDotDot).isEmpty()
            && recoverProfileRemovals(restarted.profilesDirectory()).complete,"successful recovery is idempotent and cleans journal"))return false;
    }
    QTemporaryDir root;ProfileManager manager(root.path());if(!manager.initialize())return false;
    const auto id=manager.addProfile("collision owner",false);Project project;project.addFeature("staged original");
    if(!JsonSerializer::saveToFile(project,manager.projectPath(id),manager.projectDirectory(id)))return false;
    const auto originalProject=read(manager.projectPath(id));QString error;
    const bool removed=removeProfileFilesTransaction(manager.profilesDirectory(),manager.profileDirectory(id),manager.profilePackagePath(id),[&]{
        write(QDir(manager.profileDirectory(id)).filePath("conflict.txt"),"new conflicting data");return false;
    },&error);
    if(!expect(!removed && !error.isEmpty(),"rollback conflict retains backup and reports location"))return false;
    const auto manifest=QDir(manager.profilesDirectory()).filePath("manifest.json");const auto originalManifest=read(manifest);
    ProfileManager blocked(root.path());
    if(!expect(!blocked.initialize() && !blocked.storageReady() && !blocked.startupWarnings().isEmpty()
        && read(QDir(manager.profileDirectory(id)).filePath("conflict.txt"))=="new conflicting data"
        && read(manifest)==originalManifest && !blocked.ensureProfileWorkspace(id)
        && !blocked.saveSettings(id,ProgramSettings::ProfileSettings{}) && blocked.addProfile("not allowed").isEmpty(),
        "conflict blocks regeneration/writes and preserves both sides"))return false;
    QFile::remove(QDir(manager.profileDirectory(id)).filePath("conflict.txt"));QDir().rmdir(manager.profileDirectory(id));
    ProfileManager retry(root.path());
    if(!expect(retry.initialize() && read(retry.projectPath(id))==originalProject,"retry restores retained workspace after conflict resolution"))return false;
    const QString invalid=QDir(manager.profilesDirectory()).filePath(".profile-remove-invalid");QDir().mkpath(invalid);
    if(!write(QDir(invalid).filePath("recovery.json"),"not JSON"))return false;
    ProfileManager corrupt(root.path());
    if(!expect(!corrupt.initialize() && !corrupt.storageReady() && read(manifest)==originalManifest
        && read(QDir(invalid).filePath("recovery.json"))=="not JSON","corrupt journal preserves manifest and recovery data"))return false;
    const auto previousData=qgetenv("PIPBONG_DATA_DIR");
    qputenv("PIPBONG_DATA_DIR",root.path().toUtf8());
    bool windowProtected=false;
    {
        MainWindow blockedWindow;
        const bool invoked=QMetaObject::invokeMethod(&blockedWindow,"onNewProject",Qt::DirectConnection);
        const bool absent=!QFileInfo::exists(QDir(manager.profilesDirectory()).filePath("project.json"));
        const bool closed=blockedWindow.prepareForShutdown();
        const bool sameManifest=read(manifest)==originalManifest;
        windowProtected=invoked && absent && closed && sameManifest;
        if(!windowProtected)std::cerr<<"Blocked MainWindow diagnostic: invoked="<<invoked<<" no-file="<<absent
            <<" closed="<<closed<<" manifest="<<sameManifest<<'\n';
    }
    qputenv("PIPBONG_DATA_DIR",previousData);
    if(!expect(windowProtected,"actual blocked MainWindow rejects new-project writes and closes without profile saves"))return false;
    QTemporaryDir outside;const QString outsidePath=QDir(outside.path()).filePath("outside");
    if(!write(outsidePath,"outside sentinel"))return false;
    const QJsonObject escaped{{"workspace",outsidePath},{"package",outsidePath+".pipbong"}};
    if(!write(QDir(invalid).filePath("recovery.json"),QJsonDocument(escaped).toJson()))return false;
    if(!expect(!recoverProfileRemovals(manager.profilesDirectory()).complete && read(outsidePath)=="outside sentinel",
        "journal cannot restore or delete paths outside profile root"))return false;
    if(!write(manifest,"{broken manifest"))return false;
    if(!expect(!recoverProfileRemovals(manager.profilesDirectory()).complete && read(manifest)=="{broken manifest",
        "invalid membership never implies committed deletion"))return false;
    std::cout<<"Profile recovery: actual isolated child termination before/after manifest commit, restart byte restoration/no resurrection, rollback conflict retention/retry, corrupt journal/manifest, root escape, blocked writes and actual blocked MainWindow new-project/close guards OK (no native input)\n";
#endif
    return true;
}
