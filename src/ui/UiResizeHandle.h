#pragma once

#include <QSplitter>
#include <QtGlobal>

// Shared grab zones for column dividers, splitter bars, frameless window edges,
// and list/table row-height resize (header bottom drag).
// Horizontal dividers: ±kDividerHalfWidthPx on each side of the divider line.
namespace UiResizeHandle {

constexpr int kDividerHalfWidthPx = 10;
constexpr int kDividerHalfHeightPx = 10;
constexpr int kSplitterHandleWidthPx = 12;
constexpr int kWindowResizeBorderPx = 10;

// Shared list/table row-height drag (feature list header + workflow block list header).
constexpr int kMinListRowHeightPx = 20;
constexpr int kMaxListRowHeightPx = 64;
constexpr int kDefaultFeatureListRowHeightPx = 26;
constexpr int kDefaultBlockListRowHeightPx = 36;

/// Program-wide splitter policy: wide grab handle, panes not collapsible to zero.
inline void configureSplitter(QSplitter* splitter) {
    if (!splitter) {
        return;
    }
    splitter->setHandleWidth(kSplitterHandleWidthPx);
    splitter->setChildrenCollapsible(false);
}

inline bool isWithinHorizontalGrab(int pos, int dividerPos) {
    return qAbs(pos - dividerPos) <= kDividerHalfWidthPx;
}

inline bool isWithinBottomGrab(int posY, int widgetHeight) {
    return posY >= widgetHeight - kDividerHalfHeightPx;
}

inline int clampListRowHeight(int rowHeight) {
    return qBound(kMinListRowHeightPx, rowHeight, kMaxListRowHeightPx);
}

inline int nearestHorizontalDivider(const int* dividerPositions, int dividerCount, int pos) {
    int bestIndex = -1;
    int bestDistance = kDividerHalfWidthPx + 1;
    for (int i = 0; i < dividerCount; ++i) {
        const int distance = qAbs(pos - dividerPositions[i]);
        if (distance <= kDividerHalfWidthPx && distance < bestDistance) {
            bestDistance = distance;
            bestIndex = i;
        }
    }
    return bestIndex;
}

} // namespace UiResizeHandle
