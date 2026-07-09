#include "ui/widgets/ReorderableListWidget.h"

#include "ui/widgets/ListDragVisuals.h"

#include <QCursor>
#include <QDrag>
#include <QDragEnterEvent>
#include <QDragLeaveEvent>
#include <QDragMoveEvent>
#include <QDropEvent>
#include <QListWidgetItem>
#include <QMimeData>
#include <QPainter>
#include <QResizeEvent>
#include <QTimer>

namespace {

constexpr const char* kInternalReorderMime = "application/x-pipbong-list-reorder";

class DropInsertionIndicator : public QWidget {
public:
    explicit DropInsertionIndicator(QWidget* parent)
        : QWidget(parent) {
        setAttribute(Qt::WA_TransparentForMouseEvents);
        setAttribute(Qt::WA_NoSystemBackground);
        setFixedHeight(6);
        m_timer.setInterval(380);
        connect(&m_timer, &QTimer::timeout, this, [this]() {
            m_bright = !m_bright;
            update();
        });
    }

    void showAt(int y, int width) {
        setGeometry(0, y - height() / 2, width, height());
        if (!m_timer.isActive()) {
            m_timer.start();
        }
        show();
        raise();
        update();
    }

    void hideIndicator() {
        m_timer.stop();
        hide();
    }

protected:
    void paintEvent(QPaintEvent*) override {
        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing);

        const QColor lineColor = m_bright ? QColor(130, 165, 195) : QColor(165, 188, 210);
        const QColor dotColor = m_bright ? QColor(110, 145, 175) : QColor(145, 170, 195);
        const int centerY = height() / 2;
        const int lineHeight = 3;

        painter.setPen(Qt::NoPen);
        painter.setBrush(lineColor);
        painter.drawRoundedRect(0, centerY - lineHeight / 2, width(), lineHeight, 1, 1);

        const int dotRadius = 4;
        painter.setBrush(dotColor);
        painter.drawEllipse(QPointF(dotRadius, centerY), dotRadius, dotRadius);
        painter.drawEllipse(QPointF(width() - dotRadius, centerY), dotRadius, dotRadius);
    }

private:
    QTimer m_timer;
    bool m_bright = true;
};

} // namespace

ReorderableListWidget::ReorderableListWidget(QWidget* parent)
    : QListWidget(parent) {
    setEditTriggers(QAbstractItemView::NoEditTriggers);
    setDefaultDropAction(Qt::MoveAction);
    setDragDropOverwriteMode(false);
    setDropIndicatorShown(false);
    setReorderEnabled(true);

    m_dropIndicator = new DropInsertionIndicator(viewport());
    m_dropIndicator->hide();
}

void ReorderableListWidget::setReorderEnabled(bool enabled) {
    m_reorderEnabled = enabled;
    setDragEnabled(enabled);
    setAcceptDrops(enabled);
    setDragDropMode(enabled ? QAbstractItemView::DragDrop : QAbstractItemView::NoDragDrop);
    if (!enabled) {
        clearDropIndicator();
        m_dragSourceRow = -1;
        m_externalDropHover = false;
        if (viewport()) {
            viewport()->update();
        }
    }
}

bool ReorderableListWidget::canStartDragFromRow(int row) const {
    return row >= 0;
}

int ReorderableListWidget::minimumDropInsertionIndex() const {
    return 0;
}

int ReorderableListWidget::maximumDropInsertionIndex() const {
    return count();
}

QMimeData* ReorderableListWidget::buildDragMimeData(int /*row*/) const {
    return nullptr;
}

bool ReorderableListWidget::acceptsExternalMime(const QMimeData* /*mime*/) const {
    return false;
}

Qt::DropAction ReorderableListWidget::preferredExternalDropAction(const QMimeData* /*mime*/) const {
    return Qt::CopyAction;
}

bool ReorderableListWidget::showInsertionIndicator() const {
    return m_dragSourceRow >= 0 || m_externalDropHover;
}

