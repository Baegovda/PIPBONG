#pragma once

#include <QObject>
#include <QPointer>
#include <functional>

class UpdateChecker;
class QPushButton;
class QWidget;
class QTimer;

/// Update scheduling, button state and install deferral belong here. The host
/// supplies session operations; no MainWindow dependency or friend access.
class UpdateUiController : public QObject {
public:
    struct HostCallbacks {
        std::function<void()> stopSessions;
        std::function<bool()> hasActiveEngine;
        std::function<bool()> autoInstallEnabled;
        std::function<void(const QString&, int)> showStatus;
    };
    UpdateUiController(UpdateChecker* checker, QPushButton* button, QWidget* window,
                       HostCallbacks host, QObject* parent = nullptr);
    void applySettings(int intervalMinutes);
    void onWindowShown();
    void checkInteractively();
    void onButtonClicked();
    void maybeStartAutomaticUpdate();
    void stop();
    bool installInProgress() const { return m_installStarted; }

private:
    void refreshButton();
    void silentCheck();
    void checkFinished(bool success, bool updateAvailable, const QString& error);
    QPointer<UpdateChecker> m_checker;
    QPointer<QPushButton> m_button;
    QPointer<QWidget> m_window;
    QTimer* m_timer = nullptr;
    HostCallbacks m_host;
    bool m_initialCheckDone = false;
    bool m_silent = false;
    bool m_deferred = false;
    bool m_installStarted = false;
    bool m_stopped = false;
};
