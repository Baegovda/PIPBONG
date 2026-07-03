#include "ui/TitleBarIconEasterEgg.h"

#include <QApplication>
#include <QColor>
#include <QCursor>
#include <QEasingCurve>
#include <QFont>
#include <QGuiApplication>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QPainter>
#include <QPauseAnimation>
#include <QPropertyAnimation>
#include <QPointer>
#include <QRadialGradient>
#include <QScreen>
#include <QSequentialAnimationGroup>

namespace {

QPointer<TitleBarIconEasterEgg> g_activeOverlay;

QRect targetScreenGeometry(QWidget* anchor) {
    QScreen* screen = nullptr;
    if (anchor && anchor->window()) {
        screen = anchor->window()->screen();
    }
    if (!screen) {
        screen = QGuiApplication::screenAt(QCursor::pos());
    }
    if (!screen) {
        screen = QGuiApplication::primaryScreen();
    }
    return screen ? screen->geometry() : QRect(0, 0, 1280, 720);
}

QPointF anchorCenterInScreen(QWidget* anchor, const QRect& screenRect) {
    if (!anchor) {
        return QPointF(screenRect.width() / 2.0, screenRect.height() * 0.06);
    }
    const QPoint globalCenter = anchor->mapToGlobal(anchor->rect().center());
    return QPointF(globalCenter.x() - screenRect.x(), globalCenter.y() - screenRect.y());
}

qreal easedBounce(qreal t) {
    return QEasingCurve(QEasingCurve::OutBounce).valueForProgress(qBound(0.0, t, 1.0));
}

qreal easedSnapIn(qreal t) {
    const qreal clamped = qBound(0.0, t, 1.0);
    const qreal bounce = QEasingCurve(QEasingCurve::OutBounce).valueForProgress(clamped);
    const qreal snap = QEasingCurve(QEasingCurve::InCubic).valueForProgress(clamped);
    return 0.35 * bounce + 0.65 * snap;
}

} // namespace

void TitleBarIconEasterEgg::play(const QIcon& icon, QWidget* anchor) {
    if (g_activeOverlay) {
        g_activeOverlay->close();
    }

    auto* overlay = new TitleBarIconEasterEgg(icon, anchor);
    g_activeOverlay = overlay;
    overlay->show();
    overlay->raise();

    auto* pop = new QPropertyAnimation(overlay, "popProgress", overlay);
    pop->setDuration(720);
    pop->setStartValue(0.0);
    pop->setEndValue(1.0);
    pop->setEasingCurve(QEasingCurve::OutCubic);

    auto* hold = new QPauseAnimation(280, overlay);

    auto* ret = new QPropertyAnimation(overlay, "returnProgress", overlay);
    ret->setDuration(920);
    ret->setStartValue(0.0);
    ret->setEndValue(1.0);
    ret->setEasingCurve(QEasingCurve::Linear);

    auto* sequence = new QSequentialAnimationGroup(overlay);
    sequence->addAnimation(pop);
    sequence->addAnimation(hold);
    sequence->addAnimation(ret);
    QObject::connect(sequence, &QSequentialAnimationGroup::finished, overlay, &QWidget::close);
    QObject::connect(overlay, &QObject::destroyed, sequence, &QObject::deleteLater);
    sequence->start();
}

TitleBarIconEasterEgg::TitleBarIconEasterEgg(const QIcon& icon, QWidget* anchor)
    : QWidget(nullptr) {
    setWindowFlags(Qt::Tool | Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint);
    setAttribute(Qt::WA_TranslucentBackground);
    setAttribute(Qt::WA_DeleteOnClose);
    setFocusPolicy(Qt::StrongFocus);
    setCursor(Qt::PointingHandCursor);

    const QRect screenRect = targetScreenGeometry(anchor);
    setGeometry(screenRect);
    m_anchorCenter = anchorCenterInScreen(anchor, screenRect);

    const qreal screenMin = qMin(screenRect.width(), screenRect.height());
    const qreal dpr =
        anchor && anchor->window() ? anchor->window()->devicePixelRatioF() : qApp->devicePixelRatio();
    const int pixmapSize = qMax(768, qRound(screenMin * 0.96 * dpr));
    m_icon = icon.pixmap(pixmapSize, pixmapSize);
    if (m_icon.isNull()) {
        m_icon = QPixmap(pixmapSize, pixmapSize);
        m_icon.fill(Qt::transparent);
        QPainter painter(&m_icon);
        painter.setRenderHint(QPainter::Antialiasing);
        painter.setBrush(QColor(56, 189, 248));
        painter.setPen(Qt::NoPen);
        painter.drawEllipse(m_icon.rect().adjusted(24, 24, -24, -24));
        painter.setPen(QPen(Qt::white, 18));
        painter.setFont(QFont(QStringLiteral("Segoe UI"), pixmapSize / 9, QFont::Black));
        painter.drawText(m_icon.rect(), Qt::AlignCenter, QStringLiteral("PIP"));
    }
}

