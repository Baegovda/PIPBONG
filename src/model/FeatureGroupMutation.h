#pragma once

#include "model/Project.h"

#include <set>
#include <string>
#include <vector>

/// Pure project-level feature-group mutations. UI must call these instead of ad-hoc setGroupId + repair.
namespace FeatureGroupMutation {

std::vector<int> indicesInGroup(const Project& project, const std::string& groupId);

bool isGroupContiguous(const Project& project, const std::string& groupId);

/// Set groupId and move all members into one contiguous block (always consolidates this group).
bool assignToGroup(Project& project, const std::vector<int>& featureIndices, const std::string& groupId);

bool removeFromGroup(Project& project, const std::vector<int>& featureIndices);

/// Creates group, assigns members, consolidates. Returns new group id or empty on failure.
std::string createGroup(Project& project,
                        const std::string& name,
                        const std::vector<int>& memberIndices);

bool deleteGroup(Project& project, const std::string& groupId);

bool renameGroup(Project& project, const std::string& groupId, const std::string& newName);

bool consolidateGroup(Project& project,
                      const std::string& groupId,
                      const std::set<std::string>& skipFeatureIds = {});

bool consolidateAll(Project& project, const std::set<std::string>& skipFeatureIds = {});

void pruneEmptyGroups(Project& project);

/// Clear groupId on features whose group record is missing.
void repairOrphanGroupIds(Project& project);

/// After JSON load: orphan repair + consolidate all + prune empty (no name-based merge).
bool repairOnLoad(Project& project);

/// Remove groupId when feature has no adjacent sibling in the same group (project index adjacency).
void clearIsolatedMembership(Project& project, const std::vector<int>& featureIndices);

} // namespace FeatureGroupMutation
