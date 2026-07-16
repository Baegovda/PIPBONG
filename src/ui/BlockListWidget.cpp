#include "ui/BlockListWidget.h"

#include "ui/UiHoverFeedback.h"
#include "ui/UiResizeHandle.h"
#include "ui/widgets/ListColumnHeaderWidget.h"
#include "ui/widgets/ListDragAutoScroll.h"

#include "core/workflow/Block.h"
#include "core/workflow/LoopExitCondition.h"
#include "ui/UiStrings.h"
#include "ui/widgets/DragAdjustSpinMouse.h"
#include "ui/widgets/ListDragVisuals.h"

#include <QApplication>
#include <QCursor>
#include <QDrag>
#include <QMimeData>
#include <QSettings>
#include <QStyle>
#include <QWheelEvent>
#include <QDragLeaveEvent>
#include <QDragMoveEvent>
#include <QDropEvent>
#include <QEvent>
#include <QHeaderView>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QIcon>
#include <QKeyEvent>
#include <cmath>
#include <functional>
#include <QMouseEvent>
#include <QObject>
#include <QPushButton>
#include <QLinearGradient>
#include <QPainter>
#include <QResizeEvent>
#include <QShowEvent>
#include <QScrollBar>
#include <QSignalBlocker>
#include <QStyledItemDelegate>
#include <QStyleOptionButton>
#include <QStyleOptionViewItem>
#include <QTimer>
#include <QVariantAnimation>
#include <QEasingCurve>

#include <climits>
#include <cmath>

namespace {

constexpr int kThumbnailSize = 32;
constexpr int kLoopRegionHeaderHeight = 28;
constexpr int kColIndex = 0;
constexpr int kColPreview = 1;
constexpr int kColAction = 2;
constexpr int kColSummary = 3;
constexpr int kColDuration = 4;
constexpr int kColMatchDuration = 5;
constexpr int kColAttempts = 6;
constexpr int kColReturnPrev = 7;
constexpr int kColRetryAfterNext = 8;
constexpr int kColScore = 9;
constexpr int kColRoiCorrection = 10;
constexpr int kColMatch = 11;
constexpr int kColumnCount = 12;
constexpr int kMatchColumnWidth = kThumbnailSize + 6;
constexpr int kMinSummaryWidth = 48;
constexpr int kMaxSummaryColumnWidth = 2000;
constexpr int kMinMetricColumnWidth = 36;
constexpr int kMaxMetricColumnWidth = 220;
constexpr int kMinIndexColumnWidth = 20;
constexpr int kMaxIndexColumnWidth = 48;
constexpr int kMinPreviewColumnWidth = 28;
constexpr int kMaxPreviewColumnWidth = 80;
constexpr int kMinActionColumnWidth = 48;
constexpr int kMaxActionColumnWidth = 160;
constexpr int kMinMatchColumnWidth = 28;
constexpr int kMaxMatchColumnWidth = 80;

int blockListColumnWidthFromLayout(const BlockListColumnLayout& layout, int column) {
    switch (column) {
    case kColIndex:
        return layout.indexWidth;
    case kColPreview:
        return layout.previewWidth;
    case kColAction:
        return layout.actionWidth;
    case kColSummary:
        return layout.summaryWidth;
    case kColDuration:
        return layout.durationWidth;
    case kColMatchDuration:
        return layout.matchDurationWidth;
    case kColAttempts:
        return layout.attemptsWidth;
    case kColReturnPrev:
        return layout.returnPrevWidth;
    case kColRetryAfterNext:
        return layout.retryWidth;
    case kColScore:
        return layout.scoreWidth;
    case kColRoiCorrection:
        return layout.roiCorrectionWidth;
    case kColMatch:
        return layout.matchWidth;
    default:
        return 0;
    }
}

void blockListSetLayoutColumnWidth(BlockListColumnLayout& layout, int column, int width) {
    switch (column) {
    case kColIndex:
        layout.indexWidth = width;
        break;
    case kColPreview:
        layout.previewWidth = width;
        break;
    case kColAction:
        layout.actionWidth = width;
        break;
    case kColSummary:
        layout.summaryWidth = width;
        break;
    case kColDuration:
        layout.durationWidth = width;
        break;
    case kColMatchDuration:
        layout.matchDurationWidth = width;
        break;
    case kColAttempts:
        layout.attemptsWidth = width;
        break;
    case kColReturnPrev:
        layout.returnPrevWidth = width;
        break;
    case kColRetryAfterNext:
        layout.retryWidth = width;
        break;
    case kColScore:
        layout.scoreWidth = width;
        break;
    case kColRoiCorrection:
        layout.roiCorrectionWidth = width;
        break;
    case kColMatch:
        layout.matchWidth = width;
        break;
    default:
        break;
    }
}

void blockListColumnWidthBounds(int column, int& minOut, int& maxOut) {
    switch (column) {
    case kColIndex:
        minOut = kMinIndexColumnWidth;
        maxOut = kMaxIndexColumnWidth;
        break;
    case kColPreview:
        minOut = kMinPreviewColumnWidth;
        maxOut = kMaxPreviewColumnWidth;
        break;
    case kColAction:
        minOut = kMinActionColumnWidth;
        maxOut = kMaxActionColumnWidth;
        break;
    case kColSummary:
        minOut = kMinSummaryWidth;
        maxOut = kMaxSummaryColumnWidth;
        break;
    case kColMatch:
        minOut = kMinMatchColumnWidth;
        maxOut = kMaxMatchColumnWidth;
        break;
    default:
        minOut = kMinMetricColumnWidth;
        maxOut = kMaxMetricColumnWidth;
        break;
    }
}

int blockListPreviousVisibleColumn(const QTableWidget* table, int column) {
    for (int col = column - 1; col >= 0; --col) {
        if (!table->isColumnHidden(col)) {
            return col;
        }
    }
    return -1;
}

struct BlockListColumnRects {
    QRect index;
    QRect preview;
    QRect action;
    QRect summary;
    QRect duration;
    QRect matchDuration;
    QRect attempts;
    QRect returnPrev;
    QRect retry;
    QRect score;
    QRect roiCorrection;
    QRect match;
};

struct BlockListColumnEdges {
    BlockListColumnRects cols;
    int previewDividerX = 0;
    int actionDividerX = 0;
    int summaryDividerX = 0;
    int durationDividerX = 0;
    int matchDurationDividerX = 0;
    int attemptsDividerX = 0;
    int returnPrevDividerX = 0;
    int retryDividerX = 0;
    int scoreDividerX = 0;
    int roiCorrectionDividerX = 0;
    int matchDividerX = 0;
};

BlockListColumnEdges blockListColumnEdgesFromTable(const BlockListWidget* table, const QRect& itemRect) {
    BlockListColumnEdges edges;
    const int top = itemRect.top();
    const int height = itemRect.height();
    const int inset = table->frameWidth();
    const int left = itemRect.left();

    const auto columnRect = [&](int col) -> QRect {
        if (table->isColumnHidden(col)) {
            return {};
        }
        const int x = left + inset + table->columnViewportPosition(col);
        return QRect(x, top, table->columnWidth(col), height);
    };

    edges.cols.index = columnRect(kColIndex);
    edges.cols.preview = columnRect(kColPreview);
    edges.previewDividerX = edges.cols.preview.isValid() ? edges.cols.preview.left() : 0;
    edges.cols.action = columnRect(kColAction);
    edges.actionDividerX = edges.cols.action.isValid() ? edges.cols.action.left() : 0;
    edges.cols.summary = columnRect(kColSummary);
    edges.summaryDividerX = edges.cols.summary.isValid() ? edges.cols.summary.left() : 0;
    edges.cols.duration = columnRect(kColDuration);
    edges.durationDividerX = edges.cols.duration.isValid() ? edges.cols.duration.left() : 0;
    edges.cols.matchDuration = columnRect(kColMatchDuration);
    edges.matchDurationDividerX =
        edges.cols.matchDuration.isValid() ? edges.cols.matchDuration.left() : 0;
    edges.cols.attempts = columnRect(kColAttempts);
    edges.attemptsDividerX = edges.cols.attempts.isValid() ? edges.cols.attempts.left() : 0;
    edges.cols.returnPrev = columnRect(kColReturnPrev);
    edges.returnPrevDividerX = edges.cols.returnPrev.isValid() ? edges.cols.returnPrev.left() : 0;
    edges.cols.retry = columnRect(kColRetryAfterNext);
    edges.retryDividerX = edges.cols.retry.isValid() ? edges.cols.retry.left() : 0;
    edges.cols.score = columnRect(kColScore);
    edges.scoreDividerX = edges.cols.score.isValid() ? edges.cols.score.left() : 0;
    if (!table->isColumnHidden(kColRoiCorrection)) {
        edges.cols.roiCorrection = columnRect(kColRoiCorrection);
        edges.roiCorrectionDividerX =
            edges.cols.roiCorrection.isValid() ? edges.cols.roiCorrection.left() : 0;
    }
    edges.cols.match = columnRect(kColMatch);
    edges.matchDividerX = edges.cols.match.isValid() ? edges.cols.match.left() : 0;
    return edges;
}

int blockListDividerHandleAt(const QPoint& pos, const BlockListColumnEdges& edges, bool roiVisible) {
    struct Candidate {
        int x = 0;
        int rightColumn = 0;
    };
    QList<Candidate> candidates = {{edges.previewDividerX, kColPreview},
                                   {edges.actionDividerX, kColAction},
                                   {edges.summaryDividerX, kColSummary},
                                   {edges.durationDividerX, kColDuration},
                                   {edges.matchDurationDividerX, kColMatchDuration},
                                   {edges.attemptsDividerX, kColAttempts},
                                   {edges.returnPrevDividerX, kColReturnPrev},
                                   {edges.retryDividerX, kColRetryAfterNext},
                                   {edges.scoreDividerX, kColScore},
                                   {edges.matchDividerX, kColMatch}};
    if (roiVisible) {
        candidates.append({edges.roiCorrectionDividerX, kColRoiCorrection});
    }

    int bestColumn = 0;
    int bestDistance = INT_MAX;
    for (const Candidate& candidate : candidates) {
        if (!UiResizeHandle::isWithinHorizontalGrab(pos.x(), candidate.x)) {
            continue;
        }
        const int distance = qAbs(pos.x() - candidate.x);
        if (distance < bestDistance) {
            bestDistance = distance;
            bestColumn = candidate.rightColumn;
        }
    }
    return bestColumn;
}

QList<int> blockListDividerXs(const BlockListColumnEdges& edges, bool roiVisible) {
    QList<int> xs = {edges.previewDividerX,
                     edges.actionDividerX,
                     edges.summaryDividerX,
                     edges.durationDividerX,
                     edges.matchDurationDividerX,
                     edges.attemptsDividerX,
                     edges.returnPrevDividerX,
                     edges.retryDividerX,
                     edges.scoreDividerX,
                     edges.matchDividerX};
    if (roiVisible) {
        xs.append(edges.roiCorrectionDividerX);
    }
    return xs;
}

void initBlockListColumnHeader(BlockListWidget* table) {
    table->setColumnCount(kColumnCount);
    table->setHorizontalHeaderLabels({QStringLiteral("#"),
                                    QStringLiteral("◻"),
                                    BlockListWidget::tr("동작"),
                                    QStringLiteral("요약"),
                                    BlockListWidget::tr("동작 시간"),
                                    BlockListWidget::tr("매칭 시간"),
                                    BlockListWidget::tr("시도 횟수"),
                                    BlockListWidget::tr("이전 복귀"),
                                    BlockListWidget::tr("재시도"),
                                    BlockListWidget::tr("기준/감지"),
                                    BlockListWidget::tr("ROI 보정"),
                                    BlockListWidget::tr("매칭")});

    if (QTableWidgetItem* previewHeader = table->horizontalHeaderItem(kColPreview)) {
        previewHeader->setToolTip(
            BlockListWidget::tr("블록 아이콘·썸네일 (클릭하여 편집)"));
    }

    if (QHeaderView* header = table->horizontalHeader()) {
        header->setDefaultAlignment(Qt::AlignCenter);
        header->setSectionsClickable(false);
        header->hide();
        header->setFixedHeight(0);
    }
}

class WorkflowChipHeaderRowWidget : public QWidget {
public:
    WorkflowChipHeaderRowWidget(const QColor& accent,
                                const QString& badgeText,
                                const QString& title,
                                const QString& conditionText,
                                QWidget* parent = nullptr)
        : QWidget(parent)
        , m_accent(accent) {
        auto* outer = new QHBoxLayout(this);
        outer->setContentsMargins(2, 3, 6, 3);
        outer->setSpacing(8);

        m_leftBar = new QWidget(this);
        m_leftBar->setFixedWidth(4);

        m_chip = new QFrame(this);
        auto* chipLayout = new QHBoxLayout(m_chip);
        chipLayout->setContentsMargins(10, 3, 10, 3);
        chipLayout->setSpacing(6);

        m_badge = new QLabel(badgeText, m_chip);
        m_badge->setFixedSize(18, 18);
        m_badge->setAlignment(Qt::AlignCenter);

        m_titleLabel = new QLabel(title, m_chip);
        QFont titleFont = m_titleLabel->font();
        titleFont.setBold(true);
        m_titleLabel->setFont(titleFont);

        m_conditionLabel = new QLabel(
            conditionText.isEmpty() ? QString() : QStringLiteral(" · ") + conditionText, m_chip);

        chipLayout->addWidget(m_badge);
        chipLayout->addWidget(m_titleLabel);
        chipLayout->addWidget(m_conditionLabel);

        m_editButton = new QPushButton(tr("편집"), this);
        m_deleteButton = new QPushButton(tr("삭제"), this);
        for (QPushButton* button : {m_editButton, m_deleteButton}) {
            button->setFixedHeight(20);
            button->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
        }

        outer->addWidget(m_leftBar, 0, Qt::AlignVCenter);
        outer->addWidget(m_chip, 0, Qt::AlignVCenter);
        outer->addWidget(m_editButton, 0, Qt::AlignVCenter);
        outer->addWidget(m_deleteButton, 0, Qt::AlignVCenter);
        outer->addStretch(1);

        applyChrome();
    }

    QPushButton* editButton() const { return m_editButton; }
    QPushButton* deleteButton() const { return m_deleteButton; }

protected:
    void changeEvent(QEvent* event) override {
        QWidget::changeEvent(event);
        if (event->type() == QEvent::PaletteChange || event->type() == QEvent::ApplicationPaletteChange) {
            applyChrome();
        }
    }

private:
    void applyChrome() {
        const QPalette pal = palette();
        m_leftBar->setStyleSheet(
            QStringLiteral("background:%1; border-radius:2px;").arg(m_accent.name()));

        QColor chipBorder = m_accent;
        chipBorder.setAlpha(160);
        m_chip->setStyleSheet(
            QStringLiteral("QFrame { background:%1; border:1px solid %2; border-radius:6px; }")
                .arg(pal.color(QPalette::Button).name(), chipBorder.name()));

        QFont badgeFont = m_badge->font();
        badgeFont.setPointSizeF(badgeFont.pointSizeF() * 0.85);
        m_badge->setFont(badgeFont);
        m_badge->setStyleSheet(
            QStringLiteral("background:%1; color:%2; border-radius:9px;")
                .arg(m_accent.name(), pal.color(QPalette::HighlightedText).name()));

        QColor muted = pal.color(QPalette::WindowText);
        muted.setAlpha(175);
        m_titleLabel->setPalette(pal);
        m_conditionLabel->setPalette(pal);
        m_conditionLabel->setStyleSheet(
            QStringLiteral("color:%1;").arg(muted.name()));
    }

