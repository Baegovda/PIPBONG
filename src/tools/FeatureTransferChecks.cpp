#include "app/FeatureTransferController.h"
#include "app/ProfileManager.h"
#include "app/FeatureLibraryManager.h"
#include "model/Project.h"
#include "model/Feature.h"
#include "storage/JsonSerializer.h"
#include "core/workflow/blocks/ImageFindBlock.h"
#include <QDir>
#include <QFile>
#include <QTemporaryDir>
#include <iostream>
#ifdef Q_OS_WIN
#include <windows.h>
#endif

namespace {
bool expect(bool result, const char* label) {
    if (!result) std::cerr << "Feature transfer check failed: " << label << '\n';
    return result;
}
bool write(const QString& path, const QByteArray& data) {
    QDir().mkpath(QFileInfo(path).absolutePath());
    QFile file(path); return file.open(QIODevice::WriteOnly) && file.write(data) == data.size();
}
QByteArray read(const QString& path) {
    QFile file(path); return file.open(QIODevice::ReadOnly) ? file.readAll() : QByteArray();
}
}

bool runFeatureTransferLifetimeChecks();
bool runFeatureTransferChecks() {
    if(!runFeatureTransferLifetimeChecks())return false;
    QTemporaryDir data;
    ProfileManager profiles(data.path());
    if (!profiles.initialize()) return false;
    const QString activeId = profiles.activeProfileId();
    const QString otherId = profiles.addProfile(QStringLiteral("대상"));
    if (!profiles.setActiveProfile(activeId)) return false;
    Project empty;
    if (!JsonSerializer::saveToFile(empty, profiles.projectPath(otherId), profiles.projectDirectory(otherId))) return false;
    auto lease=std::make_shared<Project>();Project& active=*lease;
    auto* group = active.addFeatureGroup("source group");
    auto* source = active.addFeature("copy source");
    source->setRepeatCount(7);
    source->setGroupId(group->id());
    const QString sourceId = QString::fromStdString(source->id());
    auto image = std::make_unique<ImageFindBlock>();
    image->templatePaths = {"templates/same.png"};
    image->templateLabels["templates/same.png"] = "captured target";
    source->workflow().addBlock(std::move(image));
    const QString sourceAsset = QDir(profiles.projectDirectory(activeId)).filePath(QStringLiteral("templates/same.png"));
    const QString targetAsset = QDir(profiles.projectDirectory(otherId)).filePath(QStringLiteral("templates/same.png"));
    if (!write(sourceAsset, "source image") || !write(sourceAsset + ".capture.json", "source metadata")
        || !write(targetAsset, "existing image") || !write(targetAsset + ".capture.json", "existing metadata")) return false;
    FeatureLibraryManager library(data.path());
    int changed = 0, saved = 0, stopped = 0, warnings = 0;
    FeatureTransferController::HostCallbacks host;
    host.activeProject = [&] { return lease; };
    host.activeChanged = [&](const QString&) { ++changed; };
    host.profileSaved = [&](const QString&) { ++saved; };
    host.stopFeature = [&](const QString&,const QString&) { ++stopped; };
    host.libraryChanged = [] {};
    host.warning = [&](const QString&) { ++warnings; };
    FeatureTransferController transfers(profiles, library, host);
    if (!expect(transfers.copy(sourceId, activeId, otherId, -1), "copy to inactive profile")) return false;
    auto target = JsonSerializer::loadFromFile(profiles.projectPath(otherId));
    if (!expect(target && target->features().size() == 1 && target->featureAt(0)->id() != sourceId.toStdString()
                && target->featureAt(0)->groupId().empty() && target->featureAt(0)->repeatCount() == 7,
                "new identity, preserved options and valid target grouping")) return false;
    auto* copied = dynamic_cast<ImageFindBlock*>(target->featureAt(0)->workflow().blocks().front().get());
    if (!copied) return false;
    const std::string remapped = copied->templatePaths.front();
    if (!expect(remapped != "templates/same.png" && read(targetAsset) == "existing image"
                && read(targetAsset + ".capture.json") == "existing metadata"
                && read(QDir(profiles.projectDirectory(otherId)).filePath(QString::fromStdString(remapped))) == "source image"
                && read(QDir(profiles.projectDirectory(otherId)).filePath(QString::fromStdString(remapped)) + ".capture.json") == "source metadata"
                && copied->templateLabels.at(remapped) == "captured target", "collision preserves old image/metadata and remaps label")) return false;
    profiles.storeCachedProject(otherId, profiles.projectPath(otherId), target->clone(), profiles.projectDirectory(otherId));
    if (!expect(transfers.saveLibrary(sourceId, activeId, QStringLiteral("saved name")), "save library")) return false;
    const auto entries = library.listEntries();
    if (entries.size() != 1) return false;
    QString inserted;
    if (!expect(transfers.importLibrary(entries.front().id, otherId, 0, true, &inserted)
                && !inserted.isEmpty() && !profiles.cloneCachedProject(otherId, profiles.projectPath(otherId)),
                "library import invalidates inactive cache")) return false;
    target = JsonSerializer::loadFromFile(profiles.projectPath(otherId));
    if (!expect(target && target->features().size() == 2 && target->featureAt(0)->name() == "saved name"
                && read(targetAsset) == "existing image", "library import also preserves existing asset")) return false;
    if (!expect(transfers.copy(inserted, otherId, activeId, 0) && changed == 1, "copy back to active model publishes once")) return false;
    if (!expect(transfers.importLibrary(entries.front().id, activeId, -1, false) && changed == 1, "batch import defers UI publication")) return false;
#ifdef Q_OS_WIN
    const QString targetPath = profiles.projectPath(otherId);
    const QByteArray original = read(targetPath);
    HANDLE lock = CreateFileW(reinterpret_cast<LPCWSTR>(targetPath.utf16()), GENERIC_READ,
        FILE_SHARE_READ, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (lock == INVALID_HANDLE_VALUE) return false;
    QString output = QStringLiteral("unchanged");
    const int beforeSaved = saved;
    const bool refused = !transfers.importLibrary(entries.front().id, otherId, -1, true, &output);
    CloseHandle(lock);
    if (!expect(refused && output == "unchanged" && saved == beforeSaved && read(targetPath) == original,
                "failed inactive save preserves file and output state")) return false;
#endif
    if (!expect(transfers.remove(activeId, sourceId) && stopped == 1 && !active.featureById(sourceId.toStdString()), "remove active feature")) return false;
    if (!expect(transfers.remove(otherId, inserted) && saved == 3, "remove inactive feature commits")) return false;
    if (!expect(!transfers.copy(sourceId, activeId, QStringLiteral("../outside"), -1)
                && !transfers.importLibrary(QStringLiteral("../outside"), activeId, -1) && warnings == 0,
                "unknown profile/library identifiers rejected")) return false;
    const QString entryFile = QDir(library.entryProjectDirectory(entries.front().id)).filePath(QStringLiteral("feature.json"));
    const QByteArray validEntry = read(entryFile);
    if (!write(entryFile, "{\"name\":false}")) return false;
    if (!expect(!library.loadEntryFeature(entries.front().id)
                && !library.importEntryToProfile(entries.front().id, profiles.projectDirectory(activeId)).feature,
                "malformed typed library data rejected without throwing")) return false;
    if (!write(entryFile, validEntry)) return false;
    Feature escaped("unsafe asset");
    auto escapedImage = std::make_unique<ImageFindBlock>();
    escapedImage->templatePaths = {"../escape.png"};
    escaped.workflow().addBlock(std::move(escapedImage));
    const QString outside = QDir(profiles.profilesDirectory()).filePath(QStringLiteral("escape.png"));
    if (!write(outside, "preserved")) return false;
    const auto rejected = FeatureLibraryManager::copyFeatureTemplatesBetweenDirectories(escaped,
        profiles.projectDirectory(activeId), profiles.projectDirectory(otherId));
    if (!expect(!rejected.isEmpty() && read(outside) == "preserved", "asset copy cannot escape profile roots")) return false;
    std::cout << "Feature transfers: active/inactive copy+import+remove, cache invalidation, collision-safe assets, failed-save retention OK\n";
    return true;
}
