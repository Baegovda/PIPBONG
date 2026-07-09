#include "ui/FeatureListWidget.h"

#include "ui/FeatureDragMime.h"

#include <QKeyEvent>
#include <QListWidgetItem>
#include <QMimeData>

FeatureListWidget::FeatureListWidget(QWidget* parent)
    : ReorderableListWidget(parent) {
    setSelectionMode(QAbstractItemView::ExtendedSelection);
    setToolTip(tr("Ctrl/Shift+클릭 다중 선택 · 드래그하여 기능 순서 변경 · 기능/라이브러리/프로필 간 드래그 이동 · Ctrl+C/V · Delete"));
    connect(this, &ReorderableListWidget::rowsReordered, this, &FeatureListWidget::featureRowsReordered);
    connect(this,
            &ReorderableListWidget::externalItemDropped,
            this,
            &FeatureListWidget::featureDropped);
}

int FeatureListWidget::dragSourceRow() const {
    return ReorderableListWidget::dragSourceRow();
}

bool FeatureListWidget::canStartDragFromRow(int row) const {
    if (!ReorderableListWidget::canStartDragFromRow(row)) {
        return false;
    }
    QListWidgetItem* item = this->item(row);
    if (!item) {
        return false;
    }
    const QString featureId = item->data(Qt::UserRole).toString();
    return !featureId.isEmpty() && !m_activeProfileId.isEmpty();
}

QMimeData* FeatureListWidget::buildDragMimeData(int row) const {
    QListWidgetItem* item = this->item(row);
    if (!item) {
        return nullptr;
    }
    const QString featureId = item->data(Qt::UserRole).toString();
    if (featureId.isEmpty() || m_activeProfileId.isEmpty()) {
        return nullptr;
    }

    FeatureDragMime::Payload payload;
    payload.source = FeatureDragMime::Source::Profile;
    payload.id = featureId;
    payload.profileId = m_activeProfileId;
    return FeatureDragMime::createMimeData(payload);
}

bool FeatureListWidget::acceptsExternalMime(const QMimeData* mime) const {
    return FeatureDragMime::accepts(mime);
}

Qt::DropAction FeatureListWidget::preferredExternalDropAction(const QMimeData* /*mime*/) const {
    return Qt::CopyAction;
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
    ReorderableListWidget::keyPressEvent(event);
}
