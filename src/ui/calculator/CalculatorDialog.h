#pragma once

#include "core/poeninja/PoeNinjaClient.h"
#include "ui/calculator/CurrencyIconCache.h"
#include "ui/calculator/CurrencyPickerDialog.h"
#include "ui/calculator/FormulaBuilderDialog.h"
#include "ui/calculator/SpreadsheetBorders.h"
#include "ui/calculator/SpreadsheetCellDelegate.h"
#include "ui/calculator/SpreadsheetModel.h"

#include <QColor>
#include <QDialog>
#include <QPointer>
#include <QPoint>
#include <QSet>

class QComboBox;
class QLabel;
class QLineEdit;
class QPushButton;
class QTableView;
class QTimer;

class ListDragAutoScroll;
class SpreadsheetTableView;

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
    void onUndo();
    void onRedo();
    void onHelpClicked();
    void saveSheetState();
    void onFormulaBarCommit();

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
    void clearSelectedCells();
    void openFormulaBuilder(int row, int col);
    bool primarySelectedCell(int& row, int& col) const;
    void applyTargetCellFromCurrentSelection();
    void enterFormulaPickMode(FormulaPickSlot slot);
    void exitFormulaPickMode();
    void commitFormulaPickFromSelection();
    void applyAutoRefreshSchedule();
    void updateFormulaBarFromSelection();
    void commitFormulaBarEdit();
    QString selectionReferenceLabel() const;
    static QString cellEditText(const SpreadsheetCell& cell);
    bool selectedCellBounds(int& minRow, int& minCol, int& maxRow, int& maxCol) const;
    void applyBorderPreset(SpreadsheetBorderPreset preset);
    void applyCellBackgroundColor(const QColor& color);
    void applyCellForegroundColor(const QColor& color);
    void clearSelectedCellColors();
    void showCustomBackgroundColorDialog();
    void showCustomForegroundColorDialog();
    void insertRowAtSelection();
    void insertColumnAtSelection();
    void deleteRowAtSelection();
    void deleteColumnAtSelection();
    void applyCellBaseCurrencyToSelection();
    void clearCellBaseCurrencyFromSelection();
    void cancelCellMoveDrag();
    void commitCellMoveDrag(const QModelIndex& dropIndex);
    void selectCellRange(int minRow, int minCol, int maxRow, int maxCol);
    QString calculatorHelpHtml() const;
    void showHelpDialog();
    void updateReferencedCellHighlight();

    PoeNinjaClient m_client;
    CurrencyIconCache m_iconCache;
    SpreadsheetModel m_model;
    SpreadsheetCellDelegate* m_cellDelegate = nullptr;
    SpreadsheetTableView* m_table = nullptr;
    ListDragAutoScroll* m_dragAutoScroll = nullptr;
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
    QPushButton* m_helpButton = nullptr;
    QLabel* m_cellNameLabel = nullptr;
    QLineEdit* m_formulaBarEdit = nullptr;
    QTimer* m_saveTimer = nullptr;
    QTimer* m_autoRefreshTimer = nullptr;
    QTimer* m_referencePulseTimer = nullptr;
    QSet<QPair<int, int>> m_referencedCells;
    qreal m_referencePulsePhase = 0.0;
    bool m_initialFetchDone = false;
    bool m_formulaPickActive = false;
    bool m_updatingFormulaBar = false;
    bool m_cellMoveDragArmed = false;
    bool m_cellMoveDragging = false;
    QPoint m_cellMovePressPos;
    int m_cellMoveSrcMinRow = -1;
    int m_cellMoveSrcMinCol = -1;
    int m_cellMoveSrcMaxRow = -1;
    int m_cellMoveSrcMaxCol = -1;
};
