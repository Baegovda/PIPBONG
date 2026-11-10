#pragma once
#include <QString>
#include <QTemporaryDir>
#include <vector>

enum class WorkspaceHistoryScope { Project, Workspace };
struct WorkspaceHistoryPaths {
    QString dataRoot, profilesDirectory, libraryDirectory, profileId, projectPath;
};
struct WorkspaceHistorySnapshot {
    QString directory;
    WorkspaceHistoryScope scope = WorkspaceHistoryScope::Workspace;
    QString profileId;
    bool profilesPresent = false;
    bool libraryPresent = false;
};

/// File transactions only. A job owns this store and immutable paths; no GUI callbacks.
class WorkspaceHistoryStore {
public:
    using Scope = WorkspaceHistoryScope;
    using Paths = WorkspaceHistoryPaths;
    using Snapshot = WorkspaceHistorySnapshot;
    explicit WorkspaceHistoryStore(const QString& temporaryParent);
    Snapshot capture(const Paths& paths, Scope scope);
    bool restore(const Paths& paths, const Snapshot& snapshot);
    void discard(const Snapshot& snapshot);
    void discard(const std::vector<Snapshot>& snapshots);
    QString lastError() const { return m_error; }
private:
    bool fail(const QString& error);
    QTemporaryDir m_storage;
    QString m_error;
};
