#include "app/UpdateUiController.h"
#include "app/UpdateChecker.h"
#include <QCoreApplication>
#include <QMessageBox>
#include <QPushButton>
#include <QTimer>
#include <algorithm>
#include <limits>
#include <utility>

UpdateUiController::UpdateUiController(UpdateChecker* checker, QPushButton* button, QWidget* window,
                                       HostCallbacks host, QObject* parent)
    : QObject(parent), m_checker(checker), m_button(button), m_window(window),
      m_timer(new QTimer(this)), m_host(std::move(host)) {
    connect(m_timer, &QTimer::timeout, this, [this] { silentCheck(); });
    connect(checker, &UpdateChecker::checkFinished, this,
        [this](bool success, bool available, const QString& error) { checkFinished(success, available, error); });
    connect(checker, &UpdateChecker::installFailed, this, [this] {
        m_installStarted = false;
        m_deferred = false; // Require a new check/settings change, not a retry loop.
        refreshButton();
    });
    connect(checker, &UpdateChecker::installStarted, this, [this] {
        m_installStarted = true;
        refreshButton();
    });
    refreshButton();
}

void UpdateUiController::applySettings(int minutes) {
    if (m_stopped) return;
    if (minutes <= 0) m_timer->stop();
    else {
        const qint64 interval = std::min(qint64(minutes) * 60000, qint64(std::numeric_limits<int>::max()));
        m_timer->start(static_cast<int>(interval));
    }
    if (m_checker && m_checker->hasPendingUpdate() && m_host.autoInstallEnabled()) {
        m_deferred = true;
        maybeStartAutomaticUpdate();
    }
}

void UpdateUiController::onWindowShown() {
    if (m_initialCheckDone || m_stopped) return;
    m_initialCheckDone = true;
    QTimer::singleShot(0, this, [this] { silentCheck(); });
}

void UpdateUiController::stop() {
    m_stopped = true;
    m_timer->stop();
}

void UpdateUiController::refreshButton() {
    if (!m_button || !m_checker) return;
    if (m_checker->hasPendingUpdate()) {
        const QString version = m_checker->pendingUpdate().version.toString();
        m_button->setText(tr("v%1 버전으로 업데이트").arg(version));
        m_button->setToolTip(tr("GitHub 릴리즈 v%1을(를) 다운로드해 설치합니다.").arg(version));
    } else {
        m_button->setText(tr("v%1 · 업데이트 확인").arg(QCoreApplication::applicationVersion()));
        m_button->setToolTip(tr("클릭하면 GitHub 릴리즈에서 업데이트를 다시 확인합니다."));
    }
    m_button->setEnabled(!m_installStarted);
}

void UpdateUiController::silentCheck() {
    if (m_stopped || !m_checker || m_installStarted || qEnvironmentVariableIntValue("PIPBONG_SMOKE_TEST") == 1) return;
    m_silent = true;
    m_checker->checkForUpdates(UpdateChecker::CheckUiMode::Silent);
}

void UpdateUiController::checkFinished(bool success, bool available, const QString& error) {
    refreshButton();
    if (!success && !error.isEmpty() && m_button) m_button->setToolTip(error);
    const bool silent = std::exchange(m_silent, false);
    if (success && !available && !silent) {
        m_host.showStatus(tr("현재 최신 버전입니다. (v%1)").arg(QCoreApplication::applicationVersion()), 3000);
    }
    if (success && available && silent && m_host.autoInstallEnabled()) {
        m_deferred = true;
        maybeStartAutomaticUpdate();
    }
}

void UpdateUiController::checkInteractively() {
    if (m_stopped || !m_checker || m_installStarted) return;
    m_host.stopSessions();
    m_silent = false;
    m_checker->checkForUpdates(UpdateChecker::CheckUiMode::Interactive);
}

void UpdateUiController::onButtonClicked() {
    if (m_stopped || !m_checker || m_installStarted) return;
    if (!m_checker->hasPendingUpdate()) { checkInteractively(); return; }
    m_host.stopSessions();
    if (!m_window) return;
    const auto reply = QMessageBox::question(m_window, tr("업데이트"),
        tr("v%1(으)로 업데이트하시겠습니까?\n현재 버전: v%2")
            .arg(m_checker->pendingUpdate().version.toString(), QCoreApplication::applicationVersion()),
        QMessageBox::Yes | QMessageBox::No, QMessageBox::Yes);
    if (reply == QMessageBox::Yes) {
        m_installStarted = true;
        refreshButton();
        m_checker->installPendingUpdate();
    }
}

void UpdateUiController::maybeStartAutomaticUpdate() {
    if (m_stopped || m_installStarted || !m_deferred || !m_checker
        || !m_checker->hasPendingUpdate() || !m_host.autoInstallEnabled()) return;
    m_host.stopSessions();
    if (m_host.hasActiveEngine()) return;
    m_deferred = false;
    m_installStarted = true;
    refreshButton();
    m_host.showStatus(tr("v%1 자동 업데이트를 시작합니다.")
        .arg(m_checker->pendingUpdate().version.toString()), 5000);
    m_checker->installPendingUpdate();
}
