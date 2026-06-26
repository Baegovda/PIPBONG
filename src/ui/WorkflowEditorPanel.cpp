#include "ui/WorkflowEditorPanel.h"

#include "core/workflow/BlockFactory.h"
#include "core/workflow/blocks/ClickBlock.h"
#include "core/workflow/blocks/ImageFindBlock.h"
#include "core/workflow/blocks/KeyPressBlock.h"
#include "core/workflow/blocks/WaitBlock.h"
#include "core/workflow/WorkflowLoopRegion.h"
#include "core/input/HotkeyBinding.h"
#include "core/input/InputSimulator.h"
#include "model/Feature.h"
#include "ui/BlockListWidget.h"
#include "ui/HotkeyBindingIcon.h"
#include "ui/editors/WorkflowLoopRegionsDialog.h"
#include "ui/UiStrings.h"
#include "ui/editors/BlockEditorDialog.h"

#include <QDialog>
#include <QDir>
#include <QFileInfo>
#include <QFont>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QInputDialog>
#include <QItemSelection>
#include <QItemSelectionModel>
#include <QKeySequence>
#include <QLabel>
#include <QMenu>
#include <QMessageBox>
#include <QPainter>
#include <QPixmap>
#include <QPolygonF>
#include <QPushButton>
#include <QSet>
#include <QSizePolicy>
#include <QSplitter>
#include <QVBoxLayout>

#include <QtMath>

#include <algorithm>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#endif

namespace {

constexpr int kThumbnailSize = 32;

bool loopRegionOverlaps(const std::vector<WorkflowLoopRegion>& regions,
                        int startIndex,
                        int endIndex,
                        const std::string& excludeId = {}) {
    for (const WorkflowLoopRegion& region : regions) {
        if (!excludeId.empty() && region.id == excludeId) {
            continue;
        }
        if (startIndex <= region.endIndex && region.startIndex <= endIndex) {
            return true;
        }
    }
    return false;
}

QRectF mouseBodyRect() {
    return QRectF(7.0, 4.0, 18.0, 24.0);
}

void drawMouseBody(QPainter& painter, const QRectF& body) {
    painter.setPen(QPen(QColor(90, 90, 90), 1.2));
    painter.setBrush(QColor(245, 245, 245));
    painter.drawRoundedRect(body, 4.0, 4.0);

    painter.setPen(QPen(QColor(90, 90, 90), 1.0));
    painter.drawLine(body.center().x(), body.top() + 1.0, body.center().x(), body.top() + body.height() * 0.55);
    painter.drawLine(body.left() + 2.0,
                     body.top() + body.height() * 0.55,
                     body.right() - 2.0,
                     body.top() + body.height() * 0.55);
}

QPixmap clickBlockPreviewIcon(MouseButton button) {
    QPixmap pixmap(kThumbnailSize, kThumbnailSize);
    pixmap.fill(Qt::transparent);

    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing);

    const QRectF body = mouseBodyRect();
    const QColor accent(74, 144, 217);

