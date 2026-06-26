#pragma once

#include <string>

class Project;

class RunWarmup {
public:
    static void prefetch(const Project* project, const std::string& projectDirectory);
};
