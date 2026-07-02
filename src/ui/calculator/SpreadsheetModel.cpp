#include "ui/calculator/SpreadsheetModel.h"

#include "core/poeninja/PoeNinjaEconomyCategories.h"
#include "ui/calculator/SpreadsheetCellColors.h"

#include <QBrush>
#include <QColor>
#include <QSet>

#include <algorithm>
#include <cmath>
#include <unordered_set>

namespace {

QString kindToString(SpreadsheetCellKind kind) {
    switch (kind) {
    case SpreadsheetCellKind::Empty:
        return QStringLiteral("empty");
    case SpreadsheetCellKind::Number:
        return QStringLiteral("number");
    case SpreadsheetCellKind::Text:
        return QStringLiteral("text");
    case SpreadsheetCellKind::ApiRef:
        return QStringLiteral("api");
    case SpreadsheetCellKind::Formula:
        return QStringLiteral("formula");
    }
    return QStringLiteral("empty");
}

SpreadsheetCellKind kindFromString(const QString& value) {
    if (value == QLatin1String("number")) {
        return SpreadsheetCellKind::Number;
    }
    if (value == QLatin1String("text")) {
        return SpreadsheetCellKind::Text;
    }
    if (value == QLatin1String("api")) {
        return SpreadsheetCellKind::ApiRef;
    }
    if (value == QLatin1String("formula")) {
        return SpreadsheetCellKind::Formula;
    }
    return SpreadsheetCellKind::Empty;
}

} // namespace

SpreadsheetModel::SpreadsheetModel(QObject* parent)
    : QAbstractTableModel(parent) {}

int SpreadsheetModel::rowCount(const QModelIndex& parent) const {
    if (parent.isValid()) {
        return 0;
    }
    return m_rowCount;
}

int SpreadsheetModel::columnCount(const QModelIndex& parent) const {
    if (parent.isValid()) {
        return 0;
    }
    return m_columnCount;
}

SpreadsheetModel::CellKey SpreadsheetModel::makeKey(int row, int col) const {
    return qMakePair(row, col);
}

QVariant SpreadsheetModel::data(const QModelIndex& index, int role) const {
    if (!index.isValid()) {
        return {};
    }

    const CellKey key = makeKey(index.row(), index.column());
    const SpreadsheetCellState state = m_states.value(key);

    if (role == Qt::DisplayRole || role == Qt::EditRole) {
        if (role == Qt::EditRole) {
            if (state.input.kind == SpreadsheetCellKind::Formula) {
                return state.input.raw.startsWith(QLatin1Char('=')) ? state.input.raw
                                                                    : QLatin1Char('=') + state.input.raw;
            }
            if (state.input.kind == SpreadsheetCellKind::ApiRef) {
                return state.input.currencyName;
            }
            return state.input.raw;
        }
        return state.displayText;
    }

    if (role == Qt::TextAlignmentRole) {
        return static_cast<int>(Qt::AlignCenter);
    }

    if (role == Qt::ToolTipRole) {
        if (state.hasError) {
            return errorText(state.error);
        }
        if (state.input.kind == SpreadsheetCellKind::ApiRef) {
            const QString baseName = effectiveBaseCurrencyName(index.row(), index.column());
            return tr("poe.ninja 시세: %1 (%2 기준)")
                .arg(state.input.currencyName, baseName);
        }
        if (hasCellBaseCurrencyOverride(index.row(), index.column())) {
            return tr("기준 화폐: %1 (셀별 지정)")
                .arg(effectiveBaseCurrencyName(index.row(), index.column()));
        }
        if (state.input.kind == SpreadsheetCellKind::Formula) {
            return state.input.raw.startsWith(QLatin1Char('=')) ? state.input.raw
                                                                 : QLatin1Char('=') + state.input.raw;
        }
        return {};
    }

    if (role == Qt::ForegroundRole) {
        if (state.hasError) {
            return QBrush(QColor(220, 80, 80));
        }
        if (const QColor fg = m_foregroundColors.value(makeKey(index.row(), index.column()));
            fg.isValid()) {
            return QBrush(fg);
        }
        return {};
    }

    if (role == Qt::BackgroundRole) {
        if (const QColor bg = m_backgroundColors.value(makeKey(index.row(), index.column()));
            bg.isValid()) {
            return QBrush(bg);
        }
        if (state.input.kind == SpreadsheetCellKind::ApiRef) {
            return QBrush(QColor(42, 74, 110, 48));
        }
        if (state.input.kind == SpreadsheetCellKind::Formula) {
            return QBrush(QColor(74, 110, 42, 40));
        }
    }

    return {};
}

QVariant SpreadsheetModel::headerData(int section, Qt::Orientation orientation, int role) const {
    if (role != Qt::DisplayRole) {
        return {};
    }
    if (orientation == Qt::Vertical) {
        return QString::number(section + 1);
    }
    return FormulaEvaluator::cellReference(0, section).left(
        FormulaEvaluator::cellReference(0, section).length() - 1);
}

Qt::ItemFlags SpreadsheetModel::flags(const QModelIndex& index) const {
    if (!index.isValid()) {
        return Qt::NoItemFlags;
    }
    return Qt::ItemIsSelectable | Qt::ItemIsEnabled | Qt::ItemIsEditable;
}

bool SpreadsheetModel::setData(const QModelIndex& index, const QVariant& value, int role) {
    if (!index.isValid() || role != Qt::EditRole) {
        return false;
    }

    pushUndoSnapshot();

    QString text = value.toString().trimmed();
    SpreadsheetCell cell;

    if (text.isEmpty()) {
        cell.kind = SpreadsheetCellKind::Empty;
    } else if (text.startsWith(QLatin1Char('='))) {
        cell.kind = SpreadsheetCellKind::Formula;
        cell.raw = text;
    } else {
        bool ok = false;
        text.toDouble(&ok);
        if (ok) {
            cell.kind = SpreadsheetCellKind::Number;
            cell.raw = text;
        } else {
            cell.kind = SpreadsheetCellKind::Text;
            cell.raw = text;
        }
    }

    setCellInput(index.row(), index.column(), cell);
    return true;
}

void SpreadsheetModel::ensureDimensions(int row, int col) {
    m_rowCount = std::max(m_rowCount, row + 1);
    m_columnCount = std::max(m_columnCount, col + 1);
}

void SpreadsheetModel::setCellInput(int row, int col, SpreadsheetCell cell) {
    const int oldRowCount = m_rowCount;
    const int oldColumnCount = m_columnCount;
    ensureDimensions(row, col);
    const bool dimensionsChanged = m_rowCount != oldRowCount || m_columnCount != oldColumnCount;
    const CellKey key = makeKey(row, col);
    m_inputs.insert(key, std::move(cell));
    if (dimensionsChanged) {
        recalculateAll();
    } else {
        refreshCellStates();
    }
    emit sheetModified();
}

SpreadsheetCell SpreadsheetModel::cellInput(int row, int col) const {
    return m_inputs.value(makeKey(row, col));
}

SpreadsheetCellState SpreadsheetModel::cellState(int row, int col) const {
    return m_states.value(makeKey(row, col));
}

bool SpreadsheetModel::bindCurrencyToCell(int row, int col, const QString& currencyId, const QString& currencyName) {
    const QString normalizedId = normalizeEconomyItemCompositeId(currencyId);
    if (normalizedId.isEmpty() || currencyName.isEmpty()) {
        return false;
    }
    SpreadsheetCell cell;
    cell.kind = SpreadsheetCellKind::ApiRef;
    cell.currencyId = normalizedId;
    cell.currencyName = currencyName;
    setCellInput(row, col, cell);
    return true;
}

