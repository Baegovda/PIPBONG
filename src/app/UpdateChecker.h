#pragma once

#include <QObject>
#include <QVersionNumber>

class QWidget;

class UpdateChecker : public QObject {
    Q_OBJECT
public:
    struct ReleaseInfo {
        QVersionNumber version;
        QString downloadUrl;
        QString releaseNotes;
        qint64 assetSizeBytes = 0;
    };

    explicit UpdateChecker(QWidget* parentWidget);

    void checkForUpdates();
    void downloadAndInstall(const ReleaseInfo& release);

signals:
    void readyToRestartForUpdate();

private:
    void finishCheck(bool success, const QString& errorMessage, const ReleaseInfo& release, bool updateAvailable);
    void onCheckReplyFinished();
    void onDownloadProgress(qint64 received, qint64 total);
    void onDownloadFinished();

    QWidget* m_parentWidget = nullptr;
    class QNetworkAccessManager* m_network = nullptr;
    class QNetworkReply* m_activeReply = nullptr;
    QString m_pendingDownloadPath;
    ReleaseInfo m_pendingRelease;
};
