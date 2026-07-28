#pragma once

#include <QtGlobal>

enum class FeatureListRowKind {
    Feature = 0,
    Group = 1,
};

inline constexpr int kFeatureListRowKindRole = Qt::UserRole + 20;
inline constexpr int kFeatureListFeatureIndexRole = Qt::UserRole + 21;
inline constexpr int kFeatureListGroupIdRole = Qt::UserRole + 22;
inline constexpr int kFeatureListGroupMemberCountRole = Qt::UserRole + 23;

inline constexpr int kFeatureListGroupChevronWidthPx = 24;
inline constexpr int kFeatureListGroupIndentPx = 12;
