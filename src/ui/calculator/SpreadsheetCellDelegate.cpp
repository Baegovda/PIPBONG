#include "ui/calculator/SpreadsheetCellDelegate.h"

#include "ui/calculator/CurrencyIconCache.h"
#include "ui/calculator/SpreadsheetModel.h"

#include <QApplication>
#include <QPainter>
#include <QStyle>

namespace {

constexpr int kIconSize = 22;
constexpr int kIconTextGap = 6;
constexpr int kCellPadding = 4;

} // namespace

SpreadsheetCellDelegate::SpreadsheetCellDelegate(SpreadsheetModel* model,
                                                 CurrencyIconCache* icons,
                                                 QObject* parent)
    : QStyledItemDelegate(parent)
    , m_model(model)
    , m_icons(icons) {}

void SpreadsheetCellDelegate::paint(QPainter* painter,
                                    const QStyleOptionViewItem& option,
                                    const QModelIndex& index) const {
    if (!m_model || !index.isValid()) {
        QStyledItemDelegate::paint(painter, option, index);
        return;
    }

    const SpreadsheetCell cell = m_model->cellInput(index.row(), index.column());
    if (cell.kind != SpreadsheetCellKind::ApiRef || !m_icons) {
        QStyledItemDelegate::paint(painter, option, index);
        return;
    }

    m_icons->ensureIcon(cell.currencyId);
    const QPixmap iconPixmap = m_icons->pixmapOrPlaceholder(cell.currencyId);

    QStyleOptionViewItem opt = option;
    initStyleOption(&opt, index);
    opt.icon = QIcon();
    opt.features &= ~QStyleOptionViewItem::HasDecoration;

    const QWidget* widget = option.widget;
    QStyle* style = widget ? widget->style() : QApplication::style();
    style->drawControl(QStyle::CE_ItemViewItem, &opt, painter, widget);

    const QRect iconRect(option.rect.left() + kCellPadding,
                         option.rect.top() + (option.rect.height() - kIconSize) / 2,
                         kIconSize,
                         kIconSize);
    painter->drawPixmap(iconRect, iconPixmap.scaled(kIconSize,
                                                    kIconSize,
                                                    Qt::KeepAspectRatio,
                                                    Qt::SmoothTransformation));

    const QString text = index.data(Qt::DisplayRole).toString();
    if (text.isEmpty()) {
        return;
    }

    QRect textRect = option.rect.adjusted(kCellPadding + kIconSize + kIconTextGap,
                                          0,
                                          -kCellPadding,
                                          0);
    painter->save();
    if (opt.state & QStyle::State_Selected) {
        painter->setPen(opt.palette.color(QPalette::HighlightedText));
    } else if (const QVariant fg = index.data(Qt::ForegroundRole); fg.canConvert<QBrush>()) {
        painter->setPen(fg.value<QBrush>().color());
    } else {
        painter->setPen(opt.palette.color(QPalette::Text));
    }
    painter->drawText(textRect, Qt::AlignVCenter | Qt::AlignLeft, text);
    painter->restore();
}
