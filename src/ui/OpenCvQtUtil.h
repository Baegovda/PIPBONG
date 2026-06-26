#pragma once

#include "core/vision/ImageMatcher.h"

#include <QColor>
#include <QPixmap>
#include <QRect>

#include <opencv2/core.hpp>

namespace OpenCvQtUtil {

QPixmap matToPixmap(const cv::Mat& image);

QPixmap drawRoiOverlay(const QPixmap& base,
                       const QRect& roiRect,
                       const QColor& color = QColor(0, 200, 0),
                       int penWidth = 2);

QPixmap drawMatchPreview(const cv::Mat& haystack,
                         const cv::Mat& needle,
                         const MatchResult& match,
                         double threshold);

} // namespace OpenCvQtUtil
