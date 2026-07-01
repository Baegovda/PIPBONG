#include "ui/calculator/CalculatorDialog.h"

#include "core/poeninja/PoeNinjaEconomyCategories.h"
#include "ui/widgets/HintLabel.h"

#include <nlohmann/json.hpp>

#include <QCloseEvent>
#include <QComboBox>
#include <QDesktopServices>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QMenu>
#include <QMessageBox>
#include <QPushButton>
#include <QSettings>
#include <QSignalBlocker>
#include <QTableView>
#include <QTimer>
#include <QTimeZone>
#include <QUrl>
#include <QVBoxLayout>

#include <algorithm>

namespace {

constexpr auto kSheetSettingsKey = "calculator/sheet_v1";
constexpr auto kLeagueSettingsKey = "calculator/lastLeague";
constexpr auto kBaseCurrencySettingsKey = "calculator/baseCurrencyId";
constexpr auto kGeometrySettingsKey = "calculator/geometry";

const QTimeZone& koreaTimeZone() {
    static const QTimeZone zone(QByteArrayLiteral("Asia/Seoul"));
    return zone;
}

QString leagueUrlForDisplayName(const QList<PoeNinjaLeagueInfo>& leagues, const QString& displayName) {
    for (const PoeNinjaLeagueInfo& league : leagues) {
        if (league.displayName == displayName) {
            return league.url;
        }
    }
    return QStringLiteral("runesofaldur");
}

} // namespace

CalculatorDialog::CalculatorDialog(QWidget* parent)
    : QDialog(parent) {
    setWindowTitle(tr("시세 계산기"));
    setMinimumSize(920, 560);
    setupUi();
    loadPersistedState();

    connect(&m_client, &PoeNinjaClient::busyChanged, this, &CalculatorDialog::onClientBusyChanged);
    connect(&m_model, &SpreadsheetModel::sheetModified, this, &CalculatorDialog::onSheetModified);
    connect(&m_model, &SpreadsheetModel::baseCurrencyChanged, this, &CalculatorDialog::updateStatusLabels);
    connect(&m_iconCache, &CurrencyIconCache::iconUpdated, this, [this](const QString&) {
        refreshBaseCurrencyIcons();
        if (m_table) {
            m_table->viewport()->update();
        }
    });
}

CalculatorDialog::~CalculatorDialog() {
    persistState();
}

void CalculatorDialog::setupUi() {
    auto* rootLayout = new QVBoxLayout(this);
    rootLayout->setContentsMargins(12, 12, 12, 12);
    rootLayout->setSpacing(8);

    auto* toolbar = new QHBoxLayout();
    toolbar->setSpacing(8);

    m_leagueCombo = new QComboBox(this);
    m_leagueCombo->setMinimumWidth(220);

    m_baseCurrencyCombo = new QComboBox(this);
    m_baseCurrencyCombo->setMinimumWidth(200);
    m_baseCurrencyCombo->setIconSize(QSize(20, 20));

    m_refreshButton = new QPushButton(tr("새로고침"), this);
    m_bindCurrencyButton = new QPushButton(tr("시세 연동"), this);
    auto* openLinkButton = new QPushButton(tr("poe.ninja 열기"), this);

    toolbar->addWidget(new QLabel(tr("리그"), this));
    toolbar->addWidget(m_leagueCombo, 1);
    toolbar->addWidget(new QLabel(tr("기준 화폐"), this));
    toolbar->addWidget(m_baseCurrencyCombo);
    toolbar->addWidget(m_refreshButton);
    toolbar->addWidget(m_bindCurrencyButton);
    toolbar->addWidget(openLinkButton);
    rootLayout->addLayout(toolbar);

    m_statusLabel = new QLabel(this);
    m_statusLabel->setWordWrap(true);
    rootLayout->addWidget(m_statusLabel);

    m_hintLabel = new HintLabel(
        tr("셀에 숫자를 입력하거나 =A1+B1 형식의 수식을 입력하세요. 시세 연동으로 화폐·파편·룬 등 poe.ninja 카테고리 시세를 불러올 수 있습니다. "
           "기준 화폐는 상단 콤보에서 선택하며 GGG 집계로 최대 약 1시간 지연될 수 있습니다."),
        this);
    m_hintLabel->setWordWrap(true);
    rootLayout->addWidget(m_hintLabel);

    m_table = new QTableView(this);
    m_table->setModel(&m_model);
    m_table->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    m_table->verticalHeader()->setDefaultSectionSize(32);
    m_table->setAlternatingRowColors(true);
    m_table->setSelectionBehavior(QAbstractItemView::SelectItems);
    m_table->setSelectionMode(QAbstractItemView::SingleSelection);
    m_table->setContextMenuPolicy(Qt::CustomContextMenu);
    m_cellDelegate = new SpreadsheetCellDelegate(&m_model, &m_iconCache, m_table);
    m_table->setItemDelegate(m_cellDelegate);
    rootLayout->addWidget(m_table, 1);

    m_saveTimer = new QTimer(this);
    m_saveTimer->setSingleShot(true);
    m_saveTimer->setInterval(400);
    connect(m_saveTimer, &QTimer::timeout, this, &CalculatorDialog::saveSheetState);

    connect(m_refreshButton, &QPushButton::clicked, this, &CalculatorDialog::onRefreshClicked);
    connect(m_bindCurrencyButton, &QPushButton::clicked, this, &CalculatorDialog::onBindCurrencyClicked);
    connect(m_table, &QTableView::customContextMenuRequested,
            this, &CalculatorDialog::onTableContextMenu);
    connect(openLinkButton, &QPushButton::clicked, this, &CalculatorDialog::onOpenPoeNinjaClicked);
    connect(m_leagueCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &CalculatorDialog::onLeagueChanged);
    connect(m_baseCurrencyCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &CalculatorDialog::onBaseCurrencyChanged);
}

