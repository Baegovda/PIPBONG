#include "app/FeatureLibraryManager.h"

#include "core/workflow/blocks/ImageFindBlock.h"
#include "model/Feature.h"
#include "storage/JsonSerializer.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QHash>
#include <QSet>
#include <QUuid>
#include <QSaveFile>

#include <set>
#include <algorithm>

namespace {

bool isNonEmptyRelativeTemplatePath(const QString& path) {
    if (path.isEmpty()) {
        return false;
    }
    // Template paths are stored relative to the profile project directory (e.g. "templates/xxx.png").
    // If a path is absolute, importing between profiles would break, so we skip copying.
    return !QFileInfo(path).isAbsolute();
}

void copyFileOverwrite(const QString& fromPath, const QString& toPath, bool& outOk) {
    if (outOk == false) {
        return;
    }
    if (!QFileInfo(fromPath).exists()) {
        outOk = false;
        return;
    }
    QDir().mkpath(QFileInfo(toPath).absolutePath());
    if (QFileInfo(toPath).exists()) {
        QFile::remove(toPath);
    }
    outOk = QFile::copy(fromPath, toPath);
}

void copyTemplateWithMetadata(const QString& srcTemplatePath, const QString& dstTemplatePath, bool& outOk) {
    copyFileOverwrite(srcTemplatePath, dstTemplatePath, outOk);
    if (!outOk) {
        return;
    }
    const QString metaSrc = srcTemplatePath + QStringLiteral(".capture.json");
    if (!QFileInfo(metaSrc).exists()) {
        return;
    }
    copyFileOverwrite(metaSrc, dstTemplatePath + QStringLiteral(".capture.json"), outOk);
}

bool sameFileBytes(const QString& first, const QString& second) {
    QFile a(first), b(second);
    if (!a.exists() || !b.exists()) return a.exists() == b.exists();
    if (a.size() != b.size() || !a.open(QIODevice::ReadOnly) || !b.open(QIODevice::ReadOnly)) return false;
    while (!a.atEnd()) {
        if (a.read(1024 * 1024) != b.read(1024 * 1024)) return false;
    }
    return a.error() == QFileDevice::NoError && b.error() == QFileDevice::NoError;
}

bool copyInsideRoot(const QString& path, const QString& root) {
    const QString base = QDir::cleanPath(QFileInfo(root).absoluteFilePath());
    const QString absolute = QDir::cleanPath(QFileInfo(path).absoluteFilePath());
    if (!absolute.startsWith(base + QLatin1Char('/'), Qt::CaseInsensitive)) return false;
    QFileInfo current(absolute);
    while (current.absoluteFilePath().compare(base, Qt::CaseInsensitive) != 0) {
        if (current.isSymLink() || current.isJunction()) return false;
        const QString parent = current.absolutePath();
        if (parent == current.absoluteFilePath()) return false;
        current.setFile(parent);
    }
    return !current.isSymLink() && !current.isJunction();
}

int countTemplatesRecursive(const nlohmann::json& j) {
    if (j.is_object()) {
        int count = 0;
        const auto typeIt = j.find("type");
        if (typeIt != j.end() && typeIt->is_string() && typeIt->get<std::string>() == "ImageFind") {
            const auto templatesIt = j.find("templates");
            if (templatesIt != j.end() && templatesIt->is_array()) {
                for (const auto& item : *templatesIt) {
                    if (item.is_string() && !item.get<std::string>().empty()) {
                        ++count;
                    }
                }
            }
        }
        for (const auto& kv : j.items()) {
            count += countTemplatesRecursive(kv.value());
        }
        return count;
    }
    if (j.is_array()) {
        int count = 0;
        for (const auto& item : j) {
            count += countTemplatesRecursive(item);
        }
        return count;
    }
    return 0;
}

std::vector<QString> templatePathsRecursive(const nlohmann::json& featureJson) {
    std::set<QString> out;
    if (!featureJson.is_object()) {
        return {};
    }
    // Fast path: only ImageFind blocks have "templates" arrays.
    // We'll do a simple traversal over objects/arrays and extract string arrays for ImageFind.
    std::vector<const nlohmann::json*> stack;
    stack.push_back(&featureJson);
    while (!stack.empty()) {
        const nlohmann::json* cur = stack.back();
        stack.pop_back();
        if (!cur) {
            continue;
        }
        if (cur->is_object()) {
            const auto typeIt = cur->find("type");
            if (typeIt != cur->end() && typeIt->is_string() && typeIt->get<std::string>() == "ImageFind") {
                const auto templatesIt = cur->find("templates");
                if (templatesIt != cur->end() && templatesIt->is_array()) {
                    for (const auto& item : *templatesIt) {
                        if (item.is_string()) {
                            const QString p = QString::fromStdString(item.get<std::string>());
                            if (isNonEmptyRelativeTemplatePath(p)) {
                                out.insert(p);
                            }
                        }
                    }
                }
            }
            for (const auto& kv : cur->items()) {
                stack.push_back(&kv.value());
            }
        } else if (cur->is_array()) {
            for (const auto& item : *cur) {
                stack.push_back(&item);
            }
        }
    }
    return std::vector<QString>(out.begin(), out.end());
}

} // namespace

