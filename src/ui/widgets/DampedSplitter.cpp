#include "ui/widgets/DampedSplitter.h"

#include "ui/UiResizeHandle.h"

#include <QMouseEvent>
#include <QtGlobal>

namespace UiResizeHandle {

DampedSplitterHandle::DampedSplitterHandle(Qt::Orientation orientation, QSplitter* parent)
    : QSplitterHandle(orientation, parent) {}

void DampedSplitterHandle::mousePressEvent(QMouseEvent* event) {
    if (event->button() == Qt::LeftButton) {
        m_dragAnchor = orientation() == Qt::Horizontal ? event->globalPosition().toPoint().x()
                                                       : event->globalPosition().toPoint().y();
    }
    QSplitterHandle::mousePressEvent(event);
}

void DampedSplitterHandle::mouseMoveEvent(QMouseEvent* event) {
    if (!(event->buttons() & Qt::LeftButton)) {
        QSplitterHandle::mouseMoveEvent(event);
        return;
    }

    const int current = orientation() == Qt::Horizontal ? event->globalPosition().toPoint().x()
                                                        : event->globalPosition().toPoint().y();
    const int rawDelta = current - m_dragAnchor;
    const int step = qMax(1, kSplitterDragPixelsPerStep);
    const int scaledDelta = rawDelta / step;
    if (scaledDelta == 0) {
        return;
    }

    m_dragAnchor += scaledDelta * step;
    moveSplitter(scaledDelta);
}

DampedSplitter::DampedSplitter(Qt::Orientation orientation, QWidget* parent)
    : QSplitter(orientation, parent) {}

QSplitterHandle* DampedSplitter::createHandle() {
    return new DampedSplitterHandle(orientation(), this);
}

} // namespace UiResizeHandle
