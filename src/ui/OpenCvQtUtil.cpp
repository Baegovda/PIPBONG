#include "ui/OpenCvQtUtil.h"

#include <QPainter>
#include <QPen>

#include <opencv2/imgproc.hpp>

namespace OpenCvQtUtil {

QPixmap matToPixmap(const cv::Mat& image) {
    if (image.empty()) {
        return {};
    }

    cv::Mat rgb;
    switch (image.channels()) {
    case 1:
        cv::cvtColor(image, rgb, cv::COLOR_GRAY2RGB);
        break;
    case 4:
        cv::cvtColor(image, rgb, cv::COLOR_BGRA2RGB);
        break;
    default:
        cv::cvtColor(image, rgb, cv::COLOR_BGR2RGB);
        break;
    }

    QImage qImage(rgb.data, rgb.cols, rgb.rows, static_cast<int>(rgb.step), QImage::Format_RGB888);
    return QPixmap::fromImage(qImage.copy());
}

QPixmap drawRoiOverlay(const QPixmap& base,
                       const QRect& roiRect,
                       const QColor& color,
                       int penWidth) {
    if (base.isNull()) {
        return {};
    }

    QPixmap result = base;
    QPainter painter(&result);
    painter.setRenderHint(QPainter::Antialiasing);

    QPen pen(color, penWidth);
    painter.setPen(pen);
    painter.setBrush(Qt::NoBrush);

    const QRect drawRect = roiRect.isValid() ? roiRect : result.rect().adjusted(1, 1, -1, -1);
    painter.drawRect(drawRect);
    return result;
}

QPixmap drawMatchPreview(const cv::Mat& haystack,
                         const cv::Mat& needle,
                         const MatchResult& match,
                         double threshold) {
    QPixmap base = matToPixmap(haystack);
    if (base.isNull()) {
        return {};
    }

    QPixmap annotated = drawRoiOverlay(base, base.rect().adjusted(1, 1, -2, -2));

    const cv::Size drawSize =
        match.matchedSize.width > 0 && match.matchedSize.height > 0 ? match.matchedSize
                                                                      : needle.size();

    QPainter painter(&annotated);
    painter.setRenderHint(QPainter::Antialiasing);

    if (match.found) {
        QPen pen(QColor(0, 220, 0), 2);
        painter.setPen(pen);
        painter.setBrush(QColor(0, 220, 0, 40));
        painter.drawRect(QRect(match.location.x,
                               match.location.y,
                               drawSize.width,
                               drawSize.height));
    } else if (!needle.empty() && match.confidence > 0.0) {
        QPen pen(QColor(220, 60, 60), 2, Qt::DashLine);
        painter.setPen(pen);
        painter.setBrush(QColor(220, 60, 60, 30));
        painter.drawRect(QRect(match.location.x,
                               match.location.y,
                               drawSize.width,
                               drawSize.height));
    }

    Q_UNUSED(threshold)
    return annotated;
}

} // namespace OpenCvQtUtil
