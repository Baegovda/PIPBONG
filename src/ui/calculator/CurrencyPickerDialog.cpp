#include "ui/calculator/CurrencyPickerDialog.h"

#include "core/poeninja/PoeNinjaEconomyCategories.h"
#include "ui/calculator/CurrencyIconCache.h"
#include "ui/calculator/EconomyFavoritesStore.h"
#include "ui/UiStrings.h"

#include <QComboBox>
#include <QDialogButtonBox>
#include <QFont>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMenu>
#include <QVBoxLayout>

#include <algorithm>
#include <cmath>

namespace {

constexpr int kCurrencyIdRole = Qt::UserRole;
constexpr int kCurrencyNameRole = Qt::UserRole + 1;

bool isCategoryHeaderItem(const QListWidgetItem* item) {
    return item && item->data(kCurrencyIdRole).toString().isEmpty();
}

bool isPlaceholderItem(const QListWidgetItem* item) {
    return item && item->data(kCurrencyIdRole).toString() == QLatin1String("__placeholder__");
}

} // namespace

CurrencyPickerDialog::CurrencyPickerDialog(const QList<CurrencyRate>& rates,
                                           CurrencyIconCache* icons,
                                           QWidget* parent,
                                           const QString& dialogTitle,
                                           const QString& baseCurrencyId,
                                           int decimalPlaces)
    : QDialog(parent)
    , m_rates(rates)
    , m_icons(icons)
    , m_baseCurrencyId(baseCurrencyId)
    , m_decimalPlaces(std::clamp(decimalPlaces, 0, 8)) {
    setWindowTitle(dialogTitle.isEmpty() ? tr("시세 연동") : dialogTitle);
    setMinimumSize(460, 580);

    auto* layout = new QVBoxLayout(this);
    layout->addWidget(new QLabel(
        tr("상단 즐겨찾기 또는 아래 목록에서 아이템을 고르세요. 우클릭으로 즐겨찾기를 추가·제거할 수 있습니다."),
        this));

    auto* favoritesGroup = new QGroupBox(tr("즐겨찾기"), this);
    auto* favoritesLayout = new QVBoxLayout(favoritesGroup);
    favoritesLayout->setContentsMargins(8, 8, 8, 8);

    const CurrencyRate* baseRate = findRate(m_baseCurrencyId);
    const QString favoritesHint = baseRate && !baseRate->name.isEmpty()
        ? tr("기준 화폐 %1 대비 시세가 이름 옆에 표시됩니다.").arg(baseRate->name)
        : tr("기준 화폐 대비 시세가 이름 옆에 표시됩니다.");
    auto* favoritesHintLabel = new QLabel(favoritesHint, favoritesGroup);
    favoritesHintLabel->setWordWrap(true);
    favoritesLayout->addWidget(favoritesHintLabel);

    m_favoritesList = new QListWidget(favoritesGroup);
    m_favoritesList->setIconSize(QSize(24, 24));
    m_favoritesList->setAlternatingRowColors(true);
    m_favoritesList->setContextMenuPolicy(Qt::CustomContextMenu);
    m_favoritesList->setMaximumHeight(160);
    favoritesLayout->addWidget(m_favoritesList);
    layout->addWidget(favoritesGroup);

    m_categoryCombo = new QComboBox(this);
    populateCategoryCombo();

    auto* categoryRow = new QHBoxLayout();
    categoryRow->addWidget(new QLabel(tr("카테고리"), this));
    categoryRow->addWidget(m_categoryCombo, 1);
    layout->addLayout(categoryRow);

    m_filterEdit = new QLineEdit(this);
    m_filterEdit->setPlaceholderText(tr("이름 또는 카테고리로 검색…"));
    m_filterEdit->setClearButtonEnabled(true);
    layout->addWidget(m_filterEdit);

    m_list = new QListWidget(this);
    m_list->setIconSize(QSize(24, 24));
    m_list->setAlternatingRowColors(true);
    m_list->setContextMenuPolicy(Qt::CustomContextMenu);
    layout->addWidget(m_list, 1);

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    localizeDialogButtons(buttons);
    layout->addWidget(buttons);

    connect(m_categoryCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
            [this](int) {
                rebuildFavoritesList(m_filterEdit->text());
                rebuildList(m_filterEdit->text());
            });
    connect(m_filterEdit, &QLineEdit::textChanged, this, &CurrencyPickerDialog::onFilterChanged);
    connect(m_favoritesList, &QListWidget::itemDoubleClicked, this, [this](QListWidgetItem* item) {
        acceptItem(item);
    });
    connect(m_list, &QListWidget::itemDoubleClicked, this, [this](QListWidgetItem* item) {
        acceptItem(item);
    });
    connect(m_favoritesList, &QListWidget::itemSelectionChanged, this, [this]() {
        clearOtherListSelection(m_favoritesList);
    });
    connect(m_list, &QListWidget::itemSelectionChanged, this, [this]() {
        clearOtherListSelection(m_list);
    });
    connect(m_favoritesList, &QListWidget::customContextMenuRequested,
            this, &CurrencyPickerDialog::onFavoritesContextMenu);
    connect(m_list, &QListWidget::customContextMenuRequested,
            this, &CurrencyPickerDialog::onListContextMenu);
    connect(buttons, &QDialogButtonBox::accepted, this, [this]() {
        if (m_favoritesList && m_favoritesList->currentItem()) {
            acceptItem(m_favoritesList->currentItem());
            return;
        }
        acceptItem(m_list->currentItem());
    });
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);

    if (m_icons) {
        connect(m_icons, &CurrencyIconCache::iconUpdated, this, &CurrencyPickerDialog::onIconUpdated);
    }

    rebuildFavoritesList({});
    rebuildList({});
}

