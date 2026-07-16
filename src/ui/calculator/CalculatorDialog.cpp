#include "ui/calculator/CalculatorDialog.h"

#include "core/poeninja/PoeNinjaEconomyCategories.h"
#include "ui/calculator/EconomyFavoritesStore.h"
#include "ui/calculator/FormulaEvaluator.h"
#include "ui/calculator/SpreadsheetCellColors.h"
#include "ui/widgets/DragAdjustSpinBox.h"
#include "ui/widgets/ListDragAutoScroll.h"

#include <nlohmann/json.hpp>

#include <QApplication>
#include <QCloseEvent>
#include <QCheckBox>
#include <QColorDialog>
#include <QComboBox>
#include <QDesktopServices>
#include <QDialog>
#include <QDialogButtonBox>
#include <QEvent>
#include <QFrame>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QItemSelection>
#include <QItemSelectionModel>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QMap>
#include <QMenu>
#include <QMessageBox>
#include <QMouseEvent>
#include <QPushButton>
#include <QSettings>
#include <QShortcut>
#include <QSignalBlocker>
#include <QTableView>
#include <QTextBrowser>
#include <QTimer>
#include <QToolButton>
#include <QTimeZone>
#include <QUrl>
#include <QVBoxLayout>
#include <QWidgetAction>

#include <QSet>

#include <algorithm>
#include <climits>
#include <functional>

class SpreadsheetTableView : public QTableView {
public:
    using QTableView::QTableView;
    bool isEditingView() const { return state() == EditingState; }
};

