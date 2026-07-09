#include "ui/FeatureLibraryListWidget.h"

#include "ui/FeatureDragMime.h"

#include <QKeyEvent>
#include <QListWidgetItem>
#include <QMimeData>

FeatureLibraryListWidget::FeatureLibraryListWidget(QWidget* parent)
    : ReorderableListWidget(parent) {
    setSelectionMode(QAbstractItemView::ExtendedSelection);
    setToolTip(tr("Ctrl/Shift+클릭 다중 선택 · 드래그하여 라이브러리 순서 변경 · 기능 목록에서 드래그하여 저장 · Delete"));
    connect(this, &ReorderableListWidget::rowsReordered, this, &FeatureLibraryListWidget::libraryRowsReordered);
    connect(this,
            &ReorderableListWidget::externalItemDropped,
            this,
            [this](const QMimeData* mime, int /*insertIndex*/) {
                emit featureDroppedOnLibrary(mime);
            });
}

void FeatureLibraryListWidget::setTransferEnabled(bool enabled) {
    m_transferEnabled = enabled;
    setReorderEnabled(enabled);
}

QMimeData* FeatureLibraryListWidget::buildDragMimeData(int row) const {
    QListWidgetItem* item = this->item(row);
    if (!item) {
        return nullptr;
    }
    const QString entryId = item->data(Qt::UserRole).toString();
    if (entryId.isEmpty()) {
        return nullptr;
    }

    FeatureDragMime::Payload payload;
    payload.source = FeatureDragMime::Source::Library;
    payload.id = entryId;
    return FeatureDragMime::createMimeData(payload);
}

bool FeatureLibraryListWidget::acceptsExternalMime(const QMimeData* mime) const {
    if (!FeatureDragMime::accepts(mime)) {
        return false;
    }
    const FeatureDragMime::Payload payload = FeatureDragMime::parse(mime);
    return payload.source == FeatureDragMime::Source::Profile;
}

Qt::DropAction FeatureLibraryListWidget::preferredExternalDropAction(const QMimeData* /*mime*/) const {
    return Qt::CopyAction;
}

void FeatureLibraryListWidget::keyPressEvent(QKeyEvent* event) {
    if (event->key() == Qt::Key_Delete) {
        emit deleteRequested();
        event->accept();
        return;
    }
    ReorderableListWidget::keyPressEvent(event);
}