QList<CurrencyRate> SpreadsheetModel::availableCurrencies() const {
    QList<CurrencyRate> currencies;
    if (!m_snapshot || !m_snapshot->valid) {
        return currencies;
    }
    currencies = m_snapshot->ratesById.values();

    const QList<EconomyCategoryDef> categories = poeNinjaEconomyCategories();
    auto categoryOrder = [&categories](const QString& categoryId) -> int {
        for (int i = 0; i < categories.size(); ++i) {
            if (categories.at(i).id == categoryId) {
                return i;
            }
        }
        return static_cast<int>(categories.size());
    };

    std::sort(currencies.begin(), currencies.end(), [&](const CurrencyRate& a, const CurrencyRate& b) {
        const int orderA = categoryOrder(a.categoryId);
        const int orderB = categoryOrder(b.categoryId);
        if (orderA != orderB) {
            return orderA < orderB;
        }
        return a.name.compare(b.name, Qt::CaseInsensitive) < 0;
    });
    return currencies;
}

QString SpreadsheetModel::baseCurrencyId() const {
    return m_baseCurrencyId;
}

QString SpreadsheetModel::baseCurrencyName() const {
    return baseCurrencyNameForId(m_baseCurrencyId);
}

QString SpreadsheetModel::baseCurrencyNameForId(const QString& currencyId) const {
    if (!m_snapshot || !m_snapshot->valid) {
        return currencyId;
    }
    const QString name = m_snapshot->ratesById.value(currencyId).name;
    return name.isEmpty() ? currencyId : name;
}

QString SpreadsheetModel::effectiveBaseCurrencyId(int row, int col) const {
    const QString overrideId = m_cellBaseCurrencyIds.value(makeKey(row, col));
    if (!overrideId.isEmpty()) {
        return overrideId;
    }
    return m_baseCurrencyId;
}

QString SpreadsheetModel::effectiveBaseCurrencyName(int row, int col) const {
    return baseCurrencyNameForId(effectiveBaseCurrencyId(row, col));
}

bool SpreadsheetModel::hasCellBaseCurrencyOverride(int row, int col) const {
    return m_cellBaseCurrencyIds.contains(makeKey(row, col));
}

void SpreadsheetModel::applyCellBaseCurrency(int minRow,
                                             int minCol,
                                             int maxRow,
                                             int maxCol,
                                             const QString& currencyId) {
    const QString normalizedId = normalizeEconomyItemCompositeId(currencyId);
    if (minRow < 0 || minCol < 0 || maxRow < minRow || maxCol < minCol || normalizedId.isEmpty()) {
        return;
    }
    for (int row = minRow; row <= maxRow; ++row) {
        for (int col = minCol; col <= maxCol; ++col) {
            ensureDimensions(row, col);
            m_cellBaseCurrencyIds.insert(makeKey(row, col), normalizedId);
        }
    }
    refreshCellStates();
    emit sheetModified();
}

void SpreadsheetModel::clearCellBaseCurrency(int minRow, int minCol, int maxRow, int maxCol) {
    if (minRow < 0 || minCol < 0 || maxRow < minRow || maxCol < minCol) {
        return;
    }
    for (int row = minRow; row <= maxRow; ++row) {
        for (int col = minCol; col <= maxCol; ++col) {
            m_cellBaseCurrencyIds.remove(makeKey(row, col));
        }
    }
    refreshCellStates();
    emit sheetModified();
}

void SpreadsheetModel::setBaseCurrencyId(const QString& currencyId) {
    const QString normalizedId = normalizeEconomyItemCompositeId(currencyId);
    if (normalizedId.isEmpty() || normalizedId == m_baseCurrencyId) {
        return;
    }
    m_baseCurrencyId = normalizedId;
    refreshCellStates();
    emit baseCurrencyChanged();
}

std::optional<double> SpreadsheetModel::rateValueInBase(const QString& currencyId,
                                                        const QString& baseCurrencyId) const {
    if (!m_snapshot || !m_snapshot->valid || currencyId.isEmpty()) {
        return std::nullopt;
    }
    if (!m_snapshot->ratesById.contains(currencyId)) {
        return std::nullopt;
    }
    const QString baseId = baseCurrencyId.isEmpty() ? m_baseCurrencyId : baseCurrencyId;
    const double currencyDivineValue = m_snapshot->ratesById.value(currencyId).primaryValue;
    const double baseDivineValue = m_snapshot->ratesById.value(baseId).primaryValue;
    if (baseDivineValue <= 0.0) {
        return currencyDivineValue;
    }
    return currencyDivineValue / baseDivineValue;
}

std::optional<double> SpreadsheetModel::convertValueBetweenBases(double value,
                                                                 const QString& fromBaseId,
                                                                 const QString& toBaseId) const {
    if (fromBaseId == toBaseId) {
        return value;
    }
    if (!m_snapshot || !m_snapshot->valid) {
        return std::nullopt;
    }
    const double fromDivineValue = m_snapshot->ratesById.value(fromBaseId).primaryValue;
    const double toDivineValue = m_snapshot->ratesById.value(toBaseId).primaryValue;
    if (toDivineValue <= 0.0) {
        return value;
    }
    return value * (fromDivineValue / toDivineValue);
}

void SpreadsheetModel::applyEconomySnapshot(const EconomySnapshot& snapshot) {
    m_snapshot = snapshot;
    if (m_snapshot && m_snapshot->valid) {
        if (!m_snapshot->ratesById.contains(m_baseCurrencyId)) {
            const QString legacyBase = economyApiItemIdFromCompositeId(m_baseCurrencyId);
            const QString compositeBase =
                economyItemCompositeId(QStringLiteral("currency"), legacyBase);
            if (m_snapshot->ratesById.contains(compositeBase)) {
                m_baseCurrencyId = compositeBase;
            } else if (m_snapshot->ratesById.contains(QStringLiteral("currency:divine"))) {
                m_baseCurrencyId = QStringLiteral("currency:divine");
            } else if (!m_snapshot->ratesById.isEmpty()) {
                m_baseCurrencyId = m_snapshot->ratesById.constBegin().key();
            }
        }
        for (auto it = m_inputs.begin(); it != m_inputs.end(); ++it) {
            SpreadsheetCell& cell = it.value();
            if (cell.kind != SpreadsheetCellKind::ApiRef || cell.currencyId.isEmpty()) {
                continue;
            }
            const CurrencyRate rate = m_snapshot->ratesById.value(cell.currencyId);
            if (!rate.name.isEmpty()) {
                cell.currencyName = rate.name;
            }
        }
    }
    refreshCellStates();
}

const EconomySnapshot* SpreadsheetModel::economySnapshot() const {
    return m_snapshot ? &*m_snapshot : nullptr;
}

QString SpreadsheetModel::errorText(FormulaError error) const {
    switch (error) {
    case FormulaError::Ref:
        return tr("잘못된 셀 참조 (#REF!)");
    case FormulaError::DivZero:
        return tr("0으로 나눌 수 없습니다 (#DIV/0!)");
    case FormulaError::Parse:
        return tr("수식 오류 (#ERR)");
    case FormulaError::None:
        break;
    }
    return {};
}

QString SpreadsheetModel::formatNumber(double value) const {
    if (!std::isfinite(value)) {
        return QStringLiteral("#ERR");
    }
    QString text = QString::number(value, 'f', m_decimalPlaces);
    const int dotIndex = text.indexOf(QLatin1Char('.'));
    if (dotIndex >= 0) {
        while (text.endsWith(QLatin1Char('0'))) {
            text.chop(1);
        }
        if (text.endsWith(QLatin1Char('.'))) {
            text.chop(1);
        }
    }
    return text;
}

int SpreadsheetModel::decimalPlaces() const {
    return m_decimalPlaces;
}

void SpreadsheetModel::setDecimalPlaces(int places) {
    const int clamped = std::clamp(places, kMinDecimalPlaces, kMaxDecimalPlaces);
    if (m_decimalPlaces == clamped) {
        return;
    }
    m_decimalPlaces = clamped;
    refreshCellStates();
    emit decimalPlacesChanged();
}

