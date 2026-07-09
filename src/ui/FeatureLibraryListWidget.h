#pragma once

#include "ui/widgets/ReorderableListWidget.h"

class QMimeData;

class FeatureLibraryListWidget : public ReorderableListWidget {
    Q_OBJECT
public:
    explicit FeatureLibraryListWidget(QWidget* parent = nullptr);

    void setTransferEnabled(bool enabled);
    bool transferEnabled() const { return m_transferEnabled; }

signals:
    void featureDroppedOnLibrary(const QMimeData* mime);
    void libraryRowsReordered(int fromRow, int toRow);
    void deleteRequested();

protected:
    void keyPressEvent(QKeyEvent* event) override;
    QMimeData* buildDragMimeData(int row) const override;
    bool acceptsExternalMime(const QMimeData* mime) const override;
    Qt::DropAction preferredExternalDropAction(const QMimeData* mime) const override;

private:
    bool m_transferEnabled = true;
};
