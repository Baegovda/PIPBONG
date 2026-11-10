#include "app/ProfileManager.h"
#include "app/ProjectWorkspaceController.h"
#include "storage/ProjectPackage.h"
#include "storage/JsonSerializer.h"
#include "model/Project.h"
#include "core/workflow/ExecutionContext.h"
#include "core/workflow/blocks/ImageFindBlock.h"
#include "core/vision/TemplateCache.h"
#include <QTemporaryDir>
#include <QDir>
#include <QFile>
#include <QImage>
#include <QColor>
#include <iostream>
#include <QThreadPool>
#ifdef _WIN32
#include <windows.h>
#endif

namespace {
QByteArray bytes(const QString& path){QFile f(path);return f.open(QIODevice::ReadOnly)?f.readAll():QByteArray();}
bool write(const QString& path,const QByteArray& data){QFile f(path);return f.open(QIODevice::WriteOnly) && f.write(data)==data.size();}
// Tiny stored ZIP fixtures allow names/modes Windows cannot create on disk.
bool zip(const QString& path,const QList<QPair<QByteArray,QByteArray>>& entries,quint32 mode=0100644) {
    QByteArray out,central;const auto put=[](QByteArray& b,quint32 n,int size){for(int i=0;i<size;++i)b.append(char(n>>(8*i)));};
    for(const auto& e:entries){const auto& name=e.first;const auto& data=e.second;quint32 crc=0xffffffff;
        for(auto c:data){crc^=static_cast<unsigned char>(c);for(int i=0;i<8;++i)crc=(crc>>1)^(0xedb88320u & (0u-(crc&1)));}crc^=0xffffffff;
        const quint32 offset=out.size();put(out,0x04034b50,4);put(out,20,2);put(out,0x800,2);put(out,0,2);put(out,0,4);
        put(out,crc,4);put(out,data.size(),4);put(out,data.size(),4);put(out,name.size(),2);put(out,0,2);out+=name;out+=data;
        put(central,0x02014b50,4);put(central,0x0314,2);put(central,20,2);put(central,0x800,2);put(central,0,2);put(central,0,4);
        put(central,crc,4);put(central,data.size(),4);put(central,data.size(),4);put(central,name.size(),2);put(central,0,2);put(central,0,2);
        put(central,0,2);put(central,0,2);put(central,mode<<16,4);put(central,offset,4);central+=name;
    }
    const quint32 offset=out.size();out+=central;put(out,0x06054b50,4);put(out,0,4);put(out,entries.size(),2);put(out,entries.size(),2);
    put(out,central.size(),4);put(out,offset,4);put(out,0,2);return write(path,out);
}
}

