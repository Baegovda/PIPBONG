#include "storage/ProfilePackageImport.h"
#include "storage/ProjectPackage.h"
#include "storage/JsonSerializer.h"
#include "model/Project.h"
#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>
#include <QSaveFile>
#include <QTemporaryDir>
#include <QtConcurrent/QtConcurrent>

namespace {
bool copyAtomic(const QString& source,const QString& destination) {
    QFile input(source);QSaveFile output(destination);
    if(!input.open(QIODevice::ReadOnly) || !output.open(QIODevice::WriteOnly))return false;
    while(!input.atEnd()) {const auto bytes=input.read(1024*1024);if(bytes.isEmpty() || output.write(bytes)!=bytes.size())return false;}
    return input.error()==QFileDevice::NoError && output.commit();
}
}
bool rebaseUnpackedProfile(const QString& workspace,const QString& destination,QString* error) {
    const auto fail=[&](const QString& message){if(error)*error=message;return false;};
    QDirIterator entries(workspace,QDir::AllEntries|QDir::Hidden|QDir::System|QDir::NoDotAndDotDot,QDirIterator::Subdirectories);
    while(entries.hasNext()){entries.next();const auto info=entries.fileInfo();if(info.isSymLink() || info.isJunction())return fail(QStringLiteral("연결된 파일이나 폴더가 있는 패키지는 가져올 수 없어요."));}
    const auto projectPath=QDir(workspace).filePath("project.json");
    if(!JsonSerializer::loadFromFile(projectPath))return fail(QStringLiteral("올바른 프로젝트가 없는 패키지예요."));
    QFile project(projectPath);if(!project.open(QIODevice::ReadOnly))return fail(QStringLiteral("프로젝트를 읽지 못했어요."));
    auto document=QJsonDocument::fromJson(project.readAll());project.close();
    if(!document.isObject())return fail(QStringLiteral("프로젝트 형식을 확인해 주세요."));
    const auto settingsPath=QDir(workspace).filePath("profile-settings.json");QFile settings(settingsPath);
    if(settings.exists() && (!settings.open(QIODevice::ReadOnly) || !QJsonDocument::fromJson(settings.readAll()).isObject()))
        return fail(QStringLiteral("프로필 설정 형식이 올바르지 않아요."));
    auto object=document.object();object["projectDirectory"]=QDir::cleanPath(QFileInfo(destination).absoluteFilePath());
    const auto bytes=QJsonDocument(object).toJson();QSaveFile output(projectPath);
    if(!output.open(QIODevice::WriteOnly) || output.write(bytes)!=bytes.size() || !output.commit())
        return fail(QStringLiteral("가져온 프로젝트 경로를 준비하지 못했어요."));
    return true;
}
std::shared_ptr<PreparedProfileImport> prepareProfileImport(const ProfileImportRequest& request) {
    const auto& root=request.root;const auto& id=request.id;const auto& name=request.name;const auto& source=request.source;
    auto result=std::make_shared<PreparedProfileImport>();result->root=QDir::cleanPath(QFileInfo(root).absoluteFilePath());result->id=id;result->name=name;
    const auto fail=[&](const QString& message){result->error=message;return result;};
    if(!QRegularExpression("^[A-Za-z0-9_-]+$").match(id).hasMatch() || !QFileInfo(result->root).isDir()
        || !ProjectPackage::isPackagePath(source))return fail(QStringLiteral("가져올 패키지 위치를 확인해 주세요."));
    result->directory=QDir(result->root).filePath(id);result->package=QDir(result->root).filePath(id+".pipbong");
    if(!request.recovery && (QFileInfo::exists(result->directory) || QFileInfo::exists(result->package)))return fail(QStringLiteral("가져오기 저장 위치가 이미 사용 중이에요."));
    result->temporary=std::shared_ptr<QTemporaryDir>(new QTemporaryDir(QDir(result->root).filePath(".pipbong-import-XXXXXX")),
        [](QTemporaryDir* directory){(void)QtConcurrent::run([directory]{delete directory;});});
    if(!result->temporary->isValid())return fail(QStringLiteral("임시 저장 공간을 준비하지 못했어요."));
    result->workspace=result->temporary->filePath("workspace");result->archive=result->temporary->filePath("source.pipbong");
    if(!copyAtomic(source,result->archive))return fail(QStringLiteral("패키지 사본을 준비하지 못했어요."));
    if(!ProjectPackage::unpackToDirectory(result->archive,result->workspace))return fail(QStringLiteral("패키지를 풀지 못했어요. 파일을 확인해 주세요."));
    if(!rebaseUnpackedProfile(result->workspace,result->directory,&result->error))return result;
    const auto model=JsonSerializer::loadFromFile(QDir(result->workspace).filePath("project.json"));
    if(!model)return fail(QStringLiteral("준비 중 프로젝트 파일이 바뀌었어요. 다시 가져와 주세요."));
    result->projectTitle=QString::fromStdString(model->targetWindowTitle());
    if(!request.recovery) {
        // Cache a package with the new root too; later workspace recovery must
        // not reintroduce the foreign machine/profile path from the source ZIP.
        const auto prepared=result->temporary->filePath("prepared.pipbong");
        if(!ProjectPackage::packDirectoryAtAssetRoot(result->workspace,prepared,result->workspace))return fail(QStringLiteral("가져올 프로필의 복구 패키지를 준비하지 못했어요."));
        result->archive=prepared;
    }
    result->ready=true;return result;
}
bool recoverProfilePackage(const QString& root,const QString& id,const QString& package) {
    auto prepared=prepareProfileImport({root,id,QString(),package,true});if(!prepared->ready)return false;
    const auto destination=prepared->directory;QFileInfo top(destination);
    if(top.isSymLink() || top.isJunction() || (top.exists() && !top.isDir()))return false;
    if(!QDir().mkpath(destination))return false;
    QDirIterator entries(prepared->workspace,QDir::Files|QDir::Dirs|QDir::Hidden|QDir::System|QDir::NoDotAndDotDot,QDirIterator::Subdirectories);
    while(entries.hasNext()) {
        const auto source=entries.next();const auto info=entries.fileInfo();const auto relative=QDir(prepared->workspace).relativeFilePath(source);
        if(relative=="project.json")continue;
        const auto parts=relative.split('/');auto parent=destination;
        for(int i=0;i<parts.size()-1;++i){parent=QDir(parent).filePath(parts[i]);QFileInfo p(parent);
            if(p.isSymLink() || p.isJunction() || (p.exists() && !p.isDir()) || !QDir().mkpath(parent))return false;}
        const auto target=QDir(destination).filePath(relative);QFileInfo existing(target);
        if(existing.isSymLink() || existing.isJunction())return false;
        if(existing.exists()){if(existing.isDir()!=info.isDir())return false;continue;}
        if(info.isDir()){if(!QDir().mkpath(target))return false;}
        else if(!copyAtomic(source,target))return false;
    }
    // The project is the readiness marker. Preserve existing user files and
    // publish this last, so an interrupted/failed restore stays retryable.
    const auto target=QDir(destination).filePath("project.json");
    if(QFileInfo::exists(target))return bool(JsonSerializer::loadFromFile(target));
    return copyAtomic(QDir(prepared->workspace).filePath("project.json"),target);
}
