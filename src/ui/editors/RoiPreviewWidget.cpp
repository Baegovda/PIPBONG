#include "ui/editors/RoiPreviewWidget.h"

#include <QMouseEvent>
#include <QPainter>
#include <QPen>
#include <QResizeEvent>

namespace {

QRect clampRectToImage(const QRect& rect, const QSize& imageSize) {
    if (imageSize.isEmpty()) {
        return {};
    }

    QRect clamped = rect.normalized();
    if (clamped.left() < 0) {
        clamped.moveLeft(0);
    }
    if (clamped.top() < 0) {
        clamped.moveTop(0);
    }
    if (clamped.right() >= imageSize.width()) {
        clamped.setRight(imageSize.width() - 1);
    }
    if (clamped.bottom() >= imageSize.height()) {
        clamped.setBottom(imageSize.height() - 1);
    }
    if (clamped.width() < 1) {
        clamped.setWidth(1);
    }
    if (clamped.height() < 1) {
        clamped.setHeight(1);
    }
    return clamped;
}

} // namespace

RoiPreviewWidget::RoiPreviewWidget(QWidget* parent)
    : QWidget(parent) {
    setMinimumSize(320, 180);
    setMouseTracking(true);
}

void RoiPreviewWidget::setImage(const QPixmap& pixmap, const QRect& roiRect, bool editable) {
    m_sourcePixmap = pixmap;
    m_roiImageRect = clampRectToImage(roiRect, pixmap.size());
    m_editable = editable;
    updateScaledPixmap();
    update();
}

QRect RoiPreviewWidget::roiRect() const {
    return m_roiImageRect;
}

void RoiPreviewWidget::updateScaledPixmap() {
    if (m_sourcePixmap.isNull()) {
        m_scaledPixmap = {};
        m_drawRect = {};
        return;
    }

    m_scaledPixmap = m_sourcePixmap.scaled(size(), Qt::KeepAspectRatio, Qt::SmoothTransformation);
    const int x = (width() - m_scaledPixmap.width()) / 2;
    const int y = (height() - m_scaledPixmap.height()) / 2;
    m_drawRect = QRect(x, y, m_scaledPixmap.width(), m_scaledPixmap.height());
}

QRect RoiPreviewWidget::imageRectToWidget(const QRect& imageRect) const {
    if (m_sourcePixmap.isNull() || m_drawRect.isEmpty()) {
        return {};
    }

    const double scaleX = static_cast<double>(m_drawRect.width()) / m_sourcePixmap.width();
    const double scaleY = static_cast<double>(m_drawRect.height()) / m_sourcePixmap.height();
    return QRect(m_drawRect.x() + static_cast<int>(imageRect.x() * scaleX),
                 m_drawRect.y() + static_cast<int>(imageRect.y() * scaleY),
                 static_cast<int>(imageRect.width() * scaleX),
                 static_cast<int>(imageRect.height() * scaleY));
}

QRect RoiPreviewWidget::widgetRectToImage(const QRect& widgetRect) const {
    if (m_sourcePixmap.isNull() || m_drawRect.isEmpty()) {
        return {};
    }

    const double scaleX = static_cast<double>(m_sourcePixmap.width()) / m_drawRect.width();
    const double scaleY = static_cast<double>(m_sourcePixmap.height()) / m_drawRect.height();

    const int x = static_cast<int>((widgetRect.x() - m_drawRect.x()) * scaleX);
    const int y = static_cast<int>((widgetRect.y() - m_drawRect.y()) * scaleY);
    const int w = static_cast<int>(widgetRect.width() * scaleX);
    const int h = static_cast<int>(widgetRect.height() * scaleY);
    return clampRectToImage(QRect(x, y, w, h), m_sourcePixmap.size());
}

QRect RoiPreviewWidget::normalizedWidgetRect(const QPoint& a, const QPoint& b) const {
    return QRect(a, b).normalized().intersected(m_drawRect);
}

void RoiPreviewWidget::paintEvent(QPaintEvent* event) {
    Q_UNUSED(event)

    QPainter painter(this);
    painter.fillRect(rect(), palette().color(QPalette::Window));

    if (m_scaledPixmap.isNull()) {
        painter.setPen(palette().color(QPalette::Text));
        painter.drawText(rect(), Qt::AlignCenter, tr("미리보기 없음"));
        return;
    }

    painter.drawPixmap(m_drawRect.topLeft(), m_scaledPixmap);

    const QRect roiWidgetRect = imageRectToWidget(m_roiImageRect);
    QPen pen(QColor(0, 200, 0), 2);
    painter.setPen(pen);
    painter.setBrush(QColor(0, 200, 0, 40));
    painter.drawRect(roiWidgetRect);

    if (m_editable) {
        painter.setPen(Qt::NoPen);
        painter.setBrush(QColor(255, 255, 255, 180));
        painter.drawText(m_drawRect.adjusted(8, 8, -8, -8),
                         Qt::AlignTop | Qt::AlignLeft,
                         tr("드래그하여 탐색 영역을 조정하세요"));
    }
}

void RoiPreviewWidget::mousePressEvent(QMouseEvent* event) {
    if (!m_editable || !m_drawRect.contains(event->pos())) {
        return;
    }
    m_dragging = true;
    m_dragOriginWidget = event->pos();
}

void RoiPreviewWidget::mouseMoveEvent(QMouseEvent* event) {
    if (!m_dragging) {
        return;
    }

    const QRect dragRect = normalizedWidgetRect(m_dragOriginWidget, event->pos());
    if (dragRect.width() < 2 || dragRect.height() < 2) {
        return;
    }

    m_roiImageRect = widgetRectToImage(dragRect);
    update();
}

void RoiPreviewWidget::mouseReleaseEvent(QMouseEvent* event) {
    if (!m_dragging) {
        return;
    }

    m_dragging = false;
    const QRect dragRect = normalizedWidgetRect(m_dragOriginWidget, event->pos());
    if (dragRect.width() >= 2 && dragRect.height() >= 2) {
        m_roiImageRect = widgetRectToImage(dragRect);
        emit roiChanged(m_roiImageRect);
    }
    Q_UNUSED(event)
    update();
}

void RoiPreviewWidget::resizeEvent(QResizeEvent* event) {
    QWidget::resizeEvent(event);
    updateScaledPixmap();
    update();
}
