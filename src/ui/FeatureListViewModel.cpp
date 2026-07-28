#include "FeatureListViewModel.h"

#include "model/Feature.h"
#include "model/FeatureGroup.h"
#include "model/Project.h"

#include <QSet>

namespace {

const FeatureGroup* findGroup(const Project& project, const std::string& groupId) {
    if (groupId.empty()) {
        return nullptr;
    }
    for (const FeatureGroup& group : project.featureGroups()) {
        if (group.id() == groupId) {
            return &group;
        }
    }
    return nullptr;
}

int countMembersInGroup(const Project& project, const std::string& groupId) {
    int count = 0;
    for (const auto& feature : project.features()) {
        if (feature && feature->groupId() == groupId) {
            ++count;
        }
    }
    return count;
}

} // namespace

std::vector<FeatureListViewRow> FeatureListViewModel::buildRows(const Project& project,
                                                                const QSet<QString>& collapsedGroupIds) {
    std::vector<FeatureListViewRow> rows;
    if (project.features().empty()) {
        return rows;
    }
    QSet<QString> groupHeaderShown;
    for (int i = 0; i < static_cast<int>(project.features().size()); ++i) {
        const Feature* feature = project.featureAt(i);
        if (!feature) {
            continue;
        }
        const std::string groupId = feature->groupId();
        if (!groupId.empty()) {
            const QString groupIdQString = QString::fromStdString(groupId);
            if (!groupHeaderShown.contains(groupIdQString)) {
                if (findGroup(project, groupId)) {
                    FeatureListViewRow header;
                    header.kind = FeatureListRowKind::Group;
                    header.groupId = groupIdQString;
                    header.groupMemberCount = countMembersInGroup(project, groupId);
                    rows.push_back(header);
                }
                groupHeaderShown.insert(groupIdQString);
            }
            if (collapsedGroupIds.contains(groupIdQString)) {
                continue;
            }
        }

        FeatureListViewRow row;
        row.kind = FeatureListRowKind::Feature;
        row.featureIndex = i;
        rows.push_back(row);
    }
    return rows;
}
