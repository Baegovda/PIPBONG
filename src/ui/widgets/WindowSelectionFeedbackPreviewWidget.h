#pragma once

#include "app/PointerFeedbackSettings.h"

#include <QWidget>

class QTimer;

/// Looped window-pick confirmation animation preview (in-dialog).
class WindowSelectionFeedbackPreviewWidget : public QWidget {
    Q_OBJECT
public:
    explicit WindowSelectionFeedbackPreviewWidget(QWidget* parent = nullptr);

    void setSettings(const WindowSelectionFeedbackSettings& settings);
    void restartAnimation();

protected:
    void paintEvent(QPaintEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;

private:
    void onTick();

    WindowSelectionFeedbackSettings m_settings;
    QTimer* m_timer = nullptr;
    qint64 m_startMs = 0;
};