    switch (button) {
    case MouseButton::Left: {
        drawMouseBody(painter, body);
        const QRectF leftButton(body.left(), body.top(), body.width() * 0.5, body.height() * 0.55);
        painter.setPen(Qt::NoPen);
        painter.setBrush(accent);
        painter.drawRoundedRect(leftButton, 3.0, 3.0);
        break;
    }
    case MouseButton::Right: {
        drawMouseBody(painter, body);
        const QRectF rightButton(body.left() + body.width() * 0.5, body.top(), body.width() * 0.5, body.height() * 0.55);
        painter.setPen(Qt::NoPen);
        painter.setBrush(accent);
        painter.drawRoundedRect(rightButton, 3.0, 3.0);
        break;
    }
    case MouseButton::Middle: {
        drawMouseBody(painter, body);
        const QRectF wheelSlot(body.center().x() - 2.5, body.top() + body.height() * 0.58, 5.0, 7.0);
        painter.setPen(QPen(QColor(90, 90, 90), 1.0));
        painter.setBrush(accent);
        painter.drawRoundedRect(wheelSlot, 2.0, 2.0);
        break;
    }
    case MouseButton::Back:
        drawMouseSideNavigationIcon(painter, false);
        break;
    case MouseButton::Forward:
        drawMouseSideNavigationIcon(painter, true);
        break;
    case MouseButton::WheelUp:
    case MouseButton::WheelDown: {
        const QRectF wheelBody(10.0, 6.0, 12.0, 20.0);
        painter.setPen(QPen(QColor(90, 90, 90), 1.2));
        painter.setBrush(QColor(245, 245, 245));
        painter.drawRoundedRect(wheelBody, 3.0, 3.0);

        painter.setPen(QPen(accent, 2.0, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
        const qreal cx = wheelBody.center().x();
        const qreal cy = wheelBody.center().y();
        if (button == MouseButton::WheelUp) {
            painter.drawLine(QPointF(cx, cy + 4.0), QPointF(cx, cy - 5.0));
            painter.drawLine(QPointF(cx, cy - 5.0), QPointF(cx - 4.0, cy - 1.0));
            painter.drawLine(QPointF(cx, cy - 5.0), QPointF(cx + 4.0, cy - 1.0));
        } else {
            painter.drawLine(QPointF(cx, cy - 4.0), QPointF(cx, cy + 5.0));
            painter.drawLine(QPointF(cx, cy + 5.0), QPointF(cx - 4.0, cy + 1.0));
            painter.drawLine(QPointF(cx, cy + 5.0), QPointF(cx + 4.0, cy + 1.0));
        }
        break;
    }
    }

    return pixmap;
}

QString compactKeyLabel(int virtualKey) {
#ifdef _WIN32
    if (virtualKey >= 'A' && virtualKey <= 'Z') {
        return QString(QChar(virtualKey));
    }
    if (virtualKey >= '0' && virtualKey <= '9') {
        return QString(QChar(virtualKey));
    }
    switch (virtualKey) {
    case VK_SPACE:
        return QStringLiteral("Sp");
    case VK_RETURN:
        return QStringLiteral("↵");
    case VK_ESCAPE:
        return QStringLiteral("Esc");
    case VK_TAB:
        return QStringLiteral("Tab");
    case VK_BACK:
        return QStringLiteral("⌫");
    case VK_DELETE:
        return QStringLiteral("Del");
    case VK_HOME:
        return QStringLiteral("Hm");
    case VK_END:
        return QStringLiteral("End");
    case VK_PRIOR:
        return QStringLiteral("PU");
    case VK_NEXT:
        return QStringLiteral("PD");
    case VK_LEFT:
        return QStringLiteral("←");
    case VK_RIGHT:
        return QStringLiteral("→");
    case VK_UP:
        return QStringLiteral("↑");
    case VK_DOWN:
        return QStringLiteral("↓");
    case VK_F1:
    case VK_F2:
    case VK_F3:
    case VK_F4:
    case VK_F5:
    case VK_F6:
    case VK_F7:
    case VK_F8:
    case VK_F9:
    case VK_F10:
    case VK_F11:
    case VK_F12:
        return QStringLiteral("F%1").arg(virtualKey - VK_F1 + 1);
    default: {
        const int qtKey = HotkeyBinding::virtualKeyToQtKey(virtualKey);
        const QString native = QKeySequence(qtKey).toString(QKeySequence::NativeText);
        if (!native.isEmpty() && native.size() <= 4) {
            return native;
        }
        return QStringLiteral("K");
    }
    }
#else
    (void)virtualKey;
    return QStringLiteral("K");
#endif
}

QString modifierPrefix(const KeyPressModifierActions& modifierActions) {
    QStringList parts;
    if (modifierActions.ctrl != ModifierKeyAction::None) {
        parts << QStringLiteral("C");
    }
    if (modifierActions.alt != ModifierKeyAction::None) {
        parts << QStringLiteral("A");
    }
    if (modifierActions.shift != ModifierKeyAction::None) {
        parts << QStringLiteral("S");
    }
    return parts.join(QStringLiteral("+"));
}

QPixmap keyPressPreviewIcon(int virtualKey,
                            const KeyPressModifierActions& modifierActions,
                            bool useMainKey) {
    QPixmap pixmap(kThumbnailSize, kThumbnailSize);
    pixmap.fill(Qt::transparent);

    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setRenderHint(QPainter::TextAntialiasing);

    const QRectF cap(3.0, 5.0, 26.0, 22.0);
    painter.setPen(QPen(QColor(100, 100, 100), 1.1));
    painter.setBrush(QColor(248, 248, 248));
    painter.drawRoundedRect(cap, 3.0, 3.0);

    const QString modText = modifierPrefix(modifierActions);
    const QString keyText = useMainKey ? compactKeyLabel(virtualKey) : QString();

    if (!modText.isEmpty()) {
        QFont modFont = painter.font();
        modFont.setPointSize(useMainKey && !keyText.isEmpty() ? 6 : 8);
        modFont.setBold(true);
        painter.setFont(modFont);
        painter.setPen(QColor(90, 120, 160));
        if (useMainKey && !keyText.isEmpty()) {
            painter.drawText(QRectF(cap.left(), cap.top() + 1.0, cap.width(), 8.0),
                             Qt::AlignHCenter | Qt::AlignTop,
                             modText);
        } else {
            painter.drawText(cap, Qt::AlignCenter, modText);
        }
    }

    if (!keyText.isEmpty()) {
        QFont keyFont = painter.font();
        keyFont.setPointSize(keyText.size() > 2 ? 7 : 10);
        keyFont.setBold(true);
        painter.setFont(keyFont);
        painter.setPen(QColor(50, 50, 50));
        const qreal textTop = modText.isEmpty() ? cap.top() : cap.top() + 8.0;
        painter.drawText(QRectF(cap.left(), textTop, cap.width(), cap.bottom() - textTop),
                         Qt::AlignCenter,
                         keyText);
    }

    return pixmap;
}

QPixmap waitBlockPreviewIcon() {
    QPixmap pixmap(kThumbnailSize, kThumbnailSize);
    pixmap.fill(Qt::transparent);

    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing);

    const QRectF face(6.0, 4.0, 20.0, 20.0);
    const QPointF center = face.center();
    const QColor accent(74, 144, 217);

    painter.setPen(QPen(QColor(90, 90, 90), 1.2));
    painter.setBrush(QColor(248, 248, 248));
    painter.drawEllipse(face);

    painter.setPen(QPen(QColor(180, 180, 180), 1.0));
    for (int i = 0; i < 12; ++i) {
        const qreal angleDeg = i * 30.0 - 90.0;
        const qreal angleRad = qDegreesToRadians(angleDeg);
        const qreal innerR = face.width() * 0.38;
        const qreal outerR = face.width() * 0.46;
        const QPointF inner(center.x() + innerR * qCos(angleRad), center.y() + innerR * qSin(angleRad));
        const QPointF outer(center.x() + outerR * qCos(angleRad), center.y() + outerR * qSin(angleRad));
        painter.drawLine(inner, outer);
    }

    painter.setPen(QPen(accent, 2.0, Qt::SolidLine, Qt::RoundCap));
    painter.drawLine(center, QPointF(center.x(), center.y() - face.height() * 0.22));
    painter.setPen(QPen(accent, 1.6, Qt::SolidLine, Qt::RoundCap));
    painter.drawLine(center, QPointF(center.x() + face.width() * 0.28, center.y()));

    painter.setPen(Qt::NoPen);
    painter.setBrush(accent);
    painter.drawEllipse(center, 1.8, 1.8);

    return pixmap;
}

QPixmap loopBlockPreviewIcon() {
    QPixmap pixmap(kThumbnailSize, kThumbnailSize);
    pixmap.fill(Qt::transparent);

    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing);

    const QColor accent(74, 144, 217);
    painter.setPen(QPen(accent, 1.6, Qt::SolidLine, Qt::RoundCap));
    painter.setBrush(Qt::NoBrush);

    const QRectF arcRect(6.0, 6.0, 20.0, 20.0);
    painter.drawArc(arcRect, 45 * 16, 270 * 16);

    const QPointF tip(arcRect.right() - 1.0, arcRect.center().y() - 2.0);
    QPolygonF arrow;
    arrow << tip << QPointF(tip.x() - 5.0, tip.y() - 3.0) << QPointF(tip.x() - 2.0, tip.y() + 4.0);
    painter.setPen(Qt::NoPen);
    painter.setBrush(accent);
    painter.drawPolygon(arrow);

    return pixmap;
}

