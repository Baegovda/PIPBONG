#include "app/FeatureLibraryManager.h"

#include "core/workflow/blocks/ImageFindBlock.h"
#include "model/Feature.h"
#include "storage/JsonSerializer.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QUuid>

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

std::vector<FeatureLibraryManager::Entry> FeatureLibraryManager::listEntries() const {
    std::vector<Entry> out;
    const QDir entriesDir(m_entriesDir);
    if (!entriesDir.exists()) {
        return out;
    }

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
            out.push_back(std::move(e));
        } catch (...) {
            continue;
        }
    }

    std::sort(out.begin(), out.end(), [](const Entry& a, const Entry& b) { return a.name < b.name; });
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

bool FeatureLibraryManager::saveFeatureToLibrary(const Feature& feature,
                                                  const QString& sourceProjectDirectory,
                                                  const QString& entryNameOverride,
                                                  QString* outEntryId,
                                                  QStringList* missingTemplatePaths) {
    const QString entryId = QUuid::createUuid().toString(QUuid::WithoutBraces);
    const QString dir = entryDir(entryId);
    QDir().mkpath(dir);
    if (missingTemplatePaths) {
        missingTemplatePaths->clear();
    }

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
        if (!QFileInfo(src).exists()) {
            if (missingTemplatePaths) {
                missingTemplatePaths->push_back(relPath);
            }
            continue;
        }
        bool ok = true;
        copyFileOverwrite(src, dst, ok);
        if (!ok && missingTemplatePaths) {
            missingTemplatePaths->push_back(relPath);
        }
    }

    const QString featureJsonOutPath = featureJsonPath(entryId);
    QFile outFile(featureJsonOutPath);
    if (!outFile.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        return false;
    }
    outFile.write(QString::fromStdString(featureJson.dump(2)).toUtf8());
    outFile.close();

    if (outEntryId) {
        *outEntryId = entryId;
    }
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
    JsonSerializer::featureFromJson(featureJson, *temp);

    // Duplicate to ensure new id + hotkey is cleared.
    res.feature = temp->duplicateAsNewInstance();
    res.importedName = QString::fromStdString(featureJson.value("name", std::string{}));
    if (res.feature) {
        res.feature->setName(res.importedName.toStdString());
    }

    // Copy referenced templates into this profile's templates folder.
    const std::vector<QString> relTemplatePaths =
        res.feature ? templatePathsFromFeature(*res.feature) : templatePathsFromFeatureJson(featureJson);
    for (const QString& relPath : relTemplatePaths) {
        const QString src = QDir(dir).filePath(relPath);
        const QString dst = QDir(targetProjectDirectory).filePath(relPath);
        if (!QFileInfo(src).exists()) {
            res.missingTemplatePaths.push_back(relPath);
            continue;
        }
        bool ok = true;
        copyFileOverwrite(src, dst, ok);
        if (!ok) {
            res.missingTemplatePaths.push_back(relPath);
        }
    }
    return res;
}

bool FeatureLibraryManager::removeEntry(const QString& entryId) {
    if (entryId.isEmpty()) {
        return false;
    }
    QDir dir(entryDir(entryId));
    if (!dir.exists()) {
        return false;
    }
    return dir.removeRecursively();
}