CellBorderMask SpreadsheetModel::cellBorders(int row, int col) const {
    return m_borders.value(makeKey(row, col), 0);
}

void SpreadsheetModel::setCellBorderMask(int row, int col, CellBorderMask mask) {
    ensureDimensions(row, col);
    const CellKey key = makeKey(row, col);
    if (mask == 0) {
        m_borders.remove(key);
    } else {
        m_borders.insert(key, mask);
    }
}

void SpreadsheetModel::addCellBorder(int row, int col, CellBorder edge) {
    const CellKey key = makeKey(row, col);
    const CellBorderMask current = m_borders.value(key, 0);
    setCellBorderMask(row, col, current | cellBorderMask(edge));
}

void SpreadsheetModel::removeCellBorder(int row, int col, CellBorder edge) {
    const CellKey key = makeKey(row, col);
    if (!m_borders.contains(key)) {
        return;
    }
    const CellBorderMask updated = m_borders.value(key) & ~cellBorderMask(edge);
    setCellBorderMask(row, col, updated);
}

void SpreadsheetModel::emitBorderRangeChanged(int minRow, int minCol, int maxRow, int maxCol) {
    if (minRow > maxRow || minCol > maxCol) {
        return;
    }
    emit dataChanged(index(minRow, minCol), index(maxRow, maxCol));
    emit sheetModified();
}

void SpreadsheetModel::emitColorRangeChanged(int minRow, int minCol, int maxRow, int maxCol) {
    if (minRow > maxRow || minCol > maxCol) {
        return;
    }
    emit dataChanged(index(minRow, minCol), index(maxRow, maxCol),
                     {Qt::BackgroundRole, Qt::ForegroundRole});
    emit sheetModified();
}

std::optional<QColor> SpreadsheetModel::cellBackgroundColor(int row, int col) const {
    const QColor color = m_backgroundColors.value(makeKey(row, col));
    if (!color.isValid()) {
        return std::nullopt;
    }
    return color;
}

std::optional<QColor> SpreadsheetModel::cellForegroundColor(int row, int col) const {
    const QColor color = m_foregroundColors.value(makeKey(row, col));
    if (!color.isValid()) {
        return std::nullopt;
    }
    return color;
}

void SpreadsheetModel::applyBackgroundColor(int minRow,
                                            int minCol,
                                            int maxRow,
                                            int maxCol,
                                            const QColor& color) {
    if (minRow < 0 || minCol < 0 || maxRow < minRow || maxCol < minCol || !color.isValid()) {
        return;
    }
    for (int row = minRow; row <= maxRow; ++row) {
        for (int col = minCol; col <= maxCol; ++col) {
            ensureDimensions(row, col);
            m_backgroundColors.insert(makeKey(row, col), color);
        }
    }
    emitColorRangeChanged(minRow, minCol, maxRow, maxCol);
}

void SpreadsheetModel::applyForegroundColor(int minRow,
                                            int minCol,
                                            int maxRow,
                                            int maxCol,
                                            const QColor& color) {
    if (minRow < 0 || minCol < 0 || maxRow < minRow || maxCol < minCol || !color.isValid()) {
        return;
    }
    for (int row = minRow; row <= maxRow; ++row) {
        for (int col = minCol; col <= maxCol; ++col) {
            ensureDimensions(row, col);
            m_foregroundColors.insert(makeKey(row, col), color);
        }
    }
    emitColorRangeChanged(minRow, minCol, maxRow, maxCol);
}

void SpreadsheetModel::clearCellBackgroundColors(int minRow, int minCol, int maxRow, int maxCol) {
    if (minRow < 0 || minCol < 0 || maxRow < minRow || maxCol < minCol) {
        return;
    }
    for (int row = minRow; row <= maxRow; ++row) {
        for (int col = minCol; col <= maxCol; ++col) {
            m_backgroundColors.remove(makeKey(row, col));
        }
    }
    emitColorRangeChanged(minRow, minCol, maxRow, maxCol);
}

void SpreadsheetModel::clearCellForegroundColors(int minRow, int minCol, int maxRow, int maxCol) {
    if (minRow < 0 || minCol < 0 || maxRow < minRow || maxCol < minCol) {
        return;
    }
    for (int row = minRow; row <= maxRow; ++row) {
        for (int col = minCol; col <= maxCol; ++col) {
            m_foregroundColors.remove(makeKey(row, col));
        }
    }
    emitColorRangeChanged(minRow, minCol, maxRow, maxCol);
}

void SpreadsheetModel::clearCellColors(int minRow, int minCol, int maxRow, int maxCol) {
    if (minRow < 0 || minCol < 0 || maxRow < minRow || maxCol < minCol) {
        return;
    }
    for (int row = minRow; row <= maxRow; ++row) {
        for (int col = minCol; col <= maxCol; ++col) {
            const CellKey key = makeKey(row, col);
            m_backgroundColors.remove(key);
            m_foregroundColors.remove(key);
        }
    }
    emitColorRangeChanged(minRow, minCol, maxRow, maxCol);
}

void SpreadsheetModel::applyBorderPreset(int minRow,
                                         int minCol,
                                         int maxRow,
                                         int maxCol,
                                         SpreadsheetBorderPreset preset) {
    if (minRow < 0 || minCol < 0 || maxRow < minRow || maxCol < minCol) {
        return;
    }

    switch (preset) {
    case SpreadsheetBorderPreset::None:
        for (int row = minRow; row <= maxRow; ++row) {
            for (int col = minCol; col <= maxCol; ++col) {
                setCellBorderMask(row, col, 0);
            }
        }
        break;
    case SpreadsheetBorderPreset::All:
        for (int row = minRow; row <= maxRow; ++row) {
            for (int col = minCol; col <= maxCol; ++col) {
                setCellBorderMask(row, col,
                                  cellBorderMask(CellBorder::Top) | cellBorderMask(CellBorder::Right)
                                      | cellBorderMask(CellBorder::Bottom) | cellBorderMask(CellBorder::Left));
            }
        }
        break;
    case SpreadsheetBorderPreset::Outside:
        for (int row = minRow; row <= maxRow; ++row) {
            for (int col = minCol; col <= maxCol; ++col) {
                CellBorderMask mask = m_borders.value(makeKey(row, col), 0);
                if (row == minRow) {
                    mask |= cellBorderMask(CellBorder::Top);
                }
                if (row == maxRow) {
                    mask |= cellBorderMask(CellBorder::Bottom);
                }
                if (col == minCol) {
                    mask |= cellBorderMask(CellBorder::Left);
                }
                if (col == maxCol) {
                    mask |= cellBorderMask(CellBorder::Right);
                }
                setCellBorderMask(row, col, mask);
            }
        }
        break;
    case SpreadsheetBorderPreset::Inside:
        if (minRow == maxRow && minCol == maxCol) {
            return;
        }
        for (int row = minRow; row <= maxRow; ++row) {
            for (int col = minCol; col <= maxCol; ++col) {
                CellBorderMask mask = m_borders.value(makeKey(row, col), 0);
                if (row > minRow) {
                    mask |= cellBorderMask(CellBorder::Top);
                }
                if (row < maxRow) {
                    mask |= cellBorderMask(CellBorder::Bottom);
                }
                if (col > minCol) {
                    mask |= cellBorderMask(CellBorder::Left);
                }
                if (col < maxCol) {
                    mask |= cellBorderMask(CellBorder::Right);
                }
                setCellBorderMask(row, col, mask);
            }
        }
        break;
    case SpreadsheetBorderPreset::Top:
        for (int col = minCol; col <= maxCol; ++col) {
            addCellBorder(minRow, col, CellBorder::Top);
        }
        break;
    case SpreadsheetBorderPreset::Bottom:
        for (int col = minCol; col <= maxCol; ++col) {
            addCellBorder(maxRow, col, CellBorder::Bottom);
        }
        break;
    case SpreadsheetBorderPreset::Left:
        for (int row = minRow; row <= maxRow; ++row) {
            addCellBorder(row, minCol, CellBorder::Left);
        }
        break;
    case SpreadsheetBorderPreset::Right:
        for (int row = minRow; row <= maxRow; ++row) {
            addCellBorder(row, maxCol, CellBorder::Right);
        }
        break;
    }

    emitBorderRangeChanged(minRow, minCol, maxRow, maxCol);
}

