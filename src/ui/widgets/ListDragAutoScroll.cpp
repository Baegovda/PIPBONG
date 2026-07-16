#include "ui/widgets/ListDragAutoScroll.h"

#include <QAbstractScrollArea>
#include <QApplication>
#include <QCursor>
#include <QScrollBar>
#include <QTimer>
#include <QWheelEvent>

#include <cmath>

namespace {

constexpr int kIntervalMs = 16;
/// Overshoot distance (px) outside the viewport that reaches maximum scroll step.
constexpr int kOvershootForMaxStepPx = 96;
constexpr int kMinStepPx = 1;
constexpr int kMaxStepPx = 8;

int edgeScrollStepForOvershoot(int overshootPx) {
    if (overshootPx <= 0) {
        return 0;
    }
    const double t = std::clamp(overshootPx / static_cast<double>(kOvershootForMaxStepPx), 0.0, 1.0);
    return kMinStepPx + static_cast<int>(std::lround((kMaxStepPx - kMinStepPx) * t));
}

} // namespace

ListDragAutoScroll::ListDragAutoScroll(QAbstractScrollArea* scrollArea, QObject* parent)
    : QObject(parent)
    , m_scrollArea(scrollArea) {
    m_edgeTimer = new QTimer(this);
    m_edgeTimer->setInterval(kIntervalMs);
    connect(m_edgeTimer, &QTimer::timeout, this, [this]() { tickEdgeScroll(); });
}

void ListDragAutoScroll::setOnScrolled(std::function<void()> callback) {
    m_onScrolled = std::move(callback);
}

void ListDragAutoScroll::begin() {
    if (m_active || !m_scrollArea) {
        return;
    }
    m_active = true;
    qApp->installEventFilter(this);
}

void ListDragAutoScroll::end() {
    if (!m_active) {
        return;
    }
    stopEdgeScroll();
    m_active = false;
    qApp->removeEventFilter(this);
}

void ListDragAutoScroll::updateFromViewportPos(const QPoint& viewportPos) {
    if (!m_active || !m_scrollArea || !m_scrollArea->viewport()) {
        stopEdgeScroll();
        return;
    }

    QScrollBar* bar = m_scrollArea->verticalScrollBar();
    if (!bar || bar->maximum() <= 0) {
        stopEdgeScroll();
        return;
    }

    const int viewportHeight = m_scrollArea->viewport()->height();
    const int y = viewportPos.y();
    int direction = 0;
    int overshootPx = 0;

    if (y < 0) {
        direction = -1;
        overshootPx = -y;
    } else if (y >= viewportHeight) {
        direction = 1;
        overshootPx = y - viewportHeight + 1;
    }

    const int step = edgeScrollStepForOvershoot(overshootPx);
    if (direction == 0 || step <= 0) {
        stopEdgeScroll();
        return;
    }

    m_edgeDirection = direction;
    m_edgeStep = step;
    if (m_edgeTimer && !m_edgeTimer->isActive()) {
        m_edgeTimer->start();
    }
}

bool ListDragAutoScroll::handleWheel(QWheelEvent* wheelEvent) {
    if (!wheelEvent || !m_scrollArea) {
        return false;
    }

    QScrollBar* bar = m_scrollArea->verticalScrollBar();
    if (!bar) {
        return false;
    }

    int deltaPx = 0;
    if (!wheelEvent->pixelDelta().isNull()) {
        deltaPx = -wheelEvent->pixelDelta().y();
    } else {
        const int angleY = wheelEvent->angleDelta().y();
        if (angleY == 0) {
            return false;
        }
        const int steps = angleY / 120;
        const int notchSteps = steps != 0 ? steps : (angleY > 0 ? 1 : -1);
        deltaPx = -notchSteps * bar->singleStep();
    }

    if (deltaPx == 0) {
        return false;
    }

    // Gentler than stock wheel while a drag session owns scrolling.
    deltaPx = static_cast<int>(std::lround(deltaPx * 0.65));

    if (deltaPx == 0) {
        return false;
    }

    scrollBy(deltaPx);
    wheelEvent->accept();
    return true;
}

void ListDragAutoScroll::releaseEdgeScroll() {
    stopEdgeScroll();
}

bool ListDragAutoScroll::eventFilter(QObject* watched, QEvent* event) {
    Q_UNUSED(watched);
    if (!m_active || event->type() != QEvent::Wheel || !m_scrollArea) {
        return QObject::eventFilter(watched, event);
    }

    auto* wheelEvent = static_cast<QWheelEvent*>(event);
    const QPoint globalPos = wheelEvent->globalPosition().toPoint();
    const QRect areaGlobalRect(m_scrollArea->mapToGlobal(QPoint(0, 0)), m_scrollArea->size());
    if (!areaGlobalRect.contains(globalPos)) {
        return false;
    }

    if (handleWheel(wheelEvent)) {
        return true;
    }
    return false;
}

void ListDragAutoScroll::scrollBy(int deltaPx) {
    if (!m_scrollArea || deltaPx == 0) {
        return;
    }

    QScrollBar* bar = m_scrollArea->verticalScrollBar();
    if (!bar) {
        return;
    }

    const int next = std::clamp(bar->value() + deltaPx, bar->minimum(), bar->maximum());
    if (next == bar->value()) {
        return;
    }
    bar->setValue(next);

    if (m_onScrolled) {
        m_onScrolled();
    }
}

void ListDragAutoScroll::stopEdgeScroll() {
    m_edgeDirection = 0;
    m_edgeStep = 0;
    if (m_edgeTimer) {
        m_edgeTimer->stop();
    }
}

void ListDragAutoScroll::tickEdgeScroll() {
    if (m_edgeDirection == 0 || !m_scrollArea || !m_scrollArea->viewport()) {
        stopEdgeScroll();
        return;
    }

    const QPoint viewportPos = m_scrollArea->viewport()->mapFromGlobal(QCursor::pos());
    updateFromViewportPos(viewportPos);

    if (m_edgeDirection == 0 || m_edgeStep <= 0) {
        return;
    }

    QScrollBar* bar = m_scrollArea->verticalScrollBar();
    if (!bar) {
        stopEdgeScroll();
        return;
    }

    const int before = bar->value();
    scrollBy(m_edgeDirection * m_edgeStep);
    if (bar->value() == before) {
        stopEdgeScroll();
    }
}
