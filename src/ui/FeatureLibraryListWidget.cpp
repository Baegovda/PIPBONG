#include "ui/FeatureLibraryListWidget.h"

#include "ui/FeatureDragMime.h"
#include "ui/UiHoverFeedback.h"
#include "ui/UiThemeColors.h"

#include <QApplication>
#include <QFontMetrics>
#include <QKeyEvent>
#include <QListWidgetItem>
#include <QMimeData>
#include <QPainter>
#include <QPainterPath>
#include <QStyle>
#include <QStyledItemDelegate>

#include <algorithm>

namespace {

QStringList selectedIdsInRowOrder(const QListWidget* list) {
    QStringList ids;
    if (!list) {
        return ids;
    }
    QList<int> rows;
    for (QListWidgetItem* selected : list->selectedItems()) {
        if (!selected) {
            continue;
        }
        const int row = list->row(selected);
        if (row >= 0) {
            rows.push_back(row);
        }
    }
    std::sort(rows.begin(), rows.end());
    rows.erase(std::unique(rows.begin(), rows.end()), rows.end());
    for (int row : rows) {
        if (QListWidgetItem* item = list->item(row)) {
            const QString id = item->data(Qt::UserRole).toString();
            if (!id.isEmpty()) {
                ids.push_back(id);
            }
        }
    }
    return ids;
}

class FeatureLibraryItemDelegate : public QStyledItemDelegate {
public:
    explicit FeatureLibraryItemDelegate(QObject* parent = nullptr)
        : QStyledItemDelegate(parent) {}

    void paint(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index) const override {
        QStyleOptionViewItem opt = option;
        initStyleOption(&opt, index);
        painter->save();
        painter->setRenderHint(QPainter::Antialiasing);

        const QPalette pal = opt.widget ? opt.widget->palette() : QApplication::palette();
        const bool selected = opt.state.testFlag(QStyle::State_Selected);
        const bool hovered = opt.state.testFlag(QStyle::State_MouseOver) && !selected;
        const QColor contentColor = primaryContentTextColor(pal, selected);
        const QColor accent = contentColor.lightness() < 128 ? QColor(0x64, 0xb5, 0xf6)
                                                             : QColor(0x1e, 0x88, 0xe5);
        QColor fill = selected ? pal.color(QPalette::Highlight)
                               : (pal.color(QPalette::Button).isValid() ? pal.color(QPalette::Button)
                                                                        : pal.color(QPalette::AlternateBase));
        if (hovered) {
            fill = UiHoverFeedback::cardRowHoverFill(pal);
        }
        const QColor border = selected || hovered ? accent : fill.darker(108);

        const QRect rowRect = opt.rect.adjusted(2, 1, -2, -1);
        QPainterPath path;
        path.addRoundedRect(rowRect, 6, 6);
        painter->fillPath(path, fill);
        painter->setPen(QPen(border, 1));
        painter->drawPath(path);

        QFont nameFont = opt.font;
        nameFont.setBold(true);
        painter->setFont(nameFont);
        painter->setPen(contentColor);
        painter->drawText(rowRect.adjusted(8, 0, -8, 0), Qt::AlignCenter, opt.text);

        if (selected && opt.state.testFlag(QStyle::State_HasFocus)) {
            QStyleOptionFocusRect focus;
            focus.rect = rowRect.adjusted(1, 1, -1, -1);
            focus.state = opt.state;
            focus.palette = pal;
            if (const QStyle* style = opt.widget ? opt.widget->style() : QApplication::style()) {
                style->drawPrimitive(QStyle::PE_FrameFocusRect, &focus, painter, opt.widget);
            }
        }

        painter->restore();
    }

    QSize sizeHint(const QStyleOptionViewItem& option, const QModelIndex& index) const override {
        Q_UNUSED(index);
        QSize size = QStyledItemDelegate::sizeHint(option, index);
        size.setHeight(qMax(size.height(), 28));
        return size;
    }
};

} // namespace

FeatureLibraryListWidget::FeatureLibraryListWidget(QWidget* parent)
    : ReorderableListWidget(parent) {
    setSelectionMode(QAbstractItemView::ExtendedSelection);
    setItemDelegate(new FeatureLibraryItemDelegate(this));
    setToolTip(tr("Ctrl/Shift+클릭 다중 선택 · 드래그하여 라이브러리 순서 변경 · 기능 목록에서 드래그하여 저장 · Delete"));
    connect(this, &ReorderableListWidget::rowsReordered, this, &FeatureLibraryListWidget::libraryRowsReordered);
    connect(this,
            &ReorderableListWidget::externalItemDropped,
            this,
            [this](const QMimeData* mime, int /*insertIndex*/) {
                emit featureDroppedOnLibrary(mime);
            });
}

void FeatureLibraryListWidget::setTransferEnabled(bool enabled) {
    m_transferEnabled = enabled;
    setReorderEnabled(enabled);
}

QMimeData* FeatureLibraryListWidget::buildDragMimeData(int row) const {
    Q_UNUSED(row);
    const QStringList ids = selectedIdsInRowOrder(this);
    if (ids.isEmpty()) {
        return nullptr;
    }

    FeatureDragMime::Payload payload;
    payload.source = FeatureDragMime::Source::Library;
    payload.ids = ids;
    payload.id = ids.first();
    return FeatureDragMime::createMimeData(payload);
}

bool FeatureLibraryListWidget::acceptsExternalMime(const QMimeData* mime) const {
    if (!FeatureDragMime::accepts(mime)) {
        return false;
    }
    const FeatureDragMime::Payload payload = FeatureDragMime::parse(mime);
    return payload.source == FeatureDragMime::Source::Profile;
}

Qt::DropAction FeatureLibraryListWidget::preferredExternalDropAction(const QMimeData* /*mime*/) const {
    return Qt::CopyAction;
}

void FeatureLibraryListWidget::keyPressEvent(QKeyEvent* event) {
    if (event->key() == Qt::Key_Delete) {
        emit deleteRequested();
        event->accept();
        return;
    }
    ReorderableListWidget::keyPressEvent(event);
}
