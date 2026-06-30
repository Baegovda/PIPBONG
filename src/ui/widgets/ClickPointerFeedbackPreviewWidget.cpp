#include "ui/widgets/ClickPointerFeedbackPreviewWidget.h"

#include "ui/ClickPointerFeedbackRenderer.h"

#include <QDateTime>
#include <QImage>
#include <QLinearGradient>
#include <QPainter>
#include <QPaintEvent>
#include <QResizeEvent>
#include <QSizePolicy>
#include <QTimer>

#include <cstring>
#include <vector>

namespace {

constexpr int kTimerMs = 33;

void paintCheckerBackground(QPainter& painter, const QRect& rect, const QPalette& pal) {
    const QColor base = pal.color(QPalette::Window).darker(108);
    const QColor alt = base.lighter(112);
    painter.fillRect(rect, base);
    constexpr int tile = 10;
    for (int y = rect.top(); y < rect.bottom(); y += tile) {
        for (int x = rect.left(); x < rect.right(); x += tile) {
            const bool odd = ((x / tile) + (y / tile)) % 2 == 1;
            if (odd) {
                painter.fillRect(x, y, tile, tile, alt);
            }
        }
    }
}

} // namespace

ClickPointerFeedbackPreviewWidget::ClickPointerFeedbackPreviewWidget(QWidget* parent)
    : QWidget(parent) {
    setMinimumHeight(196);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);

    m_timer = new QTimer(this);
    m_timer->setInterval(kTimerMs);
    connect(m_timer, &QTimer::timeout, this, &ClickPointerFeedbackPreviewWidget::onTick);
    restartAnimation();
}

void ClickPointerFeedbackPreviewWidget::setSettings(const ClickPointerFeedbackSettings& settings) {
    m_settings = settings;
    update();
}

void ClickPointerFeedbackPreviewWidget::restartAnimation() {
    m_startMs = QDateTime::currentMSecsSinceEpoch();
    if (!m_timer->isActive()) {
        m_timer->start();
    }
    update();
}

void ClickPointerFeedbackPreviewWidget::onTick() {
    update();
    const qint64 age = QDateTime::currentMSecsSinceEpoch() - m_startMs;
    if (age > m_settings.displayDurationMs + 120) {
        restartAnimation();
    }
}

void ClickPointerFeedbackPreviewWidget::resizeEvent(QResizeEvent* event) {
    QWidget::resizeEvent(event);
    update();
}

void ClickPointerFeedbackPreviewWidget::paintEvent(QPaintEvent* event) {
    Q_UNUSED(event);

    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, true);

    const QRect frame = rect().adjusted(1, 1, -1, -1);
    paintCheckerBackground(painter, frame, palette());

    QLinearGradient vignette(frame.topLeft(), frame.bottomLeft());
    vignette.setColorAt(0.0, QColor(0, 0, 0, 28));
    vignette.setColorAt(0.5, QColor(0, 0, 0, 0));
    vignette.setColorAt(1.0, QColor(0, 0, 0, 40));
    painter.fillRect(frame, vignette);

    painter.setPen(QPen(palette().color(QPalette::Mid), 1));
    painter.setBrush(Qt::NoBrush);
    painter.drawRoundedRect(frame, 8, 8);

    const int centerX = frame.center().x();
    const int centerY = frame.center().y();

    QImage image(frame.width(), frame.height(), QImage::Format_ARGB32);
    image.fill(Qt::transparent);

    std::vector<uint32_t> pixels(static_cast<size_t>(frame.width()) * static_cast<size_t>(frame.height()), 0u);
    const qint64 age = qMax<qint64>(0, QDateTime::currentMSecsSinceEpoch() - m_startMs);
    renderClickPointerFeedbackFrame(pixels.data(),
                                    frame.width(),
                                    frame.height(),
                                    centerX - frame.left(),
                                    centerY - frame.top(),
                                    static_cast<uint64_t>(age),
                                    static_cast<uint64_t>(m_settings.displayDurationMs),
                                    m_settings);
    if (image.bytesPerLine() == frame.width() * static_cast<int>(sizeof(uint32_t))) {
        std::memcpy(image.bits(), pixels.data(), pixels.size() * sizeof(uint32_t));
    } else {
        for (int y = 0; y < frame.height(); ++y) {
            std::memcpy(image.scanLine(y),
                        pixels.data() + static_cast<size_t>(y) * static_cast<size_t>(frame.width()),
                        static_cast<size_t>(frame.width()) * sizeof(uint32_t));
        }
    }

    painter.drawImage(frame.topLeft(), image);

    painter.setPen(palette().color(QPalette::Mid));
    QFont caption = font();
    caption.setPointSize(qMax(8, caption.pointSize() - 1));
    painter.setFont(caption);
    painter.drawText(frame.adjusted(10, 8, -10, -8),
                     Qt::AlignTop | Qt::AlignLeft,
                     tr("미리보기"));
}
