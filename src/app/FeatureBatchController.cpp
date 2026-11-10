#include "app/FeatureBatchController.h"
#include "app/FeatureTransferController.h"
#include "app/FeatureLibraryManager.h"
#include "app/ProfileManager.h"
#include "model/Project.h"
#include "model/Feature.h"
#include <QPointer>
#include <QSet>
#include <algorithm>

namespace {
struct BusyGuard {
    std::shared_ptr<bool> busy;
    explicit BusyGuard(std::shared_ptr<bool> state):busy(std::move(state)){*busy=true;}
    ~BusyGuard(){*busy=false;}
};
QStringList order(const FeatureLibraryManager& library) {
    QStringList ids;
    for(const auto& entry:library.listEntries())ids.push_back(entry.id);
    return ids;
}
}

FeatureBatchController::FeatureBatchController(ProfileManager& profiles,FeatureLibraryManager& library,
    FeatureTransferController& transfers,Host host,QObject* parent)
    :QObject(parent),m_profiles(profiles),m_library(library),m_transfers(transfers),m_host(std::move(host)){}

FeatureBatchController::Result FeatureBatchController::run(const Request& request) {
    const Host host=m_host;
    Result result;result.kind=request.kind;
    QStringList ids;
    for(const auto& id:request.ids)if(!id.isEmpty() && !ids.contains(id))ids.push_back(id);
    result.requested=ids.size();
    if(ids.isEmpty())return result;
    QPointer<FeatureBatchController> life(this);
    if(*m_busy || !m_profiles.storageReady() || !host.canBegin || !host.canBegin() || !life){result.cancelled=true;return result;}
    BusyGuard guard(m_busy);
    const QString active=m_profiles.activeProfileId();
    const quint64 generation=host.generation();
    Project* const project=host.activeProject();
    const auto valid=[this,life,host,active,generation,project]{
        return life && host.canBegin() && life && m_profiles.activeProfileId()==active
            && host.generation()==generation && host.activeProject()==project;
    };
    if((request.kind==Kind::Import || request.kind==Kind::Copy) && !m_profiles.profileById(request.targetProfile)) {
        result.failed=ids;return result;
    }
    if(request.kind==Kind::Save && request.sourceProfile!=active){result.failed=ids;return result;}
    if(request.kind==Kind::Copy && (request.sourceProfile==request.targetProfile || !m_profiles.profileById(request.sourceProfile))) {
        result.failed=ids;return result;
    }
    if(!host.checkpoint(request.undoReason) || !valid() || (request.validate && !request.validate()) || !life){result.cancelled=true;return result;}
    const auto entries=m_library.listEntries();
    int index=request.insertIndex;
    for(const auto& id:ids) {
        if(!valid()){result.cancelled=true;break;}
        QString name=id, inserted;
        const auto entry=std::find_if(entries.begin(),entries.end(),[&](const auto& item){return item.id==id;});
        bool success=false;
        switch(request.kind) {
        case Kind::Import:
            if(entry!=entries.end()) {
                name=entry->name;
                success=m_transfers.importLibrary(id,request.targetProfile,index,false,&inserted);
            }
            break;
        case Kind::Save:
            if(const auto* feature=project ? project->featureById(id.toStdString()) : nullptr)name=QString::fromStdString(feature->name());
            if(ids.size()==1 && !request.name.trimmed().isEmpty())name=request.name;
            success=m_transfers.saveLibrary(id,request.sourceProfile,request.name,false);
            break;
        case Kind::Copy:
            if(request.sourceProfile==active) {
                if(const auto* feature=project ? project->featureById(id.toStdString()) : nullptr)name=QString::fromStdString(feature->name());
            }
            success=m_transfers.copy(id,request.sourceProfile,request.targetProfile,index,false,&inserted);
            break;
        case Kind::Remove:
            if(entry!=entries.end()){name=entry->name;success=m_library.removeEntry(id);}
            break;
        }
        // A callback may have destroyed the controller. Only stack-owned state is safe now.
        if(success) {
            result.completed.push_back(id);result.names.push_back(name);
            if(!inserted.isEmpty())result.lastFeatureId=inserted;
            if(index>=0)++index;
        } else result.failed.push_back(id);
        if(!life){result.cancelled=true;return result;}
    }
    if(result.completed.isEmpty())return result;
    if((request.kind==Kind::Import || request.kind==Kind::Copy) && request.targetProfile==active && valid())
        host.activeChanged(result.lastFeatureId);
    if(!life)return result;
    if(request.kind==Kind::Save || request.kind==Kind::Remove)host.libraryChanged();
    return result;
}

FeatureBatchController::OrderResult FeatureBatchController::saveOrder(const QStringList& before,
    const QStringList& after,const QString& reason,const std::function<bool()>& validate) {
    const Host host=m_host;
    using R=OrderResult;
    QPointer<FeatureBatchController> life(this);
    if(*m_busy || !m_profiles.storageReady() || !host.canBegin || !host.canBegin() || !life)return R::Cancelled;
    BusyGuard guard(m_busy);
    if(before==after){host.libraryChanged();return R::Unchanged;}
    const auto generation=host.generation();
    const bool checkpoint=host.checkpoint(reason);
    if(!life)return R::Cancelled;
    if(!checkpoint){host.libraryChanged();return R::Cancelled;}
    if(validate && !validate()){if(life)host.libraryChanged();return R::Cancelled;}
    if(!life)return R::Cancelled;
    const bool ready=host.canBegin();
    if(!life)return R::Cancelled;
    const bool stale=!ready || generation!=host.generation() || before!=order(m_library);
    const bool saved=!stale && m_library.reorderEntries(after);
    // The list already moved optimistically: repaint the persisted order on every outcome.
    host.libraryChanged();
    return stale ? R::Cancelled : saved ? R::Changed : R::Failed;
}

FeatureBatchController::OrderResult FeatureBatchController::reorderOne(int from,int to,const std::function<bool()>& validate) {
    const auto before=order(m_library);
    if(from<0 || to<0 || from>=before.size() || to>=before.size()) {
        const auto publish=m_host.libraryChanged;publish();return OrderResult::Unchanged;
    }
    auto after=before;const auto moved=after.takeAt(from);after.insert(to,moved);
    return saveOrder(before,after,QStringLiteral("library-reorder"),validate);
}

FeatureBatchController::OrderResult FeatureBatchController::reorder(QList<int> rows,int insertIndex,const std::function<bool()>& validate) {
    const auto before=order(m_library);
    if(rows.isEmpty() || insertIndex<0 || insertIndex>before.size()) {
        const auto publish=m_host.libraryChanged;publish();return OrderResult::Unchanged;
    }
    std::sort(rows.begin(),rows.end());rows.erase(std::unique(rows.begin(),rows.end()),rows.end());
    if(rows.first()<0 || rows.last()>=before.size()){const auto publish=m_host.libraryChanged;publish();return OrderResult::Unchanged;}
    auto after=before;QStringList moved;
    int destination=insertIndex;
    for(int row:rows){moved.push_back(before[row]);if(row<insertIndex)--destination;}
    for(auto it=rows.crbegin();it!=rows.crend();++it)after.removeAt(*it);
    for(int i=0;i<moved.size();++i)after.insert(destination+i,moved[i]);
    return saveOrder(before,after,QStringLiteral("library-reorder-multi"),validate);
}