QMenu* buildColorPaletteMenu(QWidget* parent,
                              const QVector<QColor>& palette,
                              int columns,
                              const std::function<void(const QColor&)>& onPick,
                              const std::function<void()>& onClear,
                              const std::function<void()>& onCustom) {
    auto* menu = new QMenu(parent);

    auto* paletteHost = new QWidget(menu);
    auto* grid = new QGridLayout(paletteHost);
    grid->setContentsMargins(8, 8, 8, 4);
    grid->setSpacing(4);

    int row = 0;
    int col = 0;
    for (const QColor& color : palette) {
        auto* swatch = new QPushButton(paletteHost);
        swatch->setFixedSize(26, 26);
        swatch->setFlat(true);
        swatch->setToolTip(color.name(QColor::HexRgb));
        swatch->setStyleSheet(QStringLiteral(
            "QPushButton { background-color: %1; border: 1px solid #666; border-radius: 2px; }"
            "QPushButton:hover { border: 2px solid #0078d4; }")
                                  .arg(color.name(QColor::HexRgb)));
        QObject::connect(swatch, &QPushButton::clicked, menu, [menu, onPick, color]() {
            onPick(color);
            menu->close();
        });
        grid->addWidget(swatch, row, col);
        ++col;
        if (col >= columns) {
            col = 0;
            ++row;
        }
    }

    auto* paletteAction = new QWidgetAction(menu);
    paletteAction->setDefaultWidget(paletteHost);
    menu->addAction(paletteAction);
    menu->addSeparator();
    menu->addAction(QObject::tr("사용자 지정…"), menu, onCustom);
    menu->addAction(QObject::tr("색 없음"), menu, [menu, onClear]() {
        onClear();
        menu->close();
    });
    return menu;
}

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
    setProperty("pipbong_featureHotkeyGateExempt", true);
    setWindowTitle(tr("시세 계산기"));
    setMinimumSize(920, 560);
    setupUi();
    loadPersistedState();
    applyAutoRefreshSchedule();

    connect(&m_client, &PoeNinjaClient::busyChanged, this, &CalculatorDialog::onClientBusyChanged);
    connect(&m_model, &SpreadsheetModel::sheetModified, this, &CalculatorDialog::onSheetModified);

    auto* undoShortcut = new QShortcut(QKeySequence::Undo, this);
    undoShortcut->setContext(Qt::WidgetWithChildrenShortcut);
    connect(undoShortcut, &QShortcut::activated, this, &CalculatorDialog::onUndo);

    auto* redoShortcut = new QShortcut(QKeySequence::Redo, this);
    redoShortcut->setContext(Qt::WidgetWithChildrenShortcut);
    connect(redoShortcut, &QShortcut::activated, this, &CalculatorDialog::onRedo);

    auto* redoAltShortcut = new QShortcut(QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_Z), this);
    redoAltShortcut->setContext(Qt::WidgetWithChildrenShortcut);
    connect(redoAltShortcut, &QShortcut::activated, this, &CalculatorDialog::onRedo);
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
    m_helpButton = new QPushButton(tr("도움말"), this);
    m_helpButton->setToolTip(tr("계산기 사용 방법"));
    optionsRow->addWidget(m_helpButton);
    rootLayout->addLayout(optionsRow);

    m_statusLabel = new QLabel(this);
    m_statusLabel->setWordWrap(true);
    rootLayout->addWidget(m_statusLabel);

    auto* formulaBarRow = new QHBoxLayout();
    formulaBarRow->setSpacing(6);

    m_cellNameLabel = new QLabel(this);
    m_cellNameLabel->setFixedWidth(72);
    m_cellNameLabel->setMinimumHeight(28);
    m_cellNameLabel->setFrameStyle(QFrame::Panel | QFrame::Sunken);
    m_cellNameLabel->setAlignment(Qt::AlignCenter);
    m_cellNameLabel->setToolTip(tr("선택한 셀 또는 범위"));
    formulaBarRow->addWidget(m_cellNameLabel);

    auto* fxLabel = new QLabel(QStringLiteral("fx"), this);
    fxLabel->setFixedWidth(24);
    fxLabel->setAlignment(Qt::AlignCenter);
    fxLabel->setToolTip(tr("셀 내용·수식"));
    formulaBarRow->addWidget(fxLabel);

    m_formulaBarEdit = new QLineEdit(this);
    m_formulaBarEdit->setPlaceholderText(tr("셀 내용 또는 =A1+B1 형식의 수식"));
    m_formulaBarEdit->setClearButtonEnabled(true);
    m_formulaBarEdit->setMinimumHeight(28);
    formulaBarRow->addWidget(m_formulaBarEdit, 1);
    rootLayout->addLayout(formulaBarRow);

    auto* borderRow = new QHBoxLayout();
    borderRow->setSpacing(6);
    borderRow->addWidget(new QLabel(tr("테두리"), this));

    auto* borderMenuButton = new QToolButton(this);
    borderMenuButton->setText(tr("테두리 적용 ▾"));
    borderMenuButton->setToolButtonStyle(Qt::ToolButtonTextOnly);
    borderMenuButton->setPopupMode(QToolButton::InstantPopup);
    auto* borderMenu = new QMenu(borderMenuButton);
    const auto addBorderAction = [this, borderMenu](const QString& label, SpreadsheetBorderPreset preset) {
        QAction* action = borderMenu->addAction(label);
        connect(action, &QAction::triggered, this, [this, preset]() { applyBorderPreset(preset); });
    };
    addBorderAction(tr("모든 테두리"), SpreadsheetBorderPreset::All);
    addBorderAction(tr("바깥쪽 테두리"), SpreadsheetBorderPreset::Outside);
    addBorderAction(tr("안쪽 테두리"), SpreadsheetBorderPreset::Inside);
    borderMenu->addSeparator();
    addBorderAction(tr("위쪽 테두리"), SpreadsheetBorderPreset::Top);
    addBorderAction(tr("아래쪽 테두리"), SpreadsheetBorderPreset::Bottom);
    addBorderAction(tr("왼쪽 테두리"), SpreadsheetBorderPreset::Left);
    addBorderAction(tr("오른쪽 테두리"), SpreadsheetBorderPreset::Right);
    borderMenu->addSeparator();
    addBorderAction(tr("테두리 없음"), SpreadsheetBorderPreset::None);
    borderMenuButton->setMenu(borderMenu);
    borderRow->addWidget(borderMenuButton);

    auto* insertRowButton = new QPushButton(tr("행 추가"), this);
    insertRowButton->setToolTip(tr("선택한 행 위치에 빈 행을 삽입합니다. 아래 셀과 수식 참조가 자동으로 밀립니다."));
    connect(insertRowButton, &QPushButton::clicked, this, &CalculatorDialog::insertRowAtSelection);
    borderRow->addWidget(insertRowButton);

    auto* insertColumnButton = new QPushButton(tr("열 추가"), this);
    insertColumnButton->setToolTip(tr("선택한 열 위치에 빈 열을 삽입합니다. 오른쪽 셀과 수식 참조가 자동으로 밀립니다."));
    connect(insertColumnButton, &QPushButton::clicked, this, &CalculatorDialog::insertColumnAtSelection);
    borderRow->addWidget(insertColumnButton);

    auto* deleteRowButton = new QPushButton(tr("행 삭제"), this);
    deleteRowButton->setToolTip(tr("선택한 행을 삭제합니다. 아래 셀과 수식 참조가 자동으로 당겨집니다."));
    connect(deleteRowButton, &QPushButton::clicked, this, &CalculatorDialog::deleteRowAtSelection);
    borderRow->addWidget(deleteRowButton);

    auto* deleteColumnButton = new QPushButton(tr("열 삭제"), this);
    deleteColumnButton->setToolTip(tr("선택한 열을 삭제합니다. 오른쪽 셀과 수식 참조가 자동으로 당겨집니다."));
    connect(deleteColumnButton, &QPushButton::clicked, this, &CalculatorDialog::deleteColumnAtSelection);
    borderRow->addWidget(deleteColumnButton);

    auto* cellBaseCurrencyButton = new QPushButton(tr("셀 기준 화폐"), this);
    cellBaseCurrencyButton->setToolTip(
        tr("선택한 셀마다 기준 화폐를 개별 지정합니다. 지정하지 않으면 상단 글로벌 기준 화폐를 사용합니다."));
    connect(cellBaseCurrencyButton, &QPushButton::clicked, this, &CalculatorDialog::applyCellBaseCurrencyToSelection);
    borderRow->addWidget(cellBaseCurrencyButton);

    auto* clearCellBaseCurrencyButton = new QPushButton(tr("셀 기준 화폐 초기화"), this);
    clearCellBaseCurrencyButton->setToolTip(
        tr("선택 셀의 개별 기준 화폐를 지우고 글로벌 기준 화폐를 따릅니다."));
    connect(clearCellBaseCurrencyButton, &QPushButton::clicked, this,
            &CalculatorDialog::clearCellBaseCurrencyFromSelection);
    borderRow->addWidget(clearCellBaseCurrencyButton);

    borderRow->addStretch(1);
    rootLayout->addLayout(borderRow);

    auto* colorRow = new QHBoxLayout();
    colorRow->setSpacing(6);
    colorRow->addWidget(new QLabel(tr("셀 색상"), this));

    auto* backgroundColorButton = new QToolButton(this);
    backgroundColorButton->setText(tr("배경색 ▾"));
    backgroundColorButton->setToolButtonStyle(Qt::ToolButtonTextOnly);
    backgroundColorButton->setPopupMode(QToolButton::InstantPopup);
    backgroundColorButton->setMenu(buildColorPaletteMenu(
        backgroundColorButton,
        spreadsheetBackgroundPalette(),
        6,
        [this](const QColor& color) { applyCellBackgroundColor(color); },
        [this]() {
            int minRow = 0;
            int minCol = 0;
            int maxRow = 0;
            int maxCol = 0;
            if (selectedCellBounds(minRow, minCol, maxRow, maxCol)) {
                m_model.clearCellBackgroundColors(minRow, minCol, maxRow, maxCol);
                if (m_table) {
                    m_table->viewport()->update();
                }
            }
        },
        [this]() { showCustomBackgroundColorDialog(); }));
    colorRow->addWidget(backgroundColorButton);

    auto* foregroundColorButton = new QToolButton(this);
    foregroundColorButton->setText(tr("글자색 ▾"));
    foregroundColorButton->setToolButtonStyle(Qt::ToolButtonTextOnly);
    foregroundColorButton->setPopupMode(QToolButton::InstantPopup);
    foregroundColorButton->setMenu(buildColorPaletteMenu(
        foregroundColorButton,
        spreadsheetForegroundPalette(),
        4,
        [this](const QColor& color) { applyCellForegroundColor(color); },
        [this]() {
            int minRow = 0;
            int minCol = 0;
            int maxRow = 0;
            int maxCol = 0;
            if (selectedCellBounds(minRow, minCol, maxRow, maxCol)) {
                m_model.clearCellForegroundColors(minRow, minCol, maxRow, maxCol);
                if (m_table) {
                    m_table->viewport()->update();
                }
            }
        },
        [this]() { showCustomForegroundColorDialog(); }));
    colorRow->addWidget(foregroundColorButton);
    colorRow->addStretch(1);
    rootLayout->addLayout(colorRow);

    m_table = new SpreadsheetTableView(this);
    m_table->setModel(&m_model);
    m_table->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    m_table->verticalHeader()->setDefaultSectionSize(32);
    m_table->setAlternatingRowColors(true);
    m_table->setSelectionBehavior(QAbstractItemView::SelectItems);
    m_table->setSelectionMode(QAbstractItemView::ExtendedSelection);
    m_table->setContextMenuPolicy(Qt::CustomContextMenu);
    m_cellDelegate = new SpreadsheetCellDelegate(&m_model, &m_iconCache, m_table);
    m_table->setItemDelegate(m_cellDelegate);
    m_table->installEventFilter(this);
    m_table->viewport()->installEventFilter(this);
    m_dragAutoScroll = new ListDragAutoScroll(m_table, this);
    rootLayout->addWidget(m_table, 1);

    m_saveTimer = new QTimer(this);
    m_saveTimer->setSingleShot(true);
    m_saveTimer->setInterval(400);
    connect(m_saveTimer, &QTimer::timeout, this, &CalculatorDialog::saveSheetState);

    m_autoRefreshTimer = new QTimer(this);
    m_autoRefreshTimer->setSingleShot(false);
    connect(m_autoRefreshTimer, &QTimer::timeout, this, &CalculatorDialog::onAutoRefreshTimer);

    m_referencePulseTimer = new QTimer(this);
    m_referencePulseTimer->setSingleShot(false);
    m_referencePulseTimer->setInterval(60);
    connect(m_referencePulseTimer, &QTimer::timeout, this, [this]() {
        m_referencePulsePhase += 0.07;
        if (m_referencePulsePhase >= 1.0) {
            m_referencePulsePhase -= 1.0;
        }
        if (m_cellDelegate) {
            m_cellDelegate->setReferencePulsePhase(m_referencePulsePhase);
        }
        if (m_table && !m_referencedCells.isEmpty()) {
            m_table->viewport()->update();
        }
    });

    connect(m_refreshButton, &QPushButton::clicked, this, &CalculatorDialog::onRefreshClicked);
    connect(m_bindCurrencyButton, &QPushButton::clicked, this, &CalculatorDialog::onBindCurrencyClicked);
    connect(m_formulaButton, &QPushButton::clicked, this, &CalculatorDialog::onFormulaBuilderClicked);
    connect(m_helpButton, &QPushButton::clicked, this, &CalculatorDialog::onHelpClicked);
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
    connect(m_formulaBarEdit, &QLineEdit::returnPressed, this, &CalculatorDialog::onFormulaBarCommit);
    connect(m_formulaBarEdit, &QLineEdit::editingFinished, this, &CalculatorDialog::onFormulaBarCommit);

    if (QItemSelectionModel* selectionModel = m_table->selectionModel()) {
        connect(selectionModel, &QItemSelectionModel::currentChanged, this,
                [this](const QModelIndex&, const QModelIndex&) {
                    updateFormulaBarFromSelection();
                    updateReferencedCellHighlight();
                });
        connect(selectionModel, &QItemSelectionModel::selectionChanged, this,
                [this](const QItemSelection&, const QItemSelection&) {
                    updateFormulaBarFromSelection();
                    updateReferencedCellHighlight();
                });
    }
    updateFormulaBarFromSelection();
    updateReferencedCellHighlight();
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
    cancelCellMoveDrag();
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

    m_model.clearUndoHistory();
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
    updateFormulaBarFromSelection();
    updateReferencedCellHighlight();
}

