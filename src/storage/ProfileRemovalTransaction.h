#pragma once
#include <QString>
#include <functional>

/// Stage owned paths, commit the manifest, then discard backups; retain failed rollback data.
bool removeProfileFilesTransaction(const QString& profilesRoot,const QString& workspace,const QString& package,
                                   const std::function<bool()>& commit,QString* error);
/// After commit or complete rollback only: delete staged data before its recovery journal.
bool discardProfileRemovalBackup(const QString& profilesRoot,const QString& backup);
