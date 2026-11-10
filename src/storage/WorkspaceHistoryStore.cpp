#include "storage/WorkspaceHistoryStore.h"
#include "storage/ProjectHistoryStore.h"
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSaveFile>
#include <QUuid>

namespace {
QString uniqueId() { return QUuid::createUuid().toString(QUuid::WithoutBraces); }

bool inside(const QString& path, const QString& root) {
    const QString child = QDir::cleanPath(QFileInfo(path).absoluteFilePath());
    const QString parent = QDir::cleanPath(QFileInfo(root).absoluteFilePath());
    return child.startsWith(parent + QLatin1Char('/'), Qt::CaseInsensitive);
}

// Refuse links anywhere along a replacement path. A directory selected through a
// junction must not redirect a recursive copy/remove outside the supplied root.
bool plainPath(const QString& path) {
    QFileInfo file(path);
    for (;;) {
        if (file.isSymLink() || file.isJunction()) return false;
        const QString parent = file.absolutePath();
        if (parent == file.absoluteFilePath()) return true;
        file.setFile(parent);
    }
}

bool copyTree(const QString& sourcePath, const QString& targetPath) {
    if (!plainPath(sourcePath) || !plainPath(targetPath)) return false;
    QDir source(sourcePath);
    if (!source.exists()) return true;
    if (!QDir().mkpath(targetPath)) return false;
    const auto entries = source.entryInfoList(QDir::NoDotAndDotDot | QDir::Files | QDir::Dirs
                                               | QDir::Hidden | QDir::System);
    for (const auto& entry : entries) {
        if (entry.isSymLink() || entry.isJunction()) return false;
        const QString destination = QDir(targetPath).filePath(entry.fileName());
        if (entry.isDir()) {
            if (!copyTree(entry.absoluteFilePath(), destination)) return false;
        } else if (!QFile::copy(entry.absoluteFilePath(), destination)) {
            return false;
        }
    }
    return true;
}

bool removeOwnedTree(const QString& path, const QString& owner) {
    // Every recursive deletion is restricted to an owned, non-linked child.
    if (!inside(path, owner) || !plainPath(path) || !plainPath(owner)) return false;
    return !QFileInfo::exists(path) || QDir(path).removeRecursively();
}

bool validPaths(const WorkspaceHistoryStore::Paths& paths) {
    return !paths.dataRoot.isEmpty() && QFileInfo(paths.dataRoot).isDir() && plainPath(paths.dataRoot)
        && inside(paths.profilesDirectory, paths.dataRoot) && plainPath(paths.profilesDirectory)
        && inside(paths.libraryDirectory, paths.dataRoot) && plainPath(paths.libraryDirectory)
        && (!QFileInfo::exists(paths.profilesDirectory) || QFileInfo(paths.profilesDirectory).isDir())
        && (!QFileInfo::exists(paths.libraryDirectory) || QFileInfo(paths.libraryDirectory).isDir())
        && paths.profilesDirectory.compare(paths.libraryDirectory, Qt::CaseInsensitive) != 0
        && !inside(paths.profilesDirectory, paths.libraryDirectory)
        && !inside(paths.libraryDirectory, paths.profilesDirectory);
}
}

WorkspaceHistoryStore::WorkspaceHistoryStore(const QString& temporaryParent)
    : m_storage(QDir(temporaryParent.isEmpty() ? QDir::tempPath() : temporaryParent)
        .filePath(QStringLiteral("pipbong-history-XXXXXX"))) {}

bool WorkspaceHistoryStore::fail(const QString& error) {
    m_error = error;
    return false;
}

WorkspaceHistoryStore::Snapshot WorkspaceHistoryStore::capture(const Paths& paths, Scope scope) {
    m_error.clear();
    if (!m_storage.isValid()) return {};
    if (!validPaths(paths)) return {};
    Snapshot snapshot{QDir(m_storage.path()).filePath(uniqueId()), scope, paths.profileId};
    if (!QDir().mkpath(snapshot.directory)) return {};
    bool ok = false;
    if (scope == Scope::Project) {
        ok = !paths.profileId.isEmpty() && inside(paths.projectPath, paths.profilesDirectory)
            && plainPath(paths.projectPath)
            && QFile::copy(paths.projectPath, QDir(snapshot.directory).filePath(QStringLiteral("project.json")));
    } else {
        snapshot.profilesPresent = QFileInfo(paths.profilesDirectory).isDir();
        snapshot.libraryPresent = QFileInfo(paths.libraryDirectory).isDir();
        ok = copyTree(paths.profilesDirectory, QDir(snapshot.directory).filePath(QStringLiteral("profiles")))
            && copyTree(paths.libraryDirectory, QDir(snapshot.directory).filePath(QStringLiteral("featureLibrary")));
    }
    if (!ok) { discard(snapshot); return {}; }
    return snapshot;
}