void CalculatorDialog::onHelpClicked() {
    showHelpDialog();
}

QString CalculatorDialog::calculatorHelpHtml() const {
    const auto section = [this](const QString& title, const QStringList& bullets) {
        QString html = QStringLiteral("<p style='margin-top:14px; margin-bottom:6px;'>"
                                        "<b>%1</b></p><ul style='margin-top:0; margin-bottom:0;'>")
                             .arg(title.toHtmlEscaped());
        for (const QString& bullet : bullets) {
            html += QStringLiteral("<li style='margin-bottom:4px;'>%1</li>").arg(bullet.toHtmlEscaped());
        }
        html += QStringLiteral("</ul>");
        return html;
    };

    QString html = QStringLiteral(
        "<body style='font-size:10pt; line-height:1.45;'>"
        "<p style='margin-top:0;'>%1</p>")
                       .arg(tr("poe.ninja 시세를 표에서 계산·정리하는 계산기입니다.").toHtmlEscaped());

    html += section(tr("기본 입력"),
                    {tr("셀에 숫자를 입력하거나 =A1+B1 형식의 수식을 입력합니다."),
                     tr("수식 만들기 버튼으로 사칙연산을 구성할 수 있습니다."),
                     tr("수식 바(fx)에서도 셀 내용을 직접 편집할 수 있습니다.")});

    html += section(tr("시세 연동"),
                    {tr("시세 연동으로 화폐·파편·룬 등 poe.ninja 시세를 셀에 연결합니다."),
                     tr("리그와 기준 화폐는 상단에서 선택합니다."),
                     tr("새로고침 또는 자동 새로고침으로 시세를 갱신합니다."),
                     tr("GGG 집계 기준이라 최대 약 1시간 지연될 수 있습니다.")});

    html += section(tr("셀 선택·편집"),
                    {tr("드래그·Ctrl+클릭·Shift+클릭으로 여러 셀을 선택합니다."),
                     tr("Delete로 선택한 셀을 일괄 지웁니다."),
                     tr("선택한 셀을 드래그하면 이동하며, 다른 셀의 수식 참조도 함께 따라갑니다."),
                     tr("Ctrl+Z / Ctrl+Y(또는 Ctrl+Shift+Z)로 되돌리기·다시 실행합니다.")});

    html += section(tr("행·열"),
                    {tr("행 추가·열 추가: 중간에 빈 줄/열을 넣으면 데이터와 수식 참조(A1 등)가 자동으로 밀립니다."),
                     tr("행 삭제·열 삭제: 선택한 줄/열을 제거하고 아래·오른쪽 셀과 참조를 당깁니다.")});

    html += section(tr("셀 기준 화폐"),
                    {tr("셀 기준 화폐로 셀마다 다른 기준 화폐를 지정할 수 있습니다."),
                     tr("지정하지 않은 셀은 상단 글로벌 기준 화폐를 사용합니다.")});

    html += section(tr("서식"),
                    {tr("테두리·배경색·글자색 메뉴로 셀 표시를 꾸밀 수 있습니다."),
                     tr("소수 자릿수는 상단에서 전체 표시 형식을 조절합니다.")});

    html += QStringLiteral("</body>");
    return html;
}