QString SpreadsheetModel::adjustFormulaForMove(const QString& raw,
                                               int srcMinRow,
                                               int srcMinCol,
                                               int srcMaxRow,
                                               int srcMaxCol,
                                               int deltaRow,
                                               int deltaCol) {
    if (raw.isEmpty()) {
        return raw;
    }
    const bool hasEquals = raw.startsWith(QLatin1Char('='));
    const QString body = hasEquals ? raw.mid(1) : raw;
    const QString shifted = FormulaEvaluator::shiftFormulaReferencesInRegion(
        body, srcMinRow, srcMinCol, srcMaxRow, srcMaxCol, deltaRow, deltaCol);
    if (shifted.isEmpty()) {
        return raw;
    }
    return hasEquals ? QStringLiteral("=") + shifted : shifted;
}

QString SpreadsheetModel::adjustFormulaForRowInsert(const QString& raw, int insertRow) {
    if (raw.isEmpty()) {
        return raw;
    }
    const bool hasEquals = raw.startsWith(QLatin1Char('='));
    const QString body = hasEquals ? raw.mid(1) : raw;
    const QString shifted = FormulaEvaluator::shiftFormulaReferencesAtOrAfter(body, insertRow, -1, 1, 0);
    if (shifted.isEmpty()) {
        return raw;
    }
    return hasEquals ? QStringLiteral("=") + shifted : shifted;
}

QString SpreadsheetModel::adjustFormulaForColumnInsert(const QString& raw, int insertCol) {
    if (raw.isEmpty()) {
        return raw;
    }
    const bool hasEquals = raw.startsWith(QLatin1Char('='));
    const QString body = hasEquals ? raw.mid(1) : raw;
    const QString shifted = FormulaEvaluator::shiftFormulaReferencesAtOrAfter(body, -1, insertCol, 0, 1);
    if (shifted.isEmpty()) {
        return raw;
    }
    return hasEquals ? QStringLiteral("=") + shifted : shifted;
}

QString SpreadsheetModel::adjustFormulaForRowDelete(const QString& raw, int deleteMinRow, int deleteMaxRow) {
    if (raw.isEmpty()) {
        return raw;
    }
    const bool hasEquals = raw.startsWith(QLatin1Char('='));
    const QString body = hasEquals ? raw.mid(1) : raw;
    const QString shifted =
        FormulaEvaluator::shiftFormulaReferencesForRowDelete(body, deleteMinRow, deleteMaxRow);
    if (shifted.isEmpty()) {
        return raw;
    }
    return hasEquals ? QStringLiteral("=") + shifted : shifted;
}

QString SpreadsheetModel::adjustFormulaForColumnDelete(const QString& raw, int deleteMinCol, int deleteMaxCol) {
    if (raw.isEmpty()) {
        return raw;
    }
    const bool hasEquals = raw.startsWith(QLatin1Char('='));
    const QString body = hasEquals ? raw.mid(1) : raw;
    const QString shifted =
        FormulaEvaluator::shiftFormulaReferencesForColumnDelete(body, deleteMinCol, deleteMaxCol);
    if (shifted.isEmpty()) {
        return raw;
    }
    return hasEquals ? QStringLiteral("=") + shifted : shifted;
}

void SpreadsheetModel::adjustAllFormulasForRowInsert(int insertRow) {
    for (auto it = m_inputs.begin(); it != m_inputs.end(); ++it) {
        if (it->kind != SpreadsheetCellKind::Formula) {
            continue;
        }
        it->raw = adjustFormulaForRowInsert(it->raw, insertRow);
    }
}

void SpreadsheetModel::adjustAllFormulasForColumnInsert(int insertCol) {
    for (auto it = m_inputs.begin(); it != m_inputs.end(); ++it) {
        if (it->kind != SpreadsheetCellKind::Formula) {
            continue;
        }
        it->raw = adjustFormulaForColumnInsert(it->raw, insertCol);
    }
}

void SpreadsheetModel::adjustAllFormulasForRowDelete(int minRow, int maxRow) {
    for (auto it = m_inputs.begin(); it != m_inputs.end(); ++it) {
        if (it->kind != SpreadsheetCellKind::Formula) {
            continue;
        }
        it->raw = adjustFormulaForRowDelete(it->raw, minRow, maxRow);
    }
}

void SpreadsheetModel::adjustAllFormulasForColumnDelete(int minCol, int maxCol) {
    for (auto it = m_inputs.begin(); it != m_inputs.end(); ++it) {
        if (it->kind != SpreadsheetCellKind::Formula) {
            continue;
        }
        it->raw = adjustFormulaForColumnDelete(it->raw, minCol, maxCol);
    }
}

void SpreadsheetModel::shiftAllMapsForRowInsert(int atRow) {
    auto shiftInputs = [this, atRow]() {
        QMap<CellKey, SpreadsheetCell> updated;
        for (auto it = m_inputs.constBegin(); it != m_inputs.constEnd(); ++it) {
            const int row = it.key().first;
            const int col = it.key().second;
            updated.insert(makeKey(row >= atRow ? row + 1 : row, col), it.value());
        }
        m_inputs = std::move(updated);
    };
    auto shiftBorders = [this, atRow]() {
        QMap<CellKey, CellBorderMask> updated;
        for (auto it = m_borders.constBegin(); it != m_borders.constEnd(); ++it) {
            const int row = it.key().first;
            const int col = it.key().second;
            updated.insert(makeKey(row >= atRow ? row + 1 : row, col), it.value());
        }
        m_borders = std::move(updated);
    };
    auto shiftBackgrounds = [this, atRow]() {
        QMap<CellKey, QColor> updated;
        for (auto it = m_backgroundColors.constBegin(); it != m_backgroundColors.constEnd(); ++it) {
            const int row = it.key().first;
            const int col = it.key().second;
            updated.insert(makeKey(row >= atRow ? row + 1 : row, col), it.value());
        }
        m_backgroundColors = std::move(updated);
    };
    auto shiftForegrounds = [this, atRow]() {
        QMap<CellKey, QColor> updated;
        for (auto it = m_foregroundColors.constBegin(); it != m_foregroundColors.constEnd(); ++it) {
            const int row = it.key().first;
            const int col = it.key().second;
            updated.insert(makeKey(row >= atRow ? row + 1 : row, col), it.value());
        }
        m_foregroundColors = std::move(updated);
    };
    auto shiftBaseCurrencies = [this, atRow]() {
        QMap<CellKey, QString> updated;
        for (auto it = m_cellBaseCurrencyIds.constBegin(); it != m_cellBaseCurrencyIds.constEnd(); ++it) {
            const int row = it.key().first;
            const int col = it.key().second;
            updated.insert(makeKey(row >= atRow ? row + 1 : row, col), it.value());
        }
        m_cellBaseCurrencyIds = std::move(updated);
    };

    shiftInputs();
    shiftBorders();
    shiftBackgrounds();
    shiftForegrounds();
    shiftBaseCurrencies();
}