    QColor m_accent;
    QWidget* m_leftBar = nullptr;
    QFrame* m_chip = nullptr;
    QLabel* m_badge = nullptr;
    QLabel* m_titleLabel = nullptr;
    QLabel* m_conditionLabel = nullptr;
    QPushButton* m_editButton = nullptr;
    QPushButton* m_deleteButton = nullptr;
};

void attachChipHeaderRowActions(BlockListWidget* table,
                                int tableRow,
                                const QColor& accent,
                                const QString& badgeText,
                                const QString& title,
                                const QString& conditionText,
                                const QString& editToolTip,
                                const QString& deleteToolTip,
                                const std::function<void()>& onEdit,
                                const std::function<void()>& onDelete) {
    if (!table || tableRow < 0) {
        return;
    }

    table->setSpan(tableRow, 0, 1, table->columnCount());

    auto* header = new WorkflowChipHeaderRowWidget(accent, badgeText, title, conditionText, table);
    header->editButton()->setToolTip(editToolTip);
    header->deleteButton()->setToolTip(deleteToolTip);
    header->setEnabled(table->isReorderEnabled());
    QObject::connect(header->editButton(), &QPushButton::clicked, table, [onEdit]() { onEdit(); });
    QObject::connect(header->deleteButton(), &QPushButton::clicked, table, [onDelete]() { onDelete(); });
    table->setCellWidget(tableRow, 0, header);
}

constexpr int kMissFlashMs = 72;
constexpr int kSuccessFlashMs = 140;
constexpr int kFailedFlashMs = 170;
constexpr int kReturnToPreviousHoldMs = 560;
constexpr int kReturnToPreviousFadeMs = 840;
constexpr int kReturnToPreviousGlassAlphaCap = 78;
constexpr qreal kRunningGlassIntensity = 0.16;
constexpr int kMaxGlassAlpha = 52;

QColor blendOver(const QColor& base, const QColor& overlay) {
    if (!overlay.isValid() || overlay.alpha() <= 0) {
        return base;
    }
    const qreal alpha = overlay.alphaF();
    return QColor(static_cast<int>(base.red() * (1.0 - alpha) + overlay.red() * alpha),
                  static_cast<int>(base.green() * (1.0 - alpha) + overlay.green() * alpha),
                  static_cast<int>(base.blue() * (1.0 - alpha) + overlay.blue() * alpha));
}

QColor tintOverlay(const QColor& tint, qreal intensity, int alphaCap = kMaxGlassAlpha) {
    const int alpha = qBound(0, static_cast<int>(alphaCap * qBound(0.0, intensity, 1.0)), 255);
    QColor overlay = tint;
    overlay.setAlpha(alpha);
    return overlay;
}

struct GlassColors {
    QColor tint;
    qreal intensity = 0.0;
    QColor foreground;
    QColor scoreForeground;
};

GlassColors glassColorsFor(BlockListWidget::ExecutionHighlight highlight,
                           qreal intensity,
                           const QPalette& palette) {
    GlassColors colors;
    colors.intensity = qBound(0.0, intensity, 1.0);
    colors.foreground = palette.color(QPalette::Text);
    colors.scoreForeground = palette.color(QPalette::Text);

    switch (highlight) {
    case BlockListWidget::ExecutionHighlight::Running:
        colors.tint = QColor(255, 196, 92);
        break;
    case BlockListWidget::ExecutionHighlight::TriggerWatch:
        colors.tint = QColor(128, 188, 172);
        break;
    case BlockListWidget::ExecutionHighlight::TriggerCooldown:
        colors.tint = QColor(140, 146, 154);
        colors.intensity = qMin(1.0, colors.intensity * 0.75);
        break;
    case BlockListWidget::ExecutionHighlight::ImageFindMiss:
        colors.tint = QColor(228, 88, 102);
        break;
    case BlockListWidget::ExecutionHighlight::Success:
        colors.tint = QColor(82, 196, 132);
        break;
    case BlockListWidget::ExecutionHighlight::Failed:
        colors.tint = QColor(214, 96, 96);
        break;
    case BlockListWidget::ExecutionHighlight::ReturnToPrevious:
        colors.tint = QColor(168, 108, 232);
        colors.intensity = qMin(1.0, colors.intensity * 1.12);
        break;
    case BlockListWidget::ExecutionHighlight::None:
    default:
        colors.tint = palette.color(QPalette::Highlight);
        break;
    }
    return colors;
}

QBrush glassBodyBrush(const GlassColors& colors, const QPalette& palette, int rowHeight) {
    const QColor base = palette.color(QPalette::Base);
    const QColor alt = palette.color(QPalette::AlternateBase);
    const QColor tint = tintOverlay(colors.tint, colors.intensity);
    const QColor specular = tintOverlay(QColor(255, 255, 255), colors.intensity, 28);

    const QColor top = blendOver(base, specular);
    const QColor mid = blendOver(base, tint);
    const QColor bottom = blendOver(alt.isValid() ? alt : base, tintOverlay(colors.tint, colors.intensity * 0.55));

    QLinearGradient gradient(0, 0, 0, qMax(rowHeight, 1));
    gradient.setColorAt(0.0, top);
    gradient.setColorAt(0.18, mid);
    gradient.setColorAt(1.0, bottom);
    gradient.setCoordinateMode(QGradient::ObjectBoundingMode);
    return QBrush(gradient);
}

QBrush glassIndexBrush(const GlassColors& colors, const QPalette& palette, int rowHeight) {
    const QColor base = palette.color(QPalette::Base);
    const QColor edge = tintOverlay(colors.tint, qMin(1.0, colors.intensity * 1.35), kMaxGlassAlpha + 18);
    const QColor body = tintOverlay(colors.tint, colors.intensity * 0.75);

    QLinearGradient gradient(0, 0, 36, 0);
    gradient.setColorAt(0.0, blendOver(base, edge));
    gradient.setColorAt(0.12, blendOver(base, body));
    gradient.setColorAt(1.0, blendOver(base, tintOverlay(colors.tint, colors.intensity * 0.35)));
    gradient.setCoordinateMode(QGradient::ObjectBoundingMode);
    Q_UNUSED(rowHeight)
    return QBrush(gradient);
}

class CenterIconDelegate : public QStyledItemDelegate {
public:
    using QStyledItemDelegate::QStyledItemDelegate;

    void paint(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index) const override {
        QStyleOptionViewItem opt = option;
        initStyleOption(&opt, index);

        const QIcon icon = opt.icon;
        opt.icon = {};
        opt.text.clear();

        const QWidget* widget = opt.widget;
        QStyle* style = widget ? widget->style() : QApplication::style();
        style->drawControl(QStyle::CE_ItemViewItem, &opt, painter, widget);

        if (icon.isNull()) {
            return;
        }

        const QSize iconSize = opt.decorationSize.isValid() ? opt.decorationSize : QSize(kThumbnailSize, kThumbnailSize);
        const QRect rect = opt.rect;
        const QRect iconRect(rect.left() + (rect.width() - iconSize.width()) / 2,
                             rect.top() + (rect.height() - iconSize.height()) / 2,
                             iconSize.width(),
                             iconSize.height());
        const qreal dpr = painter->device() ? painter->device()->devicePixelRatioF() : 1.0;
        const QPixmap pm = icon.pixmap(QSize(qRound(iconSize.width() * dpr),
                                             qRound(iconSize.height() * dpr)));
        if (pm.isNull()) {
            return;
        }
        painter->save();
        painter->setRenderHint(QPainter::SmoothPixmapTransform, true);
        painter->drawPixmap(iconRect, pm);
        painter->restore();
    }
};

class CenterCheckBoxDelegate : public QStyledItemDelegate {
public:
    using QStyledItemDelegate::QStyledItemDelegate;

    static QRect centeredCheckIndicatorRect(const QStyleOptionViewItem& option, const QWidget* widget) {
        QStyleOptionButton buttonOpt;
        buttonOpt.state = QStyle::State_Enabled;
        QStyle* style = widget ? widget->style() : QApplication::style();
        const QRect indicator = style->subElementRect(QStyle::SE_CheckBoxIndicator, &buttonOpt, widget);
        const QRect cell = option.rect;
        return QRect(cell.left() + (cell.width() - indicator.width()) / 2,
                     cell.top() + (cell.height() - indicator.height()) / 2,
                     indicator.width(),
                     indicator.height());
    }

    void paint(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index) const override {
        QStyleOptionViewItem opt = option;
        initStyleOption(&opt, index);

        const bool checkable = index.flags() & Qt::ItemIsUserCheckable;
        if (checkable) {
            opt.features &= ~QStyleOptionViewItem::HasCheckIndicator;
            opt.text.clear();
        } else {
            opt.displayAlignment = Qt::AlignCenter;
        }

        const QWidget* widget = opt.widget;
        QStyle* style = widget ? widget->style() : QApplication::style();
        style->drawControl(QStyle::CE_ItemViewItem, &opt, painter, widget);

        if (!checkable) {
            return;
        }

        QStyleOptionButton checkOpt;
        checkOpt.state = QStyle::State_Enabled;
        const auto checkState = static_cast<Qt::CheckState>(index.data(Qt::CheckStateRole).toInt());
        switch (checkState) {
        case Qt::Checked:
            checkOpt.state |= QStyle::State_On;
            break;
        case Qt::PartiallyChecked:
            checkOpt.state |= QStyle::State_NoChange;
            break;
        default:
            checkOpt.state |= QStyle::State_Off;
            break;
        }
        checkOpt.rect = centeredCheckIndicatorRect(opt, widget);
        style->drawControl(QStyle::CE_CheckBox, &checkOpt, painter, widget);
    }

    bool editorEvent(QEvent* event,
                     QAbstractItemModel* model,
                     const QStyleOptionViewItem& option,
                     const QModelIndex& index) override {
        if (!(index.flags() & Qt::ItemIsUserCheckable)) {
            return QStyledItemDelegate::editorEvent(event, model, option, index);
        }
        if (event->type() != QEvent::MouseButtonRelease) {
            return false;
        }
        const auto* mouseEvent = static_cast<const QMouseEvent*>(event);
        if (mouseEvent->button() != Qt::LeftButton) {
            return false;
        }

        QStyleOptionViewItem opt = option;
        initStyleOption(&opt, index);
        if (!centeredCheckIndicatorRect(opt, opt.widget).contains(mouseEvent->pos())) {
            return false;
        }

        const auto state = static_cast<Qt::CheckState>(index.data(Qt::CheckStateRole).toInt());
        return model->setData(index, state == Qt::Checked ? Qt::Unchecked : Qt::Checked, Qt::CheckStateRole);
    }
};

class DropInsertionIndicator : public QWidget {
public:
    explicit DropInsertionIndicator(QWidget* parent)
        : QWidget(parent) {
        setAttribute(Qt::WA_TransparentForMouseEvents);
        setAttribute(Qt::WA_NoSystemBackground);
        setFixedHeight(6);
        m_timer.setInterval(380);
        connect(&m_timer, &QTimer::timeout, this, [this]() {
            m_bright = !m_bright;
            update();
        });
    }

    void showAt(int y, int width) {
        setGeometry(0, y - height() / 2, width, height());
        if (!m_timer.isActive()) {
            m_timer.start();
        }
        show();
        raise();
        update();
    }

    void hideIndicator() {
        m_timer.stop();
        hide();
    }

protected:
    void paintEvent(QPaintEvent*) override {
        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing);

        const QColor lineColor = m_bright ? QColor(130, 165, 195) : QColor(165, 188, 210);
        const QColor dotColor = m_bright ? QColor(110, 145, 175) : QColor(145, 170, 195);
        const int centerY = height() / 2;
        const int lineHeight = 3;

        painter.setPen(Qt::NoPen);
        painter.setBrush(lineColor);
        painter.drawRoundedRect(0, centerY - lineHeight / 2, width(), lineHeight, 1, 1);

        const int dotRadius = 4;
        painter.setBrush(dotColor);
        painter.drawEllipse(QPointF(dotRadius, centerY), dotRadius, dotRadius);
        painter.drawEllipse(QPointF(width() - dotRadius, centerY), dotRadius, dotRadius);
    }

private:
    QTimer m_timer;
    bool m_bright = true;
};

struct LoopRegionChromeGeometry {
    QRect groupFill;
    QRect leftBar;
    bool visible = false;
};

QString loopRegionHeaderTitle() {
    return BlockListWidget::tr("구간 반복");
}

QString loopRegionHeaderCondition(const WorkflowLoopRegion& region) {
    QString condition = QString::fromStdString(loopExitConditionDisplayLabel(region.exitCondition));
    if (region.exitCondition == LoopExitCondition::DetectionFailed && region.detectionMissLimit > 1) {
        condition += BlockListWidget::tr(" (%1회)").arg(region.detectionMissLimit);
    }
    return condition;
}

QString loopRegionHeaderLabel(const WorkflowLoopRegion& region) {
    return QStringLiteral("↻ ") + loopRegionHeaderTitle() + QStringLiteral(" · ")
           + loopRegionHeaderCondition(region);
}

const WorkflowLoopRegion* loopRegionForTableRow(const BlockListWidget* table, int tableRow) {
    if (!table) {
        return nullptr;
    }
    const QString regionId = table->loopRegionIdForTableRow(tableRow);
    if (regionId.isEmpty()) {
        return nullptr;
    }
    for (const WorkflowLoopRegion& region : table->loopRegions()) {
        if (QString::fromStdString(region.id) == regionId) {
            return &region;
        }
    }
    return nullptr;
}

QRect tableRowRect(const QTableWidget* table, int tableRow) {
    if (!table || tableRow < 0 || tableRow >= table->rowCount()) {
        return {};
    }
    QRect rowRect = table->visualRect(table->model()->index(tableRow, 0));
    const int lastColumn = kColMatch;
    if (lastColumn > 0) {
        const QRect lastRect = table->visualRect(table->model()->index(tableRow, lastColumn));
        if (lastRect.isValid()) {
            rowRect.setRight(lastRect.right());
        }
    }
    return rowRect;
}

QRect loopRegionHeaderRowRect(const QTableWidget* table, int tableRow) {
    return tableRowRect(table, tableRow);
}

void paintWorkflowChipHeader(QPainter* painter,
                             const QRect& rowRect,
                             const QPalette& pal,
                             bool selected,
                             const QColor& accent,
                             const QString& badgeText,
                             const QString& title,
                             const QString& conditionText) {
    const QColor base = pal.color(QPalette::Base);

    QColor rowTint = accent;
    rowTint.setAlpha(selected ? 16 : 8);
    painter->fillRect(rowRect, blendOver(base, rowTint));

    const int insetY = 3;
    const int barWidth = 4;
    QRect leftBar(rowRect.left() + 2, rowRect.top() + insetY, barWidth, rowRect.height() - insetY * 2);
    painter->setPen(Qt::NoPen);
    painter->setBrush(accent);
    painter->drawRoundedRect(leftBar, 2, 2);

    QFont titleFont = painter->font();
    titleFont.setBold(true);
    QFont conditionFont = painter->font();
    conditionFont.setBold(false);

    QFontMetrics titleMetrics(titleFont);
    QFontMetrics conditionMetrics(conditionFont);
    const QString separator = QStringLiteral(" · ");
    const int chipPadX = 10;
    const int chipPadY = 4;
    const int badgeSize = qMax(16, titleMetrics.height());
    const int chipWidth = badgeSize + 8 + titleMetrics.horizontalAdvance(title)
                          + conditionMetrics.horizontalAdvance(separator + conditionText) + chipPadX * 2;
    const int chipHeight = qMax(badgeSize + 4, titleMetrics.height() + chipPadY * 2);
    QRect chipRect(rowRect.left() + leftBar.right() + 8,
                   rowRect.top() + (rowRect.height() - chipHeight) / 2,
                   chipWidth,
                   chipHeight);

    QColor chipFill = pal.color(QPalette::Button);
    if (selected) {
        chipFill = blendOver(chipFill, tintOverlay(accent, 0.08));
    }
    painter->setPen(Qt::NoPen);
    painter->setBrush(chipFill);
    painter->drawRoundedRect(chipRect, 6, 6);

    QColor chipBorder = accent;
    chipBorder.setAlpha(selected ? 200 : 140);
    painter->setPen(QPen(chipBorder, 1.0));
    painter->setBrush(Qt::NoBrush);
    painter->drawRoundedRect(chipRect, 6, 6);

    QRect badgeRect(chipRect.left() + chipPadX,
                    chipRect.top() + (chipRect.height() - badgeSize) / 2,
                    badgeSize,
                    badgeSize);
    QColor badgeFill = accent;
    badgeFill.setAlpha(selected ? 220 : 180);
    painter->setPen(Qt::NoPen);
    painter->setBrush(badgeFill);
  if (badgeText.size() == 1) {
        painter->drawEllipse(badgeRect);
    } else {
        painter->drawRoundedRect(badgeRect, 4, 4);
    }

    QFont badgeFont = titleFont;
    badgeFont.setPointSizeF(badgeFont.pointSizeF() * 0.85);
    painter->setFont(badgeFont);
    painter->setPen(pal.color(QPalette::HighlightedText));
    painter->drawText(badgeRect, Qt::AlignCenter, badgeText);

    const int textLeft = badgeRect.right() + 6;
    const QRect titleRect(textLeft, chipRect.top(), titleMetrics.horizontalAdvance(title), chipRect.height());
    painter->setFont(titleFont);
    painter->setPen(pal.color(QPalette::WindowText));
    painter->drawText(titleRect, Qt::AlignLeft | Qt::AlignVCenter, title);

    const QRect conditionRect(titleRect.right(),
                              chipRect.top(),
                              chipRect.right() - titleRect.right() - chipPadX,
                              chipRect.height());
    QColor mutedText = pal.color(QPalette::WindowText);
    mutedText.setAlpha(175);
    painter->setFont(conditionFont);
    painter->setPen(mutedText);
    painter->drawText(conditionRect, Qt::AlignLeft | Qt::AlignVCenter, separator + conditionText);
}

