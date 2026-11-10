#include "app/WorkspaceHistoryController.h"
#include <QCoreApplication>
#include <QEventLoop>
#include <QFutureWatcher>
#include <QScopedValueRollback>
#include <QStringList>
#include <QtConcurrent/QtConcurrent>
#include <utility>

namespace {
struct OnExit {
    std::function<void()> cleanup;
    ~OnExit() { if (cleanup) cleanup(); }
};
template<class Work> auto io(Work work) {
    using Result = decltype(work());
    QFutureWatcher<Result> watcher;
    QEventLoop loop;
    QObject::connect(&watcher, &QFutureWatcher<Result>::finished, &loop, &QEventLoop::quit);
    watcher.setFuture(QtConcurrent::run(std::move(work)));
    // Host consumes editing input and suspends writers; painting/timers stay live.
    while (!watcher.isFinished()) {
        if (QCoreApplication::instance()) loop.exec();
        else watcher.waitForFinished();
    }
    return watcher.result();
}
struct Outcome { WorkspaceHistorySnapshot snapshot; QString error; };
WorkspaceHistoryScope scopeFor(const QString& reason) {
    // These FeatureList mutations change project JSON only; they retain assets.
    // Imports, library/profile mutations and unknown future commands keep full trees.
    static const QStringList projectOnly{
        QStringLiteral("feature-toggle-enabled"), QStringLiteral("feature-reorder"),
        QStringLiteral("feature-reorder-multi"), QStringLiteral("feature-add"),
        QStringLiteral("feature-paste"), QStringLiteral("feature-remove"),
        QStringLiteral("feature-edit"), QStringLiteral("feature-rename"),
        QStringLiteral("feature-assign-group"), QStringLiteral("feature-clear-group"),
        QStringLiteral("feature-group-assign-drop"), QStringLiteral("feature-group-add"),
        QStringLiteral("feature-group-rename"), QStringLiteral("feature-group-remove")};
    return projectOnly.contains(reason) ? WorkspaceHistoryScope::Project : WorkspaceHistoryScope::Workspace;
}
}

WorkspaceHistoryController::WorkspaceHistoryController(HostCallbacks host, const QString& temporaryParent)
    : m_host(std::move(host)), m_store(std::make_shared<WorkspaceHistoryStore>(temporaryParent)) {}

WorkspaceHistoryController::~WorkspaceHistoryController() {
    // Final teardown joins cleanup without running events on a partially
    // destructed host. Ordinary push/undo/redo cleanup runs in their I/O jobs.
    auto cleanup = QtConcurrent::run([store = std::move(m_store)]() mutable { store.reset(); });
    cleanup.waitForFinished();
}

bool WorkspaceHistoryController::fail(const QString& error) { m_error = error; return false; }

bool WorkspaceHistoryController::push(const QString& reason) {
    const QString frozenReason=reason;
    const auto host=m_host;
    const auto valid=[host]{return !host.validate || host.validate();};
    if (m_busy || !valid()) return false;
    QScopedValueRollback<bool> busy(m_busy, true);
    QScopedValueRollback<bool> preparing(m_preparing, true);
    OnExit guard{host.endOperation};
    m_error.clear();
    if (host.beginOperation && !host.beginOperation(false)) return fail(QStringLiteral("현재 작업을 마친 뒤 다시 시도해 주세요."));
    if (!valid()) return fail(QStringLiteral("작업 창이 닫혀 보관을 중단했어요."));
    if (!host.prepare || !host.prepare(false)) return fail(QStringLiteral("현재 내용을 저장하지 못했어요."));
    if (!valid()) return fail(QStringLiteral("작업 창이 닫혀 보관을 중단했어요."));
    if (!host.paths) return fail(QStringLiteral("저장 경로를 확인하지 못했어요."));
    const Paths paths = host.paths(QString());
    if (!valid()) return fail(QStringLiteral("작업 창이 닫혀 보관을 중단했어요."));
    const Scope scope = scopeFor(frozenReason);
    auto discarded = m_redo;
    if (static_cast<int>(m_undo.size()) >= MaxDepth) discarded.push_back(m_undo.front());
    m_preparing = false;
    const auto snapshot = io([store = m_store, paths, scope, discarded] {
        auto captured = store->capture(paths, scope);
        if (!captured.directory.isEmpty()) store->discard(discarded);
        return captured;
    });
    if (!valid()) return fail(QStringLiteral("작업 창이 닫혀 보관을 중단했어요."));
    if (snapshot.directory.isEmpty()) return fail(QStringLiteral("실행 취소 저장본을 만들지 못했어요."));
    if (static_cast<int>(m_undo.size()) >= MaxDepth) m_undo.erase(m_undo.begin());
    m_undo.push_back(snapshot);
    m_redo.clear();
    return true;
}

bool WorkspaceHistoryController::transfer(std::vector<Snapshot>& from, std::vector<Snapshot>& to) {
    if (m_busy) return false;
    if (from.empty()) {m_error.clear();return false;}
    const auto host=m_host;
    const auto valid=[host]{return !host.validate || host.validate();};
    if(!valid())return false;
    QScopedValueRollback<bool> busy(m_busy, true);
    QScopedValueRollback<bool> preparing(m_preparing, true);
    QScopedValueRollback<bool> restoring(m_restoring, true);
    OnExit guard{host.endOperation};
    m_error.clear();
    if (host.beginOperation && !host.beginOperation(true)) return fail(QStringLiteral("현재 작업을 마친 뒤 다시 시도해 주세요."));
    if (!valid()) return fail(QStringLiteral("작업 창이 닫혀 복구를 중단했어요."));
    if (!host.prepare || !host.prepare(true)) return fail(QStringLiteral("현재 내용을 저장하지 못해 복구를 중단했어요."));
    if (!valid()) return fail(QStringLiteral("작업 창이 닫혀 복구를 중단했어요."));
    if (!host.paths) return fail(QStringLiteral("복구 경로를 확인하지 못했어요."));
    const auto target = from.back();
    const auto paths = host.paths(target.profileId);
    if (!valid()) return fail(QStringLiteral("작업 창이 닫혀 복구를 중단했어요."));
    const std::vector<Snapshot> discarded = static_cast<int>(to.size()) >= MaxDepth
        ? std::vector<Snapshot>{to.front()} : std::vector<Snapshot>{};
    m_preparing = false;
    const auto outcome = io([store = m_store, paths, target, discarded] {
        const auto opposite = store->capture(paths, target.scope);
        if (opposite.directory.isEmpty()) return Outcome{{}, QStringLiteral("복구 전 내용을 보관하지 못했어요.")};
        if (!store->restore(paths, target)) {
            const auto error = store->lastError();
            store->discard(opposite);
            return Outcome{{}, error};
        }
        store->discard(target);
        store->discard(discarded);
        return Outcome{opposite, {}};
    });
    if (outcome.snapshot.directory.isEmpty()) return fail(outcome.error);
    from.pop_back();
    if (static_cast<int>(to.size()) >= MaxDepth) to.erase(to.begin());
    to.push_back(outcome.snapshot);
    if (valid() && host.restored) {
        QScopedValueRollback<bool> reloading(m_reloading,true);
        host.restored(target.scope, target.profileId);
    }
    return true;
}

bool WorkspaceHistoryController::undo() { return transfer(m_undo, m_redo); }
bool WorkspaceHistoryController::redo() { return transfer(m_redo, m_undo); }