void CalculatorDialog::showHelpDialog() {
    QDialog dialog(this);
    dialog.setWindowTitle(tr("계산기 도움말"));
    dialog.setModal(true);
    dialog.resize(540, 520);

    auto* layout = new QVBoxLayout(&dialog);
    layout->setContentsMargins(12, 12, 12, 12);
    layout->setSpacing(8);

    auto* browser = new QTextBrowser(&dialog);
    browser->setReadOnly(true);
    browser->setOpenExternalLinks(true);
    browser->setHtml(calculatorHelpHtml());
    layout->addWidget(browser, 1);

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Close, &dialog);
    connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::accept);
    layout->addWidget(buttons);

    dialog.exec();
}

void CalculatorDialog::onUndo() {
    if (m_table && m_table->isEditingView()) {
        return;
    }
    if (m_formulaBarEdit && m_formulaBarEdit->hasFocus()) {
        return;
    }
    if (!m_model.canUndo()) {
        return;
    }

    cancelCellMoveDrag();
    m_model.undo();
    if (m_table) {
        m_table->viewport()->update();
    }
    updateFormulaBarFromSelection();
    updateReferencedCellHighlight();
}

void CalculatorDialog::onRedo() {
    if (m_table && m_table->isEditingView()) {
        return;
    }
    if (m_formulaBarEdit && m_formulaBarEdit->hasFocus()) {
        return;
    }
    if (!m_model.canRedo()) {
        return;
    }

    cancelCellMoveDrag();
    m_model.redo();
    if (m_table) {
        m_table->viewport()->update();
    }
    updateFormulaBarFromSelection();
    updateReferencedCellHighlight();
}

QString CalculatorDialog::cellEditText(const SpreadsheetCell& cell) {
    switch (cell.kind) {
    case SpreadsheetCellKind::Formula:
        return cell.raw.startsWith(QLatin1Char('=')) ? cell.raw : QLatin1Char('=') + cell.raw;
    case SpreadsheetCellKind::ApiRef:
        return cell.currencyName;
    case SpreadsheetCellKind::Empty:
        return {};
    default:
        return cell.raw;
    }
}

QString CalculatorDialog::selectionReferenceLabel() const {
    if (!m_table || !m_table->selectionModel()) {
        return {};
    }

    const QModelIndexList indexes = m_table->selectionModel()->selectedIndexes();
    if (indexes.isEmpty()) {
        return {};
    }
    if (indexes.size() == 1) {
        const QModelIndex index = indexes.first();
        return FormulaEvaluator::cellReference(index.row(), index.column());
    }

    int minRow = INT_MAX;
    int minCol = INT_MAX;
    int maxRow = -1;
    int maxCol = -1;
    for (const QModelIndex& index : indexes) {
        if (!index.isValid()) {
            continue;
        }
        minRow = qMin(minRow, index.row());
        minCol = qMin(minCol, index.column());
        maxRow = qMax(maxRow, index.row());
        maxCol = qMax(maxCol, index.column());
    }
    if (maxRow < 0) {
        return {};
    }
    return FormulaEvaluator::formatCellRange(minRow, minCol, maxRow, maxCol);
}

void CalculatorDialog::updateReferencedCellHighlight() {
    m_referencedCells.clear();
    if (!m_table || !m_cellDelegate) {
        return;
    }

    const QModelIndex current = m_table->currentIndex();
    if (current.isValid()) {
        const SpreadsheetCell cell = m_model.cellInput(current.row(), current.column());
        if (cell.kind == SpreadsheetCellKind::Formula && !cell.raw.isEmpty()) {
            const QList<QPair<int, int>> references = FormulaEvaluator::collectCellReferences(cell.raw);
            for (const QPair<int, int>& ref : references) {
                if (ref.first < 0 || ref.second < 0
                    || ref.first >= m_model.rowCount()
                    || ref.second >= m_model.columnCount()) {
                    continue;
                }
                m_referencedCells.insert(ref);
            }
        }
    }

    m_cellDelegate->setReferencedCells(m_referencedCells);
    m_cellDelegate->setReferencePulsePhase(m_referencePulsePhase);

    if (m_referencedCells.isEmpty()) {
        if (m_referencePulseTimer) {
            m_referencePulseTimer->stop();
        }
    } else if (m_referencePulseTimer && !m_referencePulseTimer->isActive()) {
        m_referencePulseTimer->start();
    }

    m_table->viewport()->update();
}

void CalculatorDialog::updateFormulaBarFromSelection() {
    if (!m_cellNameLabel || !m_formulaBarEdit) {
        return;
    }
    if (m_formulaBarEdit->hasFocus()) {
        return;
    }

    m_updatingFormulaBar = true;

    const QString referenceLabel = selectionReferenceLabel();
    m_cellNameLabel->setText(referenceLabel);
    m_cellNameLabel->setEnabled(!referenceLabel.isEmpty());

    if (m_table && m_table->currentIndex().isValid()) {
        const QModelIndex current = m_table->currentIndex();
        const SpreadsheetCell cell = m_model.cellInput(current.row(), current.column());
        m_formulaBarEdit->setText(cellEditText(cell));
        m_formulaBarEdit->setEnabled(true);
    } else {
        m_formulaBarEdit->clear();
        m_formulaBarEdit->setEnabled(false);
    }

    m_updatingFormulaBar = false;
}

