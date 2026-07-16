#pragma once

#include <QObject>
#include <QPoint>

#include <QtGlobal>

#include <functional>

class QAbstractScrollArea;
class QWheelEvent;

/// Edge auto-scroll and mouse-wheel scroll during list/table drag sessions.
/// Used by reorderable lists, workflow block list, calculator cell drag, etc.
class ListDragAutoScroll : public QObject {
public:
    explicit ListDragAutoScroll(QAbstractScrollArea* scrollArea, QObject* parent = nullptr);

    void setOnScrolled(std::function<void()> callback);

    bool isActive() const { return m_active; }

    void begin();
    void end();

    void updateFromViewportPos(const QPoint& viewportPos);
    void updateFromGlobalCursor();
    void releaseEdgeScroll();
    bool handleWheel(QWheelEvent* wheelEvent);

    QAbstractScrollArea* scrollArea() const { return m_scrollArea; }
    bool applyWheelSteps(int notchSteps);

protected:
    bool eventFilter(QObject* watched, QEvent* event) override;

private:
    void scrollBy(int deltaPx);
    void clearEdgeMotion();
    void stopSession();
    void tickEdgeScroll();

    QAbstractScrollArea* m_scrollArea = nullptr;
    std::function<void()> m_onScrolled;
    class QTimer* m_edgeTimer = nullptr;
    int m_edgeDirection = 0;
    double m_edgeStep = 0.0;
    double m_edgeScrollCarry = 0.0;
    bool m_active = false;
};
