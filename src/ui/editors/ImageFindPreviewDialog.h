#pragma once

#include "core/workflow/blocks/ImageFindBlock.h"

#include <QDialog>

class RoiPreviewWidget;

class ImageFindPreviewDialog : public QDialog {
    Q_OBJECT
public:
    explicit ImageFindPreviewDialog(const ImageFindRoiPreviewData& previewData,
                                      SearchArea searchArea,
                                      QWidget* parent = nullptr);

    CaptureRegion customRegion() const;

private:
    void setupUi(const ImageFindRoiPreviewData& previewData, SearchArea searchArea);

    RoiPreviewWidget* m_previewWidget = nullptr;
    CaptureRegion m_customRegion;
    bool m_editableRoi = false;
};