void CalculatorDialog::commitFormulaBarEdit() {
    if (m_updatingFormulaBar || !m_table || !m_formulaBarEdit) {
        return;
    }

    const QModelIndex index = m_table->currentIndex();
    if (!index.isValid()) {
        return;
    }

    if (m_table->isEditingView()) {
        const QWidget* editor = m_table->indexWidget(index);
        if (editor) {
            m_table->closePersistentEditor(index);
        }
    }

    m_model.setData(index, m_formulaBarEdit->text(), Qt::EditRole);
}

void CalculatorDialog::onFormulaBarCommit() {
    if (m_updatingFormulaBar) {
        return;
    }
    commitFormulaBarEdit();
    updateFormulaBarFromSelection();
    updateReferencedCellHighlight();
}

bool CalculatorDialog::selectedCellBounds(int& minRow, int& minCol, int& maxRow, int& maxCol) const {
    if (!m_table || !m_table->selectionModel()) {
        return false;
    }

    const QModelIndexList indexes = m_table->selectionModel()->selectedIndexes();
    if (indexes.isEmpty()) {
        return false;
    }

    minRow = INT_MAX;
    minCol = INT_MAX;
    maxRow = -1;
    maxCol = -1;
    for (const QModelIndex& index : indexes) {
        if (!index.isValid()) {
            continue;
        }
        minRow = qMin(minRow, index.row());
        minCol = qMin(minCol, index.column());
        maxRow = qMax(maxRow, index.row());
        maxCol = qMax(maxCol, index.column());
    }
    return maxRow >= 0;
}

void CalculatorDialog::applyBorderPreset(SpreadsheetBorderPreset preset) {
    int minRow = 0;
    int minCol = 0;
    int maxRow = 0;
    int maxCol = 0;
    if (!selectedCellBounds(minRow, minCol, maxRow, maxCol)) {
        QMessageBox::information(this, tr("테두리"), tr("테두리를 적용할 셀을 먼저 선택하세요."));
        return;
    }

    m_model.pushUndoSnapshot();
    m_model.applyBorderPreset(minRow, minCol, maxRow, maxCol, preset);
    if (m_table) {
        m_table->viewport()->update();
    }
}

void CalculatorDialog::applyCellBackgroundColor(const QColor& color) {
    int minRow = 0;
    int minCol = 0;
    int maxRow = 0;
    int maxCol = 0;
    if (!selectedCellBounds(minRow, minCol, maxRow, maxCol)) {
        QMessageBox::information(this, tr("셀 색상"), tr("색상을 적용할 셀을 먼저 선택하세요."));
        return;
    }

    m_model.pushUndoSnapshot();
    m_model.applyBackgroundColor(minRow, minCol, maxRow, maxCol, color);
    if (m_table) {
        m_table->viewport()->update();
    }
}

void CalculatorDialog::applyCellForegroundColor(const QColor& color) {
    int minRow = 0;
    int minCol = 0;
    int maxRow = 0;
    int maxCol = 0;
    if (!selectedCellBounds(minRow, minCol, maxRow, maxCol)) {
        QMessageBox::information(this, tr("셀 색상"), tr("색상을 적용할 셀을 먼저 선택하세요."));
        return;
    }

    m_model.pushUndoSnapshot();
    m_model.applyForegroundColor(minRow, minCol, maxRow, maxCol, color);
    if (m_table) {
        m_table->viewport()->update();
    }
}

void CalculatorDialog::clearSelectedCellColors() {
    int minRow = 0;
    int minCol = 0;
    int maxRow = 0;
    int maxCol = 0;
    if (!selectedCellBounds(minRow, minCol, maxRow, maxCol)) {
        return;
    }

    m_model.pushUndoSnapshot();
    m_model.clearCellColors(minRow, minCol, maxRow, maxCol);
    if (m_table) {
        m_table->viewport()->update();
    }
}

void CalculatorDialog::showCustomBackgroundColorDialog() {
    QColor initial(255, 255, 255);
    if (m_table && m_table->currentIndex().isValid()) {
        initial = m_model.cellBackgroundColor(m_table->currentIndex().row(),
                                              m_table->currentIndex().column())
                      .value_or(initial);
    }
    const QColor chosen = QColorDialog::getColor(initial, this, tr("배경색 선택"));
    if (chosen.isValid()) {
        applyCellBackgroundColor(chosen);
    }
}

void CalculatorDialog::showCustomForegroundColorDialog() {
    QColor initial(0, 0, 0);
    if (m_table && m_table->currentIndex().isValid()) {
        initial = m_model.cellForegroundColor(m_table->currentIndex().row(),
                                              m_table->currentIndex().column())
                      .value_or(initial);
    }
    const QColor chosen = QColorDialog::getColor(initial, this, tr("글자색 선택"));
    if (chosen.isValid()) {
        applyCellForegroundColor(chosen);
    }
}

void CalculatorDialog::insertRowAtSelection() {
    int row = 0;
    int col = 0;
    if (!primarySelectedCell(row, col)) {
        QMessageBox::information(this, tr("행 추가"), tr("행을 삽입할 위치의 셀을 먼저 선택하세요."));
        return;
    }

    int minRow = row;
    int minCol = 0;
    int maxRow = row;
    int maxCol = 0;
    selectedCellBounds(minRow, minCol, maxRow, maxCol);

    m_model.pushUndoSnapshot();
    if (!m_model.insertRow(minRow)) {
        return;
    }

    if (m_table) {
        m_table->setCurrentIndex(m_model.index(minRow, col));
        m_table->viewport()->update();
    }
    updateFormulaBarFromSelection();
}

