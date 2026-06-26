#pragma once

#include <QApplication>
#include <QMouseEvent>
#include <QPoint>
#include <QWidget>

#include <cmath>

struct DragAdjustSpinMouseState {
    bool pressPending = false;
    bool dragging = false;
    QPoint dragStartGlobalPos;
};

enum class DragAdjustReleaseResult { NotHandled, DragFinished, ClickToEdit };

inline int dragAdjustStartDragDistancePx() {
    const int distance = QApplication::startDragDistance();
    return distance > 0 ? distance : 4;
}

/// Horizontal pixels required before one drag step is applied (higher = lower sensitivity).
inline int dragAdjustHorizontalPixelsPerStep() {
    return 4;
}

inline bool dragAdjustBeginPress(DragAdjustSpinMouseState& state, QMouseEvent* event) {
    if (event->button() != Qt::LeftButton) {
        return false;
    }
    state.pressPending = true;
    state.dragging = false;
    state.dragStartGlobalPos = event->globalPosition().toPoint();
    event->accept();
    return true;
}

inline bool dragAdjustContinueMove(DragAdjustSpinMouseState& state, QMouseEvent* event, QWidget* grabWidget) {
    if (!state.pressPending && !state.dragging) {
        return false;
    }
    if (!event->buttons().testFlag(Qt::LeftButton)) {
        return false;
    }

    const int deltaPx = event->globalPosition().toPoint().x() - state.dragStartGlobalPos.x();
    if (state.pressPending && !state.dragging) {
        if (std::abs(deltaPx) < dragAdjustStartDragDistancePx()) {
            return true;
        }
        state.dragging = true;
        state.pressPending = false;
        if (grabWidget) {
            grabWidget->grabMouse();
        }
    }

    if (state.dragging) {
        event->accept();
        return true;
    }
    return false;
}

inline int dragAdjustDeltaPx(const DragAdjustSpinMouseState& state, QMouseEvent* event) {
    const int rawDelta = event->globalPosition().toPoint().x() - state.dragStartGlobalPos.x();
    return rawDelta / dragAdjustHorizontalPixelsPerStep();
}

inline DragAdjustReleaseResult dragAdjustFinishRelease(DragAdjustSpinMouseState& state,
                                                       QMouseEvent* event,
                                                       QWidget* grabWidget) {
    if (event->button() != Qt::LeftButton) {
        return DragAdjustReleaseResult::NotHandled;
    }

    if (state.dragging) {
        if (grabWidget) {
            grabWidget->releaseMouse();
        }
        state.dragging = false;
        state.pressPending = false;
        event->accept();
        return DragAdjustReleaseResult::DragFinished;
    }

    if (state.pressPending) {
        state.pressPending = false;
        event->accept();
        return DragAdjustReleaseResult::ClickToEdit;
    }

    return DragAdjustReleaseResult::NotHandled;
}
