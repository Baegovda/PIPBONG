#include "storage/ProfileRemovalTransaction.h"
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QObject>
#include <QSaveFile>
#include <QTemporaryDir>

bool discardProfileRemovalBackup(const QString& profilesRoot,const QString& backupPath) {
    const QString root=QDir::cleanPath(QFileInfo(profilesRoot).absoluteFilePath());
    const QString backup=QDir::cleanPath(QFileInfo(backupPath).absoluteFilePath());
    const QFileInfo directory(backup);
    if(directory.absolutePath().compare(root,Qt::CaseInsensitive)!=0 || !directory.fileName().startsWith(".profile-remove-")
        || directory.isSymLink() || directory.isJunction())return false;
    const auto entries=QDir(backup).entryInfoList(QDir::Files|QDir::Dirs|QDir::Hidden|QDir::System|QDir::NoDotAndDotDot);
    for(const auto& entry:entries)if(entry.isSymLink() || entry.isJunction()
        || (entry.fileName()!="workspace" && entry.fileName()!="profile.pipbong" && entry.fileName()!="recovery.json"))return false;
    const QString workspace=QDir(backup).filePath("workspace"),package=QDir(backup).filePath("profile.pipbong"),journal=QDir(backup).filePath("recovery.json");
    if(QFileInfo::exists(workspace) && (!QFileInfo(workspace).isDir() || !QDir(workspace).removeRecursively()))return false;
    if(QFileInfo::exists(package) && (!QFileInfo(package).isFile() || !QFile::remove(package)))return false;
    if(QFileInfo::exists(journal) && (!QFileInfo(journal).isFile() || !QFile::remove(journal)))return false;
    return QDir().rmdir(backup);
}

bool removeProfileFilesTransaction(const QString& rootPath,const QString& workspace,const QString& package,
    const std::function<bool()>& commit,QString* error) {
    const auto fail=[&](const QString& message){if(error)*error=message;return false;};
    const QString root=QDir::cleanPath(QFileInfo(rootPath).absoluteFilePath());
    const QString paths[]{QDir::cleanPath(QFileInfo(workspace).absoluteFilePath()),
                          QDir::cleanPath(QFileInfo(package).absoluteFilePath())};
    for(const auto& path:paths) {
        const QFileInfo file(path);
        if(path==root || file.absolutePath().compare(root,Qt::CaseInsensitive)!=0 || file.isSymLink() || file.isJunction())
            return fail(QObject::tr("프로필 삭제 경로를 확인할 수 없어 중단했어요."));
    }
    QTemporaryDir transaction(QDir(root).filePath(".profile-remove-XXXXXX"));
    if(!transaction.isValid())return fail(QObject::tr("프로필 삭제 백업을 준비하지 못했어요."));
    transaction.setAutoRemove(false);
    const QString backup=transaction.path();
    const QString staged[]{QDir(backup).filePath("workspace"),QDir(backup).filePath("profile.pipbong")};
    QSaveFile journal(QDir(backup).filePath("recovery.json"));
    const auto record=QJsonDocument(QJsonObject{{"workspace",paths[0]},{"package",paths[1]},
        {"workspaceExisted",QFileInfo::exists(paths[0])},{"packageExisted",QFileInfo::exists(paths[1])}}).toJson();
    if(!journal.open(QIODevice::WriteOnly) || journal.write(record)!=record.size() || !journal.commit()) {
        QDir(backup).removeRecursively();return fail(QObject::tr("프로필 삭제 복구 기록을 저장하지 못했어요."));
    }
    bool moved[]{false,false},ready=true;
    for(int i=0;i<2 && ready;++i)if(QFileInfo::exists(paths[i])) {
        moved[i]=QDir().rename(paths[i],staged[i]);ready=moved[i];
    }
    if(!ready || !commit()) {
        bool restored=true;
        for(int i=1;i>=0;--i)if(moved[i] && !QDir().rename(staged[i],paths[i]))restored=false;
        if(restored)discardProfileRemovalBackup(root,backup);
        return fail(restored ? QObject::tr("프로필을 삭제하지 못했어요. 기존 파일을 유지했어요.")
            : QObject::tr("프로필 삭제를 중단했지만 일부 파일을 복원하지 못했어요. 복구 파일: %1").arg(backup));
    }
    if(!discardProfileRemovalBackup(root,backup) && error)
        *error=QObject::tr("프로필은 삭제했지만 백업 파일이 남아 있어요: %1").arg(backup);
    return true;
}