void CalculatorDialog::insertColumnAtSelection() {
    int row = 0;
    int col = 0;
    if (!primarySelectedCell(row, col)) {
        QMessageBox::information(this, tr("열 추가"), tr("열을 삽입할 위치의 셀을 먼저 선택하세요."));
        return;
    }

    int minRow = 0;
    int minCol = col;
    int maxRow = 0;
    int maxCol = col;
    selectedCellBounds(minRow, minCol, maxRow, maxCol);

    m_model.pushUndoSnapshot();
    if (!m_model.insertColumn(minCol)) {
        return;
    }

    if (m_table) {
        m_table->setCurrentIndex(m_model.index(row, minCol));
        m_table->viewport()->update();
    }
    updateFormulaBarFromSelection();
}

void CalculatorDialog::deleteRowAtSelection() {
    int row = 0;
    int col = 0;
    if (!primarySelectedCell(row, col)) {
        QMessageBox::information(this, tr("행 삭제"), tr("삭제할 행의 셀을 먼저 선택하세요."));
        return;
    }

    int minRow = row;
    int minCol = 0;
    int maxRow = row;
    int maxCol = 0;
    selectedCellBounds(minRow, minCol, maxRow, maxCol);

    m_model.pushUndoSnapshot();
    if (!m_model.deleteRows(minRow, maxRow)) {
        QMessageBox::information(this, tr("행 삭제"), tr("최소 1개 행은 남겨야 합니다."));
        return;
    }

    if (m_table) {
        const int focusRow = qMin(minRow, m_model.rowCount() - 1);
        m_table->setCurrentIndex(m_model.index(focusRow, col));
        m_table->viewport()->update();
    }
    updateFormulaBarFromSelection();
}

void CalculatorDialog::deleteColumnAtSelection() {
    int row = 0;
    int col = 0;
    if (!primarySelectedCell(row, col)) {
        QMessageBox::information(this, tr("열 삭제"), tr("삭제할 열의 셀을 먼저 선택하세요."));
        return;
    }

    int minRow = 0;
    int minCol = col;
    int maxRow = 0;
    int maxCol = col;
    selectedCellBounds(minRow, minCol, maxRow, maxCol);

    m_model.pushUndoSnapshot();
    if (!m_model.deleteColumns(minCol, maxCol)) {
        QMessageBox::information(this, tr("열 삭제"), tr("최소 1개 열은 남겨야 합니다."));
        return;
    }

    if (m_table) {
        const int focusCol = qMin(minCol, m_model.columnCount() - 1);
        m_table->setCurrentIndex(m_model.index(row, focusCol));
        m_table->viewport()->update();
    }
    updateFormulaBarFromSelection();
}

void CalculatorDialog::cancelCellMoveDrag() {
    if (!m_cellMoveDragArmed && !m_cellMoveDragging) {
        return;
    }
    const bool wasDragging = m_cellMoveDragging;
    m_cellMoveDragArmed = false;
    m_cellMoveDragging = false;
    if (wasDragging && !m_formulaPickActive) {
        m_dragAutoScroll->end();
    }
    if (m_table) {
        m_table->unsetCursor();
        m_table->viewport()->unsetCursor();
    }
    updateStatusLabels();
}

void CalculatorDialog::selectCellRange(int minRow, int minCol, int maxRow, int maxCol) {
    if (!m_table || !m_table->selectionModel()) {
        return;
    }

    QItemSelection selection;
    for (int row = minRow; row <= maxRow; ++row) {
        for (int col = minCol; col <= maxCol; ++col) {
            const QModelIndex index = m_model.index(row, col);
            if (!index.isValid()) {
                continue;
            }
            selection.select(index, index);
        }
    }

    QItemSelectionModel* selectionModel = m_table->selectionModel();
    selectionModel->select(selection, QItemSelectionModel::ClearAndSelect);
    m_table->setCurrentIndex(m_model.index(minRow, minCol));
}

