#include "ui/editors/TemplateCaptureDialog.h"

#include "core/capture/ScreenCapture.h"
#include "ui/editors/CaptureCanvas.h"

#include <QDateTime>
#include <QDialogButtonBox>
#include <QDir>
#include <QHBoxLayout>
#include <QLabel>
#include <QMessageBox>
#include <QPushButton>
#include <QScrollArea>
#include <QTimer>
#include <QVBoxLayout>

#include <opencv2/imgcodecs.hpp>
#include <opencv2/imgproc.hpp>

TemplateCaptureDialog::TemplateCaptureDialog(const QString& projectDirectory, QWidget* parent)
    : QDialog(parent)
    , m_projectDirectory(projectDirectory) {
    setWindowTitle(tr("Capture Template"));
    resize(900, 700);
    setupUi();
}

void TemplateCaptureDialog::setupUi() {
    auto* layout = new QVBoxLayout(this);

    layout->addWidget(new QLabel(
        tr("Click Capture Now, switch to the game window, then drag on the screenshot to select a template region."),
        this));

    m_statusLabel = new QLabel(tr("No capture yet. Click Capture Now."), this);
    layout->addWidget(m_statusLabel);

    auto* buttonRow = new QHBoxLayout();
    m_captureButton = new QPushButton(tr("Capture Now"), this);
    buttonRow->addWidget(m_captureButton);
    buttonRow->addStretch();
    layout->addLayout(buttonRow);

    m_canvas = new CaptureCanvas(this);

    auto* scroll = new QScrollArea(this);
    scroll->setWidget(m_canvas);
    scroll->setWidgetResizable(true);
    layout->addWidget(scroll, 1);

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Save | QDialogButtonBox::Cancel, this);
    layout->addWidget(buttons);

    connect(m_captureButton, &QPushButton::clicked, this, &TemplateCaptureDialog::onCaptureNow);
    connect(buttons, &QDialogButtonBox::accepted, this, &TemplateCaptureDialog::onSaveSelection);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
}

cv::Mat TemplateCaptureDialog::captureForTemplate() const {
    cv::Mat capture = ScreenCapture::captureTargetWindow();
    if (capture.empty() || ScreenCapture::isMostlyBlack(capture)) {
        capture = ScreenCapture::captureFullScreen();
    }
    if (!capture.empty() && ScreenCapture::isMostlyBlack(capture)) {
        return {};
    }
    return capture;
}

void TemplateCaptureDialog::applyCapture(const cv::Mat& capture) {
    if (capture.empty()) {
        m_capturePixmap = QPixmap();
        m_canvas->setPixmap(m_capturePixmap);
        m_statusLabel->setText(tr("Capture failed."));
        return;
    }

    cv::Mat rgb;
    cv::cvtColor(capture, rgb, cv::COLOR_BGR2RGB);
    QImage image(rgb.data, rgb.cols, rgb.rows, static_cast<int>(rgb.step), QImage::Format_RGB888);
    m_capturePixmap = QPixmap::fromImage(image.copy());
    m_canvas->setPixmap(m_capturePixmap);
    m_statusLabel->setText(tr("Capture OK (%1 x %2). Drag to select a region.")
                               .arg(m_capturePixmap.width())
                               .arg(m_capturePixmap.height()));
    m_captureButton->setText(tr("Recapture"));
}

void TemplateCaptureDialog::showCaptureFailure() {
    QMessageBox::warning(
        this,
        tr("Capture Template"),
        tr("Could not capture a usable image.\n\n"
           "Tips:\n"
           "- Use Find Window in the main window to verify the game title\n"
           "- Keep the game visible and not minimized\n"
           "- Prefer windowed or borderless window mode\n"
           "- Exclusive fullscreen may not be capturable"));
}

void TemplateCaptureDialog::onCaptureNow() {
    m_statusLabel->setText(tr("Capturing in 0.6s... switch to the game window now."));
    m_captureButton->setEnabled(false);

    setWindowState(windowState() | Qt::WindowMinimized);

    QTimer::singleShot(600, this, [this]() {
        const cv::Mat capture = captureForTemplate();
        setWindowState(windowState() & ~Qt::WindowMinimized);
        showNormal();
        activateWindow();
        raise();

        if (capture.empty()) {
            showCaptureFailure();
            m_statusLabel->setText(tr("Capture failed. Click Capture Now to retry."));
        } else {
            applyCapture(capture);
        }

        m_captureButton->setEnabled(true);
    });
}

void TemplateCaptureDialog::onSaveSelection() {
    const QRect selection = m_canvas->selectedRect();
    if (selection.width() < 2 || selection.height() < 2) {
        QMessageBox::warning(this, tr("Capture Template"), tr("Select a region first."));
        return;
    }

    if (m_capturePixmap.isNull()) {
        QMessageBox::warning(this, tr("Capture Template"), tr("Capture the screen first."));
        return;
    }

    QImage image = m_capturePixmap.toImage().convertToFormat(QImage::Format_RGB888);
    if (image.isNull()) {
        QMessageBox::critical(this, tr("Capture Template"), tr("Failed to read captured image."));
        return;
    }

    const cv::Rect roi(selection.x(), selection.y(), selection.width(), selection.height());
    if (roi.x + roi.width > image.width() || roi.y + roi.height > image.height()) {
        QMessageBox::warning(this, tr("Capture Template"), tr("Selection is outside captured image."));
        return;
    }

    QImage cropped = image.copy(roi.x, roi.y, roi.width, roi.height);
    cv::Mat bgr(cropped.height(), cropped.width(), CV_8UC3);
    for (int y = 0; y < cropped.height(); ++y) {
        const uchar* src = cropped.constScanLine(y);
        uchar* dst = bgr.ptr(y);
        for (int x = 0; x < cropped.width(); ++x) {
            dst[x * 3 + 0] = src[x * 3 + 2];
            dst[x * 3 + 1] = src[x * 3 + 1];
            dst[x * 3 + 2] = src[x * 3 + 0];
        }
    }

    const QString templatesDir = QDir(m_projectDirectory).filePath(QStringLiteral("templates"));
    QDir().mkpath(templatesDir);
    const QString fileName = QStringLiteral("capture_%1.png")
                                 .arg(QDateTime::currentDateTime().toString(QStringLiteral("yyyyMMdd_hhmmss")));
    const QString absolutePath = QDir(templatesDir).filePath(fileName);

    if (!cv::imwrite(absolutePath.toStdString(), bgr)) {
        QMessageBox::critical(this, tr("Capture Template"), tr("Failed to save template image."));
        return;
    }

    m_savedRelativePath = QDir(m_projectDirectory).relativeFilePath(absolutePath);
    accept();
}
