#include "ui/editors/ImageFindPreviewDialog.h"

#include "ui/OpenCvQtUtil.h"
#include "ui/UiStrings.h"
#include "ui/editors/RoiPreviewWidget.h"

#include <QDialogButtonBox>
#include <QFormLayout>
#include <QLabel>
#include <QVBoxLayout>

ImageFindPreviewDialog::ImageFindPreviewDialog(const ImageFindRoiPreviewData& previewData,
                                                 SearchArea searchArea,
                                                 QWidget* parent)
    : QDialog(parent)
    , m_editableRoi(previewData.editableRoi) {
    setWindowTitle(tr("ROI 미리보기"));
    setupUi(previewData, searchArea);
    resize(720, 520);
}

void ImageFindPreviewDialog::setupUi(const ImageFindRoiPreviewData& previewData,
                                     SearchArea searchArea) {
    auto* layout = new QVBoxLayout(this);

    m_previewWidget = new RoiPreviewWidget(this);
    const QPixmap pixmap = OpenCvQtUtil::matToPixmap(previewData.displayImage);
    m_previewWidget->setImage(pixmap, previewData.roiRect, previewData.editableRoi);
    layout->addWidget(m_previewWidget, 1);

    auto* metadataForm = new QFormLayout();
    metadataForm->addRow(tr("탐색 영역"),
                         new QLabel(searchArea == SearchArea::TargetWindow
                                        ? tr("대상 창")
                                        : searchArea == SearchArea::FullScreen
                                              ? tr("전체 화면")
                                              : searchArea == SearchArea::ScreenPercent
                                                    ? tr("화면 비율 (%)")
                                                    : tr("사용자 영역"),
                                    this));
    metadataForm->addRow(tr("캡처 크기"),
                         new QLabel(tr("%1 x %2 px")
                                        .arg(previewData.displayImage.cols)
                                        .arg(previewData.displayImage.rows),
                                    this));
    metadataForm->addRow(tr("ROI"),
                         new QLabel(tr("x=%1, y=%2, w=%3, h=%4")
                                        .arg(previewData.roiRect.x())
                                        .arg(previewData.roiRect.y())
                                        .arg(previewData.roiRect.width())
                                        .arg(previewData.roiRect.height()),
                                    this));
    layout->addLayout(metadataForm);

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    localizeDialogButtons(buttons);
    layout->addWidget(buttons);

    m_customRegion.x = previewData.roiRect.x();
    m_customRegion.y = previewData.roiRect.y();
    m_customRegion.width = previewData.roiRect.width();
    m_customRegion.height = previewData.roiRect.height();

    connect(m_previewWidget, &RoiPreviewWidget::roiChanged, this, [this](const QRect& roiRect) {
        m_customRegion.x = roiRect.x();
        m_customRegion.y = roiRect.y();
        m_customRegion.width = roiRect.width();
        m_customRegion.height = roiRect.height();
    });
    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
}

CaptureRegion ImageFindPreviewDialog::customRegion() const {
    if (m_editableRoi && m_previewWidget) {
        const QRect roiRect = m_previewWidget->roiRect();
        CaptureRegion region;
        region.x = roiRect.x();
        region.y = roiRect.y();
        region.width = roiRect.width();
        region.height = roiRect.height();
        return region;
    }
    return m_customRegion;
}