void CalculatorDialog::commitCellMoveDrag(const QModelIndex& dropIndex) {
    if (!dropIndex.isValid()
        || m_cellMoveSrcMinRow < 0
        || m_cellMoveSrcMinCol < 0
        || m_cellMoveSrcMaxRow < m_cellMoveSrcMinRow
        || m_cellMoveSrcMaxCol < m_cellMoveSrcMinCol) {
        cancelCellMoveDrag();
        return;
    }

    const int dstMinRow = dropIndex.row();
    const int dstMinCol = dropIndex.column();
    const int blockHeight = m_cellMoveSrcMaxRow - m_cellMoveSrcMinRow + 1;
    const int blockWidth = m_cellMoveSrcMaxCol - m_cellMoveSrcMinCol + 1;
    const int dstMaxRow = dstMinRow + blockHeight - 1;
    const int dstMaxCol = dstMinCol + blockWidth - 1;

    if (dstMinRow < 0 || dstMinCol < 0) {
        cancelCellMoveDrag();
        return;
    }

    m_model.pushUndoSnapshot();
    if (!m_model.moveCellRange(m_cellMoveSrcMinRow,
                               m_cellMoveSrcMinCol,
                               m_cellMoveSrcMaxRow,
                               m_cellMoveSrcMaxCol,
                               dstMinRow,
                               dstMinCol)) {
        cancelCellMoveDrag();
        return;
    }

    cancelCellMoveDrag();
    selectCellRange(dstMinRow, dstMinCol, dstMaxRow, dstMaxCol);
    updateFormulaBarFromSelection();
    if (m_table) {
        m_table->viewport()->update();
    }
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

    QItemSelectionModel* selectionModel = m_table->selectionModel();
    if (!selectionModel->isSelected(index)) {
        m_table->setCurrentIndex(index);
    }

    const QModelIndexList selectedIndexes = selectionModel->selectedIndexes();
    const bool multiSelected = selectedIndexes.size() > 1;

    QMenu menu(this);
    QAction* formulaAction = menu.addAction(tr("수식 만들기"));
    QAction* bindAction = menu.addAction(tr("시세 연동"));
    menu.addSeparator();
    QAction* backgroundColorAction = menu.addAction(tr("배경색 설정…"));
    QAction* foregroundColorAction = menu.addAction(tr("글자색 설정…"));
    QAction* clearColorsAction = menu.addAction(tr("색상 초기화"));
    menu.addSeparator();
    QAction* insertRowAction = menu.addAction(tr("행 추가"));
    QAction* insertColumnAction = menu.addAction(tr("열 추가"));
    QAction* deleteRowAction = menu.addAction(tr("행 삭제"));
    QAction* deleteColumnAction = menu.addAction(tr("열 삭제"));
    menu.addSeparator();
    QAction* cellBaseCurrencyAction = menu.addAction(tr("셀 기준 화폐 지정…"));
    QAction* clearCellBaseCurrencyAction = menu.addAction(tr("셀 기준 화폐 초기화"));
    menu.addSeparator();
    QAction* clearAction = menu.addAction(multiSelected ? tr("선택 셀 지우기") : tr("셀 지우기"));

    bindAction->setEnabled(!m_model.availableCurrencies().isEmpty() && !multiSelected);

    bool anyClearable = false;
    const QModelIndexList cellsToCheck = multiSelected ? selectedIndexes : QModelIndexList{index};
    for (const QModelIndex& cellIndex : cellsToCheck) {
        if (!cellIndex.isValid()) {
            continue;
        }
        if (m_model.cellInput(cellIndex.row(), cellIndex.column()).kind != SpreadsheetCellKind::Empty) {
            anyClearable = true;
            break;
        }
    }
    clearAction->setEnabled(anyClearable);

    const QAction* chosen = menu.exec(m_table->viewport()->mapToGlobal(pos));
    if (chosen == formulaAction) {
        openFormulaBuilder(index.row(), index.column());
    } else if (chosen == bindAction) {
        bindCurrencyToCellAt(index.row(), index.column());
    } else if (chosen == backgroundColorAction) {
        showCustomBackgroundColorDialog();
    } else if (chosen == foregroundColorAction) {
        showCustomForegroundColorDialog();
    } else if (chosen == clearColorsAction) {
        clearSelectedCellColors();
    } else if (chosen == insertRowAction) {
        insertRowAtSelection();
    } else if (chosen == insertColumnAction) {
        insertColumnAtSelection();
    } else if (chosen == deleteRowAction) {
        deleteRowAtSelection();
    } else if (chosen == deleteColumnAction) {
        deleteColumnAtSelection();
    } else if (chosen == cellBaseCurrencyAction) {
        applyCellBaseCurrencyToSelection();
    } else if (chosen == clearCellBaseCurrencyAction) {
        clearCellBaseCurrencyFromSelection();
    } else if (chosen == clearAction) {
        if (multiSelected) {
            clearSelectedCells();
        } else {
            m_model.pushUndoSnapshot();
            clearCellAt(index.row(), index.column());
        }
    }
}

void CalculatorDialog::bindCurrencyToCellAt(int row, int col) {
    const QList<CurrencyRate> currencies = m_model.availableCurrencies();
    if (currencies.isEmpty()) {
        QMessageBox::warning(this, tr("시세 연동"), tr("먼저 시세를 새로고침하세요."));
        return;
    }

    CurrencyPickerDialog picker(currencies,
                                &m_iconCache,
                                this,
                                QString(),
                                m_model.baseCurrencyId(),
                                m_model.decimalPlaces());
    if (picker.exec() != QDialog::Accepted) {
        return;
    }

    const QString selectedId = picker.selectedCurrencyId();
    const QString selectedName = picker.selectedCurrencyName();
    if (selectedId.isEmpty() || selectedName.isEmpty()) {
        return;
    }

    m_model.pushUndoSnapshot();
    m_model.bindCurrencyToCell(row, col, selectedId, selectedName);
}

void CalculatorDialog::clearCellAt(int row, int col) {
    SpreadsheetCell cell;
    cell.kind = SpreadsheetCellKind::Empty;
    m_model.setCellInput(row, col, cell);
    m_model.clearCellBaseCurrency(row, col, row, col);
}

void CalculatorDialog::applyCellBaseCurrencyToSelection() {
    const QList<CurrencyRate> currencies = m_model.availableCurrencies();
    if (currencies.isEmpty()) {
        QMessageBox::warning(this, tr("셀 기준 화폐"), tr("먼저 시세를 새로고침하세요."));
        return;
    }

    int minRow = 0;
    int minCol = 0;
    int maxRow = 0;
    int maxCol = 0;
    if (!selectedCellBounds(minRow, minCol, maxRow, maxCol)) {
        QMessageBox::information(this, tr("셀 기준 화폐"), tr("기준 화폐를 지정할 셀을 먼저 선택하세요."));
        return;
    }

    CurrencyPickerDialog picker(currencies,
                                &m_iconCache,
                                this,
                                tr("셀 기준 화폐 선택"),
                                m_model.baseCurrencyId(),
                                m_model.decimalPlaces());
    if (picker.exec() != QDialog::Accepted) {
        return;
    }

    const QString selectedId = picker.selectedCurrencyId();
    if (selectedId.isEmpty()) {
        return;
    }

    m_model.pushUndoSnapshot();
    m_model.applyCellBaseCurrency(minRow, minCol, maxRow, maxCol, selectedId);
    if (m_table) {
        m_table->viewport()->update();
    }
    updateFormulaBarFromSelection();
}

void CalculatorDialog::clearCellBaseCurrencyFromSelection() {
    int minRow = 0;
    int minCol = 0;
    int maxRow = 0;
    int maxCol = 0;
    if (!selectedCellBounds(minRow, minCol, maxRow, maxCol)) {
        return;
    }

    m_model.pushUndoSnapshot();
    m_model.clearCellBaseCurrency(minRow, minCol, maxRow, maxCol);
    if (m_table) {
        m_table->viewport()->update();
    }
    updateFormulaBarFromSelection();
}

void CalculatorDialog::clearSelectedCells() {
    if (!m_table || m_table->isEditingView()) {
        return;
    }

    const QItemSelectionModel* selection = m_table->selectionModel();
    if (!selection) {
        return;
    }

    QSet<QPair<int, int>> cleared;
    bool pushedUndo = false;
    for (const QModelIndex& index : selection->selectedIndexes()) {
        if (!index.isValid()) {
            continue;
        }
        const QPair<int, int> key(index.row(), index.column());
        if (cleared.contains(key)) {
            continue;
        }
        if (!pushedUndo) {
            m_model.pushUndoSnapshot();
            pushedUndo = true;
        }
        cleared.insert(key);
        clearCellAt(index.row(), index.column());
    }
}

