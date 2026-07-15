#include "ui/widgets/AnimatedTwoWaySwitch.h"

#include "ui/UiThemeColors.h"

#include <QEnterEvent>
#include <QMouseEvent>
#include <QPainter>
#include <QPropertyAnimation>
#include <QEasingCurve>

AnimatedTwoWaySwitch::AnimatedTwoWaySwitch(const QString& leftLabel,
                                           const QString& rightLabel,
                                           QWidget* parent)
    : QWidget(parent)
    , m_leftLabel(leftLabel)
    , m_rightLabel(rightLabel) {
    setCursor(Qt::PointingHandCursor);
    setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
}

void AnimatedTwoWaySwitch::setRightSelected(bool right, bool animate) {
    if (m_rightSelected == right && qFuzzyCompare(m_thumbPosition, right ? 1.0 : 0.0)) {
        return;
    }
    m_rightSelected = right;
    if (animate) {
        animateTo(right);
    } else {
        m_thumbPosition = right ? 1.0 : 0.0;
        update();
    }
    emit rightSelectedChanged(right);
}

void AnimatedTwoWaySwitch::setThumbPosition(qreal position) {
    m_thumbPosition = qBound(0.0, position, 1.0);
    update();
}

QSize AnimatedTwoWaySwitch::sizeHint() const {
    return {220, 36};
}

QSize AnimatedTwoWaySwitch::minimumSizeHint() const {
    return sizeHint();
}

QRectF AnimatedTwoWaySwitch::trackRect() const {
    constexpr qreal margin = 1.0;
    return QRectF(margin, margin, width() - margin * 2.0, height() - margin * 2.0);
}

QRectF AnimatedTwoWaySwitch::thumbRectForPosition(qreal position) const {
    const QRectF track = trackRect();
    const qreal thumbDiameter = track.height() - 4.0;
    const qreal minX = track.left() + 2.0;
    const qreal maxX = track.right() - 2.0 - thumbDiameter;
    const qreal thumbX = minX + (maxX - minX) * position;
    return QRectF(thumbX,
                  track.center().y() - thumbDiameter / 2.0,
                  thumbDiameter,
                  thumbDiameter);
}

void AnimatedTwoWaySwitch::animateTo(bool right) {
    if (m_animation) {
        m_animation->stop();
        m_animation->deleteLater();
        m_animation = nullptr;
    }

    auto* animation = new QPropertyAnimation(this, "thumbPosition", this);
    animation->setDuration(180);
    animation->setStartValue(m_thumbPosition);
    animation->setEndValue(right ? 1.0 : 0.0);
    animation->setEasingCurve(QEasingCurve::OutCubic);
    connect(animation, &QPropertyAnimation::finished, this, [this, animation]() {
        if (m_animation == animation) {
            m_animation = nullptr;
        }
    });
    m_animation = animation;
    animation->start(QAbstractAnimation::DeleteWhenStopped);
}

void AnimatedTwoWaySwitch::paintEvent(QPaintEvent* /*event*/) {
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);

    const QRectF track = trackRect();
    const qreal radius = track.height() / 2.0;

    const QColor trackColor = m_hovered ? palette().color(QPalette::Light)
                                        : palette().color(QPalette::Midlight);
    const QColor accentColor = palette().color(QPalette::Highlight);
    const QColor thumbColor = palette().color(QPalette::Window);
    const QColor inactiveText = secondaryHintTextColor(palette());

    painter.setPen(Qt::NoPen);
    painter.setBrush(trackColor);
    painter.drawRoundedRect(track, radius, radius);

    if (m_hovered) {
        painter.setPen(QPen(accentColor, 1.5));
        painter.setBrush(Qt::NoBrush);
        painter.drawRoundedRect(track.adjusted(0.75, 0.75, -0.75, -0.75), radius, radius);
    }

    QFont labelFont = font();
    labelFont.setBold(true);
    painter.setFont(labelFont);

    const QRectF leftLabelRect(track.left(), track.top(), track.width() / 2.0, track.height());
    const QRectF rightLabelRect(track.left() + track.width() / 2.0, track.top(), track.width() / 2.0, track.height());
    const bool leftActive = m_thumbPosition < 0.5;

    painter.setPen(inactiveText);
    painter.drawText(leftLabelRect, Qt::AlignCenter, m_leftLabel);
    painter.drawText(rightLabelRect, Qt::AlignCenter, m_rightLabel);

    const QRectF thumb = thumbRectForPosition(m_thumbPosition);
    painter.setPen(QPen(accentColor.darker(120), 1.0));
    painter.setBrush(thumbColor);
    painter.drawEllipse(thumb);

    painter.setPen(accentColor);
    painter.drawText(leftActive ? leftLabelRect : rightLabelRect,
                     Qt::AlignCenter,
                     leftActive ? m_leftLabel : m_rightLabel);
}

void AnimatedTwoWaySwitch::mousePressEvent(QMouseEvent* event) {
    if (event->button() != Qt::LeftButton) {
        QWidget::mousePressEvent(event);
        return;
    }

    const QRectF track = trackRect();
    const bool selectRight = event->position().x() >= track.center().x();
    if (selectRight != m_rightSelected) {
        setRightSelected(selectRight);
    }
    event->accept();
}

void AnimatedTwoWaySwitch::mouseReleaseEvent(QMouseEvent* event) {
    event->accept();
}

void AnimatedTwoWaySwitch::enterEvent(QEnterEvent* event) {
    QWidget::enterEvent(event);
    m_hovered = true;
    update();
}

void AnimatedTwoWaySwitch::leaveEvent(QEvent* event) {
    QWidget::leaveEvent(event);
    m_hovered = false;
    update();
}

void AnimatedTwoWaySwitch::resizeEvent(QResizeEvent* event) {
    QWidget::resizeEvent(event);
    update();
}
