#include "app/FeatureBatchController.h"
#include "app/FeatureTransferController.h"
#include "app/FeatureLibraryManager.h"
#include "app/ProfileManager.h"
#include "model/Project.h"
#include "model/Feature.h"
#include "storage/JsonSerializer.h"
#include <QDir>
#include <QFile>
#include <QTemporaryDir>
#include <iostream>
#ifdef Q_OS_WIN
#include <windows.h>
#endif

namespace {
bool expect(bool ok,const char* label){if(!ok)std::cerr<<"Feature batches FAILED: "<<label<<'\n';return ok;}
QStringList ids(const FeatureLibraryManager& library){QStringList out;for(const auto& entry:library.listEntries())out<<entry.id;return out;}
}
bool runFeatureBatchChecks() {
    using C=FeatureBatchController;
    QTemporaryDir root;
    ProfileManager profiles(root.path());if(!profiles.initialize())return false;
    const auto owner=profiles.activeProfileId();
    const auto other=profiles.addProfile("batch target",false);
    auto lease=std::make_shared<Project>();Project& active=*lease;Project empty;
    if(other.isEmpty() || !JsonSerializer::saveToFile(empty,profiles.projectPath(other),profiles.projectDirectory(other)))return false;
    QStringList source;
    for(int i=0;i<3;++i)source<<QString::fromStdString(active.addFeature("source "+std::to_string(i))->id());
    FeatureLibraryManager library(root.path());
    int activeChanged=0,libraryChanged=0,saved=0,checkpoints=0;
    QString selected;
    FeatureTransferController::HostCallbacks transfersHost;
    transfersHost.activeProject=[&]{return lease;};
    transfersHost.activeChanged=[&](const QString& id){++activeChanged;selected=id;};
    transfersHost.libraryChanged=[&]{++libraryChanged;};
    transfersHost.profileSaved=[&](const QString&){++saved;};
    transfersHost.warning=[](const QString&){};transfersHost.stopFeature=[](const QString&,const QString&){};
    FeatureTransferController transfers(profiles,library,transfersHost);
    quint64 generation=1;bool ready=true;
    std::function<bool()> checkpoint=[] {return true;};
    C::Host host;
    host.canBegin=[&]{return ready;};host.generation=[&]{return generation;};host.activeProject=[&]{return &active;};
    host.checkpoint=[&](const QString&){++checkpoints;return checkpoint();};
    host.activeChanged=transfersHost.activeChanged;host.libraryChanged=transfersHost.libraryChanged;
    C batches(profiles,library,transfers,host);
    C::Request request;request.kind=C::Kind::Save;request.sourceProfile=owner;
    request.ids=source;request.ids<<source[0]<<QString();request.undoReason="batch-save";
    auto result=batches.run(request);
    if(!expect(result.requested==3 && result.completed.size()==3 && result.failed.isEmpty()
        && checkpoints==1 && libraryChanged==1 && library.listEntries().size()==3,"deduplicated save; one checkpoint and publication"))return false;
    const auto entries=ids(library);
    request={};request.targetProfile=owner;request.ids={"missing-entry",entries[1],entries[1]};request.insertIndex=1;
    result=batches.run(request);
    if(!expect(result.requested==2 && result.failed==QStringList{"missing-entry"} && result.completed==QStringList{entries[1]}
        && result.names.first()==library.listEntries()[1].name && active.features().size()==4
        && activeChanged==1 && selected==result.lastFeatureId && active.featureAt(1)->id()==selected.toStdString(),
        "partial import reports actual successful name and selects last inserted once"))return false;
    // The checkpoint pumps events in production. Reject replacement even if profile ID stays the same.
    const auto count=active.features().size();checkpoint=[&]{++generation;return true;};
    result=batches.run(request);
    if(!expect(result.cancelled && result.completed.isEmpty() && active.features().size()==count,"checkpoint generation change cancels before mutation"))return false;
    checkpoint=[&]{return profiles.setActiveProfile(other);};
    result=batches.run(request);
    if(!expect(result.cancelled && result.completed.isEmpty() && active.features().size()==count,"checkpoint profile change cannot redirect import"))return false;
    if(!profiles.setActiveProfile(owner))return false;
    checkpoint=[] {return false;};result=batches.run(request);
    if(!expect(result.cancelled && result.completed.isEmpty(),"failed snapshot prevents mutations"))return false;
    // Nested UI requests cannot mutate the same batch while its snapshot is being prepared.
    bool rejected=false;
    checkpoint=[&]{const auto nested=batches.run(request);rejected=nested.cancelled && nested.completed.isEmpty();return true;};
    result=batches.run(request);
    if(!expect(rejected && result.completed.size()==1,"checkpoint reentry rejected without losing outer operation"))return false;
    checkpoint=[] {return true;};
    request={};request.targetProfile=other;request.ids={entries[0],entries[2]};
    result=batches.run(request);
    auto target=JsonSerializer::loadFromFile(profiles.projectPath(other));
    if(!expect(result.completed.size()==2 && target && target->features().size()==2 && saved==2,"inactive target persists both imported features"))return false;
    C::Request copy;copy.kind=C::Kind::Copy;copy.ids={source[0]};copy.sourceProfile=owner;copy.targetProfile=other;
    if(!expect(batches.run(copy).completed.size()==1,"copy batch persists to inactive target"))return false;
    copy.ids={QString::fromStdString(target->featureAt(0)->id())};copy.sourceProfile=other;copy.targetProfile=owner;
    const auto copied=batches.run(copy);
    if(!expect(copied.completed.size()==1 && !copied.lastFeatureId.isEmpty() && selected==copied.lastFeatureId,
        "copy back to active target selects new identity"))return false;
    const int savedBeforeLock=saved;
#ifdef Q_OS_WIN
    const QString targetPath=profiles.projectPath(other);
    QFile targetFile(targetPath);if(!targetFile.open(QIODevice::ReadOnly))return false;
    const auto original=targetFile.readAll();targetFile.close();
    HANDLE lock=CreateFileW(reinterpret_cast<LPCWSTR>(targetPath.utf16()),GENERIC_READ,FILE_SHARE_READ,nullptr,OPEN_EXISTING,0,nullptr);
    if(lock==INVALID_HANDLE_VALUE)return false;
    result=batches.run(request);CloseHandle(lock);
    if(!targetFile.open(QIODevice::ReadOnly))return false;
    const bool retained=targetFile.readAll()==original;targetFile.close();
    if(!expect(result.completed.isEmpty() && result.failed.size()==2 && saved==savedBeforeLock && retained,"locked inactive destination: explicit failures, old file retained"))return false;
#endif
    const int beforeCheckpoints=checkpoints;
    // Deliberately unsorted, duplicated rows: original order is preserved within the moved block.
    const auto reordered=batches.reorder({2,0,2},3);
    if(!expect(reordered==C::OrderResult::Changed && ids(library)==QStringList{entries[1],entries[0],entries[2]}
        && checkpoints==beforeCheckpoints+1,"multi reorder normalizes duplicate/unsorted selection"))return false;
    const int repaint=libraryChanged;
    if(!expect(batches.reorder({-1,2},0)==C::OrderResult::Unchanged && libraryChanged==repaint+1,"invalid rows repaint persisted order without mutation"))return false;
    checkpoint=[&]{library.reorderEntries(entries);return true;};
    if(!expect(batches.reorderOne(0,2)==C::OrderResult::Cancelled && ids(library)==entries,"stale order after checkpoint cannot overwrite newer order"))return false;
    checkpoint=[] {return false;};const int refreshBefore=libraryChanged;
    if(!expect(batches.reorderOne(0,2)==C::OrderResult::Cancelled && libraryChanged==refreshBefore+1,"snapshot failure restores optimistic UI order"))return false;
    checkpoint=[] {return true;};
#ifdef Q_OS_WIN
    const QString manifest=QDir(root.path()).filePath("featureLibrary/manifest.json");
    // Confirm the exact manifest location exists before applying an OS file lock.
    if(!QFile::exists(manifest))return false;
    lock=CreateFileW(reinterpret_cast<LPCWSTR>(manifest.utf16()),GENERIC_READ,FILE_SHARE_READ,nullptr,OPEN_EXISTING,0,nullptr);
    if(lock==INVALID_HANDLE_VALUE)return false;
    const auto failedOrder=batches.reorderOne(0,2);CloseHandle(lock);
    if(!expect(failedOrder==C::OrderResult::Failed && ids(library)==entries,"locked library manifest retains persisted order"))return false;
#endif
    // Removing an unknown path cannot escape the library; duplicates count only once.
    request={};request.kind=C::Kind::Remove;request.ids={entries[0],entries[0],"../outside"};
    result=batches.run(request);
    if(!expect(result.completed==QStringList{entries[0]} && result.failed==QStringList{"../outside"} && ids(library).size()==2,"remove membership and honest partial result"))return false;
    // A controller deleted by an asynchronous checkpoint leaves no dangling busy-state guard.
    auto* disposable=new C(profiles,library,transfers,host);
    checkpoint=[&]{delete disposable;disposable=nullptr;return true;};
    request={};request.targetProfile=owner;request.ids={entries[1]};
    result=disposable->run(request);
    if(!expect(!disposable && result.cancelled && result.completed.isEmpty(),"checkpoint deletion safely cancels"))return false;
    std::cout<<"Feature batches: deduplication, single publication, partial names/counts, frozen owner/generation, reentry/deletion, inactive persistence/lock failures and stable/stale/locked reorder OK (no native input)\n";
    return true;
}
