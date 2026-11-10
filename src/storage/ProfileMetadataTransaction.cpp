#include "storage/ProfileMetadataTransaction.h"
#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QObject>
#include <QSaveFile>
#include <QSet>
#include <QTemporaryDir>

namespace {
QString hash(const QByteArray& bytes){return QString::fromLatin1(QCryptographicHash::hash(bytes,QCryptographicHash::Sha256).toHex());}
bool read(const QString& path,QByteArray& bytes){QFile f(path);if(!f.open(QIODevice::ReadOnly) || f.size()>32*1024*1024)return false;bytes=f.readAll();return f.error()==QFileDevice::NoError;}
bool write(const QString& path,const QByteArray& bytes){QSaveFile f(path);return f.open(QIODevice::WriteOnly) && f.write(bytes)==bytes.size() && f.commit();}
QString absolute(const QString& path){return QDir::cleanPath(QFileInfo(path).absoluteFilePath());}
bool safe(const QString& root,const QString& path) {
    const QString relative=QDir(root).relativeFilePath(absolute(path));
    const auto parts=relative.split('/');
    if(relative=="manifest.json")return !QFileInfo(path).isSymLink();
    return parts.size()==2 && !parts[0].isEmpty() && !parts[0].startsWith('.') && !parts[0].contains(':')
        && (parts[1]=="project.json" || parts[1]=="profile-settings.json")
        && !QFileInfo(QDir(root).filePath(parts[0])).isSymLink() && !QFileInfo(QDir(root).filePath(parts[0])).isJunction()
        && !QFileInfo(path).isSymLink();
}
bool discard(const QString& folder) {
    const auto entries=QDir(folder).entryInfoList(QDir::Files|QDir::Dirs|QDir::Hidden|QDir::System|QDir::NoDotAndDotDot);
    for(const auto& file:entries)if(file.isSymLink() || file.isDir()
        || (file.fileName()!="journal.json" && file.fileName()!="0" && file.fileName()!="1" && file.fileName()!="2"))return false;
    for(const auto& file:entries)if(file.fileName()!="journal.json" && !QFile::remove(file.absoluteFilePath()))return false;
    if(QFileInfo::exists(QDir(folder).filePath("journal.json")) && !QFile::remove(QDir(folder).filePath("journal.json")))return false;
    return QDir().rmdir(folder);
}
ProfileMetadataResult restore(const QString& root,const QString& folder,const QJsonObject& journal) {
    ProfileMetadataResult result;
    if(!journal["files"].isArray() || journal["files"].toArray().isEmpty() || journal["files"].toArray().size()>3)return result;
    struct Original{QString path;QByteArray bytes;bool existed;};QVector<Original> originals;QSet<QString> seen;
    int index=0;
    for(const auto& value:journal["files"].toArray()) {
        const auto file=value.toObject();const QString path=absolute(QDir(root).filePath(file["relative"].toString()));
        if(!file["relative"].isString() || !file["existed"].isBool() || !safe(root,path) || seen.contains(path.toCaseFolded()))return result;
        seen.insert(path.toCaseFolded());QByteArray before,current;
        const QString saved=QDir(folder).filePath(QString::number(index++));
        if(QFileInfo(saved).isSymLink() || !read(saved,before) || hash(before)!=file["before"].toString())return result;
        const bool exists=QFileInfo::exists(path);
        if(exists && (!read(path,current) || (hash(current)!=file["before"].toString() && hash(current)!=file["after"].toString())))return result;
        if(!exists && file["existed"].toBool())return result;
        originals.push_back({path,before,file["existed"].toBool()});
    }
    // Validate every snapshot and current destination before touching any file.
    bool restored=true;
    for(auto it=originals.crbegin();it!=originals.crend();++it) {
        if(it->existed) {QByteArray current;if(!read(it->path,current) || (current!=it->bytes && !write(it->path,it->bytes)))restored=false;}
        else if(QFileInfo::exists(it->path) && !QFile::remove(it->path))restored=false;
    }
    result.success=restored;
    return result;
}
}

