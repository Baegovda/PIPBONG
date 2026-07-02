#pragma once

#include <QtGlobal>

enum class CellBorder : quint8 {
    Top = 1 << 0,
    Right = 1 << 1,
    Bottom = 1 << 2,
    Left = 1 << 3
};

using CellBorderMask = quint8;

inline CellBorderMask cellBorderMask(CellBorder edge) {
    return static_cast<CellBorderMask>(edge);
}

inline bool hasCellBorder(CellBorderMask mask, CellBorder edge) {
    return (mask & cellBorderMask(edge)) != 0;
}

enum class SpreadsheetBorderPreset {
    None,
    All,
    Outside,
    Inside,
    Top,
    Bottom,
    Left,
    Right
};
