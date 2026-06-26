#include "ui/HotkeyBindingIcon.h"

#include "core/input/InputSimulator.h"

#include <QPainter>
#include <QKeySequence>
#include <QPainterPath>
#include <QStyle>

#ifdef _WIN32
#include <windows.h>
#endif

void drawMouseSideNavigationIcon(QPainter& painter, bool forward) {
    const QColor accent(58, 128, 210);
    const QColor accentDark = accent.darker(125);
    const QColor inactiveFill(210, 210, 210);
    const QColor inactiveBorder(160, 160, 160);

    // Thumb-side buttons sit on the left; leave room for a direction chevron badge.
    const QRectF body(12.0, 5.0, 14.0, 20.0);
    painter.setPen(QPen(QColor(130, 130, 130), 1.0));
    painter.setBrush(QColor(238, 238, 238));
    painter.drawRoundedRect(body, 3.5, 3.5);
    painter.drawLine(body.center().x(), body.top() + 1.0, body.center().x(), body.top() + body.height() * 0.52);

    const qreal paddleW = 4.0;
    const qreal paddleH = 5.5;
    const QRectF backPaddle(body.left() - 3.5, body.top() + 6.0, paddleW, paddleH);
    const QRectF forwardPaddle(body.left() - 3.5, body.top() + 13.5, paddleW, paddleH);
    const QRectF inactivePaddle = forward ? backPaddle : forwardPaddle;
    const QRectF activePaddle = forward ? forwardPaddle : backPaddle;

    painter.setPen(QPen(inactiveBorder, 0.9));
    painter.setBrush(inactiveFill);
    painter.drawRoundedRect(inactivePaddle, 1.5, 1.5);

    painter.setPen(QPen(accentDark, 1.0));
    painter.setBrush(accent);
    painter.drawRoundedRect(activePaddle, 1.5, 1.5);

    // Chevron badge on the far left: ← back, → forward (browser navigation direction).
    const QRectF badge(forward ? QRectF(1.0, 12.0, 10.0, 13.0) : QRectF(1.0, 5.0, 10.0, 13.0));
    painter.setPen(QPen(accentDark, 1.1));
    painter.setBrush(accent.lighter(108));
    painter.drawRoundedRect(badge, 3.0, 3.0);

    const qreal cx = badge.center().x();
    const qreal cy = badge.center().y();
    painter.setPen(QPen(Qt::white, 2.0, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
    painter.setBrush(Qt::NoBrush);
    if (forward) {
        painter.drawLine(QPointF(cx - 2.5, cy), QPointF(cx + 2.5, cy));
        painter.drawLine(QPointF(cx + 2.5, cy), QPointF(cx - 0.5, cy - 3.5));
        painter.drawLine(QPointF(cx + 2.5, cy), QPointF(cx - 0.5, cy + 3.5));
    } else {
        painter.drawLine(QPointF(cx + 2.5, cy), QPointF(cx - 2.5, cy));
        painter.drawLine(QPointF(cx - 2.5, cy), QPointF(cx + 0.5, cy - 3.5));
        painter.drawLine(QPointF(cx - 2.5, cy), QPointF(cx + 0.5, cy + 3.5));
    }
}

namespace {

constexpr int kLogicalIconSize = 32;

QRectF mouseBodyRect() {
    return QRectF(8.0, 4.0, 16.0, 24.0);
}

void drawMouseBody(QPainter& painter, const QRectF& body) {
    painter.setPen(QPen(QColor(90, 90, 90), 1.2));
    painter.setBrush(QColor(245, 245, 245));
    painter.drawRoundedRect(body, 4.0, 4.0);
    painter.drawLine(body.center().x(), body.top() + 1.0, body.center().x(), body.top() + body.height() * 0.55);
    painter.drawLine(body.left() + 2.0,
                     body.top() + body.height() * 0.55,
                     body.right() - 2.0,
                     body.top() + body.height() * 0.55);
}

void drawMouseHotkeyIcon(QPainter& painter, int virtualKey) {
    const QRectF body = mouseBodyRect();
    const QColor accent(74, 144, 217);

    switch (virtualKey) {
#ifdef _WIN32
    case VK_LBUTTON: {
        drawMouseBody(painter, body);
        const QRectF leftButton(body.left(), body.top(), body.width() * 0.5, body.height() * 0.55);
        painter.setPen(Qt::NoPen);
        painter.setBrush(accent);
        painter.drawRoundedRect(leftButton, 3.0, 3.0);
        break;
    }
    case VK_RBUTTON: {
        drawMouseBody(painter, body);
        const QRectF rightButton(body.left() + body.width() * 0.5, body.top(), body.width() * 0.5, body.height() * 0.55);
        painter.setPen(Qt::NoPen);
        painter.setBrush(accent);
        painter.drawRoundedRect(rightButton, 3.0, 3.0);
        break;
    }
    case VK_MBUTTON: {
        drawMouseBody(painter, body);
        const QRectF wheelSlot(body.center().x() - 2.5, body.top() + body.height() * 0.58, 5.0, 7.0);
        painter.setPen(QPen(QColor(90, 90, 90), 1.0));
        painter.setBrush(accent);
        painter.drawRoundedRect(wheelSlot, 2.0, 2.0);
        break;
    }
    case VK_XBUTTON1:
        drawMouseSideNavigationIcon(painter, false);
        break;
    case VK_XBUTTON2:
        drawMouseSideNavigationIcon(painter, true);
        break;
#endif
    default:
        drawMouseBody(painter, body);
        break;
    }
}

QString compactKeyLabel(int virtualKey) {
#ifdef _WIN32
    if (virtualKey >= 'A' && virtualKey <= 'Z') {
        return QString(QChar(virtualKey));
    }
    if (virtualKey >= '0' && virtualKey <= '9') {
        return QString(QChar(virtualKey));
    }
    switch (virtualKey) {
    case VK_SPACE:
        return QStringLiteral("Sp");
    case VK_RETURN:
        return QStringLiteral("↵");
    case VK_ESCAPE:
        return QStringLiteral("Esc");
    case VK_TAB:
        return QStringLiteral("Tab");
    case VK_BACK:
        return QStringLiteral("⌫");
    case VK_DELETE:
        return QStringLiteral("Del");
    case VK_HOME:
        return QStringLiteral("Hm");
    case VK_END:
        return QStringLiteral("End");
    case VK_PRIOR:
        return QStringLiteral("PU");
    case VK_NEXT:
        return QStringLiteral("PD");
    case VK_LEFT:
        return QStringLiteral("←");
    case VK_RIGHT:
        return QStringLiteral("→");
    case VK_UP:
        return QStringLiteral("↑");
    case VK_DOWN:
        return QStringLiteral("↓");
    case VK_F1:
    case VK_F2:
    case VK_F3:
    case VK_F4:
    case VK_F5:
    case VK_F6:
    case VK_F7:
    case VK_F8:
    case VK_F9:
    case VK_F10:
    case VK_F11:
    case VK_F12:
        return QStringLiteral("F%1").arg(virtualKey - VK_F1 + 1);
    default: {
        const int qtKey = HotkeyBinding::virtualKeyToQtKey(virtualKey);
        const QString native = QKeySequence(qtKey).toString(QKeySequence::NativeText);
        if (!native.isEmpty() && native.size() <= 4) {
            return native;
        }
        return QStringLiteral("K");
    }
    }
#else
    (void)virtualKey;
    return QStringLiteral("K");
#endif
}

QString modifierPrefix(const KeyModifiers& modifiers) {
    QStringList parts;
    if (modifiers.ctrl) {
        parts << QStringLiteral("C");
    }
    if (modifiers.alt) {
        parts << QStringLiteral("A");
    }
    if (modifiers.shift) {
        parts << QStringLiteral("S");
    }
    return parts.join(QStringLiteral("+"));
}

void drawKeyboardHotkeyIcon(QPainter& painter, int virtualKey, const KeyModifiers& modifiers) {
    const QRectF cap(3.0, 5.0, 26.0, 22.0);
    painter.setPen(QPen(QColor(100, 100, 100), 1.1));
    painter.setBrush(QColor(248, 248, 248));
    painter.drawRoundedRect(cap, 3.0, 3.0);

    const QString modText = modifierPrefix(modifiers);
    const QString keyText = compactKeyLabel(virtualKey);

    if (!modText.isEmpty()) {
        QFont modFont = painter.font();
        modFont.setPointSize(6);
        modFont.setBold(true);
        painter.setFont(modFont);
        painter.setPen(QColor(90, 120, 160));
        painter.drawText(QRectF(cap.left(), cap.top() + 1.0, cap.width(), 8.0),
                         Qt::AlignHCenter | Qt::AlignTop,
                         modText);
    }

    QFont keyFont = painter.font();
    keyFont.setPointSize(keyText.size() > 2 ? 7 : 10);
    keyFont.setBold(true);
    painter.setFont(keyFont);
    painter.setPen(QColor(50, 50, 50));
    const qreal textTop = modText.isEmpty() ? cap.top() : cap.top() + 8.0;
    painter.drawText(QRectF(cap.left(), textTop, cap.width(), cap.bottom() - textTop),
                     Qt::AlignCenter,
                     keyText);
}

void paintHotkeyBinding(QPainter& painter, const HotkeyBinding& binding) {
#ifdef _WIN32
    if (HotkeyBinding::isMouseVirtualKey(binding.virtualKey)) {
        drawMouseHotkeyIcon(painter, binding.virtualKey);
        return;
    }
#endif
    KeyModifiers modifiers;
    modifiers.ctrl = binding.ctrl;
    modifiers.alt = binding.alt;
    modifiers.shift = binding.shift;
    drawKeyboardHotkeyIcon(painter, binding.virtualKey, modifiers);
}

} // namespace

QPixmap hotkeyBindingIcon(const HotkeyBinding& binding, int size) {
    if (binding.isEmpty() || size <= 0) {
        return {};
    }

    QPixmap pixmap(size, size);
    pixmap.fill(Qt::transparent);

    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setRenderHint(QPainter::TextAntialiasing);
    const qreal scale = static_cast<qreal>(size) / static_cast<qreal>(kLogicalIconSize);
    painter.scale(scale, scale);
    paintHotkeyBinding(painter, binding);
    return pixmap;
}

void drawHotkeyBindingInRect(QPainter* painter,
                             const QRect& rect,
                             const HotkeyBinding& binding,
                             qreal opacity) {
    if (!painter || binding.isEmpty() || rect.isEmpty()) {
        return;
    }

    const int iconSize = qBound(16, qMin(rect.width(), rect.height()) - 2, 32);
    const QPixmap icon = hotkeyBindingIcon(binding, iconSize);
    if (icon.isNull()) {
        return;
    }

    const QRect target =
        QStyle::alignedRect(Qt::LeftToRight, Qt::AlignCenter, icon.size(), rect);
    if (opacity < 1.0) {
        painter->setOpacity(opacity);
    }
    painter->drawPixmap(target, icon);
    if (opacity < 1.0) {
        painter->setOpacity(1.0);
    }
}
