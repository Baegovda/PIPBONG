#include "model/FeatureGroupMutation.h"
#include "model/Feature.h"
#include "model/Project.h"

#include <cstdlib>
#include <iostream>
#include <string>

namespace {

int gFailures = 0;

void expect(bool ok, const char* label) {
    if (!ok) {
        ++gFailures;
        std::cerr << "FAIL: " << label << '\n';
    }
}

void scenarioAssignConsolidates() {
    Project project;
    project.addFeature("A");
    project.addFeature("B");
    project.addFeature("C");
    const std::string gid = FeatureGroupMutation::createGroup(project, "G", {0, 2});
    expect(!gid.empty(), "createGroup");
    expect(FeatureGroupMutation::isGroupContiguous(project, gid), "assign contiguous");
    const auto indices = FeatureGroupMutation::indicesInGroup(project, gid);
    expect(indices.size() == 2, "two members");
}

void scenarioRepairOnLoad() {
    Project project;
    project.addFeature("A");
    project.addFeature("B");
    FeatureGroup* g = project.addFeatureGroup("G");
    project.featureAt(0)->setGroupId(g->id());
    project.featureAt(1)->setGroupId(g->id());
    FeatureGroupMutation::repairOnLoad(project);
    expect(FeatureGroupMutation::isGroupContiguous(project, g->id()), "repairOnLoad contiguous");
}

void scenarioOrphanClears() {
    Project project;
    project.addFeature("A");
    project.featureAt(0)->setGroupId("orphan-id");
    FeatureGroupMutation::repairOrphanGroupIds(project);
    expect(project.featureAt(0)->groupId().empty(), "orphan cleared");
}

void scenarioPartialConsolidateSkipsRunningMember() {
    Project project;
    project.addFeature("A");
    project.addFeature("B");
    project.addFeature("X");
    project.addFeature("C");
    FeatureGroup* g = project.addFeatureGroup("G");
    const std::string gid = g->id();
    project.featureAt(0)->setGroupId(gid);
    project.featureAt(1)->setGroupId(gid);
    project.featureAt(3)->setGroupId(gid);
    const std::string blockedId = project.featureAt(1)->id();
    std::set<std::string> skip{blockedId};
    expect(FeatureGroupMutation::consolidateGroup(project, gid, skip), "partial consolidate");
    expect(FeatureGroupMutation::isGroupContiguous(project, gid), "movable members contiguous around skip");
}

void scenarioAfterReorderById() {
    Project project;
    project.addFeature("A");
    project.addFeature("B");
    project.addFeature("C");
    const std::string gid = FeatureGroupMutation::createGroup(project, "G", {0, 2});
    const std::string cId = project.featureAt(1)->id();
    project.moveFeature(1, 2);
    FeatureGroupMutation::afterFeaturesReordered(project, {cId}, {});
    expect(FeatureGroupMutation::isGroupContiguous(project, gid), "after reorder contiguous");
}

} // namespace

int main() {
    scenarioAssignConsolidates();
    scenarioRepairOnLoad();
    scenarioOrphanClears();
    scenarioPartialConsolidateSkipsRunningMember();
    scenarioAfterReorderById();
    if (gFailures != 0) {
        std::cerr << "FeatureGroupLayoutSim: " << gFailures << " failure(s)\n";
        return 1;
    }
    std::cout << "FeatureGroupLayoutSim: OK\n";
    return 0;
}
