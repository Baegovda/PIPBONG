#include "ui/widgets/ListColumnHeaderWidget.h"

#include "ui/UiResizeHandle.h"
#include "ui/UiThemeColors.h"

#include <QEnterEvent>
#include <QEvent>
#include <QMouseEvent>
#include <climits>
#include <QPainter>
#include <QPalette>

namespace {

bool darkThemeFromPalette(const QPalette& pal) {
    const QColor window = pal.color(QPalette::Window);
    if (window.isValid()) {
        return window.lightness() < 128;
    }
    return pal.color(QPalette::WindowText).lightness() > 128;
}

} // namespace

ListColumnHeaderWidget::ListColumnHeaderWidget(QWidget* parent)
    : QWidget(parent) {
    setMouseTracking(true);
    setAttribute(Qt::WA_Hover, true);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    updateHeight();
}

void ListColumnHeaderWidget::setRowHeightProvider(RowHeightFn fn) {
    m_rowHeight = std::move(fn);
    updateHeight();
}

void ListColumnHeaderWidget::setPaintLabelsProvider(PaintLabelsFn fn) {
    m_paintLabels = std::move(fn);
}

void ListColumnHeaderWidget::setDividerXsProvider(DividerXsFn fn) {
    m_dividerXs = std::move(fn);
}

void ListColumnHeaderWidget::setDividerHitProvider(DividerHitFn fn) {
    m_dividerHit = std::move(fn);
}

void ListColumnHeaderWidget::setApplyDragProvider(ApplyDragFn fn) {
    m_applyDrag = std::move(fn);
}

void ListColumnHeaderWidget::syncToLayout() {
    updateHeight();
    update();
}

void ListColumnHeaderWidget::paintHeaderChrome(QPainter* painter, const QRect& rect, const QPalette& pal) {
    QColor headerBg = pal.color(QPalette::Button);
    if (!headerBg.isValid()) {
        headerBg = pal.color(QPalette::AlternateBase);
    }
    if (darkThemeFromPalette(pal)) {
        headerBg = headerBg.darker(108);
    } else {
        headerBg = headerBg.lighter(102);
    }
    painter->fillRect(rect, headerBg);

    QColor bottomLine = pal.color(QPalette::Mid);
    if (darkThemeFromPalette(pal) && bottomLine.lightness() < 90) {
        bottomLine = bottomLine.lighter(140);
    }
    painter->setPen(QPen(bottomLine, 1));
    painter->drawLine(rect.left(), rect.bottom(), rect.right(), rect.bottom());
}

void ListColumnHeaderWidget::paintDividerGuides(QPainter* painter,
                                                const QList<int>& dividerXs,
                                                int height,
                                                const QPalette& pal) {
    const bool dark = darkThemeFromPalette(pal);
    QColor lineColor = pal.color(QPalette::Mid);
    if (dark && lineColor.lightness() < 90) {
        lineColor = lineColor.lighter(150);
    } else if (!dark) {
        lineColor = lineColor.darker(115);
    }
    lineColor.setAlpha(dark ? 170 : 150);

    const int lineTop = 3;
    const int lineBottom = qMax(lineTop + 1, height - 3);
    painter->setPen(QPen(lineColor, 1));
    int lastX = INT_MIN;
    for (int x : dividerXs) {
        if (x <= 0 || x == lastX) {
            continue;
        }
        lastX = x;
        painter->drawLine(x, lineTop, x, lineBottom);
    }

    QColor pipeColor = headerTextColor(pal);
    pipeColor.setAlpha(dark ? 150 : 130);
    painter->setPen(pipeColor);
    QFont pipeFont = painter->font();
    pipeFont.setPointSize(qMax(8, pipeFont.pointSize() - 1));
    pipeFont.setBold(false);
    painter->setFont(pipeFont);
    const QString pipe = QStringLiteral("|");
    const QFontMetrics metrics(pipeFont);
    const int pipeBaseline = (height + metrics.ascent() - metrics.descent()) / 2;
    lastX = INT_MIN;
    for (int x : dividerXs) {
        if (x <= 0 || x == lastX) {
            continue;
        }
        lastX = x;
        painter->drawText(x - metrics.horizontalAdvance(pipe) / 2, pipeBaseline, pipe);
    }
}

QColor ListColumnHeaderWidget::headerTextColor(const QPalette& pal) {
    const QColor window = pal.color(QPalette::Window);
    const QColor windowText = pal.color(QPalette::WindowText);
    const QColor highlight = pal.color(QPalette::Highlight);
    const bool darkTheme = darkThemeFromPalette(pal);

    if (highlight.isValid() && highlight != windowText) {
        if (darkTheme) {
            QColor accent = highlight.lighter(155);
            if (accent.lightness() < 125) {
                accent = QColor(0x8e, 0xc5, 0xff);
            }
            return accent;
        }
        QColor accent = highlight.darker(120);
        if (accent.lightness() > 165 || accent.lightness() < 50) {
            accent = QColor(0x1e, 0x5a, 0x8e);
        }
        return accent;
    }

    if (darkTheme) {
        return QColor(0xb8, 0xc4, 0xd4);
    }
    return QColor(0x2f, 0x5d, 0x8a);
}

