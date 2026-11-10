#include "storage/ProfileRemovalRecovery.h"
#include "storage/ProfileRemovalTransaction.h"
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QSet>
#include <QObject>

namespace {
bool readObject(const QString& path,QJsonObject& result) {
    const QFileInfo info(path);
    if(!info.isFile() || info.isSymLink() || info.isJunction())return false;
    QFile file(path);if(!file.open(QIODevice::ReadOnly) || file.size()>1024*1024)return false;
    QJsonParseError error;
    const auto document=QJsonDocument::fromJson(file.readAll(),&error);
    if(error.error!=QJsonParseError::NoError || !document.isObject())return false;
    result=document.object();return true;
}
QString absolute(const QString& path){return QDir::cleanPath(QFileInfo(path).absoluteFilePath());}
bool safeNode(const QString& path){const QFileInfo node(path);return !node.isSymLink() && !node.isJunction();}
}
ProfileRemovalRecovery recoverProfileRemovals(const QString& profilesRoot) {
    ProfileRemovalRecovery result;
    const QString root=absolute(profilesRoot);
    const auto pending=QDir(root).entryInfoList({".profile-remove-*"},QDir::Dirs|QDir::Hidden|QDir::NoDotAndDotDot,QDir::Name);
    if(pending.isEmpty())return result;
    const auto blocked=[&](const QString& path){
        result.complete=false;
        result.warnings<<QObject::tr("프로필 삭제 복구를 완료하지 못했어요. 복구 기록과 남은 파일을 확인해 주세요: %1").arg(path);
    };
    QJsonObject manifest;
    if(!readObject(QDir(root).filePath("manifest.json"),manifest) || !manifest["profiles"].isArray()) {
        blocked(root);return result;
    }
    QSet<QString> members;
    for(const auto& entry:manifest["profiles"].toArray()) {
        if(!entry.isObject() || !entry.toObject()["id"].isString()) {blocked(root);return result;}
        const QString id=entry.toObject()["id"].toString();
        if(id.isEmpty() || id=="." || id==".." || id.contains('/') || id.contains('\\') || id.contains(':') || members.contains(id)) {
            blocked(root);return result;
        }
        members.insert(id);
    }
    // An empty/damaged membership list must never be interpreted as committed deletions.
    if(members.isEmpty() || !members.contains(manifest["defaultProfileId"].toString())) {blocked(root);return result;}
    for(const auto& directory:pending) {
        const QString backup=directory.absoluteFilePath();
        if(safeNode(backup) && QDir(backup).entryList(QDir::Files|QDir::Dirs|QDir::Hidden|QDir::System|QDir::NoDotAndDotDot).isEmpty()) {
            if(!QDir().rmdir(backup))result.warnings<<QObject::tr("빈 프로필 보관 폴더를 정리하지 못했어요: %1").arg(backup);
            continue;
        }
        QJsonObject journal;
        if(!safeNode(backup) || !readObject(QDir(backup).filePath("recovery.json"),journal)
            || !journal["workspace"].isString() || !journal["package"].isString()
            || (journal.contains("workspaceExisted") && !journal["workspaceExisted"].isBool())
            || (journal.contains("packageExisted") && !journal["packageExisted"].isBool())) {blocked(backup);continue;}
        const QString original[]{absolute(journal["workspace"].toString()),absolute(journal["package"].toString())};
        const QString staged[]{QDir(backup).filePath("workspace"),QDir(backup).filePath("profile.pipbong")};
        const QString id=QFileInfo(original[0]).fileName();
        bool valid=!id.isEmpty() && !id.startsWith('.') && original[1]==original[0]+".pipbong";
        for(int i=0;i<2;++i)valid=valid && original[i]!=root
            && QFileInfo(original[i]).absolutePath().compare(root,Qt::CaseInsensitive)==0
            && safeNode(original[i]) && safeNode(staged[i]);
        if(!valid){blocked(backup);continue;}
        bool restored=true;
        if(members.contains(id)) {
            // A conflicting newly-created destination is preserved, never overwritten.
            for(int i=1;i>=0;--i)if(QFileInfo::exists(staged[i])) {
                if(QFileInfo::exists(original[i]) || !QDir().rename(staged[i],original[i]))restored=false;
            } else {
                const bool needed=journal.value(i==0?"workspaceExisted":"packageExisted").toBool(i==0);
                if(needed && !QFileInfo::exists(original[i]))restored=false;
            }
        }
        if(!restored){blocked(backup);continue;}
        if(!discardProfileRemovalBackup(root,backup))
            result.warnings<<QObject::tr("프로필 데이터 상태는 확인했지만 보관본을 정리하지 못했어요: %1").arg(backup);
        if(members.contains(id))result.warnings<<QObject::tr("중단된 프로필 삭제에서 파일을 복구했어요: %1").arg(id);
    }
    return result;
}