void CalculatorDialog::showEvent(QShowEvent* event) {
    QDialog::showEvent(event);
    if (m_initialFetchDone) {
        return;
    }
    m_initialFetchDone = true;

    m_client.fetchIndexState([this](IndexStateResult indexState) {
        if (!indexState.success) {
            showError(indexState.errorMessage);
            return;
        }

        const QSettings settings;
        const QString preferredLeague = settings.value(QLatin1String(kLeagueSettingsKey)).toString();
        populateLeagues(indexState, preferredLeague);
        refreshExchangeRates();
    });
}

void CalculatorDialog::closeEvent(QCloseEvent* event) {
    persistState();
    QDialog::closeEvent(event);
}

void CalculatorDialog::loadPersistedState() {
    QSettings settings;
    restoreGeometry(settings.value(QLatin1String(kGeometrySettingsKey)).toByteArray());

    const QByteArray sheetJson = settings.value(QLatin1String(kSheetSettingsKey)).toByteArray();
    if (sheetJson.isEmpty()) {
        return;
    }

    try {
        const auto document = nlohmann::json::parse(sheetJson.constData());
        m_model.fromJson(document);
    } catch (const std::exception&) {
    }

    const QString baseCurrencyId = settings.value(QLatin1String(kBaseCurrencySettingsKey)).toString();
    if (!baseCurrencyId.isEmpty()) {
        m_model.setBaseCurrencyId(normalizeEconomyItemCompositeId(baseCurrencyId));
    }
}

void CalculatorDialog::persistState() {
    QSettings settings;
    settings.setValue(QLatin1String(kGeometrySettingsKey), saveGeometry());
    settings.setValue(QLatin1String(kLeagueSettingsKey), selectedLeagueDisplayName());
    settings.setValue(QLatin1String(kBaseCurrencySettingsKey), m_model.baseCurrencyId());

    const nlohmann::json document = m_model.toJson();
    settings.setValue(QLatin1String(kSheetSettingsKey),
                      QByteArray::fromStdString(document.dump()));
}

void CalculatorDialog::saveSheetState() {
    persistState();
}

void CalculatorDialog::onSheetModified() {
    m_saveTimer->start();
}

void CalculatorDialog::populateLeagues(const IndexStateResult& indexState, const QString& preferredLeague) {
    QSignalBlocker blocker(m_leagueCombo);
    m_leagueCombo->clear();

    QList<PoeNinjaLeagueInfo> leagues = indexState.economyLeagues;
    leagues.erase(std::remove_if(leagues.begin(), leagues.end(),
                                 [&indexState](const PoeNinjaLeagueInfo& league) {
                                     return indexState.versionByLeagueUrl.value(league.url).isEmpty();
                                 }),
                  leagues.end());
    std::stable_sort(leagues.begin(), leagues.end(), [](const PoeNinjaLeagueInfo& a, const PoeNinjaLeagueInfo& b) {
        if (a.indexed != b.indexed) {
            return a.indexed && !b.indexed;
        }
        return a.displayName < b.displayName;
    });

    int selectedIndex = -1;
    for (int i = 0; i < leagues.size(); ++i) {
        m_leagueCombo->addItem(leagues[i].displayName, leagues[i].displayName);
        if (!preferredLeague.isEmpty()
            && leagues[i].displayName.compare(preferredLeague, Qt::CaseInsensitive) == 0) {
            selectedIndex = i;
        }
    }

    if (selectedIndex < 0) {
        for (int i = 0; i < leagues.size(); ++i) {
            if (leagues[i].indexed) {
                selectedIndex = i;
                break;
            }
        }
    }
    if (selectedIndex < 0 && m_leagueCombo->count() > 0) {
        selectedIndex = 0;
    }
    if (selectedIndex >= 0) {
        m_leagueCombo->setCurrentIndex(selectedIndex);
    }
}

