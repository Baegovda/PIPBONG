#pragma once

#include "model/Project.h"
#include "ui/FeatureListItemRoles.h"

#include <QSet>
#include <QString>
#include <vector>

struct FeatureListViewRow {
    FeatureListRowKind kind = FeatureListRowKind::Feature;
    int featureIndex = -1;
    QString groupId;
    int groupMemberCount = 0;
};

class FeatureListViewModel {
public:
    static std::vector<FeatureListViewRow> buildRows(const Project& project,
                                                     const QSet<QString>& collapsedGroupIds);
};
