#pragma once

#include <QWidget>

#include <functional>

class QPainter;
class QPalette;

/// Shared draggable list/table column header (feature list + workflow block list).
class ListColumnHeaderWidget : public QWidget {
    Q_OBJECT

public:
    struct PaintContext {
        QPainter* painter = nullptr;
        QRect rect;
        QPalette palette;
    };

    using RowHeightFn = std::function<int()>;
    using PaintLabelsFn = std::function<void(PaintContext&)>;
    using DividerXsFn = std::function<QList<int>(const QRect& rect)>;
    using DividerHitFn = std::function<int(const QPoint& pos, const QRect& rect)>;
    using ApplyDragFn = std::function<void(int handleId, int deltaX, int deltaY, const QPoint& pos)>;

    explicit ListColumnHeaderWidget(QWidget* parent = nullptr);

    void setRowHeightProvider(RowHeightFn fn);
    void setPaintLabelsProvider(PaintLabelsFn fn);
    void setDividerXsProvider(DividerXsFn fn);
    void setDividerHitProvider(DividerHitFn fn);
    void setApplyDragProvider(ApplyDragFn fn);

    void syncToLayout();

    QSize sizeHint() const override;

    static void paintHeaderChrome(QPainter* painter, const QRect& rect, const QPalette& pal);
    static void paintDividerGuides(QPainter* painter,
                                   const QList<int>& dividerXs,
                                   int height,
                                   const QPalette& pal);
    static QColor headerTextColor(const QPalette& pal);

signals:
    void dividerDragFinished();
    void dividerDragStarted();

protected:
    void changeEvent(QEvent* event) override;
    void paintEvent(QPaintEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;

private:
    static constexpr int kHeaderExtraHeight = 4;
    static constexpr int kRowHeightHandleId = -1;

    int handleAt(const QPoint& pos) const;
    static Qt::CursorShape cursorForHandle(int handleId);
    void applyDrag(const QPoint& pos);
    void updateHeight();

    RowHeightFn m_rowHeight;
    PaintLabelsFn m_paintLabels;
    DividerXsFn m_dividerXs;
    DividerHitFn m_dividerHit;
    ApplyDragFn m_applyDrag;

    int m_activeHandle = 0;
    QPoint m_dragStartPos;
    int m_dragGuideX = 0;
    int m_dragGuideY = 0;
};
