#include "app/UpdateChecker.h"

#include "PipbongVersion.h"

#include <nlohmann/json.hpp>

#include <QCoreApplication>
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

UpdateChecker::UpdateChecker(QWidget* parentWidget)
    : QObject(parentWidget)
    , m_parentWidget(parentWidget)
    , m_network(new QNetworkAccessManager(this)) {}

void UpdateChecker::checkForUpdates(CheckUiMode mode) {
    m_checkUiMode = mode;
    if (m_activeReply) {
        m_activeReply->abort();
        m_activeReply->deleteLater();
        m_activeReply = nullptr;
    }

    QNetworkReply* reply = m_network->get(makeGitHubRequest(QUrl(githubLatestReleaseApiUrl())));
    m_activeReply = reply;
    connect(reply, &QNetworkReply::finished, this, &UpdateChecker::onCheckReplyFinished);
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
                release.assetSizeBytes = asset.value("size", 0);
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

    emit checkFinished(success, updateAvailable, errorMessage);

    if (m_checkUiMode == CheckUiMode::Silent || !m_parentWidget) {
        return;
    }

    if (!success) {
        QMessageBox::warning(m_parentWidget, tr("업데이트"), errorMessage);
        return;
    }

    if (!updateAvailable) {
        QMessageBox::information(
            m_parentWidget,
            tr("업데이트"),
            tr("현재 최신 버전입니다. (v%1)").arg(QCoreApplication::applicationVersion()));
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
    if (!QFileInfo::exists(updaterExecutablePath())) {
        QMessageBox::warning(
            m_parentWidget,
            tr("업데이트"),
            tr("업데이터(%1)를 찾을 수 없습니다.\n"
               "프로그램 폴더에 PIPBONGUpdater.exe가 있는지 확인하거나,\n"
               "GitHub 릴리즈에서 PIPBONG-win64.zip을 수동으로 받아 주세요.")
                .arg(updaterExecutablePath()));
        return;
    }

    const QString tempDir = QStandardPaths::writableLocation(QStandardPaths::TempLocation);
    m_pendingDownloadPath =
        QDir(tempDir).filePath(QStringLiteral("PIPBONG-win64.zip"));
    QFile::remove(m_pendingDownloadPath);
    m_pendingRelease = release;

    if (m_activeReply) {
        m_activeReply->abort();
        m_activeReply->deleteLater();
        m_activeReply = nullptr;
    }

    auto* progress = new QProgressDialog(tr("업데이트 다운로드 중…"), tr("취소"), 0, 100, m_parentWidget);
    progress->setWindowModality(Qt::WindowModal);
    progress->setMinimumDuration(0);
    progress->setValue(0);
    progress->setAttribute(Qt::WA_DeleteOnClose);

    QNetworkReply* reply = m_network->get(makeGitHubRequest(QUrl(release.downloadUrl)));
    m_activeReply = reply;

    connect(reply, &QNetworkReply::downloadProgress, this, [progress](qint64 received, qint64 total) {
        if (total > 0) {
            progress->setMaximum(static_cast<int>(total));
            progress->setValue(static_cast<int>(received));
        }
    });
    connect(progress, &QProgressDialog::canceled, reply, &QNetworkReply::abort);
    connect(reply, &QNetworkReply::finished, this, &UpdateChecker::onDownloadFinished);
}

void UpdateChecker::onDownloadFinished() {
    QNetworkReply* reply = m_activeReply;
    m_activeReply = nullptr;
    if (!reply) {
        return;
    }

    reply->deleteLater();

    if (reply->error() != QNetworkReply::NoError) {
        QMessageBox::warning(m_parentWidget,
                             tr("업데이트"),
                             tr("다운로드에 실패했습니다.\n%1").arg(reply->errorString()));
        QFile::remove(m_pendingDownloadPath);
        return;
    }

    QFile file(m_pendingDownloadPath);
    if (!file.open(QIODevice::WriteOnly)) {
        QMessageBox::warning(m_parentWidget,
                             tr("업데이트"),
                             tr("다운로드 파일을 저장하지 못했습니다.\n%1").arg(file.errorString()));
        return;
    }
    file.write(reply->readAll());
    file.close();

    const QFileInfo downloaded(m_pendingDownloadPath);
    if (!downloaded.exists() || downloaded.size() < 5 * 1024 * 1024) {
        QMessageBox::warning(m_parentWidget,
                             tr("업데이트"),
                             tr("다운로드한 파일이 올바르지 않습니다."));
        QFile::remove(m_pendingDownloadPath);
        return;
    }

    const QString installDir = QCoreApplication::applicationDirPath();
    const QString updaterExe = updaterExecutablePath();
#ifdef _WIN32
    const QString pid = QString::number(static_cast<qulonglong>(GetCurrentProcessId()));
#else
    const QString pid = QString::number(QCoreApplication::applicationPid());
#endif

    const bool started = QProcess::startDetached(
        updaterExe,
        {QStringLiteral("--install-zip"),
         QDir::toNativeSeparators(m_pendingDownloadPath),
         QDir::toNativeSeparators(installDir),
         pid});

    if (!started) {
        QMessageBox::warning(m_parentWidget,
                             tr("업데이트"),
                             tr("업데이터를 실행하지 못했습니다."));
        return;
    }

    emit readyToRestartForUpdate();
    QCoreApplication::quit();
}
