#pragma once



#include "core/workflow/WorkflowLoopRegion.h"

#include "ui/BlockListRowMeta.h"

#include "ui/widgets/DragAdjustSpinMouse.h"



#include <QPixmap>

#include <QPoint>

#include <QTableWidget>

#include <QByteArray>

#include <QBrush>

#include <QColor>

#include <QKeyEvent>



class QDragEnterEvent;

class QDragMoveEvent;

class QDragLeaveEvent;

class QDropEvent;

class QResizeEvent;

class QTimer;

class QVariantAnimation;



class BlockListWidget : public QTableWidget {

    Q_OBJECT

public:

    enum class ExecutionHighlight {

        None,

        Running,

        ImageFindMiss,

        Success,

        Failed

    };



    explicit BlockListWidget(QWidget* parent = nullptr);

    static constexpr int PreviewColumn = 1;
    static constexpr int LastDataColumn = 9;

    void setRoiCorrectionColumnVisible(bool visible);

    void setBlockRoiCorrection(int row, bool enabled, bool interactive);

    void setBlockCount(int count);

    void setBlockInfo(int row,

                      const QString& type,

                      const QString& summary,

                      const QPixmap& thumbnail = {});

    void setBlockMatchResult(int row,

                             double matchThreshold,

                             double confidence,

                             const QPixmap& matchedImage,

                             bool matched);

    void setBlockMatchBaseline(int row, double matchThreshold);

    void setBlockDuration(int row, qint64 durationMs);

    void setBlockImageFindMatchDuration(int row, qint64 matchDurationMs);

    void setBlockImageFindAttemptCount(int row, int attemptCount);

    void clearBlockMatchResults();

    void setLoopRegions(const std::vector<WorkflowLoopRegion>& regions);

    void clearLoopRegionVisuals();

    void setLoopRegionPickMode(bool active);

    bool isLoopRegionPickMode() const { return m_loopRegionPickActive; }

    void cancelLoopRegionPick();

    void setReorderEnabled(bool enabled);

    bool isReorderEnabled() const { return m_reorderEnabled; }

    void setActiveRow(int row, ExecutionHighlight highlight = ExecutionHighlight::Running);

    void notifyImageFindRetry(int row);

    void clearActiveRow();

    void commitRowSuccess(int row);

    bool isMatchSuccessLocked(int row) const;

    int blockRowForTableRow(int tableRow) const;

    int tableRowForBlockRow(int blockRow) const;

    int blockCount() const { return m_blockCount; }

    const std::vector<WorkflowLoopRegion>& loopRegions() const { return m_loopRegions; }

    QString loopRegionIdForTableRow(int tableRow) const;

    BlockListRowKind tableRowKind(int tableRow) const;

    const BlockListRowMeta& rowMeta(int tableRow) const;

    void selectBlockRow(int blockRow);

    void applyHeaderResizeModes();

signals:

    void blockRowsReordered(int fromRow, int toRow);

    void loopRegionRangePicked(int startRow, int endRow);

    void loopRegionPickCancelled();

    void loopRegionEditRequested(const QString& regionId);

    void loopRegionDeleteRequested(const QString& regionId);

    void imageFindThresholdChanged(int blockRow, double threshold);

    void imageFindRoiCorrectionChanged(int blockRow, bool enabled);

    void copyRequested();

    void pasteRequested();

    void deleteRequested();

    void undoRequested();

    void redoRequested();



protected:

    bool eventFilter(QObject* watched, QEvent* event) override;

    void keyPressEvent(QKeyEvent* event) override;

    void startDrag(Qt::DropActions supportedActions) override;

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

    void applyActiveRowVisuals();

    void onFlashAnimationValueChanged(const QVariant& value);

    void onFlashAnimationFinished();

    void triggerRowFlash(int row, ExecutionHighlight highlight, int durationMs);

    void updateLoopRegionPickPreview();

    void updateLoopRegionChrome();

    void rebuildTableRows();

    void populateLoopRegionHeaderRow(int tableRow, const WorkflowLoopRegion& region);

    QString loopRegionIdAtViewportPos(const QPoint& viewportPos) const;

    int rowAtViewportY(int viewportY) const;

    int blockRowAtViewportY(int viewportY) const;

    int blockRowForDropInsertion(int insertionIndex) const;

    qreal rowGlassIntensity(int row, ExecutionHighlight highlight) const;

    QColor matchScoreForegroundColor(bool succeeded, bool onMissHighlightRow) const;

    void lockMatchSuccess(int row);

    void markRowCompleted(int row, ExecutionHighlight highlight);

    ExecutionHighlight rowVisualHighlight(int row) const;

    bool imageFindScoreColumnAt(const QPoint& viewportPos, int& blockRowOut) const;

    double imageFindThresholdDragStep(Qt::KeyboardModifiers modifiers) const;

    void updateImageFindThresholdDisplay(int blockRow, double threshold);

    void finishThresholdDrag(QMouseEvent* mouseEvent);

    bool m_roiCorrectionColumnVisible = false;

    bool m_updatingRoiCorrectionItem = false;

    bool m_reorderEnabled = true;

    int m_blockCount = 0;

    QVector<int> m_blockTableRow;

    QVector<int> m_tableRowBlockIndex;

    QVector<QString> m_tableRowRegionId;

    QVector<BlockListRowMeta> m_tableRowMeta;

    int m_dragSourceRow = -1;

    int m_pendingReorderFrom = -1;

    int m_pendingReorderTo = -1;

    int m_dropInsertionIndex = -1;

    int m_activeRow = -1;

    ExecutionHighlight m_activeHighlight = ExecutionHighlight::None;

    QVariantAnimation* m_flashAnimation = nullptr;

    int m_flashRow = -1;

    ExecutionHighlight m_flashKind = ExecutionHighlight::None;

    qreal m_flashIntensity = 0.0;

    QWidget* m_dropIndicator = nullptr;

    QVector<bool> m_rowMatchHasScore;

    QVector<bool> m_rowMatchSucceeded;

    QVector<bool> m_rowMatchLockedSuccess;

    QVector<double> m_rowDisplayConfidences;

    QVector<bool> m_rowIsImageFind;

    QVector<double> m_rowImageFindThresholds;

    QVector<int> m_rowImageFindAttemptCounts;

    DragAdjustSpinMouseState m_thresholdDragMouse;

    int m_thresholdDragBlockRow = -1;

    double m_thresholdDragStartValue = 0.85;

    QVector<ExecutionHighlight> m_rowCompletedHighlight;

    QVector<bool> m_loopRegionMember;

    QVector<bool> m_loopRegionStart;

    QVector<bool> m_loopRegionEnd;

    std::vector<WorkflowLoopRegion> m_loopRegions;

    QWidget* m_loopRegionChrome = nullptr;

    bool m_loopRegionPickActive = false;

    bool m_loopRegionPickDragging = false;

    int m_loopRegionPickAnchorRow = -1;

    int m_loopRegionPickCurrentRow = -1;

    QVector<bool> m_loopRegionPickPreview;

    QString m_defaultToolTip;

};

