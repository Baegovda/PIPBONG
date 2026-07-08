#pragma once

#include <QDialog>
#include <QString>
#include <vector>

class QListWidget;

class FeatureLibraryDialog : public QDialog {
    Q_OBJECT
public:
    struct EntryUi {
        QString id;
        QString name;
        int templateCount = 0;
    };

    explicit FeatureLibraryDialog(const std::vector<EntryUi>& entries, QWidget* parent = nullptr);

    QString selectedEntryId() const;

private:
    void populateEntries(const std::vector<EntryUi>& entries);

    QListWidget* m_list = nullptr;
};

