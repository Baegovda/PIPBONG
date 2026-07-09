#include "ui/BlockListWidget.h"

#include "ui/UiResizeHandle.h"

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
#include <functional>
#include <QMouseEvent>
#include <QObject>
#include <QPushButton>
#include <QLinearGradient>
#include <QPainter>
#include <QResizeEvent>
#include <QScrollBar>
#include <QSignalBlocker>
#include <QStyledItemDelegate>
#include <QStyleOptionButton>
#include <QStyleOptionViewItem>
#include <QTimer>
#include <QVariantAnimation>
#include <QEasingCurve>

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

class BlockListHeaderView : public QHeaderView {
public:
    explicit BlockListHeaderView(BlockListWidget* owner)
        : QHeaderView(Qt::Horizontal, owner)
        , m_owner(owner) {
        setMouseTracking(true);
    }

protected:
    int resizeHandleSectionAt(int x) const {
        int bestSection = -1;
        int bestDistance = UiResizeHandle::kDividerHalfWidthPx + 1;
        for (int visual = 0; visual < count(); ++visual) {
            const int logical = logicalIndex(visual);
            if (isSectionHidden(logical)) {
                continue;
            }
            if (sectionResizeMode(logical) != QHeaderView::Interactive) {
                continue;
            }
            const int handleX = sectionViewportPosition(logical) + sectionSize(logical);
            const int distance = qAbs(x - handleX);
            if (distance <= UiResizeHandle::kDividerHalfWidthPx && distance < bestDistance) {
                bestDistance = distance;
                bestSection = logical;
            }
        }
        return bestSection;
    }

    bool isRowHeightHandle(const QPoint& pos) const {
        return UiResizeHandle::isWithinBottomGrab(pos.y(), height());
    }

    void mousePressEvent(QMouseEvent* event) override {
        if (event->button() == Qt::LeftButton) {
            if (isRowHeightHandle(event->pos())) {
                m_resizingRowHeight = true;
                m_resizingSection = -1;
                m_resizePressPos = event->globalPosition().toPoint().y();
                m_resizeStartSize = m_owner ? m_owner->blockRowHeight()
                                            : UiResizeHandle::kDefaultBlockListRowHeightPx;
                setCursor(Qt::SizeVerCursor);
                event->accept();
                return;
            }
            const int section = resizeHandleSectionAt(event->pos().x());
            if (section >= 0) {
                m_resizingRowHeight = false;
                m_resizingSection = section;
                m_resizePressPos = event->globalPosition().toPoint().x();
                m_resizeStartSize = sectionSize(section);
                event->accept();
                return;
            }
        }
        m_resizingSection = -1;
        m_resizingRowHeight = false;
        QHeaderView::mousePressEvent(event);
    }

    void mouseMoveEvent(QMouseEvent* event) override {
        if (m_resizingRowHeight && (event->buttons() & Qt::LeftButton)) {
            const int delta = event->globalPosition().toPoint().y() - m_resizePressPos;
            if (m_owner) {
                m_owner->setBlockRowHeight(m_resizeStartSize + delta, true);
            }
            setCursor(Qt::SizeVerCursor);
            event->accept();
            return;
        }
        if (m_resizingSection >= 0 && (event->buttons() & Qt::LeftButton)) {
            const int delta = event->globalPosition().toPoint().x() - m_resizePressPos;
            resizeSection(m_resizingSection, qMax(minimumSectionSize(), m_resizeStartSize + delta));
            event->accept();
            return;
        }

        QHeaderView::mouseMoveEvent(event);
        if (m_resizingSection < 0 && !m_resizingRowHeight) {
            if (isRowHeightHandle(event->pos())) {
                setCursor(Qt::SizeVerCursor);
            } else if (resizeHandleSectionAt(event->pos().x()) >= 0) {
                setCursor(Qt::SplitHCursor);
            } else {
                unsetCursor();
            }
        }
    }

    void mouseReleaseEvent(QMouseEvent* event) override {
        if (m_resizingRowHeight || m_resizingSection >= 0) {
            m_resizingRowHeight = false;
            m_resizingSection = -1;
            unsetCursor();
            event->accept();
            return;
        }
        QHeaderView::mouseReleaseEvent(event);
    }

