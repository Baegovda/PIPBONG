#include "ui/calculator/CalculatorDialog.h"

#include "core/poeninja/PoeNinjaEconomyCategories.h"
#include "ui/calculator/EconomyFavoritesStore.h"
#include "ui/calculator/FormulaEvaluator.h"
#include "ui/widgets/DragAdjustSpinBox.h"
#include "ui/widgets/HintLabel.h"

#include <nlohmann/json.hpp>

#include <QCloseEvent>
#include <QCheckBox>
#include <QComboBox>
#include <QDesktopServices>
#include <QEvent>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QItemSelectionModel>
#include <QKeyEvent>
#include <QLabel>
#include <QMap>
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

#include <QSet>

#include <algorithm>

namespace {

constexpr auto kSheetSettingsKey = "calculator/sheet_v1";
constexpr auto kLeagueSettingsKey = "calculator/lastLeague";
constexpr auto kBaseCurrencySettingsKey = "calculator/baseCurrencyId";
constexpr auto kDecimalPlacesSettingsKey = "calculator/decimalPlaces";
constexpr auto kAutoRefreshEnabledKey = "calculator/autoRefreshEnabled";
constexpr auto kAutoRefreshMinutesKey = "calculator/autoRefreshMinutes";
constexpr auto kGeometrySettingsKey = "calculator/geometry";

constexpr int kDefaultAutoRefreshMinutes = 5;
constexpr int kMinAutoRefreshMinutes = 1;
constexpr int kMaxAutoRefreshMinutes = 120;

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
    applyAutoRefreshSchedule();

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
    m_baseCurrencyCombo->setContextMenuPolicy(Qt::CustomContextMenu);

    m_baseCurrencyFavoriteButton = new QPushButton(QStringLiteral("☆"), this);
    m_baseCurrencyFavoriteButton->setFixedSize(28, 28);
    m_baseCurrencyFavoriteButton->setToolTip(tr("기준 화폐 즐겨찾기"));
    m_baseCurrencyFavoriteButton->setFlat(true);

    m_refreshButton = new QPushButton(tr("새로고침"), this);
    m_bindCurrencyButton = new QPushButton(tr("시세 연동"), this);
    m_formulaButton = new QPushButton(tr("수식 만들기"), this);
    auto* openLinkButton = new QPushButton(tr("poe.ninja 열기"), this);

    m_decimalPlacesSpin = new DragAdjustSpinBox(this);
    m_decimalPlacesSpin->setRange(SpreadsheetModel::kMinDecimalPlaces, SpreadsheetModel::kMaxDecimalPlaces);
    m_decimalPlacesSpin->setValue(SpreadsheetModel::kDefaultDecimalPlaces);
    m_decimalPlacesSpin->setSingleStep(1);
    m_decimalPlacesSpin->setToolTip(tr("셀에 표시할 소수점 자릿수 (0~8)"));

    m_autoRefreshCheck = new QCheckBox(tr("자동 새로고침"), this);
    m_autoRefreshCheck->setToolTip(tr("설정한 간격마다 poe.ninja 시세를 자동으로 다시 불러옵니다."));

    m_autoRefreshMinutesSpin = new DragAdjustSpinBox(this);
    m_autoRefreshMinutesSpin->setRange(kMinAutoRefreshMinutes, kMaxAutoRefreshMinutes);
    m_autoRefreshMinutesSpin->setValue(kDefaultAutoRefreshMinutes);
    m_autoRefreshMinutesSpin->setSingleStep(1);
    m_autoRefreshMinutesSpin->setToolTip(tr("자동 새로고침 간격 (분)"));

    toolbar->addWidget(new QLabel(tr("리그"), this));
    toolbar->addWidget(m_leagueCombo, 1);
    toolbar->addWidget(new QLabel(tr("기준 화폐"), this));
    toolbar->addWidget(m_baseCurrencyCombo);
    toolbar->addWidget(m_baseCurrencyFavoriteButton);
    toolbar->addWidget(m_refreshButton);
    toolbar->addWidget(m_bindCurrencyButton);
    toolbar->addWidget(m_formulaButton);
    toolbar->addWidget(openLinkButton);
    rootLayout->addLayout(toolbar);

