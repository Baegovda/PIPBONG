#pragma once

#include "ui/widgets/ReorderableListWidget.h"

#include <functional>

class QDropEvent;
class QKeyEvent;
class QMimeData;

class FeatureListWidget : public ReorderableListWidget {
    Q_OBJECT
public:
    explicit FeatureListWidget(QWidget* parent = nullptr);

    int dragSourceRow() const;

    void setActiveProfileId(const QString& profileId) { m_activeProfileId = profileId; }
    QString activeProfileId() const { return m_activeProfileId; }

    void setRowDragEnabledPredicate(std::function<bool(int row)> predicate);
    void setGroupDragMimeBuilder(std::function<QMimeData*(int row)> builder);
    void setGroupDropHandler(std::function<bool(int row, const QMimeData* mime)> handler);
    void setGroupDragPrepare(std::function<void(int row)> prepare);

signals:
    void featureRowsReordered(int fromRow, int toRow);
    void featureDropped(const QMimeData* mime, int insertIndex);
    void deleteRequested();
    void copyRequested();
    void pasteRequested();
    void renameRequested();

protected:
    void startDrag(Qt::DropActions supportedActions) override;
    QMimeData* buildDragMimeData(int row) const override;
    bool canStartDragFromRow(int row) const override;
    bool acceptsExternalMime(const QMimeData* mime) const override;
    Qt::DropAction preferredExternalDropAction(const QMimeData* mime) const override;
    void dropEvent(QDropEvent* event) override;
    void keyPressEvent(QKeyEvent* event) override;

private:
    bool isGroupListRow(int row) const;

    QString m_activeProfileId;
    std::function<bool(int row)> m_rowDragEnabledPredicate;
    std::function<QMimeData*(int row)> m_groupDragMimeBuilder;
    std::function<bool(int row, const QMimeData* mime)> m_groupDropHandler;
    std::function<void(int row)> m_groupDragPrepare;
};
