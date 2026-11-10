#include "app/UpdateChecker.h"

#include "PipbongVersion.h"

#include <nlohmann/json.hpp>

#include <QCoreApplication>
#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QMessageBox>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QProcess>
#include <QProgressDialog>
#include <QStandardPaths>
#include <QTimer>
#include <QUrl>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#endif

namespace {

QString githubLatestReleaseApiUrl() {
    return QStringLiteral("https://api.github.com/repos/%1/releases/latest")
        .arg(QStringLiteral(PIPBONG_UPDATE_GITHUB_REPO));
}

QNetworkRequest makeGitHubRequest(const QUrl& url) {
    QNetworkRequest request(url);
    request.setHeader(QNetworkRequest::UserAgentHeader,
                      QStringLiteral("PIPBONG/%1").arg(QCoreApplication::applicationVersion()));
    request.setRawHeader("Accept", "application/vnd.github+json");
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                         QNetworkRequest::NoLessSafeRedirectPolicy);
    return request;
}

QVersionNumber parseReleaseVersion(const QString& tagName) {
    QString normalized = tagName.trimmed();
    if (normalized.startsWith(QLatin1Char('v'), Qt::CaseInsensitive)) {
        normalized = normalized.mid(1);
    }
    return QVersionNumber::fromString(normalized);
}

QString updaterExecutablePath() {
    return QDir(QCoreApplication::applicationDirPath())
        .filePath(QStringLiteral("PIPBONGUpdater.exe"));
}

} // namespace

UpdateChecker::UpdateChecker(QWidget* parentWidget, QNetworkAccessManager* network)
    : QObject(parentWidget)
    , m_parentWidget(parentWidget)
    , m_network(network ? network : new QNetworkAccessManager(this)) {}

bool UpdateChecker::verifyDownload(const ReleaseInfo& release, const QByteArray& payload) {
    return release.assetSizeBytes > 0 && payload.size() == release.assetSizeBytes
        && release.assetSha256.size() == 64
        && QCryptographicHash::hash(payload, QCryptographicHash::Sha256).toHex()
            == release.assetSha256.toLatin1();
}

void UpdateChecker::checkForUpdates(CheckUiMode mode) {
    if (m_downloading) return;
    cancelActiveReply();
    m_checkUiMode = mode;

    QNetworkReply* reply = m_network->get(makeGitHubRequest(QUrl(githubLatestReleaseApiUrl())));
    m_activeReply = reply;
    connect(reply, &QNetworkReply::finished, this, [this, reply] {
        if (m_activeReply == reply) onCheckReplyFinished();
    });
}

void UpdateChecker::cancelActiveReply() {
    QNetworkReply* old = m_activeReply;
    m_activeReply = nullptr;
    if (!old) return;
    disconnect(old, nullptr, this, nullptr);
    old->abort();
    old->deleteLater();
}

void UpdateChecker::failInstall(const QString& message) {
    m_downloading = false;
    m_downloadDirectory.reset();
    if (m_downloadProgressDialog) m_downloadProgressDialog->close();
    emit installFailed();
    if (m_parentWidget) QMessageBox::warning(m_parentWidget, tr("업데이트"), message);
}

bool UpdateChecker::hasPendingUpdate() const {
    return m_hasPendingUpdate;
}

UpdateChecker::ReleaseInfo UpdateChecker::pendingUpdate() const {
    return m_availableRelease;
}

void UpdateChecker::installPendingUpdate() {
    if (!m_hasPendingUpdate) {
        return;
    }
    downloadAndInstall(m_availableRelease);
}

