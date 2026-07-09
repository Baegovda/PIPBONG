#include "ui/widgets/ListDragVisuals.h"

#include <QAbstractItemView>
#include <QCursor>
#include <QDrag>
#include <QListWidget>
#include <QPainter>
#include <QPainterPath>
#include <QStyle>
#include <QStyleOptionViewItem>
#include <QTableWidget>
#include <QElapsedTimer>
#include <QTimer>

class DragSettleOverlay : public QWidget {
public:
    explicit DragSettleOverlay(QWidget* parent)
        : QWidget(parent) {
        setAttribute(Qt::WA_TransparentForMouseEvents);
        setAttribute(Qt::WA_NoSystemBackground);
        if (parent) {
            setGeometry(parent->rect());
        }

        m_timer.setInterval(16);
        connect(&m_timer, &QTimer::timeout, this, [this]() {
            const qint64 elapsed = m_clock.elapsed();
            m_progress = qBound(0.0, static_cast<qreal>(elapsed) / m_durationMs, 1.0);
            update();
            if (m_progress >= 1.0) {
                m_timer.stop();
                if (m_onFinished) {
                    m_onFinished();
                }
                deleteLater();
            }
        });
    }

    void start(const QPixmap& rowPixmap, const QRect& targetRect, std::function<void()> onFinished) {
        m_rowPixmap = rowPixmap;
        m_targetRect = targetRect;
        m_onFinished = std::move(onFinished);
        m_progress = 0.0;
        m_durationMs = 280;
        m_clock.start();
        show();
        raise();
        m_timer.start();
    }

protected:
    void paintEvent(QPaintEvent*) override {
        if (m_rowPixmap.isNull() || m_targetRect.isEmpty()) {
            return;
        }

        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing);
        painter.setRenderHint(QPainter::SmoothPixmapTransform);

        const qreal t = outBackEased(m_progress);
        const qreal scale = 1.14 - 0.14 * t;
        const qreal opacity = 0.55 + 0.45 * t;

        const QSize logicalSize = m_rowPixmap.size() / m_rowPixmap.devicePixelRatioF();
        const int drawW = qRound(logicalSize.width() * scale);
        const int drawH = qRound(logicalSize.height() * scale);
        QRect drawRect(0, 0, drawW, drawH);
        drawRect.moveCenter(m_targetRect.center());

        painter.setOpacity(opacity);
        painter.drawPixmap(drawRect, m_rowPixmap);
    }

private:
    static qreal outBackEased(qreal t) {
        constexpr qreal c1 = 1.70158;
        constexpr qreal c3 = c1 + 1.0;
        const qreal x = t - 1.0;
        return 1.0 + c3 * x * x * x + c1 * x * x;
    }

    QPixmap m_rowPixmap;
    QRect m_targetRect;
    qreal m_progress = 0.0;
    int m_durationMs = 280;
    QElapsedTimer m_clock;
    QTimer m_timer;
    std::function<void()> m_onFinished;
};

namespace {

constexpr qreal kLiftScale = 1.06;
constexpr int kLiftPadding = 12;

class DragSlotPlaceholder : public QWidget {
public:
    explicit DragSlotPlaceholder(QWidget* parent)
        : QWidget(parent) {
        setAttribute(Qt::WA_TransparentForMouseEvents);
        setAttribute(Qt::WA_NoSystemBackground);
    }

    void showAt(const QRect& rect) {
        setGeometry(rect);
        show();
        raise();
        update();
    }

protected:
    void paintEvent(QPaintEvent*) override {
        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing);

        const QPalette pal = parentWidget() ? parentWidget()->palette() : palette();
        QColor fill = pal.color(QPalette::Base);
        fill.setAlpha(72);
        QColor border = pal.color(QPalette::Mid);
        border.setAlpha(150);

        QRect r = rect().adjusted(2, 1, -2, -1);
        QPainterPath path;
        path.addRoundedRect(r, 5, 5);
        painter.fillPath(path, fill);

        QPen pen(border, 1.5, Qt::DashLine);
        pen.setDashPattern({4, 3});
        painter.setPen(pen);
        painter.setBrush(Qt::NoBrush);
        painter.drawRoundedRect(r, 5, 5);
    }
};

QPoint hotSpotInRowRect(const QRect& rowRectInViewport, const QPoint& cursorGlobalPos, QWidget* viewport) {
    const QPoint cursorInViewport = viewport->mapFromGlobal(cursorGlobalPos);
    return cursorInViewport - rowRectInViewport.topLeft();
}

} // namespace

namespace ListDragVisuals {

QPixmap captureListRowPixmap(QListWidget* list, int row) {
    if (!list || row < 0 || row >= list->count()) {
        return {};
    }
    QListWidgetItem* listItem = list->item(row);
    if (!listItem) {
        return {};
    }

    const QRect rect = list->visualItemRect(listItem);
    if (rect.isEmpty()) {
        return {};
    }

    const qreal dpr = list->devicePixelRatioF();
    QPixmap pixmap(rect.size() * dpr);
    pixmap.setDevicePixelRatio(dpr);
    pixmap.fill(Qt::transparent);

    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing);

