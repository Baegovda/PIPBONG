#include "ui/calculator/SpreadsheetCellDelegate.h"

#include "ui/calculator/CurrencyIconCache.h"
#include "ui/calculator/SpreadsheetModel.h"

#include <QApplication>
#include <QFontMetrics>
#include <QPainter>
#include <QStyle>

namespace {

constexpr int kIconSize = 22;
constexpr int kIconTextGap = 6;

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
    opt.text.clear();
    opt.features &= ~(QStyleOptionViewItem::HasDecoration | QStyleOptionViewItem::HasDisplay);

    const QWidget* widget = option.widget;
    QStyle* style = widget ? widget->style() : QApplication::style();
    style->drawControl(QStyle::CE_ItemViewItem, &opt, painter, widget);

    const QString text = index.data(Qt::DisplayRole).toString();

    painter->save();
    if (opt.state & QStyle::State_Selected) {
        painter->setPen(opt.palette.color(QPalette::HighlightedText));
    } else if (const QVariant fg = index.data(Qt::ForegroundRole); fg.canConvert<QBrush>()) {
        painter->setPen(fg.value<QBrush>().color());
    } else {
        painter->setPen(opt.palette.color(QPalette::Text));
    }

    const QFontMetrics fm(painter->font());
    const int textWidth = text.isEmpty() ? 0 : fm.horizontalAdvance(text);
    const int contentWidth = text.isEmpty() ? kIconSize : kIconSize + kIconTextGap + textWidth;
    const int startX = option.rect.left() + (option.rect.width() - contentWidth) / 2;

    const QRect iconRect(startX,
                         option.rect.top() + (option.rect.height() - kIconSize) / 2,
                         kIconSize,
                         kIconSize);
    painter->drawPixmap(iconRect, iconPixmap.scaled(kIconSize,
                                                    kIconSize,
                                                    Qt::KeepAspectRatio,
                                                    Qt::SmoothTransformation));

    if (!text.isEmpty()) {
        const QRect textRect(iconRect.right() + kIconTextGap,
                             option.rect.top(),
                             textWidth,
                             option.rect.height());
        painter->drawText(textRect, Qt::AlignVCenter | Qt::AlignLeft, text);
    }
    painter->restore();
}
