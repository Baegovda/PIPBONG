#pragma once

#include <QtGlobal>

// Shared grab zones for column dividers, splitter bars, and frameless window edges.
// Horizontal dividers: ±kDividerHalfWidthPx on each side of the divider line.
namespace UiResizeHandle {

constexpr int kDividerHalfWidthPx = 10;
constexpr int kDividerHalfHeightPx = 10;
constexpr int kSplitterHandleWidthPx = 12;
constexpr int kWindowResizeBorderPx = 10;

inline bool isWithinHorizontalGrab(int pos, int dividerPos) {
    return qAbs(pos - dividerPos) <= kDividerHalfWidthPx;
}

inline bool isWithinBottomGrab(int posY, int widgetHeight) {
    return posY >= widgetHeight - kDividerHalfHeightPx;
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
