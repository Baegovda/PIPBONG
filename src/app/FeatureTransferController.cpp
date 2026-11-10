#include "app/FeatureTransferController.h"
#include "app/ProfileManager.h"
#include "app/FeatureLibraryManager.h"
#include "model/Project.h"
#include "storage/JsonSerializer.h"
#include <QObject>
#include <QFile>
#include <algorithm>
#include <utility>

namespace {
int indexOf(const Project& project,const QString& id){
    for(int i=0;i<int(project.features().size());++i)if(project.featureAt(i)->id()==id.toStdString())return i;
    return -1;
}
QByteArray bytes(const QString& path){QFile file(path);return file.open(QIODevice::ReadOnly)?file.readAll():QByteArray();}
bool sameOwner(const std::shared_ptr<Project>& a,const std::shared_ptr<Project>& b){
    return a.get()==b.get() && !a.owner_before(b) && !b.owner_before(a);
}
}
struct FeatureTransferController::State {bool alive=true,busy=false;};
struct FeatureTransferController::Target {
    QString id,path,directory;bool active=false;std::shared_ptr<Project> model;QByteArray original;
};
struct FeatureTransferController::Operation {
    std::shared_ptr<State> state;HostCallbacks host;ProfileManager* profiles=nullptr;FeatureLibraryManager* library=nullptr;
    QString activeId,directory;std::shared_ptr<Project> model;quint64 generation=0;bool entered=false,ready=false;
    explicit Operation(FeatureTransferController& c):state(c.m_state){
        if(!state->alive || state->busy)return;
        state->busy=true;entered=true;host=c.m_host;profiles=&c.m_profiles;library=&c.m_library;
        if(!profiles->storageReady())return;activeId=profiles->activeProfileId();
        if(host.activeProject)model=host.activeProject();if(!alive())return;
        if(host.generation)generation=host.generation();if(!alive())return;
        directory=host.activeDirectory?host.activeDirectory():profiles->projectDirectory(activeId);if(!alive())return;
        ready=true;
    }
    ~Operation(){if(entered)state->busy=false;}
    bool alive()const{return state && state->alive;}
    bool current()const {
        if(!ready || !alive() || !profiles->storageReady() || !profiles->profileById(activeId) || profiles->activeProfileId()!=activeId)return false;
        const auto now=host.activeProject?host.activeProject():std::shared_ptr<Project>();if(!alive() || !sameOwner(now,model))return false;
        const auto revision=host.generation?host.generation():0;if(!alive() || revision!=generation)return false;
        const auto root=host.activeDirectory?host.activeDirectory():profiles->projectDirectory(activeId);
        return alive() && root==directory && profiles->storageReady() && profiles->activeProfileId()==activeId;
    }
    Target target(const QString& id)const {
        Target t;if(!current() || !profiles->profileById(id))return t;
        t.id=id;t.path=profiles->projectPath(id);t.directory=profiles->projectDirectory(id);t.active=id==activeId;
        if(t.active){t.model=model;t.directory=directory;}
        else{t.original=bytes(t.path);t.model=JsonSerializer::loadFromFile(t.path,&t.directory);}
        if(t.directory.isEmpty())t.model.reset();return t;
    }
    bool current(const Target& t)const {
        if(!current() || !t.model || !profiles->profileById(t.id) || profiles->projectPath(t.id)!=t.path)return false;
        return t.active?sameOwner(t.model,model):!t.original.isEmpty() && bytes(t.path)==t.original;
    }
    bool insert(Target& t,std::unique_ptr<Feature> feature,int index)const {
        if(!feature || !current(t))return false;
        if(!feature->groupId().empty() && !t.model->featureGroupById(feature->groupId()))feature->setGroupId({});
        const int count=t.model->features().size();t.model->insertFeature(index<0?count:qBound(0,index,count),std::move(feature));
        if(!t.active){if(!JsonSerializer::saveToFile(*t.model,t.path,t.directory))return false;profiles->invalidateCachedProject(t.id);}
        return true;
    }
    void publish(const Target& t,const QString& selected,bool activePublish=true)const {
        if(!alive())return;
        if(t.active){if(activePublish && current() && host.activeChanged)host.activeChanged(selected);}
        else if(profiles->storageReady() && profiles->profileById(t.id) && host.profileSaved)host.profileSaved(t.id);
    }
    void warning(const QString& text)const {if(alive() && host.warning)host.warning(text);}
};
FeatureTransferController::FeatureTransferController(ProfileManager& profiles,FeatureLibraryManager& library,HostCallbacks host)
    :m_state(std::make_shared<State>()),m_profiles(profiles),m_library(library),m_host(std::move(host)){}
