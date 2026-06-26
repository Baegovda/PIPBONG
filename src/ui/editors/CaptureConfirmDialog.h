#pragma once

#include <opencv2/core.hpp>

#include <QDialog>
#include <QRect>

class CaptureConfirmDialog : public QDialog {
    Q_OBJECT
public:
    explicit CaptureConfirmDialog(const cv::Mat& captured,
                                  const QRect& selectionRect,
                                  QWidget* parent = nullptr);
};
