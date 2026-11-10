#pragma once

#include <QString>
#include "storage/WorkspaceHistoryStore.h"
#include <memory>
#include <functional>
#include <vector>

/// Owns undo/redo storage and replacement. The window supplies paths and reloads
/// UI after a successful restore; this class never includes or calls MainWindow.
class WorkspaceHistoryController {
public:
    using Scope = WorkspaceHistoryScope;
    using Paths = WorkspaceHistoryPaths;
    struct HostCallbacks {
        /// Owner remains eligible before preparation/publication. In-flight file
        /// restores finish transactionally even if this becomes false.
        std::function<bool()> validate;
        std::function<Paths(const QString& profileId)> paths;
        /// GUI transaction guards: block mutations, suspend writers, pause packages.
        std::function<bool(bool restoring)> beginOperation;
        std::function<void()> endOperation;
        /// Flush current edits; stop sessions when restoring, but not when recording.
        std::function<bool(bool restoring)> prepare;
        std::function<void(Scope, const QString& profileId)> restored;
    };

    explicit WorkspaceHistoryController(HostCallbacks host, const QString& temporaryParent = QString());
    ~WorkspaceHistoryController();
    bool push(const QString& reason);
    bool undo();
    bool redo();
    bool isRestoring() const { return m_restoring; }
    bool isBusy() const { return m_busy; }
    bool isPreparing() const { return m_preparing; }
    bool isReloading() const { return m_reloading; }
    int undoCount() const { return static_cast<int>(m_undo.size()); }
    int redoCount() const { return static_cast<int>(m_redo.size()); }
    QString lastError() const { return m_error; }
    static constexpr int MaxDepth = 20;

private:
    using Snapshot = WorkspaceHistorySnapshot;
    bool transfer(std::vector<Snapshot>& from, std::vector<Snapshot>& to);
    bool fail(const QString& error);

    HostCallbacks m_host;
    std::shared_ptr<WorkspaceHistoryStore> m_store;
    std::vector<Snapshot> m_undo;
    std::vector<Snapshot> m_redo;
    QString m_error;
    bool m_restoring = false;
    bool m_busy = false;
    bool m_preparing = false;
    bool m_reloading = false;
};
