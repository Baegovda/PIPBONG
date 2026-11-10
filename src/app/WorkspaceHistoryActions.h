#pragma once
#include "app/WorkspaceHistoryController.h"
#include <memory>
class QWidget;
class MemoDialog;
class ProfilePackageController;
class WorkspaceHistoryActivity;

/// GUI transaction orchestration; the window supplies policy and view bindings.
class WorkspaceHistoryActions {
public:
    struct HostCallbacks {
        WorkspaceHistoryController::HostCallbacks history;
        std::function<bool(bool restoring)> canBegin;
        std::function<MemoDialog*()> memo;
        std::function<void()> suspendAutoSave;
        std::function<void()> finished;
        std::function<void(const QString&)> reportError;
    };
    WorkspaceHistoryActions(QWidget* parent, ProfilePackageController& packages, HostCallbacks host);
    ~WorkspaceHistoryActions();
    bool push(const QString& reason);
    bool undo();
    bool redo();
    bool isBusy() const { return m_history->isBusy(); }
    bool isPreparing() const { return m_history->isPreparing(); }
    bool isReloading() const { return m_history->isReloading(); }
    QString lastError() const { return m_history->lastError(); }
private:
    enum class Operation { Push, Undo, Redo };
    struct State;
    bool run(Operation, QString reason = {});
    std::shared_ptr<State> m_state;
    std::shared_ptr<WorkspaceHistoryController> m_history;
};
