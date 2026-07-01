#pragma once

#include "core/poeninja/PoeNinjaClient.h"
#include "ui/calculator/CurrencyIconCache.h"
#include "ui/calculator/CurrencyPickerDialog.h"
#include "ui/calculator/SpreadsheetCellDelegate.h"
#include "ui/calculator/SpreadsheetModel.h"

#include <QDialog>
#include <QPointer>

class QComboBox;
class QLabel;
class QPushButton;
class QTableView;
class QTimer;

class CalculatorDialog : public QDialog {
    Q_OBJECT
public:
    explicit CalculatorDialog(QWidget* parent = nullptr);
    ~CalculatorDialog() override;

protected:
    void closeEvent(QCloseEvent* event) override;
    void showEvent(QShowEvent* event) override;

private slots:
    void onRefreshClicked();
    void onBindCurrencyClicked();
    void onTableContextMenu(const QPoint& pos);
    void onOpenPoeNinjaClicked();
    void onLeagueChanged(int index);
    void onBaseCurrencyChanged(int index);
    void onClientBusyChanged(bool busy);
    void onSheetModified();
    void saveSheetState();

private:
    void setupUi();
    void loadPersistedState();
    void persistState();
    void populateLeagues(const IndexStateResult& indexState, const QString& preferredLeague);
    void refreshExchangeRates();
    void updateStatusLabels();
    QString selectedLeagueDisplayName() const;
    void applyEconomyIcons(const EconomySnapshot& snapshot);
    void populateBaseCurrencies();
    void refreshBaseCurrencyIcons();
    void showError(const QString& message);
    void bindCurrencyToCellAt(int row, int col);
    void clearCellAt(int row, int col);

    PoeNinjaClient m_client;
    CurrencyIconCache m_iconCache;
    SpreadsheetModel m_model;
    SpreadsheetCellDelegate* m_cellDelegate = nullptr;
    QTableView* m_table = nullptr;
    QComboBox* m_leagueCombo = nullptr;
    QComboBox* m_baseCurrencyCombo = nullptr;
    QPushButton* m_refreshButton = nullptr;
    QPushButton* m_bindCurrencyButton = nullptr;
    QLabel* m_statusLabel = nullptr;
    QLabel* m_hintLabel = nullptr;
    QTimer* m_saveTimer = nullptr;
    bool m_initialFetchDone = false;
};