class BlockListChromeRowDelegate : public QStyledItemDelegate {
public:
    explicit BlockListChromeRowDelegate(BlockListWidget* owner)
        : QStyledItemDelegate(owner)
        , m_owner(owner) {}

    void paint(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index) const override {
        if (!m_owner || index.column() != kColIndex) {
            QStyledItemDelegate::paint(painter, option, index);
            return;
        }

        const BlockListRowMeta& meta = m_owner->rowMeta(index.row());
        if (meta.kind == BlockListRowKind::MainBlock
            || meta.kind == BlockListRowKind::LoopRegionHeader) {
            QStyledItemDelegate::paint(painter, option, index);
            return;
        }

        QStyleOptionViewItem opt = option;
        initStyleOption(&opt, index);
        opt.text.clear();
        opt.icon = {};

        const QWidget* widget = opt.widget;
        QStyle* style = widget ? widget->style() : QApplication::style();
        style->drawPrimitive(QStyle::PE_PanelItemViewItem, &opt, painter, widget);

        const QRect rowRect = tableRowRect(m_owner, index.row());
        if (!rowRect.isValid()) {
            return;
        }

        painter->save();
        painter->setRenderHint(QPainter::Antialiasing);

        const QPalette pal = m_owner->palette();
        const bool selected = opt.state & QStyle::State_Selected;

        if (meta.kind == BlockListRowKind::LoopRegionHeader) {
            const WorkflowLoopRegion* region = loopRegionForTableRow(m_owner, index.row());
            if (region) {
                paintWorkflowChipHeader(painter,
                                        rowRect,
                                        pal,
                                        selected,
                                        pal.color(QPalette::Highlight),
                                        QStringLiteral("↻"),
                                        loopRegionHeaderTitle(),
                                        loopRegionHeaderCondition(*region));
            }
        }

        painter->restore();
    }

    QSize sizeHint(const QStyleOptionViewItem& option, const QModelIndex& index) const override {
        if (!m_owner || index.column() != kColIndex) {
            return QStyledItemDelegate::sizeHint(option, index);
        }
        switch (m_owner->tableRowKind(index.row())) {
        case BlockListRowKind::LoopRegionHeader:
            return QSize(option.rect.width(), kLoopRegionHeaderHeight);
        default:
            break;
        }
        return QStyledItemDelegate::sizeHint(option, index);
    }

private:
    BlockListWidget* m_owner = nullptr;
};

LoopRegionChromeGeometry loopRegionChromeGeometry(const BlockListWidget* table,
                                                const WorkflowLoopRegion& region) {
    LoopRegionChromeGeometry geometry;
    if (!table || region.startIndex < 0 || region.endIndex >= table->blockCount()) {
        return geometry;
    }

    const int startTableRow = table->tableRowForBlockRow(region.startIndex);
    const int endTableRow = table->tableRowForBlockRow(region.endIndex);
    if (startTableRow < 0 || endTableRow < 0) {
        return geometry;
    }

    const QRect topRect = table->visualRect(table->model()->index(startTableRow, 0));
    const QRect bottomRect = table->visualRect(table->model()->index(endTableRow, 0));
    if (!topRect.isValid() || !bottomRect.isValid()) {
        return geometry;
    }

    const int viewportHeight = table->viewport() ? table->viewport()->height() : 0;
    if (bottomRect.bottom() < 0 || topRect.top() > viewportHeight) {
        return geometry;
    }

    geometry.visible = true;
    const int marginLeft = 1;
    const int barWidth = 5;
    const int groupTop = topRect.top();
    const int groupBottom = bottomRect.bottom();
    const int viewportWidth = table->viewport() ? table->viewport()->width() : table->width();

    geometry.leftBar = QRect(marginLeft, groupTop + 1, barWidth, groupBottom - groupTop - 1);
    geometry.groupFill =
        QRect(marginLeft + barWidth + 2, groupTop, viewportWidth - marginLeft - barWidth - 6, groupBottom - groupTop + 1);
    return geometry;
}

class LoopRegionChromeOverlay : public QWidget {
public:
    explicit LoopRegionChromeOverlay(BlockListWidget* owner, QWidget* parent)
        : QWidget(parent)
        , m_owner(owner) {
        setAttribute(Qt::WA_TransparentForMouseEvents);
        setAttribute(Qt::WA_NoSystemBackground);
    }

    void setRegions(const std::vector<WorkflowLoopRegion>& regions) {
        m_regions = regions;
        setVisible(!m_regions.empty());
        update();
    }

protected:
    void paintEvent(QPaintEvent*) override {
        if (!m_owner || m_regions.empty()) {
            return;
        }

        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing);
        const QPalette pal = m_owner->palette();
        const QColor accent = pal.color(QPalette::Highlight);
        const QColor accentSoft = [&accent]() {
            QColor color = accent;
            color.setAlpha(18);
            return color;
        }();
        const QColor border = [&accent]() {
            QColor color = accent;
            color.setAlpha(72);
            return color;
        }();

        for (const WorkflowLoopRegion& region : m_regions) {
            const LoopRegionChromeGeometry geometry = loopRegionChromeGeometry(m_owner, region);
            if (!geometry.visible) {
                continue;
            }

            painter.fillRect(geometry.groupFill, accentSoft);

            painter.setPen(Qt::NoPen);
            painter.setBrush(accent);
            painter.drawRoundedRect(geometry.leftBar, 2, 2);

            painter.setPen(QPen(border, 1.0));
            painter.setBrush(Qt::NoBrush);
            painter.drawLine(geometry.groupFill.left(),
                             geometry.groupFill.top() + 1,
                             geometry.groupFill.right(),
                             geometry.groupFill.top() + 1);
            painter.drawLine(geometry.groupFill.left(),
                             geometry.groupFill.bottom() - 1,
                             geometry.groupFill.right(),
                             geometry.groupFill.bottom() - 1);
            painter.drawLine(geometry.leftBar.right() + 1,
                             geometry.groupFill.top() + 1,
                             geometry.leftBar.right() + 1,
                             geometry.groupFill.bottom() - 1);
        }
    }

private:
    BlockListWidget* m_owner = nullptr;
    std::vector<WorkflowLoopRegion> m_regions;
};

} // namespace

BlockListWidget::BlockListWidget(QWidget* parent)
    : QTableWidget(parent)
    , m_blockRowHeight(UiResizeHandle::kDefaultBlockListRowHeightPx) {
    m_columnLayout.rowHeight = m_blockRowHeight;
    m_columnLayout.matchWidth = kMatchColumnWidth;
    initBlockListColumnHeader(this);
    setColumnLayout(m_columnLayout, false);
    setIconSize(QSize(kThumbnailSize, kThumbnailSize));
    setItemDelegateForColumn(0, new BlockListChromeRowDelegate(this));
    setItemDelegateForColumn(1, new CenterIconDelegate(this));
    setItemDelegateForColumn(kColRoiCorrection, new CenterCheckBoxDelegate(this));

    verticalHeader()->setDefaultSectionSize(m_blockRowHeight);
    verticalHeader()->setVisible(false);
    setStyleSheet(QStringLiteral("QTableWidget::item { padding-top: 1px; padding-bottom: 1px; }"));
    setSelectionBehavior(QAbstractItemView::SelectRows);
    setSelectionMode(QAbstractItemView::ExtendedSelection);
    setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_defaultToolTip =
        tr("드래그하여 순서 변경 · Ctrl+C/V 복사/붙여넣기 · Ctrl+Z/Y 실행 취소/다시 · Delete 삭제");
    setToolTip(m_defaultToolTip);
    setFocusPolicy(Qt::StrongFocus);
    setDefaultDropAction(Qt::MoveAction);
    setDragDropOverwriteMode(false);
    setDropIndicatorShown(false);
    setReorderEnabled(true);
    setAlternatingRowColors(false);
    setShowGrid(false);
    setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

    m_flashAnimation = new QVariantAnimation(this);
    m_flashAnimation->setStartValue(1.0);
    m_flashAnimation->setEndValue(0.0);
    m_flashAnimation->setEasingCurve(QEasingCurve::OutCubic);
    connect(m_flashAnimation,
            &QVariantAnimation::valueChanged,
            this,
            &BlockListWidget::onFlashAnimationValueChanged);
    connect(m_flashAnimation,
            &QVariantAnimation::finished,
            this,
            &BlockListWidget::onFlashAnimationFinished);

    m_returnFlashHoldTimer = new QTimer(this);
    m_returnFlashHoldTimer->setSingleShot(true);
    connect(m_returnFlashHoldTimer, &QTimer::timeout, this, [this]() {
        if (m_flashKind == ExecutionHighlight::ReturnToPrevious) {
            startReturnToPreviousFade(kReturnToPreviousFadeMs);
        }
    });

    m_dragAutoScroll = new ListDragAutoScroll(this, this);
    m_dragAutoScroll->setOnScrolled([this]() { refreshDragScrollDependentUi(); });

    m_dropIndicator = new DropInsertionIndicator(viewport());
    m_dropIndicator->hide();

    m_loopRegionChrome = new LoopRegionChromeOverlay(this, viewport());
    m_loopRegionChrome->hide();

    viewport()->setMouseTracking(true);
    viewport()->installEventFilter(this);

    connect(verticalScrollBar(), &QScrollBar::valueChanged, this, [this](int) {
        updateLoopRegionChrome();
    });
    connect(horizontalScrollBar(), &QScrollBar::valueChanged, this, [this](int) {
        updateLoopRegionChrome();
    });

    connect(this, &QTableWidget::itemChanged, this, [this](QTableWidgetItem* changedItem) {
        if (!changedItem || changedItem->column() != kColRoiCorrection || m_updatingRoiCorrectionItem) {
            return;
        }
        const int blockRow = blockRowForTableRow(changedItem->row());
        if (blockRow < 0 || blockRow >= static_cast<int>(m_rowIsImageFind.size())
            || !m_rowIsImageFind[static_cast<size_t>(blockRow)]) {
            return;
        }
        emit imageFindRoiCorrectionChanged(blockRow, changedItem->checkState() == Qt::Checked);
    });
}

void BlockListWidget::keyPressEvent(QKeyEvent* event) {
    if (m_loopRegionPickActive && event->key() == Qt::Key_Escape) {
        cancelLoopRegionPick();
        emit loopRegionPickCancelled();
        event->accept();
        return;
    }
    if (m_reorderEnabled && !m_loopRegionPickActive) {
        if (event->matches(QKeySequence::Copy)) {
            emit copyRequested();
            event->accept();
            return;
        }
        if (event->matches(QKeySequence::Paste)) {
            emit pasteRequested();
            event->accept();
            return;
        }
        if (event->matches(QKeySequence::Undo)) {
            emit undoRequested();
            event->accept();
            return;
        }
        if (event->matches(QKeySequence::Redo)) {
            emit redoRequested();
            event->accept();
            return;
        }
        if (event->key() == Qt::Key_Delete) {
            emit deleteRequested();
            event->accept();
            return;
        }
        if (event->key() == Qt::Key_Return || event->key() == Qt::Key_Enter) {
            emit editRequested();
            event->accept();
            return;
        }
    }
    QTableWidget::keyPressEvent(event);
}

void BlockListWidget::setBlockRowHeight(int height, bool persist) {
    BlockListColumnLayout layout = m_columnLayout;
    layout.rowHeight = height;
    const bool restoring = m_restoringRowHeight;
    if (!restoring) {
        setColumnLayout(layout, persist);
    } else {
        const int clamped = UiResizeHandle::clampListRowHeight(height);
        m_columnLayout.rowHeight = clamped;
        m_blockRowHeight = clamped;
        verticalHeader()->setDefaultSectionSize(m_blockRowHeight);
        for (int tableRow = 0; tableRow < rowCount(); ++tableRow) {
            if (blockRowForTableRow(tableRow) >= 0) {
                setRowHeight(tableRow, m_blockRowHeight);
            }
        }
    }
    if (persist && !restoring) {
        emit rowHeightChanged();
    }
}

void BlockListWidget::saveRowHeight(QSettings& settings, const QString& settingsKey) const {
    settings.setValue(settingsKey + QStringLiteral("/rowHeight"), m_blockRowHeight);
}

void BlockListWidget::restoreRowHeight(const QSettings& settings, const QString& settingsKey) {
    if (!settings.contains(settingsKey + QStringLiteral("/rowHeight"))) {
        return;
    }
    const int restored = settings.value(settingsKey + QStringLiteral("/rowHeight")).toInt();
    m_restoringRowHeight = true;
    setBlockRowHeight(restored, false);
    m_restoringRowHeight = false;
}

int BlockListWidget::totalFixedColumnWidth(bool includeSummaryWidth, int summaryWidth) const {
    int total = m_columnLayout.indexWidth + m_columnLayout.previewWidth + m_columnLayout.actionWidth
                + m_columnLayout.durationWidth + m_columnLayout.matchDurationWidth
                + m_columnLayout.attemptsWidth + m_columnLayout.returnPrevWidth + m_columnLayout.retryWidth
                + m_columnLayout.scoreWidth + m_columnLayout.matchWidth;
    if (m_roiCorrectionColumnVisible) {
        total += m_columnLayout.roiCorrectionWidth;
    }
    if (includeSummaryWidth) {
        total += summaryWidth;
    }
    return total;
}

int BlockListWidget::summaryColumnWidthForViewport(int viewportWidth) const {
    const int fixedWithoutSummary = totalFixedColumnWidth(false, 0);
    return qMax(kMinSummaryWidth, viewportWidth - fixedWithoutSummary);
}

int BlockListWidget::columnWidthFromLayout(const BlockListColumnLayout& layout, int column) {
    return blockListColumnWidthFromLayout(layout, column);
}

void BlockListWidget::setLayoutColumnWidth(BlockListColumnLayout& layout, int column, int width) {
    blockListSetLayoutColumnWidth(layout, column, width);
}

void BlockListWidget::columnWidthBounds(int column, int& minOut, int& maxOut) {
    blockListColumnWidthBounds(column, minOut, maxOut);
}

void BlockListWidget::syncColumnLayoutFromTable(BlockListColumnLayout& layout) const {
    for (int col = 0; col < columnCount(); ++col) {
        if (isColumnHidden(col)) {
            continue;
        }
        setLayoutColumnWidth(layout, col, columnWidth(col));
    }
}

int BlockListWidget::headerContentLeftInset() const {
    return frameWidth();
}

