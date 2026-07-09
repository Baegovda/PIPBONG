#include "ui/FeatureListWidget.h"

#include "ui/FeatureDragMime.h"

#include <QDrag>
#include <QDragEnterEvent>
#include <QDragLeaveEvent>
#include <QDragMoveEvent>
#include <QDropEvent>
#include <QKeyEvent>
#include <QListWidgetItem>
#include <QMimeData>
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
    setSelectionMode(QAbstractItemView::ExtendedSelection);
    setEditTriggers(QAbstractItemView::NoEditTriggers);
    setDefaultDropAction(Qt::MoveAction);
    setDragDropOverwriteMode(false);
    setDropIndicatorShown(false);
    setToolTip(tr("Ctrl/Shift+클릭 다중 선택 · 드래그하여 기능 순서 변경 · 기능/라이브러리/프로필 간 드래그 이동 · Ctrl+C/V · Delete"));
    setReorderEnabled(true);

    m_dropIndicator = new DropInsertionIndicator(viewport());
    m_dropIndicator->hide();
}

void FeatureListWidget::setReorderEnabled(bool enabled) {
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

void FeatureListWidget::startDrag(Qt::DropActions supportedActions) {
    if (selectedItems().size() != 1) {
        m_dragSourceRow = -1;
        return;
    }
    m_dragSourceRow = currentRow();
    m_pendingReorderFrom = -1;
    m_pendingReorderTo = -1;
    m_dropInsertionIndex = -1;
    if (m_dragSourceRow < 0 || !m_reorderEnabled) {
        m_dragSourceRow = -1;
        return;
    }

    QListWidgetItem* item = currentItem();
    if (!item) {
        m_dragSourceRow = -1;
        return;
    }

    const QString featureId = item->data(Qt::UserRole).toString();
    if (featureId.isEmpty() || m_activeProfileId.isEmpty()) {
        m_dragSourceRow = -1;
        return;
    }

    updateDragSourceVisuals();

    FeatureDragMime::Payload payload;
    payload.source = FeatureDragMime::Source::Profile;
    payload.id = featureId;
    payload.profileId = m_activeProfileId;

    QMimeData* mimeData = FeatureDragMime::createMimeData(payload);
    if (!mimeData) {
        m_dragSourceRow = -1;
        return;
    }

    auto* drag = new QDrag(this);
    drag->setMimeData(mimeData);
    drag->exec(supportedActions | Qt::CopyAction, Qt::MoveAction);

    clearDropIndicator();
    m_externalDropHover = false;
    if (viewport()) {
        viewport()->update();
    }

    const bool internalReorder = m_pendingReorderFrom >= 0 && m_pendingReorderTo >= 0
                                 && m_pendingReorderFrom != m_pendingReorderTo;
    if (internalReorder) {
        emit featureRowsReordered(m_pendingReorderFrom, m_pendingReorderTo);
    }

    m_dragSourceRow = -1;
    m_pendingReorderFrom = -1;
    m_pendingReorderTo = -1;
}

void FeatureListWidget::dragEnterEvent(QDragEnterEvent* event) {
    if (m_reorderEnabled && event->source() == this && m_dragSourceRow >= 0) {
        m_externalDropHover = false;
        m_dropInsertionIndex = dropInsertionIndex(event->position().toPoint());
        updateDropIndicator();
        event->acceptProposedAction();
        return;
    }
    if (m_reorderEnabled && FeatureDragMime::accepts(event->mimeData()) && event->source() != this) {
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

void FeatureListWidget::dragMoveEvent(QDragMoveEvent* event) {
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
    if (m_reorderEnabled && FeatureDragMime::accepts(event->mimeData()) && event->source() != this) {
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

void FeatureListWidget::dragLeaveEvent(QDragLeaveEvent* event) {
    clearDropIndicator();
    m_externalDropHover = false;
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
    if (!m_dropIndicator || m_dropInsertionIndex < 0) {
        return;
    }
    if (m_dragSourceRow < 0 && !m_externalDropHover) {
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
    if (m_reorderEnabled && FeatureDragMime::accepts(event->mimeData()) && event->source() != this) {
        const int insertIndex = dropInsertionIndex(event->position().toPoint());
        clearDropIndicator();
        m_externalDropHover = false;
        emit featureDropped(event->mimeData(), insertIndex);
        event->setDropAction(Qt::CopyAction);
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

void FeatureListWidget::keyPressEvent(QKeyEvent* event) {
    if (event->matches(QKeySequence::Copy)) {
        emit copyRequested();
        event->accept();
        return;
    }
    if (event->matches(QKeySequence::Paste)) {
        emit pasteRequested();
        event->accept();
        return;
    }
    if (event->key() == Qt::Key_Delete) {
        emit deleteRequested();
        event->accept();
        return;
    }
    QListWidget::keyPressEvent(event);
}
