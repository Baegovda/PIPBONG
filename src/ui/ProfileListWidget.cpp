#include "ui/ProfileListWidget.h"

#include "ui/FeatureDragMime.h"

#include <QDragEnterEvent>
#include <QDragLeaveEvent>
#include <QDragMoveEvent>
#include <QDropEvent>
#include <QListWidgetItem>
#include <QMimeData>
#include <QMouseEvent>

ProfileListWidget::ProfileListWidget(QWidget* parent)
    : QListWidget(parent) {}

void ProfileListWidget::setFeatureDropEnabled(bool enabled) {
    m_featureDropEnabled = enabled;
    if (!enabled) {
        updateFeatureDropHighlight(-1);
    }
}

void ProfileListWidget::mousePressEvent(QMouseEvent* event) {
    if (event && event->button() == Qt::LeftButton) {
        if (QListWidgetItem* hit = itemAt(event->position().toPoint())) {
            setCurrentItem(hit);
        }
    }
    QListWidget::mousePressEvent(event);
}

void ProfileListWidget::dragEnterEvent(QDragEnterEvent* event) {
    if (m_featureDropEnabled && FeatureDragMime::accepts(event->mimeData())) {
        const int highlightRow = this->row(itemAt(event->position().toPoint()));
        updateFeatureDropHighlight(highlightRow);
        event->acceptProposedAction();
        return;
    }
    updateFeatureDropHighlight(-1);
    QListWidget::dragEnterEvent(event);
}

void ProfileListWidget::dragMoveEvent(QDragMoveEvent* event) {
    if (m_featureDropEnabled && FeatureDragMime::accepts(event->mimeData())) {
        const int highlightRow = this->row(itemAt(event->position().toPoint()));
        updateFeatureDropHighlight(highlightRow);
        event->acceptProposedAction();
        return;
    }
    updateFeatureDropHighlight(-1);
    QListWidget::dragMoveEvent(event);
}

void ProfileListWidget::dragLeaveEvent(QDragLeaveEvent* event) {
    updateFeatureDropHighlight(-1);
    QListWidget::dragLeaveEvent(event);
}

QString ProfileListWidget::profileIdAt(const QPoint& pos) const {
    QListWidgetItem* item = itemAt(pos);
    if (!item) {
        return {};
    }
    return item->data(Qt::UserRole).toString();
}

void ProfileListWidget::updateFeatureDropHighlight(int row) {
    if (m_featureDropHighlightRow == row) {
        return;
    }
    if (m_featureDropHighlightRow >= 0 && m_featureDropHighlightRow < count()) {
        if (QListWidgetItem* previous = item(m_featureDropHighlightRow)) {
            previous->setData(Qt::UserRole + 1, {});
        }
    }
    m_featureDropHighlightRow = row;
    if (row >= 0 && row < count()) {
        if (QListWidgetItem* current = item(row)) {
            current->setData(Qt::UserRole + 1, QStringLiteral("featureDropHover"));
        }
    }
    viewport()->update();
}

void ProfileListWidget::dropEvent(QDropEvent* event) {
    updateFeatureDropHighlight(-1);

    if (m_featureDropEnabled && FeatureDragMime::accepts(event->mimeData())) {
        const QString profileId = profileIdAt(event->position().toPoint());
        if (profileId.isEmpty()) {
            event->ignore();
            return;
        }

        const FeatureDragMime::Payload payload = FeatureDragMime::parse(event->mimeData());
        if (!payload.isValid()) {
            event->ignore();
            return;
        }

        if (payload.source == FeatureDragMime::Source::Profile
            && payload.profileId == profileId) {
            event->ignore();
            return;
        }

        emit featureDroppedOnProfile(profileId, event->mimeData());
        event->setDropAction(payload.source == FeatureDragMime::Source::Library ? Qt::CopyAction
                                                                              : Qt::MoveAction);
        event->accept();
        return;
    }

    QListWidget::dropEvent(event);
}
