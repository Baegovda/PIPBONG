#include "app/ProfileManager.h"
#include "storage/ProfileRemovalTransaction.h"
#include "storage/JsonSerializer.h"
#include "model/Project.h"
#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>
#include <QTemporaryDir>
#include <iostream>
#ifdef Q_OS_WIN
#include <windows.h>
#endif
namespace {
QByteArray read(const QString& path){QFile f(path);return f.open(QIODevice::ReadOnly)?f.readAll():QByteArray();}
bool write(const QString& path,const QByteArray& data){QSaveFile f(path);return f.open(QIODevice::WriteOnly) && f.write(data)==data.size() && f.commit();}
QStringList state(const ProfileManager& profiles){QStringList out{profiles.activeProfileId(),profiles.defaultProfileId()};for(const auto& p:profiles.profiles())out<<p.id<<p.name;return out;}
#ifdef Q_OS_WIN
bool legacy(bool promote,bool duplicate) {
    QTemporaryDir root;ProfileManager manager(root.path());if(!manager.initialize())return false;
    const auto original=manager.activeProfileId(),one=manager.addProfile("one",false),two=manager.addProfile("two",false);
    if(one.isEmpty() || two.isEmpty())return false;
    const QString manifest=QDir(manager.profilesDirectory()).filePath("manifest.json");
    const auto before=read(manifest);const auto memory=state(manager);
    const auto project=read(manager.projectPath(one)),settings=read(QDir(manager.profileDirectory(one)).filePath("profile-settings.json"));
    HANDLE lock=INVALID_HANDLE_VALUE;
    if(!duplicate)lock=CreateFileW(reinterpret_cast<LPCWSTR>(manifest.utf16()),GENERIC_READ,FILE_SHARE_READ,nullptr,OPEN_EXISTING,0,nullptr);
    if(!duplicate && lock==INVALID_HANDLE_VALUE)return false;
    const bool saved=promote?manager.setDefaultProfile(one):manager.reorderProfiles(duplicate?QStringList{original,one,one}:QStringList{two,one,original});
    if(lock!=INVALID_HANDLE_VALUE)CloseHandle(lock);
    const bool preserved=!saved && read(manifest)==before && state(manager)==memory && read(manager.projectPath(one))==project
        && read(QDir(manager.profileDirectory(one)).filePath("profile-settings.json"))==settings;
    if(!preserved || duplicate)return preserved;
    const bool retry=promote?manager.setDefaultProfile(one):manager.reorderProfiles({two,one,original});
    if(!retry || manager.activeProfileId()!=original)return false;
    if(promote) {
        const auto committed=read(manifest);
        if(manager.defaultProfileId()!=one || manager.profiles().front().id!=one || !manager.loadSettings(one).runWithoutTargetWindow
            || !manager.targetWindowTitle(one).isEmpty() || !manager.setDefaultProfile(one) || read(manifest)!=committed)return false;
    } else if(manager.profiles()[0].id!=original || manager.profiles()[1].id!=two || manager.profiles()[2].id!=one)return false;
    ProfileManager reopened(root.path());
    return reopened.initialize() && reopened.defaultProfileId()==(promote?one:original) && reopened.profiles().size()==3;
}
bool cleanupLock() {
    QTemporaryDir root;ProfileManager manager(root.path());if(!manager.initialize())return false;
    const auto id=manager.addProfile("cleanup owner",false);Project project;project.addFeature("preserved");
    if(id.isEmpty() || !JsonSerializer::saveToFile(project,manager.projectPath(id),manager.profileDirectory(id))
        || !write(manager.profilePackagePath(id),"package data"))return false;
    const QString manifest=QDir(manager.profilesDirectory()).filePath("manifest.json");
    QString backup,error;HANDLE lock=INVALID_HANDLE_VALUE;
    const bool removed=removeProfileFilesTransaction(manager.profilesDirectory(),manager.profileDirectory(id),manager.profilePackagePath(id),[&]{
        const auto pending=QDir(manager.profilesDirectory()).entryList({".profile-remove-*"},QDir::Dirs|QDir::Hidden|QDir::NoDotAndDotDot);
        if(pending.size()!=1)return false;
        backup=QDir(manager.profilesDirectory()).filePath(pending[0]);
        const QString held=QDir(backup).filePath("workspace/project.json");
        lock=CreateFileW(reinterpret_cast<LPCWSTR>(held.utf16()),GENERIC_READ,FILE_SHARE_READ,nullptr,OPEN_EXISTING,0,nullptr);
        if(lock==INVALID_HANDLE_VALUE)return false;
        auto json=QJsonDocument::fromJson(read(manifest)).object();QJsonArray keep;
        for(const auto& row:json["profiles"].toArray())if(row.toObject()["id"].toString()!=id)keep.push_back(row);
        json["profiles"]=keep;return write(manifest,QJsonDocument(json).toJson());
    },&error);
    const bool journal=QFileInfo::exists(QDir(backup).filePath("recovery.json"));
    ProfileManager restart(root.path());const bool usable=restart.initialize() && restart.storageReady() && !restart.profileById(id);
    if(lock!=INVALID_HANDLE_VALUE)CloseHandle(lock);
    ProfileManager retry(root.path());const bool cleaned=retry.initialize() && retry.storageReady() && !retry.profileById(id)
        && !QFileInfo::exists(backup) && !QFileInfo::exists(manager.profileDirectory(id));
    if(!removed || !journal || !usable || !cleaned)std::cerr<<"Removal cleanup edge: committed="<<removed<<" journal="<<journal<<" usable="<<usable<<" retry="<<cleaned<<'\n';
    return removed && journal && usable && cleaned && !error.isEmpty();
}
bool missingWorkspace() {
    QTemporaryDir root;ProfileManager manager(root.path());if(!manager.initialize())return false;
    const auto id=manager.addProfile("missing recovery",false);const QString workspace=manager.profileDirectory(id);
    Project project;project.addFeature("recoverable workflow");
    if(!JsonSerializer::saveToFile(project,manager.projectPath(id),workspace))return false;
    const auto projectBefore=read(manager.projectPath(id));
    const QString held=QDir(root.path()).filePath("safety-held-workspace");
    if(id.isEmpty() || !QDir().rename(workspace,held))return false;
    const QString backup=QDir(manager.profilesDirectory()).filePath(".profile-remove-missing");
    if(!QDir().mkpath(backup))return false;
    const auto journal=QJsonDocument(QJsonObject{{"workspace",workspace},{"package",manager.profilePackagePath(id)},
        {"workspaceExisted",true},{"packageExisted",false}}).toJson();
    if(!write(QDir(backup).filePath("recovery.json"),journal))return false;
    const QString manifest=QDir(manager.profilesDirectory()).filePath("manifest.json");const auto before=read(manifest);
    ProfileManager restart(root.path());
    return !restart.initialize() && !restart.storageReady() && !QFileInfo::exists(workspace)
        && read(manifest)==before && QFileInfo::exists(QDir(backup).filePath("recovery.json"))
        && read(QDir(held).filePath("project.json"))==projectBefore;
}
#endif
}
bool runProfileStorageEdgesChecks() {
#ifdef Q_OS_WIN
    const bool order=legacy(false,false),role=legacy(true,false),duplicate=legacy(false,true),cleanup=cleanupLock(),missing=missingWorkspace();
    if(!order || !role || !duplicate || !cleanup || !missing) {
        std::cerr<<"Profile storage edges FAILED: order="<<order<<" role="<<role<<" duplicate="<<duplicate<<" cleanup="<<cleanup<<" missing="<<missing<<'\n';return false;
    }
    std::cout<<"Profile storage edges: legacy order/default locks preserve memory/files and unlocked retry/reopen succeeds, duplicate order rejected, locked committed removal retains journal/usable startup/retry, missing recovery data blocks empty regeneration OK\n";
#endif
    return true;
}