void BlockListWidget::reconcileSummarySlack() {
    if (!viewport()) {
        return;
    }
    const int viewportWidth = viewport()->width();
    int total = 0;
    for (int col = 0; col < columnCount(); ++col) {
        if (!isColumnHidden(col)) {
            total += columnWidth(col);
        }
    }
    if (total < viewportWidth) {
        const int extra = viewportWidth - total;
        m_columnLayout.summaryWidth += extra;
        setColumnWidth(kColSummary, columnWidth(kColSummary) + extra);
    } else if (total > viewportWidth) {
        const int deficit = total - viewportWidth;
        const int shrinkable = qMax(0, columnWidth(kColSummary) - kMinSummaryWidth);
        const int shrink = qMin(deficit, shrinkable);
        if (shrink > 0) {
            m_columnLayout.summaryWidth -= shrink;
            setColumnWidth(kColSummary, columnWidth(kColSummary) - shrink);
        }
    }
}

void BlockListWidget::applyPairwiseColumnResize(int rightColumnIndex,
                                                int deltaX,
                                                const BlockListColumnLayout& startLayout) {
    const int leftColumnIndex = blockListPreviousVisibleColumn(this, rightColumnIndex);
    if (leftColumnIndex < 0) {
        return;
    }

    int leftWidth = columnWidthFromLayout(startLayout, leftColumnIndex);
    int rightWidth = columnWidthFromLayout(startLayout, rightColumnIndex);
    int minLeft = 0;
    int maxLeft = 0;
    int minRight = 0;
    int maxRight = 0;
    columnWidthBounds(leftColumnIndex, minLeft, maxLeft);
    columnWidthBounds(rightColumnIndex, minRight, maxRight);

    int newLeft = qBound(minLeft, leftWidth + deltaX, maxLeft);
    int actualDelta = newLeft - leftWidth;
    int newRight = qBound(minRight, rightWidth - actualDelta, maxRight);
    actualDelta = rightWidth - newRight;
    newLeft = qBound(minLeft, leftWidth + actualDelta, maxLeft);

    BlockListColumnLayout layout = startLayout;
    setLayoutColumnWidth(layout, leftColumnIndex, newLeft);
    setLayoutColumnWidth(layout, rightColumnIndex, newRight);
    setColumnLayout(layout, true);
}

void BlockListWidget::applyColumnLayoutToTable(bool reconcileSlack) {
    if (columnCount() != kColumnCount) {
        return;
    }

    setColumnWidth(kColIndex, m_columnLayout.indexWidth);
    setColumnWidth(kColPreview, m_columnLayout.previewWidth);
    setColumnWidth(kColAction, m_columnLayout.actionWidth);
    setColumnWidth(kColSummary, m_columnLayout.summaryWidth);
    setColumnWidth(kColDuration, m_columnLayout.durationWidth);
    setColumnWidth(kColMatchDuration, m_columnLayout.matchDurationWidth);
    setColumnWidth(kColAttempts, m_columnLayout.attemptsWidth);
    setColumnWidth(kColReturnPrev, m_columnLayout.returnPrevWidth);
    setColumnWidth(kColRetryAfterNext, m_columnLayout.retryWidth);
    setColumnWidth(kColScore, m_columnLayout.scoreWidth);
    if (m_roiCorrectionColumnVisible) {
        setColumnWidth(kColRoiCorrection, m_columnLayout.roiCorrectionWidth);
    }
    setColumnWidth(kColMatch, m_columnLayout.matchWidth);

    if (reconcileSlack) {
        reconcileSummarySlack();
    }
}

void BlockListWidget::setColumnLayout(const BlockListColumnLayout& layout, bool persist) {
    BlockListColumnLayout clamped = layout;
    clamped.indexWidth =
        qBound(kMinIndexColumnWidth, layout.indexWidth, kMaxIndexColumnWidth);
    clamped.previewWidth =
        qBound(kMinPreviewColumnWidth, layout.previewWidth, kMaxPreviewColumnWidth);
    clamped.actionWidth =
        qBound(kMinActionColumnWidth, layout.actionWidth, kMaxActionColumnWidth);
    clamped.durationWidth =
        qBound(kMinMetricColumnWidth, layout.durationWidth, kMaxMetricColumnWidth);
    clamped.matchDurationWidth =
        qBound(kMinMetricColumnWidth, layout.matchDurationWidth, kMaxMetricColumnWidth);
    clamped.attemptsWidth =
        qBound(kMinMetricColumnWidth, layout.attemptsWidth, kMaxMetricColumnWidth);
    clamped.returnPrevWidth =
        qBound(kMinMetricColumnWidth, layout.returnPrevWidth, kMaxMetricColumnWidth);
    clamped.retryWidth = qBound(kMinMetricColumnWidth, layout.retryWidth, kMaxMetricColumnWidth);
    clamped.scoreWidth = qBound(kMinMetricColumnWidth, layout.scoreWidth, kMaxMetricColumnWidth);
    clamped.roiCorrectionWidth =
        qBound(kMinMetricColumnWidth, layout.roiCorrectionWidth, kMaxMetricColumnWidth);
    clamped.matchWidth = qBound(kMinMatchColumnWidth, layout.matchWidth, kMaxMatchColumnWidth);
    clamped.summaryWidth =
        qBound(kMinSummaryWidth, layout.summaryWidth, kMaxSummaryColumnWidth);
    clamped.rowHeight = UiResizeHandle::clampListRowHeight(layout.rowHeight);

    if (clamped.indexWidth == m_columnLayout.indexWidth
        && clamped.previewWidth == m_columnLayout.previewWidth
        && clamped.actionWidth == m_columnLayout.actionWidth
        && clamped.summaryWidth == m_columnLayout.summaryWidth
        && clamped.durationWidth == m_columnLayout.durationWidth
        && clamped.matchDurationWidth == m_columnLayout.matchDurationWidth
        && clamped.attemptsWidth == m_columnLayout.attemptsWidth
        && clamped.returnPrevWidth == m_columnLayout.returnPrevWidth
        && clamped.retryWidth == m_columnLayout.retryWidth
        && clamped.scoreWidth == m_columnLayout.scoreWidth
        && clamped.roiCorrectionWidth == m_columnLayout.roiCorrectionWidth
        && clamped.matchWidth == m_columnLayout.matchWidth
        && clamped.rowHeight == m_columnLayout.rowHeight) {
        return;
    }

    m_columnLayout = clamped;
    m_blockRowHeight = clamped.rowHeight;
    verticalHeader()->setDefaultSectionSize(m_blockRowHeight);
    applyColumnLayoutToTable();
    if (m_columnHeader) {
        m_columnHeader->syncToLayout();
    }
    for (int tableRow = 0; tableRow < rowCount(); ++tableRow) {
        if (blockRowForTableRow(tableRow) >= 0) {
            setRowHeight(tableRow, m_blockRowHeight);
        }
    }
    if (persist && !m_restoringColumnLayout) {
        emit columnLayoutChanged();
    }
}

void BlockListWidget::saveColumnLayout(QSettings& settings, const QString& settingsKey) const {
    settings.setValue(settingsKey + QStringLiteral("/indexWidth"), m_columnLayout.indexWidth);
    settings.setValue(settingsKey + QStringLiteral("/previewWidth"), m_columnLayout.previewWidth);
    settings.setValue(settingsKey + QStringLiteral("/actionWidth"), m_columnLayout.actionWidth);
    settings.setValue(settingsKey + QStringLiteral("/summaryWidth"), m_columnLayout.summaryWidth);
    settings.setValue(settingsKey + QStringLiteral("/durationWidth"), m_columnLayout.durationWidth);
    settings.setValue(settingsKey + QStringLiteral("/matchDurationWidth"),
                     m_columnLayout.matchDurationWidth);
    settings.setValue(settingsKey + QStringLiteral("/attemptsWidth"), m_columnLayout.attemptsWidth);
    settings.setValue(settingsKey + QStringLiteral("/returnPrevWidth"),
                     m_columnLayout.returnPrevWidth);
    settings.setValue(settingsKey + QStringLiteral("/retryWidth"), m_columnLayout.retryWidth);
    settings.setValue(settingsKey + QStringLiteral("/scoreWidth"), m_columnLayout.scoreWidth);
    settings.setValue(settingsKey + QStringLiteral("/roiCorrectionWidth"),
                     m_columnLayout.roiCorrectionWidth);
    settings.setValue(settingsKey + QStringLiteral("/matchWidth"), m_columnLayout.matchWidth);
    settings.setValue(settingsKey + QStringLiteral("/rowHeight"), m_columnLayout.rowHeight);
}

void BlockListWidget::restoreColumnLayout(const QSettings& settings, const QString& settingsKey) {
    if (!settings.contains(settingsKey + QStringLiteral("/indexWidth"))
        && !settings.contains(settingsKey + QStringLiteral("/rowHeight"))) {
        return;
    }
    auto readInt = [&](const QString& key, int fallback) {
        return settings.contains(key) ? settings.value(key).toInt() : fallback;
    };
    BlockListColumnLayout layout = m_columnLayout;
    layout.indexWidth = readInt(settingsKey + QStringLiteral("/indexWidth"), layout.indexWidth);
    layout.previewWidth = readInt(settingsKey + QStringLiteral("/previewWidth"), layout.previewWidth);
    layout.actionWidth = readInt(settingsKey + QStringLiteral("/actionWidth"), layout.actionWidth);
    layout.summaryWidth = readInt(settingsKey + QStringLiteral("/summaryWidth"), layout.summaryWidth);
    layout.durationWidth = readInt(settingsKey + QStringLiteral("/durationWidth"), layout.durationWidth);
    layout.matchDurationWidth =
        readInt(settingsKey + QStringLiteral("/matchDurationWidth"), layout.matchDurationWidth);
    layout.attemptsWidth = readInt(settingsKey + QStringLiteral("/attemptsWidth"), layout.attemptsWidth);
    layout.returnPrevWidth =
        readInt(settingsKey + QStringLiteral("/returnPrevWidth"), layout.returnPrevWidth);
    layout.retryWidth = readInt(settingsKey + QStringLiteral("/retryWidth"), layout.retryWidth);
    layout.scoreWidth = readInt(settingsKey + QStringLiteral("/scoreWidth"), layout.scoreWidth);
    layout.roiCorrectionWidth =
        readInt(settingsKey + QStringLiteral("/roiCorrectionWidth"), layout.roiCorrectionWidth);
    layout.matchWidth = readInt(settingsKey + QStringLiteral("/matchWidth"), layout.matchWidth);
    layout.rowHeight = readInt(settingsKey + QStringLiteral("/rowHeight"), layout.rowHeight);
    m_restoringColumnLayout = true;
    setColumnLayout(layout, false);
    m_restoringColumnLayout = false;
}

void BlockListWidget::wireListColumnHeader(ListColumnHeaderWidget* header, BlockListWidget* table) {
    if (!header || !table) {
        return;
    }

    table->m_columnHeader = header;

    header->setObjectName(QStringLiteral("blockListHeaderRow"));
    header->setToolTip(
        BlockListWidget::tr("헤더 구분선을 드래그해 열 너비·줄 높이를 조절합니다.\n"
                            "· 요약 열은 패널 크기에 맞춰 남는 너비를 채웁니다.\n"
                            "· 헤더 아래 가장자리: 줄 높이"));

    header->setRowHeightProvider([table] { return table->columnLayout().rowHeight; });

    header->setPaintLabelsProvider([table](ListColumnHeaderWidget::PaintContext& ctx) {
        const BlockListColumnEdges edges = blockListColumnEdgesFromTable(table, ctx.rect);
        QFont headerFont = ctx.painter->font();
        headerFont.setPointSize(qMax(headerFont.pointSize(), 9));
        headerFont.setBold(true);
        ctx.painter->setFont(headerFont);
        ctx.painter->setPen(ListColumnHeaderWidget::headerTextColor(ctx.palette));
        const Qt::Alignment align = Qt::AlignHCenter | Qt::AlignVCenter;
        ctx.painter->drawText(edges.cols.index, align, QStringLiteral("#"));
        ctx.painter->drawText(edges.cols.preview, align, QStringLiteral("◻"));
        ctx.painter->drawText(edges.cols.action, align, BlockListWidget::tr("동작"));
        ctx.painter->drawText(edges.cols.summary, align, QStringLiteral("요약"));
        ctx.painter->drawText(edges.cols.duration, align, BlockListWidget::tr("동작 시간"));
        ctx.painter->drawText(edges.cols.matchDuration, align, BlockListWidget::tr("매칭 시간"));
        ctx.painter->drawText(edges.cols.attempts, align, BlockListWidget::tr("시도 횟수"));
        ctx.painter->drawText(edges.cols.returnPrev, align, BlockListWidget::tr("이전 복귀"));
        ctx.painter->drawText(edges.cols.retry, align, BlockListWidget::tr("재시도"));
        ctx.painter->drawText(edges.cols.score, align, BlockListWidget::tr("기준/감지"));
        if (!table->isColumnHidden(kColRoiCorrection)) {
            ctx.painter->drawText(edges.cols.roiCorrection, align, BlockListWidget::tr("ROI 보정"));
        }
        ctx.painter->drawText(edges.cols.match, align, BlockListWidget::tr("매칭"));
    });

    header->setDividerXsProvider([table](const QRect& rect) {
        const BlockListColumnEdges edges = blockListColumnEdgesFromTable(table, rect);
        return blockListDividerXs(edges, !table->isColumnHidden(kColRoiCorrection));
    });

    header->setDividerHitProvider([table](const QPoint& pos, const QRect& rect) {
        const BlockListColumnEdges edges = blockListColumnEdgesFromTable(table, rect);
        return blockListDividerHandleAt(pos, edges, !table->isColumnHidden(kColRoiCorrection));
    });

    header->setApplyDragProvider([table, header](int handleId, int deltaX, int deltaY, const QPoint&) {
        if (handleId == -1) {
            BlockListColumnLayout layout = table->m_headerDragStartLayout;
            layout.rowHeight =
                UiResizeHandle::clampListRowHeight(table->m_headerDragStartLayout.rowHeight + deltaY);
            table->setColumnLayout(layout, true);
            header->syncToLayout();
            return;
        }

        if (handleId <= 0 || handleId >= table->columnCount()) {
            return;
        }
        table->applyPairwiseColumnResize(handleId, deltaX, table->m_headerDragStartLayout);
        header->syncToLayout();
    });

    QObject::connect(header, &ListColumnHeaderWidget::dividerDragStarted, table, [table]() {
        table->m_headerDragStartLayout = table->columnLayout();
        table->syncColumnLayoutFromTable(table->m_headerDragStartLayout);
    });

    QObject::connect(
        table,
        &BlockListWidget::columnLayoutChanged,
        header,
        &ListColumnHeaderWidget::syncToLayout);
}

void BlockListWidget::setRoiCorrectionColumnVisible(bool visible) {
    m_roiCorrectionColumnVisible = visible;
    if (QHeaderView* header = horizontalHeader()) {
        header->setSectionHidden(kColRoiCorrection, !visible);
    }
    applyColumnLayoutToTable(true);
}

void BlockListWidget::setBlockRoiCorrection(int row, bool enabled, bool interactive) {
    const int tableRow = tableRowForBlockRow(row);
    if (row < 0 || row >= m_blockCount || tableRow < 0) {
        return;
    }

    QTableWidgetItem* roiItem = item(tableRow, kColRoiCorrection);
    if (!roiItem) {
        return;
    }

    m_updatingRoiCorrectionItem = true;
    const QSignalBlocker blocker(this);
    const Qt::ItemFlags baseFlags = Qt::ItemIsEnabled | Qt::ItemIsSelectable | Qt::ItemIsDragEnabled;
    if (row >= static_cast<int>(m_rowIsImageFind.size()) || !m_rowIsImageFind[static_cast<size_t>(row)]) {
        roiItem->setFlags(baseFlags);
        roiItem->setText(QStringLiteral("—"));
        m_updatingRoiCorrectionItem = false;
        return;
    }

    roiItem->setText(QString());
    Qt::ItemFlags flags = baseFlags;
    if (interactive && m_roiCorrectionColumnVisible) {
        flags |= Qt::ItemIsUserCheckable;
    }
    roiItem->setFlags(flags);
    roiItem->setCheckState(enabled ? Qt::Checked : Qt::Unchecked);
    m_updatingRoiCorrectionItem = false;
}