ProfileMetadataResult recoverProfileMetadata(const QString& rootPath) {
    const QString root=absolute(rootPath);ProfileMetadataResult result;result.success=true;
    for(const auto& directory:QDir(root).entryInfoList({".profile-edit-*"},QDir::Dirs|QDir::Hidden|QDir::NoDotAndDotDot,QDir::Name)) {
        const QString folder=directory.absoluteFilePath();QByteArray raw;
        if(!directory.isSymLink() && !directory.isJunction()
            && QDir(folder).entryList(QDir::Files|QDir::Dirs|QDir::Hidden|QDir::System|QDir::NoDotAndDotDot).isEmpty()) {
            if(!QDir().rmdir(folder)){result.success=false;result.message=folder;break;}continue;
        }
        if(directory.isSymLink() || directory.isJunction() || QFileInfo(QDir(folder).filePath("journal.json")).isSymLink()
            || !read(QDir(folder).filePath("journal.json"),raw)) {result.success=false;result.message=folder;break;}
        const auto document=QJsonDocument::fromJson(raw);auto journal=document.object();const auto state=journal["state"].toString();
        if(!document.isObject() || !journal["files"].isArray() || journal["files"].toArray().isEmpty()
            || journal["files"].toArray().size()>3 || (state!="prepared" && state!="committed" && state!="rolled-back")) {result.success=false;result.message=folder;break;}
        if(state=="prepared") {
            if(!restore(root,folder,journal).success){result.success=false;result.message=folder;break;}
            journal["state"]="rolled-back";
            if(!write(QDir(folder).filePath("journal.json"),QJsonDocument(journal).toJson())){result.success=false;result.message=folder;break;}
        }
        if(!discard(folder))result.message=folder;
        if(!result.success)break;
    }
    result.recoveryRequired=!result.success;
    if(!result.message.isEmpty())result.message=QObject::tr("프로필 편집 복구 파일을 확인해 주세요: %1").arg(result.message);
    return result;
}

ProfileMetadataResult writeProfileMetadata(const QString& rootPath,const QVector<ProfileMetadataFile>& files,
    const std::function<void(int)>& afterCommitForTest) {
    ProfileMetadataResult result;const QString root=absolute(rootPath);
    if(files.isEmpty() || files.size()>3)return result;
    QTemporaryDir backup(QDir(root).filePath(".profile-edit-XXXXXX"));
    if(!backup.isValid())return result;
    QJsonArray records;QVector<QByteArray> originals;QVector<bool> existed;QSet<QString> seen;
    for(int i=0;i<files.size();++i) {
        const auto& file=files[i];const auto path=absolute(file.path);
        if(!safe(root,path) || seen.contains(path.toCaseFolded()))return result;
        seen.insert(path.toCaseFolded());const bool exists=QFileInfo::exists(path);QByteArray before;
        if((exists && !read(path,before)) || !write(QDir(backup.path()).filePath(QString::number(i)),before))return result;
        originals.push_back(before);existed.push_back(exists);
        records.push_back(QJsonObject{{"relative",QDir(root).relativeFilePath(path)},{"existed",exists},{"before",hash(before)},{"after",hash(file.bytes)}});
    }
    QJsonObject journal{{"state","prepared"},{"files",records}};
    const QString record=QDir(backup.path()).filePath("journal.json");
    if(!write(record,QJsonDocument(journal).toJson()))return result;
    backup.setAutoRemove(false);
    bool ok=true;
    for(int i=0;i<files.size() && ok;++i) {
        QByteArray current;const auto& file=files[i];const bool exists=QFileInfo::exists(file.path);
        ok=exists==existed[i] && (!exists || (read(file.path,current) && current==originals[i])) && write(file.path,file.bytes);
        if(ok && afterCommitForTest)afterCommitForTest(i);
    }
    if(ok) {journal["state"]="committed";ok=write(record,QJsonDocument(journal).toJson());}
    if(!ok) {
        result.recoveryRequired=!restore(root,backup.path(),journal).success;
        if(!result.recoveryRequired) {
            journal["state"]="rolled-back";
            result.recoveryRequired=!write(record,QJsonDocument(journal).toJson());
            if(!result.recoveryRequired)discard(backup.path());
        }
        result.message=result.recoveryRequired ? QObject::tr("프로필 저장과 이전 상태 복원에 실패했어요. 복구 파일: %1").arg(backup.path())
                                              : QObject::tr("프로필을 저장하지 못해 이전 상태로 되돌렸어요.");
        return result;
    }
    result.success=true;
    if(!discard(backup.path()))result.message=QObject::tr("프로필은 저장했지만 정리할 백업이 남았어요: %1").arg(backup.path());
    return result;
}
