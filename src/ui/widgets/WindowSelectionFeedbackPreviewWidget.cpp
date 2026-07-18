#include "ui/widgets/WindowSelectionFeedbackPreviewWidget.h"

#include "ui/WindowSelectionFeedbackRenderer.h"

#include <QDateTime>
#include <QImage>
#include <QLinearGradient>
#include <QPainter>
#include <QPaintEvent>
#include <QResizeEvent>
#include <QSizePolicy>
#include <QTimer>

#include <algorithm>
#include <cstring>
#include <vector>

namespace {

constexpr int kTimerMs = 16;

void shrinkCaptionFont(QFont& font) {
    if (font.pixelSize() > 0) {
        font.setPixelSize(qMax(8, font.pixelSize() - 1));
        return;
    }
    const int pointSize = font.pointSize();
    font.setPointSize(qMax(8, pointSize > 0 ? pointSize - 1 : 9));
}

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

QRect previewWindowRect(const QRect& frame) {
    const int marginX = std::max(18, frame.width() / 14);
    const int marginY = std::max(22, frame.height() / 10);
    return frame.adjusted(marginX, marginY, -marginX, -marginY);
}

} // namespace

WindowSelectionFeedbackPreviewWidget::WindowSelectionFeedbackPreviewWidget(QWidget* parent)
    : QWidget(parent) {
    setMinimumSize(160, 196);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);

    m_timer = new QTimer(this);
    m_timer->setInterval(kTimerMs);
    connect(m_timer, &QTimer::timeout, this, &WindowSelectionFeedbackPreviewWidget::onTick);
    restartAnimation();
}

void WindowSelectionFeedbackPreviewWidget::setSettings(const WindowSelectionFeedbackSettings& settings) {
    m_settings = settings;
    update();
}

void WindowSelectionFeedbackPreviewWidget::restartAnimation() {
    m_startMs = QDateTime::currentMSecsSinceEpoch();
    if (!m_timer->isActive()) {
        m_timer->start();
    }
    update();
}

void WindowSelectionFeedbackPreviewWidget::onTick() {
    update();
    const qint64 age = QDateTime::currentMSecsSinceEpoch() - m_startMs;
    if (age > m_settings.displayDurationMs + 160) {
        restartAnimation();
    }
}

void WindowSelectionFeedbackPreviewWidget::resizeEvent(QResizeEvent* event) {
    QWidget::resizeEvent(event);
    update();
}

void WindowSelectionFeedbackPreviewWidget::paintEvent(QPaintEvent* event) {
    Q_UNUSED(event);

    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, true);

    const QRect frame = rect().adjusted(1, 1, -1, -1);
    if (frame.width() <= 0 || frame.height() <= 0) {
        paintCheckerBackground(painter, rect(), palette());
        return;
    }
    paintCheckerBackground(painter, frame, palette());

    QLinearGradient vignette(frame.topLeft(), frame.bottomLeft());
    vignette.setColorAt(0.0, QColor(0, 0, 0, 28));
    vignette.setColorAt(0.5, QColor(0, 0, 0, 0));
    vignette.setColorAt(1.0, QColor(0, 0, 0, 40));
    painter.fillRect(frame, vignette);

    const QRect windowRect = previewWindowRect(frame);
    if (windowRect.width() <= 0 || windowRect.height() <= 0) {
        painter.setPen(palette().color(QPalette::Mid));
        QFont caption = font();
        shrinkCaptionFont(caption);
        painter.setFont(caption);
        painter.drawText(frame.adjusted(10, 8, -10, -8), Qt::AlignTop | Qt::AlignLeft, tr("미리보기"));
        return;
    }

    painter.setPen(QPen(palette().color(QPalette::Mid), 1));
    painter.setBrush(palette().color(QPalette::Window).darker(112));
    painter.drawRoundedRect(windowRect, 6, 6);

    painter.setPen(Qt::NoPen);
    painter.setBrush(palette().color(QPalette::Mid).lighter(105));
    const QRect titleBar = QRect(windowRect.left(), windowRect.top(), windowRect.width(), 14);
    painter.drawRoundedRect(titleBar, 6, 6);
    painter.fillRect(titleBar.adjusted(0, 6, 0, 0), palette().color(QPalette::Mid).lighter(105));

    QImage image(windowRect.width(), windowRect.height(), QImage::Format_ARGB32);
    image.fill(Qt::transparent);

    std::vector<uint32_t> pixels(static_cast<size_t>(windowRect.width()) * static_cast<size_t>(windowRect.height()),
                                 0u);
    const qint64 age = qMax<qint64>(0, QDateTime::currentMSecsSinceEpoch() - m_startMs);
    const double speed = std::max(0.25, m_settings.animationSpeed);
    const float progress = std::clamp(
        static_cast<float>(static_cast<double>(age) * speed)
            / static_cast<float>(std::max(1, m_settings.displayDurationMs)),
        0.0f,
        1.0f);
    renderWindowSelectionFeedbackFrame(pixels.data(),
                                       windowRect.width(),
                                       windowRect.height(),
                                       progress,
                                       m_settings);
    if (image.isNull() || !image.bits()) {
        return;
    }
    if (image.bytesPerLine() == windowRect.width() * static_cast<int>(sizeof(uint32_t))) {
        std::memcpy(image.bits(), pixels.data(), pixels.size() * sizeof(uint32_t));
    } else {
        for (int y = 0; y < windowRect.height(); ++y) {
            std::memcpy(image.scanLine(y),
                        pixels.data() + static_cast<size_t>(y) * static_cast<size_t>(windowRect.width()),
                        static_cast<size_t>(windowRect.width()) * sizeof(uint32_t));
        }
    }

    painter.drawImage(windowRect.topLeft(), image);

    painter.setPen(palette().color(QPalette::Mid));
    QFont caption = font();
    shrinkCaptionFont(caption);
    painter.setFont(caption);
    painter.drawText(frame.adjusted(10, 8, -10, -8), Qt::AlignTop | Qt::AlignLeft, tr("미리보기"));
}