void TitleBarIconEasterEgg::setPopProgress(qreal progress) {
    m_popProgress = qBound<qreal>(0.0, progress, 1.0);
    update();
}

void TitleBarIconEasterEgg::setReturnProgress(qreal progress) {
    m_returnProgress = qBound<qreal>(0.0, progress, 1.0);
    update();
}

void TitleBarIconEasterEgg::paintIcon(QPainter& painter) const {
    const QPointF screenCenter(width() / 2.0, height() / 2.0);
    const qreal screenMin = qMin(width(), height());
    const qreal minSize = 30.0;
    const qreal maxSize = screenMin * 0.94;

    qreal iconSize = minSize;
    QPointF iconCenter = m_anchorCenter;
    qreal glowStrength = 0.0;

    if (m_returnProgress > 0.0) {
        const qreal moveT = easedSnapIn(m_returnProgress);
        const qreal sizeT = easedBounce(m_returnProgress);
        iconSize = minSize + (maxSize - minSize) * (1.0 - sizeT);
        iconCenter = screenCenter + (m_anchorCenter - screenCenter) * moveT;
        glowStrength = 1.0 - moveT;
    } else {
        const qreal popT = easedBounce(m_popProgress);
        iconSize = minSize + (maxSize - minSize) * popT;
        iconCenter = m_anchorCenter + (screenCenter - m_anchorCenter) * popT;
        glowStrength = popT;
    }

    const QRectF target(iconCenter.x() - iconSize / 2.0,
                        iconCenter.y() - iconSize / 2.0,
                        iconSize,
                        iconSize);

    const qreal glowSize = iconSize * (1.08 + 0.05 * glowStrength);
    const QRectF glow(iconCenter.x() - glowSize / 2.0,
                      iconCenter.y() - glowSize / 2.0,
                      glowSize,
                      glowSize);
    QRadialGradient gradient(iconCenter, glowSize / 2.0);
    gradient.setColorAt(0.0, QColor::fromRgbF(0.22, 0.74, 0.97, 0.32 * glowStrength));
    gradient.setColorAt(0.62, QColor::fromRgbF(0.22, 0.74, 0.97, 0.10 * glowStrength));
    gradient.setColorAt(1.0, QColor::fromRgbF(0.22, 0.74, 0.97, 0.0));
    painter.setPen(Qt::NoPen);
    painter.setBrush(gradient);
    painter.drawEllipse(glow);

    painter.drawPixmap(target, m_icon, QRectF(m_icon.rect()));
}

void TitleBarIconEasterEgg::paintEvent(QPaintEvent* event) {
    Q_UNUSED(event);

    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setRenderHint(QPainter::SmoothPixmapTransform);

    qreal scrimAlpha = 0.0;
    if (m_returnProgress > 0.0) {
        scrimAlpha = (0.08 + 0.16) * (1.0 - m_returnProgress);
    } else {
        scrimAlpha = 0.08 + m_popProgress * 0.16;
    }
    painter.fillRect(rect(), QColor::fromRgbF(0.02, 0.04, 0.08, scrimAlpha));

    paintIcon(painter);
}

void TitleBarIconEasterEgg::mousePressEvent(QMouseEvent* event) {
    close();
    QWidget::mousePressEvent(event);
}

void TitleBarIconEasterEgg::keyPressEvent(QKeyEvent* event) {
    if (event->key() == Qt::Key_Escape) {
        close();
        return;
    }
    QWidget::keyPressEvent(event);
}
