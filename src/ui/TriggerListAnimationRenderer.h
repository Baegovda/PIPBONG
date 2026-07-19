#pragma once

#include "model/TriggerListAnimationSettings.h"

#include <QPalette>
#include <QRect>

class QPainter;

namespace TriggerListAnimationRenderer {

int scaledAnimationPhase(int animationPhase, double speed);

QColor resolvedAccentColor(TriggerAnimationState state,
                           const TriggerListStateAnimationSettings& settings);

void paintListIcon(QPainter* painter,
                   const QRect& rect,
                   int animationPhase,
                   const TriggerListStateAnimationSettings& settings,
                   TriggerAnimationState state,
                   const QPalette& palette);

void paintCooldownRunButton(QPainter* painter,
                            const QRect& runColumnRect,
                            qint64 remainingMs,
                            int animationPhase,
                            const TriggerListStateAnimationSettings& settings,
                            const QPalette& palette);

qreal rowPulseFactor(TriggerAnimationState state,
                     int animationPhase,
                     const TriggerListStateAnimationSettings& settings);

} // namespace TriggerListAnimationRenderer
