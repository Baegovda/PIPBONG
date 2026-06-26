#include "ui/editors/CaptureCanvas.h"

#include <QMouseEvent>
#include <QPainter>

CaptureCanvas::CaptureCanvas(QWidget* parent)
    : QWidget(parent)
    , m_rubberBand(new QRubberBand(QRubberBand::Rectangle, this)) {
    setMouseTracking(true);
}

void CaptureCanvas::setPixmap(const QPixmap& pixmap) {
    m_pixmap = pixmap;
    setMinimumSize(m_pixmap.size());
    update();
}

QRect CaptureCanvas::selectedRect() const {
    return m_selection;
}

void CaptureCanvas::paintEvent(QPaintEvent* event) {
    Q_UNUSED(event)
    QPainter painter(this);
    if (!m_pixmap.isNull()) {
        painter.drawPixmap(0, 0, m_pixmap);
    } else {
        painter.fillRect(rect(), Qt::darkGray);
    }
}

void CaptureCanvas::mousePressEvent(QMouseEvent* event) {
    m_origin = event->pos();
    m_rubberBand->setGeometry(QRect(m_origin, QSize()));
    m_rubberBand->show();
}

void CaptureCanvas::mouseMoveEvent(QMouseEvent* event) {
    m_rubberBand->setGeometry(QRect(m_origin, event->pos()).normalized());
}

void CaptureCanvas::mouseReleaseEvent(QMouseEvent* event) {
    m_selection = QRect(m_origin, event->pos()).normalized();
    m_rubberBand->hide();
}