    auto* optionsRow = new QHBoxLayout();
    optionsRow->setSpacing(8);
    optionsRow->addWidget(new QLabel(tr("소수 자릿수"), this));
    optionsRow->addWidget(m_decimalPlacesSpin);
    optionsRow->addSpacing(16);
    optionsRow->addWidget(m_autoRefreshCheck);
    optionsRow->addWidget(m_autoRefreshMinutesSpin);
    optionsRow->addWidget(new QLabel(tr("분마다"), this));
    optionsRow->addStretch(1);
    rootLayout->addLayout(optionsRow);

    m_statusLabel = new QLabel(this);
    m_statusLabel->setWordWrap(true);
    rootLayout->addWidget(m_statusLabel);

    m_hintLabel = new HintLabel(
        tr("셀에 숫자를 입력하거나 수식 만들기로 사칙연산을 구성하세요 (=A1+B1). 시세 연동으로 화폐·파편·룬 등 poe.ninja 시세를 불러올 수 있습니다. "
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

    m_autoRefreshTimer = new QTimer(this);
    m_autoRefreshTimer->setSingleShot(false);
    connect(m_autoRefreshTimer, &QTimer::timeout, this, &CalculatorDialog::onAutoRefreshTimer);

    connect(m_refreshButton, &QPushButton::clicked, this, &CalculatorDialog::onRefreshClicked);
    connect(m_bindCurrencyButton, &QPushButton::clicked, this, &CalculatorDialog::onBindCurrencyClicked);
    connect(m_formulaButton, &QPushButton::clicked, this, &CalculatorDialog::onFormulaBuilderClicked);
    connect(m_decimalPlacesSpin, QOverload<int>::of(&QSpinBox::valueChanged),
            this, &CalculatorDialog::onDecimalPlacesChanged);
    connect(m_autoRefreshCheck, &QCheckBox::toggled, this, &CalculatorDialog::onAutoRefreshSettingsChanged);
    connect(m_autoRefreshMinutesSpin, QOverload<int>::of(&QSpinBox::valueChanged),
            this, &CalculatorDialog::onAutoRefreshSettingsChanged);
    connect(m_table, &QTableView::customContextMenuRequested,
            this, &CalculatorDialog::onTableContextMenu);
    connect(openLinkButton, &QPushButton::clicked, this, &CalculatorDialog::onOpenPoeNinjaClicked);
    connect(m_leagueCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &CalculatorDialog::onLeagueChanged);
    connect(m_baseCurrencyCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &CalculatorDialog::onBaseCurrencyChanged);
    connect(m_baseCurrencyCombo, &QComboBox::customContextMenuRequested,
            this, &CalculatorDialog::onBaseCurrencyContextMenu);
    connect(m_baseCurrencyFavoriteButton, &QPushButton::clicked,
            this, &CalculatorDialog::onBaseCurrencyFavoriteClicked);
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
    exitFormulaPickMode();
    if (m_autoRefreshTimer) {
        m_autoRefreshTimer->stop();
    }
    if (m_formulaBuilder) {
        m_formulaBuilder->close();
    }
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

    const int decimalPlaces = settings.value(QLatin1String(kDecimalPlacesSettingsKey),
                                               SpreadsheetModel::kDefaultDecimalPlaces).toInt();
    m_model.setDecimalPlaces(decimalPlaces);
    if (m_decimalPlacesSpin) {
        QSignalBlocker blocker(m_decimalPlacesSpin);
        m_decimalPlacesSpin->setValue(m_model.decimalPlaces());
    }

    const bool autoRefreshEnabled =
        settings.value(QLatin1String(kAutoRefreshEnabledKey), false).toBool();
    const int autoRefreshMinutes = settings.value(QLatin1String(kAutoRefreshMinutesKey),
                                                  kDefaultAutoRefreshMinutes)
                                       .toInt();
    if (m_autoRefreshCheck) {
        QSignalBlocker blocker(m_autoRefreshCheck);
        m_autoRefreshCheck->setChecked(autoRefreshEnabled);
    }
    if (m_autoRefreshMinutesSpin) {
        QSignalBlocker blocker(m_autoRefreshMinutesSpin);
        m_autoRefreshMinutesSpin->setValue(
            std::clamp(autoRefreshMinutes, kMinAutoRefreshMinutes, kMaxAutoRefreshMinutes));
        m_autoRefreshMinutesSpin->setEnabled(autoRefreshEnabled);
    }
}

void CalculatorDialog::persistState() {
    QSettings settings;
    settings.setValue(QLatin1String(kGeometrySettingsKey), saveGeometry());
    settings.setValue(QLatin1String(kLeagueSettingsKey), selectedLeagueDisplayName());
    settings.setValue(QLatin1String(kBaseCurrencySettingsKey), m_model.baseCurrencyId());
    settings.setValue(QLatin1String(kDecimalPlacesSettingsKey), m_model.decimalPlaces());
    if (m_autoRefreshCheck) {
        settings.setValue(QLatin1String(kAutoRefreshEnabledKey), m_autoRefreshCheck->isChecked());
    }
    if (m_autoRefreshMinutesSpin) {
        settings.setValue(QLatin1String(kAutoRefreshMinutesKey), m_autoRefreshMinutesSpin->value());
    }

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
    updateBaseCurrencyFavoriteButton();
}

void CalculatorDialog::updateBaseCurrencyFavoriteButton() {
    if (!m_baseCurrencyFavoriteButton || !m_baseCurrencyCombo) {
        return;
    }

    const QString currencyId = m_model.baseCurrencyId();
    const bool starred = !currencyId.isEmpty() && EconomyFavoritesStore::instance().contains(currencyId);
    m_baseCurrencyFavoriteButton->setText(starred ? QStringLiteral("★") : QStringLiteral("☆"));
    m_baseCurrencyFavoriteButton->setEnabled(!currencyId.isEmpty());
}

void CalculatorDialog::onDecimalPlacesChanged(int places) {
    m_model.setDecimalPlaces(places);
    persistState();
}

void CalculatorDialog::onAutoRefreshSettingsChanged() {
    const bool enabled = m_autoRefreshCheck && m_autoRefreshCheck->isChecked();
    if (m_autoRefreshMinutesSpin) {
        m_autoRefreshMinutesSpin->setEnabled(enabled);
    }
    applyAutoRefreshSchedule();
    persistState();
}

void CalculatorDialog::applyAutoRefreshSchedule() {
    if (!m_autoRefreshTimer) {
        return;
    }

    m_autoRefreshTimer->stop();
    if (!m_autoRefreshCheck || !m_autoRefreshCheck->isChecked() || !m_autoRefreshMinutesSpin) {
        return;
    }

    const int minutes = m_autoRefreshMinutesSpin->value();
    if (minutes < kMinAutoRefreshMinutes) {
        return;
    }

    m_autoRefreshTimer->setInterval(minutes * 60 * 1000);
    m_autoRefreshTimer->start();
}

void CalculatorDialog::onAutoRefreshTimer() {
    if (m_client.isBusy() || selectedLeagueDisplayName().isEmpty()) {
        return;
    }
    refreshExchangeRates();
}

void CalculatorDialog::onBaseCurrencyFavoriteClicked() {
    const QString currencyId = m_model.baseCurrencyId();
    if (currencyId.isEmpty()) {
        return;
    }

    EconomyFavoritesStore::instance().toggle(currencyId);
    updateBaseCurrencyFavoriteButton();
    populateBaseCurrencies();
}

void CalculatorDialog::onBaseCurrencyContextMenu(const QPoint& pos) {
    if (!m_baseCurrencyCombo) {
        return;
    }

    const QString currencyId = m_model.baseCurrencyId();
    if (currencyId.isEmpty()) {
        return;
    }

    const bool starred = EconomyFavoritesStore::instance().contains(currencyId);

    QMenu menu(this);
    QAction* favoriteAction = starred
        ? menu.addAction(tr("즐겨찾기 제거"))
        : menu.addAction(tr("즐겨찾기 추가"));

    const QAction* chosen = menu.exec(m_baseCurrencyCombo->mapToGlobal(pos));
    if (chosen == favoriteAction) {
        EconomyFavoritesStore::instance().toggle(currencyId);
        updateBaseCurrencyFavoriteButton();
        populateBaseCurrencies();
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

    QMap<QString, CurrencyRate> currencyRates;
    for (const CurrencyRate& rate : currencies) {
        if (rate.categoryId == QLatin1String("currency")) {
            currencyRates.insert(rate.id, rate);
        }
    }

    int selectedIndex = -1;
    QSet<QString> added;

    auto addCurrencyItem = [&](const CurrencyRate& rate) {
        if (added.contains(rate.id)) {
            return;
        }
        m_iconCache.ensureIcon(rate.id);
        const bool starred = EconomyFavoritesStore::instance().contains(rate.id);
        const QString label = starred ? QStringLiteral("★ %1").arg(rate.name) : rate.name;
        m_baseCurrencyCombo->addItem(m_iconCache.iconOrPlaceholder(rate.id), label, rate.id);
        added.insert(rate.id);
        if (rate.id == selectedId) {
            selectedIndex = m_baseCurrencyCombo->count() - 1;
        }
    };

    for (const QString& favoriteId : EconomyFavoritesStore::instance().favorites()) {
        if (!favoriteId.startsWith(QLatin1String("currency:"))) {
            continue;
        }
        const auto it = currencyRates.constFind(favoriteId);
        if (it != currencyRates.constEnd()) {
            addCurrencyItem(it.value());
        }
    }

    QList<CurrencyRate> remaining = currencyRates.values();
    std::sort(remaining.begin(), remaining.end(), [](const CurrencyRate& a, const CurrencyRate& b) {
        return a.name.localeAwareCompare(b.name) < 0;
    });
    for (const CurrencyRate& rate : remaining) {
        addCurrencyItem(rate);
    }

    if (selectedIndex < 0) {
        for (int i = 0; i < m_baseCurrencyCombo->count(); ++i) {
            if (m_baseCurrencyCombo->itemData(i).toString() == QStringLiteral("currency:exalted")) {
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
    updateBaseCurrencyFavoriteButton();
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
    if (m_baseCurrencyFavoriteButton) {
        m_baseCurrencyFavoriteButton->setEnabled(!busy && !m_model.baseCurrencyId().isEmpty());
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
    QAction* formulaAction = menu.addAction(tr("수식 만들기"));
    QAction* bindAction = menu.addAction(tr("시세 연동"));
    QAction* clearAction = menu.addAction(tr("셀 지우기"));

    bindAction->setEnabled(!m_model.availableCurrencies().isEmpty());

    const SpreadsheetCell cell = m_model.cellInput(index.row(), index.column());
    clearAction->setEnabled(cell.kind != SpreadsheetCellKind::Empty);

    const QAction* chosen = menu.exec(m_table->viewport()->mapToGlobal(pos));
    if (chosen == formulaAction) {
        openFormulaBuilder(index.row(), index.column());
    } else if (chosen == bindAction) {
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

void CalculatorDialog::onFormulaBuilderClicked() {
    const QModelIndex index = m_table->currentIndex();
    if (index.isValid()) {
        openFormulaBuilder(index.row(), index.column());
        return;
    }
    openFormulaBuilder(0, 0);
}

void CalculatorDialog::openFormulaBuilder(int row, int col) {
    if (!m_formulaBuilder) {
        m_formulaBuilder = new FormulaBuilderDialog(&m_model, this);
        m_formulaBuilder->setAttribute(Qt::WA_DeleteOnClose, false);

        connect(m_formulaBuilder, &FormulaBuilderDialog::pickModeRequested, this,
                &CalculatorDialog::enterFormulaPickMode);
        connect(m_formulaBuilder, &FormulaBuilderDialog::pickModeEnded, this,
                &CalculatorDialog::exitFormulaPickMode);
        connect(m_formulaBuilder, &FormulaBuilderDialog::formulaApplied, this,
                [this](int targetRow, int targetCol, const QString& formula) {
                    SpreadsheetCell cell;
                    cell.kind = SpreadsheetCellKind::Formula;
                    cell.raw = formula.startsWith(QLatin1Char('=')) ? formula : QLatin1Char('=') + formula;
                    m_model.setCellInput(targetRow, targetCol, cell);
                    if (m_table) {
                        m_table->setCurrentIndex(m_model.index(targetRow, targetCol));
                    }
                });
        connect(m_formulaBuilder, &QDialog::finished, this, [this]() {
            exitFormulaPickMode();
        });
    }

    m_formulaBuilder->setTargetCell(row, col);
    m_formulaBuilder->show();
    m_formulaBuilder->raise();
    m_formulaBuilder->activateWindow();
    if (m_table) {
        m_table->setCurrentIndex(m_model.index(row, col));
    }
}

void CalculatorDialog::enterFormulaPickMode(FormulaPickSlot slot) {
    Q_UNUSED(slot);
    if (!m_table || m_formulaPickActive) {
        return;
    }

    m_formulaPickActive = true;
    m_table->setSelectionMode(QAbstractItemView::ExtendedSelection);
    m_table->setSelectionBehavior(QAbstractItemView::SelectItems);
    m_table->installEventFilter(this);
    m_table->viewport()->installEventFilter(this);
    m_statusLabel->setText(tr("표에서 셀을 클릭하거나 드래그해 선택하세요. Esc로 취소합니다."));
}

void CalculatorDialog::exitFormulaPickMode() {
    if (!m_formulaPickActive) {
        return;
    }

    m_formulaPickActive = false;
    if (m_table) {
        m_table->viewport()->removeEventFilter(this);
        m_table->removeEventFilter(this);
        m_table->setSelectionMode(QAbstractItemView::SingleSelection);
    }
    if (m_formulaBuilder) {
        const QSignalBlocker blocker(m_formulaBuilder);
        m_formulaBuilder->endPickMode();
    }
    updateStatusLabels();
}

void CalculatorDialog::commitFormulaPickFromSelection() {
    if (!m_formulaPickActive || !m_table || !m_formulaBuilder) {
        return;
    }

    const QModelIndexList indexes = m_table->selectionModel()->selectedIndexes();
    QSet<QPair<int, int>> uniqueCells;
    QStringList refs;
    for (const QModelIndex& index : indexes) {
        if (!index.isValid()) {
            continue;
        }
        const QPair<int, int> key(index.row(), index.column());
        if (uniqueCells.contains(key)) {
            continue;
        }
        uniqueCells.insert(key);
        refs.append(FormulaEvaluator::cellReference(index.row(), index.column()));
    }

    std::sort(refs.begin(), refs.end(), [](const QString& a, const QString& b) {
        int rowA = 0;
        int colA = 0;
        int rowB = 0;
        int colB = 0;
        FormulaEvaluator::cellAddressFromReference(a, rowA, colA);
        FormulaEvaluator::cellAddressFromReference(b, rowB, colB);
        if (rowA != rowB) {
            return rowA < rowB;
        }
        return colA < colB;
    });

    m_formulaBuilder->applyPickedCellRefs(refs);
}

bool CalculatorDialog::eventFilter(QObject* watched, QEvent* event) {
    if (!m_formulaPickActive || !m_table) {
        return QDialog::eventFilter(watched, event);
    }

    const bool isTable = watched == m_table;
    const bool isViewport = watched == m_table->viewport();
    if (!isTable && !isViewport) {
        return QDialog::eventFilter(watched, event);
    }

    if (event->type() == QEvent::MouseButtonRelease && isViewport) {
        commitFormulaPickFromSelection();
        return false;
    }
    if (event->type() == QEvent::KeyPress && isTable) {
        const auto* keyEvent = static_cast<QKeyEvent*>(event);
        if (keyEvent->key() == Qt::Key_Escape) {
            exitFormulaPickMode();
            return true;
        }
    }
    return QDialog::eventFilter(watched, event);
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
