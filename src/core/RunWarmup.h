#pragma once

#include <string>

class Project;

class RunWarmup {
public:
    static void prefetch(const Project* project, const std::string& projectDirectory);
    /// Prefetch templates for a single feature first (e.g. selected row), then the rest on a worker thread.
    static void prefetch(const Project* project,
                         const std::string& projectDirectory,
                         const std::string& priorityFeatureId);
};
