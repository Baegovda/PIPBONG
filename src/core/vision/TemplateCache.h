#pragma once

#include "core/vision/ImageMatcher.h"

#include <memory>
#include <string>
#include <vector>

/// Thread-safe LRU cache for prepared OpenCV templates (path + mtime key).
/// Returns shared_ptr so callers keep templates alive across cache eviction.
class TemplateCache {
public:
    static constexpr int kMaxEntries = 64;

    static std::shared_ptr<PreparedTemplate> getOrLoad(const std::string& resolvedPath);
    static void prefetchAsync(const std::vector<std::string>& resolvedPaths);
    static void invalidate(const std::string& resolvedPath);
    static void clear();
};
