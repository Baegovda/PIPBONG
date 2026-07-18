#pragma once

#include "ui/widgets/ReorderableListWidget.h"

#include <functional>

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

signals:
    void featureRowsReordered(int fromRow, int toRow);
    void featureDropped(const QMimeData* mime, int insertIndex);
    void deleteRequested();
    void copyRequested();
    void pasteRequested();
    void renameRequested();

protected:
    QMimeData* buildDragMimeData(int row) const override;
    bool canStartDragFromRow(int row) const override;
    bool acceptsExternalMime(const QMimeData* mime) const override;
    Qt::DropAction preferredExternalDropAction(const QMimeData* mime) const override;
    void keyPressEvent(QKeyEvent* event) override;

private:
    QString m_activeProfileId;
    std::function<bool(int row)> m_rowDragEnabledPredicate;
};
