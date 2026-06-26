#pragma once

#include <opencv2/core.hpp>

#include <QDialog>
#include <QPixmap>

class CaptureCanvas;
class QLabel;
class QPushButton;

class TemplateCaptureDialog : public QDialog {
    Q_OBJECT
public:
    explicit TemplateCaptureDialog(const QString& projectDirectory, QWidget* parent = nullptr);

    QString savedRelativePath() const { return m_savedRelativePath; }

private slots:
    void onCaptureNow();
    void onSaveSelection();

private:
    void setupUi();
    cv::Mat captureForTemplate() const;
    void applyCapture(const cv::Mat& capture);
    void showCaptureFailure();

    QString m_projectDirectory;
    QString m_savedRelativePath;
    CaptureCanvas* m_canvas = nullptr;
    QLabel* m_statusLabel = nullptr;
    QPushButton* m_captureButton = nullptr;
    QPixmap m_capturePixmap;
};
