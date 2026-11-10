#include "app/ProfileManager.h"
#include "model/Project.h"
#include "storage/JsonSerializer.h"
#include <QDir>
#include <QFile>
#include <QTemporaryDir>
#include <iostream>
#ifdef Q_OS_WIN
#include <windows.h>
#endif
namespace {
QByteArray bytes(const QString& path){QFile file(path);return file.open(QIODevice::ReadOnly)?file.readAll():QByteArray();}
}
bool runProfileMutationChecks() {
#ifdef Q_OS_WIN
    QTemporaryDir root;ProfileManager manager(root.path());
    if(!manager.initialize())return false;
    const auto original=manager.activeProfileId();
    const auto first=manager.addProfile("original name",true),second=manager.addProfile("second",false);
    Project project;project.addFeature("preserved feature");
    if(first.isEmpty() || second.isEmpty() || !JsonSerializer::saveToFile(project,manager.projectPath(first),manager.projectDirectory(first)))return false;
    QFile package(manager.profilePackagePath(first));if(!package.open(QIODevice::WriteOnly))return false;
    package.write("retained package bytes");package.close();
    const auto projectBefore=bytes(manager.projectPath(first)),packageBefore=bytes(package.fileName());
    const QString manifest=QDir(manager.profilesDirectory()).filePath("manifest.json");
    const auto before=bytes(manifest);
    HANDLE locked=CreateFileW(reinterpret_cast<LPCWSTR>(manifest.utf16()),GENERIC_READ,FILE_SHARE_READ,nullptr,OPEN_EXISTING,0,nullptr);
    if(locked==INVALID_HANDLE_VALUE)return false;
    const bool rename=!manager.renameProfile(first,"must not stick") && manager.profileById(first)->name=="original name";
    const bool removed=!manager.removeProfile(first) && manager.profileById(first)
        && manager.activeProfileId()==first && bytes(manager.projectPath(first))==projectBefore
        && bytes(package.fileName())==packageBefore && bytes(manifest)==before;
    CloseHandle(locked);
    if(!rename || !removed){std::cerr<<"Profile mutation FAILED: locked manifest rename-preserved="<<rename<<" delete-preserved="<<removed<<'\n';return false;}
    locked=CreateFileW(reinterpret_cast<LPCWSTR>(package.fileName().utf16()),GENERIC_READ,FILE_SHARE_READ,nullptr,OPEN_EXISTING,0,nullptr);
    if(locked==INVALID_HANDLE_VALUE)return false;
    const bool packageBlocked=!manager.removeProfile(first) && manager.profileById(first)
        && manager.activeProfileId()==first && bytes(manager.projectPath(first))==projectBefore && bytes(manifest)==before;
    CloseHandle(locked);
    if(!packageBlocked){std::cerr<<"Profile mutation FAILED: locked package rollback\n";return false;}
    if(!manager.renameProfile(first,"saved name") || manager.profileById(first)->name!="saved name"
        || !manager.removeProfile(first) || manager.profileById(first) || manager.activeProfileId()!=original
        || QFileInfo::exists(manager.profileDirectory(first)) || QFileInfo::exists(package.fileName()))return false;
    if(manager.removeProfile(original) || manager.removeProfile("../outside"))return false;
    std::cout<<"Profile mutations: locked manifest preserves name/model/active/profile/package bytes, locked package rolls back staged folder, unlocked rename/delete commit and default/unknown guards OK\n";
#endif
    return true;
}
