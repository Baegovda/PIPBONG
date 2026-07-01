#include "ui/calculator/CurrencyPickerDialog.h"

#include "core/poeninja/PoeNinjaEconomyCategories.h"
#include "ui/calculator/CurrencyIconCache.h"
#include "ui/calculator/EconomyFavoritesStore.h"
#include "ui/UiStrings.h"

#include <QComboBox>
#include <QDialogButtonBox>
#include <QFont>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMenu>
#include <QVBoxLayout>

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
                                           QWidget* parent)
    : QDialog(parent)
    , m_rates(rates)
    , m_icons(icons) {
    setWindowTitle(tr("시세 연동"));
    setMinimumSize(460, 520);

    auto* layout = new QVBoxLayout(this);
    layout->addWidget(new QLabel(tr("카테고리를 선택한 뒤 아이템을 고르세요. 우클릭으로 즐겨찾기를 추가·제거할 수 있습니다."), this));

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
            [this](int) { rebuildList(m_filterEdit->text()); });
    connect(m_filterEdit, &QLineEdit::textChanged, this, &CurrencyPickerDialog::onFilterChanged);
    connect(m_list, &QListWidget::itemDoubleClicked, this, [this](QListWidgetItem* item) {
        acceptItem(item);
    });
    connect(m_list, &QListWidget::customContextMenuRequested,
            this, &CurrencyPickerDialog::onListContextMenu);
    connect(buttons, &QDialogButtonBox::accepted, this, [this]() {
        acceptItem(m_list->currentItem());
    });
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);

    if (m_icons) {
        connect(m_icons, &CurrencyIconCache::iconUpdated, this, &CurrencyPickerDialog::onIconUpdated);
    }

    rebuildList({});
}

void CurrencyPickerDialog::onFilterChanged(const QString& text) {
    rebuildList(text);
}

void CurrencyPickerDialog::populateCategoryCombo() {
    if (!m_categoryCombo) {
        return;
    }

    m_categoryCombo->clear();
    m_categoryCombo->addItem(tr("즐겨찾기"), economyFavoritesCategoryId());
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
    if (!m_list || !m_icons) {
        return;
    }

    for (int row = 0; row < m_list->count(); ++row) {
        QListWidgetItem* item = m_list->item(row);
        if (item && item->data(kCurrencyIdRole).toString() == currencyId) {
            item->setIcon(m_icons->iconOrPlaceholder(currencyId));
            break;
        }
    }
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

QListWidgetItem* CurrencyPickerDialog::addRateItem(const CurrencyRate& rate) {
    const bool starred = EconomyFavoritesStore::instance().contains(rate.id);
    const QString label = starred ? QStringLiteral("★ %1").arg(rate.name) : rate.name;

    auto* item = new QListWidgetItem(label, m_list);
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
    rebuildList(m_filterEdit ? m_filterEdit->text() : QString{});
}

void CurrencyPickerDialog::onListContextMenu(const QPoint& pos) {
    QListWidgetItem* item = m_list->itemAt(pos);
    if (!item || isCategoryHeaderItem(item) || isPlaceholderItem(item)) {
        return;
    }

    m_list->setCurrentItem(item);

    const QString compositeId = item->data(kCurrencyIdRole).toString();
    const bool starred = EconomyFavoritesStore::instance().contains(compositeId);

    QMenu menu(this);
    QAction* favoriteAction = starred
        ? menu.addAction(tr("즐겨찾기 제거"))
        : menu.addAction(tr("즐겨찾기 추가"));
    QAction* bindAction = menu.addAction(tr("이 아이템 연동"));

    const QAction* chosen = menu.exec(m_list->viewport()->mapToGlobal(pos));
    if (chosen == favoriteAction) {
        toggleFavoriteForItem(item);
    } else if (chosen == bindAction) {
        acceptItem(item);
    }
}

void CurrencyPickerDialog::rebuildList(const QString& filter) {
    m_list->clear();

    const QString needle = filter.trimmed();
    const QString categoryFilter =
        m_categoryCombo ? m_categoryCombo->currentData().toString() : QString();

    if (categoryFilter == economyFavoritesCategoryId()) {
        for (const QString& compositeId : EconomyFavoritesStore::instance().favorites()) {
            const CurrencyRate* rate = findRate(compositeId);
            if (!rate || !rateMatchesFilter(*rate, needle)) {
                continue;
            }
            addRateItem(*rate);
        }

        if (m_list->count() == 0) {
            auto* placeholder = new QListWidgetItem(
                needle.isEmpty() ? tr("즐겨찾기가 비어 있습니다. 아이템을 우클릭해 추가하세요.")
                                 : tr("검색 결과가 없습니다."),
                m_list);
            placeholder->setFlags(Qt::NoItemFlags);
            placeholder->setData(kCurrencyIdRole, QStringLiteral("__placeholder__"));
        }
        return;
    }

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
        addRateItem(rate);
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
