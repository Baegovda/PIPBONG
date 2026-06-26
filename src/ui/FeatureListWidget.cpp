#include "ui/FeatureListWidget.h"

#include <QDragEnterEvent>
#include <QDragLeaveEvent>
#include <QDragMoveEvent>
#include <QDropEvent>
#include <QKeyEvent>
#include <QPainter>
#include <QResizeEvent>
#include <QTimer>

namespace {

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

FeatureListWidget::FeatureListWidget(QWidget* parent)
    : QListWidget(parent) {
    setSelectionMode(QAbstractItemView::SingleSelection);
    setEditTriggers(QAbstractItemView::NoEditTriggers);
    setDefaultDropAction(Qt::MoveAction);
    setDragDropOverwriteMode(false);
    setDropIndicatorShown(false);
    setToolTip(tr("드래그하여 기능 순서 변경"));
    setReorderEnabled(true);

    m_dropIndicator = new DropInsertionIndicator(viewport());
    m_dropIndicator->hide();
}

void FeatureListWidget::setReorderEnabled(bool enabled) {
    m_reorderEnabled = enabled;
    setDragEnabled(enabled);
    setAcceptDrops(enabled);
    setDragDropMode(enabled ? QAbstractItemView::InternalMove : QAbstractItemView::NoDragDrop);
    if (!enabled) {
        clearDropIndicator();
        m_dragSourceRow = -1;
        if (viewport()) {
            viewport()->update();
        }
    }
}

void FeatureListWidget::startDrag(Qt::DropActions supportedActions) {
    m_dragSourceRow = currentRow();
    m_pendingReorderFrom = -1;
    m_pendingReorderTo = -1;
    m_dropInsertionIndex = -1;
    if (m_dragSourceRow < 0 || !m_reorderEnabled) {
        m_dragSourceRow = -1;
        return;
    }

    updateDragSourceVisuals();

    const int sourceRow = m_dragSourceRow;
    QListWidget::startDrag(supportedActions);

    clearDropIndicator();
    if (viewport()) {
        viewport()->update();
    }

    const int fromRow = m_pendingReorderFrom >= 0 ? m_pendingReorderFrom : sourceRow;
    const int toRow = m_pendingReorderTo >= 0 ? m_pendingReorderTo : sourceRow;
    m_dragSourceRow = -1;
    m_pendingReorderFrom = -1;
    m_pendingReorderTo = -1;

    emit featureRowsReordered(fromRow, toRow);
}

void FeatureListWidget::dragEnterEvent(QDragEnterEvent* event) {
    if (m_reorderEnabled && event->source() == this && m_dragSourceRow >= 0) {
        m_dropInsertionIndex = dropInsertionIndex(event->position().toPoint());
        updateDropIndicator();
        event->acceptProposedAction();
        return;
    }
    clearDropIndicator();
    event->ignore();
}

void FeatureListWidget::dragMoveEvent(QDragMoveEvent* event) {
    if (m_reorderEnabled && event->source() == this && m_dragSourceRow >= 0) {
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
    event->ignore();
}

void FeatureListWidget::dragLeaveEvent(QDragLeaveEvent* event) {
    clearDropIndicator();
    QListWidget::dragLeaveEvent(event);
}

int FeatureListWidget::dropInsertionIndex(const QPoint& pos) const {
    const int rows = count();
    if (rows <= 0) {
        return 0;
    }

    if (QListWidgetItem* hit = itemAt(pos)) {
        const int dropRow = row(hit);
        const QRect rect = visualItemRect(hit);
        return pos.y() < rect.center().y() ? dropRow : dropRow + 1;
    }

    const QRect firstRect = visualItemRect(item(0));
    return pos.y() < firstRect.top() ? 0 : rows;
}

int FeatureListWidget::insertionLineY(int insertionIndex) const {
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

void FeatureListWidget::updateDropIndicator() {
    if (!m_dropIndicator || m_dropInsertionIndex < 0 || m_dragSourceRow < 0) {
        return;
    }
    const int y = insertionLineY(m_dropInsertionIndex);
    static_cast<DropInsertionIndicator*>(m_dropIndicator)->showAt(y, viewport()->width());
}

void FeatureListWidget::clearDropIndicator() {
    m_dropInsertionIndex = -1;
    if (m_dropIndicator) {
        static_cast<DropInsertionIndicator*>(m_dropIndicator)->hideIndicator();
    }
}

void FeatureListWidget::updateDragSourceVisuals() {
    if (viewport()) {
        viewport()->update();
    }
}

void FeatureListWidget::resizeEvent(QResizeEvent* event) {
    QListWidget::resizeEvent(event);
    if (m_dropInsertionIndex >= 0) {
        updateDropIndicator();
    }
}

int FeatureListWidget::dropTargetRow(const QPoint& pos) const {
    int insertionRow = dropInsertionIndex(pos);
    if (m_dragSourceRow >= 0 && m_dragSourceRow < insertionRow) {
        --insertionRow;
    }
    if (count() <= 0) {
        return 0;
    }
    return qBound(0, insertionRow, count() - 1);
}

void FeatureListWidget::dropEvent(QDropEvent* event) {
    if (!m_reorderEnabled || m_dragSourceRow < 0 || event->source() != this) {
        m_dragSourceRow = -1;
        clearDropIndicator();
        event->ignore();
        return;
    }

    const int toRow = dropTargetRow(event->position().toPoint());
    const int fromRow = m_dragSourceRow;
    m_dragSourceRow = -1;
    clearDropIndicator();

    if (fromRow != toRow) {
        m_pendingReorderFrom = fromRow;
        m_pendingReorderTo = toRow;
    }
    event->setDropAction(Qt::IgnoreAction);
    event->accept();
}

void FeatureListWidget::keyPressEvent(QKeyEvent* event) {
    if (event->key() == Qt::Key_Delete) {
        emit deleteRequested();
        event->accept();
        return;
    }
    QListWidget::keyPressEvent(event);
}
