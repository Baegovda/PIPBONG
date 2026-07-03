#pragma once

#include <QStyledItemDelegate>
#include <QSet>
#include <QPair>

class CurrencyIconCache;
class SpreadsheetModel;

class SpreadsheetCellDelegate : public QStyledItemDelegate {
    Q_OBJECT
public:
    SpreadsheetCellDelegate(SpreadsheetModel* model, CurrencyIconCache* icons, QObject* parent = nullptr);

    void paint(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index) const override;
    void setReferencedCells(const QSet<QPair<int, int>>& referencedCells);
    void setReferencePulsePhase(qreal phase);

private:
    SpreadsheetModel* m_model = nullptr;
    CurrencyIconCache* m_icons = nullptr;
    QSet<QPair<int, int>> m_referencedCells;
    qreal m_referencePulsePhase = 0.0;
};
