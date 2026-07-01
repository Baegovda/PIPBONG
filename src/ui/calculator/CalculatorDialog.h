#pragma once

#include "core/poeninja/PoeNinjaClient.h"
#include "ui/calculator/CurrencyIconCache.h"
#include "ui/calculator/CurrencyPickerDialog.h"
#include "ui/calculator/FormulaBuilderDialog.h"
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
    bool eventFilter(QObject* watched, QEvent* event) override;

private slots:
    void onRefreshClicked();
    void onBindCurrencyClicked();
    void onFormulaBuilderClicked();
    void onTableContextMenu(const QPoint& pos);
    void onOpenPoeNinjaClicked();
    void onLeagueChanged(int index);
    void onBaseCurrencyChanged(int index);
    void onBaseCurrencyContextMenu(const QPoint& pos);
    void onBaseCurrencyFavoriteClicked();
    void onDecimalPlacesChanged(int places);
    void onAutoRefreshSettingsChanged();
    void onAutoRefreshTimer();
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
    void updateBaseCurrencyFavoriteButton();
    void showError(const QString& message);
    void bindCurrencyToCellAt(int row, int col);
    void clearCellAt(int row, int col);
    void openFormulaBuilder(int row, int col);
    void enterFormulaPickMode(FormulaPickSlot slot);
    void exitFormulaPickMode();
    void commitFormulaPickFromSelection();
    void applyAutoRefreshSchedule();

    PoeNinjaClient m_client;
    CurrencyIconCache m_iconCache;
    SpreadsheetModel m_model;
    SpreadsheetCellDelegate* m_cellDelegate = nullptr;
    QTableView* m_table = nullptr;
    QComboBox* m_leagueCombo = nullptr;
    QComboBox* m_baseCurrencyCombo = nullptr;
    QPushButton* m_baseCurrencyFavoriteButton = nullptr;
    QPushButton* m_refreshButton = nullptr;
    QPushButton* m_bindCurrencyButton = nullptr;
    QPushButton* m_formulaButton = nullptr;
    class DragAdjustSpinBox* m_decimalPlacesSpin = nullptr;
    class QCheckBox* m_autoRefreshCheck = nullptr;
    class DragAdjustSpinBox* m_autoRefreshMinutesSpin = nullptr;
    FormulaBuilderDialog* m_formulaBuilder = nullptr;
    QLabel* m_statusLabel = nullptr;
    QLabel* m_hintLabel = nullptr;
    QTimer* m_saveTimer = nullptr;
    QTimer* m_autoRefreshTimer = nullptr;
    bool m_initialFetchDone = false;
    bool m_formulaPickActive = false;
};
