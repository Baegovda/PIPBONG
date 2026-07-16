#include "ui/widgets/ListDragAutoScroll.h"

#include <QAbstractNativeEventFilter>
#include <QAbstractScrollArea>
#include <QApplication>
#include <QCursor>
#include <QScrollBar>
#include <QTimer>
#include <QWheelEvent>

#include <cmath>

#ifdef Q_OS_WIN
#include <windows.h>
#include <windowsx.h>
#endif

namespace {

constexpr int kIntervalMs = 24;
/// Overshoot distance (px) outside the viewport that reaches maximum scroll step.
constexpr int kOvershootForMaxStepPx = 160;
constexpr int kMinStepPx = 1;
constexpr int kMaxStepPx = 4;

int edgeScrollStepForOvershoot(int overshootPx) {
    if (overshootPx <= 0) {
        return 0;
    }
    const double t = std::clamp(overshootPx / static_cast<double>(kOvershootForMaxStepPx), 0.0, 1.0);
    // Quadratic ramp: small overshoot stays slow; speed builds only when far outside.
    const double eased = t * t;
    return kMinStepPx + static_cast<int>(std::lround((kMaxStepPx - kMinStepPx) * eased));
}

#ifdef Q_OS_WIN
class ListDragWheelNativeFilter final : public QAbstractNativeEventFilter {
public:
    explicit ListDragWheelNativeFilter(ListDragAutoScroll* owner)
        : m_owner(owner) {}

    bool nativeEventFilter(const QByteArray& eventType, void* message, qintptr* result) override {
        Q_UNUSED(result);
        if (!m_owner || !m_owner->isActive()) {
            return false;
        }
        if (eventType != "windows_generic_MSG" && eventType != "windows_dispatcher_MSG") {
            return false;
        }

        auto* msg = static_cast<MSG*>(message);
        if (msg->message != WM_MOUSEWHEEL) {
            return false;
        }

        QAbstractScrollArea* area = m_owner->scrollArea();
        if (!area) {
            return false;
        }

        const QPoint globalPos(GET_X_LPARAM(msg->lParam), GET_Y_LPARAM(msg->lParam));
        const QRect areaRect(area->mapToGlobal(QPoint(0, 0)), area->size());
        if (!areaRect.contains(globalPos)) {
            return false;
        }

        const short wheelDelta = GET_WHEEL_DELTA_WPARAM(msg->wParam);
        if (wheelDelta == 0) {
            return false;
        }

        const int notchSteps = static_cast<int>(wheelDelta) / WHEEL_DELTA;
        return m_owner->applyWheelSteps(notchSteps);
    }

private:
    ListDragAutoScroll* m_owner = nullptr;
};
#endif

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
    m_scrollArea->installEventFilter(this);
    if (QWidget* viewport = m_scrollArea->viewport()) {
        viewport->installEventFilter(this);
    }
#ifdef Q_OS_WIN
    if (!m_nativeWheelFilter) {
        m_nativeWheelFilter = new ListDragWheelNativeFilter(this);
        qApp->installNativeEventFilter(m_nativeWheelFilter);
    }
#endif
    if (m_edgeTimer) {
        m_edgeTimer->start();
    }
}

void ListDragAutoScroll::end() {
    if (!m_active) {
        return;
    }
    stopSession();
}

void ListDragAutoScroll::updateFromGlobalCursor() {
    if (!m_scrollArea || !m_scrollArea->viewport()) {
        return;
    }
    updateFromViewportPos(m_scrollArea->viewport()->mapFromGlobal(QCursor::pos()));
}

void ListDragAutoScroll::updateFromViewportPos(const QPoint& viewportPos) {
    if (!m_active || !m_scrollArea || !m_scrollArea->viewport()) {
        clearEdgeMotion();
        return;
    }

    QScrollBar* bar = m_scrollArea->verticalScrollBar();
    if (!bar || bar->maximum() <= 0) {
        clearEdgeMotion();
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
        clearEdgeMotion();
        return;
    }

    m_edgeDirection = direction;
    m_edgeStep = step;
}

bool ListDragAutoScroll::applyWheelSteps(int notchSteps) {
    if (!m_active || !m_scrollArea || notchSteps == 0) {
        return false;
    }

    QScrollBar* bar = m_scrollArea->verticalScrollBar();
    if (!bar) {
        return false;
    }

    int deltaPx = -notchSteps * bar->singleStep();
    deltaPx = static_cast<int>(std::lround(deltaPx * 0.65));
    if (deltaPx == 0) {
        return false;
    }

    scrollBy(deltaPx);
    return true;
}

bool ListDragAutoScroll::handleWheel(QWheelEvent* wheelEvent) {
    if (!wheelEvent || !m_scrollArea) {
        return false;
    }

    int notchSteps = 0;
    if (!wheelEvent->pixelDelta().isNull()) {
        QScrollBar* bar = m_scrollArea->verticalScrollBar();
        if (!bar || bar->singleStep() <= 0) {
            return false;
        }
        const int deltaPx = -wheelEvent->pixelDelta().y();
        notchSteps = deltaPx / bar->singleStep();
        if (notchSteps == 0 && deltaPx != 0) {
            notchSteps = deltaPx > 0 ? 1 : -1;
        }
    } else {
        const int angleY = wheelEvent->angleDelta().y();
        if (angleY == 0) {
            return false;
        }
        const int steps = angleY / 120;
        notchSteps = steps != 0 ? steps : (angleY > 0 ? 1 : -1);
    }

    if (applyWheelSteps(notchSteps)) {
        wheelEvent->accept();
        return true;
    }
    return false;
}

void ListDragAutoScroll::releaseEdgeScroll() {
    clearEdgeMotion();
}

bool ListDragAutoScroll::eventFilter(QObject* watched, QEvent* event) {
    Q_UNUSED(watched);
    if (!m_active || event->type() != QEvent::Wheel || !m_scrollArea) {
        return QObject::eventFilter(watched, event);
    }

    auto* wheelEvent = static_cast<QWheelEvent*>(event);
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

void ListDragAutoScroll::clearEdgeMotion() {
    m_edgeDirection = 0;
    m_edgeStep = 0;
}

void ListDragAutoScroll::stopSession() {
    m_active = false;
    clearEdgeMotion();
    if (m_edgeTimer) {
        m_edgeTimer->stop();
    }
#ifdef Q_OS_WIN
    if (m_nativeWheelFilter) {
        qApp->removeNativeEventFilter(m_nativeWheelFilter);
        delete m_nativeWheelFilter;
        m_nativeWheelFilter = nullptr;
    }
#endif
    qApp->removeEventFilter(this);
    if (m_scrollArea) {
        m_scrollArea->removeEventFilter(this);
        if (QWidget* viewport = m_scrollArea->viewport()) {
            viewport->removeEventFilter(this);
        }
    }
}

void ListDragAutoScroll::tickEdgeScroll() {
    if (!m_active || !m_scrollArea || !m_scrollArea->viewport()) {
        stopSession();
        return;
    }

    updateFromGlobalCursor();

    if (m_edgeDirection == 0 || m_edgeStep <= 0) {
        return;
    }

    QScrollBar* bar = m_scrollArea->verticalScrollBar();
    if (!bar) {
        clearEdgeMotion();
        return;
    }

    const int before = bar->value();
    scrollBy(m_edgeDirection * m_edgeStep);
    if (bar->value() == before) {
        clearEdgeMotion();
    }
}