    void leaveEvent(QEvent* event) override {
        if (m_resizingSection < 0 && !m_resizingRowHeight) {
            unsetCursor();
        }
        QHeaderView::leaveEvent(event);
    }

private:
    BlockListWidget* m_owner = nullptr;
    int m_resizingSection = -1;
    bool m_resizingRowHeight = false;
    int m_resizePressPos = 0;
    int m_resizeStartSize = 0;
};

void applyDefaultBlockListColumnWidths(QTableWidget* table) {
    const QSignalBlocker blocker(table->horizontalHeader());
    table->setColumnWidth(kColIndex, 28);
    table->setColumnWidth(kColPreview, kThumbnailSize + 6);
    table->setColumnWidth(kColAction, 72);
    table->setColumnWidth(kColDuration, 72);
    table->setColumnWidth(kColMatchDuration, 72);
    table->setColumnWidth(kColAttempts, 58);
    table->setColumnWidth(kColReturnPrev, 52);
    table->setColumnWidth(kColRetryAfterNext, 52);
    table->setColumnWidth(kColScore, 78);
    table->setColumnWidth(kColRoiCorrection, 52);
    table->setColumnWidth(kColMatch, kThumbnailSize + 6);
}

void applyBlockListHeaderResizeModes(QHeaderView* header) {
    if (!header) {
        return;
    }
    header->setMinimumSectionSize(24);
    header->setStretchLastSection(false);
    header->setCascadingSectionResizes(false);
    for (int column = 0; column < kColumnCount; ++column) {
        header->setSectionResizeMode(column,
                                     column == kColSummary ? QHeaderView::Stretch
                                                           : QHeaderView::Interactive);
    }
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

    applyBlockListHeaderResizeModes(table->horizontalHeader());
    applyDefaultBlockListColumnWidths(table);
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
constexpr int kReturnToPreviousFlashMs = 480;
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
    setHorizontalHeader(new BlockListHeaderView(this));
    horizontalHeader()->setDefaultAlignment(Qt::AlignCenter);
    horizontalHeader()->setSectionsMovable(false);
    horizontalHeader()->setFirstSectionMovable(false);
    initBlockListColumnHeader(this);
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
    }
    QTableWidget::keyPressEvent(event);
}

void BlockListWidget::applyHeaderResizeModes() {
    applyBlockListHeaderResizeModes(horizontalHeader());
}

void BlockListWidget::setBlockRowHeight(int height, bool persist) {
    const int clamped = UiResizeHandle::clampListRowHeight(height);
    if (clamped == m_blockRowHeight) {
        return;
    }
    m_blockRowHeight = clamped;
    verticalHeader()->setDefaultSectionSize(m_blockRowHeight);
    for (int tableRow = 0; tableRow < rowCount(); ++tableRow) {
        if (blockRowForTableRow(tableRow) >= 0) {
            setRowHeight(tableRow, m_blockRowHeight);
        }
    }
    if (persist && !m_restoringRowHeight) {
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

void BlockListWidget::setRoiCorrectionColumnVisible(bool visible) {
    m_roiCorrectionColumnVisible = visible;
    if (QHeaderView* header = horizontalHeader()) {
        header->setSectionHidden(kColRoiCorrection, !visible);
    }
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
        tr("매칭 실패 시 바로 이전 템플릿 매칭 블록으로 돌아감 — 발동 횟수"));
    setItem(tableRow, kColReturnPrev, returnPrevItem);

    auto* retryAfterNextItem = new QTableWidgetItem(QStringLiteral("—"));
    retryAfterNextItem->setTextAlignment(Qt::AlignCenter);
    retryAfterNextItem->setFlags(itemFlags);
    retryAfterNextItem->setToolTip(
        tr("바로 다음 동작 후 다시 감지 시도 / 다음 템플릿 매칭 블록으로 넘어감 — 발동 횟수"));
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
            tr("매칭 실패 시 바로 이전 템플릿 매칭 블록으로 돌아감 — 발동 횟수"));
        setItem(tableRow, kColReturnPrev, returnPrevItem);
    }
    QTableWidgetItem* retryItem = item(tableRow, kColRetryAfterNext);
    if (!retryItem) {
        retryItem = new QTableWidgetItem();
        retryItem->setTextAlignment(Qt::AlignCenter);
        retryItem->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable | Qt::ItemIsDragEnabled);
        retryItem->setToolTip(
            tr("바로 다음 동작 후 다시 감지 시도 / 다음 템플릿 매칭 블록으로 넘어감 — 발동 횟수"));
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
    if (m_flashAnimation) {
        m_flashAnimation->stop();
    }
    m_flashRow = -1;
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
    applyActiveRowVisuals();
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
        sourceRow, targetRow, ExecutionHighlight::ReturnToPrevious, kReturnToPreviousFlashMs);
    applyActiveRowVisuals();
}