FeatureLibraryManager::FeatureLibraryManager(const QString& dataDirectory)
    : m_libraryRootDir(QDir(dataDirectory).filePath(QStringLiteral("featureLibrary")))
    , m_entriesDir(QDir(m_libraryRootDir).filePath(QStringLiteral("entries"))) {}

QString FeatureLibraryManager::featureJsonPath(const QString& entryId) const {
    return QDir(entryDir(entryId)).filePath(QStringLiteral("feature.json"));
}

QString FeatureLibraryManager::entryDir(const QString& entryId) const {
    return QDir(m_entriesDir).filePath(entryId);
}

QString FeatureLibraryManager::manifestPath() const {
    return QDir(m_libraryRootDir).filePath(QStringLiteral("manifest.json"));
}

QStringList FeatureLibraryManager::loadEntryOrder() const {
    QFile file(manifestPath());
    if (!file.open(QIODevice::ReadOnly)) {
        return {};
    }
    try {
        const nlohmann::json j = nlohmann::json::parse(file.readAll().constData());
        QStringList out;
        const auto idsIt = j.find("entryIds");
        if (idsIt == j.end() || !idsIt->is_array()) {
            return {};
        }
        for (const auto& item : *idsIt) {
            if (item.is_string()) {
                out.push_back(QString::fromStdString(item.get<std::string>()));
            }
        }
        return out;
    } catch (...) {
        return {};
    }
}

bool FeatureLibraryManager::saveEntryOrder(const QStringList& orderedIds) const {
    QDir().mkpath(m_libraryRootDir);
    nlohmann::json j;
    j["version"] = 1;
    nlohmann::json ids = nlohmann::json::array();
    for (const QString& id : orderedIds) {
        if (!id.isEmpty()) {
            ids.push_back(id.toStdString());
        }
    }
    j["entryIds"] = std::move(ids);

    QSaveFile file(manifestPath());
    if (!file.open(QIODevice::WriteOnly)) {
        return false;
    }
    const QByteArray serialized = QString::fromStdString(j.dump(2)).toUtf8();
    return file.write(serialized) == serialized.size() && file.commit();
}

void FeatureLibraryManager::appendEntryToOrder(const QString& entryId) const {
    if (entryId.isEmpty()) {
        return;
    }
    QStringList order = loadEntryOrder();
    if (!order.contains(entryId)) {
        order.push_back(entryId);
        saveEntryOrder(order);
    }
}

void FeatureLibraryManager::removeEntryFromOrder(const QString& entryId) const {
    if (entryId.isEmpty()) {
        return;
    }
    QStringList order = loadEntryOrder();
    if (order.removeAll(entryId) > 0) {
        saveEntryOrder(order);
    }
}

bool FeatureLibraryManager::reorderEntries(const QStringList& orderedIds) {
    m_revision.fetch_add(1,std::memory_order_relaxed);
    if (orderedIds.isEmpty()) {
        return false;
    }
    return saveEntryOrder(orderedIds);
}

std::vector<FeatureLibraryManager::Entry> FeatureLibraryManager::listEntries() const {
    QHash<QString, Entry> byId;
    const QDir entriesDir(m_entriesDir);
    if (entriesDir.exists()) {
        const QFileInfoList dirs = entriesDir.entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot);
        for (const QFileInfo& dirInfo : dirs) {
            const QString entryId = dirInfo.fileName();
            const QString featurePath = featureJsonPath(entryId);
            QFile file(featurePath);
            if (!file.open(QIODevice::ReadOnly)) {
                continue;
            }
            const QByteArray raw = file.readAll();
            file.close();

            try {
                const nlohmann::json j = nlohmann::json::parse(raw.constData());
                Entry e;
                e.id = entryId;
                e.name = QString::fromStdString(j.value("name", std::string{}));
                e.templateCount = countTemplatesRecursive(j);
                byId.insert(entryId, std::move(e));
            } catch (...) {
                continue;
            }
        }
    }

    if (byId.isEmpty()) {
        return {};
    }

    QStringList order = loadEntryOrder();
    std::vector<Entry> out;
    out.reserve(byId.size());
    QSet<QString> used;
    for (const QString& id : order) {
        const auto it = byId.constFind(id);
        if (it != byId.constEnd()) {
            out.push_back(it.value());
            used.insert(id);
        }
    }

    std::vector<Entry> orphans;
    orphans.reserve(byId.size() - used.size());
    for (auto it = byId.constBegin(); it != byId.constEnd(); ++it) {
        if (!used.contains(it.key())) {
            orphans.push_back(it.value());
        }
    }
    std::sort(orphans.begin(), orphans.end(), [](const Entry& a, const Entry& b) {
        return a.name < b.name;
    });
    out.insert(out.end(), orphans.begin(), orphans.end());

    if (order.isEmpty() && !out.empty()) {
        QStringList initialOrder;
        initialOrder.reserve(static_cast<int>(out.size()));
        for (const Entry& e : out) {
            initialOrder.push_back(e.id);
        }
        saveEntryOrder(initialOrder);
    } else if (!orphans.empty()) {
        QStringList mergedOrder;
        mergedOrder.reserve(static_cast<int>(out.size()));
        for (const Entry& e : out) {
            mergedOrder.push_back(e.id);
        }
        saveEntryOrder(mergedOrder);
    }

    return out;
}