void UpdateChecker::onCheckReplyFinished() {
    QNetworkReply* reply = m_activeReply;
    m_activeReply = nullptr;
    if (!reply) {
        return;
    }

    reply->deleteLater();

    if (reply->error() != QNetworkReply::NoError) {
        finishCheck(false,
                    tr("업데이트 정보를 가져오지 못했습니다.\n%1").arg(reply->errorString()),
                    {},
                    false);
        return;
    }

    ReleaseInfo release;
    bool updateAvailable = false;
    try {
        const auto document = nlohmann::json::parse(reply->readAll().constData());
        const QString tagName = QString::fromStdString(document.value("tag_name", std::string()));
        release.version = parseReleaseVersion(tagName);
        release.releaseNotes = QString::fromStdString(document.value("body", std::string())).trimmed();

        const auto assets = document.find("assets");
        if (assets != document.end() && assets->is_array()) {
            for (const auto& asset : *assets) {
                const QString name = QString::fromStdString(asset.value("name", std::string()));
                if (name.compare(QStringLiteral(PIPBONG_UPDATE_ASSET_NAME), Qt::CaseInsensitive) != 0) {
                    continue;
                }
                release.downloadUrl =
                    QString::fromStdString(asset.value("browser_download_url", std::string()));
                release.assetSizeBytes = asset.value("size", qint64{0});
                const QString digest = QString::fromStdString(asset.value("digest", std::string()));
                if (digest.startsWith(QStringLiteral("sha256:"))) {
                    release.assetSha256 = digest.mid(7).toLower();
                }
                break;
            }
        }

        if (release.downloadUrl.isEmpty() || release.version.isNull()) {
            finishCheck(false, tr("릴리즈에 %1 파일이 없습니다.").arg(QStringLiteral(PIPBONG_UPDATE_ASSET_NAME)), {}, false);
            return;
        }

        const QVersionNumber current = QVersionNumber::fromString(QCoreApplication::applicationVersion());
        updateAvailable = QVersionNumber::compare(release.version, current) > 0;
        finishCheck(true, {}, release, updateAvailable);
    } catch (const std::exception& exception) {
        finishCheck(false,
                    tr("업데이트 정보를 해석하지 못했습니다.\n%1").arg(QString::fromUtf8(exception.what())),
                    {},
                    false);
    }
}

void UpdateChecker::finishCheck(bool success,
                                const QString& errorMessage,
                                const ReleaseInfo& release,
                                bool updateAvailable) {
    if (success) {
        if (updateAvailable) {
            m_availableRelease = release;
            m_hasPendingUpdate = true;
        } else {
            m_hasPendingUpdate = false;
        }
    }

    const CheckUiMode completedMode = m_checkUiMode;
    emit checkFinished(success, updateAvailable, errorMessage);

    if (completedMode == CheckUiMode::Silent || !m_parentWidget) {
        return;
    }

    if (!success) {
        QMessageBox::warning(m_parentWidget, tr("업데이트"), errorMessage);
        return;
    }

    if (!updateAvailable) {
        return;
    }

    QString message = tr("새 버전 v%1이(가) 있습니다.\n현재 버전: v%2\n\n지금 업데이트하시겠습니까?")
                          .arg(release.version.toString())
                          .arg(QCoreApplication::applicationVersion());
    if (!release.releaseNotes.isEmpty()) {
        const QString clipped =
            release.releaseNotes.size() > 600 ? release.releaseNotes.left(600) + QStringLiteral("…")
                                              : release.releaseNotes;
        message += QStringLiteral("\n\n") + clipped;
    }

    const auto reply = QMessageBox::question(m_parentWidget,
                                             tr("업데이트"),
                                             message,
                                             QMessageBox::Yes | QMessageBox::No,
                                             QMessageBox::Yes);
    if (reply == QMessageBox::Yes) {
        downloadAndInstall(release);
    }
}

