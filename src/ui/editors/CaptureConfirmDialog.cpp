#include "ui/editors/CaptureConfirmDialog.h"

#include "ui/OpenCvQtUtil.h"
#include "ui/UiStrings.h"

#include <QDialogButtonBox>
#include <QFormLayout>
#include <QFrame>
#include <QLabel>
#include <QVBoxLayout>

#include <opencv2/imgcodecs.hpp>
#include <opencv2/imgproc.hpp>

namespace {

double meanBrightness(const cv::Mat& captured) {
    if (captured.empty()) {
        return 0.0;
    }

    cv::Mat gray;
    if (captured.channels() == 1) {
        gray = captured;
    } else if (captured.channels() == 4) {
        cv::cvtColor(captured, gray, cv::COLOR_BGRA2GRAY);
    } else {
        cv::cvtColor(captured, gray, cv::COLOR_BGR2GRAY);
    }
    return cv::mean(gray)[0];
}

qint64 estimatedPngBytes(const cv::Mat& captured) {
    if (captured.empty()) {
        return 0;
    }

    std::vector<uchar> buffer;
    if (!cv::imencode(".png", captured, buffer)) {
        return 0;
    }
    return static_cast<qint64>(buffer.size());
}

QString formatByteSize(qint64 bytes) {
    if (bytes >= 1024 * 1024) {
        return CaptureConfirmDialog::tr("%1 MB").arg(bytes / (1024.0 * 1024.0), 0, 'f', 2);
    }
    if (bytes >= 1024) {
        return CaptureConfirmDialog::tr("%1 KB").arg(bytes / 1024.0, 0, 'f', 1);
    }
    return CaptureConfirmDialog::tr("%1 bytes").arg(bytes);
}

QString channelDescription(int channels) {
    switch (channels) {
    case 1:
        return CaptureConfirmDialog::tr("그레이스케일 (1)");
    case 3:
        return CaptureConfirmDialog::tr("RGB/BGR (3)");
    case 4:
        return CaptureConfirmDialog::tr("RGBA/BGRA (4)");
    default:
        return CaptureConfirmDialog::tr("%1").arg(channels);
    }
}

} // namespace

CaptureConfirmDialog::CaptureConfirmDialog(const cv::Mat& captured,
                                           const QRect& selectionRect,
                                           QWidget* parent)
    : QDialog(parent) {
    setWindowTitle(tr("템플릿 캡처 확인"));

    auto* layout = new QVBoxLayout(this);

    auto* previewLabel = new QLabel(this);
    previewLabel->setMinimumSize(320, 180);
    previewLabel->setAlignment(Qt::AlignCenter);
    previewLabel->setFrameShape(QFrame::Box);

    const QPixmap pixmap = OpenCvQtUtil::matToPixmap(captured);
    if (pixmap.isNull()) {
        previewLabel->setText(tr("미리보기를 표시할 수 없습니다."));
    } else {
        previewLabel->setPixmap(
            pixmap.scaled(480, 360, Qt::KeepAspectRatio, Qt::SmoothTransformation));
    }
    layout->addWidget(previewLabel);

    const qint64 pngBytes = estimatedPngBytes(captured);
    auto* metadataForm = new QFormLayout();
    metadataForm->addRow(tr("크기"),
                         new QLabel(tr("%1 x %2 px").arg(captured.cols).arg(captured.rows), this));
    metadataForm->addRow(tr("채널"), new QLabel(channelDescription(captured.channels()), this));
    metadataForm->addRow(tr("형식"), new QLabel(QStringLiteral("PNG"), this));
    metadataForm->addRow(tr("예상 파일 크기"),
                         new QLabel(tr("%1 (%2 bytes)")
                                        .arg(formatByteSize(pngBytes))
                                        .arg(pngBytes),
                                    this));
    metadataForm->addRow(tr("선택 영역"),
                         new QLabel(tr("x=%1, y=%2, w=%3, h=%4")
                                        .arg(selectionRect.x())
                                        .arg(selectionRect.y())
                                        .arg(selectionRect.width())
                                        .arg(selectionRect.height()),
                                    this));
    metadataForm->addRow(tr("평균 밝기"),
                         new QLabel(tr("%1 / 255").arg(meanBrightness(captured), 0, 'f', 1), this));
    layout->addLayout(metadataForm);

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    localizeDialogButtons(buttons);
    layout->addWidget(buttons);

    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);

    resize(520, 560);
}