QPixmap mouseMovePreviewIcon() {
    QPixmap pixmap(kThumbnailSize, kThumbnailSize);
    pixmap.fill(Qt::transparent);

    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing);

    const QRectF body = mouseBodyRect();
    const QColor accent(74, 144, 217);
    drawMouseBody(painter, body);

    painter.setPen(QPen(accent, 1.8, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
    const QPointF from(body.center().x() - 5.0, body.center().y() + 2.0);
    const QPointF to(body.center().x() + 7.0, body.center().y() - 6.0);
    painter.drawLine(from, to);
    painter.drawLine(to, QPointF(to.x() - 4.0, to.y()));
    painter.drawLine(to, QPointF(to.x(), to.y() + 4.0));

    return pixmap;
}

QPixmap loadBlockThumbnail(const Block& block, const QString& projectDirectory) {
    if (block.type() == BlockType::Click) {
        const auto* clickBlock = dynamic_cast<const ClickBlock*>(&block);
        if (clickBlock) {
            if (clickBlock->action == ClickAction::MoveOnly) {
                return mouseMovePreviewIcon();
            }
            return clickBlockPreviewIcon(clickBlock->button);
        }
        return {};
    }

    if (block.type() == BlockType::KeyPress) {
        const auto* keyBlock = dynamic_cast<const KeyPressBlock*>(&block);
        if (keyBlock) {
            return keyPressPreviewIcon(keyBlock->virtualKey, keyBlock->modifierActions, keyBlock->useMainKey);
        }
        return {};
    }

    if (block.type() == BlockType::Wait) {
        return waitBlockPreviewIcon();
    }

    if (block.type() == BlockType::Loop) {
        return loopBlockPreviewIcon();
    }

    if (block.type() != BlockType::ImageFind) {
        return {};
    }

    const auto* imageFind = dynamic_cast<const ImageFindBlock*>(&block);
    const std::string primaryTemplate = imageFind ? imageFind->primaryTemplatePath() : std::string{};
    if (!imageFind || primaryTemplate.empty()) {
        return {};
    }

    const QString relativePath = QString::fromStdString(primaryTemplate);
    const QFileInfo fileInfo(relativePath);
    const QString resolvedPath = fileInfo.isAbsolute()
                                     ? relativePath
                                     : QDir(projectDirectory).filePath(relativePath);

    QPixmap pixmap(resolvedPath);
    if (pixmap.isNull()) {
        return {};
    }
    return pixmap.scaled(kThumbnailSize,
                         kThumbnailSize,
                         Qt::KeepAspectRatio,
                         Qt::SmoothTransformation);
}

} // namespace

WorkflowEditorPanel::WorkflowEditorPanel(QWidget* parent)
    : QWidget(parent) {
    setupUi();
}

void WorkflowEditorPanel::setupUi() {
    auto* layout = new QVBoxLayout(this);

    m_titleLabel = new QLabel(tr("워크플로우"), this);
    m_titleLabel->setStyleSheet(QStringLiteral("font-weight: bold; font-size: 14px;"));

    m_blockList = new BlockListWidget(this);
    connect(m_blockList, &QTableWidget::cellDoubleClicked, this,
            [this](int row, int) {
                const int blockRow = m_blockList->blockRowForTableRow(row);
                if (blockRow >= 0) {
                    onBlockDoubleClicked(blockRow);
                    return;
                }
                const QString regionId = m_blockList->loopRegionIdForTableRow(row);
                if (!regionId.isEmpty()) {
                    editLoopRegion(regionId);
                }
            });
    connect(m_blockList, &BlockListWidget::blockRowsReordered, this,
            &WorkflowEditorPanel::onBlockRowsReordered);
    connect(m_blockList, &BlockListWidget::copyRequested, this, &WorkflowEditorPanel::onCopyBlocks);
    connect(m_blockList, &BlockListWidget::pasteRequested, this, &WorkflowEditorPanel::onPasteBlocks);
    connect(m_blockList, &BlockListWidget::deleteRequested, this, &WorkflowEditorPanel::onDeleteBlocks);
    connect(m_blockList, &BlockListWidget::undoRequested, this, &WorkflowEditorPanel::onUndo);
    connect(m_blockList, &BlockListWidget::redoRequested, this, &WorkflowEditorPanel::onRedo);

    auto* toolsPane = new QWidget(this);
    auto* toolsLayout = new QVBoxLayout(toolsPane);
    toolsLayout->setContentsMargins(0, 0, 0, 0);
    toolsLayout->setSpacing(6);

    auto* addGroup = new QGroupBox(tr("블록 추가"), toolsPane);
    auto* addRow = new QHBoxLayout(addGroup);
    addRow->setSpacing(6);

    const BlockType addTypes[] = {BlockType::ImageFind,
                                  BlockType::Click,
                                  BlockType::KeyPress,
                                  BlockType::Wait};
    for (const BlockType type : addTypes) {
        auto* button = new QPushButton(blockTypeDisplayName(type), addGroup);
        button->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
        button->setToolTip(tr("%1 블록을 워크플로우에 추가").arg(blockTypeDisplayName(type)));
        connect(button, &QPushButton::clicked, this, [this, type]() { addBlockOfType(type); });
        m_addTypeButtons.append(button);
        addRow->addWidget(button, 2);
    }

    m_removeAllWaitButton = new QPushButton(tr("전체 딜레이 삭제"), addGroup);
    m_removeAllWaitButton->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    m_removeAllWaitButton->setToolTip(tr("워크플로우에 있는 딜레이 블록을 모두 제거합니다. Ctrl+Z로 되돌릴 수 있습니다."));
    addRow->addWidget(m_removeAllWaitButton, 3);

    m_insertWaitBetweenButton = new QPushButton(tr("전체 딜레이 추가"), addGroup);
    m_insertWaitBetweenButton->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    m_insertWaitBetweenButton->setToolTip(
        tr("인접한 블록 사이마다 지정한 시간(ms)의 딜레이 블록을 삽입합니다. Ctrl+Z로 되돌릴 수 있습니다."));
    addRow->addWidget(m_insertWaitBetweenButton, 3);

    m_loopRegionsButton = new QPushButton(tr("구간 반복"), addGroup);
    m_loopRegionsButton->setCheckable(true);
    m_loopRegionsButton->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    m_loopRegionsButton->setToolTip(
        tr("클릭 후 워크플로우 목록에서 블록을 드래그하여 반복 구간을 선택합니다. "
           "우클릭하면 기존 구간 목록을 엽니다."));
    m_loopRegionsButton->setContextMenuPolicy(Qt::CustomContextMenu);
    addRow->addWidget(m_loopRegionsButton, 3);

    toolsLayout->addWidget(addGroup);

    auto* workflowPane = new QWidget(this);
    auto* workflowLayout = new QVBoxLayout(workflowPane);
    workflowLayout->setContentsMargins(0, 0, 0, 0);

    m_workflowSplitter = new QSplitter(Qt::Vertical, workflowPane);
    m_workflowSplitter->addWidget(m_blockList);
    m_workflowSplitter->addWidget(toolsPane);
    m_workflowSplitter->setStretchFactor(0, 1);
    m_workflowSplitter->setStretchFactor(1, 0);
    m_workflowSplitter->setCollapsible(0, false);
    m_workflowSplitter->setCollapsible(1, false);
    m_workflowSplitter->setSizes({480, 120});

    workflowLayout->addWidget(m_workflowSplitter, 1);

    layout->addWidget(m_titleLabel);
    layout->addWidget(workflowPane, 1);

    connect(m_removeAllWaitButton, &QPushButton::clicked, this, &WorkflowEditorPanel::onRemoveAllWaitBlocks);
    connect(m_insertWaitBetweenButton, &QPushButton::clicked, this, &WorkflowEditorPanel::onInsertWaitBetweenBlocks);
    connect(m_loopRegionsButton, &QPushButton::clicked, this, &WorkflowEditorPanel::onManageLoopRegions);
    connect(m_loopRegionsButton,
            &QPushButton::customContextMenuRequested,
            this,
            &WorkflowEditorPanel::onLoopRegionsButtonContextMenu);
    connect(m_blockList,
            &BlockListWidget::loopRegionRangePicked,
            this,
            &WorkflowEditorPanel::onLoopRegionRangePicked);
    connect(m_blockList,
            &BlockListWidget::loopRegionPickCancelled,
            this,
            &WorkflowEditorPanel::onLoopRegionPickCancelled);
    connect(m_blockList,
            &BlockListWidget::loopRegionEditRequested,
            this,
            &WorkflowEditorPanel::onLoopRegionEditRequested);
    connect(m_blockList,
            &BlockListWidget::loopRegionDeleteRequested,
            this,
            &WorkflowEditorPanel::onLoopRegionDeleteRequested);
}

