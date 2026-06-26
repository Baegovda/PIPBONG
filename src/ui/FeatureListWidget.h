#pragma once

#include <QListWidget>

class QDragEnterEvent;
class QDragLeaveEvent;
class QDragMoveEvent;
class QDropEvent;

class FeatureListWidget : public QListWidget {
    Q_OBJECT
public:
    explicit FeatureListWidget(QWidget* parent = nullptr);

    void setReorderEnabled(bool enabled);
    bool reorderEnabled() const { return m_reorderEnabled; }
    int dragSourceRow() const { return m_dragSourceRow; }

signals:
    void featureRowsReordered(int fromRow, int toRow);
    void deleteRequested();

protected:
    void startDrag(Qt::DropActions supportedActions) override;
    void keyPressEvent(QKeyEvent* event) override;
    void dragEnterEvent(QDragEnterEvent* event) override;
    void dragMoveEvent(QDragMoveEvent* event) override;
    void dragLeaveEvent(QDragLeaveEvent* event) override;
    void dropEvent(QDropEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;

private:
    int dropInsertionIndex(const QPoint& pos) const;
    int insertionLineY(int insertionIndex) const;
    int dropTargetRow(const QPoint& pos) const;
    void updateDropIndicator();
    void clearDropIndicator();
    void updateDragSourceVisuals();

    bool m_reorderEnabled = true;
    int m_dragSourceRow = -1;
    int m_pendingReorderFrom = -1;
    int m_pendingReorderTo = -1;
    int m_dropInsertionIndex = -1;
    QWidget* m_dropIndicator = nullptr;
};
