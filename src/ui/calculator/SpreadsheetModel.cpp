#include "ui/calculator/SpreadsheetModel.h"

#include "core/poeninja/PoeNinjaEconomyCategories.h"

#include <QBrush>
#include <QColor>

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

    if (role == Qt::ToolTipRole) {
        if (state.hasError) {
            return errorText(state.error);
        }
        if (state.input.kind == SpreadsheetCellKind::ApiRef) {
            return tr("poe.ninja 시세: %1 (%2 기준)")
                .arg(state.input.currencyName, baseCurrencyName());
        }
        if (state.input.kind == SpreadsheetCellKind::Formula) {
            return state.input.raw.startsWith(QLatin1Char('=')) ? state.input.raw
                                                                 : QLatin1Char('=') + state.input.raw;
        }
        return {};
    }

    if (role == Qt::ForegroundRole && state.hasError) {
        return QBrush(QColor(220, 80, 80));
    }

    if (role == Qt::BackgroundRole) {
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
            return false;
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
    ensureDimensions(row, col);
    const CellKey key = makeKey(row, col);
    m_inputs.insert(key, std::move(cell));
    recalculateAll();
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
    if (!m_snapshot || !m_snapshot->valid) {
        return m_baseCurrencyId;
    }
    const QString name = m_snapshot->ratesById.value(m_baseCurrencyId).name;
    return name.isEmpty() ? m_baseCurrencyId : name;
}

void SpreadsheetModel::setBaseCurrencyId(const QString& currencyId) {
    const QString normalizedId = normalizeEconomyItemCompositeId(currencyId);
    if (normalizedId.isEmpty() || normalizedId == m_baseCurrencyId) {
        return;
    }
    m_baseCurrencyId = normalizedId;
    recalculateAll();
    emit baseCurrencyChanged();
}

std::optional<double> SpreadsheetModel::rateValueInBase(const QString& currencyId) const {
    if (!m_snapshot || !m_snapshot->valid || currencyId.isEmpty()) {
        return std::nullopt;
    }
    if (!m_snapshot->ratesById.contains(currencyId)) {
        return std::nullopt;
    }
    const double currencyDivineValue = m_snapshot->ratesById.value(currencyId).primaryValue;
    const double baseDivineValue = m_snapshot->ratesById.value(m_baseCurrencyId).primaryValue;
    if (baseDivineValue <= 0.0) {
        return currencyDivineValue;
    }
    return currencyDivineValue / baseDivineValue;
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
    recalculateAll();
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

QString SpreadsheetModel::formatNumber(double value) {
    if (!std::isfinite(value)) {
        return QStringLiteral("#ERR");
    }
    const double absValue = std::abs(value);
    if (absValue >= 1000000.0 || (absValue > 0.0 && absValue < 0.0001)) {
        return QString::number(value, 'g', 8);
    }
    if (std::abs(value - std::round(value)) < 1e-9) {
        return QString::number(static_cast<qlonglong>(std::llround(value)));
    }
    return QString::number(value, 'f', 4);
}

std::optional<double> SpreadsheetModel::resolveNumericValue(int row,
                                                            int col,
                                                            std::unordered_set<std::string>& visiting) {
    const CellKey key = makeKey(row, col);
    const SpreadsheetCell input = m_inputs.value(key);
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
    case SpreadsheetCellKind::ApiRef:
        if (!m_snapshot || !m_snapshot->valid) {
            return std::nullopt;
        }
        return rateValueInBase(input.currencyId);
    case SpreadsheetCellKind::Formula: {
        const auto resolver = [this, &visiting](int refRow, int refCol) -> std::optional<double> {
            return resolveNumericValue(refRow, refCol, visiting);
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
    case SpreadsheetCellKind::ApiRef:
        if (!m_snapshot || !m_snapshot->valid || !m_snapshot->ratesById.contains(input.currencyId)) {
            state.displayText = input.currencyName;
            state.hasError = true;
            state.error = FormulaError::Ref;
        } else if (const std::optional<double> value = rateValueInBase(input.currencyId)) {
            state.numericValue = *value;
            state.displayText = formatNumber(*value);
        } else {
            state.displayText = input.currencyName;
            state.hasError = true;
            state.error = FormulaError::Ref;
        }
        break;
    case SpreadsheetCellKind::Formula: {
        const auto resolver = [this, &visiting](int refRow, int refCol) -> std::optional<double> {
            return resolveNumericValue(refRow, refCol, visiting);
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

void SpreadsheetModel::recalculateAll() {
    beginResetModel();
    m_states.clear();
    std::unordered_set<std::string> visiting;
    for (auto it = m_inputs.constBegin(); it != m_inputs.constEnd(); ++it) {
        recalculateCell(it.key().first, it.key().second, visiting);
    }
    endResetModel();
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
    return cells;
}

void SpreadsheetModel::fromJson(const nlohmann::json& document) {
    m_inputs.clear();
    m_states.clear();

    if (document.is_array()) {
        for (const auto& entry : document) {
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
    }

    recalculateAll();
}
