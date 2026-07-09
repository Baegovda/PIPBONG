#pragma once

#include <QPixmap>
#include <QPoint>
#include <QRect>

#include <functional>

class QDrag;
class QListWidget;
class QTableWidget;
class QWidget;

namespace ListDragVisuals {

struct LiftedPixmap {
    QPixmap pixmap;
    QPoint hotSpot;
};

QPixmap captureListRowPixmap(QListWidget* list, int row);
QPixmap captureTableRowPixmap(QTableWidget* table, int row);

LiftedPixmap makeLiftedDragPixmap(const QPixmap& rowPixmap, const QPoint& hotSpotInRow);
LiftedPixmap makeLiftedListRowDrag(QListWidget* list, int row, const QPoint& cursorGlobalPos);
LiftedPixmap makeLiftedTableRowDrag(QTableWidget* table, int row, const QPoint& cursorGlobalPos);

void applyToDrag(QDrag* drag, const LiftedPixmap& lifted);

/// Dashed empty slot left behind while the row is dragged.
void showDragSlotPlaceholder(QWidget* viewport, const QRect& rowRectInViewport, QWidget** slotOut);
void hideDragSlotPlaceholder(QWidget** slotOut);

/// Snap-in bounce after a successful reorder drop.
void playDropSettle(QWidget* viewport,
                    const QPixmap& rowPixmap,
                    const QRect& targetRectInViewport,
                    std::function<void()> onFinished = {});

} // namespace
