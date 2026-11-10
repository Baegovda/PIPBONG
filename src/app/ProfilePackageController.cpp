#include "app/ProfilePackageController.h"
#include "storage/ProjectPackage.h"
#include <QElapsedTimer>
#include <QEventLoop>
#include <QTimer>
#include <QPointer>
#include <QtConcurrent/QtConcurrent>
#include <utility>

ProfilePackageController::ProfilePackageController(QObject* parent, Packer packer)
    : QObject(parent), m_packer(packer ? std::move(packer) : Packer(ProjectPackage::packDirectory)),
      m_timer(new QTimer(this)) {
    m_timer->setSingleShot(true);
    m_timer->setInterval(5000);
    connect(m_timer, &QTimer::timeout, this, [this] { dispatch(); });
    connect(&m_watcher, &QFutureWatcher<bool>::finished, this, [this] {
        m_lastResult = m_watcher.result();
        m_busy = false;
        if (m_pauseDepth == 0) dispatch();
    });
}

void ProfilePackageController::schedule(const QString& source, const QString& package) {
    if (source.isEmpty() || package.isEmpty()) return;
    m_pending.insert(package, source); // Keep the latest request for each profile.
    if (m_pauseDepth == 0) m_timer->start();
}

void ProfilePackageController::dispatch(bool whilePaused) {
    if (m_busy || (!whilePaused && m_pauseDepth > 0) || m_pending.isEmpty()) return;
    const auto next = m_pending.begin();
    const QString package = next.key();
    const QString source = next.value();
    m_pending.erase(next);
    m_busy = true;
    const Packer packer = m_packer;
    m_watcher.setFuture(QtConcurrent::run([packer, source, package] { return packer(source, package); }));
}

bool ProfilePackageController::waitForCurrent(int timeoutMs, bool processInput) {
    if (!m_busy) return true;
    const QPointer<ProfilePackageController> self(this);
    QEventLoop loop;
    connect(this,&QObject::destroyed,&loop,&QEventLoop::quit);
    QTimer timeout;
    timeout.setSingleShot(true);
    connect(&timeout, &QTimer::timeout, &loop, &QEventLoop::quit);
    connect(&m_watcher, &QFutureWatcher<bool>::finished, &loop, &QEventLoop::quit);
    timeout.start(timeoutMs);
    // Timers/painting continue, while user commands cannot reenter a restore/export.
    loop.exec(processInput ? QEventLoop::AllEvents : QEventLoop::ExcludeUserInputEvents);
    return self && !self->m_busy;
}

bool ProfilePackageController::pauseAndWait(bool processInput) {
    const QPointer<ProfilePackageController> self(this);
    ++m_pauseDepth;
    m_timer->stop();
    if (waitForCurrent(130000, processInput)) return true;
    if(!self)return false;
    --m_pauseDepth;
    return false;
}

void ProfilePackageController::resume() {
    if (m_pauseDepth > 0) --m_pauseDepth;
    if (m_pauseDepth == 0 && !m_pending.isEmpty()) m_timer->start();
}

void ProfilePackageController::discardPending() { m_pending.clear(); }
void ProfilePackageController::discardPending(const QString& package) { m_pending.remove(package); }

bool ProfilePackageController::packNow(const QString& source, const QString& package) {
    const QString inputSource=source,outputPackage=package;
    const QPointer<ProfilePackageController> self(this);
    if (!pauseAndWait()) return false;
    // Replace an older pending seal for this output. No synchronous/async writers
    // can run at the same time, including during export and application shutdown.
    m_pending.insert(outputPackage,inputSource);
    bool ok = true;
    while (!m_pending.isEmpty()) {
        const auto next = m_pending.begin();
        const QString output = next.key();
        const QString input = next.value();
        m_pending.erase(next);
        const Packer packer = m_packer;
        m_busy = true;
        m_watcher.setFuture(QtConcurrent::run([packer, input, output] { return packer(input, output); }));
        if (!waitForCurrent(130000)) {if(!self)return false;ok=false;break;}
        if(!self)return false;
        ok = m_lastResult && ok;
    }
    resume();
    return ok;
}

bool ProfilePackageController::waitForIdle(int timeoutMs) {
    const QPointer<ProfilePackageController> self(this);
    QElapsedTimer elapsed; elapsed.start();
    ++m_pauseDepth;
    m_timer->stop();
    bool ok = true;
    while (m_busy || !m_pending.isEmpty()) {
        dispatch(true);
        if (!waitForCurrent(qMax(1, timeoutMs - static_cast<int>(elapsed.elapsed())))) {if(!self)return false;ok=false;break;}
        if(!self)return false;
        if (elapsed.elapsed() >= timeoutMs && (m_busy || !m_pending.isEmpty())) { ok = false; break; }
    }
    resume();
    return ok;
}