QString CalculatorDialog::selectedLeagueDisplayName() const {
    return m_leagueCombo ? m_leagueCombo->currentData().toString() : QString();
}

void CalculatorDialog::onBaseCurrencyChanged(int index) {
    if (index < 0 || !m_baseCurrencyCombo) {
        return;
    }
    const QString currencyId = m_baseCurrencyCombo->itemData(index).toString();
    if (!currencyId.isEmpty()) {
        m_model.setBaseCurrencyId(currencyId);
    }
}

void CalculatorDialog::populateBaseCurrencies() {
    if (!m_baseCurrencyCombo) {
        return;
    }

    const QList<CurrencyRate> currencies = m_model.availableCurrencies();
    const QString selectedId = m_model.baseCurrencyId();

    QSignalBlocker blocker(m_baseCurrencyCombo);
    m_baseCurrencyCombo->clear();

    int selectedIndex = -1;
    for (int i = 0; i < currencies.size(); ++i) {
        const CurrencyRate& rate = currencies.at(i);
        if (rate.categoryId != QLatin1String("currency")) {
            continue;
        }
        m_iconCache.ensureIcon(rate.id);
        m_baseCurrencyCombo->addItem(m_iconCache.iconOrPlaceholder(rate.id), rate.name, rate.id);
        if (rate.id == selectedId) {
            selectedIndex = m_baseCurrencyCombo->count() - 1;
        }
    }

    if (selectedIndex < 0) {
        for (int i = 0; i < m_baseCurrencyCombo->count(); ++i) {
            if (m_baseCurrencyCombo->itemData(i).toString() == QStringLiteral("currency:divine")) {
                selectedIndex = i;
                break;
            }
        }
    }
    if (selectedIndex < 0 && m_baseCurrencyCombo->count() > 0) {
        selectedIndex = 0;
    }
    if (selectedIndex >= 0) {
        m_baseCurrencyCombo->setCurrentIndex(selectedIndex);
        const QString currencyId = m_baseCurrencyCombo->itemData(selectedIndex).toString();
        if (!currencyId.isEmpty() && currencyId != m_model.baseCurrencyId()) {
            m_model.setBaseCurrencyId(currencyId);
        }
    }

    m_baseCurrencyCombo->setEnabled(!currencies.isEmpty());
}

void CalculatorDialog::refreshBaseCurrencyIcons() {
    if (!m_baseCurrencyCombo) {
        return;
    }
    for (int i = 0; i < m_baseCurrencyCombo->count(); ++i) {
        const QString currencyId = m_baseCurrencyCombo->itemData(i).toString();
        if (!currencyId.isEmpty()) {
            m_baseCurrencyCombo->setItemIcon(i, m_iconCache.iconOrPlaceholder(currencyId));
        }
    }
}

void CalculatorDialog::onLeagueChanged(int index) {
    if (index < 0) {
        return;
    }
    refreshExchangeRates();
}

void CalculatorDialog::onRefreshClicked() {
    m_client.fetchIndexState([this](IndexStateResult indexState) {
        if (!indexState.success) {
            showError(indexState.errorMessage);
            updateStatusLabels();
            return;
        }
        populateLeagues(indexState, selectedLeagueDisplayName());
        refreshExchangeRates();
    });
}

void CalculatorDialog::refreshExchangeRates() {
    const QString league = selectedLeagueDisplayName();
    if (league.isEmpty()) {
        updateStatusLabels();
        return;
    }

    m_client.fetchCurrencyExchange(league, [this, league](EconomySnapshot snapshot) {
        if (snapshot.valid) {
            m_model.applyEconomySnapshot(snapshot);
            applyEconomyIcons(snapshot);
            populateBaseCurrencies();
        } else if (const EconomySnapshot* cached = m_model.economySnapshot()) {
            snapshot = *cached;
            applyEconomyIcons(snapshot);
            populateBaseCurrencies();
        }
        updateStatusLabels();
        if (!snapshot.valid) {
            showError(tr("시세 데이터를 불러오지 못했습니다. 네트워크 또는 poe.ninja API 상태를 확인하세요."));
        }
    });
}