void BlockListWidget::setBlockCount(int count) {
    m_blockCount = count;
    m_rowMatchHasScore = QVector<bool>(count, false);
    m_rowMatchSucceeded = QVector<bool>(count, false);
    m_rowMatchLockedSuccess = QVector<bool>(count, false);
    m_rowDisplayConfidences = QVector<double>(count, -1.0);
    m_rowIsImageFind = QVector<bool>(count, false);
    m_rowImageFindThresholds = QVector<double>(count, 0.85);
    m_rowImageFindAttemptCounts = QVector<int>(count, -1);
    m_rowImageFindReturnCounts = QVector<int>(count, -1);
    m_rowImageFindRetryCounts = QVector<int>(count, -1);
    m_rowCompletedHighlight = QVector<ExecutionHighlight>(count, ExecutionHighlight::None);
}

void BlockListWidget::rebuildTableRows() {
    clearSpans();
    m_tableRowBlockIndex.clear();
    m_tableRowRegionId.clear();
    m_tableRowMeta.clear();
    m_blockTableRow = QVector<int>(m_blockCount, -1);

    int extraRows = static_cast<int>(m_loopRegions.size());

    setRowCount(m_blockCount + extraRows);
    m_loopRegionMember = QVector<bool>(rowCount(), false);
    m_loopRegionStart = QVector<bool>(rowCount(), false);
    m_loopRegionEnd = QVector<bool>(rowCount(), false);
    m_loopRegionPickPreview = QVector<bool>(rowCount(), false);

    int tableRow = 0;
    for (int block = 0; block < m_blockCount; ++block) {
        for (const WorkflowLoopRegion& region : m_loopRegions) {
            if (region.startIndex != block) {
                continue;
            }
            m_tableRowBlockIndex.push_back(-1);
            m_tableRowRegionId.push_back(QString::fromStdString(region.id));
            BlockListRowMeta meta;
            meta.kind = BlockListRowKind::LoopRegionHeader;
            meta.loopRegionId = QString::fromStdString(region.id);
            m_tableRowMeta.push_back(meta);
            populateLoopRegionHeaderRow(tableRow, region);
            ++tableRow;
        }

        m_tableRowBlockIndex.push_back(block);
        m_tableRowRegionId.push_back(QString());
        BlockListRowMeta blockMeta;
        blockMeta.kind = BlockListRowKind::MainBlock;
        blockMeta.mainBlockRow = block;
        m_tableRowMeta.push_back(blockMeta);
        m_blockTableRow[block] = tableRow;
        setRowHeight(tableRow, m_blockRowHeight);
        ++tableRow;
    }
}

void BlockListWidget::populateLoopRegionHeaderRow(int tableRow, const WorkflowLoopRegion& region) {
    setRowHeight(tableRow, kLoopRegionHeaderHeight);
    const Qt::ItemFlags headerFlags = Qt::ItemIsEnabled | Qt::ItemIsSelectable;

    auto* labelItem = new QTableWidgetItem();
    labelItem->setFlags(headerFlags);
    setItem(tableRow, 0, labelItem);

    const QString regionId = QString::fromStdString(region.id);
    attachChipHeaderRowActions(
        this,
        tableRow,
        palette().color(QPalette::Highlight),
        QStringLiteral("↻"),
        loopRegionHeaderTitle(),
        loopRegionHeaderCondition(region),
        tr("구간 반복 편집"),
        tr("구간 반복 삭제"),
        [this, regionId]() { emit loopRegionEditRequested(regionId); },
        [this, regionId]() { emit loopRegionDeleteRequested(regionId); });
}

const BlockListRowMeta& BlockListWidget::rowMeta(int tableRow) const {
    static const BlockListRowMeta kEmpty;
    if (tableRow < 0 || tableRow >= m_tableRowMeta.size()) {
        return kEmpty;
    }
    return m_tableRowMeta[tableRow];
}

BlockListRowKind BlockListWidget::tableRowKind(int tableRow) const {
    return rowMeta(tableRow).kind;
}

int BlockListWidget::blockRowForTableRow(int tableRow) const {
    if (tableRow < 0 || tableRow >= m_tableRowBlockIndex.size()) {
        return -1;
    }
    return m_tableRowBlockIndex[tableRow];
}

int BlockListWidget::tableRowForBlockRow(int blockRow) const {
    if (blockRow < 0 || blockRow >= m_blockTableRow.size()) {
        return -1;
    }
    return m_blockTableRow[blockRow];
}

QString BlockListWidget::loopRegionIdForTableRow(int tableRow) const {
    if (tableRow < 0 || tableRow >= m_tableRowRegionId.size()) {
        return {};
    }
    return m_tableRowRegionId[tableRow];
}

void BlockListWidget::selectBlockRow(int blockRow) {
    const int tableRow = tableRowForBlockRow(blockRow);
    if (tableRow >= 0) {
        selectRow(tableRow);
    }
}

int BlockListWidget::blockRowAtViewportY(int viewportY) const {
    return blockRowForTableRow(rowAtViewportY(viewportY));
}

int BlockListWidget::blockRowForDropInsertion(int insertionIndex) const {
    if (m_blockCount <= 0) {
        return 0;
    }
    insertionIndex = qBound(0, insertionIndex, rowCount());
    for (int tableRow = insertionIndex; tableRow < rowCount(); ++tableRow) {
        const int blockRow = blockRowForTableRow(tableRow);
        if (blockRow >= 0) {
            return blockRow;
        }
    }
    return m_blockCount - 1;
}

void BlockListWidget::setLoopRegions(const std::vector<WorkflowLoopRegion>& regions) {
    m_loopRegions = regions;
    rebuildTableRows();
    for (const WorkflowLoopRegion& region : regions) {
        for (int block = region.startIndex; block <= region.endIndex && block < m_blockCount; ++block) {
            if (block < 0) {
                continue;
            }
            const int tableRow = tableRowForBlockRow(block);
            if (tableRow < 0) {
                continue;
            }
            m_loopRegionMember[tableRow] = true;
            if (block == region.endIndex) {
                m_loopRegionEnd[tableRow] = true;
            }
        }
    }
    if (m_loopRegionChrome) {
        static_cast<LoopRegionChromeOverlay*>(m_loopRegionChrome)->setRegions(m_loopRegions);
    }
    updateLoopRegionChrome();
    applyActiveRowVisuals();
}

void BlockListWidget::updateLoopRegionChrome() {
    if (!m_loopRegionChrome || !viewport()) {
        return;
    }
    m_loopRegionChrome->setGeometry(viewport()->rect());
    m_loopRegionChrome->raise();
    if (m_dropIndicator && m_dropIndicator->isVisible()) {
        m_dropIndicator->raise();
    }
    m_loopRegionChrome->update();
}

QString BlockListWidget::loopRegionIdAtViewportPos(const QPoint& viewportPos) const {
    const int tableRow = rowAt(viewportPos.y());
    return loopRegionIdForTableRow(tableRow);
}

void BlockListWidget::clearLoopRegionVisuals() {
    m_loopRegions.clear();
    m_loopRegionMember.clear();
    m_loopRegionStart.clear();
    m_loopRegionEnd.clear();
    m_loopRegionPickPreview.clear();
    m_tableRowBlockIndex.clear();
    m_tableRowRegionId.clear();
    m_tableRowMeta.clear();
    m_blockTableRow.clear();
    if (m_loopRegionChrome) {
        static_cast<LoopRegionChromeOverlay*>(m_loopRegionChrome)->setRegions({});
    }
}

int BlockListWidget::rowAtViewportY(int viewportY) const {
    if (rowCount() <= 0) {
        return -1;
    }
    int row = rowAt(viewportY);
    if (row >= 0) {
        return row;
    }
    if (viewportY < 0) {
        return 0;
    }
    return rowCount() - 1;
}

void BlockListWidget::updateLoopRegionPickPreview() {
    m_loopRegionPickPreview = QVector<bool>(rowCount(), false);
    if (!m_loopRegionPickDragging || m_loopRegionPickAnchorRow < 0) {
        return;
    }
    const int startBlock = qMin(m_loopRegionPickAnchorRow, m_loopRegionPickCurrentRow);
    const int endBlock = qMax(m_loopRegionPickAnchorRow, m_loopRegionPickCurrentRow);
    for (int tableRow = 0; tableRow < rowCount(); ++tableRow) {
        const int blockRow = blockRowForTableRow(tableRow);
        if (blockRow >= startBlock && blockRow <= endBlock) {
            m_loopRegionPickPreview[tableRow] = true;
        }
    }
}

void BlockListWidget::setLoopRegionPickMode(bool active) {
    if (m_loopRegionPickActive == active) {
        return;
    }
    m_loopRegionPickActive = active;
    cancelLoopRegionPick();
    if (active) {
        m_dragAutoScroll->begin();
    } else {
        m_dragAutoScroll->end();
    }
    setDragEnabled(active ? false : m_reorderEnabled);
    setAcceptDrops(active ? false : m_reorderEnabled);
    setDragDropMode((active || !m_reorderEnabled) ? QAbstractItemView::NoDragDrop
                                                  : QAbstractItemView::InternalMove);
    const Qt::CursorShape cursorShape = active ? Qt::CrossCursor : Qt::ArrowCursor;
    setCursor(cursorShape);
    viewport()->setCursor(cursorShape);
    setToolTip(active ? tr("블록을 드래그하여 반복 구간을 선택하세요. Esc로 취소.") : m_defaultToolTip);
    applyActiveRowVisuals();
}

void BlockListWidget::cancelLoopRegionPick() {
    m_loopRegionPickDragging = false;
    m_loopRegionPickAnchorRow = -1;
    m_loopRegionPickCurrentRow = -1;
    m_loopRegionPickPreview.clear();
    if (m_loopRegionPickActive) {
        applyActiveRowVisuals();
    }
}

bool BlockListWidget::imageFindScoreColumnAt(const QPoint& viewportPos, int& blockRowOut) const {
    const int tableRow = rowAtViewportY(viewportPos.y());
    const int blockRow = blockRowForTableRow(tableRow);
    if (blockRow < 0 || blockRow >= m_rowIsImageFind.size() || !m_rowIsImageFind[blockRow]) {
        return false;
    }

    constexpr int kHorizontalSlack = UiResizeHandle::kDividerHalfWidthPx;
    const QModelIndex scoreIndex = model()->index(tableRow, kColScore);
    if (!scoreIndex.isValid()) {
        return false;
    }
    const QRect hitRect = visualRect(scoreIndex).adjusted(-kHorizontalSlack, 0, kHorizontalSlack, 0);
    if (!hitRect.contains(viewportPos)) {
        return false;
    }

    blockRowOut = blockRow;
    return true;
}

double BlockListWidget::imageFindThresholdDragStep(Qt::KeyboardModifiers modifiers) const {
    constexpr double kBaseStep = 0.01;
    if (modifiers.testFlag(Qt::ControlModifier)) {
        return kBaseStep * 100.0;
    }
    if (modifiers.testFlag(Qt::ShiftModifier)) {
        return kBaseStep * 10.0;
    }
    return kBaseStep;
}

void BlockListWidget::updateImageFindThresholdDisplay(int blockRow, double threshold) {
    if (blockRow < 0 || blockRow >= m_blockCount) {
        return;
    }
    const double snapped = std::round(std::clamp(threshold, 0.1, 1.0) * 100.0) / 100.0;
    if (blockRow < m_rowImageFindThresholds.size()) {
        m_rowImageFindThresholds[blockRow] = snapped;
    }

    const int tableRow = tableRowForBlockRow(blockRow);
    if (tableRow < 0) {
        return;
    }
    QTableWidgetItem* scoreItem = item(tableRow, kColScore);
    if (!scoreItem) {
        return;
    }

    if (blockRow < m_rowDisplayConfidences.size() && m_rowDisplayConfidences[blockRow] >= 0.0) {
        scoreItem->setText(QStringLiteral("%1 / %2")
                               .arg(snapped, 0, 'f', 2)
                               .arg(m_rowDisplayConfidences[blockRow], 0, 'f', 2));
    } else {
        scoreItem->setText(QStringLiteral("%1 / —").arg(snapped, 0, 'f', 2));
    }
}

void BlockListWidget::finishThresholdDrag(QMouseEvent* mouseEvent) {
    const DragAdjustReleaseResult result =
        dragAdjustFinishRelease(m_thresholdDragMouse, mouseEvent, viewport());
    if (result == DragAdjustReleaseResult::NotHandled) {
        return;
    }

    viewport()->unsetCursor();
    const int blockRow = m_thresholdDragBlockRow;
    m_thresholdDragBlockRow = -1;

    if (blockRow < 0 || blockRow >= m_rowImageFindThresholds.size()) {
        return;
    }

    const double finalThreshold = m_rowImageFindThresholds[blockRow];
    if (result == DragAdjustReleaseResult::DragFinished
        && std::abs(finalThreshold - m_thresholdDragStartValue) > 0.0005) {
        emit imageFindThresholdChanged(blockRow, finalThreshold);
    }
}

bool BlockListWidget::eventFilter(QObject* watched, QEvent* event) {
    if (event->type() == QEvent::Wheel && m_dragAutoScroll && m_dragAutoScroll->isActive()
        && !m_loopRegionPickActive) {
        auto* wheelEvent = static_cast<QWheelEvent*>(event);
        if (m_dragAutoScroll->handleWheel(wheelEvent)) {
            return true;
        }
    }

    if (watched == viewport()) {
        switch (event->type()) {
        case QEvent::MouseMove: {
            auto* mouseEvent = static_cast<QMouseEvent*>(event);
            if (m_loopRegionPickActive) {
                m_dragAutoScroll->updateFromGlobalCursor();
            }
            const int tableRow = rowAt(mouseEvent->pos().y());
            updateHoverTableRow(tableRow);
            break;
        }
        case QEvent::Leave:
            updateHoverTableRow(-1);
            break;
        default:
            break;
        }
    }

    if (watched == viewport() && m_loopRegionPickActive && rowCount() > 0) {
        switch (event->type()) {
        case QEvent::MouseButtonPress: {
            auto* mouseEvent = static_cast<QMouseEvent*>(event);
            if (mouseEvent->button() != Qt::LeftButton) {
                break;
            }
            const int blockRow = blockRowAtViewportY(mouseEvent->pos().y());
            if (blockRow < 0) {
                break;
            }
            m_loopRegionPickDragging = true;
            m_loopRegionPickAnchorRow = blockRow;
            m_loopRegionPickCurrentRow = blockRow;
            updateLoopRegionPickPreview();
            applyActiveRowVisuals();
            mouseEvent->accept();
            return true;
        }
        case QEvent::MouseMove: {
            if (!m_loopRegionPickDragging) {
                break;
            }
            auto* mouseEvent = static_cast<QMouseEvent*>(event);
            m_loopRegionPickCurrentRow = blockRowAtViewportY(mouseEvent->pos().y());
            m_dragAutoScroll->updateFromGlobalCursor();
            updateLoopRegionPickPreview();
            applyActiveRowVisuals();
            mouseEvent->accept();
            return true;
        }
        case QEvent::MouseButtonRelease: {
            auto* mouseEvent = static_cast<QMouseEvent*>(event);
            if (mouseEvent->button() != Qt::LeftButton || !m_loopRegionPickDragging) {
                break;
            }
            m_loopRegionPickCurrentRow = blockRowAtViewportY(mouseEvent->pos().y());
            const int startRow = qMin(m_loopRegionPickAnchorRow, m_loopRegionPickCurrentRow);
            const int endRow = qMax(m_loopRegionPickAnchorRow, m_loopRegionPickCurrentRow);
            m_loopRegionPickDragging = false;
            m_loopRegionPickAnchorRow = -1;
            m_loopRegionPickCurrentRow = -1;
            m_loopRegionPickPreview.clear();
            applyActiveRowVisuals();
            if (startRow >= 0 && endRow >= 0) {
                emit loopRegionRangePicked(startRow, endRow);
            }
            mouseEvent->accept();
            return true;
        }
        default:
            break;
        }
    }

    if (watched == viewport() && m_reorderEnabled && !m_loopRegionPickActive) {
        switch (event->type()) {
        case QEvent::MouseButtonPress: {
            auto* mouseEvent = static_cast<QMouseEvent*>(event);
            int blockRow = -1;
            if (!imageFindScoreColumnAt(mouseEvent->pos(), blockRow)) {
                break;
            }
            if (!dragAdjustBeginPress(m_thresholdDragMouse, mouseEvent)) {
                break;
            }
            m_thresholdDragBlockRow = blockRow;
            m_thresholdDragStartValue = blockRow < m_rowImageFindThresholds.size()
                                            ? m_rowImageFindThresholds[blockRow]
                                            : 0.85;
            viewport()->setCursor(Qt::SizeHorCursor);
            mouseEvent->accept();
            return true;
        }
        case QEvent::MouseMove: {
            auto* mouseEvent = static_cast<QMouseEvent*>(event);
            if (m_thresholdDragBlockRow >= 0) {
                if (!dragAdjustContinueMove(m_thresholdDragMouse, mouseEvent, viewport())) {
                    break;
                }
                const int deltaPx = dragAdjustDeltaPx(m_thresholdDragMouse, mouseEvent);
                const double step = imageFindThresholdDragStep(mouseEvent->modifiers());
                const double nextValue = m_thresholdDragStartValue + static_cast<double>(deltaPx) * step;
                updateImageFindThresholdDisplay(m_thresholdDragBlockRow, nextValue);
                mouseEvent->accept();
                return true;
            }
            int hoverBlockRow = -1;
            if (imageFindScoreColumnAt(mouseEvent->pos(), hoverBlockRow)) {
                viewport()->setCursor(Qt::SizeHorCursor);
            } else {
                viewport()->unsetCursor();
            }
            break;
        }
        case QEvent::MouseButtonRelease: {
            auto* mouseEvent = static_cast<QMouseEvent*>(event);
            if (m_thresholdDragBlockRow < 0) {
                break;
            }
            finishThresholdDrag(mouseEvent);
            mouseEvent->accept();
            return true;
        }
        default:
            break;
        }
    }

    return QTableWidget::eventFilter(watched, event);
}

