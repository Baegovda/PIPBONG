#include "ui/widgets/ListDragAutoScroll.h"

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

int wheelPixelsPerNotch(const QScrollBar* bar) {
    if (!bar) {
        return 32;
    }
    int pixels = bar->singleStep();
    if (pixels < 16) {
        const int page = bar->pageStep();
        pixels = page > 0 ? std::max(16, page / 3) : 32;
    }
    return pixels;
}

#ifdef Q_OS_WIN
ListDragAutoScroll* g_dragWheelTarget = nullptr;
HHOOK g_dragWheelHook = nullptr;
int g_dragWheelHookRefs = 0;

LRESULT CALLBACK dragWheelMouseHookProc(int code, WPARAM wParam, LPARAM lParam) {
    if (code < 0) {
        return CallNextHookEx(g_dragWheelHook, code, wParam, lParam);
    }

    if (wParam == WM_MOUSEWHEEL && g_dragWheelTarget && g_dragWheelTarget->isActive()) {
        const auto* info = reinterpret_cast<MSLLHOOKSTRUCT*>(lParam);
        if (info && !(info->flags & LLMHF_INJECTED)) {
            const short wheelDelta = GET_WHEEL_DELTA_WPARAM(info->mouseData);
            if (wheelDelta != 0) {
                const int notchSteps = static_cast<int>(wheelDelta) / WHEEL_DELTA;
                if (g_dragWheelTarget->applyWheelSteps(notchSteps)) {
                    return 1;
                }
            }
        }
    }

    return CallNextHookEx(g_dragWheelHook, code, wParam, lParam);
}

void installDragWheelHook(ListDragAutoScroll* target) {
    if (!target) {
        return;
    }
    g_dragWheelTarget = target;
    if (g_dragWheelHookRefs == 0) {
        g_dragWheelHook =
            SetWindowsHookExW(WH_MOUSE_LL, dragWheelMouseHookProc, GetModuleHandleW(nullptr), 0);
        if (!g_dragWheelHook) {
            return;
        }
    }
    ++g_dragWheelHookRefs;
}

void uninstallDragWheelHook(ListDragAutoScroll* target) {
    if (g_dragWheelTarget == target) {
        g_dragWheelTarget = nullptr;
    }
    if (g_dragWheelHookRefs <= 0) {
        return;
    }
    --g_dragWheelHookRefs;
    if (g_dragWheelHookRefs == 0 && g_dragWheelHook) {
        UnhookWindowsHookEx(g_dragWheelHook);
        g_dragWheelHook = nullptr;
    }
}
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
    installDragWheelHook(this);
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

    int deltaPx = -notchSteps * wheelPixelsPerNotch(bar);
    deltaPx = static_cast<int>(std::lround(deltaPx * 0.65));
    if (deltaPx == 0) {
        deltaPx = notchSteps > 0 ? -1 : 1;
    }

    const int before = bar->value();
    scrollBy(deltaPx);
    return bar->value() != before;
}

bool ListDragAutoScroll::handleWheel(QWheelEvent* wheelEvent) {
    if (!wheelEvent || !m_scrollArea) {
        return false;
    }

    int notchSteps = 0;
    if (!wheelEvent->pixelDelta().isNull()) {
        QScrollBar* bar = m_scrollArea->verticalScrollBar();
        const int pixelsPerNotch = wheelPixelsPerNotch(bar);
        const int deltaPx = -wheelEvent->pixelDelta().y();
        notchSteps = deltaPx / pixelsPerNotch;
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
    uninstallDragWheelHook(this);
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