void CalculatorDialog::applyEconomyIcons(const EconomySnapshot& snapshot) {
    m_iconCache.setRates(snapshot.ratesById.values());
    if (m_table) {
        m_table->viewport()->update();
    }
}

void CalculatorDialog::updateStatusLabels() {
    const EconomySnapshot* snapshot = m_model.economySnapshot();
    if (snapshot && snapshot->valid) {
        const QDateTime kstTime = snapshot->fetchedAt.toTimeZone(koreaTimeZone());
        m_statusLabel->setText(
            tr("리그: %1 · 항목 %2개 (%3 카테고리) · 기준: %4 · 마지막 갱신: %5 (한국시간)")
                .arg(snapshot->leagueDisplayName)
                .arg(snapshot->ratesById.size())
                .arg(poeNinjaEconomyCategories().size())
                .arg(m_model.baseCurrencyName())
                .arg(kstTime.toString(QStringLiteral("yyyy-MM-dd HH:mm"))));
    } else {
        m_statusLabel->setText(tr("시세 데이터 없음 — 새로고침을 눌러 주세요."));
    }
}

void CalculatorDialog::onClientBusyChanged(bool busy) {
    if (m_refreshButton) {
        m_refreshButton->setEnabled(!busy);
    }
    if (m_leagueCombo) {
        m_leagueCombo->setEnabled(!busy);
    }
    if (m_baseCurrencyCombo) {
        m_baseCurrencyCombo->setEnabled(!busy && m_baseCurrencyCombo->count() > 0);
    }
}

void CalculatorDialog::onBindCurrencyClicked() {
    const QModelIndex index = m_table->currentIndex();
    if (!index.isValid()) {
        QMessageBox::information(this, tr("시세 연동"), tr("연동할 셀을 먼저 선택하세요."));
        return;
    }
    bindCurrencyToCellAt(index.row(), index.column());
}

void CalculatorDialog::onTableContextMenu(const QPoint& pos) {
    const QModelIndex index = m_table->indexAt(pos);
    if (!index.isValid()) {
        return;
    }
    m_table->setCurrentIndex(index);

    QMenu menu(this);
    QAction* bindAction = menu.addAction(tr("시세 연동"));
    QAction* clearAction = menu.addAction(tr("셀 지우기"));

    bindAction->setEnabled(!m_model.availableCurrencies().isEmpty());

    const SpreadsheetCell cell = m_model.cellInput(index.row(), index.column());
    clearAction->setEnabled(cell.kind != SpreadsheetCellKind::Empty);

    const QAction* chosen = menu.exec(m_table->viewport()->mapToGlobal(pos));
    if (chosen == bindAction) {
        bindCurrencyToCellAt(index.row(), index.column());
    } else if (chosen == clearAction) {
        clearCellAt(index.row(), index.column());
    }
}

void CalculatorDialog::bindCurrencyToCellAt(int row, int col) {
    const QList<CurrencyRate> currencies = m_model.availableCurrencies();
    if (currencies.isEmpty()) {
        QMessageBox::warning(this, tr("시세 연동"), tr("먼저 시세를 새로고침하세요."));
        return;
    }

    CurrencyPickerDialog picker(currencies, &m_iconCache, this);
    if (picker.exec() != QDialog::Accepted) {
        return;
    }

    const QString selectedId = picker.selectedCurrencyId();
    const QString selectedName = picker.selectedCurrencyName();
    if (selectedId.isEmpty() || selectedName.isEmpty()) {
        return;
    }

    m_model.bindCurrencyToCell(row, col, selectedId, selectedName);
}

void CalculatorDialog::clearCellAt(int row, int col) {
    SpreadsheetCell cell;
    cell.kind = SpreadsheetCellKind::Empty;
    m_model.setCellInput(row, col, cell);
}

void CalculatorDialog::onOpenPoeNinjaClicked() {
    const QString league = selectedLeagueDisplayName();
    QString leagueUrl = QStringLiteral("runesofaldur");
    if (const std::optional<IndexStateResult> indexState = m_client.cachedIndexState()) {
        leagueUrl = leagueUrlForDisplayName(indexState->economyLeagues, league);
    }
    const QUrl url(QStringLiteral("https://poe.ninja/poe2/economy/%1/currency").arg(leagueUrl));
    QDesktopServices::openUrl(url);
}

void CalculatorDialog::showError(const QString& message) {
    if (message.isEmpty()) {
        return;
    }
    QMessageBox::warning(this, tr("시세 계산기"), message);
}