void WorkspaceHistoryStore::discard(const Snapshot& snapshot) {
    removeOwnedTree(snapshot.directory, m_storage.path());
}

bool WorkspaceHistoryStore::restore(const Paths& paths, const Snapshot& snapshot) {
    m_error.clear();
    if (!validPaths(paths) || !inside(snapshot.directory, m_storage.path()) || !plainPath(snapshot.directory)) {
        return fail(QStringLiteral("복구 경로를 확인하지 못했어요."));
    }
    if (snapshot.scope == Scope::Project) {
        if (!inside(paths.projectPath, paths.profilesDirectory) || !plainPath(paths.projectPath)) return fail(QStringLiteral("프로젝트 경로가 올바르지 않아요."));
        QFile source(QDir(snapshot.directory).filePath(QStringLiteral("project.json")));
        if (!source.open(QIODevice::ReadOnly)) return fail(QStringLiteral("복구할 프로젝트를 읽지 못했어요."));
        const QByteArray bytes = source.readAll();
        if (source.error() != QFileDevice::NoError) return fail(QStringLiteral("복구할 프로젝트 읽기에 실패했어요."));
        QSaveFile target(paths.projectPath);
        if (!target.open(QIODevice::WriteOnly) || target.write(bytes) != bytes.size()
            || !ProjectHistoryStore::preserveBeforeSave(paths.projectPath, bytes) || !target.commit()) {
            return fail(QStringLiteral("프로젝트를 교체하지 못했어요. 기존 파일과 실행 취소 이력을 유지해요."));
        }
        return true;
    }

    // Stage both trees before touching either live directory. Directory renames
    // stay on the same volume; a failed second rename rolls back the first one.
    const QString transaction = QDir(paths.dataRoot).filePath(QStringLiteral(".workspace-restore-") + uniqueId());
    const QStringList targets{paths.profilesDirectory, paths.libraryDirectory};
    const QStringList names{QStringLiteral("profiles"), QStringLiteral("featureLibrary")};
    const bool expected[2]{snapshot.profilesPresent, snapshot.libraryPresent};
    for (int i = 0; i < names.size(); ++i) {
        if (QFileInfo(QDir(snapshot.directory).filePath(names[i])).isDir() != expected[i]) {
            return fail(QStringLiteral("실행 취소 저장본이 손상됐어요. 기존 폴더와 이력을 유지해요."));
        }
    }
    if (!QDir().mkpath(transaction)) return fail(QStringLiteral("복구 준비 폴더를 만들지 못했어요."));
    for (int i = 0; i < targets.size(); ++i) {
        if (!copyTree(QDir(snapshot.directory).filePath(names[i]), QDir(transaction).filePath(QStringLiteral("new%1").arg(i)))) {
            removeOwnedTree(transaction, paths.dataRoot);
            return fail(QStringLiteral("복구 준비에 실패했어요. 기존 폴더를 유지해요."));
        }
    }
    bool moved[2]{false, false};
    bool installed[2]{false, false};
    bool ok = true;
    for (int i = 0; i < targets.size() && ok; ++i) {
        if (QFileInfo::exists(targets[i])) {
            moved[i] = QDir().rename(targets[i], QDir(transaction).filePath(QStringLiteral("old%1").arg(i)));
            ok = moved[i];
        }
    }
    for (int i = 0; i < targets.size() && ok; ++i) {
        const QString staged = QDir(transaction).filePath(QStringLiteral("new%1").arg(i));
        if (QFileInfo::exists(staged)) {
            installed[i] = QDir().rename(staged, targets[i]);
            ok = installed[i];
        }
    }
    if (!ok) {
        bool rolledBack = true;
        for (int i = targets.size() - 1; i >= 0; --i) {
            if (installed[i] && !removeOwnedTree(targets[i], paths.dataRoot)) rolledBack = false;
            if (moved[i] && !QDir().rename(QDir(transaction).filePath(QStringLiteral("old%1").arg(i)), targets[i])) rolledBack = false;
        }
        if (!rolledBack) return fail(QStringLiteral("복구와 원상 복원에 실패했어요. 원본 백업을 유지해요: ") + transaction);
        removeOwnedTree(transaction, paths.dataRoot);
        return fail(QStringLiteral("폴더를 교체하지 못했어요. 원본과 실행 취소 이력을 유지해요."));
    }
    removeOwnedTree(transaction, paths.dataRoot);
    return true;
}

void WorkspaceHistoryStore::discard(const std::vector<Snapshot>& snapshots) {
    for (const auto& snapshot : snapshots) discard(snapshot);
}