void ReorderableListWidget::startDrag(Qt::DropActions supportedActions) {
    if (selectedItems().size() != 1) {
        m_dragSourceRow = -1;
        return;
    }

    m_dragSourceRow = currentRow();
    m_pendingReorderFrom = -1;
    m_pendingReorderTo = -1;
    m_dropInsertionIndex = -1;

    if (m_dragSourceRow < 0 || !m_reorderEnabled || !canStartDragFromRow(m_dragSourceRow)) {
        m_dragSourceRow = -1;
        return;
    }

    updateDragSourceVisuals();

    const QPoint cursorGlobal = QCursor::pos();
    if (QListWidgetItem* listItem = item(m_dragSourceRow)) {
        ListDragVisuals::showDragSlotPlaceholder(viewport(), visualItemRect(listItem), &m_dragSlotPlaceholder);
    }

    std::unique_ptr<QMimeData> ownedMime(buildDragMimeData(m_dragSourceRow));
    QMimeData* mimeData = ownedMime.release();
    if (!mimeData) {
        mimeData = new QMimeData();
        mimeData->setData(kInternalReorderMime, QByteArrayLiteral("1"));
    }

    auto* drag = new QDrag(this);
    drag->setMimeData(mimeData);
    const ListDragVisuals::LiftedPixmap lifted =
        ListDragVisuals::makeLiftedListRowDrag(this, m_dragSourceRow, cursorGlobal);
    ListDragVisuals::applyToDrag(drag, lifted);
    drag->exec(supportedActions | Qt::CopyAction, Qt::MoveAction);

    ListDragVisuals::hideDragSlotPlaceholder(&m_dragSlotPlaceholder);

    clearDropIndicator();
    m_externalDropHover = false;
    if (viewport()) {
        viewport()->update();
    }

    const bool internalReorder = m_pendingReorderFrom >= 0 && m_pendingReorderTo >= 0
                                 && m_pendingReorderFrom != m_pendingReorderTo;
    const int settleRow = m_pendingReorderTo;
    if (internalReorder) {
        emit rowsReordered(m_pendingReorderFrom, m_pendingReorderTo);
        playDropSettleAtRow(settleRow);
    }

    m_dragSourceRow = -1;
    m_pendingReorderFrom = -1;
    m_pendingReorderTo = -1;
}

void ReorderableListWidget::dragEnterEvent(QDragEnterEvent* event) {
    if (m_reorderEnabled && event->source() == this && m_dragSourceRow >= 0) {
        m_externalDropHover = false;
        m_dropInsertionIndex = dropInsertionIndex(event->position().toPoint());
        updateDropIndicator();
        event->acceptProposedAction();
        return;
    }
    if (m_reorderEnabled && acceptsExternalMime(event->mimeData()) && event->source() != this) {
        m_externalDropHover = true;
        m_dropInsertionIndex = dropInsertionIndex(event->position().toPoint());
        updateDropIndicator();
        event->acceptProposedAction();
        return;
    }
    clearDropIndicator();
    m_externalDropHover = false;
    event->ignore();
}

void ReorderableListWidget::dragMoveEvent(QDragMoveEvent* event) {
    if (m_reorderEnabled && event->source() == this && m_dragSourceRow >= 0) {
        m_externalDropHover = false;
        const int insertIdx = dropInsertionIndex(event->position().toPoint());
        if (insertIdx != m_dropInsertionIndex) {
            m_dropInsertionIndex = insertIdx;
            updateDropIndicator();
        } else if (m_dropIndicator && m_dropIndicator->isVisible()) {
            updateDropIndicator();
        }
        event->acceptProposedAction();
        return;
    }
    if (m_reorderEnabled && acceptsExternalMime(event->mimeData()) && event->source() != this) {
        m_externalDropHover = true;
        const int insertIdx = dropInsertionIndex(event->position().toPoint());
        if (insertIdx != m_dropInsertionIndex) {
            m_dropInsertionIndex = insertIdx;
            updateDropIndicator();
        } else if (m_dropIndicator && m_dropIndicator->isVisible()) {
            updateDropIndicator();
        }
        event->acceptProposedAction();
        return;
    }
    clearDropIndicator();
    m_externalDropHover = false;
    event->ignore();
}

void ReorderableListWidget::dragLeaveEvent(QDragLeaveEvent* event) {
    clearDropIndicator();
    m_externalDropHover = false;
    QListWidget::dragLeaveEvent(event);
}

