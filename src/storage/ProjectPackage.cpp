#include "storage/ProjectPackage.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QProcess>
#include <QSettings>
#include <QSaveFile>
#include <QStandardPaths>
#include <QUuid>
#include <QRegularExpression>
#include <QSet>
#include <QTemporaryDir>
#include <QHash>
#include <QDebug>
#include <nlohmann/json.hpp>
#include <memory>
#include <vector>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#endif

namespace {

bool runTar(const QStringList& arguments, int timeoutMs,QByteArray* listing=nullptr) {
    QProcess process;
    bool overflow=false;
    if(listing)QObject::connect(&process,&QProcess::readyReadStandardOutput,&process,[&]{
        const auto bytes=process.readAllStandardOutput();
        if(listing->size()+bytes.size()>16*1024*1024){overflow=true;process.kill();}else listing->append(bytes);
    });
    process.setProgram(QStringLiteral("tar"));
    process.setArguments(arguments);
#ifdef _WIN32
    process.setCreateProcessArgumentsModifier([](QProcess::CreateProcessArguments* args) {
        args->flags |= CREATE_NO_WINDOW;
    });
#endif
    process.start();
    if (!process.waitForStarted(5000)) {
        return false;
    }
    if (!process.waitForFinished(timeoutMs)) {
        process.kill();
        process.waitForFinished(3000);
        return false;
    }
    if(listing)listing->append(process.readAllStandardOutput());
    return !overflow && process.exitStatus() == QProcess::NormalExit && process.exitCode() == 0;
}

bool portableArchive(const QString& path) {
    QFile file(path);if(!file.open(QIODevice::ReadOnly) || !file.read(4).startsWith("PK"))return false;
    QByteArray names,types;
    if(!runTar({"-tf",QDir::toNativeSeparators(path)},120000,&names)
        || !runTar({"-tvf",QDir::toNativeSeparators(path)},120000,&types))return false;
    // Reject links/devices before extraction; standard packages contain only files/directories.
    for(auto line:types.split('\n')){if(line.endsWith('\r'))line.chop(1);if(!line.isEmpty() && line[0]!='-' && line[0]!='d')return false;}
    static const QRegularExpression device("^(CON|PRN|AUX|NUL|COM[1-9¹²³]|LPT[1-9¹²³])(?:\\..*)?$",QRegularExpression::CaseInsensitiveOption);
    QSet<QString> seen;
    for(auto bytes:names.split('\n')) {
        if(bytes.endsWith('\r'))bytes.chop(1);if(bytes.isEmpty())continue;
        // Windows bsdtar writes listing text in the system code page, even
        // when the ZIP entry itself uses UTF-8 (e.g. Korean template names).
        auto name=QString::fromLocal8Bit(bytes);if(name.contains(QChar::ReplacementCharacter))return false;
        name.replace('\\','/');if(name.startsWith('/'))return false;
        for(const auto c:name)if(c.unicode()<32 || QStringLiteral("<>:\"|?*").contains(c))return false;
        const auto parts=name.split('/');
        for(const auto& part:parts){if(part.isEmpty() || part==".")continue;
            if(part==".." || part.endsWith('.') || part.endsWith(' ') || device.match(part).hasMatch())return false;}
        const auto normalized=QDir::cleanPath(name).toCaseFolded();
        if(seen.contains(normalized))return false;seen.insert(normalized);
    }
    return true;
}

QString normalizedAbsolutePath(const QString& path) {
    return QDir::cleanPath(QDir::fromNativeSeparators(QFileInfo(path).absoluteFilePath()));
}

// Copy only into an owned private staging root. Never follow directory links
// or alter the original profile/document while making an export portable.
bool copyWorkspace(const QString& source,const QString& target,const QString& output){
    std::vector<QPair<QString,QString>> pending{{source,target}};
    while(!pending.empty()){
        const auto pair=pending.back();pending.pop_back();
        if(!QDir().mkpath(pair.second))return false;
        for(const auto& info:QDir(pair.first).entryInfoList(QDir::AllEntries|QDir::Hidden|QDir::System|QDir::NoDotAndDotDot)){
            if(info.fileName().endsWith(".history") || normalizedAbsolutePath(info.filePath()).compare(output,Qt::CaseInsensitive)==0)continue;
            if(info.isSymLink() || info.isJunction())return false;
            const auto destination=QDir(pair.second).filePath(info.fileName());
            if(info.isDir())pending.push_back({info.filePath(),destination});
            else if(!info.isFile() || !QFile::copy(info.filePath(),destination))return false;
        }
    }
    return true;
}

bool portableSource(const QString& source,const QString& output,const QString& overrideRoot,
                    std::unique_ptr<QTemporaryDir>& staging,QString& packedSource){
    packedSource=source;
    QFile project(QDir(source).filePath("project.json"));
    if(!project.exists())return true; // Keep folder packing/invalid-import fixtures compatible.
    if(!project.open(QIODevice::ReadOnly))return false;
    const auto bytes=project.readAll();if(project.error()!=QFileDevice::NoError)return false;
    auto json=nlohmann::json::parse(bytes.toStdString(),nullptr,false);
    if(!json.is_object() || !json.contains("features") || !json["features"].is_array())return true;
    if(json.contains("projectDirectory") && !json["projectDirectory"].is_string())return true;
    const auto declared=QString::fromStdString(json.value("projectDirectory",std::string()));
    const auto root=normalizedAbsolutePath(!overrideRoot.isEmpty()?overrideRoot:declared.isEmpty()?source:QDir(source).absoluteFilePath(declared));
    struct Asset {nlohmann::json* value;QString source;};
    std::vector<Asset> assets;std::vector<nlohmann::json*> pending{&json["features"]};
    bool needsStaging=false;
    while(!pending.empty()){
        auto* value=pending.back();pending.pop_back();
        if(value->is_object()){
            if(value->value("type",nlohmann::json())=="ImageFind"){
                const auto found=value->find("templates");
                if(found!=value->end() && found->is_array())for(auto& item:*found){
                    if(!item.is_string() || item.get_ref<const std::string&>().empty())continue;
                    const auto path=QString::fromStdString(item.get<std::string>());
                    const auto absolute=normalizedAbsolutePath(QDir(root).absoluteFilePath(path));
                    // Import-cache staging must never pull unrelated local files
                    // named by an untrusted archive's absolute/escaping reference.
                    if(!overrideRoot.isEmpty() && (QFileInfo(path).isAbsolute()
                        || !absolute.startsWith(source+QLatin1Char('/'),Qt::CaseInsensitive)))return false;
                    const QFileInfo info(absolute);
                    if(!info.isFile() || info.canonicalFilePath().isEmpty()){
                        qWarning()<<"Profile export template is missing or unreadable:"<<absolute;return false;
                    }
                    needsStaging=needsStaging || QFileInfo(path).isAbsolute()
                        || root.compare(source,Qt::CaseInsensitive)!=0
                        || !absolute.startsWith(source+QLatin1Char('/'),Qt::CaseInsensitive);
                    assets.push_back({&item,info.canonicalFilePath()});
                }
            }
            for(auto& member:value->items())pending.push_back(&member.value());
        }else if(value->is_array())for(auto& item:*value)pending.push_back(&item);
    }
    if(!needsStaging)return true;
    staging=std::make_unique<QTemporaryDir>(QDir::tempPath()+"/pipbong-export-XXXXXX");
    if(!staging->isValid() || !copyWorkspace(source,staging->path(),output))return false;
    const auto folder=QStringLiteral("templates/export-")+QUuid::createUuid().toString(QUuid::WithoutBraces);
    QHash<QString,QString> copied;
    for(const auto& asset:assets){
        const auto key=QDir::fromNativeSeparators(asset.source);
        auto relative=copied.value(key);
        if(relative.isEmpty()){
            relative=folder+QLatin1Char('/')+QString::number(copied.size()+1)+QLatin1Char('/')+QFileInfo(asset.source).fileName();
            const auto destination=QDir(staging->path()).filePath(relative);
            if(!QDir().mkpath(QFileInfo(destination).absolutePath()) || !QFile::copy(asset.source,destination))return false;
            const auto metadata=asset.source+QStringLiteral(".capture.json");
            if(QFileInfo::exists(metadata) && (!QFileInfo(metadata).isFile() || !QFile::copy(metadata,destination+".capture.json")))return false;
            copied.insert(key,relative);
        }
        *asset.value=relative.toStdString();
    }
    // Empty means the unpacked project folder; import may then rebase it to
    // its committed profile location. No original machine path is required.
    json["projectDirectory"]="";
    const auto serialized=QByteArray::fromStdString(json.dump(2))+"\n";
    QSaveFile outputProject(QDir(staging->path()).filePath("project.json"));
    if(!outputProject.open(QIODevice::WriteOnly) || outputProject.write(serialized)!=serialized.size() || !outputProject.commit())return false;
    packedSource=staging->path();return true;
}

} // namespace

