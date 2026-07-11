#include "storage/ProjectPackage.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QProcess>
#include <QSettings>
#include <QStandardPaths>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#endif

namespace {

bool runTar(const QStringList& arguments, int timeoutMs) {
    QProcess process;
    process.setProgram(QStringLiteral("tar"));
    process.setArguments(arguments);
#ifdef _WIN32
    process.setCreateProcessArgumentsModifier([](QProcess::CreateProcessArguments* args) {
        args->flags |= CREATE_NO_WINDOW;
    });
#endif
    process.start();
    if (!process.waitForStarted(5000)) {
        return false;
    }
    if (!process.waitForFinished(timeoutMs)) {
        process.kill();
        process.waitForFinished(3000);
        return false;
    }
    return process.exitStatus() == QProcess::NormalExit && process.exitCode() == 0;
}

QString normalizedAbsolutePath(const QString& path) {
    return QDir::fromNativeSeparators(QFileInfo(path).absoluteFilePath());
}

} // namespace

bool ProjectPackage::isPackagePath(const QString& path) {
    return path.endsWith(QString::fromLatin1(kExtension), Qt::CaseInsensitive);
}

QString ProjectPackage::defaultExportDirectory() {
    QSettings settings;
    const QString lastExport =
        settings.value(QStringLiteral("project/lastExportDirectory")).toString().trimmed();
    if (!lastExport.isEmpty()) {
        QDir().mkpath(lastExport);
        return lastExport;
    }

    const QString documents =
        QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation);
    const QString exportDir = QDir(documents).filePath(QStringLiteral("PIPBONG/Exports"));
    QDir().mkpath(exportDir);
    return exportDir;
}

bool ProjectPackage::packDirectory(const QString& profileDirectory, const QString& packagePath) {
    const QString sourceDir = normalizedAbsolutePath(profileDirectory);
    if (!QFileInfo(sourceDir).isDir()) {
        return false;
    }

    const QString destination = normalizedAbsolutePath(packagePath);
    QDir().mkpath(QFileInfo(destination).absolutePath());

    const QString tempPath = destination + QStringLiteral(".tmp");
    QFile::remove(tempPath);

    const QString nativeSource = QDir::toNativeSeparators(sourceDir);
    const QString nativeTemp = QDir::toNativeSeparators(tempPath);
    if (!runTar({QStringLiteral("-a"),
                 QStringLiteral("-cf"),
                 nativeTemp,
                 QStringLiteral("-C"),
                 nativeSource,
                 QStringLiteral(".")},
                120000)) {
        QFile::remove(tempPath);
        return false;
    }

    if (QFile::exists(destination)) {
        QFile::remove(destination);
    }
    if (!QFile::rename(tempPath, destination)) {
        QFile::remove(tempPath);
        return false;
    }
    return true;
}

bool ProjectPackage::unpackToDirectory(const QString& packagePath, const QString& profileDirectory) {
    const QString archivePath = normalizedAbsolutePath(packagePath);
    if (!QFileInfo::exists(archivePath)) {
        return false;
    }

    const QString destination = normalizedAbsolutePath(profileDirectory);
    QDir().mkpath(destination);

    const QString nativeArchive = QDir::toNativeSeparators(archivePath);
    const QString nativeDest = QDir::toNativeSeparators(destination);
    return runTar({QStringLiteral("-xf"), nativeArchive, QStringLiteral("-C"), nativeDest}, 120000);
}
