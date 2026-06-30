#pragma once

#include <QPixmap>

#include <opencv2/core.hpp>

namespace OpenCvQtUtil {

QPixmap matToPixmap(const cv::Mat& image);

} // namespace OpenCvQtUtil