void SpreadsheetModel::shiftAllMapsForColumnInsert(int atCol) {
    auto shiftInputs = [this, atCol]() {
        QMap<CellKey, SpreadsheetCell> updated;
        for (auto it = m_inputs.constBegin(); it != m_inputs.constEnd(); ++it) {
            const int row = it.key().first;
            const int col = it.key().second;
            updated.insert(makeKey(row, col >= atCol ? col + 1 : col), it.value());
        }
        m_inputs = std::move(updated);
    };
    auto shiftBorders = [this, atCol]() {
        QMap<CellKey, CellBorderMask> updated;
        for (auto it = m_borders.constBegin(); it != m_borders.constEnd(); ++it) {
            const int row = it.key().first;
            const int col = it.key().second;
            updated.insert(makeKey(row, col >= atCol ? col + 1 : col), it.value());
        }
        m_borders = std::move(updated);
    };
    auto shiftBackgrounds = [this, atCol]() {
        QMap<CellKey, QColor> updated;
        for (auto it = m_backgroundColors.constBegin(); it != m_backgroundColors.constEnd(); ++it) {
            const int row = it.key().first;
            const int col = it.key().second;
            updated.insert(makeKey(row, col >= atCol ? col + 1 : col), it.value());
        }
        m_backgroundColors = std::move(updated);
    };
    auto shiftForegrounds = [this, atCol]() {
        QMap<CellKey, QColor> updated;
        for (auto it = m_foregroundColors.constBegin(); it != m_foregroundColors.constEnd(); ++it) {
            const int row = it.key().first;
            const int col = it.key().second;
            updated.insert(makeKey(row, col >= atCol ? col + 1 : col), it.value());
        }
        m_foregroundColors = std::move(updated);
    };
    auto shiftBaseCurrencies = [this, atCol]() {
        QMap<CellKey, QString> updated;
        for (auto it = m_cellBaseCurrencyIds.constBegin(); it != m_cellBaseCurrencyIds.constEnd(); ++it) {
            const int row = it.key().first;
            const int col = it.key().second;
            updated.insert(makeKey(row, col >= atCol ? col + 1 : col), it.value());
        }
        m_cellBaseCurrencyIds = std::move(updated);
    };

    shiftInputs();
    shiftBorders();
    shiftBackgrounds();
    shiftForegrounds();
    shiftBaseCurrencies();
}

void SpreadsheetModel::shiftAllMapsForRowDelete(int minRow, int maxRow) {
    const int deleteCount = maxRow - minRow + 1;

    auto shiftInputs = [this, minRow, maxRow, deleteCount]() {
        QMap<CellKey, SpreadsheetCell> updated;
        for (auto it = m_inputs.constBegin(); it != m_inputs.constEnd(); ++it) {
            const int row = it.key().first;
            const int col = it.key().second;
            if (row >= minRow && row <= maxRow) {
                continue;
            }
            const int newRow = row > maxRow ? row - deleteCount : row;
            updated.insert(makeKey(newRow, col), it.value());
        }
        m_inputs = std::move(updated);
    };
    auto shiftBorders = [this, minRow, maxRow, deleteCount]() {
        QMap<CellKey, CellBorderMask> updated;
        for (auto it = m_borders.constBegin(); it != m_borders.constEnd(); ++it) {
            const int row = it.key().first;
            const int col = it.key().second;
            if (row >= minRow && row <= maxRow) {
                continue;
            }
            const int newRow = row > maxRow ? row - deleteCount : row;
            updated.insert(makeKey(newRow, col), it.value());
        }
        m_borders = std::move(updated);
    };
    auto shiftBackgrounds = [this, minRow, maxRow, deleteCount]() {
        QMap<CellKey, QColor> updated;
        for (auto it = m_backgroundColors.constBegin(); it != m_backgroundColors.constEnd(); ++it) {
            const int row = it.key().first;
            const int col = it.key().second;
            if (row >= minRow && row <= maxRow) {
                continue;
            }
            const int newRow = row > maxRow ? row - deleteCount : row;
            updated.insert(makeKey(newRow, col), it.value());
        }
        m_backgroundColors = std::move(updated);
    };
    auto shiftForegrounds = [this, minRow, maxRow, deleteCount]() {
        QMap<CellKey, QColor> updated;
        for (auto it = m_foregroundColors.constBegin(); it != m_foregroundColors.constEnd(); ++it) {
            const int row = it.key().first;
            const int col = it.key().second;
            if (row >= minRow && row <= maxRow) {
                continue;
            }
            const int newRow = row > maxRow ? row - deleteCount : row;
            updated.insert(makeKey(newRow, col), it.value());
        }
        m_foregroundColors = std::move(updated);
    };
    auto shiftBaseCurrencies = [this, minRow, maxRow, deleteCount]() {
        QMap<CellKey, QString> updated;
        for (auto it = m_cellBaseCurrencyIds.constBegin(); it != m_cellBaseCurrencyIds.constEnd(); ++it) {
            const int row = it.key().first;
            const int col = it.key().second;
            if (row >= minRow && row <= maxRow) {
                continue;
            }
            const int newRow = row > maxRow ? row - deleteCount : row;
            updated.insert(makeKey(newRow, col), it.value());
        }
        m_cellBaseCurrencyIds = std::move(updated);
    };

    shiftInputs();
    shiftBorders();
    shiftBackgrounds();
    shiftForegrounds();
    shiftBaseCurrencies();
}

void SpreadsheetModel::shiftAllMapsForColumnDelete(int minCol, int maxCol) {
    const int deleteCount = maxCol - minCol + 1;

    auto shiftInputs = [this, minCol, maxCol, deleteCount]() {
        QMap<CellKey, SpreadsheetCell> updated;
        for (auto it = m_inputs.constBegin(); it != m_inputs.constEnd(); ++it) {
            const int row = it.key().first;
            const int col = it.key().second;
            if (col >= minCol && col <= maxCol) {
                continue;
            }
            const int newCol = col > maxCol ? col - deleteCount : col;
            updated.insert(makeKey(row, newCol), it.value());
        }
        m_inputs = std::move(updated);
    };
    auto shiftBorders = [this, minCol, maxCol, deleteCount]() {
        QMap<CellKey, CellBorderMask> updated;
        for (auto it = m_borders.constBegin(); it != m_borders.constEnd(); ++it) {
            const int row = it.key().first;
            const int col = it.key().second;
            if (col >= minCol && col <= maxCol) {
                continue;
            }
            const int newCol = col > maxCol ? col - deleteCount : col;
            updated.insert(makeKey(row, newCol), it.value());
        }
        m_borders = std::move(updated);
    };
    auto shiftBackgrounds = [this, minCol, maxCol, deleteCount]() {
        QMap<CellKey, QColor> updated;
        for (auto it = m_backgroundColors.constBegin(); it != m_backgroundColors.constEnd(); ++it) {
            const int row = it.key().first;
            const int col = it.key().second;
            if (col >= minCol && col <= maxCol) {
                continue;
            }
            const int newCol = col > maxCol ? col - deleteCount : col;
            updated.insert(makeKey(row, newCol), it.value());
        }
        m_backgroundColors = std::move(updated);
    };
    auto shiftForegrounds = [this, minCol, maxCol, deleteCount]() {
        QMap<CellKey, QColor> updated;
        for (auto it = m_foregroundColors.constBegin(); it != m_foregroundColors.constEnd(); ++it) {
            const int row = it.key().first;
            const int col = it.key().second;
            if (col >= minCol && col <= maxCol) {
                continue;
            }
            const int newCol = col > maxCol ? col - deleteCount : col;
            updated.insert(makeKey(row, newCol), it.value());
        }
        m_foregroundColors = std::move(updated);
    };
    auto shiftBaseCurrencies = [this, minCol, maxCol, deleteCount]() {
        QMap<CellKey, QString> updated;
        for (auto it = m_cellBaseCurrencyIds.constBegin(); it != m_cellBaseCurrencyIds.constEnd(); ++it) {
            const int row = it.key().first;
            const int col = it.key().second;
            if (col >= minCol && col <= maxCol) {
                continue;
            }
            const int newCol = col > maxCol ? col - deleteCount : col;
            updated.insert(makeKey(row, newCol), it.value());
        }
        m_cellBaseCurrencyIds = std::move(updated);
    };

    shiftInputs();
    shiftBorders();
    shiftBackgrounds();
    shiftForegrounds();
    shiftBaseCurrencies();
}

