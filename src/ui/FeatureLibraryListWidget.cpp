#include "ui/FeatureLibraryListWidget.h"

#include "ui/FeatureDragMime.h"

#include <QDrag>
#include <QDragEnterEvent>
#include <QDragLeaveEvent>
#include <QDragMoveEvent>
#include <QDropEvent>
#include <QListWidgetItem>
#include <QMimeData>

FeatureLibraryListWidget::FeatureLibraryListWidget(QWidget* parent)
    : QListWidget(parent) {
    setDragEnabled(true);
    setAcceptDrops(true);
    setDropIndicatorShown(true);
    setDefaultDropAction(Qt::CopyAction);
    setDragDropMode(QAbstractItemView::DragDrop);
}

void FeatureLibraryListWidget::setTransferEnabled(bool enabled) {
    m_transferEnabled = enabled;
    setDragEnabled(enabled);
    setAcceptDrops(enabled);
    if (!enabled) {
        setDragDropMode(QAbstractItemView::NoDragDrop);
        return;
    }
    setDragDropMode(QAbstractItemView::DragDrop);
}

void FeatureLibraryListWidget::startDrag(Qt::DropActions supportedActions) {
    if (!m_transferEnabled || selectedItems().size() != 1) {
        return;
    }

    QListWidgetItem* item = currentItem();
    if (!item) {
        return;
    }

    const QString entryId = item->data(Qt::UserRole).toString();
    if (entryId.isEmpty()) {
        return;
    }

    FeatureDragMime::Payload payload;
    payload.source = FeatureDragMime::Source::Library;
    payload.id = entryId;

    QMimeData* mimeData = FeatureDragMime::createMimeData(payload);
    if (!mimeData) {
        return;
    }

    auto* drag = new QDrag(this);
    drag->setMimeData(mimeData);
    drag->exec(supportedActions, Qt::CopyAction);
}

void FeatureLibraryListWidget::dragEnterEvent(QDragEnterEvent* event) {
    if (m_transferEnabled && FeatureDragMime::accepts(event->mimeData())) {
        const FeatureDragMime::Payload payload = FeatureDragMime::parse(event->mimeData());
        if (payload.source == FeatureDragMime::Source::Profile) {
            event->acceptProposedAction();
            return;
        }
    }
    event->ignore();
}

void FeatureLibraryListWidget::dragMoveEvent(QDragMoveEvent* event) {
    if (m_transferEnabled && FeatureDragMime::accepts(event->mimeData())) {
        const FeatureDragMime::Payload payload = FeatureDragMime::parse(event->mimeData());
        if (payload.source == FeatureDragMime::Source::Profile) {
            event->acceptProposedAction();
            return;
        }
    }
    event->ignore();
}

void FeatureLibraryListWidget::dragLeaveEvent(QDragLeaveEvent* event) {
    QListWidget::dragLeaveEvent(event);
}

void FeatureLibraryListWidget::dropEvent(QDropEvent* event) {
    if (!m_transferEnabled || !FeatureDragMime::accepts(event->mimeData())) {
        event->ignore();
        return;
    }

    const FeatureDragMime::Payload payload = FeatureDragMime::parse(event->mimeData());
    if (payload.source != FeatureDragMime::Source::Profile) {
        event->ignore();
        return;
    }

    emit featureDroppedOnLibrary(event->mimeData());
    event->setDropAction(Qt::CopyAction);
    event->accept();
}