int ReorderableListWidget::dropInsertionIndex(const QPoint& pos) const {
    const int rows = count();
    if (rows <= 0) {
        return minimumDropInsertionIndex();
    }

    int insertIdx = 0;
    if (QListWidgetItem* hit = itemAt(pos)) {
        const int dropRow = row(hit);
        const QRect rect = visualItemRect(hit);
        insertIdx = pos.y() < rect.center().y() ? dropRow : dropRow + 1;
    } else {
        const QRect firstRect = visualItemRect(item(0));
        insertIdx = pos.y() < firstRect.top() ? 0 : rows;
    }

    return qBound(minimumDropInsertionIndex(), insertIdx, maximumDropInsertionIndex());
}

int ReorderableListWidget::insertionLineY(int insertionIndex) const {
    const int rows = count();
    if (rows <= 0) {
        return 0;
    }
    if (insertionIndex <= 0) {
        return visualItemRect(item(0)).top();
    }
    if (insertionIndex >= rows) {
        return visualItemRect(item(rows - 1)).bottom();
    }
    return visualItemRect(item(insertionIndex)).top();
}

void ReorderableListWidget::updateDropIndicator() {
    if (!m_dropIndicator || m_dropInsertionIndex < 0 || !showInsertionIndicator()) {
        return;
    }
    const int y = insertionLineY(m_dropInsertionIndex);
    static_cast<DropInsertionIndicator*>(m_dropIndicator)->showAt(y, viewport()->width());
}

void ReorderableListWidget::clearDropIndicator() {
    m_dropInsertionIndex = -1;
    if (m_dropIndicator) {
        static_cast<DropInsertionIndicator*>(m_dropIndicator)->hideIndicator();
    }
}

void ReorderableListWidget::updateDragSourceVisuals() {
    if (viewport()) {
        viewport()->update();
    }
}

void ReorderableListWidget::resizeEvent(QResizeEvent* event) {
    QListWidget::resizeEvent(event);
    if (m_dropInsertionIndex >= 0) {
        updateDropIndicator();
    }
}

int ReorderableListWidget::dropTargetRow(const QPoint& pos) const {
    int insertionRow = dropInsertionIndex(pos);
    if (m_dragSourceRow >= 0 && m_dragSourceRow < insertionRow) {
        --insertionRow;
    }
    if (count() <= 0) {
        return 0;
    }
    return qBound(0, insertionRow, count() - 1);
}

void ReorderableListWidget::dropEvent(QDropEvent* event) {
    if (m_reorderEnabled && acceptsExternalMime(event->mimeData()) && event->source() != this) {
        const bool useInsertIndex = m_externalDropHover && m_dropInsertionIndex >= 0;
        const int insertIndex = useInsertIndex ? m_dropInsertionIndex : -1;
        clearDropIndicator();
        m_externalDropHover = false;
        emit externalItemDropped(event->mimeData(), insertIndex);
        if (insertIndex >= 0 && count() > 0) {
            const int settleRow = qBound(0, insertIndex, count() - 1);
            playDropSettleAtRow(settleRow);
        }
        event->setDropAction(preferredExternalDropAction(event->mimeData()));
        event->accept();
        return;
    }

    if (!m_reorderEnabled || m_dragSourceRow < 0 || event->source() != this) {
        m_dragSourceRow = -1;
        clearDropIndicator();
        m_externalDropHover = false;
        event->ignore();
        return;
    }

    const int toRow = dropTargetRow(event->position().toPoint());
    const int fromRow = m_dragSourceRow;
    m_dragSourceRow = -1;
    clearDropIndicator();
    m_externalDropHover = false;

    if (fromRow != toRow) {
        m_pendingReorderFrom = fromRow;
        m_pendingReorderTo = toRow;
    }
    event->setDropAction(Qt::IgnoreAction);
    event->accept();
}

void ReorderableListWidget::playDropSettleAtRow(int row) {
    if (!viewport() || row < 0 || row >= count()) {
        return;
    }
    QListWidgetItem* listItem = item(row);
    if (!listItem) {
        return;
    }
    const QRect targetRect = visualItemRect(listItem);
    const QPixmap rowPixmap = ListDragVisuals::captureListRowPixmap(this, row);
    ListDragVisuals::playDropSettle(viewport(), rowPixmap, targetRect);
}
