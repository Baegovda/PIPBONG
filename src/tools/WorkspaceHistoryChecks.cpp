#include "app/WorkspaceHistoryController.h"
#include <QDir>
#include <QFile>
#include <QTemporaryDir>
#include <QCoreApplication>
#include <QThread>
#include <QTimer>
#include <iostream>
#ifdef _WIN32
#include <windows.h>
#endif

namespace {
bool expect(bool value, const char* message) {
    if (!value) std::cerr << "Workspace history failed: " << message << '\n';
    return value;
}
bool write(const QString& path, const QByteArray& contents) {
    if (!QDir().mkpath(QFileInfo(path).absolutePath())) return false;
    QFile file(path);
    return file.open(QIODevice::WriteOnly) && file.write(contents) == contents.size();
}
QByteArray read(const QString& path) {
    QFile file(path);
    return file.open(QIODevice::ReadOnly) ? file.readAll() : QByteArray();
}
}

bool runWorkspaceHistoryChecks() {
    QTemporaryDir fixture;
    if (!fixture.isValid()) return false;
    WorkspaceHistoryController::Paths paths;
    paths.dataRoot = fixture.path();
    paths.profilesDirectory = fixture.filePath(QStringLiteral("profiles"));
    paths.libraryDirectory = fixture.filePath(QStringLiteral("featureLibrary"));
    paths.profileId = QStringLiteral("one");
    paths.projectPath = QDir(paths.profilesDirectory).filePath(QStringLiteral("one/project.json"));
    const QString libraryFile = QDir(paths.libraryDirectory).filePath(QStringLiteral("entry.json"));
    const QByteArray first("{\"version\":1,\"features\":[],\"targetWindowTitle\":\"one\"}");
    const QByteArray second("{\"version\":1,\"features\":[],\"targetWindowTitle\":\"two\"}");
    bool allowSave = true;
    bool guiCallbacks = true;
    const auto ownerThread = QThread::currentThread();
    int restored = 0;
    QString requestedProfile;
    WorkspaceHistoryController::HostCallbacks host;
    host.paths = [&](const QString& id) { guiCallbacks &= QThread::currentThread() == ownerThread; requestedProfile = id; return paths; };
    host.prepare = [&](bool) { guiCallbacks &= QThread::currentThread() == ownerThread; return allowSave; };
    host.restored = [&](auto, const QString&) { guiCallbacks &= QThread::currentThread() == ownerThread; ++restored; };
    WorkspaceHistoryController history(host, fixture.path());
    if (!write(paths.projectPath, first) || !write(libraryFile, "library-one")) return false;
    if (!expect(history.push(QStringLiteral("feature-toggle-enabled")), "capture project")) return false;
    if (!write(paths.projectPath, second) || !write(libraryFile, "library-two")) return false;
    if (!expect(history.undo() && read(paths.projectPath) == first && read(libraryFile) == "library-two",
                "project undo leaves library alone")) return false;
    if (!expect(requestedProfile == "one" && history.redoCount() == 1, "profile-specific opposite capture")) return false;
    if (!expect(history.redo() && read(paths.projectPath) == second, "project redo")) return false;
    allowSave = false;
    if (!expect(!history.undo() && history.undoCount() == 1 && read(paths.projectPath) == second,
                "save failure preserves history and content")) return false;
    allowSave = true;

    if (!expect(history.push(QStringLiteral("profile-edit")), "capture workspace")) return false;
    if (!write(paths.projectPath, first) || !write(libraryFile, "library-three")) return false;
#ifdef _WIN32
    // Deny deletion/rename of the SECOND directory. The first one will already
    // have moved to the rollback area when this failure occurs.
    HANDLE lock = CreateFileW(reinterpret_cast<LPCWSTR>(paths.libraryDirectory.utf16()),
        GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_EXISTING,
        FILE_FLAG_BACKUP_SEMANTICS, nullptr);
    if (!expect(lock != INVALID_HANDLE_VALUE, "lock second directory")) return false;
    const bool failedSafely = !history.undo() && history.undoCount() == 2
        && read(paths.projectPath) == first && read(libraryFile) == "library-three";
    CloseHandle(lock);
    if (!expect(failedSafely, "second-directory failure rolls back first directory and preserves undo")) return false;
#endif
    if (!expect(history.undo() && read(paths.projectPath) == second && read(libraryFile) == "library-two",
                "workspace undo retry")) return false;
    if (!expect(history.redo() && read(paths.projectPath) == first && read(libraryFile) == "library-three",
                "workspace redo restores both trees")) return false;
    if (!expect(history.undo(), "undo before branching")) return false;
    if (!expect(history.push(QStringLiteral("profile-edit")) && history.redoCount() == 0,
                "new branch clears redo")) return false;
    for (int i = 0; i < 24; ++i) {
        if (!history.push(QStringLiteral("feature-toggle-enabled"))) return false;
    }
    if (!expect(history.undoCount() == WorkspaceHistoryController::MaxDepth, "bounded undo depth")) return false;
    const QString goodProfiles = paths.profilesDirectory;
    paths.profilesDirectory = QDir(fixture.path()).filePath(QStringLiteral("../outside"));
    if (!expect(!history.undo() && history.undoCount() == WorkspaceHistoryController::MaxDepth,
                "reject escaped replacement path")) return false;
    paths.profilesDirectory = goodProfiles;
    const QString corruptParent = fixture.filePath(QStringLiteral("corrupt-history"));
    if (!QDir().mkpath(corruptParent)) return false;
    WorkspaceHistoryController corrupt(host, corruptParent);
    if (!corrupt.push(QStringLiteral("profile-edit"))) return false;
    const QString storage = QDir(corruptParent).entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot).first().absoluteFilePath();
    const QString snapshot = QDir(storage).entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot).first().absoluteFilePath();
    const QString missingTree = QDir(snapshot).filePath(QStringLiteral("profiles"));
    if (!missingTree.startsWith(fixture.path() + QLatin1Char('/')) || !QDir(missingTree).removeRecursively()) return false;
    const QByteArray beforeCorruptRestore = read(paths.projectPath);
    if (!expect(!corrupt.undo() && corrupt.undoCount() == 1 && read(paths.projectPath) == beforeCorruptRestore,
                "missing snapshot tree cannot erase live workspace")) return false;
    if (!expect(restored >= 5 && !history.isRestoring(), "reload callback and restore guard")) return false;
    for (const auto* reason : {"feature-toggle-enabled","feature-reorder","feature-reorder-multi",
        "feature-add","feature-paste","feature-remove","feature-edit","feature-rename",
        "feature-assign-group","feature-clear-group","feature-group-assign-drop",
        "feature-group-add","feature-group-rename","feature-group-remove"}) {
        if (!write(paths.projectPath,first) || !write(libraryFile,"before-project-edit")
            || !history.push(QString::fromLatin1(reason)) || !write(paths.projectPath,second)
            || !write(libraryFile,"library-change-after-project-edit") || !history.undo()
            || read(paths.projectPath)!=first || read(libraryFile)!="library-change-after-project-edit")
            return expect(false,"project-only command restores JSON without rewinding unrelated library/assets");
    }
    for (const auto* reason : {"feature-import-library","feature-import-from-library","feature-save-to-library","unknown-command"}) {
        if (!write(libraryFile,"full-workspace") || !history.push(QString::fromLatin1(reason))
            || !write(libraryFile,"changed") || !history.undo() || read(libraryFile)!="full-workspace")
            return expect(false,"asset/library and unknown commands retain full workspace rollback");
    }
    const QByteArray payload(64 * 1024, 'h');
    for (int i=0;i<128;++i) if (!write(QDir(paths.libraryDirectory).filePath(QString::number(i)+".bin"),payload)) return false;
    int heartbeats=0;
    bool reentryBlocked=false;
    QTimer heartbeat;
    QObject::connect(&heartbeat,&QTimer::timeout,[&] {
        ++heartbeats;
        if (history.isBusy()) reentryBlocked = !history.push("nested") && !history.undo() && !history.redo();
    });
    heartbeat.start(1);
    if (!expect(history.push("profile-edit") && heartbeats > 0 && reentryBlocked && guiCallbacks && !history.isBusy(),
                "worker copies keep GUI timers live, reject nested mutation, and keep callbacks on owner thread")) return false;
    heartbeat.stop();
    std::cout << "Workspace history: worker I/O, live event loop/reentry/GUI callbacks, undo+redo, locked-folder rollback, failed-save retention and depth/path guards OK\n";
    return true;
}
