#pragma once

#include <memory>
#include <QString>
#include <QStringList>
#include <vector>

#include <nlohmann/json.hpp>

class Feature;

class FeatureLibraryManager {
public:
    struct Entry {
        QString id;
        QString name;
        int templateCount = 0;
    };

    struct ImportResult {
        std::unique_ptr<Feature> feature;
        QString importedName;
        QStringList missingTemplatePaths;
    };

    explicit FeatureLibraryManager(const QString& dataDirectory);

    std::vector<Entry> listEntries() const;

    /// Saves the given feature + referenced templates into the global feature library.
    /// @param entryNameOverride when provided, stored feature name is overridden (source feature unchanged).
    /// @param outEntryId optional output for the newly created library entry id.
    /// @param missingTemplatePaths when templates are referenced but missing in the source profile.
    bool saveFeatureToLibrary(const Feature& feature,
                               const QString& sourceProjectDirectory,
                               const QString& entryNameOverride,
                               QString* outEntryId,
                               QStringList* missingTemplatePaths);

    /// Loads feature JSON from the library entry, duplicates it as a new instance, and copies templates
    /// into the target profile's project directory.
    ImportResult importEntryToProfile(const QString& entryId,
                                       const QString& targetProjectDirectory);

    /// Permanently deletes a library entry (feature JSON + copied templates).
    bool removeEntry(const QString& entryId);

private:
    QString m_libraryRootDir;
    QString m_entriesDir;

    QString featureJsonPath(const QString& entryId) const;
    QString entryDir(const QString& entryId) const;

    static std::vector<QString> templatePathsFromFeature(const Feature& feature);
    static int countTemplatesInFeatureJson(const nlohmann::json& featureJson);
    static std::vector<QString> templatePathsFromFeatureJson(const nlohmann::json& featureJson);
};

