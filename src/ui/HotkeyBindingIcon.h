#pragma once

#include "core/input/HotkeyBinding.h"

#include <QPixmap>

class QPainter;
class QRect;

QPixmap hotkeyBindingIcon(const HotkeyBinding& binding, int size = 32);
void drawHotkeyBindingInRect(QPainter* painter,
                             const QRect& rect,
                             const HotkeyBinding& binding,
                             qreal opacity = 1.0);

// Browser-style chevron beside thumb-side paddles (32×32 logical coordinates).
void drawMouseSideNavigationIcon(QPainter& painter, bool forward);
