#include "ui/TriggerListAnimationRenderer.h"

#include "ui/FeatureRunModeTheme.h"

#include <QPainter>
#include <QPainterPath>
#include <QtMath>

#include <cmath>

namespace TriggerListAnimationRenderer {
namespace {

constexpr int kAnimationCycle = 96;

QRect centeredSquareInRect(const QRect& rect, int margin = 4) {
    const int side = qBound(12, qMin(rect.width(), rect.height()) - margin, 18);
    return QRect(rect.left() + (rect.width() - side) / 2,
                 rect.top() + (rect.height() - side) / 2,
                 side,
                 side);
}

void paintSonarRadar(QPainter* painter,
                     const QRect& iconRect,
                     int animationPhase,
                     const QColor& base,
                     bool showRipples,
                     bool showSweep) {
    const QPointF center = iconRect.center();
    const qreal maxRadius = iconRect.width() * 0.48;

    if (showRipples) {
        const auto drawSonarRipple = [&](int phaseOffset, qreal startRadiusFactor, qreal endRadiusFactor) {
            const qreal t = static_cast<qreal>((animationPhase + phaseOffset) % 48) / 48.0;
            const qreal eased = 1.0 - (1.0 - t) * (1.0 - t);
            const qreal radius = maxRadius * (startRadiusFactor + (endRadiusFactor - startRadiusFactor) * eased);
            QColor ripple = base;
            ripple.setAlpha(static_cast<int>((1.0 - t) * 150.0));
            QPen pen(ripple, qMax(1.0, iconRect.width() * 0.09));
            pen.setCapStyle(Qt::RoundCap);
            painter->setPen(pen);
            painter->setBrush(Qt::NoBrush);
            painter->drawEllipse(center, radius, radius);
        };
        drawSonarRipple(0, 0.28, 0.98);
        drawSonarRipple(24, 0.28, 0.98);
    }

    QColor ring = base;
    ring.setAlpha(qMin(255, base.alpha() + 35));
    QPen ringPen(ring, qMax(1.0, iconRect.width() * 0.08));
    ringPen.setCapStyle(Qt::RoundCap);
    painter->setPen(ringPen);
    painter->setBrush(Qt::NoBrush);
    painter->drawEllipse(center, maxRadius * 0.42, maxRadius * 0.42);

    if (showSweep) {
        const qreal sweepDeg = static_cast<qreal>(animationPhase % kAnimationCycle) / kAnimationCycle * 360.0;
        QColor sweep = base;
        sweep.setAlpha(static_cast<int>(base.alpha() * 0.55));
        QPen sweepPen(sweep, qMax(1.5, iconRect.width() * 0.11));
        sweepPen.setCapStyle(Qt::RoundCap);
        painter->setPen(sweepPen);
        const QRectF arcRect(center.x() - maxRadius * 0.9,
                             center.y() - maxRadius * 0.9,
                             maxRadius * 1.8,
                             maxRadius * 1.8);
        painter->drawArc(arcRect, static_cast<int>((90.0 - sweepDeg) * 16.0), 52 * 16);
    }

    const qreal dotPulse = 0.65 + 0.35 * std::sin(animationPhase * M_PI / 18.0);
    QColor dot = base;
    dot.setAlpha(static_cast<int>(dotPulse * 255.0));
    painter->setPen(Qt::NoPen);
    painter->setBrush(dot);
    const qreal dotRadius = qMax(1.2, iconRect.width() * 0.11 * dotPulse);
    painter->drawEllipse(center, dotRadius, dotRadius);
}

void paintStaticBullseye(QPainter* painter, const QRect& iconRect, const QColor& base) {
    const QPointF center = iconRect.center();
    const qreal outer = iconRect.width() * 0.42;
    const qreal inner = iconRect.width() * 0.22;
    QPen pen(base, qMax(1.0, iconRect.width() * 0.08));
    pen.setCapStyle(Qt::RoundCap);
    painter->setPen(pen);
    painter->setBrush(Qt::NoBrush);
    painter->drawEllipse(center, outer, outer);
    painter->drawEllipse(center, inner, inner);
}

void paintSpinningArc(QPainter* painter, const QRect& iconRect, int animationPhase, const QColor& base) {
    QPen arcPen(base, qMax(1.5, iconRect.width() * 0.12));
    arcPen.setCapStyle(Qt::RoundCap);
    painter->setPen(arcPen);
    painter->setBrush(Qt::NoBrush);
    const QRect arcRect = iconRect.adjusted(2, 2, -2, -2);
    const int startAngle = static_cast<int>(animationPhase * 15 * 16);
    painter->drawArc(arcRect, startAngle, 270 * 16);
}

void paintPulseRing(QPainter* painter, const QRect& iconRect, int animationPhase, const QColor& base) {
    const QPointF center = iconRect.center();
    const qreal pulse = 0.55 + 0.45 * std::sin(animationPhase * M_PI / 24.0);
    const qreal radius = iconRect.width() * 0.2 + iconRect.width() * 0.24 * pulse;
    QColor ring = base;
    ring.setAlpha(static_cast<int>((0.45 + 0.55 * pulse) * 220.0));
    QPen pen(ring, qMax(1.2, iconRect.width() * 0.1));
    pen.setCapStyle(Qt::RoundCap);
    painter->setPen(pen);
    painter->setBrush(Qt::NoBrush);
    painter->drawEllipse(center, radius, radius);
    QColor dot = base;
    dot.setAlpha(static_cast<int>(pulse * 255.0));
    painter->setPen(Qt::NoPen);
    painter->setBrush(dot);
    painter->drawEllipse(center, qMax(1.2, iconRect.width() * 0.08), qMax(1.2, iconRect.width() * 0.08));
}

} // namespace

int scaledAnimationPhase(int animationPhase, double speed) {
    return static_cast<int>(animationPhase * speed) % kAnimationCycle;
}

QColor resolvedAccentColor(TriggerAnimationState state,
                           const TriggerListStateAnimationSettings& settings) {
    if (settings.accentColor.isValid()) {
        return settings.accentColor;
    }
    switch (state) {
    case TriggerAnimationState::Watch:
        return featureRunModeTriggerWatchAccent(true);
    case TriggerAnimationState::Cooldown:
        return featureRunModeTriggerCooldownAccent(true);
    case TriggerAnimationState::Action:
        return featureRunModeTriggerWatchAccent(true);
    }
    return featureRunModeTriggerWatchAccent(true);
}

void paintListIcon(QPainter* painter,
                   const QRect& rect,
                   int animationPhase,
                   const TriggerListStateAnimationSettings& settings,
                   TriggerAnimationState state,
                   const QPalette& palette) {
    if (settings.style == TriggerListAnimationStyle::CountdownText
        || settings.style == TriggerListAnimationStyle::DefaultPrism) {
        return;
    }

    const QRect iconRect = centeredSquareInRect(rect);
    const int phase = scaledAnimationPhase(animationPhase, settings.speed);
    QColor base = resolvedAccentColor(state, settings);
    if (!settings.accentColor.isValid()) {
        const qreal pulse = 0.72 + 0.28 * std::sin(phase * M_PI / 36.0);
        base.setAlpha(static_cast<int>(pulse * 220.0));
    }

    painter->save();
    painter->setRenderHint(QPainter::Antialiasing, true);

    switch (settings.style) {
    case TriggerListAnimationStyle::SonarRadar:
        paintSonarRadar(painter, iconRect, phase, base, settings.showSonarRipples, settings.showRadarSweep);
        break;
    case TriggerListAnimationStyle::StaticBullseye:
        paintStaticBullseye(painter, iconRect, base);
        break;
    case TriggerListAnimationStyle::SpinningArc:
        paintSpinningArc(painter, iconRect, phase, base);
        break;
    case TriggerListAnimationStyle::PulseRing:
        paintPulseRing(painter, iconRect, phase, base);
        break;
    case TriggerListAnimationStyle::CountdownText:
    case TriggerListAnimationStyle::DefaultPrism:
        break;
    }

    painter->restore();
}

void paintCooldownRunButton(QPainter* painter,
                            const QRect& runColumnRect,
                            qint64 remainingMs,
                            int animationPhase,
                            const TriggerListStateAnimationSettings& settings,
                            const QPalette& palette) {
    const int side = qBound(14, qMin(runColumnRect.width(), runColumnRect.height()) - 4, 22);
    const QRect btnRect(runColumnRect.left() + (runColumnRect.width() - side) / 2,
                        runColumnRect.top() + (runColumnRect.height() - side) / 2,
                        side,
                        side);
    const int phase = scaledAnimationPhase(animationPhase, settings.speed);
    QColor spinColor = resolvedAccentColor(TriggerAnimationState::Cooldown, settings);
    if (!settings.accentColor.isValid()) {
        const qreal pulse = 0.62 + 0.28 * std::sin(phase * M_PI / 24.0);
        spinColor.setAlpha(static_cast<int>(130 + pulse * 90));
    }

    painter->save();
    painter->setRenderHint(QPainter::Antialiasing, true);

    QColor track = palette.color(QPalette::Mid);
    track.setAlpha(72);
    painter->setPen(Qt::NoPen);
    painter->setBrush(track);
    painter->drawEllipse(btnRect);

    switch (settings.style) {
    case TriggerListAnimationStyle::PulseRing:
        paintPulseRing(painter, btnRect, phase, spinColor);
        break;
    case TriggerListAnimationStyle::StaticBullseye:
        paintStaticBullseye(painter, btnRect, spinColor);
        break;
    case TriggerListAnimationStyle::SonarRadar:
        paintSonarRadar(painter, btnRect, phase, spinColor, settings.showSonarRipples, settings.showRadarSweep);
        break;
    case TriggerListAnimationStyle::CountdownText:
    case TriggerListAnimationStyle::SpinningArc:
    default:
        paintSpinningArc(painter, btnRect, phase, spinColor);
        break;
    }

    const int remainSec = static_cast<int>((qMax<qint64>(0, remainingMs) + 999) / 1000);
    QFont font = painter->font();
    font.setPointSize(qMax(7, btnRect.height() / 3));
    font.setBold(true);
    painter->setFont(font);
    painter->setPen(spinColor);
    painter->drawText(btnRect, Qt::AlignCenter, QString::number(remainSec));

    painter->restore();
}

qreal rowPulseFactor(TriggerAnimationState state,
                     int animationPhase,
                     const TriggerListStateAnimationSettings& settings) {
    const int phase = scaledAnimationPhase(animationPhase, settings.speed);
    const qreal basePeriod = state == TriggerAnimationState::Watch ? 36.0 : 24.0;
    const qreal wave = 0.5 + 0.5 * std::sin(phase * M_PI / basePeriod);
    if (state == TriggerAnimationState::Cooldown) {
        // Keep cooldown breathing visible — never fade highlights/text to near-black.
        return 0.62 + 0.38 * wave;
    }
    return wave;
}

} // namespace TriggerListAnimationRenderer