    QStyleOptionViewItem option;
    option.initFrom(list);
    option.rect = QRect(0, 0, rect.width(), rect.height());
    option.state |= QStyle::State_Enabled;
    const QModelIndex index = list->model()->index(row, 0);
    if (list->selectionModel() && list->selectionModel()->isSelected(index)) {
        option.state |= QStyle::State_Selected;
    }
    if (list->currentRow() == row) {
        option.state |= QStyle::State_HasFocus;
    }

    if (QAbstractItemDelegate* delegate = list->itemDelegate()) {
        delegate->paint(&painter, option, index);
    }

    return pixmap;
}

QPixmap captureTableRowPixmap(QTableWidget* table, int row) {
    if (!table || row < 0 || row >= table->rowCount() || !table->viewport()) {
        return {};
    }

    const QRect firstCol = table->visualRect(table->model()->index(row, 0));
    if (firstCol.isEmpty()) {
        return {};
    }

    const QRect grabRect(0, firstCol.top(), table->viewport()->width(), firstCol.height());
    return table->viewport()->grab(grabRect);
}

LiftedPixmap makeLiftedDragPixmap(const QPixmap& rowPixmap, const QPoint& hotSpotInRow) {
    LiftedPixmap result;
    if (rowPixmap.isNull()) {
        return result;
    }

    const qreal dpr = rowPixmap.devicePixelRatioF();
    const QSize logicalSize = rowPixmap.size() / dpr;
    const QSize scaledSize(qRound(logicalSize.width() * kLiftScale),
                           qRound(logicalSize.height() * kLiftScale));
    const QSize outLogical = scaledSize + QSize(kLiftPadding * 2, kLiftPadding * 2);

    QPixmap out(outLogical * dpr);
    out.setDevicePixelRatio(dpr);
    out.fill(Qt::transparent);

    QPainter painter(&out);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setRenderHint(QPainter::SmoothPixmapTransform);

    const QRect shadowRect(kLiftPadding + 2,
                           kLiftPadding + 4,
                           scaledSize.width(),
                           scaledSize.height());
    QColor shadow(0, 0, 0, 70);
    painter.setPen(Qt::NoPen);
    painter.setBrush(shadow);
    painter.drawRoundedRect(shadowRect, 6, 6);

    const QRect drawRect(kLiftPadding, kLiftPadding, scaledSize.width(), scaledSize.height());
    painter.drawPixmap(drawRect, rowPixmap);

    result.pixmap = out;
    result.hotSpot = QPoint(kLiftPadding + qRound(hotSpotInRow.x() * kLiftScale),
                            kLiftPadding + qRound(hotSpotInRow.y() * kLiftScale));
    return result;
}

LiftedPixmap makeLiftedListRowDrag(QListWidget* list, int row, const QPoint& cursorGlobalPos) {
    QListWidgetItem* listItem = list ? list->item(row) : nullptr;
    if (!list || !listItem || !list->viewport()) {
        return {};
    }
    const QRect rowRect = list->visualItemRect(listItem);
    const QPixmap rowPixmap = captureListRowPixmap(list, row);
    const QPoint hotSpot = hotSpotInRowRect(rowRect, cursorGlobalPos, list->viewport());
    return makeLiftedDragPixmap(rowPixmap, hotSpot);
}

LiftedPixmap makeLiftedTableRowDrag(QTableWidget* table, int row, const QPoint& cursorGlobalPos) {
    if (!table || !table->viewport() || row < 0 || row >= table->rowCount()) {
        return {};
    }
    const QRect rowRect = table->visualRect(table->model()->index(row, 0));
    const QRect grabRect(0, rowRect.top(), table->viewport()->width(), rowRect.height());
    const QPixmap rowPixmap = captureTableRowPixmap(table, row);
    const QPoint hotSpot = hotSpotInRowRect(grabRect, cursorGlobalPos, table->viewport());
    return makeLiftedDragPixmap(rowPixmap, hotSpot);
}

void applyToDrag(QDrag* drag, const LiftedPixmap& lifted) {
    if (!drag || lifted.pixmap.isNull()) {
        return;
    }
    drag->setPixmap(lifted.pixmap);
    drag->setHotSpot(lifted.hotSpot);
}

void showDragSlotPlaceholder(QWidget* viewport, const QRect& rowRectInViewport, QWidget** slotOut) {
    if (!viewport || !slotOut) {
        return;
    }
    if (!*slotOut) {
        *slotOut = new DragSlotPlaceholder(viewport);
    }
    static_cast<DragSlotPlaceholder*>(*slotOut)->showAt(rowRectInViewport);
}

void hideDragSlotPlaceholder(QWidget** slotOut) {
    if (!slotOut || !*slotOut) {
        return;
    }
    (*slotOut)->hide();
}

void playDropSettle(QWidget* viewport,
                    const QPixmap& rowPixmap,
                    const QRect& targetRectInViewport,
                    std::function<void()> onFinished) {
    if (!viewport || rowPixmap.isNull() || targetRectInViewport.isEmpty()) {
        if (onFinished) {
            onFinished();
        }
        return;
    }

    auto* overlay = new DragSettleOverlay(viewport);
    overlay->start(rowPixmap, targetRectInViewport, std::move(onFinished));
}

} // namespace
