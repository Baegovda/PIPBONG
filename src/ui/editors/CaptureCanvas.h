#pragma once

#include <QWidget>
#include <QRubberBand>
#include <QPoint>

class CaptureCanvas : public QWidget {
    Q_OBJECT
public:
    explicit CaptureCanvas(QWidget* parent = nullptr);

    void setPixmap(const QPixmap& pixmap);
    QRect selectedRect() const;

protected:
    void paintEvent(QPaintEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;

private:
    QPixmap m_pixmap;
    QRubberBand* m_rubberBand = nullptr;
    QPoint m_origin;
    QRect m_selection;
};
