#include "app/UpdateChecker.h"
#include "app/UpdateUiController.h"
#include <QCoreApplication>
#include <QElapsedTimer>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QThread>
#include <QTimer>
#include <QCryptographicHash>
#include <cstring>
#include <iostream>

namespace {
class ScriptedReply : public QNetworkReply {
public:
    ScriptedReply(const QNetworkRequest& request, QByteArray payload, QObject* parent)
        : QNetworkReply(parent), m_payload(std::move(payload)) {
        setRequest(request);
        setUrl(request.url());
        open(QIODevice::ReadOnly | QIODevice::Unbuffered);
        QTimer::singleShot(5, this, [this] {
            if (isFinished()) return;
            setFinished(true);
            emit readyRead();
            emit finished();
        });
    }
    void abort() override {
        if (isFinished()) return;
        setError(QNetworkReply::OperationCanceledError, QStringLiteral("scripted cancel"));
        setFinished(true);
        emit finished(); // Synchronous, like a real aborted reply can be.
    }
    qint64 bytesAvailable() const override { return m_payload.size() - m_offset + QNetworkReply::bytesAvailable(); }
protected:
    qint64 readData(char* data, qint64 max) override {
        const qint64 count = qMin(max, qint64(m_payload.size()) - m_offset);
        if (count <= 0) return -1;
        std::memcpy(data, m_payload.constData() + m_offset, static_cast<size_t>(count));
        m_offset += count;
        return count;
    }
private:
    QByteArray m_payload;
    qint64 m_offset = 0;
};
class ScriptedNetwork : public QNetworkAccessManager {
public:
    QByteArray payload;
    int requests = 0;
protected:
    QNetworkReply* createRequest(Operation, const QNetworkRequest& request, QIODevice*) override {
        ++requests;
        return new ScriptedReply(request, payload, this);
    }
};
class RecordingInstaller : public UpdateChecker {
public:
    explicit RecordingInstaller(QNetworkAccessManager* network) : UpdateChecker(nullptr, network) {}
    void installPendingUpdate() override { ++installs; }
    int installs = 0;
};
class GuardedDownload : public UpdateChecker {
public:
    explicit GuardedDownload(QNetworkAccessManager* network) : UpdateChecker(nullptr, network) {}
    int launches = 0;
protected:
    bool launchUpdater(const QString&, const QStringList&) override { ++launches; return false; }
};
bool waitFor(const std::function<bool()>& condition) {
    QElapsedTimer timer; timer.start();
    while (!condition() && timer.elapsed() < 2000) {
        QCoreApplication::processEvents(); QThread::msleep(1);
    }
    return condition();
}
bool expect(bool result, const char* message) {
    if (!result) std::cerr << "Update controller check failed: " << message << '\n';
    return result;
}
}

bool runUpdateControllerChecks() {
    ScriptedNetwork network;
    RecordingInstaller checker(&network);
    int finished = 0;
    int failed = 0;
    QObject::connect(&checker, &UpdateChecker::checkFinished, &checker,
        [&](bool success, bool, const QString&) { ++finished; if (!success) ++failed; });
    network.payload = R"({"tag_name":"v99.0.0","body":"fixture","assets":[{"name":"PIPBONG-win64.zip","browser_download_url":"https://example.invalid/update.zip","size":123,"digest":"sha256:aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"}]})";
    checker.checkForUpdates(UpdateChecker::CheckUiMode::Silent);
    checker.checkForUpdates(UpdateChecker::CheckUiMode::Silent);
    if (!expect(waitFor([&] { return finished == 1; }) && failed == 0 && checker.hasPendingUpdate()
                && checker.pendingUpdate().version.toString() == "99.0.0"
                && checker.pendingUpdate().assetSizeBytes == 123,
                "replaced request abort cannot finish the new request")) return false;
    bool active = true;
    bool autoInstall = true;
    int stops = 0;
    UpdateUiController::HostCallbacks host;
    host.stopSessions = [&] { ++stops; };
    host.hasActiveEngine = [&] { return active; };
    host.autoInstallEnabled = [&] { return autoInstall; };
    host.showStatus = [](const QString&, int) {};
    UpdateUiController controller(&checker, nullptr, nullptr, host);
    controller.applySettings(0);
    if (!expect(checker.installs == 0 && stops == 1, "defer automatic install while engine lives")) return false;
    active = false;
    controller.maybeStartAutomaticUpdate();
    controller.maybeStartAutomaticUpdate();
    if (!expect(checker.installs == 1, "start deferred install once")) return false;
    emit checker.installFailed();
    controller.maybeStartAutomaticUpdate();
    if (!expect(checker.installs == 1, "failure does not create retry loop")) return false;
    controller.applySettings(0);
    if (!expect(checker.installs == 2, "new request can retry after failure")) return false;
    emit checker.installFailed();
    autoInstall = false;
    controller.applySettings(0);
    controller.maybeStartAutomaticUpdate();
    if (!expect(checker.installs == 2, "automatic install honors disabled setting")) return false;
    network.payload = "{invalid";
    checker.checkForUpdates(UpdateChecker::CheckUiMode::Silent);
    if (!expect(waitFor([&] { return finished == 2; }) && failed == 1 && checker.hasPendingUpdate(),
                "malformed metadata fails without clearing known pending release")) return false;
    const int requestsBeforeStop = network.requests;
    controller.stop();
    controller.onWindowShown();
    controller.checkInteractively();
    controller.onButtonClicked();
    controller.applySettings(1);
    if (!expect(network.requests == requestsBeforeStop, "stopped controller cannot start requests")) return false;
    ScriptedNetwork downloadNetwork;
    downloadNetwork.payload = QByteArray(6 * 1024 * 1024, 'x');
    GuardedDownload download(&downloadNetwork);
    UpdateChecker::ReleaseInfo release;
    release.downloadUrl = QStringLiteral("https://example.invalid/fixture.zip");
    release.assetSizeBytes = downloadNetwork.payload.size();
    release.assetSha256 = QString::fromLatin1(QCryptographicHash::hash(downloadNetwork.payload, QCryptographicHash::Sha256).toHex());
    int failures = 0, preflights = 0;
    QObject::connect(&download, &UpdateChecker::installFailed, &download, [&] { ++failures; });
    download.setInstallPreflight([&] { ++preflights; return false; });
    download.downloadAndInstall(release);
    if (!expect(waitFor([&] { return failures == 1; }) && preflights == 1 && download.launches == 0,
                "verified real download path blocks launcher when data save fails")) return false;
    download.setInstallPreflight([&] { ++preflights; return true; });
    download.downloadAndInstall(release);
    if (!expect(waitFor([&] { return failures == 2; }) && preflights == 2 && download.launches == 1,
                "retry reaches recording launcher after save succeeds; no process spawned")) return false;
    std::cout << "Update controller: scripted replies, abort identity, metadata, engine deferral, failure retry and shutdown guard OK (no network/install)\n";
    return true;
}