bool SpreadsheetModel::insertRow(int atRow) {
    if (atRow < 0) {
        return false;
    }
    if (atRow > m_rowCount) {
        atRow = m_rowCount;
    }

    beginInsertRows(QModelIndex(), atRow, atRow);
    ++m_rowCount;

    adjustAllFormulasForRowInsert(atRow);
    shiftAllMapsForRowInsert(atRow);

    refreshCellStates();
    endInsertRows();
    emit sheetModified();
    return true;
}

bool SpreadsheetModel::insertColumn(int atCol) {
    if (atCol < 0) {
        return false;
    }
    if (atCol > m_columnCount) {
        atCol = m_columnCount;
    }

    beginInsertColumns(QModelIndex(), atCol, atCol);
    ++m_columnCount;

    adjustAllFormulasForColumnInsert(atCol);
    shiftAllMapsForColumnInsert(atCol);

    refreshCellStates();
    endInsertColumns();
    emit sheetModified();
    return true;
}

bool SpreadsheetModel::deleteRows(int minRow, int maxRow) {
    if (minRow < 0 || maxRow < minRow || maxRow >= m_rowCount) {
        return false;
    }
    const int deleteCount = maxRow - minRow + 1;
    if (m_rowCount <= deleteCount) {
        return false;
    }

    beginRemoveRows(QModelIndex(), minRow, maxRow);

    adjustAllFormulasForRowDelete(minRow, maxRow);
    shiftAllMapsForRowDelete(minRow, maxRow);
    m_rowCount -= deleteCount;

    refreshCellStates();
    endRemoveRows();
    emit sheetModified();
    return true;
}

bool SpreadsheetModel::deleteColumns(int minCol, int maxCol) {
    if (minCol < 0 || maxCol < minCol || maxCol >= m_columnCount) {
        return false;
    }
    const int deleteCount = maxCol - minCol + 1;
    if (m_columnCount <= deleteCount) {
        return false;
    }

    beginRemoveColumns(QModelIndex(), minCol, maxCol);

    adjustAllFormulasForColumnDelete(minCol, maxCol);
    shiftAllMapsForColumnDelete(minCol, maxCol);
    m_columnCount -= deleteCount;

    refreshCellStates();
    endRemoveColumns();
    emit sheetModified();
    return true;
}

void SpreadsheetModel::clearCellMapsAt(QMap<CellKey, SpreadsheetCell>& inputs,
                                       QMap<CellKey, CellBorderMask>& borders,
                                       QMap<CellKey, QColor>& backgroundColors,
                                       QMap<CellKey, QColor>& foregroundColors,
                                       QMap<CellKey, QString>& cellBaseCurrencyIds,
                                       int row,
                                       int col) {
    const CellKey key = qMakePair(row, col);
    inputs.remove(key);
    borders.remove(key);
    backgroundColors.remove(key);
    foregroundColors.remove(key);
    cellBaseCurrencyIds.remove(key);
}

bool SpreadsheetModel::moveCellRange(int srcMinRow,
                                     int srcMinCol,
                                     int srcMaxRow,
                                     int srcMaxCol,
                                     int dstMinRow,
                                     int dstMinCol) {
    if (srcMinRow < 0 || srcMinCol < 0 || srcMaxRow < srcMinRow || srcMaxCol < srcMinCol) {
        return false;
    }

    const int deltaRow = dstMinRow - srcMinRow;
    const int deltaCol = dstMinCol - srcMinCol;
    if (deltaRow == 0 && deltaCol == 0) {
        return false;
    }

    const int blockHeight = srcMaxRow - srcMinRow + 1;
    const int blockWidth = srcMaxCol - srcMinCol + 1;
    const int dstMaxRow = dstMinRow + blockHeight - 1;
    const int dstMaxCol = dstMinCol + blockWidth - 1;
    if (dstMinRow < 0 || dstMinCol < 0) {
        return false;
    }

    ensureDimensions(dstMaxRow, dstMaxCol);

    struct MovedCellPayload {
        SpreadsheetCell input;
        CellBorderMask borders = 0;
        QColor background;
        QColor foreground;
        QString cellBaseCurrencyId;
    };

    QMap<CellKey, SpreadsheetCell> inputs = m_inputs;
    QMap<CellKey, CellBorderMask> borders = m_borders;
    QMap<CellKey, QColor> backgroundColors = m_backgroundColors;
    QMap<CellKey, QColor> foregroundColors = m_foregroundColors;
    QMap<CellKey, QString> cellBaseCurrencyIds = m_cellBaseCurrencyIds;

    for (auto it = inputs.begin(); it != inputs.end(); ++it) {
        if (it->kind != SpreadsheetCellKind::Formula) {
            continue;
        }
        it->raw = adjustFormulaForMove(it->raw, srcMinRow, srcMinCol, srcMaxRow, srcMaxCol, deltaRow, deltaCol);
    }

    QMap<QPair<int, int>, MovedCellPayload> movedBlock;
    for (int row = srcMinRow; row <= srcMaxRow; ++row) {
        for (int col = srcMinCol; col <= srcMaxCol; ++col) {
            MovedCellPayload payload;
            payload.input = inputs.value(makeKey(row, col));
            payload.borders = borders.value(makeKey(row, col), 0);
            payload.background = backgroundColors.value(makeKey(row, col));
            payload.foreground = foregroundColors.value(makeKey(row, col));
            payload.cellBaseCurrencyId = cellBaseCurrencyIds.value(makeKey(row, col));
            movedBlock.insert(qMakePair(row - srcMinRow, col - srcMinCol), payload);
        }
    }

    for (int row = srcMinRow; row <= srcMaxRow; ++row) {
        for (int col = srcMinCol; col <= srcMaxCol; ++col) {
            clearCellMapsAt(inputs, borders, backgroundColors, foregroundColors, cellBaseCurrencyIds, row, col);
        }
    }

    for (int row = dstMinRow; row <= dstMaxRow; ++row) {
        for (int col = dstMinCol; col <= dstMaxCol; ++col) {
            clearCellMapsAt(inputs, borders, backgroundColors, foregroundColors, cellBaseCurrencyIds, row, col);
        }
    }

    for (auto it = movedBlock.constBegin(); it != movedBlock.constEnd(); ++it) {
        const int row = dstMinRow + it.key().first;
        const int col = dstMinCol + it.key().second;
        const CellKey key = makeKey(row, col);
        const MovedCellPayload& payload = it.value();
        if (payload.input.kind != SpreadsheetCellKind::Empty) {
            inputs.insert(key, payload.input);
        }
        if (payload.borders != 0) {
            borders.insert(key, payload.borders);
        }
        if (payload.background.isValid()) {
            backgroundColors.insert(key, payload.background);
        }
        if (payload.foreground.isValid()) {
            foregroundColors.insert(key, payload.foreground);
        }
        if (!payload.cellBaseCurrencyId.isEmpty()) {
            cellBaseCurrencyIds.insert(key, payload.cellBaseCurrencyId);
        }
    }

    m_inputs = std::move(inputs);
    m_borders = std::move(borders);
    m_backgroundColors = std::move(backgroundColors);
    m_foregroundColors = std::move(foregroundColors);
    m_cellBaseCurrencyIds = std::move(cellBaseCurrencyIds);
    refreshCellStates();
    emit sheetModified();
    return true;
}

std::optional<double> SpreadsheetModel::resolveNumericValue(int row,
                                                            int col,
                                                            std::unordered_set<std::string>& visiting) {
    return resolveNumericValueInBase(row, col, visiting, effectiveBaseCurrencyId(row, col));
}

