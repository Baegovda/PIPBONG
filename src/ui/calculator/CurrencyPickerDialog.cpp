#include "ui/calculator/CurrencyPickerDialog.h"



#include "core/poeninja/PoeNinjaEconomyCategories.h"

#include "ui/calculator/CurrencyIconCache.h"

#include "ui/UiStrings.h"



#include <QComboBox>
#include <QDialogButtonBox>

#include <QFont>

#include <QHBoxLayout>

#include <QLabel>

#include <QLineEdit>

#include <QListWidget>

#include <QVBoxLayout>



namespace {



bool isCategoryHeaderItem(const QListWidgetItem* item) {

    return item && item->data(Qt::UserRole).toString().isEmpty();

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

    layout->addWidget(new QLabel(tr("카테고리를 선택한 뒤 아이템을 고르세요."), this));

    m_categoryCombo = new QComboBox(this);
    populateCategoryCombo();

    auto* categoryRow = new QHBoxLayout();
    categoryRow->addWidget(new QLabel(tr("카테고리"), this));
    categoryRow->addWidget(m_categoryCombo, 1);
    layout->addLayout(categoryRow);

    auto* filterEdit = new QLineEdit(this);

    filterEdit->setPlaceholderText(tr("이름 또는 카테고리로 검색…"));

    filterEdit->setClearButtonEnabled(true);

    layout->addWidget(filterEdit);



    m_list = new QListWidget(this);

    m_list->setIconSize(QSize(24, 24));

    m_list->setAlternatingRowColors(true);

    layout->addWidget(m_list, 1);



    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);

    localizeDialogButtons(buttons);

    layout->addWidget(buttons);



    connect(m_categoryCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
            [this, filterEdit](int) { rebuildList(filterEdit->text()); });
    connect(filterEdit, &QLineEdit::textChanged, this, &CurrencyPickerDialog::onFilterChanged);

    connect(m_list, &QListWidget::itemDoubleClicked, this, [this](QListWidgetItem* item) {

        if (!item || isCategoryHeaderItem(item)) {

            return;

        }

        m_selectedId = item->data(Qt::UserRole).toString();

        m_selectedName = item->text();

        accept();

    });

    connect(buttons, &QDialogButtonBox::accepted, this, [this]() {

        if (QListWidgetItem* item = m_list->currentItem()) {

            if (isCategoryHeaderItem(item)) {

                return;

            }

            m_selectedId = item->data(Qt::UserRole).toString();

            m_selectedName = item->text();

            accept();

        }

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

        if (item && item->data(Qt::UserRole).toString() == currencyId) {

            item->setIcon(m_icons->iconOrPlaceholder(currencyId));

            break;

        }

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

        header->setData(Qt::UserRole, QString());

    };



    for (const CurrencyRate& rate : m_rates) {

        if (!categoryFilter.isEmpty() && rate.categoryId != categoryFilter) {

            continue;

        }

        const QString categoryLabel = economyCategoryLabel(rate.categoryId);

        if (!needle.isEmpty()

            && !rate.name.contains(needle, Qt::CaseInsensitive)

            && !rate.id.contains(needle, Qt::CaseInsensitive)

            && !categoryLabel.contains(needle, Qt::CaseInsensitive)) {

            continue;

        }



        ensureCategoryHeader(rate.categoryId);



        auto* item = new QListWidgetItem(rate.name, m_list);

        item->setData(Qt::UserRole, rate.id);

        if (m_icons) {

            m_icons->ensureIcon(rate.id);

            item->setIcon(m_icons->iconOrPlaceholder(rate.id));

        }

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