QHeaderView* WorkflowEditorPanel::blockListHeader() const {
    return m_blockList ? m_blockList->horizontalHeader() : nullptr;
}

QSplitter* WorkflowEditorPanel::workflowSplitter() const {
    return m_workflowSplitter;
}

void WorkflowEditorPanel::clearBlockMatchResults() {
    if (m_feature) {
        m_featureRunFeedback.erase(m_feature->id());
    }
    clearCurrentRunFeedbackVectors();
    m_blockList->clearBlockMatchResults();
}

void WorkflowEditorPanel::clearCurrentRunFeedbackVectors() {
    m_rowMatchImages.clear();
    m_rowMatchConfidences.clear();
    m_rowMatchThresholds.clear();
    m_rowBlockDurations.clear();
    m_rowImageFindMatchDurations.clear();
}

void WorkflowEditorPanel::saveRunFeedbackForFeature(const std::string& featureId) {
    if (featureId.empty()) {
        return;
    }

    FeatureRunFeedback feedback;
    feedback.rowMatchImages = m_rowMatchImages;
    feedback.rowMatchConfidences = m_rowMatchConfidences;
    feedback.rowMatchThresholds = m_rowMatchThresholds;
    feedback.rowBlockDurations = m_rowBlockDurations;
    feedback.rowImageFindMatchDurations = m_rowImageFindMatchDurations;
    feedback.activeBlockIndex = m_activeBlockIndex;
    feedback.executionHighlight = m_executionHighlight;
    feedback.hasLoopTiming = m_hasLoopTiming;
    feedback.loopNumber = m_loopNumber;
    feedback.loopElapsedMs = m_loopElapsedMs;
    feedback.loopSuccess = m_loopSuccess;

    const int blockCount = m_blockList->blockCount();
    feedback.rowMatchLockedSuccess.resize(blockCount);
    for (int i = 0; i < blockCount; ++i) {
        feedback.rowMatchLockedSuccess[i] = m_blockList->isMatchSuccessLocked(i);
    }

    m_featureRunFeedback[featureId] = std::move(feedback);
}

void WorkflowEditorPanel::restoreRunFeedbackForFeature(const std::string& featureId) {
    const auto it = m_featureRunFeedback.find(featureId);
    if (it == m_featureRunFeedback.end()) {
        clearCurrentRunFeedbackVectors();
        m_activeBlockIndex = -1;
        m_executionHighlight = BlockListWidget::ExecutionHighlight::None;
        clearLoopTiming();
        return;
    }

    const FeatureRunFeedback& feedback = it->second;
    m_rowMatchImages = feedback.rowMatchImages;
    m_rowMatchConfidences = feedback.rowMatchConfidences;
    m_rowMatchThresholds = feedback.rowMatchThresholds;
    m_rowBlockDurations = feedback.rowBlockDurations;
    m_rowImageFindMatchDurations = feedback.rowImageFindMatchDurations;
    m_activeBlockIndex = feedback.activeBlockIndex;
    m_executionHighlight = feedback.executionHighlight;
    if (feedback.hasLoopTiming) {
        setLoopTiming(feedback.loopNumber, feedback.loopElapsedMs, feedback.loopSuccess);
    } else {
        clearLoopTiming();
    }
}

void WorkflowEditorPanel::clearExecutionHighlight() {
    m_activeBlockIndex = -1;
    m_executionHighlight = BlockListWidget::ExecutionHighlight::None;
    m_blockList->clearActiveRow();
}

void WorkflowEditorPanel::setActiveBlockIndex(int index, BlockListWidget::ExecutionHighlight highlight) {
    m_activeBlockIndex = index;
    m_executionHighlight = index >= 0 ? highlight : BlockListWidget::ExecutionHighlight::None;
    m_blockList->setActiveRow(index, m_executionHighlight);
}

void WorkflowEditorPanel::notifyImageFindRetry(int blockIndex) {
    if (m_blockList->isMatchSuccessLocked(blockIndex)) {
        return;
    }
    m_activeBlockIndex = blockIndex;
    m_executionHighlight = BlockListWidget::ExecutionHighlight::ImageFindMiss;
    m_blockList->notifyImageFindRetry(blockIndex);
}

