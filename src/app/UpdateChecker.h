#pragma once

#include <QObject>
#include <QByteArray>
#include <QVersionNumber>
#include <functional>
#include <memory>
#include <QTemporaryDir>

class QWidget;
class QNetworkAccessManager;

class UpdateChecker : public QObject {
    Q_OBJECT
public:
    enum class CheckUiMode {
        Interactive,
        Silent
    };

    struct ReleaseInfo {
        QVersionNumber version;
        QString downloadUrl;
        QString releaseNotes;
        qint64 assetSizeBytes = 0;
        QString assetSha256;
    };

    explicit UpdateChecker(QWidget* parentWidget, QNetworkAccessManager* network = nullptr);
    static bool verifyDownload(const ReleaseInfo& release, const QByteArray& payload);

    void checkForUpdates(CheckUiMode mode = CheckUiMode::Interactive);
    void downloadAndInstall(const ReleaseInfo& release);
    virtual void installPendingUpdate();
    void setInstallPreflight(std::function<bool()> guard) { m_installPreflight = std::move(guard); }

    bool hasPendingUpdate() const;
    ReleaseInfo pendingUpdate() const;

signals:
    void checkFinished(bool success, bool updateAvailable, const QString& errorMessage);
    void readyToRestartForUpdate();
    void installStarted();
    void installFailed();

protected:
    virtual bool launchUpdater(const QString& executable, const QStringList& arguments);

private:
    void finishCheck(bool success,
                     const QString& errorMessage,
                     const ReleaseInfo& release,
                     bool updateAvailable);
    void onCheckReplyFinished();
    void onDownloadProgress(qint64 received, qint64 total);
    void onDownloadFinished();
    void cancelActiveReply();
    void failInstall(const QString& message);

    QWidget* m_parentWidget = nullptr;
    class QNetworkAccessManager* m_network = nullptr;
    class QNetworkReply* m_activeReply = nullptr;
    class QProgressDialog* m_downloadProgressDialog = nullptr;
    QString m_pendingDownloadPath;
    ReleaseInfo m_pendingRelease;
    ReleaseInfo m_availableRelease;
    bool m_hasPendingUpdate = false;
    bool m_downloading = false;
    CheckUiMode m_checkUiMode = CheckUiMode::Interactive;
    std::function<bool()> m_installPreflight;
    std::unique_ptr<QTemporaryDir> m_downloadDirectory;
};
