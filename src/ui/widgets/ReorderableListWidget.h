#pragma once

#include <QListWidget>

class QDragEnterEvent;
class QDragLeaveEvent;
class QDragMoveEvent;
class QDropEvent;
class QMimeData;

/// Shared list drag policy: internal reorder with insertion line, optional cross-widget drops.
/// Subclasses override the virtual hooks; emit rowsReordered / externalItemDropped for persistence.
class ReorderableListWidget : public QListWidget {
    Q_OBJECT
public:
    explicit ReorderableListWidget(QWidget* parent = nullptr);

    void setReorderEnabled(bool enabled);
    bool reorderEnabled() const { return m_reorderEnabled; }
    int dragSourceRow() const { return m_dragSourceRow; }

signals:
    void rowsReordered(int fromRow, int toRow);
    /// @a insertIndex is the row gap index for ordered insert; -1 when not applicable.
    void externalItemDropped(const QMimeData* mime, int insertIndex);

protected:
    void startDrag(Qt::DropActions supportedActions) override;
    void dragEnterEvent(QDragEnterEvent* event) override;
    void dragMoveEvent(QDragMoveEvent* event) override;
    void dragLeaveEvent(QDragLeaveEvent* event) override;
    void dropEvent(QDropEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;

    virtual bool canStartDragFromRow(int row) const;
    virtual int minimumDropInsertionIndex() const;
    virtual int maximumDropInsertionIndex() const;
    virtual QMimeData* buildDragMimeData(int row) const;
    virtual bool acceptsExternalMime(const QMimeData* mime) const;
    virtual Qt::DropAction preferredExternalDropAction(const QMimeData* mime) const;

    int dropInsertionIndex(const QPoint& pos) const;
    int dropTargetRow(const QPoint& pos) const;

private:
    int insertionLineY(int insertionIndex) const;
    void updateDropIndicator();
    void clearDropIndicator();
    void updateDragSourceVisuals();
    bool showInsertionIndicator() const;

    bool m_reorderEnabled = true;
    bool m_externalDropHover = false;
    int m_dragSourceRow = -1;
    int m_pendingReorderFrom = -1;
    int m_pendingReorderTo = -1;
    int m_dropInsertionIndex = -1;
    QWidget* m_dropIndicator = nullptr;
    QWidget* m_dragSlotPlaceholder = nullptr;

    void playDropSettleAtRow(int row);
};
