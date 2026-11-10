#pragma once
#include <QString>
#include <QObject>
#include <functional>

/// Orders session quiescence, durable saves and final teardown. A failed save
/// leaves the application usable; repeated close notifications finalize once.
class ShutdownCoordinator : public QObject {
public:
    struct HostCallbacks {
        std::function<bool()> canBegin;
        std::function<void()> quiesce;
        std::function<bool()> saveProject;
        std::function<bool()> saveSettings;
        std::function<bool()> saveMemo;
        std::function<void()> finalize;
    };
    explicit ShutdownCoordinator(HostCallbacks host);
    bool checkpoint();
    bool prepare();
    /// Persist under the write gate, launch a successor, then finalize without
    /// a second fallible save. Failed/cancelled launch leaves this app usable.
    bool handoff(const std::function<bool()>& launch);
    bool busy() const { return m_busy; }
    bool ready() const { return m_ready; }
    QString lastError() const { return m_error; }
private:
    bool persist();
    HostCallbacks m_host;
    QString m_error;
    bool m_busy = false;
    bool m_ready = false;
};