void UpdateChecker::downloadAndInstall(const ReleaseInfo& release) {
    if (m_downloading) return;
    if (!QFileInfo::exists(updaterExecutablePath())) {
        failInstall(
            tr("업데이터(%1)를 찾을 수 없습니다.\n"
               "프로그램 폴더에 PIPBONGUpdater.exe가 있는지 확인하거나,\n"
               "GitHub 릴리즈에서 PIPBONG-win64.zip을 수동으로 받아 주세요.")
                .arg(updaterExecutablePath()));
        return;
    }

    const QString tempDir = QStandardPaths::writableLocation(QStandardPaths::TempLocation);
    m_downloadDirectory = std::make_unique<QTemporaryDir>(QDir(tempDir).filePath(QStringLiteral("PIPBONG-update-XXXXXX")));
    if (!m_downloadDirectory->isValid()) {
        failInstall(tr("업데이트 임시 폴더를 만들지 못했어요."));
        return;
    }
    m_pendingDownloadPath = m_downloadDirectory->filePath(QStringLiteral("PIPBONG-win64.zip"));
    m_pendingRelease = release;

    cancelActiveReply();

    if (m_downloadProgressDialog) {
        m_downloadProgressDialog->close();
        m_downloadProgressDialog = nullptr;
    }

    m_downloading = true;
    emit installStarted();
    if (m_parentWidget) {
        auto* dialog = new QProgressDialog(tr("업데이트 다운로드 중…"), tr("취소"), 0, 100, m_parentWidget);
        m_downloadProgressDialog = dialog;
        dialog->setWindowModality(Qt::WindowModal);
        dialog->setMinimumDuration(0);
        dialog->setValue(0);
        dialog->setAttribute(Qt::WA_DeleteOnClose);
        connect(dialog, &QObject::destroyed, this, [this, dialog]() {
            if (m_downloadProgressDialog == dialog) m_downloadProgressDialog = nullptr;
        });
    }

    QNetworkReply* reply = m_network->get(makeGitHubRequest(QUrl(release.downloadUrl)));
    m_activeReply = reply;

    connect(reply, &QNetworkReply::downloadProgress, this, [this](qint64 received, qint64 total) {
        if (!m_downloadProgressDialog || total <= 0) {
            return;
        }
        m_downloadProgressDialog->setMaximum(static_cast<int>(total));
        m_downloadProgressDialog->setValue(static_cast<int>(received));
    });
    if (m_downloadProgressDialog) connect(m_downloadProgressDialog, &QProgressDialog::canceled, reply, &QNetworkReply::abort);
    connect(reply, &QNetworkReply::finished, this, [this, reply] {
        if (m_activeReply == reply) onDownloadFinished();
    });
}

void UpdateChecker::onDownloadFinished() {
    QNetworkReply* reply = m_activeReply;
    m_activeReply = nullptr;
    if (!reply) {
        return;
    }

    reply->deleteLater();

    if (reply->error() != QNetworkReply::NoError) {
        QFile::remove(m_pendingDownloadPath);
        failInstall(tr("다운로드에 실패했습니다.\n%1").arg(reply->errorString()));
        return;
    }

    QFile file(m_pendingDownloadPath);
    if (!file.open(QIODevice::WriteOnly)) {
        failInstall(tr("다운로드 파일을 저장하지 못했습니다.\n%1").arg(file.errorString()));
        return;
    }
    const QByteArray payload = reply->readAll();
    if (!verifyDownload(m_pendingRelease, payload)) {
        file.close();
        QFile::remove(m_pendingDownloadPath);
        failInstall(tr("업데이트 파일 검증에 실패했습니다. 다시 다운로드해 주세요."));
        return;
    }
    if (file.write(payload) != payload.size() || !file.flush()) {
        file.close();
        QFile::remove(m_pendingDownloadPath);
        failInstall(tr("업데이트 파일을 저장하지 못했습니다."));
        return;
    }
    file.close();

    const QFileInfo downloaded(m_pendingDownloadPath);
    if (!downloaded.exists() || downloaded.size() < 5 * 1024 * 1024) {
        QFile::remove(m_pendingDownloadPath);
        failInstall(tr("다운로드한 파일이 올바르지 않습니다."));
        return;
    }

    if (m_installPreflight && !m_installPreflight()) {
        failInstall(tr("현재 내용을 저장하지 못해 업데이트 설치를 중단했어요. 저장 위치와 여유 공간을 확인해 주세요."));
        return;
    }
    const QString installDir = QCoreApplication::applicationDirPath();
    const QString updaterExe = updaterExecutablePath();
#ifdef _WIN32
    const QString pid = QString::number(static_cast<qulonglong>(GetCurrentProcessId()));
#else
    const QString pid = QString::number(QCoreApplication::applicationPid());
#endif

    const bool started = launchUpdater(
        updaterExe,
        {QStringLiteral("--install-zip"),
         QDir::toNativeSeparators(m_pendingDownloadPath),
         QDir::toNativeSeparators(installDir),
         pid});

    if (!started) {
        failInstall(tr("업데이터를 실행하지 못했습니다."));
        return;
    }
    m_downloadDirectory->setAutoRemove(false); // Detached updater reads after app exit.

    if (m_downloadProgressDialog) {
        m_downloadProgressDialog->close();
        m_downloadProgressDialog = nullptr;
    }

    // Defer shutdown so the download dialog and network reply handlers finish cleanly.
    QTimer::singleShot(0, this, [this]() { emit readyToRestartForUpdate(); });
}

bool UpdateChecker::launchUpdater(const QString& executable, const QStringList& arguments) {
    return QProcess::startDetached(executable, arguments);
}
