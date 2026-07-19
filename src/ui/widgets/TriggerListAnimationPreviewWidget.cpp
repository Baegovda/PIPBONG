#include "ui/widgets/TriggerListAnimationPreviewWidget.h"

#include "ui/TriggerListAnimationRenderer.h"
#include "ui/UiThemeColors.h"

#include <QPaintEvent>
#include <QPainter>
#include <QTimer>

TriggerListAnimationPreviewWidget::TriggerListAnimationPreviewWidget(QWidget* parent)
    : QWidget(parent) {
    setMinimumSize(180, 120);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    m_timer = new QTimer(this);
    m_timer->setInterval(55);
    connect(m_timer, &QTimer::timeout, this, &TriggerListAnimationPreviewWidget::onTick);
    m_timer->start();
}

void TriggerListAnimationPreviewWidget::setState(TriggerAnimationState state) {
    m_state = state;
    update();
}

void TriggerListAnimationPreviewWidget::setSettings(const TriggerListStateAnimationSettings& settings) {
    m_settings = settings;
    update();
}

void TriggerListAnimationPreviewWidget::restartAnimation() {
    m_animationPhase = 0;
    update();
}

void TriggerListAnimationPreviewWidget::paintEvent(QPaintEvent* event) {
    Q_UNUSED(event);
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, true);

    const QColor base = palette().color(QPalette::Base);
    const QColor window = palette().color(QPalette::Window);
    painter.fillRect(rect(), base);

    QColor frame = palette().color(QPalette::Mid);
    frame.setAlpha(120);
    painter.setPen(QPen(frame, 1));
    painter.setBrush(Qt::NoBrush);
    painter.drawRoundedRect(rect().adjusted(1, 1, -1, -1), 8, 8);

    const QRect previewRect = rect().adjusted(16, 16, -16, -16);
    if (m_state == TriggerAnimationState::Action
        && m_settings.style == TriggerListAnimationStyle::DefaultPrism) {
        QLinearGradient gradient(previewRect.topLeft(), previewRect.bottomRight());
        gradient.setColorAt(0.0, QColor(120, 180, 255, 80));
        gradient.setColorAt(1.0, QColor(220, 140, 255, 60));
        painter.fillRect(previewRect, gradient);
        painter.setPen(secondaryHintTextColor(palette()));
        painter.drawText(previewRect, Qt::AlignCenter, tr("기본 실행 프리즘"));
        return;
    }

    if (m_state == TriggerAnimationState::Cooldown
        && m_settings.style == TriggerListAnimationStyle::CountdownText) {
        painter.setPen(primaryContentTextColor(palette(), false));
        QFont font = painter.font();
        font.setPointSize(qMax(10, height() / 5));
        font.setBold(true);
        painter.setFont(font);
        painter.drawText(previewRect, Qt::AlignCenter, tr("3초"));
        return;
    }

    TriggerListAnimationRenderer::paintListIcon(&painter,
                                                previewRect,
                                                m_animationPhase,
                                                m_settings,
                                                m_state,
                                                palette());
}

void TriggerListAnimationPreviewWidget::onTick() {
    m_animationPhase = (m_animationPhase + 1) % 96;
    update();
}
