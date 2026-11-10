#include "core/diagnostics/GuiContextCache.h"
#include <QCoreApplication>
#include <QThread>
namespace {
bool guiThread(){const auto* app=QCoreApplication::instance();return app && QThread::currentThread()==app->thread();}
}
quint64 GuiContextCache::set(Callback callback) {
    if(!guiThread())return 0;
    auto entry=std::make_shared<Entry>();entry->callback=std::move(callback);
    std::shared_ptr<Entry> retired;
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        if(m_closed)return 0;
        entry->token=++m_next;if(!entry->token)entry->token=++m_next;
        retired=std::move(m_entry);m_entry=entry;
    }
    retired.reset();
    refresh();return entry->token;
}
bool GuiContextCache::retire(quint64 token) {
    std::shared_ptr<Entry> retired;
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        if(!token || !m_entry || m_entry->token!=token)return false;
        retired=std::move(m_entry);
    }
    return true; // Keep the last complete snapshot for teardown crashes.
}
GuiContextCache::~GuiContextCache() {
    std::shared_ptr<Entry> retired;
    {std::lock_guard<std::mutex> lock(m_mutex);m_closed=true;retired=std::move(m_entry);}
}
bool GuiContextCache::refresh() {
    if(!guiThread())return false;
    std::shared_ptr<Entry> entry;
    {
        std::lock_guard<std::mutex> lock(m_mutex);entry=m_entry;
        if(!entry || !entry->callback || entry->refreshing)return false;
        entry->refreshing=true;
    }
    QString snapshot;bool success=true;
    try {snapshot=entry->callback();}catch(...){success=false;}
    std::lock_guard<std::mutex> lock(m_mutex);entry->refreshing=false;
    if(!success || m_entry!=entry)return false;
    m_cached=std::move(snapshot);return true;
}
QString GuiContextCache::cached()const {std::lock_guard<std::mutex> lock(m_mutex);return m_cached;}