void BlockListWidget::lockMatchSuccess(int row) {
    if (row < 0 || row >= m_blockCount) {
        return;
    }
    if (row >= m_rowMatchLockedSuccess.size()) {
        m_rowMatchLockedSuccess.resize(m_blockCount, false);
        m_rowMatchSucceeded.resize(m_blockCount, false);
    }
    m_rowMatchLockedSuccess[row] = true;
    m_rowMatchSucceeded[row] = true;
}

void BlockListWidget::markRowCompleted(int row, ExecutionHighlight highlight) {
    if (row < 0 || row >= m_blockCount || highlight == ExecutionHighlight::None) {
        return;
    }
    if (row >= m_rowCompletedHighlight.size()) {
        m_rowCompletedHighlight.resize(m_blockCount, ExecutionHighlight::None);
    }

    m_rowCompletedHighlight[row] = highlight;
    if (highlight == ExecutionHighlight::Success) {
        triggerRowFlash(row, highlight, kSuccessFlashMs);
    } else if (highlight == ExecutionHighlight::Failed) {
        triggerRowFlash(row, highlight, kFailedFlashMs);
    }
}

bool BlockListWidget::isMatchSuccessLocked(int row) const {
    return row >= 0 && row < m_rowMatchLockedSuccess.size() && m_rowMatchLockedSuccess[row];
}

void BlockListWidget::commitRowSuccess(int row) {
    if (row < 0 || row >= m_blockCount) {
        return;
    }
    lockMatchSuccess(row);
    markRowCompleted(row, ExecutionHighlight::Success);
    applyActiveRowVisuals();
}

BlockListWidget::ExecutionHighlight BlockListWidget::rowVisualHighlight(int row) const {
    if (m_flashKind == ExecutionHighlight::ReturnToPrevious && m_flashIntensity > 0.0
        && (row == m_flashRow || row == m_flashRowSecondary)) {
        return ExecutionHighlight::ReturnToPrevious;
    }
    if (row == m_activeRow && m_activeHighlight != ExecutionHighlight::None) {
        if (m_activeHighlight == ExecutionHighlight::Running) {
            return ExecutionHighlight::Running;
        }
        if (m_activeHighlight == ExecutionHighlight::TriggerWatch) {
            return ExecutionHighlight::TriggerWatch;
        }
        if (m_activeHighlight == ExecutionHighlight::TriggerCooldown) {
            return ExecutionHighlight::TriggerCooldown;
        }
        if (m_activeHighlight == ExecutionHighlight::ImageFindMiss) {
            if (m_flashRow == row && m_flashKind == ExecutionHighlight::ImageFindMiss
                && m_flashIntensity > 0.0) {
                return ExecutionHighlight::ImageFindMiss;
            }
            return ExecutionHighlight::None;
        }
        if (m_flashRow == row && m_flashKind == m_activeHighlight && m_flashIntensity > 0.0) {
            return m_activeHighlight;
        }
        return ExecutionHighlight::None;
    }
    if (row >= 0 && row < m_rowCompletedHighlight.size()) {
        const ExecutionHighlight completed = m_rowCompletedHighlight[row];
        if (completed != ExecutionHighlight::None && m_flashRow == row && m_flashKind == completed
            && m_flashIntensity > 0.0) {
            return completed;
        }
    }
    return ExecutionHighlight::None;
}

void BlockListWidget::setBlockInfo(int row,
                                   const QString& type,
                                   const QString& summary,
                                   const QPixmap& thumbnail) {
    const int tableRow = tableRowForBlockRow(row);
    if (row < 0 || row >= m_blockCount || tableRow < 0) {
        return;
    }
    const auto itemFlags = Qt::ItemIsEnabled | Qt::ItemIsSelectable | Qt::ItemIsDragEnabled;

    auto* indexItem = new QTableWidgetItem(QString::number(row + 1));
    indexItem->setTextAlignment(Qt::AlignCenter);
    indexItem->setFlags(itemFlags);
    setItem(tableRow, 0, indexItem);

    auto* thumbnailItem = new QTableWidgetItem();
    thumbnailItem->setTextAlignment(Qt::AlignCenter);
    thumbnailItem->setFlags(itemFlags);
    if (!thumbnail.isNull()) {
        thumbnailItem->setIcon(QIcon(thumbnail));
    }
    setItem(tableRow, 1, thumbnailItem);

    auto* typeItem = new QTableWidgetItem(type);
    typeItem->setTextAlignment(Qt::AlignCenter);
    typeItem->setFlags(itemFlags);
    setItem(tableRow, 2, typeItem);

    auto* roiItem = new QTableWidgetItem();
    roiItem->setTextAlignment(Qt::AlignCenter);
    roiItem->setFlags(itemFlags);
    roiItem->setToolTip(tr("두 번째 루프부터 매칭 위치 기준 템플릿보다 약 10% 넓은 영역만 탐색합니다."));
    setItem(tableRow, kColRoiCorrection, roiItem);

    auto* summaryItem = new QTableWidgetItem(summary);
    summaryItem->setFlags(itemFlags);
    setItem(tableRow, kColSummary, summaryItem);

    auto* durationItem = new QTableWidgetItem(QStringLiteral("—"));
    durationItem->setTextAlignment(Qt::AlignCenter);
    durationItem->setFlags(itemFlags);
    durationItem->setToolTip(tr("블록 시작부터 완료까지 걸린 시간"));
    setItem(tableRow, kColDuration, durationItem);

    auto* matchDurationItem = new QTableWidgetItem(QStringLiteral("—"));
    matchDurationItem->setTextAlignment(Qt::AlignCenter);
    matchDurationItem->setFlags(itemFlags);
    matchDurationItem->setToolTip(tr("화면 캡처 및 템플릿 매칭에 소요된 시간 (재시도 대기 제외)"));
    setItem(tableRow, kColMatchDuration, matchDurationItem);

    auto* attemptsItem = new QTableWidgetItem(QStringLiteral("—"));
    attemptsItem->setTextAlignment(Qt::AlignCenter);
    attemptsItem->setFlags(itemFlags);
    attemptsItem->setToolTip(tr("템플릿 매칭 감지 시도 횟수"));
    setItem(tableRow, kColAttempts, attemptsItem);

    auto* returnPrevItem = new QTableWidgetItem(QStringLiteral("—"));
    returnPrevItem->setTextAlignment(Qt::AlignCenter);
    returnPrevItem->setFlags(itemFlags);
    returnPrevItem->setToolTip(
        tr("못 찾으면 이전 템플릿 찾기로 돌아감 — 실행 중 발동 횟수"));
    setItem(tableRow, kColReturnPrev, returnPrevItem);

    auto* retryAfterNextItem = new QTableWidgetItem(QStringLiteral("—"));
    retryAfterNextItem->setTextAlignment(Qt::AlignCenter);
    retryAfterNextItem->setFlags(itemFlags);
    retryAfterNextItem->setToolTip(
        tr("못 찾으면 → 아래 블록 1회 → 재찾기 → (또 실패 시) 다음/이전 찾기 — 실행 중 발동 횟수"));
    setItem(tableRow, kColRetryAfterNext, retryAfterNextItem);

    auto* scoreItem = new QTableWidgetItem(QStringLiteral("—"));
    scoreItem->setTextAlignment(Qt::AlignCenter);
    scoreItem->setFlags(itemFlags);
    setItem(tableRow, kColScore, scoreItem);

    if (row >= m_rowIsImageFind.size()) {
        m_rowIsImageFind.resize(m_blockCount, false);
        m_rowImageFindThresholds.resize(m_blockCount, 0.85);
        m_rowDisplayConfidences.resize(m_blockCount, -1.0);
    }
    m_rowIsImageFind[row] = (type == blockTypeWorkflowActionName(BlockType::ImageFind));
    if (!m_rowIsImageFind[row]) {
        roiItem->setText(QStringLiteral("—"));
    }
    if (m_rowIsImageFind[row]) {
        scoreItem->setToolTip(tr("드래그하여 매칭 임계값 조정"));
    }

    auto* matchItem = new QTableWidgetItem();
    matchItem->setTextAlignment(Qt::AlignCenter);
    matchItem->setFlags(itemFlags);
    setItem(tableRow, kColMatch, matchItem);
}

void BlockListWidget::setBlockMatchResult(int row,
                                          double matchThreshold,
                                          double confidence,
                                          const QPixmap& matchedImage,
                                          bool matched) {
    const int tableRow = tableRowForBlockRow(row);
    if (row < 0 || row >= m_blockCount || tableRow < 0) {
        return;
    }

    if (!matched && isMatchSuccessLocked(row)) {
        return;
    }

    QTableWidgetItem* scoreItem = item(tableRow, kColScore);
    if (!scoreItem) {
        scoreItem = new QTableWidgetItem();
        scoreItem->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable | Qt::ItemIsDragEnabled);
        scoreItem->setTextAlignment(Qt::AlignCenter);
        setItem(tableRow, kColScore, scoreItem);
    }
    scoreItem->setText(QStringLiteral("%1 / %2")
                           .arg(matchThreshold, 0, 'f', 2)
                           .arg(confidence, 0, 'f', 2));
    scoreItem->setToolTip(tr("매칭 기준 / 감지 점수 · 드래그하여 기준 조정"));
    if (row < m_rowImageFindThresholds.size()) {
        m_rowImageFindThresholds[row] = matchThreshold;
    }
    if (row < m_rowDisplayConfidences.size()) {
        m_rowDisplayConfidences[row] = confidence;
    }

    if (row >= m_rowMatchHasScore.size()) {
        m_rowMatchHasScore.resize(m_blockCount, false);
        m_rowMatchSucceeded.resize(m_blockCount, false);
    }
    m_rowMatchHasScore[row] = true;
    m_rowMatchSucceeded[row] = matched;

    if (matched) {
        lockMatchSuccess(row);
        setActiveRow(row, ExecutionHighlight::Success);
    }

    QTableWidgetItem* matchItem = item(tableRow, kColMatch);
    if (!matchItem) {
        matchItem = new QTableWidgetItem();
        matchItem->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable | Qt::ItemIsDragEnabled);
        matchItem->setTextAlignment(Qt::AlignCenter);
        setItem(tableRow, kColMatch, matchItem);
    }
    if (matchedImage.isNull()) {
        matchItem->setIcon({});
    } else {
        matchItem->setIcon(QIcon(matchedImage.scaled(kThumbnailSize,
                                                   kThumbnailSize,
                                                   Qt::KeepAspectRatio,
                                                   Qt::SmoothTransformation)));
    }

    applyActiveRowVisuals();
}

void BlockListWidget::setBlockMatchBaseline(int row, double matchThreshold) {
    const int tableRow = tableRowForBlockRow(row);
    if (row < 0 || row >= m_blockCount || tableRow < 0) {
        return;
    }

    QTableWidgetItem* scoreItem = item(tableRow, kColScore);
    if (!scoreItem) {
        scoreItem = new QTableWidgetItem();
        scoreItem->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable | Qt::ItemIsDragEnabled);
        scoreItem->setTextAlignment(Qt::AlignCenter);
        setItem(tableRow, kColScore, scoreItem);
    }
    scoreItem->setText(QStringLiteral("%1 / —").arg(matchThreshold, 0, 'f', 2));
    scoreItem->setToolTip(tr("매칭 기준 / 감지 점수 · 드래그하여 기준 조정"));
    if (row < m_rowImageFindThresholds.size()) {
        m_rowImageFindThresholds[row] = matchThreshold;
    }
    if (row < m_rowDisplayConfidences.size()) {
        m_rowDisplayConfidences[row] = -1.0;
    }
    if (row < m_rowMatchHasScore.size()) {
        m_rowMatchHasScore[row] = false;
    }

    applyActiveRowVisuals();
}

void BlockListWidget::setBlockDuration(int row, qint64 durationMs) {
    const int tableRow = tableRowForBlockRow(row);
    if (row < 0 || row >= m_blockCount || tableRow < 0) {
        return;
    }

    QTableWidgetItem* durationItem = item(tableRow, kColDuration);
    if (!durationItem) {
        durationItem = new QTableWidgetItem();
        durationItem->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable | Qt::ItemIsDragEnabled);
        durationItem->setTextAlignment(Qt::AlignCenter);
        durationItem->setToolTip(tr("블록 시작부터 완료까지 걸린 시간"));
        setItem(tableRow, kColDuration, durationItem);
    }
    durationItem->setText(tr("%1 ms").arg(durationMs));
}

void BlockListWidget::setBlockImageFindMatchDuration(int row, qint64 matchDurationMs) {
    const int tableRow = tableRowForBlockRow(row);
    if (row < 0 || row >= m_blockCount || tableRow < 0) {
        return;
    }

    QTableWidgetItem* matchDurationItem = item(tableRow, kColMatchDuration);
    if (!matchDurationItem) {
        matchDurationItem = new QTableWidgetItem();
        matchDurationItem->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable | Qt::ItemIsDragEnabled);
        matchDurationItem->setTextAlignment(Qt::AlignCenter);
        matchDurationItem->setToolTip(tr("화면 캡처 및 템플릿 매칭에 소요된 시간 (재시도 대기 제외)"));
        setItem(tableRow, kColMatchDuration, matchDurationItem);
    }
    if (matchDurationMs < 0) {
        matchDurationItem->setText(QStringLiteral("—"));
    } else {
        matchDurationItem->setText(tr("%1 ms").arg(matchDurationMs));
    }
}

void BlockListWidget::setBlockImageFindAttemptCount(int row, int attemptCount) {
    const int tableRow = tableRowForBlockRow(row);
    if (row < 0 || row >= m_blockCount || tableRow < 0) {
        return;
    }
    if (row < m_rowImageFindAttemptCounts.size()) {
        m_rowImageFindAttemptCounts[row] = attemptCount;
    }

    QTableWidgetItem* attemptsItem = item(tableRow, kColAttempts);
    if (!attemptsItem) {
        attemptsItem = new QTableWidgetItem();
        attemptsItem->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable | Qt::ItemIsDragEnabled);
        attemptsItem->setTextAlignment(Qt::AlignCenter);
        attemptsItem->setToolTip(tr("템플릿 매칭 감지 시도 횟수"));
        setItem(tableRow, kColAttempts, attemptsItem);
    }
    if (row < m_rowIsImageFind.size() && m_rowIsImageFind[row] && attemptCount > 0) {
        attemptsItem->setText(QString::number(attemptCount));
    } else {
        attemptsItem->setText(QStringLiteral("—"));
    }
}

