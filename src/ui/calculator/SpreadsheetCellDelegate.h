#pragma once

#include <QStyledItemDelegate>

class CurrencyIconCache;
class SpreadsheetModel;

class SpreadsheetCellDelegate : public QStyledItemDelegate {
    Q_OBJECT
public:
    SpreadsheetCellDelegate(SpreadsheetModel* model, CurrencyIconCache* icons, QObject* parent = nullptr);

    void paint(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index) const override;

private:
    SpreadsheetModel* m_model = nullptr;
    CurrencyIconCache* m_icons = nullptr;
};
