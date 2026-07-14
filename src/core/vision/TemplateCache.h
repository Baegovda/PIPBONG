#pragma once

#include "core/vision/ImageMatcher.h"

#include <string>
#include <vector>

/// Thread-safe LRU cache for prepared OpenCV templates (path + mtime key).
class TemplateCache {
public:
    static constexpr int kMaxEntries = 64;

    static const PreparedTemplate& getOrLoad(const std::string& resolvedPath);
    static void prefetchAsync(const std::vector<std::string>& resolvedPaths);
    static void invalidate(const std::string& resolvedPath);
    static void clear();
};
