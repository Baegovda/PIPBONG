#pragma once
#include <QStringList>
struct ProfileRemovalRecovery {
    bool complete=true;
    QStringList warnings;
};
/// Consult persisted membership before loading or creating any profile workspace.
ProfileRemovalRecovery recoverProfileRemovals(const QString& profilesRoot);