void BlockListWidget::setBlockImageFindFailureHandlingCounts(int row,
                                                           int returnToPreviousCount,
                                                           int retryAfterNextCount) {
    const int tableRow = tableRowForBlockRow(row);
    if (row < 0 || row >= m_blockCount || tableRow < 0) {
        return;
    }
    if (row < m_rowImageFindReturnCounts.size()) {
        m_rowImageFindReturnCounts[row] = returnToPreviousCount;
    }
    if (row < m_rowImageFindRetryCounts.size()) {
        m_rowImageFindRetryCounts[row] = retryAfterNextCount;
    }

    QTableWidgetItem* returnPrevItem = item(tableRow, kColReturnPrev);
    if (!returnPrevItem) {
        returnPrevItem = new QTableWidgetItem();
        returnPrevItem->setTextAlignment(Qt::AlignCenter);
        returnPrevItem->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable | Qt::ItemIsDragEnabled);
        returnPrevItem->setToolTip(
            tr("못 찾으면 이전 템플릿 찾기로 돌아감 — 실행 중 발동 횟수"));
        setItem(tableRow, kColReturnPrev, returnPrevItem);
    }
    QTableWidgetItem* retryItem = item(tableRow, kColRetryAfterNext);
    if (!retryItem) {
        retryItem = new QTableWidgetItem();
        retryItem->setTextAlignment(Qt::AlignCenter);
        retryItem->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable | Qt::ItemIsDragEnabled);
        retryItem->setToolTip(
            tr("못 찾으면 → 다음 블록 1회 실행 → 재찾기 / 다음 찾기로 넘어감 — 실행 중 발동 횟수"));
        setItem(tableRow, kColRetryAfterNext, retryItem);
    }

    const bool isImageFind = row < m_rowIsImageFind.size() && m_rowIsImageFind[row];
    if (isImageFind && returnToPreviousCount > 0) {
        returnPrevItem->setText(QString::number(returnToPreviousCount));
    } else {
        returnPrevItem->setText(QStringLiteral("—"));
    }
    if (isImageFind && retryAfterNextCount > 0) {
        retryItem->setText(QString::number(retryAfterNextCount));
    } else {
        retryItem->setText(QStringLiteral("—"));
    }
}

void BlockListWidget::clearBlockMatchResults() {
    m_rowMatchHasScore.fill(false);
    m_rowMatchSucceeded.fill(false);
    m_rowMatchLockedSuccess.fill(false);
    m_rowCompletedHighlight.fill(ExecutionHighlight::None);
    cancelReturnFlashHold();
    if (m_flashAnimation) {
        m_flashAnimation->stop();
    }
    m_flashRow = -1;
    m_flashRowSecondary = -1;
    m_flashKind = ExecutionHighlight::None;
    m_flashIntensity = 0.0;
    for (int row = 0; row < m_blockCount; ++row) {
        const int tableRow = tableRowForBlockRow(row);
        if (tableRow < 0) {
            continue;
        }
        if (QTableWidgetItem* durationItem = item(tableRow, kColDuration)) {
            durationItem->setText(QStringLiteral("—"));
        }
        if (QTableWidgetItem* matchDurationItem = item(tableRow, kColMatchDuration)) {
            matchDurationItem->setText(QStringLiteral("—"));
        }
        if (QTableWidgetItem* attemptsItem = item(tableRow, kColAttempts)) {
            attemptsItem->setText(QStringLiteral("—"));
        }
        if (QTableWidgetItem* returnPrevItem = item(tableRow, kColReturnPrev)) {
            returnPrevItem->setText(QStringLiteral("—"));
        }
        if (QTableWidgetItem* retryItem = item(tableRow, kColRetryAfterNext)) {
            retryItem->setText(QStringLiteral("—"));
        }
        if (row < m_rowImageFindAttemptCounts.size()) {
            m_rowImageFindAttemptCounts[row] = -1;
        }
        if (row < m_rowImageFindReturnCounts.size()) {
            m_rowImageFindReturnCounts[row] = -1;
        }
        if (row < m_rowImageFindRetryCounts.size()) {
            m_rowImageFindRetryCounts[row] = -1;
        }
        if (QTableWidgetItem* scoreItem = item(tableRow, kColScore)) {
            scoreItem->setText(QStringLiteral("—"));
        }
        if (QTableWidgetItem* matchItem = item(tableRow, kColMatch)) {
            matchItem->setIcon({});
        }
    }
}

void BlockListWidget::setReorderEnabled(bool enabled) {
    m_reorderEnabled = enabled;
    setDragEnabled(enabled);
    setAcceptDrops(enabled);
    setDragDropMode(enabled ? QAbstractItemView::InternalMove : QAbstractItemView::NoDragDrop);

    for (int tableRow = 0; tableRow < rowCount(); ++tableRow) {
        const BlockListRowKind kind = tableRowKind(tableRow);
        if (kind != BlockListRowKind::LoopRegionHeader) {
            continue;
        }
        if (QWidget* widget = cellWidget(tableRow, 0)) {
            widget->setEnabled(enabled);
        }
    }
}

void BlockListWidget::setActiveRow(int row, ExecutionHighlight highlight) {
    const ExecutionHighlight effectiveHighlight = row >= 0 ? highlight : ExecutionHighlight::None;

    if (m_activeRow >= 0 && row != m_activeRow) {
        if (m_activeHighlight == ExecutionHighlight::Success
            || m_activeHighlight == ExecutionHighlight::Failed) {
            markRowCompleted(m_activeRow, m_activeHighlight);
        } else if (isMatchSuccessLocked(m_activeRow)) {
            markRowCompleted(m_activeRow, ExecutionHighlight::Success);
        }
    }

    if (row == m_activeRow && effectiveHighlight == m_activeHighlight) {
        return;
    }
    m_activeRow = row;
    m_activeHighlight = effectiveHighlight;
    if (effectiveHighlight == ExecutionHighlight::Success && row >= 0) {
        lockMatchSuccess(row);
        triggerRowFlash(row, ExecutionHighlight::Success, kSuccessFlashMs);
    } else if (effectiveHighlight == ExecutionHighlight::Failed && row >= 0) {
        triggerRowFlash(row, ExecutionHighlight::Failed, kFailedFlashMs);
    }
    syncAmbientAnimationTimer();
    applyActiveRowVisuals();
}

void BlockListWidget::syncAmbientAnimationTimer() {
    const bool needsAmbient = m_activeHighlight == ExecutionHighlight::TriggerWatch
        || m_activeHighlight == ExecutionHighlight::TriggerCooldown;
    if (!m_ambientAnimTimer) {
        m_ambientAnimTimer = new QTimer(this);
        m_ambientAnimTimer->setInterval(50);
        connect(m_ambientAnimTimer, &QTimer::timeout, this, [this]() {
            m_ambientAnimPhase = (m_ambientAnimPhase + 1) % 120;
            applyActiveRowVisuals();
        });
    }
    if (needsAmbient) {
        if (!m_ambientAnimTimer->isActive()) {
            m_ambientAnimTimer->start();
        }
    } else {
        m_ambientAnimTimer->stop();
        m_ambientAnimPhase = 0;
    }
}

void BlockListWidget::notifyImageFindRetry(int row) {
    if (row < 0 || row >= m_blockCount) {
        return;
    }

    if (isMatchSuccessLocked(row)) {
        return;
    }
    if (row < m_rowCompletedHighlight.size()
        && m_rowCompletedHighlight[row] == ExecutionHighlight::Success) {
        return;
    }

    m_activeRow = row;
    m_activeHighlight = ExecutionHighlight::ImageFindMiss;
    triggerRowFlash(row, ExecutionHighlight::ImageFindMiss, kMissFlashMs);
    applyActiveRowVisuals();
}

void BlockListWidget::notifyImageFindReturnToPrevious(int sourceRow, int targetRow) {
    if (sourceRow < 0 || targetRow < 0 || sourceRow >= m_blockCount || targetRow >= m_blockCount) {
        return;
    }
    triggerDualRowFlash(
        sourceRow, targetRow, ExecutionHighlight::ReturnToPrevious, kReturnToPreviousFadeMs);
    scrollReturnToPreviousRowsIntoView();
    applyActiveRowVisuals();
}

bool BlockListWidget::isReturnToPreviousFlashVisible() const {
    return m_flashKind == ExecutionHighlight::ReturnToPrevious
        && (m_flashIntensity > 0.0
            || (m_returnFlashHoldTimer && m_returnFlashHoldTimer->isActive()));
}

void BlockListWidget::cancelReturnFlashHold() {
    if (m_returnFlashHoldTimer) {
        m_returnFlashHoldTimer->stop();
    }
}

void BlockListWidget::startReturnToPreviousFade(int fadeMs) {
    if (!m_flashAnimation || fadeMs <= 0 || m_flashKind != ExecutionHighlight::ReturnToPrevious) {
        return;
    }

    m_flashIntensity = 1.0;
    m_flashAnimation->stop();
    m_flashAnimation->setDuration(fadeMs);
    m_flashAnimation->setStartValue(1.0);
    m_flashAnimation->setEndValue(0.0);
    m_flashAnimation->start();
}

void BlockListWidget::scrollReturnToPreviousRowsIntoView() {
    if (m_flashRow < 0 || m_flashRowSecondary < 0) {
        return;
    }

    const int tableA = tableRowForBlockRow(m_flashRow);
    const int tableB = tableRowForBlockRow(m_flashRowSecondary);
    if (tableA < 0 || tableB < 0) {
        return;
    }

    const int anchorTableRow = (tableA + tableB) / 2;
    if (QTableWidgetItem* anchor = item(anchorTableRow, 0)) {
        scrollToItem(anchor, QAbstractItemView::PositionAtCenter);
    }
}

void BlockListWidget::refreshDragScrollDependentUi() {
    if (m_dragSourceRow >= 0) {
        const QPoint local = viewport()->mapFromGlobal(QCursor::pos());
        const int insertIdx = dropInsertionIndex(local);
        if (insertIdx != m_dropInsertionIndex) {
            m_dropInsertionIndex = insertIdx;
        }
        updateDropIndicator();
        return;
    }

    if (m_loopRegionPickActive && m_loopRegionPickDragging) {
        m_loopRegionPickCurrentRow =
            blockRowAtViewportY(viewport()->mapFromGlobal(QCursor::pos()).y());
        updateLoopRegionPickPreview();
        applyActiveRowVisuals();
    }
}

void BlockListWidget::clearActiveRow() {
    setActiveRow(-1, ExecutionHighlight::None);
}

void BlockListWidget::triggerRowFlash(int row, ExecutionHighlight highlight, int durationMs) {
    if (!m_flashAnimation || row < 0 || durationMs <= 0) {
        return;
    }
    if (isReturnToPreviousFlashVisible() && highlight != ExecutionHighlight::ReturnToPrevious) {
        return;
    }

    cancelReturnFlashHold();
    m_flashRow = row;
    m_flashRowSecondary = -1;
    m_flashKind = highlight;
    m_flashIntensity = 1.0;
    m_flashAnimation->stop();
    m_flashAnimation->setDuration(durationMs);
    m_flashAnimation->setStartValue(1.0);
    m_flashAnimation->setEndValue(0.0);
    m_flashAnimation->start();
}

void BlockListWidget::triggerDualRowFlash(int primaryRow,
                                          int secondaryRow,
                                          ExecutionHighlight highlight,
                                          int durationMs) {
    if (!m_flashAnimation || primaryRow < 0 || secondaryRow < 0 || durationMs <= 0) {
        return;
    }

    cancelReturnFlashHold();
    m_flashAnimation->stop();
    m_flashRow = primaryRow;
    m_flashRowSecondary = secondaryRow;
    m_flashKind = highlight;
    m_flashIntensity = 1.0;

    if (highlight == ExecutionHighlight::ReturnToPrevious && m_returnFlashHoldTimer) {
        m_returnFlashHoldTimer->start(kReturnToPreviousHoldMs);
        return;
    }

    m_flashAnimation->setDuration(durationMs);
    m_flashAnimation->setStartValue(1.0);
    m_flashAnimation->setEndValue(0.0);
    m_flashAnimation->start();
}

void BlockListWidget::onFlashAnimationValueChanged(const QVariant& value) {
    m_flashIntensity = value.toReal();
    applyActiveRowVisuals();
}

void BlockListWidget::onFlashAnimationFinished() {
    m_flashIntensity = 0.0;
    if (m_flashRow >= 0 && m_flashRow < m_rowCompletedHighlight.size()
        && (m_flashKind == ExecutionHighlight::Success
            || m_flashKind == ExecutionHighlight::Failed)) {
        m_rowCompletedHighlight[m_flashRow] = ExecutionHighlight::None;
    }
    m_flashRow = -1;
    m_flashRowSecondary = -1;
    m_flashKind = ExecutionHighlight::None;
    cancelReturnFlashHold();
    applyActiveRowVisuals();
}

qreal BlockListWidget::rowGlassIntensity(int row, ExecutionHighlight highlight) const {
    if (highlight == ExecutionHighlight::Running) {
        return kRunningGlassIntensity;
    }
    if (highlight == ExecutionHighlight::TriggerWatch) {
        const qreal pulse = 0.94 + 0.06 * std::sin(m_ambientAnimPhase * M_PI / 36.0);
        return kRunningGlassIntensity * pulse;
    }
    if (highlight == ExecutionHighlight::TriggerCooldown) {
        const qreal pulse = 0.82 + 0.14 * std::sin(m_ambientAnimPhase * M_PI / 24.0);
        return kRunningGlassIntensity * 0.74 * pulse;
    }
    if (m_flashKind == highlight && m_flashIntensity > 0.0
        && (m_flashRow == row || m_flashRowSecondary == row)) {
        if (highlight == ExecutionHighlight::ReturnToPrevious) {
            return qMin<qreal>(1.0, m_flashIntensity * 1.08);
        }
        return m_flashIntensity;
    }
    if (highlight == ExecutionHighlight::ReturnToPrevious && isReturnToPreviousFlashVisible()
        && (m_flashRow == row || m_flashRowSecondary == row)) {
        return 1.0;
    }
    return 0.0;
}

QColor BlockListWidget::matchScoreForegroundColor(bool succeeded, bool onMissHighlightRow) const {
    Q_UNUSED(onMissHighlightRow)
    if (succeeded) {
        return palette().color(QPalette::Text);
    }
    const QColor text = palette().color(QPalette::Text);
    return text.lightness() < 128 ? QColor(255, 150, 158) : QColor(190, 58, 68);
}

void BlockListWidget::updateHoverTableRow(int tableRow) {
    if (tableRow == m_hoverTableRow) {
        return;
    }
    const int previous = m_hoverTableRow;
    m_hoverTableRow = tableRow;
    applyIdleRowBackground(previous);
    applyIdleRowBackground(m_hoverTableRow);
}

void BlockListWidget::applyIdleRowBackground(int tableRow) {
    if (tableRow < 0 || tableRow >= rowCount()) {
        return;
    }
    const int blockRow = blockRowForTableRow(tableRow);
    if (blockRow < 0) {
        return;
    }
    if (rowVisualHighlight(blockRow) != ExecutionHighlight::None
        || isReturnToPreviousFlashVisible()) {
        return;
    }
    if (tableRow < m_loopRegionPickPreview.size() && m_loopRegionPickPreview[tableRow]) {
        return;
    }

    const QPalette rowPalette = palette();
    const bool hovered = tableRow == m_hoverTableRow
        && !(selectionModel() && selectionModel()->isRowSelected(tableRow, QModelIndex()));
    const QBrush brush = hovered ? QBrush(UiHoverFeedback::blendListRowHover(rowPalette))
                                 : rowPalette.brush(QPalette::Base);
    for (int c = 0; c < kColumnCount; ++c) {
        if (QTableWidgetItem* cellItem = item(tableRow, c)) {
            cellItem->setBackground(brush);
        }
    }
}

