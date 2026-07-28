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

} // namespace

int main() {
    scenarioAssignConsolidates();
    scenarioRepairOnLoad();
    scenarioOrphanClears();
    if (gFailures != 0) {
        std::cerr << "FeatureGroupLayoutSim: " << gFailures << " failure(s)\n";
        return 1;
    }
    std::cout << "FeatureGroupLayoutSim: OK\n";
    return 0;
}
