#include "ui/OpenCvQtUtil.h"

#include <QImage>

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

} // namespace OpenCvQtUtil