void CalculatorDialog::onFormulaBuilderClicked() {
    int row = 0;
    int col = 0;
    if (primarySelectedCell(row, col)) {
        openFormulaBuilder(row, col);
        return;
    }
    openFormulaBuilder(0, 0);
}

bool CalculatorDialog::primarySelectedCell(int& row, int& col) const {
    if (!m_table) {
        return false;
    }

    const QModelIndex current = m_table->currentIndex();
    if (current.isValid()) {
        row = current.row();
        col = current.column();
        return true;
    }

    const QItemSelectionModel* selectionModel = m_table->selectionModel();
    if (!selectionModel) {
        return false;
    }

    const QModelIndexList selected = selectionModel->selectedIndexes();
    if (selected.isEmpty()) {
        return false;
    }

    row = selected.first().row();
    col = selected.first().column();
    return true;
}

void CalculatorDialog::applyTargetCellFromCurrentSelection() {
    if (!m_formulaBuilder) {
        return;
    }

    int row = 0;
    int col = 0;
    if (!primarySelectedCell(row, col)) {
        QMessageBox::information(this, tr("수식 만들기"), tr("적용할 셀을 표에서 먼저 선택하세요."));
        return;
    }

    m_formulaBuilder->setTargetCell(row, col);
    m_formulaBuilder->endPickMode();
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
                    updateReferencedCellHighlight();
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
    if (slot == FormulaPickSlot::Target) {
        applyTargetCellFromCurrentSelection();
        return;
    }

    if (!m_table || m_formulaPickActive) {
        return;
    }

    m_formulaPickActive = true;
    m_dragAutoScroll->begin();
    m_table->setSelectionMode(QAbstractItemView::ExtendedSelection);
    m_table->setSelectionBehavior(QAbstractItemView::SelectItems);
    if (slot == FormulaPickSlot::ArrayRange) {
        m_statusLabel->setText(tr("배열 수식 범위: 표에서 드래그해 사각형을 고르세요. Esc로 취소합니다."));
    } else {
        m_statusLabel->setText(tr("표에서 셀을 클릭하거나 드래그해 선택하세요. Esc로 취소합니다."));
    }
}

void CalculatorDialog::exitFormulaPickMode() {
    if (!m_formulaPickActive) {
        return;
    }

    m_formulaPickActive = false;
    m_dragAutoScroll->end();
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
    if (!m_table) {
        return QDialog::eventFilter(watched, event);
    }

    const bool isTable = watched == m_table;
    const bool isViewport = watched == m_table->viewport();
    if (!isTable && !isViewport) {
        return QDialog::eventFilter(watched, event);
    }

    if (event->type() == QEvent::MouseMove && isViewport && m_dragAutoScroll->isActive()) {
        const auto* mouseEvent = static_cast<const QMouseEvent*>(event);
        m_dragAutoScroll->updateFromGlobalCursor();
    }

    if (event->type() == QEvent::MouseButtonRelease && isViewport && m_formulaPickActive) {
        commitFormulaPickFromSelection();
        return false;
    }

    if (!m_formulaPickActive && !m_table->isEditingView() && isViewport) {
        if (event->type() == QEvent::MouseButtonPress) {
            const auto* mouseEvent = static_cast<const QMouseEvent*>(event);
            if (mouseEvent->button() == Qt::LeftButton) {
                const QModelIndex pressIndex = m_table->indexAt(mouseEvent->pos());
                if (pressIndex.isValid()
                    && m_table->selectionModel()
                    && m_table->selectionModel()->isSelected(pressIndex)) {
                    if (selectedCellBounds(m_cellMoveSrcMinRow,
                                           m_cellMoveSrcMinCol,
                                           m_cellMoveSrcMaxRow,
                                           m_cellMoveSrcMaxCol)) {
                        m_cellMoveDragArmed = true;
                        m_cellMoveDragging = false;
                        m_cellMovePressPos = mouseEvent->pos();
                    }
                } else {
                    cancelCellMoveDrag();
                }
            }
        } else if (event->type() == QEvent::MouseMove) {
            const auto* mouseEvent = static_cast<const QMouseEvent*>(event);
            if (m_cellMoveDragArmed
                && (mouseEvent->buttons() & Qt::LeftButton)
                && !m_cellMoveDragging
                && (mouseEvent->pos() - m_cellMovePressPos).manhattanLength()
                       >= QApplication::startDragDistance()) {
                m_cellMoveDragging = true;
                if (!m_formulaPickActive) {
                    m_dragAutoScroll->begin();
                }
                m_table->viewport()->setCursor(Qt::DragMoveCursor);
                m_statusLabel->setText(tr("셀 이동: 놓을 위치에서 마우스를 떼세요. Esc로 취소합니다."));
            }
            if (m_cellMoveDragging) {
                m_dragAutoScroll->updateFromGlobalCursor();
                return true;
            }
        } else if (event->type() == QEvent::MouseButtonRelease) {
            const auto* mouseEvent = static_cast<const QMouseEvent*>(event);
            if (mouseEvent->button() == Qt::LeftButton && m_cellMoveDragging) {
                const QModelIndex dropIndex = m_table->indexAt(mouseEvent->pos());
                commitCellMoveDrag(dropIndex);
                return true;
            }
            if (mouseEvent->button() == Qt::LeftButton) {
                cancelCellMoveDrag();
            }
        }
    }

    if (event->type() == QEvent::KeyPress && (isTable || isViewport)) {
        const auto* keyEvent = static_cast<QKeyEvent*>(event);

        if (m_cellMoveDragging && keyEvent->key() == Qt::Key_Escape) {
            cancelCellMoveDrag();
            return true;
        }

        if (m_formulaPickActive && keyEvent->key() == Qt::Key_Escape) {
            exitFormulaPickMode();
            return true;
        }

        if (!m_formulaPickActive
            && (keyEvent->key() == Qt::Key_Delete || keyEvent->key() == Qt::Key_Backspace)
            && !m_table->isEditingView()) {
            clearSelectedCells();
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
