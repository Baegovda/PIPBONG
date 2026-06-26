#pragma once

#include <QRect>
#include <QWidget>

class RoiPreviewWidget : public QWidget {
    Q_OBJECT
public:
    explicit RoiPreviewWidget(QWidget* parent = nullptr);

    void setImage(const QPixmap& pixmap, const QRect& roiRect, bool editable);
    QRect roiRect() const;

signals:
    void roiChanged(const QRect& roiRect);

protected:
    void paintEvent(QPaintEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;

private:
    QRect imageRectToWidget(const QRect& imageRect) const;
    QRect widgetRectToImage(const QRect& widgetRect) const;
    QRect normalizedWidgetRect(const QPoint& a, const QPoint& b) const;
    void updateScaledPixmap();

    QPixmap m_sourcePixmap;
    QPixmap m_scaledPixmap;
    QRect m_roiImageRect;
    QRect m_drawRect;
    bool m_editable = false;
    bool m_dragging = false;
    QPoint m_dragOriginWidget;
};