std::optional<double> SpreadsheetModel::resolveNumericValueInBase(int row,
                                                                  int col,
                                                                  std::unordered_set<std::string>& visiting,
                                                                  const QString& targetBaseId) {
    const CellKey key = makeKey(row, col);
    const SpreadsheetCell input = m_inputs.value(key);
    const QString sourceBaseId = effectiveBaseCurrencyId(row, col);

    switch (input.kind) {
    case SpreadsheetCellKind::Empty:
        return std::nullopt;
    case SpreadsheetCellKind::Number: {
        bool ok = false;
        const double value = input.raw.toDouble(&ok);
        if (!ok) {
            return std::nullopt;
        }
        return value;
    }
    case SpreadsheetCellKind::Text:
        return std::nullopt;
    case SpreadsheetCellKind::ApiRef: {
        if (!m_snapshot || !m_snapshot->valid) {
            return std::nullopt;
        }
        const std::optional<double> valueInSourceBase = rateValueInBase(input.currencyId, sourceBaseId);
        if (!valueInSourceBase.has_value()) {
            return std::nullopt;
        }
        return convertValueBetweenBases(*valueInSourceBase, sourceBaseId, targetBaseId);
    }
    case SpreadsheetCellKind::Formula: {
        const auto resolver = [this, &visiting, targetBaseId](int refRow, int refCol) -> std::optional<double> {
            return resolveNumericValueInBase(refRow, refCol, visiting, targetBaseId);
        };
        const FormulaResult result = FormulaEvaluator::evaluate(input.raw, resolver, &visiting);
        if (!result.ok) {
            return std::nullopt;
        }
        return result.value;
    }
    }
    return std::nullopt;
}

void SpreadsheetModel::recalculateCell(int row, int col, std::unordered_set<std::string>& visiting) {
    const CellKey key = makeKey(row, col);
    const SpreadsheetCell input = m_inputs.value(key);
    SpreadsheetCellState state;
    state.input = input;

    switch (input.kind) {
    case SpreadsheetCellKind::Empty:
        state.displayText.clear();
        break;
    case SpreadsheetCellKind::Number: {
        bool ok = false;
        const double value = input.raw.toDouble(&ok);
        if (!ok) {
            state.hasError = true;
            state.error = FormulaError::Parse;
            state.displayText = QStringLiteral("#ERR");
        } else {
            state.numericValue = value;
            state.displayText = formatNumber(value);
        }
        break;
    }
    case SpreadsheetCellKind::Text:
        state.displayText = input.raw;
        break;
    case SpreadsheetCellKind::ApiRef:
        if (!m_snapshot || !m_snapshot->valid || !m_snapshot->ratesById.contains(input.currencyId)) {
            state.displayText = input.currencyName;
            state.hasError = true;
            state.error = FormulaError::Ref;
        } else if (const std::optional<double> value =
                       rateValueInBase(input.currencyId, effectiveBaseCurrencyId(row, col))) {
            state.numericValue = *value;
            state.displayText = formatNumber(*value);
        } else {
            state.displayText = input.currencyName;
            state.hasError = true;
            state.error = FormulaError::Ref;
        }
        break;
    case SpreadsheetCellKind::Formula: {
        const QString targetBase = effectiveBaseCurrencyId(row, col);
        const auto resolver = [this, &visiting, targetBase](int refRow, int refCol) -> std::optional<double> {
            return resolveNumericValueInBase(refRow, refCol, visiting, targetBase);
        };
        const FormulaResult result = FormulaEvaluator::evaluate(input.raw, resolver, &visiting);
        if (!result.ok) {
            state.hasError = true;
            state.error = result.error;
            switch (result.error) {
            case FormulaError::Ref:
                state.displayText = QStringLiteral("#REF!");
                break;
            case FormulaError::DivZero:
                state.displayText = QStringLiteral("#DIV/0!");
                break;
            default:
                state.displayText = QStringLiteral("#ERR");
                break;
            }
        } else {
            state.numericValue = result.value;
            state.displayText = formatNumber(result.value);
        }
        break;
    }
    }

    m_states.insert(key, state);
}

void SpreadsheetModel::rebuildCellStates() {
    m_states.clear();
    std::unordered_set<std::string> visiting;
    for (auto it = m_inputs.constBegin(); it != m_inputs.constEnd(); ++it) {
        recalculateCell(it.key().first, it.key().second, visiting);
    }
}

void SpreadsheetModel::refreshCellStates() {
    rebuildCellStates();
    if (rowCount() > 0 && columnCount() > 0) {
        emit dataChanged(index(0, 0), index(rowCount() - 1, columnCount() - 1));
    }
}

void SpreadsheetModel::recalculateAll() {
    beginResetModel();
    rebuildCellStates();
    endResetModel();
}

FormulaResult SpreadsheetModel::evaluateFormulaAtCell(const QString& expression, int evalRow, int evalCol) {
    std::unordered_set<std::string> visiting;
    const QString targetBase = effectiveBaseCurrencyId(evalRow, evalCol);
    const auto resolver = [this, &visiting, targetBase](int refRow, int refCol) -> std::optional<double> {
        return resolveNumericValueInBase(refRow, refCol, visiting, targetBase);
    };
    return FormulaEvaluator::evaluate(expression, resolver, &visiting);
}

nlohmann::json SpreadsheetModel::toJson() const {
    nlohmann::json cells = nlohmann::json::array();
    for (auto it = m_inputs.constBegin(); it != m_inputs.constEnd(); ++it) {
        const SpreadsheetCell& cell = it.value();
        if (cell.kind == SpreadsheetCellKind::Empty) {
            continue;
        }
        nlohmann::json entry;
        entry["col"] = it.key().second;
        entry["row"] = it.key().first;
        entry["kind"] = kindToString(cell.kind).toStdString();
        entry["raw"] = cell.raw.toStdString();
        if (cell.kind == SpreadsheetCellKind::ApiRef) {
            entry["currencyId"] = cell.currencyId.toStdString();
            entry["currencyName"] = cell.currencyName.toStdString();
        }
        cells.push_back(std::move(entry));
    }

    nlohmann::json borders = nlohmann::json::array();
    for (auto it = m_borders.constBegin(); it != m_borders.constEnd(); ++it) {
        if (it.value() == 0) {
            continue;
        }
        nlohmann::json entry;
        entry["row"] = it.key().first;
        entry["col"] = it.key().second;
        entry["mask"] = it.value();
        borders.push_back(std::move(entry));
    }

    nlohmann::json colors = nlohmann::json::array();
    QSet<CellKey> styledKeys;
    for (auto it = m_backgroundColors.constBegin(); it != m_backgroundColors.constEnd(); ++it) {
        styledKeys.insert(it.key());
    }
    for (auto it = m_foregroundColors.constBegin(); it != m_foregroundColors.constEnd(); ++it) {
        styledKeys.insert(it.key());
    }
    for (const CellKey& key : styledKeys) {
        const QColor bg = m_backgroundColors.value(key);
        const QColor fg = m_foregroundColors.value(key);
        if (!bg.isValid() && !fg.isValid()) {
            continue;
        }
        nlohmann::json entry;
        entry["row"] = key.first;
        entry["col"] = key.second;
        if (bg.isValid()) {
            entry["bg"] = colorToJsonHex(bg).toStdString();
        }
        if (fg.isValid()) {
            entry["fg"] = colorToJsonHex(fg).toStdString();
        }
        colors.push_back(std::move(entry));
    }

    if (borders.empty() && colors.empty() && m_cellBaseCurrencyIds.isEmpty()) {
        return cells;
    }

    nlohmann::json cellBaseCurrencies = nlohmann::json::array();
    for (auto it = m_cellBaseCurrencyIds.constBegin(); it != m_cellBaseCurrencyIds.constEnd(); ++it) {
        if (it.value().isEmpty()) {
            continue;
        }
        nlohmann::json entry;
        entry["row"] = it.key().first;
        entry["col"] = it.key().second;
        entry["currencyId"] = it.value().toStdString();
        cellBaseCurrencies.push_back(std::move(entry));
    }

    int version = 2;
    if (!colors.empty()) {
        version = 3;
    }
    if (!cellBaseCurrencies.empty()) {
        version = 4;
    }

    nlohmann::json document;
    document["version"] = version;
    document["cells"] = std::move(cells);
    if (!borders.empty()) {
        document["borders"] = std::move(borders);
    }
    if (!colors.empty()) {
        document["colors"] = std::move(colors);
    }
    if (!cellBaseCurrencies.empty()) {
        document["cellBaseCurrencies"] = std::move(cellBaseCurrencies);
    }
    return document;
}

