#include "ui/FeatureLibraryDialog.h"

#include <QAbstractItemView>
#include <QDialogButtonBox>
#include <QListWidget>
#include <QListWidgetItem>
#include <QPushButton>
#include <QVBoxLayout>

FeatureLibraryDialog::FeatureLibraryDialog(const std::vector<EntryUi>& entries, QWidget* parent)
    : QDialog(parent) {
    setWindowTitle(tr("기능 라이브러리"));
    setModal(true);
    resize(700, 420);

    auto* layout = new QVBoxLayout(this);

    m_list = new QListWidget(this);
    m_list->setSelectionMode(QAbstractItemView::SingleSelection);
    m_list->setAlternatingRowColors(true);
    layout->addWidget(m_list, 1);

    populateEntries(entries);

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    buttons->button(QDialogButtonBox::Ok)->setText(tr("가져오기"));
    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    layout->addWidget(buttons);

    connect(m_list, &QListWidget::itemDoubleClicked, this, [this](QListWidgetItem*) {
        if (!selectedEntryId().isEmpty()) {
            accept();
        }
    });
}

void FeatureLibraryDialog::populateEntries(const std::vector<EntryUi>& entries) {
    if (!m_list) {
        return;
    }
    m_list->clear();

    for (const auto& e : entries) {
        const QString text = e.templateCount > 0
                                 ? QStringLiteral("%1 · 템플릿 %2개").arg(e.name).arg(e.templateCount)
                                 : e.name;
        auto* item = new QListWidgetItem(text, m_list);
        item->setData(Qt::UserRole, e.id);
    }
    if (m_list->count() > 0) {
        m_list->setCurrentRow(0);
    }
}

QString FeatureLibraryDialog::selectedEntryId() const {
    if (!m_list) {
        return {};
    }
    QListWidgetItem* item = m_list->currentItem();
    if (!item) {
        return {};
    }
    return item->data(Qt::UserRole).toString();
}

