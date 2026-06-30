#pragma once

#include "app/PointerFeedbackSettings.h"

#include <QWidget>

class QTimer;

/// Looped click pointer-feedback animation preview (in-dialog).
class ClickPointerFeedbackPreviewWidget : public QWidget {
    Q_OBJECT
public:
    explicit ClickPointerFeedbackPreviewWidget(QWidget* parent = nullptr);

    void setSettings(const ClickPointerFeedbackSettings& settings);
    void restartAnimation();

protected:
    void paintEvent(QPaintEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;

private:
    void onTick();

    ClickPointerFeedbackSettings m_settings;
    QTimer* m_timer = nullptr;
    qint64 m_startMs = 0;
};