QSize ListColumnHeaderWidget::sizeHint() const {
    const int rowHeight = m_rowHeight ? m_rowHeight() : 28;
    return {200, rowHeight + kHeaderExtraHeight};
}

void ListColumnHeaderWidget::changeEvent(QEvent* event) {
    if (event->type() == QEvent::PaletteChange || event->type() == QEvent::ApplicationPaletteChange) {
        update();
    }
    QWidget::changeEvent(event);
}

void ListColumnHeaderWidget::enterEvent(QEnterEvent* event) {
    QWidget::enterEvent(event);
    if (m_activeHandle == 0) {
        setCursor(cursorForHandle(handleAt(event->position().toPoint())));
    }
}

void ListColumnHeaderWidget::leaveEvent(QEvent* event) {
    if (m_activeHandle == 0) {
        unsetCursor();
    }
    QWidget::leaveEvent(event);
}

void ListColumnHeaderWidget::paintEvent(QPaintEvent* event) {
    Q_UNUSED(event);

    QPainter painter(this);
    painter.setRenderHint(QPainter::TextAntialiasing, true);

    const QPalette pal = palette();
    paintHeaderChrome(&painter, rect(), pal);

    if (m_paintLabels) {
        PaintContext ctx{&painter, rect(), pal};
        m_paintLabels(ctx);
    }

    if (m_dividerXs) {
        paintDividerGuides(&painter, m_dividerXs(rect()), height(), pal);
    }

    if (m_activeHandle != 0) {
        painter.setPen(QPen(palette().color(QPalette::Highlight), 1, Qt::DashLine));
        if (m_activeHandle == kRowHeightHandleId) {
            painter.drawLine(0, m_dragGuideY, width(), m_dragGuideY);
        } else {
            painter.drawLine(m_dragGuideX, 0, m_dragGuideX, height());
        }
    }
}

void ListColumnHeaderWidget::mouseMoveEvent(QMouseEvent* event) {
    if (m_activeHandle != 0) {
        applyDrag(event->position().toPoint());
        return;
    }
    setCursor(cursorForHandle(handleAt(event->position().toPoint())));
}

void ListColumnHeaderWidget::mousePressEvent(QMouseEvent* event) {
    if (event->button() != Qt::LeftButton) {
        return;
    }
    m_activeHandle = handleAt(event->position().toPoint());
    if (m_activeHandle == 0) {
        return;
    }
    m_dragStartPos = event->position().toPoint();
    m_dragGuideX = m_dragStartPos.x();
    m_dragGuideY = m_dragStartPos.y();
    grabMouse();
    setCursor(cursorForHandle(m_activeHandle));
    emit dividerDragStarted();
    update();
}

void ListColumnHeaderWidget::mouseReleaseEvent(QMouseEvent* event) {
    Q_UNUSED(event);
    if (m_activeHandle == 0) {
        return;
    }
    m_activeHandle = 0;
    if (mouseGrabber() == this) {
        releaseMouse();
    }
    unsetCursor();
    update();
    emit dividerDragFinished();
}

int ListColumnHeaderWidget::handleAt(const QPoint& pos) const {
    if (UiResizeHandle::isWithinBottomGrab(pos.y(), height())) {
        return kRowHeightHandleId;
    }
    if (m_dividerHit) {
        return m_dividerHit(pos, rect());
    }
    return 0;
}

Qt::CursorShape ListColumnHeaderWidget::cursorForHandle(int handleId) {
    if (handleId == kRowHeightHandleId) {
        return Qt::SizeVerCursor;
    }
    if (handleId == 0) {
        return Qt::ArrowCursor;
    }
    return Qt::SplitHCursor;
}

void ListColumnHeaderWidget::applyDrag(const QPoint& pos) {
    if (!m_applyDrag) {
        return;
    }
    const int deltaX = pos.x() - m_dragStartPos.x();
    const int deltaY = pos.y() - m_dragStartPos.y();
    m_applyDrag(m_activeHandle, deltaX, deltaY, pos);
    if (m_activeHandle == kRowHeightHandleId) {
        m_dragGuideY = pos.y();
    } else {
        m_dragGuideX = pos.x();
    }
}

void ListColumnHeaderWidget::updateHeight() {
    const int rowHeight = m_rowHeight ? m_rowHeight() : 28;
    const int h = rowHeight + kHeaderExtraHeight;
    setMinimumHeight(h);
    setMaximumHeight(h);
    updateGeometry();
}