std::vector<QString> FeatureLibraryManager::templatePathsFromFeature(const Feature& feature) {
    std::set<QString> out;
    for (const auto& block : feature.workflow().blocks()) {
        if (!block) {
            continue;
        }
        if (block->type() != BlockType::ImageFind) {
            continue;
        }
        const auto* imageFind = dynamic_cast<const ImageFindBlock*>(block.get());
        if (!imageFind) {
            continue;
        }
        for (const std::string& p : imageFind->templatePaths) {
            const QString q = QString::fromStdString(p);
            if (isNonEmptyRelativeTemplatePath(q)) {
                out.insert(q);
            }
        }
    }
    return std::vector<QString>(out.begin(), out.end());
}

int FeatureLibraryManager::countTemplatesInFeatureJson(const nlohmann::json& featureJson) {
    return countTemplatesRecursive(featureJson);
}

std::vector<QString> FeatureLibraryManager::templatePathsFromFeatureJson(const nlohmann::json& featureJson) {
    return templatePathsRecursive(featureJson);
}

std::unique_ptr<Feature> FeatureLibraryManager::loadEntryFeature(const QString& entryId) const {
    if (entryId.isEmpty()) {
        return nullptr;
    }

    QFile file(featureJsonPath(entryId));
    if (!file.open(QIODevice::ReadOnly)) {
        return nullptr;
    }
    const QByteArray raw = file.readAll();
    file.close();

    nlohmann::json featureJson;
    try {
        featureJson = nlohmann::json::parse(raw.constData());
    } catch (...) {
        return nullptr;
    }

    auto feature = std::make_unique<Feature>();
    try { JsonSerializer::featureFromJson(featureJson, *feature); }
    catch (const std::exception&) { return nullptr; }
    return feature;
}

QString FeatureLibraryManager::entryProjectDirectory(const QString& entryId) const {
    return entryDir(entryId);
}

bool FeatureLibraryManager::saveFeatureToLibrary(const Feature& feature,
                                                  const QString& sourceProjectDirectory,
                                                  const QString& entryNameOverride,
                                                  QString* outEntryId,
                                                  QStringList* missingTemplatePaths) {
    m_revision.fetch_add(1,std::memory_order_relaxed);
    const QString entryId = QUuid::createUuid().toString(QUuid::WithoutBraces);
    const QString dir = entryDir(entryId);
    QDir().mkpath(dir);
    if (missingTemplatePaths) {
        missingTemplatePaths->clear();
    }

    // Full feature snapshot: hotkey, run options, workflow blocks, and loop regions.
    nlohmann::json featureJson = JsonSerializer::featureToJson(feature);
    featureJson["id"] = entryId.toStdString();
    if (!entryNameOverride.trimmed().isEmpty()) {
        featureJson["name"] = entryNameOverride.toStdString();
    }

    // Copy templates first so a later profile import works even if the source profile is deleted.
    const std::vector<QString> relTemplatePaths = templatePathsFromFeature(feature);
    for (const QString& relPath : relTemplatePaths) {
        const QString src = QDir(sourceProjectDirectory).filePath(relPath);
        const QString dst = QDir(dir).filePath(relPath);
        if (!copyInsideRoot(src, sourceProjectDirectory) || !copyInsideRoot(dst, dir)) {
            if (missingTemplatePaths) missingTemplatePaths->push_back(relPath);
            continue;
        }
        if (!QFileInfo(src).exists()) {
            if (missingTemplatePaths) {
                missingTemplatePaths->push_back(relPath);
            }
            continue;
        }
        bool ok = true;
        copyTemplateWithMetadata(src, dst, ok);
        if (!ok && missingTemplatePaths) {
            missingTemplatePaths->push_back(relPath);
        }
    }

    const QString featureJsonOutPath = featureJsonPath(entryId);
    QSaveFile outFile(featureJsonOutPath);
    if (!outFile.open(QIODevice::WriteOnly)) {
        return false;
    }
    const QByteArray serialized = QString::fromStdString(featureJson.dump(2)).toUtf8();
    if (outFile.write(serialized) != serialized.size() || !outFile.commit()) return false;

    if (outEntryId) {
        *outEntryId = entryId;
    }
    appendEntryToOrder(entryId);
    return true;
}