FeatureTransferController::~FeatureTransferController(){retire();}
void FeatureTransferController::retire(){m_state->alive=false;}

bool FeatureTransferController::remove(const QString& profileArg,const QString& featureArg) {
    const QString profileId=profileArg,featureId=featureArg;Operation op(*this);if(featureId.isEmpty() || !op.current())return false;
    auto target=op.target(profileId);if(!op.current(target))return false;
    const auto* feature=target.model->featureById(featureId.toStdString());if(!feature)return false;
    const auto signature=JsonSerializer::featureToJson(*feature);
    if(op.host.stopFeature)op.host.stopFeature(profileId,featureId);
    if(!op.current(target))return false;
    feature=target.model->featureById(featureId.toStdString());if(!feature || JsonSerializer::featureToJson(*feature)!=signature)return false;
    const int index=indexOf(*target.model,featureId);if(index<0)return false;
    target.model->removeFeature(index);
    if(!target.active){if(!JsonSerializer::saveToFile(*target.model,target.path,target.directory))return false;op.profiles->invalidateCachedProject(profileId);}
    if(!op.profiles->updateTriggerArmedFeature(profileId,featureId,false))
        op.warning(QObject::tr("기능은 제거했지만 트리거 감시 설정을 저장하지 못했어요."));
    op.publish(target,{});return true;
}
bool FeatureTransferController::importLibrary(const QString& entryArg,const QString& profileArg,int index,bool publish,QString* insertedId) {
    const QString entryId=entryArg,profileId=profileArg;Operation op(*this);if(entryId.isEmpty() || !op.current())return false;
    const auto entries=op.library->listEntries();if(std::none_of(entries.begin(),entries.end(),[&](const auto& e){return e.id==entryId;}))return false;
    auto destination=op.target(profileId);if(!op.current(destination))return false;
    auto result=op.library->importEntryToProfile(entryId,destination.directory);if(!result.feature)return false;
    const auto id=QString::fromStdString(result.feature->id());if(!op.insert(destination,std::move(result.feature),index))return false;
    if(insertedId)*insertedId=id; // Output belongs to caller: publish before any destructive callback.
    op.publish(destination,id,publish);
    if(!result.missingTemplatePaths.isEmpty())op.warning(QObject::tr("가져온 기능의 템플릿 일부를 복사하지 못했습니다: %1").arg(result.missingTemplatePaths.join(QStringLiteral(", "))));
    return true;
}
bool FeatureTransferController::saveLibrary(const QString& featureArg,const QString& profileArg,const QString& nameArg,bool publish) {
    const QString featureId=featureArg,profileId=profileArg,name=nameArg;Operation op(*this);
    if(!op.current() || profileId!=op.activeId || !op.model || op.directory.isEmpty())return false;
    const auto* feature=op.model->featureById(featureId.toStdString());if(!feature)return false;
    QString entry;QStringList missing;
    if(!op.library->saveFeatureToLibrary(*feature,op.directory,name.isEmpty()?QString::fromStdString(feature->name()):name,&entry,&missing))return false;
    if(!missing.isEmpty())op.warning(QObject::tr("템플릿 일부를 라이브러리에 복사하지 못했습니다: %1").arg(missing.join(QStringLiteral(", "))));
    if(op.alive() && publish && op.host.libraryChanged)op.host.libraryChanged();return true;
}
bool FeatureTransferController::copy(const QString& featureArg,const QString& sourceArg,const QString& targetArg,int index,bool publish,QString* insertedId) {
    const QString featureId=featureArg,sourceId=sourceArg,targetId=targetArg;Operation op(*this);
    if(featureId.isEmpty() || sourceId==targetId || !op.current())return false;
    auto source=op.target(sourceId),destination=op.target(targetId);if(!op.current(source) || !op.current(destination))return false;
    const auto* feature=source.model->featureById(featureId.toStdString());if(!feature)return false;
    auto duplicate=feature->duplicateAsNewInstance();const auto id=QString::fromStdString(duplicate->id());
    const auto missing=FeatureLibraryManager::copyFeatureTemplatesBetweenDirectories(*duplicate,source.directory,destination.directory);
    if(!op.insert(destination,std::move(duplicate),index))return false;
    if(insertedId)*insertedId=id;
    op.publish(destination,id,publish);
    if(!missing.isEmpty())op.warning(QObject::tr("프로필 복사 시 템플릿 일부를 복사하지 못했습니다: %1").arg(missing.join(QStringLiteral(", "))));
    return true;
}