bool WorkflowEditorPanel::isBlockMatchSuccessCommitted(int blockIndex) const {
    return m_blockList->isMatchSuccessLocked(blockIndex);
}

void WorkflowEditorPanel::setBlockMatchResult(int blockIndex,
                                              double matchThreshold,
                                              double confidence,
                                              const QPixmap& image,
                                              bool matched) {
    if (!matched && m_blockList->isMatchSuccessLocked(blockIndex)) {
        return;
    }

    if (matched) {
        m_activeBlockIndex = blockIndex;
        m_executionHighlight = BlockListWidget::ExecutionHighlight::Success;
    }

    if (blockIndex >= m_rowMatchConfidences.size()) {
        m_rowMatchConfidences.resize(blockIndex + 1, 0.0);
        m_rowMatchThresholds.resize(blockIndex + 1, 0.0);
        m_rowMatchImages.resize(blockIndex + 1);
    }
    m_rowMatchThresholds[blockIndex] = matchThreshold;
    m_rowMatchConfidences[blockIndex] = confidence;
    m_rowMatchImages[blockIndex] = image;
    m_blockList->setBlockMatchResult(blockIndex, matchThreshold, confidence, image, matched);
}

void WorkflowEditorPanel::markBlockMatchSuccess(int blockIndex) {
    if (blockIndex < 0 || !m_feature) {
        return;
    }

    const auto& blocks = m_feature->workflow().blocks();
    if (blockIndex >= static_cast<int>(blocks.size())
        || blocks[blockIndex]->type() != BlockType::ImageFind) {
        return;
    }

    double threshold = static_cast<const ImageFindBlock&>(*blocks[blockIndex]).threshold;
    if (blockIndex < static_cast<int>(m_rowMatchThresholds.size()) && m_rowMatchThresholds[blockIndex] > 0.0) {
        threshold = m_rowMatchThresholds[blockIndex];
    }

    const double confidence =
        blockIndex < static_cast<int>(m_rowMatchConfidences.size()) ? m_rowMatchConfidences[blockIndex] : 0.0;
    const QPixmap image =
        blockIndex < static_cast<int>(m_rowMatchImages.size()) ? m_rowMatchImages[blockIndex] : QPixmap();

    if (confidence > 0.0 || !image.isNull()) {
        setBlockMatchResult(blockIndex, threshold, confidence > 0.0 ? confidence : 1.0, image, true);
    }
    m_blockList->commitRowSuccess(blockIndex);
}

void WorkflowEditorPanel::setBlockDuration(int blockIndex, qint64 durationMs) {
    if (blockIndex >= m_rowBlockDurations.size()) {
        m_rowBlockDurations.resize(blockIndex + 1, -1);
    }
    m_rowBlockDurations[blockIndex] = durationMs;
    m_blockList->setBlockDuration(blockIndex, durationMs);
}

void WorkflowEditorPanel::setBlockImageFindMatchDuration(int blockIndex, qint64 matchDurationMs) {
    if (blockIndex >= m_rowImageFindMatchDurations.size()) {
        m_rowImageFindMatchDurations.resize(blockIndex + 1, -1);
    }
    m_rowImageFindMatchDurations[blockIndex] = matchDurationMs;
    m_blockList->setBlockImageFindMatchDuration(blockIndex, matchDurationMs);
}

void WorkflowEditorPanel::setFeature(Feature* feature) {
    if (m_feature && m_feature != feature) {
        saveRunFeedbackForFeature(m_feature->id());
        setLoopRegionPickMode(false);
    }

    m_feature = feature;
    clearWorkflowHistory();

    if (m_feature) {
        restoreRunFeedbackForFeature(m_feature->id());
    } else {
        clearBlockMatchResults();
        clearExecutionHighlight();
        setLoopRegionPickMode(false);
    }

    refresh();

    if (m_feature) {
        const auto it = m_featureRunFeedback.find(m_feature->id());
        if (it != m_featureRunFeedback.end()) {
            for (int i = 0; i < it->second.rowMatchLockedSuccess.size(); ++i) {
                if (it->second.rowMatchLockedSuccess[i]) {
                    markBlockMatchSuccess(i);
                }
            }
        }
    }
}

void WorkflowEditorPanel::updateTitleText() {
    if (!m_titleLabel) {
        return;
    }
    if (!m_feature) {
        m_titleLabel->setText(tr("워크플로우"));
        return;
    }

    QString title = tr("워크플로우: %1").arg(QString::fromStdString(m_feature->name()));
    if (m_hasLoopTiming) {
        title += tr("  ·  루프 %1: %2 ms (%3)")
                      .arg(m_loopNumber)
                      .arg(m_loopElapsedMs)
                      .arg(m_loopSuccess ? tr("성공") : tr("실패"));
    }
    m_titleLabel->setText(title);
}

void WorkflowEditorPanel::setLoopTiming(int loopNumber, qint64 elapsedMs, bool success) {
    m_hasLoopTiming = true;
    m_loopNumber = loopNumber;
    m_loopElapsedMs = elapsedMs;
    m_loopSuccess = success;
    updateTitleText();
}

void WorkflowEditorPanel::clearLoopTiming() {
    m_hasLoopTiming = false;
    m_loopNumber = 0;
    m_loopElapsedMs = 0;
    m_loopSuccess = true;
    updateTitleText();
}

void WorkflowEditorPanel::setProjectDirectory(const QString& directory) {
    m_projectDirectory = directory;
    if (m_feature) {
        refresh();
    }
}

