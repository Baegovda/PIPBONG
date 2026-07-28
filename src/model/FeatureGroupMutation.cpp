#include "model/FeatureGroupMutation.h"

#include <algorithm>

namespace FeatureGroupMutation {

static std::vector<int> sortedUniqueIndices(const std::vector<int>& indices) {
    std::vector<int> out = indices;
    std::sort(out.begin(), out.end());
    out.erase(std::unique(out.begin(), out.end()), out.end());
    return out;
}

static bool indexBlocked(const Project& project,
                         int index,
                         const std::set<std::string>& skipFeatureIds) {
    if (skipFeatureIds.empty()) {
        return false;
    }
    const Feature* feature = project.featureAt(index);
    if (!feature) {
        return false;
    }
    return skipFeatureIds.count(feature->id()) != 0;
}

std::vector<int> indicesInGroup(const Project& project, const std::string& groupId) {
    std::vector<int> indices;
    if (groupId.empty()) {
        return indices;
    }
    for (int i = 0; i < static_cast<int>(project.features().size()); ++i) {
        const Feature* feature = project.featureAt(i);
        if (feature && feature->groupId() == groupId) {
            indices.push_back(i);
        }
    }
    return indices;
}

bool isGroupContiguous(const Project& project, const std::string& groupId) {
    std::vector<int> indices = indicesInGroup(project, groupId);
    if (indices.size() <= 1) {
        return true;
    }
    std::sort(indices.begin(), indices.end());
    for (size_t i = 1; i < indices.size(); ++i) {
        if (indices[i] != indices[i - 1] + 1) {
            return false;
        }
    }
    return true;
}

bool consolidateGroup(Project& project,
                      const std::string& groupId,
                      const std::set<std::string>& skipFeatureIds) {
    if (groupId.empty()) {
        return false;
    }
    std::vector<int> indices = indicesInGroup(project, groupId);
    if (indices.size() <= 1) {
        return false;
    }
  for (int index : indices) {
        if (indexBlocked(project, index, skipFeatureIds)) {
            return false;
        }
    }
    std::sort(indices.begin(), indices.end());
    if (isGroupContiguous(project, groupId)) {
        return false;
    }
    project.moveFeatures(indices, indices.front());
    return true;
}

bool consolidateAll(Project& project, const std::set<std::string>& skipFeatureIds) {
    bool changed = false;
    for (int pass = 0; pass < 8; ++pass) {
        bool passChanged = false;
        for (const FeatureGroup& group : project.featureGroups()) {
            if (consolidateGroup(project, group.id(), skipFeatureIds)) {
                passChanged = true;
                changed = true;
            }
        }
        if (!passChanged) {
            break;
        }
    }
    return changed;
}

void pruneEmptyGroups(Project& project) {
    std::vector<std::string> emptyIds;
    for (const FeatureGroup& group : project.featureGroups()) {
        if (indicesInGroup(project, group.id()).empty()) {
            emptyIds.push_back(group.id());
        }
    }
    for (const std::string& id : emptyIds) {
        project.removeFeatureGroup(id);
    }
}

void repairOrphanGroupIds(Project& project) {
    for (auto& feature : project.features()) {
        if (!feature) {
            continue;
        }
        const std::string& gid = feature->groupId();
        if (gid.empty()) {
            continue;
        }
        if (!project.featureGroupById(gid)) {
            feature->setGroupId({});
        }
    }
}

bool repairOnLoad(Project& project) {
    bool changed = false;
    repairOrphanGroupIds(project);
    if (consolidateAll(project)) {
        changed = true;
    }
    pruneEmptyGroups(project);
    return changed;
}

bool assignToGroup(Project& project, const std::vector<int>& featureIndices, const std::string& groupId) {
    if (groupId.empty() || featureIndices.empty() || !project.featureGroupById(groupId)) {
        return false;
    }
    const std::vector<int> sorted = sortedUniqueIndices(featureIndices);
    for (int index : sorted) {
        Feature* feature = project.featureAt(index);
        if (!feature) {
            return false;
        }
        feature->setGroupId(groupId);
    }
    consolidateGroup(project, groupId);
    pruneEmptyGroups(project);
    return true;
}

bool removeFromGroup(Project& project, const std::vector<int>& featureIndices) {
    const std::vector<int> sorted = sortedUniqueIndices(featureIndices);
    if (sorted.empty()) {
        return false;
    }
    std::set<std::string> touchedGroups;
    for (int index : sorted) {
        Feature* feature = project.featureAt(index);
        if (!feature || feature->groupId().empty()) {
            continue;
        }
        touchedGroups.insert(feature->groupId());
        feature->setGroupId({});
    }
    for (const std::string& gid : touchedGroups) {
        consolidateGroup(project, gid);
    }
    pruneEmptyGroups(project);
    return true;
}

std::string createGroup(Project& project,
                        const std::string& name,
                        const std::vector<int>& memberIndices) {
    FeatureGroup* group = project.addFeatureGroup(name);
    if (!group) {
        return {};
    }
    if (!memberIndices.empty()) {
        assignToGroup(project, memberIndices, group->id());
    }
    return group->id();
}

bool deleteGroup(Project& project, const std::string& groupId) {
    if (groupId.empty() || !project.featureGroupById(groupId)) {
        return false;
    }
    project.removeFeatureGroup(groupId);
    pruneEmptyGroups(project);
    return true;
}

bool renameGroup(Project& project, const std::string& groupId, const std::string& newName) {
    FeatureGroup* group = project.featureGroupById(groupId);
    if (!group || newName.empty()) {
        return false;
    }
    group->setName(newName);
    return true;
}

void clearIsolatedMembership(Project& project, const std::vector<int>& featureIndices) {
    for (int featureIndex : featureIndices) {
        if (featureIndex < 0 || featureIndex >= static_cast<int>(project.features().size())) {
            continue;
        }
        Feature* feature = project.featureAt(featureIndex);
        if (!feature || feature->groupId().empty()) {
            continue;
        }
        const std::string& groupId = feature->groupId();
        bool adjacentSibling = false;
        if (featureIndex > 0) {
            Feature* prev = project.featureAt(featureIndex - 1);
            if (prev && prev->groupId() == groupId) {
                adjacentSibling = true;
            }
        }
        if (!adjacentSibling && featureIndex + 1 < static_cast<int>(project.features().size())) {
            Feature* next = project.featureAt(featureIndex + 1);
            if (next && next->groupId() == groupId) {
                adjacentSibling = true;
            }
        }
        if (!adjacentSibling) {
            feature->setGroupId({});
        }
    }
}

} // namespace FeatureGroupMutation
