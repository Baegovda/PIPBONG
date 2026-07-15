#pragma once

#include <QDialog>
#include <QPoint>

class QLabel;
class QPushButton;
class QTextEdit;
class QTimer;
class QMouseEvent;
class QPaintEvent;
class QResizeEvent;

class MemoDialog : public QDialog {
    Q_OBJECT
public:
    explicit MemoDialog(QWidget* parent = nullptr);

    /// Saves the current editor to the previous profile folder, then loads @p profileDirectory.
    void setProfile(const QString& profileId,
                    const QString& profileDirectory,
                    const QString& profileDisplayName);
    void saveNow();

protected:
    void closeEvent(QCloseEvent* event) override;
    void showEvent(QShowEvent* event) override;
    void paintEvent(QPaintEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;

private:
    void scheduleSave();
    void persistGeometry();
    void restorePersistedGeometry();
    void updateWindowTitle();
    void updateChrome();
    bool isInDragRegion(const QPoint& pos) const;

    QString m_profileId;
    QString m_profileDirectory;
    QString m_profileDisplayName;
    QTextEdit* m_editor = nullptr;
    QLabel* m_profileLabel = nullptr;
    QPushButton* m_closeButton = nullptr;
    QTimer* m_saveTimer = nullptr;
    bool m_loading = false;
    bool m_dragging = false;
    QPoint m_dragOffset;
    int m_headerHeight = 32;
};
