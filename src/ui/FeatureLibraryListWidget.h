#pragma once

#include <QListWidget>

class QDragEnterEvent;
class QDragMoveEvent;
class QDropEvent;
class QMimeData;

class FeatureLibraryListWidget : public QListWidget {
    Q_OBJECT
public:
    explicit FeatureLibraryListWidget(QWidget* parent = nullptr);

    void setTransferEnabled(bool enabled);
    bool transferEnabled() const { return m_transferEnabled; }

signals:
    void featureDroppedOnLibrary(const QMimeData* mime);

protected:
    void startDrag(Qt::DropActions supportedActions) override;
    void dragEnterEvent(QDragEnterEvent* event) override;
    void dragMoveEvent(QDragMoveEvent* event) override;
    void dragLeaveEvent(QDragLeaveEvent* event) override;
    void dropEvent(QDropEvent* event) override;

private:
    bool m_transferEnabled = true;
};