void BlockListWidget::clearActiveRow() {
    setActiveRow(-1, ExecutionHighlight::None);
}

void BlockListWidget::triggerRowFlash(int row, ExecutionHighlight highlight, int durationMs) {
    if (!m_flashAnimation || row < 0 || durationMs <= 0) {
        return;
    }

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

    m_flashRow = primaryRow;
    m_flashRowSecondary = secondaryRow;
    m_flashKind = highlight;
    m_flashIntensity = 1.0;
    m_flashAnimation->stop();
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
    applyActiveRowVisuals();
}

qreal BlockListWidget::rowGlassIntensity(int row, ExecutionHighlight highlight) const {
    if (highlight == ExecutionHighlight::Running) {
        return kRunningGlassIntensity;
    }
    if (m_flashKind == highlight && m_flashIntensity > 0.0
        && (m_flashRow == row || m_flashRowSecondary == row)) {
        return m_flashIntensity;
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

        if (blockRow == m_activeRow && m_activeHighlight != ExecutionHighlight::None) {
            rowFont.setBold(true);
            switch (m_activeHighlight) {
            case ExecutionHighlight::Running:
                indexText = QStringLiteral("▶ %1").arg(blockRow + 1);
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
        } else if (m_flashKind == ExecutionHighlight::ReturnToPrevious && m_flashIntensity > 0.0) {
            if (blockRow == m_flashRow) {
                indexText = QStringLiteral("↓ %1").arg(blockRow + 1);
                rowFont.setBold(true);
            } else if (blockRow == m_flashRowSecondary) {
                indexText = QStringLiteral("↩ %1").arg(blockRow + 1);
                rowFont.setBold(true);
            }
        }

        const bool inLoopRegion = tableRow < m_loopRegionMember.size() && m_loopRegionMember[tableRow];
        const bool inPickPreview = tableRow < m_loopRegionPickPreview.size() && m_loopRegionPickPreview[tableRow];

        if (showGlass) {
            glassColors = glassColorsFor(rowHighlight, glassIntensity, rowPalette);
            rowBrush = glassBodyBrush(glassColors, rowPalette, m_blockRowHeight);
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

    if (m_activeRow >= 0) {
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
        for (int column = 0; column < columnCount(); ++column) {
            indexes << model()->index(sourceRow, column);
        }
    }

    auto* mimeData = model()->mimeData(indexes);
    auto* drag = new QDrag(this);
    drag->setMimeData(mimeData);
    const ListDragVisuals::LiftedPixmap lifted =
        ListDragVisuals::makeLiftedTableRowDrag(this, sourceRow, cursorGlobal);
    ListDragVisuals::applyToDrag(drag, lifted);
    drag->exec(supportedActions, Qt::MoveAction);

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
        event->acceptProposedAction();
        return;
    }
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
        event->acceptProposedAction();
        return;
    }
    clearDropIndicator();
    event->ignore();
}

void BlockListWidget::dragLeaveEvent(QDragLeaveEvent* event) {
    clearDropIndicator();
    QTableWidget::dragLeaveEvent(event);
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
    updateLoopRegionChrome();
    if (m_dropInsertionIndex >= 0) {
        updateDropIndicator();
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