void BlockListWidget::applyActiveRowVisuals() {
    const QPalette rowPalette = palette();
    const QBrush normalBrush = rowPalette.brush(QPalette::Base);
    const QBrush normalForeground = rowPalette.brush(QPalette::Text);
    const QFont normalFont = font();

    for (int tableRow = 0; tableRow < rowCount(); ++tableRow) {
        const int blockRow = blockRowForTableRow(tableRow);
        if (blockRow < 0) {
            for (int c = 0; c < kColumnCount; ++c) {
                if (QTableWidgetItem* cellItem = item(tableRow, c)) {
                    cellItem->setBackground(Qt::NoBrush);
                    cellItem->setForeground(rowPalette.brush(QPalette::Text));
                }
            }
            continue;
        }

        QBrush rowBrush = normalBrush;
        QBrush rowForeground = normalForeground;
        QFont rowFont = normalFont;
        rowFont.setBold(false);
        QString indexText = QString::number(blockRow + 1);
        bool useGlassIndex = false;
        GlassColors glassColors;

        const ExecutionHighlight rowHighlight = rowVisualHighlight(blockRow);
        const qreal glassIntensity = rowGlassIntensity(blockRow, rowHighlight);
        const bool showGlass = rowHighlight != ExecutionHighlight::None && glassIntensity > 0.0;
        const bool returnFlashRow = isReturnToPreviousFlashVisible()
            && (blockRow == m_flashRow || blockRow == m_flashRowSecondary);

        if (returnFlashRow) {
            rowFont.setBold(true);
            if (blockRow == m_flashRow) {
                indexText = QStringLiteral("↓ %1").arg(blockRow + 1);
            } else if (blockRow == m_flashRowSecondary) {
                indexText = QStringLiteral("↩ %1").arg(blockRow + 1);
            }
        } else if (blockRow == m_activeRow && m_activeHighlight != ExecutionHighlight::None) {
            rowFont.setBold(true);
            switch (m_activeHighlight) {
            case ExecutionHighlight::Running:
                indexText = QStringLiteral("▶ %1").arg(blockRow + 1);
                break;
            case ExecutionHighlight::TriggerWatch:
                indexText = QStringLiteral("◎ %1").arg(blockRow + 1);
                break;
            case ExecutionHighlight::TriggerCooldown:
                indexText = QStringLiteral("◔ %1").arg(blockRow + 1);
                break;
            case ExecutionHighlight::ImageFindMiss:
                indexText = QStringLiteral("⌕ %1").arg(blockRow + 1);
                break;
            case ExecutionHighlight::Success:
                indexText = QStringLiteral("✓ %1").arg(blockRow + 1);
                break;
            case ExecutionHighlight::Failed:
                indexText = QStringLiteral("✕ %1").arg(blockRow + 1);
                break;
            case ExecutionHighlight::None:
                break;
            }
        } else if (tableRow < m_loopRegionPickPreview.size() && m_loopRegionPickPreview[tableRow]
                   && blockRow == qMin(m_loopRegionPickAnchorRow, m_loopRegionPickCurrentRow)) {
            indexText = QStringLiteral("↻ %1").arg(blockRow + 1);
        }

        const bool inLoopRegion = tableRow < m_loopRegionMember.size() && m_loopRegionMember[tableRow];
        const bool inPickPreview = tableRow < m_loopRegionPickPreview.size() && m_loopRegionPickPreview[tableRow];

        if (showGlass) {
            glassColors = glassColorsFor(rowHighlight, glassIntensity, rowPalette);
            if (rowHighlight == ExecutionHighlight::ReturnToPrevious) {
                const QColor tint = tintOverlay(glassColors.tint, glassColors.intensity, kReturnToPreviousGlassAlphaCap);
                const QColor specular = tintOverlay(QColor(255, 255, 255), glassColors.intensity, 34);
                const QColor base = rowPalette.color(QPalette::Base);
                const QColor alt = rowPalette.color(QPalette::AlternateBase);
                QLinearGradient gradient(0, 0, 0, qMax(m_blockRowHeight, 1));
                gradient.setColorAt(0.0, blendOver(base, specular));
                gradient.setColorAt(0.18, blendOver(base, tint));
                gradient.setColorAt(1.0,
                                     blendOver(alt.isValid() ? alt : base,
                                               tintOverlay(glassColors.tint, glassColors.intensity * 0.55,
                                                           kReturnToPreviousGlassAlphaCap)));
                gradient.setCoordinateMode(QGradient::ObjectBoundingMode);
                rowBrush = QBrush(gradient);
            } else {
                rowBrush = glassBodyBrush(glassColors, rowPalette, m_blockRowHeight);
            }
            rowForeground = glassColors.foreground;
            useGlassIndex = true;
        }

        for (int c = 0; c < kColumnCount; ++c) {
            QTableWidgetItem* cellItem = this->item(tableRow, c);
            if (!cellItem) {
                continue;
            }
            if (c == kColIndex && useGlassIndex) {
                cellItem->setBackground(glassIndexBrush(glassColors, rowPalette, m_blockRowHeight));
            } else if (showGlass) {
                cellItem->setBackground(rowBrush);
            } else if (inPickPreview) {
                QColor pickTint = rowPalette.color(QPalette::Highlight);
                pickTint.setAlpha(48);
                cellItem->setBackground(blendOver(rowPalette.color(QPalette::Base), pickTint));
            } else if (tableRow == m_hoverTableRow
                       && !(selectionModel() && selectionModel()->isRowSelected(tableRow, QModelIndex()))) {
                cellItem->setBackground(UiHoverFeedback::blendListRowHover(rowPalette));
            } else if (inLoopRegion) {
                cellItem->setBackground(normalBrush);
            } else {
                cellItem->setBackground(normalBrush);
            }
            cellItem->setFont(rowFont);
            if (c == kColScore && blockRow < m_rowMatchHasScore.size() && m_rowMatchHasScore[blockRow]) {
                const bool onMissHighlightRow = rowHighlight == ExecutionHighlight::ImageFindMiss;
                const bool scoreSucceeded = isMatchSuccessLocked(blockRow)
                                            || m_rowMatchSucceeded[blockRow]
                                            || rowHighlight == ExecutionHighlight::Success;
                if (showGlass) {
                    cellItem->setForeground(glassColors.scoreForeground);
                } else {
                    cellItem->setForeground(matchScoreForegroundColor(scoreSucceeded, onMissHighlightRow));
                }
            } else if (showGlass) {
                cellItem->setForeground(rowForeground);
            } else {
                cellItem->setForeground(normalForeground);
            }
            if (c == kColIndex || c == kColPreview || c == kColAction || c == kColDuration || c == kColMatchDuration
                || c == kColAttempts || c == kColReturnPrev || c == kColRetryAfterNext || c == kColScore
                || c == kColRoiCorrection) {
                cellItem->setTextAlignment(Qt::AlignCenter);
            }
            if (c == kColIndex) {
                cellItem->setText(indexText);
            }
        }
    }

    if (isReturnToPreviousFlashVisible()) {
        scrollReturnToPreviousRowsIntoView();
    } else if (m_activeRow >= 0) {
        const int activeTableRow = tableRowForBlockRow(m_activeRow);
        if (activeTableRow >= 0) {
            if (QTableWidgetItem* anchor = item(activeTableRow, 0)) {
                scrollToItem(anchor, QAbstractItemView::PositionAtCenter);
            }
        }
    }
    updateLoopRegionChrome();
}

void BlockListWidget::startDrag(Qt::DropActions supportedActions) {
    m_dragSourceRow = currentRow();
    m_pendingReorderFrom = -1;
    m_pendingReorderTo = -1;
    m_dropInsertionIndex = -1;
    if (m_dragSourceRow < 0 || !m_reorderEnabled || blockRowForTableRow(m_dragSourceRow) < 0) {
        m_dragSourceRow = -1;
        return;
    }

    updateDragSourceVisuals();

    const int sourceRow = m_dragSourceRow;
    const QPoint cursorGlobal = QCursor::pos();
    const QRect rowRect = visualRect(model()->index(sourceRow, 0));
    const QRect slotRect(0, rowRect.top(), viewport()->width(), rowRect.height());
    ListDragVisuals::showDragSlotPlaceholder(viewport(), slotRect, &m_dragSlotPlaceholder);

    QModelIndexList indexes = selectedIndexes();
    if (indexes.isEmpty()) {
        for (int column = 0; column < kColumnCount; ++column) {
            indexes << model()->index(sourceRow, column);
        }
    }

    auto* mimeData = model()->mimeData(indexes);
    auto* drag = new QDrag(this);
    drag->setMimeData(mimeData);
    const ListDragVisuals::LiftedPixmap lifted =
        ListDragVisuals::makeLiftedTableRowDrag(this, sourceRow, cursorGlobal);
    ListDragVisuals::applyToDrag(drag, lifted);
    m_dragAutoScroll->begin();
    qApp->installEventFilter(this);
    drag->exec(supportedActions, Qt::MoveAction);
    qApp->removeEventFilter(this);
    m_dragAutoScroll->end();

    ListDragVisuals::hideDragSlotPlaceholder(&m_dragSlotPlaceholder);
    clearDropIndicator();
    applyActiveRowVisuals();

    const int fromBlock =
        m_pendingReorderFrom >= 0 ? m_pendingReorderFrom : blockRowForTableRow(sourceRow);
    const int toBlock = m_pendingReorderTo >= 0 ? m_pendingReorderTo : fromBlock;
    m_dragSourceRow = -1;
    m_pendingReorderFrom = -1;
    m_pendingReorderTo = -1;

    emit blockRowsReordered(fromBlock, toBlock);

    if (fromBlock != toBlock) {
        const int settleTableRow = tableRowForBlockRow(toBlock);
        if (settleTableRow >= 0) {
            playDropSettleAtTableRow(settleTableRow);
        }
    }
}

void BlockListWidget::dragEnterEvent(QDragEnterEvent* event) {
    if (m_reorderEnabled && event->source() == this && m_dragSourceRow >= 0) {
        m_dropInsertionIndex = dropInsertionIndex(event->position().toPoint());
        updateDropIndicator();
        m_dragAutoScroll->updateFromGlobalCursor();
        event->acceptProposedAction();
        return;
    }
    m_dragAutoScroll->releaseEdgeScroll();
    clearDropIndicator();
    event->ignore();
}

void BlockListWidget::dragMoveEvent(QDragMoveEvent* event) {
    if (m_reorderEnabled && event->source() == this && m_dragSourceRow >= 0) {
        const int insertIdx = dropInsertionIndex(event->position().toPoint());
        if (insertIdx != m_dropInsertionIndex) {
            m_dropInsertionIndex = insertIdx;
            updateDropIndicator();
        } else if (m_dropIndicator && m_dropIndicator->isVisible()) {
            updateDropIndicator();
        }
        m_dragAutoScroll->updateFromGlobalCursor();
        event->acceptProposedAction();
        return;
    }
    m_dragAutoScroll->releaseEdgeScroll();
    clearDropIndicator();
    event->ignore();
}

void BlockListWidget::dragLeaveEvent(QDragLeaveEvent* event) {
    clearDropIndicator();
    QTableWidget::dragLeaveEvent(event);
}

void BlockListWidget::wheelEvent(QWheelEvent* event) {
    if (m_dragAutoScroll->isActive() && m_dragAutoScroll->handleWheel(event)) {
        return;
    }
    QTableWidget::wheelEvent(event);
}

int BlockListWidget::dropInsertionIndex(const QPoint& pos) const {
    const int rows = rowCount();
    if (rows <= 0) {
        return 0;
    }

    const int dropRow = rowAt(pos.y());
    if (dropRow < 0) {
        const QRect firstRect = visualRect(model()->index(0, 0));
        return pos.y() < firstRect.top() ? 0 : rows;
    }

    const QRect rect = visualRect(model()->index(dropRow, 0));
    return pos.y() < rect.center().y() ? dropRow : dropRow + 1;
}

int BlockListWidget::insertionLineY(int insertionIndex) const {
    const int rows = rowCount();
    if (rows <= 0) {
        return 0;
    }
    if (insertionIndex <= 0) {
        return visualRect(model()->index(0, 0)).top();
    }
    if (insertionIndex >= rows) {
        return visualRect(model()->index(rows - 1, 0)).bottom();
    }
    return visualRect(model()->index(insertionIndex, 0)).top();
}

void BlockListWidget::updateDropIndicator() {
    if (!m_dropIndicator || m_dropInsertionIndex < 0 || m_dragSourceRow < 0) {
        return;
    }
    const int y = insertionLineY(m_dropInsertionIndex);
    static_cast<DropInsertionIndicator*>(m_dropIndicator)->showAt(y, viewport()->width());
}

void BlockListWidget::clearDropIndicator() {
    m_dropInsertionIndex = -1;
    if (m_dropIndicator) {
        static_cast<DropInsertionIndicator*>(m_dropIndicator)->hideIndicator();
    }
}

void BlockListWidget::updateDragSourceVisuals() {
    const QBrush normalBrush = palette().brush(QPalette::Base);
    const QColor dragSourceBg(235, 235, 235);
    const QColor dragSourceFg(130, 130, 130);

    for (int r = 0; r < rowCount(); ++r) {
        const bool isSource = m_dragSourceRow >= 0 && r == m_dragSourceRow;
        for (int c = 0; c < kColumnCount; ++c) {
            QTableWidgetItem* item = this->item(r, c);
            if (!item) {
                continue;
            }
            if (isSource) {
                item->setBackground(dragSourceBg);
                item->setForeground(dragSourceFg);
            } else if (r != m_activeRow) {
                item->setBackground(normalBrush);
                item->setForeground(palette().color(QPalette::Text));
            }
        }
    }
}

void BlockListWidget::resizeEvent(QResizeEvent* event) {
    QTableWidget::resizeEvent(event);
    applyColumnLayoutToTable(true);
    if (m_columnHeader) {
        m_columnHeader->update();
    }
    updateLoopRegionChrome();
    if (m_dropInsertionIndex >= 0) {
        updateDropIndicator();
    }
}

void BlockListWidget::showEvent(QShowEvent* event) {
    QTableWidget::showEvent(event);
    applyColumnLayoutToTable(true);
    if (m_columnHeader) {
        m_columnHeader->update();
    }
}

int BlockListWidget::dropTargetRow(const QPoint& pos) const {
    int insertionRow = dropInsertionIndex(pos);
    if (m_dragSourceRow >= 0 && m_dragSourceRow < insertionRow) {
        --insertionRow;
    }
    if (rowCount() <= 0) {
        return 0;
    }
    return qBound(0, insertionRow, rowCount() - 1);
}

void BlockListWidget::dropEvent(QDropEvent* event) {
    m_dragAutoScroll->releaseEdgeScroll();
    if (!m_reorderEnabled || m_dragSourceRow < 0 || event->source() != this) {
        m_dragSourceRow = -1;
        clearDropIndicator();
        event->ignore();
        return;
    }

    const int toRow = dropTargetRow(event->position().toPoint());
    const int fromRow = m_dragSourceRow;
    m_dragSourceRow = -1;
    clearDropIndicator();

    if (fromRow != toRow) {
        const int fromBlock = blockRowForTableRow(fromRow);
        const int toBlock = blockRowForDropInsertion(dropInsertionIndex(event->position().toPoint()));
        if (fromBlock >= 0 && toBlock >= 0 && fromBlock != toBlock) {
            m_pendingReorderFrom = fromBlock;
            m_pendingReorderTo = toBlock;
        }
    }
    // IgnoreAction prevents QAbstractItemView::startDrag from clearOrRemove().
    event->setDropAction(Qt::IgnoreAction);
    event->accept();
}

void BlockListWidget::playDropSettleAtTableRow(int row) {
    if (!viewport() || row < 0 || row >= rowCount()) {
        return;
    }
    const QRect rowRect = visualRect(model()->index(row, 0));
    const QRect targetRect(0, rowRect.top(), viewport()->width(), rowRect.height());
    const QPixmap rowPixmap = ListDragVisuals::captureTableRowPixmap(this, row);
    ListDragVisuals::playDropSettle(viewport(), rowPixmap, targetRect);
}