void WorkflowEditorPanel::refresh() {
    m_blockList->setBlockCount(0);
    m_blockList->clearLoopRegionVisuals();
    if (!m_feature) {
        m_blockList->setLoopRegions({});
        clearLoopTiming();
        setEnabled(false);
        return;
    }

    setEnabled(true);
    updateTitleText();

    const auto& blocks = m_feature->workflow().blocks();
    m_blockList->setBlockCount(static_cast<int>(blocks.size()));

    m_blockList->setLoopRegions(m_feature->workflow().loopRegions());
    for (int i = 0; i < static_cast<int>(blocks.size()); ++i) {
        const Block& block = *blocks[i];
        m_blockList->setBlockInfo(i,
                                  blockTypeWorkflowActionName(block.type()),
                                  QString::fromStdString(block.summary()),
                                  loadBlockThumbnail(block, m_projectDirectory));

        if (block.type() != BlockType::ImageFind) {
            continue;
        }

        const double threshold = static_cast<const ImageFindBlock&>(block).threshold;
        if (i < static_cast<int>(m_rowMatchThresholds.size())) {
            m_rowMatchThresholds[i] = threshold;
        }

        const bool hasRunMatch = i < m_rowMatchConfidences.size() && m_rowMatchConfidences[i] > 0.0 &&
                                 i < m_rowMatchImages.size() && !m_rowMatchImages[i].isNull();
        if (hasRunMatch) {
            m_blockList->setBlockMatchResult(i, threshold, m_rowMatchConfidences[i], m_rowMatchImages[i], true);
        } else {
            m_blockList->setBlockMatchBaseline(i, threshold);
        }
    }

    for (int i = 0; i < static_cast<int>(blocks.size()); ++i) {
        if (i < m_rowBlockDurations.size() && m_rowBlockDurations[i] >= 0) {
            m_blockList->setBlockDuration(i, m_rowBlockDurations[i]);
        }
        if (i < m_rowImageFindMatchDurations.size() && m_rowImageFindMatchDurations[i] >= 0) {
            m_blockList->setBlockImageFindMatchDuration(i, m_rowImageFindMatchDurations[i]);
        }
    }
    if (m_activeBlockIndex >= 0 && m_activeBlockIndex < static_cast<int>(blocks.size())) {
        m_blockList->setActiveRow(m_activeBlockIndex, m_executionHighlight);
    }
    updateWorkflowToolButtonStates();
}

void WorkflowEditorPanel::updateWorkflowToolButtonStates() {
    bool hasWait = false;
    int blockCount = 0;
    if (m_feature) {
        const auto& blocks = m_feature->workflow().blocks();
        blockCount = static_cast<int>(blocks.size());
        for (const auto& block : blocks) {
            if (block->type() == BlockType::Wait) {
                hasWait = true;
                break;
            }
        }
    }

    if (m_removeAllWaitButton) {
        m_removeAllWaitButton->setEnabled(m_editingEnabled && hasWait);
    }
    if (m_insertWaitBetweenButton) {
        m_insertWaitBetweenButton->setEnabled(m_editingEnabled && blockCount >= 2);
    }
    if (m_loopRegionsButton) {
        m_loopRegionsButton->setEnabled(m_editingEnabled && blockCount > 0);
    }
}

void WorkflowEditorPanel::setEditingEnabled(bool enabled) {
    m_editingEnabled = enabled;
    for (QPushButton* button : m_addTypeButtons) {
        button->setEnabled(enabled);
    }
    updateWorkflowToolButtonStates();
    m_blockList->setReorderEnabled(enabled);
    if (!enabled) {
        setLoopRegionPickMode(false);
    }
}

void WorkflowEditorPanel::addBlockOfType(BlockType type) {
    if (!m_feature) {
        return;
    }

    auto draft = BlockFactory::create(type);
    BlockEditorDialog dialog(draft.get(), m_projectDirectory, this);
    if (dialog.exec() != QDialog::Accepted) {
        return;
    }

    pushUndoSnapshot();
    m_feature->workflow().addBlock(dialog.takeBlock());
    refresh();
    m_blockList->selectBlockRow(static_cast<int>(m_feature->workflow().blocks().size()) - 1);
    emit workflowModified();
}

void WorkflowEditorPanel::onEditBlock() {
    const QList<int> rows = selectedBlockRows();
    if (rows.size() == 1) {
        editBlockAt(rows.first());
    }
}

QList<int> WorkflowEditorPanel::selectedBlockRows() const {
    QSet<int> rowSet;
    for (const QTableWidgetItem* item : m_blockList->selectedItems()) {
        const int blockRow = m_blockList->blockRowForTableRow(item->row());
        if (blockRow >= 0) {
            rowSet.insert(blockRow);
        }
    }
    QList<int> rows = rowSet.values();
    std::sort(rows.begin(), rows.end());
    return rows;
}

void WorkflowEditorPanel::selectBlockRows(const QList<int>& rows) {
    if (rows.isEmpty() || !m_blockList->selectionModel()) {
        return;
    }

    QItemSelection selection;
    const int lastColumn = m_blockList->columnCount() - 1;
    for (int blockRow : rows) {
        const int tableRow = m_blockList->tableRowForBlockRow(blockRow);
        if (tableRow < 0) {
            continue;
        }
        selection.select(m_blockList->model()->index(tableRow, 0),
                         m_blockList->model()->index(tableRow, lastColumn));
    }
    m_blockList->selectionModel()->select(selection, QItemSelectionModel::ClearAndSelect);
    if (!rows.isEmpty()) {
        const int tableRow = m_blockList->tableRowForBlockRow(rows.last());
        if (tableRow >= 0) {
            m_blockList->setCurrentCell(tableRow, 0);
        }
    }
}

void WorkflowEditorPanel::onCopyBlocks() {
    if (!m_feature) {
        return;
    }

    const QList<int> rows = selectedBlockRows();
    if (rows.isEmpty()) {
        return;
    }

    m_clipboardBlocks.clear();
    const auto& blocks = m_feature->workflow().blocks();
    for (int row : rows) {
        if (row >= 0 && row < static_cast<int>(blocks.size())) {
            m_clipboardBlocks.push_back(blocks[row]->clone());
        }
    }
}

void WorkflowEditorPanel::onPasteBlocks() {
    if (!m_feature || m_clipboardBlocks.empty()) {
        return;
    }

    const QList<int> rows = selectedBlockRows();
    int insertAt = rows.isEmpty() ? static_cast<int>(m_feature->workflow().blocks().size())
                                  : rows.last() + 1;

    pushUndoSnapshot();
    QList<int> pastedRows;
    pastedRows.reserve(static_cast<int>(m_clipboardBlocks.size()));
    for (const auto& block : m_clipboardBlocks) {
        m_feature->workflow().insertBlock(insertAt, block->clone());
        pastedRows.append(insertAt);
        ++insertAt;
    }

    refresh();
    selectBlockRows(pastedRows);
    emit workflowModified();
}