FeatureLibraryManager::ImportResult FeatureLibraryManager::importEntryToProfile(const QString& entryId,
                                                                                  const QString& targetProjectDirectory) {
    ImportResult res;
    const QString dir = entryDir(entryId);
    const QString featurePath = featureJsonPath(entryId);
    QFile file(featurePath);
    if (!file.open(QIODevice::ReadOnly)) {
        return res;
    }
    const QByteArray raw = file.readAll();
    file.close();

    nlohmann::json featureJson;
    try {
        featureJson = nlohmann::json::parse(raw.constData());
    } catch (...) {
        return res;
    }

    auto temp = std::make_unique<Feature>();
    try { JsonSerializer::featureFromJson(featureJson, *temp); }
    catch (const std::exception&) { return res; }

    // New profile feature id; preserve hotkey and all options stored in the library entry.
    res.feature = temp->duplicateAsNewInstance(/*preserveHotkey=*/true);
    res.importedName = QString::fromStdString(featureJson.value("name", std::string{}));
    if (res.feature) {
        res.feature->setName(res.importedName.toStdString());
    }

    if (res.feature) res.missingTemplatePaths = copyFeatureTemplatesBetweenDirectories(*res.feature, dir, targetProjectDirectory);

    return res;
}

QStringList FeatureLibraryManager::copyFeatureTemplatesBetweenDirectories(
    Feature& feature, const QString& sourceProjectDirectory, const QString& targetProjectDirectory) {
    QStringList missing;
    QHash<QString, QString> remaps;
    const QString metadataSuffix = QStringLiteral(".capture.json");
    for (const QString& relPath : templatePathsFromFeature(feature)) {
        const QString source = QDir(sourceProjectDirectory).filePath(relPath);
        QString destination = QDir(targetProjectDirectory).filePath(relPath);
        if (!copyInsideRoot(source, sourceProjectDirectory) || !copyInsideRoot(destination, targetProjectDirectory)) {
            missing.append(relPath);
            continue;
        }
        if (QFileInfo(source).isFile() && sameFileBytes(source, destination)
            && sameFileBytes(source + metadataSuffix, destination + metadataSuffix)) continue;
        QString newRelative = relPath;
        if (QFileInfo::exists(destination) || QFileInfo::exists(destination + metadataSuffix)) {
            const QFileInfo original(relPath);
            const QString name = original.completeBaseName() + QLatin1Char('-') + QUuid::createUuid().toString(QUuid::WithoutBraces)
                + (original.suffix().isEmpty() ? QString() : QLatin1Char('.') + original.suffix());
            newRelative = QDir(original.path()).filePath(name);
            destination = QDir(targetProjectDirectory).filePath(newRelative);
        }
        remaps.insert(relPath, newRelative);
        if (!QFileInfo::exists(source) || !QDir().mkpath(QFileInfo(destination).absolutePath()) || !QFile::copy(source, destination)) {
            missing.append(newRelative);
            continue;
        }
        if (QFileInfo::exists(source + metadataSuffix) && !QFile::copy(source + metadataSuffix, destination + metadataSuffix)) missing.append(newRelative);
    }
    for (auto& block : feature.workflow().blocks()) {
        auto* image = dynamic_cast<ImageFindBlock*>(block.get());
        if (!image) continue;
        for (std::string& path : image->templatePaths) {
            const QString original = QString::fromStdString(path);
            if (!remaps.contains(original)) continue;
            const std::string replacement = remaps.value(original).toStdString();
            auto label = image->templateLabels.find(path);
            if (label != image->templateLabels.end() && replacement != path) {
                const std::string text = label->second;
                image->templateLabels.erase(label);
                image->templateLabels[replacement] = text;
            }
            path = replacement;
        }
    }
    return missing;
}

bool FeatureLibraryManager::removeEntry(const QString& entryId) {
    m_revision.fetch_add(1,std::memory_order_relaxed);
    if (entryId.isEmpty()) {
        return false;
    }
    QDir dir(entryDir(entryId));
    if (!dir.exists()) {
        return false;
    }
    const bool removed = dir.removeRecursively();
    if (removed) {
        removeEntryFromOrder(entryId);
    }
    return removed;
}

