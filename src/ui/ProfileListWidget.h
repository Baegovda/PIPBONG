#pragma once

#include "ui/widgets/ReorderableListWidget.h"

class QMimeData;
class QMouseEvent;
class QStyledItemDelegate;

class ProfileListWidget : public ReorderableListWidget {
    Q_OBJECT
public:
    explicit ProfileListWidget(QWidget* parent = nullptr);

    void setDefaultProfileId(const QString& profileId);
    void setFeatureDropEnabled(bool enabled);
    bool featureDropEnabled() const { return m_featureDropEnabled; }

signals:
    void featureDroppedOnProfile(const QString& profileId, const QMimeData* mime);

protected:
    void mousePressEvent(QMouseEvent* event) override;
    bool canStartDragFromRow(int row) const override;
    int minimumDropInsertionIndex() const override;
    void dragEnterEvent(QDragEnterEvent* event) override;
    void dragMoveEvent(QDragMoveEvent* event) override;
    void dragLeaveEvent(QDragLeaveEvent* event) override;
    void dropEvent(QDropEvent* event) override;

private:
    QString profileIdAt(const QPoint& pos) const;
    bool isDefaultProfileItem(QListWidgetItem* item) const;
    void updateFeatureDropHighlight(int row);

    QString m_defaultProfileId;
    bool m_featureDropEnabled = true;
    int m_featureDropHighlightRow = -1;
    QStyledItemDelegate* m_itemDelegate = nullptr;
};