void WorkflowEditorPanel::removeSelectedBlocks() {
    if (!m_feature) {
        return;
    }

    const QList<int> rows = selectedBlockRows();
    if (rows.isEmpty()) {
        return;
    }

    pushUndoSnapshot();

    for (int i = rows.size() - 1; i >= 0; --i) {
        m_feature->workflow().removeBlock(rows[i]);
    }

    clearBlockMatchResults();

    const int blockCount = static_cast<int>(m_feature->workflow().blocks().size());
    refresh();

    if (blockCount > 0) {
        const int anchorRow = qMin(rows.first(), blockCount - 1);
        m_blockList->selectBlockRow(anchorRow);
    }

    emit workflowModified();
}

void WorkflowEditorPanel::onDeleteBlocks() {
    removeSelectedBlocks();
}

void WorkflowEditorPanel::onRemoveBlock() {
    removeSelectedBlocks();
}

void WorkflowEditorPanel::onRemoveAllWaitBlocks() {
    if (!m_feature) {
        return;
    }

    const auto& blocks = m_feature->workflow().blocks();
    QList<int> waitRows;
    for (int i = 0; i < static_cast<int>(blocks.size()); ++i) {
        if (blocks[i]->type() == BlockType::Wait) {
            waitRows.append(i);
        }
    }
    if (waitRows.isEmpty()) {
        return;
    }

    pushUndoSnapshot();
    for (int i = waitRows.size() - 1; i >= 0; --i) {
        m_feature->workflow().removeBlock(waitRows[i]);
    }

    clearBlockMatchResults();
    clearExecutionHighlight();

    const int blockCount = static_cast<int>(m_feature->workflow().blocks().size());
    refresh();

    if (blockCount > 0) {
        const int anchorRow = qMin(waitRows.first(), blockCount - 1);
        m_blockList->selectBlockRow(anchorRow);
    }

    emit workflowModified();
}

void WorkflowEditorPanel::onManageLoopRegions() {
    if (!m_feature || !m_editingEnabled) {
        return;
    }

    const int blockCount = static_cast<int>(m_feature->workflow().blocks().size());
    if (blockCount <= 0) {
        return;
    }

    setLoopRegionPickMode(!m_loopRegionPickActive);
}

void WorkflowEditorPanel::setLoopRegionPickMode(bool active) {
    if (m_loopRegionPickActive == active) {
        return;
    }
    m_loopRegionPickActive = active;
    m_blockList->setLoopRegionPickMode(active);
    if (m_loopRegionsButton) {
        m_loopRegionsButton->setChecked(active);
        m_loopRegionsButton->setText(active ? tr("구간 드래그…") : tr("구간 반복"));
    }
}

void WorkflowEditorPanel::onLoopRegionPickCancelled() {
    setLoopRegionPickMode(false);
}

void WorkflowEditorPanel::editLoopRegion(const QString& regionId) {
    if (!m_feature || !m_editingEnabled || regionId.isEmpty()) {
        return;
    }

    std::vector<WorkflowLoopRegion> regions = m_feature->workflow().loopRegions();
    const auto it = std::find_if(regions.begin(), regions.end(), [&](const WorkflowLoopRegion& region) {
        return QString::fromStdString(region.id) == regionId;
    });
    if (it == regions.end()) {
        return;
    }

    WorkflowLoopRegionEditDialog dialog(static_cast<int>(m_feature->workflow().blocks().size()),
                                        &(*it),
                                        it->startIndex + 1,
                                        it->endIndex + 1,
                                        false,
                                        this);
    if (dialog.exec() != QDialog::Accepted) {
        return;
    }
    const WorkflowLoopRegion edited = dialog.resultRegion();
    if (loopRegionOverlaps(regions, edited.startIndex, edited.endIndex, edited.id)) {
        QMessageBox::warning(this,
                             tr("구간 반복"),
                             tr("편집한 구간이 다른 반복 구간과 겹칩니다."));
        return;
    }
    pushUndoSnapshot();
    *it = edited;
    m_feature->workflow().setLoopRegions(std::move(regions));
    refresh();
    emit workflowModified();
}

void WorkflowEditorPanel::deleteLoopRegion(const QString& regionId) {
    if (!m_feature || !m_editingEnabled || regionId.isEmpty()) {
        return;
    }

    std::vector<WorkflowLoopRegion> regions = m_feature->workflow().loopRegions();
    const auto it = std::find_if(regions.begin(), regions.end(), [&](const WorkflowLoopRegion& region) {
        return QString::fromStdString(region.id) == regionId;
    });
    if (it == regions.end()) {
        return;
    }

    pushUndoSnapshot();
    regions.erase(it);
    m_feature->workflow().setLoopRegions(std::move(regions));
    refresh();
    emit workflowModified();
}

void WorkflowEditorPanel::deleteBlockAt(int row) {
    if (!m_feature || !m_editingEnabled) {
        return;
    }
    if (row < 0 || row >= static_cast<int>(m_feature->workflow().blocks().size())) {
        return;
    }

    pushUndoSnapshot();
    m_feature->workflow().removeBlock(row);
    clearBlockMatchResults();
    refresh();

    const int blockCount = static_cast<int>(m_feature->workflow().blocks().size());
    if (blockCount > 0) {
        m_blockList->selectBlockRow(qMin(row, blockCount - 1));
    }
    emit workflowModified();
}

void WorkflowEditorPanel::onLoopRegionEditRequested(const QString& regionId) {
    editLoopRegion(regionId);
}

void WorkflowEditorPanel::onLoopRegionDeleteRequested(const QString& regionId) {
    deleteLoopRegion(regionId);
}

void WorkflowEditorPanel::onLoopRegionsButtonContextMenu(const QPoint& pos) {
    QMenu menu(this);
    menu.addAction(tr("구간 반복 목록…"), this, &WorkflowEditorPanel::openLoopRegionsListDialog);
    menu.exec(m_loopRegionsButton->mapToGlobal(pos));
}

void WorkflowEditorPanel::openLoopRegionsListDialog() {
    if (!m_feature || !m_editingEnabled) {
        return;
    }

    const int blockCount = static_cast<int>(m_feature->workflow().blocks().size());
    if (blockCount <= 0) {
        return;
    }

    pushUndoSnapshot();

    const QList<int> rows = selectedBlockRows();
    int defaultStart = 1;
    int defaultEnd = blockCount;
    if (!rows.isEmpty()) {
        defaultStart = rows.first() + 1;
        defaultEnd = rows.last() + 1;
    }

    WorkflowLoopRegionsDialog dialog(&m_feature->workflow(), defaultStart, defaultEnd, this);
    dialog.exec();
    refresh();
    emit workflowModified();
}

