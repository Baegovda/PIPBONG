#pragma once
#include <QString>
#include <functional>
#include <memory>
#include <mutex>

// GUI callbacks execute outside the cache mutex. Background crash/watchdog
// readers only copy the last successfully published value. The cache outlives
// refresh calls; a callback may replace/retire a provider or delete its GUI owner.
class GuiContextCache {
public:
    ~GuiContextCache();
    using Callback=std::function<QString()>;
    quint64 set(Callback);
    bool retire(quint64 token);
    bool refresh();
    QString cached()const;
private:
    struct Entry {quint64 token=0;Callback callback;bool refreshing=false;};
    mutable std::mutex m_mutex;
    std::shared_ptr<Entry> m_entry;
    quint64 m_next=0;
    QString m_cached;
    bool m_closed=false;
};
