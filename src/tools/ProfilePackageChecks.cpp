#include "app/ProfilePackageController.h"
#include "storage/ProjectPackage.h"
#include <QCoreApplication>
#include <QDir>
#include <QElapsedTimer>
#include <QFile>
#include <QProcess>
#include <QSemaphore>
#include <QTemporaryDir>
#include <QThread>
#include <QTimer>
#include <atomic>
#include <memory>
#include <mutex>
#include <vector>
#include <iostream>
#ifdef _WIN32
#include <windows.h>
#endif

namespace {
bool expect(bool value, const char* message) {
    if (!value) std::cerr << "Profile package check failed: " << message << '\n';
    return value;
}
QByteArray read(const QString& path) {
    QFile file(path); return file.open(QIODevice::ReadOnly) ? file.readAll() : QByteArray();
}
bool write(const QString& path, const QByteArray& contents) {
    if (!QDir().mkpath(QFileInfo(path).absolutePath())) return false;
    QFile file(path); return file.open(QIODevice::WriteOnly) && file.write(contents) == contents.size();
}
}

bool runProfilePackageChecks() {
    std::atomic<int> active{0}, peak{0}, ticks{0};
    std::mutex mutex;
    std::vector<QString> packed;
    auto packer = [&](const QString& source, const QString&) {
        const int concurrency = ++active;
        peak.store(qMax(peak.load(), concurrency));
        QThread::msleep(25);
        { std::lock_guard<std::mutex> lock(mutex); packed.push_back(source); }
        --active;
        return source != QStringLiteral("fail");
    };
    ProfilePackageController controller(nullptr, packer);
    QTimer heartbeat;
    QObject::connect(&heartbeat, &QTimer::timeout, [&] { ++ticks; });
    heartbeat.start(2);
    controller.schedule(QStringLiteral("old"), QStringLiteral("one"));
    controller.schedule(QStringLiteral("latest"), QStringLiteral("one"));
    controller.schedule(QStringLiteral("other-profile"), QStringLiteral("two"));
    if (!expect(controller.waitForIdle(3000) && packed.size() == 2 && packed[0] == "latest"
                && peak.load() == 1 && ticks.load() > 2, "coalesced serial jobs keep event loop alive")) return false;
    if (!expect(controller.pauseAndWait(), "pause jobs")) return false;
    controller.schedule(QStringLiteral("discard"), QStringLiteral("three"));
    controller.discardPending();
    controller.resume();
    if (!expect(controller.waitForIdle(3000) && packed.size() == 2, "discard pre-restore requests")) return false;
    if (!expect(!controller.packNow(QStringLiteral("fail"), QStringLiteral("one"))
                && controller.packNow(QStringLiteral("retry"), QStringLiteral("one")), "failed seal can retry")) return false;
    heartbeat.stop();

    // Destruction disconnects watcher callbacks. The running job owns no UI/controller.
    struct DetachedState { QSemaphore started, release, done; };
    auto detached = std::make_shared<DetachedState>();
    auto owner = std::make_unique<ProfilePackageController>(nullptr,
        [detached](const QString&, const QString&) {
            detached->started.release(); detached->release.acquire(); detached->done.release(); return true;
        });
    owner->schedule(QStringLiteral("safe"), QStringLiteral("detached"));
    // A short observation timeout starts the queued job; it is still live here.
    if (!expect(!owner->waitForIdle(10) && detached->started.tryAcquire(1, 1000), "observe live worker")) return false;
    owner.reset();
    detached->release.release();
    if (!expect(detached->done.tryAcquire(1, 1000), "worker completes after controller destruction")) return false;
    QCoreApplication::processEvents();

    QTemporaryDir fixture;
    const QString source = fixture.filePath(QStringLiteral("source"));
    const QString archive = fixture.filePath(QStringLiteral("profile.pipbong"));
    if (!write(QDir(source).filePath(QStringLiteral("project.json")), "{\"version\":1}")
        || !write(QDir(source).filePath(QStringLiteral("project.json.history/snapshot-old.json")), "{}")) return false;
    if (!expect(ProjectPackage::packDirectory(source, archive), "real tar package")) return false;
    const QByteArray original = read(archive);
    QProcess tar;
    tar.start(QStringLiteral("tar"), {QStringLiteral("-tf"), archive});
    if (!tar.waitForFinished(5000)) return false;
    const QByteArray entries = tar.readAllStandardOutput();
    if (!expect(tar.exitCode() == 0 && entries.contains("project.json") && !entries.contains(".history"),
                "real package excludes recovery history")) return false;
#ifdef _WIN32
    HANDLE lock = CreateFileW(reinterpret_cast<LPCWSTR>(archive.utf16()), GENERIC_READ,
        FILE_SHARE_READ, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (!expect(lock != INVALID_HANDLE_VALUE, "lock archive")) return false;
    const bool refused = !ProjectPackage::packDirectory(source, archive);
    CloseHandle(lock);
    if (!expect(refused && read(archive) == original, "failed atomic package commit preserves previous archive")) return false;
#endif
    const QString extracted = fixture.filePath(QStringLiteral("extracted"));
    if (!expect(ProjectPackage::unpackToDirectory(archive, extracted)
                && read(QDir(extracted).filePath(QStringLiteral("project.json"))) == "{\"version\":1}",
                "real package roundtrip")) return false;
    std::cout << "Profile packages: serial/coalesced jobs, live UI, detached lifetime, tar roundtrip/history exclusion and locked-target preservation OK\n";
    return true;
}
