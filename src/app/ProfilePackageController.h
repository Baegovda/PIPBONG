#pragma once
#include <QObject>
#include <QFutureWatcher>
#include <QMap>
#include <QString>
#include <functional>

class QTimer;

/// Debounces and serializes package jobs. Workers capture values and the packer,
/// never this/controller/window, so destroying UI cannot leave a dangling callback.
class ProfilePackageController : public QObject {
public:
    using Packer = std::function<bool(const QString&, const QString&)>;
    explicit ProfilePackageController(QObject* parent = nullptr, Packer packer = {});
    void schedule(const QString& source, const QString& package);
    bool packNow(const QString& source, const QString& package);
    bool pauseAndWait(bool processInput = false);
    void resume();
    void discardPending();
    void discardPending(const QString& package);
    bool busy() const { return m_busy; }
    bool waitForIdle(int timeoutMs = 130000);
private:
    void dispatch(bool whilePaused = false);
    bool waitForCurrent(int timeoutMs, bool processInput = false);
    Packer m_packer;
    QTimer* m_timer;
    QFutureWatcher<bool> m_watcher;
    QMap<QString, QString> m_pending;
    bool m_busy = false;
    bool m_lastResult = true;
    int m_pauseDepth = 0;
};