void SpreadsheetModel::fromJson(const nlohmann::json& document) {
    m_inputs.clear();
    m_borders.clear();
    m_backgroundColors.clear();
    m_foregroundColors.clear();
    m_cellBaseCurrencyIds.clear();
    m_states.clear();

    auto loadCells = [this](const nlohmann::json& cells) {
        if (!cells.is_array()) {
            return;
        }
        for (const auto& entry : cells) {
            const int row = entry.value("row", -1);
            const int col = entry.value("col", -1);
            if (row < 0 || col < 0) {
                continue;
            }
            SpreadsheetCell cell;
            cell.kind = kindFromString(QString::fromStdString(entry.value("kind", std::string())));
            cell.raw = QString::fromStdString(entry.value("raw", std::string()));
            if (entry.contains("currencyId")) {
                if (entry["currencyId"].is_string()) {
                    cell.currencyId = QString::fromStdString(entry["currencyId"].get<std::string>());
                } else if (entry["currencyId"].is_number()) {
                    cell.currencyId = QString::number(entry["currencyId"].get<int>());
                }
            }
            cell.currencyName = QString::fromStdString(entry.value("currencyName", std::string()));
            cell.currencyId = normalizeEconomyItemCompositeId(cell.currencyId);
            ensureDimensions(row, col);
            m_inputs.insert(makeKey(row, col), cell);
        }
    };

    auto loadBorders = [this](const nlohmann::json& borders) {
        if (!borders.is_array()) {
            return;
        }
        for (const auto& entry : borders) {
            const int row = entry.value("row", -1);
            const int col = entry.value("col", -1);
            const int mask = entry.value("mask", 0);
            if (row < 0 || col < 0 || mask <= 0) {
                continue;
            }
            ensureDimensions(row, col);
            m_borders.insert(makeKey(row, col), static_cast<CellBorderMask>(mask));
        }
    };

    auto loadColors = [this](const nlohmann::json& colors) {
        if (!colors.is_array()) {
            return;
        }
        for (const auto& entry : colors) {
            const int row = entry.value("row", -1);
            const int col = entry.value("col", -1);
            if (row < 0 || col < 0) {
                continue;
            }
            ensureDimensions(row, col);
            const CellKey key = makeKey(row, col);
            const QColor bg = colorFromJsonHex(
                QString::fromStdString(entry.value("bg", std::string())));
            const QColor fg = colorFromJsonHex(
                QString::fromStdString(entry.value("fg", std::string())));
            if (bg.isValid()) {
                m_backgroundColors.insert(key, bg);
            }
            if (fg.isValid()) {
                m_foregroundColors.insert(key, fg);
            }
        }
    };

    auto loadCellBaseCurrencies = [this](const nlohmann::json& cellBaseCurrencies) {
        if (!cellBaseCurrencies.is_array()) {
            return;
        }
        for (const auto& entry : cellBaseCurrencies) {
            const int row = entry.value("row", -1);
            const int col = entry.value("col", -1);
            if (row < 0 || col < 0) {
                continue;
            }
            QString currencyId;
            if (entry.contains("currencyId")) {
                if (entry["currencyId"].is_string()) {
                    currencyId = QString::fromStdString(entry["currencyId"].get<std::string>());
                } else if (entry["currencyId"].is_number()) {
                    currencyId = QString::number(entry["currencyId"].get<int>());
                }
            }
            currencyId = normalizeEconomyItemCompositeId(currencyId);
            if (currencyId.isEmpty()) {
                continue;
            }
            ensureDimensions(row, col);
            m_cellBaseCurrencyIds.insert(makeKey(row, col), currencyId);
        }
    };

    if (document.is_array()) {
        loadCells(document);
    } else if (document.is_object()) {
        loadCells(document.value("cells", nlohmann::json::array()));
        loadBorders(document.value("borders", nlohmann::json::array()));
        loadColors(document.value("colors", nlohmann::json::array()));
        loadCellBaseCurrencies(document.value("cellBaseCurrencies", nlohmann::json::array()));
    }

    recalculateAll();
}

nlohmann::json SpreadsheetModel::captureState() const {
    nlohmann::json state;
    state["rowCount"] = m_rowCount;
    state["columnCount"] = m_columnCount;
    state["sheet"] = toJson();
    return state;
}

void SpreadsheetModel::pruneCellsOutsideDimensions() {
    const auto pruneMap = [this](auto& map) {
        for (auto it = map.begin(); it != map.end();) {
            if (it.key().first >= m_rowCount || it.key().second >= m_columnCount) {
                it = map.erase(it);
            } else {
                ++it;
            }
        }
    };

    pruneMap(m_inputs);
    pruneMap(m_borders);
    pruneMap(m_backgroundColors);
    pruneMap(m_foregroundColors);
    pruneMap(m_cellBaseCurrencyIds);
}

void SpreadsheetModel::restoreState(const nlohmann::json& state) {
    m_suppressUndoCapture = true;
    beginResetModel();

    m_rowCount = state.value("rowCount", kDefaultRowCount);
    m_columnCount = state.value("columnCount", kDefaultColumnCount);
    m_inputs.clear();
    m_borders.clear();
    m_backgroundColors.clear();
    m_foregroundColors.clear();
    m_cellBaseCurrencyIds.clear();
    m_states.clear();

    const nlohmann::json sheet = state.value("sheet", nlohmann::json::object());
    if (sheet.is_array()) {
        fromJson(sheet);
    } else {
        fromJson(sheet);
    }

    m_rowCount = state.value("rowCount", m_rowCount);
    m_columnCount = state.value("columnCount", m_columnCount);
    pruneCellsOutsideDimensions();
    recalculateAll();

    endResetModel();
    m_suppressUndoCapture = false;
    emit sheetModified();
    emit undoHistoryChanged();
}

void SpreadsheetModel::clearUndoHistory() {
    m_undoStack.clear();
    m_redoStack.clear();
    emit undoHistoryChanged();
}

void SpreadsheetModel::pushUndoSnapshot() {
    if (m_suppressUndoCapture) {
        return;
    }

    m_undoStack.push_back(captureState());
    if (static_cast<int>(m_undoStack.size()) > kMaxUndoDepth) {
        m_undoStack.erase(m_undoStack.begin());
    }
    m_redoStack.clear();
    emit undoHistoryChanged();
}

bool SpreadsheetModel::canUndo() const {
    return !m_undoStack.empty();
}

bool SpreadsheetModel::canRedo() const {
    return !m_redoStack.empty();
}

void SpreadsheetModel::undo() {
    if (m_undoStack.empty()) {
        return;
    }

    m_redoStack.push_back(captureState());
    if (static_cast<int>(m_redoStack.size()) > kMaxUndoDepth) {
        m_redoStack.erase(m_redoStack.begin());
    }

    const nlohmann::json snapshot = std::move(m_undoStack.back());
    m_undoStack.pop_back();
    restoreState(snapshot);
}

void SpreadsheetModel::redo() {
    if (m_redoStack.empty()) {
        return;
    }

    m_undoStack.push_back(captureState());
    if (static_cast<int>(m_undoStack.size()) > kMaxUndoDepth) {
        m_undoStack.erase(m_undoStack.begin());
    }

    const nlohmann::json snapshot = std::move(m_redoStack.back());
    m_redoStack.pop_back();
    restoreState(snapshot);
}
