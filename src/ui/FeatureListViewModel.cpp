#include "ui/FeatureListViewModel.h"

#include "model/FeatureGroupMutation.h"

std::vector<FeatureListViewRow> FeatureListViewModel::buildRows(const Project& project,
                                                                const QSet<QString>& collapsedGroupIds) {
    std::vector<FeatureListViewRow> rows;
    const int featureCount = static_cast<int>(project.features().size());
    for (int featureIndex = 0; featureIndex < featureCount; ++featureIndex) {
        const Feature* feature = project.featureAt(featureIndex);
        if (!feature) {
            continue;
        }
        const std::string& groupId = feature->groupId();
        if (!groupId.empty()) {
            const bool newGroupSegment =
                featureIndex == 0
                || project.features()[featureIndex - 1]->groupId() != groupId;
            if (newGroupSegment) {
                const FeatureGroup* group = project.featureGroupById(groupId);
                if (group) {
                    FeatureListViewRow header;
                    header.kind = FeatureListRowKind::Group;
                    header.groupId = QString::fromStdString(groupId);
                    header.groupMemberCount =
                        static_cast<int>(FeatureGroupMutation::indicesInGroup(project, groupId).size());
                    rows.push_back(header);
                }
            }
            if (collapsedGroupIds.contains(QString::fromStdString(groupId))) {
                continue;
            }
        }

        FeatureListViewRow row;
        row.kind = FeatureListRowKind::Feature;
        row.featureIndex = featureIndex;
        rows.push_back(row);
    }
    return rows;
}
