#pragma once

#include <QString>

/// ZIP-backed .pipbong profile package (project.json + templates/ + profile-settings.json).
class ProjectPackage {
public:
    static constexpr const char* kExtension = ".pipbong";
    static constexpr const char* kFilterPattern = "*.pipbong";

    static bool isPackagePath(const QString& path);
    static QString defaultExportDirectory();

    /// Pack profile workspace folder contents into @p packagePath (atomic replace).
    static bool packDirectory(const QString& profileDirectory, const QString& packagePath);

    /// Internal staging already contains the assets although its project JSON
    /// points at the not-yet-published import destination.
    static bool packDirectoryAtAssetRoot(const QString& profileDirectory, const QString& packagePath,
                                         const QString& assetRoot);

    /// Extract @p packagePath into @p profileDirectory (creates directory if needed).
    static bool unpackToDirectory(const QString& packagePath, const QString& profileDirectory);
};
