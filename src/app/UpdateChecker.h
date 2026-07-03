#pragma once

#include <QObject>
#include <QVersionNumber>

class QWidget;

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
    };

    explicit UpdateChecker(QWidget* parentWidget);

    void checkForUpdates(CheckUiMode mode = CheckUiMode::Interactive);
    void downloadAndInstall(const ReleaseInfo& release);
    void installPendingUpdate();

    bool hasPendingUpdate() const;
    ReleaseInfo pendingUpdate() const;

signals:
    void checkFinished(bool success, bool updateAvailable, const QString& errorMessage);
    void readyToRestartForUpdate();

private:
    void finishCheck(bool success,
                     const QString& errorMessage,
                     const ReleaseInfo& release,
                     bool updateAvailable);
    void onCheckReplyFinished();
    void onDownloadProgress(qint64 received, qint64 total);
    void onDownloadFinished();

    QWidget* m_parentWidget = nullptr;
    class QNetworkAccessManager* m_network = nullptr;
    class QNetworkReply* m_activeReply = nullptr;
    class QProgressDialog* m_downloadProgressDialog = nullptr;
    QString m_pendingDownloadPath;
    ReleaseInfo m_pendingRelease;
    ReleaseInfo m_availableRelease;
    bool m_hasPendingUpdate = false;
    CheckUiMode m_checkUiMode = CheckUiMode::Interactive;
};
