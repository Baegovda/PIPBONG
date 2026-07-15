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

#include <QTimer>



class QDragEnterEvent;

class QDragMoveEvent;

class QDragLeaveEvent;

class QDropEvent;

class QResizeEvent;

class QSettings;

class QTimer;

class QVariantAnimation;

class ListColumnHeaderWidget;



struct BlockListColumnLayout {
    int indexWidth = 28;
    int previewWidth = 38;
    int actionWidth = 72;
    int summaryWidth = 160;
    int durationWidth = 72;
    int matchDurationWidth = 72;
    int attemptsWidth = 58;
    int returnPrevWidth = 52;
    int retryWidth = 52;
    int scoreWidth = 78;
    int roiCorrectionWidth = 52;
    int matchWidth = 38;
    int rowHeight = 36;
};



class BlockListWidget : public QTableWidget {

    Q_OBJECT

public:

    enum class ExecutionHighlight {

        None,

        Running,

        ImageFindMiss,

        Success,

        Failed,

        ReturnToPrevious

    };



    explicit BlockListWidget(QWidget* parent = nullptr);

    static constexpr int PreviewColumn = 1;
    static constexpr int LastDataColumn = 11;

    void setRoiCorrectionColumnVisible(bool visible);

    void setBlockRoiCorrection(int row, bool enabled, bool interactive);

    int blockRowHeight() const { return m_blockRowHeight; }
    void setBlockRowHeight(int height, bool persist = true);
    void saveRowHeight(QSettings& settings, const QString& settingsKey) const;
    void restoreRowHeight(const QSettings& settings, const QString& settingsKey);

    const BlockListColumnLayout& columnLayout() const { return m_columnLayout; }
    void setColumnLayout(const BlockListColumnLayout& layout, bool persist = true);
    void saveColumnLayout(QSettings& settings, const QString& settingsKey) const;
    void restoreColumnLayout(const QSettings& settings, const QString& settingsKey);
    static void wireListColumnHeader(ListColumnHeaderWidget* header, BlockListWidget* table);

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
    void setBlockImageFindFailureHandlingCounts(int row,
                                                int returnToPreviousCount,
                                                int retryAfterNextCount);

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

    void notifyImageFindReturnToPrevious(int sourceRow, int targetRow);

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

    void rowHeightChanged();
    void columnLayoutChanged();



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

    void updateHoverTableRow(int tableRow);

    void applyIdleRowBackground(int tableRow);

    void onFlashAnimationValueChanged(const QVariant& value);

    void onFlashAnimationFinished();

    void triggerRowFlash(int row, ExecutionHighlight highlight, int durationMs);

    void triggerDualRowFlash(int primaryRow,
                             int secondaryRow,
                             ExecutionHighlight highlight,
                             int durationMs);

    bool isReturnToPreviousFlashVisible() const;

    void cancelReturnFlashHold();

    void startReturnToPreviousFade(int fadeMs);

    void scrollReturnToPreviousRowsIntoView();

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

    void applyColumnLayoutToTable(bool reconcileSlack = false);
    void reconcileSummarySlack();
    void applyPairwiseColumnResize(int rightColumnIndex,
                                   int deltaX,
                                   const BlockListColumnLayout& startLayout);
    QRect headerColumnRect(const QWidget* header, int column, const QRect& headerRect) const;
    int headerDividerX(const QWidget* header, int rightColumn) const;
    int summaryColumnWidthForViewport(int viewportWidth) const;
    int totalFixedColumnWidth(bool includeSummaryWidth, int summaryWidth) const;
    static int columnWidthFromLayout(const BlockListColumnLayout& layout, int column);
    static void setLayoutColumnWidth(BlockListColumnLayout& layout, int column, int width);
    static void columnWidthBounds(int column, int& minOut, int& maxOut);
    void syncColumnLayoutFromTable(BlockListColumnLayout& layout) const;

    bool m_roiCorrectionColumnVisible = false;
    BlockListColumnLayout m_columnLayout;
    bool m_restoringColumnLayout = false;
    BlockListColumnLayout m_headerDragStartLayout;
    ListColumnHeaderWidget* m_columnHeader = nullptr;

    bool m_updatingRoiCorrectionItem = false;

    bool m_reorderEnabled = true;

    int m_blockCount = 0;

    int m_blockRowHeight = 36; // UiResizeHandle::kDefaultBlockListRowHeightPx

    bool m_restoringRowHeight = false;

    QVector<int> m_blockTableRow;

    QVector<int> m_tableRowBlockIndex;

    QVector<QString> m_tableRowRegionId;

    QVector<BlockListRowMeta> m_tableRowMeta;

    int m_dragSourceRow = -1;

    int m_hoverTableRow = -1;

    int m_pendingReorderFrom = -1;

    int m_pendingReorderTo = -1;

    int m_dropInsertionIndex = -1;

    int m_activeRow = -1;

    ExecutionHighlight m_activeHighlight = ExecutionHighlight::None;

    QVariantAnimation* m_flashAnimation = nullptr;

    QTimer* m_returnFlashHoldTimer = nullptr;

    int m_flashRow = -1;

    int m_flashRowSecondary = -1;

    ExecutionHighlight m_flashKind = ExecutionHighlight::None;

    qreal m_flashIntensity = 0.0;

    QWidget* m_dropIndicator = nullptr;
    QWidget* m_dragSlotPlaceholder = nullptr;

    void playDropSettleAtTableRow(int row);

    QVector<bool> m_rowMatchHasScore;

    QVector<bool> m_rowMatchSucceeded;

    QVector<bool> m_rowMatchLockedSuccess;

    QVector<double> m_rowDisplayConfidences;

    QVector<bool> m_rowIsImageFind;

    QVector<double> m_rowImageFindThresholds;

    QVector<int> m_rowImageFindAttemptCounts;
    QVector<int> m_rowImageFindReturnCounts;
    QVector<int> m_rowImageFindRetryCounts;

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

