#pragma once

#include "core/workflow/Block.h"
#include "core/workflow/Workflow.h"

#include <QWidget>

#include <QByteArray>
#include <QList>
#include <QPixmap>
#include <QVector>

#include <algorithm>
#include <memory>
#include <unordered_map>
#include <vector>

#include "ui/BlockListWidget.h"

class QLabel;
class QPushButton;
class QSplitter;
class ListColumnHeaderWidget;
class Feature;
class Block;
class BlockEditorDialog;

class WorkflowEditorPanel : public QWidget {
    Q_OBJECT
public:
    explicit WorkflowEditorPanel(QWidget* parent = nullptr);

    void setFeature(Feature* feature);
    void setProjectDirectory(const QString& directory);
    void refresh();
    void setEditingEnabled(bool enabled);
    void clearBlockMatchResults();
    void setActiveBlockIndex(int index, BlockListWidget::ExecutionHighlight highlight = BlockListWidget::ExecutionHighlight::Running);
    void notifyImageFindRetry(int blockIndex);
    void notifyImageFindReturnToPrevious(int sourceBlockIndex, int targetBlockIndex);
    bool isBlockMatchSuccessCommitted(int blockIndex) const;
    void clearExecutionHighlight();
    void setBlockMatchResult(int blockIndex,
                             double matchThreshold,
                             double confidence,
                             const QPixmap& image,
                             bool matched);
    void markBlockMatchSuccess(int blockIndex);
    void setBlockDuration(int blockIndex, qint64 durationMs);
    void setBlockImageFindMatchDuration(int blockIndex, qint64 matchDurationMs);
    void setBlockImageFindAttemptCount(int blockIndex, int attemptCount);
    void setBlockImageFindFailureHandlingCounts(int blockIndex,
                                                int returnToPreviousCount,
                                                int retryAfterNextCount);
    void setLoopTiming(int loopNumber, qint64 elapsedMs, qint64 averageMs, bool success);
    void clearLoopTiming();
    void persistRunFeedbackForCurrentFeature();

    QSplitter* workflowSplitter() const;
    void clampWorkflowSplitterSizes();

    BlockListWidget* blockList() const { return m_blockList; }
    ListColumnHeaderWidget* blockListColumnHeader() const { return m_blockListHeader; }

signals:
    void workflowModified();

protected:
    void resizeEvent(QResizeEvent* event) override;

private slots:
    void onEditBlock();
    void onRemoveBlock();
    void onMoveUp();
    void onMoveDown();
    void onBlockDoubleClicked(int row);
    void onBlockRowsReordered(int fromRow, int toRow);
    void onCopyBlocks();
    void onPasteBlocks();
    void onDeleteBlocks();
    void onUndo();
    void onRedo();
    void onRemoveAllWaitBlocks();
    void onInsertWaitBetweenBlocks();
    void onManageLoopRegions();
    void onLoopRegionRangePicked(int startRow, int endRow);
    void onLoopRegionPickCancelled();
    void onLoopRegionEditRequested(const QString& regionId);
    void onLoopRegionDeleteRequested(const QString& regionId);
    void onImageFindThresholdChanged(int blockRow, double threshold);
    void onImageFindRoiCorrectionChanged(int blockRow, bool enabled);
    void onLoopRegionsButtonContextMenu(const QPoint& pos);
    void openLoopRegionsListDialog();
    void setLoopRegionPickMode(bool active);

private:
    void setupUi();
    void addBlockOfType(BlockType type);
    bool editBlockAt(int row);
    void editLoopRegion(const QString& regionId);
    void deleteLoopRegion(const QString& regionId);
    void deleteBlockAt(int row);
    QList<int> selectedBlockRows() const;
    void selectBlockRows(const QList<int>& rows);
    void removeSelectedBlocks();
    void pushUndoSnapshot();
    void restoreFromSnapshot(const Workflow& snapshot);
    void clearWorkflowHistory();
    void updateWorkflowToolButtonStates();
    void updateTitleText();

    static constexpr int kMaxUndoDepth = 100;

    struct FeatureRunFeedback {
        QVector<QPixmap> rowMatchImages;
        QVector<double> rowMatchConfidences;
        QVector<double> rowMatchThresholds;
        QVector<bool> rowMatchMatched;
        QVector<qint64> rowBlockDurations;
        QVector<qint64> rowImageFindMatchDurations;
        QVector<int> rowImageFindAttemptCounts;
        QVector<int> rowImageFindReturnCounts;
        QVector<int> rowImageFindRetryCounts;
        QVector<bool> rowMatchLockedSuccess;
        int activeBlockIndex = -1;
        BlockListWidget::ExecutionHighlight executionHighlight = BlockListWidget::ExecutionHighlight::None;
        bool hasLoopTiming = false;
        int loopNumber = 0;
        qint64 loopElapsedMs = 0;
        qint64 loopAverageMs = 0;
        bool loopSuccess = true;
    };

    void saveRunFeedbackForFeature(const std::string& featureId);
    void restoreRunFeedbackForFeature(const std::string& featureId);
    void clearCurrentRunFeedbackVectors();

    Feature* m_feature = nullptr;
    QString m_projectDirectory;

    QLabel* m_titleLabel = nullptr;
    bool m_hasLoopTiming = false;
    int m_loopNumber = 0;
    qint64 m_loopElapsedMs = 0;
    qint64 m_loopAverageMs = 0;
    bool m_loopSuccess = true;
    BlockListWidget* m_blockList = nullptr;
    ListColumnHeaderWidget* m_blockListHeader = nullptr;
    QSplitter* m_workflowSplitter = nullptr;
    QVector<QPushButton*> m_addTypeButtons;
    QPushButton* m_removeAllWaitButton = nullptr;
    QPushButton* m_insertWaitBetweenButton = nullptr;
    QPushButton* m_loopRegionsButton = nullptr;
    bool m_loopRegionPickActive = false;

    bool m_editingEnabled = true;

    int m_activeBlockIndex = -1;
    BlockListWidget::ExecutionHighlight m_executionHighlight = BlockListWidget::ExecutionHighlight::None;
    QVector<QPixmap> m_rowMatchImages;
    QVector<double> m_rowMatchConfidences;
    QVector<double> m_rowMatchThresholds;
    QVector<bool> m_rowMatchMatched;
    QVector<qint64> m_rowBlockDurations;
    QVector<qint64> m_rowImageFindMatchDurations;
    QVector<int> m_rowImageFindAttemptCounts;
    QVector<int> m_rowImageFindReturnCounts;
    QVector<int> m_rowImageFindRetryCounts;

    std::unordered_map<std::string, FeatureRunFeedback> m_featureRunFeedback;

    std::vector<std::unique_ptr<Block>> m_clipboardBlocks;
    std::vector<std::unique_ptr<Workflow>> m_undoStack;
    std::vector<std::unique_ptr<Workflow>> m_redoStack;
};
