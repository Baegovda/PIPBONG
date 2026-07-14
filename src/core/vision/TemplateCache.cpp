#include "core/vision/TemplateCache.h"

#include <QFileInfo>

#include <algorithm>
#include <thread>

namespace {

struct CacheEntry {
    std::string path;
    qint64 mtime = 0;
    PreparedTemplate templ;
};

std::mutex g_mutex;
std::vector<CacheEntry> g_entries;

qint64 fileMtime(const std::string& path) {
    const QFileInfo info(QString::fromStdString(path));
    return info.exists() ? info.lastModified().toMSecsSinceEpoch() : 0;
}

int findEntryIndex(const std::string& path, qint64 mtime) {
    for (int i = 0; i < static_cast<int>(g_entries.size()); ++i) {
        if (g_entries[static_cast<size_t>(i)].path == path
            && g_entries[static_cast<size_t>(i)].mtime == mtime) {
            return i;
        }
    }
    return -1;
}

void touchEntry(int index) {
    if (index <= 0 || index >= static_cast<int>(g_entries.size())) {
        return;
    }
    CacheEntry entry = std::move(g_entries[static_cast<size_t>(index)]);
    g_entries.erase(g_entries.begin() + index);
    g_entries.insert(g_entries.begin(), std::move(entry));
}

void evictIfNeeded() {
    while (static_cast<int>(g_entries.size()) > TemplateCache::kMaxEntries) {
        g_entries.pop_back();
    }
}

} // namespace

const PreparedTemplate& TemplateCache::getOrLoad(const std::string& resolvedPath) {
    const qint64 mtime = fileMtime(resolvedPath);
    {
        std::lock_guard<std::mutex> lock(g_mutex);
        const int index = findEntryIndex(resolvedPath, mtime);
        if (index >= 0) {
            touchEntry(index);
            return g_entries.front().templ;
        }
    }

    PreparedTemplate loaded = ImageMatcher::loadTemplate(resolvedPath);
    {
        std::lock_guard<std::mutex> lock(g_mutex);
        const int staleIndex = [&]() {
            for (int i = 0; i < static_cast<int>(g_entries.size()); ++i) {
                if (g_entries[static_cast<size_t>(i)].path == resolvedPath) {
                    return i;
                }
            }
            return -1;
        }();
        if (staleIndex >= 0) {
            g_entries.erase(g_entries.begin() + staleIndex);
        }

        CacheEntry entry;
        entry.path = resolvedPath;
        entry.mtime = mtime;
        entry.templ = std::move(loaded);
        g_entries.insert(g_entries.begin(), std::move(entry));
        evictIfNeeded();
        return g_entries.front().templ;
    }
}

void TemplateCache::prefetchAsync(const std::vector<std::string>& resolvedPaths) {
    if (resolvedPaths.empty()) {
        return;
    }

    std::thread([paths = resolvedPaths]() {
        for (const std::string& path : paths) {
            if (path.empty()) {
                continue;
            }
            (void)TemplateCache::getOrLoad(path);
        }
    }).detach();
}

void TemplateCache::invalidate(const std::string& resolvedPath) {
    std::lock_guard<std::mutex> lock(g_mutex);
    g_entries.erase(std::remove_if(g_entries.begin(),
                                   g_entries.end(),
                                   [&](const CacheEntry& entry) { return entry.path == resolvedPath; }),
                    g_entries.end());
}

void TemplateCache::clear() {
    std::lock_guard<std::mutex> lock(g_mutex);
    g_entries.clear();
}
