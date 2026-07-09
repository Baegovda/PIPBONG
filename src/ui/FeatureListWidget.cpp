#include "ui/FeatureListWidget.h"

#include "ui/FeatureDragMime.h"

#include <QKeyEvent>
#include <QListWidgetItem>
#include <QMimeData>

#include <algorithm>

namespace {

QStringList selectedIdsInRowOrder(const QListWidget* list) {
    QStringList ids;
    if (!list) {
        return ids;
    }
    QList<int> rows;
    for (QListWidgetItem* selected : list->selectedItems()) {
        if (!selected) {
            continue;
        }
        const int row = list->row(selected);
        if (row >= 0) {
            rows.push_back(row);
        }
    }
    std::sort(rows.begin(), rows.end());
    rows.erase(std::unique(rows.begin(), rows.end()), rows.end());
    for (int row : rows) {
        if (QListWidgetItem* item = list->item(row)) {
            const QString id = item->data(Qt::UserRole).toString();
            if (!id.isEmpty()) {
                ids.push_back(id);
            }
        }
    }
    return ids;
}

} // namespace

FeatureListWidget::FeatureListWidget(QWidget* parent)
    : ReorderableListWidget(parent) {
    setSelectionMode(QAbstractItemView::ExtendedSelection);
    setToolTip(tr("Ctrl/Shift+클릭 다중 선택 · 드래그하여 기능 순서 변경 · 기능/라이브러리/프로필 간 드래그 복사 · Ctrl+C/V · Delete"));
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
    Q_UNUSED(row);
    if (m_activeProfileId.isEmpty()) {
        return nullptr;
    }
    const QStringList ids = selectedIdsInRowOrder(this);
    if (ids.isEmpty()) {
        return nullptr;
    }

    FeatureDragMime::Payload payload;
    payload.source = FeatureDragMime::Source::Profile;
    payload.ids = ids;
    payload.id = ids.first();
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