void CurrencyPickerDialog::clearOtherListSelection(QListWidget* activeList) {
    if (!activeList || !activeList->selectedItems().isEmpty()) {
        if (activeList == m_favoritesList && m_list) {
            m_list->blockSignals(true);
            m_list->clearSelection();
            m_list->blockSignals(false);
        } else if (activeList == m_list && m_favoritesList) {
            m_favoritesList->blockSignals(true);
            m_favoritesList->clearSelection();
            m_favoritesList->blockSignals(false);
        }
    }
}

void CurrencyPickerDialog::onFilterChanged(const QString& text) {
    rebuildFavoritesList(text);
    rebuildList(text);
}

void CurrencyPickerDialog::populateCategoryCombo() {
    if (!m_categoryCombo) {
        return;
    }

    m_categoryCombo->clear();
    m_categoryCombo->addItem(tr("전체"), QString());

    for (const EconomyCategoryDef& def : poeNinjaEconomyCategories()) {
        bool hasItems = false;
        for (const CurrencyRate& rate : m_rates) {
            if (rate.categoryId == def.id) {
                hasItems = true;
                break;
            }
        }
        if (!hasItems) {
            continue;
        }
        m_categoryCombo->addItem(economyCategoryLabel(def.id), def.id);
    }
}

void CurrencyPickerDialog::onIconUpdated(const QString& currencyId) {
    if (!m_icons) {
        return;
    }

    const auto updateList = [&](QListWidget* listWidget) {
        if (!listWidget) {
            return;
        }
        for (int row = 0; row < listWidget->count(); ++row) {
            QListWidgetItem* item = listWidget->item(row);
            if (item && item->data(kCurrencyIdRole).toString() == currencyId) {
                item->setIcon(m_icons->iconOrPlaceholder(currencyId));
                break;
            }
        }
    };

    updateList(m_favoritesList);
    updateList(m_list);
}

const CurrencyRate* CurrencyPickerDialog::findRate(const QString& compositeId) const {
    for (const CurrencyRate& rate : m_rates) {
        if (rate.id == compositeId) {
            return &rate;
        }
    }
    return nullptr;
}

bool CurrencyPickerDialog::rateMatchesFilter(const CurrencyRate& rate, const QString& needle) const {
    if (needle.isEmpty()) {
        return true;
    }

    const QString categoryLabel = economyCategoryLabel(rate.categoryId);
    return rate.name.contains(needle, Qt::CaseInsensitive)
        || rate.id.contains(needle, Qt::CaseInsensitive)
        || categoryLabel.contains(needle, Qt::CaseInsensitive);
}