bool runProfilePackageImportChecks() try {
    std::cerr<<"Package import check: deleted-source Korean asset fixture\n";
    const auto fail=[](const char* s){std::cerr<<"Package import FAILED: "<<s<<'\n';return false;};
    {
        QTemporaryDir exported,importRoot;const auto archive=exported.filePath("external.pipbong");
        const auto profile=exported.filePath("profile");QDir().mkpath(QDir(profile).filePath("templates"));
        QByteArray first,second,sourceJson;QString originalAbsolute;
        {
            QTemporaryDir assets,absolute;QDir().mkpath(assets.filePath("templates"));
            const auto relative=QString::fromUtf8("templates/같은 이름.png");const auto absolutePath=absolute.filePath(QString::fromUtf8("같은 이름.png"));
            QImage image(8,8,QImage::Format_RGB32);image.fill(QColor(30,160,90));if(!image.save(assets.filePath(relative)))return fail("external fixture image");first=bytes(assets.filePath(relative));
            image.fill(QColor(190,60,120));if(!image.save(absolutePath))return fail("absolute fixture image");second=bytes(absolutePath);originalAbsolute=absolutePath;
            image.fill(Qt::red);if(!image.save(QDir(profile).filePath(relative)))return fail("canonical collision fixture");
            if(!write(assets.filePath(relative)+".capture.json","{\"origin\":\"relative\"}") || !write(absolutePath+".capture.json","{\"origin\":\"absolute\"}"))return false;
            Project project;auto* feature=project.addFeature("external-assets");feature->setId("external-assets");
            auto find=std::make_unique<ImageFindBlock>();find->templatePaths={relative.toStdString(),absolutePath.toStdString(),absolutePath.toStdString()};feature->workflow().addBlock(std::move(find));
            if(!JsonSerializer::saveToFile(project,QDir(profile).filePath("project.json"),assets.path()))return false;
            sourceJson=bytes(QDir(profile).filePath("project.json"));
            if(!ProjectPackage::packDirectory(profile,archive) || bytes(QDir(profile).filePath("project.json"))!=sourceJson)return fail("export preserves source document");
        }
        ProfileManager imported(importRoot.path());if(!imported.initialize())return false;
        const auto id=imported.importProfileFromPackage(archive);if(id.isEmpty())return fail("external archive import after originals disappear");
        auto project=JsonSerializer::loadFromFile(imported.projectPath(id));if(!project)return false;
        auto* find=static_cast<ImageFindBlock*>(project->featureById("external-assets")->workflow().blocks()[0].get());
        ExecutionContext context;context.setProjectDirectory(imported.projectDirectory(id).toStdString());
        if(find->templatePaths.size()!=3)return false;
        const auto a=QString::fromStdString(context.resolvePath(find->templatePaths[0]));
        const auto b=QString::fromStdString(context.resolvePath(find->templatePaths[1]));
        if(QFileInfo(QString::fromStdString(find->templatePaths[0])).isAbsolute() || QFileInfo(QString::fromStdString(find->templatePaths[1])).isAbsolute()
            || a==b || find->templatePaths[1]!=find->templatePaths[2] || bytes(a)!=first || bytes(b)!=second)
            return fail("exported profile must own external relative/absolute templates with collision-safe paths");
        if(bytes(a+".capture.json")!="{\"origin\":\"relative\"}" || bytes(b+".capture.json")!="{\"origin\":\"absolute\"}")return fail("external template metadata travels with image");
        const auto loaded=TemplateCache::getOrLoad(context.resolvePath(find->templatePaths[0]));
        if(!loaded || loaded->empty() || loaded->bgr.at<cv::Vec3b>(0,0)!=cv::Vec3b(90,160,30))return fail("materialized external PNG pixels");
        const auto previous=bytes(archive);
        if(ProjectPackage::packDirectory(profile,archive) || bytes(archive)!=previous || bytes(QDir(profile).filePath("project.json"))!=sourceJson)
            return fail("missing external image rejects export and preserves prior archive/source");
        std::cerr<<"Package external assets: deleted-source relative/absolute Korean PNGs, equal-name collision, duplicate identity, sidecars, pixels and failed-export retention OK\n";
    }
    {
        QTemporaryDir sandbox,document;ProfileManager profiles(sandbox.path());if(!profiles.initialize())return false;
        const auto privateImage=sandbox.filePath("private.png");QImage image(4,4,QImage::Format_RGB32);image.fill(Qt::yellow);
        if(!image.save(privateImage))return false;const auto original=bytes(privateImage);const auto count=profiles.profiles().size();
        for(const auto& reference:QStringList{privateImage,"../../../private.png","..\\..\\..\\private.png"}){
            Project project;auto* feature=project.addFeature("unsafe reference");auto find=std::make_unique<ImageFindBlock>();find->templatePaths={reference.toStdString()};feature->workflow().addBlock(std::move(find));
            if(!JsonSerializer::saveToFile(project,document.filePath("project.json"),document.path()))return false;
            const auto archive=sandbox.filePath("unsafe.pipbong");
            if(!zip(archive,{{"project.json",bytes(document.filePath("project.json"))}}))return false;
            auto prepared=prepareProfileImport(profiles.importRequest(archive));
            if(prepared->ready || bytes(privateImage)!=original || profiles.profiles().size()!=count)return fail("import cache must not materialize absolute/escaping local references");
            prepared.reset();QThreadPool::globalInstance()->waitForDone(15000);
        }
        std::cerr<<"Package external assets: import cache refuses absolute and both-separator escaping local references OK\n";
    }
    const auto settings=ProgramSettings::profileSettings();struct Restore{ProgramSettings::ProfileSettings s;~Restore(){ProgramSettings::applyProfileSettings(s);QThreadPool::globalInstance()->waitForDone(15000);}} restore{settings};
    QTemporaryDir data(QDir::tempPath()+QStringLiteral("/pipbong-한글-XXXXXX"));ProfileManager profiles(data.path());if(!data.isValid() || !profiles.initialize())return fail("fixture");
    const auto archive=data.filePath("incoming.pipbong");QByteArray originalImage;
    const QString relative=QStringLiteral("templates/한글 이미지.png");
    {
        QTemporaryDir source;QDir().mkpath(source.filePath("templates"));
        QImage image(8,8,QImage::Format_RGB32);image.fill(QColor(80,150,130));if(!image.save(source.filePath(relative)))return fail("image");
        originalImage=bytes(source.filePath(relative));
        Project project;auto* f=project.addFeature("portable");f->setId("portable");auto block=std::make_unique<ImageFindBlock>();block->templatePaths={relative.toStdString()};f->workflow().addBlock(std::move(block));
        if(!JsonSerializer::saveToFile(project,source.filePath("project.json"),source.path())
            || !write(source.filePath("profile-settings.json"),"{}") || !write(source.filePath("memo.html"),"old memo")
            || !ProjectPackage::packDirectory(source.path(),archive))return fail("archive");
    } // The original PC/source directory no longer exists.
    const auto id=profiles.importProfileFromPackage(archive);if(id.isEmpty())return fail("import");
    ProjectWorkspaceController workspace(profiles,profiles.projectPath(id));if(!workspace.loadActive(true,false))return fail("load imported");
    ExecutionContext context;context.setProjectDirectory(workspace.state().directory.toStdString());
    if(workspace.state().directory!=profiles.projectDirectory(id)
        || bytes(QString::fromStdString(context.resolvePath(relative.toStdString())))!=originalImage)
        return fail("new profile runtime template path must use imported root after original source disappears");
    const auto loaded=TemplateCache::getOrLoad(context.resolvePath(relative.toStdString()));
    if(!loaded || loaded->empty() || loaded->bgr.cols!=8 || loaded->bgr.rows!=8
        || loaded->bgr.at<cv::Vec3b>(0,0)!=cv::Vec3b(130,150,80))return fail("actual template loader decodes imported Korean PNG pixels");
    const auto manifest=QDir(profiles.profilesDirectory()).filePath("manifest.json");const auto before=bytes(manifest);const auto count=profiles.profiles().size();
    std::cerr<<"Package import check: invalid metadata/archive retention\n";
    const auto unchanged=[&]{return profiles.activeProfileId()==id && profiles.profiles().size()==count && bytes(manifest)==before && !bytes(profiles.projectPath(id)).isEmpty();};
    for(int invalid=0;invalid<3;++invalid){QTemporaryDir source;Project project;
        if(invalid!=0 && !JsonSerializer::saveToFile(project,source.filePath("project.json"),source.path()))return false;
        if(invalid==1 && !write(source.filePath("project.json"),"{broken"))return false;
        if(invalid==2 && !write(source.filePath("profile-settings.json"),"[1]"))return false;
        const auto bad=data.filePath(QString("invalid-%1.pipbong").arg(invalid));
        if(!ProjectPackage::packDirectory(source.path(),bad) || !profiles.importProfileFromPackage(bad).isEmpty() || !unchanged())return fail("invalid metadata retains old profile and manifest");
    }
    const auto bad=data.filePath("bad.pipbong");
    if(!write(bad,"not a ZIP") || !profiles.importProfileFromPackage(bad).isEmpty() || !unchanged())return fail("corrupt archive retention");
    for(const auto& name:QList<QByteArray>{"../escaped.txt","/escaped.txt","C:/escaped.txt","file.txt:stream","templates/NUL.txt","trailing. ","nested/../escaped.txt"})
        if(!zip(bad,{{name,"bad"}}) || ProjectPackage::unpackToDirectory(bad,data.filePath("rejected")))return fail("unsafe ZIP path rejected before extraction");
    if(!zip(bad,{{"same.txt","a"},{"SAME.TXT","b"}}) || ProjectPackage::unpackToDirectory(bad,data.filePath("rejected")))return fail("case alias ZIP rejected");
    if(!zip(bad,{{"link","../outside"}},0120777) || ProjectPackage::unpackToDirectory(bad,data.filePath("rejected")))return fail("ZIP symlink rejected");
    if(QFileInfo::exists(data.filePath("rejected")) || QFileInfo::exists(data.filePath("escaped.txt")))return fail("rejected archive creates no destination");
#ifdef _WIN32
    std::cerr<<"Package import check: manifest lock rollback\n";
    auto prepared=prepareProfileImport(profiles.importRequest(archive));if(!prepared->ready)return fail("prepare rollback");
    const auto handle=CreateFileW(reinterpret_cast<LPCWSTR>(manifest.utf16()),GENERIC_READ,FILE_SHARE_READ,nullptr,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,nullptr);
    if(handle==INVALID_HANDLE_VALUE)return fail("manifest lock");
    QString error;const auto imported=profiles.commitImportedProfile(prepared,&error);CloseHandle(handle);
    if(!imported.isEmpty() || error.isEmpty() || !unchanged() || QFileInfo::exists(prepared->directory) || QFileInfo::exists(prepared->package))return fail("manifest failure rolls back newly owned files and registration");
    prepared.reset();
#endif
    const auto directory=profiles.profileDirectory(id);const auto memo=QDir(directory).filePath("memo.html");
    std::cerr<<"Package import check: legacy recovery and restart\n";
    const auto profileSettings=QDir(directory).filePath("profile-settings.json");
    if(!QFile::remove(profiles.projectPath(id)) || !QFile::remove(QDir(directory).filePath(relative))
        || !write(memo,"new local memo") || !write(profileSettings,"{\"local\":true}") || !write(QDir(directory).filePath("keep.txt"),"keep"))return fail("recovery setup");
    const auto cache=profiles.profilePackagePath(id);if(!QFile::remove(cache) || !QFile::copy(archive,cache))return fail("legacy recovery cache");
    if(!profiles.ensureProfileWorkspace(id) || bytes(memo)!="new local memo" || bytes(profileSettings)!="{\"local\":true}"
        || bytes(QDir(directory).filePath("keep.txt"))!="keep" || bytes(QDir(directory).filePath(relative))!=originalImage)return fail("recovery preserves local edits");
    ProfileManager restarted(data.path());if(!restarted.initialize() || !restarted.setActiveProfile(id))return false;
    ProjectWorkspaceController reloaded(restarted,restarted.projectPath(id));if(!reloaded.loadActive(true,false))return false;
    context.setProjectDirectory(reloaded.state().directory.toStdString());
    if(reloaded.state().directory!=restarted.projectDirectory(id)
        || bytes(QString::fromStdString(context.resolvePath(relative.toStdString())))!=originalImage)return fail("recovery rebases runtime root across restart");
    std::cout<<"Package import assets: deleted-source Korean PNG runtime resolution, staged validation, corrupt/missing metadata retention, unsafe ZIP rejection, manifest rollback and non-destructive legacy recovery/restart OK\n";
    return true;
} catch(const std::exception& error) {
    std::cerr<<"Package import FAILED with exception: "<<error.what()<<'\n';return false;
}