void WorkflowEditorPanel::onLoopRegionRangePicked(int startRow, int endRow) {
    if (!m_feature || !m_editingEnabled) {
        return;
    }

    const int startIndex = qMin(startRow, endRow);
    const int endIndex = qMax(startRow, endRow);
    if (loopRegionOverlaps(m_feature->workflow().loopRegions(), startIndex, endIndex)) {
        QMessageBox::warning(this,
                             tr("구간 반복"),
                             tr("선택한 구간이 기존 반복 구간과 겹칩니다. 다른 범위를 선택하세요."));
        return;
    }

    WorkflowLoopRegionEditDialog dialog(static_cast<int>(m_feature->workflow().blocks().size()),
                                        nullptr,
                                        startIndex + 1,
                                        endIndex + 1,
                                        true,
                                        this);
    if (dialog.exec() != QDialog::Accepted) {
        return;
    }

    pushUndoSnapshot();
    std::vector<WorkflowLoopRegion> regions = m_feature->workflow().loopRegions();
    regions.push_back(dialog.resultRegion());
    m_feature->workflow().setLoopRegions(std::move(regions));
    refresh();
    emit workflowModified();
    setLoopRegionPickMode(false);
}

void WorkflowEditorPanel::onInsertWaitBetweenBlocks() {
    if (!m_feature) {
        return;
    }

    const int blockCount = static_cast<int>(m_feature->workflow().blocks().size());
    if (blockCount < 2) {
        return;
    }

    bool accepted = false;
    const int waitMs = snapWaitDelayMs(QInputDialog::getInt(this,
                                            tr("전체 딜레이 추가"),
                                            tr("삽입할 딜레이 시간 (ms):"),
                                            500,
                                            0,
                                            600000,
                                            kWaitDelayStepMs,
                                            &accepted));
    if (!accepted) {
        return;
    }

    pushUndoSnapshot();
    for (int i = blockCount - 2; i >= 0; --i) {
        auto waitBlock = BlockFactory::create(BlockType::Wait);
        static_cast<WaitBlock*>(waitBlock.get())->ms = waitMs;
        m_feature->workflow().insertBlock(i + 1, std::move(waitBlock));
    }

    clearBlockMatchResults();
    clearExecutionHighlight();
    refresh();
    emit workflowModified();
}

void WorkflowEditorPanel::onMoveUp() {
    if (!m_feature) {
        return;
    }
    const int row = m_blockList->currentRow();
    if (row <= 0) {
        return;
    }
    pushUndoSnapshot();
    m_feature->workflow().moveBlockUp(row);
    refresh();
    m_blockList->selectBlockRow(row - 1);
    emit workflowModified();
}

void WorkflowEditorPanel::onMoveDown() {
    if (!m_feature) {
        return;
    }
    const int row = m_blockList->currentRow();
    if (row < 0 || row + 1 >= static_cast<int>(m_feature->workflow().blocks().size())) {
        return;
    }
    pushUndoSnapshot();
    m_feature->workflow().moveBlockDown(row);
    refresh();
    m_blockList->selectBlockRow(row + 1);
    emit workflowModified();
}

void WorkflowEditorPanel::onBlockRowsReordered(int fromRow, int toRow) {
    if (!m_feature) {
        return;
    }
    if (fromRow != toRow) {
        pushUndoSnapshot();
        m_feature->workflow().moveBlock(fromRow, toRow);
        emit workflowModified();
    }
    refresh();
    if (fromRow != toRow) {
        m_blockList->selectBlockRow(toRow);
    } else if (fromRow >= 0) {
        m_blockList->selectBlockRow(fromRow);
    }
}

void WorkflowEditorPanel::onBlockDoubleClicked(int row) {
    editBlockAt(row);
}

bool WorkflowEditorPanel::editBlockAt(int row) {
    if (!m_feature || row < 0 || row >= static_cast<int>(m_feature->workflow().blocks().size())) {
        return false;
    }

    Block* block = m_feature->workflow().blocks()[row].get();
    BlockEditorDialog dialog(block, m_projectDirectory, this);
    if (dialog.exec() != QDialog::Accepted) {
        return false;
    }

    pushUndoSnapshot();
    m_feature->workflow().replaceBlock(row, dialog.takeBlock());
    refresh();
    m_blockList->selectBlockRow(row);
    emit workflowModified();
    return true;
}

void WorkflowEditorPanel::clearWorkflowHistory() {
    m_undoStack.clear();
    m_redoStack.clear();
}

void WorkflowEditorPanel::pushUndoSnapshot() {
    if (!m_feature) {
        return;
    }

    m_undoStack.push_back(m_feature->workflow().clone());
    if (static_cast<int>(m_undoStack.size()) > kMaxUndoDepth) {
        m_undoStack.erase(m_undoStack.begin());
    }
    m_redoStack.clear();
}

void WorkflowEditorPanel::restoreFromSnapshot(const Workflow& snapshot) {
    if (!m_feature) {
        return;
    }

    m_feature->workflow().assignFrom(snapshot);
    clearBlockMatchResults();
    clearExecutionHighlight();
    refresh();
    emit workflowModified();
}

void WorkflowEditorPanel::onUndo() {
    if (!m_feature || m_undoStack.empty()) {
        return;
    }

    m_redoStack.push_back(m_feature->workflow().clone());
    if (static_cast<int>(m_redoStack.size()) > kMaxUndoDepth) {
        m_redoStack.erase(m_redoStack.begin());
    }

    std::unique_ptr<Workflow> snapshot = std::move(m_undoStack.back());
    m_undoStack.pop_back();
    restoreFromSnapshot(*snapshot);
}

void WorkflowEditorPanel::onRedo() {
    if (!m_feature || m_redoStack.empty()) {
        return;
    }

    m_undoStack.push_back(m_feature->workflow().clone());
    if (static_cast<int>(m_undoStack.size()) > kMaxUndoDepth) {
        m_undoStack.erase(m_undoStack.begin());
    }

    std::unique_ptr<Workflow> snapshot = std::move(m_redoStack.back());
    m_redoStack.pop_back();
    restoreFromSnapshot(*snapshot);
}