QString CurrencyPickerDialog::formatRateValue(const CurrencyRate& rate) const {
    double value = rate.primaryValue;
    if (!m_baseCurrencyId.isEmpty() && rate.id != m_baseCurrencyId) {
        const CurrencyRate* baseRate = findRate(m_baseCurrencyId);
        if (baseRate && baseRate->primaryValue > 0.0) {
            value = rate.primaryValue / baseRate->primaryValue;
        }
    }

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

QListWidgetItem* CurrencyPickerDialog::addRateItem(QListWidget* listWidget,
                                                   const CurrencyRate& rate,
                                                   bool showRate) {
    const bool starred = EconomyFavoritesStore::instance().contains(rate.id);
    QString label = starred ? QStringLiteral("★ %1").arg(rate.name) : rate.name;
    if (showRate) {
        label = QStringLiteral("%1  %2").arg(label, formatRateValue(rate));
    }

    auto* item = new QListWidgetItem(label, listWidget);
    item->setData(kCurrencyIdRole, rate.id);
    item->setData(kCurrencyNameRole, rate.name);
    if (m_icons) {
        m_icons->ensureIcon(rate.id);
        item->setIcon(m_icons->iconOrPlaceholder(rate.id));
    }
    return item;
}

void CurrencyPickerDialog::acceptItem(QListWidgetItem* item) {
    if (!item || isCategoryHeaderItem(item) || isPlaceholderItem(item)) {
        return;
    }

    m_selectedId = item->data(kCurrencyIdRole).toString();
    m_selectedName = item->data(kCurrencyNameRole).toString();
    if (m_selectedName.isEmpty()) {
        m_selectedName = item->text();
    }
    accept();
}

void CurrencyPickerDialog::toggleFavoriteForItem(QListWidgetItem* item) {
    if (!item || isCategoryHeaderItem(item) || isPlaceholderItem(item)) {
        return;
    }

    const QString compositeId = item->data(kCurrencyIdRole).toString();
    EconomyFavoritesStore::instance().toggle(compositeId);
    const QString filter = m_filterEdit ? m_filterEdit->text() : QString{};
    rebuildFavoritesList(filter);
    rebuildList(filter);
}

void CurrencyPickerDialog::onFavoritesContextMenu(const QPoint& pos) {
    showContextMenuForList(m_favoritesList, pos);
}

void CurrencyPickerDialog::onListContextMenu(const QPoint& pos) {
    showContextMenuForList(m_list, pos);
}

void CurrencyPickerDialog::showContextMenuForList(QListWidget* sourceList, const QPoint& pos) {
    if (!sourceList) {
        return;
    }

    QListWidgetItem* item = sourceList->itemAt(pos);
    if (!item || isCategoryHeaderItem(item) || isPlaceholderItem(item)) {
        return;
    }

    sourceList->setCurrentItem(item);

    const QString compositeId = item->data(kCurrencyIdRole).toString();
    const bool starred = EconomyFavoritesStore::instance().contains(compositeId);

    QMenu menu(this);
    QAction* favoriteAction = starred
        ? menu.addAction(tr("즐겨찾기 제거"))
        : menu.addAction(tr("즐겨찾기 추가"));
    QAction* bindAction = menu.addAction(tr("이 아이템 연동"));

    const QAction* chosen = menu.exec(sourceList->viewport()->mapToGlobal(pos));
    if (chosen == favoriteAction) {
        toggleFavoriteForItem(item);
    } else if (chosen == bindAction) {
        acceptItem(item);
    }
}

void CurrencyPickerDialog::rebuildFavoritesList(const QString& filter) {
    if (!m_favoritesList) {
        return;
    }

    m_favoritesList->clear();
    const QString needle = filter.trimmed();

    for (const QString& compositeId : EconomyFavoritesStore::instance().favorites()) {
        const CurrencyRate* rate = findRate(compositeId);
        if (!rate || !rateMatchesFilter(*rate, needle)) {
            continue;
        }
        addRateItem(m_favoritesList, *rate, true);
    }

    if (m_favoritesList->count() == 0) {
        auto* placeholder = new QListWidgetItem(
            needle.isEmpty() ? tr("즐겨찾기가 비어 있습니다. 아래 목록에서 우클릭해 추가하세요.")
                             : tr("검색 결과가 없습니다."),
            m_favoritesList);
        placeholder->setFlags(Qt::NoItemFlags);
        placeholder->setData(kCurrencyIdRole, QStringLiteral("__placeholder__"));
    }
}

void CurrencyPickerDialog::rebuildList(const QString& filter) {
    m_list->clear();

    const QString needle = filter.trimmed();
    const QString categoryFilter =
        m_categoryCombo ? m_categoryCombo->currentData().toString() : QString();

    QString currentCategory;
    auto ensureCategoryHeader = [&](const QString& categoryId) {
        if (!categoryFilter.isEmpty() || categoryId == currentCategory) {
            return;
        }
        currentCategory = categoryId;
        const QString label = economyCategoryLabel(categoryId);
        auto* header = new QListWidgetItem(label, m_list);
        QFont font = header->font();
        font.setBold(true);
        header->setFont(font);
        header->setFlags(Qt::NoItemFlags);
        header->setData(kCurrencyIdRole, QString());
    };

    for (const CurrencyRate& rate : m_rates) {
        if (!categoryFilter.isEmpty() && rate.categoryId != categoryFilter) {
            continue;
        }
        if (!rateMatchesFilter(rate, needle)) {
            continue;
        }

        ensureCategoryHeader(rate.categoryId);
        addRateItem(m_list, rate, false);
    }

    if (m_list->count() == 0) {
        auto* placeholder = new QListWidgetItem(tr("검색 결과가 없습니다."), m_list);
        placeholder->setFlags(Qt::NoItemFlags);
        placeholder->setData(kCurrencyIdRole, QStringLiteral("__placeholder__"));
        return;
    }

    for (int row = 0; row < m_list->count(); ++row) {
        QListWidgetItem* item = m_list->item(row);
        if (item && !isCategoryHeaderItem(item)) {
            m_list->setCurrentRow(row);
            break;
        }
    }
}

QString CurrencyPickerDialog::selectedCurrencyId() const {
    return m_selectedId;
}

QString CurrencyPickerDialog::selectedCurrencyName() const {
    return m_selectedName;
}
