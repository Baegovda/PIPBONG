#pragma once

#include "model/TriggerListAnimationSettings.h"

#include <QWidget>

class QTimer;

class TriggerListAnimationPreviewWidget : public QWidget {
    Q_OBJECT
public:
    explicit TriggerListAnimationPreviewWidget(QWidget* parent = nullptr);

    void setState(TriggerAnimationState state);
    void setSettings(const TriggerListStateAnimationSettings& settings);
    void restartAnimation();

protected:
    void paintEvent(QPaintEvent* event) override;

private:
    void onTick();

    TriggerAnimationState m_state = TriggerAnimationState::Watch;
    TriggerListStateAnimationSettings m_settings =
        TriggerListStateAnimationSettings::watchDefaults();
    QTimer* m_timer = nullptr;
    int m_animationPhase = 0;
};
