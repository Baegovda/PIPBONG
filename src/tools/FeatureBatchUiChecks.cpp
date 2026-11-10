#include "app/MainWindow.h"
#include "app/FeatureLibraryManager.h"
#include "app/ForegroundWindowMonitor.h"
#include "app/ProjectWorkspaceController.h"
#include "ui/FeatureDragMime.h"
#include "ui/FeatureListPanel.h"
#include "model/Project.h"
#include "model/Feature.h"
#include "storage/JsonSerializer.h"
#include <QApplication>
#include <QElapsedTimer>
#include <QFile>
#include <QMessageBox>
#include <QMimeData>
#include <QThread>
#include <QTimer>
#include <iostream>
#include <algorithm>
#ifdef Q_OS_WIN
#include <windows.h>
#endif

class FeatureBatchUiChecks {
public:
static bool run() {
    if(QApplication::platformName()!=QStringLiteral("offscreen"))return false;
    const auto fail=[](const char* label){std::cerr<<"Feature batches MainWindow FAILED: "<<label<<'\n';return false;};
    MainWindow window;
#ifdef Q_OS_WIN
    window.m_foregroundMonitor->stop();
#endif
    const QString owner=window.m_profileManager->activeProfileId();
    const auto settle=[&]{QElapsedTimer elapsed;elapsed.start();while(window.m_profileSwitchPipelineActive && elapsed.elapsed()<4000){QApplication::processEvents();QThread::msleep(1);}return !window.m_profileSwitchPipelineActive;};
    if(!settle())return fail("initial pipeline");
    QStringList source;
    for(int i=0;i<2;++i){auto* feature=window.m_project->addFeature("batch UI "+std::to_string(i));feature->setPointerVisualFeedback(false);source<<QString::fromStdString(feature->id());}
    window.m_featureList->refresh();window.onProjectModified();
    const auto before=window.m_featureLibraryManager->listEntries();
    FeatureDragMime::Payload payload;payload.source=FeatureDragMime::Source::Profile;payload.profileId=owner;payload.ids=source;
    std::unique_ptr<QMimeData> mime(FeatureDragMime::createMimeData(payload));
    window.onFeatureDroppedOnLibrary(mime.get());
    const auto entries=window.m_featureLibraryManager->listEntries();
    QStringList created;
    for(const auto& entry:entries) {
        if(std::none_of(before.begin(),before.end(),[&](const auto& old){return old.id==entry.id;}))created<<entry.id;
    }
    if(created.size()!=2)return fail("profile drop saves two entries");
    const int oldCount=static_cast<int>(window.m_project->features().size());
    payload={};payload.source=FeatureDragMime::Source::Library;payload.ids={created[0],created[1],created[0]};
    mime.reset(FeatureDragMime::createMimeData(payload));
    window.onFeatureDroppedOnFeatureList(mime.get(),1);
    const QString firstInserted=QString::fromStdString(window.m_project->featureAt(1)->id());
    const QString secondInserted=QString::fromStdString(window.m_project->featureAt(2)->id());
    if(window.m_project->features().size()!=oldCount+2 || source.contains(firstInserted)
        || window.m_featureList->selectedFeatureId()!=secondInserted)return fail("library drop deduplicates, inserts in order and selects last");
    const QString target=window.m_profileManager->addProfile("batch UI destination",false);
    Project empty;
    if(target.isEmpty() || !JsonSerializer::saveToFile(empty,window.m_profileManager->projectPath(target),window.m_profileManager->projectDirectory(target)))return fail("destination fixture");
    window.onFeatureDroppedOnProfile(target,mime.get());
    auto loaded=JsonSerializer::loadFromFile(window.m_profileManager->projectPath(target));
    if(!loaded || loaded->features().size()!=2 || window.m_profileManager->activeProfileId()!=owner)return fail("inactive profile drop remains owner scoped");
    // A pinned workspace must reject a transfer before taking a snapshot or changing the model.
    window.m_profileSwitchPipelineActive=true;
    const auto blockedCount=window.m_project->features().size();
    window.onFeatureDroppedOnFeatureList(mime.get(),0);
    window.m_profileSwitchPipelineActive=false;
    if(window.m_project->features().size()!=blockedCount)return fail("pipeline guard");
    window.m_libraryPreviewEntryId=created[0];window.m_libraryPreviewFeature=window.m_featureLibraryManager->loadEntryFeature(created[0]);
    bool replied=false;
    QTimer::singleShot(0,&window,[&]{
        for(auto* widget:QApplication::topLevelWidgets())if(auto* box=qobject_cast<QMessageBox*>(widget)) {
            if(box->windowTitle()==QStringLiteral("라이브러리에서 삭제")){replied=true;box->done(QMessageBox::Yes);}
        }
    });
    window.onDeleteLibraryEntriesRequested({created[0]});
    if(!replied || window.m_libraryPreviewEntryId==created[0] || window.m_libraryPreviewFeature)return fail("confirmed deletion clears owned preview");
    if(!window.m_featureLibraryManager->removeEntry(created[1]) || !window.m_profileManager->removeProfile(target))return fail("fixture cleanup");
    if(!window.prepareForShutdown())return fail("shutdown checkpoint");
    std::cout<<"Feature batches MainWindow: real MIME save/import/inactive-profile drops, deduplicated ordering/selection, pipeline rejection and confirmed preview deletion OK (offscreen; no native input)\n";
    return true;
}
};
bool runFeatureBatchUiChecks(){return FeatureBatchUiChecks::run();}