bool ProjectPackage::isPackagePath(const QString& path) {
    return path.endsWith(QString::fromLatin1(kExtension), Qt::CaseInsensitive);
}

QString ProjectPackage::defaultExportDirectory() {
    QSettings settings;
    const QString lastExport =
        settings.value(QStringLiteral("project/lastExportDirectory")).toString().trimmed();
    if (!lastExport.isEmpty()) {
        QDir().mkpath(lastExport);
        return lastExport;
    }

    const QString documents =
        QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation);
    const QString exportDir = QDir(documents).filePath(QStringLiteral("PIPBONG/Exports"));
    QDir().mkpath(exportDir);
    return exportDir;
}

bool ProjectPackage::packDirectory(const QString& profileDirectory,const QString& packagePath){
    return packDirectoryAtAssetRoot(profileDirectory,packagePath,QString());
}

bool ProjectPackage::packDirectoryAtAssetRoot(const QString& profileDirectory,const QString& packagePath,const QString& assetRoot){
    const QString sourceDir = normalizedAbsolutePath(profileDirectory);
    if (!QFileInfo(sourceDir).isDir()) {
        return false;
    }

    const QString destination = normalizedAbsolutePath(packagePath);
    QDir().mkpath(QFileInfo(destination).absolutePath());

    const QString tempPath = destination + QStringLiteral(".tmp-")
        + QUuid::createUuid().toString(QUuid::WithoutBraces) + QStringLiteral(".zip");

    std::unique_ptr<QTemporaryDir> staging;QString packedSource;
    if(!portableSource(sourceDir,destination,assetRoot,staging,packedSource))return false;
    const QString nativeSource = QDir::toNativeSeparators(packedSource);
    const QString nativeTemp = QDir::toNativeSeparators(tempPath);
    if (!runTar({QStringLiteral("-a"),
                 QStringLiteral("--exclude=*.history"),
                 QStringLiteral("-cf"),
                 nativeTemp,
                 QStringLiteral("-C"),
                 nativeSource,
                 QStringLiteral(".")},
                120000)) {
        QFile::remove(tempPath);
        return false;
    }

    QFile archive(tempPath);
    QSaveFile output(destination);
    bool ok = archive.open(QIODevice::ReadOnly) && output.open(QIODevice::WriteOnly);
    while (ok && !archive.atEnd()) {
        const QByteArray chunk = archive.read(1024 * 1024);
        ok = archive.error() == QFileDevice::NoError && output.write(chunk) == chunk.size();
    }
    ok = ok && output.commit();
    archive.close();
    QFile::remove(tempPath);
    return ok;
}

bool ProjectPackage::unpackToDirectory(const QString& packagePath, const QString& profileDirectory) {
    const QString archivePath = normalizedAbsolutePath(packagePath);
    if (!QFileInfo::exists(archivePath) || !portableArchive(archivePath)) {
        return false;
    }

    const QString destination = normalizedAbsolutePath(profileDirectory);
    QDir().mkpath(destination);

    const QString nativeArchive = QDir::toNativeSeparators(archivePath);
    const QString nativeDest = QDir::toNativeSeparators(destination);
    return runTar({QStringLiteral("-xf"), nativeArchive, QStringLiteral("-C"), nativeDest}, 120000);
}
